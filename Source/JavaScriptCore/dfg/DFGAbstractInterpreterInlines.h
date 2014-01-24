/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef DFGAbstractInterpreterInlines_h
#define DFGAbstractInterpreterInlines_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGAbstractInterpreter.h"
#include "GetByIdStatus.h"
#include "Operations.h"
#include "PutByIdStatus.h"
#include "StringObject.h"

namespace JSC { namespace DFG {

template<typename AbstractStateType>
AbstractInterpreter<AbstractStateType>::AbstractInterpreter(Graph& graph, AbstractStateType& state)
    : m_codeBlock(graph.m_codeBlock)
    , m_graph(graph)
    , m_state(state)
{
}

template<typename AbstractStateType>
AbstractInterpreter<AbstractStateType>::~AbstractInterpreter()
{
}

template<typename AbstractStateType>
typename AbstractInterpreter<AbstractStateType>::BooleanResult
AbstractInterpreter<AbstractStateType>::booleanResult(
    Node* node, AbstractValue& value)
{
    JSValue childConst = value.value();
    if (childConst) {
        if (childConst.toBoolean(m_codeBlock->globalObjectFor(node->codeOrigin)->globalExec()))
            return DefinitelyTrue;
        return DefinitelyFalse;
    }

    // Next check if we can fold because we know that the source is an object or string and does not equal undefined.
    if (isCellSpeculation(value.m_type)
        && value.m_currentKnownStructure.hasSingleton()) {
        Structure* structure = value.m_currentKnownStructure.singleton();
        if (!structure->masqueradesAsUndefined(m_codeBlock->globalObjectFor(node->codeOrigin))
            && structure->typeInfo().type() != StringType)
            return DefinitelyTrue;
    }
    
    return UnknownBooleanResult;
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::startExecuting(Node* node)
{
    ASSERT(m_state.block());
    ASSERT(m_state.isValid());
    
    m_state.setDidClobber(false);
    
    node->setCanExit(false);
    
    return node->shouldGenerate();
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::startExecuting(unsigned indexInBlock)
{
    return startExecuting(m_state.block()->at(indexInBlock));
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::executeEdges(Node* node)
{
    DFG_NODE_DO_TO_CHILDREN(m_graph, node, filterEdgeByUse);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::executeEdges(unsigned indexInBlock)
{
    executeEdges(m_state.block()->at(indexInBlock));
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::verifyEdge(Node*, Edge edge)
{
    RELEASE_ASSERT(!(forNode(edge).m_type & ~typeFilterFor(edge.useKind())));
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::verifyEdges(Node* node)
{
    DFG_NODE_DO_TO_CHILDREN(m_graph, node, verifyEdge);
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::executeEffects(unsigned clobberLimit, Node* node)
{
    if (!ASSERT_DISABLED)
        verifyEdges(node);
    
    m_state.createValueForNode(node);
    
    switch (node->op()) {
    case JSConstant:
    case WeakJSConstant:
    case PhantomArguments: {
        forNode(node).set(m_graph, m_graph.valueOfJSConstant(node));
        break;
    }
        
    case Identity: {
        forNode(node) = forNode(node->child1());
        break;
    }
        
    case GetArgument: {
        ASSERT(m_graph.m_form == SSA);
        VariableAccessData* variable = node->variableAccessData();
        AbstractValue& value = m_state.variables().operand(variable->local().offset());
        ASSERT(value.isHeapTop());
        FiltrationResult result =
            value.filter(typeFilterFor(useKindFor(variable->flushFormat())));
        ASSERT_UNUSED(result, result == FiltrationOK);
        forNode(node) = value;
        break;
    }
        
    case ExtractOSREntryLocal: {
        if (!(node->unlinkedLocal().isArgument())
            && m_graph.m_lazyVars.get(node->unlinkedLocal().toLocal())) {
            // This is kind of pessimistic - we could know in some cases that the
            // DFG code at the point of the OSR had already initialized the lazy
            // variable. But maybe this is fine, since we're inserting OSR
            // entrypoints very early in the pipeline - so any lazy initializations
            // ought to be hoisted out anyway.
            forNode(node).makeBytecodeTop();
        } else
            forNode(node).makeHeapTop();
        break;
    }
            
    case GetLocal: {
        VariableAccessData* variableAccessData = node->variableAccessData();
        if (variableAccessData->prediction() == SpecNone) {
            m_state.setIsValid(false);
            break;
        }
        AbstractValue value = m_state.variables().operand(variableAccessData->local().offset());
        if (!variableAccessData->isCaptured()) {
            if (value.isClear())
                node->setCanExit(true);
        }
        if (value.value())
            m_state.setFoundConstants(true);
        forNode(node) = value;
        break;
    }
        
    case GetLocalUnlinked: {
        AbstractValue value = m_state.variables().operand(node->unlinkedLocal().offset());
        if (value.value())
            m_state.setFoundConstants(true);
        forNode(node) = value;
        break;
    }
        
    case SetLocal: {
        m_state.variables().operand(node->local().offset()) = forNode(node->child1());
        break;
    }
        
    case MovHint: {
        // Don't need to do anything. A MovHint only informs us about what would have happened
        // in bytecode, but this code is just concerned with what is actually happening during
        // DFG execution.
        break;
    }
        
    case SetArgument:
        // Assert that the state of arguments has been set.
        ASSERT(!m_state.block()->valuesAtHead.operand(node->local()).isClear());
        break;
            
    case BitAnd:
    case BitOr:
    case BitXor:
    case BitRShift:
    case BitLShift:
    case BitURShift: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        if (left && right && left.isInt32() && right.isInt32()) {
            int32_t a = left.asInt32();
            int32_t b = right.asInt32();
            switch (node->op()) {
            case BitAnd:
                setConstant(node, JSValue(a & b));
                break;
            case BitOr:
                setConstant(node, JSValue(a | b));
                break;
            case BitXor:
                setConstant(node, JSValue(a ^ b));
                break;
            case BitRShift:
                setConstant(node, JSValue(a >> static_cast<uint32_t>(b)));
                break;
            case BitLShift:
                setConstant(node, JSValue(a << static_cast<uint32_t>(b)));
                break;
            case BitURShift:
                setConstant(node, JSValue(static_cast<uint32_t>(a) >> static_cast<uint32_t>(b)));
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            break;
        }
        forNode(node).setType(SpecInt32);
        break;
    }
        
    case UInt32ToNumber: {
        JSValue child = forNode(node->child1()).value();
        if (doesOverflow(node->arithMode())) {
            if (child && child.isInt32()) {
                uint32_t value = child.asInt32();
                setConstant(node, jsNumber(value));
                break;
            }
            forNode(node).setType(SpecDouble);
            break;
        }
        if (child && child.isInt32()) {
            int32_t value = child.asInt32();
            if (value >= 0) {
                setConstant(node, jsNumber(value));
                break;
            }
        }
        forNode(node).setType(SpecInt32);
        node->setCanExit(true);
        break;
    }
            
    case DoubleAsInt32: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            double asDouble = child.asNumber();
            int32_t asInt = JSC::toInt32(asDouble);
            if (bitwise_cast<int64_t>(static_cast<double>(asInt)) == bitwise_cast<int64_t>(asDouble)) {
                setConstant(node, JSValue(asInt));
                break;
            }
        }
        node->setCanExit(true);
        forNode(node).setType(SpecInt32);
        break;
    }
            
    case ValueToInt32: {
        JSValue child = forNode(node->child1()).value();
        if (child) {
            if (child.isNumber()) {
                if (child.isInt32())
                    setConstant(node, child);
                else
                    setConstant(node, JSValue(JSC::toInt32(child.asDouble())));
                break;
            }
            if (child.isBoolean()) {
                setConstant(node, JSValue(child.asBoolean()));
                break;
            }
        }
        
        forNode(node).setType(SpecInt32);
        break;
    }
        
    case Int32ToDouble: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, JSValue(JSValue::EncodeAsDouble, child.asNumber()));
            break;
        }
        if (isInt32Speculation(forNode(node->child1()).m_type))
            forNode(node).setType(SpecDoubleReal);
        else
            forNode(node).setType(SpecDouble);
        break;
    }
        
    case Int52ToDouble: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, child);
            break;
        }
        forNode(node).setType(SpecDouble);
        break;
    }
        
    case Int52ToValue: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, child);
            break;
        }
        SpeculatedType type = forNode(node->child1()).m_type;
        if (type & SpecInt52)
            type = (type | SpecInt32 | SpecInt52AsDouble) & ~SpecInt52;
        forNode(node).setType(type);
        break;
    }
        
    case ValueAdd: {
        ASSERT(node->binaryUseKind() == UntypedUse);
        clobberWorld(node->codeOrigin, clobberLimit);
        forNode(node).setType(SpecString | SpecBytecodeNumber);
        break;
    }
        
    case ArithAdd: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        switch (node->binaryUseKind()) {
        case Int32Use:
            if (left && right && left.isInt32() && right.isInt32()) {
                if (!shouldCheckOverflow(node->arithMode())) {
                    setConstant(node, jsNumber(left.asInt32() + right.asInt32()));
                    break;
                }
                JSValue result = jsNumber(left.asNumber() + right.asNumber());
                if (result.isInt32()) {
                    setConstant(node, result);
                    break;
                }
            }
            forNode(node).setType(SpecInt32);
            if (shouldCheckOverflow(node->arithMode()))
                node->setCanExit(true);
            break;
        case MachineIntUse:
            if (left && right && left.isMachineInt() && right.isMachineInt()) {
                JSValue result = jsNumber(left.asMachineInt() + right.asMachineInt());
                if (result.isMachineInt()) {
                    setConstant(node, result);
                    break;
                }
            }
            forNode(node).setType(SpecInt52);
            if (!forNode(node->child1()).isType(SpecInt32)
                || !forNode(node->child2()).isType(SpecInt32))
                node->setCanExit(true);
            break;
        case NumberUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsNumber(left.asNumber() + right.asNumber()));
                break;
            }
            if (isFullRealNumberSpeculation(forNode(node->child1()).m_type)
                && isFullRealNumberSpeculation(forNode(node->child2()).m_type))
                forNode(node).setType(SpecDoubleReal);
            else
                forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
        
    case MakeRope: {
        forNode(node).set(m_graph, m_graph.m_vm.stringStructure.get());
        break;
    }
            
    case ArithSub: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        switch (node->binaryUseKind()) {
        case Int32Use:
            if (left && right && left.isInt32() && right.isInt32()) {
                if (!shouldCheckOverflow(node->arithMode())) {
                    setConstant(node, jsNumber(left.asInt32() - right.asInt32()));
                    break;
                }
                JSValue result = jsNumber(left.asNumber() - right.asNumber());
                if (result.isInt32()) {
                    setConstant(node, result);
                    break;
                }
            }
            forNode(node).setType(SpecInt32);
            if (shouldCheckOverflow(node->arithMode()))
                node->setCanExit(true);
            break;
        case MachineIntUse:
            if (left && right && left.isMachineInt() && right.isMachineInt()) {
                JSValue result = jsNumber(left.asMachineInt() - right.asMachineInt());
                if (result.isMachineInt() || !shouldCheckOverflow(node->arithMode())) {
                    setConstant(node, result);
                    break;
                }
            }
            forNode(node).setType(SpecInt52);
            if (!forNode(node->child1()).isType(SpecInt32)
                || !forNode(node->child2()).isType(SpecInt32))
                node->setCanExit(true);
            break;
        case NumberUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsNumber(left.asNumber() - right.asNumber()));
                break;
            }
            forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
        
    case ArithNegate: {
        JSValue child = forNode(node->child1()).value();
        switch (node->child1().useKind()) {
        case Int32Use:
            if (child && child.isInt32()) {
                if (!shouldCheckOverflow(node->arithMode())) {
                    setConstant(node, jsNumber(-child.asInt32()));
                    break;
                }
                double doubleResult;
                if (shouldCheckNegativeZero(node->arithMode()))
                    doubleResult = -child.asNumber();
                else
                    doubleResult = 0 - child.asNumber();
                JSValue valueResult = jsNumber(doubleResult);
                if (valueResult.isInt32()) {
                    setConstant(node, valueResult);
                    break;
                }
            }
            forNode(node).setType(SpecInt32);
            if (shouldCheckOverflow(node->arithMode()))
                node->setCanExit(true);
            break;
        case MachineIntUse:
            if (child && child.isMachineInt()) {
                double doubleResult;
                if (shouldCheckNegativeZero(node->arithMode()))
                    doubleResult = -child.asNumber();
                else
                    doubleResult = 0 - child.asNumber();
                JSValue valueResult = jsNumber(doubleResult);
                if (valueResult.isMachineInt()) {
                    setConstant(node, valueResult);
                    break;
                }
            }
            forNode(node).setType(SpecInt52);
            if (m_state.forNode(node->child1()).couldBeType(SpecInt52))
                node->setCanExit(true);
            if (shouldCheckNegativeZero(node->arithMode()))
                node->setCanExit(true);
            break;
        case NumberUse:
            if (child && child.isNumber()) {
                setConstant(node, jsNumber(-child.asNumber()));
                break;
            }
            forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
        
    case ArithMul: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        switch (node->binaryUseKind()) {
        case Int32Use:
            if (left && right && left.isInt32() && right.isInt32()) {
                if (!shouldCheckOverflow(node->arithMode())) {
                    setConstant(node, jsNumber(left.asInt32() * right.asInt32()));
                    break;
                }
                double doubleResult = left.asNumber() * right.asNumber();
                if (!shouldCheckNegativeZero(node->arithMode()))
                    doubleResult += 0; // Sanitizes zero.
                JSValue valueResult = jsNumber(doubleResult);
                if (valueResult.isInt32()) {
                    setConstant(node, valueResult);
                    break;
                }
            }
            forNode(node).setType(SpecInt32);
            if (shouldCheckOverflow(node->arithMode()))
                node->setCanExit(true);
            break;
        case MachineIntUse:
            if (left && right && left.isMachineInt() && right.isMachineInt()) {
                double doubleResult = left.asNumber() * right.asNumber();
                if (!shouldCheckNegativeZero(node->arithMode()))
                    doubleResult += 0;
                JSValue valueResult = jsNumber(doubleResult);
                if (valueResult.isMachineInt()) {
                    setConstant(node, valueResult);
                    break;
                }
            }
            forNode(node).setType(SpecInt52);
            node->setCanExit(true);
            break;
        case NumberUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsNumber(left.asNumber() * right.asNumber()));
                break;
            }
            if (isFullRealNumberSpeculation(forNode(node->child1()).m_type)
                || isFullRealNumberSpeculation(forNode(node->child2()).m_type))
                forNode(node).setType(SpecDoubleReal);
            else
                forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
        
    case ArithDiv: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        switch (node->binaryUseKind()) {
        case Int32Use:
            if (left && right && left.isInt32() && right.isInt32()) {
                double doubleResult = left.asNumber() / right.asNumber();
                if (!shouldCheckOverflow(node->arithMode()))
                    doubleResult = toInt32(doubleResult);
                else if (!shouldCheckNegativeZero(node->arithMode()))
                    doubleResult += 0; // Sanitizes zero.
                JSValue valueResult = jsNumber(doubleResult);
                if (valueResult.isInt32()) {
                    setConstant(node, valueResult);
                    break;
                }
            }
            forNode(node).setType(SpecInt32);
            node->setCanExit(true);
            break;
        case NumberUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsNumber(left.asNumber() / right.asNumber()));
                break;
            }
            forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }

    case ArithMod: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        switch (node->binaryUseKind()) {
        case Int32Use:
            if (left && right && left.isInt32() && right.isInt32()) {
                double doubleResult = fmod(left.asNumber(), right.asNumber());
                if (!shouldCheckOverflow(node->arithMode()))
                    doubleResult = toInt32(doubleResult);
                else if (!shouldCheckNegativeZero(node->arithMode()))
                    doubleResult += 0; // Sanitizes zero.
                JSValue valueResult = jsNumber(doubleResult);
                if (valueResult.isInt32()) {
                    setConstant(node, valueResult);
                    break;
                }
            }
            forNode(node).setType(SpecInt32);
            node->setCanExit(true);
            break;
        case NumberUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsNumber(fmod(left.asNumber(), right.asNumber())));
                break;
            }
            forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }

    case ArithMin: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        switch (node->binaryUseKind()) {
        case Int32Use:
            if (left && right && left.isInt32() && right.isInt32()) {
                setConstant(node, jsNumber(std::min(left.asInt32(), right.asInt32())));
                break;
            }
            forNode(node).setType(SpecInt32);
            node->setCanExit(true);
            break;
        case NumberUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                double a = left.asNumber();
                double b = right.asNumber();
                setConstant(node, jsNumber(a < b ? a : (b <= a ? b : a + b)));
                break;
            }
            forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
            
    case ArithMax: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        switch (node->binaryUseKind()) {
        case Int32Use:
            if (left && right && left.isInt32() && right.isInt32()) {
                setConstant(node, jsNumber(std::max(left.asInt32(), right.asInt32())));
                break;
            }
            forNode(node).setType(SpecInt32);
            node->setCanExit(true);
            break;
        case NumberUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                double a = left.asNumber();
                double b = right.asNumber();
                setConstant(node, jsNumber(a > b ? a : (b >= a ? b : a + b)));
                break;
            }
            forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
            
    case ArithAbs: {
        JSValue child = forNode(node->child1()).value();
        switch (node->child1().useKind()) {
        case Int32Use:
            if (child && child.isInt32()) {
                JSValue result = jsNumber(fabs(child.asNumber()));
                if (result.isInt32()) {
                    setConstant(node, result);
                    break;
                }
            }
            forNode(node).setType(SpecInt32);
            node->setCanExit(true);
            break;
        case NumberUse:
            if (child && child.isNumber()) {
                setConstant(node, jsNumber(child.asNumber()));
                break;
            }
            forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
            
    case ArithSqrt: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, jsNumber(sqrt(child.asNumber())));
            break;
        }
        forNode(node).setType(SpecDouble);
        break;
    }
        
    case ArithSin: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, jsNumber(sin(child.asNumber())));
            break;
        }
        forNode(node).setType(SpecDouble);
        break;
    }
    
    case ArithCos: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, jsNumber(cos(child.asNumber())));
            break;
        }
        forNode(node).setType(SpecDouble);
        break;
    }
            
    case LogicalNot: {
        switch (booleanResult(node, forNode(node->child1()))) {
        case DefinitelyTrue:
            setConstant(node, jsBoolean(false));
            break;
        case DefinitelyFalse:
            setConstant(node, jsBoolean(true));
            break;
        default:
            switch (node->child1().useKind()) {
            case BooleanUse:
            case Int32Use:
            case NumberUse:
            case UntypedUse:
            case StringUse:
                break;
            case ObjectOrOtherUse:
                node->setCanExit(true);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            forNode(node).setType(SpecBoolean);
            break;
        }
        break;
    }
        
    case IsUndefined:
    case IsBoolean:
    case IsNumber:
    case IsString:
    case IsObject:
    case IsFunction: {
        node->setCanExit(
            node->op() == IsUndefined
            && m_graph.masqueradesAsUndefinedWatchpointIsStillValid(node->codeOrigin));
        JSValue child = forNode(node->child1()).value();
        if (child) {
            bool constantWasSet = true;
            switch (node->op()) {
            case IsUndefined:
                setConstant(node, jsBoolean(
                    child.isCell()
                    ? child.asCell()->structure()->masqueradesAsUndefined(m_codeBlock->globalObjectFor(node->codeOrigin))
                    : child.isUndefined()));
                break;
            case IsBoolean:
                setConstant(node, jsBoolean(child.isBoolean()));
                break;
            case IsNumber:
                setConstant(node, jsBoolean(child.isNumber()));
                break;
            case IsString:
                setConstant(node, jsBoolean(isJSString(child)));
                break;
            case IsObject:
                if (child.isNull() || !child.isObject()) {
                    setConstant(node, jsBoolean(child.isNull()));
                    break;
                }
                constantWasSet = false;
                break;
            default:
                constantWasSet = false;
                break;
            }
            if (constantWasSet)
                break;
        }

        forNode(node).setType(SpecBoolean);
        break;
    }

    case TypeOf: {
        VM* vm = m_codeBlock->vm();
        JSValue child = forNode(node->child1()).value();
        AbstractValue& abstractChild = forNode(node->child1());
        if (child) {
            JSValue typeString = jsTypeStringForValue(*vm, m_codeBlock->globalObjectFor(node->codeOrigin), child);
            setConstant(node, typeString);
            break;
        }
        
        if (isFullNumberSpeculation(abstractChild.m_type)) {
            setConstant(node, vm->smallStrings.numberString());
            break;
        }
        
        if (isStringSpeculation(abstractChild.m_type)) {
            setConstant(node, vm->smallStrings.stringString());
            break;
        }
        
        if (isFinalObjectSpeculation(abstractChild.m_type) || isArraySpeculation(abstractChild.m_type) || isArgumentsSpeculation(abstractChild.m_type)) {
            setConstant(node, vm->smallStrings.objectString());
            break;
        }
        
        if (isFunctionSpeculation(abstractChild.m_type)) {
            setConstant(node, vm->smallStrings.functionString());
            break;
        }
        
        if (isBooleanSpeculation(abstractChild.m_type)) {
            setConstant(node, vm->smallStrings.booleanString());
            break;
        }

        switch (node->child1().useKind()) {
        case StringUse:
        case CellUse:
            node->setCanExit(true);
            break;
        case UntypedUse:
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        forNode(node).set(m_graph, m_graph.m_vm.stringStructure.get());
        break;
    }
            
    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq:
    case CompareEqConstant: {
        JSValue leftConst = forNode(node->child1()).value();
        JSValue rightConst = forNode(node->child2()).value();
        if (leftConst && rightConst) {
            if (leftConst.isNumber() && rightConst.isNumber()) {
                double a = leftConst.asNumber();
                double b = rightConst.asNumber();
                switch (node->op()) {
                case CompareLess:
                    setConstant(node, jsBoolean(a < b));
                    break;
                case CompareLessEq:
                    setConstant(node, jsBoolean(a <= b));
                    break;
                case CompareGreater:
                    setConstant(node, jsBoolean(a > b));
                    break;
                case CompareGreaterEq:
                    setConstant(node, jsBoolean(a >= b));
                    break;
                case CompareEq:
                    setConstant(node, jsBoolean(a == b));
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                break;
            }
            
            if (node->op() == CompareEq && leftConst.isString() && rightConst.isString()) {
                const StringImpl* a = asString(leftConst)->tryGetValueImpl();
                const StringImpl* b = asString(rightConst)->tryGetValueImpl();
                if (a && b) {
                    setConstant(node, jsBoolean(WTF::equal(a, b)));
                    break;
                }
            }
        }
        
        if (node->op() == CompareEqConstant || node->op() == CompareEq) {
            SpeculatedType leftType = forNode(node->child1()).m_type;
            SpeculatedType rightType = forNode(node->child2()).m_type;
            if ((isInt32Speculation(leftType) && isOtherSpeculation(rightType))
                || (isOtherSpeculation(leftType) && isInt32Speculation(rightType))) {
                setConstant(node, jsBoolean(false));
                break;
            }
        }
        
        forNode(node).setType(SpecBoolean);
        
        // This is overly conservative. But the only thing this prevents is store elimination,
        // and how likely is it, really, that you'll have redundant stores across a comparison
        // operation? Comparison operations are typically at the end of basic blocks, so
        // unless we have global store elimination (super unlikely given how unprofitable that
        // optimization is to begin with), you aren't going to be wanting to store eliminate
        // across an equality op.
        node->setCanExit(true);
        break;
    }
            
    case CompareStrictEq:
    case CompareStrictEqConstant: {
        Node* leftNode = node->child1().node();
        Node* rightNode = node->child2().node();
        JSValue left = forNode(leftNode).value();
        JSValue right = forNode(rightNode).value();
        if (left && right) {
            if (left.isNumber() && right.isNumber()) {
                setConstant(node, jsBoolean(left.asNumber() == right.asNumber()));
                break;
            }
            if (left.isString() && right.isString()) {
                const StringImpl* a = asString(left)->tryGetValueImpl();
                const StringImpl* b = asString(right)->tryGetValueImpl();
                if (a && b) {
                    setConstant(node, jsBoolean(WTF::equal(a, b)));
                    break;
                }
            }
        }
        forNode(node).setType(SpecBoolean);
        node->setCanExit(true); // This is overly conservative.
        break;
    }
        
    case StringCharCodeAt:
        node->setCanExit(true);
        forNode(node).setType(SpecInt32);
        break;
        
    case StringFromCharCode:
        forNode(node).setType(SpecString);
        break;

    case StringCharAt:
        node->setCanExit(true);
        forNode(node).set(m_graph, m_graph.m_vm.stringStructure.get());
        break;
            
    case GetByVal: {
        node->setCanExit(true);
        switch (node->arrayMode().type()) {
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
        case Array::Undecided:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case Array::ForceExit:
            m_state.setIsValid(false);
            break;
        case Array::Generic:
            clobberWorld(node->codeOrigin, clobberLimit);
            forNode(node).makeHeapTop();
            break;
        case Array::String:
            if (node->arrayMode().isOutOfBounds()) {
                // If the watchpoint was still valid we could totally set this to be
                // SpecString | SpecOther. Except that we'd have to be careful. If we
                // tested the watchpoint state here then it could change by the time
                // we got to the backend. So to do this right, we'd have to get the
                // fixup phase to check the watchpoint state and then bake into the
                // GetByVal operation the fact that we're using a watchpoint, using
                // something like Array::SaneChain (except not quite, because that
                // implies an in-bounds access). None of this feels like it's worth it,
                // so we're going with TOP for now. The same thing applies to
                // clobbering the world.
                clobberWorld(node->codeOrigin, clobberLimit);
                forNode(node).makeHeapTop();
            } else
                forNode(node).set(m_graph, m_graph.m_vm.stringStructure.get());
            break;
        case Array::Arguments:
            forNode(node).makeHeapTop();
            break;
        case Array::Int32:
            if (node->arrayMode().isOutOfBounds()) {
                clobberWorld(node->codeOrigin, clobberLimit);
                forNode(node).makeHeapTop();
            } else
                forNode(node).setType(SpecInt32);
            break;
        case Array::Double:
            if (node->arrayMode().isOutOfBounds()) {
                clobberWorld(node->codeOrigin, clobberLimit);
                forNode(node).makeHeapTop();
            } else if (node->arrayMode().isSaneChain())
                forNode(node).setType(SpecDouble);
            else
                forNode(node).setType(SpecDoubleReal);
            break;
        case Array::Contiguous:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld(node->codeOrigin, clobberLimit);
            forNode(node).makeHeapTop();
            break;
        case Array::Int8Array:
            forNode(node).setType(SpecInt32);
            break;
        case Array::Int16Array:
            forNode(node).setType(SpecInt32);
            break;
        case Array::Int32Array:
            forNode(node).setType(SpecInt32);
            break;
        case Array::Uint8Array:
            forNode(node).setType(SpecInt32);
            break;
        case Array::Uint8ClampedArray:
            forNode(node).setType(SpecInt32);
            break;
        case Array::Uint16Array:
            forNode(node).setType(SpecInt32);
            break;
        case Array::Uint32Array:
            if (node->shouldSpeculateInt32())
                forNode(node).setType(SpecInt32);
            else if (enableInt52() && node->shouldSpeculateMachineInt())
                forNode(node).setType(SpecInt52);
            else
                forNode(node).setType(SpecDouble);
            break;
        case Array::Float32Array:
            forNode(node).setType(SpecDouble);
            break;
        case Array::Float64Array:
            forNode(node).setType(SpecDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
            
    case PutByValDirect:
    case PutByVal:
    case PutByValAlias: {
        node->setCanExit(true);
        switch (node->arrayMode().modeForPut().type()) {
        case Array::ForceExit:
            m_state.setIsValid(false);
            break;
        case Array::Generic:
            clobberWorld(node->codeOrigin, clobberLimit);
            break;
        case Array::Int32:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld(node->codeOrigin, clobberLimit);
            break;
        case Array::Double:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld(node->codeOrigin, clobberLimit);
            break;
        case Array::Contiguous:
        case Array::ArrayStorage:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld(node->codeOrigin, clobberLimit);
            break;
        case Array::SlowPutArrayStorage:
            if (node->arrayMode().mayStoreToHole())
                clobberWorld(node->codeOrigin, clobberLimit);
            break;
        default:
            break;
        }
        break;
    }
            
    case ArrayPush:
        node->setCanExit(true);
        clobberWorld(node->codeOrigin, clobberLimit);
        forNode(node).setType(SpecBytecodeNumber);
        break;
            
    case ArrayPop:
        node->setCanExit(true);
        clobberWorld(node->codeOrigin, clobberLimit);
        forNode(node).makeHeapTop();
        break;
            
    case RegExpExec:
        forNode(node).makeHeapTop();
        break;

    case RegExpTest:
        forNode(node).setType(SpecBoolean);
        break;
            
    case Jump:
        break;
            
    case Branch: {
        Node* child = node->child1().node();
        BooleanResult result = booleanResult(node, forNode(child));
        if (result == DefinitelyTrue) {
            m_state.setBranchDirection(TakeTrue);
            break;
        }
        if (result == DefinitelyFalse) {
            m_state.setBranchDirection(TakeFalse);
            break;
        }
        // FIXME: The above handles the trivial cases of sparse conditional
        // constant propagation, but we can do better:
        // We can specialize the source variable's value on each direction of
        // the branch.
        node->setCanExit(true); // This is overly conservative.
        m_state.setBranchDirection(TakeBoth);
        break;
    }
        
    case Switch: {
        // Nothing to do for now.
        // FIXME: Do sparse conditional things.
        break;
    }
            
    case Return:
        m_state.setIsValid(false);
        break;
        
    case Throw:
    case ThrowReferenceError:
        m_state.setIsValid(false);
        node->setCanExit(true);
        break;
            
    case ToPrimitive: {
        JSValue childConst = forNode(node->child1()).value();
        if (childConst && childConst.isNumber()) {
            setConstant(node, childConst);
            break;
        }
        
        ASSERT(node->child1().useKind() == UntypedUse);
        
        AbstractValue& source = forNode(node->child1());
        AbstractValue& destination = forNode(node);
        
        // NB. The more canonical way of writing this would have been:
        //
        // destination = source;
        // if (destination.m_type & !(SpecFullNumber | SpecString | SpecBoolean)) {
        //     destination.filter(SpecFullNumber | SpecString | SpecBoolean);
        //     AbstractValue string;
        //     string.set(vm->stringStructure);
        //     destination.merge(string);
        // }
        //
        // The reason why this would, in most other cases, have been better is that
        // then destination would preserve any non-SpeculatedType knowledge of source.
        // As it stands, the code below forgets any non-SpeculatedType knowledge that
        // source would have had. Fortunately, though, for things like strings and
        // numbers and booleans, we don't care about the non-SpeculatedType knowedge:
        // the structure won't tell us anything we don't already know, and neither
        // will ArrayModes. And if the source was a meaningful constant then we
        // would have handled that above. Unfortunately, this does mean that
        // ToPrimitive will currently forget string constants. But that's not a big
        // deal since we don't do any optimization on those currently.
        
        clobberWorld(node->codeOrigin, clobberLimit);
        
        SpeculatedType type = source.m_type;
        if (type & ~(SpecFullNumber | SpecString | SpecBoolean))
            type = (SpecHeapTop & ~SpecCell) | SpecString;

        destination.setType(type);
        if (destination.isClear())
            m_state.setIsValid(false);
        break;
    }
        
    case ToString: {
        switch (node->child1().useKind()) {
        case StringObjectUse:
            // This also filters that the StringObject has the primordial StringObject
            // structure.
            filter(
                node->child1(),
                m_graph.globalObjectFor(node->codeOrigin)->stringObjectStructure());
            node->setCanExit(true); // We could be more precise but it's likely not worth it.
            break;
        case StringOrStringObjectUse:
            node->setCanExit(true); // We could be more precise but it's likely not worth it.
            break;
        case CellUse:
        case UntypedUse:
            clobberWorld(node->codeOrigin, clobberLimit);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        forNode(node).set(m_graph, m_graph.m_vm.stringStructure.get());
        break;
    }
        
    case NewStringObject: {
        ASSERT(node->structure()->classInfo() == StringObject::info());
        forNode(node).set(m_graph, node->structure());
        break;
    }
            
    case NewArray:
        node->setCanExit(true);
        forNode(node).set(
            m_graph,
            m_graph.globalObjectFor(node->codeOrigin)->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()));
        m_state.setHaveStructures(true);
        break;
        
    case NewArrayBuffer:
        node->setCanExit(true);
        forNode(node).set(
            m_graph,
            m_graph.globalObjectFor(node->codeOrigin)->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()));
        m_state.setHaveStructures(true);
        break;

    case NewArrayWithSize:
        node->setCanExit(true);
        forNode(node).setType(SpecArray);
        m_state.setHaveStructures(true);
        break;
        
    case NewTypedArray:
        switch (node->child1().useKind()) {
        case Int32Use:
            break;
        case UntypedUse:
            clobberWorld(node->codeOrigin, clobberLimit);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        forNode(node).set(
            m_graph,
            m_graph.globalObjectFor(node->codeOrigin)->typedArrayStructure(
                node->typedArrayType()));
        m_state.setHaveStructures(true);
        break;
            
    case NewRegexp:
        forNode(node).set(m_graph, m_graph.globalObjectFor(node->codeOrigin)->regExpStructure());
        m_state.setHaveStructures(true);
        break;
            
    case ToThis: {
        AbstractValue& source = forNode(node->child1());
        AbstractValue& destination = forNode(node);
            
        if (m_graph.executableFor(node->codeOrigin)->isStrictMode())
            destination.makeHeapTop();
        else {
            destination = source;
            destination.merge(SpecObject);
        }
        break;
    }

    case CreateThis: {
        forNode(node).setType(SpecFinalObject);
        break;
    }
        
    case AllocationProfileWatchpoint:
        node->setCanExit(true);
        break;

    case NewObject:
        ASSERT(node->structure());
        forNode(node).set(m_graph, node->structure());
        m_state.setHaveStructures(true);
        break;
        
    case CreateActivation:
        forNode(node).set(
            m_graph, m_codeBlock->globalObjectFor(node->codeOrigin)->activationStructure());
        m_state.setHaveStructures(true);
        break;
        
    case FunctionReentryWatchpoint:
    case TypedArrayWatchpoint:
        break;
    
    case CreateArguments:
        forNode(node) = forNode(node->child1());
        forNode(node).filter(~SpecEmpty);
        forNode(node).merge(SpecArguments);
        break;
        
    case TearOffActivation:
    case TearOffArguments:
        // Does nothing that is user-visible.
        break;

    case CheckArgumentsNotCreated:
        if (isEmptySpeculation(
                m_state.variables().operand(
                    m_graph.argumentsRegisterFor(node->codeOrigin).offset()).m_type))
            m_state.setFoundConstants(true);
        else
            node->setCanExit(true);
        break;
        
    case GetMyArgumentsLength:
        // We know that this executable does not escape its arguments, so we can optimize
        // the arguments a bit. Note that this is not sufficient to force constant folding
        // of GetMyArgumentsLength, because GetMyArgumentsLength is a clobbering operation.
        // We perform further optimizations on this later on.
        if (node->codeOrigin.inlineCallFrame) {
            forNode(node).set(
                m_graph, jsNumber(node->codeOrigin.inlineCallFrame->arguments.size() - 1));
        } else
            forNode(node).setType(SpecInt32);
        node->setCanExit(
            !isEmptySpeculation(
                m_state.variables().operand(
                    m_graph.argumentsRegisterFor(node->codeOrigin)).m_type));
        break;
        
    case GetMyArgumentsLengthSafe:
        // This potentially clobbers all structures if the arguments object had a getter
        // installed on the length property.
        clobberWorld(node->codeOrigin, clobberLimit);
        // We currently make no guarantee about what this returns because it does not
        // speculate that the length property is actually a length.
        forNode(node).makeHeapTop();
        break;
        
    case GetMyArgumentByVal:
        node->setCanExit(true);
        // We know that this executable does not escape its arguments, so we can optimize
        // the arguments a bit. Note that this ends up being further optimized by the
        // ArgumentsSimplificationPhase.
        forNode(node).makeHeapTop();
        break;
        
    case GetMyArgumentByValSafe:
        node->setCanExit(true);
        // This potentially clobbers all structures if the property we're accessing has
        // a getter. We don't speculate against this.
        clobberWorld(node->codeOrigin, clobberLimit);
        // And the result is unknown.
        forNode(node).makeHeapTop();
        break;
        
    case NewFunction: {
        AbstractValue& value = forNode(node);
        value = forNode(node->child1());
        
        if (!(value.m_type & SpecEmpty)) {
            m_state.setFoundConstants(true);
            break;
        }

        value.setType((value.m_type & ~SpecEmpty) | SpecFunction);
        break;
    }

    case NewFunctionExpression:
    case NewFunctionNoCheck:
        forNode(node).set(
            m_graph, m_codeBlock->globalObjectFor(node->codeOrigin)->functionStructure());
        break;
        
    case GetCallee:
        forNode(node).setType(SpecFunction);
        break;
        
    case GetScope: // FIXME: We could get rid of these if we know that the JSFunction is a constant. https://bugs.webkit.org/show_bug.cgi?id=106202
    case GetMyScope:
    case SkipTopScope:
        forNode(node).setType(SpecObjectOther);
        break;

    case SkipScope: {
        JSValue child = forNode(node->child1()).value();
        if (child) {
            setConstant(node, JSValue(jsCast<JSScope*>(child.asCell())->next()));
            break;
        }
        forNode(node).setType(SpecObjectOther);
        break;
    }

    case GetClosureRegisters:
        forNode(node).clear(); // The result is not a JS value.
        break;

    case GetClosureVar:
        forNode(node).makeHeapTop();
        break;
            
    case PutClosureVar:
        clobberCapturedVars(node->codeOrigin);
        break;
            
    case GetById:
    case GetByIdFlush:
        node->setCanExit(true);
        if (!node->prediction()) {
            m_state.setIsValid(false);
            break;
        }
        if (isCellSpeculation(node->child1()->prediction())) {
            if (Structure* structure = forNode(node->child1()).bestProvenStructure()) {
                GetByIdStatus status = GetByIdStatus::computeFor(
                    m_graph.m_vm, structure,
                    m_graph.identifiers()[node->identifierNumber()]);
                if (status.isSimple()) {
                    // Assert things that we can't handle and that the computeFor() method
                    // above won't be able to return.
                    ASSERT(status.structureSet().size() == 1);
                    ASSERT(!status.chain());
                    
                    if (status.specificValue())
                        setConstant(node, status.specificValue());
                    else
                        forNode(node).makeHeapTop();
                    filter(node->child1(), status.structureSet());
                    
                    m_state.setFoundConstants(true);
                    m_state.setHaveStructures(true);
                    break;
                }
            }
        }
        clobberWorld(node->codeOrigin, clobberLimit);
        forNode(node).makeHeapTop();
        break;
            
    case GetArrayLength:
        node->setCanExit(true); // Lies, but it's true for the common case of JSArray, so it's good enough.
        forNode(node).setType(SpecInt32);
        break;
        
    case CheckExecutable: {
        // FIXME: We could track executables in AbstractValue, which would allow us to get rid of these checks
        // more thoroughly. https://bugs.webkit.org/show_bug.cgi?id=106200
        // FIXME: We could eliminate these entirely if we know the exact value that flows into this.
        // https://bugs.webkit.org/show_bug.cgi?id=106201
        node->setCanExit(true);
        break;
    }

    case CheckStructure: {
        // FIXME: We should be able to propagate the structure sets of constants (i.e. prototypes).
        AbstractValue& value = forNode(node->child1());
        ASSERT(!(value.m_type & ~SpecCell)); // Edge filtering should have already ensured this.

        StructureSet& set = node->structureSet();

        if (value.m_currentKnownStructure.isSubsetOf(set)) {
            m_state.setFoundConstants(true);
            break;
        }

        node->setCanExit(true);
        m_state.setHaveStructures(true);

        // If this structure check is attempting to prove knowledge already held in
        // the futurePossibleStructure set then the constant folding phase should
        // turn this into a watchpoint instead.
        if (value.m_futurePossibleStructure.isSubsetOf(set)
            && value.m_futurePossibleStructure.hasSingleton()) {
            m_state.setFoundConstants(true);
            filter(value, value.m_futurePossibleStructure.singleton());
            break;
        }

        filter(value, set);
        break;
    }
        
    case StructureTransitionWatchpoint: {
        AbstractValue& value = forNode(node->child1());

        filter(value, node->structure());
        m_state.setHaveStructures(true);
        node->setCanExit(true);
        break;
    }
            
    case PutStructure:
    case PhantomPutStructure:
        if (!forNode(node->child1()).m_currentKnownStructure.isClear()) {
            clobberStructures(clobberLimit);
            forNode(node->child1()).set(m_graph, node->structureTransitionData().newStructure);
            m_state.setHaveStructures(true);
        }
        break;
    case GetButterfly:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
        forNode(node).clear(); // The result is not a JS value.
        break;
    case CheckArray: {
        if (node->arrayMode().alreadyChecked(m_graph, node, forNode(node->child1()))) {
            m_state.setFoundConstants(true);
            break;
        }
        node->setCanExit(true); // Lies, but this is followed by operations (like GetByVal) that always exit, so there is no point in us trying to be clever here.
        switch (node->arrayMode().type()) {
        case Array::String:
            filter(node->child1(), SpecString);
            break;
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            break;
        case Array::Arguments:
            filter(node->child1(), SpecArguments);
            break;
        case Array::Int8Array:
            filter(node->child1(), SpecInt8Array);
            break;
        case Array::Int16Array:
            filter(node->child1(), SpecInt16Array);
            break;
        case Array::Int32Array:
            filter(node->child1(), SpecInt32Array);
            break;
        case Array::Uint8Array:
            filter(node->child1(), SpecUint8Array);
            break;
        case Array::Uint8ClampedArray:
            filter(node->child1(), SpecUint8ClampedArray);
            break;
        case Array::Uint16Array:
            filter(node->child1(), SpecUint16Array);
            break;
        case Array::Uint32Array:
            filter(node->child1(), SpecUint32Array);
            break;
        case Array::Float32Array:
            filter(node->child1(), SpecFloat32Array);
            break;
        case Array::Float64Array:
            filter(node->child1(), SpecFloat64Array);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        filterArrayModes(node->child1(), node->arrayMode().arrayModesThatPassFiltering());
        m_state.setHaveStructures(true);
        break;
    }
    case Arrayify: {
        if (node->arrayMode().alreadyChecked(m_graph, node, forNode(node->child1()))) {
            m_state.setFoundConstants(true);
            break;
        }
        ASSERT(node->arrayMode().conversion() == Array::Convert
            || node->arrayMode().conversion() == Array::RageConvert);
        node->setCanExit(true);
        clobberStructures(clobberLimit);
        filterArrayModes(node->child1(), node->arrayMode().arrayModesThatPassFiltering());
        m_state.setHaveStructures(true);
        break;
    }
    case ArrayifyToStructure: {
        AbstractValue& value = forNode(node->child1());
        StructureSet set = node->structure();
        if (value.m_futurePossibleStructure.isSubsetOf(set)
            || value.m_currentKnownStructure.isSubsetOf(set))
            m_state.setFoundConstants(true);
        node->setCanExit(true);
        clobberStructures(clobberLimit);
        filter(value, set);
        m_state.setHaveStructures(true);
        break;
    }
    case GetIndexedPropertyStorage:
    case ConstantStoragePointer: {
        forNode(node).clear();
        break; 
    }
        
    case GetTypedArrayByteOffset: {
        forNode(node).setType(SpecInt32);
        break;
    }
        
    case GetByOffset: {
        forNode(node).makeHeapTop();
        break;
    }
            
    case PutByOffset: {
        break;
    }
            
    case CheckFunction: {
        JSValue value = forNode(node->child1()).value();
        if (value == node->function()) {
            m_state.setFoundConstants(true);
            ASSERT(value);
            break;
        }
        
        node->setCanExit(true); // Lies! We can do better.
        filterByValue(node->child1(), node->function());
        break;
    }
        
    case CheckInBounds: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        if (left && right && left.isInt32() && right.isInt32()
            && static_cast<uint32_t>(left.asInt32()) < static_cast<uint32_t>(right.asInt32())) {
            m_state.setFoundConstants(true);
            break;
        }
        
        node->setCanExit(true);
        break;
    }
        
    case PutById:
    case PutByIdDirect:
        node->setCanExit(true);
        if (Structure* structure = forNode(node->child1()).bestProvenStructure()) {
            PutByIdStatus status = PutByIdStatus::computeFor(
                m_graph.m_vm,
                m_graph.globalObjectFor(node->codeOrigin),
                structure,
                m_graph.identifiers()[node->identifierNumber()],
                node->op() == PutByIdDirect);
            if (status.isSimpleReplace()) {
                filter(node->child1(), structure);
                m_state.setFoundConstants(true);
                m_state.setHaveStructures(true);
                break;
            }
            if (status.isSimpleTransition()) {
                clobberStructures(clobberLimit);
                forNode(node->child1()).set(m_graph, status.newStructure());
                m_state.setHaveStructures(true);
                m_state.setFoundConstants(true);
                break;
            }
        }
        clobberWorld(node->codeOrigin, clobberLimit);
        break;
        
    case In:
        // FIXME: We can determine when the property definitely exists based on abstract
        // value information.
        clobberWorld(node->codeOrigin, clobberLimit);
        forNode(node).setType(SpecBoolean);
        break;
            
    case GetGlobalVar:
        forNode(node).makeHeapTop();
        break;
        
    case VariableWatchpoint:
    case VarInjectionWatchpoint:
        node->setCanExit(true);
        break;
            
    case PutGlobalVar:
    case NotifyWrite:
        break;
            
    case CheckHasInstance:
        node->setCanExit(true);
        // Sadly, we don't propagate the fact that we've done CheckHasInstance
        break;
            
    case InstanceOf:
        node->setCanExit(true);
        // Again, sadly, we don't propagate the fact that we've done InstanceOf
        forNode(node).setType(SpecBoolean);
        break;
            
    case Phi:
        RELEASE_ASSERT(m_graph.m_form == SSA);
        // The state of this node would have already been decided.
        break;
        
    case Upsilon: {
        m_state.createValueForNode(node->phi());
        AbstractValue& value = forNode(node->child1());
        forNode(node) = value;
        forNode(node->phi()) = value;
        break;
    }
        
    case Flush:
    case PhantomLocal:
        break;
            
    case Call:
    case Construct:
        node->setCanExit(true);
        clobberWorld(node->codeOrigin, clobberLimit);
        forNode(node).makeHeapTop();
        break;

    case ForceOSRExit:
        node->setCanExit(true);
        m_state.setIsValid(false);
        break;
        
    case InvalidationPoint:
        node->setCanExit(true);
        break;

    case Breakpoint:
    case ProfileWillCall:
    case ProfileDidCall:
    case CheckWatchdogTimer:
        node->setCanExit(true);
        break;

    case Phantom:
    case Check:
    case CountExecution:
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
        break;

    case ConditionalStoreBarrier: {
        if (!needsTypeCheck(node->child2().node(), ~SpecCell))
            m_state.setFoundConstants(true);
        filter(node->child1(), SpecCell);
        break;
    }

    case StoreBarrier: {
        filter(node->child1(), SpecCell);
        break;
    }

    case StoreBarrierWithNullCheck: {
        break;
    }

    case CheckTierUpAndOSREnter:
    case LoopHint:
        // We pretend that it can exit because it may want to get all state.
        node->setCanExit(true);
        break;

    case ZombieHint:
    case Unreachable:
    case LastNodeType:
    case ArithIMul:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    
    return m_state.isValid();
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::executeEffects(unsigned indexInBlock)
{
    return executeEffects(indexInBlock, m_state.block()->at(indexInBlock));
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::execute(unsigned indexInBlock)
{
    Node* node = m_state.block()->at(indexInBlock);
    if (!startExecuting(node))
        return true;
    
    executeEdges(node);
    return executeEffects(indexInBlock, node);
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::execute(Node* node)
{
    if (!startExecuting(node))
        return true;
    
    executeEdges(node);
    return executeEffects(UINT_MAX, node);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::clobberWorld(
    const CodeOrigin& codeOrigin, unsigned clobberLimit)
{
    clobberCapturedVars(codeOrigin);
    clobberStructures(clobberLimit);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::clobberCapturedVars(const CodeOrigin& codeOrigin)
{
    if (codeOrigin.inlineCallFrame) {
        const BitVector& capturedVars = codeOrigin.inlineCallFrame->capturedVars;
        for (size_t i = capturedVars.size(); i--;) {
            if (!capturedVars.quickGet(i))
                continue;
            m_state.variables().local(i).makeHeapTop();
        }
    } else {
        for (size_t i = m_codeBlock->m_numVars; i--;) {
            if (m_codeBlock->isCaptured(virtualRegisterForLocal(i)))
                m_state.variables().local(i).makeHeapTop();
        }
    }

    for (size_t i = m_state.variables().numberOfArguments(); i--;) {
        if (m_codeBlock->isCaptured(virtualRegisterForArgument(i)))
            m_state.variables().argument(i).makeHeapTop();
    }
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::clobberStructures(unsigned clobberLimit)
{
    if (!m_state.haveStructures())
        return;
    if (clobberLimit >= m_state.block()->size())
        clobberLimit = m_state.block()->size();
    else
        clobberLimit++;
    ASSERT(clobberLimit <= m_state.block()->size());
    for (size_t i = clobberLimit; i--;)
        forNode(m_state.block()->at(i)).clobberStructures();
    if (m_graph.m_form == SSA) {
        HashSet<Node*>::iterator iter = m_state.block()->ssa->liveAtHead.begin();
        HashSet<Node*>::iterator end = m_state.block()->ssa->liveAtHead.end();
        for (; iter != end; ++iter)
            forNode(*iter).clobberStructures();
    }
    for (size_t i = m_state.variables().numberOfArguments(); i--;)
        m_state.variables().argument(i).clobberStructures();
    for (size_t i = m_state.variables().numberOfLocals(); i--;)
        m_state.variables().local(i).clobberStructures();
    m_state.setHaveStructures(true);
    m_state.setDidClobber(true);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::dump(PrintStream& out)
{
    CommaPrinter comma(" ");
    if (m_graph.m_form == SSA) {
        HashSet<Node*>::iterator iter = m_state.block()->ssa->liveAtHead.begin();
        HashSet<Node*>::iterator end = m_state.block()->ssa->liveAtHead.end();
        for (; iter != end; ++iter) {
            Node* node = *iter;
            AbstractValue& value = forNode(node);
            if (value.isClear())
                continue;
            out.print(comma, node, ":", value);
        }
    }
    for (size_t i = 0; i < m_state.block()->size(); ++i) {
        Node* node = m_state.block()->at(i);
        AbstractValue& value = forNode(node);
        if (value.isClear())
            continue;
        out.print(comma, node, ":", value);
    }
}

template<typename AbstractStateType>
FiltrationResult AbstractInterpreter<AbstractStateType>::filter(
    AbstractValue& value, const StructureSet& set)
{
    if (value.filter(m_graph, set) == FiltrationOK)
        return FiltrationOK;
    m_state.setIsValid(false);
    return Contradiction;
}

template<typename AbstractStateType>
FiltrationResult AbstractInterpreter<AbstractStateType>::filterArrayModes(
    AbstractValue& value, ArrayModes arrayModes)
{
    if (value.filterArrayModes(arrayModes) == FiltrationOK)
        return FiltrationOK;
    m_state.setIsValid(false);
    return Contradiction;
}

template<typename AbstractStateType>
FiltrationResult AbstractInterpreter<AbstractStateType>::filter(
    AbstractValue& value, SpeculatedType type)
{
    if (value.filter(type) == FiltrationOK)
        return FiltrationOK;
    m_state.setIsValid(false);
    return Contradiction;
}

template<typename AbstractStateType>
FiltrationResult AbstractInterpreter<AbstractStateType>::filterByValue(
    AbstractValue& abstractValue, JSValue concreteValue)
{
    if (abstractValue.filterByValue(concreteValue) == FiltrationOK)
        return FiltrationOK;
    m_state.setIsValid(false);
    return Contradiction;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAbstractInterpreterInlines_h

