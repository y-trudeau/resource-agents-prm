/*
 * Copyright (C) Sistina Software, Inc.  1997-2003 All rights reserved.
 * Copyright (C) 2004-2005 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/buffer_head.h>
#include <linux/crc32.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include "gfs2.h"
#include "glock.h"
#include "lm.h"

kmem_cache_t *gfs2_glock_cachep;
kmem_cache_t *gfs2_inode_cachep;
kmem_cache_t *gfs2_bufdata_cachep;

uint32_t gfs2_disk_hash(const char *data, int len)
{
	return crc32_le(0xFFFFFFFF, data, len) ^ 0xFFFFFFFF;
}

/**
 * gfs2_assert_i - Cause the machine to panic if @assertion is false
 *
 */

void gfs2_assert_i(struct gfs2_sbd *sdp, char *assertion, const char *function,
		   char *file, unsigned int line)
{
	if (sdp->sd_args.ar_oopses_ok) {
		printk("GFS2: fsid=%s: fatal: assertion \"%s\" failed\n"
		       "GFS2: fsid=%s:   function = %s\n"
		       "GFS2: fsid=%s:   file = %s, line = %u\n"
		       "GFS2: fsid=%s:   time = %lu\n",
		       sdp->sd_fsname, assertion,
		       sdp->sd_fsname, function,
		       sdp->sd_fsname, file, line,
		       sdp->sd_fsname, get_seconds());
		BUG();
	}
	dump_stack();
	panic("GFS2: fsid=%s: fatal: assertion \"%s\" failed\n"
	      "GFS2: fsid=%s:   function = %s\n"
	      "GFS2: fsid=%s:   file = %s, line = %u\n"
	      "GFS2: fsid=%s:   time = %lu\n",
	      sdp->sd_fsname, assertion,
	      sdp->sd_fsname, function,
	      sdp->sd_fsname, file, line,
	      sdp->sd_fsname, get_seconds());
}

/**
 * gfs2_assert_withdraw_i - Cause the machine to withdraw if @assertion is false
 * Returns: -1 if this call withdrew the machine,
 *          -2 if it was already withdrawn
 */

int gfs2_assert_withdraw_i(struct gfs2_sbd *sdp, char *assertion,
			   const char *function, char *file, unsigned int line)
{
	int me;
	me = gfs2_lm_withdraw(sdp,
			     "GFS2: fsid=%s: fatal: assertion \"%s\" failed\n"
			     "GFS2: fsid=%s:   function = %s\n"
			     "GFS2: fsid=%s:   file = %s, line = %u\n"
			     "GFS2: fsid=%s:   time = %lu\n",
			     sdp->sd_fsname, assertion,
			     sdp->sd_fsname, function,
			     sdp->sd_fsname, file, line,
			     sdp->sd_fsname, get_seconds());
	return (me) ? -1 : -2;
}

/**
 * gfs2_assert_warn_i - Print a message to the console if @assertion is false
 * Returns: -1 if we printed something
 *          -2 if we didn't
 */

int gfs2_assert_warn_i(struct gfs2_sbd *sdp, char *assertion,
		       const char *function, char *file, unsigned int line)
{
	if (time_before(jiffies,
			sdp->sd_last_warning +
			gfs2_tune_get(sdp, gt_complain_secs) * HZ))
		return -2;

	printk("GFS2: fsid=%s: warning: assertion \"%s\" failed\n"
	       "GFS2: fsid=%s:   function = %s\n"
	       "GFS2: fsid=%s:   file = %s, line = %u\n"
	       "GFS2: fsid=%s:   time = %lu\n",
	       sdp->sd_fsname, assertion,
	       sdp->sd_fsname, function,
	       sdp->sd_fsname, file, line,
	       sdp->sd_fsname, get_seconds());

	if (sdp->sd_args.ar_debug)
		BUG();

	sdp->sd_last_warning = jiffies;

	return -1;
}

/**
 * gfs2_consist_i - Flag a filesystem consistency error and withdraw
 * Returns: -1 if this call withdrew the machine,
 *          0 if it was already withdrawn
 */

int gfs2_consist_i(struct gfs2_sbd *sdp, int cluster_wide, const char *function,
		   char *file, unsigned int line)
{
	return gfs2_lm_withdraw(sdp,
			"GFS2: fsid=%s: fatal: filesystem consistency error\n"
			"GFS2: fsid=%s:   function = %s\n"
			"GFS2: fsid=%s:   file = %s, line = %u\n"
			"GFS2: fsid=%s:   time = %lu\n",
			sdp->sd_fsname,
			sdp->sd_fsname, function,
			sdp->sd_fsname, file, line,
			sdp->sd_fsname, get_seconds());
}

/**
 * gfs2_consist_inode_i - Flag an inode consistency error and withdraw
 * Returns: -1 if this call withdrew the machine,
 *          0 if it was already withdrawn
 */

int gfs2_consist_inode_i(struct gfs2_inode *ip, int cluster_wide,
			 const char *function, char *file, unsigned int line)
{
	struct gfs2_sbd *sdp = ip->i_sbd;
	return gfs2_lm_withdraw(sdp,
			"GFS2: fsid=%s: fatal: filesystem consistency error\n"
			"GFS2: fsid=%s:   inode = %"PRIu64"/%"PRIu64"\n"
			"GFS2: fsid=%s:   function = %s\n"
			"GFS2: fsid=%s:   file = %s, line = %u\n"
			"GFS2: fsid=%s:   time = %lu\n",
			sdp->sd_fsname,
			sdp->sd_fsname,
			ip->i_num.no_formal_ino, ip->i_num.no_addr,
			sdp->sd_fsname, function,
			sdp->sd_fsname, file, line,
			sdp->sd_fsname, get_seconds());
}

/**
 * gfs2_consist_rgrpd_i - Flag a RG consistency error and withdraw
 * Returns: -1 if this call withdrew the machine,
 *          0 if it was already withdrawn
 */

int gfs2_consist_rgrpd_i(struct gfs2_rgrpd *rgd, int cluster_wide,
			 const char *function, char *file, unsigned int line)
{
	struct gfs2_sbd *sdp = rgd->rd_sbd;
	return gfs2_lm_withdraw(sdp,
			"GFS2: fsid=%s: fatal: filesystem consistency error\n"
			"GFS2: fsid=%s:   RG = %"PRIu64"\n"
			"GFS2: fsid=%s:   function = %s\n"
			"GFS2: fsid=%s:   file = %s, line = %u\n"
			"GFS2: fsid=%s:   time = %lu\n",
			sdp->sd_fsname,
			sdp->sd_fsname, rgd->rd_ri.ri_addr,
			sdp->sd_fsname, function,
			sdp->sd_fsname, file, line,
			sdp->sd_fsname, get_seconds());
}

/**
 * gfs2_meta_check_ii - Flag a magic number consistency error and withdraw
 * Returns: -1 if this call withdrew the machine,
 *          -2 if it was already withdrawn
 */

int gfs2_meta_check_ii(struct gfs2_sbd *sdp, struct buffer_head *bh,
		       const char *type, const char *function, char *file,
		       unsigned int line)
{
	int me;
	me = gfs2_lm_withdraw(sdp,
			     "GFS2: fsid=%s: fatal: invalid metadata block\n"
			     "GFS2: fsid=%s:   bh = %"PRIu64" (%s)\n"
			     "GFS2: fsid=%s:   function = %s\n"
			     "GFS2: fsid=%s:   file = %s, line = %u\n"
			     "GFS2: fsid=%s:   time = %lu\n",
			     sdp->sd_fsname,
			     sdp->sd_fsname, (uint64_t)bh->b_blocknr, type,
			     sdp->sd_fsname, function,
			     sdp->sd_fsname, file, line,
			     sdp->sd_fsname, get_seconds());
	return (me) ? -1 : -2;
}

/**
 * gfs2_metatype_check_ii - Flag a metadata type consistency error and withdraw
 * Returns: -1 if this call withdrew the machine,
 *          -2 if it was already withdrawn
 */

int gfs2_metatype_check_ii(struct gfs2_sbd *sdp, struct buffer_head *bh,
			   uint16_t type, uint16_t t, const char *function,
			   char *file, unsigned int line)
{
	int me;
	me = gfs2_lm_withdraw(sdp,
		"GFS2: fsid=%s: fatal: invalid metadata block\n"
		"GFS2: fsid=%s:   bh = %"PRIu64" (type: exp=%u, found=%u)\n"
		"GFS2: fsid=%s:   function = %s\n"
		"GFS2: fsid=%s:   file = %s, line = %u\n"
		"GFS2: fsid=%s:   time = %lu\n",
		sdp->sd_fsname,
		sdp->sd_fsname, (uint64_t)bh->b_blocknr, type, t,
		sdp->sd_fsname, function,
		sdp->sd_fsname, file, line,
		sdp->sd_fsname, get_seconds());
	return (me) ? -1 : -2;
}

/**
 * gfs2_io_error_i - Flag an I/O error and withdraw
 * Returns: -1 if this call withdrew the machine,
 *          0 if it was already withdrawn
 */

int gfs2_io_error_i(struct gfs2_sbd *sdp, const char *function, char *file,
		    unsigned int line)
{
	return gfs2_lm_withdraw(sdp,
			       "GFS2: fsid=%s: fatal: I/O error\n"
			       "GFS2: fsid=%s:   function = %s\n"
			       "GFS2: fsid=%s:   file = %s, line = %u\n"
			       "GFS2: fsid=%s:   time = %lu\n",
			       sdp->sd_fsname,
			       sdp->sd_fsname, function,
			       sdp->sd_fsname, file, line,
			       sdp->sd_fsname, get_seconds());
}

/**
 * gfs2_io_error_bh_i - Flag a buffer I/O error and withdraw
 * Returns: -1 if this call withdrew the machine,
 *          0 if it was already withdrawn
 */

int gfs2_io_error_bh_i(struct gfs2_sbd *sdp, struct buffer_head *bh,
		       const char *function, char *file, unsigned int line)
{
	return gfs2_lm_withdraw(sdp,
			       "GFS2: fsid=%s: fatal: I/O error\n"
			       "GFS2: fsid=%s:   block = %"PRIu64"\n"
			       "GFS2: fsid=%s:   function = %s\n"
			       "GFS2: fsid=%s:   file = %s, line = %u\n"
			       "GFS2: fsid=%s:   time = %lu\n",
			       sdp->sd_fsname,
			       sdp->sd_fsname, (uint64_t)bh->b_blocknr,
			       sdp->sd_fsname, function,
			       sdp->sd_fsname, file, line,
			       sdp->sd_fsname, get_seconds());
}

/**
 * gfs2_add_bh_to_ub - copy a buffer up to user space
 * @ub: the structure representing where to copy
 * @bh: the buffer
 *
 * Returns: errno
 */

int gfs2_add_bh_to_ub(struct gfs2_user_buffer *ub, struct buffer_head *bh)
{
	uint64_t blkno = bh->b_blocknr;

	if (ub->ub_count + sizeof(uint64_t) + bh->b_size > ub->ub_size)
		return -ENOMEM;

	if (copy_to_user(ub->ub_data + ub->ub_count,
			  &blkno,
			  sizeof(uint64_t)))
		return -EFAULT;
	ub->ub_count += sizeof(uint64_t);

	if (copy_to_user(ub->ub_data + ub->ub_count,
			  bh->b_data,
			  bh->b_size))
		return -EFAULT;
	ub->ub_count += bh->b_size;

	return 0;
}

int gfs2_printf_i(char *buf, unsigned int size, unsigned int *count,
		  char *fmt, ...)
{
	va_list args;
	int left, out;

	if (!buf) {
		va_start(args, fmt);
		vprintk(fmt, args);
		va_end(args);
		return 0;
	}

	left = size - *count;
	if (left <= 0)
		return 1;

	va_start(args, fmt);
	out = vsnprintf(buf + *count, left, fmt, args);
	va_end(args);

	if (out < left)
		*count += out;
	else
		return 1;

	return 0;
}

void gfs2_icbit_munge(struct gfs2_sbd *sdp, unsigned char **bitmap,
		      unsigned int bit, int new_value)
{
	unsigned int c, o, b = bit;
	int old_value;

	c = b / (8 * PAGE_SIZE);
	b %= 8 * PAGE_SIZE;
	o = b / 8;
	b %= 8;

	old_value = (bitmap[c][o] & (1 << b));
	gfs2_assert_withdraw(sdp, !old_value != !new_value);

	if (new_value)
		bitmap[c][o] |= 1 << b;
	else
		bitmap[c][o] &= ~(1 << b);
}

