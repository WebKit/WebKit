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

#include "DeclarativeAnimation.h"
#include <wtf/Ref.h>

namespace WebCore {

class Animation;
class Element;
class RenderStyle;

class CSSAnimation final : public DeclarativeAnimation {
    WTF_MAKE_ISO_ALLOCATED(CSSAnimation);
public:
    static Ref<CSSAnimation> create(Element&, const Animation&, const RenderStyle* oldStyle, const RenderStyle& newStyle);
    ~CSSAnimation() = default;

    bool isCSSAnimation() const override { return true; }
    const String& animationName() const { return m_animationName; }

    ExceptionOr<void> bindingsPlay() final;
    ExceptionOr<void> bindingsPause() final;

protected:
    void syncPropertiesWithBackingAnimation() final;
    Ref<AnimationEventBase> createEvent(const AtomString& eventType, double elapsedTime, const String& pseudoId, Optional<Seconds> timelineTime) final;

private:
    CSSAnimation(Element&, const Animation&);

    String m_animationName;
    bool m_stickyPaused { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(CSSAnimation, isCSSAnimation())
