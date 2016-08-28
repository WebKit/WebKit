/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(FONT_LOAD_EVENTS)

#include "CSSFontFaceRule.h"
#include "CSSValue.h"
#include "Event.h"
#include "EventNames.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class DOMError;

struct CSSFontFaceLoadEventInit : public EventInit {
    RefPtr<CSSFontFaceRule> fontface;
    RefPtr<DOMError> error;
};

class CSSFontFaceLoadEvent final : public Event {
public:
    static Ref<CSSFontFaceLoadEvent> create()
    {
        return adoptRef<CSSFontFaceLoadEvent>(*new CSSFontFaceLoadEvent);
    }

    static Ref<CSSFontFaceLoadEvent> create(const AtomicString& type, const CSSFontFaceLoadEventInit& initializer)
    {
        return adoptRef<CSSFontFaceLoadEvent>(*new CSSFontFaceLoadEvent(type, initializer));
    }

    static Ref<CSSFontFaceLoadEvent> createForFontFaceRule(const AtomicString& type, RefPtr<CSSFontFaceRule>&& rule)
    {
        return adoptRef<CSSFontFaceLoadEvent>(*new CSSFontFaceLoadEvent(type, WTFMove(rule), nullptr));
    }

    static Ref<CSSFontFaceLoadEvent> createForError(RefPtr<CSSFontFaceRule>&& rule, RefPtr<DOMError>&& error)
    {
        return adoptRef<CSSFontFaceLoadEvent>(*new CSSFontFaceLoadEvent(eventNames().errorEvent, WTFMove(rule), WTFMove(error)));
    }

    virtual ~CSSFontFaceLoadEvent();

    CSSFontFaceRule* fontface() const { return m_fontface.get(); }
    DOMError* error() const { return m_error.get(); }

    EventInterface eventInterface() const final;

private:
    CSSFontFaceLoadEvent();
    CSSFontFaceLoadEvent(const AtomicString&, RefPtr<CSSFontFaceRule>&&, RefPtr<DOMError>&&);
    CSSFontFaceLoadEvent(const AtomicString&, const CSSFontFaceLoadEventInit&);

    RefPtr<CSSFontFaceRule> m_fontface;
    RefPtr<DOMError> m_error;
};

} // namespace WebCore

#endif // ENABLE(FONT_LOAD_EVENTS)
