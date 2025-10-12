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

#include "DisplayVBlankMonitor.h"
#include <wtf/Condition.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>

namespace WebKit {
class DisplayVBlankMonitorThreaded;
}

namespace WTF {
template<typename T> struct IsDeprecatedTimerSmartPointerException;
template<> struct IsDeprecatedTimerSmartPointerException<WebKit::DisplayVBlankMonitorThreaded> : std::true_type { };
}

namespace WebKit {

class DisplayVBlankMonitorThreaded : public DisplayVBlankMonitor {
public:
    virtual ~DisplayVBlankMonitorThreaded();

protected:
    explicit DisplayVBlankMonitorThreaded(unsigned);

    virtual bool waitForVBlank() const = 0;

private:
    enum class State { Stop, Active, Failed, Invalid };

    void start() final;
    void stop() final;
    bool isActive() final;
    void invalidate() final;

    void setHandler(Function<void()>&&) final;

    bool startThreadIfNeeded();
    void destroyThreadTimerFired();

    RefPtr<Thread> m_thread;
    Lock m_lock;
    Condition m_condition;
    State m_state WTF_GUARDED_BY_LOCK(m_lock) { State::Stop };
    RunLoop::Timer m_destroyThreadTimer;
};

} // namespace WebKit
