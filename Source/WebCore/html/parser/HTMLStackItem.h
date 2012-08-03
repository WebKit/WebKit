/*
 * Copyright (C) 2012 Company 100, Inc. All rights reserved.
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

#ifndef HTMLStackItem_h
#define HTMLStackItem_h

#include "Element.h"
#include "HTMLNames.h"
#include "HTMLToken.h"

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class ContainerNode;

class HTMLStackItem : public RefCounted<HTMLStackItem> {
public:
    enum ItemType {
        ItemForContextElement,
        ItemForDocumentFragmentNode
    };

    // Used by document fragment node and context element.
    static PassRefPtr<HTMLStackItem> create(PassRefPtr<ContainerNode> node, ItemType type)
    {
        return adoptRef(new HTMLStackItem(node, type));
    }

    // Used by HTMLElementStack and HTMLFormattingElementList.
    static PassRefPtr<HTMLStackItem> create(PassRefPtr<ContainerNode> node, PassRefPtr<AtomicHTMLToken> token, const AtomicString& namespaceURI = HTMLNames::xhtmlNamespaceURI)
    {
        return adoptRef(new HTMLStackItem(node, token, namespaceURI));
    }

    Element* element() const { return toElement(m_node.get()); }
    ContainerNode* node() const { return m_node.get(); }

    bool isDocumentFragmentNode() const { return m_isDocumentFragmentNode; }
    bool isElementNode() const { return !m_isDocumentFragmentNode; }

    AtomicHTMLToken* token() { return m_token.get(); }
    const AtomicString& namespaceURI() const { return m_namespaceURI; }
    const AtomicString& localName() const { return m_token->name(); }

    bool hasLocalName(const AtomicString& name) const { return m_token->name() == name; }
    bool hasTagName(const QualifiedName& name) const { return m_token->name() == name.localName() && m_namespaceURI == name.namespaceURI(); }

    bool causesFosterParenting()
    {
        return hasTagName(HTMLNames::tableTag)
            || hasTagName(HTMLNames::tbodyTag)
            || hasTagName(HTMLNames::tfootTag)
            || hasTagName(HTMLNames::theadTag)
            || hasTagName(HTMLNames::trTag);
    }

private:
    HTMLStackItem(PassRefPtr<ContainerNode> node, ItemType type)
        : m_node(node)
    {
        switch (type) {
        case ItemForDocumentFragmentNode:
            // Create a fake token for a document fragment node. This looks ugly but required for performance
            // because we want to use m_token->name() in localName(), hasLocalName() and hasTagName() without
            // checking m_isDocumentFragmentNode flag.
            m_token = AtomicHTMLToken::create(HTMLTokenTypes::StartTag, nullAtom);
            m_isDocumentFragmentNode = true;
            break;
        case ItemForContextElement:
            // Create a fake token for a context element for the same reason as above.
            m_token = AtomicHTMLToken::create(HTMLTokenTypes::StartTag, m_node->localName());
            m_namespaceURI = m_node->namespaceURI();
            m_isDocumentFragmentNode = false;
            break;
        }
    }

    HTMLStackItem(PassRefPtr<ContainerNode> node, PassRefPtr<AtomicHTMLToken> token, const AtomicString& namespaceURI = HTMLNames::xhtmlNamespaceURI)
        : m_node(node)
        , m_token(token)
        , m_namespaceURI(namespaceURI)
        , m_isDocumentFragmentNode(false)
    {
    }

    RefPtr<ContainerNode> m_node;

    RefPtr<AtomicHTMLToken> m_token;
    AtomicString m_namespaceURI;
    bool m_isDocumentFragmentNode;
};

} // namespace WebCore

#endif // HTMLStackItem_h
