/*
 * Copyright (C) 2023 Igalia S.L.
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

#pragma once

#include <wtf/Condition.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

using PlatformDisplayID = uint32_t;

class DisplayVBlankMonitor {
    WTF_MAKE_TZONE_ALLOCATED(DisplayVBlankMonitor);
public:
    static std::unique_ptr<DisplayVBlankMonitor> create(PlatformDisplayID);
    virtual ~DisplayVBlankMonitor();

    enum class Type { Drm, Timer };
    virtual Type type() const = 0;

    unsigned refreshRate() const { return m_refreshRate; }

    void start();
    void stop();
    bool isActive();
    void invalidate();

    void setHandler(Function<void()>&&);

protected:
    explicit DisplayVBlankMonitor(unsigned);

    virtual bool waitForVBlank() const = 0;

    unsigned m_refreshRate;

private:
    enum class State { Stop, Active, Failed, Invalid };

    bool startThreadIfNeeded();
    void destroyThreadTimerFired();

    RefPtr<Thread> m_thread;
    Lock m_lock;
    Condition m_condition;
    State m_state WTF_GUARDED_BY_LOCK(m_lock) { State::Stop };
    Function<void()> m_handler;
    RunLoop::Timer m_destroyThreadTimer;
};

} // namespace WebKit
