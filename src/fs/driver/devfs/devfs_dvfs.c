/**
 * @file devfs_dvfs.c
 * @brief
 * @author Denis Deryugin <deryugin.denis@gmail.com>
 * @version 0.1
 * @date 2015-09-28
 */

/* IMPORTANT NOTE
 *
 * In current implementation, devfs is supposed to have no nested folders,
 * so all operations are threated just like you use /dev directory,
 * or whereever did you mount devfs. Future release should fix it, probably
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include <drivers/block_dev.h>
#include <drivers/char_dev.h>
#include <drivers/device.h>
#include <framework/mod/options.h>
#include <fs/dvfs.h>
#include <kernel/task/resource/idesc.h>
#include <lib/libds/array.h>
#include <util/err.h>

extern struct idesc_ops idesc_bdev_ops;
extern int devfs_destroy_inode(struct inode *inode);
extern int devfs_fill_sb(struct super_block *sb, const char *source);

/* Call device-specific open() handler */
static struct idesc *devfs_open_idesc(struct lookup *l, int __oflag) {
	struct inode *i_no;
	struct char_dev *cdev;
	struct idesc *idesc;

	assert(l);
	assert(l->item);
	assert(l->item->d_inode);

	i_no = l->item->d_inode;

	if (S_ISBLK(i_no->i_mode)) {
		/* XXX */
		idesc = dvfs_file_open_idesc(l, __oflag);

		idesc->idesc_ops = &idesc_bdev_ops;

		return idesc;
	}

	cdev = i_no->i_privdata;
	assert(cdev);

	return char_dev_open(cdev, __oflag);
}

struct block_dev *bdev_by_path(const char *dev_name) {
	struct lookup lookup = {};
	struct dev_module *devmod;
	int res;

	if (!dev_name) {
		return NULL;
	}
	if (!strlen(dev_name)) {
		return NULL;
	}

	/* Check if devfs is initialized */
	res = dvfs_lookup("/dev", &lookup);
	if (res) {
		/* devfs is not mounted yet */
		return block_dev_find(dev_name);
	}
	dentry_ref_dec(lookup.item);

	/* devfs presents, perform usual mount */
	memset(&lookup, 0, sizeof(lookup));
	dvfs_lookup(dev_name, &lookup);
	if (!lookup.item) {
		SET_ERRNO(ENOENT);
		return NULL;
	}

	assert(lookup.item->d_inode);

	devmod = inode_priv(lookup.item->d_inode);

	dentry_ref_dec(lookup.item);

	return dev_module_to_bdev(devmod);
}

struct super_block_operations devfs_sbops = {
    .open_idesc = devfs_open_idesc,
    .destroy_inode = devfs_destroy_inode,
};
