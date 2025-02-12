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

#include <framework/mod/options.h>

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
#define BUFFER_SIZE 32


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

int send_broadcast_message() {
    int sock;
    struct sockaddr_in broadcast_addr, local_addr;
    char buffer[BUFFER_SIZE];
    int broadcast_enable = 1;
    int timeout_sec = 5;
    
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

    // 타임아웃 설정
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Setsockopt SO_RCVTIMEO failed");
        close(sock);
        return -1;
    }
    
    // 브로드캐스트 주소 설정
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(broadcast_ip);
    
    // "whoareyou" 메시지 전송
    const char *message = "whoareyou";
    if (sendto(sock, message, strlen(message), 0, 
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("Broadcast send failed");
        close(sock);
        return -1;
    }
    
    printf("Broadcast message sent: %s\n", message);
    
    // 응답 대기
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    int received_bytes = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                                (struct sockaddr*)&sender_addr, &sender_len);
    
    if (received_bytes < 0) {
        perror("No response received within timeout");
        close(sock);
        return -1;
    }
    
    buffer[received_bytes] = '\0';
    printf("Received response: %s from %s\n", buffer, inet_ntoa(sender_addr.sin_addr));
    
    // "iamdiag" 메시지 확인
    int result = (strcmp(buffer, "iamdiag") == 0) ? 0 : -1;
    
    close(sock);
    return result;
}


int main(int argc, char **argv) {
	int opt;

	while (-1 != (opt = getopt(argc, argv, "phm"))) {
		switch (opt) {
			case 'p': {
				printf("Hello world\n");
            }
			break;
			case 'h':
				print_help(argv);
				return 0;
			case 'm':
				if(0 == send_broadcast_message())
					printf("handshake ok\n");
				else
					printf("handshake failed\n");
				break;
			default:
				printf("default\n");
				break;
		}
	}

	return 0;
}
