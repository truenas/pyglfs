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
	{"st_atime", "time of last access"},
	{"st_mtime", "time of last modification"},
	{"st_ctime", "time of last change"},
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
#if 0 /* TODO: add timestamps */

#endif
	PyStructSequence_SET_ITEM(v, 10, PyLong_FromLong((long)st->st_blocks));
	PyStructSequence_SET_ITEM(v, 11, PyLong_FromLong((long)st->st_rdev));

	if (PyErr_Occurred()) {
		Py_DECREF(v);
		return NULL;
	}

	return v;
}

PyTypeObject *init_pystat_type(void)
{
	pystat_typ = PyStructSequence_NewType(&stat_result_desc);
	Py_INCREF(pystat_typ);
	return pystat_typ;
}
