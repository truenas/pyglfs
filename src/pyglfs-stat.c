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

PyTypeObject *pystat_typ;

static PyStructSequence_Field stat_result_fields[] = {
	{"st_mode", "protection bits"},
	{"st_ino", "inode"},
	{"st_dev", "device"},
	{"st_nlink", "number of hard links"},
	{"st_uid", "user ID of owner"},
	{"st_gid", "group ID of owner"},
	{"st_size", "total size in bytes"},
	{"st_atime", "integer time of last access"},
	{"st_mtime", "integer time of last modification"},
	{"st_ctime", "integer time of last change"},
	{"st_atime_float", "time of last access"},
	{"st_mtime_float", "time of last modification"},
	{"st_ctime_float", "time of last change"},
	{"st_atime_ns",   "time of last access in nanoseconds"},
	{"st_mtime_ns",   "time of last modification in nanoseconds"},
	{"st_ctime_ns",   "time of last change in nanoseconds"},
	{"st_blksize", "blocksize for filesystem I/O"},
	{"st_blocks", "number of blocks allocated"},
	{"st_rdev", "device type (if inode device)"},
	{0}
};

PyDoc_STRVAR(stat_result__doc__,
"stat_result: Result from stat, fstat, or lstat.\n");

static PyStructSequence_Desc stat_result_desc = {
	"stat_result", /* name */
	stat_result__doc__, /* doc */
	stat_result_fields,
	10
};

#define XID2PY(xid) (xid == (uid_t)-1 ? PyLong_FromLong(-1) : PyLong_FromUnsignedLong(xid))

static void
fill_time(PyObject *v, int index, time_t sec, unsigned long nsec)
{
	PyObject *s = PyLong_FromLongLong((long long)sec);
	PyObject *ns_fractional = PyLong_FromUnsignedLong(nsec);
	PyObject *billion = PyLong_FromLong(1000000000);
	PyObject *s_in_ns = NULL;
	PyObject *ns_total = NULL;
	PyObject *float_s = NULL;

	if (!(s && ns_fractional && billion))
		goto exit;

	s_in_ns = PyNumber_Multiply(s, billion);
	if (!s_in_ns)
		goto exit;

	ns_total = PyNumber_Add(s_in_ns, ns_fractional);
	if (!ns_total)
		goto exit;

	float_s = PyFloat_FromDouble(sec + 1e-9*nsec);
	if (!float_s) {
		goto exit;
	}

	PyStructSequence_SET_ITEM(v, index, s);
	PyStructSequence_SET_ITEM(v, index+3, float_s);
	PyStructSequence_SET_ITEM(v, index+6, ns_total);
	s = NULL;
	float_s = NULL;
	ns_total = NULL;
exit:
	Py_XDECREF(s);
	Py_XDECREF(billion);
	Py_XDECREF(ns_fractional);
	Py_XDECREF(s_in_ns);
	Py_XDECREF(ns_total);
	Py_XDECREF(float_s);
}

PyObject *stat_to_pystat(struct stat *st)
{
	PyObject *v = PyStructSequence_New(pystat_typ);
	if (v == NULL) {
		return NULL;
	}
	PyStructSequence_SET_ITEM(v, 0, PyLong_FromLong((long)st->st_mode));
	PyStructSequence_SET_ITEM(v, 1, PyLong_FromUnsignedLong(st->st_ino));
	PyStructSequence_SET_ITEM(v, 2, PyLong_FromUnsignedLong(st->st_dev));
	PyStructSequence_SET_ITEM(v, 3, PyLong_FromLong((long)st->st_nlink));
	PyStructSequence_SET_ITEM(v, 4, XID2PY(st->st_uid));
	PyStructSequence_SET_ITEM(v, 5, XID2PY(st->st_gid));
	PyStructSequence_SET_ITEM(v, 6, PyLong_FromLongLong(st->st_size));

	fill_time(v, 7, st->st_atime, st->st_atim.tv_nsec);
	fill_time(v, 8, st->st_mtime, st->st_mtim.tv_nsec);
	fill_time(v, 9, st->st_ctime, st->st_ctim.tv_nsec);

	PyStructSequence_SET_ITEM(v, 16, PyLong_FromLong((long)st->st_blocks));
	PyStructSequence_SET_ITEM(v, 19, PyLong_FromLong((long)st->st_rdev));

	if (PyErr_Occurred()) {
		Py_DECREF(v);
		return NULL;
	}

	return v;
}

PyTypeObject *init_pystat_type(void)
{
	stat_result_desc.name = "pyglfs.stat_result";
	pystat_typ = PyStructSequence_NewType(&stat_result_desc);
	Py_INCREF(pystat_typ);
	return pystat_typ;
}
