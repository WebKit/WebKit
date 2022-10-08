/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "DFGArgumentsUtilities.h"

#if ENABLE(DFG_JIT)

#include "JSImmutableButterfly.h"

namespace JSC { namespace DFG {

bool argumentsInvolveStackSlot(InlineCallFrame* inlineCallFrame, Operand operand)
{
    if (operand.isTmp())
        return false;

    VirtualRegister reg = operand.virtualRegister();
    if (!inlineCallFrame)
        return (reg.isArgument() && reg.toArgument()) || reg.isHeader();

    if (inlineCallFrame->isClosureCall
        && reg == VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::callee))
        return true;
    
    if (inlineCallFrame->isVarargs()
        && reg == VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::argumentCountIncludingThis))
        return true;
    
    // We do not include fixups here since it is not related to |arguments|, rest parameters, and varargs.
    unsigned numArguments = static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1);
    VirtualRegister argumentStart =
        VirtualRegister(inlineCallFrame->stackOffset) + CallFrame::argumentOffset(0);
    return reg >= argumentStart && reg < argumentStart + numArguments;
}

bool argumentsInvolveStackSlot(Node* candidate, Operand operand)
{
    return argumentsInvolveStackSlot(candidate->origin.semantic.inlineCallFrame(), operand);
}

Node* emitCodeToGetArgumentsArrayLength(
    InsertionSet& insertionSet, Node* arguments, unsigned nodeIndex, NodeOrigin origin, bool addThis)
{
    Graph& graph = insertionSet.graph();

    DFG_ASSERT(
        graph, arguments,
        arguments->op() == CreateDirectArguments || arguments->op() == CreateScopedArguments
        || arguments->op() == CreateClonedArguments || arguments->op() == CreateRest
        || arguments->op() == NewArrayBuffer
        || arguments->op() == PhantomDirectArguments || arguments->op() == PhantomClonedArguments
        || arguments->op() == PhantomCreateRest || arguments->op() == PhantomNewArrayBuffer
        || arguments->op() == PhantomNewArrayWithSpread || arguments->op() == PhantomSpread,
        arguments->op());

    if (arguments->op() == PhantomSpread)
        return emitCodeToGetArgumentsArrayLength(insertionSet, arguments->child1().node(), nodeIndex, origin, addThis);

    if (arguments->op() == PhantomNewArrayWithSpread) {
        unsigned numberOfNonSpreadArguments = addThis;
        BitVector* bitVector = arguments->bitVector();
        Node* currentSum = nullptr;
        for (unsigned i = 0; i < arguments->numChildren(); i++) {
            if (bitVector->get(i)) {
                Node* child = graph.varArgChild(arguments, i).node();
                DFG_ASSERT(graph, child, child->op() == PhantomSpread, child->op());
                DFG_ASSERT(graph, child->child1().node(),
                    child->child1()->op() == PhantomCreateRest || child->child1()->op() == PhantomNewArrayBuffer,
                    child->child1()->op());
                Node* lengthOfChild = emitCodeToGetArgumentsArrayLength(insertionSet, child->child1().node(), nodeIndex, origin);
                if (currentSum)
                    currentSum = insertionSet.insertNode(nodeIndex, SpecInt32Only, ArithAdd, origin, OpInfo(Arith::CheckOverflow), Edge(currentSum, Int32Use), Edge(lengthOfChild, Int32Use));
                else
                    currentSum = lengthOfChild;
            } else
                numberOfNonSpreadArguments++;
        }
        if (currentSum) {
            if (!numberOfNonSpreadArguments)
                return currentSum;
            return insertionSet.insertNode(
                nodeIndex, SpecInt32Only, ArithAdd, origin, OpInfo(Arith::CheckOverflow), Edge(currentSum, Int32Use),
                insertionSet.insertConstantForUse(nodeIndex, origin, jsNumber(numberOfNonSpreadArguments), Int32Use));
        }
        return insertionSet.insertConstant(nodeIndex, origin, jsNumber(numberOfNonSpreadArguments));
    }

    if (arguments->op() == NewArrayBuffer || arguments->op() == PhantomNewArrayBuffer) {
        return insertionSet.insertConstant(
            nodeIndex, origin, jsNumber(arguments->castOperand<JSImmutableButterfly*>()->length() + addThis));
    }
    
    InlineCallFrame* inlineCallFrame = arguments->origin.semantic.inlineCallFrame();

    unsigned numberOfArgumentsToSkip = 0;
    if (arguments->op() == CreateRest || arguments->op() == PhantomCreateRest)
        numberOfArgumentsToSkip = arguments->numberOfArgumentsToSkip();
    
    if (inlineCallFrame && !inlineCallFrame->isVarargs()) {
        unsigned argumentsSize = inlineCallFrame->argumentCountIncludingThis - !addThis;
        if (argumentsSize >= numberOfArgumentsToSkip)
            argumentsSize -= numberOfArgumentsToSkip;
        else
            argumentsSize = 0;
        return insertionSet.insertConstant(
            nodeIndex, origin, jsNumber(argumentsSize));
    }
    
    Node* argumentCount = insertionSet.insertNode(nodeIndex,
        SpecInt32Only, GetArgumentCountIncludingThis, origin, OpInfo(inlineCallFrame));

    Node* result = insertionSet.insertNode(
        nodeIndex, SpecInt32Only, ArithSub, origin, OpInfo(Arith::Unchecked),
        Edge(argumentCount, Int32Use),
        insertionSet.insertConstantForUse(
            nodeIndex, origin, jsNumber(numberOfArgumentsToSkip + !addThis), Int32Use));

    if (numberOfArgumentsToSkip) {
        // The above subtraction may produce a negative number if this number is non-zero. We correct that here.
        unsigned firstChild = graph.m_varArgChildren.size();
        graph.m_varArgChildren.append(Edge(result, Int32Use));
        graph.m_varArgChildren.append(insertionSet.insertConstantForUse(nodeIndex, origin, jsNumber(static_cast<unsigned>(addThis)), Int32Use));

        result = insertionSet.insertNode(
            nodeIndex, SpecInt32Only, Node::VarArg, ArithMax, origin,
            OpInfo(), OpInfo(), firstChild, graph.m_varArgChildren.size() - firstChild);

        result->setResult(NodeResultInt32);
    }

    return result;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

