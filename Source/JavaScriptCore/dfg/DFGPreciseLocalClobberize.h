/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#ifndef DFGPreciseLocalClobberize_h
#define DFGPreciseLocalClobberize_h

#if ENABLE(DFG_JIT)

#include "DFGClobberize.h"
#include "DFGMayExit.h"

namespace JSC { namespace DFG {

template<typename ReadFunctor, typename WriteFunctor, typename DefFunctor>
class PreciseLocalClobberizeAdaptor {
public:
    PreciseLocalClobberizeAdaptor(
        Graph& graph, Node* node,
        const ReadFunctor& read, const WriteFunctor& write, const DefFunctor& def)
        : m_graph(graph)
        , m_node(node)
        , m_read(read)
        , m_unconditionalWrite(write)
        , m_def(def)
    {
    }
    
    void read(AbstractHeap heap)
    {
        if (heap.kind() == Stack) {
            if (heap.payload().isTop()) {
                readTop();
                return;
            }
            
            callIfAppropriate(m_read, VirtualRegister(heap.payload().value32()));
            return;
        }
        
        if (heap.overlaps(Stack)) {
            readTop();
            return;
        }
    }
    
    void write(AbstractHeap heap)
    {
        // We expect stack writes to already be precisely characterized by DFG::clobberize().
        if (heap.kind() == Stack) {
            RELEASE_ASSERT(!heap.payload().isTop());
            callIfAppropriate(m_unconditionalWrite, VirtualRegister(heap.payload().value32()));
            return;
        }
        
        RELEASE_ASSERT(!heap.overlaps(Stack));
    }
    
    void def(PureValue)
    {
        // PureValue defs never have anything to do with locals, so ignore this.
    }
    
    void def(HeapLocation location, LazyNode node)
    {
        if (location.kind() != StackLoc)
            return;
        
        RELEASE_ASSERT(location.heap().kind() == Stack);
        
        m_def(VirtualRegister(location.heap().payload().value32()), node);
    }
    
private:
    template<typename Functor>
    void callIfAppropriate(const Functor& functor, VirtualRegister operand)
    {
        if (operand.isLocal() && static_cast<unsigned>(operand.toLocal()) >= m_graph.block(0)->variablesAtHead.numberOfLocals())
            return;
        
        if (operand.isArgument() && !operand.isHeader() && static_cast<unsigned>(operand.toArgument()) >= m_graph.block(0)->variablesAtHead.numberOfArguments())
            return;
        
        functor(operand);
    }
    
    void readTop()
    {
        switch (m_node->op()) {
        case GetMyArgumentByVal:
        case GetMyArgumentByValOutOfBounds:
        case ForwardVarargs:
        case CallForwardVarargs:
        case ConstructForwardVarargs:
        case TailCallForwardVarargs:
        case TailCallForwardVarargsInlinedCaller: {

            InlineCallFrame* inlineCallFrame;
            if (m_node->argumentsChild())
                inlineCallFrame = m_node->argumentsChild()->origin.semantic.inlineCallFrame;
            else
                inlineCallFrame = m_node->origin.semantic.inlineCallFrame;
            if (!inlineCallFrame) {
                // Read the outermost arguments and argument count.
                for (unsigned i = m_graph.m_codeBlock->numParameters(); i-- > 1;)
                    m_read(virtualRegisterForArgument(i));
                m_read(VirtualRegister(CallFrameSlot::argumentCount));
                break;
            }
            
            for (unsigned i = inlineCallFrame->arguments.size(); i-- > 1;)
                m_read(VirtualRegister(inlineCallFrame->stackOffset + virtualRegisterForArgument(i).offset()));
            if (inlineCallFrame->isVarargs())
                m_read(VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::argumentCount));
            break;
        }
            
        default: {
            // All of the outermost arguments, except this, are definitely read.
            for (unsigned i = m_graph.m_codeBlock->numParameters(); i-- > 1;)
                m_read(virtualRegisterForArgument(i));
        
            // The stack header is read.
            for (unsigned i = 0; i < CallFrameSlot::thisArgument; ++i)
                m_read(VirtualRegister(i));
        
            // Read all of the inline arguments and call frame headers that we didn't already capture.
            for (InlineCallFrame* inlineCallFrame = m_node->origin.semantic.inlineCallFrame; inlineCallFrame; inlineCallFrame = inlineCallFrame->getCallerInlineFrameSkippingTailCalls()) {
                for (unsigned i = inlineCallFrame->arguments.size(); i-- > 1;)
                    m_read(VirtualRegister(inlineCallFrame->stackOffset + virtualRegisterForArgument(i).offset()));
                if (inlineCallFrame->isClosureCall)
                    m_read(VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::callee));
                if (inlineCallFrame->isVarargs())
                    m_read(VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::argumentCount));
            }
            break;
        } }
    }
    
    Graph& m_graph;
    Node* m_node;
    const ReadFunctor& m_read;
    const WriteFunctor& m_unconditionalWrite;
    const DefFunctor& m_def;
};

template<typename ReadFunctor, typename WriteFunctor, typename DefFunctor>
void preciseLocalClobberize(
    Graph& graph, Node* node,
    const ReadFunctor& read, const WriteFunctor& write, const DefFunctor& def)
{
    PreciseLocalClobberizeAdaptor<ReadFunctor, WriteFunctor, DefFunctor>
        adaptor(graph, node, read, write, def);
    clobberize(graph, node, adaptor);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGPreciseLocalClobberize_h

