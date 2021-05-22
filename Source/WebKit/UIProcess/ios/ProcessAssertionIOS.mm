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
#import <wtf/WorkQueue.h>

using WebKit::ProcessAndUIAssertion;

// This gives some time to our child processes to process the ProcessWillSuspendImminently IPC but makes sure we release
// the background task before the UIKit timeout (We get killed if we do not release the background task within 5 seconds
// on the expiration handler getting called).
static const Seconds releaseBackgroundTaskAfterExpirationDelay { 2_s };

static bool processHasActiveRunTimeLimitation()
{
    return [RBSProcessHandle currentProcess].activeLimitations.runTime != RBSProcessTimeLimitationNone;
}

@interface WKProcessAssertionBackgroundTaskManager
    : NSObject <RBSAssertionObserving>

+ (WKProcessAssertionBackgroundTaskManager *)shared;

- (void)addAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;
- (void)removeAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;

@end

@implementation WKProcessAssertionBackgroundTaskManager
{
    RetainPtr<RBSAssertion> _backgroundTask;
    std::atomic<bool> _backgroundTaskWasInvalidated;
    WeakHashSet<ProcessAndUIAssertion> _assertionsNeedingBackgroundTask;
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

    // FIXME: Stop relying on UIApplication notifications as this does not work as expected for daemons or ViewServices.
    // We should likely use ProcessTaskStateObserver to monitor suspension state.
    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationWillEnterForegroundNotification object:[UIApplication sharedApplication] queue:nil usingBlock:^(NSNotification *) {
        [self _cancelPendingReleaseTask];
        [self _updateBackgroundTask];
    }];

    [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidEnterBackgroundNotification object:[UIApplication sharedApplication] queue:nil usingBlock:^(NSNotification *) {
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
    WorkQueue::main().dispatchAfter(releaseBackgroundTaskAfterExpirationDelay, [self, retainedSelf = retainPtr(self)] {
        _pendingTaskReleaseTask = nil;
        [self _releaseBackgroundTask];
    });
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
    return !!_backgroundTask;
}

- (void)_updateBackgroundTask
{
    if (!_assertionsNeedingBackgroundTask.computesEmpty() && (![self _hasBackgroundTask] || _backgroundTaskWasInvalidated)) {
        if (processHasActiveRunTimeLimitation()) {
            RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: Ignored request to start a new background task because RunningBoard has already started the expiration timer", self);
            return;
        }
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: beginBackgroundTaskWithName", self);
        RBSTarget *target = [RBSTarget currentProcess];
        RBSDomainAttribute *domainAttribute = [RBSDomainAttribute attributeWithDomain:@"com.apple.common" name:@"FinishTaskInterruptable"];
        _backgroundTask = adoptNS([[RBSAssertion alloc] initWithExplanation:@"WebKit UIProcess background task" target:target attributes:@[domainAttribute]]);
        [_backgroundTask addObserver:self];
        _backgroundTaskWasInvalidated = false;
        [_backgroundTask acquireWithInvalidationHandler:nil];
        RELEASE_LOG(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: Took a FinishTaskInterruptable assertion for own process");
    } else if (_assertionsNeedingBackgroundTask.computesEmpty()) {
        // Release the background task asynchronously because releasing the background task may destroy the ProcessThrottler and we don't
        // want it to get destroyed while in the middle of updating its assertion.
        RunLoop::main().dispatch([self, strongSelf = retainPtr(self)] {
            if (_assertionsNeedingBackgroundTask.computesEmpty())
                [self _releaseBackgroundTask];
        });
    }
}

- (void)assertionWillInvalidate:(RBSAssertion *)assertion
{
    ASSERT(assertion == _backgroundTask.get());
    [self _handleBackgroundTaskExpiration];
}

- (void)assertion:(RBSAssertion *)assertion didInvalidateWithError:(NSError *)error
{
    ASSERT(assertion == _backgroundTask.get());
    RELEASE_LOG_ERROR(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: FinishTaskInterruptable assertion was invalidated, error: %{public}@", error);
    _backgroundTaskWasInvalidated = true;
}

- (void)_handleBackgroundTaskExpiration
{
    auto remainingTime = [RBSProcessHandle currentProcess].activeLimitations.runTime;
    RELEASE_LOG(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: Background task expired while holding WebKit ProcessAssertion (isMainThread=%d, remainingTime=%g).", RunLoop::isMain(), remainingTime);

    callOnMainRunLoopAndWait([self] {
        [self _handleBackgroundTaskExpirationOnMainThread];
    });
}

- (void)_handleBackgroundTaskExpirationOnMainThread
{
    ASSERT(RunLoop::isMain());

    auto remainingTime = [RBSProcessHandle currentProcess].activeLimitations.runTime;
    RELEASE_LOG(ProcessSuspension, "WKProcessAssertionBackgroundTaskManager: _handleBackgroundTaskExpirationOnMainThread (remainingTime=%g).", remainingTime);

    // If there is no time limitation, then it means that the process is now allowed to run again and the expiration notification
    // is outdated (e.g. we did not have time to process the expiration notification before suspending and thus only process it
    // upon resuming, or the user reactivated the app shortly after expiration).
    if (remainingTime == RBSProcessTimeLimitationNone) {
        [self _releaseBackgroundTask];
        RunLoop::main().dispatch([self, strongSelf = retainPtr(self)] {
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
    if (processHasActiveRunTimeLimitation())
        WebKit::WebProcessPool::notifyProcessPoolsApplicationIsAboutToSuspend();

    [_backgroundTask removeObserver:self];
    [_backgroundTask invalidate];
    _backgroundTask = nullptr;
}

@end

namespace WebKit {

static NSString *runningBoardNameForAssertionType(ProcessAssertionType assertionType)
{
    switch (assertionType) {
    case ProcessAssertionType::Suspended:
        return @"Suspended";
    case ProcessAssertionType::Background:
        return @"Background";
    case ProcessAssertionType::UnboundedNetworking:
        return @"UnboundedNetworking";
    case ProcessAssertionType::Foreground:
        return @"Foreground";
    case ProcessAssertionType::MediaPlayback:
        return @"MediaPlayback";
    }
}

ProcessAssertion::ProcessAssertion(pid_t pid, const String& reason, ProcessAssertionType assertionType)
    : m_assertionType(assertionType)
    , m_pid(pid)
{
    NSString *runningBoardAssertionName = runningBoardNameForAssertionType(assertionType);
    ASSERT(runningBoardAssertionName);
    if (!pid) {
        RELEASE_LOG_ERROR(ProcessSuspension, "%p - ProcessAssertion: Failed to acquire RBS %{public}@ assertion '%{public}s' for process because PID is invalid", this, runningBoardAssertionName, reason.utf8().data());
        return;
    }

    RBSTarget *target = [RBSTarget targetWithPid:pid];
    RBSDomainAttribute *domainAttribute = [RBSDomainAttribute attributeWithDomain:@"com.apple.webkit" name:runningBoardAssertionName];
    m_rbsAssertion = adoptNS([[RBSAssertion alloc] initWithExplanation:reason target:target attributes:@[domainAttribute]]);
    [m_rbsAssertion acquireWithInvalidationHandler:[weakThis = makeWeakPtr(*this), pid, runningBoardAssertionName = retainPtr(runningBoardAssertionName)](RBSAssertion *assertion, NSError *error) mutable {
        callOnMainRunLoop([weakThis = WTFMove(weakThis), pid, runningBoardAssertionName = WTFMove(runningBoardAssertionName), error = retainPtr(error)] {
            if (!weakThis)
                return;
            RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() RBS %{public}@ assertion for process with PID=%d was invalidated, error: %{public}@", weakThis.get(), runningBoardAssertionName.get(), pid, error.get());
            weakThis->processAssertionWasInvalidated();
        });
    }];

    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion: Took RBS %{public}@ assertion '%{public}s' for process with PID=%d", this, runningBoardAssertionName, reason.utf8().data(), pid);
}

ProcessAssertion::~ProcessAssertion()
{
    RELEASE_LOG(ProcessSuspension, "%p - ~ProcessAssertion() Releasing process assertion for process with PID=%d", this, m_pid);

    if (m_rbsAssertion)
        [m_rbsAssertion invalidate];
}

void ProcessAssertion::processAssertionWasInvalidated()
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion::processAssertionWasInvalidated() PID=%d", this, m_pid);

    m_wasInvalidated = true;
    if (m_invalidationHandler)
        m_invalidationHandler();
}

bool ProcessAssertion::isValid() const
{
    return m_rbsAssertion && !m_wasInvalidated;
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
    if (m_uiAssertionExpirationHandler)
        m_uiAssertionExpirationHandler();
}

void ProcessAndUIAssertion::processAssertionWasInvalidated()
{
    ASSERT(RunLoop::isMain());

    auto weakThis = makeWeakPtr(*this);
    ProcessAssertion::processAssertionWasInvalidated();

    // Calling ProcessAssertion::processAssertionWasInvalidated() may have destroyed |this|.
    if (weakThis)
        updateRunInBackgroundCount();
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
