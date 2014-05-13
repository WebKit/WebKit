/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebCoreThread.h"

#if PLATFORM(IOS)

#import "FloatingPointEnvironment.h"
#import "JSDOMWindowBase.h"
#import "ThreadGlobalData.h"
#import "RuntimeApplicationChecksIOS.h"
#import "WebCoreThreadInternal.h"
#import "WebCoreThreadMessage.h"
#import "WebCoreThreadRun.h"
#import "WebCoreThreadSafe.h"
#import "WKUtilities.h"

#import <runtime/InitializeThreading.h>
#import <runtime/JSLock.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/Threading.h>
#import <wtf/text/AtomicString.h>

#import <CoreFoundation/CFPriv.h>
#import <Foundation/NSInvocation.h>
#import <libkern/OSAtomic.h>
#import <objc/runtime.h>

#define LOG_MESSAGES 0
#define LOG_WEB_LOCK 0
#define LOG_MAIN_THREAD_LOCKING 0
#define LOG_RELEASES 0

#define DistantFuture   (86400.0 * 2000 * 365.2425 + 86400.0)   // same as +[NSDate distantFuture]
#define MaxArgCount         5
#define DrawWaitInterval     10
#define DelegateWaitInterval 10

static void _WebThreadAutoLock(void);
static bool _WebTryThreadLock(bool shouldTry);
static void _WebThreadLockFromAnyThread(bool shouldLog);
static void _WebThreadUnlock(void);

@interface NSObject(ForwardDeclarations)
-(void)_webcore_releaseOnWebThread;
-(void)_webcore_releaseWithWebThreadLock;
@end

@implementation NSObject(WebCoreThreadAdditions)

- (void)releaseOnMainThread {
    if ([NSThread isMainThread]) {
        [self release];
    } else {
        [self performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    }
}

@end

typedef void *NSAutoreleasePoolMark;
#ifdef __cplusplus
extern "C" {
#endif
extern NSAutoreleasePoolMark NSPushAutoreleasePool(unsigned ignored);
extern void NSPopAutoreleasePool(NSAutoreleasePoolMark token);
#ifdef __cplusplus
}
#endif

static int WebTimedConditionLock (pthread_cond_t *condition, pthread_mutex_t *lock, CFAbsoluteTime interval);

static pthread_mutex_t webLock;
static NSAutoreleasePoolMark autoreleasePoolMark;
static CFRunLoopRef webThreadRunLoop;
static NSRunLoop* webThreadNSRunLoop;
static pthread_t webThread;
static BOOL isWebThreadLocked;
static BOOL webThreadStarted;
static unsigned webThreadLockCount;

static NSAutoreleasePoolMark savedAutoreleasePoolMark;
static BOOL isNestedWebThreadRunLoop;
typedef enum {
    PushOrPopAutoreleasePool,
    IgnoreAutoreleasePool
} AutoreleasePoolOperation;

static pthread_mutex_t WebThreadReleaseLock = PTHREAD_MUTEX_INITIALIZER;
static CFRunLoopSourceRef WebThreadReleaseSource;
static CFMutableArrayRef WebThreadReleaseObjArray;

static void MainThreadAdoptAndRelease(id obj);

static pthread_mutex_t delegateLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t delegateCondition = PTHREAD_COND_INITIALIZER;
static NSInvocation *delegateInvocation;
static CFRunLoopSourceRef delegateSource = NULL;
static BOOL delegateHandled;
#if LOG_MAIN_THREAD_LOCKING
static BOOL sendingDelegateMessage;
#endif

static CFRunLoopObserverRef mainRunLoopAutoUnlockObserver;

static pthread_mutex_t startupLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t startupCondition = PTHREAD_COND_INITIALIZER;

static WebThreadContext *webThreadContext;
static pthread_key_t threadContextKey;
static unsigned mainThreadLockCount;
static unsigned otherThreadLockCount;
static unsigned sMainThreadModalCount;

volatile bool webThreadShouldYield;

static pthread_mutex_t WebCoreReleaseLock;
static void WebCoreObjCDeallocOnWebThreadImpl(id self, SEL _cmd);
static void WebCoreObjCDeallocWithWebThreadLockImpl(id self, SEL _cmd);

static NSMutableArray *sAsyncDelegates = nil;

static CFStringRef delegateSourceRunLoopMode;

static inline void SendMessage(NSInvocation *invocation)
{
    [invocation invoke];
    MainThreadAdoptAndRelease(invocation);
}

static void HandleDelegateSource(void *info)
{
    UNUSED_PARAM(info);
    ASSERT(!WebThreadIsCurrent());

#if LOG_MAIN_THREAD_LOCKING
    sendingDelegateMessage = YES;
#endif

    _WebThreadAutoLock();

    int result = pthread_mutex_lock(&delegateLock);
    ASSERT_WITH_MESSAGE(result == 0, "delegate lock failed with code:%d", result);

#if LOG_MESSAGES
    if ([[delegateInvocation target] isKindOfClass:[NSNotificationCenter class]]) {
        id argument0;
        [delegateInvocation getArgument:&argument0 atIndex:0];
        NSLog(@"notification receive: %@", argument0);
    } else {
        NSLog(@"delegate receive: %@", NSStringFromSelector([delegateInvocation selector]));
    }
#endif

    SendMessage(delegateInvocation);

    delegateHandled = YES;
    pthread_cond_signal(&delegateCondition);

    result = pthread_mutex_unlock(&delegateLock);
    ASSERT_WITH_MESSAGE(result == 0, "delegate unlock failed with code:%d", result);

#if LOG_MAIN_THREAD_LOCKING
    sendingDelegateMessage = NO;
#endif
}

static void SendDelegateMessage(NSInvocation *invocation)
{
    if (WebThreadIsCurrent()) {
        ASSERT(delegateSource);
        int result = pthread_mutex_lock(&delegateLock);
        ASSERT_WITH_MESSAGE(result == 0, "delegate lock failed with code:%d", result);

        delegateInvocation = invocation;
        delegateHandled = NO;

#if LOG_MESSAGES
        if ([[delegateInvocation target] isKindOfClass:[NSNotificationCenter class]]) {
            id argument0;
            [delegateInvocation getArgument:&argument0 atIndex:0];
            NSLog(@"notification send: %@", argument0);
        } else {
            NSLog(@"delegate send: %@", NSStringFromSelector([delegateInvocation selector]));
        }
#endif

        {
            // Code block created to scope JSC::JSLock::DropAllLocks outside of WebThreadLock()
            JSC::JSLock::DropAllLocks dropAllLocks(WebCore::JSDOMWindowBase::commonVM());
            _WebThreadUnlock();

            CFRunLoopSourceSignal(delegateSource);
            CFRunLoopWakeUp(CFRunLoopGetMain());

            while (!delegateHandled) {
                if (WebTimedConditionLock(&delegateCondition, &delegateLock, DelegateWaitInterval) != 0) {
                    id delegateInformation;
                    if ([[delegateInvocation target] isKindOfClass:[NSNotificationCenter class]])
                        [delegateInvocation getArgument:&delegateInformation atIndex:0];
                    else
                        delegateInformation = NSStringFromSelector([delegateInvocation selector]);
        
                    CFStringRef mode = CFRunLoopCopyCurrentMode(CFRunLoopGetMain());
                    NSLog(@"%s: delegate (%@) failed to return after waiting %d seconds. main run loop mode: %@", __PRETTY_FUNCTION__, delegateInformation, DelegateWaitInterval, mode);
                    if (mode)
                        CFRelease(mode);
                }
            }
            result = pthread_mutex_unlock(&delegateLock);

            ASSERT_WITH_MESSAGE(result == 0, "delegate unlock failed with code:%d", result);
            _WebTryThreadLock(false);
        }
    } else {
        SendMessage(invocation);
    }
}

void WebThreadRunOnMainThread(void(^delegateBlock)())
{
    if (!WebThreadIsCurrent()) {
        ASSERT(pthread_main_np());
        delegateBlock();
        return;
    }

    JSC::JSLock::DropAllLocks dropAllLocks(WebCore::JSDOMWindowBase::commonVM());
    _WebThreadUnlock();

    void (^delegateBlockCopy)() = Block_copy(delegateBlock);
    dispatch_sync(dispatch_get_main_queue(), delegateBlockCopy);
    Block_release(delegateBlockCopy);

    _WebTryThreadLock(false);
}

static void MainThreadAdoptAndRelease(id obj)
{
    if (!WebThreadIsEnabled() || CFRunLoopGetMain() == CFRunLoopGetCurrent()) {
        [obj release];
        return;
    }
#if LOG_RELEASES
    NSLog(@"Release send [web thread] : %@", obj);
#endif
    // We own obj at this point, so we don't need the block to implicitly
    // retain it.
    __block id objNotRetained = obj;
    dispatch_async(dispatch_get_main_queue(), ^{
        [objNotRetained release];
    });
}

void WebThreadAdoptAndRelease(id obj)
{
    ASSERT(!WebThreadIsCurrent());
    ASSERT(WebThreadReleaseSource);

#if LOG_RELEASES
    NSLog(@"Release send [main thread]: %@", obj);
#endif        

    int result = pthread_mutex_lock(&WebThreadReleaseLock);
    ASSERT_WITH_MESSAGE(result == 0, "Release lock failed with code:%d", result);

    if (WebThreadReleaseObjArray == nil)
        WebThreadReleaseObjArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
    CFArrayAppendValue(WebThreadReleaseObjArray, obj);
    CFRunLoopSourceSignal(WebThreadReleaseSource);
    CFRunLoopWakeUp(webThreadRunLoop);

    result = pthread_mutex_unlock(&WebThreadReleaseLock);    
    ASSERT_WITH_MESSAGE(result == 0, "Release unlock failed with code:%d", result);
}

static inline void lockWebCoreReleaseLock()
{
    int lockcode = pthread_mutex_lock(&WebCoreReleaseLock);
#pragma unused (lockcode)
    ASSERT_WITH_MESSAGE(lockcode == 0, "WebCoreReleaseLock lock failed with code:%d", lockcode);
}

static inline void unlockWebCoreReleaseLock()
{
    int lockcode = pthread_mutex_unlock(&WebCoreReleaseLock);
#pragma unused (lockcode)
    ASSERT_WITH_MESSAGE(lockcode == 0, "WebCoreReleaseLock unlock failed with code:%d", lockcode);
}

void WebCoreObjCDeallocOnWebThread(Class cls)
{
    SEL releaseSEL = @selector(release);
    SEL webThreadReleaseSEL = @selector(_webcore_releaseOnWebThread);

    // get the existing release method
    Method releaseMethod = class_getInstanceMethod(cls, releaseSEL);
    if (!releaseMethod) {
        ASSERT_WITH_MESSAGE(releaseMethod, "WebCoreObjCDeallocOnWebThread() failed to find %s for %@", releaseSEL, NSStringFromClass(cls));
        return;
    }

    // add the implementation that ensures release WebThread release/deallocation
    if (!class_addMethod(cls, webThreadReleaseSEL, (IMP)WebCoreObjCDeallocOnWebThreadImpl, method_getTypeEncoding(releaseMethod))) {
        ASSERT_WITH_MESSAGE(releaseMethod, "WebCoreObjCDeallocOnWebThread() failed to add %s for %@", webThreadReleaseSEL, NSStringFromClass(cls));
        return;
    }

    // ensure the implementation exists at cls in the class hierarchy
    if (class_addMethod(cls, releaseSEL, class_getMethodImplementation(cls, releaseSEL), method_getTypeEncoding(releaseMethod)))
        releaseMethod = class_getInstanceMethod(cls, releaseSEL);

    // swizzle the old release for the new implementation
    method_exchangeImplementations(releaseMethod, class_getInstanceMethod(cls, webThreadReleaseSEL));
}

void WebCoreObjCDeallocWithWebThreadLock(Class cls)
{
    SEL releaseSEL = @selector(release);
    SEL webThreadLockReleaseSEL = @selector(_webcore_releaseWithWebThreadLock);

    // get the existing release method
    Method releaseMethod = class_getInstanceMethod(cls, releaseSEL);
    if (!releaseMethod) {
        ASSERT_WITH_MESSAGE(releaseMethod, "WebCoreObjCDeallocWithWebThreadLock() failed to find %s for %@", releaseSEL, NSStringFromClass(cls));
        return;
    }

    // add the implementation that ensures release WebThreadLock release/deallocation
    if (!class_addMethod(cls, webThreadLockReleaseSEL, (IMP)WebCoreObjCDeallocWithWebThreadLockImpl, method_getTypeEncoding(releaseMethod))) {
        ASSERT_WITH_MESSAGE(releaseMethod, "WebCoreObjCDeallocWithWebThreadLock() failed to add %s for %@", webThreadLockReleaseSEL, NSStringFromClass(cls));
        return;
    }

    // ensure the implementation exists at cls in the class hierarchy
    if (class_addMethod(cls, releaseSEL, class_getMethodImplementation(cls, releaseSEL), method_getTypeEncoding(releaseMethod)))
        releaseMethod = class_getInstanceMethod(cls, releaseSEL);

    // swizzle the old release for the new implementation
    method_exchangeImplementations(releaseMethod, class_getInstanceMethod(cls, webThreadLockReleaseSEL));
}

void WebCoreObjCDeallocOnWebThreadImpl(id self, SEL _cmd)
{
    UNUSED_PARAM(_cmd);
    if (!WebThreadIsEnabled())
        [self _webcore_releaseOnWebThread];
    else {
        lockWebCoreReleaseLock();
        if ([self retainCount] == 1) {
            // This is the only reference retaining the object, so we can
            // safely release the WebCoreReleaseLock now.
            unlockWebCoreReleaseLock();
            if (WebThreadIsCurrent())
                [self _webcore_releaseOnWebThread];
            else
                WebThreadAdoptAndRelease(self);
        } else {
            // This is not the only reference retaining the object, so another
            // thread could also call release - hold the lock whilst calling
            // release to avoid a race condition.
            [self _webcore_releaseOnWebThread];
            unlockWebCoreReleaseLock();
        }
    }
}

void WebCoreObjCDeallocWithWebThreadLockImpl(id self, SEL _cmd)
{
    UNUSED_PARAM(_cmd);
    lockWebCoreReleaseLock();
    if (WebThreadIsLockedOrDisabled() || 1 != [self retainCount])
        [self _webcore_releaseWithWebThreadLock];
    else
        WebThreadAdoptAndRelease(self);
    unlockWebCoreReleaseLock();
}

static void HandleWebThreadReleaseSource(void *info)
{
    UNUSED_PARAM(info);
    ASSERT(WebThreadIsCurrent());

    int result = pthread_mutex_lock(&WebThreadReleaseLock);
    ASSERT_WITH_MESSAGE(result == 0, "Release lock failed with code:%d", result);

    CFMutableArrayRef objects = NULL;
    if (CFArrayGetCount(WebThreadReleaseObjArray)) {
        objects = CFArrayCreateMutableCopy(NULL, 0, WebThreadReleaseObjArray);
        CFArrayRemoveAllValues(WebThreadReleaseObjArray);
    }

    result = pthread_mutex_unlock(&WebThreadReleaseLock);
    ASSERT_WITH_MESSAGE(result == 0, "Release unlock failed with code:%d", result);

    if (!objects)
        return;

    unsigned count = CFArrayGetCount(objects);
    unsigned i;
    for (i = 0; i < count; i++) {
        id obj = (id)CFArrayGetValueAtIndex(objects, i);
#if LOG_RELEASES
        NSLog(@"Release recv [web thread] : %@", obj);
#endif
        [obj release];
    }

    CFRelease(objects);
}

void WebThreadCallDelegate(NSInvocation *invocation)
{
    // NSInvocation released in SendMessage()
    SendDelegateMessage([invocation retain]);
}

void WebThreadPostNotification(NSString *name, id object, id userInfo)
{
    if (pthread_main_np())
        [[NSNotificationCenter defaultCenter] postNotificationName:name object:object userInfo:userInfo];
    else {
        dispatch_async(dispatch_get_main_queue(), ^ {
            [[NSNotificationCenter defaultCenter] postNotificationName:name object:object userInfo:userInfo];
        });
    }
}

void WebThreadCallDelegateAsync(NSInvocation *invocation)
{
    ASSERT(invocation);
    if (WebThreadIsCurrent())
        [sAsyncDelegates addObject:invocation];
    else
        WebThreadCallDelegate(invocation);
}

// Note: despite the name, returns an autoreleased object.
NSInvocation *WebThreadMakeNSInvocation(id target, SEL selector)
{
    NSMethodSignature *signature = [target methodSignatureForSelector:selector];
    ASSERT(signature);
    if (signature) {
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
        [invocation setSelector:selector];
        [invocation setTarget:target];
        [invocation retainArguments];
        return invocation;
    }
    return nil;
}

static void MainRunLoopAutoUnlock(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *context)
{
    UNUSED_PARAM(observer);
    UNUSED_PARAM(activity);
    UNUSED_PARAM(context);
    ASSERT(!WebThreadIsCurrent());

    if (sMainThreadModalCount != 0)
        return;
    
    CFRunLoopRemoveObserver(CFRunLoopGetCurrent(), mainRunLoopAutoUnlockObserver, kCFRunLoopCommonModes);

    _WebThreadUnlock();
}

static void _WebThreadAutoLock(void)
{
    ASSERT(!WebThreadIsCurrent());

    if (mainThreadLockCount == 0) {
        CFRunLoopAddObserver(CFRunLoopGetCurrent(), mainRunLoopAutoUnlockObserver, kCFRunLoopCommonModes);    
        _WebTryThreadLock(false);
        CFRunLoopWakeUp(CFRunLoopGetMain());
    }
}

static void WebRunLoopLockInternal(AutoreleasePoolOperation poolOperation)
{
    _WebTryThreadLock(false);
    if (poolOperation == PushOrPopAutoreleasePool)
        autoreleasePoolMark = NSPushAutoreleasePool(0);
    isWebThreadLocked = YES;
}

static void WebRunLoopUnlockInternal(AutoreleasePoolOperation poolOperation)
{
    ASSERT(sAsyncDelegates);
    if ([sAsyncDelegates count]) {
        for (NSInvocation* invocation in sAsyncDelegates)
            SendDelegateMessage([invocation retain]);
        [sAsyncDelegates removeAllObjects];
    }

    if (poolOperation == PushOrPopAutoreleasePool)
        NSPopAutoreleasePool(autoreleasePoolMark);

    _WebThreadUnlock();
    isWebThreadLocked = NO;
}

static void WebRunLoopLock(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *context)
{
    UNUSED_PARAM(observer);
    UNUSED_PARAM(context);
    ASSERT(WebThreadIsCurrent());
    ASSERT_UNUSED(activity, activity == kCFRunLoopAfterWaiting || activity == kCFRunLoopBeforeTimers || activity == kCFRunLoopBeforeSources);

    // If the WebThread is locked by the main thread then we want to
    // grab the lock ourselves when the main thread releases the lock.
    if (isWebThreadLocked && !mainThreadLockCount)
        return;
    WebRunLoopLockInternal(PushOrPopAutoreleasePool);
}

static void WebRunLoopUnlock(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *context)
{
    UNUSED_PARAM(observer);
    UNUSED_PARAM(context);
    ASSERT(WebThreadIsCurrent());
    ASSERT_UNUSED(activity, activity == kCFRunLoopBeforeWaiting || activity == kCFRunLoopExit);
    ASSERT(!mainThreadLockCount);

    if (!isWebThreadLocked)
        return;
    WebRunLoopUnlockInternal(PushOrPopAutoreleasePool);
}

static void MainRunLoopUnlockGuard(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *context)
{
    UNUSED_PARAM(observer);
    UNUSED_PARAM(activity);
    UNUSED_PARAM(context);
    ASSERT(!WebThreadIsCurrent());

    // We shouldn't have the web lock at this point.  However, MobileMail sometimes
    // get to a state where the main thread has web lock but it didn't release it on last
    // runloop exit, and web thread gets stuck at waiting for the lock. If this happens,
    // we need to help release the lock.  See <rdar://problem/8005192>.
    if (mainThreadLockCount != 0 && sMainThreadModalCount == 0) {
        NSLog(@"WARNING: Main thread didn't release the lock at last runloop exit!");

        MainRunLoopAutoUnlock(observer, activity, context);
        if (mainThreadLockCount != 0)
            mainThreadLockCount = 0;
    }
}

static void _WebRunLoopEnableNestedFromMainThread()
{
    CFRunLoopRemoveObserver(CFRunLoopGetCurrent(), mainRunLoopAutoUnlockObserver, kCFRunLoopCommonModes);
}

static void _WebRunLoopDisableNestedFromMainThread()
{
    CFRunLoopAddObserver(CFRunLoopGetCurrent(), mainRunLoopAutoUnlockObserver, kCFRunLoopCommonModes); 
}

void WebRunLoopEnableNested()
{
    if (!WebThreadIsEnabled())
        return;

    ASSERT(!isNestedWebThreadRunLoop);

    if (!WebThreadIsCurrent())
        _WebRunLoopEnableNestedFromMainThread();

    savedAutoreleasePoolMark = autoreleasePoolMark;
    autoreleasePoolMark = 0;
    WebRunLoopUnlockInternal(IgnoreAutoreleasePool);
    isNestedWebThreadRunLoop = YES;
}

void WebRunLoopDisableNested()
{
    if (!WebThreadIsEnabled())
        return;

    ASSERT(isNestedWebThreadRunLoop);

    if (!WebThreadIsCurrent())
        _WebRunLoopDisableNestedFromMainThread();

    autoreleasePoolMark = savedAutoreleasePoolMark;
    savedAutoreleasePoolMark = 0;
    WebRunLoopLockInternal(IgnoreAutoreleasePool);
    isNestedWebThreadRunLoop = NO;
}

static void FreeThreadContext(void *threadContext)
{
    if (threadContext != NULL)
       free(threadContext);
}

static void InitThreadContextKey()
{
    pthread_key_create(&threadContextKey, FreeThreadContext);
}

static WebThreadContext *CurrentThreadContext(void)
{
    static pthread_once_t initControl = PTHREAD_ONCE_INIT; 
    pthread_once(&initControl, InitThreadContextKey);    

    WebThreadContext *threadContext = (WebThreadContext*)pthread_getspecific(threadContextKey);
    if (threadContext == NULL) {
        threadContext = (WebThreadContext *)calloc(sizeof(WebThreadContext), 1);
        pthread_setspecific(threadContextKey, threadContext);
    }        
    return threadContext;
}

static
#ifndef __llvm__
NO_RETURN
#endif
void *RunWebThread(void *arg)
{
    FloatingPointEnvironment::shared().propagateMainThreadEnvironment();

    UNUSED_PARAM(arg);
    // WTF::initializeMainThread() needs to be called before JSC::initializeThreading() since the
    // code invoked by the latter needs to know if it's running on the WebThread. See
    // <rdar://problem/8502487>.
    WTF::initializeMainThread();
    WTF::initializeWebThread();
    JSC::initializeThreading();
    
    // Make sure that the WebThread and the main thread share the same ThreadGlobalData objects.
    WebCore::threadGlobalData().setWebCoreThreadData();
    initializeWebThreadIdentifier();

#if HAVE(PTHREAD_SETNAME_NP)
    pthread_setname_np("WebThread");
#endif

    webThreadContext = CurrentThreadContext();
    webThreadRunLoop = CFRunLoopGetCurrent();
    webThreadNSRunLoop = [[NSRunLoop currentRunLoop] retain];

    CFRunLoopObserverRef webRunLoopLockObserverRef = CFRunLoopObserverCreate(NULL, kCFRunLoopBeforeTimers|kCFRunLoopBeforeSources|kCFRunLoopAfterWaiting, YES, 0, WebRunLoopLock, NULL);
    CFRunLoopAddObserver(webThreadRunLoop, webRunLoopLockObserverRef, kCFRunLoopCommonModes);
    CFRelease(webRunLoopLockObserverRef);
    
    WebThreadInitRunQueue();

    // We must have the lock when CA paints in the web thread. CA commits at 2000000 so we use larger order number than that to free the lock.
    CFRunLoopObserverRef webRunLoopUnlockObserverRef = CFRunLoopObserverCreate(NULL, kCFRunLoopBeforeWaiting|kCFRunLoopExit, YES, 2500000, WebRunLoopUnlock, NULL);    
    CFRunLoopAddObserver(webThreadRunLoop, webRunLoopUnlockObserverRef, kCFRunLoopCommonModes);
    CFRelease(webRunLoopUnlockObserverRef);    

    CFRunLoopSourceContext ReleaseSourceContext = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, HandleWebThreadReleaseSource};
    WebThreadReleaseSource = CFRunLoopSourceCreate(NULL, -1, &ReleaseSourceContext);
    CFRunLoopAddSource(webThreadRunLoop, WebThreadReleaseSource, kCFRunLoopDefaultMode);

    int result = pthread_mutex_lock(&startupLock);
    ASSERT_WITH_MESSAGE(result == 0, "startup lock failed with code:%d", result);

    result = pthread_cond_signal(&startupCondition);
    ASSERT_WITH_MESSAGE(result == 0, "startup signal failed with code:%d", result);

    result = pthread_mutex_unlock(&startupLock);
    ASSERT_WITH_MESSAGE(result == 0, "startup unlock failed with code:%d", result);

    while (1)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, DistantFuture, true);

#ifdef __llvm__
    return NULL;
#endif  
}

static void StartWebThread()
{
    webThreadStarted = TRUE;

    // Initialize ThreadGlobalData on the main UI thread so that the WebCore thread
    // can later set it's thread-specific data to point to the same objects.
    WebCore::ThreadGlobalData& unused = WebCore::threadGlobalData();
    (void)unused;

    // Initialize AtomicString on the main thread.
    WTF::AtomicString::init();

    // register class for WebThread deallocation
    WebCoreObjCDeallocOnWebThread([DOMObject class]);
    WebCoreObjCDeallocOnWebThread([WAKWindow class]);
    WebCoreObjCDeallocWithWebThreadLock([WAKView class]);

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&webLock, &mattr);
    pthread_mutexattr_destroy(&mattr);            

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);    
    pthread_mutex_init(&WebCoreReleaseLock, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    CFRunLoopRef runLoop = CFRunLoopGetCurrent();
    CFRunLoopSourceContext delegateSourceContext = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, HandleDelegateSource};
    delegateSource = CFRunLoopSourceCreate(NULL, 0, &delegateSourceContext);
    // We shouldn't get delegate callbacks while scrolling, but there might be
    // one outstanding when we start.  Add the source for all common run loop
    // modes so we don't block the web thread while scrolling.
    if (!delegateSourceRunLoopMode)
        delegateSourceRunLoopMode = kCFRunLoopCommonModes;
    CFRunLoopAddSource(runLoop, delegateSource, delegateSourceRunLoopMode);

    sAsyncDelegates = [[NSMutableArray alloc] init];

    mainRunLoopAutoUnlockObserver = CFRunLoopObserverCreate(NULL, kCFRunLoopBeforeWaiting | kCFRunLoopExit, YES, 3000001, MainRunLoopAutoUnlock, NULL);

    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
    // The web thread is a secondary thread, and secondary threads are usually given
    // a 512 kb stack, but we need more space in order to have room for the JavaScriptCore
    // reentrancy limit. This limit works on both the simulator and the device.
    pthread_attr_setstacksize(&tattr, 200 * 4096);

    struct sched_param param;
    pthread_attr_getschedparam(&tattr, &param);
    param.sched_priority--;
    pthread_attr_setschedparam(&tattr, &param);

    // Wait for the web thread to startup completely before we continue.
    int result = pthread_mutex_lock(&startupLock);
    ASSERT_WITH_MESSAGE(result == 0, "startup lock failed with code:%d", result);

    // Propagate the mainThread's fenv to workers & the web thread.
    FloatingPointEnvironment::shared().saveMainThreadEnvironment();

    pthread_create(&webThread, &tattr, RunWebThread, NULL);
    pthread_attr_destroy(&tattr);

    result = pthread_cond_wait(&startupCondition, &startupLock);
    ASSERT_WITH_MESSAGE(result == 0, "startup wait failed with code:%d", result);

    result = pthread_mutex_unlock(&startupLock);
    ASSERT_WITH_MESSAGE(result == 0, "startup unlock failed with code:%d", result);

    initializeApplicationUIThreadIdentifier();
}

static int WebTimedConditionLock (pthread_cond_t *condition, pthread_mutex_t *lock, CFAbsoluteTime interval)
{
    struct timespec time;
    CFAbsoluteTime at = CFAbsoluteTimeGetCurrent() + interval;
    time.tv_sec = (time_t)(floor(at) + kCFAbsoluteTimeIntervalSince1970);
    time.tv_nsec = (int32_t)((at - floor(at)) * 1000000000.0);        
    return pthread_cond_timedwait(condition, lock, &time);
}


#if LOG_WEB_LOCK || LOG_MAIN_THREAD_LOCKING
static unsigned lockCount;
#endif

static bool _WebTryThreadLock(bool shouldTry)
{
    // Suspend the web thread if the main thread is trying to lock.
    bool onMainThread = pthread_main_np();
    if (onMainThread)
        webThreadShouldYield = true;
    else if (!WebThreadIsCurrent()) {
        NSLog(@"%s, %p: Tried to obtain the web lock from a thread other than the main thread or the web thread. This may be a result of calling to UIKit from a secondary thread. Crashing now...", __PRETTY_FUNCTION__, CurrentThreadContext());
        CRASH();
    }
        
            
    int result;
    bool busy = false;
    if (shouldTry) {
        result = pthread_mutex_trylock(&webLock);
        if (result == EBUSY) {
            busy = true;
        } else
            ASSERT_WITH_MESSAGE(result == 0, "try web lock failed with code:%d", result);
    }
    else {
        result = pthread_mutex_lock(&webLock);
        ASSERT_WITH_MESSAGE(result == 0, "web lock failed with code:%d", result);
    }
    
    if (!busy) {
#if LOG_WEB_LOCK || LOG_MAIN_THREAD_LOCKING
        lockCount++;
#if LOG_WEB_LOCK
        NSLog(@"lock   %d, web-thread: %d", lockCount, WebThreadIsCurrent());
#endif
#endif
        if (onMainThread) {
            ASSERT(CFRunLoopGetCurrent() == CFRunLoopGetMain());
            webThreadShouldYield = false;
            mainThreadLockCount++;
#if LOG_MAIN_THREAD_LOCKING
            if (!sendingDelegateMessage && lockCount == 1)
                NSLog(@"Main thread locking outside of delegate messages.");
#endif
        } else {
            webThreadLockCount++;
            if (webThreadLockCount > 1) {
                NSLog(@"%s, %p: Multiple locks on web thread not allowed! Please file a bug. Crashing now...", __PRETTY_FUNCTION__, CurrentThreadContext());
                CRASH();
            }
        }
    }
    
    return !busy;
}

void WebThreadLock(void)
{
    if (!webThreadStarted || pthread_equal(webThread, pthread_self()))
        return;
    _WebThreadAutoLock();
}

void WebThreadUnlock(void)
{
    // This is a no-op, we unlock automatically on top of the runloop
    ASSERT(!WebThreadIsCurrent());
}

void WebThreadLockFromAnyThread()
{
    _WebThreadLockFromAnyThread(true);
}
    
void WebThreadLockFromAnyThreadNoLog()
{
    _WebThreadLockFromAnyThread(false);
}

static void _WebThreadLockFromAnyThread(bool shouldLog)
{
    if (!webThreadStarted)
        return;
    ASSERT(!WebThreadIsCurrent());
    if (pthread_main_np()) {
        _WebThreadAutoLock();
        return;
    }
    if (shouldLog)
        NSLog(@"%s, %p: Obtaining the web lock from a thread other than the main thread or the web thread. UIKit should not be called from a secondary thread.", __PRETTY_FUNCTION__, CurrentThreadContext());

    pthread_mutex_lock(&webLock);
    
    // This used for any thread other than the web thread.
    otherThreadLockCount++;
    webThreadShouldYield = false;
}

void WebThreadUnlockFromAnyThread()
{
    if (!webThreadStarted)
        return;
    ASSERT(!WebThreadIsCurrent());
    // No-op except from a secondary thread.
    if (pthread_main_np())
        return;
    
    ASSERT(otherThreadLockCount);
    otherThreadLockCount--;
    
    int result;
    result = pthread_mutex_unlock(&webLock); 
    ASSERT_WITH_MESSAGE(result == 0, "web unlock failed with code:%d", result);
}

void WebThreadUnlockGuardForMail()
{
    ASSERT(!WebThreadIsCurrent());

    CFRunLoopObserverRef mainRunLoopUnlockGuardObserver = CFRunLoopObserverCreate(NULL, kCFRunLoopEntry, YES, 0, MainRunLoopUnlockGuard, NULL);
    CFRunLoopAddObserver(CFRunLoopGetMain(), mainRunLoopUnlockGuardObserver, kCFRunLoopCommonModes);
    CFRelease(mainRunLoopUnlockGuardObserver);
}

void _WebThreadUnlock(void)
{
#if LOG_WEB_LOCK || LOG_MAIN_THREAD_LOCKING
    lockCount--;
#if LOG_WEB_LOCK
    NSLog(@"unlock %d, web-thread: %d", lockCount, WebThreadIsCurrent());
#endif
#endif
    
    if (!WebThreadIsCurrent()) {
        ASSERT(mainThreadLockCount != 0);
        mainThreadLockCount--;
    } else {    
        webThreadLockCount--;
        webThreadShouldYield = false;
    }
    
    int result;
    result = pthread_mutex_unlock(&webLock); 
    ASSERT_WITH_MESSAGE(result == 0, "web unlock failed with code:%d", result);    
}

bool WebThreadIsLocked(void)
{
    if (WebThreadIsCurrent())
        return webThreadLockCount;
    else if (pthread_main_np())
        return mainThreadLockCount;
    else
        return otherThreadLockCount;
}

bool WebThreadIsLockedOrDisabled(void)
{
    return !WebThreadIsEnabled() || WebThreadIsLocked();
}

void WebThreadLockPushModal(void)
{
    if (WebThreadIsCurrent())
        return;
    
    ASSERT(WebThreadIsLocked());
    ++sMainThreadModalCount;
}

void WebThreadLockPopModal(void)
{
    if (WebThreadIsCurrent())
        return;
    
    ASSERT(WebThreadIsLocked());
    ASSERT(sMainThreadModalCount != 0);
    --sMainThreadModalCount;
}

CFRunLoopRef WebThreadRunLoop(void)
{
    if (webThreadStarted) {
        ASSERT(webThreadRunLoop);
        return webThreadRunLoop;
    }
    
    return CFRunLoopGetCurrent();
}

NSRunLoop* WebThreadNSRunLoop(void)
{
    if (webThreadStarted) {
        ASSERT(webThreadNSRunLoop);
        return webThreadNSRunLoop;
    }

    return [NSRunLoop currentRunLoop];
}

WebThreadContext *WebThreadCurrentContext(void)
{
    return CurrentThreadContext();
}

bool WebThreadContextIsCurrent(void)
{   
    return WebThreadCurrentContext() == webThreadContext;
}

void WebThreadSetDelegateSourceRunLoopMode(CFStringRef mode)
{
    ASSERT(!webThreadStarted);
    delegateSourceRunLoopMode = mode;
}

void WebThreadEnable(void)
{
    RELEASE_ASSERT_WITH_MESSAGE(!WebCore::applicationIsWebProcess(), "The WebProcess should never run a Web Thread");

    static pthread_once_t initControl = PTHREAD_ONCE_INIT;
    pthread_once(&initControl, StartWebThread);
}

bool WebThreadIsEnabled(void)
{
    return webThreadStarted;
}

bool WebThreadIsCurrent(void)
{
    return webThreadStarted && pthread_equal(webThread, pthread_self());
}

bool WebThreadNotCurrent(void)
{
    return webThreadStarted && !pthread_equal(webThread, pthread_self());
}

#endif // PLATFORM(IOS)
