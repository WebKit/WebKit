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
#include "WebElement.h"
#include "WebDocument.h"
#include "Element.h"
#include "NamedNodeMap.h"
#include "RenderBoxModelObject.h"
#include "RenderObject.h"
#include "ShadowRoot.h"
#include <public/WebRect.h>
#include <wtf/PassRefPtr.h>


using namespace WebCore;

namespace WebKit {

bool WebElement::isFormControlElement() const
{
    return constUnwrap<Element>()->isFormControlElement();
}

bool WebElement::isTextFormControlElement() const
{
    return constUnwrap<Element>()->isTextFormControl();
}

WebString WebElement::tagName() const
{
    return constUnwrap<Element>()->tagName();
}

bool WebElement::hasTagName(const WebString& tagName) const
{
    return equalIgnoringCase(constUnwrap<Element>()->tagName(),
                             tagName.operator String());
}

bool WebElement::hasHTMLTagName(const WebString& tagName) const
{
    // How to create                     class              nodeName localName
    // createElement('input')            HTMLInputElement   INPUT    input
    // createElement('INPUT')            HTMLInputElement   INPUT    input
    // createElementNS(xhtmlNS, 'input') HTMLInputElement   INPUT    input
    // createElementNS(xhtmlNS, 'INPUT') HTMLUnknownElement INPUT    INPUT
    const Element* element = constUnwrap<Element>();
    return HTMLNames::xhtmlNamespaceURI == element->namespaceURI() && element->localName() == String(tagName).lower();
}

bool WebElement::hasAttribute(const WebString& attrName) const
{
    return constUnwrap<Element>()->hasAttribute(attrName);
}

void WebElement::removeAttribute(const WebString& attrName)
{
    unwrap<Element>()->removeAttribute(attrName);
}

WebString WebElement::getAttribute(const WebString& attrName) const
{
    return constUnwrap<Element>()->getAttribute(attrName);
}

bool WebElement::setAttribute(const WebString& attrName, const WebString& attrValue)
{
    ExceptionCode exceptionCode = 0;
    unwrap<Element>()->setAttribute(attrName, attrValue, exceptionCode);
    return !exceptionCode;
}

unsigned WebElement::attributeCount() const
{
    if (!constUnwrap<Element>()->hasAttributes())
        return 0;
    return constUnwrap<Element>()->attributeCount();
}

WebNode WebElement::shadowRoot() const
{
    Node* shadowRoot = constUnwrap<Element>()->shadowRoot()->toNode();
    return WebNode(shadowRoot);
}

WebString WebElement::attributeLocalName(unsigned index) const
{
    if (index >= attributeCount())
        return WebString();
    return constUnwrap<Element>()->attributeItem(index)->localName();
}

WebString WebElement::attributeValue(unsigned index) const
{
    if (index >= attributeCount())
        return WebString();
    return constUnwrap<Element>()->attributeItem(index)->value();
}

WebString WebElement::innerText()
{
    return unwrap<Element>()->innerText();
}

WebString WebElement::computeInheritedLanguage() const
{
    return WebString(constUnwrap<Element>()->computeInheritedLanguage());
}

void WebElement::requestFullScreen()
{
#if ENABLE(FULLSCREEN_API)
    unwrap<Element>()->webkitRequestFullScreen(Element::ALLOW_KEYBOARD_INPUT);
#endif
}

WebDocument WebElement::document() const
{
    return WebDocument(constUnwrap<Element>()->document());
}

WebRect WebElement::boundsInViewportSpace()
{
    return unwrap<Element>()->boundsInRootViewSpace();
}

WebElement::WebElement(const PassRefPtr<Element>& elem)
    : WebNode(elem)
{
}

WebElement& WebElement::operator=(const PassRefPtr<Element>& elem)
{
    m_private = elem;
    return *this;
}

WebElement::operator PassRefPtr<Element>() const
{
    return static_cast<Element*>(m_private.get());
}

} // namespace WebKit
