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

#pragma once

#include "CSSPropertyNames.h"
#include "DeclarativeAnimation.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Ref.h>

namespace WebCore {

class Animation;
class Element;
class RenderStyle;

class CSSTransition final : public DeclarativeAnimation {
    WTF_MAKE_ISO_ALLOCATED(CSSTransition);
public:
    static Ref<CSSTransition> create(Element&, CSSPropertyID, MonotonicTime generationTime, const Animation&, const RenderStyle* oldStyle, const RenderStyle& newStyle, Seconds delay, Seconds duration, const RenderStyle& reversingAdjustedStartStyle, double);
    ~CSSTransition() = default;

    bool isCSSTransition() const override { return true; }
    String transitionProperty() const { return getPropertyNameString(m_property); }
    CSSPropertyID property() const { return m_property; }
    MonotonicTime generationTime() const { return m_generationTime; }
    const RenderStyle& targetStyle() const { return *m_targetStyle; }
    const RenderStyle& currentStyle() const { return *m_currentStyle; }
    const RenderStyle& reversingAdjustedStartStyle() const { return *m_reversingAdjustedStartStyle; }
    double reversingShorteningFactor() const { return m_reversingShorteningFactor; }

    void resolve(RenderStyle&) final;

private:
    CSSTransition(Element&, CSSPropertyID, MonotonicTime generationTime, const Animation&, const RenderStyle& oldStyle, const RenderStyle& targetStyle, const RenderStyle& reversingAdjustedStartStyle, double);
    void setTimingProperties(Seconds delay, Seconds duration);
    Ref<AnimationEventBase> createEvent(const AtomString& eventType, double elapsedTime, const String& pseudoId, Optional<Seconds> timelineTime) final;

    CSSPropertyID m_property;
    MonotonicTime m_generationTime;
    std::unique_ptr<RenderStyle> m_targetStyle;
    std::unique_ptr<RenderStyle> m_currentStyle;
    std::unique_ptr<RenderStyle> m_reversingAdjustedStartStyle;
    double m_reversingShorteningFactor;

};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(CSSTransition, isCSSTransition())
