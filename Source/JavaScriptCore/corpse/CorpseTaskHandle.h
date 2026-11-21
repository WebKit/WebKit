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

#if ENABLE(LIB_JSC_TOOLS)

#include <utility>

#if OS(DARWIN)
#include <mach/mach.h>
#endif

namespace JSC {
namespace Corpse {

// Owns a task's Cookie (which, on Darwin, is a Mach send right).
class TaskHandle {
public:
#if OS(DARWIN)
    using Cookie = mach_port_t;
    static constexpr Cookie invalidCookie = MACH_PORT_NULL;
    static bool isValidCookie(Cookie cookie) { return MACH_PORT_VALID(cookie); }
#else
    using Cookie = int;
    static constexpr Cookie invalidCookie = -1;
    static bool isValidCookie(Cookie cookie) { return cookie >= 0; }
#endif

    static TaskHandle adopt(Cookie cookie) { return TaskHandle { cookie }; }

    TaskHandle() = default;
    TaskHandle(TaskHandle&& other)
        : m_cookie(std::exchange(other.m_cookie, invalidCookie))
    {
    }

    TaskHandle& operator=(TaskHandle&& other)
    {
        if (this != &other) {
            release();
            m_cookie = std::exchange(other.m_cookie, invalidCookie);
        }
        return *this;
    }
    ~TaskHandle() { release(); }

    TaskHandle(const TaskHandle&) = delete;
    TaskHandle& operator=(const TaskHandle&) = delete;

    // The lifetime of the cookie is bounded by the lifetime of its TaskHandle.
    // So, make sure the TaskHandle remains alive while its cookie is in use.
    Cookie cookie() const { return m_cookie; }
    bool isValid() const { return isValidCookie(m_cookie); }

private:
    explicit TaskHandle(Cookie cookie)
        : m_cookie(cookie)
    {
    }

    void release()
    {
#if OS(DARWIN)
        if (isValid())
            mach_port_deallocate(mach_task_self(), m_cookie);
#endif
        m_cookie = invalidCookie;
    }

    Cookie m_cookie { invalidCookie };
};

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(LIB_JSC_TOOLS)
