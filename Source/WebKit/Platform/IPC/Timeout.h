/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <wtf/ApproximateTime.h>
#include <wtf/TimeWithDynamicClockType.h>

namespace IPC {

class Timeout {
public:
    Timeout(Seconds timeDelta)
        : m_deadline(std::isinf(timeDelta) ? ApproximateTime::infinity() : ApproximateTime::now() + timeDelta)
    {
    }

    static constexpr Timeout infinity() { return Timeout { }; }
    bool isInfinity() const { return std::isinf(m_deadline); }
    static Timeout now() { return 0_s; }
    Seconds secondsUntilDeadline() const { return std::max(m_deadline - ApproximateTime::now(), 0_s ); }
    constexpr ApproximateTime deadline() const { return m_deadline; }
    bool didTimeOut() const { return ApproximateTime::now() >= m_deadline; }

private:
    explicit constexpr Timeout()
        : m_deadline(ApproximateTime::infinity())
    {
    }

    ApproximateTime m_deadline;
};

}
