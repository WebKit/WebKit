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
#include "RenderStyle.h"
#include "WebAnimation.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Seconds.h>

namespace WebCore {

class CSSAnimation;
class CSSTransition;
class DeclarativeAnimation;
class Element;

class AnimationTimeline : public RefCounted<AnimationTimeline> {
public:
    bool isDocumentTimeline() const { return m_classType == DocumentTimelineClass; }

    virtual void animationTimingDidChange(WebAnimation&);
    virtual void removeAnimation(WebAnimation&);

    std::optional<double> bindingsCurrentTime();
    virtual std::optional<Seconds> currentTime() { return m_currentTime; }

    virtual void timingModelDidChange() { };

    enum class Ordering { Sorted, Unsorted };
    const ListHashSet<RefPtr<WebAnimation>>& animations() const { return m_animations; }
    Vector<RefPtr<WebAnimation>> animationsForElement(Element&, Ordering ordering = Ordering::Unsorted) const;
    void elementWasRemoved(Element&);
    void removeAnimationsForElement(Element&);
    void cancelDeclarativeAnimationsForElement(Element&);
    virtual void animationWasAddedToElement(WebAnimation&, Element&);
    virtual void animationWasRemovedFromElement(WebAnimation&, Element&);

    void updateCSSAnimationsForElement(Element&, const RenderStyle* currentStyle, const RenderStyle& afterChangeStyle);
    void updateCSSTransitionsForElement(Element&, const RenderStyle& currentStyle, const RenderStyle& afterChangeStyle);

    virtual ~AnimationTimeline();

protected:
    enum ClassType {
        DocumentTimelineClass
    };

    ClassType classType() const { return m_classType; }

    explicit AnimationTimeline(ClassType);

    ListHashSet<RefPtr<WebAnimation>> m_animations;

private:
    HashMap<Element*, ListHashSet<RefPtr<WebAnimation>>>& relevantMapForAnimation(WebAnimation&);
    void cancelOrRemoveDeclarativeAnimation(RefPtr<DeclarativeAnimation>);
    RefPtr<WebAnimation> cssAnimationForElementAndProperty(Element&, CSSPropertyID);

    ClassType m_classType;
    std::optional<Seconds> m_currentTime;
    HashMap<Element*, ListHashSet<RefPtr<WebAnimation>>> m_elementToAnimationsMap;
    HashMap<Element*, ListHashSet<RefPtr<WebAnimation>>> m_elementToCSSAnimationsMap;
    HashMap<Element*, ListHashSet<RefPtr<WebAnimation>>> m_elementToCSSTransitionsMap;
    HashMap<Element*, HashMap<String, RefPtr<CSSAnimation>>> m_elementToCSSAnimationByName;
    HashMap<Element*, HashMap<CSSPropertyID, RefPtr<CSSTransition>>> m_elementToRunningCSSTransitionByCSSPropertyID;
    HashMap<Element*, HashMap<CSSPropertyID, RefPtr<CSSTransition>>> m_elementToCompletedCSSTransitionByCSSPropertyID;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_TIMELINE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::AnimationTimeline& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
