/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "UIEvent.h"

namespace WebCore {

class CompositionEvent final : public UIEvent {
    WTF_MAKE_ISO_ALLOCATED(CompositionEvent);
public:
    static Ref<CompositionEvent> create(const AtomString& type, RefPtr<WindowProxy>&& view, const String& data)
    {
        return adoptRef(*new CompositionEvent(type, WTFMove(view), data));
    }

    static Ref<CompositionEvent> createForBindings()
    {
        return adoptRef(*new CompositionEvent);
    }

    struct Init : UIEventInit {
        String data;
    };

    static Ref<CompositionEvent> create(const AtomString& type, const Init& initializer)
    {
        return adoptRef(*new CompositionEvent(type, initializer));
    }

    virtual ~CompositionEvent();

    void initCompositionEvent(const AtomString& type, bool canBubble, bool cancelable, RefPtr<WindowProxy>&&, const String& data);

    String data() const { return m_data; }

    EventInterface eventInterface() const override;

private:
    CompositionEvent();
    CompositionEvent(const AtomString& type, RefPtr<WindowProxy>&&, const String&);
    CompositionEvent(const AtomString& type, const Init&);

    bool isCompositionEvent() const override;

    String m_data;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENT(CompositionEvent)
