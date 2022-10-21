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
