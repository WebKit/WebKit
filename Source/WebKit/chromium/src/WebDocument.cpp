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

#include "config.h"
#include "WebDocument.h"

#include "AXObjectCache.h"
#include "CSSParserMode.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "Element.h"
#include "HTMLAllCollection.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLElement.h"
#include "HTMLFormElement.h"
#include "HTMLHeadElement.h"
#include "NodeList.h"
#include "SecurityOrigin.h"
#include "WebAccessibilityObject.h"
#include "WebDOMEvent.h"
#include "WebDocumentType.h"
#include "WebElement.h"
#include "WebFormElement.h"
#include "WebFrameImpl.h"
#include "WebNodeCollection.h"
#include "WebNodeList.h"
#include "platform/WebURL.h"

#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

WebURL WebDocument::url() const
{
    return constUnwrap<Document>()->url();
}

WebSecurityOrigin WebDocument::securityOrigin() const
{
    if (!constUnwrap<Document>())
        return WebSecurityOrigin();
    return WebSecurityOrigin(constUnwrap<Document>()->securityOrigin());
}

WebString WebDocument::encoding() const
{
    return constUnwrap<Document>()->encoding();
}

WebURL WebDocument::openSearchDescriptionURL() const
{
    return const_cast<Document*>(constUnwrap<Document>())->openSearchDescriptionURL();
}

WebFrame* WebDocument::frame() const
{
    return WebFrameImpl::fromFrame(constUnwrap<Document>()->frame());
}

bool WebDocument::isHTMLDocument() const
{
    return constUnwrap<Document>()->isHTMLDocument();
}

bool WebDocument::isXHTMLDocument() const
{
    return constUnwrap<Document>()->isXHTMLDocument();
}

bool WebDocument::isPluginDocument() const
{
    return constUnwrap<Document>()->isPluginDocument();
}

WebURL WebDocument::baseURL() const
{
    return constUnwrap<Document>()->baseURL();
}

WebURL WebDocument::firstPartyForCookies() const
{
    return constUnwrap<Document>()->firstPartyForCookies();
}

WebElement WebDocument::documentElement() const
{
    return WebElement(constUnwrap<Document>()->documentElement());
}

WebElement WebDocument::body() const
{
    return WebElement(constUnwrap<Document>()->body());
}

WebElement WebDocument::head()
{
    return WebElement(unwrap<Document>()->head());
}

WebString WebDocument::title() const
{
    return WebString(constUnwrap<Document>()->title());
}

WebNodeCollection WebDocument::all()
{
    return WebNodeCollection(unwrap<Document>()->all());
}

void WebDocument::forms(WebVector<WebFormElement>& results) const
{
    RefPtr<HTMLCollection> forms = const_cast<Document*>(constUnwrap<Document>())->forms();
    size_t sourceLength = forms->length();
    Vector<WebFormElement> temp;
    temp.reserveCapacity(sourceLength);
    for (size_t i = 0; i < sourceLength; ++i) {
        Node* node = forms->item(i);
        // Strange but true, sometimes node can be 0.
        if (node && node->isHTMLElement())
            temp.append(WebFormElement(static_cast<HTMLFormElement*>(node)));
    }
    results.assign(temp);
}

WebURL WebDocument::completeURL(const WebString& partialURL) const
{
    return constUnwrap<Document>()->completeURL(partialURL);
}

WebElement WebDocument::getElementById(const WebString& id) const
{
    return WebElement(constUnwrap<Document>()->getElementById(id));
}

WebNode WebDocument::focusedNode() const
{
    return WebNode(constUnwrap<Document>()->focusedNode());
}

WebDocumentType WebDocument::doctype() const
{
    return WebDocumentType(constUnwrap<Document>()->doctype());
}

void WebDocument::insertUserStyleSheet(const WebString& sourceCode, UserStyleLevel level)
{
    RefPtr<Document> document = unwrap<Document>();

    RefPtr<StyleSheetInternal> parsedSheet = StyleSheetInternal::create(document.get());
    parsedSheet->setIsUserStyleSheet(level == UserStyleUserLevel);
    parsedSheet->parseString(sourceCode);
    document->addUserSheet(parsedSheet.release());
}

void WebDocument::cancelFullScreen()
{
#if ENABLE(FULLSCREEN_API)
    unwrap<Document>()->webkitCancelFullScreen();
#endif
}

WebElement WebDocument::fullScreenElement() const
{
    Element* fullScreenElement = 0;
#if ENABLE(FULLSCREEN_API)
    fullScreenElement = constUnwrap<Document>()->webkitCurrentFullScreenElement();
#endif
    return WebElement(fullScreenElement);
}

WebDOMEvent WebDocument::createEvent(const WebString& eventType)
{
    ExceptionCode ec = 0;
    WebDOMEvent event(unwrap<Document>()->createEvent(eventType, ec));
    if (ec)
        return WebDOMEvent();
    return event;
}

WebReferrerPolicy WebDocument::referrerPolicy() const
{
    return static_cast<WebReferrerPolicy>(constUnwrap<Document>()->referrerPolicy());
}

WebAccessibilityObject WebDocument::accessibilityObject() const
{
    const Document* document = constUnwrap<Document>();
    return WebAccessibilityObject(
        document->axObjectCache()->getOrCreate(document->renderer()));
}

WebAccessibilityObject WebDocument::accessibilityObjectFromID(int axID) const
{
    const Document* document = constUnwrap<Document>();
    return WebAccessibilityObject(
        document->axObjectCache()->objectFromAXID(axID));
}

WebDocument::WebDocument(const PassRefPtr<Document>& elem)
    : WebNode(elem)
{
}

WebDocument& WebDocument::operator=(const PassRefPtr<Document>& elem)
{
    m_private = elem;
    return *this;
}

WebDocument::operator PassRefPtr<Document>() const
{
    return static_cast<Document*>(m_private.get());
}

} // namespace WebKit
