#ifdef __cplusplus
#define NULL __null
#else
#define NULL ((void *)0)
#endif

#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <regex.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus

#include <algorithm>
#include <cstddef>
#include <new>

#ifndef NDEBUG
#include <ostream>
#endif

#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>

#endif
