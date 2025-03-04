/**
 * @file
 * @brief
 *
 * @author Aleksey Zhmulin
 * @date 17.07.23
 */
#include <lib/libds/msg_buff.h>
#include <lib/libds/ring_buff.h>

int msg_buff_empty(struct msg_buff *buf) {
	return (0 == ring_buff_get_cnt(&buf->rbuf));
}
