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
#include "HTMLTreeBuilder.h"

#include "Comment.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLToken.h"
#include "HTMLTokenizer.h"
#include "LegacyHTMLDocumentParser.h"
#include "LegacyHTMLTreeBuilder.h"
#include "LocalizedStrings.h"
#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif
#include "NotImplemented.h"
#if ENABLE(SVG)
#include "SVGNames.h"
#endif
#include "ScriptController.h"
#include "Settings.h"
#include "Text.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

using namespace HTMLNames;

namespace {

bool hasImpliedEndTag(Element* element)
{
    return element->hasTagName(ddTag)
        || element->hasTagName(dtTag)
        || element->hasTagName(liTag)
        || element->hasTagName(optionTag)
        || element->hasTagName(optgroupTag)
        || element->hasTagName(pTag)
        || element->hasTagName(rpTag)
        || element->hasTagName(rtTag);
}

} // namespace

template<typename ChildType>
PassRefPtr<ChildType> HTMLConstructionSite::attach(Node* parent, PassRefPtr<ChildType> prpChild)
{
    RefPtr<ChildType> child = prpChild;

    if (m_redirectAttachToFosterParent) {
        fosterParent(child.get());
        return child.release();
    }

    parent->parserAddChild(child);
    // It's slightly unfortunate that we need to hold a reference to child
    // here to call attach().  We should investigate whether we can rely on
    // |parent| to hold a ref at this point.  In the common case (at least
    // for elements), however, we'll get to use this ref in the stack of
    // open elements.
    child->attach();
    return child.release();
}

HTMLConstructionSite::HTMLConstructionSite(Document* document, FragmentScriptingPermission scriptingPermission)
    : m_document(document)
    , m_fragmentScriptingPermission(scriptingPermission)
    , m_redirectAttachToFosterParent(false)
{
}

HTMLConstructionSite::~HTMLConstructionSite()
{
}

void HTMLConstructionSite::insertHTMLHtmlStartTagBeforeHTML(AtomicHTMLToken& token)
{
    RefPtr<Element> element = HTMLHtmlElement::create(m_document);
    element->setAttributeMap(token.takeAtributes(), m_fragmentScriptingPermission);
    m_openElements.pushHTMLHtmlElement(attach(m_document, element.release()));
}

void HTMLConstructionSite::mergeAttributesFromTokenIntoElement(AtomicHTMLToken& token, Element* element)
{
    if (!token.attributes())
        return;

    NamedNodeMap* attributes = element->attributes(false);
    for (unsigned i = 0; i < token.attributes()->length(); ++i) {
        Attribute* attribute = token.attributes()->attributeItem(i);
        if (!attributes->getAttributeItem(attribute->name()))
            element->setAttribute(attribute->name(), attribute->value());
    }
}

void HTMLConstructionSite::insertHTMLHtmlStartTagInBody(AtomicHTMLToken& token)
{
    // FIXME: parse error
    mergeAttributesFromTokenIntoElement(token, m_openElements.htmlElement());
}

void HTMLConstructionSite::insertHTMLBodyStartTagInBody(AtomicHTMLToken& token)
{
    // FIXME: parse error
    notImplemented(); // fragment case
    mergeAttributesFromTokenIntoElement(token, m_openElements.bodyElement());
}

void HTMLConstructionSite::insertDoctype(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::DOCTYPE);
    attach(m_document, DocumentType::create(m_document, token.name(), String::adopt(token.publicIdentifier()), String::adopt(token.systemIdentifier())));
    // FIXME: Move quirks mode detection from DocumentType element to here.
    notImplemented();
    if (token.forceQuirks())
        m_document->setParseMode(Document::Compat);
}

void HTMLConstructionSite::insertComment(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(currentElement(), Comment::create(m_document, token.comment()));
}

void HTMLConstructionSite::insertCommentOnDocument(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(m_document, Comment::create(m_document, token.comment()));
}

void HTMLConstructionSite::insertCommentOnHTMLHtmlElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(m_openElements.htmlElement(), Comment::create(m_document, token.comment()));
}

PassRefPtr<Element> HTMLConstructionSite::createElementAndAttachToCurrent(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    return attach(currentElement(), createElement(token));
}

void HTMLConstructionSite::insertHTMLHtmlElement(AtomicHTMLToken& token)
{
    ASSERT(!m_redirectAttachToFosterParent);
    m_openElements.pushHTMLHtmlElement(createElementAndAttachToCurrent(token));
}

void HTMLConstructionSite::insertHTMLHeadElement(AtomicHTMLToken& token)
{
    ASSERT(!m_redirectAttachToFosterParent);
    m_head = createElementAndAttachToCurrent(token);
    m_openElements.pushHTMLHeadElement(m_head);
}

void HTMLConstructionSite::insertHTMLBodyElement(AtomicHTMLToken& token)
{
    ASSERT(!m_redirectAttachToFosterParent);
    m_openElements.pushHTMLBodyElement(createElementAndAttachToCurrent(token));
}

void HTMLConstructionSite::insertHTMLElement(AtomicHTMLToken& token)
{
    m_openElements.push(createElementAndAttachToCurrent(token));
}

void HTMLConstructionSite::insertSelfClosingHTMLElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    attach(currentElement(), createElement(token));
    // FIXME: Do we want to acknowledge the token's self-closing flag?
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#acknowledge-self-closing-flag
}

void HTMLConstructionSite::insertFormattingElement(AtomicHTMLToken& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#the-stack-of-open-elements
    // Possible active formatting elements include:
    // a, b, big, code, em, font, i, nobr, s, small, strike, strong, tt, and u.
    insertHTMLElement(token);
    m_activeFormattingElements.append(currentElement());
}

void HTMLConstructionSite::insertScriptElement(AtomicHTMLToken& token)
{
    RefPtr<HTMLScriptElement> element = HTMLScriptElement::create(scriptTag, m_document, true);
    element->setAttributeMap(token.takeAtributes(), m_fragmentScriptingPermission);
    m_openElements.push(attach(currentElement(), element.release()));
}

void HTMLConstructionSite::insertTextNode(AtomicHTMLToken& token)
{
    if (Node* lastChild = currentElement()->lastChild()) {
        if (lastChild->isTextNode()) {
            // FIXME: We're only supposed to append to this text node if it
            // was the last text node inserted by the parser.
            CharacterData* textNode = static_cast<CharacterData*>(lastChild);
            textNode->parserAppendData(token.characters());
            return;
        }
    }
    attach(currentElement(), Text::create(m_document, token.characters()));
}

PassRefPtr<Element> HTMLConstructionSite::createElement(AtomicHTMLToken& token)
{
    RefPtr<Element> element = HTMLElementFactory::createHTMLElement(QualifiedName(nullAtom, token.name(), xhtmlNamespaceURI), m_document, 0);
    element->setAttributeMap(token.takeAtributes(), m_fragmentScriptingPermission);
    return element.release();
}

bool HTMLConstructionSite::indexOfFirstUnopenFormattingElement(unsigned& firstUnopenElementIndex) const
{
    if (m_activeFormattingElements.isEmpty())
        return false;
    unsigned index = m_activeFormattingElements.size();
    do {
        --index;
        const HTMLFormattingElementList::Entry& entry = m_activeFormattingElements.at(index);
        if (entry.isMarker() || m_openElements.contains(entry.element())) {
            firstUnopenElementIndex = index + 1;
            return firstUnopenElementIndex < m_activeFormattingElements.size();
        }
    } while (index);
    firstUnopenElementIndex = index;
    return true;
}

void HTMLConstructionSite::reconstructTheActiveFormattingElements()
{
    unsigned firstUnopenElementIndex;
    if (!indexOfFirstUnopenFormattingElement(firstUnopenElementIndex))
        return;

    unsigned unopenEntryIndex = firstUnopenElementIndex;
    ASSERT(unopenEntryIndex < m_activeFormattingElements.size());
    for (; unopenEntryIndex < m_activeFormattingElements.size(); ++unopenEntryIndex) {
        HTMLFormattingElementList::Entry& unopenedEntry = m_activeFormattingElements.at(unopenEntryIndex);
        // FIXME: We're supposed to save the original token in the entry.
        AtomicHTMLToken fakeToken(HTMLToken::StartTag, unopenedEntry.element()->localName());
        insertHTMLElement(fakeToken);
        unopenedEntry.replaceElement(currentElement());
    }
}

void HTMLConstructionSite::generateImpliedEndTagsWithExclusion(const AtomicString& tagName)
{
    while (hasImpliedEndTag(currentElement()) && !currentElement()->hasLocalName(tagName))
        m_openElements.pop();
}

void HTMLConstructionSite::generateImpliedEndTags()
{
    while (hasImpliedEndTag(currentElement()))
        m_openElements.pop();
}

void HTMLConstructionSite::fosterParent(Node* node)
{
    Element* fosterParentElement = 0;
    HTMLElementStack::ElementRecord* lastTableElementRecord = m_openElements.topmost(tableTag.localName());
    if (lastTableElementRecord) {
        Element* lastTableElement = lastTableElementRecord->element();
        if (lastTableElement->parent()) {
            // FIXME: We need an insertHTMLElement which does not send mutation events.
            ExceptionCode ec = 0;
            lastTableElement->parent()->insertBefore(node, lastTableElement, ec);
            // FIXME: Do we need to call attach()?
            ASSERT(!ec);
            return;
        }
        fosterParentElement = lastTableElementRecord->next()->element();
    } else {
        // Fragment case
        fosterParentElement = m_openElements.bottom(); // <html> element
    }

    fosterParentElement->parserAddChild(node);
    // FIXME: Do we need to call attach()?
}

}
