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

PyObject *os_module;

PyObject *stat_to_pystat(struct stat *st)
{
	PyObject *out = NULL;
	PyObject *v = NULL;

	v = Py_BuildValue(
		"(lkklkkLLLL)",
		(long)st->st_mode,
		st->st_ino,
		st->st_dev,
		st->st_nlink,
		st->st_uid,
		st->st_gid,
		st->st_size,
		st->st_atime,
		st->st_mtime,
		st->st_ctime
	);

	if (v == NULL) {
		return NULL;
	}

	out = PyObject_CallMethod(os_module, "stat_result", "(O)", v);
	Py_DECREF(v);

	return out;
}

bool init_pystat_type(void)
{
	if (os_module != NULL) {
		Py_INCREF(os_module);
	}
	os_module = PyImport_ImportModule("os");
	return os_module ? true : false;
}
