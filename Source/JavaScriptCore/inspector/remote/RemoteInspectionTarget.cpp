/*
 * Copyright (C) 2013-2023 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteInspectionTarget.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "JSGlobalObjectDebugger.h"
#include "RemoteInspector.h"
#include <wtf/RunLoop.h>

#if PLATFORM(COCOA)
#include <wtf/spi/darwin/OSVariantSPI.h>
#endif

namespace Inspector {

bool RemoteInspectionTarget::remoteControlAllowed() const
{
    return allowsInspectionByPolicy() || hasLocalDebugger();
}

bool RemoteInspectionTarget::allowsInspectionByPolicy() const
{
    switch (m_inspectable) {
    case Inspectable::Yes:
        return true;
    case Inspectable::No:
#if PLATFORM(COCOA)
        static bool allowInternalSecurityPolicies = os_variant_allows_internal_security_policies("com.apple.WebInspector");
        if (allowInternalSecurityPolicies && !RemoteInspector::singleton().isSimulatingCustomerInstall())
            return true;
        FALLTHROUGH;
#endif
    case Inspectable::NoIgnoringInternalPolicies:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool RemoteInspectionTarget::inspectable() const
{
    switch (m_inspectable) {
    case Inspectable::Yes:
        return true;
    case Inspectable::No:
    case Inspectable::NoIgnoringInternalPolicies:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void RemoteInspectionTarget::setInspectable(bool inspectable)
{
    if (inspectable)
        m_inspectable = Inspectable::Yes;
    else {
        if (!JSRemoteInspectorGetInspectionFollowsInternalPolicies())
            m_inspectable = Inspectable::NoIgnoringInternalPolicies;
        else
            m_inspectable = Inspectable::No;
    }

    if (allowsInspectionByPolicy() && automaticInspectionAllowed())
        RemoteInspector::singleton().updateAutomaticInspectionCandidate(this);
    else
        RemoteInspector::singleton().updateTarget(this);
}

void RemoteInspectionTarget::pauseWaitingForAutomaticInspection()
{
    ASSERT(targetIdentifier());
    ASSERT(allowsInspectionByPolicy());
    ASSERT(automaticInspectionAllowed());

    while (RemoteInspector::singleton().waitingForAutomaticInspection(targetIdentifier())) {
        if (RunLoop::cycle(JSGlobalObjectDebugger::runLoopMode()) == RunLoop::CycleResult::Stop)
            break;
    }
}

void RemoteInspectionTarget::unpauseForInitializedInspector()
{
    RemoteInspector::singleton().setupCompleted(targetIdentifier());
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
