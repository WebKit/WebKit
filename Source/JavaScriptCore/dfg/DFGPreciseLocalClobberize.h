/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGClobberize.h"

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
            
            callIfAppropriate(m_read, heap.operand());
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
            callIfAppropriate(m_unconditionalWrite, heap.operand());
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
        
        m_def(location.heap().operand(), node);
    }
    
private:
    template<typename Functor>
    void callIfAppropriate(const Functor& functor, Operand operand)
    {
        if (operand.isLocal() && static_cast<unsigned>(operand.toLocal()) >= m_graph.block(0)->variablesAtHead.numberOfLocals())
            return;
        
        if (operand.isArgument() && !operand.isHeader() && static_cast<unsigned>(operand.toArgument()) >= m_graph.block(0)->variablesAtHead.numberOfArguments())
            return;
        
        functor(operand);
    }
    
    void readTop()
    {
        auto readWorld = [&] (Node* node) {
            // All of the outermost arguments, except this, are read in sloppy mode.
            if (!m_graph.m_codeBlock->ownerExecutable()->isInStrictContext()) {
                for (unsigned i = m_graph.m_codeBlock->numParameters(); i--;)
                    m_read(virtualRegisterForArgumentIncludingThis(i));
            }

            // The stack header is read.
            for (unsigned i = 0; i < CallFrameSlot::thisArgument; ++i)
                m_read(VirtualRegister(i));

            // Read all of the inline arguments and call frame headers that we didn't already capture.
            for (InlineCallFrame* inlineCallFrame = node->origin.semantic.inlineCallFrame(); inlineCallFrame; inlineCallFrame = inlineCallFrame->getCallerInlineFrameSkippingTailCalls()) {
                if (!inlineCallFrame->isInStrictContext()) {
                    for (unsigned i = inlineCallFrame->m_argumentsWithFixup.size(); i--;)
                        m_read(VirtualRegister(inlineCallFrame->stackOffset + virtualRegisterForArgumentIncludingThis(i).offset()));
                }
                if (inlineCallFrame->isClosureCall)
                    m_read(VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::callee));
                if (inlineCallFrame->isVarargs())
                    m_read(VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::argumentCountIncludingThis));
            }
        };

        auto readFrame = [&] (InlineCallFrame* inlineCallFrame, unsigned numberOfArgumentsToSkip) {
            if (!inlineCallFrame) {
                // Read the outermost arguments and argument count.
                for (unsigned i = numberOfArgumentsToSkip; i < static_cast<unsigned>(m_graph.m_codeBlock->numParameters()); i++)
                    m_read(virtualRegisterForArgumentIncludingThis(i));
                m_read(VirtualRegister(CallFrameSlot::argumentCountIncludingThis));
                return;
            }
            
            for (unsigned i = numberOfArgumentsToSkip; i < inlineCallFrame->m_argumentsWithFixup.size(); i++)
                m_read(VirtualRegister(inlineCallFrame->stackOffset + virtualRegisterForArgumentIncludingThis(i).offset()));
            if (inlineCallFrame->isVarargs())
                m_read(VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::argumentCountIncludingThis));
        };

        auto readSpread = [&] (Node* spread) {
            ASSERT(spread->op() == Spread || spread->op() == PhantomSpread);
            if (!spread->child1()->isPhantomAllocation()) {
                readWorld(spread);
                return;
            }

            ASSERT(spread->child1()->op() == PhantomCreateRest || spread->child1()->op() == PhantomNewArrayBuffer);
            if (spread->child1()->op() == PhantomNewArrayBuffer) {
                // This reads from a constant buffer.
                return;
            }
            InlineCallFrame* inlineCallFrame = spread->child1()->origin.semantic.inlineCallFrame();
            unsigned numberOfArgumentsToSkip = spread->child1()->numberOfArgumentsToSkip();
            readFrame(inlineCallFrame, numberOfArgumentsToSkip);
        };

        auto readNewArrayWithSpreadNode = [&] (Node* arrayWithSpread) {
            ASSERT(arrayWithSpread->op() == NewArrayWithSpread || arrayWithSpread->op() == PhantomNewArrayWithSpread);
            BitVector* bitVector = arrayWithSpread->bitVector();
            for (unsigned i = 0; i < arrayWithSpread->numChildren(); i++) {
                if (bitVector->get(i)) {
                    Node* child = m_graph.varArgChild(arrayWithSpread, i).node();
                    if (child->op() == PhantomSpread)
                        readSpread(child);
                }
            }
        };

        switch (m_node->op()) {
        case ForwardVarargs:
        case CallForwardVarargs:
        case ConstructForwardVarargs:
        case TailCallForwardVarargs:
        case TailCallForwardVarargsInlinedCaller:
        case GetMyArgumentByVal:
        case GetMyArgumentByValOutOfBounds:
        case CreateDirectArguments:
        case CreateScopedArguments:
        case CreateClonedArguments:
        case CreateArgumentsButterfly:
        case PhantomDirectArguments:
        case PhantomClonedArguments:
        case GetRestLength:
        case CreateRest: {
            bool isForwardingNode = false;
            bool isPhantomNode = false;
            switch (m_node->op()) {
            case ForwardVarargs:
            case CallForwardVarargs:
            case ConstructForwardVarargs:
            case TailCallForwardVarargs:
            case TailCallForwardVarargsInlinedCaller:
                isForwardingNode = true;
                break;
            case PhantomDirectArguments:
            case PhantomClonedArguments:
                isPhantomNode = true;
                break;
            default:
                break;
            }

            if (isPhantomNode && m_graph.m_plan.isFTL())
                break;
            
            if (isForwardingNode && m_node->hasArgumentsChild() && m_node->argumentsChild()
                && (m_node->argumentsChild()->op() == PhantomNewArrayWithSpread || m_node->argumentsChild()->op() == PhantomSpread)) {
                if (m_node->argumentsChild()->op() == PhantomNewArrayWithSpread)
                    readNewArrayWithSpreadNode(m_node->argumentsChild().node());
                else
                    readSpread(m_node->argumentsChild().node());
            } else {
                InlineCallFrame* inlineCallFrame;
                if (m_node->hasArgumentsChild() && m_node->argumentsChild())
                    inlineCallFrame = m_node->argumentsChild()->origin.semantic.inlineCallFrame();
                else
                    inlineCallFrame = m_node->origin.semantic.inlineCallFrame();

                unsigned numberOfArgumentsToSkip = 0;
                if (m_node->op() == GetMyArgumentByVal || m_node->op() == GetMyArgumentByValOutOfBounds) {
                    // The value of numberOfArgumentsToSkip guarantees that GetMyArgumentByVal* will never
                    // read any arguments below the number of arguments to skip. For example, if numberOfArgumentsToSkip is 2,
                    // we will never read argument 0 or argument 1.
                    numberOfArgumentsToSkip = m_node->numberOfArgumentsToSkip();
                }

                readFrame(inlineCallFrame, numberOfArgumentsToSkip);
            }

            break;
        }
        
        case Spread:
            readSpread(m_node);
            break;
        
        case NewArrayWithSpread: {
            readNewArrayWithSpreadNode(m_node);
            break;
        }

        case GetArgument: {
            InlineCallFrame* inlineCallFrame = m_node->origin.semantic.inlineCallFrame();
            unsigned indexIncludingThis = m_node->argumentIndex();
            if (!inlineCallFrame) {
                if (indexIncludingThis < static_cast<unsigned>(m_graph.m_codeBlock->numParameters()))
                    m_read(virtualRegisterForArgumentIncludingThis(indexIncludingThis));
                m_read(VirtualRegister(CallFrameSlot::argumentCountIncludingThis));
                break;
            }

            ASSERT_WITH_MESSAGE(inlineCallFrame->isVarargs(), "GetArgument is only used for InlineCallFrame if the call frame is varargs.");
            if (indexIncludingThis < inlineCallFrame->m_argumentsWithFixup.size())
                m_read(VirtualRegister(inlineCallFrame->stackOffset + virtualRegisterForArgumentIncludingThis(indexIncludingThis).offset()));
            m_read(VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::argumentCountIncludingThis));
            break;
        }
            
        default: {
            readWorld(m_node);
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
