/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebDOMEvent_h
#define WebDOMEvent_h

#include "WebNode.h"
#include "platform/WebCommon.h"
#include "platform/WebString.h"

namespace WebCore { class Event; }
#if WEBKIT_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebDOMEvent {
public:
    enum PhaseType {
        CapturingPhase     = 1,
        AtTarget           = 2,
        BubblingPhase      = 3
    };

    ~WebDOMEvent() { reset(); }

    WebDOMEvent() : m_private(0) { }
    WebDOMEvent(const WebDOMEvent& e) : m_private(0) { assign(e); }
    WebDOMEvent& operator=(const WebDOMEvent& e)
    {
        assign(e);
        return *this;
    }

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebDOMEvent&);

    bool isNull() const { return !m_private; }

    WEBKIT_EXPORT WebString type() const;
    WEBKIT_EXPORT WebNode target() const;
    WEBKIT_EXPORT WebNode currentTarget() const;

    WEBKIT_EXPORT PhaseType eventPhase() const;
    WEBKIT_EXPORT bool bubbles() const;
    WEBKIT_EXPORT bool cancelable() const;

    WEBKIT_EXPORT bool isUIEvent() const;
    WEBKIT_EXPORT bool isMouseEvent() const;
    WEBKIT_EXPORT bool isMutationEvent() const;
    WEBKIT_EXPORT bool isKeyboardEvent() const;
    WEBKIT_EXPORT bool isTextEvent() const;
    WEBKIT_EXPORT bool isCompositionEvent() const;
    WEBKIT_EXPORT bool isDragEvent() const;
    WEBKIT_EXPORT bool isClipboardEvent() const;
    WEBKIT_EXPORT bool isMessageEvent() const;
    WEBKIT_EXPORT bool isWheelEvent() const;
    WEBKIT_EXPORT bool isBeforeTextInsertedEvent() const;
    WEBKIT_EXPORT bool isOverflowEvent() const;
    WEBKIT_EXPORT bool isPageTransitionEvent() const;
    WEBKIT_EXPORT bool isPopStateEvent() const;
    WEBKIT_EXPORT bool isProgressEvent() const;
    WEBKIT_EXPORT bool isXMLHttpRequestProgressEvent() const;
    WEBKIT_EXPORT bool isWebKitAnimationEvent() const;
    WEBKIT_EXPORT bool isWebKitTransitionEvent() const;
    WEBKIT_EXPORT bool isBeforeLoadEvent() const;

#if WEBKIT_IMPLEMENTATION
    WebDOMEvent(const WTF::PassRefPtr<WebCore::Event>&);
    operator WTF::PassRefPtr<WebCore::Event>() const;
#endif

    template<typename T> T to()
    {
        T res;
        res.WebDOMEvent::assign(*this);
        return res;
    }

    template<typename T> const T toConst() const
    {
        T res;
        res.WebDOMEvent::assign(*this);
        return res;
    }

protected:
    typedef WebCore::Event WebDOMEventPrivate;
    void assign(WebDOMEventPrivate*);
    WebDOMEventPrivate* m_private;

    template<typename T> T* unwrap()
    {
        return static_cast<T*>(m_private);
    }

    template<typename T> const T* constUnwrap() const
    {
        return static_cast<const T*>(m_private);
    }
};

} // namespace WebKit

#endif
