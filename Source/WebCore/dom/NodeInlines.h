/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include "CharacterData.h"
#include "Document.h"
#include "Element.h"
#include "Node.h"
#include "WebCoreOpaqueRoot.h"

namespace WebCore {

inline RefPtr<ScriptExecutionContext> Node::protectedScriptExecutionContext() const
{
    return scriptExecutionContext();
}

inline WebCoreOpaqueRoot Node::opaqueRoot() const
{
    // FIXME: Possible race?
    // https://bugs.webkit.org/show_bug.cgi?id=165713
    if (isConnected())
        return WebCoreOpaqueRoot { &document() };
    return traverseToOpaqueRoot();
}

inline Ref<Document> Node::protectedDocument() const
{
    return document();
}

inline bool Node::hasAttributes() const
{
    auto* element = dynamicDowncast<Element>(*this);
    return element && element->hasAttributes();
}

inline NamedNodeMap* Node::attributesMap() const
{
    if (auto* element = dynamicDowncast<Element>(*this))
        return &element->attributesMap();
    return nullptr;
}

inline Element* Node::parentElement() const
{
    return dynamicDowncast<Element>(parentNode());
}

inline RefPtr<Element> Node::protectedParentElement() const
{
    return parentElement();
}

inline void Node::setTabIndexState(TabIndexState state)
{
    auto bitfields = rareDataBitfields();
    bitfields.tabIndexState = enumToUnderlyingType(state);
    setRareDataBitfields(bitfields);
}

inline unsigned Node::length() const
{
    if (auto characterData = dynamicDowncast<CharacterData>(*this))
        return characterData->length();
    return countChildNodes();
}

inline unsigned Node::countChildNodes() const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->countChildNodes() : 0;
}

inline Node* Node::traverseToChildAt(unsigned index) const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->traverseToChildAt(index) : nullptr;
}

inline Node* Node::firstChild() const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->firstChild() : nullptr;
}

inline RefPtr<Node> Node::protectedFirstChild() const
{
    return firstChild();
}

inline Node* Node::lastChild() const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->lastChild() : nullptr;
}

inline RefPtr<Node> Node::protectedLastChild() const
{
    return lastChild();
}

inline bool Node::hasChildNodes() const
{
    return firstChild();
}

inline Node& Node::rootNode() const
{
    if (isInTreeScope())
        return treeScope().rootNode();
    return traverseToRootNode();
}

inline void Node::setParentNode(ContainerNode* parent)
{
    ASSERT(isMainThread());
    m_parentNode = parent;
    m_refCountAndParentBit = (m_refCountAndParentBit & s_refCountMask) | !!parent;
}

inline RefPtr<ContainerNode> Node::protectedParentNode() const
{
    return parentNode();
}

inline void collectChildNodes(Node& node, NodeVector& children)
{
    for (SUPPRESS_UNCOUNTED_LOCAL Node* child = node.firstChild(); child; child = child->nextSibling())
        children.append(*child);
}

} // namespace WebCore
