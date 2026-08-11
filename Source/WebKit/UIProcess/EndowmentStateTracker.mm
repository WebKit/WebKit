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
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

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
        RELEASE_LOG_ERROR(ProcessSuspension, "handleForPID: Failed to get RBSProcessHandle for process with PID %d, error: %{public}@", pid, error);
        return nil;
    }

    return processHandle;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(EndowmentStateTracker);

inline auto EndowmentStateTracker::stateFromProcessState(RBSProcessState *processState) -> State
{
    State state;
#if ENABLE(ENDOWMENT_BASED_APPLICATION_STATE_TRACKING)
    for (RBSProcessEndowmentInfo *info in processState.endowmentInfos) {
        NSString *endowmentNamespace = info.endowmentNamespace;
        if ([endowmentNamespace isEqualToString:userfacingEndowment])
            state.isUserFacing = true;
        else if ([endowmentNamespace isEqualToString:visibilityEndowment]) {
            state.isVisible = true;
            if (NSString *environment = info.environment)
                state.visibilityEnvironments.add(String(environment));
        }
    }
#else
    NSSet<NSString *> *endowmentNamespaces = processState.endowmentNamespaces;
    state.isUserFacing = [endowmentNamespaces containsObject:protect(userfacingEndowment)];
    state.isVisible = [endowmentNamespaces containsObject:protect(visibilityEndowment)];
#endif
    return state;
}

bool EndowmentStateTracker::isApplicationForeground(pid_t pid)
{
    return stateForHandle(protect(handleForPID(pid))).isVisible;
}

auto EndowmentStateTracker::stateForHandle(RBSProcessHandle *processHandle) -> State
{
    if (processHandle) {
        RBSProcessState *processState = processHandle.currentState;
        if (processState.taskState == RBSTaskStateRunningScheduled)
            return stateFromProcessState(processState);
        RELEASE_LOG_ERROR(ProcessSuspension, "stateForHandle: Process with PID %d is not running", processHandle.pid);
    }

    // Assume foreground when unable to determine state to maintain pre-existing behavior
    // and to avoid not rendering anything when we fail.
    return State { true, true, { } };
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
#if ENABLE(ENDOWMENT_BASED_APPLICATION_STATE_TRACKING)
        stateDescriptor.values = RBSProcessStateValueEndowmentInfos;
#endif
        [config setStateDescriptor:stateDescriptor];

        [config setUpdateHandler:[this] (RBSProcessMonitor * _Nonnull monitor, RBSProcessHandle * _Nonnull process, RBSProcessStateUpdate * _Nonnull update) mutable {
            RunLoop::mainSingleton().dispatch([this, state = stateFromProcessState(update.state)]() mutable {
                setState(WTF::move(state));
            });
        }];
    }];
}

void EndowmentStateTracker::addClient(EndowmentStateTrackerClient& client)
{
    m_clients.add(client);
    registerMonitorIfNecessary();
}

void EndowmentStateTracker::removeClient(EndowmentStateTrackerClient& client)
{
    m_clients.remove(client);
}

auto EndowmentStateTracker::ensureState() const -> const State&
{
    if (!m_state)
        m_state = stateForHandle([RBSProcessHandle currentProcess]);
    return *m_state;
}

void EndowmentStateTracker::setState(State&& state)
{
    bool isUserFacingChanged = !m_state || m_state->isUserFacing != state.isUserFacing;
    bool isVisibleChanged = !m_state || m_state->isVisible != state.isVisible;
    bool visibilityEnvironmentsChanged = !m_state || m_state->visibilityEnvironments != state.visibilityEnvironments;
    if (!isUserFacingChanged && !isVisibleChanged && !visibilityEnvironmentsChanged)
        return;

    m_state = WTF::move(state);

    RELEASE_LOG(ViewState, "%p - EndowmentStateTracker::setState() isUserFacing: %{public}s isVisible: %{public}s visibilityEnvironmentCount: %u", this, m_state->isUserFacing ? "true" : "false", m_state->isVisible ? "true" : "false", m_state->visibilityEnvironments.size());

    m_clients.forEach([&](auto& client) {
        if (isUserFacingChanged)
            client.isUserFacingChanged(m_state->isUserFacing);
        if (isVisibleChanged)
            client.isVisibleChanged(m_state->isVisible);
        if (visibilityEnvironmentsChanged)
            client.visibilityEndowmentEnvironmentsChanged(m_state->visibilityEnvironments);
    });
}

#if ENABLE(ENDOWMENT_BASED_APPLICATION_STATE_TRACKING)
void EndowmentStateTracker::setStateForTesting(bool isUserFacing, bool isVisible)
{
    setState(State { isUserFacing, isVisible });
}
#endif

}

#endif // PLATFORM(IOS_FAMILY)
