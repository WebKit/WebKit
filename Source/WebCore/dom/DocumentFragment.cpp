/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "DocumentFragment.h"

#include "Document.h"
#include "ElementIterator.h"
#include "HTMLDocumentParser.h"
#include "Page.h"
#include "XMLDocumentParser.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DocumentFragment);

DocumentFragment::DocumentFragment(Document& document, ConstructionType constructionType)
    : ContainerNode(document, constructionType)
{
}

Ref<DocumentFragment> DocumentFragment::create(Document& document)
{
    return adoptRef(*new DocumentFragment(document, Node::CreateDocumentFragment));
}

String DocumentFragment::nodeName() const
{
    return "#document-fragment"_s;
}

Node::NodeType DocumentFragment::nodeType() const
{
    return DOCUMENT_FRAGMENT_NODE;
}

bool DocumentFragment::childTypeAllowed(NodeType type) const
{
    switch (type) {
        case ELEMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        case COMMENT_NODE:
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
            return true;
        default:
            return false;
    }
}

Ref<Node> DocumentFragment::cloneNodeInternal(Document& targetDocument, CloningOperation type)
{
    Ref<DocumentFragment> clone = create(targetDocument);
    switch (type) {
    case CloningOperation::OnlySelf:
    case CloningOperation::SelfWithTemplateContent:
        break;
    case CloningOperation::Everything:
        cloneChildNodes(clone);
        break;
    }
    return clone;
}

void DocumentFragment::parseHTML(const String& source, Element* contextElement, ParserContentPolicy parserContentPolicy)
{
    ASSERT(contextElement);
    HTMLDocumentParser::parseDocumentFragment(source, *this, *contextElement, parserContentPolicy);
}

bool DocumentFragment::parseXML(const String& source, Element* contextElement, ParserContentPolicy parserContentPolicy)
{
    return XMLDocumentParser::parseDocumentFragment(source, *this, contextElement, parserContentPolicy);
}

Element* DocumentFragment::getElementById(const AtomString& id) const
{
    if (id.isEmpty())
        return nullptr;

    // Fast path for ShadowRoot, where we are both a DocumentFragment and a TreeScope.
    if (isTreeScope())
        return treeScope().getElementById(id);

    // Otherwise, fall back to iterating all of the element descendants.
    for (auto& element : descendantsOfType<Element>(*this)) {
        if (element.getIdAttribute() == id)
            return const_cast<Element*>(&element);
    }

    return nullptr;
}

}
