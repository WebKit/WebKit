/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#import "ProcessAssertion.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "RunningBoardServicesSPI.h"
#import "WebProcessPool.h"
#import <UIKit/UIApplication.h>
#import <wtf/HashMap.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
#import <wtf/WeakHashSet.h>

#if !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
#import "AssertionServicesSPI.h"
#endif

using WebKit::ProcessAndUIAssertion;

// This gives some time to our child processes to process the ProcessWillSuspendImminently IPC but makes sure we release
// the background task before the UIKit timeout (We get killed if we do not release the background task within 5 seconds
// on the expiration handler getting called).
static const Seconds releaseBackgroundTaskAfterExpirationDelay { 2_s };

@interface WKProcessAssertionBackgroundTaskManager
#if HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    : NSObject <RBSAssertionObserving>
#else
    : NSObject
#endif

+ (WKProcessAssertionBackgroundTaskManager *)shared;

- (void)addAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;
- (void)removeAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;

@end

@implementation WKProcessAssertionBackgroundTaskManager
{
#if HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    RetainPtr<RBSAssertion> _backgroundTask;
#else
    UIBackgroundTaskIdentifier _backgroundTask;
#endif

    WeakHashSet<ProcessAndUIAssertion> _assertionsNeedingBackgroundTask;
    BOOL _applicationIsBackgrounded;
    dispatch_block_t _pendingTaskReleaseTask;
}

+ (WKProcessAssertionBackgroundTaskManager *)shared
{
    static WKProcessAssertionBackgroundTaskManager *shared = [WKProcessAssertionBackgroundTaskManager new];
    return shared;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

#if !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    _backgroundTask = UIBackgroundTaskInvalid;
#endif

    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationWillEnterForegroundNotification object:[UIApplication sharedApplication] queue:nil usingBlock:^(NSNotification *) {
        _applicationIsBackgrounded = NO;
        [self _cancelPendingReleaseTask];
        [self _updateBackgroundTask];
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidEnterBackgroundNotification object:[UIApplication sharedApplication] queue:nil usingBlock:^(NSNotification *) {
        _applicationIsBackgrounded = YES;
        
        if (![self _hasBackgroundTask])
            WebKit::WebProcessPool::notifyProcessPoolsApplicationIsAboutToSuspend();
    }];

    return self;
}

- (void)dealloc
{
    [self _releaseBackgroundTask];
    [super dealloc];
}

- (void)addAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion
{
    _assertionsNeedingBackgroundTask.add(&assertion);
    [self _updateBackgroundTask];
}

- (void)removeAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion
{
    _assertionsNeedingBackgroundTask.remove(assertion);
    [self _updateBackgroundTask];
}

- (void)_notifyAssertionsOfImminentSuspension
{
    ASSERT(RunLoop::isMain());

    Vector<WeakPtr<ProcessAndUIAssertion>> assertionsNeedingBackgroundTask;
    for (auto& assertion : _assertionsNeedingBackgroundTask)
        assertionsNeedingBackgroundTask.append(makeWeakPtr(assertion));

    // Note that we don't expect clients to register new assertions when getting notified that the UI assertion will expire imminently.
    // If clients were to do so, then those new assertions would not get notified of the imminent suspension.
    for (auto assertion : assertionsNeedingBackgroundTask) {
        if (assertion)
            assertion->uiAssertionWillExpireImminently();
    }
}


- (void)_scheduleReleaseTask
{
    ASSERT(!_pendingTaskReleaseTask);
    if (_pendingTaskReleaseTask)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: _scheduleReleaseTask because the expiration handler has been called", self);
    _pendingTaskReleaseTask = dispatch_block_create((dispatch_block_flags_t)0, ^{
        _pendingTaskReleaseTask = nil;
        [self _releaseBackgroundTask];
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, releaseBackgroundTaskAfterExpirationDelay.value() * NSEC_PER_SEC), dispatch_get_main_queue(), _pendingTaskReleaseTask);
#if !__has_feature(objc_arc)
    // dispatch_async() does a Block_copy() / Block_release() on behalf of the caller.
    Block_release(_pendingTaskReleaseTask);
#endif
}

- (void)_cancelPendingReleaseTask
{
    if (!_pendingTaskReleaseTask)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: _cancelPendingReleaseTask because the application is foreground again", self);
    dispatch_block_cancel(_pendingTaskReleaseTask);
    _pendingTaskReleaseTask = nil;
}

- (BOOL)_hasBackgroundTask
{
#if HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    return !!_backgroundTask;
#else
    return _backgroundTask != UIBackgroundTaskInvalid;
#endif
}

- (void)_updateBackgroundTask
{
    if (!_assertionsNeedingBackgroundTask.computesEmpty() && ![self _hasBackgroundTask]) {
        if (_applicationIsBackgrounded) {
            RELEASE_LOG_ERROR(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: Ignored request to start a new background task because the application is already in the background", self);
            return;
        }
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: beginBackgroundTaskWithName", self);
#if HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
        RBSTarget *target = [RBSTarget currentProcess];
        RBSDomainAttribute *domainAttribute = [RBSDomainAttribute attributeWithDomain:@"com.apple.common" name:@"FinishTaskInterruptable"];
        _backgroundTask = adoptNS([[RBSAssertion alloc] initWithExplanation:@"WebKit UIProcess background task" target:target attributes:@[domainAttribute]]);
        [_backgroundTask addObserver:self];

        NSError *acquisitionError = nil;
        if (![_backgroundTask acquireWithError:&acquisitionError])
            RELEASE_LOG_ERROR(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: Failed to acquire FinishTaskInterruptable assertion for own process, error: %{public}@", acquisitionError);
        else
            RELEASE_LOG(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: Successfully took a FinishTaskInterruptable assertion for own process");
#else
        _backgroundTask = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"com.apple.WebKit.ProcessAssertion" expirationHandler:^{
            [self _handleBackgroundTaskExpiration];
        }];
#endif
    } else if (_assertionsNeedingBackgroundTask.computesEmpty())
        [self _releaseBackgroundTask];
}

#if HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
- (void)assertionWillInvalidate:(RBSAssertion *)assertion
{
    ASSERT(assertion == _backgroundTask.get());
    [self _handleBackgroundTaskExpiration];
}

- (void)assertion:(RBSAssertion *)assertion didInvalidateWithError:(NSError *)error
{
    ASSERT(assertion == _backgroundTask.get());
    RELEASE_LOG_ERROR(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: FinishTaskInterruptable assertion was invalidated, error: %{public}@", error);
}
#endif

- (void)_handleBackgroundTaskExpiration
{
    RELEASE_LOG(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: Background task expired while holding WebKit ProcessAssertion (isMainThread? %d).", RunLoop::isMain());
    callOnMainRunLoopAndWait([self] {
        [self _handleBackgroundTaskExpirationOnMainThread];
    });
}

- (void)_handleBackgroundTaskExpirationOnMainThread
{
    ASSERT(RunLoop::isMain());

    // FIXME: While relying on _applicationIsBackgrounded is acceptable for applications, this is inacurate in the case
    // of daemons.
    if (!_applicationIsBackgrounded) {
        // We've received the invalidation warning after the app has become foreground again. In this case, we should not
        // warn clients of imminent suspension. To be safe (avoid potential killing), we end the task right away and call
        // _updateBackgroundTask asynchronously to start a new task if necessary.
        [self _releaseBackgroundTask];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self _updateBackgroundTask];
        });
        return;
    }

    [self _notifyAssertionsOfImminentSuspension];
    [self _scheduleReleaseTask];
}

- (void)_releaseBackgroundTask
{
    if (![self _hasBackgroundTask])
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: endBackgroundTask", self);
    if (_applicationIsBackgrounded)
        WebKit::WebProcessPool::notifyProcessPoolsApplicationIsAboutToSuspend();

#if HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    [_backgroundTask removeObserver:self];
    [_backgroundTask invalidate];
    _backgroundTask = nullptr;
#else
    [[UIApplication sharedApplication] endBackgroundTask:_backgroundTask];
    _backgroundTask = UIBackgroundTaskInvalid;
#endif
}

@end

typedef void(^RBSAssertionInvalidationCallbackType)();

@interface WKRBSAssertionDelegate : NSObject<RBSAssertionObserving>
@property (copy) RBSAssertionInvalidationCallbackType invalidationCallback;
@end

@implementation WKRBSAssertionDelegate
- (void)dealloc
{
    [_invalidationCallback release];
    [super dealloc];
}

- (void)assertionWillInvalidate:(RBSAssertion *)assertion
{
    RELEASE_LOG(ProcessSuspension, "%p - WKRBSAssertionDelegate: assertionWillInvalidate", self);
}

- (void)assertion:(RBSAssertion *)assertion didInvalidateWithError:(NSError *)error
{
    RELEASE_LOG(ProcessSuspension, "%p - WKRBSAssertionDelegate: assertion was invalidated, error: %{public}@", error, self);

    __weak WKRBSAssertionDelegate *weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        WKRBSAssertionDelegate *strongSelf = weakSelf;
        if (strongSelf && strongSelf.invalidationCallback)
            strongSelf.invalidationCallback();
    });
}
@end

namespace WebKit {

static NSString *runningBoardNameForAssertionType(ProcessAssertionType assertionType)
{
#if HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    switch (assertionType) {
    case ProcessAssertionType::Suspended:
        return @"Suspended";
    case ProcessAssertionType::Background:
        return @"Background";
    case ProcessAssertionType::UnboundedNetworking:
        return @"UnboundedNetworking";
    case ProcessAssertionType::Foreground:
        return @"Foreground";
    case ProcessAssertionType::DependentProcessLink:
        return @"DependentProcessLink";
    case ProcessAssertionType::MediaPlayback:
        return @"MediaPlayback";
    }
#else
    if (assertionType == ProcessAssertionType::DependentProcessLink)
        return @"DependentProcessLink";
    return nil;
#endif
}

#if !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)

const BKSProcessAssertionFlags suspendedTabFlags = (BKSProcessAssertionAllowIdleSleep);
const BKSProcessAssertionFlags backgroundTabFlags = (BKSProcessAssertionPreventTaskSuspend);
const BKSProcessAssertionFlags foregroundTabFlags = (BKSProcessAssertionPreventTaskSuspend | BKSProcessAssertionWantsForegroundResourcePriority | BKSProcessAssertionPreventTaskThrottleDown);

static BKSProcessAssertionFlags flagsForAssertionType(ProcessAssertionType assertionType)
{
    switch (assertionType) {
    case ProcessAssertionType::Suspended:
        return suspendedTabFlags;
    case ProcessAssertionType::Background:
    case ProcessAssertionType::UnboundedNetworking:
        return backgroundTabFlags;
    case ProcessAssertionType::Foreground:
    case ProcessAssertionType::MediaPlayback:
        return foregroundTabFlags;
    case ProcessAssertionType::DependentProcessLink:
        ASSERT_NOT_REACHED();
        return backgroundTabFlags;
    }
}

static BKSProcessAssertionReason toBKSProcessAssertionReason(ProcessAssertionType assertionType)
{
    switch (assertionType) {
    case ProcessAssertionType::Suspended:
    case ProcessAssertionType::Background:
    case ProcessAssertionType::Foreground:
        return BKSProcessAssertionReasonExtension;
    case ProcessAssertionType::UnboundedNetworking:
        return BKSProcessAssertionReasonFinishTaskUnbounded;
    case ProcessAssertionType::MediaPlayback:
        return BKSProcessAssertionReasonMediaPlayback;
    case ProcessAssertionType::DependentProcessLink:
        ASSERT_NOT_REACHED();
        return BKSProcessAssertionReasonExtension;
    }
}

#endif // !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)

ProcessAssertion::ProcessAssertion(pid_t pid, const String& reason, ProcessAssertionType assertionType)
    : m_assertionType(assertionType)
    , m_pid(pid)
{
    auto weakThis = makeWeakPtr(*this);
    NSString *runningBoardAssertionName = runningBoardNameForAssertionType(assertionType);

#if !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    if (!runningBoardAssertionName) {
        // Legacy code path.
        BKSProcessAssertionAcquisitionHandler handler = ^(BOOL acquired) {
            if (!acquired) {
                RELEASE_LOG_ERROR(ProcessSuspension, " %p - ProcessAssertion() PID %d Unable to acquire BKS assertion for process with PID %d", this, getpid(), pid);
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (weakThis)
                        processAssertionWasInvalidated();
                });
            }
        };
        RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() PID %d acquiring BKS assertion for process with PID %d, name '%s'", this, getpid(), pid, reason.utf8().data());

        m_bksAssertion = adoptNS([[BKSProcessAssertion alloc] initWithPID:pid flags:flagsForAssertionType(assertionType) reason:toBKSProcessAssertionReason(assertionType) name:reason withHandler:handler]);

        m_bksAssertion.get().invalidationHandler = ^() {
            dispatch_async(dispatch_get_main_queue(), ^{
                RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() BKS Process assertion for process with PID %d was invalidated", this, pid);
                if (weakThis)
                    processAssertionWasInvalidated();
            });
        };
        return;
    }
#endif // !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)

    ASSERT(runningBoardAssertionName);
    if (!pid) {
        RELEASE_LOG_ERROR(ProcessSuspension, "%p - ProcessAssertion: Failed to acquire RBS %{public}@ assertion '%{public}s' for process because PID is invalid", this, runningBoardAssertionName, reason.utf8().data());
        return;
    }

    RBSTarget *target = [RBSTarget targetWithPid:pid];
    RBSDomainAttribute *domainAttribute = [RBSDomainAttribute attributeWithDomain:@"com.apple.webkit" name:runningBoardAssertionName];
    m_rbsAssertion = adoptNS([[RBSAssertion alloc] initWithExplanation:reason target:target attributes:@[domainAttribute]]);

    m_delegate = adoptNS([[WKRBSAssertionDelegate alloc] init]);
    [m_rbsAssertion addObserver:m_delegate.get()];
    m_delegate.get().invalidationCallback = ^{
        RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() RBS %{public}@ assertion for process with PID %d was invalidated", this, runningBoardAssertionName, pid);
        processAssertionWasInvalidated();
    };

    NSError *acquisitionError = nil;
    if (![m_rbsAssertion acquireWithError:&acquisitionError]) {
        RELEASE_LOG_ERROR(ProcessSuspension, "%p - ProcessAssertion: Failed to acquire RBS %{public}@ assertion '%{public}s' for process with PID %d, error: %{public}@", this, runningBoardAssertionName, reason.utf8().data(), pid, acquisitionError);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (weakThis)
                processAssertionWasInvalidated();
        });
    } else
        RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion: Successfully took RBS %{public}@ assertion '%{public}s' for process with PID %d", this, runningBoardAssertionName, reason.utf8().data(), pid);
}

ProcessAssertion::~ProcessAssertion()
{
    RELEASE_LOG(ProcessSuspension, "%p - ~ProcessAssertion() Releasing process assertion for process with PID %d", this, m_pid);

    if (m_rbsAssertion) {
        m_delegate.get().invalidationCallback = nil;
        [m_rbsAssertion removeObserver:m_delegate.get()];
        m_delegate = nil;
        [m_rbsAssertion invalidate];
    }

#if !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    if (m_bksAssertion) {
        m_bksAssertion.get().invalidationHandler = nil;
        [m_bksAssertion invalidate];
    }
#endif
}

void ProcessAssertion::processAssertionWasInvalidated()
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion::processAssertionWasInvalidated() PID: %d", this, m_pid);

    if (auto* client = this->client())
        client->assertionWasInvalidated();
}

bool ProcessAssertion::isValid() const
{
    if (m_rbsAssertion)
        return m_rbsAssertion.get().valid;

#if !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    if (m_bksAssertion)
        return m_bksAssertion.get().valid;
#endif

    return false;
}

void ProcessAndUIAssertion::updateRunInBackgroundCount()
{
    bool shouldHoldBackgroundTask = isValid() && type() != ProcessAssertionType::Suspended;
    if (m_isHoldingBackgroundTask == shouldHoldBackgroundTask)
        return;

    if (shouldHoldBackgroundTask)
        [[WKProcessAssertionBackgroundTaskManager shared] addAssertionNeedingBackgroundTask:*this];
    else
        [[WKProcessAssertionBackgroundTaskManager shared] removeAssertionNeedingBackgroundTask:*this];

    m_isHoldingBackgroundTask = shouldHoldBackgroundTask;
}

ProcessAndUIAssertion::ProcessAndUIAssertion(pid_t pid, const String& reason, ProcessAssertionType assertionType)
    : ProcessAssertion(pid, reason, assertionType)
{
    updateRunInBackgroundCount();
}

ProcessAndUIAssertion::~ProcessAndUIAssertion()
{
    if (m_isHoldingBackgroundTask)
        [[WKProcessAssertionBackgroundTaskManager shared] removeAssertionNeedingBackgroundTask:*this];
}

void ProcessAndUIAssertion::uiAssertionWillExpireImminently()
{
    if (auto* client = this->client())
        client->uiAssertionWillExpireImminently();
}

void ProcessAndUIAssertion::processAssertionWasInvalidated()
{
    ProcessAssertion::processAssertionWasInvalidated();
    updateRunInBackgroundCount();
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
