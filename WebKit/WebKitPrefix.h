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
#import <QD/ATSUnicodePriv.h>

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#import <Foundation/NSPrivateDecls.h>
#endif

#endif
