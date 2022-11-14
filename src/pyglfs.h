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

#ifndef _PYGLFS_H_
#define _PYGLFS_H_

#include <Python.h>

#define Py_TPFLAGS_HAVE_ITER 0

typedef struct glfs_volfile_server {
	char host[PATH_MAX]; /* hosts or local path */
	char proto[5]; /* TCP, UNIX, RDMA */
	int port;
} glfs_volfile_server_t;

typedef struct {
	PyObject_HEAD
	PyObject *xlators;
	glfs_t *fs;
	char name[NAME_MAX + 1]; /* GD_VOLUME_NAME_MAX */;
	glfs_volfile_server_t *volfile_servers;
	size_t srv_cnt;
	char log_file[PATH_MAX];
	char vol_id[39]; /* GF_UUID_BUF_SIZE + 1 */
	int log_level;
} py_glfs_t;

typedef struct {
	PyObject_HEAD
	PyObject *name;
	py_glfs_t *py_fs;
	struct stat st;
	char uuid_str[37];
	glfs_object_t *gl_obj;
} py_glfs_obj_t;

typedef struct {
	PyObject_HEAD
	glfs_fd_t *fd;
	py_glfs_obj_t *parent;
	int flags;
} py_glfs_fd_t;

/*
 * do_stat, fn, and state may be set by
 * user of iterator, but _prev_dirent only
 * provides consistent buffer for internal
 * call. Saved thread state should only be
 * manipulated via macros defined below.
 */
typedef struct iter_dir {
	glfs_fd_t *fd;
	glfs_object_t *obj;
	struct dirent _dir;
	char *abspath;
	size_t depth;
	struct iter_dir *next;
} iter_dir_t;

struct iter_children {
	size_t sz;
	iter_dir_t *next;
};

typedef struct glfs_object_cb {
	PyThreadState *_save;
	int flags;
	int max_depth;
	iter_dir_t root;
	struct iter_children children;
	void *state;
	bool (*fn)(py_glfs_obj_t *root,
		   glfs_object_t *obj,
		   struct dirent *dir,
		   struct stat *st,
		   size_t depth,
		   const char *parent_path,
		   void *private);
} glfs_object_cb_t;

#define PYGLFS_FTS_FLAG_DO_CHDIR	0x01
#define PYGLFS_FTS_FLAG_DO_STAT		0x02
#define PYGLFS_FTS_FLAG_DO_RECURSE	0x04
#define FTS_FLAGS PYGLFS_FTS_FLAG_DO_CHDIR \
	| PYGLFS_FTS_FLAG_DO_STAT \
	| PYGLFS_FTS_FLAG_DO_RECURSE

extern PyTypeObject PyGlfsObject;
extern PyTypeObject PyGlfsVolume;
extern PyTypeObject PyGlfsObject;
extern PyTypeObject PyGlfsVolume;
extern PyTypeObject PyGlfsFd;
extern PyTypeObject PyGlfsObjectIter;
extern PyTypeObject PyGlfsFTS;
extern PyTypeObject PyGlfsFTSENT;

extern void _set_glfs_exc(const char *additional_info, const char *location);
#define set_glfs_exc(additional_info) _set_glfs_exc(additional_info, __location__)

extern void set_exc_from_errno(const char *func);
extern bool init_pystat_type(void);
extern PyObject *stat_to_pystat(struct stat *st);
extern PyObject *py_file_type_str(mode_t mode);

extern int iter_glfs_object_handle(py_glfs_obj_t *root, glfs_object_cb_t *cb);
extern bool iter_cb_cleanup(glfs_object_cb_t *cb);

extern bool init_glfd(void);
extern PyObject *init_glfs_object(py_glfs_t *, glfs_object_t *, const struct stat *, const char *);
extern PyObject *init_glfs_fd(glfs_fd_t *fd_in, py_glfs_obj_t *hdl, int flags);

/*
 * Macros to take / release GIL in iterator
 * This allows us to re-take GIL inside the callback function if needed.
 */
#define OBJ_ITER_ALLOW_THREADS(cb) { cb->_save = PyEval_SaveThread(); }
#define OBJ_ITER_END_ALLOW_THREADS(cb) { PyEval_RestoreThread(cb->_save); }
#endif
