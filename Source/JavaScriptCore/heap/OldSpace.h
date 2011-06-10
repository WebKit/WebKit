/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef OldSpace_h
#define OldSpace_h

#include "MarkedBlock.h"
#include <wtf/DoublyLinkedList.h>
#include <wtf/Noncopyable.h>

namespace JSC {
    
class Heap;

class OldSpace {
    WTF_MAKE_NONCOPYABLE(OldSpace);
public:
    OldSpace(Heap*);

    void addBlock(MarkedBlock*);
    void removeBlock(MarkedBlock*);

    template<typename Functor> typename Functor::ReturnType forEachBlock();
    template<typename Functor> typename Functor::ReturnType forEachBlock(Functor&);

private:
    DoublyLinkedList<MarkedBlock> m_blocks;
    Heap* m_heap;
};

template <typename Functor> inline typename Functor::ReturnType OldSpace::forEachBlock(Functor& functor)
{
    MarkedBlock* next;
    for (MarkedBlock* block = m_blocks.head(); block; block = next) {
        next = block->next();
        functor(block);
    }

    return functor.returnValue();
}

template <typename Functor> inline typename Functor::ReturnType OldSpace::forEachBlock()
{
    Functor functor;
    return forEachBlock(functor);
}

} // namespace JSC

#endif // OldSpace_h
