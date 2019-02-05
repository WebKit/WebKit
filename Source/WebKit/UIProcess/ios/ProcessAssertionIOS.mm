/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import <wtf/HashSet.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>

#if !PLATFORM(IOS_FAMILY_SIMULATOR)

using WebKit::ProcessAssertionClient;

@interface WKProcessAssertionBackgroundTaskManager : NSObject

+ (WKProcessAssertionBackgroundTaskManager *)shared;

- (void)incrementNeedsToRunInBackgroundCount;
- (void)decrementNeedsToRunInBackgroundCount;

- (void)addClient:(ProcessAssertionClient&)client;
- (void)removeClient:(ProcessAssertionClient&)client;

@end

@implementation WKProcessAssertionBackgroundTaskManager
{
    unsigned _needsToRunInBackgroundCount;
    UIBackgroundTaskIdentifier _backgroundTask;
    HashSet<ProcessAssertionClient*> _clients;
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
    if (_backgroundTask != UIBackgroundTaskInvalid)
        [[UIApplication sharedApplication] endBackgroundTask:_backgroundTask];

    [super dealloc];
}

- (void)addClient:(ProcessAssertionClient&)client
{
    _clients.add(&client);
}

- (void)removeClient:(ProcessAssertionClient&)client
{
    _clients.remove(&client);
}

- (void)_notifyClientsOfImminentSuspension
{
    ASSERT(RunLoop::isMain());

    for (auto* client : copyToVector(_clients))
        client->assertionWillExpireImminently();
}

- (void)_updateBackgroundTask
{
    if (_needsToRunInBackgroundCount && _backgroundTask == UIBackgroundTaskInvalid) {
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - beginBackgroundTaskWithName", self);
        _backgroundTask = [[UIApplication sharedApplication] beginBackgroundTaskWithName:@"com.apple.WebKit.ProcessAssertion" expirationHandler:^{
            RELEASE_LOG_ERROR(ProcessSuspension, "Background task expired while holding WebKit ProcessAssertion (isMainThread? %d).", RunLoop::isMain());
            // The expiration handler gets called on a non-main thread when the underlying assertion could not be taken (rdar://problem/27278419).
            if (RunLoop::isMain())
                [self _notifyClientsOfImminentSuspension];
            else {
                dispatch_sync(dispatch_get_main_queue(), ^{
                    [self _notifyClientsOfImminentSuspension];
                });
            }
            [[UIApplication sharedApplication] endBackgroundTask:_backgroundTask];
            _backgroundTask = UIBackgroundTaskInvalid;
        }];
    }

    if (!_needsToRunInBackgroundCount && _backgroundTask != UIBackgroundTaskInvalid) {
        RELEASE_LOG(ProcessSuspension, "%p - WKProcessAssertionBackgroundTaskManager - endBackgroundTask", self);
        [[UIApplication sharedApplication] endBackgroundTask:_backgroundTask];
        _backgroundTask = UIBackgroundTaskInvalid;
    }
}

- (void)incrementNeedsToRunInBackgroundCount
{
    ++_needsToRunInBackgroundCount;
    [self _updateBackgroundTask];
}

- (void)decrementNeedsToRunInBackgroundCount
{
    --_needsToRunInBackgroundCount;
    [self _updateBackgroundTask];
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
    case AssertionState::Download:
        return backgroundTabFlags;
    case AssertionState::Foreground:
        return foregroundTabFlags;
    }
}

static BKSProcessAssertionReason reasonForState(AssertionState assertionState)
{
    switch (assertionState) {
    case AssertionState::Download:
        return BKSProcessAssertionReasonFinishTaskUnbounded;
    case AssertionState::Suspended:
    case AssertionState::Background:
    case AssertionState::Foreground:
        return BKSProcessAssertionReasonExtension;
    }
}

ProcessAssertion::ProcessAssertion(pid_t pid, AssertionState assertionState, Function<void()>&& invalidationCallback)
    : ProcessAssertion(pid, "Web content visibility"_s, assertionState, WTFMove(invalidationCallback))
{
}

ProcessAssertion::ProcessAssertion(pid_t pid, const String& name, AssertionState assertionState, Function<void()>&& invalidationCallback)
    : m_invalidationCallback(WTFMove(invalidationCallback))
    , m_assertionState(assertionState)
{
    auto weakThis = makeWeakPtr(*this);
    BKSProcessAssertionAcquisitionHandler handler = ^(BOOL acquired) {
        if (!acquired) {
            RELEASE_LOG_ERROR(ProcessSuspension, " %p - ProcessAssertion() PID %d Unable to acquire assertion for process with PID %d", this, getpid(), pid);
            ASSERT_NOT_REACHED();
            dispatch_async(dispatch_get_main_queue(), ^{
                if (weakThis)
                    markAsInvalidated();
            });
        }
    };
    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() PID %d acquiring assertion for process with PID %d", this, getpid(), pid);
    
    m_assertion = adoptNS([[BKSProcessAssertion alloc] initWithPID:pid flags:flagsForState(assertionState) reason:reasonForState(assertionState) name:(NSString *)name withHandler:handler]);
    m_assertion.get().invalidationHandler = ^() {
        dispatch_async(dispatch_get_main_queue(), ^{
            RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion() Process assertion for process with PID %d was invalidated", this, pid);
            if (weakThis)
                markAsInvalidated();
        });
    };
}

ProcessAssertion::~ProcessAssertion()
{
    m_assertion.get().invalidationHandler = nil;

    if (ProcessAssertionClient* client = this->client())
        [[WKProcessAssertionBackgroundTaskManager shared] removeClient:*client];

    RELEASE_LOG(ProcessSuspension, "%p - ~ProcessAssertion() Releasing process assertion", this);
    [m_assertion invalidate];
}

void ProcessAssertion::markAsInvalidated()
{
    ASSERT(RunLoop::isMain());

    m_validity = Validity::No;
    if (m_invalidationCallback)
        m_invalidationCallback();
}

void ProcessAssertion::setState(AssertionState assertionState)
{
    if (m_assertionState == assertionState)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - ProcessAssertion::setState(%u)", this, static_cast<unsigned>(assertionState));
    m_assertionState = assertionState;
    [m_assertion setFlags:flagsForState(assertionState)];
}

void ProcessAndUIAssertion::updateRunInBackgroundCount()
{
    bool shouldHoldBackgroundAssertion = validity() != Validity::No && state() != AssertionState::Suspended;

    if (shouldHoldBackgroundAssertion) {
        if (!m_isHoldingBackgroundAssertion)
            [[WKProcessAssertionBackgroundTaskManager shared] incrementNeedsToRunInBackgroundCount];
    } else {
        if (m_isHoldingBackgroundAssertion)
            [[WKProcessAssertionBackgroundTaskManager shared] decrementNeedsToRunInBackgroundCount];
    }

    m_isHoldingBackgroundAssertion = shouldHoldBackgroundAssertion;
}

ProcessAndUIAssertion::ProcessAndUIAssertion(pid_t pid, AssertionState assertionState)
    : ProcessAssertion(pid, assertionState, [this] { updateRunInBackgroundCount(); })
{
    updateRunInBackgroundCount();
}

ProcessAndUIAssertion::~ProcessAndUIAssertion()
{
    if (m_isHoldingBackgroundAssertion)
        [[WKProcessAssertionBackgroundTaskManager shared] decrementNeedsToRunInBackgroundCount];
}

void ProcessAndUIAssertion::setState(AssertionState assertionState)
{
    ProcessAssertion::setState(assertionState);
    updateRunInBackgroundCount();
}

void ProcessAndUIAssertion::setClient(ProcessAssertionClient& newClient)
{
    [[WKProcessAssertionBackgroundTaskManager shared] addClient:newClient];
    if (ProcessAssertionClient* oldClient = this->client())
        [[WKProcessAssertionBackgroundTaskManager shared] removeClient:*oldClient];
    ProcessAssertion::setClient(newClient);
}

} // namespace WebKit

#else // PLATFORM(IOS_FAMILY_SIMULATOR)

namespace WebKit {

ProcessAssertion::ProcessAssertion(pid_t, AssertionState assertionState, Function<void()>&&)
    : m_assertionState(assertionState)
{
}

ProcessAssertion::~ProcessAssertion()
{
}

void ProcessAssertion::setState(AssertionState assertionState)
{
    m_assertionState = assertionState;
}

ProcessAndUIAssertion::ProcessAndUIAssertion(pid_t pid, AssertionState assertionState)
    : ProcessAssertion(pid, assertionState)
{
}

ProcessAndUIAssertion::~ProcessAndUIAssertion()
{
}

void ProcessAndUIAssertion::setState(AssertionState assertionState)
{
    ProcessAssertion::setState(assertionState);
}

void ProcessAndUIAssertion::setClient(ProcessAssertionClient& newClient)
{
    ProcessAssertion::setClient(newClient);
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY_SIMULATOR)

#endif // PLATFORM(IOS_FAMILY)
