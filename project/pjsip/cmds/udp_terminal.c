#include <arpa/inet.h>
#include <assert.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <framework/mod/options.h>
#include <net/if.h>
#include <net/inetdevice.h>
#include <net/netdevice.h>
#include <net/util/macaddr.h>

#define THIS_FILE "UDT"

#define PORT       OPTION_GET(NUMBER, udp_port)
#define MODEL_NAME "NetSpeaker"
#define VERSION    "1.0.0"

static int sock;
static struct sockaddr_in broadcast_addr, local_addr;

char *get_broadcast_address() {
	struct ifaddrs *ifap, *ifa;
	static char br_addr[INET_ADDRSTRLEN];

	// Get list of network interfaces
	if (getifaddrs(&ifap) == -1) {
		perror("getifaddrs failed");
		return NULL;
	}

	// Find first active non-loopback interface with IPv4
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET
		    && !(ifa->ifa_flags & IFF_LOOPBACK)
		    && (ifa->ifa_flags & IFF_BROADCAST)) {
			struct sockaddr_in *broadcast =
			    (struct sockaddr_in *)ifa->ifa_broadaddr;
			inet_ntop(AF_INET, &broadcast->sin_addr, br_addr, INET_ADDRSTRLEN);
			printf("Using interface: %s, Broadcast address: %s\n",
			    ifa->ifa_name, br_addr);
			freeifaddrs(ifap);
			return br_addr;
		}
	}

	freeifaddrs(ifap);
	return NULL;
}

char *get_ip_address() {
	struct ifaddrs *ifap, *ifa;
	static char ip[INET_ADDRSTRLEN];

	if (getifaddrs(&ifap) == -1) {
		perror("getifaddrs failed");
		return NULL;
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET
		    && !(ifa->ifa_flags & IFF_LOOPBACK)) {
			struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
			inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
			freeifaddrs(ifap);
			return ip;
		}
	}

	freeifaddrs(ifap);
	return NULL;
}

/**
 * 시스템 리셋을 수행하는 함수
 * 
 * @return 성공 시 0, 실패 시 -1을 반환합니다.
 * 일반적으로 성공적인 리셋의 경우 이 함수는 반환되지 않습니다.
 */
int system_reset(void) {
	printf("System reset...\n");

	/* 모든 인터럽트 비활성화 */
	ipl_t ipl = ipl_save();

    /* CPU 리셋 */
    platform_shutdown(SHUTDOWN_MODE_REBOOT);

	/* 리셋이 실패한 경우 인터럽트 복원 */
	ipl_restore(ipl);

	/* 리셋이 실패하면 이 지점에 도달합니다 */
	fprintf(stderr, "Fail to reset system\n");
	return -1;
}

/**
 * 세이프 리셋을 수행하는 함수
 * 중요한 작업을 완료한 후 리셋을 수행합니다.
 * 
 * @return 성공 시 0, 실패 시 -1을 반환합니다.
 */
int safe_system_reset(void) {
	/* 열린 파일 핸들을 닫고 버퍼를 플러시 */
	fflush(NULL);

	/* 필요한 경우 파일 시스템 동기화 */
	sync();

	/* 잠시 대기 (선택적) */
	sleep(1);

	/* 실제 리셋 수행 */
	return system_reset();
}

int init_udp_socket() {
	int broadcast_enable = 1;

	// 브로드캐스트 주소 가져오기
	char *broadcast_ip = get_broadcast_address();
	if (broadcast_ip == NULL) {
		printf("No suitable network interface found\n");
		return -1;
	}

	// 소켓 생성
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Socket creation failed");
		return -1;
	}

	// SO_BROADCAST 옵션 설정
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable,
	        sizeof(broadcast_enable))
	    < 0) {
		perror("Setsockopt SO_BROADCAST failed");
		close(sock);
		return -1;
	}

	// 로컬 주소 설정
	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	local_addr.sin_port = htons(PORT);

	// 바인딩
	if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
		perror("Bind failed");
		close(sock);
		return -1;
	}

	// 브로드캐스트 주소 설정
	memset(&broadcast_addr, 0, sizeof(broadcast_addr));
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(PORT);
	broadcast_addr.sin_addr.s_addr = inet_addr(broadcast_ip);

	return sock;
}

void *udp_command_listener(void *arg) {
	char buffer[64];
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);

	struct timeval timeout;

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))
	    < 0) {
		perror("Setsockopt SO_RCVTIMEO failed");
		return NULL;
	}

	while (1) {
		int len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
		    (struct sockaddr *)&client_addr, &addr_len);
		if (len < 0) {
			//perror("recvfrom failed");
			continue;
		}
		buffer[len] = '\0';

		printf("Received(%s): %s\n", inet_ntoa(client_addr.sin_addr), buffer);

		if (strcmp(buffer, "getip") == 0) {
			char *ip;
			ip = get_ip_address();
			sendto(sock, ip, strlen(ip), 0, (struct sockaddr *)&client_addr,
			    addr_len);
			printf("IP:%s\n", ip);
		}
		else if (strcmp(buffer, "reboot") == 0) {
			const char *msg = "reboot system";
			sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr,
			    addr_len);
			safe_system_reset();
		}
		else if (strcmp(buffer, "model") == 0) {
			sendto(sock, MODEL_NAME, strlen(MODEL_NAME), 0,
			    (struct sockaddr *)&client_addr, addr_len);
			printf("Model:%s\n", MODEL_NAME);
		}
		else if (strcmp(buffer, "version") == 0) {
			sendto(sock, VERSION, strlen(VERSION), 0,
			    (struct sockaddr *)&client_addr, addr_len);
			printf("Version:%s\n", VERSION);
		}
		else {
			const char *unknown_cmd = "Unknown command\n";
			sendto(sock, unknown_cmd, strlen(unknown_cmd), 0,
			    (struct sockaddr *)&client_addr, addr_len);
		}
	}

	return NULL;
}

int init_udp_terminal() {
	sock = init_udp_socket();
	if (sock < 0) {
		printf("Failed to initialize UDP terminal\n");
		return -1;
	}

	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, udp_command_listener, NULL) != 0) {
		perror("Failed to create UDP command listener thread");
		close(sock);
		return -1;
	}

	printf("UDP terminal initialized\n");

	return 0;
}