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
struct WebRect;

    // Provides access to some properties of a DOM element node.
    class WebElement : public WebNode {
    public:
        WebElement() : WebNode() { }
        WebElement(const WebElement& e) : WebNode(e) { }

        WebElement& operator=(const WebElement& e) { WebNode::assign(e); return *this; }
        void assign(const WebElement& e) { WebNode::assign(e); }

        WEBKIT_EXPORT bool isFormControlElement() const;
        WEBKIT_EXPORT bool isTextFormControlElement() const;
        // Returns the qualified name, which may contain a prefix and a colon.
        WEBKIT_EXPORT WebString tagName() const;
        // Check if this element has the specified qualified name. This function
        // doesn't makes much sense because we have no ways to check namespace
        // URI. Do not use this.
        WEBKIT_EXPORT bool hasTagName(const WebString&) const;
        // Check if this element has the specified local tag name, and the HTML
        // namespace. Tag name matching is case-insensitive.
        WEBKIT_EXPORT bool hasHTMLTagName(const WebString&) const;
        WEBKIT_EXPORT bool hasAttribute(const WebString&) const;
        WEBKIT_EXPORT void removeAttribute(const WebString&);
        WEBKIT_EXPORT WebString getAttribute(const WebString&) const;
        WEBKIT_EXPORT bool setAttribute(const WebString& name, const WebString& value);
        WEBKIT_EXPORT WebString innerText();
        WEBKIT_EXPORT WebDocument document() const;
        WEBKIT_EXPORT void requestFullScreen();
        WEBKIT_EXPORT WebString attributeLocalName(unsigned index) const;
        WEBKIT_EXPORT WebString attributeValue(unsigned index) const;
        WEBKIT_EXPORT unsigned attributeCount() const;
        WEBKIT_EXPORT WebNode shadowRoot() const;

        // Returns the language code specified for this element.  This attribute
        // is inherited, so the returned value is drawn from the closest parent
        // element that has the lang attribute set, or from the HTTP
        // "Content-Language" header as a fallback.
        WEBKIT_EXPORT WebString computeInheritedLanguage() const;

        // Returns the bounds of the element in viewport space. The bounds
        // have been adjusted to include any transformations. This view is
        // also called the Root View in WebKit.
        // This function will update the layout if required.
        WEBKIT_EXPORT WebRect boundsInViewportSpace();

#if WEBKIT_IMPLEMENTATION
        WebElement(const WTF::PassRefPtr<WebCore::Element>&);
        WebElement& operator=(const WTF::PassRefPtr<WebCore::Element>&);
        operator WTF::PassRefPtr<WebCore::Element>() const;
#endif
    };

} // namespace WebKit

#endif
