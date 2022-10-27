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

static PyObject *py_glfs_get_xlators(PyObject *obj, void *closure)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	PyObject *out = NULL;
	Py_ssize_t len, i;

	if (self->xlators == NULL) {
		Py_RETURN_NONE;
	}

	len = PyList_Size(self->xlators);
	out = PyList_New(len);
	for (i = 0; i < len; i++) {
		PyObject *xlator = NULL;
		xlator = PyList_GetItem(self->xlators, i);
		if (xlator == NULL) {
			Py_DECREF(out);
			return NULL;
		}

		if (PyList_SetItem(out, i, xlator) == -1) {
			Py_DECREF(out);
			return NULL;
		}
		Py_INCREF(xlator);
	}

	return out;
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


/*
 * Set the specified xlator entry. This should be a tuple
 * of (xlator, key, value) strings.
 */
static bool set_xlator(PyObject *entry, glfs_t *fs, Py_ssize_t idx)
{
	Py_ssize_t i;
	struct {
		const char *desc;
		const char *res;
	} p[] = {
		{ "xlator", NULL },
		{ "key", NULL },
		{ "value", NULL },
	};

	if (!PyTuple_Check(entry)) {
		PyErr_Format(
			PyExc_TypeError,
			"xlator list entry %zu is not a tuple.", idx
		);
		return false;
	}

	if (PyTuple_Size(entry) != 3) {
		PyErr_Format(
			PyExc_ValueError,
			"xlator list entry %zu is invalid. "
			"list entries must be a tuple of "
			"(xlator name, key, and value)", idx
		);
	}

	for (i = 0; i < PyTuple_Size(entry); i++) {
		PyObject *obj = PyTuple_GetItem(entry, i);

		if (!PyUnicode_Check(obj)) {
			PyErr_Format(
				PyExc_TypeError,
				"xlator entry %zu has invalid %s"
				"must be string.", idx, p[i].desc
			);
		}

		p[i].res = PyUnicode_AsUTF8AndSize(obj, NULL);
		if (p[i].res == NULL) {
			return false;
		}
	}

	if (!glfs_set_xlator_option(fs, p[0].res, p[1].res, p[2].res)) {
		PyErr_Format(
			PyExc_RuntimeError,
			"set_xlator_option() failed: %s. Payload was: "
			"xlator - %s, key - %s, value - %s",
			strerror(errno), p[0].res, p[1].res, p[2].res
		);
		return false;
	}

	return true;
}

/* iterate through list of xlator tuples, validate, and set them */
static bool set_xlators(PyObject *xlators, glfs_t *fs)
{
	Py_ssize_t i;

	if (xlators == NULL) {
		return true;
	}

	if (!PyList_Check(xlators)) {
		PyErr_SetString(
			PyExc_TypeError,
			"Must be list of xlator tuples."
		);
		return false;
	}

	for (i = 0; i < PyList_Size(xlators); i++) {
		PyObject *entry = PyList_GetItem(xlators, i);
		if (entry == NULL) {
			return false;
		}

		if (!set_xlator(entry, fs, i)) {
			return false;
		}
	}

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
		set_glfs_exc("glfs_new()");
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
			glfs_fini(fs);
			return false;
		}
        }

	if (!set_xlators(self->xlators, fs)) {
		free(self->volfile_servers);
		self->volfile_servers = NULL;
		glfs_fini(fs);
		return false;
	}

	if (self->log_file[0] != '\0') {
		err = glfs_set_logging(fs, self->log_file, self->log_level);
		if (err) {
			set_glfs_exc("glfs_set_logging()");
			glfs_fini(fs);
			return false;
		}
	}

	Py_BEGIN_ALLOW_THREADS
	err = glfs_init(fs);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_init()");
		glfs_fini(fs);
		return false;
	}

	sz = glfs_get_volumeid(fs, buf, sizeof(buf));
	if (sz == -1) {
		set_glfs_exc("glfs_get_volumeid()");
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
	PyObject *xlators = NULL;
	const char *log_file = NULL;
	int log_level = -1;

	const char *kwnames [] = {
		"volume_name",
		"volfile_servers",
		"xlators",
		"log_file",
		"log_level",
		NULL
	};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|Osi",
					 discard_const_p(char *, kwnames),
					 &volname, &volfile_list, &xlators,
					 &log_file, &log_level)) {
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

	self->log_level = log_level;
	self->xlators = xlators;

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
		Py_BEGIN_ALLOW_THREADS
		glfs_fini(self->fs);
		Py_END_ALLOW_THREADS
		self->fs = NULL;
	}
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(py_glfs_get_root_handle__doc__,
"get_root_handle()\n"
"--\n\n"
"Opens a GLFS object handle for the root `/` of gluster volume.\n"
"This handle may be used as basis of opening object handles for other\n"
"files and directories in the gluster volume.\n\n"
"Parameters\n"
"----------\n"
"None\n\n"
"Returns\n"
"-------\n"
"volume_root : glfs.ObjectHandle\n"
);

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
		set_glfs_exc("glfs_h_lookupat()");
		return NULL;
	}

	return init_glfs_object(self, gl_obj, &st, "/");
}

static PyObject *py_glfs_volume_repr(PyObject *obj)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	return PyUnicode_FromFormat(
		"pyglfs.Volume(name=%s, uuid=%s)",
		self->name, self->vol_id
	);
}

PyDoc_STRVAR(py_glfs_getcwd__doc__,
"getcwd()\n"
"--\n\n"
"Print the current working directory for the glusterfs virtual\n"
"mount of the glusterfs volume.\n\n"
"Parameters\n"
"----------\n"
"None\n\n"
"Returns\n"
"-------\n"
"cwd : string\n"
);

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
		set_glfs_exc("glfs_getcwd()");
		return NULL;
	}

	return Py_BuildValue("s", cwd);
}

PyDoc_STRVAR(py_glfs_open_by_uuid__doc__,
"open_by_uuid(uuid)\n"
"--\n\n"
"Open a new pyglfs.ObjectHandle by UUID. "
"This requires knowing the UUID for the object beforehand."
"Parameters\n"
"----------\n"
"uuid : str\n"
"    UUID of gluster file or directory for which to open handle.\n\n"
"Returns\n"
"-------\n"
"found_handle : glfs.ObjectHandle\n"
);

static PyObject *py_glfs_open_by_uuid(PyObject *obj,
				      PyObject *args,
				      PyObject *kwargs_unused)
{
	py_glfs_t *self = (py_glfs_t *)obj;
	const char *uuid_str = NULL;
	glfs_object_t *gl_obj = NULL;
	struct stat st;
	uuid_t ui;

	if (!PyArg_ParseTuple(args, "s", &uuid_str)) {
		return NULL;
	}

	if (uuid_parse(uuid_str, ui) == -1) {
		set_exc_from_errno("uuid_parse()");
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	gl_obj = glfs_h_create_from_handle(
		self->fs,
		ui,
		sizeof(ui),
		&st
	);
	Py_END_ALLOW_THREADS

	if (gl_obj == NULL) {
		set_glfs_exc("glfs_h_create_from_handle()");
		return NULL;
	}

	return init_glfs_object(self, gl_obj, &st, NULL);
}

static PyMethodDef py_glfs_volume_methods[] = {
	{
		.ml_name = "get_root_handle",
		.ml_meth = (PyCFunction)py_glfs_get_root,
		.ml_flags = METH_NOARGS,
		.ml_doc = py_glfs_get_root_handle__doc__
	},
	{
		.ml_name = "open_by_uuid",
		.ml_meth = (PyCFunction)py_glfs_open_by_uuid,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_open_by_uuid__doc__
	},
	{
		.ml_name = "getcwd",
		.ml_meth = (PyCFunction)py_glfs_getcwd,
		.ml_flags = METH_NOARGS,
		.ml_doc = py_glfs_getcwd__doc__
	},
	{ NULL, NULL, 0, NULL }
};

PyDoc_STRVAR(py_glfs_get_volume_name__doc__,
"Name of the volume. This identifies the server-side volume and\n"
"the fetched volfile (equivalent of --volfile-id command line\n"
"parameter to glusterfsd).\n"
);

PyDoc_STRVAR(py_glfs_get_volume_id__doc__,
"Volume uuid for the gluster volume"
);

PyDoc_STRVAR(py_glfs_get_volfile_servers__doc__,
"List of addresses for the management server.\n"
"The list is comprised of python Dict objects containing the following keys:\n"
"`proto` - string specifying the transport used to connect to the\n"
"  management daemon (permitted values are `tcp` and `rdma`).\n"
"`host` - string specifying the address where to find the management daemon\n"
"  (IP address or FQDN may be used).\n"
"`port` - int TCP port number where gluster management daemon is listening.\n"
"  Value of `0` uses the default port number GF_DEFAULT_BASE_PORT.\n\n"
);

PyDoc_STRVAR(py_glfs_get_logging__doc__,
":log_file: The logfile to be used for logging. "
"If this is not specified then directory associated with the glusterfs "
"installation is used.\n"
":log_level: Int specifying the degree of logging verbosity.\n"
);

PyDoc_STRVAR(py_glfs_get_xlators__doc__,
"List of glusterfs xlator options enabled on the virtual mount.\n"
"Each list entry is a tuple of strings: (<name of the xlator>, <key>, <value>)\n"
);

static PyGetSetDef py_glfs_volume_getsetters[] = {
	{
		.name    = discard_const_p(char, "name"),
		.get     = (getter)py_glfs_get_volume_name,
		.doc     = py_glfs_get_volume_name__doc__,
	},
	{
		.name    = discard_const_p(char, "uuid"),
		.get     = (getter)py_glfs_get_volume_id,
		.doc     = py_glfs_get_volume_id__doc__,
	},
	{
		.name    = discard_const_p(char, "volfile_servers"),
		.get     = (getter)py_glfs_get_volfile_servers,
		.doc     = py_glfs_get_volfile_servers__doc__,
	},
	{
		.name    = discard_const_p(char, "logging"),
		.get     = (getter)py_glfs_get_logging,
		.doc     = py_glfs_get_logging__doc__,
	},
	{
		.name    = discard_const_p(char, "translators"),
		.get     = (getter)py_glfs_get_xlators,
		.doc     = py_glfs_get_xlators__doc__,
	},
	{ .name = NULL }
};

PyDoc_STRVAR(py_glfs_volume__doc__,
"Volume: This object wraps around a glfs_t (virtual mount) object.\n\n"
"Takes the following parameters:\n"
":volume_name: Name of the volume. This identifies the server-side volume and\n"
"  the fetched volfile (equivalent of --volfile-id command line\n"
"  parameter to glusterfsd).\n\n"
":volfile_servers: specifies the list of addresses for the management server\n"
"  (glusterd) to connect, and establish the volume configuration. \n"
"  The list is comprised of python Dict objects containing the following keys:\n"
"  `proto` - string specifying the transport used to connect to the\n"
"    management daemon (permitted values are `tcp` and `rdma`).\n"
"  `host` - string specifying the address where to find the management daemon\n"
"    (IP address or FQDN may be used).\n"
"  `port` - int TCP port number where gluster management daemon is listening.\n"
"    Value of `0` uses the default port number GF_DEFAULT_BASE_PORT.\n\n"
":translators: specifies list of glusterfs xlators to enable on the virtual mount.\n"
"  Each list entry must be a tuple of strings as follows:\n"
"  (<name of the xlator>, <key>, <value>)\n\n"
":log_file: The logfile to be used for logging. Will be created if it does not\n"
"  already exist (provided system permissions allow).\n"
"  If this is not specified then a new logfile will be created in default log\n"
"  directory associated with the glusterfs installation.\n\n"
":log_level: Int specifying the degree of logging verbosity.\n"
);

PyTypeObject PyGlfsVolume = {
	.tp_name = "pyglfs.Volume",
	.tp_basicsize = sizeof(py_glfs_t),
	.tp_methods = py_glfs_volume_methods,
	.tp_getset = py_glfs_volume_getsetters,
	.tp_new = py_glfs_new,
	.tp_init = py_glfs_init,
	.tp_repr = py_glfs_volume_repr,
	.tp_doc = py_glfs_volume__doc__,
	.tp_dealloc = (destructor)py_glfs_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
};
