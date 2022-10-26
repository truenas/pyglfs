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
			   struct stat *pst)
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

	return init_glfs_object(self->py_fs, gl_obj, do_stat ? &st : NULL);
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

	return init_glfs_object(self->py_fs, gl_obj, do_stat ? &st : NULL);
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

	return init_glfs_object(self->py_fs, gl_obj, do_stat ? &st : NULL);
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
	gl_fd = glfs_h_open(self->py_fs->fs, self->gl_obj, flags);
	Py_END_ALLOW_THREADS

	if (gl_fd == NULL) {
		set_glfs_exc("glfs_h_open()");
		return NULL;
	}

	return init_glfs_fd(gl_fd, self, flags);
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
                "pyglfs.ObjectHandle(uuid=%s, file_type=%U)",
                self->uuid_str, file_type_str
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
