#include <config.h>

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __cplusplus

#include <cstddef>
// FIXME: We really need to precompile iostream, but we can't until Radar 2920556 is fixed.
//#include <iostream>
#include <new>

#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#ifdef __OBJC__

#import <Foundation/NSURLPathUtilities.h>

#import <Cocoa/Cocoa.h>

#endif
