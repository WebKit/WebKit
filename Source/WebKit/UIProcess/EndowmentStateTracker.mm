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

#import "Logging.h"
#import "RunningBoardServicesSPI.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(IOS_FAMILY)

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

bool EndowmentStateTracker::isApplicationForeground(pid_t pid)
{
    return [endowmentsForHandle(handleForPID(pid)) containsObject:visibilityEndowment];
}

EndowmentStateTracker& EndowmentStateTracker::singleton()
{
    static auto tracker = NeverDestroyed<EndowmentStateTracker>();
    return tracker;
}

EndowmentStateTracker::EndowmentStateTracker()
{
    auto processHandle = [RBSProcessHandle currentProcess];
    auto endowmentNamespaces = endowmentsForHandle(processHandle);

    m_isUserFacing = [endowmentNamespaces containsObject:userfacingEndowment];
    m_isVisible = [endowmentNamespaces containsObject:visibilityEndowment];

    m_processMonitor = [RBSProcessMonitor monitorWithConfiguration:[this, processHandle = retainPtr(processHandle)] (id<RBSProcessMonitorConfiguring> config) {

        RBSProcessPredicate *processPredicate = [RBSProcessPredicate predicateMatchingHandle:processHandle.get()];

        RBSProcessStateDescriptor *stateDescriptor = [RBSProcessStateDescriptor descriptor];
        stateDescriptor.endowmentNamespaces = @[visibilityEndowment, userfacingEndowment];

        [config setPredicates:@[processPredicate]];
        [config setStateDescriptor:stateDescriptor];

        [config setUpdateHandler:[this] (RBSProcessMonitor * _Nonnull monitor, RBSProcessHandle * _Nonnull process, RBSProcessStateUpdate * _Nonnull update) mutable {
            dispatch_async(dispatch_get_main_queue(), [this, endowmentNamespaces = retainPtr(update.state.endowmentNamespaces)] {
                setIsUserFacing([endowmentNamespaces containsObject:userfacingEndowment]);
                setIsVisible([endowmentNamespaces containsObject:visibilityEndowment]);
            });
        }];
    }];
}

void EndowmentStateTracker::addClient(Client& client)
{
    m_clients.add(client);
}

void EndowmentStateTracker::removeClient(Client& client)
{
    m_clients.remove(client);
}

void EndowmentStateTracker::setIsUserFacing(bool isUserFacing)
{
    if (m_isUserFacing == isUserFacing)
        return;
    m_isUserFacing = isUserFacing;

    RELEASE_LOG(ViewState, "%p - EndowmentStateTracker::setIsUserFacing(%{public}s)", this, isUserFacing ? "true" : "false");

    for (auto& client : copyToVector(m_clients)) {
        if (client)
            client->isUserFacingChanged(m_isUserFacing);
    }
}

void EndowmentStateTracker::setIsVisible(bool isVisible)
{
    if (m_isVisible == isVisible)
        return;
    m_isVisible = isVisible;

    RELEASE_LOG(ViewState, "%p - EndowmentStateTracker::setIsVisible(%{public}s)", this, isVisible ? "true" : "false");

    for (auto& client : copyToVector(m_clients)) {
        if (client)
            client->isVisibleChanged(m_isVisible);
    }
}

}

#endif
