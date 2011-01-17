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

#ifndef WebOptionElement_h
#define WebOptionElement_h

#include "WebFormControlElement.h"
#include "WebVector.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class HTMLOptionElement; }
#endif

namespace WebKit {

// Provides readonly access to some properties of a DOM option element node.
class WebOptionElement : public WebFormControlElement {
public:
    WebOptionElement() : WebFormControlElement() { }
    WebOptionElement(const WebOptionElement& element) : WebFormControlElement(element) { }

    WebOptionElement& operator=(const WebOptionElement& element)
    {
        WebFormControlElement::assign(element);
        return *this;
    }
    void assign(const WebOptionElement& element) { WebFormControlElement::assign(element); }

    WEBKIT_API void setValue(const WebString&);
    WEBKIT_API WebString value() const;

    WEBKIT_API int index() const;
    WEBKIT_API WebString text() const;
    WEBKIT_API bool defaultSelected() const;
    WEBKIT_API void setDefaultSelected(bool);
    WEBKIT_API WebString label() const;
    WEBKIT_API bool isEnabled() const;

#if WEBKIT_IMPLEMENTATION
    WebOptionElement(const WTF::PassRefPtr<WebCore::HTMLOptionElement>&);
    WebOptionElement& operator=(const WTF::PassRefPtr<WebCore::HTMLOptionElement>&);
    operator WTF::PassRefPtr<WebCore::HTMLOptionElement>() const;
#endif
};

} // namespace WebKit

#endif
