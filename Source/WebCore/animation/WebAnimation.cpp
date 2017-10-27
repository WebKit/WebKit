/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebAnimation.h"

#include "AnimationEffect.h"
#include "AnimationTimeline.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<WebAnimation> WebAnimation::create(AnimationTimeline* timeline)
{
    auto result = adoptRef(*new WebAnimation(timeline));

    if (timeline)
        timeline->addAnimation(result.copyRef());
    
    return result;
}

WebAnimation::WebAnimation(AnimationTimeline* timeline)
    : m_timeline(timeline)
{
}

WebAnimation::~WebAnimation()
{
    if (m_timeline)
        m_timeline->removeAnimation(*this);
}

void WebAnimation::setEffect(RefPtr<AnimationEffect>&& effect)
{
    m_effect = WTFMove(effect);
}

std::optional<double> WebAnimation::bindingsStartTime() const
{
    if (m_startTime)
        return m_startTime->value();
    return std::nullopt;
}

void WebAnimation::setBindingsStartTime(std::optional<double> startTime)
{
    if (startTime == std::nullopt)
        m_startTime = std::nullopt;
    else
        m_startTime = Seconds(startTime.value());
}

String WebAnimation::description()
{
    return "Animation";
}

} // namespace WebCore
