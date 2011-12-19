/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XMLTreeBuilder.h"

#include "CachedScript.h"
#include "CDATASection.h"
#include "Comment.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Frame.h"
// FIXME: Why are we including HTML entity information in the XML parser?
#include "HTMLEntitySearch.h"
#include "HTMLEntityTable.h"
#include "NewXMLDocumentParser.h"
#include "ProcessingInstruction.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include "XMLNSNames.h"
#include "XMLNames.h"

namespace WebCore {

XMLTreeBuilder::XMLTreeBuilder(NewXMLDocumentParser* parser, Document* document)
    : m_document(document)
    , m_parser(parser)
    , m_isXHTML(false)
    , m_sawFirstElement(false)
{
    m_currentNodeStack.append(NodeStackItem(document));
}

XMLTreeBuilder::XMLTreeBuilder(NewXMLDocumentParser* parser, DocumentFragment* fragment, Element* parent)
    : m_document(fragment->document())
    , m_parser(parser)
    , m_isXHTML(false)
    , m_sawFirstElement(true)
{
    NodeStackItem stackItem(fragment);

    // Figure out namespaces
    Vector<Element*> nodeStack;
    while (parent) {
        nodeStack.append(parent);

        ContainerNode* node = parent->parentNode();
        if (!node || !node->isElementNode())
            break;
        parent = static_cast<Element*>(node);
    }

    if (nodeStack.isEmpty()) {
        m_currentNodeStack.append(stackItem);
        return;
    }

    for (Element* element; !nodeStack.isEmpty(); nodeStack.removeLast()) {
        element = nodeStack.last();
        if (NamedNodeMap* attrs = element->attributes()) {
            for (size_t i = 0; i < attrs->length(); ++i) {
                Attribute* attr = attrs->attributeItem(i);
                if (attr->localName() == xmlnsAtom)
                    stackItem.setNamespaceURI(attr->value());
                else if (attr->prefix() == xmlnsAtom)
                    stackItem.setNamespaceURI(attr->localName(), attr->value());
            }
        }
    }

    // If the parent element is not in document tree, there may be no xmlns attribute; just default to the parent's namespace.
    if (stackItem.namespaceURI().isNull() && !parent->inDocument())
        stackItem.setNamespaceURI(parent->namespaceURI());

    m_currentNodeStack.append(stackItem);
}

void XMLTreeBuilder::processToken(const AtomicXMLToken& token)
{
    switch (token.type()) {
    case XMLTokenTypes::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case XMLTokenTypes::ProcessingInstruction:
        processProcessingInstruction(token);
        break;
    case XMLTokenTypes::XMLDeclaration:
        processXMLDeclaration(token);
        break;
    case XMLTokenTypes::DOCTYPE:
        processDOCTYPE(token);
        break;
    case XMLTokenTypes::StartTag:
        processStartTag(token);
        break;
    case XMLTokenTypes::EndTag:
        processEndTag(token);
        break;
    case XMLTokenTypes::CDATA:
        processCDATA(token);
        break;
    case XMLTokenTypes::Character:
        processCharacter(token);
        break;
    case XMLTokenTypes::Comment:
        processComment(token);
        break;
    case XMLTokenTypes::Entity:
        processEntity(token);
        break;
    case XMLTokenTypes::EndOfFile:
        exitText();
        return;
    }
}

void XMLTreeBuilder::finish()
{
    exitText();
}

void XMLTreeBuilder::pushCurrentNode(const NodeStackItem& stackItem)
{
    ASSERT(stackItem.node());
    m_currentNodeStack.append(stackItem);
    // FIXME: is there a maximum DOM depth?
}

void XMLTreeBuilder::popCurrentNode()
{
    ASSERT(m_currentNodeStack.size());

    m_currentNodeStack.removeLast();
}

void XMLTreeBuilder::closeElement(PassRefPtr<Element> element)
{
    element->finishParsingChildren();

    ScriptElement* scriptElement = toScriptElement(element.get());
    if (scriptElement)
        m_parser->processScript(scriptElement);

    popCurrentNode();
}

void XMLTreeBuilder::processProcessingInstruction(const AtomicXMLToken& token)
{
    if (!failOnText())
        return;

    // FIXME: fall back if we can't handle the PI ourself.

    add(ProcessingInstruction::create(m_document, token.target(), token.data()));
}

void XMLTreeBuilder::processXMLDeclaration(const AtomicXMLToken& token)
{
    if (!failOnText())
        return;

    ExceptionCode ec = 0;

    m_document->setXMLVersion(String(token.xmlVersion()), ec);
    if (ec)
        m_parser->stopParsing();

    m_document->setXMLStandalone(token.xmlStandalone(), ec);
    if (ec)
        m_parser->stopParsing();
    // FIXME: how should this behave if standalone is not specified?
    // FIXME: set encoding.
}

void XMLTreeBuilder::processDOCTYPE(const AtomicXMLToken& token)
{
    DEFINE_STATIC_LOCAL(AtomicString, xhtmlTransitional, ("-//W3C//DTD XHTML 1.0 Transitional//EN"));
    DEFINE_STATIC_LOCAL(AtomicString, xhtml11, ("-//W3C//DTD XHTML 1.1//EN"));
    DEFINE_STATIC_LOCAL(AtomicString, xhtmlStrict, ("-//W3C//DTD XHTML 1.0 Strict//EN"));
    DEFINE_STATIC_LOCAL(AtomicString, xhtmlFrameset, ("-//W3C//DTD XHTML 1.0 Frameset//EN"));
    DEFINE_STATIC_LOCAL(AtomicString, xhtmlBasic, ("-//W3C//DTD XHTML Basic 1.0//EN"));
    DEFINE_STATIC_LOCAL(AtomicString, xhtmlMathML, ("-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN"));
    DEFINE_STATIC_LOCAL(AtomicString, xhtmlMathMLSVG, ("-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN"));
    DEFINE_STATIC_LOCAL(AtomicString, xhtmlMobile, ("-//WAPFORUM//DTD XHTML Mobile 1.0//EN"));

    if (!failOnText())
        return;

    AtomicString publicIdentifier(token.publicIdentifier().data(), token.publicIdentifier().size());
    AtomicString systemIdentifier(token.systemIdentifier().data(), token.systemIdentifier().size());
    RefPtr<DocumentType> doctype = DocumentType::create(m_document, token.name(), publicIdentifier, systemIdentifier);
    m_document->setDocType(doctype);
    m_document->parserAddChild(doctype);

    if ((publicIdentifier == xhtmlTransitional)
        || (publicIdentifier == xhtml11)
        || (publicIdentifier == xhtmlStrict)
        || (publicIdentifier == xhtmlFrameset)
        || (publicIdentifier == xhtmlBasic)
        || (publicIdentifier == xhtmlMathML)
        || (publicIdentifier == xhtmlMathMLSVG)
        || (publicIdentifier == xhtmlMobile))
        m_isXHTML = true;
}

void XMLTreeBuilder::processStartTag(const AtomicXMLToken& token)
{
    exitText();

    bool isFirstElement = !m_sawFirstElement;
    m_sawFirstElement = true;

    NodeStackItem top = m_currentNodeStack.last();

    processNamespaces(token, top);

    QualifiedName qName(token.prefix(), token.name(), top.namespaceForPrefix(token.prefix(), top.namespaceURI()));
    RefPtr<Element> newElement = m_document->createElement(qName, true);

    processAttributes(token, top, newElement);

    newElement->beginParsingChildren();
    m_currentNodeStack.last().node()->parserAddChild(newElement.get());

    top.setNode(newElement);
    pushCurrentNode(top);

    if (!newElement->attached())
        newElement->attach();

    if (isFirstElement && m_document->frame())
        m_document->frame()->loader()->dispatchDocumentElementAvailable();

    if (token.selfClosing())
        closeElement(newElement);
}

void XMLTreeBuilder::processEndTag(const AtomicXMLToken& token)
{
    exitText();

    RefPtr<ContainerNode> node = m_currentNodeStack.last().node();

    if (!node->hasTagName(QualifiedName(token.prefix(), token.name(), m_currentNodeStack.last().namespaceForPrefix(token.prefix(), m_currentNodeStack.last().namespaceURI()))))
        m_parser->stopParsing();

    closeElement(toElement(node.get()));
}

void XMLTreeBuilder::processCharacter(const AtomicXMLToken& token)
{
    appendToText(token.characters().data(), token.characters().size());
}

void XMLTreeBuilder::processCDATA(const AtomicXMLToken& token)
{
    exitText();
    add(CDATASection::create(m_document, token.data()));
}

void XMLTreeBuilder::processComment(const AtomicXMLToken& token)
{
    exitText();
    add(Comment::create(m_document, token.comment()));
}

void XMLTreeBuilder::processEntity(const AtomicXMLToken& token)
{
    // FIXME: we should support internal subset.
    if (m_isXHTML)
        processHTMLEntity(token);
    else
        processXMLEntity(token);
}

void XMLTreeBuilder::processNamespaces(const AtomicXMLToken& token, NodeStackItem& stackItem)
{
    if (!token.attributes())
        return;

    for (size_t i = 0; i < token.attributes()->length(); ++i) {
        Attribute* attribute = token.attributes()->attributeItem(i);
        if (attribute->name().prefix() == xmlnsAtom)
            stackItem.setNamespaceURI(attribute->name().localName(), attribute->value());
        else if (attribute->name() == xmlnsAtom)
            stackItem.setNamespaceURI(attribute->value());
    }
}

void XMLTreeBuilder::processAttributes(const AtomicXMLToken& token, NodeStackItem& stackItem, PassRefPtr<Element> newElement)
{
    if (!token.attributes())
        return;

    for (size_t i = 0; i < token.attributes()->length(); ++i) {
        Attribute* attribute = token.attributes()->attributeItem(i);
        ExceptionCode ec = 0;
        if (attribute->name().prefix() == xmlnsAtom)
            newElement->setAttributeNS(XMLNSNames::xmlnsNamespaceURI, "xmlns:" + attribute->name().localName(), attribute->value(), ec);
        else if (attribute->name() == xmlnsAtom)
            newElement->setAttributeNS(XMLNSNames::xmlnsNamespaceURI, xmlnsAtom, attribute->value(), ec);
        else {
            QualifiedName qName(attribute->prefix(), attribute->localName(), stackItem.namespaceForPrefix(attribute->prefix(), nullAtom));
            newElement->setAttribute(qName, attribute->value(), ec);
        }
        if (ec) {
            m_parser->stopParsing();
            return;
        }
    }
}

void XMLTreeBuilder::processXMLEntity(const AtomicXMLToken& token)
{
    DEFINE_STATIC_LOCAL(AtomicString, amp, ("amp"));
    DEFINE_STATIC_LOCAL(AtomicString, apos, ("apos"));
    DEFINE_STATIC_LOCAL(AtomicString, gt, ("gt"));
    DEFINE_STATIC_LOCAL(AtomicString, lt, ("lt"));
    DEFINE_STATIC_LOCAL(AtomicString, quot, ("quot"));
    DEFINE_STATIC_LOCAL(String, ampS, ("&"));
    DEFINE_STATIC_LOCAL(String, aposS, ("'"));
    DEFINE_STATIC_LOCAL(String, gtS, (">"));
    DEFINE_STATIC_LOCAL(String, ltS, ("<"));
    DEFINE_STATIC_LOCAL(String, quotS, ("\""));

    if (token.name() == amp)
        appendToText(ampS.characters(), 1);
    else if (token.name() == apos)
        appendToText(aposS.characters(), 1);
    else if (token.name() == gt)
        appendToText(gtS.characters(), 1);
    else if (token.name() == lt)
        appendToText(ltS.characters(), 1);
    else if (token.name() == quot)
        appendToText(quotS.characters(), 1);
    else
        m_parser->stopParsing();
}

void XMLTreeBuilder::processHTMLEntity(const AtomicXMLToken& token)
{
    HTMLEntitySearch search;
    const AtomicString& name = token.name();
    for (size_t i = 0; i < name.length(); ++i) {
        search.advance(name[i]);
        if (!search.isEntityPrefix()) {
            m_parser->stopParsing();
            return;
        }
    }
    search.advance(';');
    if (!search.isEntityPrefix()) {
        m_parser->stopParsing();
        return;
    }
    UChar32 entityValue = search.mostRecentMatch()->firstValue;
    // FIXME: We need to account for secondValue if any XML entities are longer
    // than one unicode character.
    ASSERT_NOT_REACHED();
    // Darin Adler writes:
    //   You can see given the code above that this else is dead code. This code is in a strange state.
    //   And the reinterpret_cast to UChar* makes the code little-endian-specific. That is not good!
    if (entityValue <= 0xFFFF)
        appendToText(reinterpret_cast<UChar*>(&entityValue), 1);
    else {
        UChar utf16Pair[2] = { U16_LEAD(entityValue), U16_TRAIL(entityValue) };
        appendToText(utf16Pair, 2);
    }
}

inline void XMLTreeBuilder::add(PassRefPtr<Node> node)
{
    m_currentNodeStack.last().node()->parserAddChild(node.get());
    if (!node->attached())
        node->attach();
}

void XMLTreeBuilder::appendToText(const UChar* text, size_t length)
{
    enterText();

    if (!m_leafText)
        return;

    m_leafText->append(text, length);
}

void XMLTreeBuilder::enterText()
{
    if (!m_sawFirstElement) {
        // FIXME: Guarantee the text is only whitespace.
        return;
    }

    if (!m_leafText)
        m_leafText = adoptPtr(new StringBuilder());
}

void XMLTreeBuilder::exitText()
{
    if (!m_leafText.get())
        return;

    add(Text::create(m_document, m_leafText->toString()));

    m_leafText.clear();
}

bool XMLTreeBuilder::failOnText()
{
    if (!m_leafText)
        return true;

    // FIXME: Guarantee the text is only whitespace.

    m_leafText.clear();
    return true;
}

XMLTreeBuilder::NodeStackItem::NodeStackItem(PassRefPtr<ContainerNode> n, NodeStackItem* parent)
    : m_node(n)
{
    if (!parent) {
        m_scopedNamespaces.set(xmlAtom, XMLNames::xmlNamespaceURI);
        return;
    }

    m_namespace = parent->m_namespace;
    m_scopedNamespaces = parent->m_scopedNamespaces;
}

bool XMLTreeBuilder::NodeStackItem::hasNamespaceURI(AtomicString prefix)
{
    ASSERT(!prefix.isNull());
    return m_scopedNamespaces.contains(prefix);
}

AtomicString XMLTreeBuilder::NodeStackItem::namespaceURI(AtomicString prefix)
{
    ASSERT(!prefix.isNull());
    if (m_scopedNamespaces.contains(prefix))
        return m_scopedNamespaces.get(prefix);
    return nullAtom;
}

void XMLTreeBuilder::NodeStackItem::setNamespaceURI(AtomicString prefix, AtomicString uri)
{
    m_scopedNamespaces.set(prefix, uri);
}

AtomicString XMLTreeBuilder::NodeStackItem::namespaceForPrefix(AtomicString prefix, AtomicString fallback)
{
    AtomicString uri = fallback;
    if (!prefix.isNull() && hasNamespaceURI(prefix))
        uri = namespaceURI(prefix);

    return uri;
}

}
