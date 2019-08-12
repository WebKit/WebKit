/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <wtf/GraphNodeWorklist.h>

template<typename Graph>
class SpanningTree {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SpanningTree(Graph& graph)
        : m_graph(graph)
        , m_data(graph.template newMap<Data>())
    {
        ExtendedGraphNodeWorklist<typename Graph::Node, unsigned, typename Graph::Set> worklist;
        worklist.push(m_graph.root(), 0);
    
        size_t number = 0;

        while (GraphNodeWith<typename Graph::Node, unsigned> item = worklist.pop()) {
            typename Graph::Node block = item.node;
            unsigned successorIndex = item.data;
        
            // We initially push with successorIndex = 0 regardless of whether or not we have any
            // successors. This is so that we can assign our prenumber. Subsequently we get pushed
            // with higher successorIndex values. We finally push successorIndex == # successors
            // to calculate our post number.
            ASSERT(!successorIndex || successorIndex <= m_graph.successors(block).size());

            if (!successorIndex)
                m_data[block].pre = number++;
        
            if (successorIndex < m_graph.successors(block).size()) {
                unsigned nextSuccessorIndex = successorIndex + 1;
                // We need to push this even if this is out of bounds so we can compute
                // the post number.
                worklist.forcePush(block, nextSuccessorIndex);

                typename Graph::Node successorBlock = m_graph.successors(block)[successorIndex];
                worklist.push(successorBlock, 0);
            } else
                m_data[block].post = number++;
        }
    }

    // Returns true if a is a descendent of b.
    // Note a is a descendent of b if they're equal.
    bool isDescendent(typename Graph::Node a, typename Graph::Node b)
    {
        return m_data[b].pre <= m_data[a].pre
            && m_data[b].post >= m_data[a].post;
    }

private:
    struct Data {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        size_t pre;
        size_t post;
    };

    Graph& m_graph;
    typename Graph::template Map<Data> m_data;
};
