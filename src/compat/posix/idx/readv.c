/**
 * @file
 * @brief
 *
 * @date 28.04.2016
 * @author Denis Chusovitin
 */

#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>

#include <kernel/task/resource/index_descriptor.h>
#include <kernel/task/resource/idesc.h>
#include <kernel/task/resource/idesc_table.h>

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
	int ret;
	struct idesc *idesc;

	if (iovcnt <= 0) {
		return SET_ERRNO(EINVAL);
	}

	if (!idesc_index_valid(fd)
			|| (NULL == (idesc = index_descriptor_get(fd)))
			|| ((idesc->idesc_flags & O_ACCESS_MASK) == O_WRONLY)) {
		return SET_ERRNO(EBADF);
	}

	assert(idesc->idesc_ops);
	assert(idesc->idesc_ops->id_readv);

	ret = idesc->idesc_ops->id_readv(idesc, iov, iovcnt);
	if (ret < 0) {
		return SET_ERRNO(-ret);
	}

	return ret;
}
