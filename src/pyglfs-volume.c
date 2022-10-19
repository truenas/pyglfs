
#include <Python.h>
#include "includes.h"
#include "pyglfs.h"

#define Py_TPFLAGS_HAVE_ITER 0

static PyObject *py_glfs_get_logging(PyObject *obj, void *closure)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	return Py_BuildValue(
		"s:s,s:i",
		"log_file", self->log_file,
		"log_level", self->log_level
	);
}

static PyObject *py_glfs_get_volume_name(PyObject *obj, void *closure)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	return Py_BuildValue("s", self->name);
}

static PyObject *py_glfs_get_volume_id(PyObject *obj, void *closure)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	return Py_BuildValue("s", self->vol_id);
}

static PyObject *py_glfs_get_volfile_servers(PyObject *obj, void *closure)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	PyObject *servers = NULL;
	size_t i;

	servers = PyList_New(self->srv_cnt);
	if (servers == NULL) {
		return NULL;
	}

	for (i = 0; i < self->srv_cnt; i++) {
		glfs_volfile_server_t *srv = &self->volfile_servers[i];
		PyObject *entry = NULL;

		entry = Py_BuildValue(
			"s:s,s:s,s:i",
			"host", srv->host,
			"proto", srv->proto,
			"port", srv->port
		);
		if (entry == NULL) {
			Py_CLEAR(servers);
			return NULL;
		}

		if (PyList_SetItem(servers, i, entry) == -1) {
			Py_CLEAR(servers);
			return NULL;
		}
	}

	return servers;
}

static PyObject *py_glfs_new(PyTypeObject *obj,
			     PyObject *args_unused,
			     PyObject *kwargs_unused)
{
	py_glfs_t *self = NULL;

	self = (py_glfs_t *)obj->tp_alloc(obj, 0);
	if (self == NULL) {
		return NULL;
	}
	self->log_level = -1;
	return (PyObject *)self;
}

static bool py_glfs_init_parse_list_entry(Py_ssize_t idx, PyObject *entry,
					  glfs_volfile_server_t *target)
{
	PyObject *pyhost = NULL, *pyproto = NULL, *pyport = NULL;
	const char *host = NULL, *proto = NULL;
	Py_ssize_t sz;

	if (!PyDict_Check(entry)) {
		return false;
	}

	pyhost = PyDict_GetItemString(entry, "host");
	if (pyhost == NULL) {
		PyErr_Format(
			PyExc_KeyError,
			"Entry (%zu) is missing required key `host`",
			idx
		);
		return false;
	}

	if (!PyUnicode_Check(pyhost)) {
		PyErr_SetString(
			PyExc_TypeError,
			"`host` value must be string."
		);
	}

	host = PyUnicode_AsUTF8AndSize(pyhost, &sz);
	if (host == NULL) {
		return false;
	}

	if (sz >= (Py_ssize_t)sizeof(target->host)) {
		PyErr_Format(
			PyExc_ValueError,
			"%s: host name for entry %zu is too long.",
			host, idx
		);
		return false;
	}

	strlcpy(target->host, host, sizeof(target->host));

	pyproto = PyDict_GetItemString(entry, "proto");
	if (pyproto == NULL) {
		PyErr_Format(
			PyExc_KeyError,
			"Entry (%zu) is missing required key `proto`",
			idx
		);
		return false;
	}

	if (!PyUnicode_Check(pyproto)) {
		PyErr_SetString(
			PyExc_TypeError,
			"`proto` value must be string."
		);
	}

	proto = PyUnicode_AsUTF8AndSize(pyproto, &sz);
	if (host == NULL) {
		return false;
	}

	if ((strcmp(proto, "tcp") != 0) && (strcmp(proto, "rdma") != 0)) {
		PyErr_Format(
			PyExc_ValueError,
			"%s: proto for entry %zu is invalid. Permitted "
			"values are `tcp` and `rdma`",
			proto, idx
		);
		return false;
	}

	strlcpy(target->proto, proto, sizeof(target->proto));

	pyport = PyDict_GetItemString(entry, "port");
	if (pyport == NULL) {
		PyErr_Format(
			PyExc_KeyError,
			"Entry (%zu) is missing required key `port`",
			idx
		);
		return false;
	}

	if (!PyLong_Check(pyport)) {
		PyErr_SetString(
			PyExc_TypeError,
			"`port` value must be integer."
		);
		return false;
	}

	sz = PyLong_AsSsize_t(pyport);
	if (sz == -1) {
		if (PyErr_Occurred())
			return false;

		PyErr_SetString(
			PyExc_ValueError,
			"-1 is not a valid port number"
		);
		return false;
	} else if (sz >= 65535) {
		PyErr_Format(
			PyExc_ValueError,
			"%zu: invalid port number for entry %zu.",
			sz, idx
		);
		return false;
	}

	target->port = (int)sz;

	return true;
}

static bool py_glfs_init_fill_volfile(py_glfs_t *self,
				      PyObject *volfile_list)
{
	Py_ssize_t sz, i;
	glfs_volfile_server_t *servers;

	if (!PyList_Check(volfile_list)) {
		PyErr_SetString(
			PyExc_TypeError,
			"Must be list of volfile servers."
		);
		return false;
	}

	sz = PyList_Size(volfile_list);

	servers = calloc(sz, sizeof(glfs_volfile_server_t));
	if (servers == NULL) {
		PyErr_SetString(
			PyExc_MemoryError,
			"Failed to allocate volfile server array."
		);
		return false;
	}

	for (i = 0; i < sz; i++) {
		PyObject *entry = PyList_GetItem(volfile_list, i);
		if (entry == NULL) {
			free(servers);
			return false;
		}

		if (!py_glfs_init_parse_list_entry(i, entry, &servers[i])) {
			free(servers);
			return false;
		}
	}

	self->volfile_servers = servers;
	self->srv_cnt = sz;

	return true;
}

static bool py_glfs_init_ctx(py_glfs_t *self)
{
	glfs_t *fs = NULL;
	size_t i;
	ssize_t sz;
	int err;
	char buf[16];

	fs = glfs_new(self->name);
	if (fs == NULL) {
		set_exc_from_errno("glfs_new()");
		return false;
        }

	for (i = 0; i < self->srv_cnt; i++) {
		glfs_volfile_server_t *srv = &self->volfile_servers[i];

		err = glfs_set_volfile_server(
			fs, srv->proto, srv->host, srv->port
		);
		if (err) {
			PyErr_Format(
				PyExc_ValueError,
				"glfs_set_volfile_server() failed for entry %i "
				"proto: %s, host: %s, port: %d",
				i, srv->proto, srv->host, srv->port
			);
			return false;
		}
        }

	if (self->log_file[0] != '\0') {
		err = glfs_set_logging(fs, self->log_file, self->log_level);
		if (err) {
			set_exc_from_errno("glfs_set_logging()");
			return false;
		}
	}

	Py_BEGIN_ALLOW_THREADS
	err = glfs_init(fs);
	Py_END_ALLOW_THREADS

	if (err) {
		set_exc_from_errno("glfs_init()");
		return false;
	}

	sz = glfs_get_volumeid(fs, buf, sizeof(buf));
	if (sz == -1) {
		set_exc_from_errno("glfs_get_volumeid()");
		glfs_fini(fs);
		return false;
	}
	if (sz == 16) {
		uuid_t ui;
		for (i = 0; i < sizeof(buf); i++) {
			ui[i] = (unsigned char)buf[i];
		}
		uuid_unparse(ui, self->vol_id);
	}

	self->fs = fs;

	return true;
}

static int py_glfs_init(PyObject *obj,
		        PyObject *args,
		        PyObject *kwargs)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	const char *volname = NULL;
	PyObject *volfile_list = NULL;
	const char *log_file = NULL;
	int log_level = -1;

	const char *kwnames [] = {
		"volume_name",
		"volfile_servers",
		"log_file",
		"log_level",
		NULL
	};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|si",
					 kwnames, &volname,
					 &volfile_list, &log_file,
					 &log_level)) {
		return -1;
	}

	if (volname == NULL) {
		PyErr_SetString(
			PyExc_ValueError,
			"volume must be specified"
		);
		return -1;
	}

	if (strlen(volname) > sizeof(self->name)) {
		PyErr_Format(
			PyExc_ValueError,
			"%s: volume name is too long.",
			volname
		);
		return -1;
	}
	strlcpy(self->name, volname, sizeof(self->name));

	if (log_file && (strlen(log_file) > sizeof(self->log_file))) {
		PyErr_Format(
			PyExc_ValueError,
			"%s: logfile path too long.",
			log_file
		);
		return -1;
	}

	if (log_file != NULL) {
		strlcpy(self->log_file, log_file, sizeof(self->log_file));
	}

	if (!py_glfs_init_fill_volfile(self, volfile_list)) {
		return -1;
	}

	if (!py_glfs_init_ctx(self)) {
		return -1;
	}

	return 0;
}

static void py_glfs_dealloc(py_glfs_t *self)
{
	if (self->volfile_servers != NULL) {
		free(self->volfile_servers);
		self->volfile_servers = NULL;
	}

	if (self->fs != NULL) {
		glfs_fini(self->fs);
		self->fs = NULL;
	}
	Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *py_glfs_get_root(PyObject *obj,
				  PyObject *args_unused,
				  PyObject *kwargs_unused)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	glfs_object_t *gl_obj = NULL;
	struct stat st;
	const char *path = "/";

	Py_BEGIN_ALLOW_THREADS
	gl_obj = glfs_h_lookupat(
		self->fs,
		NULL,
		path,
		&st,
		false
	);
	Py_END_ALLOW_THREADS

	if (gl_obj == NULL) {
		set_exc_from_errno("glfs_h_lookupat()");
		return NULL;
	}

	return init_glfs_object(self, gl_obj, &st);
}

static PyObject *py_glfs_volume_repr(PyObject *obj)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	return PyUnicode_FromFormat(
		"pyglfs.Volume(name=%s, uuid=%s)",
		self->name, self->vol_id
	);
}

static PyObject *py_glfs_getcwd(PyObject *obj,
			        PyObject *args_unused,
			        PyObject *kwargs_unused)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	char buf[PATH_MAX + 1];
	char *cwd = NULL;

	Py_BEGIN_ALLOW_THREADS
	cwd = glfs_getcwd(self->fs, buf, PATH_MAX);
	Py_END_ALLOW_THREADS

	if (cwd == NULL) {
		set_exc_from_errno("glfs_getcwd()");
	}

	return Py_BuildValue("s", cwd);
}

static PyMethodDef py_glfs_volume_methods[] = {
	{
		.ml_name = "get_root_handle",
		.ml_meth = (PyCFunction)py_glfs_get_root,
		.ml_flags = METH_NOARGS,
		.ml_doc = "Get glusterfs object for root of volume"
	},
	{
		.ml_name = "getcwd",
		.ml_meth = (PyCFunction)py_glfs_getcwd,
		.ml_flags = METH_NOARGS,
		.ml_doc = "Get current working directory"
	},
	{ NULL, NULL, 0, NULL }
};

static PyGetSetDef py_glfs_volume_getsetters[] = {
	{
		.name    = discard_const_p(char, "name"),
		.get     = (getter)py_glfs_get_volume_name,
	},
	{
		.name    = discard_const_p(char, "uuid"),
		.get     = (getter)py_glfs_get_volume_id,
	},
	{
		.name    = discard_const_p(char, "volfile_servers"),
		.get     = (getter)py_glfs_get_volfile_servers,
	},
	{
		.name    = discard_const_p(char, "logging"),
		.get     = (getter)py_glfs_get_logging,
	},
	{ .name = NULL }
};

PyTypeObject PyGlfsVolume = {
	.tp_name = "pyglfs.Volume",
	.tp_basicsize = sizeof(py_glfs_t),
	.tp_methods = py_glfs_volume_methods,
	.tp_getset = py_glfs_volume_getsetters,
	.tp_new = py_glfs_new,
	.tp_init = py_glfs_init,
	.tp_repr = py_glfs_volume_repr,
	.tp_doc = "Glusterfs filesystem handle",
	.tp_dealloc = (destructor)py_glfs_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
};
