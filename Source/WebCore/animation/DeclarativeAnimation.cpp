/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "DeclarativeAnimation.h"

#include "Animation.h"
#include "AnimationEffectTimingReadOnly.h"
#include "Element.h"
#include "KeyframeEffectReadOnly.h"

namespace WebCore {

DeclarativeAnimation::DeclarativeAnimation(Document& document, const Animation& backingAnimation)
    : WebAnimation(document)
    , m_backingAnimation(const_cast<Animation&>(backingAnimation))
{
}

void DeclarativeAnimation::setBackingAnimation(const Animation& backingAnimation)
{
    m_backingAnimation = const_cast<Animation&>(backingAnimation);
    syncPropertiesWithBackingAnimation();
}

void DeclarativeAnimation::initialize(const Element& target)
{
    // We need to suspend invalidation of the animation's keyframe effect during its creation
    // as it would otherwise trigger invalidation of the document's style and this would be
    // incorrect since it would happen during style invalidation.
    suspendEffectInvalidation();

    setEffect(KeyframeEffectReadOnly::create(target));
    setTimeline(&target.document().timeline());
    syncPropertiesWithBackingAnimation();
    if (backingAnimation().playState() == AnimPlayStatePlaying)
        play();

    unsuspendEffectInvalidation();
}

void DeclarativeAnimation::syncPropertiesWithBackingAnimation()
{
    suspendEffectInvalidation();

    auto* timing = effect()->timing();
    timing->setDelay(Seconds(m_backingAnimation->delay()));
    timing->setIterationDuration(Seconds(m_backingAnimation->duration()));
    timing->setTimingFunction(m_backingAnimation->timingFunction());

    unsuspendEffectInvalidation();
}

} // namespace WebCore
