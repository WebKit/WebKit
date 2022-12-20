/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "DeclarativeAnimationEvent.h"

namespace WebCore {

class CSSTransitionEvent final : public DeclarativeAnimationEvent {
    WTF_MAKE_ISO_ALLOCATED(CSSTransitionEvent);
public:
    static Ref<CSSTransitionEvent> create(const AtomString& type, WebAnimation* animation, std::optional<Seconds> scheduledTime,  double elapsedTime, PseudoId pseudoId, const String propertyName)
    {
        return adoptRef(*new CSSTransitionEvent(type, animation, scheduledTime, elapsedTime, pseudoId, propertyName));
    }

    struct Init : EventInit {
        String propertyName;
        double elapsedTime { 0 };
        String pseudoElement;
    };

    static Ref<CSSTransitionEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new CSSTransitionEvent(type, initializer, isTrusted));
    }

    virtual ~CSSTransitionEvent();

    bool isCSSTransitionEvent() const final { return true; }

    const String& propertyName() const { return m_propertyName; }

    EventInterface eventInterface() const override { return CSSTransitionEventInterfaceType; }

private:
    CSSTransitionEvent(const AtomString& type, WebAnimation*, std::optional<Seconds> scheduledTime, double elapsedTime, PseudoId, const String propertyName);
    CSSTransitionEvent(const AtomString& type, const Init& initializer, IsTrusted);

    String m_propertyName;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_EVENT_BASE(CSSTransitionEvent, isCSSTransitionEvent())
