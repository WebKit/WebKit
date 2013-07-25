/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "FTLCapabilities.h"

#if ENABLE(FTL_JIT)

namespace JSC { namespace FTL {

using namespace DFG;

bool canCompile(Graph& graph)
{
    for (BlockIndex blockIndex = graph.m_blocks.size(); blockIndex--;) {
        BasicBlock* block = graph.m_blocks[blockIndex].get();
        if (!block)
            continue;
        
        for (unsigned nodeIndex = block->size(); nodeIndex--;) {
            Node* node = block->at(nodeIndex);
            
            for (unsigned childIndex = graph.numChildren(node); childIndex--;) {
                Edge edge = graph.child(node, childIndex);
                if (!edge)
                    continue;
                switch (edge.useKind()) {
                case UntypedUse:
                case Int32Use:
                case KnownInt32Use:
                case NumberUse:
                case KnownNumberUse:
                case RealNumberUse:
                case BooleanUse:
                case CellUse:
                case KnownCellUse:
                case ObjectUse:
                    // These are OK.
                    break;
                default:
                    // Don't know how to handle anything else.
                    return false;
                }
            }
            
            switch (node->op()) {
            case JSConstant:
            case WeakJSConstant:
            case GetLocal:
            case SetLocal:
            case MovHintAndCheck:
            case MovHint:
            case ZombieHint:
            case Phantom:
            case Flush:
            case PhantomLocal:
            case SetArgument:
            case Return:
            case BitAnd:
            case BitOr:
            case BitXor:
            case BitRShift:
            case BitLShift:
            case BitURShift:
            case CheckStructure:
            case StructureTransitionWatchpoint:
            case PutStructure:
            case PhantomPutStructure:
            case GetButterfly:
            case GetByOffset:
            case PutByOffset:
            case GetGlobalVar:
            case PutGlobalVar:
            case ValueAdd:
            case ArithAdd:
            case ArithSub:
            case ArithMul:
            case ArithNegate:
            case UInt32ToNumber:
            case Int32ToDouble:
            case CompareEqConstant:
            case CompareStrictEqConstant:
                // These are OK.
                break;
            case GetArrayLength:
                switch (node->arrayMode().type()) {
                case Array::Int32:
                case Array::Double:
                case Array::Contiguous:
                    break;
                default:
                    return false;
                }
                break;
            case GetByVal:
                switch (node->arrayMode().type()) {
                case Array::Int32:
                case Array::Double:
                case Array::Contiguous:
                    break;
                default:
                    return false;
                }
                switch (node->arrayMode().speculation()) {
                case Array::SaneChain:
                case Array::InBounds:
                    break;
                default:
                    return false;
                }
                break;
            case CompareEq:
            case CompareStrictEq:
                if (node->isBinaryUseKind(Int32Use))
                    break;
                if (node->isBinaryUseKind(NumberUse))
                    break;
                if (node->isBinaryUseKind(ObjectUse))
                    break;
                return false;
            case CompareLess:
            case CompareLessEq:
            case CompareGreater:
            case CompareGreaterEq:
                if (node->isBinaryUseKind(Int32Use))
                    break;
                if (node->isBinaryUseKind(NumberUse))
                    break;
                return false;
            case Branch:
            case LogicalNot:
                if (node->child1().useKind() == BooleanUse)
                    break;
                return false;
            default:
                // Don't know how to handle anything else.
                return false;
            }
        }
    }
    
    return true;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

