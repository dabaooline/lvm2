/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2009 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tools.h"

static int _lvmetad;

static int vgscan_single(struct cmd_context *cmd, const char *vg_name,
			 struct volume_group *vg,
			 void *handle __attribute__((unused)))
{
	log_print_unless_silent("Found %svolume group \"%s\" using metadata type %s",
				vg_is_exported(vg) ? "exported " : "", vg_name,
				vg->fid->fmt->name);

	check_current_backup(vg);

	/* keep lvmetad up to date, restore the "active" state temporarily */
	lvmetad_set_active(_lvmetad);
	if (!lvmetad_vg_update(vg))
		stack;
	lvmetad_set_active(0);

	return ECMD_PROCESSED;
}

int vgscan(struct cmd_context *cmd, int argc, char **argv)
{
	int maxret, ret;

	if (argc) {
		log_error("Too many parameters on command line");
		return EINVALID_CMD_LINE;
	}

	if (!lock_vol(cmd, VG_GLOBAL, LCK_VG_WRITE)) {
		log_error("Unable to obtain global lock.");
		return ECMD_FAILED;
	}

	persistent_filter_wipe(cmd->filter);
	lvmcache_destroy(cmd, 1);

	_lvmetad = lvmetad_active();
	if (arg_count(cmd, cache_ARG)) {
		if (_lvmetad)
			lvmetad_set_active(0); /* do not rely on lvmetad info */
		else {
			log_error("Cannot proceed since lvmetad is not active.");
			unlock_vg(cmd, VG_GLOBAL);
			return ECMD_FAILED;
		}
	}

	log_print_unless_silent("Reading all physical volumes.  This may take a while...");

	maxret = process_each_vg(cmd, argc, argv, 0, NULL,
				 &vgscan_single);

	if (arg_count(cmd, mknodes_ARG)) {
		ret = vgmknodes(cmd, argc, argv);
		if (ret > maxret)
			maxret = ret;
	}

	lvmetad_set_active(_lvmetad); /* restore */
	unlock_vg(cmd, VG_GLOBAL);
	return maxret;
}
