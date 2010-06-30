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
#include "SVGNames.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

using namespace HTMLNames;

class HTMLElementStack::ElementRecord : public Noncopyable {
public:
    ElementRecord(PassRefPtr<Element> element, PassOwnPtr<ElementRecord> next)
        : m_element(element)
        , m_next(next)
    {
    }

    Element* element() const { return m_element.get(); }
    ElementRecord* next() const { return m_next.get(); }
    PassOwnPtr<ElementRecord> releaseNext() { return m_next.release(); }
    void setNext(PassOwnPtr<ElementRecord> next) { m_next = next; }

private:
    RefPtr<Element> m_element;
    OwnPtr<ElementRecord> m_next;
};

HTMLElementStack::HTMLElementStack()
    : m_htmlElement(0)
    , m_headElement(0)
    , m_bodyElement(0)
{
}

HTMLElementStack::~HTMLElementStack()
{
}

void HTMLElementStack::popHTMLHeadElement()
{
    ASSERT(top() == m_headElement);
    m_headElement = 0;
    popCommon();
}

void HTMLElementStack::pop()
{
    ASSERT(!top()->hasTagName(HTMLNames::headTag));
    popCommon();
}

void HTMLElementStack::pushHTMLHtmlElement(PassRefPtr<Element> element)
{
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

Element* HTMLElementStack::top() const
{
    return m_top->element();
}

void HTMLElementStack::removeHTMLHeadElement(Element* element)
{
    ASSERT(m_headElement == element);
    if (m_top->element() == element) {
        popHTMLHeadElement();
        return;
    }
    m_headElement = 0;
    removeNonFirstCommon(element);
}

void HTMLElementStack::remove(Element* element)
{
    ASSERT(!element->hasTagName(HTMLNames::headTag));
    if (m_top->element() == element) {
        pop();
        return;
    }
    removeNonFirstCommon(element);
}

bool HTMLElementStack::contains(Element* element) const
{
    for (ElementRecord* pos = m_top.get(); pos; pos = pos->next()) {
        if (pos->element() == element)
            return true;
    }
    return false;
}

namespace {

inline bool isScopeMarker(const Element* element)
{
    return element->hasTagName(appletTag)
        || element->hasTagName(captionTag)
        || element->hasTagName(appletTag)
        || element->hasTagName(htmlTag)
        || element->hasTagName(tableTag)
        || element->hasTagName(tdTag)
        || element->hasTagName(thTag)
        || element->hasTagName(buttonTag)
        || element->hasTagName(marqueeTag)
        || element->hasTagName(objectTag)
        || element->hasTagName(SVGNames::foreignObjectTag);
}

inline bool isListItemScopeMarker(const Element* element)
{
    return isScopeMarker(element)
        || element->hasTagName(olTag)
        || element->hasTagName(ulTag);
}
inline bool isTableScopeMarker(const Element* element)
{
    return element->hasTagName(htmlTag)
        || element->hasTagName(tableTag);
}

}

template <bool isMarker(const Element*)>
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

bool HTMLElementStack::inListItemScope(const AtomicString& targetTag) const
{
    return inScopeCommon<isListItemScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inTableScope(const AtomicString& targetTag) const
{
    return inScopeCommon<isTableScopeMarker>(m_top.get(), targetTag);
}

Element* HTMLElementStack::htmlElement()
{
    ASSERT(m_htmlElement);
    return m_htmlElement;
}

Element* HTMLElementStack::headElement()
{
    ASSERT(m_headElement);
    return m_headElement;
}

Element* HTMLElementStack::bodyElement()
{
    ASSERT(m_bodyElement);
    return m_bodyElement;
}

void HTMLElementStack::pushCommon(PassRefPtr<Element> element)
{
    m_top.set(new ElementRecord(element, m_top.release()));
    top()->beginParsingChildren();
}

void HTMLElementStack::popCommon()
{
    ASSERT(!top()->hasTagName(HTMLNames::htmlTag));
    ASSERT(!top()->hasTagName(HTMLNames::bodyTag));
    top()->finishParsingChildren();
    m_top = m_top->releaseNext();
}

void HTMLElementStack::removeNonFirstCommon(Element* element)
{
    ASSERT(!element->hasTagName(HTMLNames::htmlTag));
    ASSERT(!element->hasTagName(HTMLNames::bodyTag));
    ElementRecord* pos = m_top.get();
    ASSERT(pos->element() != element);
    while (pos->next()) {
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

}
