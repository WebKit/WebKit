/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "ScrollAnimation.h"
#include "platform/ScrollTypes.h"
#include "platform/graphics/FloatSize.h"

namespace WebCore {

class FloatPoint;
class TimingFunction;

class ScrollAnimationKeyboard final: public ScrollAnimation {
public:
    ScrollAnimationKeyboard(ScrollAnimationClient&);
    virtual ~ScrollAnimationKeyboard();

    bool retargetActiveAnimation(const FloatPoint&) final;

    bool animateScroll(MonotonicTime);

    bool startKeyboardScroll(ScrollDirection, ScrollGranularity);

private:
    void serviceAnimation(MonotonicTime) final;

    String debugDescription() const final;

    WebCore::FloatSize m_velocity;
    MonotonicTime m_timeAtLastFrame;
    WebCore::ScrollDirection m_direction;
    WebCore::FloatSize m_currentForce;
    WebCore::FloatSize m_maximumVelocity;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLL_ANIMATION(WebCore::ScrollAnimationKeyboard, type() == WebCore::ScrollAnimation::Type::Keyboard)
