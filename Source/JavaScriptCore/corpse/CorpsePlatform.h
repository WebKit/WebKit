/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#include <wtf/Platform.h>

#if ENABLE(MYA)

#include <utility>
#if OS(DARWIN)
#include <mach/mach.h>
#endif

namespace JSC {
namespace Corpse {

#if OS(DARWIN)
using TaskHandle = mach_port_t;
constexpr TaskHandle invalidTaskHandle = MACH_PORT_NULL;
inline bool isValidTaskHandle(TaskHandle handle) { return MACH_PORT_VALID(handle); }

using KernelResult = kern_return_t;
constexpr KernelResult kernelSuccess = KERN_SUCCESS;
#else
using TaskHandle = int;
constexpr TaskHandle invalidTaskHandle = -1;
inline bool isValidTaskHandle(TaskHandle handle) { return handle >= 0; }

using KernelResult = int; // An errno.
constexpr KernelResult kernelSuccess = 0;
#endif

// Owns a task handle. On Darwin that is a send right, given back on destruction.
class OwnedTaskHandle {
public:
    static OwnedTaskHandle adopt(TaskHandle handle) { return OwnedTaskHandle { handle }; }

    OwnedTaskHandle() = default;
    OwnedTaskHandle(OwnedTaskHandle&& other)
        : m_handle(std::exchange(other.m_handle, invalidTaskHandle))
    {
    }
    OwnedTaskHandle& operator=(OwnedTaskHandle&& other)
    {
        if (this != &other) {
            release();
            m_handle = std::exchange(other.m_handle, invalidTaskHandle);
        }
        return *this;
    }
    ~OwnedTaskHandle() { release(); }

    OwnedTaskHandle(const OwnedTaskHandle&) = delete;
    OwnedTaskHandle& operator=(const OwnedTaskHandle&) = delete;

private:
    explicit OwnedTaskHandle(TaskHandle handle)
        : m_handle(handle)
    {
    }

    void release()
    {
#if OS(DARWIN)
        if (isValidTaskHandle(m_handle))
            mach_port_deallocate(mach_task_self(), m_handle);
#endif
        m_handle = invalidTaskHandle;
    }

    friend TaskHandle taskHandle(const OwnedTaskHandle& owned) { return owned.m_handle; }

    TaskHandle m_handle { invalidTaskHandle };
};

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
