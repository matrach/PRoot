/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2010, 2011, 2012 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <sys/types.h> /* mkdir(2), */
#include <sys/stat.h> /* mkdir(2), */
#include <fcntl.h>    /* mknod(2), */
#include <unistd.h>   /* mknod(2), */
#include <stdlib.h>   /* mkdtemp(3), */
#include <string.h>   /* string(3),  */
#include <assert.h>   /* assert(3), */
#include <limits.h>   /* PATH_MAX, */
#include <errno.h>    /* errno, E* */
#include <talloc.h>   /* talloc_*, */

#include "path/binding.h"
#include "path/path.h"
#include "notice.h"

#include "compat.h"

/**
 * Delete only empty files and directories from the glue: the files
 * created by the user inside this glue are left.
 *
 * Note: this is a Talloc desctructor.
 */
static int remove_glue(char *path)
{
	char *command;

	command = talloc_asprintf(NULL, "find '%s' -delete 2>/dev/null", path);
	if (command != NULL) {
		int status;

		status = system(command);
		if (status != 0)
			notice(NULL, INFO, USER, "can't delete '%s'", path);
	}

	TALLOC_FREE(command);

	return 0;
}

/**
 * Build in a temporary filesystem the glue between the guest part and
 * the host part of the @binding_path.  This function returns the type
 * of the bound path, otherwise 0 if an error occured.
 *
 * For example, assuming the host path "/opt" is mounted/bound to the
 * guest path "/black/holes/and/revelations", and assuming this path
 * can't be created in the guest rootfs (eg. permission denied), then
 * it is created in a temporary rootfs and all these paths are glued
 * that way:
 *
 *   $GUEST/black/ --> $GLUE/black/
 *                               ./holes
 *                               ./holes/and
 *                               ./holes/and/revelations --> $HOST/opt/
 *
 * This glue allows operations on paths that do not exist in the guest
 * rootfs but that were specified as the guest part of a binding.
 */
mode_t build_glue(Tracee *tracee, const char *guest_path, char host_path[PATH_MAX], Finality is_final)
{
	Comparison comparison;
	Binding *binding;
	mode_t type;
	int status;

	assert(tracee->glue_type != 0);

	/* Create the temporary directory where the "glue" rootfs will
	 * lie.  */
	if (tracee->glue == NULL) {
		tracee->glue = talloc_asprintf(tracee, "/tmp/proot-%d-XXXXXX", getpid());
		if (tracee->glue == NULL)
			return 0;
		talloc_set_name_const(tracee->glue, "$glue");

		if (mkdtemp(tracee->glue) == NULL) {
			TALLOC_FREE(tracee->glue);
			return 0;
		}

		talloc_set_destructor(tracee->glue, remove_glue);
	}

	/* If it's not a final component then it is a directory.  I definitively
	 * hate how the protential type of the final component is propagated
	 * from initialize_binding() down to here, sadly there's no elegant way
	 * to know its type at this stage.  */
	if (is_final) {
		struct stat statl;

		/* Trust glue_type only if the destination doesn't exist.  */
		status = lstat(host_path, &statl);
		if (status < 0)
			type = tracee->glue_type;
		else
			type = (statl.st_mode & S_IFMT);
	}
	else
		type = S_IFDIR;

	/* Attempt to create new paths only in gluefs. */
	comparison = compare_paths(tracee->glue, host_path);
	if (   comparison == PATHS_ARE_EQUAL
	    || comparison == PATH1_IS_PREFIX) {

		notice(tracee, INFO, INTERNAL, "creating a glue bind for %s -> %s",
			host_path, guest_path);
		/* Try to create this component into the "glue" rootfs. */
		if (S_ISDIR(type))
			status = mkdir(host_path, 0700);
		else
			status = mknod(host_path, 0700 | type, 0);

		/* mkdir/mknod are supposed to always succeed in
		 * tracee->glue.  */
		if (status < 0 && errno != EEXIST) {
			notice(tracee, WARNING, SYSTEM, "mkdir/mknod");
			return 0;
		} else
			return type;
	}

	/* Nothing else to do if the path already exist or it is
	 * the final component it will be pointed to by the
	 * binding being initialized (from the example,
	 * "$GUEST/black/holes/and/revelations" -> "$HOST/opt").  */
	if (is_final || access(host_path, F_OK) != -1)
		return type;

	/* From the example, create the binding "/black" ->
	 * "$GLUE/black".  */
	binding = talloc_zero(tracee->glue, Binding);
	if (binding == NULL)
		return 0;

	strcpy(binding->host.path, tracee->glue);
	strcpy(binding->guest.path, guest_path);

	binding->host.length = strlen(binding->host.path);
	binding->guest.length = strlen(binding->guest.path);

	insort_binding2(tracee, binding);

	/* TODO: emulation of getdents(parent(guest_path)) to finalize
	 * the glue, "black" in getdents("/") from the example.  */

	return type;
}
