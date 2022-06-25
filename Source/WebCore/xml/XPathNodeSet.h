/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "Node.h"

namespace WebCore {
    namespace XPath {

        class NodeSet {
        public:
            NodeSet() : m_isSorted(true), m_subtreesAreDisjoint(false) { }
            explicit NodeSet(RefPtr<Node>&& node)
                : m_isSorted(true), m_subtreesAreDisjoint(false), m_nodes(1, WTFMove(node))
            { }
            
            size_t size() const { return m_nodes.size(); }
            bool isEmpty() const { return m_nodes.isEmpty(); }
            Node* operator[](unsigned i) const { return m_nodes.at(i).get(); }
            void reserveCapacity(size_t newCapacity) { m_nodes.reserveCapacity(newCapacity); }
            void clear() { m_nodes.clear(); }

            // NodeSet itself does not verify that nodes in it are unique.
            void append(RefPtr<Node>&& node) { m_nodes.append(WTFMove(node)); }
            void append(const NodeSet& nodeSet) { m_nodes.appendVector(nodeSet.m_nodes); }

            // Returns the set's first node in document order, or nullptr if the set is empty.
            Node* firstNode() const;

            // Returns nullptr if the set is empty.
            Node* anyNode() const;

            // NodeSet itself doesn't check if it contains nodes in document order - the caller should tell it if it does not.
            void markSorted(bool isSorted) { m_isSorted = isSorted; }
            bool isSorted() const { return m_isSorted || m_nodes.size() < 2; }

            void sort() const;

            // No node in the set is ancestor of another. Unlike m_isSorted, this is assumed to be false, unless the caller sets it to true.
            void markSubtreesDisjoint(bool disjoint) { m_subtreesAreDisjoint = disjoint; }
            bool subtreesAreDisjoint() const { return m_subtreesAreDisjoint || m_nodes.size() < 2; }

            const RefPtr<Node>* begin() const { return m_nodes.begin(); }
            const RefPtr<Node>* end() const { return m_nodes.end(); }

        private:
            void traversalSort() const;

            mutable bool m_isSorted;
            bool m_subtreesAreDisjoint;
            mutable Vector<RefPtr<Node>> m_nodes;
        };

    } // namespace XPath
} // namespace WebCore
