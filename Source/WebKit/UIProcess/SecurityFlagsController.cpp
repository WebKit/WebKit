/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "SecurityFlagsController.h"

#include "GPUProcessProxy.h"
#include "Logging.h"
#include "ModelProcessProxy.h"
#include "NetworkProcessProxy.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

namespace WebKit {

SecurityFlagsController& SecurityFlagsController::singleton()
{
    static NeverDestroyed<SecurityFlagsController> controller;
    return controller.get();
}

void SecurityFlagsController::setDisabledFlagsNamed(const Vector<String>& names)
{
    ASSERT(RunLoop::isMain());

    SecurityFlags newFlags;
    Vector<String> knownNames;
    for (auto& name : names) {
        if (newFlags.disableFlagNamed(name))
            knownNames.append(name);
    }

    // Logged even when nothing changes, because that is the case to diagnose when a disablement does not take effect.
    if (knownNames.size() < names.size())
        RELEASE_LOG(Process, "SecurityFlagsController: ignoring %zu of %zu names, which no flag in this build has", names.size() - knownNames.size(), names.size());

    if (newFlags == m_securityFlags)
        return;

    for (auto& name : knownNames) {
        UNUSED_VARIABLE(name);
        RELEASE_LOG(Process, "SecurityFlagsController: security flag %" PUBLIC_LOG_STRING " is disabled", name.utf8().data());
    }

    m_securityFlags.replaceWith(newFlags);
    propagateToChildProcesses();
}

void SecurityFlagsController::propagateToChildProcesses()
{
    ASSERT(RunLoop::isMain());

    // Processes launched after this point pick the new value up through their creation parameters instead.
    for (Ref networkProcess : NetworkProcessProxy::allNetworkProcesses())
        networkProcess->securityFlagsDidChange(m_securityFlags);

#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated())
        gpuProcess->securityFlagsDidChange(m_securityFlags);
#endif

#if ENABLE(MODEL_PROCESS)
    if (RefPtr modelProcess = ModelProcessProxy::singletonIfCreated())
        modelProcess->securityFlagsDidChange(m_securityFlags);
#endif
}

} // namespace WebKit
