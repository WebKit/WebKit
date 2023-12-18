/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingStateNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateFixedNode.h"
#include "ScrollingStateScrollingNode.h"
#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

#include <wtf/text/WTFString.h>

namespace WebCore {

ScrollingStateNode::ScrollingStateNode(ScrollingNodeType nodeType, ScrollingStateTree& scrollingStateTree, ScrollingNodeID nodeID)
    : m_nodeType(nodeType)
    , m_nodeID(nodeID)
    , m_scrollingStateTree(&scrollingStateTree)
{
}

// This copy constructor is used for cloning nodes in the tree, and it doesn't make sense
// to clone the relationship pointers, so don't copy that information from the original node.
ScrollingStateNode::ScrollingStateNode(const ScrollingStateNode& stateNode, ScrollingStateTree& adoptiveTree)
    : m_nodeType(stateNode.nodeType())
    , m_nodeID(stateNode.scrollingNodeID())
    , m_changedProperties(stateNode.changedProperties())
    , m_scrollingStateTree(&adoptiveTree)
{
    if (hasChangedProperty(Property::Layer))
        setLayer(stateNode.layer().toRepresentation(adoptiveTree.preferredLayerRepresentation()));

    scrollingStateTree().addNode(*this);
}

ScrollingStateNode::ScrollingStateNode(ScrollingNodeType nodeType, ScrollingNodeID nodeID, Vector<Ref<ScrollingStateNode>>&& children, OptionSet<ScrollingStateNodeProperty> changedProperties, std::optional<PlatformLayerIdentifier> layerID)
    : m_nodeType(nodeType)
    , m_nodeID(nodeID)
    , m_changedProperties(changedProperties)
    , m_children(WTFMove(children))
    , m_layer(layerID.value_or(PlatformLayerIdentifier { }))
{
    for (auto& child : m_children) {
        ASSERT(!child->parent());
        child->setParent(this);
    }
    ASSERT(parentPointersAreCorrect());
}

ScrollingStateNode::~ScrollingStateNode() = default;

void ScrollingStateNode::attachAfterDeserialization(ScrollingStateTree& tree)
{
    ASSERT(!m_scrollingStateTree);
    m_scrollingStateTree = &tree;

    // Non-deserialization ScrollingStateScrollingNode constructors do this during construction,
    // but since we didn't have a ScrollingStateTree pointer until attaching, do it now.
    if (is<ScrollingStateScrollingNode>(*this))
        tree.scrollingNodeAdded();

    for (auto& child : m_children)
        child->attachAfterDeserialization(tree);
}

void ScrollingStateNode::setChildren(Vector<Ref<ScrollingStateNode>>&& children)
{
    m_children = WTFMove(children);
    for (auto& child : m_children) {
        ASSERT(!child->parent());
        child->setParent(this);
    }
    ASSERT(parentPointersAreCorrect());
}

void ScrollingStateNode::traverse(const Function<void(ScrollingStateNode&)>& function)
{
    function(*this);
    for (auto& child : m_children)
        child->traverse(function);
}

void ScrollingStateNode::setPropertyChanged(Property property)
{
    if (hasChangedProperty(property))
        return;

    setPropertyChangedInternal(property);
    ASSERT(m_scrollingStateTree);
    m_scrollingStateTree->setHasChangedProperties();
}

OptionSet<ScrollingStateNode::Property> ScrollingStateNode::applicableProperties() const
{
    return { Property::Layer, Property::ChildNodes };
}

void ScrollingStateNode::setPropertyChangesAfterReattach()
{
    auto allPropertiesForNodeType = applicableProperties();
    setPropertiesChangedInternal(allPropertiesForNodeType);
    ASSERT(m_scrollingStateTree);
    m_scrollingStateTree->setHasChangedProperties();
}

Ref<ScrollingStateNode> ScrollingStateNode::cloneAndReset(ScrollingStateTree& adoptiveTree)
{
    auto clone = this->clone(adoptiveTree);

    // Now that this node is cloned, reset our change properties.
    resetChangedProperties();

    cloneAndResetChildren(clone.get(), adoptiveTree);

    return clone;
}

void ScrollingStateNode::cloneAndResetChildren(ScrollingStateNode& clone, ScrollingStateTree& adoptiveTree)
{
    for (auto& child : m_children)
        clone.appendChild(child->cloneAndReset(adoptiveTree));
}

void ScrollingStateNode::appendChild(Ref<ScrollingStateNode>&& childNode)
{
    childNode->setParent(this);

    m_children.append(WTFMove(childNode));
    setPropertyChanged(Property::ChildNodes);
}

void ScrollingStateNode::insertChild(Ref<ScrollingStateNode>&& childNode, size_t index)
{
    childNode->setParent(this);

    if (index > m_children.size()) {
        ASSERT_NOT_REACHED();  // Crash data suggest we can get here.
        m_children.append(WTFMove(childNode));
    } else
        m_children.insert(index, WTFMove(childNode));
    
    setPropertyChanged(Property::ChildNodes);
}

void ScrollingStateNode::removeFromParent()
{
    RefPtr parent = m_parent.get();
    if (!parent)
        return;

    parent->removeChild(*this);
    m_parent = nullptr;
}

void ScrollingStateNode::removeChild(ScrollingStateNode& childNode)
{
    auto didRemove = m_children.removeFirstMatching([&](auto& child) {
        return child.ptr() == &childNode;
    });
    if (didRemove)
        setPropertyChanged(Property::ChildNodes);
}

RefPtr<ScrollingStateNode> ScrollingStateNode::childAtIndex(size_t index) const
{
    if (index >= m_children.size())
        return nullptr;
    return m_children[index].copyRef();
}

void ScrollingStateNode::setLayer(const LayerRepresentation& layerRepresentation)
{
    if (layerRepresentation == m_layer)
        return;

    m_layer = layerRepresentation;

    setPropertyChanged(Property::Layer);
}

void ScrollingStateNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeNodeIDs)
        ts.dumpProperty("nodeID", scrollingNodeID());
    
    if (behavior & ScrollingStateTreeAsTextBehavior::IncludeLayerIDs)
        ts.dumpProperty("layerID", layer().layerID());
}

void ScrollingStateNode::dump(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "\n";
    ts << indent << "(";
    ts.increaseIndent();
    dumpProperties(ts, behavior);

    if (!m_children.isEmpty()) {
        ts << "\n";
        ts << indent <<"(";
        {
            TextStream::IndentScope indentScope(ts);
            ts << "children " << children().size();
            for (auto& child : m_children)
                child->dump(ts, behavior);
            ts << "\n";
        }
        ts << indent << ")";
    }
    ts << "\n";
    ts.decreaseIndent();
    ts << indent << ")";
}

String ScrollingStateNode::scrollingStateTreeAsText(OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    TextStream ts(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);

    dump(ts, behavior);
    ts << "\n";
    return ts.release();
}

#if ASSERT_ENABLED
bool ScrollingStateNode::parentPointersAreCorrect() const
{
    for (auto& child : m_children) {
        if (child->parent().get() != this || !child->parentPointersAreCorrect())
            return false;
    }
    return true;
}
#endif

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
