/**
 * @file memtest.c
 * @brief Simple utility to measure memory performance
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version
 * @date 21.03.2019
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <ifaddrs.h>

#include <hal/arch.h>
#include <hal/ipl.h>
#include <kernel/irq.h>

#include <framework/mod/options.h>

#include "diagtest.h"

static void print_help(char **argv) {
	assert(argv);
	assert(argv[0]);
	printf("Usage: %s [-hp]\n", argv[0]);
	printf("\tOptions: -h print help\n");
	printf("\t         -p do something\n");
}

#define STT_FILE  OPTION_STRING_GET(diagstatus)

#define BROADCAST_IP "255.255.255.255"
#define PORT 9090
#define SIZEOF_RXBUFFER 32
#define MSG_HANDSHAKE "whoareyou"
#define MSG_RESPONSE "iamdiag"

static int sock;
static struct sockaddr_in broadcast_addr, local_addr;


char* get_broadcast_address() {
    struct ifaddrs *ifap, *ifa;
    static char broadcast_addr[INET_ADDRSTRLEN];
    
    // Get list of network interfaces
    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs failed");
        return NULL;
    }
    
    // Find first active non-loopback interface with IPv4
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && 
            !(ifa->ifa_flags & IFF_LOOPBACK) && (ifa->ifa_flags & IFF_BROADCAST)) {
            
            struct sockaddr_in *broadcast = (struct sockaddr_in *)ifa->ifa_broadaddr;
            inet_ntop(AF_INET, &broadcast->sin_addr, broadcast_addr, INET_ADDRSTRLEN);
            printf("Using interface: %s, Broadcast address: %s\n", ifa->ifa_name, broadcast_addr);
            freeifaddrs(ifap);
            return broadcast_addr;
        }
    }
    
    freeifaddrs(ifap);
    return NULL;
}

char* get_ip_address() {
    struct ifaddrs *ifap, *ifa;
    static char ip[INET_ADDRSTRLEN];
    
    if (getifaddrs(&ifap) == -1) {
        perror("getifaddrs failed");
        return NULL;
    }
    
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && 
            !(ifa->ifa_flags & IFF_LOOPBACK)) {
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

int send_message(char *message) {
    if (sendto(sock, message, strlen(message), 0, 
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("Broadcast send failed");
        return -1;
    }
    
    printf("> %s\n", message);
    return 0;
}

int recv_message(char *message, int len, int waitsec)
{
    // 타임아웃 설정
    struct timeval timeout;

    timeout.tv_sec = waitsec;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Setsockopt SO_RCVTIMEO failed");
        return -1;
    }

    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    int received_bytes = recvfrom(sock, message, len - 1, 0,
                                (struct sockaddr*)&sender_addr, &sender_len);
    
    if (received_bytes < 0) {
        //perror("No response received within timeout");
        return -1;
    }
    
    message[received_bytes] = '\0';
    printf("Received response: %s from %s\n", message, inet_ntoa(sender_addr.sin_addr));
    
    return received_bytes;
}

int proc_handshake() {

    char buffer[SIZEOF_RXBUFFER];

    if(send_message(MSG_HANDSHAKE))
    {
        perror("Broadcast send failed");
        return -1;
    }

    if(recv_message(buffer, SIZEOF_RXBUFFER, 1) <= 0)
    {
        perror("No response received within timeout");
        return -1;
    }
   
    int result = (strcmp(buffer, MSG_RESPONSE) == 0) ? 0 : -1;
    
    return result;
}

int init_udp_socket()
{
    int broadcast_enable = 1;
    
    // 브로드캐스트 주소 가져오기
    char* broadcast_ip = get_broadcast_address();
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
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
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
    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
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

void proc_udpTerminal()
{
    char buffer[SIZEOF_RXBUFFER];
    int received_bytes;
    int result = -1;
    
	send_message("MODEL: NetSpeaker");
    send_message("VERSION: 1.0.0");
    send_message("DIAGNOSTIC MODE");
    snprintf(buffer, SIZEOF_RXBUFFER, "IP: %s", get_ip_address());
    send_message(buffer);
    send_message("Type 'exit' to quit");

    while (1) {
        received_bytes = recv_message(buffer, SIZEOF_RXBUFFER, 1);
		if (received_bytes > 0) {
			printf("Received message: %s\n", buffer);
			if (strcmp(buffer, "exit") == 0) {
                result = 0;
				break;
			}
			else if (strcmp(buffer, "reboot") == 0) {
				safe_system_reset();
                result = 0;
			}
			else if (strncmp(buffer, "spk", 3) == 0) {
				result = proc_testSpeaker(buffer);
			}
			else if (strcmp(buffer, "mic") == 0) {
				result = proc_testMIC();
			}
            else if (strncmp(buffer, "volume ", 7) == 0) {
                result = proc_setVolume(buffer);
            }

            if (result == 0) {
                send_message("OK");
            } else if (result == -1) {
                send_message("??");
            } else {
                send_message("NG");
            }

            result = -1;
		}
	}
}

int main(int argc, char **argv) {
	int opt;
	int ret;

	while (-1 != (opt = getopt(argc, argv, "phmo"))) {
		switch (opt) {
			case 'p': {
				printf("Hello world\n");
            }
			break;
			case 'h':
				print_help(argv);
				return 0;
			case 'm':
				ret = init_udp_socket();
				if(ret < 0)
				{
					printf("init_udp_socket failed\n");
					return -1;
				}

				if(0 == proc_handshake())
				{
					printf("handshake ok\n");
					proc_udpTerminal();
				}
				else
				{
					printf("handshake failed\n");
				}

				close(sock);
				break;
			case 'o':
                printf("output source\n");
                proc_outputSource();
                break;
			default:
				printf("default\n");
				break;
		}
	}

	return 0;
}
