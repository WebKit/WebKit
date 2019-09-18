/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL)

#include "Event.h"

namespace WebCore {

class WebGLContextEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(WebGLContextEvent);
public:
    static Ref<WebGLContextEvent> create(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, const String& statusMessage)
    {
        return adoptRef(*new WebGLContextEvent(type, canBubble, cancelable, statusMessage));
    }

    struct Init : EventInit {
        String statusMessage;
    };

    static Ref<WebGLContextEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new WebGLContextEvent(type, initializer, isTrusted));
    }
    virtual ~WebGLContextEvent();

    const String& statusMessage() const { return m_statusMessage; }

    EventInterface eventInterface() const override;

private:
    WebGLContextEvent(const AtomString& type, CanBubble, IsCancelable, const String& statusMessage);
    WebGLContextEvent(const AtomString&, const Init&, IsTrusted);

    String m_statusMessage;
};

} // namespace WebCore

#endif
