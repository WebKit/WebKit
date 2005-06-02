#ifdef __cplusplus
#define NULL __null
#else
#define NULL ((void *)0)
#endif

#import <stddef.h>
#import <stdio.h>
#import <fcntl.h>
#import <errno.h>
#import <unistd.h>
#import <signal.h>
#import <sys/types.h>
#import <sys/time.h>
#import <sys/resource.h>

#import <pthread.h>

#import <CoreGraphics/CoreGraphics.h>

#ifdef __cplusplus

#include <cstddef>
#include <new>

#else

#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif

#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_3
#define BUILDING_ON_PANTHER 1
#endif

#if BUILDING_ON_PANTHER
#define OMIT_TIGER_FEATURES 1
#endif
