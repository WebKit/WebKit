/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>
#include <wtf/FastMalloc.h>
#include <wtf/MathExtras.h>
#include <wtf/Range.h>
#include <wtf/Vector.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

// IntervalSet: Stores a set of Range<T> to Value. Optimized with the following assumptions:
// - hasOverlap() is the most frequent operation.
// - find() is the next most frequent operation.
// - insert() is much less frequent.
// - erase() is the least frequent operation.
//
// Implemented as a cache-line-aware B+ tree specialized for storing Range<T> keys.

template<typename T, typename Value, size_t cacheLinesPerNode = 1>
    requires std::is_trivially_destructible_v<T> && std::is_trivially_destructible_v<Value>
class IntervalSet {
public:
    using Interval = Range<T>;

    static constexpr size_t cpuCacheLineSize = 64;
    static constexpr size_t targetNodeSize = cacheLinesPerNode * cpuCacheLineSize;

    // Calculate optimal order for each node type based on target cache line usage
    static constexpr size_t calculateLeafOrder()
    {
        constexpr size_t sizePerOrder = sizeof(Interval) + sizeof(Value);
        return targetNodeSize / sizePerOrder;
    }

    static constexpr size_t calculateInnerOrder()
    {
        constexpr size_t sizePerOrder = sizeof(Interval) + sizeof(uintptr_t);
        return targetNodeSize / sizePerOrder;
    }

    static constexpr size_t leafOrder = calculateLeafOrder();
    static constexpr size_t innerOrder = calculateInnerOrder();

    // Ensure cacheLinesPerNode parameter is large enough for valid B+ tree orders
    static_assert(leafOrder >= 2, "cacheLinesPerNode parameter too small: LeafNode order must be at least 2 for a valid B+ tree");
    static_assert(innerOrder >= 2, "cacheLinesPerNode parameter too small: InnerNode order must be at least 2 for a valid B+ tree");

    IntervalSet() = default;

    ~IntervalSet()
    {
        freeAllNodes();
        ASSERT(!assertOnlyNumNodes);
    }

    bool isEmpty() const { return !m_rootInterval; }

    // Insert an interval-value pair. The interval must not overlap with an existing interval.
    // Invalidates all iterators.
    void insert(const Interval& interval, const Value& value)
    {
        if (!m_root) [[unlikely]] {
            LeafNode* leaf = allocNode<LeafNode>();
            m_root = NodeRef(leaf, 0);
            m_height = 0;
        }

        Path path;
        NodeRef* nodeRef = &m_root;

        // Descend down the tree, recording the path taken.
        for (unsigned depth = 0; depth < m_height; depth++) {
            InnerNode* inner = nodeRef->asInner();
            size_t index = inner->subtreeForInsert(nodeRef->size(), interval.end());
            path.append({ nodeRef, index });
            nodeRef = &inner->child(index);
        }
        // Found the correct leaf for the insert, now determine the index within that leaf.
        size_t insertionIndex = nodeRef->asLeaf()->firstIntervalEndAfter(nodeRef->size(), interval.end());
        path.append({ nodeRef, insertionIndex });
        ASSERT(path.size() == m_height + 1);

        auto [newNode, newNodeCoverage] = insertInNodeSplitIfNeeded<LeafNode>(path, m_height, interval, value);

        // Ascend back up along the same path, inserting any new children and splitting inner nodes as needed.
        for (int depth = m_height - 1; depth >= 0; depth--) {
            if (!newNode) [[likely]]
                return;
            PathEntry& entry = path[depth];

            ASSERT(entry.nodeRef->asInner()->child(entry.index).size() + newNode.size() == (static_cast<unsigned>(depth + 1) == m_height ? leafOrder : innerOrder) + 1);
            ASSERT(newNodeCoverage);
            entry.index++; // Insert new parent immediately after the existing parent
            std::tie(newNode, newNodeCoverage) = insertInNodeSplitIfNeeded<InnerNode>(path, depth, newNodeCoverage, newNode);
        }

        // If there's a new node at depth 0 then a new level is required.
        if (newNode) [[unlikely]] {
            ASSERT(m_root.size() + newNode.size() == (m_height ? innerOrder : leafOrder) + 1);
            // Need to add another level to the tree.
            InnerNode* newRoot = allocNode<InnerNode>();
            newRoot->interval(0) = m_rootInterval;
            newRoot->child(0) = m_root;
            newRoot->interval(1) = newNodeCoverage;
            newRoot->child(1) = newNode;
            m_height++;
            m_root = NodeRef(newRoot, 2);
            m_rootInterval = newRoot->coverage(2);
        }
    }

    // Remove the given interval from the IntervalSet. The interval must be present.
    // Invalidates all iterators.
    void erase(const Interval& interval)
    {
        Path path;
        ASSERT(interval.overlaps(m_rootInterval));
        ASSERT(m_root);
        NodeRef* nodeRef = &m_root;

        for (unsigned depth = 0; depth < m_height; ++depth) {
            InnerNode* inner = nodeRef->asInner();
            size_t index = inner->firstIntervalEndAfter(nodeRef->size(), interval.begin());
            ASSERT(index < nodeRef->size());
            ASSERT(inner->interval(index).begin() < interval.end());
            path.append({ nodeRef, index });
            nodeRef = &inner->child(index);
        }
        LeafNode* leaf = nodeRef->asLeaf();
        size_t eraseIndex = leaf->firstIntervalEndAfter(nodeRef->size(), interval.begin());
        ASSERT(leaf->interval(eraseIndex).begin() == interval.begin() && leaf->interval(eraseIndex).end() == interval.end());
        path.append({ nodeRef, eraseIndex });

        bool removedNode = eraseFromNode<LeafNode>(path, m_height);

        // Ascend removing references to any child that was removed, which may in turn cause the parent to become empty.
        for (int depth = m_height - 1; depth >= 0; depth--) {
            if (!removedNode) [[likely]]
                return;
            removedNode = eraseFromNode<InnerNode>(path, depth);
        }

        // If removedNode was true at every depth, the tree is now empty.
        if (removedNode) [[unlikely]] {
            ASSERT(!assertOnlyNumNodes);
            ASSERT(!m_root);
            m_rootInterval = Interval();
            m_height = 0;
        }
    }

    // Returns the Interval and Value for the first interval that overlaps with the query interval,
    // if an overlapping interval exists. Otherwise, returns std::nullopt.
    std::optional<std::pair<Interval, Value>> find(const Interval& query) const
    {
        if (!query.overlaps(m_rootInterval))
            return std::nullopt;

        ASSERT(m_root);
        NodeRef nodeRef = m_root;
        for (unsigned depth = 0; depth < m_height; ++depth) {
            InnerNode* inner = nodeRef.asInner();
            size_t index = inner->firstIntervalEndAfter(nodeRef.size(), query.begin());
            if (index == nodeRef.size())
                return std::nullopt; // query is entirely after this subtree
            if (query.end() <= inner->interval(index).begin())
                return std::nullopt; // query is entirely before this subtree
            nodeRef = inner->child(index);
        }
        LeafNode* leaf = nodeRef.asLeaf();
        size_t index = leaf->firstIntervalEndAfter(nodeRef.size(), query.begin());
        ASSERT(index < nodeRef.size()); // coverage check at parent level ensures this
        ASSERT(query.begin() < leaf->interval(index).end());
        if (query.end() <= leaf->interval(index).begin())
            return std::nullopt;
        return std::make_pair(leaf->interval(index), leaf->value(index));
    }

    // Returns true iff an interval in the set overlaps with the query interval. Similar to find() but
    // can sometimes terminate before descending the full depth since the Interval-Value result is not needed.
    bool hasOverlap(const Interval& query) const
    {
        if (!query.overlaps(m_rootInterval))
            return false;

        ASSERT(m_root);
        NodeRef nodeRef = m_root;
        for (unsigned depth = 0; depth < m_height; ++depth) {
            InnerNode* inner = nodeRef.asInner();
            size_t index = inner->firstIntervalEndAfter(nodeRef.size(), query.begin());
            if (index == nodeRef.size())
                return false; // query starts after all intervals
            // query start lands either within the subtree or the gap immediately preceding that subtree
            ASSERT(query.begin() < inner->interval(index).end());
            if (query.end() <= inner->interval(index).begin())
                return false; // query is entirely in the gap before this subtree
            if (inner->interval(index).end() <= query.end())
                return true; // query spans subtree end point so it must overlap the last interval
            if (query.begin() <= inner->interval(index).begin())
                return true; // query spans subtree start point so it must overlap the first interval
            // Otherwise, subtree encompasses query so need to search subtree
            ASSERT(inner->interval(index).begin() < query.begin() && query.end() < inner->interval(index).end());
            nodeRef = inner->child(index);
        }

        LeafNode* leaf = nodeRef.asLeaf();
        size_t index = leaf->firstIntervalEndAfter(nodeRef.size(), query.begin());
        ASSERT(query.begin() < leaf->interval(index).end());
        return leaf->interval(index).begin() < query.end();
    }

    void dump(PrintStream& out) const
    {
        out.print("IntervalSet(height=", m_height, ", leafOrder=", leafOrder, ", innerOrder=", innerOrder, ")");
        if (!m_root) {
            out.print(" <empty>");
            return;
        }
        out.println(" coverage=", m_rootInterval);
        dumpSubtree(out, m_root, m_height, 0);
    }

    // Height indicates the number of edges to reach the leaf level in a non-empty tree.
    unsigned height() const { return m_height; }

private:
    struct LeafNode;
    struct InnerNode;

    // Common base class for all nodes - provides type identity for NodeRef
    struct Node { };

    template<typename Payload, size_t order>
    struct NodeImpl : public Node {
        using PayloadType = Payload;
        static constexpr size_t capacity = order;

        Interval& interval(size_t index)
        {
            ASSERT(index < capacity);
            return intervals[index];
        }

        const Interval coverage(size_t size) const
        {
            RELEASE_ASSERT(size);
            return { intervals[0].begin(), intervals[size - 1].end() };
        }

        // Transfer count intervals and values from the rightNode to this node, where the rightNode
        // is the immediate right cousin of this.
        void shiftLeftFrom(size_t& size, NodeImpl* rightNode, size_t& rightSize, size_t count)
        {
            ASSERT(size + count <= capacity);
            ASSERT(count <= rightSize);
            for (size_t i = 0; i < count; i++) {
                intervals[i + size] = rightNode->intervals[i];
                payloads[i + size] = rightNode->payloads[i];
            }
            for (size_t i = 0; i < rightSize - count; i++) {
                rightNode->intervals[i] = rightNode->intervals[i + count];
                rightNode->payloads[i] = rightNode->payloads[i + count];
            }
            size += count;
            rightSize -= count;
        }

        // Transfer count intervals and values from this node to the rightNode, where the rightNode
        // is the immediate right cousin of this.
        void shiftRightTo(size_t& size, NodeImpl* rightNode, size_t& rightSize, size_t count)
        {
            ASSERT(rightSize + count <= capacity);
            ASSERT(count <= size);
            for (size_t i = rightSize + count - 1; i >= count; i--) {
                rightNode->intervals[i] = rightNode->intervals[i - count];
                rightNode->payloads[i] = rightNode->payloads[i - count];
            }
            for (size_t i = 0; i < count; i++) {
                rightNode->intervals[i] = intervals[size - count + i];
                rightNode->payloads[i] = payloads[size - count + i];
            }
            size -= count;
            rightSize += count;
        }

        void insertAt(size_t& size, size_t index, const Interval& interval, const Payload& value)
        {
            ASSERT(size < capacity);
            ASSERT(index <= size);
            for (size_t i = size; i > index; --i) {
                intervals[i] = intervals[i - 1];
                payloads[i] = payloads[i - 1];
            }
            intervals[index] = interval;
            payloads[index] = value;
            size++;
        }

        void removeAt(size_t& size, size_t index)
        {
            ASSERT(size <= capacity);
            ASSERT(index < size);
            for (size_t i = index; i < size - 1; ++i) {
                intervals[i] = intervals[i + 1];
                payloads[i] = payloads[i + 1];
            }
            size--;
        }

        // Find the least interval with end greater than the given point, and return the index, if exists.
        // Otherwise, returns size if no such interval exists.
        size_t firstIntervalEndAfter(size_t size, T point) const
        {
            ASSERT(size <= capacity);
            for (size_t i = 0; i < size; i++) {
                if (point < intervals[i].end())
                    return i;
            }
            return size;
        }

        // Intervals and payloads are stored separately for better cache access patterns in the case
        // that cacheLinesPerNode > 1.
        std::array<Interval, order> intervals;
        std::array<Payload, order> payloads; // Either the NodeRefs to children (InnerNode) or the values (LeafNode)
    };

    // NodeRef is used to hold links from parent to children. The NodeRef contains both the pointer to the
    // child node (which may be either another InnerNode or a LeafNode) and the number of elements stored
    // in that pointed to node. This is more space and cache efficient than storing the size in each node
    // because it uses less storage and the size of a child node can be determined without accessing the
    // child node's cacheline.
    class NodeRef {
    public:
        static_assert(isPowerOfTwo(cpuCacheLineSize));

        static constexpr uintptr_t sizeMask = cpuCacheLineSize - 1;
        static_assert(leafOrder <= sizeMask && innerOrder <= sizeMask);

        NodeRef()
            : m_bits(0) { }

        NodeRef(Node* ptr, size_t size)
            : m_bits(reinterpret_cast<uintptr_t>(ptr) | size)
        {
            ASSERT(!(reinterpret_cast<uintptr_t>(ptr) & sizeMask));
            ASSERT(size <= sizeMask);
        }

        Node* node() const
        {
            return reinterpret_cast<Node*>(m_bits & ~sizeMask);
        }

        size_t size() const
        {
            return m_bits & sizeMask;
        }

        void setSize(size_t newSize)
        {
            ASSERT(newSize <= sizeMask);
            m_bits = (m_bits & ~sizeMask) | newSize;
        }

        explicit operator bool() const { return m_bits; }

        template<typename NodeType> requires std::is_base_of_v<Node, NodeType>
        NodeType* as() const
        {
            return static_cast<NodeType*>(node());
        }

        LeafNode* asLeaf() const
        {
            return as<LeafNode>();
        }

        InnerNode* asInner() const
        {
            return as<InnerNode>();
        }

    private:
        uintptr_t m_bits;
    };

    // LeafNodes are always at depth of m_height.
    struct LeafNode : public NodeImpl<Value, leafOrder> {
        Value& value(size_t index)
        {
            ASSERT(index < leafOrder);
            return this->payloads[index];
        }
    };

    // InnerNode are at all depths != m_height.
    struct InnerNode : public NodeImpl<NodeRef, innerOrder> {
        NodeRef& child(size_t index)
        {
            ASSERT(index < innerOrder);
            return this->payloads[index];
        }

        size_t subtreeForInsert(size_t size, T endPoint) const
        {
            ASSERT(size);
            ASSERT(size <= innerOrder);
            for (size_t i = 0; i < size - 1; i++) {
                if (endPoint <= this->intervals[i + 1].begin())
                    return i;
            }
            return size - 1;
        }
    };

private:
    struct PathEntry {
        NodeRef* nodeRef; // Indirection allows insert/erase to perform tree modifications
        size_t index;

        bool operator==(const PathEntry& other) const
        {
            return nodeRef->node() == other.nodeRef->node() && index == other.index;
        }
    };

    // Path specifies which NodeRef and index were traversed at each depth to reach a particular payload within the tree.
    class Path : public Vector<PathEntry, 8> {
        using Base = Vector<PathEntry, 8>;

    public:
        Path() = default;

        Path(const Path& from, unsigned depth)
            : Base(from)
        {
            ASSERT(this->size() > depth);
            this->shrink(depth + 1);
        }

        // Advances to the next index of the leaf node, if exists. If the current leaf node is exhausted,
        // advances to the leaf node to the immediate right and set index to 0.
        void nextIndexInLeaf()
        {
            ASSERT(this->size());
            PathEntry& leafEntry = this->last();
            if (++leafEntry.index < leafEntry.nodeRef->size()) [[likely]]
                return;
            // Move on to the next leaf node, if exists.
            toRightCousin();
            ASSERT(!this->size() || !this->last().index);
        }

        // Cousin means node at the same depth (includes siblings, aka 0th cousin). The immediate
        // left and right cousins may be in different subtrees, i.e. not necessarily siblings.
        void toLeftCousin() { toCousin<TraverseLeft>(); }
        void toRightCousin() { toCousin<TraverseRight>(); }

    private:
        struct TraverseLeft {
            static bool hasMoreChildren(const PathEntry& entry)
            {
                // If index != 0, then we can traverse left at this level.
                return !!entry.index;
            }

            static size_t nextSubtreeIndex(const PathEntry& entry)
            {
                ASSERT(entry.index);
                // Left sibling is in the previous subtree.
                return entry.index - 1;
            }

            static size_t descendIndex(const NodeRef nodeRef)
            {
                ASSERT(nodeRef.size());
                // Descend down the right-most branches.
                return nodeRef.size() - 1;
            }
        };

        struct TraverseRight {
            static bool hasMoreChildren(const PathEntry& entry)
            {
                // If index != size() - 1, then we can traverse right at this level.
                return entry.index < entry.nodeRef->size() - 1;
            }

            static size_t nextSubtreeIndex(const PathEntry& entry)
            {
                ASSERT(entry.index < entry.nodeRef->size() - 1);
                // Right sibling is in the next subtree.
                return entry.index + 1;
            }

            static size_t descendIndex(const NodeRef nodeRef)
            {
                ASSERT_UNUSED(nodeRef, nodeRef.size());
                // Descend down the left-most branches.
                return 0;
            }
        };

        // Modifies the path so that it becomes the path to the immediate left or right cousin.
        template<typename Traverser>
        void toCousin()
        {
            int initialDepth = this->size() - 1;
            if (!initialDepth) {
                this->clear(); // Root has no cousins
                return;
            }
            // Ascend up until we find a node with indicies to the left.
            int depth = initialDepth - 1;
            for (; depth >= 0; depth--) {
                PathEntry& innerEntry = this->at(depth);
                if (Traverser::hasMoreChildren(innerEntry))
                    break;
            }
            if (depth < 0) {
                // Exhausted all indicies of the root node.
                this->clear();
                return;
            }
            // Descend down the right-most edge of the left subtree.
            PathEntry& innerEntry = this->at(depth);
            innerEntry.index = Traverser::nextSubtreeIndex(innerEntry);
            depth++;
            NodeRef* childRef = &innerEntry.nodeRef->asInner()->child(innerEntry.index);
            for (; depth < initialDepth; depth++) {
                ASSERT(childRef->size());
                size_t childIndex = Traverser::descendIndex(*childRef);
                this->at(depth).nodeRef = childRef;
                this->at(depth).index = childIndex;
                childRef = &childRef->asInner()->child(childIndex);
            }
            ASSERT(childRef->size());
            this->at(depth).nodeRef = childRef;
            this->at(depth).index = Traverser::descendIndex(*childRef);
        }
    };

public:
    class iterator {
    public:
        iterator() = default;

        iterator(Path&& path)
            : m_path(WTF::move(path)) { }

        const Interval& interval() const
        {
            auto [leaf, index] = leafAndIndex();
            return leaf->interval(index);
        }

        const Value& value() const
        {
            auto [leaf, index] = leafAndIndex();
            return leaf->value(index);
        }

        const std::pair<Interval, Value> operator*() const
        {
            return { interval(), value() };
        }

        iterator& operator++()
        {
            m_path.nextIndexInLeaf();
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return m_path == other.m_path;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

    private:
        const std::pair<LeafNode*, unsigned> leafAndIndex() const
        {
            const PathEntry& entry = m_path.last();
            return { entry.nodeRef->asLeaf(), entry.index };
        }

        Path m_path;
    };

    // Returns an iterator with the path to the left-most leaf node and index 0
    iterator begin() const
    {
        if (isEmpty())
            return end();
        Path path;
        NodeRef* nodeRef = const_cast<NodeRef*>(&m_root);
        // Generate path to the left-most leaf node.
        for (unsigned depth = 0; depth < m_height; depth++) {
            ASSERT(nodeRef->size());
            path.append({ nodeRef, 0 });
            nodeRef = &nodeRef->asInner()->child(0);
        }
        // Leaf node
        ASSERT(nodeRef->size());
        path.append({ nodeRef, 0 });
        ASSERT(path.size() == m_height + 1);
        return iterator(WTF::move(path));
    }

    iterator end() const
    {
        return iterator();
    }

private:
    bool isFirstOrLastIndex(NodeRef nodeRef, size_t index)
    {
        ASSERT(index < nodeRef.size());
        return !index || index == nodeRef.size() - 1;
    }

    // After an interval within a node, give by path and depth, is modified, propagate the new interval
    // information upwards, as necessary, in order to keep inner nodes' "coverage" intervals consistent.
    void updateCoverage(const Path& path, int depth, Interval coverage)
    {
        ASSERT(depth >= 0);
        depth--; // So that depth is at the parent of the node with 'coverage'.
        while (depth >= 0) {
            const PathEntry& entry = path[depth];
            InnerNode* inner = entry.nodeRef->asInner();
            inner->interval(entry.index) = coverage;

            if (!isFirstOrLastIndex(*entry.nodeRef, entry.index)) {
                // Since first/last of this node was not modified, its coverage hasn't changed - no need to continue upward.
                verifyCoverageConsistency(path, depth, inner->coverage(entry.nodeRef->size()));
                return;
            }
            coverage = inner->coverage(entry.nodeRef->size());
            depth--;
        }
        m_rootInterval = coverage;
    }

    void verifyCoverageConsistency(const Path& path, int depth, Interval coverage)
    {
#ifdef ASSERT_ENABLED
        ASSERT(depth >= 0);
        depth--;
        while (depth >= 0) {
            const PathEntry& entry = path[depth];
            InnerNode* inner = entry.nodeRef->asInner();
            ASSERT(inner->interval(entry.index) == coverage);
            coverage = inner->coverage(entry.nodeRef->size());
            depth--;
        }
        if (m_rootInterval != coverage)
            dataLogLn("FAIL: m_rootInterval=", m_rootInterval, " coverage=", coverage, " Tree=", *this);
        ASSERT(m_rootInterval == coverage);
#endif
    }

    // Inserts interval and payload into the node referred to by path at the given depth. Updates affected NodePtr
    // sizes and coverages for the affected subtree. If the node needed to be split then returns the NodePtr and
    // coverage interval for the new node so that the caller can insert the new node into the parent.
    template<typename NodeType>
    std::pair<NodeRef, Interval> insertInNodeSplitIfNeeded(const Path& path, int depth, const Interval& interval, const typename NodeType::PayloadType& payload)
    {
        NodeRef* nodeRef = path[depth].nodeRef;
        size_t nodeSize = nodeRef->size();
        ASSERT(nodeSize <= NodeType::capacity);

        if (nodeSize < NodeType::capacity) [[likely]] {
            auto insertionIndex = path[depth].index;
            auto node = nodeRef->template as<NodeType>();
            node->insertAt(nodeSize, insertionIndex, interval, payload);
            nodeRef->setSize(nodeSize);
            if (isFirstOrLastIndex(*nodeRef, insertionIndex))
                updateCoverage(path, depth, node->coverage(nodeSize));
            return { NodeRef(), Interval() };
        }
        if (tryRedistributeLeftAndInsert<NodeType>(path, depth, interval, payload))
            return { NodeRef(), Interval() };
        if (tryRedistributeRightAndInsert<NodeType>(path, depth, interval, payload))
            return { NodeRef(), Interval() };
        return splitNodeAndInsert<NodeType>(path, depth, interval, payload);
    }

    template<typename NodeType>
    bool tryRedistributeLeftAndInsert(const Path& path, int depth, const Interval& interval, const typename NodeType::PayloadType& payload)
    {
        NodeRef* nodeRef = path[depth].nodeRef;
        auto insertionIndex = path[depth].index;
        auto node = nodeRef->template as<NodeType>();
        size_t nodeSize = nodeRef->size();

        Path leftPath(path, depth);
        leftPath.toLeftCousin();
        if (leftPath.isEmpty())
            return false;

        // Note that since interval begin is used as the boundary between nodes and intervals are not allowed to
        // overlap, insertionIndex will never be 0 if there is a left node -- the left node would have been chosen instead.
        // Therefore if there is only one empty slot, the empty slot can be put into the right node without danger of
        // shifting the insertionIndex into the left node.
        ASSERT(0 < insertionIndex && insertionIndex <= nodeSize);

        NodeRef* leftNodeRef = leftPath[depth].nodeRef;
        size_t leftNodeSize = leftNodeRef->size();
        if (leftNodeSize == NodeType::capacity)
            return false;

        auto leftNode = leftNodeRef->template as<NodeType>();
        size_t newSize = (leftNodeSize + nodeSize) / 2;
        ASSERT(newSize < NodeType::capacity);
        size_t numToMove = nodeSize - newSize;
        leftNode->shiftLeftFrom(leftNodeSize, node, nodeSize, numToMove);
        ASSERT(nodeSize == newSize);
        if (insertionIndex < numToMove)
            leftNode->insertAt(leftNodeSize, leftNodeSize + insertionIndex - numToMove, interval, payload);
        else
            node->insertAt(nodeSize, insertionIndex - numToMove, interval, payload);
        leftNodeRef->setSize(leftNodeSize);
        updateCoverage(leftPath, depth, leftNode->coverage(leftNodeSize));
        nodeRef->setSize(nodeSize);
        updateCoverage(path, depth, node->coverage(nodeSize));
        return true;
    }

    template<typename NodeType>
    bool tryRedistributeRightAndInsert(const Path& path, int depth, const Interval& interval, const typename NodeType::PayloadType& payload)
    {
        NodeRef* nodeRef = path[depth].nodeRef;
        auto insertionIndex = path[depth].index;
        auto node = nodeRef->template as<NodeType>();
        size_t nodeSize = nodeRef->size();

        Path rightPath(path, depth);
        rightPath.toRightCousin();
        if (rightPath.isEmpty())
            return false;

        NodeRef* rightNodeRef = rightPath[depth].nodeRef;
        size_t rightNodeSize = rightNodeRef->size();
        if (rightNodeSize == NodeType::capacity)
            return false;

        auto rightNode = rightNodeRef->template as<NodeType>();
        // If the insertion index is after all items of the left node and we only have one empty slot
        // we need to insert into the head of the right node.
        if (insertionIndex == NodeType::capacity) {
            rightNode->insertAt(rightNodeSize, 0, interval, payload);
            rightNodeRef->setSize(rightNodeSize);
            updateCoverage(rightPath, depth, rightNode->coverage(rightNodeSize));
            return true;
        }
        // Now, we know that insertionINdex < capacity, so if there's only one empty slot between both nodes,
        // we should put it in the left node and the insertion point will still always be in the left node.
        size_t newSize = (rightNodeSize + nodeSize) / 2;
        ASSERT(newSize < NodeType::capacity);
        size_t numToMove = nodeSize - newSize;
        node->shiftRightTo(nodeSize, rightNode, rightNodeSize, numToMove);
        ASSERT(nodeSize == newSize);
        if (insertionIndex <= nodeSize)
            node->insertAt(nodeSize, insertionIndex, interval, payload);
        else
            rightNode->insertAt(rightNodeSize, insertionIndex - nodeSize, interval, payload);
        nodeRef->setSize(nodeSize);
        updateCoverage(path, depth, node->coverage(nodeSize));
        rightNodeRef->setSize(rightNodeSize);
        updateCoverage(rightPath, depth, rightNode->coverage(rightNodeSize));
        return true;
    }

    template<typename NodeType>
    std::pair<NodeRef, Interval> splitNodeAndInsert(const Path& path, int depth, const Interval& interval, const typename NodeType::PayloadType& payload)
    {
        NodeRef* nodeRef = path[depth].nodeRef;
        auto insertionIndex = path[depth].index;
        auto node = nodeRef->template as<NodeType>();
        size_t nodeSize = nodeRef->size();
        constexpr size_t splitPoint = (NodeType::capacity + 1) / 2;
        auto newNode = allocNode<NodeType>();

        ASSERT(nodeSize == NodeType::capacity);

        for (size_t i = splitPoint; i < nodeSize; ++i) {
            newNode->intervals[i - splitPoint] = node->intervals[i];
            newNode->payloads[i - splitPoint] = node->payloads[i];
        }
        size_t newNodeSize = nodeSize - splitPoint;
        nodeSize = splitPoint;

        if (insertionIndex <= nodeSize)
            node->insertAt(nodeSize, insertionIndex, interval, payload);
        else
            newNode->insertAt(newNodeSize, insertionIndex - nodeSize, interval, payload);
        nodeRef->setSize(nodeSize);
        updateCoverage(path, depth, node->coverage(nodeSize));
        return { NodeRef(newNode, newNodeSize), newNode->coverage(newNodeSize) };
    }

    template<typename NodeType>
    bool eraseFromNode(const Path& path, int depth)
    {
        NodeRef* nodeRef = path[depth].nodeRef;
        auto eraseIndex = path[depth].index;
        auto node = nodeRef->template as<NodeType>();
        size_t nodeSize = nodeRef->size();
        ASSERT(nodeSize <= NodeType::capacity);

        if (nodeSize == 1) [[unlikely]] {
            ASSERT(!eraseIndex);
            freeNode(node);
            *nodeRef = NodeRef();
            return true;
        }
        node->removeAt(nodeSize, eraseIndex);
        if (isFirstOrLastIndex(*nodeRef, eraseIndex))
            updateCoverage(path, depth, node->coverage(nodeSize));
        nodeRef->setSize(nodeSize);
        return false;
    }

    template<typename NodeType>
    NodeType* allocNode()
    {
        ASSERT(++assertOnlyNumNodes);
        static_assert(sizeof(NodeType) <= targetNodeSize);
        return static_cast<NodeType*>(fastAlignedMalloc(cpuCacheLineSize, targetNodeSize));
    }

    template<typename NodeType>
    void freeNode(NodeType* node)
    {
        ASSERT(assertOnlyNumNodes--);
        fastFree(node);
    }

    void freeAllNodes()
    {
        if (!m_root)
            return;

        Vector<std::pair<NodeRef, unsigned>, 16> stack;
        stack.append({ m_root, m_height });

        while (!stack.isEmpty()) {
            auto [node, distanceToLeaf] = stack.takeLast();

            if (!distanceToLeaf) {
                freeNode(node.asLeaf());
                continue;
            }
            InnerNode* inner = node.asInner();
            for (size_t i = 0; i < node.size(); ++i)
                stack.append({ inner->child(i), distanceToLeaf - 1 });
            freeNode(inner);
        }
    }

    void dumpSubtree(PrintStream& out, NodeRef nodeRef, unsigned distanceToLeaf, unsigned indent) const
    {
        auto printIndent = [&] {
            for (unsigned i = 0; i < indent; ++i)
                out.print("  ");
        };

        if (distanceToLeaf) {
            InnerNode* inner = nodeRef.asInner();
            printIndent();
            out.println("Inner(size=", nodeRef.size(), ", coverage=", inner->coverage(nodeRef.size()), "):");

            for (size_t i = 0; i < nodeRef.size(); ++i) {
                printIndent();
                out.println("  [", i, "] ", inner->interval(i));
                dumpSubtree(out, inner->child(i), distanceToLeaf - 1, indent + 2);
            }
        } else {
            CommaPrinter comma;
            LeafNode* leaf = nodeRef.asLeaf();
            printIndent();
            out.print("Leaf(size=", nodeRef.size(), "): ");
            for (size_t i = 0; i < nodeRef.size(); ++i)
                out.print(comma, leaf->interval(i), "=", leaf->value(i));
            out.println();
        }
    }

    NodeRef m_root { };
    Interval m_rootInterval { };
    unsigned m_height { 0 };

#if ASSERT_ENABLED
    unsigned assertOnlyNumNodes { 0 };
#endif
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

using WTF::IntervalSet;
