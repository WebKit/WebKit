/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "CommonVM.h"
#import "FloatingPointEnvironment.h"
#import "GraphicsContextGLOpenGL.h"
#import "Logging.h"
#import "RuntimeApplicationChecks.h"
#import "ThreadGlobalData.h"
#import "WAKWindow.h"
#import "WKUtilities.h"
#import "WebCoreThreadInternal.h"
#import "WebCoreThreadMessage.h"
#import "WebCoreThreadRun.h"
#import <Foundation/NSInvocation.h>
#import <JavaScriptCore/InitializeThreading.h>
#import <JavaScriptCore/JSLock.h>
#import <libkern/OSAtomic.h>
#import <objc/runtime.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/RecursiveLockAdapter.h>
#import <wtf/RunLoop.h>
#import <wtf/ThreadSpecific.h>
#import <wtf/Threading.h>
#import <wtf/spi/cocoa/objcSPI.h>
#import <wtf/text/AtomString.h>

#define LOG_MESSAGES 0
#define LOG_WEB_LOCK 0
#define LOG_MAIN_THREAD_LOCKING 0
#define LOG_RELEASES 0

#define DistantFuture   (86400.0 * 2000 * 365.2425 + 86400.0)   // same as +[NSDate distantFuture]

namespace {
// Release global state that is accessed from both web thread as well
// as any client thread that calls into WebKit.
void ReleaseWebThreadGlobalState()
{
    // GraphicsContextGLOpenGL current context is owned by the web thread lock. Release the context
    // before the lock is released.

    // In single-threaded environments we do not need to unset the context, as there should not be access from
    // multiple threads.
    ASSERT(WebThreadIsEnabled());
    using ReleaseBehavior = WebCore::GraphicsContextGLOpenGL::ReleaseBehavior;
    // For non-web threads, we don't know if we ever see another call from the thread.
    ReleaseBehavior releaseBehavior =
        WebThreadIsCurrent() ? ReleaseBehavior::PreserveThreadResources : ReleaseBehavior::ReleaseThreadResources;
    WebCore::GraphicsContextGLOpenGL::releaseCurrentContext(releaseBehavior);
}

}

static const constexpr Seconds DelegateWaitInterval { 10_s };

static void _WebThreadAutoLock();
static void _WebThreadLock();
static void _WebThreadLockFromAnyThread(bool shouldLog);
static void _WebThreadUnlock();

@interface NSObject(ForwardDeclarations)
-(void)_webcore_releaseOnWebThread;
-(void)_webcore_releaseWithWebThreadLock;
@end

@implementation NSObject(WebCoreThreadAdditions)

- (void)releaseOnMainThread {
    if ([NSThread isMainThread])
        [self release];
    else
        [self performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
}

@end

static RecursiveLock webLock;
static Lock webThreadReleaseLock;
static RecursiveLock webCoreReleaseLock;

static void* autoreleasePoolMark;
static CFRunLoopRef webThreadRunLoop;
static NSRunLoop* webThreadNSRunLoop;
static pthread_t webThread;
static BOOL isWebThreadLocked;
static BOOL webThreadStarted;
static unsigned webThreadLockCount;

static void* savedAutoreleasePoolMark;
static BOOL isNestedWebThreadRunLoop;
typedef enum {
    PushOrPopAutoreleasePool,
    IgnoreAutoreleasePool
} AutoreleasePoolOperation;

static CFRunLoopSourceRef WebThreadReleaseSource;
static CFMutableArrayRef WebThreadReleaseObjArray;

static void MainThreadAdoptAndRelease(id obj);

static Lock delegateLock;
static StaticCondition delegateCondition;
static NSInvocation* delegateInvocation;
static CFRunLoopSourceRef delegateSource = nullptr;
static BOOL delegateHandled;
#if LOG_MAIN_THREAD_LOCKING
static BOOL sendingDelegateMessage;
#endif

static CFRunLoopObserverRef mainRunLoopAutoUnlockObserver;
static BOOL mainThreadHasPendingAutoUnlock;

static Lock startupLock;
static StaticCondition startupCondition;

static WebThreadContext* webThreadContext;
static unsigned mainThreadLockCount;
static unsigned otherThreadLockCount;
static unsigned sMainThreadModalCount;

WEBCORE_EXPORT volatile bool webThreadShouldYield;

static void WebCoreObjCDeallocOnWebThreadImpl(id self, SEL _cmd);
static void WebCoreObjCDeallocWithWebThreadLock(Class cls);
static void WebCoreObjCDeallocWithWebThreadLockImpl(id self, SEL _cmd);

static NSMutableArray* sAsyncDelegates = nil;

WEBCORE_EXPORT volatile unsigned webThreadDelegateMessageScopeCount = 0;

static inline void SendMessage(NSInvocation* invocation)
{
    [invocation invoke];
    MainThreadAdoptAndRelease(invocation);
}

static void HandleDelegateSource(void*)
{
    ASSERT(!WebThreadIsCurrent());

#if LOG_MAIN_THREAD_LOCKING
    sendingDelegateMessage = YES;
#endif

    _WebThreadAutoLock();

    {
        auto locker = holdLock(delegateLock);

#if LOG_MESSAGES
        if ([[delegateInvocation target] isKindOfClass:[NSNotificationCenter class]]) {
            id argument0;
            [delegateInvocation getArgument:&argument0 atIndex:0];
            NSLog(@"notification receive: %@", argument0);
        } else
            NSLog(@"delegate receive: %@", NSStringFromSelector([delegateInvocation selector]));
#endif

        SendMessage(delegateInvocation);

        delegateHandled = YES;
        delegateCondition.notifyOne();
    }

#if LOG_MAIN_THREAD_LOCKING
    sendingDelegateMessage = NO;
#endif
}

class WebThreadDelegateMessageScope {
public:
    WebThreadDelegateMessageScope() { ++webThreadDelegateMessageScopeCount; }
    ~WebThreadDelegateMessageScope()
    {
        ASSERT(webThreadDelegateMessageScopeCount);
        --webThreadDelegateMessageScopeCount;
    }
};

static void SendDelegateMessage(NSInvocation* invocation)
{
    if (!WebThreadIsCurrent()) {
        SendMessage(invocation);
        return;
    }

    ASSERT(delegateSource);
    delegateLock.lock();

    delegateInvocation = invocation;
    delegateHandled = NO;

#if LOG_MESSAGES
    if ([[delegateInvocation target] isKindOfClass:[NSNotificationCenter class]]) {
        id argument0;
        [delegateInvocation getArgument:&argument0 atIndex:0];
        NSLog(@"notification send: %@", argument0);
    } else
        NSLog(@"delegate send: %@", NSStringFromSelector([delegateInvocation selector]));
#endif

    {
        WebThreadDelegateMessageScope delegateScope;
        // Code block created to scope JSC::JSLock::DropAllLocks outside of WebThreadLock()
        JSC::JSLock::DropAllLocks dropAllLocks(WebCore::commonVM());
        _WebThreadUnlock();

        CFRunLoopSourceSignal(delegateSource);
        CFRunLoopWakeUp(CFRunLoopGetMain());

        while (!delegateHandled) {
            if (!delegateCondition.waitFor(delegateLock, DelegateWaitInterval)) {
                id delegateInformation;
                if ([[delegateInvocation target] isKindOfClass:[NSNotificationCenter class]])
                    [delegateInvocation getArgument:&delegateInformation atIndex:0];
                else
                    delegateInformation = NSStringFromSelector([delegateInvocation selector]);
    
                CFStringRef mode = CFRunLoopCopyCurrentMode(CFRunLoopGetMain());
                NSLog(@"%s: delegate (%@) failed to return after waiting %f seconds. main run loop mode: %@", __PRETTY_FUNCTION__, delegateInformation, DelegateWaitInterval.seconds(), mode);
                if (mode)
                    CFRelease(mode);
            }
        }
        delegateLock.unlock();
        _WebThreadLock();
    }
}

void WebThreadRunOnMainThread(void(^delegateBlock)())
{
    if (!WebThreadIsCurrent()) {
        ASSERT(pthread_main_np());
        delegateBlock();
        return;
    }

    JSC::JSLock::DropAllLocks dropAllLocks(WebCore::commonVM());
    _WebThreadUnlock();

    void (^delegateBlockCopy)() = Block_copy(delegateBlock);
    dispatch_sync(dispatch_get_main_queue(), delegateBlockCopy);
    Block_release(delegateBlockCopy);

    _WebThreadLock();
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

    auto locker = holdLock(webThreadReleaseLock);

    if (WebThreadReleaseObjArray == nil)
        WebThreadReleaseObjArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, nullptr);
    CFArrayAppendValue(WebThreadReleaseObjArray, obj);
    CFRunLoopSourceSignal(WebThreadReleaseSource);
    CFRunLoopWakeUp(webThreadRunLoop);
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

void WebCoreObjCDeallocOnWebThreadImpl(id self, SEL)
{
    if (!WebThreadIsEnabled()) {
        [self _webcore_releaseOnWebThread];
        return;
    }

    {
        auto locker = holdLock(webCoreReleaseLock);
        if ([self retainCount] != 1) {
            // This is not the only reference retaining the object, so another
            // thread could also call release - hold the lock whilst calling
            // release to avoid a race condition.
            [self _webcore_releaseOnWebThread];
            return;
        }
        // This is the only reference retaining the object, so we can
        // safely release the webCoreReleaseLock now.
    }
    if (WebThreadIsCurrent())
        [self _webcore_releaseOnWebThread];
    else
        WebThreadAdoptAndRelease(self);
}

void WebCoreObjCDeallocWithWebThreadLockImpl(id self, SEL)
{
    auto locker = holdLock(webCoreReleaseLock);
    if (WebThreadIsLockedOrDisabled() || 1 != [self retainCount])
        [self _webcore_releaseWithWebThreadLock];
    else
        WebThreadAdoptAndRelease(self);
}

static void HandleWebThreadReleaseSource(void*)
{
    ASSERT(WebThreadIsCurrent());

    CFMutableArrayRef objects = nullptr;
    {
        auto locker = holdLock(webThreadReleaseLock);
        if (CFArrayGetCount(WebThreadReleaseObjArray)) {
            objects = CFArrayCreateMutableCopy(nullptr, 0, WebThreadReleaseObjArray);
            CFArrayRemoveAllValues(WebThreadReleaseObjArray);
        }
    }

    if (!objects)
        return;

    for (unsigned i = 0, count = CFArrayGetCount(objects); i < count; ++i) {
        id obj = (id)CFArrayGetValueAtIndex(objects, i);
#if LOG_RELEASES
        NSLog(@"Release recv [web thread] : %@", obj);
#endif
        [obj release];
    }

    CFRelease(objects);
}

void WebThreadCallDelegate(NSInvocation* invocation)
{
    // NSInvocation released in SendMessage()
    SendDelegateMessage([invocation retain]);
}

void WebThreadPostNotification(NSString* name, id object, id userInfo)
{
    if (pthread_main_np())
        [[NSNotificationCenter defaultCenter] postNotificationName:name object:object userInfo:userInfo];
    else {
        dispatch_async(dispatch_get_main_queue(), ^ {
            [[NSNotificationCenter defaultCenter] postNotificationName:name object:object userInfo:userInfo];
        });
    }
}

void WebThreadCallDelegateAsync(NSInvocation* invocation)
{
    ASSERT(invocation);
    if (WebThreadIsCurrent())
        [sAsyncDelegates addObject:invocation];
    else
        WebThreadCallDelegate(invocation);
}

// Note: despite the name, returns an autoreleased object.
NSInvocation* WebThreadMakeNSInvocation(id target, SEL selector)
{
    NSMethodSignature* signature = [target methodSignatureForSelector:selector];
    ASSERT(signature);
    if (signature) {
        NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
        [invocation setSelector:selector];
        [invocation setTarget:target];
        [invocation retainArguments];
        return invocation;
    }
    return nil;
}

static void MainRunLoopAutoUnlock(CFRunLoopObserverRef, CFRunLoopActivity, void*)
{
    ASSERT(!WebThreadIsCurrent());

    if (sMainThreadModalCount)
        return;

    if (!mainThreadHasPendingAutoUnlock)
        return;

    mainThreadHasPendingAutoUnlock = NO;
    CFRunLoopRemoveObserver(CFRunLoopGetCurrent(), mainRunLoopAutoUnlockObserver, kCFRunLoopCommonModes);

    _WebThreadUnlock();
}

static void _WebThreadAutoLock(void)
{
    ASSERT(!WebThreadIsCurrent());

    if (!mainThreadLockCount) {
        mainThreadHasPendingAutoUnlock = YES;
        CFRunLoopAddObserver(CFRunLoopGetCurrent(), mainRunLoopAutoUnlockObserver, kCFRunLoopCommonModes);    
        _WebThreadLock();
        CFRunLoopWakeUp(CFRunLoopGetMain());
    }
}

static void WebRunLoopLockInternal(AutoreleasePoolOperation poolOperation)
{
    _WebThreadLock();
    if (poolOperation == PushOrPopAutoreleasePool)
        autoreleasePoolMark = objc_autoreleasePoolPush();
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
        objc_autoreleasePoolPop(autoreleasePoolMark);

    _WebThreadUnlock();
    isWebThreadLocked = NO;
}

static void WebRunLoopLock(CFRunLoopObserverRef, CFRunLoopActivity activity, void*)
{
    ASSERT(WebThreadIsCurrent());
    ASSERT_UNUSED(activity, activity == kCFRunLoopAfterWaiting || activity == kCFRunLoopBeforeTimers || activity == kCFRunLoopBeforeSources);

    // If the WebThread is locked by the main thread then we want to
    // grab the lock ourselves when the main thread releases the lock.
    if (isWebThreadLocked && !mainThreadLockCount)
        return;
    WebRunLoopLockInternal(PushOrPopAutoreleasePool);
}

static void WebRunLoopUnlock(CFRunLoopObserverRef, CFRunLoopActivity activity, void*)
{
    ASSERT(WebThreadIsCurrent());
    ASSERT_UNUSED(activity, activity == kCFRunLoopBeforeWaiting || activity == kCFRunLoopExit);
    ASSERT(!mainThreadLockCount);

    if (!isWebThreadLocked)
        return;
    WebRunLoopUnlockInternal(PushOrPopAutoreleasePool);
}

static void MainRunLoopUnlockGuard(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void* context)
{
    ASSERT(!WebThreadIsCurrent());

    // We shouldn't have the web lock at this point.  However, MobileMail sometimes
    // get to a state where the main thread has web lock but it didn't release it on last
    // runloop exit, and web thread gets stuck at waiting for the lock. If this happens,
    // we need to help release the lock.  See <rdar://problem/8005192>.
    if (mainThreadLockCount && !sMainThreadModalCount) {
        NSLog(@"WARNING: Main thread didn't release the lock at last runloop exit!");

        MainRunLoopAutoUnlock(observer, activity, context);
        if (mainThreadLockCount)
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

static ThreadSpecific<WebThreadContext, WTF::CanBeGCThread::True>* threadContext;
static WebThreadContext* CurrentThreadContext()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        threadContext = new ThreadSpecific<WebThreadContext, WTF::CanBeGCThread::True>();
    });
    return *threadContext;
}

static void* RunWebThread(void*)
{
    FloatingPointEnvironment::singleton().propagateMainThreadEnvironment();

    // WTF::initializeWebThread() needs to be called before JSC::initialize() since the
    // code invoked by the latter needs to know if it's running on the WebThread. See
    // <rdar://problem/8502487>.
    WTF::initializeWebThread();
    JSC::initialize();
    
    // Make sure that the WebThread and the main thread share the same ThreadGlobalData objects.
    WebCore::threadGlobalData().setWebCoreThreadData();

#if HAVE(PTHREAD_SETNAME_NP)
    pthread_setname_np("WebThread");
#endif

    webThreadContext = CurrentThreadContext();
    webThreadRunLoop = CFRunLoopGetCurrent();
    webThreadNSRunLoop = [[NSRunLoop currentRunLoop] retain];

    CFRunLoopObserverRef webRunLoopLockObserverRef = CFRunLoopObserverCreate(nullptr, kCFRunLoopBeforeTimers | kCFRunLoopBeforeSources | kCFRunLoopAfterWaiting, YES, 0, WebRunLoopLock, nullptr);
    CFRunLoopAddObserver(webThreadRunLoop, webRunLoopLockObserverRef, kCFRunLoopCommonModes);
    CFRelease(webRunLoopLockObserverRef);
    
    WebThreadInitRunQueue();

    // We must have the lock when CA paints in the web thread. CA commits at 2000000 so we use larger order number than that to free the lock.
    CFRunLoopObserverRef webRunLoopUnlockObserverRef = CFRunLoopObserverCreate(nullptr, kCFRunLoopBeforeWaiting | kCFRunLoopExit, YES, 2500000, WebRunLoopUnlock, nullptr);    
    CFRunLoopAddObserver(webThreadRunLoop, webRunLoopUnlockObserverRef, kCFRunLoopCommonModes);
    CFRelease(webRunLoopUnlockObserverRef);    

    CFRunLoopSourceContext ReleaseSourceContext = {0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, HandleWebThreadReleaseSource};
    WebThreadReleaseSource = CFRunLoopSourceCreate(nullptr, -1, &ReleaseSourceContext);
    CFRunLoopAddSource(webThreadRunLoop, WebThreadReleaseSource, kCFRunLoopDefaultMode);

    {
        LockHolder locker(startupLock);
        startupCondition.notifyOne();
    }

    while (1)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, DistantFuture, true);

    return nullptr;
}

static void StartWebThread()
{
    webThreadStarted = TRUE;

    // ThreadGlobalData touches AtomString, which requires Threading initialization.
    WTF::initializeMainThread();

    // Initialize AtomString on the main thread.
    WTF::AtomString::init();

    // Initialize ThreadGlobalData on the main UI thread so that the WebCore thread
    // can later set it's thread-specific data to point to the same objects.
    WebCore::ThreadGlobalData& unused = WebCore::threadGlobalData();
    UNUSED_PARAM(unused);

    // register class for WebThread deallocation
    WebCoreObjCDeallocOnWebThread([WAKWindow class]);
    WebCoreObjCDeallocWithWebThreadLock([WAKView class]);

    CFRunLoopRef runLoop = CFRunLoopGetCurrent();
    CFRunLoopSourceContext delegateSourceContext = {0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, HandleDelegateSource};
    delegateSource = CFRunLoopSourceCreate(nullptr, 0, &delegateSourceContext);

    // We shouldn't get delegate callbacks while scrolling, but there might be
    // one outstanding when we start.  Add the source for all common run loop
    // modes so we don't block the web thread while scrolling.
    CFRunLoopAddSource(runLoop, delegateSource, kCFRunLoopCommonModes);

    sAsyncDelegates = [[NSMutableArray alloc] init];

    mainRunLoopAutoUnlockObserver = CFRunLoopObserverCreate(nullptr, kCFRunLoopBeforeWaiting | kCFRunLoopExit, YES, 3000001, MainRunLoopAutoUnlock, nullptr);

    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setscope(&tattr, PTHREAD_SCOPE_SYSTEM);
    // The web thread is a secondary thread, and secondary threads are usually given
    // a 512 kb stack, but we need more space in order to have room for the JavaScriptCore
    // reentrancy limit. This limit works on both the simulator and the device.
    pthread_attr_setstacksize(&tattr, 800 * KB);

    pthread_attr_set_qos_class_np(&tattr, QOS_CLASS_USER_INTERACTIVE, -10);

    // Wait for the web thread to startup completely before we continue.
    {
        LockHolder locker(startupLock);

        // Propagate the mainThread's fenv to workers & the web thread.
        FloatingPointEnvironment::singleton().saveMainThreadEnvironment();

        pthread_create(&webThread, &tattr, RunWebThread, nullptr);
        pthread_attr_destroy(&tattr);

        startupCondition.wait(startupLock);
    }

    initializeApplicationUIThread();
}

#if LOG_WEB_LOCK || LOG_MAIN_THREAD_LOCKING
static unsigned lockCount;
#endif

static void _WebThreadLock()
{
    // Suspend the web thread if the main thread is trying to lock.
    bool onMainThread = pthread_main_np();
    if (onMainThread)
        webThreadShouldYield = true;
    else if (!WebThreadIsCurrent()) {
        NSLog(@"%s, %p: Tried to obtain the web lock from a thread other than the main thread or the web thread. This may be a result of calling to UIKit from a secondary thread. Crashing now...", __PRETTY_FUNCTION__, CurrentThreadContext());
        CRASH();
    }

    webLock.lock();

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

void WebThreadLockFromAnyThread(void)
{
    _WebThreadLockFromAnyThread(true);
}
    
void WebThreadLockFromAnyThreadNoLog(void)
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

    webLock.lock();

    // This used for any thread other than the web thread.
    otherThreadLockCount++;
    webThreadShouldYield = false;
}

void WebThreadUnlockFromAnyThread(void)
{
    if (!webThreadStarted)
        return;
    ASSERT(!WebThreadIsCurrent());
    ReleaseWebThreadGlobalState();

    // No-op except from a secondary thread.
    if (pthread_main_np())
        return;
    
    ASSERT(otherThreadLockCount);
    otherThreadLockCount--;

    webLock.unlock();
}

void WebThreadUnlockGuardForMail(void)
{
    ASSERT(!WebThreadIsCurrent());

    CFRunLoopObserverRef mainRunLoopUnlockGuardObserver = CFRunLoopObserverCreate(nullptr, kCFRunLoopEntry, YES, 0, MainRunLoopUnlockGuard, nullptr);
    CFRunLoopAddObserver(CFRunLoopGetMain(), mainRunLoopUnlockGuardObserver, kCFRunLoopCommonModes);
    CFRelease(mainRunLoopUnlockGuardObserver);
}

void _WebThreadUnlock()
{
    ReleaseWebThreadGlobalState();

#if LOG_WEB_LOCK || LOG_MAIN_THREAD_LOCKING
    lockCount--;
#if LOG_WEB_LOCK
    NSLog(@"unlock %d, web-thread: %d", lockCount, WebThreadIsCurrent());
#endif
#endif
    
    if (!WebThreadIsCurrent()) {
        ASSERT(mainThreadLockCount);
        mainThreadLockCount--;
    } else {    
        webThreadLockCount--;
        webThreadShouldYield = false;
    }

    webLock.unlock();
}

bool WebThreadIsLocked(void)
{
    if (WebThreadIsCurrent())
        return webThreadLockCount;

    if (pthread_main_np())
        return mainThreadLockCount;

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
    ASSERT(sMainThreadModalCount);
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

WebThreadContext* WebThreadCurrentContext(void)
{
    return CurrentThreadContext();
}

void WebThreadEnable(void)
{
    RELEASE_ASSERT_WITH_MESSAGE(!WebCore::IOSApplication::isWebProcess(), "The WebProcess should never run a Web Thread");
    if (WebCore::IOSApplication::isSpringBoard()) {
        using WebCore::LogThreading;
        RELEASE_LOG_FAULT(Threading, "SpringBoard enabled WebThread.");
    }

    static std::once_flag flag;
    std::call_once(flag, StartWebThread);
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

#endif // PLATFORM(IOS_FAMILY)
