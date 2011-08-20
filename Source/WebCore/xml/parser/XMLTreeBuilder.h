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

#ifndef XMLTreeBuilder_h
#define XMLTreeBuilder_h

#include "Text.h"
#include "XMLToken.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class ContainerNode;
class Document;
class NewXMLDocumentParser;

class XMLTreeBuilder {
    WTF_MAKE_NONCOPYABLE(XMLTreeBuilder);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<XMLTreeBuilder> create(NewXMLDocumentParser* parser, Document* document)
    {
        return adoptPtr(new XMLTreeBuilder(parser, document));
    }

    static PassOwnPtr<XMLTreeBuilder> create(NewXMLDocumentParser* parser, DocumentFragment* document, Element* parent)
    {
        return adoptPtr(new XMLTreeBuilder(parser, document, parent));
    }

    void processToken(const AtomicXMLToken&);
    void finish();

private:
    XMLTreeBuilder(NewXMLDocumentParser*, Document*);
    XMLTreeBuilder(NewXMLDocumentParser*, DocumentFragment*, Element* parent);

    class NodeStackItem {
    public:
        NodeStackItem(PassRefPtr<ContainerNode> item, NodeStackItem* parent = 0);

        bool hasNamespaceURI(AtomicString prefix);
        AtomicString namespaceURI(AtomicString prefix);
        AtomicString namespaceURI() { return m_namespace; }
        void setNamespaceURI(AtomicString prefix, AtomicString uri);
        void setNamespaceURI(AtomicString uri) { m_namespace = uri; }
        AtomicString namespaceForPrefix(AtomicString prefix, AtomicString fallback);

        PassRefPtr<ContainerNode> node() { return m_node; }
        const ContainerNode* node() const { return m_node.get(); }
        void setNode(PassRefPtr<ContainerNode> node) { m_node = node; }

    private:
        HashMap<AtomicString, AtomicString> m_scopedNamespaces;
        RefPtr<ContainerNode> m_node;
        AtomicString m_namespace;
    };

    void pushCurrentNode(const NodeStackItem&);
    void popCurrentNode();
    void closeElement(PassRefPtr<Element>);

    void processProcessingInstruction(const AtomicXMLToken&);
    void processXMLDeclaration(const AtomicXMLToken&);
    void processDOCTYPE(const AtomicXMLToken&);
    void processStartTag(const AtomicXMLToken&);
    void processEndTag(const AtomicXMLToken&);
    void processCDATA(const AtomicXMLToken&);
    void processCharacter(const AtomicXMLToken&);
    void processComment(const AtomicXMLToken&);
    void processEntity(const AtomicXMLToken&);

    void processNamespaces(const AtomicXMLToken&, NodeStackItem&);
    void processAttributes(const AtomicXMLToken&, NodeStackItem&, PassRefPtr<Element> newElement);
    void processXMLEntity(const AtomicXMLToken&);
    void processHTMLEntity(const AtomicXMLToken&);

    inline void add(PassRefPtr<Node>);

    void appendToText(const UChar* characters, size_t length);
    void enterText();
    void exitText();
    bool failOnText();

    AtomicString namespaceForPrefix(AtomicString prefix, AtomicString fallback);

    Document* m_document;
    NewXMLDocumentParser* m_parser;

    bool m_isXHTML;

    bool m_sawFirstElement;
    Vector<NodeStackItem> m_currentNodeStack;

    OwnPtr<StringBuilder> m_leafText;
};

}

#endif // XMLTreeBuilder_h
