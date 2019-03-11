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

@interface WKProcessAssertionBackgroundTaskManager : NSObject

+ (WKProcessAssertionBackgroundTaskManager *)shared;

- (void)addAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;
- (void)removeAssertionNeedingBackgroundTask:(ProcessAndUIAssertion&)assertion;

@end

@implementation WKProcessAssertionBackgroundTaskManager
{
    UIBackgroundTaskIdentifier _backgroundTask;
    HashSet<ProcessAndUIAssertion*> _assertionsNeedingBackgroundTask;
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

    _backgroundTask = UIBackgroundTaskInvalid;

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

- (void)_updateBackgroundTask
{
    if (!_assertionsNeedingBackgroundTask.isEmpty() && _backgroundTask == UIBackgroundTaskInvalid) {
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - beginBackgroundTaskWithName", self);
        _backgroundTask = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"com.apple.WebKit.ProcessAssertion" expirationHandler:^{
            RELEASE_LOG_ERROR(ProcessSuspension, "Background task expired while holding WebKit ProcessAssertion (isMainThread? %d).", RunLoop::isMain());
            // The expiration handler gets called on a non-main thread when the underlying assertion could not be taken (rdar://problem/27278419).
            if (RunLoop::isMain())
                [self _notifyAssertionsOfImminentSuspension];
            else {
                dispatch_sync(dispatch_get_main_queue(), ^{
                    [self _notifyAssertionsOfImminentSuspension];
                });
            }
            [self _releaseBackgroundTask];
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
    _backgroundTask = UIBackgroundTaskInvalid;
}

@end

namespace WebKit {

const BKSProcessAssertionFlags suspendedTabFlags = (BKSProcessAssertionAllowIdleSleep);
const BKSProcessAssertionFlags backgroundTabFlags = (BKSProcessAssertionAllowIdleSleep | BKSProcessAssertionPreventTaskSuspend);
const BKSProcessAssertionFlags foregroundTabFlags = (BKSProcessAssertionAllowIdleSleep | BKSProcessAssertionPreventTaskSuspend | BKSProcessAssertionWantsForegroundResourcePriority | BKSProcessAssertionPreventTaskThrottleDown);

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

static BKSProcessAssertionReason reasonForState(AssertionState assertionState)
{
    switch (assertionState) {
    case AssertionState::UnboundedNetworking:
        return BKSProcessAssertionReasonFinishTaskUnbounded;
    case AssertionState::Suspended:
    case AssertionState::Background:
    case AssertionState::Foreground:
        return BKSProcessAssertionReasonExtension;
    }
}

ProcessAssertion::ProcessAssertion(pid_t pid, const String& name, AssertionState assertionState)
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
    
    m_assertion = adoptNS([[BKSProcessAssertion alloc] initWithPID:pid flags:flagsForState(assertionState) reason:reasonForState(assertionState) name:(NSString *)name withHandler:handler]);
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
