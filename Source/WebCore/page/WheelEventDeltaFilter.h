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

#pragma once

#include "FloatSize.h"
#include <wtf/Deque.h>

namespace WebCore {

class WheelEventDeltaFilter {
public:
    WheelEventDeltaFilter();
    virtual ~WheelEventDeltaFilter();

    WEBCORE_EXPORT static std::unique_ptr<WheelEventDeltaFilter> create();
    WEBCORE_EXPORT virtual void updateFromDelta(const FloatSize&) = 0;
    WEBCORE_EXPORT virtual void beginFilteringDeltas() = 0;
    WEBCORE_EXPORT virtual void endFilteringDeltas() = 0;
    WEBCORE_EXPORT FloatSize filteredVelocity() const;
    WEBCORE_EXPORT bool isFilteringDeltas() const;
    WEBCORE_EXPORT FloatSize filteredDelta() const;

protected:
    FloatSize m_currentFilteredDelta;
    FloatSize m_currentFilteredVelocity;
    bool m_isFilteringDeltas { false };
};

enum class DominantScrollGestureDirection {
    None,
    Vertical,
    Horizontal
};

class BasicWheelEventDeltaFilter final : public WheelEventDeltaFilter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BasicWheelEventDeltaFilter();
    void updateFromDelta(const FloatSize&) override;
    void beginFilteringDeltas() override;
    void endFilteringDeltas() override;

private:
    DominantScrollGestureDirection dominantScrollGestureDirection() const;

    Deque<FloatSize> m_recentWheelEventDeltas;
};

} // namespace WebCore
