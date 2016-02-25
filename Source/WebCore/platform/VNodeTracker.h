/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef VNodeTracker_h
#define VNodeTracker_h

#include "MemoryPressureHandler.h"
#include "Timer.h"
#include <chrono>
#include <wtf/Forward.h>
#include <wtf/RefCounter.h>

namespace WebCore {

class VNodeTracker {
    friend class WTF::NeverDestroyed<VNodeTracker>;
public:
    using PressureHandler = std::function<void(Critical)>;

    enum VNodeCounterType { };
    using VNodeCounter = RefCounter<VNodeCounterType>;
    using Token = VNodeCounter::Token;

    WEBCORE_EXPORT static VNodeTracker& singleton();

    void setPressureHandler(PressureHandler);
    Token token();

private:
    VNodeTracker();

    void checkPressureState();
    void pressureWarningTimerFired();
    std::chrono::milliseconds nextPressureWarningInterval() const;

    void platformInitialize();

    unsigned m_hardVNodeLimit { 400 };
    unsigned m_softVNodeLimit { 300 };
    PressureHandler m_pressureHandler;
    VNodeCounter m_vnodeCounter;
    Timer m_pressureWarningTimer;
    std::chrono::steady_clock::time_point m_lastWarningTime;
};

inline void VNodeTracker::setPressureHandler(PressureHandler handler)
{
    m_pressureHandler = handler;
}

inline auto VNodeTracker::token() -> Token
{
    if (!m_pressureHandler)
        return Token();

    Token token(m_vnodeCounter.count());
    checkPressureState();
    return token;
}

} // namespace WebCore

#endif // VNodeTracker_h
