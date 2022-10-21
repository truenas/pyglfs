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
	Py_CLEAR(self->py_st);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyObject *init_glfs_object(py_glfs_t *py_fs,
			   glfs_object_t *gl_obj,
			   struct stat *pst)
{
	py_glfs_obj_t *hdl = NULL;
	PyObject *pyst = NULL;
	uuid_t ui;
	ssize_t rv;

	hdl = (py_glfs_obj_t *)PyObject_CallFunction((PyObject *)&PyGlfsObject, "");
	if (hdl == NULL) {
		return NULL;
	}

	if (pst != NULL) {
		pyst = stat_to_pystat(pst);
		if (pyst == NULL) {
			Py_DECREF(hdl);
			return NULL;
		}
	}

	Py_BEGIN_ALLOW_THREADS
	rv = glfs_h_extract_handle(gl_obj, ui, sizeof(ui));
	Py_END_ALLOW_THREADS

	if (rv == -1) {
		set_glfs_exc("glfs_h_extract_handle()");
		Py_DECREF(hdl);
		Py_DECREF(pyst);
		return NULL;
	}
	uuid_unparse(ui, hdl->uuid_str);
	hdl->py_fs = py_fs;
	Py_INCREF(hdl->py_fs);
	hdl->gl_obj = gl_obj;
	hdl->py_st = pyst;
	return (PyObject *)hdl;
}

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

static PyObject *py_glfs_obj_stat(PyObject *obj,
				  PyObject *args_unused,
				  PyObject *kwargs_unused)
{
	py_glfs_obj_t *self = (py_glfs_obj_t *)obj;
	PyObject *new_st = NULL;
	struct stat st;
	int err;

	Py_BEGIN_ALLOW_THREADS
	err = glfs_h_stat(self->py_fs->fs, self->gl_obj, &st);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_h_stat()");
		return NULL;
	}

	new_st = stat_to_pystat(&st);
	if (new_st == NULL) {
		return NULL;
	}

	Py_CLEAR(self->py_st);
	/* store reference to latest stat in object handle */
	self->py_st = new_st;
	Py_INCREF(new_st);
	return new_st;
}

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
		.ml_doc = "lookup existing path and return handle."
	},
	{
		.ml_name = "create",
		.ml_meth = (PyCFunction)py_glfs_obj_create,
		.ml_flags = METH_VARARGS | METH_KEYWORDS,
		.ml_doc = "create file and return handle."
	},
	{
		.ml_name = "mkdir",
		.ml_meth = (PyCFunction)py_glfs_obj_mkdir,
		.ml_flags = METH_VARARGS | METH_KEYWORDS,
		.ml_doc = "mkdir and return handle."
	},
	{
		.ml_name = "unlink",
		.ml_meth = (PyCFunction)py_glfs_obj_unlink,
		.ml_flags = METH_VARARGS,
		.ml_doc = "unlinkat path under object."
	},
	{
		.ml_name = "stat",
		.ml_meth = (PyCFunction)py_glfs_obj_stat,
		.ml_flags = METH_VARARGS,
		.ml_doc = "stat object."
	},
	{
		.ml_name = "open",
		.ml_meth = (PyCFunction)py_glfs_obj_open,
		.ml_flags = METH_VARARGS,
		.ml_doc = "Open gluster object and get gluster fd"
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
	if (self->py_st == NULL) {
		Py_RETURN_NONE;
	}

	Py_INCREF(self->py_st);
	return self->py_st;
}

static PyGetSetDef py_glfs_obj_getsetters[] = {
	{
		.name    = discard_const_p(char, "cached_stat"),
		.get     = (getter)py_glfs_obj_get_stat,
	},
	{
		.name    = discard_const_p(char, "uuid"),
		.get     = (getter)py_glfs_obj_get_uuid,
	},
	{ .name = NULL }
};

PyTypeObject PyGlfsObject = {
	.tp_name = "pyglfs.ObjectHandle",
	.tp_basicsize = sizeof(py_glfs_obj_t),
	.tp_methods = py_glfs_obj_methods,
	.tp_getset = py_glfs_obj_getsetters,
	.tp_new = py_glfs_obj_new,
	.tp_init = py_glfs_obj_init,
	.tp_doc = "Glusterfs filesystem object handle",
	.tp_dealloc = (destructor)py_glfs_obj_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
};
