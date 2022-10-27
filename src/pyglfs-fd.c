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

static PyObject *py_glfs_fd_new(PyTypeObject *obj,
				PyObject *args_unused,
				PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = NULL;

	self = (py_glfs_fd_t *)obj->tp_alloc(obj, 0);
	if (self == NULL) {
		return NULL;
	}
	return (PyObject *)self;
}

static int py_glfs_fd_init(PyObject *obj,
			   PyObject *args,
			   PyObject *kwargs)
{
	return 0;
}

void py_glfs_fd_dealloc(py_glfs_fd_t *self)
{
	if (self->fd) {
		if (glfs_close(self->fd) == -1) {
			fprintf(stderr, "glfs_close() failed: %s",
				strerror(errno));
		}
		self->fd = NULL;
	}
	Py_CLEAR(self->parent);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

PyObject *init_glfs_fd(glfs_fd_t *fd_in, py_glfs_obj_t *hdl, int flags)
{
	py_glfs_fd_t *pyfd = NULL;

	pyfd = (py_glfs_fd_t *)PyObject_CallFunction((PyObject *)&PyGlfsFd, NULL);
	if (pyfd == NULL) {
		glfs_close(fd_in);
		return NULL;
	}

	pyfd->fd = fd_in;
	pyfd->flags = flags;
	pyfd->parent = hdl;
	Py_INCREF(hdl);

	return (PyObject *)pyfd;
}

PyDoc_STRVAR(py_glfs_fd_fstat__doc__,
"fstat()\n"
"--\n\n"
"Perform fstat on open glusterfs file descriptor object\n\n"
"Parameters\n"
"----------\n"
"None\n\n"
"Returns\n"
"-------\n"
"stat_result\n"
);

static PyObject *py_glfs_fd_fstat(PyObject *obj,
				  PyObject *args_unused,
				  PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	struct stat st;
	int err;

	Py_BEGIN_ALLOW_THREADS
	err = glfs_fstat(self->fd, &st);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_fstat()");
		return NULL;
	}

	return stat_to_pystat(&st);
}

PyDoc_STRVAR(py_glfs_fd_fsync__doc__,
"fsync()\n"
"--\n\n"
"Perform fsync() on open glusterfs file descriptor object\n"
"See manpage for fsync(2) for more details.\n\n"
"Parameters\n"
"----------\n"
"None\n\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_fd_fsync(PyObject *obj,
				  PyObject *args_unused,
				  PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	int err;

	Py_BEGIN_ALLOW_THREADS
	err = glfs_fsync(self->fd, NULL, NULL);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_fsync()");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_glfs_fd_fchdir__doc__,
"fchdir()\n"
"--\n\n"
"Change the current working directory of the glusterfs mount\n"
"to the directory as represented by the open glfs.FD object.\n"
"Perform fsync() on open glusterfs file descriptor object\n\n"
"Parameters\n"
"----------\n"
"None\n\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_fd_fchdir(PyObject *obj,
				  PyObject *args_unused,
				  PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	int err;

	Py_BEGIN_ALLOW_THREADS
	err = glfs_fchdir(self->fd);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_fchdir()");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_glfs_fd_fchmod__doc__,
"fchmod(mode)\n"
"--\n\n"
"Change the mode of the file given by glusterfs FD to the numeric mode.\n"
"See documentation for os.chmod() for more details.\n\n"
"Parameters\n"
"----------\n"
"mode : int\n"
"    New mode for object.\n\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_fd_fchmod(PyObject *obj,
				   PyObject *args,
				   PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	int mode = 0;
	int err;

	if (!PyArg_ParseTuple(args, "i", &mode)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	err = glfs_fchmod(self->fd, mode);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_fchmod()");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_glfs_fd_fchown__doc__,
"fchown(uid, gid)\n"
"--\n\n"
"Change the owner and group id of the file given by glusterfs FD to the\n"
"specified numeric uid and gid.\n\n"
"Parameters\n"
"----------\n"
"uid : int\n"
"    New owner UID. Special value of `-1` may be used to leave unchanged.\n"
"gid : int\n"
"    New owner GID. Special value of `-1` may be used to leave unchanged.\n\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_fd_fchown(PyObject *obj,
				   PyObject *args,
				   PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	int uid = -1;
	int gid = -1;
	int err;

	if (!PyArg_ParseTuple(args, "ii", &uid, &gid)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	err = glfs_fchown(self->fd, (uid_t)uid, (gid_t)gid);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_fchown()");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_glfs_fd_ftruncate__doc__,
"ftruncate(length)\n"
"--\n\n"
"Truncate the file corresponding to glusterfs FD, \n"
"so that it is at most `length` bytes in size.\n\n"
"Parameters\n"
"----------\n"
"length : int\n"
"    New length of file.\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_fd_ftruncate(PyObject *obj,
				      PyObject *args,
				      PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	off_t length;
	int err;

	if (!PyArg_ParseTuple(args, "L", &length)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	err = glfs_ftruncate(self->fd, length, NULL, NULL);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_ftruncate()");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_glfs_fd_lseek__doc__,
"lseek(offset, whence=0)\n"
"--\n\n"
"Repositions the file offset of the open file description\n"
"associated with the file descriptor fd to the argument `position`\n"
"according to the directive `whence`. See manpage for lseek(2) for more\n"
"details.\n\n"
"Parameters\n"
"----------\n"
"offset : int\n"
"    Offset relative to `whence` to which the fd will be repositioned.\n"
"whence : int, optional, default=SEEK_SET\n"
"    See manpage for lseek(2), and documentation for os.lseek() for further\n"
"    information. Some possible values are SEEK_SET (0), SEEK_CUR (1), and\n"
"    SEEK_END (2)\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_fd_lseek(PyObject *obj,
				  PyObject *args,
				  PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	off_t pos;
	int how = SEEK_SET;
	int err;

	if (!PyArg_ParseTuple(args, "L|i", &pos, &how)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	err = glfs_lseek(self->fd, pos, how);
	Py_END_ALLOW_THREADS

	if (err) {
		set_glfs_exc("glfs_lseek()");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_glfs_fd_pread__doc__,
"pread(offset, cnt)\n"
"--\n\n"
"Read at most `cnt` bytes from glusterfs file descriptor at a position of `offset`\n"
"leaving the file offset unchanged.\n\n"
"Parameters\n"
"----------\n"
"offset : int\n"
"    Position from which to read.\n"
"cnt: int\n"
"    Number of bytes to read from offset\n\n"
"Returns\n"
"-------\n"
"bytes\n"
"    bytestring containing the bytes read. If the end of the file referred to by fd has\n"
"    been reached, an empty bytes object is returned.\n"
);

static PyObject *py_glfs_fd_pread(PyObject *obj,
				  PyObject *args,
				  PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	off_t offset;
	Py_ssize_t cnt, n;
	int flags = 0;
	PyObject *buffer = NULL;

	if (!PyArg_ParseTuple(args, "Ln", &offset, &cnt)) {
		return NULL;
	}

	if (cnt < 0) {
		errno = EINVAL;
		set_exc_from_errno("glfs_pread()");
		return NULL;
	}

	buffer = PyBytes_FromStringAndSize((char *)NULL, cnt);
	if (buffer == NULL) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	n = glfs_pread(self->fd, PyBytes_AS_STRING(buffer), cnt, offset, flags, NULL);
	Py_END_ALLOW_THREADS

	if (n < 0) {
		Py_DECREF(buffer);
		set_glfs_exc("glfs_pread()");
		return NULL;
	}

	if (n != cnt) {
		_PyBytes_Resize(&buffer, n);
	}

	return buffer;
}

PyDoc_STRVAR(py_glfs_fd_pwrite__doc__,
"pwrite(buf, offset)\n"
"--\n\n"
"Write the bytestring `buf` to file descriptor at position of `offset`, "
"leaving the file offset unchanged.\n"
"Parameters\n"
"----------\n"
"buf : bytes\n"
"    Bytestring to write.\n"
"offset : int\n"
"    Position to which to write.\n\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_fd_pwrite(PyObject *obj,
				   PyObject *args,
				   PyObject *kwargs_unused)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	PyObject *return_value = NULL;
	PyObject *buf = NULL;
	Py_buffer buffer = {NULL, NULL};
	off_t offset;
	Py_ssize_t _return_value;
	int flags = 0;

	if (!PyArg_ParseTuple(args, "OL", &buf, &offset)) {
		return NULL;
	}

	if (!PyObject_CheckBuffer(buf)) {
		PyErr_SetString(
			PyExc_TypeError,
			"not a buffer."
		);
		goto cleanup;
	}

	if (PyObject_GetBuffer(buf, &buffer, PyBUF_SIMPLE) != 0) {
		goto cleanup;
	}

	if (!PyBuffer_IsContiguous(&buffer, 'C')) {
		PyErr_SetString(
                        PyExc_TypeError,
			"buffer must be contiguous."
		);
		goto cleanup;
	}

	Py_BEGIN_ALLOW_THREADS
	_return_value = glfs_pwrite(
		self->fd, buffer.buf, (size_t)buffer.len, offset, flags,
		NULL, NULL
	);
	Py_END_ALLOW_THREADS

	if (_return_value == -1) {
		set_glfs_exc("glfs_pwrite()");
	} else {
		return_value = PyLong_FromSsize_t(_return_value);
	}

cleanup:
	if (buffer.obj) {
		PyBuffer_Release(&buffer);
	}
	return return_value;
}

PyDoc_STRVAR(py_glfs_fd_posix_lock__doc__,
"posix_lock(cmd, type, whence=0, start=0, length=1, verbose=False)\n"
"--\n\n"
"Apply, test, or remove POSIX lock from the specified glusterfs file descriptor.\n"
"Parameters\n"
"----------\n"
"cmd : int\n"
"    locking operation to perform. See documentation for fcntl for more information.\n"
"    Possible values are fcntl.F_GETLK, fcntl.F_SETLK, fcntl.SETLKW\n"
"type : int\n"
"    type of lock to operate on possible values are:\n"
"    fcntl.F_RDLCK, fcntl.F_WRLCK, fcntl.F_UNLCK\n"
"    Bytestring to write.\n"
"whence : int, default=SEEK_SET\n"
"    how to interpret the `start` value. Default is `SEEK_SET` (beginning of file)\n"
"start : int, default=0\n"
"    starting position of POSIX lock on file relative to `whence`.\n"
"length : int, default=1\n"
"    length of lock on file (from starting position).\n"
"verbose : bool, default=False\n"
"    Whether to return dictionary containing information from flock struct.\n\n"
"Returns\n"
"-------\n"
"flock : dict or None\n"
"    dict containing flock info if `verbose` is True (non-default)\n"
);

static PyObject *py_glfs_fd_posix_lock(PyObject *obj,
				       PyObject *args,
				       PyObject *kwargs)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	int cmd;
	bool verbose = false;
	struct flock fl = {
		.l_type = -1,
		.l_whence = SEEK_SET,
		.l_len = 1,
		.l_start = 0,
		.l_pid = 0
	};
	const char *kwnames [] = {
		"cmd",
		"type",
		"whence",
		"start",
		"len",
		"verbose",
		NULL
	};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs,
					 "ih|hLLb",
					 discard_const_p(char *, kwnames),
					 &cmd,
					 &fl.l_type,
					 &fl.l_whence,
					 &fl.l_start,
					 &fl.l_len,
					 &verbose)) {
		return NULL;
	}

	if ((cmd != F_GETLK) &&
	    (cmd != F_SETLK) &&
	    (cmd != F_SETLKW)) {
		PyErr_Format(
			PyExc_ValueError,
			"%d: Invalid locking command.", cmd
		);
		return NULL;
	}

	if ((fl.l_type != F_RDLCK) &&
	    (fl.l_type != F_WRLCK) &&
	    (fl.l_type != F_UNLCK)) {
		PyErr_Format(
			PyExc_ValueError,
			"%d: Invalid lock type.", fl.l_type
		);
		return NULL;
	}

	if ((cmd == F_GETLK) && (fl.l_type == F_UNLCK)) {
		PyErr_SetString(
			PyExc_ValueError,
			"Lock type of F_UNLCK may not be specified "
			"for an operation to read lock"
		);
		return NULL;
	}

	if (glfs_posix_lock(self->fd, cmd, &fl) != 0) {
		set_glfs_exc("glfs_posix_lock()");
		return NULL;
	}

	if (!verbose)
		Py_RETURN_NONE;

	return Py_BuildValue(
		"{s:i,s:h,s:h,s:L,s:L,s:i}",
		"command", cmd,
		"type", fl.l_type,
		"whence", fl.l_whence,
		"start", fl.l_start,
		"length", fl.l_len,
		"pid", fl.l_pid
	);
}

PyDoc_STRVAR(py_glfs_fd_flistxattr__doc__,
"flistxattr()\n"
"--\n\n"
"retrieves the list of extended attribute names associated with the given "
"glusterfs file descriptor object.\n\n"
"Parameters\n"
"----------\n"
"None\n\n"
"Returns\n"
"-------\n"
"names : list\n"
"   list of strings containing xattr names\n"
);

static PyObject *py_glfs_fd_flistxattr(PyObject *obj,
				       PyObject *no_args,
				       PyObject *no_kwargs)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	Py_ssize_t i;
	PyObject *result = NULL;
	char *buffer = NULL;

	for (i = 0; ; i++) {
		const char *start, *trace, *end;
		ssize_t length;
		static const Py_ssize_t buffer_sizes[] = { 256, XATTR_LIST_MAX, 0 };
		Py_ssize_t buffer_size = buffer_sizes[i];

		if (!buffer_size) {
			/* ERANGE */
			set_glfs_exc("glfs_flistxattr()");
			break;
		}
		buffer = PyMem_MALLOC(buffer_size);
		if (!buffer) {
			PyErr_NoMemory();
			break;
		}

		Py_BEGIN_ALLOW_THREADS;
		length = glfs_flistxattr(self->fd, buffer, buffer_size);
		Py_END_ALLOW_THREADS;

		if (length < 0) {
			if (errno == ERANGE) {
				PyMem_FREE(buffer);
				buffer = NULL;
				continue;
			}
			set_glfs_exc("glfs_flistxattr()");
			break;
		}

		result = PyList_New(0);
		if (!result) {
			goto exit;
		}
		end = buffer + length;
		for (trace = start = buffer; trace != end; trace++) {
			if (!*trace) {
				int error;
				PyObject *attribute = PyUnicode_DecodeFSDefaultAndSize(
					start, trace - start
				);
				if (!attribute) {
					Py_DECREF(result);
					result = NULL;
					goto exit;
				}
				error = PyList_Append(result, attribute);
				Py_DECREF(attribute);
				if (error) {
					Py_DECREF(result);
					result = NULL;
					goto exit;
				}
				start = trace + 1;
			}
		}
		break;
	}
exit:
	if (buffer)
		PyMem_FREE(buffer);

	return result;
}

PyDoc_STRVAR(py_glfs_fd_fgetxattr__doc__,
"fgetxattr(xattr_name)\n"
"--\n\n"
"retrieves value of the specified extended attribute associated with the given "
"glusterfs file descriptor object.\n\n"
"Parameters\n"
"----------\n"
"xattr_name : str\n"
"    Name of the extended attribute to retrieve\n"
"Returns\n"
"-------\n"
"value : bytes\n"
"   Value of specified extended attribute.\n"
);

static PyObject *py_glfs_fd_fgetxattr(PyObject *obj,
				      PyObject *args,
				      PyObject *no_kwargs)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	const char *attr = NULL;
	Py_ssize_t i;
	PyObject *buffer = NULL;

	if (!PyArg_ParseTuple(args, "s", &attr)) {
		return NULL;
	}

	for (i = 0; ; i++) {
		void *ptr;
		ssize_t result;
		static const Py_ssize_t buffer_sizes[] = {128, XATTR_SIZE_MAX, 0};
		Py_ssize_t buffer_size = buffer_sizes[i];

		if (!buffer_size) {
			set_glfs_exc("glfs_fgetxattr()");
			return NULL;
		}

		buffer = PyBytes_FromStringAndSize(NULL, buffer_size);
		if (!buffer)
			return NULL;

		ptr = PyBytes_AS_STRING(buffer);

		Py_BEGIN_ALLOW_THREADS;
		result = glfs_fgetxattr(self->fd, attr, ptr, buffer_size);
		Py_END_ALLOW_THREADS;

		if (result < 0) {
			Py_DECREF(buffer);
			if (errno == ERANGE)
				continue;

			set_glfs_exc("glfs_fgetxattr()");
			return NULL;
		}

		if (result != buffer_size) {
			/* Can only shrink. */
			_PyBytes_Resize(&buffer, result);
		}
		break;
	}

	return buffer;
}

PyDoc_STRVAR(py_glfs_fd_fsetxattr__doc__,
"fsetxattr(xattr_name, xattr_value, flags)\n"
"--\n\n"
"Sets value of the specified extended attribute associated with the given "
"glusterfs file descriptor object.\n\n"
"Parameters\n"
"----------\n"
"xattr_name : str\n"
"    Name of the extended attribute to set.\n"
"xattr_value : bytes\n"
"    Value of the extended attribute to set.\n"
"flags : int\n"
"    flags may be XATTR_REPLACE or XATTR_CREATE. For description\n"
"    of these flags see manpage for fsetxattr(2).\n\n"
"Returns\n"
"-------\n"
"None\n"
);

static PyObject *py_glfs_fd_fsetxattr(PyObject *obj,
				      PyObject *args,
				      PyObject *no_kwargs)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	PyObject *buf = NULL;
	ssize_t result = -1;
	const char *attr = NULL;
	int flags;
	Py_buffer value = {NULL, NULL};

	if (!PyArg_ParseTuple(args, "sOi", &attr, &buf, &flags)) {
		return NULL;
	}

	if (!PyObject_CheckBuffer(buf)) {
		PyErr_SetString(
			PyExc_TypeError,
			"not a buffer."
		);
		goto cleanup;
	}

	if (PyObject_GetBuffer(buf, &value, PyBUF_SIMPLE) != 0) {
		goto cleanup;
	}

	if (!PyBuffer_IsContiguous(&value, 'C')) {
		PyErr_SetString(
			PyExc_TypeError,
			"buffer must be contiguous."
		);
		goto cleanup;
	}

	Py_BEGIN_ALLOW_THREADS;
	result = glfs_fsetxattr(self->fd, attr,
				value.buf, value.len, flags);
	Py_END_ALLOW_THREADS;

	if (result) {
		set_glfs_exc("glfs_fsetxattr()");
	}

cleanup:
	if (value.obj) {
		PyBuffer_Release(&value);
	}

	if (result == -1) {
		return NULL;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(py_glfs_fd_fremovexattr__doc__,
"fremovexattr(xattr_name)\n"
"--\n\n"
"Remove the specified extended attribute associated with the given "
"glusterfs file descriptor object.\n\n"
"Parameters\n"
"----------\n"
"xattr_name : str\n"
"    Name of the extended attribute to remove.\n\n"
"Returns\n"
"-------\n"
"None\n"

);

static PyObject *py_glfs_fd_fremovexattr(PyObject *obj,
					 PyObject *args,
					 PyObject *no_kwargs)
{
	py_glfs_fd_t *self = (py_glfs_fd_t *)obj;
	const char *attr = NULL;
	int err;

	if (!PyArg_ParseTuple(args, "s", &attr)) {
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS;
	err = glfs_fremovexattr(self->fd, attr);
	Py_END_ALLOW_THREADS;

	if (err) {
		set_glfs_exc("glfs_fremovexattr()");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyMethodDef py_glfs_fd_methods[] = {
	{
		.ml_name = "fstat",
		.ml_meth = (PyCFunction)py_glfs_fd_fstat,
		.ml_flags = METH_NOARGS,
		.ml_doc = py_glfs_fd_fstat__doc__
	},
	{
		.ml_name = "fsync",
		.ml_meth = (PyCFunction)py_glfs_fd_fsync,
		.ml_flags = METH_NOARGS,
		.ml_doc = py_glfs_fd_fsync__doc__
	},
	{
		.ml_name = "fchdir",
		.ml_meth = (PyCFunction)py_glfs_fd_fchdir,
		.ml_flags = METH_NOARGS,
		.ml_doc = py_glfs_fd_fchdir__doc__
	},
	{
		.ml_name = "fchmod",
		.ml_meth = (PyCFunction)py_glfs_fd_fchmod,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_fchmod__doc__
	},
	{
		.ml_name = "fchown",
		.ml_meth = (PyCFunction)py_glfs_fd_fchown,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_fchown__doc__
	},
	{
		.ml_name = "ftruncate",
		.ml_meth = (PyCFunction)py_glfs_fd_ftruncate,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_ftruncate__doc__
	},
	{
		.ml_name = "lseek",
		.ml_meth = (PyCFunction)py_glfs_fd_lseek,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_lseek__doc__
	},
	{
		.ml_name = "pread",
		.ml_meth = (PyCFunction)py_glfs_fd_pread,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_pread__doc__
	},
	{
		.ml_name = "pwrite",
		.ml_meth = (PyCFunction)py_glfs_fd_pwrite,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_pwrite__doc__
	},
	{
		.ml_name = "posix_lock",
		.ml_meth = (PyCFunction)py_glfs_fd_posix_lock,
		.ml_flags = METH_VARARGS | METH_KEYWORDS,
		.ml_doc = py_glfs_fd_posix_lock__doc__
	},
	{
		.ml_name = "flistxattr",
		.ml_meth = (PyCFunction)py_glfs_fd_flistxattr,
		.ml_flags = METH_NOARGS,
		.ml_doc = py_glfs_fd_flistxattr__doc__
	},
	{
		.ml_name = "fgetxattr",
		.ml_meth = (PyCFunction)py_glfs_fd_fgetxattr,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_fgetxattr__doc__
	},
	{
		.ml_name = "fsetxattr",
		.ml_meth = (PyCFunction)py_glfs_fd_fsetxattr,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_fsetxattr__doc__
	},
	{
		.ml_name = "fremovexattr",
		.ml_meth = (PyCFunction)py_glfs_fd_fremovexattr,
		.ml_flags = METH_VARARGS,
		.ml_doc = py_glfs_fd_fremovexattr__doc__
	},
	{ NULL, NULL, 0, NULL }
};

static PyGetSetDef py_glfs_fd_getsetters[] = {
	{ .name = NULL }
};

PyTypeObject PyGlfsFd = {
	.tp_name = "pyglfs.FD",
	.tp_basicsize = sizeof(py_glfs_fd_t),
	.tp_methods = py_glfs_fd_methods,
	.tp_getset = py_glfs_fd_getsetters,
	.tp_new = py_glfs_fd_new,
	.tp_init = py_glfs_fd_init,
	.tp_doc = "Glusterfs file handle",
	.tp_dealloc = (destructor)py_glfs_fd_dealloc,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
};
