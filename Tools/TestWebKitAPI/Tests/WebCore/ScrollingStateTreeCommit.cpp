/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(ASYNC_SCROLLING)

#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <WebCore/ScrollingNodeID.h>
#include <WebCore/ScrollingStateFixedNode.h>
#include <WebCore/ScrollingStateFrameScrollingNode.h>
#include <WebCore/ScrollingStateOverflowScrollProxyNode.h>
#include <WebCore/ScrollingStateOverflowScrollingNode.h>
#include <WebCore/ScrollingStatePositionedNode.h>
#include <WebCore/ScrollingStateScrollingNode.h>
#include <WebCore/ScrollingStateStickyNode.h>
#include <WebCore/ScrollingStateTree.h>
#include <wtf/Assertions.h>
#include <wtf/MonotonicTime.h>

namespace TestWebKitAPI {

using namespace WebCore;

namespace {

// Builds a tree with a MainFrame root and the requested number of overflow children
// directly under the root, returns the tree along with the IDs in insertion order
// (root first).
struct BuiltTree {
    std::unique_ptr<ScrollingStateTree> tree;
    Vector<ScrollingNodeID> nodeIDs;
};

BuiltTree buildTreeWithOverflowChildren(size_t childCount)
{
    auto tree = makeUnique<ScrollingStateTree>();
    Vector<ScrollingNodeID> ids;

    auto rootID = ScrollingNodeID::generate();
    auto rootInserted = tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    EXPECT_TRUE(rootInserted.has_value());
    ids.append(rootID);

    for (size_t i = 0; i < childCount; ++i) {
        auto childID = ScrollingNodeID::generate();
        auto childInserted = tree->insertNode(ScrollingNodeType::Overflow, childID, rootID, i);
        EXPECT_TRUE(childInserted.has_value());
        ids.append(childID);
    }

    return { WTF::move(tree), WTF::move(ids) };
}

} // namespace

TEST(ScrollingStateTreeCommit, FirstCommitMarksAllNodesDirty)
{
    auto built = buildTreeWithOverflowChildren(2);
    auto& tree = *built.tree;

    auto commitResult = tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    EXPECT_TRUE(commitResult);

    auto& dirty = tree.dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(3u, dirty.size());
}

TEST(ScrollingStateTreeCommit, IdleCommitProducesNoDirtyNodes)
{
    auto built = buildTreeWithOverflowChildren(2);
    auto& tree = *built.tree;

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    // Second commit with no mutations between.
    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree.dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(0u, dirty.size());
}

TEST(ScrollingStateTreeCommit, FirstCommitReturnsNonEmptyTransaction)
{
    auto built = buildTreeWithOverflowChildren(2);
    auto& tree = *built.tree;

    auto firstCommit = tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, firstCommit.get());
    EXPECT_TRUE(firstCommit->hasChangedProperties());
    EXPECT_TRUE(firstCommit->rootStateNode().get());
}

TEST(ScrollingStateTreeCommit, IdleCommitReturnsEmptyTransaction)
{
    auto built = buildTreeWithOverflowChildren(2);
    auto& tree = *built.tree;

    auto firstCommit = tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, firstCommit.get());

    // Confirm empty-transaction short-circuit returns an empty tree with no changed properties.
    auto secondCommit = tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, secondCommit.get());
    EXPECT_FALSE(secondCommit->hasChangedProperties());
    EXPECT_FALSE(secondCommit->rootStateNode().get());
}

TEST(ScrollingStateTreeCommit, MutationProducesNonEmptyTransaction)
{
    auto built = buildTreeWithOverflowChildren(2);
    auto& tree = *built.tree;

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto targetID = built.nodeIDs[1];
    RefPtr targetNode = tree.stateNodeForID(targetID);
    auto* scrollingNode = dynamicDowncast<ScrollingStateScrollingNode>(targetNode.get());
    ASSERT_NE(nullptr, scrollingNode);
    scrollingNode->setScrollPosition(FloatPoint(10, 20));

    auto secondCommit = tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, secondCommit.get());
    EXPECT_TRUE(secondCommit->hasChangedProperties());
    EXPECT_TRUE(secondCommit->rootStateNode().get());
}

TEST(ScrollingStateTreeCommit, RepeatedNoOpCommitsAllShortCircuit)
{
    auto built = buildTreeWithOverflowChildren(3);
    auto& tree = *built.tree;

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    // After the first commit, every subsequent idle commit must short-circuit and return an
    // empty (but non-null) tree.
    for (unsigned i = 0; i < 5; ++i) {
        auto idleCommit = tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
        ASSERT_NE(nullptr, idleCommit.get());
        EXPECT_FALSE(idleCommit->hasChangedProperties());
        EXPECT_FALSE(idleCommit->rootStateNode().get());
    }
}

TEST(ScrollingStateTreeCommit, ScrollPositionMutationMarksOnlyMutatedNodeDirty)
{
    auto built = buildTreeWithOverflowChildren(2);
    auto& tree = *built.tree;

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    // Mutate the scroll position on the second overflow child only.
    auto targetID = built.nodeIDs[2];
    RefPtr targetNode = tree.stateNodeForID(targetID);
    ASSERT_NE(nullptr, targetNode.get());
    auto* scrollingNode = dynamicDowncast<ScrollingStateScrollingNode>(targetNode.get());
    ASSERT_NE(nullptr, scrollingNode);
    scrollingNode->setScrollPosition(FloatPoint(10, 20));

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree.dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(1u, dirty.size());
    if (dirty.size() == 1u)
        EXPECT_TRUE(targetID == dirty[0]);
}

TEST(ScrollingStateTreeCommit, StaticLayoutGroupMutationMarksOnlyMutatedNodeDirty)
{
    auto built = buildTreeWithOverflowChildren(2);
    auto& tree = *built.tree;

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    // Mutate scrollableAreaSize (StaticLayout group) on the first overflow child.
    auto targetID = built.nodeIDs[1];
    RefPtr targetNode = tree.stateNodeForID(targetID);
    ASSERT_NE(nullptr, targetNode.get());
    auto* scrollingNode = dynamicDowncast<ScrollingStateScrollingNode>(targetNode.get());
    ASSERT_NE(nullptr, scrollingNode);
    scrollingNode->setScrollableAreaSize(FloatSize(800, 600));

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree.dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(1u, dirty.size());
    if (dirty.size() == 1u)
        EXPECT_TRUE(targetID == dirty[0]);
}

TEST(ScrollingStateTreeCommit, RepeatedScrollPositionWritesWithSameValueAreNoOp)
{
    auto built = buildTreeWithOverflowChildren(1);
    auto& tree = *built.tree;

    auto targetID = built.nodeIDs[1];
    RefPtr targetNode = tree.stateNodeForID(targetID);
    auto* scrollingNode = dynamicDowncast<ScrollingStateScrollingNode>(targetNode.get());
    ASSERT_NE(nullptr, scrollingNode);
    scrollingNode->setScrollPosition(FloatPoint(50, 75));

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    // Re-set to the same value; setter's equality short-circuit should make this a no-op.
    scrollingNode->setScrollPosition(FloatPoint(50, 75));

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree.dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(0u, dirty.size());
}

TEST(ScrollingStateTreeCommit, NewlyAddedChildAppearsInDirtyList)
{
    auto built = buildTreeWithOverflowChildren(1);
    auto& tree = *built.tree;
    auto rootID = built.nodeIDs[0];

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto newChildID = ScrollingNodeID::generate();
    auto inserted = tree.insertNode(ScrollingNodeType::Overflow, newChildID, rootID, notFound);
    ASSERT_TRUE(inserted.has_value());

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree.dirtyNodeIDsFromLastCommit();
    // The newly-added node has no committed counterpart and must appear dirty.
    EXPECT_TRUE(dirty.contains(newChildID));
    // Note: the parent's m_children vector changed, but children-list mutations live outside
    // the CoW partition, so hasUnchangedGroupsAs does not flag the parent here. This is a
    // documented gap for Patch 4; Patch 5 will combine this signal with Property::ChildNodes.
}

TEST(ScrollingStateTreeCommit, GroupSharingAcrossUnchangedNodes)
{
    auto built = buildTreeWithOverflowChildren(3);
    auto& tree = *built.tree;

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    // Mutate one child only; the other two children's StaticLayout/StaticConfig groups
    // should still be Ref-shared with the committed snapshot, so they remain clean.
    auto mutatedID = built.nodeIDs[2];
    RefPtr mutatedNode = tree.stateNodeForID(mutatedID);
    auto* scrollingNode = dynamicDowncast<ScrollingStateScrollingNode>(mutatedNode.get());
    ASSERT_NE(nullptr, scrollingNode);
    scrollingNode->setTotalContentsSize(FloatSize(2000, 4000));

    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree.dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(1u, dirty.size());
    if (dirty.size() == 1u)
        EXPECT_TRUE(mutatedID == dirty[0]);
}

TEST(ScrollingStateTreeCommit, FixedNodeConstraintsMutationMarksDirty)
{
    auto tree = makeUnique<ScrollingStateTree>();
    auto rootID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    auto fixedID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Fixed, fixedID, rootID, 0);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    RefPtr fixedNodeBase = tree->stateNodeForID(fixedID);
    auto* fixedNode = dynamicDowncast<ScrollingStateFixedNode>(fixedNodeBase.get());
    ASSERT_NE(nullptr, fixedNode);
    FixedPositionViewportConstraints newConstraints;
    newConstraints.setViewportRectAtLastLayout(FloatRect(0, 0, 800, 600));
    fixedNode->updateConstraints(newConstraints);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree->dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(1u, dirty.size());
    if (dirty.size() == 1u)
        EXPECT_TRUE(fixedID == dirty[0]);
}

TEST(ScrollingStateTreeCommit, PositionedNodeConstraintsMutationMarksDirty)
{
    auto tree = makeUnique<ScrollingStateTree>();
    auto rootID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    auto positionedID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Positioned, positionedID, rootID, 0);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    RefPtr positionedNodeBase = tree->stateNodeForID(positionedID);
    auto* positionedNode = dynamicDowncast<ScrollingStatePositionedNode>(positionedNodeBase.get());
    ASSERT_NE(nullptr, positionedNode);
    AbsolutePositionConstraints newConstraints;
    newConstraints.setLayerPositionAtLastLayout(FloatPoint(100, 200));
    positionedNode->updateConstraints(newConstraints);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree->dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(1u, dirty.size());
    if (dirty.size() == 1u)
        EXPECT_TRUE(positionedID == dirty[0]);
}

TEST(ScrollingStateTreeCommit, StickyNodeConstraintsMutationMarksDirty)
{
    auto tree = makeUnique<ScrollingStateTree>();
    auto rootID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    auto stickyID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Sticky, stickyID, rootID, 0);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    RefPtr stickyNodeBase = tree->stateNodeForID(stickyID);
    auto* stickyNode = dynamicDowncast<ScrollingStateStickyNode>(stickyNodeBase.get());
    ASSERT_NE(nullptr, stickyNode);
    StickyPositionViewportConstraints newConstraints;
    newConstraints.setTopOffset(50);
    stickyNode->updateConstraints(newConstraints);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree->dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(1u, dirty.size());
    if (dirty.size() == 1u)
        EXPECT_TRUE(stickyID == dirty[0]);
}

TEST(ScrollingStateTreeCommit, StickyNodeViewportAnchorLayerMutationMarksDirty)
{
    auto tree = makeUnique<ScrollingStateTree>();
    auto rootID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    auto stickyID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Sticky, stickyID, rootID, 0);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    RefPtr stickyNodeBase = tree->stateNodeForID(stickyID);
    auto* stickyNode = dynamicDowncast<ScrollingStateStickyNode>(stickyNodeBase.get());
    ASSERT_NE(nullptr, stickyNode);

    // Mutate the viewport-anchor layer with a real (non-empty) identifier so the setter is a
    // genuine change, exercising the dirty-detection wiring on the anchor-layer path this test names.
    stickyNode->setViewportAnchorLayer(LayerRepresentation { PlatformLayerIdentifier::generate() });

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    auto& dirty = tree->dirtyNodeIDsFromLastCommit();
    EXPECT_EQ(1u, dirty.size());
    if (dirty.size() == 1u)
        EXPECT_TRUE(stickyID == dirty[0]);
}

TEST(ScrollingStateTreeCommit, ReparentingChildProducesNonEmptyTransaction)
{
    auto tree = makeUnique<ScrollingStateTree>();
    auto rootID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    auto firstParentID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Overflow, firstParentID, rootID, 0);
    auto secondParentID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Overflow, secondParentID, rootID, 1);
    auto childID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Overflow, childID, firstParentID, 0);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    // Reparent child from firstParent to secondParent. The reparent path goes through
    // unparentNode -> insertNode reattachment; Property::ChildNodes must fire on both parents
    // so m_hasChangedProperties is set on the live tree, defeating the short-circuit.
    tree->insertNode(ScrollingNodeType::Overflow, childID, secondParentID, 0);

    auto secondCommit = tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, secondCommit.get());
    EXPECT_TRUE(secondCommit->hasChangedProperties());
    EXPECT_TRUE(secondCommit->rootStateNode().get());
}

TEST(ScrollingStateTreeCommit, NonPartitionedSubclassesCommitAndShortCircuit)
{
    // Tree containing a non-partitioned subclass (OverflowScrollProxyNode). Its
    // hasUnchangedGroupsAs falls through to the base class default; this test asserts the
    // tree-level commit & short-circuit gates still work correctly via m_hasChangedProperties.
    auto tree = makeUnique<ScrollingStateTree>();
    auto rootID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    auto overflowID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Overflow, overflowID, rootID, 0);
    auto proxyID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::OverflowProxy, proxyID, rootID, 1);

    // Wire the OverflowScrollProxy to its target Overflow node — ScrollingStateTree::isValid()
    // (asserted at the top of commit()) rejects proxy nodes that don't reference a real
    // overflow node in the tree's node map.
    RefPtr proxyBase = tree->stateNodeForID(proxyID);
    auto* proxyNode = dynamicDowncast<ScrollingStateOverflowScrollProxyNode>(proxyBase.get());
    ASSERT_NE(nullptr, proxyNode);
    proxyNode->setOverflowScrollingNode(overflowID);

    auto firstCommit = tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, firstCommit.get());
    EXPECT_TRUE(firstCommit->hasChangedProperties());
    EXPECT_TRUE(firstCommit->rootStateNode().get());

    // Idle: short-circuit must still fire even with a non-partitioned subclass in the tree.
    auto idleCommit = tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, idleCommit.get());
    EXPECT_FALSE(idleCommit->hasChangedProperties());
    EXPECT_FALSE(idleCommit->rootStateNode().get());
}

TEST(ScrollingStateTreeCommit, RequestedScrollDataMergesAcrossSetCallsBetweenCommits)
{
    // RequestedScrollData is intentionally exempt from CoW partitioning (design doc §3.1).
    // mergeOrAppendScrollRequest has stateful merge semantics: two DeltaUpdate calls between
    // commits accumulate their FloatSize deltas. This test verifies both calls arrive at the
    // committed IPC clone via the live tree's m_requestedScrollData.
    auto tree = makeUnique<ScrollingStateTree>();
    auto rootID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    auto overflowID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Overflow, overflowID, rootID, 0);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    RefPtr overflowNodeBase = tree->stateNodeForID(overflowID);
    auto* overflowNode = dynamicDowncast<ScrollingStateScrollingNode>(overflowNodeBase.get());
    ASSERT_NE(nullptr, overflowNode);

    RequestedScrollData firstRequest;
    firstRequest.requestType = ScrollRequestType::DeltaUpdate;
    firstRequest.scrollPositionOrDelta = FloatSize { 10.f, 0.f };
    overflowNode->setRequestedScrollData(RequestedScrollData { firstRequest });

    RequestedScrollData secondRequest;
    secondRequest.requestType = ScrollRequestType::DeltaUpdate;
    secondRequest.scrollPositionOrDelta = FloatSize { 0.f, 5.f };
    overflowNode->setRequestedScrollData(RequestedScrollData { secondRequest });

    // Both calls must be visible on the IPC clone — the second merged into the first via
    // accumulateDelta, producing a single DeltaUpdate of (10, 5).
    auto commit = tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, commit.get());
    EXPECT_TRUE(commit->hasChangedProperties());

    RefPtr clonedOverflow = commit->stateNodeForID(overflowID);
    auto* clonedScrolling = dynamicDowncast<ScrollingStateScrollingNode>(clonedOverflow.get());
    ASSERT_NE(nullptr, clonedScrolling);

    auto& mergedData = clonedScrolling->requestedScrollData();
    ASSERT_EQ(1u, mergedData.size());
    EXPECT_EQ(ScrollRequestType::DeltaUpdate, mergedData[0].requestType);
    auto mergedDelta = std::get<FloatSize>(mergedData[0].scrollPositionOrDelta);
    EXPECT_FLOAT_EQ(10.f, mergedDelta.width());
    EXPECT_FLOAT_EQ(5.f, mergedDelta.height());
}

TEST(ScrollingStateTreeCommit, EmptyTreeRoundTripsThroughDeserializationPath)
{
    // The short-circuit returns a non-null but otherwise empty ScrollingStateTree
    // (m_rootStateNode == nullptr, hasChangedProperties() == false, hasNewRootStateNode() ==
    // false). The receiver's deserialization path calls createAfterReconstruction +
    // attachDeserializedNodes on the wire-decoded fields. This test asserts those calls are
    // safe on the empty-tree shape we produce — guards the same surface a CVE-class null-deref
    // was found on earlier in this patch.
    auto tree = buildTreeWithOverflowChildren(2);
    tree.tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);
    auto idleCommit = tree.tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, idleCommit.get());
    EXPECT_FALSE(idleCommit->hasChangedProperties());
    EXPECT_FALSE(idleCommit->hasNewRootStateNode());
    EXPECT_EQ(nullptr, idleCommit->rootStateNode().get());

    // Mirror the receiver's reconstruction: pass the producer's three observable fields back
    // into createAfterReconstruction, then run attachDeserializedNodes — that's what
    // RemoteScrollingCoordinatorTransaction does on the UI process side.
    auto reconstructed = ScrollingStateTree::createAfterReconstruction(
        idleCommit->hasNewRootStateNode(),
        idleCommit->hasChangedProperties(),
        nullptr);
    ASSERT_TRUE(reconstructed.has_value());
    EXPECT_FALSE(reconstructed->hasChangedProperties());
    EXPECT_FALSE(reconstructed->hasNewRootStateNode());
    EXPECT_EQ(nullptr, reconstructed->rootStateNode().get());

    reconstructed->attachDeserializedNodes();
    EXPECT_FALSE(reconstructed->hasChangedProperties());
    EXPECT_EQ(nullptr, reconstructed->rootStateNode().get());
}

TEST(ScrollingStateTreeCommit, ScrollbarFadeAndHoverLoopShortCircuits)
{
    // Motivating Spotify case: scrollbar hover/fade animations mutate ContentAreaHoverState,
    // MouseActivityState, and ScrollbarHoverState in tight loops. With per-setter equality
    // short-circuits in place, a re-set of the same value MUST NOT flip m_hasChangedProperties
    // and the next commit MUST short-circuit. Guards against future code paths that mark
    // dirty without value-checking.
    auto built = buildTreeWithOverflowChildren(1);
    auto& tree = *built.tree;
    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    RefPtr overflowNodeBase = tree.stateNodeForID(built.nodeIDs[1]);
    auto* overflowNode = dynamicDowncast<ScrollingStateScrollingNode>(overflowNodeBase.get());
    ASSERT_NE(nullptr, overflowNode);

    auto commitIsIdle = [&]() {
        auto result = tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
        return result && !result->hasChangedProperties();
    };

    // Transition: mouse enters content area. Expected non-idle.
    overflowNode->setMouseIsOverContentArea(true);
    EXPECT_FALSE(commitIsIdle());

    // Re-set same value. Expected idle (equality short-circuit in setter).
    overflowNode->setMouseIsOverContentArea(true);
    EXPECT_TRUE(commitIsIdle());

    // Mouse motion: new location. Expected non-idle.
    MouseLocationState locationA;
    locationA.locationInHorizontalScrollbar = IntPoint { 10, 20 };
    overflowNode->setMouseMovedInContentArea(locationA);
    EXPECT_FALSE(commitIsIdle());

    // Same location. Expected idle.
    overflowNode->setMouseMovedInContentArea(locationA);
    EXPECT_TRUE(commitIsIdle());

    // Hover state change. Expected non-idle.
    ScrollbarHoverState hoverA;
#if USE(COORDINATED_GRAPHICS_ASYNC_SCROLLBAR)
    hoverA.hoveredPartInVerticalScrollbar = ThumbPart;
#else
    hoverA.mouseIsOverVerticalScrollbar = true;
#endif
    overflowNode->setScrollbarHoverState(hoverA);
    EXPECT_FALSE(commitIsIdle());

    // Same hover state. Expected idle.
    overflowNode->setScrollbarHoverState(hoverA);
    EXPECT_TRUE(commitIsIdle());

    // Tight loop of identity re-sets across all three fields. Every commit must short-circuit.
    for (unsigned i = 0; i < 16; ++i) {
        overflowNode->setMouseIsOverContentArea(true);
        overflowNode->setMouseMovedInContentArea(locationA);
        overflowNode->setScrollbarHoverState(hoverA);
        EXPECT_TRUE(commitIsIdle());
    }
}

TEST(ScrollingStateTreeCommit, TinyPODDirectMembersPropagateThroughCommit)
{
    // Regression test: 12 fields were moved out of StaticConfiguration{Scroll,Frame}State
    // CoW groups into direct members of their respective node classes (to avoid forcing a
    // whole-group copy on a single-byte mutation). These direct members are NOT auto-
    // propagated by the DataRef share in the copy constructor — each must be explicitly
    // copied. A missing copy silently sends default values to the UI process.
    //
    // This test catches that class of regression for both partitioned subclasses by
    // mutating each direct member and asserting the IPC clone carries the value.
    auto tree = makeUnique<ScrollingStateTree>();
    auto rootID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::MainFrame, rootID, std::nullopt, 0);
    auto overflowID = ScrollingNodeID::generate();
    tree->insertNode(ScrollingNodeType::Overflow, overflowID, rootID, 0);

    tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);

    RefPtr rootBase = tree->stateNodeForID(rootID);
    auto* frameNode = dynamicDowncast<ScrollingStateFrameScrollingNode>(rootBase.get());
    ASSERT_NE(nullptr, frameNode);

    RefPtr overflowBase = tree->stateNodeForID(overflowID);
    auto* scrollNode = dynamicDowncast<ScrollingStateScrollingNode>(overflowBase.get());
    ASSERT_NE(nullptr, scrollNode);

    // Scrolling-node direct members.
    scrollNode->setUseDarkAppearanceForScrollbars(true);
    scrollNode->setScrollbarWidth(ScrollbarWidth::Thin);
    scrollNode->setScrollbarLayoutDirection(UserInterfaceLayoutDirection::RTL);
    scrollNode->setScrollbarEnabledState(ScrollbarOrientation::Vertical, true);

    // Frame-node direct members.
    frameNode->setHeaderHeight(42);
    frameNode->setFooterHeight(7);
    frameNode->setScrollBehaviorForFixedElements(ScrollBehaviorForFixedElements::StickToViewportBounds);
    frameNode->setVisualViewportIsSmallerThanLayoutViewport(true);
    frameNode->setAsyncFrameOrOverflowScrollingEnabled(true);
    frameNode->setWheelEventGesturesBecomeNonBlocking(true);
    frameNode->setScrollingPerformanceTestingEnabled(true);
    frameNode->setOverlayScrollbarsEnabled(true);

    auto commit = tree->commit(LayerRepresentation::PlatformLayerIDRepresentation);
    ASSERT_NE(nullptr, commit.get());
    EXPECT_TRUE(commit->hasChangedProperties());

    RefPtr clonedScrollBase = commit->stateNodeForID(overflowID);
    auto* clonedScroll = dynamicDowncast<ScrollingStateScrollingNode>(clonedScrollBase.get());
    ASSERT_NE(nullptr, clonedScroll);
    EXPECT_TRUE(clonedScroll->useDarkAppearanceForScrollbars());
    EXPECT_EQ(ScrollbarWidth::Thin, clonedScroll->scrollbarWidth());
    EXPECT_EQ(UserInterfaceLayoutDirection::RTL, clonedScroll->scrollbarLayoutDirection());
    EXPECT_TRUE(clonedScroll->scrollbarEnabledState().verticalScrollbarIsEnabled);

    RefPtr clonedFrameBase = commit->stateNodeForID(rootID);
    auto* clonedFrame = dynamicDowncast<ScrollingStateFrameScrollingNode>(clonedFrameBase.get());
    ASSERT_NE(nullptr, clonedFrame);
    EXPECT_EQ(42, clonedFrame->headerHeight());
    EXPECT_EQ(7, clonedFrame->footerHeight());
    EXPECT_EQ(ScrollBehaviorForFixedElements::StickToViewportBounds, clonedFrame->scrollBehaviorForFixedElements());
    EXPECT_TRUE(clonedFrame->visualViewportIsSmallerThanLayoutViewport());
    EXPECT_TRUE(clonedFrame->asyncFrameOrOverflowScrollingEnabled());
    EXPECT_TRUE(clonedFrame->wheelEventGesturesBecomeNonBlocking());
    EXPECT_TRUE(clonedFrame->scrollingPerformanceTestingEnabled());
    EXPECT_TRUE(clonedFrame->overlayScrollbarsEnabled());
}


#ifndef LOG_DISABLED
// Benchmark — exercises commit() repeatedly on a Spotify-shaped tree (30 overflow children)
// and reports per-commit cost for three scenarios:
//   1. Idle commits: Patch 5's empty-transaction short-circuit fires every time.
//   2. Forced slow path: we set m_hasChangedProperties to defeat the short-circuit, exercising
//      the full clone path without any actual mutations (simulates pre-Patch-5 cost).
//   3. Real scroll mutation: per-frame setScrollPosition + commit, the realistic non-idle case.
// Output goes to the Scrolling log channel (enable it to see the numbers). Sanity assertions are loose so the test passes
// across machines, but a large regression in the short-circuit path will trip them.
TEST(DISABLED_ScrollingStateTreeCommit, BenchmarkCommitCost)
{
    constexpr unsigned childCount = 30;
    constexpr unsigned iterations = 1000;

    auto built = buildTreeWithOverflowChildren(childCount);
    auto& tree = *built.tree;

    // First commit establishes m_lastCommittedTree; subsequent idle commits can short-circuit.
    tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);

    // Scenario 1: idle commits — short-circuit fires.
    auto idleStart = MonotonicTime::now();
    for (unsigned i = 0; i < iterations; ++i)
        tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    auto idleSeconds = MonotonicTime::now() - idleStart;

    // Scenario 2: forced slow path — same tree, no actual mutation, but we defeat the
    // short-circuit by setting m_hasChangedProperties. Approximates pre-Patch-5 commit cost.
    auto forcedSlowStart = MonotonicTime::now();
    for (unsigned i = 0; i < iterations; ++i) {
        tree.setHasChangedProperties(true);
        tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    }
    auto forcedSlowSeconds = MonotonicTime::now() - forcedSlowStart;

    // Scenario 3: realistic per-frame mutation (a scroll position update).
    RefPtr rootNode = tree.stateNodeForID(built.nodeIDs[0]);
    auto* rootScrolling = dynamicDowncast<ScrollingStateScrollingNode>(rootNode.get());
    ASSERT_NE(nullptr, rootScrolling);
    auto mutationStart = MonotonicTime::now();
    for (unsigned i = 0; i < iterations; ++i) {
        rootScrolling->setScrollPosition(FloatPoint(static_cast<float>(i), 0));
        tree.commit(LayerRepresentation::PlatformLayerIDRepresentation);
    }
    auto mutationSeconds = MonotonicTime::now() - mutationStart;

    double idleUsPerCommit = idleSeconds.microseconds() / static_cast<double>(iterations);
    double forcedSlowUsPerCommit = forcedSlowSeconds.microseconds() / static_cast<double>(iterations);
    double mutationUsPerCommit = mutationSeconds.microseconds() / static_cast<double>(iterations);
    double speedupVsForcedSlow = forcedSlowSeconds.microseconds() / std::max(1.0, idleSeconds.microseconds());
    double speedupVsMutation = mutationSeconds.microseconds() / std::max(1.0, idleSeconds.microseconds());

    LOG(Scrolling, "[ScrollingStateTreeCommit Bench] %u children, %u iterations:", childCount, iterations);
    LOG(Scrolling, "  Idle (short-circuit):       %8.2f us/commit", idleUsPerCommit);
    LOG(Scrolling, "  Forced slow path:           %8.2f us/commit", forcedSlowUsPerCommit);
    LOG(Scrolling, "  Real scroll mutation:       %8.2f us/commit", mutationUsPerCommit);
    LOG(Scrolling, "  Speedup vs forced slow:     %8.2fx", speedupVsForcedSlow);
    LOG(Scrolling, "  Speedup vs mutation:        %8.2fx", speedupVsMutation);

    // Loose sanity assertions: the short-circuit path must be measurably faster than the
    // paths that perform the full clone.
    EXPECT_LT(idleSeconds, forcedSlowSeconds);
    EXPECT_LT(idleSeconds, mutationSeconds);
}
#endif

} // namespace TestWebKitAPI

#endif // ENABLE(ASYNC_SCROLLING)
