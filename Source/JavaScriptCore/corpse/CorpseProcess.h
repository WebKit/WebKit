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

#pragma once

#if OS(MACOS) || USE(APPLE_INTERNAL_SDK)

#include <mach/mach.h>
#include <sys/types.h>
#include <wtf/Assertions.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace JSC {
namespace Corpse {

// Represents a target corpse process identified by PID. It manages the Mach task
// port for that process: attach() acquires it, detach() releases it (but keeps the
// PID so the same Process can be reattached later).
class Process final : public RefCounted<Process> {
public:
    static Ref<Process> create(pid_t pid) { return adoptRef(*new Process(pid)); }

    ~Process() { detach(); }

    bool attach();
    void detach();

    pid_t pid() const { return m_pid; }
    mach_port_t taskPort() const { return m_taskPort; }

    bool isAttached() const { return MACH_PORT_VALID(m_taskPort); }

    // The target process may have terminated while we still hold the port.
    bool holdsLiveTask() const;

    // True if the target runs under Rosetta translation. Such a process executes as
    // arm64 whatever its own architecture is, so its thread state describes the
    // translator rather than the program, and cannot be read as the program's.
    bool isTranslated() const;

private:
    explicit Process(pid_t pid)
        : m_pid(pid)
    {
        RELEASE_ASSERT(pid > 0);
    }

    pid_t m_pid;
    mach_port_t m_taskPort { MACH_PORT_NULL };
};

} // namespace Corpse
} // namespace JSC

#endif // OS(MACOS) || USE(APPLE_INTERNAL_SDK)
