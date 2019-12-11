/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "DocumentFragment.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLTableElement.h"

namespace WebCore {

using namespace HTMLNames;

namespace {

inline bool isRootNode(HTMLStackItem& item)
{
    return item.isDocumentFragment() || item.hasTagName(htmlTag);
}

inline bool isScopeMarker(HTMLStackItem& item)
{
    return item.hasTagName(appletTag)
        || item.hasTagName(captionTag)
        || item.hasTagName(marqueeTag)
        || item.hasTagName(objectTag)
        || is<HTMLTableElement>(item.node())
        || item.hasTagName(tdTag)
        || item.hasTagName(thTag)
        || item.hasTagName(MathMLNames::miTag)
        || item.hasTagName(MathMLNames::moTag)
        || item.hasTagName(MathMLNames::mnTag)
        || item.hasTagName(MathMLNames::msTag)
        || item.hasTagName(MathMLNames::mtextTag)
        || item.hasTagName(MathMLNames::annotation_xmlTag)
        || item.hasTagName(SVGNames::foreignObjectTag)
        || item.hasTagName(SVGNames::descTag)
        || item.hasTagName(SVGNames::titleTag)
        || item.hasTagName(templateTag)
        || isRootNode(item);
}

inline bool isListItemScopeMarker(HTMLStackItem& item)
{
    return isScopeMarker(item)
        || item.hasTagName(olTag)
        || item.hasTagName(ulTag);
}

inline bool isTableScopeMarker(HTMLStackItem& item)
{
    return is<HTMLTableElement>(item.node())
        || item.hasTagName(templateTag)
        || isRootNode(item);
}

inline bool isTableBodyScopeMarker(HTMLStackItem& item)
{
    return item.hasTagName(tbodyTag)
        || item.hasTagName(tfootTag)
        || item.hasTagName(theadTag)
        || item.hasTagName(templateTag)
        || isRootNode(item);
}

inline bool isTableRowScopeMarker(HTMLStackItem& item)
{
    return item.hasTagName(trTag)
        || item.hasTagName(templateTag)
        || isRootNode(item);
}

inline bool isForeignContentScopeMarker(HTMLStackItem& item)
{
    return HTMLElementStack::isMathMLTextIntegrationPoint(item)
        || HTMLElementStack::isHTMLIntegrationPoint(item)
        || isInHTMLNamespace(item);
}

inline bool isButtonScopeMarker(HTMLStackItem& item)
{
    return isScopeMarker(item)
        || item.hasTagName(buttonTag);
}

inline bool isSelectScopeMarker(HTMLStackItem& item)
{
    return !is<HTMLOptGroupElement>(item.node()) && !is<HTMLOptionElement>(item.node());
}

}

HTMLElementStack::ElementRecord::ElementRecord(Ref<HTMLStackItem>&& item, std::unique_ptr<ElementRecord> next)
    : m_item(WTFMove(item))
    , m_next(WTFMove(next))
{
}

HTMLElementStack::ElementRecord::~ElementRecord() = default;

void HTMLElementStack::ElementRecord::replaceElement(Ref<HTMLStackItem>&& item)
{
    ASSERT(m_item->isElement());
    // FIXME: Should this call finishParsingChildren?
    m_item = WTFMove(item);
}

bool HTMLElementStack::ElementRecord::isAbove(ElementRecord& other) const
{
    for (auto* below = next(); below; below = below->next()) {
        if (below == &other)
            return true;
    }
    return false;
}

HTMLElementStack::~HTMLElementStack() = default;

bool HTMLElementStack::hasOnlyOneElement() const
{
    return !topRecord().next();
}

bool HTMLElementStack::secondElementIsHTMLBodyElement() const
{
    // This is used the fragment case of <body> and <frameset> in the "in body"
    // insertion mode.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
    ASSERT(m_rootNode);
    // If we have a body element, it must always be the second element on the
    // stack, as we always start with an html element, and any other element
    // would cause the implicit creation of a body element.
    return !!m_bodyElement;
}

void HTMLElementStack::popHTMLHeadElement()
{
    ASSERT(&top() == m_headElement);
    m_headElement = nullptr;
    popCommon();
}

void HTMLElementStack::popHTMLBodyElement()
{
    ASSERT(&top() == m_bodyElement);
    m_bodyElement = nullptr;
    popCommon();
}

void HTMLElementStack::popAll()
{
    m_rootNode = nullptr;
    m_headElement = nullptr;
    m_bodyElement = nullptr;
    m_stackDepth = 0;
    while (m_top) {
        topNode().finishParsingChildren();
        m_top = m_top->releaseNext();
    }
}

void HTMLElementStack::pop()
{
    ASSERT(!topStackItem().hasTagName(HTMLNames::headTag));
    popCommon();
}

void HTMLElementStack::popUntil(const AtomString& tagName)
{
    while (!topStackItem().matchesHTMLTag(tagName)) {
        // pop() will ASSERT if a <body>, <head> or <html> will be popped.
        pop();
    }
}

void HTMLElementStack::popUntilPopped(const AtomString& tagName)
{
    popUntil(tagName);
    pop();
}

void HTMLElementStack::popUntilNumberedHeaderElementPopped()
{
    while (!isNumberedHeaderElement(topStackItem()))
        pop();
    pop();
}

void HTMLElementStack::popUntil(Element& element)
{
    while (&top() != &element)
        pop();
}

void HTMLElementStack::popUntilPopped(Element& element)
{
    popUntil(element);
    pop();
}

void HTMLElementStack::popUntilTableScopeMarker()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-context
    while (!isTableScopeMarker(topStackItem()))
        pop();
}

void HTMLElementStack::popUntilTableBodyScopeMarker()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-body-context
    while (!isTableBodyScopeMarker(topStackItem()))
        pop();
}

void HTMLElementStack::popUntilTableRowScopeMarker()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#clear-the-stack-back-to-a-table-row-context
    while (!isTableRowScopeMarker(topStackItem()))
        pop();
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#mathml-text-integration-point
bool HTMLElementStack::isMathMLTextIntegrationPoint(HTMLStackItem& item)
{
    return item.hasTagName(MathMLNames::miTag)
        || item.hasTagName(MathMLNames::moTag)
        || item.hasTagName(MathMLNames::mnTag)
        || item.hasTagName(MathMLNames::msTag)
        || item.hasTagName(MathMLNames::mtextTag);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#html-integration-point
bool HTMLElementStack::isHTMLIntegrationPoint(HTMLStackItem& item)
{
    if (item.hasTagName(MathMLNames::annotation_xmlTag)) {
        const Attribute* encodingAttr = item.findAttribute(MathMLNames::encodingAttr);
        if (encodingAttr) {
            const String& encoding = encodingAttr->value();
            return equalLettersIgnoringASCIICase(encoding, "text/html")
                || equalLettersIgnoringASCIICase(encoding, "application/xhtml+xml");
        }
        return false;
    }
    return item.hasTagName(SVGNames::foreignObjectTag)
        || item.hasTagName(SVGNames::descTag)
        || item.hasTagName(SVGNames::titleTag);
}

void HTMLElementStack::popUntilForeignContentScopeMarker()
{
    while (!isForeignContentScopeMarker(topStackItem()))
        pop();
}
    
void HTMLElementStack::pushRootNode(Ref<HTMLStackItem>&& rootItem)
{
    ASSERT(rootItem->isDocumentFragment());
    pushRootNodeCommon(WTFMove(rootItem));
}

void HTMLElementStack::pushHTMLHtmlElement(Ref<HTMLStackItem>&& item)
{
    ASSERT(item->hasTagName(HTMLNames::htmlTag));
    pushRootNodeCommon(WTFMove(item));
}
    
void HTMLElementStack::pushRootNodeCommon(Ref<HTMLStackItem>&& rootItem)
{
    ASSERT(!m_top);
    ASSERT(!m_rootNode);
    m_rootNode = &rootItem->node();
    pushCommon(WTFMove(rootItem));
}

void HTMLElementStack::pushHTMLHeadElement(Ref<HTMLStackItem>&& item)
{
    ASSERT(item->hasTagName(HTMLNames::headTag));
    ASSERT(!m_headElement);
    m_headElement = &item->element();
    pushCommon(WTFMove(item));
}

void HTMLElementStack::pushHTMLBodyElement(Ref<HTMLStackItem>&& item)
{
    ASSERT(item->hasTagName(HTMLNames::bodyTag));
    ASSERT(!m_bodyElement);
    m_bodyElement = &item->element();
    pushCommon(WTFMove(item));
}

void HTMLElementStack::push(Ref<HTMLStackItem>&& item)
{
    ASSERT(!item->hasTagName(HTMLNames::htmlTag));
    ASSERT(!item->hasTagName(HTMLNames::headTag));
    ASSERT(!item->hasTagName(HTMLNames::bodyTag));
    ASSERT(m_rootNode);
    pushCommon(WTFMove(item));
}

void HTMLElementStack::insertAbove(Ref<HTMLStackItem>&& item, ElementRecord& recordBelow)
{
    ASSERT(m_top);
    ASSERT(!item->hasTagName(HTMLNames::htmlTag));
    ASSERT(!item->hasTagName(HTMLNames::headTag));
    ASSERT(!item->hasTagName(HTMLNames::bodyTag));
    ASSERT(m_rootNode);
    if (&recordBelow == m_top.get()) {
        push(item.copyRef());
        return;
    }

    for (auto* recordAbove = m_top.get(); recordAbove; recordAbove = recordAbove->next()) {
        if (recordAbove->next() != &recordBelow)
            continue;

        ++m_stackDepth;
        recordAbove->setNext(makeUnique<ElementRecord>(WTFMove(item), recordAbove->releaseNext()));
        recordAbove->next()->element().beginParsingChildren();
        return;
    }
    ASSERT_NOT_REACHED();
}

auto HTMLElementStack::topRecord() const -> ElementRecord&
{
    ASSERT(m_top);
    return *m_top;
}

HTMLStackItem* HTMLElementStack::oneBelowTop() const
{
    // We should never call this if there are fewer than 2 elements on the stack.
    ASSERT(m_top);
    ASSERT(m_top->next());
    if (m_top->next()->stackItem().isElement())
        return &m_top->next()->stackItem();
    return nullptr;
}

void HTMLElementStack::removeHTMLHeadElement(Element& element)
{
    ASSERT(m_headElement == &element);
    if (&m_top->element() == &element) {
        popHTMLHeadElement();
        return;
    }
    m_headElement = nullptr;
    removeNonTopCommon(element);
}

void HTMLElementStack::remove(Element& element)
{
    ASSERT(!element.hasTagName(HTMLNames::headTag));
    if (&m_top->element() == &element) {
        pop();
        return;
    }
    removeNonTopCommon(element);
}

auto HTMLElementStack::find(Element& element) const -> ElementRecord*
{
    for (auto* record = m_top.get(); record; record = record->next()) {
        if (&record->node() == &element)
            return record;
    }
    return nullptr;
}

auto HTMLElementStack::topmost(const AtomString& tagName) const -> ElementRecord*
{
    for (auto* record = m_top.get(); record; record = record->next()) {
        if (record->stackItem().matchesHTMLTag(tagName))
            return record;
    }
    return nullptr;
}

bool HTMLElementStack::contains(Element& element) const
{
    return !!find(element);
}

bool HTMLElementStack::contains(const AtomString& tagName) const
{
    return !!topmost(tagName);
}

template <bool isMarker(HTMLStackItem&)> bool inScopeCommon(HTMLElementStack::ElementRecord* top, const AtomString& targetTag)
{
    for (auto* record = top; record; record = record->next()) {
        auto& item = record->stackItem();
        if (item.matchesHTMLTag(targetTag))
            return true;
        if (isMarker(item))
            return false;
    }
    ASSERT_NOT_REACHED(); // <html> is always on the stack and is a scope marker.
    return false;
}

bool HTMLElementStack::hasNumberedHeaderElementInScope() const
{
    for (auto* record = m_top.get(); record; record = record->next()) {
        auto& item = record->stackItem();
        if (isNumberedHeaderElement(item))
            return true;
        if (isScopeMarker(item))
            return false;
    }
    ASSERT_NOT_REACHED(); // <html> is always on the stack and is a scope marker.
    return false;
}

bool HTMLElementStack::inScope(Element& targetElement) const
{
    for (auto* record = m_top.get(); record; record = record->next()) {
        auto& item = record->stackItem();
        if (&item.node() == &targetElement)
            return true;
        if (isScopeMarker(item))
            return false;
    }
    ASSERT_NOT_REACHED(); // <html> is always on the stack and is a scope marker.
    return false;
}

bool HTMLElementStack::inScope(const AtomString& targetTag) const
{
    return inScopeCommon<isScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inScope(const QualifiedName& tagName) const
{
    return inScope(tagName.localName());
}

bool HTMLElementStack::inListItemScope(const AtomString& targetTag) const
{
    return inScopeCommon<isListItemScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inListItemScope(const QualifiedName& tagName) const
{
    return inListItemScope(tagName.localName());
}

bool HTMLElementStack::inTableScope(const AtomString& targetTag) const
{
    return inScopeCommon<isTableScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inTableScope(const QualifiedName& tagName) const
{
    return inTableScope(tagName.localName());
}

bool HTMLElementStack::inButtonScope(const AtomString& targetTag) const
{
    return inScopeCommon<isButtonScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inButtonScope(const QualifiedName& tagName) const
{
    return inButtonScope(tagName.localName());
}

bool HTMLElementStack::inSelectScope(const AtomString& targetTag) const
{
    return inScopeCommon<isSelectScopeMarker>(m_top.get(), targetTag);
}

bool HTMLElementStack::inSelectScope(const QualifiedName& tagName) const
{
    return inSelectScope(tagName.localName());
}

bool HTMLElementStack::hasTemplateInHTMLScope() const
{
    return inScopeCommon<isRootNode>(m_top.get(), templateTag->localName());
}

Element& HTMLElementStack::htmlElement() const
{
    return downcast<Element>(rootNode());
}

Element& HTMLElementStack::headElement() const
{
    ASSERT(m_headElement);
    return *m_headElement;
}

Element& HTMLElementStack::bodyElement() const
{
    ASSERT(m_bodyElement);
    return *m_bodyElement;
}
    
ContainerNode& HTMLElementStack::rootNode() const
{
    ASSERT(m_rootNode);
    return *m_rootNode;
}

void HTMLElementStack::pushCommon(Ref<HTMLStackItem>&& item)
{
    ASSERT(m_rootNode);

    ++m_stackDepth;
    m_top = makeUnique<ElementRecord>(WTFMove(item), WTFMove(m_top));
}

void HTMLElementStack::popCommon()
{
    ASSERT(!topStackItem().hasTagName(HTMLNames::htmlTag));
    ASSERT(!topStackItem().hasTagName(HTMLNames::headTag) || !m_headElement);
    ASSERT(!topStackItem().hasTagName(HTMLNames::bodyTag) || !m_bodyElement);

    top().finishParsingChildren();
    m_top = m_top->releaseNext();

    --m_stackDepth;
}

void HTMLElementStack::removeNonTopCommon(Element& element)
{
    ASSERT(!element.hasTagName(HTMLNames::htmlTag));
    ASSERT(!element.hasTagName(HTMLNames::bodyTag));
    ASSERT(&top() != &element);
    for (auto* record = m_top.get(); record; record = record->next()) {
        if (&record->next()->element() == &element) {
            // FIXME: Is it OK to call finishParsingChildren()
            // when the children aren't actually finished?
            element.finishParsingChildren();
            record->setNext(record->next()->releaseNext());
            --m_stackDepth;
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

auto HTMLElementStack::furthestBlockForFormattingElement(Element& formattingElement) const -> ElementRecord*
{
    ElementRecord* furthestBlock = nullptr;
    for (auto* record = m_top.get(); record; record = record->next()) {
        if (&record->element() == &formattingElement)
            return furthestBlock;
        if (isSpecialNode(record->stackItem()))
            furthestBlock = record;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

#if ENABLE(TREE_DEBUGGING)

void HTMLElementStack::show()
{
    for (auto* record = m_top.get(); record; record = record->next())
        record->element().showNode();
}

#endif

}
