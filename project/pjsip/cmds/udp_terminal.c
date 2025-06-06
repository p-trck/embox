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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_LINE 64
#define CONFIG_FILE "/conf/network"
#define PARAMS_FILE "/conf/params"

// IP 주소 유효성 검사
int validate_ip(const char *str) {
    char temp[32];
    char *ptr = temp;
    int count = 0;

    strncpy(temp, str, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(ptr, ".");
    while (token && count < 4) {
        char *endptr;
        long val = strtol(token, &endptr, 10);
        if (*endptr != '\0' || val < 0 || val > 255) {
            return 0; // 잘못된 숫자 또는 범위 초과
        }
        token = strtok(NULL, ".");
		count++;
    }
    return (count == 4 && strtok(NULL, ".") == NULL) ? 1 : 0;
}

// MAC 주소 유효성 검사
int validate_mac(const char *str) {
    int count = 0;
    char temp[32];
    char *ptr = temp;

    strncpy(temp, str, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(ptr, ":");
    while (token && count < 6) {
        if (strlen(token) != 2) return 0;
        for (int i = 0; i < 2; i++) {
            if (!isxdigit((unsigned char)token[i])) return 0;
        }
        count++;
        token = strtok(NULL, ":");
    }
    return (count == 6 && strtok(NULL, ":") == NULL) ? 1 : 0;
}

// 네트워크 설정 수정 함수
int set_network_config(const char *option, const char *value) {
    // 유효한 옵션인지 확인
    if (strcmp(option, "address") != 0 &&
        strcmp(option, "netmask") != 0 &&
        strcmp(option, "gateway") != 0 &&
        strcmp(option, "hwaddress") != 0) {
        return -1;
    }

    // 값 형식 검증
    if (strcmp(option, "hwaddress") == 0) {
        if (!validate_mac(value)) return -1;
    } else {
        if (!validate_ip(value)) return -1;
    }

    // 파일 읽기
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) return -1;

    char lines[32][MAX_LINE];
    int line_count = 0;

    // 파일을 라인 단위로 읽어 메모리에 저장
    while (line_count < 32 && fgets(lines[line_count], MAX_LINE, fp)) {
        lines[line_count][strcspn(lines[line_count], "\n")] = '\0'; // 개행 문자 제거
        line_count++;
    }
    fclose(fp);

    // 파일 쓰기
    fp = fopen(CONFIG_FILE, "w");
    if (!fp) return -1;

    // 라인을 순회하며 옵션에 해당하는 라인을 수정
    int updated = 0;
    for (int i = 0; i < line_count; i++) {
        char *line = lines[i];
        char *trimmed = line;

        // 앞쪽 스페이스 또는 탭 제거
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        // 옵션에 해당하는 라인인지 확인
        if (strncmp(trimmed, option, strlen(option)) == 0 &&
            (trimmed[strlen(option)] == ' ' || trimmed[strlen(option)] == '\t')) {
            // 원본 라인의 앞쪽 공백(스페이스/탭)을 유지
            fprintf(fp, "%.*s%s %s\n", (int)(trimmed - line), line, option, value);
            updated = 1;
        } else {
            fprintf(fp, "%s\n", line);
        }
    }

    fclose(fp);
    if (!updated) {
        return -1;
    }

	return 0;
}

int saveParams(const char *option, const char *value) {
    // 파일 읽기
    FILE *fp = fopen(PARAMS_FILE, "r");
    if (!fp) return -1;

    char lines[32][MAX_LINE];
    int line_count = 0;

    // 파일을 라인 단위로 읽어 메모리에 저장
    while (line_count < 32 && fgets(lines[line_count], MAX_LINE, fp)) {
        lines[line_count][strcspn(lines[line_count], "\n")] = '\0'; // 개행 문자 제거
        line_count++;
    }
    fclose(fp);

    // 파일 쓰기
    fp = fopen(PARAMS_FILE, "w");
    if (!fp) return -1;

    // 라인을 순회하며 옵션에 해당하는 라인을 수정
    int updated = 0;
    for (int i = 0; i < line_count; i++) {
        char *line = lines[i];

        // 옵션에 해당하는 라인인지 확인
        if (strncmp(line, option, strlen(option)) == 0){
            fprintf(fp, "%s %s\n", option, value);
            updated = 1;
        } else {
            fprintf(fp, "%s\n", line);
        }
    }

    fclose(fp);
    if (!updated) {
        return -1;
    }

	return 0;
}

int restoreVolumeLevel()
{
	FILE *fp;
	char line[64];
	int volume = -1; // 기본 볼륨 레벨

	fp = fopen(PARAMS_FILE, "r");
	if (fp != NULL) {
		while (fgets(line, sizeof(line), fp)) {
			if (strncmp(line, "Volume:", 7) == 0) {
				volume = atoi(line + 7);
				break;
			}
		}
		fclose(fp);
	}

	if(volume < 0 || volume > 100) {
		printf("Invalid volume level.(%d)\n", volume);
		saveParams("Volume:", "70");
	}
	else {
		printf("Restoring volume level: %d\n", volume);
		extern uint8_t BSP_AUDIO_OUT_SetVolume(uint8_t Volume);
		BSP_AUDIO_OUT_SetVolume(volume);
	}

	return 0;
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

void printNetInfo(int sock, struct sockaddr_in *client_addr, socklen_t addr_len) {
	FILE *fp = fopen(CONFIG_FILE, "r");

	if (fp != NULL) {
		char line[MAX_LINE];
		while (fgets(line, sizeof(line), fp)) {
	        char *trimmed = line;

			while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

			if (strncmp(trimmed, "address", 7) == 0) {
				sendto(sock, trimmed, strlen(trimmed), 0, (struct sockaddr *)client_addr, addr_len);
				printf("%s", trimmed);
			}
			else if (strncmp(trimmed, "netmask", 7) == 0) {
				sendto(sock, trimmed, strlen(trimmed), 0, (struct sockaddr *)client_addr, addr_len);
				printf("%s", trimmed);
			}
			else if (strncmp(trimmed, "gateway", 7) == 0) {
				sendto(sock, trimmed, strlen(trimmed), 0, (struct sockaddr *)client_addr, addr_len);
				printf("%s", trimmed);
			}
			else if (strncmp(trimmed, "hwaddress", 9) == 0) {
				sendto(sock, trimmed, strlen(trimmed), 0, (struct sockaddr *)client_addr, addr_len);
				printf("%s", trimmed);
			}
		}
		fclose(fp);
	}
}

void *udp_command_listener(void *arg) {
	char buffer[64];
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);

	struct timeval timeout;

	restoreVolumeLevel();

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
			sendto(sock, ip, strlen(ip), 0, (struct sockaddr *)&client_addr, addr_len);
			sendto(sock, "\n", 1, 0, (struct sockaddr *)&client_addr, addr_len);
			printf("IP:%s\n", ip);
		}
		else if (strcmp(buffer, "reboot") == 0) {
			const char *msg = "reboot system\n";
			sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addr_len);
			safe_system_reset();
		}
		else if (strcmp(buffer, "version") == 0) {
			FILE *fp;
			
			fp = fopen("/version", "r");
			if (fp != NULL) {
				while (fgets(buffer, sizeof(buffer), fp)) {
					uint8_t show = 0;

					if (strncmp(buffer, "Version", 7) == 0) {
						show = 1;
					}
					else if (strncmp(buffer, "Product", 7) == 0) {
						show = 1;
					}
					else if (strncmp(buffer, "Date", 4) == 0) {
						show = 1;
					}

					if(show) {
						sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr *)&client_addr, addr_len);
						printf("%s", buffer);
					}
				}
				fclose(fp);
			} else {
				perror("Failed to open version file");
			}
		}
		else if (strncmp(buffer, "volume", 6) == 0) {
			char vol_str[6];

			if(buffer[6] != ' ') { // get volume
				extern uint8_t BSP_AUDIO_OUT_GetVolume(void);
				int vol = BSP_AUDIO_OUT_GetVolume();
				snprintf(vol_str, sizeof(vol_str), "%d\n", vol);
				sendto(sock, vol_str, strlen(vol_str), 0, (struct sockaddr *)&client_addr, addr_len);
				printf("Volume: %d\n", vol);
				continue;
			}

			int vol = atoi(buffer + 7);
			if (vol >= 0 && vol <= 100) {
				extern uint8_t BSP_AUDIO_OUT_SetVolume(uint8_t Volume);

				BSP_AUDIO_OUT_SetVolume(vol);

				const char *msg = "DO:Wait saving..\n";
				sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addr_len);
				snprintf(vol_str, sizeof(vol_str), "%d", vol);
				if(saveParams("Volume:", vol_str) == 0)
				{
					const char *msg = "OK:Saved\n";
					sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addr_len);
					printf(msg);
				}
				else
				{
					const char *msg = "ERR:Failed to save volume\n";
					sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addr_len);
					printf(msg);
				}
			} else {
				const char *msg = "ERR:Volume must be between 0-100\n";
				sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addr_len);
			}
		}
		else if (strncmp(buffer, "net", 3) == 0) {
			if (buffer[3] != ' ') {
				printNetInfo(sock, &client_addr, addr_len);
				continue;
			}

			extern uint8_t get_call_state(void);
			if(get_call_state() == 1) {
				const char *msg = "ERR:Calling\n";
				sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addr_len);
			}
			else
			{
				char *option = buffer + 4;
				char *value = strchr(option, ' ');
				if (value != NULL) {
					*value = '\0';
					value++;
					const char *msg = "DO:Wait saving..\n";
					sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addr_len);
					if (set_network_config(option, value) == 0) {
						const char *msg = "OK:Saved params\n";
						sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addr_len);
						printf("Modified: %s %s\n", option, value);
					} else {
						const char *error_msg = "ERR:Invalid option or value\n";
						sendto(sock, error_msg, strlen(error_msg), 0, (struct sockaddr *)&client_addr, addr_len);
					}
				}
			}
		} else {
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