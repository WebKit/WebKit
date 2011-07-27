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

#include "WebFormElement.h"
#include "WebNode.h"
#include "WebSecurityOrigin.h"

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

    WEBKIT_API WebURL url() const;
    // Note: Security checks should use the securityOrigin(), not url().
    WEBKIT_API WebSecurityOrigin securityOrigin() const;

    WEBKIT_API WebString encoding() const;

    // The url of the OpenSearch Desription Document (if any).
    WEBKIT_API WebURL openSearchDescriptionURL() const;

    // Returns the frame the document belongs to or 0 if the document is frameless.
    WEBKIT_API WebFrame* frame() const;
    WEBKIT_API bool isHTMLDocument() const;
    WEBKIT_API bool isXHTMLDocument() const;
    WEBKIT_API bool isPluginDocument() const;
    WEBKIT_API WebURL baseURL() const;
    WEBKIT_API WebURL firstPartyForCookies() const;
    WEBKIT_API WebElement documentElement() const;
    WEBKIT_API WebElement body() const;
    WEBKIT_API WebElement head();
    WEBKIT_API WebString title() const;
    WEBKIT_API WebNodeCollection all();
    WEBKIT_API void forms(WebVector<WebFormElement>&) const;
    WEBKIT_API WebURL completeURL(const WebString&) const;
    WEBKIT_API WebElement getElementById(const WebString&) const;
    WEBKIT_API WebNode focusedNode() const;
    WEBKIT_API WebDocumentType doctype() const;
    WEBKIT_API WebAccessibilityObject accessibilityObject() const;

    // Insert the given text as a STYLE element at the beginning of the
    // document. |elementId| can be empty, but if specified then it is used
    // as the id for the newly inserted element (replacing an existing one
    // with the same id, if any).
    // FIXME: Remove this once Chromium callers have been updated to call
    // insertUserStyleSheet instead.
    WEBKIT_API bool insertStyleText(const WebString& styleText,
                                    const WebString& elementId);

    // Inserts the given CSS source code as a user stylesheet in the document.
    // Meant for programatic/one-off injection, as opposed to
    // WebView::addUserStyleSheet which inserts styles for the lifetime of the
    // WebView.
    WEBKIT_API void insertUserStyleSheet(const WebString& sourceCode, UserStyleLevel);

#if WEBKIT_IMPLEMENTATION
    WebDocument(const WTF::PassRefPtr<WebCore::Document>&);
    WebDocument& operator=(const WTF::PassRefPtr<WebCore::Document>&);
    operator WTF::PassRefPtr<WebCore::Document>() const;
#endif
};

} // namespace WebKit

#endif
