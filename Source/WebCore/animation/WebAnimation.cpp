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
#include "Document.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<WebAnimation> WebAnimation::create(Document& document, AnimationEffect* effect, AnimationTimeline* timeline)
{
    auto result = adoptRef(*new WebAnimation());

    result->setEffect(effect);
    
    // FIXME: the spec mandates distinguishing between an omitted timeline parameter
    // and an explicit null or undefined value (webkit.org/b/179065).
    result->setTimeline(timeline ? timeline : &document.timeline());
    
    return result;
}

WebAnimation::WebAnimation()
{
}

WebAnimation::~WebAnimation()
{
    if (m_timeline)
        m_timeline->removeAnimation(*this);
}

void WebAnimation::setEffect(RefPtr<AnimationEffect>&& effect)
{
    if (effect == m_effect)
        return;

    m_effect = WTFMove(effect);
}

void WebAnimation::setTimeline(RefPtr<AnimationTimeline>&& timeline)
{
    if (timeline == m_timeline)
        return;

    // FIXME: If the animation start time of animation is resolved, make animation’s
    // hold time unresolved (webkit.org/b/178932).

    if (m_timeline)
        m_timeline->removeAnimation(*this);

    if (timeline)
        timeline->addAnimation(*this);

    m_timeline = WTFMove(timeline);
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
        setStartTime(std::nullopt);
    else
        setStartTime(Seconds(startTime.value()));
}

std::optional<Seconds> WebAnimation::startTime() const
{
    return m_startTime;
}

void WebAnimation::setStartTime(std::optional<Seconds> startTime)
{
    if (startTime == m_startTime)
        return;

    m_startTime = startTime;
}

std::optional<double> WebAnimation::bindingsCurrentTime() const
{
    auto time = currentTime();
    if (!time)
        return std::nullopt;
    return time->value();
}

ExceptionOr<void> WebAnimation::setBindingsCurrentTime(std::optional<double> currentTime)
{
    if (!currentTime)
        return Exception { TypeError };
    setCurrentTime(Seconds(currentTime.value()));
    return { };
}

std::optional<Seconds> WebAnimation::currentTime() const
{
    // FIXME: return the hold time when we support pausing (webkit.org/b/178932).

    if (!m_timeline || !m_startTime)
        return std::nullopt;

    auto timelineTime = m_timeline->currentTime();
    if (!timelineTime)
        return std::nullopt;

    return (timelineTime.value() - m_startTime.value()) * m_playbackRate;
}

void WebAnimation::setCurrentTime(std::optional<Seconds> seekTime)
{
    // FIXME: account for hold time when we support it (webkit.org/b/178932),
    // including situations where playbackRate is 0.

    if (!m_timeline) {
        setStartTime(std::nullopt);
        return;
    }

    auto timelineTime = m_timeline->currentTime();
    if (!timelineTime) {
        setStartTime(std::nullopt);
        return;
    }

    setStartTime(timelineTime.value() - (seekTime.value() / m_playbackRate));
}

void WebAnimation::setPlaybackRate(double newPlaybackRate)
{
    if (m_playbackRate == newPlaybackRate)
        return;

    // 3.5.17.1. Updating the playback rate of an animation
    // Changes to the playback rate trigger a compensatory seek so that that the animation's current time
    // is unaffected by the change to the playback rate.
    auto previousTime = currentTime();
    m_playbackRate = newPlaybackRate;
    if (previousTime)
        setCurrentTime(previousTime);
}

String WebAnimation::description()
{
    return "Animation";
}

} // namespace WebCore
