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

#include <WebCore/CharacterData.h>
#include <WebCore/Document.h>
#include <WebCore/Element.h>
#include <WebCore/InspectorInstrumentationPublic.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/Node.h>
#include <WebCore/PseudoElement.h>
#include <WebCore/RenderBox.h>
#include <WebCore/ShadowRoot.h>
#include <WebCore/TreeScopeInlines.h>
#include <WebCore/WebCoreOpaqueRoot.h>

namespace WebCore {

inline bool Node::isUserAgentShadowRoot() const
{
    auto* shadowRoot = dynamicDowncast<ShadowRoot>(*this);
    return shadowRoot && shadowRoot->mode() == ShadowRootMode::UserAgent;
}

inline ContainerNode* Node::parentOrShadowHostNode() const
{
    ASSERT(isMainThreadOrGCThread());
    if (auto* shadowRoot = dynamicDowncast<ShadowRoot>(*this))
        return shadowRoot->host();
    return parentNode();
}

inline WebCoreOpaqueRoot Node::opaqueRoot() const
{
    if (isConnected()) {
        Locker locker { TreeScope::treeScopeMutationLock() };
        return WebCoreOpaqueRoot { &treeScope().documentScope() };
    }
    // FIXME: Possible race?
    return traverseToOpaqueRoot();
}

inline RenderBox* Node::renderBox() const
{
    return dynamicDowncast<RenderBox>(renderer());
}

inline CheckedPtr<RenderBox> Node::checkedRenderBox() const
{
    return renderBox();
}

inline RenderBoxModelObject* Node::renderBoxModelObject() const
{
    return dynamicDowncast<RenderBoxModelObject>(renderer());
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

CheckedPtr<RenderObject> Node::checkedRenderer() const
{
    return renderer();
}

inline void Node::setRenderer(RenderObject* renderer)
{
    m_renderer = renderer;

    if (InspectorInstrumentationPublic::hasFrontends()) [[unlikely]]
        notifyInspectorOfRendererChange();
}

inline Element* Node::parentElement() const
{
    return dynamicDowncast<Element>(parentNode());
}

bool Node::isBeforePseudoElement() const
{
    auto* pseudoElement = dynamicDowncast<PseudoElement>(*this);
    return pseudoElement && pseudoElement->pseudoElementType() == PseudoElementType::Before;
}

bool Node::isAfterPseudoElement() const
{
    auto* pseudoElement = dynamicDowncast<PseudoElement>(*this);
    return pseudoElement && pseudoElement->pseudoElementType() == PseudoElementType::After;
}

std::optional<PseudoElementType> Node::pseudoElementType() const
{
    if (auto* pseudoElement = dynamicDowncast<PseudoElement>(*this))
        return pseudoElement->pseudoElementType();
    return { };
}

std::optional<Style::PseudoElementIdentifier> Node::pseudoElementIdentifier() const
{
    if (auto type = pseudoElementType())
        return Style::PseudoElementIdentifier { *type };
    return { };
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

inline Node* Node::lastChild() const
{
    auto* containerNode = dynamicDowncast<ContainerNode>(*this);
    return containerNode ? containerNode->lastChild() : nullptr;
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

ALWAYS_INLINE bool Node::hasOneRef() const
{
    ASSERT(!deletionHasBegun());
    ASSERT(!m_inRemovedLastRefFunction);
    return refCount() == 1;
}

ALWAYS_INLINE void Node::clearStyleFlags(OptionSet<NodeStyleFlag> flags)
{
    auto bitfields = styleBitfields();
    bitfields.clearFlags(flags);
    setStyleBitfields(bitfields);
}

inline void Node::clearChildNeedsStyleRecalc()
{
    auto bitfields = styleBitfields();
    bitfields.clearDescendantsNeedStyleResolution();
    setStyleBitfields(bitfields);
}

inline void Node::setHasValidStyle()
{
    auto bitfields = styleBitfields();
    bitfields.setStyleValidity(Style::Validity::Valid);
    setStyleBitfields(bitfields);
    clearStateFlag(StateFlag::IsComputedStyleInvalidFlag);
    clearStateFlag(StateFlag::HasInvalidRenderer);
    clearStateFlag(StateFlag::StyleResolutionShouldRecompositeLayer);
}

inline void Node::setTreeScopeRecursively(TreeScope& newTreeScope)
{
    ASSERT(!isDocumentNode());
    ASSERT(!deletionHasBegun());
    if (m_treeScope != &newTreeScope) {
        Ref oldTreeScope = *m_treeScope;
        moveTreeToNewScope(*this, oldTreeScope, newTreeScope);
    }
}

inline ContainerNode* Node::parentNodeGuaranteedHostFree() const
{
    ASSERT(!isShadowRoot());
    return parentNode();
}

inline void Node::setTreeScope(TreeScope& scope)
{
    Locker locker { TreeScope::treeScopeMutationLock() };
    m_treeScope = &scope;
}

template<typename NodeClass>
inline NodeClass& Node::traverseToRootNodeInternal(const NodeClass& node)
{
    auto* current = const_cast<NodeClass*>(&node);
    while (current->parentNode())
        current = current->parentNode();
    return *current;
}

inline void Node::relaxAdoptionRequirement()
{
#if ASSERT_ENABLED
    ASSERT_WITH_SECURITY_IMPLICATION(!deletionHasBegun());
    ASSERT(m_adoptionIsRequired);
    m_adoptionIsRequired = false;
#endif
}

inline IntRect Node::pixelSnappedAbsoluteBoundingRect(bool* isReplaced)
{
    return snappedIntRect(absoluteBoundingRect(isReplaced));
}

// Used in Node::addSubresourceAttributeURLs() and in addSubresourceStyleURLs()
inline void addSubresourceURL(ListHashSet<URL>& urls, const URL& url)
{
    if (!url.isNull())
        urls.add(url);
}

inline void collectChildNodes(Node& node, NodeVector& children)
{
    for (SUPPRESS_UNCOUNTED_LOCAL Node* child = node.firstChild(); child; child = child->nextSibling())
        children.append(*child);
}

} // namespace WebCore
