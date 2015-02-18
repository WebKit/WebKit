/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
        , m_write(write)
        , m_def(def)
    {
    }
    
    void read(AbstractHeap heap)
    {
        if (heap.kind() == Variables) {
            if (heap.payload().isTop()) {
                readTop();
                return;
            }
            
            callIfAppropriate(m_read, VirtualRegister(heap.payload().value32()));
            return;
        }
        
        if (heap.overlaps(Variables)) {
            readTop();
            return;
        }
    }
    
    void write(AbstractHeap heap)
    {
        if (heap.kind() == Variables) {
            if (heap.payload().isTop()) {
                writeTop();
                return;
            }
            
            callIfAppropriate(m_write, VirtualRegister(heap.payload().value32()));
            return;
        }
        
        if (heap.overlaps(Variables)) {
            writeTop();
            return;
        }
    }
    
    void def(PureValue)
    {
        // PureValue defs never have anything to do with locals, so ignore this.
    }
    
    void def(HeapLocation location, Node* node)
    {
        if (location.kind() != VariableLoc)
            return;
        
        RELEASE_ASSERT(location.heap().kind() == Variables);
        
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
        // All of the outermost arguments, except this, are definitely read.
        for (unsigned i = m_graph.m_codeBlock->numParameters(); i-- > 1;)
            m_read(virtualRegisterForArgument(i));
        
        // The stack header is read.
        for (unsigned i = 0; i < JSStack::ThisArgument; ++i)
            m_read(VirtualRegister(i));
        
        // Read all of the captured variables.
        const BitVector& capturedVars =
            m_graph.capturedVarsFor(m_node->origin.semantic.inlineCallFrame);
        for (unsigned i : capturedVars.setBits())
            m_read(virtualRegisterForLocal(i));
        
        // Read all of the inline arguments and call frame headers that we didn't already capture.
        for (InlineCallFrame* inlineCallFrame = m_node->origin.semantic.inlineCallFrame; inlineCallFrame; inlineCallFrame = inlineCallFrame->caller.inlineCallFrame) {
            for (unsigned i = inlineCallFrame->arguments.size(); i-- > 1;)
                m_read(VirtualRegister(inlineCallFrame->stackOffset + virtualRegisterForArgument(i).offset()));
            if (inlineCallFrame->isClosureCall)
                m_read(VirtualRegister(inlineCallFrame->stackOffset + JSStack::Callee));
            if (inlineCallFrame->isVarargs())
                m_read(VirtualRegister(inlineCallFrame->stackOffset + JSStack::ArgumentCount));
        }
    }
    
    void writeTop()
    {
        if (m_node->op() == LoadVarargs) {
            // Make sure we note the writes to the locals that will store the array elements and
            // count.
            LoadVarargsData* data = m_node->loadVarargsData();
            m_write(data->count);
            for (unsigned i = data->limit; i--;)
                m_write(VirtualRegister(data->start.offset() + i));
        }
        
        // Note that we don't need to do anything special for CallForwardVarargs, since it reads
        // our arguments the same way that any effectful thing might.
        
        if (m_graph.m_codeBlock->usesArguments()) {
            for (unsigned i = m_graph.m_codeBlock->numParameters(); i-- > 1;)
                m_write(virtualRegisterForArgument(i));
        }

        const BitVector& capturedVars =
            m_graph.capturedVarsFor(m_node->origin.semantic.inlineCallFrame);
        for (unsigned i : capturedVars.setBits())
            m_write(virtualRegisterForLocal(i));
    }
    
    Graph& m_graph;
    Node* m_node;
    const ReadFunctor& m_read;
    const WriteFunctor& m_write;
    const DefFunctor& m_def;
};

template<typename ReadFunctor>
void forEachLocalReadByUnwind(Graph& graph, CodeOrigin codeOrigin, const ReadFunctor& read)
{
    if (graph.uncheckedActivationRegister().isValid())
        read(graph.activationRegister());
    if (graph.m_codeBlock->usesArguments())
        read(unmodifiedArgumentsRegister(graph.argumentsRegisterFor(nullptr)));
    
    for (InlineCallFrame* inlineCallFrame = codeOrigin.inlineCallFrame; inlineCallFrame; inlineCallFrame = inlineCallFrame->caller.inlineCallFrame) {
        if (inlineCallFrame->executable->usesArguments())
            read(unmodifiedArgumentsRegister(graph.argumentsRegisterFor(inlineCallFrame)));
    }
}

template<typename ReadFunctor, typename WriteFunctor, typename DefFunctor>
void preciseLocalClobberize(
    Graph& graph, Node* node,
    const ReadFunctor& read, const WriteFunctor& write, const DefFunctor& def)
{
    PreciseLocalClobberizeAdaptor<ReadFunctor, WriteFunctor, DefFunctor>
        adaptor(graph, node, read, write, def);
    clobberize(graph, node, adaptor);
    if (mayExit(graph, node))
        forEachLocalReadByUnwind(graph, node->origin.forExit, read);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGPreciseLocalClobberize_h

