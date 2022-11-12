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

static PyObject *py_glfs_obj_new(PyTypeObject *obj,
				 PyObject *args_unused,
				 PyObject *kwargs_unused)
{
	py_glfs_obj_t *self = NULL;

	self = (py_glfs_obj_t *)obj->tp_alloc(obj, 0);
	if (self == NULL) {
		return NULL;
	}
	return (PyObject *)self;
}

static int py_glfs_obj_init(PyObject *obj,
			    PyObject *args,
			    PyObject *kwargs)
{
	return 0;
}

void py_glfs_obj_dealloc(py_glfs_obj_t *self)
{
	if (self->gl_obj) {
		if (glfs_h_close(self->gl_obj) == -1) {
			fprintf(stderr, "glfs_h_close() failed: %s",
				strerror(errno));
		}
		self->gl_obj = NULL;
	}
	Py_CLEAR(self->py_fs);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyObject *init_glfs_object(py_glfs_t *py_fs,
			   glfs_object_t *gl_obj,
			   const struct stat *pst,
			   const char *name)
{
	py_glfs_obj_t *hdl = NULL;
	uuid_t ui;
	ssize_t rv;

	hdl = (py_glfs_obj_t *)PyObject_CallNoArgs((PyObject *)&PyGlfsObject);
	if (hdl == NULL) {
		return NULL;
	}

	if (pst != NULL) {
		memcpy(&hdl->st, pst, sizeof(struct stat));
	}

	Py_BEGIN_ALLOW_THREADS
	rv = glfs_h_extract_handle(gl_obj, ui, sizeof(ui));
	Py_END_ALLOW_THREADS

	if (rv == -1) {
		set_glfs_exc("glfs_h_extract_handle()");
		Py_DECREF(hdl);
		return NULL;
	}

	if (name != NULL) {
		hdl->name = PyUnicode_FromString(name);
	}
	uuid_unparse(ui, hdl->uuid_str);
	hdl->py_fs = py_fs;
	Py_INCREF(hdl->py_fs);
	hdl->gl_obj = gl_obj;
	return (PyObject *)hdl;
}

PyObject *py_file_type_str(mode_t mode)
{
	const char *file_type_str = NULL;

	switch (mode & S_IFMT) {
	case S_IFDIR:
		file_type_str = "DIRECTORY";
		break;
	case S_IFREG:
		file_type_str = "FILE";
		break;
	case S_IFLNK:
		file_type_str = "SYMLINK";
		break;
	case S_IFIFO:
		file_type_str = "FIFO";
		break;
	case S_IFSOCK:
		file_type_str = "SOCKET";
		break;
	case S_IFCHR:
		file_type_str = "CHAR";
		break;
	case S_IFBLK:
		file_type_str = "BLOCK";
		break;
	default:
		file_type_str = "UNKNOWN";
	};

	return PyUnicode_FromString(file_type_str);
}

PyDoc_STRVAR(py_glfs_obj_lookup__doc__,
"lookup(path, stat=False, symlink_follow=False)\n"
"--\n\n"
"Lookup existing GLFS object by path.\n\n"
"Parameters\n"
"----------\n"
"path : str\n"
"    Path of object relative to this handle.\n"
"stat : bool, optional, default=True\n"
"    Retrieve stat information for object while performing lookup.\n"
"symlink_follow: bool, optional, default=True\n"
"    Follow symlinks while performing lookup.\n\n"
"Returns\n"
"-------\n"
"pyglfs.ObjectHandle\n"
"    New GLFS handle\n"
);

static PyObject *py_glfs_obj_lookup(PyObject *obj, PyObject *args, PyObject *kwargs)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	glfs_object_t *gl_obj = NULL;
	char *path = NULL;
	struct stat st;
	bool do_stat = true, follow = true;
	const char *kwnames [] = { "path", "stat", "symlink_follow", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|bb",
					 discard_const_p(char *, kwnames),
					 &path,
					 &do_stat, &follow)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	gl_obj = glfs_h_lookupat(
		self->py_fs->fs,
		self->gl_obj,
		path,
		do_stat ? &st : NULL,
		follow
	);
	Py_END_ALLOW_THREADS

	if (gl_obj == NULL) {
		set_glfs_exc("glfs_h_lookupat()");
		return NULL;
	}

	return init_glfs_object(self->py_fs, gl_obj, do_stat ? &st : NULL, path);
}

PyDoc_STRVAR(py_glfs_obj_create__doc__,
"create(path, flags, stat=False, symlink_follow=False, mode=0o644)\n"
"--\n\n"
"Create new GLFS object (file) by path.\n\n"
"Parameters\n"
"----------\n"
"path : str\n"
"    Path of object relative to this handle.\n"
"flags : int\n"
"    open(2) flags to use to open the handle.\n"
"stat : bool, optional, default=True\n"
"    Retrieve stat information for object while performing create.\n"
"symlink_follow: bool, optional, default=True\n"
"    Follow symlinks while performing create.\n\n"
"mode : bool, optional, default=0o644\n"
"    Permissions to set on newly created directory.\n\n"
"Returns\n"
"-------\n"
"pyglfs.ObjectHandle\n"
"    New GLFS handle\n"
);

static PyObject *py_glfs_obj_create(PyObject *obj, PyObject *args, PyObject *kwargs)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	glfs_object_t *gl_obj = NULL;
	char *path = NULL;
	struct stat st;
	bool do_stat = true;
	int flags = 0;
	int mode = 420; /* 0o644 */
	const char *kwnames [] = { "path", "flags", "stat", "mode", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "si|bi",
					 discard_const_p(char *, kwnames),
					 &path, &flags,
					 &do_stat, &mode)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	gl_obj = glfs_h_creat(
		self->py_fs->fs,
		self->gl_obj,
		path,
		flags,
		mode,
		do_stat ? &st : NULL
	);
	Py_END_ALLOW_THREADS

	if (gl_obj == NULL) {
		set_glfs_exc("glfs_h_creat()");
		return NULL;
	}

	return init_glfs_object(self->py_fs, gl_obj, do_stat ? &st : NULL, path);
}

PyDoc_STRVAR(py_glfs_obj_mkdir__doc__,
"mkdir(path, stat=False, mode=0o755)\n"
"--\n\n"
"Create new GLFS object (directory) by path.\n\n"
"Parameters\n"
"----------\n"
"path : str\n"
"    Path of object relative to this handle.\n"
"flags : int\n"
"    open(2) flags to use to open the handle.\n"
"stat : bool, optional, default=True\n"
"    Retrieve stat information for object while performing create.\n"
"mode : bool, optional, default=0o755\n"
"    Permissions to set on newly created directory.\n\n"
"Returns\n"
"-------\n"
"pyglfs.ObjectHandle\n"
"    New GLFS handle\n"
);

static PyObject *py_glfs_obj_mkdir(PyObject *obj, PyObject *args, PyObject *kwargs)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	glfs_object_t *gl_obj = NULL;
	char *path = NULL;
	struct stat st;
	bool do_stat = true;
	int mode = 493; /* 0o755 */
	const char *kwnames [] = { "path", "stat", "mode", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|bi",
					 discard_const_p(char *, kwnames),
					 &path, &do_stat, &mode)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	gl_obj = glfs_h_mkdir(
		self->py_fs->fs,
		self->gl_obj,
		path,
		mode,
		do_stat ? &st : NULL
	);
	Py_END_ALLOW_THREADS

	if (gl_obj == NULL) {
		set_glfs_exc("glfs_h_mkdir()");
		return NULL;
	}

	return init_glfs_object(self->py_fs, gl_obj, do_stat ? &st : NULL, path);
}

PyDoc_STRVAR(py_glfs_obj_unlink__doc__,
"unlink(path)\n"
"--\n\n"
"Unlink (delete) path under GLFS object.\n\n"
"Parameters\n"
"----------\n"
"path : str\n"
"    Path relative to this handle.\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_obj_unlink(PyObject *obj,
				    PyObject *args,
				    PyObject *kwargs_unused)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	char *path = NULL;
	int err;

	if (!PyArg_ParseTuple(args, "s", &path)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	err = glfs_h_unlink(self->py_fs->fs, self->gl_obj, path);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_h_unlink()");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_glfs_obj_stat__doc__,
"stat()\n"
"--\n\n"
"Stat the glfs object. Performs fresh stat and updates\n"
"cache for object.\n\n"
"Parameters\n"
"----------\n"
"None\n\n"
"Returns\n"
"-------\n"
"stat_result\n"
);

static PyObject *py_glfs_obj_stat(PyObject *obj,
				  PyObject *args_unused,
				  PyObject *kwargs_unused)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	struct stat st;
	int err;

	Py_BEGIN_ALLOW_THREADS
	err = glfs_h_stat(self->py_fs->fs, self->gl_obj, &st);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_h_stat()");
		return NULL;
	}

	memcpy(&self->st, &st, sizeof(struct stat));
	return stat_to_pystat(&st);
}

PyDoc_STRVAR(py_glfs_obj_open__doc__,
"open(flags)\n"
"--\n\n"
"Open a GLFS file descriptor.\n\n"
"Parameters\n"
"----------\n"
"flags : int\n"
"    open(2) flags to use to open the handle. O_CREAT is not supported.\n\n"
"Returns\n"
"-------\n"
"pyglfs.FD\n"
"    GLFS file descriptor object\n"
);
static PyObject *py_glfs_obj_open(PyObject *obj,
				  PyObject *args,
				  PyObject *kwargs_unused)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	glfs_fd_t *gl_fd = NULL;
	int flags;

	if (!PyArg_ParseTuple(args, "i", &flags)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	if (flags & O_DIRECTORY) {
		gl_fd = glfs_h_opendir(self->py_fs->fs, self->gl_obj);
	} else {
		gl_fd = glfs_h_open(self->py_fs->fs, self->gl_obj, flags);
	}
	Py_END_ALLOW_THREADS

	if (gl_fd == NULL) {
		set_glfs_exc("glfs_h_open()");
		return NULL;
	}

	return init_glfs_fd(gl_fd, self, flags);
}

PyDoc_STRVAR(py_glfs_obj_contents__doc__,
"contents()\n"
"--\n\n"
"Read handle contents. Return will vary depending on underlying\n"
"file type.\n"
"FILE - contents of file.\n"
"DIRECTORY - list of directory entry names.\n"
"SYMLINK - destination of symlink\n\n"
"Parameters\n"
"----------\n"
"flags : int\n"
"    open(2) flags to use to open the handle. O_CREAT is not supported.\n\n"
"Returns\n"
"-------\n"
"contents : bytes (FILE), list (DIRECTORY), str (SYMLINK)\n"
);

static PyObject *read_contents_reg(py_glfs_obj_t *self)
{
	glfs_fd_t *gl_fd = NULL;
	PyObject *pyfd = NULL;
	PyObject *data = NULL;


	// short-circuit if size is 0.
	if (self->st.st_size == 0) {
		return PyBytes_FromString("");
	}

	Py_BEGIN_ALLOW_THREADS
	gl_fd = glfs_h_open(self->py_fs->fs, self->gl_obj, O_RDONLY);
	Py_END_ALLOW_THREADS

	if (gl_fd == NULL) {
		set_glfs_exc("glfs_h_open()");
		return NULL;
	}

	pyfd = init_glfs_fd(gl_fd, self, O_RDONLY);
	if (pyfd == NULL) {
		return NULL;
	}

	data = PyObject_CallMethod(
		pyfd,
		"pread",
		"Ln",
		0,
		self->st.st_size
	);

	// dealloc of pyfd closes gl_fd
	Py_DECREF(pyfd);

	return data;
}

typedef char dstring[256];

struct d_array {
	dstring *dirs;
	size_t next_idx;
	size_t alloc;
};

static bool get_dir_listing(glfs_fd_t *fd, struct d_array *arr)
{
	struct dirent prev, *result = NULL;
	int err;

	*arr = (struct d_array) {
		.dirs = NULL,
		.next_idx = 0,
		.alloc = 16
	};

	arr->dirs = calloc(arr->alloc, sizeof(dstring));
	if (arr->dirs == NULL) {
		return false;
	}

	for (;;) {
		err = glfs_readdir_r(fd, &prev, &result);
		if (err) {
			free(arr->dirs);
			arr->dirs = NULL;
			return false;
		}
		if (result == NULL) {
			break;
		}

		if (strcmp(result->d_name, ".") == 0 ||
		    (strcmp(result->d_name, "..") == 0)) {
			continue;
		}

		strlcpy(arr->dirs[arr->next_idx], result->d_name, sizeof(dstring));
		arr->next_idx++;
		if (arr->next_idx == arr->alloc) {
			dstring *new_dirs = NULL;
			arr->alloc *= 8;
			new_dirs = realloc(arr->dirs, sizeof(dstring) * arr->alloc);
			if (new_dirs == NULL) {
				free(arr->dirs);
				arr->dirs = NULL;
				return false;
			}

			arr->dirs = new_dirs;
		}
	}

	return true;
}

static PyObject *read_contents_dir(py_glfs_obj_t *self)
{
	PyObject *data = NULL;
	struct d_array arr;
	glfs_fd_t *gl_fd = NULL;
	bool ok;
	size_t i;

	Py_BEGIN_ALLOW_THREADS
	gl_fd = glfs_h_opendir(self->py_fs->fs, self->gl_obj);
	Py_END_ALLOW_THREADS
	if (gl_fd == NULL) {
		set_glfs_exc("glfs_h_opendir()");
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS
	ok = get_dir_listing(gl_fd, &arr);
	glfs_closedir(gl_fd);
	Py_END_ALLOW_THREADS
	if (!ok) {
		set_glfs_exc("glfs_h_readdir()");
		return NULL;
	}

	data = PyList_New(arr.next_idx);
	if (data == NULL) {
		return NULL;
	}

	for (i = 0; i < arr.next_idx; i++) {
		PyObject *pydir = NULL;
		pydir = PyUnicode_FromString(arr.dirs[i]);
		if (pydir == NULL) {
			Py_CLEAR(data);
			break;
		}

		if (PyList_SetItem(data, i, pydir) == -1) {
			Py_CLEAR(data);
			break;
		}
	}
	free(arr.dirs);
	return data;
}

static PyObject *read_contents_lnk(py_glfs_obj_t *self)
{
	/* readlink does not NULL-terminate after copying */
	char path[PATH_MAX + 1] = { 0 };
	int err;

	Py_BEGIN_ALLOW_THREADS
	err = glfs_h_readlink(self->py_fs->fs, self->gl_obj, path, PATH_MAX);
	Py_END_ALLOW_THREADS

	if (err == -1) {
		set_glfs_exc("glfs_h_readlink()");
		return NULL;
	}

	return PyUnicode_FromString(path);
}

static PyObject *py_glfs_obj_contents(PyObject *obj,
				      PyObject *args_unused,
				      PyObject *kwargs_unused)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	PyObject *rv = NULL;

	switch (self->st.st_mode & S_IFMT) {
	case S_IFDIR:
		rv = read_contents_dir(self);
		break;
	case S_IFREG:
		rv = read_contents_reg(self);
		break;
	case S_IFLNK:
		rv = read_contents_lnk(self);
		break;
	default:
		PyErr_SetString(
			PyExc_NotImplementedError,
			"contents() not implemented for file type"
                );
	}

	return rv;
}

PyDoc_STRVAR(py_glfs_obj_fts_open__doc__,
"fts_open(stat=true, max_depth=-1)\n"
"--\n\n"
"Open glfs.FTSHandle for directory iteration.\n\n"
"Parameters\n"
"----------\n"
"stat : bool, optional, default=True\n"
"    Retrieve stat information for object while iterating.\n"
"max_depth: int, optional, default=-1\n"
"    Maximum recursion depth for iteration.\n"
"    Defaults to -1 (no limit)\n\n"
"Returns\n"
"    Open glfs.FTSHandle\n"
);
static PyObject *py_glfs_obj_fts_open(PyObject *obj,
				      PyObject *args,
				      PyObject *kwargs)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	int flags = PYGLFS_FTS_FLAG_DO_RECURSE;
	int max_depth = -1;
	bool do_stat = true;

	const char *kwnames [] = {
		"stat",
		"max_depth",
		NULL
	};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs,
					 "|pi",
					 discard_const_p(char *, kwnames),
					 &do_stat,
					 &max_depth)) {
		return NULL;
	}

	if (do_stat) {
		flags |= PYGLFS_FTS_FLAG_DO_STAT;
	}

	return PyObject_CallFunction(
		(PyObject *)&PyGlfsFTS,
		"Oii",
		self, flags, max_depth
	);
}

static PyMethodDef py_glfs_obj_methods[] = {
	{
		.ml_name = "lookup",
		.ml_meth = (PyCFunction)py_glfs_obj_lookup,
		.ml_flags = METH_VARARGS | METH_KEYWORDS,
		.ml_doc = py_glfs_obj_lookup__doc__
	},
	{
		.ml_name = "create",
		.ml_meth = (PyCFunction)py_glfs_obj_create,
		.ml_flags = METH_VARARGS | METH_KEYWORDS,
		.ml_doc = py_glfs_obj_create__doc__
	},
	{
		.ml_name = "mkdir",
		.ml_meth = (PyCFunction)py_glfs_obj_mkdir,
		.ml_flags = METH_VARARGS | METH_KEYWORDS,
		.ml_doc = py_glfs_obj_mkdir__doc__
	},
	{
		.ml_name = "unlink",
		.ml_meth = (PyCFunction)py_glfs_obj_unlink,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_obj_unlink__doc__
	},
	{
		.ml_name = "stat",
		.ml_meth = (PyCFunction)py_glfs_obj_stat,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_obj_stat__doc__
	},
	{
		.ml_name = "open",
		.ml_meth = (PyCFunction)py_glfs_obj_open,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_obj_open__doc__
	},
	{
		.ml_name = "fts_open",
		.ml_meth = (PyCFunction)py_glfs_obj_fts_open,
		.ml_flags = METH_VARARGS | METH_KEYWORDS,
		.ml_doc = py_glfs_obj_fts_open__doc__
	},
	{
		.ml_name = "contents",
		.ml_meth = (PyCFunction)py_glfs_obj_contents,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_obj_contents__doc__
	},
	{ NULL, NULL, 0, NULL }
};

static PyObject *py_glfs_obj_get_uuid(PyObject *obj, void *closure)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;

	return Py_BuildValue("s", self->uuid_str);
}

static PyObject *py_glfs_obj_get_stat(PyObject *obj, void *closure)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;

	if (self->st.st_dev == 0) {
		/* stat struct is not populated */
		Py_RETURN_NONE;
	}

	return stat_to_pystat(&self->st);
}

static PyObject *py_glfs_obj_get_file_type(PyObject *obj, void *closure)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	PyObject *file_type_str = NULL;

	if (self->st.st_dev == 0) {
		/* stat struct is not populated */
		Py_RETURN_NONE;
	}

	file_type_str = py_file_type_str(self->st.st_mode);
	if (file_type_str == NULL) {
		return NULL;
	}

	return Py_BuildValue(
		"{s:l,s:N}",
		"raw", self->st.st_mode & S_IFMT,
		"parsed", file_type_str
	);
}

static PyObject *py_glfs_obj_get_name(PyObject *obj, void *closure)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;

	if (self->name == NULL) {
		Py_RETURN_NONE;
	}

	Py_INCREF(self->name);
	return self->name;
}


PyDoc_STRVAR(py_glfs_obj_get_stat__doc__,
"Cached stat information. This may be auto-populated depending on\n"
"how the GLFS object was created or refreshed via arguments passed\n"
"to operation on the handle.\n"
);


PyDoc_STRVAR(py_glfs_obj_get_uuid__doc__,
"UUID for GLFS object handle.\n"
"This is extracted when the python object was created, and may be used\n"
"to open the file via the open_by_uuid() method for the glfs.Volume object.\n"
);

PyDoc_STRVAR(py_glfs_obj_get_file_type__doc__,
"File type of GLFS object handle.\n"
"Dict containing raw and parsed file type bits from cached stat information.\n"
"returns None if stat struct internal to handle is unpopulated.\n"
);

PyDoc_STRVAR(py_glfs_obj_get_name__doc__,
"Name of GLFS object handle.\n"
"Path used during operation to create the GLFS object handle. May return\n"
"None if handle was opened by UUID rather than path.\n"
);

static PyGetSetDef py_glfs_obj_getsetters[] = {
	{
		.name    = discard_const_p(char, "cached_stat"),
		.get     = (getter)py_glfs_obj_get_stat,
		.doc     = py_glfs_obj_get_stat__doc__,
	},
	{
		.name    = discard_const_p(char, "uuid"),
		.get     = (getter)py_glfs_obj_get_uuid,
		.doc     = py_glfs_obj_get_uuid__doc__,
	},
	{
		.name    = discard_const_p(char, "file_type"),
		.get     = (getter)py_glfs_obj_get_file_type,
		.doc     = py_glfs_obj_get_file_type__doc__,
	},
	{
		.name    = discard_const_p(char, "name"),
		.get     = (getter)py_glfs_obj_get_name,
		.doc     = py_glfs_obj_get_name__doc__,
	},
	{ .name = NULL }
};


static PyObject *py_glfs_obj_repr(PyObject *obj)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	PyObject *file_type_str = NULL;
	PyObject *out = NULL;

	file_type_str = py_file_type_str(self->st.st_mode);
	if (file_type_str == NULL) {
		return NULL;
	}

	out = PyUnicode_FromFormat(
		"pyglfs.ObjectHandle(uuid=%s, name=%V, file_type=%U)",
		self->uuid_str, self->name, "<UNKNOWN>", file_type_str
	);

	Py_DECREF(file_type_str);
	return out;
}

PyDoc_STRVAR(py_glfs_object_handle__doc__,
"GLFS object handle\n"
"This handle provides methods to work with glusster objects (files and\n"
"directories), instead of absolute paths.\n"
"The intention using glfs objects is to operate based on parent parent\n"
"object and looking up or creating objects within, OR to be used on the\n"
"actual object thus looked up or created, and retrieve information regarding\n"
"the same.\n"
"The object handles may be used to open / create a gluster FD object for\n"
"gluster FD - based operations\n"
"The object handle is automatically closed when it is deallocated.\n"
);

PyTypeObject PyGlfsObject = {
	.tp_name = "pyglfs.ObjectHandle",
	.tp_basicsize = sizeof(py_glfs_obj_t),
	.tp_methods = py_glfs_obj_methods,
	.tp_getset = py_glfs_obj_getsetters,
	.tp_new = py_glfs_obj_new,
	.tp_init = py_glfs_obj_init,
	.tp_repr = py_glfs_obj_repr,
	.tp_doc = py_glfs_object_handle__doc__,
	.tp_dealloc = (destructor)py_glfs_obj_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
};
