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

#pragma once

#include "CSSValue.h"
#include "ComputedEffectTiming.h"
#include "RenderStyle.h"
#include "WebAnimation.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/Markable.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Seconds.h>

namespace WebCore {

class CSSAnimation;
class CSSTransition;
class DeclarativeAnimation;
class Element;

class AnimationTimeline : public RefCounted<AnimationTimeline>, public CanMakeWeakPtr<AnimationTimeline> {
public:
    virtual ~AnimationTimeline();

    virtual bool isDocumentTimeline() const { return false; }

    const AnimationCollection& relevantAnimations() const { return m_animations; }

    void forgetAnimation(WebAnimation*);
    virtual void animationTimingDidChange(WebAnimation&);
    virtual void removeAnimation(WebAnimation&);

    Optional<double> bindingsCurrentTime();
    virtual Optional<Seconds> currentTime() { return m_currentTime; }

    void elementWasRemoved(Element&);

    void willChangeRendererForElement(Element&);
    void cancelDeclarativeAnimationsForElement(Element&, WebAnimation::Silently);

    virtual void animationWasAddedToElement(WebAnimation&, Element&);
    virtual void animationWasRemovedFromElement(WebAnimation&, Element&);

    void removeDeclarativeAnimationFromListsForOwningElement(WebAnimation&, Element&);

    void updateCSSAnimationsForElement(Element&, const RenderStyle* currentStyle, const RenderStyle& afterChangeStyle);
    void updateCSSTransitionsForElement(Element&, const RenderStyle& currentStyle, const RenderStyle& newStyle);

protected:
    explicit AnimationTimeline();

    Vector<WeakPtr<WebAnimation>> m_allAnimations;
    AnimationCollection m_animations;

private:
    void updateGlobalPosition(WebAnimation&);
    void updateCSSTransitionsForElementAndProperty(Element&, CSSPropertyID, const RenderStyle& currentStyle, const RenderStyle& afterChangeStyle, const MonotonicTime);
    void removeCSSAnimationCreatedByMarkup(Element&, CSSAnimation&);

    Markable<Seconds, Seconds::MarkableTraits> m_currentTime;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_TIMELINE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::AnimationTimeline& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
