/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "EventTarget.h"
#include "UIEvent.h"

namespace WebCore {

class Node;

class FocusEvent final : public UIEvent {
public:
    static Ref<FocusEvent> create(const AtomicString& type, bool canBubble, bool cancelable, DOMWindow* view, int detail, RefPtr<EventTarget>&& relatedTarget)
    {
        return adoptRef(*new FocusEvent(type, canBubble, cancelable, view, detail, WTFMove(relatedTarget)));
    }

    struct Init : UIEventInit {
        RefPtr<EventTarget> relatedTarget;
    };

    static Ref<FocusEvent> create(const AtomicString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new FocusEvent(type, initializer, isTrusted));
    }

    EventTarget* relatedTarget() const override { return m_relatedTarget.get(); }
    void setRelatedTarget(RefPtr<EventTarget>&& relatedTarget) { m_relatedTarget = WTFMove(relatedTarget); }

    EventInterface eventInterface() const override;

private:
    FocusEvent(const AtomicString& type, bool canBubble, bool cancelable, DOMWindow*, int, RefPtr<EventTarget>&&);
    FocusEvent(const AtomicString& type, const Init&, IsTrusted);

    bool isFocusEvent() const override;

    RefPtr<EventTarget> m_relatedTarget;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(FocusEvent)
