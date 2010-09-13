/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLElementStack.h"

#include "Element.h"
#include "HTMLNames.h"
#include "MathMLNames.h"
#include <wtf/PassOwnPtr.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

namespace {

inline bool isNumberedHeaderElement(Element* element)
{
    return element->hasTagName(h1Tag)
        || element->hasTagName(h2Tag)
        || element->hasTagName(h3Tag)
        || element->hasTagName(h4Tag)
        || element->hasTagName(h5Tag)
        || element->hasTagName(h6Tag);
}

inline bool isScopeMarker(Element* element)
{
    return element->hasTagName(appletTag)
        || element->hasTagName(captionTag)
#if ENABLE(SVG_FOREIGN_OBJECT)
        || element->hasTagName(SVGNames::foreignObjectTag)
#endif
        || element->hasTagName(htmlTag)
        || element->hasTagName(marqueeTag)
        || element->hasTagName(objectTag)
        || element->hasTagName(tableTag)
        || element->hasTagName(tdTag)
        || element->hasTagName(thTag);
}

inline bool isListItemScopeMarker(Element* element)
{
    return isScopeMarker(element)
        || element->hasTagName(olTag)
        || element->hasTagName(ulTag);
}

inline bool isTableScopeMarker(Element* element)
{
    return element->hasTagName(tableTag)
        || element->hasTagName(htmlTag);
}

inline bool isTableBodyScopeMarker(Element* element)
{
    return element->hasTagName(tbodyTag)
        || element->hasTagName(tfootTag)
        || element->hasTagName(theadTag)
        || element->hasTagName(htmlTag);
}

inline bool isTableRowScopeMarker(Element* element)
{
    return element->hasTagName(trTag)
        || element->hasTagName(htmlTag);
}

inline bool isForeignContentScopeMarker(Element* element)
{
    return element->hasTagName(MathMLNames::miTag)
        || element->hasTagName(MathMLNames::moTag)
        || element->hasTagName(MathMLNames::mnTag)
        || element->hasTagName(MathMLNames::msTag)
        || element->hasTagName(MathMLNames::mtextTag)
        || element->hasTagName(SVGNames::foreignObjectTag)
        || element->hasTagName(SVGNames::descTag)
        || element->hasTagName(SVGNames::titleTag)
        || element->namespaceURI() == HTMLNames::xhtmlNamespaceURI;
}

inline bool isButtonScopeMarker(Element* element)
{
    return isScopeMarker(element)
        || element->hasTagName(buttonTag);
}

}

HTMLElementStack::ElementRecord::ElementRecord(PassRefPtr<Element> element, PassOwnPtr<ElementRecord> next)
    : m_element(element)
    , m_next(next)
{
    ASSERT(m_element);
}

HTMLElementStack::ElementRecord::~ElementRecord()
{
}

void HTMLElementStack::ElementRecord::replaceElement(PassRefPtr<Element> element)
{
    ASSERT(element);
    // FIXME: Should this call finishParsingChildren?
    m_element = element;
}

bool HTMLElementStack::ElementRecord::isAbove(ElementRecord* other) const
{
    for (ElementRecord* below = next(); below; below = below->next()) {
        if (below == other)
            return true;
    }
    return false;
}

HTMLElementStack::HTMLElementStack()
    : m_htmlElement(0)
    , m_headElement(0)
    , m_bodyElement(0)
{
}

HTMLElementStack::~HTMLElementStack()
{
}

bool HTMLElementStack::hasOnlyOneElement() const
{
    return !topRecord()->next();
}

bool HTMLElementStack::secondElementIsHTMLBodyElement() const
{
    // This is used the fragment case of <body> and <frameset> in the "in body"
    // insertion mode.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
    ASSERT(m_htmlElement);
    // If we have a body element, it must always be the second element on the
    // stack, as we always start with an html element, and any other element
    // would cause the implicit creation of a body element.
    return !!m_bodyElement;
}

void HTMLElementStack::popHTMLHeadElement()
{
    ASSERT(top() == m_headElement);
    m_headElement = 0;
    popCommon();
}

void HTMLElementStack::popHTMLBodyElement()
{
    ASSERT(top() == m_bodyElement);
    m_bodyElement = 0;
    popCommon();
}

void HTMLElementStack::popAll()
{
    m_htmlElement = 0;
    m_headElement = 0;
    m_bodyElement = 0;
    while (m_top) {
        top()->finishParsingChildren();
        m_top = m_top->releaseNext();
    }
}

void HTMLElementStack::pop()
{
    ASSERT(!top()->hasTagName(HTMLNames::headTag));
    popCommon();
}

void HTMLElementStack::popUntil(const AtomicString& tagName)
{
    while (!top()->hasLocalName(tagName)) {
        // pop() will ASSERT at <body> if callers fail to check that there is an
        // element with localName |tagName| on the stack of open elements.
        pop();
    }
}

void HTMLElementStack::popUntilPopped(const AtomicString& tagName)
{
    popUntil(tagName);
    pop();
}

void HTMLElementStack::popUntilNumberedHeaderElementPopped()
{
    while (!isNumberedHeaderElement(top()))
        pop();
    pop();
}

void HTMLElementStack::popUntil(Element* element)
{
    while (top() != element)
        pop();
}

void HTMLElementStack::popUntilPopped(Element* element)
{
    popUntil(element);
    pop();
}

void HTMLElementStack::popUntilTableScopeMarker()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-context
    while (!isTableScopeMarker(top()))
        pop();
}

void HTMLElementStack::popUntilTableBodyScopeMarker()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-body-context
    while (!isTableBodyScopeMarker(top()))
        pop();
}

void HTMLElementStack::popUntilTableRowScopeMarker()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-row-context
    while (!isTableRowScopeMarker(top()))
        pop();
}

void HTMLElementStack::popUntilForeignContentScopeMarker()
{
    while (!isForeignContentScopeMarker(top()))
        pop();
}

void HTMLElementStack::pushHTMLHtmlElement(PassRefPtr<Element> element)
{
    ASSERT(!m_top); // <html> should always be the bottom of the stack.
    ASSERT(element->hasTagName(HTMLNames::htmlTag));
    ASSERT(!m_htmlElement);
    m_htmlElement = element.get();
    pushCommon(element);
}

void HTMLElementStack::pushHTMLHeadElement(PassRefPtr<Element> element)
{
    ASSERT(element->hasTagName(HTMLNames::headTag));
    ASSERT(!m_headElement);
    m_headElement = element.get();
    pushCommon(element);
}

void HTMLElementStack::pushHTMLBodyElement(PassRefPtr<Element> element)
{
    ASSERT(element->hasTagName(HTMLNames::bodyTag));
    ASSERT(!m_bodyElement);
    m_bodyElement = element.get();
    pushCommon(element);
}

void HTMLElementStack::push(PassRefPtr<Element> element)
{
    ASSERT(!element->hasTagName(HTMLNames::htmlTag));
    ASSERT(!element->hasTagName(HTMLNames::headTag));
    ASSERT(!element->hasTagName(HTMLNames::bodyTag));
    ASSERT(m_htmlElement);
    pushCommon(element);
}

void HTMLElementStack::insertAbove(PassRefPtr<Element> element, ElementRecord* recordBelow)
{
    ASSERT(element);
    ASSERT(recordBelow);
    ASSERT(m_top);
    ASSERT(!element->hasTagName(HTMLNames::htmlTag));
    ASSERT(!element->hasTagName(HTMLNames::headTag));
    ASSERT(!element->hasTagName(HTMLNames::bodyTag));
    ASSERT(m_htmlElement);
    if (recordBelow == m_top) {
        push(element);
        return;
    }

    for (ElementRecord* recordAbove = m_top.get(); recordAbove; recordAbove = recordAbove->next()) {
        if (recordAbove->next() != recordBelow)
            continue;

        recordAbove->setNext(adoptPtr(new ElementRecord(element, recordAbove->releaseNext())));
        recordAbove->next()->element()->beginParsingChildren();
        return;
    }
    ASSERT_NOT_REACHED();
}

HTMLElementStack::ElementRecord* HTMLElementStack::topRecord() const
{
    ASSERT(m_top);
    return m_top.get();
}

Element* HTMLElementStack::oneBelowTop() const
{
    // We should never be calling this if it could be 0.
    ASSERT(m_top);
    ASSERT(m_top->next());
    return m_top->next()->element();
}

Element* HTMLElementStack::bottom() const
{
    return htmlElement();
}

void HTMLElementStack::removeHTMLHeadElement(Element* element)
{
    ASSERT(m_headElement == element);
    if (m_top->element() == element) {
        popHTMLHeadElement();
        return;
    }
    m_headElement = 0;
    removeNonTopCommon(element);
}

void HTMLElementStack::remove(Element* element)
{
    ASSERT(!element->hasTagName(HTMLNames::headTag));
    if (m_top->element() == element) {
        pop();
        return;
    }
    removeNonTopCommon(element);
}

HTMLElementStack::ElementRecord* HTMLElementStack::find(Element* element) const
{
    for (ElementRecord* pos = m_top.get(); pos; pos = pos->next()) {
        if (pos->element() == element)
            return pos;
    }
    return 0;
}

HTMLElementStack::ElementRecord* HTMLElementStack::topmost(const AtomicString& tagName) const
{
    for (ElementRecord* pos = m_top.get(); pos; pos = pos->next()) {
        if (pos->element()->hasLocalName(tagName))
            return pos;
    }
    return 0;
}

bool HTMLElementStack::contains(Element* element) const
{
    return !!find(element);
}

bool HTMLElementStack::contains(const AtomicString& tagName) const
{
    return !!topmost(tagName);
}

template <bool isMarker(Element*)>
bool inScopeCommon(HTMLElementStack::ElementRecord* top, const AtomicString& targetTag)
{
    for (HTMLElementStack::ElementRecord* pos = top; pos; pos = pos->next()) {
        Element* element = pos->element();
        if (element->hasLocalName(targetTag))
            return true;
        if (isMarker(element))
            return false;
    }
    ASSERT_NOT_REACHED(); // <html> is always on the stack and is a scope marker.
    return false;
}

bool HTMLElementStack::hasOnlyHTMLElementsInScope() const
{
    for (ElementRecord* record = m_top.get(); record; record = record->next()) {
        Element* element = record->element();
        if (element->namespaceURI() != xhtmlNamespaceURI)
            return false;
        if (isScopeMarker(element))
            return true;
    }
    ASSERT_NOT_REACHED(); // <html> is always on the stack and is a scope marker.
    return true;
}

bool HTMLElementStack::hasNumberedHeaderElementInScope() const
{
    for (ElementRecord* record = m_top.get(); record; record = record->next()) {
        Element* element = record->element();
        if (isNumberedHeaderElement(element))
            return true;
        if (isScopeMarker(element))
            return false;
    }
    ASSERT_NOT_REACHED(); // <html> is always on the stack and is a scope marker.
    return false;
}

bool HTMLElementStack::inScope(Element* targetElement) const
{
    for (ElementRecord* pos = m_top.get(); pos; pos = pos->next()) {
        Element* element = pos->element();
        if (element == targetElement)
            return true;
        if (isScopeMarker(element))
            return false;
    }
    ASSERT_NOT_REACHED(); // <html> is always on the stack and is a scope marker.
    return false;
}

bool HTMLElementStack::inScope(const AtomicString& targetTag) const
{
    return inScopeCommon<isScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inScope(const QualifiedName& tagName) const
{
    // FIXME: Is localName() right for non-html elements?
    return inScope(tagName.localName());
}

bool HTMLElementStack::inListItemScope(const AtomicString& targetTag) const
{
    return inScopeCommon<isListItemScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inListItemScope(const QualifiedName& tagName) const
{
    // FIXME: Is localName() right for non-html elements?
    return inListItemScope(tagName.localName());
}

bool HTMLElementStack::inTableScope(const AtomicString& targetTag) const
{
    return inScopeCommon<isTableScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inTableScope(const QualifiedName& tagName) const
{
    // FIXME: Is localName() right for non-html elements?
    return inTableScope(tagName.localName());
}

bool HTMLElementStack::inButtonScope(const AtomicString& targetTag) const
{
    return inScopeCommon<isButtonScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inButtonScope(const QualifiedName& tagName) const
{
    // FIXME: Is localName() right for non-html elements?
    return inButtonScope(tagName.localName());
}

Element* HTMLElementStack::htmlElement() const
{
    ASSERT(m_htmlElement);
    return m_htmlElement;
}

Element* HTMLElementStack::headElement() const
{
    ASSERT(m_headElement);
    return m_headElement;
}

Element* HTMLElementStack::bodyElement() const
{
    ASSERT(m_bodyElement);
    return m_bodyElement;
}

void HTMLElementStack::pushCommon(PassRefPtr<Element> element)
{
    ASSERT(m_htmlElement);
    m_top = adoptPtr(new ElementRecord(element, m_top.release()));
    top()->beginParsingChildren();
}

void HTMLElementStack::popCommon()
{
    ASSERT(!top()->hasTagName(HTMLNames::htmlTag));
    ASSERT(!top()->hasTagName(HTMLNames::headTag) || !m_headElement);
    ASSERT(!top()->hasTagName(HTMLNames::bodyTag) || !m_bodyElement);
    top()->finishParsingChildren();
    m_top = m_top->releaseNext();
}

void HTMLElementStack::removeNonTopCommon(Element* element)
{
    ASSERT(!element->hasTagName(HTMLNames::htmlTag));
    ASSERT(!element->hasTagName(HTMLNames::bodyTag));
    ASSERT(top() != element);
    for (ElementRecord* pos = m_top.get(); pos; pos = pos->next()) {
        if (pos->next()->element() == element) {
            // FIXME: Is it OK to call finishParsingChildren()
            // when the children aren't actually finished?
            element->finishParsingChildren();
            pos->setNext(pos->next()->releaseNext());
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

#ifndef NDEBUG

void HTMLElementStack::show()
{
    for (ElementRecord* record = m_top.get(); record; record = record->next())
        record->element()->showNode();
}

#endif

}
