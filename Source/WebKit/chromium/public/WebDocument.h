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

#ifndef WebDocument_h
#define WebDocument_h

#include "WebDraggableRegion.h"
#include "WebNode.h"
#include "WebSecurityOrigin.h"
#include "platform/WebReferrerPolicy.h"
#include "platform/WebVector.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore {
class Document;
class DocumentType;
}
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {
class WebAccessibilityObject;
class WebDocumentType;
class WebElement;
class WebFormElement;
class WebFrame;
class WebNodeCollection;
class WebNodeList;
class WebString;
class WebURL;

// Provides readonly access to some properties of a DOM document.
class WebDocument : public WebNode {
public:
    enum UserStyleLevel {
        UserStyleUserLevel,
        UserStyleAuthorLevel
    };

    WebDocument() { }
    WebDocument(const WebDocument& e) : WebNode(e) { }

    WebDocument& operator=(const WebDocument& e)
    {
        WebNode::assign(e);
        return *this;
    }
    void assign(const WebDocument& e) { WebNode::assign(e); }

    WEBKIT_EXPORT WebURL url() const;
    // Note: Security checks should use the securityOrigin(), not url().
    WEBKIT_EXPORT WebSecurityOrigin securityOrigin() const;

    WEBKIT_EXPORT WebString encoding() const;
    WEBKIT_EXPORT WebString contentLanguage() const;

    // The url of the OpenSearch Desription Document (if any).
    WEBKIT_EXPORT WebURL openSearchDescriptionURL() const;

    // Returns the frame the document belongs to or 0 if the document is frameless.
    WEBKIT_EXPORT WebFrame* frame() const;
    WEBKIT_EXPORT bool isHTMLDocument() const;
    WEBKIT_EXPORT bool isXHTMLDocument() const;
    WEBKIT_EXPORT bool isPluginDocument() const;
    WEBKIT_EXPORT WebURL baseURL() const;
    WEBKIT_EXPORT WebURL firstPartyForCookies() const;
    WEBKIT_EXPORT WebElement documentElement() const;
    WEBKIT_EXPORT WebElement body() const;
    WEBKIT_EXPORT WebElement head();
    WEBKIT_EXPORT WebString title() const;
    WEBKIT_EXPORT WebNodeCollection all();
    WEBKIT_EXPORT void forms(WebVector<WebFormElement>&) const;
    WEBKIT_EXPORT void images(WebVector<WebElement>&);
    WEBKIT_EXPORT WebURL completeURL(const WebString&) const;
    WEBKIT_EXPORT WebElement getElementById(const WebString&) const;
    WEBKIT_EXPORT WebNode focusedNode() const;
    WEBKIT_EXPORT WebDocumentType doctype() const;
    WEBKIT_EXPORT void cancelFullScreen();
    WEBKIT_EXPORT WebElement fullScreenElement() const;
    WEBKIT_EXPORT WebDOMEvent createEvent(const WebString& eventType);
    WEBKIT_EXPORT WebReferrerPolicy referrerPolicy() const;
    WEBKIT_EXPORT WebElement createElement(const WebString& tagName);

    // Accessibility support. These methods should only be called on the
    // top-level document, because one accessibility cache spans all of
    // the documents on the page.

    // Gets the accessibility object for this document.
    WEBKIT_EXPORT WebAccessibilityObject accessibilityObject() const;

    // Gets the accessibility object for an object on this page by ID.
    WEBKIT_EXPORT WebAccessibilityObject accessibilityObjectFromID(int axID) const;
    // Inserts the given CSS source code as a user stylesheet in the document.
    // Meant for programatic/one-off injection, as opposed to
    // WebView::addUserStyleSheet which inserts styles for the lifetime of the
    // WebView.
    WEBKIT_EXPORT void insertUserStyleSheet(const WebString& sourceCode, UserStyleLevel);

    WEBKIT_EXPORT WebVector<WebDraggableRegion> draggableRegions() const;

#if WEBKIT_IMPLEMENTATION
    WebDocument(const WTF::PassRefPtr<WebCore::Document>&);
    WebDocument& operator=(const WTF::PassRefPtr<WebCore::Document>&);
    operator WTF::PassRefPtr<WebCore::Document>() const;
#endif
};

} // namespace WebKit

#endif
