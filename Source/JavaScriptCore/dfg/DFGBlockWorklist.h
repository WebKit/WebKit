/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef DFGBlockWorklist_h
#define DFGBlockWorklist_h

#if ENABLE(DFG_JIT)

#include "DFGBasicBlock.h"
#include "DFGBlockSet.h"
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

struct BasicBlock;

class BlockWorklist {
public:
    BlockWorklist();
    ~BlockWorklist();
    
    bool push(BasicBlock*); // Returns true if we didn't know about the block before.
    
    bool notEmpty() const { return !m_stack.isEmpty(); }
    BasicBlock* pop();
    
private:
    BlockSet m_seen;
    Vector<BasicBlock*, 16> m_stack;
};

// When you say BlockWith<int> you should read it as "block with an int".
template<typename T>
struct BlockWith {
    BlockWith()
        : block(nullptr)
    {
    }
    
    BlockWith(BasicBlock* block, const T& data)
        : block(block)
        , data(data)
    {
    }
    
    typedef void* (BlockWith<T>::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const
    {
        return block ? reinterpret_cast<UnspecifiedBoolType*>(1) : nullptr;
    }

    BasicBlock* block;
    T data;
};

// Extended block worklist is useful for enqueueing some meta-data along with the block. It also
// permits forcibly enqueueing things even if the block has already been seen. It's useful for
// things like building a spanning tree, in which case T (the auxiliary payload) would be the
// successor index.
template<typename T>
class ExtendedBlockWorklist {
public:
    ExtendedBlockWorklist() { }
    
    void forcePush(const BlockWith<T>& entry)
    {
        m_stack.append(entry);
    }
    
    void forcePush(BasicBlock* block, const T& data)
    {
        forcePush(BlockWith<T>(block, data));
    }
    
    bool push(const BlockWith<T>& entry)
    {
        if (!m_seen.add(entry.block))
            return false;
        
        forcePush(entry);
        return true;
    }
    
    bool push(BasicBlock* block, const T& data)
    {
        return push(BlockWith<T>(block, data));
    }
    
    bool notEmpty() const { return !m_stack.isEmpty(); }
    
    BlockWith<T> pop()
    {
        if (m_stack.isEmpty())
            return BlockWith<T>();
        
        return m_stack.takeLast();
    }

private:
    BlockSet m_seen;
    Vector<BlockWith<T>> m_stack;
};

enum VisitOrder {
    PreOrder,
    PostOrder
};

struct BlockWithOrder {
    BlockWithOrder()
        : block(nullptr)
        , order(PreOrder)
    {
    }
    
    BlockWithOrder(BasicBlock* block, VisitOrder order)
        : block(block)
        , order(order)
    {
    }
    
    typedef void* (BlockWithOrder::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const
    {
        return block ? reinterpret_cast<UnspecifiedBoolType*>(1) : nullptr;
    }

    BasicBlock* block;
    VisitOrder order;
};

// Block worklist suitable for post-order traversal.
class PostOrderBlockWorklist {
public:
    PostOrderBlockWorklist();
    ~PostOrderBlockWorklist();
    
    bool pushPre(BasicBlock*);
    void pushPost(BasicBlock*);
    
    bool push(BasicBlock* block, VisitOrder order = PreOrder)
    {
        switch (order) {
        case PreOrder:
            return pushPre(block);
        case PostOrder:
            pushPost(block);
            return true;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
    bool push(const BlockWithOrder& data)
    {
        return push(data.block, data.order);
    }
    
    bool notEmpty() const { return m_worklist.notEmpty(); }
    BlockWithOrder pop();

private:
    ExtendedBlockWorklist<VisitOrder> m_worklist;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGBlockWorklist_h

