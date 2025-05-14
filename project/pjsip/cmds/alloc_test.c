/**
 * @file
 *
 * Call and accept calls with account registration
 *
 * @date 26.10.2018
 * @author Alexander Kalmuk
 */

#include <framework/mod/options.h>
#include <mem/heap/mspace_malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define THIS_FILE "APP"

#define MM_SET_HEAP(type, p_prev_type) mspace_set_heap(type, p_prev_type)

#define NUMOF_ALLOCS       1000
#define SIZEOF_ALLOC       16
#define SIZEOF_TEST_ALLOC  13
#define TEST_DATA_INDEX    16

#define DATA_PATTERN_55    0x55
#define DATA_PATTERN_AA    0xaa
#define DATA_PATTERN_00    0x00
#define DATA_PATTERN_FF    0xff

#define DATA_PATTERN       ((uint8_t)((uint32_t)curr_addr>>4)&0xff)
#define ADDR_EXTERNAL_SDRAM  ((uint32_t)0x60000000)

static const uint8_t src_data[] = 
{
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static void dump_mem(uint8_t *ptr, size_t size) {
    printf("Memory dump: %p\n", ptr);

    for (int i = 0; i < size; i++) {
        if(((i+1) % 16 != 0) && ((i+1) % 4 == 0)) {
            printf("%02x|", ptr[i]);
        }
        else
        {
            printf("%02x ", ptr[i]);
        }
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    printf("\n");
}

static int check_mem(int i, uint8_t *ptr, size_t size) {
    for (int j = 0; j < size; j++) {
#if 0
        if (ptr[j] != (i & 0xff)) {
            printf("Memory check failed at index %d: expected %d, got %d\n", j, i, ptr[j]);
            dump_mem(ptr, size);
            return -1;
        }
#else
        if (ptr[j] != src_data[TEST_DATA_INDEX + j]) {
            dump_mem(ptr, size);
            printf("Memory check failed at index %d: expected %02x, got %02x\n", j, src_data[TEST_DATA_INDEX + j], ptr[j]);
            return -1;
        }
#endif
    }
    return 0;
}
#if 0
static void bufcpy(uint8_t *dst, const uint8_t *src, size_t size) {
    for (size_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}
#endif
static uint8_t *allocmems[NUMOF_ALLOCS];

int main(int argc, char *argv[]) {
    uint8_t *ptr;
    int opt;
    int sizeof_test_alloc = SIZEOF_TEST_ALLOC;
    int test_count = NUMOF_ALLOCS;
    int test_type = 0;
    uint8_t *start_ptr = (uint8_t *)ADDR_EXTERNAL_SDRAM;

    MM_SET_HEAP(HEAP_EXTERN_MEM, NULL);

    while (-1 != (opt = getopt(argc, argv, "ies:c:t:d:"))) {
		switch (opt) {
			case 's':
				sizeof_test_alloc = strtol(optarg, NULL, 0);
                if (sizeof_test_alloc < 1 || sizeof_test_alloc > SIZEOF_ALLOC) {
                    printf("Invalid size: must be 1-%d\n", SIZEOF_ALLOC);
                    return -1;
                }
                printf("sizeof_test_alloc: %d\n", sizeof_test_alloc);
				break;
            case 'e':
                printf("Using external memory\n");
                MM_SET_HEAP(HEAP_EXTERN_MEM, NULL);
                start_ptr = (uint8_t *)ADDR_EXTERNAL_SDRAM;
                break;
            case 'i':
                printf("Using internal memory\n");
                MM_SET_HEAP(HEAP_RAM, NULL);
                start_ptr = (uint8_t *)0x20000000;
                break;
            case 'c':
                test_count = strtol(optarg, NULL, 0);
                if (test_count < 1 || test_count > NUMOF_ALLOCS) {
                    printf("Test count must be 1-%d\n", NUMOF_ALLOCS);
                    return -1;
                }              
                printf("Test count : %d\n", test_count);
                break;
            case 't':
                test_type = strtol(optarg, NULL, 0);
                if (test_type < 0 ) {
                    printf("Test type error\n");
                    return -1;
                }
                printf("Test type : %d\n", test_type);
                break;
            case 'd':
                ptr = (uint8_t *)strtol(optarg, NULL, 0);
                dump_mem(ptr, 0x100);
                test_type = 0xff;
                break;
            default:
                break;
		}
	}


    if(test_type == 0)
    {
        printf("alloc_test 1: %s\n", __DATE__);
        ptr = malloc(SIZEOF_ALLOC);
        if (ptr == NULL) {
            printf("malloc failed\n");
            return -1;
        }
        else {
            free(ptr);
        }

        for(int i = 0 ; i < test_count; i++) {
            ptr = malloc(SIZEOF_ALLOC);
            if (ptr == NULL) {
                printf("malloc failed\n");
                return -1;
            }
            printf("malloc %d:%p\n", i, ptr);

            allocmems[i] = ptr;
            memset(ptr, 0xff, SIZEOF_ALLOC);
            memcpy(ptr, &src_data[TEST_DATA_INDEX], sizeof_test_alloc);
            //bufcpy(ptr, &src_data[TEST_DATA_INDEX], sizeof_test_alloc);
        }

        for(int i = 0 ; i < test_count; i++) {
            printf("check_mem %d:%p\n", i, allocmems[i]);
            //dump_mem(allocmems[i], SIZEOF_ALLOC);
            check_mem(i, allocmems[i], sizeof_test_alloc);
        }

        //for(int i = 0 ; i < test_count; i++) {
        //    free(allocmems[i]);
        //    printf("free %d:%p\n", i, allocmems[i]);
        //}
        //printf("free done\n");
    }
    else if (test_type == 1)
    {
        #define TEST_PATTERN1 0x55
        #define TEST_PATTERN2 0xAA
        uint8_t *start_addr = start_ptr;
        size_t test_size = test_count;
        size_t block_size = sizeof_test_alloc;

        uint8_t *curr_addr;
        size_t i;
        
        // Write TEST_PATTERN1
        printf("Writing pattern 0x%02X\n", TEST_PATTERN1);
        for (i = 0; i < test_size; i += block_size) {
            curr_addr = start_addr + i;
            for (size_t j = 0; j < block_size; j++) {
                curr_addr[j] = TEST_PATTERN1;
            }
        }
    
        // Verify TEST_PATTERN1
        for (i = 0; i < test_size; i += block_size) {
            curr_addr = start_addr + i;
            for (size_t j = 0; j < block_size; j++) {
                if (curr_addr[j] != TEST_PATTERN1) {
                    printf("Error at address 0x%08X: expected 0x%02X, got 0x%02X\n", 
                        (uint32_t)&curr_addr[j], TEST_PATTERN1, curr_addr[j]);
                    return -1;
                }
            }
        }
    
        // Write TEST_PATTERN2
        printf("Writing pattern 0x%02X\n", TEST_PATTERN2);
        for (i = 0; i < test_size; i += block_size) {
            curr_addr = start_addr + i;
            for (size_t j = 0; j < block_size; j++) {
                curr_addr[j] = TEST_PATTERN2;
            }
        }
    
        // Verify TEST_PATTERN2
        for (i = 0; i < test_size; i += block_size) {
            curr_addr = start_addr + i;
            for (size_t j = 0; j < block_size; j++) {
                if (curr_addr[j] != TEST_PATTERN2) {
                    printf("Error at address 0x%08X: expected 0x%02X, got 0x%02X\n", 
                        (uint32_t)&curr_addr[j], TEST_PATTERN2, curr_addr[j]);
                }
            }
        }
    
        printf("Memory test 1 finished\n");
        return 0;
    }
    else if (test_type == 2)
    {
        printf("Memory test 2: %s\n", __DATE__);
        uint8_t *start_addr = (uint8_t *)ADDR_EXTERNAL_SDRAM;
        size_t test_size = test_count * 0x1000;
        uint32_t i;

        for(i = 0; i < test_size; i++) {
            uint8_t *curr_addr = start_addr + i;
            *curr_addr = ((i+0x30)&0xff);
        }

        for(i = 0; i < test_size; i++) {
            uint8_t *curr_addr = start_addr + i;
            if(*curr_addr != ((i+0x30)&0xff)) {
                printf(">>>> Error at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                    curr_addr, i, ((i+0x30)&0xff), *curr_addr);
                
                // Align to 16-byte boundary and dump
                uint8_t *aligned_addr = (uint8_t *)((uintptr_t)curr_addr & ~0xFUL);
                dump_mem(aligned_addr, 0x20);
                *curr_addr = ((i+0x30)&0xff);
                if(*curr_addr != ((i+0x30)&0xff)) {
                    printf("Error Again at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, ((i+0x30)&0xff), *curr_addr);
                }
                else {
                    printf("Fixed at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, ((i+0x30)&0xff), *curr_addr);
                }
                dump_mem(aligned_addr, 0x20);

                if(*curr_addr != ((i+0x30)&0xff)) {
                    printf("Error Again at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, ((i+0x30)&0xff), *curr_addr);
                }
                else {
                    printf("Fixed at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, ((i+0x30)&0xff), *curr_addr);
                }
                *curr_addr = ((i+0x30)&0xff);
                dump_mem(aligned_addr, 0x20);
            }
        }

        printf("Memory test 2 finished\n");
    }
    else if (test_type == 3)
    {
        printf("Memory test 3: %s\n", __DATE__);
        uint8_t *start_addr = (uint8_t *)ADDR_EXTERNAL_SDRAM;
        size_t test_size = test_count;
        uint32_t i;

        for(i = 0; i < test_size; i++) {
            uint8_t *curr_addr = start_addr + i * 0x10;
            for(int j = 0; j < sizeof_test_alloc; j++) {
                curr_addr[j] = ((i*0x10+j)&0xff);
            }
        }

        for(i = 0; i < test_size; i++) {
            uint8_t *curr_addr = start_addr + i * 0x10;
            for(int j = 0; j < sizeof_test_alloc; j++) {
                if(curr_addr[j] != ((i*0x10+j)&0xff)) {
                    printf("Error at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, ((i*0x10+j)&0xff), curr_addr[j]);
                    
                    // Align to 16-byte boundary and dump
                    uint8_t *aligned_addr = (uint8_t *)((uintptr_t)curr_addr & ~0xFUL);
                    dump_mem(aligned_addr - 0x20, 0x40);
                }
            }
        }

        printf("Memory test 3 finished\n");
    }
    else if (test_type == 4)
    {
        printf("Memory test 4: %s\n", __DATE__);
        uint8_t *start_addr = (uint8_t *)ADDR_EXTERNAL_SDRAM;
        size_t test_size = test_count * 0x1000;
        uint32_t i;

        for(i = 0; i < test_size; i++) {
            uint8_t *curr_addr = start_addr + i;
            *curr_addr = DATA_PATTERN;
        }

        for(i = 0; i < test_size; i++) {
            uint8_t *curr_addr = start_addr + i;
            if(*curr_addr != DATA_PATTERN) {
                printf(">>>> Error at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                    curr_addr, i, DATA_PATTERN, *curr_addr);
                
                // Align to 16-byte boundary and dump
                uint8_t *aligned_addr = (uint8_t *)((uintptr_t)curr_addr & ~0xFUL) - 0x30;
                if(aligned_addr < start_addr) {
                    aligned_addr = start_addr;
                }
                dump_mem(aligned_addr, 0x40);

                // Fix the error
                *curr_addr = DATA_PATTERN;
                if(*curr_addr != DATA_PATTERN) {
                    printf("Error Again at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, DATA_PATTERN, *curr_addr);
                }
                else {
                    printf("Fixed at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, DATA_PATTERN, *curr_addr);
                }
                dump_mem(aligned_addr, 0x40);

                // Check again
                if(*curr_addr != DATA_PATTERN) {
                    printf("Error Again at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, DATA_PATTERN, *curr_addr);
                }
                else {
                    printf("Fixed at address %p (%x): expected 0x%02x, got 0x%02x\n", 
                        curr_addr, i, DATA_PATTERN, *curr_addr);
                }
                *curr_addr = DATA_PATTERN;
                dump_mem(aligned_addr, 0x40);
            }
        }

        printf("Memory test 4 finished\n");
    }
    return 0;
}
