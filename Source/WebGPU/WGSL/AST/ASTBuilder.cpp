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

#include "ASTNode.h"

namespace WGSL::AST {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(WGSLAST);

Builder::Builder(Builder&& other)
{
    m_arena = std::exchange(other.m_arena, nullptr);
    m_arenaEnd = std::exchange(other.m_arenaEnd, nullptr);
    m_arenas = WTFMove(other.m_arenas);
    m_nodes = WTFMove(other.m_nodes);
}

inline uint8_t* Builder::arena()
{
    ASSERT(m_arenaEnd);
    return m_arenaEnd - arenaSize;
}

Builder::~Builder()
{
    size_t size = m_nodes.size();
    for (size_t i = 0; i < size; ++i)
        m_nodes[i]->~Node();
}

void Builder::allocateArena()
{
    m_arenas.append(MallocPtr<uint8_t, WGSLASTMalloc>::malloc(arenaSize));
    m_arena = m_arenas.last().get();
    m_arenaEnd = m_arena + arenaSize;
    ASSERT(arena() == m_arena);
}

auto Builder::saveCurrentState() -> State
{
    State state;
    state.m_arena = m_arena;
#if ASSERT_ENABLED
    state.m_arenaEnd = m_arenaEnd;
    if (m_arenaEnd)
        state.m_arenaStart = arena();
    else
        state.m_arenaStart = nullptr;
#endif
    state.m_numberOfArenas = m_arenas.size();
    state.m_numberOfNodes = m_nodes.size();
    allocateArena();
    return state;
}

void Builder::restore(State&& state)
{
    for (size_t i = state.m_numberOfNodes; i < m_nodes.size(); ++i)
        m_nodes[i]->~Node();
    m_nodes.shrink(state.m_numberOfNodes);
    m_arena = state.m_arena;
    m_arenas.shrink(state.m_numberOfArenas);
    if (m_arenas.isEmpty())
        m_arenaEnd = nullptr;
    else {
        m_arenaEnd = m_arenas.last().get() + arenaSize;
#if ASSERT_ENABLED
        ASSERT(state.m_arenaStart == m_arenas.last().get());
        ASSERT(state.m_arenaEnd == m_arenaEnd);
#endif
    }
}

} // namespace WGSL::AST
