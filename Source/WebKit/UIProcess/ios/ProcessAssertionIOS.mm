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

#import "AssertionServicesSPI.h"
#import "Logging.h"
#import <UIKit/UIApplication.h>
#import <wtf/HashMap.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>

using WebKit::ProcessAndUIAssertion;

// This gives some time to our child processes to process the ProcessWillSuspendImminently IPC but makes sure we release
// the background task before the UIKit timeout (We get killed if we do not release the background task within 5 seconds
// on the expiration handler getting called).
static const Seconds releaseBackgroundTaskAfterExpirationDelay { 2_s };
static const Seconds maximumBackgroundTaskDuration { 20_s };

@interface WKProcessAssertionBackgroundTaskManager : NSObject

+ (WKProcessAssertionBackgroundTaskManager *)shared;

- (void)addAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;
- (void)removeAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;

@end

@implementation WKProcessAssertionBackgroundTaskManager
{
    UIBackgroundTaskIdentifier _backgroundTask;
    HashSet<ProcessAndUIAssertion*> _assertionsNeedingBackgroundTask;
    BOOL _applicationIsBackgrounded;
    dispatch_block_t _releaseTask;
    dispatch_block_t _timeoutTask;
}

+ (WKProcessAssertionBackgroundTaskManager *)shared
{
    static WKProcessAssertionBackgroundTaskManager *shared = [WKProcessAssertionBackgroundTaskManager new];
    return shared;
}

static bool isBackgroundState(BKSApplicationState state)
{
    return state == BKSApplicationStateBackgroundRunning || state == BKSApplicationStateBackgroundTaskSuspended;
}

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    _backgroundTask = UIBackgroundTaskInvalid;

    {
        auto applicationStateMonitor = adoptNS([[BKSApplicationStateMonitor alloc] init]);
        _applicationIsBackgrounded = isBackgroundState([applicationStateMonitor mostElevatedApplicationStateForPID:getpid()]);
        [applicationStateMonitor invalidate];
    }

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    UIApplication *application = [UIApplication sharedApplication];

    [notificationCenter addObserverForName:UIApplicationWillEnterForegroundNotification object:application queue:nil usingBlock:^(NSNotification *) {
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: Application will enter foreground", self);
        _applicationIsBackgrounded = NO;
        [self _cancelReleaseTask];
        [self _cancelTimeoutTask];
        [self _updateBackgroundTask];
    }];

    [notificationCenter addObserverForName:UIApplicationDidEnterBackgroundNotification object:application queue:nil usingBlock:^(NSNotification *) {
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: Application did enter background", self);
        _applicationIsBackgrounded = YES;

        // We do not want to keep the app awake for more than 20 seconds after it gets backgrounded, so start the timeout timer now.
        if (_backgroundTask != UIBackgroundTaskInvalid)
            [self _scheduleTimeoutTask];
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
    _assertionsNeedingBackgroundTask.remove(&assertion);
    [self _updateBackgroundTask];
}

- (void)_notifyAssertionsOfImminentSuspension
{
    ASSERT(RunLoop::isMain());

    for (auto* assertion : copyToVector(_assertionsNeedingBackgroundTask))
        assertion->uiAssertionWillExpireImminently();
}

- (void)_scheduleTimeoutTask
{
    ASSERT(_backgroundTask != UIBackgroundTaskInvalid);

    if (_timeoutTask)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - _scheduleTimeoutTask", self);
    _timeoutTask = dispatch_block_create((dispatch_block_flags_t)0, ^{
        _timeoutTask = nil;
        RELEASE_LOG_ERROR(ProcessSuspension, "Background task was running for too long so WebKit will end it shortly.");
        [self _backgroundTaskExpired];
    });

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, maximumBackgroundTaskDuration.value() * NSEC_PER_SEC), dispatch_get_main_queue(), _timeoutTask);
#if !__has_feature(objc_arc)
    // dispatch_async() does a Block_copy() / Block_release() on behalf of the caller.
    Block_release(_timeoutTask);
#endif
}

- (void)_cancelTimeoutTask
{
    if (!_timeoutTask)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - _cancelTimeoutTask", self);
    dispatch_block_cancel(_timeoutTask);
    _timeoutTask = nil;
}

- (void)_scheduleReleaseTask
{
    ASSERT(!_releaseTask);
    if (_releaseTask)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - _scheduleReleaseTask because the expiration handler has been called", self);
    _releaseTask = dispatch_block_create((dispatch_block_flags_t)0, ^{
        _releaseTask = nil;
        [self _releaseBackgroundTask];
    });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, releaseBackgroundTaskAfterExpirationDelay.value() * NSEC_PER_SEC), dispatch_get_main_queue(), _releaseTask);
#if !__has_feature(objc_arc)
    // dispatch_async() does a Block_copy() / Block_release() on behalf of the caller.
    Block_release(_releaseTask);
#endif
}

- (void)_cancelReleaseTask
{
    if (!_releaseTask)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - _cancelReleaseTask because the application is foreground again", self);
    dispatch_block_cancel(_releaseTask);
    _releaseTask = nil;
}

- (void)_backgroundTaskExpired
{
    RELEASE_LOG_ERROR(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - _backgroundTaskExpired", self);
    [self _cancelTimeoutTask];

    // Tell our child processes they will suspend imminently.
    if (RunLoop::isMain())
        [self _notifyAssertionsOfImminentSuspension];
    else {
        // The expiration handler gets called on a non-main thread when the underlying assertion could not be taken (rdar://problem/27278419).
        dispatch_sync(dispatch_get_main_queue(), ^{
            [self _notifyAssertionsOfImminentSuspension];
        });
    }

    [self _scheduleReleaseTask];
}

- (void)_updateBackgroundTask
{
    if (!_assertionsNeedingBackgroundTask.isEmpty() && _backgroundTask == UIBackgroundTaskInvalid) {
        if (_applicationIsBackgrounded) {
            RELEASE_LOG_ERROR(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager: Ignored request to start a new background task because the application is already in the background", self);
            return;
        }
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - beginBackgroundTaskWithName", self);
        _backgroundTask = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"com.apple.WebKit.ProcessAssertion" expirationHandler:^{
            RELEASE_LOG_ERROR(ProcessSuspension, "Background task expired while holding WebKit ProcessAssertion (isMainThread? %d, _applicationIsBackgrounded? %d).", RunLoop::isMain(), _applicationIsBackgrounded);
            [self _backgroundTaskExpired];
        }];
    } else if (_assertionsNeedingBackgroundTask.isEmpty())
        [self _releaseBackgroundTask];
}

- (void)_releaseBackgroundTask
{
    if (_backgroundTask == UIBackgroundTaskInvalid)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - endBackgroundTask", self);
    [[UIApplication sharedApplication] endBackgroundTask:_backgroundTask];
    [self _cancelReleaseTask];
    [self _cancelTimeoutTask];
    _backgroundTask = UIBackgroundTaskInvalid;
}

@end

namespace WebKit {

const BKSProcessAssertionFlags suspendedTabFlags = (BKSProcessAssertionAllowIdleSleep);
const BKSProcessAssertionFlags backgroundTabFlags = (BKSProcessAssertionPreventTaskSuspend);
const BKSProcessAssertionFlags foregroundTabFlags = (BKSProcessAssertionPreventTaskSuspend | BKSProcessAssertionWantsForegroundResourcePriority | BKSProcessAssertionPreventTaskThrottleDown);

static BKSProcessAssertionFlags flagsForState(AssertionState assertionState)
{
    switch (assertionState) {
    case AssertionState::Suspended:
        return suspendedTabFlags;
    case AssertionState::Background:
    case AssertionState::UnboundedNetworking:
        return backgroundTabFlags;
    case AssertionState::Foreground:
        return foregroundTabFlags;
    }
}

static AssertionReason reasonForState(AssertionState assertionState)
{
    switch (assertionState) {
    case AssertionState::UnboundedNetworking:
        return AssertionReason::FinishTaskUnbounded;
    case AssertionState::Suspended:
    case AssertionState::Background:
    case AssertionState::Foreground:
        return AssertionReason::Extension;
    }
}

static BKSProcessAssertionReason toBKSProcessAssertionReason(AssertionReason reason)
{
    switch (reason) {
    case AssertionReason::Extension:
        return BKSProcessAssertionReasonExtension;
    case AssertionReason::FinishTask:
        return BKSProcessAssertionReasonFinishTask;
    case AssertionReason::FinishTaskUnbounded:
        return BKSProcessAssertionReasonFinishTaskUnbounded;
    case AssertionReason::MediaPlayback:
        return BKSProcessAssertionReasonMediaPlayback;
    }
}

ProcessAssertion::ProcessAssertion(pid_t pid, const String& name, AssertionState assertionState)
    : ProcessAssertion(pid, name, assertionState, reasonForState(assertionState))
{
}

ProcessAssertion::ProcessAssertion(pid_t pid, const String& name, AssertionState assertionState, AssertionReason assertionReason)
    : m_assertionState(assertionState)
{
    auto weakThis = makeWeakPtr(*this);
    BKSProcessAssertionAcquisitionHandler handler = ^(BOOL acquired) {
        if (!acquired) {
            RELEASE_LOG_ERROR(ProcessSuspension, " %p - ProcessAssertion() PID %d Unable to acquire assertion for process with PID %d", this, getpid(), pid);
            dispatch_async(dispatch_get_main_queue(), ^{
                if (weakThis)
                    processAssertionWasInvalidated();
            });
        }
    };
    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() PID %d acquiring assertion for process with PID %d, name '%s'", this, getpid(), pid, name.utf8().data());
    
    m_assertion = adoptNS([[BKSProcessAssertion alloc] initWithPID:pid flags:flagsForState(assertionState) reason:toBKSProcessAssertionReason(assertionReason) name:(NSString *)name withHandler:handler]);
    m_assertion.get().invalidationHandler = ^() {
        dispatch_async(dispatch_get_main_queue(), ^{
            RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() Process assertion for process with PID %d was invalidated", this, pid);
            if (weakThis)
                processAssertionWasInvalidated();
        });
    };
}

ProcessAssertion::~ProcessAssertion()
{
    m_assertion.get().invalidationHandler = nil;

    RELEASE_LOG(ProcessSuspension, "%p - ~ProcessAssertion() Releasing process assertion", this);
    [m_assertion invalidate];
}

void ProcessAssertion::processAssertionWasInvalidated()
{
    ASSERT(RunLoop::isMain());
    RELEASE_LOG_ERROR(ProcessSuspension, "%p - ProcessAssertion::processAssertionWasInvalidated()", this);

    m_validity = Validity::No;
}

void ProcessAssertion::setState(AssertionState assertionState)
{
    if (m_assertionState == assertionState)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion::setState(%u) previousState: %u", this, static_cast<unsigned>(assertionState), static_cast<unsigned>(m_assertionState));
    m_assertionState = assertionState;
    [m_assertion setFlags:flagsForState(assertionState)];
}

void ProcessAndUIAssertion::updateRunInBackgroundCount()
{
    bool shouldHoldBackgroundTask = validity() != Validity::No && state() != AssertionState::Suspended;
    if (m_isHoldingBackgroundTask == shouldHoldBackgroundTask)
        return;

    if (shouldHoldBackgroundTask)
        [[WKProcessAssertionBackgroundTaskManager shared] addAssertionNeedingBackgroundTask:*this];
    else
        [[WKProcessAssertionBackgroundTaskManager shared] removeAssertionNeedingBackgroundTask:*this];

    m_isHoldingBackgroundTask = shouldHoldBackgroundTask;
}

ProcessAndUIAssertion::ProcessAndUIAssertion(pid_t pid, const String& reason, AssertionState assertionState)
    : ProcessAssertion(pid, reason, assertionState)
{
    updateRunInBackgroundCount();
}

ProcessAndUIAssertion::~ProcessAndUIAssertion()
{
    if (m_isHoldingBackgroundTask)
        [[WKProcessAssertionBackgroundTaskManager shared] removeAssertionNeedingBackgroundTask:*this];
}

void ProcessAndUIAssertion::setState(AssertionState assertionState)
{
    ProcessAssertion::setState(assertionState);
    updateRunInBackgroundCount();
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
