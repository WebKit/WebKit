/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Event.h"
#include "EventInit.h"
#include <wtf/Forward.h>

namespace WebCore {

class Element;
class HTMLElement;

class InvokeEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(InvokeEvent);

public:
    struct Init : EventInit {
        RefPtr<Element> invoker;
        String action;
    };

    static Ref<InvokeEvent> create(const AtomString& type, const Init&, IsTrusted = IsTrusted::No);
    static Ref<InvokeEvent> createForBindings();

    RefPtr<Element> invoker() const;

    String action() const { return m_action; }

private:
    InvokeEvent() = default;
    InvokeEvent(const AtomString& type, const Init&, IsTrusted = IsTrusted::No);

    EventInterface eventInterface() const final;
    bool isInvokeEvent() const final;

    void setInvoker(RefPtr<Element>&& invoker) { m_invoker = WTFMove(invoker); }

    RefPtr<Element> m_invoker;
    String m_action;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(InvokeEvent)
