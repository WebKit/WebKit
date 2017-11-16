/*
 * Copyright (C) Canon Inc. 2016
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
#include "AnimationTimeline.h"

#include "DocumentTimeline.h"
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

AnimationTimeline::AnimationTimeline(ClassType classType)
    : m_classType(classType)
{
}

AnimationTimeline::~AnimationTimeline()
{
}

void AnimationTimeline::addAnimation(Ref<WebAnimation>&& animation)
{
    m_animations.add(WTFMove(animation));
    animationTimingModelDidChange();
}

void AnimationTimeline::removeAnimation(Ref<WebAnimation>&& animation)
{
    m_animations.remove(WTFMove(animation));
    animationTimingModelDidChange();
}

std::optional<double> AnimationTimeline::bindingsCurrentTime()
{
    auto time = currentTime();
    if (!time)
        return std::nullopt;
    return time->milliseconds();
}

void AnimationTimeline::setCurrentTime(Seconds currentTime)
{
    m_currentTime = currentTime;
    animationTimingModelDidChange();
}

void AnimationTimeline::animationWasAddedToElement(WebAnimation& animation, Element& element)
{
    auto result = m_elementToAnimationsMap.ensure(&element, [] {
        return Vector<RefPtr<WebAnimation>>();
    });
    result.iterator->value.append(&animation);
}

void AnimationTimeline::animationWasRemovedFromElement(WebAnimation& animation, Element& element)
{
    auto iterator = m_elementToAnimationsMap.find(&element);
    if (iterator == m_elementToAnimationsMap.end())
        return;

    auto& animations = iterator->value;
    animations.removeFirst(&animation);
    if (!animations.size())
        m_elementToAnimationsMap.remove(iterator);
}

Vector<RefPtr<WebAnimation>> AnimationTimeline::animationsForElement(Element& element)
{
    Vector<RefPtr<WebAnimation>> animations;
    if (m_elementToAnimationsMap.contains(&element))
        animations = m_elementToAnimationsMap.get(&element);
    return animations;
}

String AnimationTimeline::description()
{
    TextStream stream;
    int count = 1;
    stream << (m_classType == DocumentTimelineClass ? "DocumentTimeline" : "AnimationTimeline") << " with " << m_animations.size() << " animations:";
    stream << "\n";
    for (const auto& animation : m_animations) {
        writeIndent(stream, 1);
        stream << count << ". " << animation->description();
        stream << "\n";
        count++;
    }
    return stream.release();
}

} // namespace WebCore
