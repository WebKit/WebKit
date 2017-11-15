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

#include "WebAnimation.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Seconds.h>

namespace WebCore {

class AnimationEffect;
class Element;
class WebAnimation;

class AnimationTimeline : public RefCounted<AnimationTimeline> {
public:
    bool isDocumentTimeline() const { return m_classType == DocumentTimelineClass; }
    void addAnimation(Ref<WebAnimation>&&);
    void removeAnimation(Ref<WebAnimation>&&);
    std::optional<double> bindingsCurrentTime();
    virtual std::optional<Seconds> currentTime() { return m_currentTime; }
    WEBCORE_EXPORT void setCurrentTime(Seconds);
    WEBCORE_EXPORT String description();
    WEBCORE_EXPORT virtual void pause() { };

    virtual void animationTimingModelDidChange() { };

    const HashSet<RefPtr<WebAnimation>>& animations() const { return m_animations; }
    Vector<RefPtr<WebAnimation>> animationsForElement(Element&);
    void animationWasAddedToElement(WebAnimation&, Element&);
    void animationWasRemovedFromElement(WebAnimation&, Element&);

    virtual ~AnimationTimeline();

protected:
    enum ClassType {
        DocumentTimelineClass
    };

    ClassType classType() const { return m_classType; }

    explicit AnimationTimeline(ClassType);

    const HashMap<RefPtr<Element>, Vector<RefPtr<WebAnimation>>>& elementToAnimationsMap() { return m_elementToAnimationsMap; }

private:
    ClassType m_classType;
    std::optional<Seconds> m_currentTime;
    HashMap<RefPtr<Element>, Vector<RefPtr<WebAnimation>>> m_elementToAnimationsMap;
    HashSet<RefPtr<WebAnimation>> m_animations;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ANIMATION_TIMELINE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::AnimationTimeline& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
