/**
 * @file
 *
 * @date 06.08.09
 * @author Anton Bondarev
 */

#include <stddef.h>
#include <assert.h>
#include <errno.h>

#include <util/err.h>

#include <fs/inode.h>
#include <fs/file_desc.h>
#include <fs/kfile.h>

struct idesc *kopen(struct inode *node, int flag) {
	struct super_block *sb;
	struct file_desc *desc;
	const struct file_operations *ops;
	int ret;
	struct idesc *idesc;

	assert(node);
	assertf(!(flag & (O_CREAT | O_EXCL)), "use kcreat() instead kopen()");
	assertf(!(flag & O_DIRECTORY), "use mkdir() instead kopen()");


	sb = node->i_sb;
	/* if we try open a file (not special) we must have the file system */
	if (NULL == sb) {
		SET_ERRNO(ENOSUPP);
		return NULL;
	}

	if (S_ISDIR(node->i_mode)) {
		ops = sb->sb_fops;
	} else {
		if (NULL == sb->sb_fops) {
			SET_ERRNO(ENOSUPP);
			return NULL;
		}

		ops = sb->sb_fops;

		if (S_ISCHR(node->i_mode)) {
			/* Note: we suppose this node belongs to devfs */
			idesc = ops->open(node, NULL, flag);
			idesc->idesc_flags = flag;
			return idesc;
		}
	}

	if(ops == NULL) {
		SET_ERRNO(ENOSUPP);
		return NULL;
	}

	desc = file_desc_create(node, flag);
	if (0 != ptr2err(desc)) {
		SET_ERRNO(-(uintptr_t)desc);
		return NULL;
	}
	desc->f_ops = ops;

	if (desc->f_ops->open != NULL) {
		idesc = desc->f_ops->open(node, &desc->f_idesc, flag);
		if (ptr2err(idesc)){
			ret = (uintptr_t)idesc;
			goto free_out;
		}
	} else {
		idesc = &desc->f_idesc;
	}

	if ((struct idesc *)idesc == &desc->f_idesc) {
		goto out;
	} else {
		file_desc_destroy(desc);
		return idesc;
	}

free_out:
	if (ret < 0) {
		file_desc_destroy(desc);
		SET_ERRNO(-ret);
		return NULL;
	}

out:
	return &desc->f_idesc;
}
