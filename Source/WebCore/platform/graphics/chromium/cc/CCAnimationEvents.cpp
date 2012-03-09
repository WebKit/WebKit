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

CCAnimationEvent::CCAnimationEvent(int layerId)
    : m_layerId(layerId)
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

const CCAnimationFinishedEvent* CCAnimationEvent::toAnimationFinishedEvent() const
{
    ASSERT(type() == Finished);
    return static_cast<const CCAnimationFinishedEvent*>(this);
}

PassOwnPtr<CCAnimationStartedEvent> CCAnimationStartedEvent::create(int layerId)
{
    return adoptPtr(new CCAnimationStartedEvent(layerId));
}

CCAnimationStartedEvent::CCAnimationStartedEvent(int layerId)
    : CCAnimationEvent(layerId)
{
}

CCAnimationStartedEvent::~CCAnimationStartedEvent()
{
}

CCAnimationEvent::Type CCAnimationStartedEvent::type() const
{
    return Started;
}

PassOwnPtr<CCAnimationFinishedEvent> CCAnimationFinishedEvent::create(int layerId, int animationId)
{
    return adoptPtr(new CCAnimationFinishedEvent(layerId, animationId));
}

CCAnimationFinishedEvent::CCAnimationFinishedEvent(int layerId, int animationId)
    : CCAnimationEvent(layerId)
    , m_animationId(animationId)
{
}

CCAnimationFinishedEvent::~CCAnimationFinishedEvent()
{
}

CCAnimationEvent::Type CCAnimationFinishedEvent::type() const
{
    return Finished;
}

} // namespace WebCore
