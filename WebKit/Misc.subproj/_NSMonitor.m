/*	NSMonitor.m
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>
#import <Foundation/NSPrivateDecls.h>
#import "_NSMonitor.h"
#import <objc/objc-class.h>
#import <objc/objc-runtime.h>

#if defined(__MACH__)
    // moved to .h file #define F_STRUCT_MUTEX_T pthread_mutex_t
    #define F_MUTEX_INIT(m) pthread_mutex_init(m, NULL)
    #define F_MUTEX_FINI(m) pthread_mutex_destroy(m)
    #define F_MUTEX_LOCK(m) pthread_mutex_lock(m)
    #define F_MUTEX_TRYLOCK(m) (EBUSY != pthread_mutex_trylock(m))
    #define F_MUTEX_UNLOCK(m) pthread_mutex_unlock(m)
    // moved to .h file #define F_STRUCT_COND_T pthread_cond_t
    #define F_COND_INIT(c) pthread_cond_init(c, NULL)
    #define F_COND_FINI(c) pthread_cond_destroy(c)
    #define F_CONDITION_WAIT(c, m) pthread_cond_wait(c, m)
    #define F_CONDITION_SIGNAL(c) pthread_cond_signal(c)
    #define F_CONDITION_BROADCAST(c) pthread_cond_broadcast(c)
    /* Returns 0 on timeout, 1 on regular return; t is double seconds */
    static int F_CONDITION_WAIT_TIMEOUT(F_STRUCT_COND_T *c, F_STRUCT_MUTEX_T *m, NSTimeInterval t, unsigned int *value) {
        struct timespec ts;
        int result;
        CFAbsoluteTime at = CFAbsoluteTimeGetCurrent() + t;
        ts.tv_sec = (time_t)(floor(at) + kCFAbsoluteTimeIntervalSince1970);
        ts.tv_nsec = (int32_t)((at - floor(at)) * 1000000000.0);
        result = pthread_cond_timedwait(c, m, &ts);
        if (ETIMEDOUT != result) {
            (*value)--;
            return 1;
        }
        return 0;
    }
#else
    // moved to .h file #define F_STRUCT_MUTEX_T CRITICAL_SECTION
    #define F_MUTEX_INIT(m) InitializeCriticalSection(m)
    #define F_MUTEX_FINI(m) DeleteCriticalSection(m)
    #define F_MUTEX_LOCK(m) EnterCriticalSection(m)
    #define F_MUTEX_TRYLOCK(m) (0 != TryEnterCriticalSection(m))
    #define F_MUTEX_UNLOCK(m) (LeaveCriticalSection(m), 0)
    // moved to .h file #define F_STRUCT_COND_T struct _F_wincond
    #define F_COND_INIT(c) do { \
        (c)->aevent = CreateEvent(NULL, FALSE, FALSE, NULL); \
        (c)->mevent = CreateEvent(NULL, TRUE, FALSE, NULL); } while (0)
    #define F_COND_FINI(c) do { \
        CloseHandle((c)->aevent); CloseHandle((c)->mevent); } while (0)
    #define F_CONDITION_WAIT(c, m) do { \
        HANDLE _h_[2] = {(c)->aevent, (c)->mevent}; \
        F_MUTEX_UNLOCK(m); \
        WaitForMultipleObjects(2, _h_, FALSE, INFINITE); \
        F_MUTEX_LOCK(m); } while (0)
    #define F_CONDITION_SIGNAL(c) SetEvent((c)->aevent)
    #define F_CONDITION_BROADCAST(c) do { \
        PulseEvent((c)->mevent); SetEvent((c)->aevent);} while (0)
    /* Returns 0 on timeout, 1 on regular return; t is double seconds */
    static int F_CONDITION_WAIT_TIMEOUT(F_STRUCT_COND_T *c, F_STRUCT_MUTEX_T *m, NSTimeInterval t, unsigned int *value) {
        HANDLE _h_[2] = {(c)->aevent, (c)->mevent};
        DWORD timeout, result;
        t *= 1000.0;
        while (0.0 <= t) {
            timeout = (INT_MAX <= t) ? INT_MAX : (DWORD)t;
            F_MUTEX_UNLOCK(m);
            result = WaitForMultipleObjects(2, _h_, FALSE, timeout);
            F_MUTEX_LOCK(m);
            if (WAIT_TIMEOUT != result) {
                (*value)--;
                return 1;
            }
            t -= timeout;
            /* shave off an extra 1us to account for overhead,
            plus make loop terminate if t falls to 0.0 */
            t -= 0.001;
        }
        return 0;
    }
#endif


@implementation NSMonitor

- init {
    F_MUTEX_INIT(&mutex);
    F_COND_INIT(&cond);
    value = 0;
    return self;
}

- (void)dealloc {
    F_MUTEX_FINI(&mutex);
    F_COND_FINI(&cond);
    [super dealloc];
}

- (void)lock {
    F_MUTEX_LOCK(&mutex);
}

- (void)unlock {
    F_MUTEX_UNLOCK(&mutex);
}

- (void)wait {    
    while (value == 0) {
        F_CONDITION_WAIT(&cond, &mutex);
    }
    value--;
}

- (BOOL)waitUntilDate:(NSDate *)limit {
    NSTimeInterval ti = [limit timeIntervalSinceNow];
    return [self waitInterval:ti];
}

- (BOOL)waitInterval:(NSTimeInterval)ti {
    BOOL result;
    if (ti < 0.0) {
        result = NO;
    }
    else {
        result = F_CONDITION_WAIT_TIMEOUT(&cond, &mutex, ti, &value);
    }
    return result;
}

- (void)signal {
    value++;
    F_CONDITION_SIGNAL(&cond);
}

- (void)broadcast {
    value++;
    F_CONDITION_BROADCAST(&cond);
}

- (NSString *)description {
    return [(id)CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s(0x%x)"), "_NSMonitor", self) autorelease];
}

@end
