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

#if ENABLE(DFG_JIT)

#include "DFGAbstractInterpreter.h"
#include "GetByIdStatus.h"
#include "GetterSetter.h"
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
        if (childConst.toBoolean(m_codeBlock->globalObjectFor(node->origin.semantic)->globalExec()))
            return DefinitelyTrue;
        return DefinitelyFalse;
    }

    // Next check if we can fold because we know that the source is an object or string and does not equal undefined.
    if (isCellSpeculation(value.m_type) && !value.m_structure.isTop()) {
        bool allTrue = true;
        for (unsigned i = value.m_structure.size(); i--;) {
            Structure* structure = value.m_structure[i];
            if (structure->masqueradesAsUndefined(m_codeBlock->globalObjectFor(node->origin.semantic))
                || structure->typeInfo().type() == StringType) {
                allTrue = false;
                break;
            }
        }
        if (allTrue)
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
void AbstractInterpreter<AbstractStateType>::verifyEdge(Node* node, Edge edge)
{
    if (!(forNode(edge).m_type & ~typeFilterFor(edge.useKind())))
        return;
    
    DFG_CRASH(m_graph, node, toCString("Edge verification error: ", node, "->", edge, " was expected to have type ", SpeculationDump(typeFilterFor(edge.useKind())), " but has type ", SpeculationDump(forNode(edge).m_type)).data());
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
    case DoubleConstant:
    case Int52Constant:
    case PhantomArguments: {
        setBuiltInConstant(node, *node->constant());
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
        AbstractValue value = m_state.variables().operand(variableAccessData->local().offset());
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
            forNode(node).setType(SpecInt52AsDouble);
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
        break;
    }
        
    case BooleanToNumber: {
        JSValue concreteValue = forNode(node->child1()).value();
        if (concreteValue) {
            if (concreteValue.isBoolean())
                setConstant(node, jsNumber(concreteValue.asBoolean()));
            else
                setConstant(node, concreteValue);
            break;
        }
        AbstractValue& value = forNode(node);
        value = forNode(node->child1());
        if (node->child1().useKind() == UntypedUse && !(value.m_type & ~SpecBoolean))
            m_state.setFoundConstants(true);
        if (value.m_type & SpecBoolean) {
            value.merge(SpecInt32);
            value.filter(~SpecBoolean);
        }
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
                setConstant(node, jsNumber(child.asBoolean()));
                break;
            }
            if (child.isUndefinedOrNull()) {
                setConstant(node, jsNumber(0));
                break;
            }
        }
        
        forNode(node).setType(SpecInt32);
        break;
    }
        
    case DoubleRep: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, jsDoubleNumber(child.asNumber()));
            break;
        }
        forNode(node).setType(forNode(node->child1()).m_type);
        forNode(node).fixTypeForRepresentation(node);
        break;
    }
        
    case Int52Rep: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isMachineInt()) {
            setConstant(node, child);
            break;
        }
        
        forNode(node).setType(SpecInt32);
        break;
    }
        
    case ValueRep: {
        JSValue value = forNode(node->child1()).value();
        if (value) {
            setConstant(node, value);
            break;
        }
        
        forNode(node).setType(forNode(node->child1()).m_type & ~SpecDoubleImpureNaN);
        forNode(node).fixTypeForRepresentation(node);
        break;
    }
        
    case ValueAdd: {
        ASSERT(node->binaryUseKind() == UntypedUse);
        clobberWorld(node->origin.semantic, clobberLimit);
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
            break;
        case Int52RepUse:
            if (left && right && left.isMachineInt() && right.isMachineInt()) {
                JSValue result = jsNumber(left.asMachineInt() + right.asMachineInt());
                if (result.isMachineInt()) {
                    setConstant(node, result);
                    break;
                }
            }
            forNode(node).setType(SpecMachineInt);
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsDoubleNumber(left.asNumber() + right.asNumber()));
                break;
            }
            forNode(node).setType(
                typeOfDoubleSum(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
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
            break;
        case Int52RepUse:
            if (left && right && left.isMachineInt() && right.isMachineInt()) {
                JSValue result = jsNumber(left.asMachineInt() - right.asMachineInt());
                if (result.isMachineInt() || !shouldCheckOverflow(node->arithMode())) {
                    setConstant(node, result);
                    break;
                }
            }
            forNode(node).setType(SpecMachineInt);
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsDoubleNumber(left.asNumber() - right.asNumber()));
                break;
            }
            forNode(node).setType(
                typeOfDoubleDifference(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
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
            break;
        case Int52RepUse:
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
            forNode(node).setType(SpecMachineInt);
            break;
        case DoubleRepUse:
            if (child && child.isNumber()) {
                setConstant(node, jsDoubleNumber(-child.asNumber()));
                break;
            }
            forNode(node).setType(
                typeOfDoubleNegation(
                    forNode(node->child1()).m_type));
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
            break;
        case Int52RepUse:
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
            forNode(node).setType(SpecMachineInt);
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsDoubleNumber(left.asNumber() * right.asNumber()));
                break;
            }
            forNode(node).setType(
                typeOfDoubleProduct(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
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
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsDoubleNumber(left.asNumber() / right.asNumber()));
                break;
            }
            forNode(node).setType(
                typeOfDoubleQuotient(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
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
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsDoubleNumber(fmod(left.asNumber(), right.asNumber())));
                break;
            }
            forNode(node).setType(
                typeOfDoubleBinaryOp(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
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
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                double a = left.asNumber();
                double b = right.asNumber();
                setConstant(node, jsDoubleNumber(a < b ? a : (b <= a ? b : a + b)));
                break;
            }
            forNode(node).setType(
                typeOfDoubleMinMax(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
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
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                double a = left.asNumber();
                double b = right.asNumber();
                setConstant(node, jsDoubleNumber(a > b ? a : (b >= a ? b : a + b)));
                break;
            }
            forNode(node).setType(
                typeOfDoubleMinMax(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
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
            break;
        case DoubleRepUse:
            if (child && child.isNumber()) {
                setConstant(node, jsDoubleNumber(child.asNumber()));
                break;
            }
            forNode(node).setType(typeOfDoubleAbs(forNode(node->child1()).m_type));
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
            setConstant(node, jsDoubleNumber(sqrt(child.asNumber())));
            break;
        }
        forNode(node).setType(typeOfDoubleUnaryOp(forNode(node->child1()).m_type));
        break;
    }
        
    case ArithFRound: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, jsDoubleNumber(static_cast<float>(child.asNumber())));
            break;
        }
        forNode(node).setType(typeOfDoubleFRound(forNode(node->child1()).m_type));
        break;
    }
        
    case ArithSin: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, jsDoubleNumber(sin(child.asNumber())));
            break;
        }
        forNode(node).setType(typeOfDoubleUnaryOp(forNode(node->child1()).m_type));
        break;
    }
    
    case ArithCos: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isNumber()) {
            setConstant(node, jsDoubleNumber(cos(child.asNumber())));
            break;
        }
        forNode(node).setType(typeOfDoubleUnaryOp(forNode(node->child1()).m_type));
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
        JSValue child = forNode(node->child1()).value();
        if (child) {
            bool constantWasSet = true;
            switch (node->op()) {
            case IsUndefined:
                setConstant(node, jsBoolean(
                    child.isCell()
                    ? child.asCell()->structure()->masqueradesAsUndefined(m_codeBlock->globalObjectFor(node->origin.semantic))
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
            JSValue typeString = jsTypeStringForValue(*vm, m_codeBlock->globalObjectFor(node->origin.semantic), child);
            setConstant(node, *m_graph.freeze(typeString));
            break;
        }
        
        if (isFullNumberSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(vm->smallStrings.numberString()));
            break;
        }
        
        if (isStringSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(vm->smallStrings.stringString()));
            break;
        }
        
        if (isFinalObjectSpeculation(abstractChild.m_type) || isArraySpeculation(abstractChild.m_type) || isArgumentsSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(vm->smallStrings.objectString()));
            break;
        }
        
        if (isFunctionSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(vm->smallStrings.functionString()));
            break;
        }
        
        if (isBooleanSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(vm->smallStrings.booleanString()));
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
            if (!valuesCouldBeEqual(leftType, rightType)) {
                setConstant(node, jsBoolean(false));
                break;
            }
        }
        
        forNode(node).setType(SpecBoolean);
        break;
    }
            
    case CompareStrictEq: {
        Node* leftNode = node->child1().node();
        Node* rightNode = node->child2().node();
        JSValue left = forNode(leftNode).value();
        JSValue right = forNode(rightNode).value();
        if (left && right) {
            if (left.isString() && right.isString()) {
                // We need this case because JSValue::strictEqual is otherwise too racy for
                // string comparisons.
                const StringImpl* a = asString(left)->tryGetValueImpl();
                const StringImpl* b = asString(right)->tryGetValueImpl();
                if (a && b) {
                    setConstant(node, jsBoolean(WTF::equal(a, b)));
                    break;
                }
            } else {
                setConstant(node, jsBoolean(JSValue::strictEqual(0, left, right)));
                break;
            }
        }
        
        SpeculatedType leftLUB = leastUpperBoundOfStrictlyEquivalentSpeculations(forNode(leftNode).m_type);
        SpeculatedType rightLUB = leastUpperBoundOfStrictlyEquivalentSpeculations(forNode(rightNode).m_type);
        if (!(leftLUB & rightLUB)) {
            setConstant(node, jsBoolean(false));
            break;
        }
        
        forNode(node).setType(SpecBoolean);
        break;
    }
        
    case StringCharCodeAt:
        forNode(node).setType(SpecInt32);
        break;
        
    case StringFromCharCode:
        forNode(node).setType(SpecString);
        break;

    case StringCharAt:
        forNode(node).set(m_graph, m_graph.m_vm.stringStructure.get());
        break;
            
    case GetByVal: {
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
            clobberWorld(node->origin.semantic, clobberLimit);
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
                clobberWorld(node->origin.semantic, clobberLimit);
                forNode(node).makeHeapTop();
            } else
                forNode(node).set(m_graph, m_graph.m_vm.stringStructure.get());
            break;
        case Array::Arguments:
            forNode(node).makeHeapTop();
            break;
        case Array::Int32:
            if (node->arrayMode().isOutOfBounds()) {
                clobberWorld(node->origin.semantic, clobberLimit);
                forNode(node).makeHeapTop();
            } else
                forNode(node).setType(SpecInt32);
            break;
        case Array::Double:
            if (node->arrayMode().isOutOfBounds()) {
                clobberWorld(node->origin.semantic, clobberLimit);
                forNode(node).makeHeapTop();
            } else if (node->arrayMode().isSaneChain())
                forNode(node).setType(SpecBytecodeDouble);
            else
                forNode(node).setType(SpecDoubleReal);
            break;
        case Array::Contiguous:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld(node->origin.semantic, clobberLimit);
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
                forNode(node).setType(SpecInt52AsDouble);
            break;
        case Array::Float32Array:
            forNode(node).setType(SpecFullDouble);
            break;
        case Array::Float64Array:
            forNode(node).setType(SpecFullDouble);
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
        switch (node->arrayMode().modeForPut().type()) {
        case Array::ForceExit:
            m_state.setIsValid(false);
            break;
        case Array::Generic:
            clobberWorld(node->origin.semantic, clobberLimit);
            break;
        case Array::Int32:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld(node->origin.semantic, clobberLimit);
            break;
        case Array::Double:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld(node->origin.semantic, clobberLimit);
            break;
        case Array::Contiguous:
        case Array::ArrayStorage:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld(node->origin.semantic, clobberLimit);
            break;
        case Array::SlowPutArrayStorage:
            if (node->arrayMode().mayStoreToHole())
                clobberWorld(node->origin.semantic, clobberLimit);
            break;
        default:
            break;
        }
        break;
    }
            
    case ArrayPush:
        clobberWorld(node->origin.semantic, clobberLimit);
        forNode(node).setType(SpecBytecodeNumber);
        break;
            
    case ArrayPop:
        clobberWorld(node->origin.semantic, clobberLimit);
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
        break;
            
    case ToPrimitive: {
        JSValue childConst = forNode(node->child1()).value();
        if (childConst && childConst.isNumber()) {
            setConstant(node, childConst);
            break;
        }
        
        ASSERT(node->child1().useKind() == UntypedUse);
        
        if (!forNode(node->child1()).m_type) {
            m_state.setIsValid(false);
            break;
        }
        
        if (!(forNode(node->child1()).m_type & ~(SpecFullNumber | SpecBoolean | SpecString))) {
            m_state.setFoundConstants(true);
            forNode(node) = forNode(node->child1());
            break;
        }
        
        clobberWorld(node->origin.semantic, clobberLimit);
        
        forNode(node).setType((SpecHeapTop & ~SpecCell) | SpecString);
        break;
    }
        
    case ToString: {
        switch (node->child1().useKind()) {
        case StringObjectUse:
            // This also filters that the StringObject has the primordial StringObject
            // structure.
            filter(
                node->child1(),
                m_graph.globalObjectFor(node->origin.semantic)->stringObjectStructure());
            break;
        case StringOrStringObjectUse:
            break;
        case CellUse:
        case UntypedUse:
            clobberWorld(node->origin.semantic, clobberLimit);
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
        forNode(node).set(
            m_graph,
            m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()));
        break;
        
    case NewArrayBuffer:
        forNode(node).set(
            m_graph,
            m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()));
        break;

    case NewArrayWithSize:
        forNode(node).setType(SpecArray);
        break;
        
    case NewTypedArray:
        switch (node->child1().useKind()) {
        case Int32Use:
            break;
        case UntypedUse:
            clobberWorld(node->origin.semantic, clobberLimit);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        forNode(node).set(
            m_graph,
            m_graph.globalObjectFor(node->origin.semantic)->typedArrayStructure(
                node->typedArrayType()));
        break;
            
    case NewRegexp:
        forNode(node).set(m_graph, m_graph.globalObjectFor(node->origin.semantic)->regExpStructure());
        break;
            
    case ToThis: {
        AbstractValue& source = forNode(node->child1());
        AbstractValue& destination = forNode(node);
            
        if (m_graph.executableFor(node->origin.semantic)->isStrictMode())
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
        break;

    case NewObject:
        ASSERT(node->structure());
        forNode(node).set(m_graph, node->structure());
        break;
        
    case CreateActivation:
        forNode(node).set(
            m_graph, m_codeBlock->globalObjectFor(node->origin.semantic)->activationStructure());
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
                    m_graph.argumentsRegisterFor(node->origin.semantic).offset()).m_type))
            m_state.setFoundConstants(true);
        break;
        
    case GetMyArgumentsLength:
        // We know that this executable does not escape its arguments, so we can optimize
        // the arguments a bit. Note that this is not sufficient to force constant folding
        // of GetMyArgumentsLength, because GetMyArgumentsLength is a clobbering operation.
        // We perform further optimizations on this later on.
        if (node->origin.semantic.inlineCallFrame) {
            setConstant(
                node, jsNumber(node->origin.semantic.inlineCallFrame->arguments.size() - 1));
            m_state.setDidClobber(true); // Pretend that we clobbered to prevent constant folding.
        } else
            forNode(node).setType(SpecInt32);
        break;
        
    case GetMyArgumentsLengthSafe:
        // This potentially clobbers all structures if the arguments object had a getter
        // installed on the length property.
        clobberWorld(node->origin.semantic, clobberLimit);
        // We currently make no guarantee about what this returns because it does not
        // speculate that the length property is actually a length.
        forNode(node).makeHeapTop();
        break;
        
    case GetMyArgumentByVal: {
        InlineCallFrame* inlineCallFrame = node->origin.semantic.inlineCallFrame;
        JSValue value = forNode(node->child1()).m_value;
        if (inlineCallFrame && value && value.isInt32()) {
            int32_t index = value.asInt32();
            if (index >= 0
                && static_cast<size_t>(index + 1) < inlineCallFrame->arguments.size()) {
                forNode(node) = m_state.variables().operand(
                    inlineCallFrame->stackOffset +
                    m_graph.baselineCodeBlockFor(inlineCallFrame)->argumentIndexAfterCapture(index));
                m_state.setFoundConstants(true);
                break;
            }
        }
        forNode(node).makeHeapTop();
        break;
    }
        
    case GetMyArgumentByValSafe:
        // This potentially clobbers all structures if the property we're accessing has
        // a getter. We don't speculate against this.
        clobberWorld(node->origin.semantic, clobberLimit);
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
            m_graph, m_codeBlock->globalObjectFor(node->origin.semantic)->functionStructure());
        break;
        
    case GetCallee:
        forNode(node).setType(SpecFunction);
        break;
        
    case GetGetter: {
        JSValue base = forNode(node->child1()).m_value;
        if (base) {
            if (JSObject* getter = jsCast<GetterSetter*>(base)->getterConcurrently()) {
                setConstant(node, *m_graph.freeze(getter));
                break;
            }
        }
        
        forNode(node).setType(SpecObject);
        break;
    }
        
    case GetSetter: {
        JSValue base = forNode(node->child1()).m_value;
        if (base) {
            if (JSObject* setter = jsCast<GetterSetter*>(base)->setterConcurrently()) {
                setConstant(node, *m_graph.freeze(setter));
                break;
            }
        }
        
        forNode(node).setType(SpecObject);
        break;
    }
        
    case GetScope: // FIXME: We could get rid of these if we know that the JSFunction is a constant. https://bugs.webkit.org/show_bug.cgi?id=106202
    case GetMyScope:
        forNode(node).setType(SpecObjectOther);
        break;

    case SkipScope: {
        JSValue child = forNode(node->child1()).value();
        if (child) {
            setConstant(node, *m_graph.freeze(JSValue(jsCast<JSScope*>(child.asCell())->next())));
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
        clobberCapturedVars(node->origin.semantic);
        break;
            
    case GetById:
    case GetByIdFlush: {
        if (!node->prediction()) {
            m_state.setIsValid(false);
            break;
        }
        
        AbstractValue& value = forNode(node->child1());
        if (!value.m_structure.isTop() && !value.m_structure.isClobbered()
            && (node->child1().useKind() == CellUse || !(value.m_type & ~SpecCell))) {
            GetByIdStatus status = GetByIdStatus::computeFor(
                m_graph.m_vm, value.m_structure.set(),
                m_graph.identifiers()[node->identifierNumber()]);
            if (status.isSimple()) {
                // Figure out what the result is going to be - is it TOP, a constant, or maybe
                // something more subtle?
                AbstractValue result;
                for (unsigned i = status.numVariants(); i--;) {
                    DFG_ASSERT(m_graph, node, !status[i].alternateBase());
                    JSValue constantResult =
                        m_graph.tryGetConstantProperty(value, status[i].offset());
                    if (!constantResult) {
                        result.makeHeapTop();
                        break;
                    }
                    
                    AbstractValue thisResult;
                    thisResult.set(
                        m_graph, *m_graph.freeze(constantResult),
                        m_state.structureClobberState());
                    result.merge(thisResult);
                }
                if (status.numVariants() == 1 || isFTL(m_graph.m_plan.mode))
                    m_state.setFoundConstants(true);
                forNode(node) = result;
                break;
            }
        }

        clobberWorld(node->origin.semantic, clobberLimit);
        forNode(node).makeHeapTop();
        break;
    }
            
    case GetArrayLength:
        forNode(node).setType(SpecInt32);
        break;
        
    case CheckStructure: {
        // FIXME: We should be able to propagate the structure sets of constants (i.e. prototypes).
        AbstractValue& value = forNode(node->child1());
        ASSERT(!(value.m_type & ~SpecCell)); // Edge filtering should have already ensured this.

        StructureSet& set = node->structureSet();
        
        // It's interesting that we could have proven that the object has a larger structure set
        // that includes the set we're testing. In that case we could make the structure check
        // more efficient. We currently don't.
        
        if (value.m_structure.isSubsetOf(set)) {
            m_state.setFoundConstants(true);
            break;
        }

        filter(value, set);
        break;
    }
        
    case PutStructure:
        if (!forNode(node->child1()).m_structure.isClear()) {
            if (forNode(node->child1()).m_structure.onlyStructure() == node->transition()->next)
                m_state.setFoundConstants(true);
            else {
                observeTransition(
                    clobberLimit, node->transition()->previous, node->transition()->next);
                forNode(node->child1()).changeStructure(m_graph, node->transition()->next);
            }
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
        break;
    }
    case Arrayify: {
        if (node->arrayMode().alreadyChecked(m_graph, node, forNode(node->child1()))) {
            m_state.setFoundConstants(true);
            break;
        }
        ASSERT(node->arrayMode().conversion() == Array::Convert
            || node->arrayMode().conversion() == Array::RageConvert);
        clobberStructures(clobberLimit);
        filterArrayModes(node->child1(), node->arrayMode().arrayModesThatPassFiltering());
        break;
    }
    case ArrayifyToStructure: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isSubsetOf(StructureSet(node->structure())))
            m_state.setFoundConstants(true);
        clobberStructures(clobberLimit);
        
        // We have a bunch of options of how to express the abstract set at this point. Let set S
        // be the set of structures that the value had before clobbering and assume that all of
        // them are watchable. The new value should be the least expressible upper bound of the
        // intersection of "values that currently have structure = node->structure()" and "values
        // that have structure in S plus any structure transition-reachable from S". Assume that
        // node->structure() is not in S but it is transition-reachable from S. Then we would
        // like to say that the result is "values that have structure = node->structure() until
        // we invalidate", but there is no way to express this using the AbstractValue syntax. So
        // we must choose between:
        //
        // 1) "values that currently have structure = node->structure()". This is a valid
        //    superset of the value that we really want, and it's specific enough to satisfy the
        //    preconditions of the array access that this is guarding. It's also specific enough
        //    to allow relevant optimizations in the case that we didn't have a contradiction
        //    like in this example. Notice that in the abscence of any contradiction, this result
        //    is precise rather than being a conservative LUB.
        //
        // 2) "values that currently hava structure in S plus any structure transition-reachable
        //    from S". This is also a valid superset of the value that we really want, but it's
        //    not specific enough to satisfy the preconditions of the array access that this is
        //    guarding - so playing such shenanigans would preclude us from having assertions on
        //    the typing preconditions of any array accesses. This would also not be a desirable
        //    answer in the absence of a contradiction.
        //
        // Note that it's tempting to simply say that the resulting value is BOTTOM because of
        // the contradiction. That would be wrong, since we haven't hit an invalidation point,
        // yet.
        value.set(m_graph, node->structure());
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
        StorageAccessData data = m_graph.m_storageAccessData[node->storageAccessDataIndex()];
        JSValue result = m_graph.tryGetConstantProperty(forNode(node->child2()), data.offset);
        if (result) {
            setConstant(node, *m_graph.freeze(result));
            break;
        }
        
        forNode(node).makeHeapTop();
        break;
    }
        
    case GetGetterSetterByOffset: {
        StorageAccessData data = m_graph.m_storageAccessData[node->storageAccessDataIndex()];
        JSValue result = m_graph.tryGetConstantProperty(forNode(node->child2()), data.offset);
        if (result && jsDynamicCast<GetterSetter*>(result)) {
            setConstant(node, *m_graph.freeze(result));
            break;
        }
        
        forNode(node).set(m_graph, m_graph.m_vm.getterSetterStructure.get());
        break;
    }
        
    case MultiGetByOffset: {
        // This code will filter the base value in a manner that is possibly different (either more
        // or less precise) than the way it would be filtered if this was strength-reduced to a
        // CheckStructure. This is fine. It's legal for different passes over the code to prove
        // different things about the code, so long as all of them are sound. That even includes
        // one guy proving that code should never execute (due to a contradiction) and another guy
        // not finding that contradiction. If someone ever proved that there would be a
        // contradiction then there must always be a contradiction even if subsequent passes don't
        // realize it. This is the case here.
        
        // Ordinarily you have to be careful with calling setFoundConstants()
        // because of the effect on compile times, but this node is FTL-only.
        m_state.setFoundConstants(true);
        
        AbstractValue base = forNode(node->child1());
        StructureSet baseSet;
        AbstractValue result;
        for (unsigned i = node->multiGetByOffsetData().variants.size(); i--;) {
            GetByIdVariant& variant = node->multiGetByOffsetData().variants[i];
            StructureSet set = variant.structureSet();
            set.filter(base);
            if (set.isEmpty())
                continue;
            baseSet.merge(set);
            
            JSValue baseForLoad;
            if (variant.alternateBase())
                baseForLoad = variant.alternateBase();
            else
                baseForLoad = base.m_value;
            JSValue constantResult =
                m_graph.tryGetConstantProperty(
                    baseForLoad, variant.baseStructure(), variant.offset());
            if (!constantResult) {
                result.makeHeapTop();
                continue;
            }
            AbstractValue thisResult;
            thisResult.set(
                m_graph,
                *m_graph.freeze(constantResult),
                m_state.structureClobberState());
            result.merge(thisResult);
        }
        
        if (forNode(node->child1()).changeStructure(m_graph, baseSet) == Contradiction)
            m_state.setIsValid(false);
        
        forNode(node) = result;
        break;
    }
            
    case PutByOffset: {
        break;
    }
        
    case MultiPutByOffset: {
        StructureSet newSet;
        TransitionVector transitions;
        
        // Ordinarily you have to be careful with calling setFoundConstants()
        // because of the effect on compile times, but this node is FTL-only.
        m_state.setFoundConstants(true);
        
        AbstractValue base = forNode(node->child1());
        
        for (unsigned i = node->multiPutByOffsetData().variants.size(); i--;) {
            const PutByIdVariant& variant = node->multiPutByOffsetData().variants[i];
            StructureSet thisSet = variant.oldStructure();
            thisSet.filter(base);
            if (thisSet.isEmpty())
                continue;
            if (variant.kind() == PutByIdVariant::Transition) {
                if (thisSet.onlyStructure() != variant.newStructure()) {
                    transitions.append(
                        Transition(variant.oldStructureForTransition(), variant.newStructure()));
                } // else this is really a replace.
                newSet.add(variant.newStructure());
            } else {
                ASSERT(variant.kind() == PutByIdVariant::Replace);
                newSet.merge(thisSet);
            }
        }
        
        observeTransitions(clobberLimit, transitions);
        if (forNode(node->child1()).changeStructure(m_graph, newSet) == Contradiction)
            m_state.setIsValid(false);
        break;
    }
        
    case GetExecutable: {
        JSValue value = forNode(node->child1()).value();
        if (value) {
            JSFunction* function = jsDynamicCast<JSFunction*>(value);
            if (function) {
                setConstant(node, *m_graph.freeze(function->executable()));
                break;
            }
        }
        forNode(node).setType(SpecCellOther);
        break;
    }
    
    case CheckCell: {
        JSValue value = forNode(node->child1()).value();
        if (value == node->cellOperand()->value()) {
            m_state.setFoundConstants(true);
            ASSERT(value);
            break;
        }
        
        filterByValue(node->child1(), *node->cellOperand());
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
        break;
    }
        
    case PutById:
    case PutByIdFlush:
    case PutByIdDirect: {
        AbstractValue& value = forNode(node->child1());
        if (!value.m_structure.isTop() && !value.m_structure.isClobbered()) {
            PutByIdStatus status = PutByIdStatus::computeFor(
                m_graph.m_vm,
                m_graph.globalObjectFor(node->origin.semantic),
                value.m_structure.set(),
                m_graph.identifiers()[node->identifierNumber()],
                node->op() == PutByIdDirect);
            
            if (status.isSimple()) {
                StructureSet newSet;
                TransitionVector transitions;
                
                for (unsigned i = status.numVariants(); i--;) {
                    const PutByIdVariant& variant = status[i];
                    if (variant.kind() == PutByIdVariant::Transition) {
                        transitions.append(
                            Transition(
                                variant.oldStructureForTransition(), variant.newStructure()));
                        m_graph.registerStructure(variant.newStructure());
                        newSet.add(variant.newStructure());
                    } else {
                        ASSERT(variant.kind() == PutByIdVariant::Replace);
                        newSet.merge(variant.oldStructure());
                    }
                }
                
                if (status.numVariants() == 1 || isFTL(m_graph.m_plan.mode))
                    m_state.setFoundConstants(true);
                
                observeTransitions(clobberLimit, transitions);
                if (forNode(node->child1()).changeStructure(m_graph, newSet) == Contradiction)
                    m_state.setIsValid(false);
                break;
            }
        }
        
        clobberWorld(node->origin.semantic, clobberLimit);
        break;
    }
        
    case In: {
        // FIXME: We can determine when the property definitely exists based on abstract
        // value information.
        clobberWorld(node->origin.semantic, clobberLimit);
        forNode(node).setType(SpecBoolean);
        break;
    }
            
    case GetEnumerableLength: {
        forNode(node).setType(SpecInt32);
        break;
    }
    case HasGenericProperty: {
        forNode(node).setType(SpecBoolean);
        break;
    }
    case HasStructureProperty: {
        forNode(node).setType(SpecBoolean);
        break;
    }
    case HasIndexedProperty: {
        ArrayMode mode = node->arrayMode();
        switch (mode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage: {
            break;
        }
        default: {
            clobberWorld(node->origin.semantic, clobberLimit);
            break;
        }
        }
        forNode(node).setType(SpecBoolean);
        break;
    }
    case GetDirectPname: {
        clobberWorld(node->origin.semantic, clobberLimit);
        forNode(node).makeHeapTop();
        break;
    }
    case GetStructurePropertyEnumerator: {
        forNode(node).setType(SpecCell);
        break;
    }
    case GetGenericPropertyEnumerator: {
        forNode(node).setType(SpecCell);
        break;
    }
    case GetEnumeratorPname: {
        forNode(node).setType(SpecString | SpecOther);
        break;
    }
    case ToIndexString: {
        forNode(node).setType(SpecString);
        break;
    }

    case GetGlobalVar:
        forNode(node).makeHeapTop();
        break;
        
    case VariableWatchpoint:
    case VarInjectionWatchpoint:
    case PutGlobalVar:
    case NotifyWrite:
        break;
            
    case CheckHasInstance:
        // Sadly, we don't propagate the fact that we've done CheckHasInstance
        break;
            
    case InstanceOf:
        // Again, sadly, we don't propagate the fact that we've done InstanceOf
        forNode(node).setType(SpecBoolean);
        break;
            
    case Phi:
        RELEASE_ASSERT(m_graph.m_form == SSA);
        // The state of this node would have already been decided, but it may have become a
        // constant, in which case we'd like to know.
        if (forNode(node).m_value)
            m_state.setFoundConstants(true);
        break;
        
    case Upsilon: {
        m_state.createValueForNode(node->phi());
        forNode(node->phi()) = forNode(node->child1());
        break;
    }
        
    case Flush:
    case PhantomLocal:
        break;
            
    case Call:
    case Construct:
    case NativeCall:
    case NativeConstruct:
        clobberWorld(node->origin.semantic, clobberLimit);
        forNode(node).makeHeapTop();
        break;

    case ProfiledCall:
    case ProfiledConstruct:
        if (forNode(m_graph.varArgChild(node, 0)).m_value)
            m_state.setFoundConstants(true);
        clobberWorld(node->origin.semantic, clobberLimit);
        forNode(node).makeHeapTop();
        break;

    case ForceOSRExit:
    case CheckBadCell:
        m_state.setIsValid(false);
        break;
        
    case InvalidationPoint:
        forAllValues(clobberLimit, AbstractValue::observeInvalidationPointFor);
        m_state.setStructureClobberState(StructuresAreWatched);
        break;

    case CheckWatchdogTimer:
        break;

    case Breakpoint:
    case ProfileWillCall:
    case ProfileDidCall:
    case Phantom:
    case HardPhantom:
    case CountExecution:
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
        break;

    case Check: {
        // Simplify out checks that don't actually do checking.
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge edge = node->children.child(i);
            if (!edge)
                break;
            if (edge.isProved() || edge.willNotHaveCheck()) {
                m_state.setFoundConstants(true);
                break;
            }
        }
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
        break;

    case ZombieHint:
    case Unreachable:
    case LastNodeType:
    case ArithIMul:
    case FiatInt52:
    case BottomValue:
        DFG_CRASH(m_graph, node, "Unexpected node type");
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
    SamplingRegion samplingRegion("DFG AI Clobber Captured Vars");
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
template<typename Functor>
void AbstractInterpreter<AbstractStateType>::forAllValues(
    unsigned clobberLimit, Functor& functor)
{
    SamplingRegion samplingRegion("DFG AI For All Values");
    if (clobberLimit >= m_state.block()->size())
        clobberLimit = m_state.block()->size();
    else
        clobberLimit++;
    ASSERT(clobberLimit <= m_state.block()->size());
    for (size_t i = clobberLimit; i--;)
        functor(forNode(m_state.block()->at(i)));
    if (m_graph.m_form == SSA) {
        HashSet<Node*>::iterator iter = m_state.block()->ssa->liveAtHead.begin();
        HashSet<Node*>::iterator end = m_state.block()->ssa->liveAtHead.end();
        for (; iter != end; ++iter)
            functor(forNode(*iter));
    }
    for (size_t i = m_state.variables().numberOfArguments(); i--;)
        functor(m_state.variables().argument(i));
    for (size_t i = m_state.variables().numberOfLocals(); i--;)
        functor(m_state.variables().local(i));
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::clobberStructures(unsigned clobberLimit)
{
    SamplingRegion samplingRegion("DFG AI Clobber Structures");
    forAllValues(clobberLimit, AbstractValue::clobberStructuresFor);
    setDidClobber();
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::observeTransition(
    unsigned clobberLimit, Structure* from, Structure* to)
{
    AbstractValue::TransitionObserver transitionObserver(from, to);
    forAllValues(clobberLimit, transitionObserver);
    
    ASSERT(!from->dfgShouldWatch()); // We don't need to claim to be in a clobbered state because 'from' was never watchable (during the time we were compiling), hence no constants ever introduced into the DFG IR that ever had a watchable structure would ever have the same structure as from.
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::observeTransitions(
    unsigned clobberLimit, const TransitionVector& vector)
{
    AbstractValue::TransitionsObserver transitionsObserver(vector);
    forAllValues(clobberLimit, transitionsObserver);
    
    if (!ASSERT_DISABLED) {
        // We don't need to claim to be in a clobbered state because none of the Transition::previous structures are watchable.
        for (unsigned i = vector.size(); i--;)
            ASSERT(!vector[i].previous->dfgShouldWatch());
    }
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::setDidClobber()
{
    m_state.setDidClobber(true);
    m_state.setStructureClobberState(StructuresAreClobbered);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::dump(PrintStream& out) const
{
    const_cast<AbstractInterpreter<AbstractStateType>*>(this)->dump(out);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::dump(PrintStream& out)
{
    CommaPrinter comma(" ");
    HashSet<Node*> seen;
    if (m_graph.m_form == SSA) {
        HashSet<Node*>::iterator iter = m_state.block()->ssa->liveAtHead.begin();
        HashSet<Node*>::iterator end = m_state.block()->ssa->liveAtHead.end();
        for (; iter != end; ++iter) {
            Node* node = *iter;
            seen.add(node);
            AbstractValue& value = forNode(node);
            if (value.isClear())
                continue;
            out.print(comma, node, ":", value);
        }
    }
    for (size_t i = 0; i < m_state.block()->size(); ++i) {
        Node* node = m_state.block()->at(i);
        seen.add(node);
        AbstractValue& value = forNode(node);
        if (value.isClear())
            continue;
        out.print(comma, node, ":", value);
    }
    if (m_graph.m_form == SSA) {
        HashSet<Node*>::iterator iter = m_state.block()->ssa->liveAtTail.begin();
        HashSet<Node*>::iterator end = m_state.block()->ssa->liveAtTail.end();
        for (; iter != end; ++iter) {
            Node* node = *iter;
            if (seen.contains(node))
                continue;
            AbstractValue& value = forNode(node);
            if (value.isClear())
                continue;
            out.print(comma, node, ":", value);
        }
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
    AbstractValue& abstractValue, FrozenValue concreteValue)
{
    if (abstractValue.filterByValue(concreteValue) == FiltrationOK)
        return FiltrationOK;
    m_state.setIsValid(false);
    return Contradiction;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGAbstractInterpreterInlines_h

