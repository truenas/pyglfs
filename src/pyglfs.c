
#include <Python.h>
#include "includes.h"
#include "pyglfs.h"

#define Py_TPFLAGS_HAVE_ITER 0

void set_exc_from_errno(const char *func)
{
	PyErr_Format(
		PyExc_RuntimeError,
		"%s failed: %s", func, strerror(errno)
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

	if (PyType_Ready(init_pystat_type()) < 0)
		return NULL;

	PyModule_AddObject(m, "Volume", (PyObject *)&PyGlfsVolume);

	return m;
}

PyMODINIT_FUNC PyInit_pyglfs(void)
{
	return module_init();
}
