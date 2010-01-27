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

#ifndef WebEvent_h
#define WebEvent_h

#include "WebCommon.h"
#include "WebNode.h"
#include "WebString.h"

namespace WebCore { class Event; }
#if WEBKIT_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebEvent {
public:
    enum PhaseType {
        CapturingPhase     = 1,
        AtTarget           = 2,
        BubblingPhase      = 3
    };

    WebEvent() : m_private(0) { }
    WebEvent(const WebEvent& e) : m_private(0) { assign(e); }
    WebEvent& operator=(const WebEvent& e)
    {
        assign(e);
        return *this;
    }

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebEvent&);

    bool isNull() const { return !m_private; }

    WEBKIT_API WebString type() const;
    WEBKIT_API WebNode target() const;
    WEBKIT_API WebNode currentTarget() const;

    WEBKIT_API PhaseType eventPhase() const;
    WEBKIT_API bool bubbles() const;
    WEBKIT_API bool cancelable() const;

    WEBKIT_API bool isUIEvent() const;
    WEBKIT_API bool isMouseEvent() const;
    WEBKIT_API bool isMutationEvent() const;
    WEBKIT_API bool isKeyboardEvent() const;
    WEBKIT_API bool isTextEvent() const;
    WEBKIT_API bool isCompositionEvent() const;
    WEBKIT_API bool isDragEvent() const;
    WEBKIT_API bool isClipboardEvent() const;
    WEBKIT_API bool isMessageEvent() const;
    WEBKIT_API bool isWheelEvent() const;
    WEBKIT_API bool isBeforeTextInsertedEvent() const;
    WEBKIT_API bool isOverflowEvent() const;
    WEBKIT_API bool isPageTransitionEvent() const;
    WEBKIT_API bool isPopStateEvent() const;
    WEBKIT_API bool isProgressEvent() const;
    WEBKIT_API bool isXMLHttpRequestProgressEvent() const;
    WEBKIT_API bool isWebKitAnimationEvent() const;
    WEBKIT_API bool isWebKitTransitionEvent() const;
    WEBKIT_API bool isBeforeLoadEvent() const;

#if WEBKIT_IMPLEMENTATION
    WebEvent(const WTF::PassRefPtr<WebCore::Event>&);
#endif

protected:
    typedef WebCore::Event WebEventPrivate;
    void assign(WebEventPrivate*);
    WebEventPrivate* m_private;

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
