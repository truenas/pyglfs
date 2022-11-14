
#include <Python.h>
#include "includes.h"
#include "pyglfs.h"

static PyObject *PyExc_GLFSError;

void set_exc_from_errno(const char *func)
{
	PyErr_Format(
		PyExc_RuntimeError,
		"%s failed: %s", func, strerror(errno)
	);
}

void _set_glfs_exc(const char *additional_info, const char *location)
{
	PyObject *v = NULL;
	PyObject *args = NULL;
	PyObject *errstr = NULL;

	if (additional_info) {
		errstr = PyUnicode_FromFormat(
			"%s: %s",
			additional_info,
			strerror(errno)
		);
	} else {
		errstr = Py_BuildValue("%s", strerror(errno));
	}
	if (errstr == NULL) {
		goto simple_err;
	}

	args = Py_BuildValue("(iNs)", errno, errstr, location);
	if (args == NULL) {
		Py_XDECREF(errstr);
		goto simple_err;
	}

	v = PyObject_Call(PyExc_GLFSError, args, NULL);
	if (v == NULL) {
		Py_CLEAR(args);
		return;
	}

	if (PyObject_SetAttrString(v, "errno", PyTuple_GetItem(args, 0)) == -1) {
		Py_CLEAR(args);
		Py_CLEAR(v);
		return;
	}

	if (PyObject_SetAttrString(v, "location", PyTuple_GetItem(args, 2)) == -1) {
		Py_CLEAR(args);
		Py_CLEAR(v);
		return;
	}
	Py_CLEAR(args);

	PyErr_SetObject((PyObject *) Py_TYPE(v), v);
	Py_DECREF(v);
	return;

simple_err:
	PyErr_Format(
		PyExc_GLFSError, "[%d]: %s",
		errno, strerror(errno)
	);
}


#define MODULE_DOC "Minimal libglfs python bindings."

static PyMethodDef glfs_methods[] = { { .ml_name = NULL } };
static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	.m_name = "pyglfs",
	.m_doc = MODULE_DOC,
	.m_size = -1,
	.m_methods = glfs_methods,
};

PyObject* module_init(void)
{
	PyObject *m = NULL;
	m = PyModule_Create(&moduledef);
	if (m == NULL) {
		fprintf(stderr, "failed to initalize module\n");
		return NULL;
	}

	if (PyType_Ready(&PyGlfsVolume) < 0)
		return NULL;

	if (PyType_Ready(&PyGlfsObject) < 0)
		return NULL;

	if (PyType_Ready(&PyGlfsFd) < 0)
		return NULL;

	if (PyType_Ready(&PyGlfsFTS) < 0)
		return NULL;

	if (PyType_Ready(&PyGlfsFTSENT) < 0)
		return NULL;

        if (!init_pystat_type()) {
		return NULL;
	}

	PyExc_GLFSError = PyErr_NewException("pyglfs.GLFSError",
					     PyExc_RuntimeError,
					     NULL);
	if (PyExc_GLFSError == NULL) {
		Py_DECREF(m);
		return NULL;
	}

	if (PyModule_AddObject(m, "GLFSError", PyExc_GLFSError) < 0) {
		Py_DECREF(m);
		return NULL;
	}

	if (PyModule_AddObject(m, "Volume", (PyObject *)&PyGlfsVolume) < 0) {
		Py_DECREF(m);
		return NULL;
	}

	return m;
}

PyMODINIT_FUNC PyInit_pyglfs(void)
{
	return module_init();
}
