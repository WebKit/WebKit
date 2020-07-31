/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "EndowmentStateTracker.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "RunningBoardServicesSPI.h"
#include <wtf/NeverDestroyed.h>

namespace WebKit {

static NSString* visibilityEndowment = @"com.apple.frontboard.visibility";
static NSString* userfacingEndowment = @"com.apple.launchservices.userfacing";

static RBSProcessHandle *handleForPID(pid_t pid)
{
    RBSProcessIdentifier *processIdentifier = [RBSProcessIdentifier identifierWithPid:pid];
    if (!processIdentifier) {
        RELEASE_LOG_ERROR(ProcessSuspension, "handleForPID: Failed to construct RBSProcessIdentifier from PID %d", pid);
        return nil;
    }

    NSError *error = nil;
    RBSProcessHandle *processHandle = [RBSProcessHandle handleForIdentifier:processIdentifier error:&error];
    if (!processHandle) {
        RELEASE_LOG_ERROR(ProcessSuspension, "endowmentsForPid: Failed to get RBSProcessHandle for process with PID %d, error: %{public}@", pid, error);
        return nil;
    }

    return processHandle;
}

static NSSet<NSString *> *endowmentsForHandle(RBSProcessHandle *processHandle)
{
    if (!processHandle) {
        // We assume foreground when unable to determine state to maintain pre-existing behavior and to avoid
        // not rendering anything when we fail.
        return [NSSet setWithObjects:visibilityEndowment, userfacingEndowment, nil];
    }

    RBSProcessState *state = processHandle.currentState;
    if (state.taskState != RBSTaskStateRunningScheduled) {
        RELEASE_LOG_ERROR(ProcessSuspension, "endowmentsForHandle: Process with PID %d is not running", processHandle.pid);
        return nil;
    }

    return [state endowmentNamespaces];
}

inline auto EndowmentStateTracker::stateFromEndowments(NSSet *endowments) -> State
{
    return State {
        [endowments containsObject:userfacingEndowment],
        [endowments containsObject:visibilityEndowment]
    };
}

bool EndowmentStateTracker::isApplicationForeground(pid_t pid)
{
    return [endowmentsForHandle(handleForPID(pid)) containsObject:visibilityEndowment];
}

EndowmentStateTracker& EndowmentStateTracker::singleton()
{
    static auto tracker = NeverDestroyed<EndowmentStateTracker>();
    return tracker;
}

void EndowmentStateTracker::registerMonitorIfNecessary()
{
    if (m_processMonitor)
        return;

    m_processMonitor = [RBSProcessMonitor monitorWithConfiguration:[this] (id<RBSProcessMonitorConfiguring> config) {
        [config setPredicates:@[[RBSProcessPredicate predicateMatchingHandle:[RBSProcessHandle currentProcess]]]];

        RBSProcessStateDescriptor *stateDescriptor = [RBSProcessStateDescriptor descriptor];
        stateDescriptor.endowmentNamespaces = @[visibilityEndowment, userfacingEndowment];
        [config setStateDescriptor:stateDescriptor];

        [config setUpdateHandler:[this] (RBSProcessMonitor * _Nonnull monitor, RBSProcessHandle * _Nonnull process, RBSProcessStateUpdate * _Nonnull update) mutable {
            dispatch_async(dispatch_get_main_queue(), [this, state = stateFromEndowments(update.state.endowmentNamespaces)]() mutable {
                setState(WTFMove(state));
            });
        }];
    }];
}

void EndowmentStateTracker::addClient(Client& client)
{
    m_clients.add(client);
    registerMonitorIfNecessary();
}

void EndowmentStateTracker::removeClient(Client& client)
{
    m_clients.remove(client);
}

auto EndowmentStateTracker::ensureState() const -> const State&
{
    if (!m_state)
        m_state = stateFromEndowments(endowmentsForHandle([RBSProcessHandle currentProcess]));
    return *m_state;
}

void EndowmentStateTracker::setState(State&& state)
{
    bool isUserFacingChanged = !m_state || m_state->isUserFacing != state.isUserFacing;
    bool isVisibleChanged = !m_state || m_state->isVisible != state.isVisible;
    if (!isUserFacingChanged && !isVisibleChanged)
        return;

    m_state = WTFMove(state);

    RELEASE_LOG(ViewState, "%p - EndowmentStateTracker::setState() isUserFacing: %{public}s isVisible: %{public}s", this, m_state->isUserFacing ? "true" : "false", m_state->isVisible ? "true" : "false");

    for (auto& client : copyToVector(m_clients)) {
        if (isUserFacingChanged && client)
            client->isUserFacingChanged(m_state->isUserFacing);
        if (isVisibleChanged && client)
            client->isVisibleChanged(m_state->isVisible);
    }
}

}

#endif // PLATFORM(IOS_FAMILY)
