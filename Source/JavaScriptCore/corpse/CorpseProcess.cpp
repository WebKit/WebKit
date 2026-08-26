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
#include "CorpseProcess.h"

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include "CorpseError.h"

#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_traps.h>
#include <signal.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Corpse {

// A task port name outlives the task it named: when the target exits, the right we
// hold becomes a dead name while the name itself is unchanged. MACH_PORT_VALID only
// looks at the name, so it keeps reporting the port as good. Asking the kernel which
// pid the port names is what tells a still-attached process apart from one that has
// since exited -- and, because the answer is compared against m_pid, from a later
// process that inherited the same pid.
bool Process::holdsLiveTask() const
{
    if (!MACH_PORT_VALID(m_taskPort))
        return false;
    int pid = -1;
    return pid_for_task(m_taskPort, &pid) == KERN_SUCCESS && pid == m_pid;
}

bool Process::isTranslated() const
{
    struct kinfo_proc info;
    size_t length = sizeof info;
    int selector[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, m_pid };
    // A pid that no longer exists is not an error here: sysctl succeeds and reports
    // that it wrote nothing, so the size has to be checked rather than the result.
    if (sysctl(selector, 4, &info, &length, nullptr, 0) || length < sizeof info)
        return false;
    return info.kp_proc.p_flag & P_TRANSLATED;
}

bool Process::attach()
{
    if (isAttached()) {
        if (holdsLiveTask())
            return true;
        // The target exited while we held its port.
        detach();
    }

    mach_port_t taskPort = MACH_PORT_NULL;
    kern_return_t kr = task_for_pid(mach_task_self(), m_pid, &taskPort);
    if (kr == KERN_SUCCESS) {
        m_taskPort = taskPort;
        return true;
    }

    if (kill(m_pid, 0) && errno == ESRCH)
        Error::report("No process with PID %d", static_cast<int>(m_pid));
    else {
        Error::report("Could not attach to PID %u: %s (0x%x) -- may need to run as root "
            "or add the appropriate debugger entitlement",
            static_cast<unsigned>(m_pid), mach_error_string(kr), kr);
    }
    return false;
}

void Process::detach()
{
    if (MACH_PORT_VALID(m_taskPort))
        mach_port_deallocate(mach_task_self(), m_taskPort);
    m_taskPort = MACH_PORT_NULL;
}

} // namespace Corpse
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
