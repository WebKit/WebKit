/*
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef XPathNodeSet_h
#define XPathNodeSet_h

#if ENABLE(XPATH)

#include <wtf/Vector.h>
#include <wtf/Forward.h>

#include "Node.h"

namespace WebCore {

    namespace XPath {

        class NodeSet {
        public:
        
            NodeSet() : m_isSorted(true) {}
            NodeSet(const NodeSet& other) : m_isSorted(other.m_isSorted), m_nodes(other.m_nodes) {}
            NodeSet& operator=(const NodeSet& other) { m_isSorted = other.m_isSorted; m_nodes = other.m_nodes; return *this; }
            
            size_t size() const { return m_nodes.size(); }
            bool isEmpty() const { return !m_nodes.size(); }
            Node* operator[](unsigned i) const { return m_nodes.at(i).get(); }
            void reserveCapacity(size_t newCapacity) { m_nodes.reserveCapacity(newCapacity); }
            void clear() { m_nodes.clear(); }
            void swap(NodeSet& other) { std::swap(m_isSorted, other.m_isSorted); m_nodes.swap(other.m_nodes); }

            // NodeSet itself does not verify that nodes in it are unique.
            void append(Node* node) { m_nodes.append(node); }
            void append(PassRefPtr<Node> node) { m_nodes.append(node); }
            void append(const NodeSet& nodeSet) { m_nodes.append(nodeSet.m_nodes); }

            // Returns the set's first node in document order, or 0 if the set is empty.
            Node* firstNode() const;

            // Returns 0 if the set is empty.
            Node* anyNode() const;

            // NodeSet itself doesn't check if it is contains sorted data - the caller should tell it if it does not.
            void markSorted(bool isSorted) { m_isSorted = isSorted; }
            bool isSorted() const { return m_isSorted; }

            void sort() const;

            void reverse();
        
        private:
            bool m_isSorted;
            Vector<RefPtr<Node> > m_nodes;
        };

    }
}

#endif // ENABLE(XPATH)
#endif // XPathNodeSet_h
