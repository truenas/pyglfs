/*
 * Python language bindings for libgfapi
 *
 * Copyright (C) Andrew Walker, 2022
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <Python.h>
#include "includes.h"
#include "pyglfs.h"

static bool remove_last(struct iter_children *list)
{
	iter_dir_t *target = list->next;

	if (target == NULL) {
		return true;
	}

	list->next = target->next;
	glfs_closedir(target->fd);
	glfs_h_close(target->obj);
	free(target->abspath);
	free(target);
	list->sz--;
	return true;
}

static bool add_child(struct iter_children *list,
		      glfs_object_t *obj,
		      glfs_t *fs,
		      struct dirent *entry)
{
	iter_dir_t *child = NULL;
	int rv;

	child = calloc(1, sizeof(iter_dir_t));
	if (child == NULL) {
		return false;
	}

	/*
	 * obj passed in will be temporary one based on xstat
	 * this means that we need a new object for opening the
	 * fd so that it persists after temporary one is closed
	 */
	child->obj = glfs_object_copy(obj);
	if (child->obj == NULL) {
		free(child);
		return false;
	}

	child->fd = glfs_h_opendir(fs, child->obj);
	if (child->fd == NULL) {
		glfs_h_close(child->obj);
		free(child);
		return false;
	}

	if (list->next == NULL) {
		list->next = child;
	} else {
		child->next = list->next;
		list->next = child;
	}
	list->sz++;
	child->depth = list->sz;
	/*
	 * Store string for current approximation of absolute
	 * path. We are not using path-based calls here, and so
	 * this is a convenience feature.
	 */
	rv = asprintf(
		&child->abspath,
		"%s/%s",
		child->next ? child->next->abspath : ".",
		entry->d_name
	);
	if (rv == -1) {
		/*
		 * memory allocation failure. remove this entry
		 */
		child->abspath = NULL;
		remove_last(list);
		return false;
	}

        return true;
}

bool iter_cb_cleanup(glfs_object_cb_t *cb)
{
	while (cb->children.next != NULL) {
		remove_last(&cb->children);
	}

        glfs_closedir(cb->root.fd);
	return true;
}

/*
 * This is general purpose iterator for directory handles.
 *
 * Currently configurable features are:
 * - recursion depth (via `max_depth` in glfs_object_cb_t)
 * - whether to provide stat info to callback function
 *
 * Callback function receives the following arguments:
 * - py_glfs_obj_t *root (the original target of iteration)
 * - glfs_object_t *tmp_obj (temporary glfs object handle)
 *   NOTE: this will be freed after callback function is called.
 *         hence, onus is on cb to make a copy if needed long-term.
 * - struct dirent *entry (temporary dirent struct for file).
 * - struct stat *st (temporary stat struct) -- see caveat above
 * - size_t depth current recursion depth
 * - const char *parent_path (temporary path of parent directory)
 * - void *private (private data passed in / out of function)
 *
 * Returns
 * -3 if failed to open child directory during recursive op
 * -2 if callback function returned false (breaking iteration)
 * -1 if glfs_xreaddirplus_r() failed
 * 0 if no more entries
 * > 0 if success and not EOF
 */
int iter_glfs_object_handle(py_glfs_obj_t *root, glfs_object_cb_t *cb)
{
	glfs_object_t *tmp = NULL; /* gets freed via glfs_free(self->xstat_p) */
	struct stat *st = NULL;
	glfs_xreaddirp_stat_t *xstat_p = NULL;
	int flags = GFAPI_XREADDIRP_HANDLE;
	int rv;
	struct dirent *entry = NULL;

	if (cb->flags & PYGLFS_FTS_FLAG_DO_STAT) {
		flags |= GFAPI_XREADDIRP_STAT;
	}

	for (;;) {
		bool ok;
		iter_dir_t *target = NULL;

		if (cb->children.sz) {
			target = cb->children.next;
		} else {
			target = &cb->root;
		}

		rv = glfs_xreaddirplus_r(
			target->fd,
			flags,
			&xstat_p,
			&target->_dir,
			&entry
		);

		if (rv == -1) {
			break;
		}

		if (entry == NULL) {
			if (cb->children.next == NULL) {
				break;
			}
			remove_last(&cb->children);
			entry = NULL;
			continue;
		}

		if (entry == NULL) {
			break;
		}

		/*
		 * Object handles can't be created for
		 * DOT and DOTDOT
		 */
		if ((strcmp(entry->d_name, ".") == 0) ||
		    (strcmp(entry->d_name, "..") == 0)) {
			glfs_free(xstat_p);
			xstat_p = NULL;
			continue;
		}

		tmp = glfs_xreaddirplus_get_object(xstat_p);
		st = glfs_xreaddirplus_get_stat(xstat_p);

		/*
		 * for case of recursive ops, add
		 * the dir to our child list. Next iteration
		 * will start to readdir from that directory
		 */
		if ((entry->d_type == DT_DIR) &&
		    (cb->flags & PYGLFS_FTS_FLAG_DO_RECURSE) &&
		    (cb->max_depth != (int)cb->children.sz)) {
			if (!add_child(&cb->children, tmp, root->py_fs->fs, entry)) {
				rv = -3;
			}
		}

		ok = cb->fn(
			root, tmp, entry, st,
			target->depth,
		        target->abspath,
			cb->state
		);
		glfs_free(xstat_p);
		if (!ok) {
			rv = -2;
			break;
		}
	}

	if (entry == NULL) {
		rv = 0;
	}

	return rv;
}
