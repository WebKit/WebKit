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

#pragma once

#include <wtf/MallocPtr.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/Vector.h>

#define WGSL_AST_BUILDER_NODE(Node) \
protected: \
    Node(const Node&) = default; \
    Node(Node&&) = default; \
    Node& operator=(const Node&) = default; \
    Node& operator=(Node&&) = default; \
private: \
    friend class Builder; \
    friend ShaderModule;

namespace WGSL {

class ShaderModule;

namespace AST {

class Node;

class Builder {
    WTF_MAKE_NONCOPYABLE(Builder);

public:
    static constexpr size_t arenaSize = 0x4000;

    Builder() = default;
    Builder(Builder&&);
    ~Builder();

    template<typename T, typename... Arguments, typename = std::enable_if_t<std::is_base_of_v<Node, T>>>
    T& construct(Arguments&&... arguments)
    {
        constexpr size_t size = sizeof(T);
        constexpr size_t alignedSize = alignSize(size);
        static_assert(alignedSize <= arenaSize);
        if (UNLIKELY(m_arena.size() < alignedSize))
            allocateArena();

        auto* node = new (m_arena.data()) T(std::forward<Arguments>(arguments)...);
        m_arena = m_arena.subspan(alignedSize);
        m_nodes.append(node);
        return *node;
    }

    class State {
        friend Builder;
    private:
        State() = default;

        std::span<uint8_t> m_arena;
        unsigned m_numberOfArenas;
        unsigned m_numberOfNodes;
    };

    State saveCurrentState();
    void restore(State&&);

private:
    static constexpr size_t alignSize(size_t size)
    {
        return (size + sizeof(WTF::AllocAlignmentInteger) - 1) & ~(sizeof(WTF::AllocAlignmentInteger) - 1);
    }

    void allocateArena();

    std::span<uint8_t> m_arena { };
    uint8_t* m_arenaEnd { nullptr };
    Vector<FixedVector<uint8_t>> m_arenas;
    Vector<Node*> m_nodes;
};

} // namespace AST
} // namespace WGSL
