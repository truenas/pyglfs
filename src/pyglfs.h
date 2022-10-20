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
	py_glfs_t *py_fs;
	PyObject *py_st;
	glfs_object_t *gl_obj;
} py_glfs_obj_t;

typedef struct {
	PyObject_HEAD
	glfs_fd_t *fd;
	py_glfs_obj_t *parent;
	int flags;
} py_glfs_fd_t;

extern PyTypeObject PyGlfsObject;
extern PyTypeObject PyGlfsVolume;
extern PyTypeObject PyGlfsFd;

extern void set_exc_from_errno(const char *func);
extern PyTypeObject *init_pystat_type(void);
extern PyObject *stat_to_pystat(struct stat *st);

extern bool init_glfd(void);
extern PyObject *init_glfs_object(py_glfs_t *py_fs, glfs_object_t *gl_obj, struct stat *pst);
extern PyObject *init_glfs_fd(glfs_fd_t *fd_in, py_glfs_obj_t *hdl, int flags);
#endif
