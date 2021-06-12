/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "LayoutContainerBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

class LayoutContainingBlockChainIterator {
public:
    LayoutContainingBlockChainIterator() = default;
    LayoutContainingBlockChainIterator(const ContainerBox*);
    const ContainerBox& operator*() const { return *m_current; }
    const ContainerBox* operator->() const { return m_current; }

    LayoutContainingBlockChainIterator& operator++();
    bool operator==(const LayoutContainingBlockChainIterator& other) const { return m_current == other.m_current; }
    bool operator!=(const LayoutContainingBlockChainIterator& other) const {  return !(*this == other); }

private:
    const ContainerBox* m_current { nullptr };
};

class LayoutContainingBlockChainIteratorAdapter {
public:
    LayoutContainingBlockChainIteratorAdapter(const ContainerBox&, const ContainerBox* stayWithin = nullptr);
    auto begin() { return LayoutContainingBlockChainIterator(&m_containingBlock); }
    auto end() { return LayoutContainingBlockChainIterator(m_stayWithin); }

private:
    const ContainerBox& m_containingBlock;
    const ContainerBox* m_stayWithin { nullptr };
};

LayoutContainingBlockChainIteratorAdapter containingBlockChain(const Box&);
LayoutContainingBlockChainIteratorAdapter containingBlockChain(const Box&, const ContainerBox& stayWithin);
LayoutContainingBlockChainIteratorAdapter containingBlockChainWithinFormattingContext(const Box&);

inline LayoutContainingBlockChainIterator::LayoutContainingBlockChainIterator(const ContainerBox* current)
    : m_current(current)
{
}

inline LayoutContainingBlockChainIterator& LayoutContainingBlockChainIterator::operator++()
{
    ASSERT(m_current);
    m_current = m_current->isInitialContainingBlock() ? nullptr : &m_current->containingBlock();
    return *this;
}

inline LayoutContainingBlockChainIteratorAdapter::LayoutContainingBlockChainIteratorAdapter(const ContainerBox& containingBlock, const ContainerBox* stayWithin)
    : m_containingBlock(containingBlock)
    , m_stayWithin(stayWithin)
{
}

inline LayoutContainingBlockChainIteratorAdapter containingBlockChain(const Box& layoutBox)
{
    return LayoutContainingBlockChainIteratorAdapter(layoutBox.containingBlock());
}

inline LayoutContainingBlockChainIteratorAdapter containingBlockChain(const Box& layoutBox, const ContainerBox& stayWithin)
{
    ASSERT(layoutBox.isDescendantOf(stayWithin));
    return LayoutContainingBlockChainIteratorAdapter(layoutBox.containingBlock(), &stayWithin);
}

inline LayoutContainingBlockChainIteratorAdapter containingBlockChainWithinFormattingContext(const Box& layoutBox)
{
    return containingBlockChain(layoutBox, layoutBox.formattingContextRoot());
}

}
}
#endif
