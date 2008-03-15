/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef NodeWithIndexAfter_h
#define NodeWithIndexAfter_h

#include "Node.h"

namespace WebCore {

// Like NodeWithIndex, but treats a node pointer of 0 as meaning
// "before the last node in the container" and returns the index
// value after the node rather than the value of the node.
class NodeWithIndexAfter {
public:
    NodeWithIndexAfter(Node* node)
        : m_node(node)
        , m_haveIndex(false)
    {
    }

    int indexAfter() const
    {
        if (!m_haveIndex) {
            m_indexAfter = m_node ? m_node->nodeIndex() + 1 : 0;
            m_haveIndex = true;
        }
        ASSERT(m_indexAfter == (m_node ? static_cast<int>(m_node->nodeIndex()) + 1 : 0));
        return m_indexAfter;
    }

private:
    Node* m_node;
    mutable bool m_haveIndex;
    mutable int m_indexAfter;
};

}

#endif
