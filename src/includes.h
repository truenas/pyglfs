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

#ifndef _INCLUDES_H_
#define _INCLUDES_H_

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <bsd/string.h>
#include <glusterfs/api/glfs.h>
#include <glusterfs/api/glfs-handles.h>
#include <uuid/uuid.h>

#define discard_const(ptr) ((void *)((uintptr_t)(ptr)))
#define discard_const_p(type, ptr) ((type *)discard_const(ptr))

#define __STRING(x) #x
#define __STRINGSTRING(x) __STRING(x)
#define __LINESTR__ __STRINGSTRING(__LINE__)
#define __location__ __FILE__ ":" __LINESTR__
#endif /* _INCLUDES_H_ */
