/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "ProcessStateMonitor.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "NetworkProcessMessages.h"
#import "ProcessAssertion.h"
#import "RunningBoardServicesSPI.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

static constexpr double maxPrepareForSuspensionDelayInSecond = 15;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProcessStateMonitor);

ProcessStateMonitor::ProcessStateMonitor(Function<void(bool)>&& becomeSuspendedHandler)
    : m_becomeSuspendedHandler(WTFMove(becomeSuspendedHandler))
    , m_suspendTimer(RunLoop::mainSingleton(), [this] { suspendTimerFired(); })
{
    RELEASE_LOG(ProcessSuspension, "%p - ProcessStateMonitor::ProcessStateMonitor", this);
    RELEASE_ASSERT(RunLoop::isMain());

    m_rbsMonitor = [RBSProcessMonitor monitorWithConfiguration:[weakThis = WeakPtr { *this }](id<RBSProcessMonitorConfiguring> config) mutable {
        RBSProcessStateDescriptor *descriptor = [RBSProcessStateDescriptor descriptor];
        [descriptor setValues:RBSProcessStateValueLegacyAssertions | RBSProcessStateValueModernAssertions];
        [config setStateDescriptor:descriptor];
        [config setPredicates:@[[RBSProcessPredicate predicateMatchingHandle:[RBSProcessHandle currentProcess]]]];
        [config setUpdateHandler:[weakThis = WTFMove(weakThis)](RBSProcessMonitor *monitor, RBSProcessHandle *process, RBSProcessStateUpdate *update) {
            ensureOnMainRunLoop([weakThis] {
                if (weakThis)
                    weakThis->checkRemainingRunTime();
            });
        }];
    }];

    checkRemainingRunTime();
}

ProcessStateMonitor::~ProcessStateMonitor()
{
    RELEASE_LOG(ProcessSuspension, "%p - ProcessStateMonitor::~ProcessStateMonitor", this);
    RELEASE_ASSERT(RunLoop::isMain());

    [m_rbsMonitor.get() invalidate];
    processDidBecomeRunning();
}

void ProcessStateMonitor::processDidBecomeRunning()
{
    if (m_state == State::Running)
        return;

    if (m_state == State::Suspended)
        m_becomeSuspendedHandler(false);
    else {
        ASSERT(m_suspendTimer.isActive());
        m_suspendTimer.stop();
    }

    m_state = State::Running;
}

void ProcessStateMonitor::processWillBeSuspended(Seconds timeout)
{
    if (m_state != State::Running)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - ProcessStateMonitor::processWillBeSuspended starts timer for %fs", this, timeout.value());
    ASSERT(!m_suspendTimer.isActive());
    m_suspendTimer.startOneShot(timeout);
    m_state = State::WillSuspend;
}

void ProcessStateMonitor::processWillBeSuspendedImmediately()
{
    if (m_state == State::Suspended)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - ProcessStateMonitor::processWillBeSuspendedImmediately", this);
    m_suspendTimer.stop();
    m_becomeSuspendedHandler(true);
    m_state = State::Suspended;
}

void ProcessStateMonitor::suspendTimerFired()
{
    if (m_state != State::WillSuspend)
        return;

    RELEASE_LOG(ProcessSuspension, "%p - ProcessStateMonitor::suspendTimerFired", this);
    processWillBeSuspendedImmediately();
}

void ProcessStateMonitor::checkRemainingRunTime()
{
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    // runTime is meaningful only on device.
    double remainingRunTime = [[[RBSProcessHandle currentProcess] activeLimitations] runTime];
    if (remainingRunTime == RBSProcessTimeLimitationNone)
        return processDidBecomeRunning();

    if (remainingRunTime <= maxPrepareForSuspensionDelayInSecond)
        return processWillBeSuspendedImmediately();

    processWillBeSuspended(Seconds(remainingRunTime - maxPrepareForSuspensionDelayInSecond));
#endif
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
