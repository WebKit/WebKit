/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ASTBuilder.h"

#include "ASTExpression.h"
#include "ASTNode.h"
#include "Types.h"
#include <wtf/Borrow.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>

namespace WGSL::AST {

Builder::Builder(Builder&& other)
    : m_slabs(WTF::move(other.m_slabs))
    , m_nodes(WTF::move(other.m_nodes))
    , m_currentSlabOffset(std::exchange(other.m_currentSlabOffset, slabCapacity))
{
}

Builder::~Builder()
{
    {
        Borrow nodes = m_nodes;
        for (size_t i = nodes->size(); i--; )
            nodes[i]->~Node();
    }
    for (auto* slab : borrow(m_slabs))
        fastFree(slab);
}

void* Builder::allocate(size_t size, size_t alignment)
{
    size_t alignedOffset = roundUpToMultipleOf(alignment, static_cast<size_t>(m_currentSlabOffset));
    if (alignedOffset + size > slabCapacity) [[unlikely]] {
        m_slabs.append(static_cast<Slab*>(fastAlignedMalloc(alignof(Slab), sizeof(Slab))));
        alignedOffset = 0;
    }
    m_currentSlabOffset = static_cast<unsigned>(alignedOffset + size);
    return std::span { m_slabs.last()->bytes }.subspan(alignedOffset, size).data();
}

auto Builder::saveCurrentState() -> State
{
    State state;
    state.m_numberOfNodes = m_nodes.size();
    state.m_numberOfSlabs = m_slabs.size();
    state.m_slabOffset = m_currentSlabOffset;
    return state;
}

void Builder::restore(State&& state)
{
    while (m_nodes.size() > state.m_numberOfNodes)
        m_nodes.takeLast()->~Node();
    while (m_slabs.size() > state.m_numberOfSlabs)
        fastFree(m_slabs.takeLast());
    m_currentSlabOffset = state.m_slabOffset;
}

size_t Builder::memoryFootprint() const
{
    size_t total = m_slabs.size() * slabCapacity + m_nodes.capacity() * sizeof(Node*) + m_nodes.size() * 24;
    if (m_slabs.capacity() > 4)
        total += m_slabs.capacity() * sizeof(Slab*);

    HashSet<const void*> seenConstants;
    for (const auto* node : borrow(m_nodes)) {
        if (const auto* expression = dynamicDowncast<const Expression>(*node)) {
            const auto& constant = expression->constantValue();
            if (const void* id = constant.storageIdentity()) {
                if (seenConstants.add(id).isNewEntry)
                    total += constant.storageMemoryFootprint();
            }
        }
    }
    return total;
}

} // namespace WGSL::AST
