/*	_NSMonitor.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#include <Carbon/Carbon.h>

#import <Foundation/Foundation.h>

// NSMonitor ----------------------------------------------------------------

#if defined(__MACH__)
#import <mach/message.h>
#include <pthread.h>
#import <sys/time.h>
#endif
#if defined(__WIN32__)
#include <windows.h>
#include <process.h>
#import <sys/utime.h>
#endif

#if defined(__MACH__)
    #define F_STRUCT_MUTEX_T pthread_mutex_t
    #define F_STRUCT_COND_T pthread_cond_t
#else
    #define F_STRUCT_MUTEX_T CRITICAL_SECTION
    /* Due to the fact that the unblock-and-wait is not atomic in the
        condition wait, we need to be a little tricky in broadcasting. 
        We make wake up a bit more than we should, but that should
        be harmless, since a condition should be being checked. */
    struct _F_wincond {
        HANDLE aevent;
        HANDLE mevent;
    };
    #define F_STRUCT_COND_T struct _F_wincond
#endif

@interface NSMonitor : NSObject <NSLocking> {
    F_STRUCT_MUTEX_T mutex;
    F_STRUCT_COND_T cond;
    unsigned int value;
}

-(id)init;
-(void)wait;
-(BOOL)waitUntilDate:(NSDate *)limit;
-(BOOL)waitInterval:(NSTimeInterval)ti;
-(void)signal;
-(void)broadcast;

@end
