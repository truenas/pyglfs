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

typedef struct {
	PyObject_HEAD
	py_glfs_obj_t *obj;
	int flags;
	int max_depth;
} py_glfs_fts_t;

typedef struct {
	PyObject_HEAD
	py_glfs_fts_t *fts_root;
	glfs_object_cb_t iter_cb;
} py_glfs_fts_iter_t;

typedef struct {
	PyObject_HEAD
	py_glfs_fts_t *fts_root;
	py_glfs_obj_t *obj;
	PyObject *name;
	PyObject *file_type;
	PyObject *parent_path;
	size_t depth;
} py_glfs_ftsent_t;

static void py_glfs_ftsent_dealloc(py_glfs_ftsent_t *self)
{
	Py_CLEAR(self->fts_root);
	Py_CLEAR(self->obj);
	Py_CLEAR(self->name);
	Py_CLEAR(self->file_type);
	Py_CLEAR(self->parent_path);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *py_glfs_ftsent_new(PyTypeObject *obj,
				    PyObject *args_unused,
				    PyObject *kwargs_unused)
{
	return obj->tp_alloc(obj, 0);
}

PyDoc_STRVAR(ftsent_name__doc__,
"Name of glfs.FTSEntry.\n\n"
"This is the value of struct dentry d_name at time when\n"
"entry was generated. It will not update as files are renamed\n"
);

static PyObject *ftsent_get_name(PyObject *obj, void *closure)
{
	py_glfs_ftsent_t *self = (py_glfs_ftsent_t *)obj;

	if (self->name == NULL) {
		Py_RETURN_NONE;
	}

	Py_INCREF(self->name);
	return self->name;
}

PyDoc_STRVAR(ftsent_handle__doc__,
"Get reference to underlying glfs object handle.\n\n"
"This glfs object handle is a copy of the temporary ones\n"
"generated during directory iteration, and will be freed\n"
"when it is deallocated.\n"
);
static PyObject *ftsent_get_handle(PyObject *obj, void *closure)
{
	py_glfs_ftsent_t *self = (py_glfs_ftsent_t *)obj;

	if (self->obj == NULL) {
		Py_RETURN_NONE;
	}

	Py_INCREF(self->obj);
	return (PyObject *)self->obj;
}

PyDoc_STRVAR(ftsent_depth__doc__,
"Directory depth of glfs FTSEntry relative to root.\n\n"
"`root` in this case refers to the target of fts_open()\n"
"and not the absolute root of the glusterfs volume.\n"
);
static PyObject *ftsent_get_depth(PyObject *obj, void *closure)
{
	py_glfs_ftsent_t *self = (py_glfs_ftsent_t *)obj;
	return Py_BuildValue("l", self->depth);
}

PyDoc_STRVAR(ftsent_root__doc__,
"Get reference to gfs.FTSHandle object used to create this\n"
"entry. fts_root->obj will reference the pyglfs object\n"
"originally used to open the fts handle. It will therefore not be\n"
"identical to the glusterfs object handle for the FTSEntry\n"
"object.\n"
);
static PyObject *ftsent_get_root(PyObject *obj, void *closure)
{
	py_glfs_ftsent_t *self = (py_glfs_ftsent_t *)obj;

	if (self->fts_root == NULL) {
		Py_RETURN_NONE;
	}

	Py_INCREF(self->fts_root);
	return (PyObject *)self->fts_root;
}

PyDoc_STRVAR(ftsent_file_type__doc__,
"File type underlying the FTSEntry object\n\n"
"This is retrieved from struct dirent d_type info and so\n"
"is available even if stat() on files is not performed.\n"
);
static PyObject *ftsent_get_file_type(PyObject *obj, void *closure)
{
	py_glfs_ftsent_t *self = (py_glfs_ftsent_t *)obj;

	if (self->file_type == NULL) {
		Py_RETURN_NONE;
	}

	Py_INCREF(self->file_type);
	return (PyObject *)self->file_type;
}

PyDoc_STRVAR(ftsent_parent_path__doc__,
"Parent path for this FTSEntry\n\n"
"This is a reconstruction of approximate path components\n"
"leading from the open glfs handle in fts_root to this\n"
"handle. It is primarily a convenience feature for API users\n"
"who wish more info about the handle. Generally, handles should\n"
"be used rather than paths to ensure safety from symlink races.\n"
);
static PyObject *ftsent_get_parent_path(PyObject *obj, void *closure)
{
	py_glfs_ftsent_t *self = (py_glfs_ftsent_t *)obj;

	if (self->parent_path == NULL) {
		Py_RETURN_NONE;
	}

	Py_INCREF(self->parent_path);
	return (PyObject *)self->parent_path;
}

static PyGetSetDef py_glfs_ftsent_getsetters[] = {
	{
		.name    = discard_const_p(char, "name"),
		.get     = (getter)ftsent_get_name,
		.doc     = ftsent_name__doc__,
	},
	{
		.name    = discard_const_p(char, "handle"),
		.get     = (getter)ftsent_get_handle,
		.doc     = ftsent_handle__doc__,
	},
	{
		.name    = discard_const_p(char, "depth"),
		.get     = (getter)ftsent_get_depth,
		.doc     = ftsent_depth__doc__,
	},
	{
		.name    = discard_const_p(char, "root"),
		.get     = (getter)ftsent_get_root,
		.doc     = ftsent_root__doc__,
	},
	{
		.name    = discard_const_p(char, "file_type"),
		.get     = (getter)ftsent_get_file_type,
		.doc     = ftsent_file_type__doc__,
	},
	{
		.name    = discard_const_p(char, "parent_path"),
		.get     = (getter)ftsent_get_parent_path,
		.doc     = ftsent_parent_path__doc__,
	},
	{ .name = NULL }
};

static PyObject *py_glfs_ftsent_repr(PyObject *obj)
{
	py_glfs_ftsent_t *self = (py_glfs_ftsent_t *)obj;
	return PyUnicode_FromFormat(
		"pyglfs.FTSEntry(name=%U, depth=%zu, file_type=%U, parent_path=%U)",
		self->name, self->depth, self->file_type, self->parent_path
	);
}

PyDoc_STRVAR(py_glfs_ftsent__doc__,
"GLFS FTS handle\n"
);

PyTypeObject PyGlfsFTSENT = {
	.tp_name = "pyglfs.FTSEntry",
	.tp_basicsize = sizeof(py_glfs_ftsent_t),
	.tp_new = py_glfs_ftsent_new,
	.tp_getset = py_glfs_ftsent_getsetters,
	.tp_doc = py_glfs_ftsent__doc__,
	.tp_repr = py_glfs_ftsent_repr,
	.tp_dealloc = (destructor)py_glfs_ftsent_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
};

static PyObject *init_ftsent_object(py_glfs_fts_t *fts_root,
				    glfs_object_t *newobj,
				    struct stat *st,
				    struct dirent *dp,
				    size_t depth,
				    const char *parent_path)
{
	py_glfs_ftsent_t *ftsent;

	ftsent = (py_glfs_ftsent_t *)PyObject_CallNoArgs((PyObject *)&PyGlfsFTSENT);
	if (ftsent == NULL) {
		return NULL;
	}

	ftsent->obj = (py_glfs_obj_t *)init_glfs_object(
		fts_root->obj->py_fs,
		newobj,
		st,
		dp->d_name
	);

	if (ftsent->obj == NULL) {
		Py_CLEAR(ftsent);
		return NULL;
	}

	ftsent->fts_root = fts_root;
	Py_INCREF(ftsent->fts_root);

	ftsent->name = PyUnicode_FromString(dp->d_name);
	if (ftsent->name == NULL) {
		Py_CLEAR(ftsent);
		return NULL;
	}

	ftsent->depth = depth;
	ftsent->parent_path = PyUnicode_FromString(parent_path ? parent_path : ".");
	if (ftsent->parent_path == NULL) {
		Py_CLEAR(ftsent);
		return NULL;
	}

	ftsent->file_type = py_file_type_str(DTTOIF(dp->d_type));
	if (ftsent->file_type == NULL) {
		Py_CLEAR(ftsent);
		return NULL;
	}

	return (PyObject *)ftsent;
}

/* internal terator for pyglfs object handles */
static void py_fts_iter_dealloc(py_glfs_fts_iter_t *self)
{
	iter_cb_cleanup(&self->iter_cb);
	Py_CLEAR(self->fts_root);
	PyObject_Del(self);
}

struct py_fts_iter_cb_state {
	PyObject *out;
	py_glfs_fts_t *fts_root;
	bool copy_failed;
	glfs_object_cb_t *cb;
};

static bool py_fts_do_iter(py_glfs_obj_t *root,
			   glfs_object_t *obj,
			   struct dirent *dp,
			   struct stat *st,
			   size_t depth,
			   const char *parent_path,
			   void *priv)
{
	glfs_object_t *newobj = NULL;
	struct py_fts_iter_cb_state *state = (struct py_fts_iter_cb_state *)priv;

	newobj = glfs_object_copy(obj);
	if (newobj == NULL) {
		state->copy_failed = true;
		return false;
	}

	/* re-aquire GIL for creating our output python object */
	OBJ_ITER_END_ALLOW_THREADS(state->cb)
	state->out = init_ftsent_object(state->fts_root, newobj, st, dp, depth, parent_path);
	OBJ_ITER_ALLOW_THREADS(state->cb)

	// break loop
	return false;
}

/*
 * This function does main work of iterating contents of handle.
 *
 * Python will repeatedly call this function until either
 * exception indicating an error is raised, or PyExc_StopIteration
 * is raised. 
 */
static PyObject *py_fts_iter_next(py_glfs_fts_iter_t *self)
{
	int rv;
	struct py_fts_iter_cb_state state = {
		.out = NULL,
		.fts_root = self->fts_root,
		.copy_failed = false,
		.cb = &self->iter_cb
	};

	self->iter_cb.state = &state;

	OBJ_ITER_ALLOW_THREADS(state.cb)
	rv = iter_glfs_object_handle(
		self->fts_root->obj,
		&self->iter_cb
	);
	OBJ_ITER_END_ALLOW_THREADS(state.cb)

	/*
	 * rv -2 here indicates that the
	 * callback function intentially broke
	 * the loop iterating contents.
	 * this is expected here because python
	 * iteration should stop until python
	 * requests nexst iteration.
	 */
	if ((rv == -2) && (state.copy_failed)) {
		set_glfs_exc("glfs_object_copy()");
	}

	/*
	 * rv 0 is a special value indicating that
	 * we should stop iteration.
	 */
	if (rv == 0) {
		PyErr_SetNone(PyExc_StopIteration);
	}

	return state.out;
}

PyTypeObject PyFTSIter = {
	.tp_name = "pyglfs.FTSIterator",
	.tp_basicsize = sizeof(py_glfs_fts_iter_t),
	.tp_iternext = (iternextfunc)py_fts_iter_next,
	.tp_doc = "GLFS object handle iterator",
	.tp_dealloc = (destructor)py_fts_iter_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_iter = PyObject_SelfIter,
};

PyObject *init_fts_iter(py_glfs_fts_t *self)
{
	py_glfs_fts_iter_t *iter = NULL;

	iter = PyObject_New(py_glfs_fts_iter_t, &PyFTSIter);
	if (iter == NULL) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	iter->iter_cb.root.fd = glfs_h_opendir(
		self->obj->py_fs->fs,
		self->obj->gl_obj
	);
	Py_END_ALLOW_THREADS

	if (iter->iter_cb.root.fd == NULL) {
		set_glfs_exc("glfs_h_opendir()");
		Py_DECREF(iter);
		return NULL;
	}
	iter->fts_root = self;
	Py_INCREF(self);

	iter->iter_cb.flags = self->flags;
	iter->iter_cb.fn = py_fts_do_iter;
	iter->iter_cb.max_depth = self->max_depth;

	return (PyObject *)iter;
}

static PyObject *py_glfs_fts_new(PyTypeObject *obj,
				 PyObject *args_unused,
				 PyObject *kwargs_unused)
{
	py_glfs_fts_t *self = NULL;

	self = (py_glfs_fts_t *)obj->tp_alloc(obj, 0);
	if (self == NULL) {
		return NULL;
	}
	return (PyObject *)self;
}

static int py_glfs_fts_init(PyObject *obj,
			    PyObject *args,
			    PyObject *kwargs)
{
	py_glfs_fts_t *self = (py_glfs_fts_t *)obj;
	PyObject *target = NULL;
	int flags = 0;
	int max_depth = -1;
	const char *kwnames [] = {
		"obj",
		"flags",
		"max_depth",
		NULL
	};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs,
					 "O|ii",
					 discard_const_p(char *, kwnames),
					 &target,
					 &flags,
					 &max_depth)) {
		return -1;
	}

	self->obj = (py_glfs_obj_t *)target;
	Py_INCREF(self->obj);
	self->flags = flags;
	self->max_depth = max_depth;
	return 0;
}

void py_glfs_fts_dealloc(py_glfs_fts_t *self)
{
	Py_CLEAR(self->obj);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMethodDef py_glfs_fts_methods[] = {
	{ NULL, NULL, 0, NULL }
};

static PyGetSetDef py_glfs_fts_getsetters[] = {
	{ .name = NULL }
};

PyDoc_STRVAR(py_glfs_fts_handle__doc__,
"GLFS FTS Handle\n\n"
"This is a handle for recursively iterating directory contents\n"
"for a glfs object handle. Iterator returns glfs.FTSEntry objects.\n" 
);

PyTypeObject PyGlfsFTS = {
	.tp_name = "pyglfs.FTSHandle",
	.tp_basicsize = sizeof(py_glfs_fts_t),
	.tp_methods = py_glfs_fts_methods,
	.tp_getset = py_glfs_fts_getsetters,
	.tp_new = py_glfs_fts_new,
	.tp_init = py_glfs_fts_init,
	.tp_doc = py_glfs_fts_handle__doc__,
	.tp_dealloc = (destructor)py_glfs_fts_dealloc,
	.tp_iter = (getiterfunc)init_fts_iter,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE|Py_TPFLAGS_HAVE_ITER,
};
