/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebElement_h
#define WebElement_h

#include "WebNode.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class Element; }
#endif

namespace WebKit {
class WebNamedNodeMap;

    // Provides access to some properties of a DOM element node.
    class WebElement : public WebNode {
    public:
        WebElement() : WebNode() { }
        WebElement(const WebElement& e) : WebNode(e) { }

        WebElement& operator=(const WebElement& e) { WebNode::assign(e); return *this; }
        void assign(const WebElement& e) { WebNode::assign(e); }

        WEBKIT_API bool isFormControlElement() const;
        WEBKIT_API bool isTextFormControlElement() const;
        WEBKIT_API WebString tagName() const;
        WEBKIT_API bool hasTagName(const WebString&) const;
        WEBKIT_API bool hasAttribute(const WebString&) const;
        WEBKIT_API WebString getAttribute(const WebString&) const;
        WEBKIT_API bool setAttribute(const WebString& name, const WebString& value);
        WEBKIT_API WebNamedNodeMap attributes() const;
        WEBKIT_API WebString innerText() const;

        WEBKIT_API WebNode shadowRoot();
        WEBKIT_API WebNode ensureShadowRoot();
        WEBKIT_API void removeShadowRoot();
        WEBKIT_API WebString shadowPseudoId() const;

        // Returns the language code specified for this element.  This attribute
        // is inherited, so the returned value is drawn from the closest parent
        // element that has the lang attribute set, or from the HTTP
        // "Content-Language" header as a fallback.
        WEBKIT_API WebString computeInheritedLanguage() const;

#if WEBKIT_IMPLEMENTATION
        WebElement(const WTF::PassRefPtr<WebCore::Element>&);
        WebElement& operator=(const WTF::PassRefPtr<WebCore::Element>&);
        operator WTF::PassRefPtr<WebCore::Element>() const;
#endif
    };

} // namespace WebKit

#endif
