/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCAnimationEvents.h"

#include <wtf/OwnPtr.h>

namespace WebCore {

CCAnimationEvent::CCAnimationEvent(int layerId, CCActiveAnimation::TargetProperty targetProperty)
    : m_layerId(layerId)
    , m_targetProperty(targetProperty)
{
}

CCAnimationEvent::~CCAnimationEvent()
{
}

const CCAnimationStartedEvent* CCAnimationEvent::toAnimationStartedEvent() const
{
    ASSERT(type() == Started);
    return static_cast<const CCAnimationStartedEvent*>(this);
}

const CCFloatAnimationFinishedEvent* CCAnimationEvent::toFloatAnimationFinishedEvent() const
{
    ASSERT(type() == FinishedFloatAnimation);
    return static_cast<const CCFloatAnimationFinishedEvent*>(this);
}

const CCTransformAnimationFinishedEvent* CCAnimationEvent::toTransformAnimationFinishedEvent() const
{
    ASSERT(type() == FinishedTransformAnimation);
    return static_cast<const CCTransformAnimationFinishedEvent*>(this);
}

PassOwnPtr<CCAnimationStartedEvent> CCAnimationStartedEvent::create(int layerId, CCActiveAnimation::TargetProperty targetProperty)
{
    return adoptPtr(new CCAnimationStartedEvent(layerId, targetProperty));
}

CCAnimationStartedEvent::CCAnimationStartedEvent(int layerId, CCActiveAnimation::TargetProperty targetProperty)
    : CCAnimationEvent(layerId, targetProperty)
{
}

CCAnimationStartedEvent::~CCAnimationStartedEvent()
{
}

CCAnimationEvent::Type CCAnimationStartedEvent::type() const
{
    return Started;
}

PassOwnPtr<CCFloatAnimationFinishedEvent> CCFloatAnimationFinishedEvent::create(int layerId, CCActiveAnimation::TargetProperty targetProperty, float finalValue)
{
    return adoptPtr(new CCFloatAnimationFinishedEvent(layerId, targetProperty, finalValue));
}

CCFloatAnimationFinishedEvent::CCFloatAnimationFinishedEvent(int layerId, CCActiveAnimation::TargetProperty targetProperty, float finalValue)
    : CCAnimationEvent(layerId, targetProperty)
    , m_finalValue(finalValue)
{
}

CCFloatAnimationFinishedEvent::~CCFloatAnimationFinishedEvent()
{
}

CCAnimationEvent::Type CCFloatAnimationFinishedEvent::type() const
{
    return FinishedFloatAnimation;
}

PassOwnPtr<CCTransformAnimationFinishedEvent> CCTransformAnimationFinishedEvent::create(int layerId, CCActiveAnimation::TargetProperty targetProperty, const TransformationMatrix& finalValue)
{
    return adoptPtr(new CCTransformAnimationFinishedEvent(layerId, targetProperty, finalValue));
}

CCTransformAnimationFinishedEvent::CCTransformAnimationFinishedEvent(int layerId, CCActiveAnimation::TargetProperty targetProperty, const TransformationMatrix& finalValue)
    : CCAnimationEvent(layerId, targetProperty)
    , m_finalValue(finalValue)
{
}

CCTransformAnimationFinishedEvent::~CCTransformAnimationFinishedEvent()
{
}

CCAnimationEvent::Type CCTransformAnimationFinishedEvent::type() const
{
    return FinishedTransformAnimation;
}

} // namespace WebCore
