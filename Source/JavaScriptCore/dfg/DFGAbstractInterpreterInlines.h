/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "dfg/DFGNodeType.h"
#if ENABLE(DFG_JIT)

#include "ArrayConstructor.h"
#include "ArrayPrototype.h"
#include "CacheableIdentifierInlines.h"
#include "CheckPrivateBrandStatus.h"
#include "DFGAbstractInterpreter.h"
#include "DFGAbstractInterpreterClobberState.h"
#include "DOMJITGetterSetter.h"
#include "DOMJITSignature.h"
#include "FunctionPrototype.h"
#include "GetByStatus.h"
#include "GetterSetter.h"
#include "HashMapHelper.h"
#include "JITOperations.h"
#include "JSAsyncGenerator.h"
#include "JSGenerator.h"
#include "JSImmutableButterfly.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSPromiseConstructor.h"
#include "JSWebAssemblyInstance.h"
#include "MathCommon.h"
#include "NumberConstructor.h"
#include "ObjectConstructor.h"
#include "PutByStatus.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "SetPrivateBrandStatus.h"
#include "StringObject.h"
#include "StructureCache.h"
#include "StructureRareDataInlines.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyModuleRecord.h"
#include <wtf/BooleanLattice.h>
#include <wtf/CheckedArithmetic.h>

namespace JSC { namespace DFG {

template<typename AbstractStateType>
AbstractInterpreter<AbstractStateType>::AbstractInterpreter(Graph& graph, AbstractStateType& state)
    : m_codeBlock(graph.m_codeBlock)
    , m_graph(graph)
    , m_vm(m_graph.m_vm)
    , m_state(state)
{
    if (m_graph.m_form == SSA)
        m_phiChildren = makeUnique<PhiChildren>(m_graph);
}

template<typename AbstractStateType>
AbstractInterpreter<AbstractStateType>::~AbstractInterpreter()
{
}

template<typename AbstractStateType>
TriState AbstractInterpreter<AbstractStateType>::booleanResult(Node* node, AbstractValue& value)
{
    JSValue childConst = value.value();
    if (childConst) {
        if (childConst.toBoolean(m_codeBlock->globalObjectFor(node->origin.semantic)))
            return TriState::True;
        return TriState::False;
    }

    // Next check if we can fold because we know that the source is an object or string and does not equal undefined.
    if (isCellSpeculation(value.m_type) && !value.m_structure.isTop()) {
        bool allTrue = true;
        for (unsigned i = value.m_structure.size(); i--;) {
            RegisteredStructure structure = value.m_structure[i];
            if (structure->masqueradesAsUndefined(m_codeBlock->globalObjectFor(node->origin.semantic)) || structure->typeInfo().type() == StringType || structure->typeInfo().type() == HeapBigIntType) {
                allTrue = false;
                break;
            }
        }
        if (allTrue)
            return TriState::True;
    }
    
    return TriState::Indeterminate;
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::startExecuting()
{
    ASSERT(m_state.block());
    ASSERT(m_state.isValid());
    
    m_state.setClobberState(AbstractInterpreterClobberState::NotClobbered);
}

template<typename AbstractStateType>
class AbstractInterpreterExecuteEdgesFunc {
public:
    AbstractInterpreterExecuteEdgesFunc(AbstractInterpreter<AbstractStateType>& interpreter)
        : m_interpreter(interpreter)
    {
    }
    
    // This func is manually written out so that we can put ALWAYS_INLINE on it.
    ALWAYS_INLINE void operator()(Edge& edge) const
    {
        m_interpreter.filterEdgeByUse(edge);
    }
    
private:
    AbstractInterpreter<AbstractStateType>& m_interpreter;
};

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::executeEdges(Node* node)
{
    m_graph.doToChildren(node, AbstractInterpreterExecuteEdgesFunc<AbstractStateType>(*this));
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::executeKnownEdgeTypes(Node* node)
{
    // Some use kinds are required to not have checks, because we know somehow that the incoming
    // value will already have the type we want. In those cases, AI may not be smart enough to
    // prove that this is indeed the case. But the existance of the edge is enough to prove that
    // it is indeed the case. Taking advantage of this is not optional, since otherwise the DFG
    // and FTL backends may emit checks in a node that lacks a valid exit origin.
    m_graph.doToChildren(
        node,
        [&] (Edge& edge) {
            if (mayHaveTypeCheck(edge.useKind()))
                return;
            
            filterEdgeByUse(edge);
        });
}

template<typename AbstractStateType>
ALWAYS_INLINE void AbstractInterpreter<AbstractStateType>::filterByType(Edge& edge, SpeculatedType type)
{
    AbstractValue& value = m_state.forNodeWithoutFastForward(edge);
    if (value.isType(type)) {
        m_state.setProofStatus(edge, IsProved);
        return;
    }
    m_state.setProofStatus(edge, NeedsCheck);
    m_state.fastForwardAndFilterUnproven(value, type);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::verifyEdge(Node* node, Edge edge)
{
    if (edge.node()->isTuple()) {
        if (edge.useKind() == UntypedUse && node->op() == ExtractFromTuple)
            return;
        DFG_CRASH(m_graph, node, toCString("Tuple edge verification error: ", node, "->", edge, " was expected to have Untyped use kind (had ", edge.useKind(),
            "). Has type ", SpeculationDump(m_state.forTupleNodeWithoutFastForward(edge.node(), node->extractOffset()).m_type)).data(),
            AbstractInterpreterInvalidType, node->op(), edge->op(), edge.useKind(), m_state.forNodeWithoutFastForward(node).m_type);
    }

    if (!(m_state.forNodeWithoutFastForward(edge).m_type & ~typeFilterFor(edge.useKind())))
        return;
    
    DFG_CRASH(m_graph, node, toCString("Edge verification error: ", node, "->", edge, " was expected to have type ", SpeculationDump(typeFilterFor(edge.useKind())), " but has type ", SpeculationDump(forNode(edge).m_type), " (", forNode(edge).m_type, ")").data(), AbstractInterpreterInvalidType, node->op(), edge->op(), edge.useKind(), forNode(edge).m_type);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::verifyEdges(Node* node)
{
    DFG_NODE_DO_TO_CHILDREN(m_graph, node, verifyEdge);
}

enum class ToThisResult {
    Identity,
    Undefined,
    GlobalThis,
    Dynamic,
};
inline ToThisResult isToThisAnIdentity(ECMAMode ecmaMode, AbstractValue& valueForNode)
{
    // We look at the type first since that will cover most cases and does not require iterating all the structures.
    if (ecmaMode.isStrict()) {
        if (valueForNode.m_type && !(valueForNode.m_type & SpecObjectOther))
            return ToThisResult::Identity;
    } else {
        if (valueForNode.m_type && !(valueForNode.m_type & (~SpecObject | SpecObjectOther)))
            return ToThisResult::Identity;
    }

    if (JSValue value = valueForNode.value()) {
        if (value.isCell()) {
            if (value.asCell()->isObject()) {
                if (value.asCell()->inherits<JSScope>()) {
                    if (ecmaMode.isStrict())
                        return ToThisResult::Undefined;
                    return ToThisResult::GlobalThis;
                }
                return ToThisResult::Identity;
            }
        }
    }

    bool onlyObjects = valueForNode.m_type && !(valueForNode.m_type & ~SpecObject);
    if ((ecmaMode.isStrict() || onlyObjects) && valueForNode.m_structure.isFinite()) {
        bool allStructuresAreJSScope = !valueForNode.m_structure.isClear();
        bool overridesToThis = false;
        valueForNode.m_structure.forEach([&](RegisteredStructure structure) {
            TypeInfo type = structure->typeInfo();
            ASSERT(type.isObject() || type.type() == StringType || type.type() == SymbolType || type.type() == HeapBigIntType);
            if (!ecmaMode.isStrict())
                ASSERT(type.isObject());
            // We don't need to worry about strings/symbols here since either:
            // 1) We are in strict mode and strings/symbols are not wrapped
            // 2) The AI has proven that the type of this is a subtype of object
            if (type.isObject() && (FirstScopeType <= type.type() && type.type() <= LastScopeType))
                overridesToThis = true;

            // If all the structures are JSScope's ones, we know the details of JSScope::toThis() operation.
            allStructuresAreJSScope &= structure->classInfoForCells()->isSubClassOf(JSScope::info());
        });

        // This is correct for strict mode even if this can have non objects, since the right semantics is Identity.
        if (!overridesToThis)
            return ToThisResult::Identity;

        // But this folding is available only if input is always an object.
        if (onlyObjects && allStructuresAreJSScope) {
            if (ecmaMode.isStrict())
                return ToThisResult::Undefined;
            return ToThisResult::GlobalThis;
        }
    }

    return ToThisResult::Dynamic;
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::handleConstantBinaryBitwiseOp(Node* node)
{
    JSValue left = forNode(node->child1()).value();
    JSValue right = forNode(node->child2()).value();
    if (left && right && left.isInt32() && right.isInt32()) {
        int32_t a = left.asInt32();
        int32_t b = right.asInt32();
        if (node->isBinaryUseKind(UntypedUse))
            didFoldClobberWorld();
        NodeType op = node->op();
        switch (op) {
        case ValueBitAnd:
        case ArithBitAnd:
            setConstant(node, JSValue(a & b));
            break;
        case ValueBitOr:
        case ArithBitOr:
            setConstant(node, JSValue(a | b));
            break;
        case ValueBitXor:
        case ArithBitXor:
            setConstant(node, JSValue(a ^ b));
            break;
        case ArithBitRShift:
        case ValueBitRShift:
            setConstant(node, JSValue(a >> (static_cast<uint32_t>(b) & 0x1f)));
            break;
        case ValueBitLShift:
        case ArithBitLShift:
            setConstant(node, JSValue(a << (static_cast<uint32_t>(b) & 0x1f)));
            break;
        case BitURShift:
            setConstant(node, JSValue(static_cast<int32_t>(static_cast<uint32_t>(a) >> (static_cast<uint32_t>(b) & 0x1f))));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        return true;
    }

    return false;
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::handleConstantDivOp(Node* node)
{
    JSValue left = forNode(node->child1()).value();
    JSValue right = forNode(node->child2()).value();

    if (left && right) {
        NodeType op = node->op();
        bool isDivOperation = op == ValueDiv || op == ArithDiv;

        // Only possible case of ValueOp below is UntypedUse,
        // so we need to reflect clobberize rules.
        bool isClobbering = op == ValueDiv || op == ValueMod;

        if (left.isInt32() && right.isInt32()) {
            double doubleResult;
            if (isDivOperation)
                doubleResult = left.asNumber() / right.asNumber();
            else
                doubleResult = fmod(left.asNumber(), right.asNumber());

            if (node->hasArithMode()) {
                if (!shouldCheckOverflow(node->arithMode()))
                    doubleResult = toInt32(doubleResult);
                else if (!shouldCheckNegativeZero(node->arithMode()))
                    doubleResult += 0; // Sanitizes zero.
            }

            JSValue valueResult = jsNumber(doubleResult);
            if (valueResult.isInt32()) {
                if (isClobbering)
                    didFoldClobberWorld();
                setConstant(node, valueResult);
                return true;
            }
        } else if (left.isNumber() && right.isNumber()) {
            if (isClobbering)
                didFoldClobberWorld();

            if (isDivOperation) {
                if (op == ValueDiv)
                    setConstant(node, jsNumber(left.asNumber() / right.asNumber()));
                else
                    setConstant(node, jsDoubleNumber(left.asNumber() / right.asNumber()));
            } else {
                if (op == ValueMod)
                    setConstant(node, jsNumber(fmod(left.asNumber(), right.asNumber())));
                else
                    setConstant(node, jsDoubleNumber(fmod(left.asNumber(), right.asNumber())));
            }

            return true;
        }
    }

    return false;
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::executeEffects(unsigned clobberLimit, Node* node)
{
    verifyEdges(node);
    
    m_state.createValueForNode(node);
    
    switch (node->op()) {
    case JSConstant:
    case DoubleConstant:
    case Int52Constant: {
        setBuiltInConstant(node, *node->constant());
        break;
    }

    case LazyJSConstant: {
        LazyJSValue value = node->lazyJSValue();
        switch (value.kind()) {
        case LazyJSValue::KnownValue:
            setConstant(node, value.value()->value());
            break;
        case LazyJSValue::SingleCharacterString:
        case LazyJSValue::KnownStringImpl:
        case LazyJSValue::NewStringImpl:
            setTypeForNode(node, SpecString);
            break;
        }
        break;
    }

    case IdentityWithProfile:
    case Identity: {
        setForNode(node, forNode(node->child1()));
        if (forNode(node).value())
            m_state.setShouldTryConstantFolding(true);
        break;
    }

    case ExtractFromTuple: {
        setForNode(node, m_state.forTupleNode(node->child1(), node->extractOffset()));
        if (forNode(node).value())
            m_state.setShouldTryConstantFolding(true);
        break;
    }
        
    case ExtractCatchLocal:
    case ExtractOSREntryLocal: {
        makeBytecodeTopForNode(node);
        break;
    }
            
    case GetLocal: {
        VariableAccessData* variableAccessData = node->variableAccessData();
        AbstractValue& value = m_state.operand(variableAccessData->operand());
        // The value in the local should already be checked.
        DFG_ASSERT(m_graph, node, value.isType(typeFilterFor(variableAccessData->flushFormat())));
        if (value.value())
            m_state.setShouldTryConstantFolding(true);
        setForNode(node, value);
        break;
    }
        
    case GetStack: {
        StackAccessData* data = node->stackAccessData();
        AbstractValue& value = m_state.operand(data->operand);
        // The value in the local should already be checked.
        DFG_ASSERT(m_graph, node, value.isType(typeFilterFor(data->format)));
        if (value.value())
            m_state.setShouldTryConstantFolding(true);
        setForNode(node, value);
        break;
    }
        
    case SetLocal: {
        m_state.operand(node->operand()) = forNode(node->child1());
        break;
    }
        
    case PutStack: {
        m_state.operand(node->stackAccessData()->operand) = forNode(node->child1());
        break;
    }
        
    case ZombieHint:
    case MovHint: {
        // Don't need to do anything. A MovHint only informs us about what would have happened
        // in bytecode, but this code is just concerned with what is actually happening during
        // DFG execution.
        break;
    }

    case KillStack: {
        // This is just a hint telling us that the OSR state of the local is no longer inside the
        // flushed data.
        break;
    }
        
    case SetArgumentDefinitely:
    case SetArgumentMaybe:
        // Assert that the state of arguments has been set. SetArgumentDefinitely/SetArgumentMaybe means
        // that someone set the argument values out-of-band, and currently this always means setting to a
        // non-clear value.
        ASSERT(!m_state.operand(node->operand()).isClear());
        break;

    case InitializeEntrypointArguments: {
        unsigned entrypointIndex = node->entrypointIndex();
        const Vector<FlushFormat>& argumentFormats = m_graph.m_argumentFormats[entrypointIndex];
        for (unsigned argument = 0; argument < argumentFormats.size(); ++argument) {
            AbstractValue& value = m_state.argument(argument);
            switch (argumentFormats[argument]) {
            case FlushedInt32:
                value.setNonCellType(SpecInt32Only);
                break;
            case FlushedBoolean:
                value.setNonCellType(SpecBoolean);
                break;
            case FlushedCell:
                value.setType(m_graph, SpecCellCheck);
                break;
            case FlushedJSValue:
                value.makeBytecodeTop();
                break;
            default:
                DFG_CRASH(m_graph, node, "Bad flush format for argument");
                break;
            }
        }
        break;
    }

    case VarargsLength: {
        clobberWorld();
        setTypeForNode(node, SpecInt32Only);
        break;
    }

    case LoadVarargs:
    case ForwardVarargs: {
        // FIXME: ForwardVarargs should check if the count becomes known, and if it does, it should turn
        // itself into a straight-line sequence of GetStack/PutStack.
        // https://bugs.webkit.org/show_bug.cgi?id=143071
        switch (node->op()) {
        case LoadVarargs:
            if (node->argumentsChild().useKind() != OtherUse)
                clobberWorld();
            break;
        case ForwardVarargs:
            break;
        default:
            DFG_CRASH(m_graph, node, "Bad opcode");
            break;
        }
        LoadVarargsData* data = node->loadVarargsData();
        m_state.operand(data->count).setNonCellType(SpecInt32Only);
        for (unsigned i = data->limit - 1; i--;)
            m_state.operand(data->start + i).makeHeapTop();
        break;
    }

    case ValueBitNot: {
        JSValue operand = forNode(node->child1()).value();
        if (operand && operand.isInt32()) {
            didFoldClobberWorld();
            int32_t a = operand.asInt32();
            setConstant(node, JSValue(~a));
            break;
        }

        if (node->child1().useKind() == BigInt32Use) {
#if USE(BIGINT32)
            setTypeForNode(node, SpecBigInt32);
#else
            RELEASE_ASSERT_NOT_REACHED();
#endif
        } else if (node->child1().useKind() == HeapBigIntUse) {
            // FIXME: We will want an arithmetic mode here that allows us to speculate or dictate
            // the format of our result:
            // https://bugs.webkit.org/show_bug.cgi?id=210982
            setTypeForNode(node, SpecBigInt);
        } else if (node->child1().useKind() == AnyBigIntUse)
            setTypeForNode(node, SpecBigInt);
        else {
            clobberWorld();
            setTypeForNode(node, SpecInt32Only | SpecBigInt);
        }

        break;
    }

    case ArithBitNot: {
        JSValue operand = forNode(node->child1()).value();
        if (operand && operand.isInt32()) {
            int32_t a = operand.asInt32();
            setConstant(node, JSValue(~a));
            break;
        }

        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }

    case ValueBitXor:
    case ValueBitAnd:
    case ValueBitOr:
    case ValueBitRShift:
    case ValueBitLShift: {
        if (handleConstantBinaryBitwiseOp(node))
            break;

        // FIXME: this use of binaryUseKind means that we cannot specialize to (for example) a HeapBigInt left-operand and a BigInt32 right-operand.
        // https://bugs.webkit.org/show_bug.cgi?id=210977
        if (node->binaryUseKind() == BigInt32Use) {
#if USE(BIGINT32)
            switch (node->op()) {
            case ValueBitXor:
            case ValueBitAnd:
            case ValueBitOr:
                setTypeForNode(node, SpecBigInt32);
                break;

            // FIXME: We should have inlined implementation that always returns BigInt32.
            // https://bugs.webkit.org/show_bug.cgi?id=210847
            case ValueBitRShift:
            case ValueBitLShift:
                setTypeForNode(node, SpecBigInt);
                break;
            default:
                DFG_CRASH(m_graph, node, "Incorrect DFG op");
            }
#else
            DFG_CRASH(m_graph, node, "No BigInt32 support");
#endif
        } else if (node->isBinaryUseKind(HeapBigIntUse)) {
            // FIXME: We will want an arithmetic mode here that allows us to speculate or dictate
            // the format of our result:
            // https://bugs.webkit.org/show_bug.cgi?id=210982
            setTypeForNode(node, SpecBigInt);
        } else if (node->binaryUseKind() == AnyBigIntUse)
            setTypeForNode(node, SpecBigInt);
        else {
            auto& value1 = forNode(node->child1());
            auto& value2 = forNode(node->child2());
            if (value1.isType(SpecInt32Only) && value2.isType(SpecInt32Only)) {
                didFoldClobberWorld();
                setTypeForNode(node, SpecInt32Only);
                break;
            }

            clobberWorld();
            if (value1.isType(SpecFullNumber) || value2.isType(SpecFullNumber))
                setTypeForNode(node, SpecInt32Only);
            else if (value1.isType(SpecBigInt) || value2.isType(SpecBigInt))
                setTypeForNode(node, SpecBigInt);
            else
                setTypeForNode(node, SpecInt32Only | SpecBigInt);
        }
        break;
    }
            
    case ArithBitAnd:
    case ArithBitOr:
    case ArithBitXor:
    case ArithBitRShift:
    case ArithBitLShift:
    case BitURShift: {
        if (node->child1().useKind() == UntypedUse || node->child2().useKind() == UntypedUse) {
            clobberWorld();
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        }

        if (handleConstantBinaryBitwiseOp(node))
            break;

        if (node->op() == ArithBitAnd) {
            ASSERT(node->child1().useKind() != UntypedUse);
            if (node->child2()->isInt32Constant() && !node->child2()->asInt32()) {
                setConstant(node, jsNumber(0));
                break;
            }

            if ((node->child2()->isInt32Constant() && node->child2()->asInt32() == -1) || (node->child1() == node->child2())) {
                m_state.setShouldTryConstantFolding(true);
                forNode(node) = forNode(node->child1());
                break;
            }

            if (isBoolInt32Speculation(forNode(node->child1()).m_type) || isBoolInt32Speculation(forNode(node->child2()).m_type)) {
                setNonCellTypeForNode(node, SpecBoolInt32);
                break;
            }
        }

        if (node->op() == ArithBitOr) {
            ASSERT(node->child1().useKind() != UntypedUse);
            if (node->child2()->isInt32Constant() && node->child2()->asInt32() == -1) {
                setConstant(node, jsNumber(-1));
                break;
            }

            if ((node->child2()->isInt32Constant() && !node->child2()->asInt32()) || (node->child1() == node->child2())) {
                m_state.setShouldTryConstantFolding(true);
                forNode(node) = forNode(node->child1());
                break;
            }
        }

        if (node->op() == ArithBitXor) {
            ASSERT(node->child1().useKind() != UntypedUse);
            // x ^ x -> 0 (XOR with itself results in 0)
            if (node->child1() == node->child2()) {
                setConstant(node, jsNumber(0));
                break;
            }

            // x ^ 0 -> x (Identity for XOR)
            if (node->child2()->isInt32Constant() && !node->child2()->asInt32()) {
                m_state.setShouldTryConstantFolding(true);
                forNode(node) = forNode(node->child1());
                break;
            }
        }

        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }
        
    case UInt32ToNumber: {
        JSValue child = forNode(node->child1()).value();
        if (doesOverflow(node->arithMode())) {
            if (enableInt52()) {
                if (child && child.isAnyInt()) {
                    int64_t machineInt = child.asAnyInt();
                    setConstant(node, jsNumber(static_cast<uint32_t>(machineInt)));
                    break;
                }
                setNonCellTypeForNode(node, SpecInt52Any);
                break;
            }
            if (child && child.isInt32()) {
                uint32_t value = child.asInt32();
                setConstant(node, jsNumber(value));
                break;
            }
            setNonCellTypeForNode(node, SpecAnyIntAsDouble);
            break;
        }
        if (child && child.isInt32()) {
            int32_t value = child.asInt32();
            if (value >= 0) {
                setConstant(node, jsNumber(value));
                break;
            }
        }
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }
        
    case BooleanToNumber: {
        JSValue concreteValue = forNode(node->child1()).value();
        if (concreteValue) {
            if (concreteValue.isBoolean())
                setConstant(node, jsNumber(concreteValue.asBoolean()));
            else
                setConstant(node, *m_graph.freeze(concreteValue));
            break;
        }
        AbstractValue& value = forNode(node);
        value = forNode(node->child1());
        if (node->child1().useKind() == UntypedUse && !(value.m_type & ~SpecBoolean))
            m_state.setShouldTryConstantFolding(true);
        if (value.m_type & SpecBoolean) {
            value.merge(SpecBoolInt32);
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
        setNonCellTypeForNode(node, SpecInt32Only);
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
        
        if (isBooleanSpeculation(forNode(node->child1()).m_type)) {
            setNonCellTypeForNode(node, SpecBoolInt32);
            break;
        }
        
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }
        
    case DoubleRep: {
        JSValue child = forNode(node->child1()).value();
        if (std::optional<double> number = child.toNumberFromPrimitive()) {
            setConstant(node, jsDoubleNumber(*number));
            break;
        }

        SpeculatedType type = forNode(node->child1()).m_type;
        switch (node->child1().useKind()) {
        case NotCellNorBigIntUse: {
            if (type & SpecOther) {
                type &= ~SpecOther;
                type |= SpecDoublePureNaN | SpecBoolInt32; // Null becomes zero, undefined becomes NaN.
            }
            if (type & SpecBoolean) {
                type &= ~SpecBoolean;
                type |= SpecBoolInt32; // True becomes 1, false becomes 0.
            }
            type &= SpecBytecodeNumber;
            break;
        }

        case Int52RepUse:
        case NumberUse:
        case RealNumberUse:
            break;

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        setNonCellTypeForNode(node, type);
        forNode(node).fixTypeForRepresentation(m_graph, node);
        break;
    }
        
    case Int52Rep: {
        JSValue child = forNode(node->child1()).value();
        if (child && child.isAnyInt()) {
            setConstant(node, child);
            break;
        }

        setTypeForNode(node, forNode(node->child1()).m_type);
        forNode(node).fixTypeForRepresentation(m_graph, node);
        break;
    }
        
    case ValueRep: {
        JSValue value = forNode(node->child1()).value();
        if (value) {
            if (node->child1().useKind() == Int52RepUse) {
                if (auto int32 = value.tryGetAsInt32())
                    value = jsNumber(*int32);
            }
            setConstant(node, value);
            break;
        }
        
        setTypeForNode(node, forNode(node->child1()).m_type & ~SpecDoubleImpureNaN);
        forNode(node).fixTypeForRepresentation(m_graph, node);
        break;
    }

    case ValueSub:
    case ValueAdd: {
        if (node->isBinaryUseKind(HeapBigIntUse)) {
            // FIXME: We will want an arithmetic mode here that allows us to speculate or dictate
            // the format of our result:
            // https://bugs.webkit.org/show_bug.cgi?id=210982
            setTypeForNode(node, SpecBigInt);
        } else if (node->isBinaryUseKind(AnyBigIntUse))
            setTypeForNode(node, SpecBigInt);
        else if (node->isBinaryUseKind(BigInt32Use))
            setTypeForNode(node, SpecBigInt32);
        else {
            DFG_ASSERT(m_graph, node, node->binaryUseKind() == UntypedUse);
            clobberWorld();
            if (node->op() == ValueAdd)
                setTypeForNode(node, SpecString | SpecBytecodeNumber | SpecBigInt);
            else {
                auto& value1 = forNode(node->child1());
                auto& value2 = forNode(node->child2());
                if (value1.isType(SpecFullNumber) || value2.isType(SpecFullNumber))
                    setTypeForNode(node, SpecBytecodeNumber);
                else if (value1.isType(SpecBigInt) || value2.isType(SpecBigInt))
                    setTypeForNode(node, SpecBigInt);
                else
                    setTypeForNode(node, SpecBytecodeNumber | SpecBigInt);
            }
        }
        break;
    }

    case StrCat: {
        bool goodToGo = true;
        m_graph.doToChildren(
            node,
            [&](Edge& edge) {
                if (m_state.forNode(edge).isType(SpecString))
                    return;
                goodToGo = false;
            });
        if (goodToGo)
            m_state.setShouldTryConstantFolding(true);
        setTypeForNode(node, SpecString);
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
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Int52RepUse:
            if (left && right && left.isAnyInt() && right.isAnyInt()) {
                JSValue result = jsNumber(left.asAnyInt() + right.asAnyInt());
                if (result.isAnyInt()) {
                    setConstant(node, result);
                    break;
                }
            }
            setNonCellTypeForNode(node, SpecInt52Any);
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsDoubleNumber(left.asNumber() + right.asNumber()));
                break;
            }
            setNonCellTypeForNode(node, 
                typeOfDoubleSum(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
        
    case AtomicsIsLockFree: {
        if (m_graph.child(node, 0).useKind() != Int32Use)
            clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case ArithClz32: {
        JSValue operand = forNode(node->child1()).value();
        if (std::optional<double> number = operand.toNumberFromPrimitive()) {
            switch (node->child1().useKind()) {
            case Int32Use:
            case KnownInt32Use:
                break;
            default:
                didFoldClobberWorld();
                break;
            }
            uint32_t value = toUInt32(*number);
            setConstant(node, jsNumber(clz(value)));
            break;
        }
        switch (node->child1().useKind()) {
        case Int32Use:
        case KnownInt32Use:
            break;
        default:
            clobberWorld();
            break;
        }
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }

    case MakeRope: {
        unsigned numberOfRemovedChildren = 0;
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge& edge = node->children.child(i);
            if (!edge)
                break;
            JSValue childConstant = m_state.forNode(edge).value();
            if (!childConstant)
                continue;
            if (!childConstant.isString())
                continue;
            if (asString(childConstant)->length())
                continue;
            ++numberOfRemovedChildren;
        }

        if (numberOfRemovedChildren)
            m_state.setShouldTryConstantFolding(true);
        setForNode(node, m_vm.stringStructure.get());
        break;
    }

    case MakeAtomString: {
        unsigned numberOfRemovedChildren = 0;
        for (unsigned i = 0; i < AdjacencyList::Size; ++i) {
            Edge& edge = node->children.child(i);
            if (!edge)
                break;
            JSValue childConstant = m_state.forNode(edge).value();
            if (!childConstant)
                continue;
            if (!childConstant.isString())
                continue;
            if (asString(childConstant)->length())
                continue;
            ++numberOfRemovedChildren;
        }

        if (numberOfRemovedChildren)
            m_state.setShouldTryConstantFolding(true);
        setTypeForNode(node, SpecStringIdent);
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
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Int52RepUse:
            if (left && right && left.isAnyInt() && right.isAnyInt()) {
                JSValue result = jsNumber(left.asAnyInt() - right.asAnyInt());
                if (result.isAnyInt()) {
                    setConstant(node, result);
                    break;
                }
            }
            setNonCellTypeForNode(node, SpecInt52Any);
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsDoubleNumber(left.asNumber() - right.asNumber()));
                break;
            }
            setNonCellTypeForNode(node, 
                typeOfDoubleDifference(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
            break;
        case UntypedUse:
            clobberWorld();
            setNonCellTypeForNode(node, SpecBytecodeNumber);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
        
    case ValueNegate: {
        // FIXME: we could do much smarter things for BigInts, see ValueAdd/ValueSub.
        clobberWorld();
        setTypeForNode(node, SpecBytecodeNumber | SpecBigInt);
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
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Int52RepUse:
            if (child && child.isAnyInt()) {
                double doubleResult;
                if (shouldCheckNegativeZero(node->arithMode()))
                    doubleResult = -child.asNumber();
                else
                    doubleResult = 0 - child.asNumber();
                JSValue valueResult = jsNumber(doubleResult);
                if (valueResult.isAnyInt()) {
                    setConstant(node, valueResult);
                    break;
                }
            }
            setNonCellTypeForNode(node, SpecInt52Any);
            break;
        case DoubleRepUse:
            if (child && child.isNumber()) {
                setConstant(node, jsDoubleNumber(-child.asNumber()));
                break;
            }
            setNonCellTypeForNode(node, 
                typeOfDoubleNegation(
                    forNode(node->child1()).m_type));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }

    case Inc:
    case Dec: {
        // FIXME: support some form of constant folding here.
        // https://bugs.webkit.org/show_bug.cgi?id=204258
        switch (node->child1().useKind()) {
        case Int32Use:
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Int52RepUse:
            setNonCellTypeForNode(node, SpecInt52Any);
            break;
        case DoubleRepUse:
            setNonCellTypeForNode(node, typeOfDoubleIncOrDec(forNode(node->child1()).m_type));
            break;
        case HeapBigIntUse:
            // FIXME: We will want an arithmetic mode here that allows us to speculate or dictate
            // the format of our result:
            // https://bugs.webkit.org/show_bug.cgi?id=210982
            setTypeForNode(node, SpecBigInt);
            break;
        case AnyBigIntUse:
        case BigInt32Use:
            setTypeForNode(node, SpecBigInt);
            break;
        default:
            setTypeForNode(node, SpecBytecodeNumber | SpecBigInt);
            clobberWorld(); // Because of the call to ToNumeric()
            break;
        }
        break;
    }
        
    case ValuePow: {
        JSValue childX = forNode(node->child1()).value();
        JSValue childY = forNode(node->child2()).value();
        if (childX && childY && childX.isNumber() && childY.isNumber()) {
            // We need to call `didFoldClobberWorld` here because this path is only possible
            // when node->useKind is UntypedUse. In the case of AnyBigIntUse or friends, children will be
            // cleared by `AbstractInterpreter::executeEffects`.
            didFoldClobberWorld();
            // Our boxing scheme here matches what we do in operationValuePow.
            setConstant(node, jsNumber(operationMathPow(childX.asNumber(), childY.asNumber())));
            break;
        }

        bool isBigIntBinaryUsedKind = node->isBinaryUseKind(HeapBigIntUse) || node->isBinaryUseKind(AnyBigIntUse) || node->isBinaryUseKind(BigInt32Use);
        if (node->mustGenerate() && isBigIntBinaryUsedKind) {
            if (childY && childY.isBigInt() && !childY.isNegativeBigInt()) {
                node->clearFlags(NodeMustGenerate);
                m_state.setShouldTryConstantFolding(true);
            }
        }

        if (node->isBinaryUseKind(HeapBigIntUse)) {
            // FIXME: We will want an arithmetic mode here that allows us to speculate or dictate
            // the format of our result:
            // https://bugs.webkit.org/show_bug.cgi?id=210982
            setTypeForNode(node, SpecBigInt);
        } else if (node->binaryUseKind() == AnyBigIntUse || node->binaryUseKind() == BigInt32Use)
            setTypeForNode(node, SpecBigInt);
        else {
            clobberWorld();
            setTypeForNode(node, SpecBytecodeNumber | SpecBigInt);
        }
        break;
    }

    case ValueMul: {
        // FIXME: why is this code not shared with ValueSub?
        if (node->isBinaryUseKind(HeapBigIntUse)) {
            // FIXME: We will want an arithmetic mode here that allows us to speculate or dictate
            // the format of our result:
            // https://bugs.webkit.org/show_bug.cgi?id=210982
            setTypeForNode(node, SpecBigInt);
        } else if (node->isBinaryUseKind(AnyBigIntUse))
            setTypeForNode(node, SpecBigInt);
        else if (node->isBinaryUseKind(BigInt32Use))
            setTypeForNode(node, SpecBigInt32);
        else {
            clobberWorld();
            auto& value1 = forNode(node->child1());
            auto& value2 = forNode(node->child2());
            if (value1.isType(SpecFullNumber) || value2.isType(SpecFullNumber))
                setTypeForNode(node, SpecBytecodeNumber);
            else if (value1.isType(SpecBigInt) || value2.isType(SpecBigInt))
                setTypeForNode(node, SpecBigInt);
            else
                setTypeForNode(node, SpecBytecodeNumber | SpecBigInt);
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
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Int52RepUse:
            if (left && right && left.isAnyInt() && right.isAnyInt()) {
                double doubleResult = left.asNumber() * right.asNumber();
                if (!shouldCheckNegativeZero(node->arithMode()))
                    doubleResult += 0;
                JSValue valueResult = jsNumber(doubleResult);
                if (valueResult.isAnyInt()) {
                    setConstant(node, valueResult);
                    break;
                }
            }
            setNonCellTypeForNode(node, SpecInt52Any);
            break;
        case DoubleRepUse:
            if (left && right && left.isNumber() && right.isNumber()) {
                setConstant(node, jsDoubleNumber(left.asNumber() * right.asNumber()));
                break;
            }
            setNonCellTypeForNode(node, 
                typeOfDoubleProduct(
                    forNode(node->child1()).m_type, forNode(node->child2()).m_type));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
        
    case ValueMod:
    case ValueDiv: {
        if (handleConstantDivOp(node))
            break;

        bool isBigIntBinaryUsedKind = node->isBinaryUseKind(HeapBigIntUse) || node->isBinaryUseKind(AnyBigIntUse) || node->isBinaryUseKind(BigInt32Use);
        if (node->mustGenerate() && isBigIntBinaryUsedKind) {
            JSValue left = forNode(node->child2()).value();
            if (left && left.isBigInt() && !left.isZeroBigInt()) {
                node->clearFlags(NodeMustGenerate);
                m_state.setShouldTryConstantFolding(true);
            }
        }

        if (node->isBinaryUseKind(HeapBigIntUse)) {
            // FIXME: We will want an arithmetic mode here that allows us to speculate or dictate
            // the format of our result:
            // https://bugs.webkit.org/show_bug.cgi?id=210982
            setTypeForNode(node, SpecBigInt);
        } else if (node->binaryUseKind() == AnyBigIntUse || node->binaryUseKind() == BigInt32Use)
            setTypeForNode(node, SpecBigInt);
        else {
            clobberWorld();
            auto& value1 = forNode(node->child1());
            auto& value2 = forNode(node->child2());
            if (value1.isType(SpecFullNumber) || value2.isType(SpecFullNumber))
                setTypeForNode(node, SpecBytecodeNumber);
            else if (value1.isType(SpecBigInt) || value2.isType(SpecBigInt))
                setTypeForNode(node, SpecBigInt);
            else
                setTypeForNode(node, SpecBytecodeNumber | SpecBigInt);
        }
        break;
    }

    case ArithMod:
    case ArithDiv: {
        if (handleConstantDivOp(node))
            break;

        switch (node->binaryUseKind()) {
        case Int32Use:
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case DoubleRepUse:
            if (node->op() == ArithDiv) {
                setNonCellTypeForNode(node, 
                    typeOfDoubleQuotient(
                        forNode(node->child1()).m_type, forNode(node->child2()).m_type));
            } else {
                setNonCellTypeForNode(node, 
                    typeOfDoubleBinaryOp(
                        forNode(node->child1()).m_type, forNode(node->child2()).m_type));
            }
            
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }

    case ArithMin: {
        switch (m_graph.child(node, 0).useKind()) {
        case Int32Use: {
            std::optional<int32_t> result;
            bool found = true;
            m_graph.doToChildren(node, [&](Edge& child) {
                JSValue constant = forNode(child).value();
                if (!constant || !constant.isInt32()) {
                    found = false;
                    return;
                }
                if (!result)
                    result = constant.asInt32();
                else
                    result = std::min(result.value(), constant.asInt32());
            });
            if (found && result) {
                setConstant(node, jsNumber(result.value()));
                break;
            }
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        }
        case DoubleRepUse: {
            std::optional<double> result;
            bool found = true;
            SpeculatedType type = SpecNone;
            m_graph.doToChildren(node, [&](Edge& child) {
                type = typeOfDoubleMinMax(type, forNode(child).m_type);
                JSValue constant = forNode(child).value();
                if (!constant || !constant.isNumber()) {
                    found = false;
                    return;
                }
                if (!result)
                    result = constant.asNumber();
                else {
                    double a = result.value();
                    double b = constant.asNumber();
                    result = a < b || (!a && !b && std::signbit(a)) ? a : (b <= a ? b : a + b);
                }
            });
            if (found && result) {
                setConstant(node, jsDoubleNumber(result.value()));
                break;
            }
            setNonCellTypeForNode(node, type);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
            
    case ArithMax: {
        switch (m_graph.child(node, 0).useKind()) {
        case Int32Use: {
            std::optional<int32_t> result;
            bool found = true;
            m_graph.doToChildren(node, [&](Edge& child) {
                JSValue constant = forNode(child).value();
                if (!constant || !constant.isInt32()) {
                    found = false;
                    return;
                }
                if (!result)
                    result = constant.asInt32();
                else
                    result = std::max(result.value(), constant.asInt32());
            });
            if (found && result) {
                setConstant(node, jsNumber(result.value()));
                break;
            }
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        }
        case DoubleRepUse: {
            std::optional<double> result;
            bool found = true;
            SpeculatedType type = SpecNone;
            m_graph.doToChildren(node, [&](Edge& child) {
                type = typeOfDoubleMinMax(type, forNode(child).m_type);
                JSValue constant = forNode(child).value();
                if (!constant || !constant.isNumber()) {
                    found = false;
                    return;
                }
                if (!result)
                    result = constant.asNumber();
                else {
                    double a = result.value();
                    double b = constant.asNumber();
                    result = a > b || (!a && !b && !std::signbit(a)) ? a : (b >= a ? b : a + b);
                }
            });
            if (found && result) {
                setConstant(node, jsDoubleNumber(result.value()));
                break;
            }
            setNonCellTypeForNode(node, type);
            break;
        }
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
            if (std::optional<double> number = child.toNumberFromPrimitive()) {
                JSValue result = jsNumber(std::abs(*number));
                if (result.isInt32()) {
                    setConstant(node, result);
                    break;
                }
            }
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case DoubleRepUse:
            if (std::optional<double> number = child.toNumberFromPrimitive()) {
                setConstant(node, jsDoubleNumber(std::abs(*number)));
                break;
            }
            setNonCellTypeForNode(node, typeOfDoubleAbs(forNode(node->child1()).m_type));
            break;
        default:
            DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());
            clobberWorld();
            setNonCellTypeForNode(node, SpecBytecodeNumber);
            break;
        }
        break;
    }

    case ArithPow: {
        JSValue childY = forNode(node->child2()).value();
        if (childY && childY.isNumber()) {
            if (!childY.asNumber()) {
                setConstant(node, jsDoubleNumber(1));
                break;
            }

            JSValue childX = forNode(node->child1()).value();
            if (childX && childX.isNumber()) {
                setConstant(node, jsDoubleNumber(operationMathPow(childX.asNumber(), childY.asNumber())));
                break;
            }
        }
        setNonCellTypeForNode(node, typeOfDoublePow(forNode(node->child1()).m_type, forNode(node->child2()).m_type));
        break;
    }

    case ArithRandom: {
        setNonCellTypeForNode(node, SpecDoubleReal);
        break;
    }

    case ArithRound:
    case ArithFloor:
    case ArithCeil:
    case ArithTrunc: {
        JSValue operand = forNode(node->child1()).value();
        if (std::optional<double> number = operand.toNumberFromPrimitive()) {
            if (node->child1().useKind() != DoubleRepUse)
                didFoldClobberWorld();
            
            double roundedValue = 0;
            if (node->op() == ArithRound)
                roundedValue = Math::roundDouble(*number);
            else if (node->op() == ArithFloor)
                roundedValue = Math::floorDouble(*number);
            else if (node->op() == ArithCeil)
                roundedValue = Math::ceilDouble(*number);
            else {
                ASSERT(node->op() == ArithTrunc);
                roundedValue = trunc(*number);
            }

            if (node->child1().useKind() == UntypedUse) {
                setConstant(node, jsNumber(roundedValue));
                break;
            }
            if (producesInteger(node->arithRoundingMode())) {
                int32_t roundedValueAsInt32 = static_cast<int32_t>(roundedValue);
                if (roundedValueAsInt32 == roundedValue) {
                    if (shouldCheckNegativeZero(node->arithRoundingMode())) {
                        if (roundedValueAsInt32 || !std::signbit(roundedValue)) {
                            setConstant(node, jsNumber(roundedValueAsInt32));
                            break;
                        }
                    } else {
                        setConstant(node, jsNumber(roundedValueAsInt32));
                        break;
                    }
                }
            } else {
                setConstant(node, jsDoubleNumber(roundedValue));
                break;
            }
        }
        if (node->child1().useKind() == DoubleRepUse) {
            if (producesInteger(node->arithRoundingMode()))
                setNonCellTypeForNode(node, SpecInt32Only);
            else if (node->child1().useKind() == DoubleRepUse)
                setNonCellTypeForNode(node, typeOfDoubleRounding(forNode(node->child1()).m_type));
        } else {
            DFG_ASSERT(m_graph, node, node->child1().useKind() == UntypedUse, node->child1().useKind());
            clobberWorld();
            setNonCellTypeForNode(node, SpecBytecodeNumber);
        }
        break;
    }
            
    case ArithSqrt:
        executeDoubleUnaryOpEffects(node, [](double value) -> double { return sqrt(value); });
        break;

    case ArithFRound:
        executeDoubleUnaryOpEffects(node, [](double value) -> double { return static_cast<float>(value); });
        break;

    case ArithF16Round:
        executeDoubleUnaryOpEffects(node, [](double value) -> double { return static_cast<double>(Float16 { value }); });
        break;

    case ArithUnary:
        executeDoubleUnaryOpEffects(node, arithUnaryFunction(node->arithUnaryType()));
        break;
            
    case ToBoolean:
    case LogicalNot: {
        TriState result = booleanResult(node, forNode(node->child1()));
        if (result == TriState::Indeterminate) {
            setNonCellTypeForNode(node, SpecBoolean);
            break;
        }

        bool resultAsBool = result == TriState::True;
        if (node->op() == LogicalNot)
            resultAsBool = !resultAsBool;
        setConstant(node, jsBoolean(resultAsBool));
        break;
    }

    case MapHash: {
        if (JSValue key = forNode(node->child1()).value()) {
            if (std::optional<uint32_t> hash = concurrentJSMapHash(key)) {
                // Although C++ code uses uint32_t for the hash, the closest type in DFG IR is Int32
                // and that's what MapHash returns. So, we have to cast to int32_t to avoid large
                // unsigned values becoming doubles. This casting between signed and unsigned
                // happens in the assembly code we emit when we don't constant fold this node.
                setConstant(node, jsNumber(static_cast<int32_t>(*hash)));
                break;
            }
        }
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }

    case NormalizeMapKey: {
        if (JSValue key = forNode(node->child1()).value()) {
            setConstant(node, *m_graph.freeze(normalizeMapKey(key)));
            break;
        }

        SpeculatedType typesNeedingNormalization = (SpecFullNumber & ~SpecInt32Only) | SpecHeapBigInt;
        if (!(forNode(node->child1()).m_type & typesNeedingNormalization)) {
            m_state.setShouldTryConstantFolding(true);
            forNode(node) = forNode(node->child1());
            break;
        }

        makeHeapTopForNode(node);
        break;
    }

    case StringValueOf: {
        clobberWorld();
        setTypeForNode(node, SpecString);
        break;
    }

    case StringSubstring:
    case StringSlice: {
        setTypeForNode(node, SpecString);
        break;
    }

    case ToLowerCase: {
        AbstractValue& property = forNode(m_graph.child(node, 0));
        if (JSValue value = property.value()) {
            if (value.isString()) {
                JSString* string = asString(value);
                if (const StringImpl* a = asString(string)->tryGetValueImpl()) {
                    bool lower = true;
                    for (unsigned index = 0; index < a->length(); ++index) {
                        UChar character = a->at(index);
                        if (!isASCII(character) || isASCIIUpper(character)) {
                            lower = false;
                            break;
                        }
                    }
                    if (lower) {
                        setConstant(node, *m_graph.freeze(string));
                        break;
                    }
                }
            }
        }
        setTypeForNode(node, SpecString);
        break;
    }

    case MapIterationEntryKey:
    case MapIterationEntryValue:
    case MapIteratorKey:
    case MapIteratorValue:
    case LoadMapValue:
    case ExtractValueFromWeakMapGet:
        makeHeapTopForNode(node);
        break;

    case SetAdd:
    case MapSet:
        break;

    case MapGet:
        clearForNode(node);
        break;

    case MapIterationEntry:
        setTypeForNode(node, SpecInt32Only);
        break;

    case MapStorage:
    case MapIterationNext:
        setTypeForNode(node, SpecCellOther);
        break;

    case MapIteratorNext:
    case IsEmptyStorage:
        setTypeForNode(node, SpecBoolean);
        break;

    case MapOrSetDelete:
        setNonCellTypeForNode(node, SpecBoolean);
        break;

    case WeakSetAdd:
    case WeakMapSet:
        break;

    case WeakMapGet:
        makeBytecodeTopForNode(node);
        break;

    case IsEmpty:
    case TypeOfIsUndefined:
    case TypeOfIsObject:
    case TypeOfIsFunction:
    case IsUndefinedOrNull:
    case IsBoolean:
    case IsNumber:
    case IsBigInt:
    case NumberIsInteger:
    case IsObject:
    case IsCallable:
    case IsConstructor:
    case IsCellWithType:
    case IsTypedArrayView: {
        AbstractValue& child = forNode(node->child1());
        if (child.value()) {
            bool constantWasSet = true;
            switch (node->op()) {
            case IsCellWithType:
                setConstant(node, jsBoolean(child.value().isCell() && child.value().asCell()->type() == node->queriedType()));
                break;
            case TypeOfIsUndefined:
                setConstant(node, jsBoolean(
                    child.value().isCell()
                    ? child.value().asCell()->structure()->masqueradesAsUndefined(m_codeBlock->globalObjectFor(node->origin.semantic))
                    : child.value().isUndefined()));
                break;
            case TypeOfIsObject: {
                TriState result = jsTypeofIsObjectWithConcurrency<Concurrency::ConcurrentThread>(m_codeBlock->globalObjectFor(node->origin.semantic), child.value());
                if (result != TriState::Indeterminate)
                    setConstant(node, jsBoolean(result == TriState::True));
                else
                    constantWasSet = false;
                break;
            }
            case TypeOfIsFunction: {
                TriState result = jsTypeofIsFunctionWithConcurrency<Concurrency::ConcurrentThread>(m_codeBlock->globalObjectFor(node->origin.semantic), child.value());
                if (result != TriState::Indeterminate)
                    setConstant(node, jsBoolean(result == TriState::True));
                else
                    constantWasSet = false;
                break;
            }
            case IsUndefinedOrNull:
                setConstant(node, jsBoolean(child.value().isUndefinedOrNull()));
                break;
            case IsBoolean:
                setConstant(node, jsBoolean(child.value().isBoolean()));
                break;
            case IsNumber:
                setConstant(node, jsBoolean(child.value().isNumber()));
                break;
            case IsBigInt:
                setConstant(node, jsBoolean(child.value().isBigInt()));
                break;
            case NumberIsInteger:
                setConstant(node, jsBoolean(NumberConstructor::isIntegerImpl(child.value())));
                break;
            case IsObject:
                setConstant(node, jsBoolean(child.value().isObject()));
                break;
            case IsCallable: {
                TriState result = child.value().isCallableWithConcurrency<Concurrency::ConcurrentThread>();
                if (result != TriState::Indeterminate)
                    setConstant(node, jsBoolean(result == TriState::True));
                else
                    constantWasSet = false;
                break;
            }
            case IsConstructor: {
                TriState result = child.value().isConstructorWithConcurrency<Concurrency::ConcurrentThread>();
                if (result != TriState::Indeterminate)
                    setConstant(node, jsBoolean(result == TriState::True));
                else
                    constantWasSet = false;
                break;
            }
            case IsEmpty:
                setConstant(node, jsBoolean(child.value().isEmpty()));
                break;
            case IsTypedArrayView:
                setConstant(node, jsBoolean(child.value().isObject() && isTypedView(child.value().getObject()->type())));
                break;
            default:
                constantWasSet = false;
                break;
            }
            if (constantWasSet)
                break;
        }

        if (!(child.m_type & ~SpecCell)) {
            if (child.m_structure.isFinite()) {
                bool constantWasSet = false;
                switch (node->op()) {
                case IsCellWithType: {
                    bool ok = true;
                    std::optional<bool> result;
                    child.m_structure.forEach(
                        [&] (RegisteredStructure structure) {
                            bool matched = structure->typeInfo().type() == node->queriedType();
                            if (!result)
                                result = matched;
                            else {
                                if (result.value() != matched)
                                    ok = false;
                            }
                        });
                    if (ok && result) {
                        setConstant(node, jsBoolean(result.value()));
                        constantWasSet = true;
                    }
                    break;
                }
                default:
                    break;
                }
                if (constantWasSet)
                    break;
            }
        }

        // FIXME: This code should really use AbstractValue::isType() and
        // AbstractValue::couldBeType().
        // https://bugs.webkit.org/show_bug.cgi?id=146870
        
        bool constantWasSet = false;
        switch (node->op()) {
        case IsEmpty: {
            if (child.m_type && !(child.m_type & SpecEmpty)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }

            if (child.m_type && !(child.m_type & ~SpecEmpty)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }

            break;
        }
        case TypeOfIsUndefined:
            // FIXME: Use the masquerades-as-undefined watchpoint thingy.
            // https://bugs.webkit.org/show_bug.cgi?id=144456
            
            if (!(child.m_type & (SpecOther | SpecObjectOther))) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            
            break;
        case TypeOfIsObject:
            // FIXME: Use the masquerades-as-undefined watchpoint thingy.
            // https://bugs.webkit.org/show_bug.cgi?id=144456
            
            // These expressions are complicated to parse. A helpful way to parse this is that
            // "!(T & ~S)" means "T is a subset of S". Conversely, "!(T & S)" means "T is a
            // disjoint set from S". Things like "T - S" means that, provided that S is a
            // subset of T, it's the "set of all things in T but not in S". Things like "T | S"
            // mean the "union of T and S".
            
            // Is the child's type an object that isn't an other-object (i.e. object that could
            // have masquaredes-as-undefined traps) and isn't a function? Then: we should fold
            // this to true.
            if (!(child.m_type & ~(SpecObject - SpecTypeofMightBeFunction))) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }
            
            // Is the child's type definitely not either of: an object that isn't a function,
            // or either undefined or null? Then: we should fold this to false. This means
            // for example that if it's any non-function object, including those that have
            // masquerades-as-undefined traps, then we don't fold. It also means we won't fold
            // if it's undefined-or-null, since the type bits don't distinguish between
            // undefined (which should fold to false) and null (which should fold to true).
            if (!(child.m_type & ((SpecObject - SpecFunction) | SpecOther))) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            
            break;
        case IsUndefinedOrNull:
            if (!(child.m_type & ~SpecOther)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }

            if (!(child.m_type & SpecOther)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            break;
        case IsBoolean:
            if (!(child.m_type & ~SpecBoolean)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }
            
            if (!(child.m_type & SpecBoolean)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            
            break;
        case IsNumber:
            if (!(child.m_type & ~SpecFullNumber)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }
            
            if (!(child.m_type & SpecFullNumber)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            
            break;
        case IsBigInt:
            if (!(child.m_type & ~SpecBigInt)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }

            if (!(child.m_type & SpecBigInt)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }

            // FIXME: if the SpeculatedType informs us that we won't have a BigInt32 (or that we won't have a HeapBigInt), then we can transform this node into a IsCellWithType(HeapBigIntType) (or a hypothetical IsBigInt32 node).

            break;
        case NumberIsInteger:
            if (!(child.m_type & ~SpecInt32Only)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }
            
            if (!(child.m_type & SpecFullNumber)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            
            break;

        case IsObject:
            if (!(child.m_type & ~SpecObject)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }
            
            if (!(child.m_type & SpecObject)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            
            break;
        case TypeOfIsFunction:
        case IsCallable:
            if (!(child.m_type & ~SpecFunction)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }
            
            if (!(child.m_type & SpecTypeofMightBeFunction)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            break;

        case IsConstructor:
            // FIXME: We can speculate constructability from child's m_structure.
            // https://bugs.webkit.org/show_bug.cgi?id=211796
            break;

        case IsCellWithType: {
            std::optional<SpeculatedType> filter = node->speculatedTypeForQuery();
            if (!filter) {
                if (!(child.m_type & SpecCell)) {
                    setConstant(node, jsBoolean(false));
                    constantWasSet = true;
                }
                break;
            }
            if (!(child.m_type & ~filter.value())) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }
            if (!(child.m_type & filter.value())) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            break;
        }

        case IsTypedArrayView:
            if (!(child.m_type & ~SpecTypedArrayView)) {
                setConstant(node, jsBoolean(true));
                constantWasSet = true;
                break;
            }
            if (!(child.m_type & SpecTypedArrayView)) {
                setConstant(node, jsBoolean(false));
                constantWasSet = true;
                break;
            }
            break;

        default:
            break;
        }
        if (constantWasSet)
            break;
        
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case GlobalIsNaN: {
        AbstractValue& child = forNode(node->child1());
        if (JSValue value = child.value(); value && value.isNumber()) {
            if (node->child1().useKind() != DoubleRepUse)
                didFoldClobberWorld();
            setConstant(node, jsBoolean(std::isnan(value.asNumber())));
            break;
        }
        if (node->child1().useKind() != DoubleRepUse)
            clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case NumberIsNaN: {
        AbstractValue& child = forNode(node->child1());
        if (JSValue value = child.value()) {
            setConstant(node, jsBoolean(value.isNumber() && std::isnan(value.asNumber())));
            break;
        }
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case TypeOf: {
        JSValue child = forNode(node->child1()).value();
        AbstractValue& abstractChild = forNode(node->child1());
        if (child) {
            if (JSString* typeString = jsTypeStringForValueWithConcurrency(m_vm, m_codeBlock->globalObjectFor(node->origin.semantic), child, Concurrency::ConcurrentThread)) {
                setConstant(node, *m_graph.freeze(typeString));
                break;
            }
        }
        
        if (isFullNumberSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(m_vm.smallStrings.numberString()));
            break;
        }
        
        if (isStringSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(m_vm.smallStrings.stringString()));
            break;
        }

        // FIXME: We could use the masquerades-as-undefined watchpoint here.
        // https://bugs.webkit.org/show_bug.cgi?id=144456
        if (!(abstractChild.m_type & ~(SpecObject - SpecTypeofMightBeFunction))) {
            setConstant(node, *m_graph.freeze(m_vm.smallStrings.objectString()));
            break;
        }
        
        if (isFunctionSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(m_vm.smallStrings.functionString()));
            break;
        }
        
        if (isBooleanSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(m_vm.smallStrings.booleanString()));
            break;
        }

        if (isSymbolSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(m_vm.smallStrings.symbolString()));
            break;
        }

        if (isBigIntSpeculation(abstractChild.m_type)) {
            setConstant(node, *m_graph.freeze(m_vm.smallStrings.bigintString()));
            break;
        }

        setTypeForNode(node, SpecStringIdent);
        break;
    }

    case CompareBelow:
    case CompareBelowEq: {
        JSValue leftConst = forNode(node->child1()).value();
        JSValue rightConst = forNode(node->child2()).value();
        if (leftConst && rightConst) {
            if (leftConst.isInt32() && rightConst.isInt32()) {
                uint32_t a = static_cast<uint32_t>(leftConst.asInt32());
                uint32_t b = static_cast<uint32_t>(rightConst.asInt32());
                switch (node->op()) {
                case CompareBelow:
                    setConstant(node, jsBoolean(a < b));
                    break;
                case CompareBelowEq:
                    setConstant(node, jsBoolean(a <= b));
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                break;
            }
        }

        if (node->child1() == node->child2()) {
            switch (node->op()) {
            case CompareBelow:
                setConstant(node, jsBoolean(false));
                break;
            case CompareBelowEq:
                setConstant(node, jsBoolean(true));
                break;
            default:
                DFG_CRASH(m_graph, node, "Unexpected node type");
                break;
            }
            break;
        }
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case CompareLess:
    case CompareLessEq:
    case CompareGreater:
    case CompareGreaterEq:
    case CompareEq: {
        bool isClobbering = node->isBinaryUseKind(UntypedUse);
        
        if (isClobbering)
            didFoldClobberWorld();
        
        JSValue leftConst = forNode(node->child1()).value();
        JSValue rightConst = forNode(node->child2()).value();
        if (leftConst && rightConst) {
            if (leftConst.isNumber() && rightConst.isNumber()) {
                auto compareNumber = [&](double a, double b) {
                    switch (node->op()) {
                    case CompareLess:
                        return jsBoolean(a < b);
                    case CompareLessEq:
                        return jsBoolean(a <= b);
                    case CompareGreater:
                        return jsBoolean(a > b);
                    case CompareGreaterEq:
                        return jsBoolean(a >= b);
                    case CompareEq:
                        return jsBoolean(a == b);
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                        break;
                    }
                };
                double a = leftConst.asNumber();
                double b = rightConst.asNumber();
                setConstant(node, compareNumber(a, b));
                break;
            }

            if (leftConst.isBigInt() && rightConst.isBigInt()) {
                switch (node->op()) {
                case CompareLess:
                    setConstant(node, jsBoolean(bigIntCompareResult(compareBigInt(leftConst, rightConst), JSBigInt::ComparisonMode::LessThan)));
                    break;
                case CompareLessEq:
                    setConstant(node, jsBoolean(bigIntCompareResult(compareBigInt(leftConst, rightConst), JSBigInt::ComparisonMode::LessThanOrEqual)));
                    break;
                case CompareGreater:
                    setConstant(node, jsBoolean(bigIntCompareResult(compareBigInt(rightConst, leftConst), JSBigInt::ComparisonMode::LessThan)));
                    break;
                case CompareGreaterEq:
                    setConstant(node, jsBoolean(bigIntCompareResult(compareBigInt(rightConst, leftConst), JSBigInt::ComparisonMode::LessThanOrEqual)));
                    break;
                case CompareEq:
                    setConstant(node, jsBoolean(compareBigInt(leftConst, rightConst) == JSBigInt::ComparisonResult::Equal));
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                break;
            }
            
            if (leftConst.isString() && rightConst.isString()) {
                const StringImpl* a = asString(leftConst)->tryGetValueImpl();
                const StringImpl* b = asString(rightConst)->tryGetValueImpl();
                if (a && b) {
                    bool result;
                    if (node->op() == CompareEq)
                        result = WTF::equal(a, b);
                    else if (node->op() == CompareLess)
                        result = codePointCompare(a, b) < 0;
                    else if (node->op() == CompareLessEq)
                        result = codePointCompare(a, b) <= 0;
                    else if (node->op() == CompareGreater)
                        result = codePointCompare(a, b) > 0;
                    else if (node->op() == CompareGreaterEq)
                        result = codePointCompare(a, b) >= 0;
                    else
                        RELEASE_ASSERT_NOT_REACHED();
                    setConstant(node, jsBoolean(result));
                    break;
                }
            }

            if (node->op() == CompareEq && leftConst.isSymbol() && rightConst.isSymbol()) {
                setConstant(node, jsBoolean(asSymbol(leftConst) == asSymbol(rightConst)));
                break;
            }
        }
        
        if (node->op() == CompareEq) {
            SpeculatedType leftType = forNode(node->child1()).m_type;
            SpeculatedType rightType = forNode(node->child2()).m_type;
            if (!valuesCouldBeEqual(leftType, rightType)) {
                setConstant(node, jsBoolean(false));
                break;
            }

            if (leftType == SpecOther)
                std::swap(leftType, rightType);
            if (rightType == SpecOther) {
                // Undefined and Null are always equal when compared to eachother.
                if (!(leftType & ~SpecOther)) {
                    setConstant(node, jsBoolean(true));
                    break;
                }

                // Any other type compared to Null or Undefined is always false
                // as long as the MasqueradesAsUndefined watchpoint is valid.
                //
                // MasqueradesAsUndefined only matters for SpecObjectOther, other
                // cases are always "false".
                if (!(leftType & (SpecObjectOther | SpecOther))) {
                    setConstant(node, jsBoolean(false));
                    break;
                }

                if (!(leftType & SpecOther) && m_graph.isWatchingMasqueradesAsUndefinedWatchpointSet(node)) {
                    setConstant(node, jsBoolean(false));
                    break;
                }
            }
        }
        
        if (node->child1() == node->child2()) {
            auto& value = forNode(node->child1());
            if (node->isBinaryUseKind(Int32Use)
                || node->isBinaryUseKind(Int52RepUse)
                || node->isBinaryUseKind(BigInt32Use)
                || node->isBinaryUseKind(HeapBigIntUse)
                || node->isBinaryUseKind(AnyBigIntUse)
                || node->isBinaryUseKind(StringUse)
                || node->isBinaryUseKind(BooleanUse)
                || node->isBinaryUseKind(SymbolUse)
                || node->isBinaryUseKind(StringUse)
                || node->isBinaryUseKind(StringIdentUse)
                || node->isBinaryUseKind(ObjectUse)
                || node->isSymmetricBinaryUseKind(ObjectUse, ObjectOrOtherUse)
                || value.isType(SpecInt32Only)
                || value.isType(SpecInt52Any)
                || value.isType(SpecAnyIntAsDouble)
                || value.isType(SpecBigInt)
                || value.isType(SpecString)
                || value.isType(SpecBoolean)
                || value.isType(SpecSymbol)
                || value.isType(SpecObject)
                || value.isType(SpecOther)) {
                switch (node->op()) {
                case CompareLess:
                case CompareGreater:
                    setConstant(node, jsBoolean(false));
                    break;
                case CompareLessEq:
                case CompareGreaterEq:
                case CompareEq:
                    setConstant(node, jsBoolean(true));
                    break;
                default:
                    DFG_CRASH(m_graph, node, "Unexpected node type");
                    break;
                }
                break;
            }
        }

        if (isClobbering)
            clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }
            
    case CompareStrictEq:
    case SameValue: {
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
                if (node->op() == CompareStrictEq)
                    setConstant(node, jsBoolean(JSValue::strictEqual(nullptr, left, right)));
                else
                    setConstant(node, jsBoolean(sameValue(nullptr, left, right)));
                break;
            }
        }

        // FIXME: why is this check here, and not later (after the type-based replacement by false).
        // Saam seems to agree that the two checks could be switched, I'll try that in a separate patch
        if (node->isBinaryUseKind(UntypedUse)) {
            auto isNonStringAndNonBigIntCellConstant = [] (JSValue value) {
                return value && value.isCell() && !value.isString() && !value.isHeapBigInt();
            };

            if (isNonStringAndNonBigIntCellConstant(left) || isNonStringAndNonBigIntCellConstant(right)) {
                m_state.setShouldTryConstantFolding(true);
                setNonCellTypeForNode(node, SpecBoolean);
                break;
            }
        }
        
        SpeculatedType leftLUB = leastUpperBoundOfStrictlyEquivalentSpeculations(forNode(leftNode).m_type);
        SpeculatedType rightLUB = leastUpperBoundOfStrictlyEquivalentSpeculations(forNode(rightNode).m_type);
        if (!(leftLUB & rightLUB)) {
            setConstant(node, jsBoolean(false));
            break;
        }
        
        if (node->child1() == node->child2()) {
            // FIXME: Is there any case not involving NaN where x === x is not guaranteed to return true?
            // If not I might slightly simplify that check.
            auto& value = forNode(node->child1());
            if (node->isBinaryUseKind(BooleanUse)
                || node->isSymmetricBinaryUseKind(BooleanUse, UntypedUse)
                || node->isBinaryUseKind(Int32Use)
                || node->isBinaryUseKind(Int52RepUse)
                || node->isBinaryUseKind(StringUse)
                || node->isBinaryUseKind(StringIdentUse)
                || node->isBinaryUseKind(SymbolUse)
                || node->isBinaryUseKind(ObjectUse)
                || node->isBinaryUseKind(OtherUse)
                || node->isSymmetricBinaryUseKind(OtherUse, UntypedUse)
                || node->isBinaryUseKind(MiscUse)
                || node->isSymmetricBinaryUseKind(MiscUse, UntypedUse)
                || node->isSymmetricBinaryUseKind(StringIdentUse, NotStringVarUse)
                || node->isSymmetricBinaryUseKind(StringUse, UntypedUse)
                || node->isBinaryUseKind(BigInt32Use)
                || node->isBinaryUseKind(HeapBigIntUse)
                || node->isBinaryUseKind(AnyBigIntUse)
                || value.isType(SpecInt32Only)
                || value.isType(SpecInt52Any)
                || value.isType(SpecAnyIntAsDouble)
                || value.isType(SpecBigInt)
                || value.isType(SpecString)
                || value.isType(SpecBoolean)
                || value.isType(SpecSymbol)
                || value.isType(SpecObject)
                || value.isType(SpecOther)) {
                setConstant(node, jsBoolean(true));
                break;
            }
        }

        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }
        
    case CompareEqPtr: {
        Node* childNode = node->child1().node();
        JSValue childValue = forNode(childNode).value();
        if (childValue) {
            setConstant(node, jsBoolean(childValue.isCell() && childValue.asCell() == node->cellOperand()->cell()));
            break;
        }
        
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }
        
    case StringCharCodeAt:
    case StringCodePointAt:
        setNonCellTypeForNode(node, SpecInt32Only);
        break;

    case StringIndexOf:
        setNonCellTypeForNode(node, SpecInt32Only);
        break;

    case StringFromCharCode:
        switch (node->child1().useKind()) {
        case Int32Use:
        case KnownInt32Use:
            break;
        case UntypedUse:
            clobberWorld();
            break;
        default:
            DFG_CRASH(m_graph, node, "Bad use kind");
            break;
        }
        setTypeForNode(node, SpecString);
        break;

    case StringCharAt:
        setForNode(node, m_vm.stringStructure.get());
        break;

    case StringLocaleCompare:
        setNonCellTypeForNode(node, SpecInt32Only);
        break;

    case EnumeratorGetByVal: {
        // FIXME: This should be able to do code motion for OwnStructureMode but we'd need to teach AI about the enumerator's cached structure for that to be profitable.
        clobberWorld();
        makeHeapTopForNode(node);
        break;
    }

    case GetByVal:
    case GetByValMegamorphic:
    case AtomicsAdd:
    case AtomicsAnd:
    case AtomicsCompareExchange:
    case AtomicsExchange:
    case AtomicsLoad:
    case AtomicsOr:
    case AtomicsStore:
    case AtomicsSub:
    case AtomicsXor: {
        ArrayMode arrayMode = node->arrayMode();
        if (node->op() == GetByVal || node->op() == GetByValMegamorphic) {
            auto foldGetByValOnConstantProperty = [&] (Edge& arrayEdge, Edge& indexEdge) {
                // FIXME: We can expand this for non x86 environments.
                // https://bugs.webkit.org/show_bug.cgi?id=134641
                if (!isX86())
                    return false;

                AbstractValue& arrayValue = forNode(arrayEdge);

                // Check the structure set is finite. This means that this constant's structure is watched and guaranteed the one of this set.
                // When the structure is changed, this code should be invalidated. This is important since the following code relies on the
                // constant object's is not changed.
                if (!arrayValue.m_structure.isFinite())
                    return false;

                JSValue arrayConstant = arrayValue.value();
                if (!arrayConstant)
                    return false;

                JSObject* array = jsDynamicCast<JSObject*>(arrayConstant);
                if (!array)
                    return false;

                JSValue indexConstant = forNode(indexEdge).value();
                if (!indexConstant || !indexConstant.isInt32() || indexConstant.asInt32() < 0)
                    return false;
                uint32_t index = indexConstant.asUInt32();

                // Check that the early StructureID is not nuked, get the butterfly, and check the late StructureID again.
                // And we check the indexing mode of the structure. If the indexing mode is CoW, the butterfly is
                // definitely JSImmutableButterfly.
                StructureID structureIDEarly = array->structureID();
                if (structureIDEarly.isNuked())
                    return false;

                if (arrayMode.arrayClass() == Array::OriginalCopyOnWriteArray) {

                    WTF::loadLoadFence();
                    Butterfly* butterfly = array->butterfly();

                    WTF::loadLoadFence();
                    StructureID structureIDLate = array->structureID();

                    if (structureIDEarly != structureIDLate)
                        return false;

                    Structure* structure = structureIDLate.decode();
                    switch (arrayMode.type()) {
                    case Array::Int32:
                    case Array::Contiguous:
                    case Array::Double:
                        if (structure->indexingMode() != (toIndexingShape(arrayMode.type()) | CopyOnWrite | IsArray))
                            return false;
                        break;
                    default:
                        return false;
                    }
                    ASSERT(isCopyOnWrite(structure->indexingMode()));

                    JSImmutableButterfly* immutableButterfly = JSImmutableButterfly::fromButterfly(butterfly);
                    if (index < immutableButterfly->length()) {
                        JSValue value = immutableButterfly->get(index);
                        ASSERT(value);
                        if (value.isCell())
                            setConstant(node, *m_graph.freeze(value.asCell()));
                        else
                            setConstant(node, value);
                        return true;
                    }

                    if (arrayMode.isOutOfBounds()) {
                        if (m_graph.isWatchingArrayPrototypeChainIsSaneWatchpoint(node)) {
                            if (arrayMode.type() == Array::Double && arrayMode.isOutOfBoundsSaneChain() && !(node->flags() & NodeBytecodeUsesAsOther))
                                setConstant(node, jsNumber(PNaN));
                            else
                                setConstant(node, jsUndefined());
                            return true;
                        }
                    }
                    return false;
                }

                if (arrayMode.type() == Array::ArrayStorage || arrayMode.type() == Array::SlowPutArrayStorage) {
                    JSValue value;
                    {
                        // ArrayStorage's Butterfly can be half-broken state.
                        Locker locker { array->cellLock() };

                        WTF::loadLoadFence();
                        Butterfly* butterfly = array->butterfly();

                        WTF::loadLoadFence();
                        StructureID structureIDLate = array->structureID();

                        if (structureIDEarly != structureIDLate)
                            return false;

                        Structure* structure = structureIDLate.decode();
                        if (!hasAnyArrayStorage(structure->indexingMode()))
                            return false;

                        if (structure->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
                            return false;

                        ArrayStorage* storage = butterfly->arrayStorage();
                        if (index >= storage->length())
                            return false;

                        if (index < storage->vectorLength())
                            return false;

                        SparseArrayValueMap* map = storage->m_sparseMap.get();
                        if (!map)
                            return false;

                        value = map->getConcurrently(index);
                    }
                    if (!value)
                        return false;

                    if (value.isCell())
                        setConstant(node, *m_graph.freeze(value.asCell()));
                    else
                        setConstant(node, value);
                    return true;
                }

                return false;
            };

            bool didFold = false;
            switch (arrayMode.type()) {
            case Array::Generic:
            case Array::Int32:
            case Array::Double:
            case Array::Contiguous:
            case Array::ArrayStorage:
            case Array::SlowPutArrayStorage:
                if (foldGetByValOnConstantProperty(m_graph.child(node, 0), m_graph.child(node, 1))) {
                    if (arrayMode.isEffectfulOutOfBounds())
                        didFoldClobberWorld();
                    didFold = true;
                }
                break;
            default:
                break;
            }

            if (didFold)
                break;

            if (m_graph.child(node, 0).useKind() == ObjectUse && arrayMode.type() == Array::Generic) {
                AbstractValue& property = forNode(m_graph.child(node, 1));
                if (JSValue constant = property.value()) {
                    if (constant.isString()) {
                        JSString* string = asString(constant);
                        if (CacheableIdentifier::isCacheableIdentifierCell(string) && !parseIndex(CacheableIdentifier::createFromCell(string).uid())) {
                            m_state.setShouldTryConstantFolding(true);
                            clobberWorld(); // Regardless of folding (to PutById etc.), we still clobber world.
                            makeHeapTopForNode(node);
                            break;
                        }
                    }
                }
            }
        } else {
            unsigned numExtraArgs = numExtraAtomicsArgs(node->op());
            Edge storageEdge = m_graph.child(node, 2 + numExtraArgs);
            if (!storageEdge)
                clobberWorld();
        }

        if (node->op() == AtomicsStore) {
            // The returned value from Atomics.store does not rely on typed array types. It is relying
            // on input's UseKind. For example,
            //
            //     Atomics.store(uint8Array, /* index */ 0, Infinity) // => returned value is Infinity
            //
            // Since the other ReadModifyWrite atomics return values stored in the typed array previously,
            // the returned values rely on the typed array types. On the other hand, Atomics.store's
            // returned value is input value. This means that Atomics.store + Uint8Array can return doubles
            // while the typed array is Uint8Array (the above one is the example).
            switch (arrayMode.type()) {
            case Array::Generic:
                clobberWorld();
                makeHeapTopForNode(node);
                break;
            default: {
                Edge operand = m_graph.child(node, 2);
                switch (operand.useKind()) {
                case Int32Use:
                    setNonCellTypeForNode(node, SpecInt32Only);
                    break;
                case Int52RepUse:
                    setNonCellTypeForNode(node, SpecInt52Any);
                    break;
                case DoubleRepUse:
                    setNonCellTypeForNode(node, SpecFullDouble);
                    break;
                default:
                    DFG_CRASH(m_graph, node, "Bad use kind");
                    break;
                }
                break;
            }
            }
            break;
        }

        switch (arrayMode.type()) {
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
        case Array::SelectUsingArguments:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case Array::ForceExit:
            m_state.setIsValid(false);
            break;
        case Array::Undecided: {
            JSValue index = forNode(m_graph.child(node, 1)).value();
            if (index && index.isInt32() && index.asInt32() >= 0) {
                setConstant(node, jsUndefined());
                break;
            }
            setNonCellTypeForNode(node, SpecOther);
            break;
        }
        case Array::Generic:
            clobberWorld();
            makeHeapTopForNode(node);
            break;
        case Array::String:
            if (arrayMode.isOutOfBounds()) {
                // If the watchpoint was still valid we could totally set this to be
                // SpecString | SpecOther. Except that we'd have to be careful. If we
                // tested the watchpoint state here then it could change by the time
                // we got to the backend. So to do this right, we'd have to get the
                // fixup phase to check the watchpoint state and then bake into the
                // GetByVal operation the fact that we're using a watchpoint, using
                // something like Array::InBoundsSaneChain (except not quite, because that
                // implies an in-bounds access). None of this feels like it's worth it,
                // so we're going with TOP for now. The same thing applies to
                // clobbering the world.
                clobberWorld();
                makeHeapTopForNode(node);
            } else
                setForNode(node, m_vm.stringStructure.get());
            break;
        case Array::DirectArguments:
        case Array::ScopedArguments:
            if (arrayMode.isOutOfBounds())
                clobberWorld();
            makeHeapTopForNode(node);
            break;
        case Array::Int32:
            if (arrayMode.isEffectfulOutOfBounds()) {
                clobberWorld();
                makeHeapTopForNode(node);
            } else if (arrayMode.isOutOfBoundsSaneChain())
                setNonCellTypeForNode(node, SpecInt32Only | SpecOther);
            else
                setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Array::Double:
            if (arrayMode.isEffectfulOutOfBounds()) {
                clobberWorld();
                makeHeapTopForNode(node);
            } else if (arrayMode.isInBoundsSaneChain())
                setNonCellTypeForNode(node, SpecBytecodeDouble);
            else if (arrayMode.isOutOfBoundsSaneChain()) {
                if (!!(node->flags() & NodeBytecodeUsesAsOther))
                    setNonCellTypeForNode(node, SpecBytecodeDouble | SpecOther);
                else
                    setNonCellTypeForNode(node, SpecBytecodeDouble);
            } else
                setNonCellTypeForNode(node, SpecDoubleReal);
            break;
        case Array::Contiguous:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            if (arrayMode.isEffectfulOutOfBounds())
                clobberWorld();
            makeHeapTopForNode(node);
            break;
        case Array::Int8Array:
            if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                setNonCellTypeForNode(node, SpecInt32Only | SpecOther);
            else
                setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Array::Int16Array:
            if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                setNonCellTypeForNode(node, SpecInt32Only | SpecOther);
            else
                setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Array::Int32Array:
            if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                setNonCellTypeForNode(node, SpecInt32Only | SpecOther);
            else
                setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Array::Uint8Array:
            if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                setNonCellTypeForNode(node, SpecInt32Only | SpecOther);
            else
                setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Array::Uint8ClampedArray:
            if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                setNonCellTypeForNode(node, SpecInt32Only | SpecOther);
            else
                setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Array::Uint16Array:
            if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                setNonCellTypeForNode(node, SpecInt32Only | SpecOther);
            else
                setNonCellTypeForNode(node, SpecInt32Only);
            break;
        case Array::Uint32Array: {
            if (node->shouldSpeculateInt32()) {
                if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                    setNonCellTypeForNode(node, SpecInt32Only | SpecOther);
                else
                    setNonCellTypeForNode(node, SpecInt32Only);
            } else if (!(node->op() == GetByVal && arrayMode.isOutOfBounds()) && node->shouldSpeculateInt52())
                setNonCellTypeForNode(node, SpecInt52Any);
            else {
                if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                    setNonCellTypeForNode(node, SpecAnyIntAsDouble | SpecOther);
                else
                    setNonCellTypeForNode(node, SpecAnyIntAsDouble);
            }
            break;
        }
        case Array::Float16Array:
        case Array::Float32Array:
        case Array::Float64Array:
            if (node->op() == GetByVal && arrayMode.isOutOfBounds())
                setNonCellTypeForNode(node, SpecBytecodeDouble | SpecOther);
            else
                setNonCellTypeForNode(node, SpecFullDouble);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
            
    case PutByValDirect:
    case PutByVal:
    case PutByValAlias:
    case PutByValMegamorphic: {
        switch (node->arrayMode().modeForPut().type()) {
        case Array::ForceExit:
            m_state.setIsValid(false);
            break;
        case Array::Generic: {
            if (node->op() == PutByVal || node->op() == PutByValMegamorphic) {
                if (m_graph.child(node, 0).useKind() == CellUse && m_graph.child(node, 1).useKind() == StringUse) {
                    AbstractValue& property = forNode(m_graph.child(node, 1));
                    if (JSValue constant = property.value()) {
                        if (constant.isString()) {
                            JSString* string = asString(constant);
                            if (CacheableIdentifier::isCacheableIdentifierCell(string) && !parseIndex(CacheableIdentifier::createFromCell(string).uid())) {
                                m_state.setShouldTryConstantFolding(true);
                                clobberWorld(); // Regardless of folding (to PutById etc.), we still clobber world.
                                break;
                            }
                        }
                    }
                }
            }
            clobberWorld();
            break;
        }
        case Array::Int32:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld();
            break;
        case Array::Double:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld();
            break;
        case Array::Contiguous:
        case Array::ArrayStorage:
            if (node->arrayMode().isOutOfBounds())
                clobberWorld();
            break;
        case Array::SlowPutArrayStorage:
            if (node->arrayMode().mayStoreToHole())
                clobberWorld();
            break;
        default:
            break;
        }
        break;
    }

    case EnumeratorPutByVal: {
        clobberWorld();
        break;
    }

            
    case ArrayPush:
        switch (node->arrayMode().type()) {
        case Array::ForceExit:
            m_state.setIsValid(false);
            break;
        default:
            break;
        }
        clobberWorld();
        setNonCellTypeForNode(node, SpecBytecodeNumber);
        break;

    case ArraySlice: {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);

        // FIXME: We could do better here if we prove that the
        // incoming value has only a single structure.
        RegisteredStructureSet structureSet;
        structureSet.add(m_graph.registerStructure(globalObject->originalArrayStructureForIndexingType(ArrayWithInt32)));
        structureSet.add(m_graph.registerStructure(globalObject->originalArrayStructureForIndexingType(ArrayWithContiguous)));
        structureSet.add(m_graph.registerStructure(globalObject->originalArrayStructureForIndexingType(ArrayWithDouble)));

        setForNode(node, structureSet);
        break;
    }

    case ArraySplice:
        clobberWorld();
        makeBytecodeTopForNode(node);
        break;

    case ArrayIndexOf: {
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }
            
    case ArrayPop:
        clobberWorld();
        makeHeapTopForNode(node);
        break;
        
    case GetMyArgumentByVal:
    case GetMyArgumentByValOutOfBounds: {
        JSValue index = forNode(node->child2()).m_value;
        InlineCallFrame* inlineCallFrame = node->child1()->origin.semantic.inlineCallFrame();

        if (index && index.isUInt32()) {
            // This pretends to return TOP for accesses that are actually proven out-of-bounds because
            // that's the conservative thing to do. Otherwise we'd need to write more code to mark such
            // paths as unreachable, or to return undefined. We could implement that eventually.

            CheckedUint32 argumentIndexChecked = index.asUInt32();
            argumentIndexChecked += node->numberOfArgumentsToSkip();
            if (!argumentIndexChecked.hasOverflowed()) {
                unsigned argumentIndex = argumentIndexChecked;
                if (inlineCallFrame) {
                    if (argumentIndex < static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1)) {
                        setForNode(node, m_state.operand(
                            virtualRegisterForArgumentIncludingThis(argumentIndex + 1) + inlineCallFrame->stackOffset));
                        m_state.setShouldTryConstantFolding(true);
                        break;
                    }
                } else {
                    if (argumentIndex < m_state.numberOfArguments() - 1) {
                        setForNode(node, m_state.argument(argumentIndex + 1));
                        m_state.setShouldTryConstantFolding(true);
                        break;
                    }
                }
            }
        }
        
        if (inlineCallFrame) {
            // We have a bound on the types even though it's random access. Take advantage of this.
            
            AbstractValue result;
            for (unsigned i = 1 + node->numberOfArgumentsToSkip(); i < inlineCallFrame->argumentCountIncludingThis; ++i) {
                result.merge(
                    m_state.operand(
                        virtualRegisterForArgumentIncludingThis(i) + inlineCallFrame->stackOffset));
            }
            
            if (node->op() == GetMyArgumentByValOutOfBounds)
                result.merge(SpecOther);
            
            if (result.value())
                m_state.setShouldTryConstantFolding(true);
            
            setForNode(node, result);
            break;
        }
        
        makeHeapTopForNode(node);
        break;
    }
            
    case RegExpExec:
    case RegExpExecNonGlobalOrSticky:
        if (node->op() == RegExpExec) {
            // Even if we've proven known input types as RegExpObject and String,
            // accessing lastIndex is effectful if it's a global regexp.
            clobberWorld();
        }

        if (JSValue globalObjectValue = forNode(node->child1()).m_value) {
            if (JSGlobalObject* globalObject = jsDynamicCast<JSGlobalObject*>(globalObjectValue)) {
                if (m_graph.m_plan.isUnlinked() && globalObject != m_graph.globalObjectFor(node->origin.semantic))
                    break;

                if (!globalObject->isHavingABadTime()) {
                    m_graph.watchpoints().addLazily(globalObject->havingABadTimeWatchpointSet());
                    RegisteredStructureSet structureSet;
                    structureSet.add(m_graph.registerStructure(globalObject->regExpMatchesArrayStructure()));
                    structureSet.add(m_graph.registerStructure(globalObject->regExpMatchesArrayWithIndicesStructure()));
                    setForNode(node, structureSet);
                    forNode(node).merge(SpecOther);
                    break;
                }
            }
        }
        setTypeForNode(node, SpecOther | SpecArray);
        break;

    case RegExpTest:
    case RegExpTestInline:
        // Even if we've proven known input types as RegExpObject and String,
        // accessing lastIndex is effectful if it's a global regexp.
        clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;

    case RegExpMatchFast:
        ASSERT(node->child2().useKind() == RegExpObjectUse);
        ASSERT(node->child3().useKind() == StringUse || node->child3().useKind() == KnownStringUse);
        setTypeForNode(node, SpecOther | SpecArray);
        break;

    case RegExpMatchFastGlobal:
        ASSERT(node->child2().useKind() == StringUse || node->child2().useKind() == KnownStringUse);
        setTypeForNode(node, SpecOther | SpecArray);
        break;
            
    case StringReplace:
    case StringReplaceRegExp:
        if (node->child1().useKind() == StringUse
            && node->child2().useKind() == RegExpObjectUse
            && node->child3().useKind() == StringUse) {
            // This doesn't clobber the world. It just reads and writes regexp state.
        } else
            clobberWorld();
        setForNode(node, m_vm.stringStructure.get());
        break;

    case StringReplaceString:
        if (node->child3().useKind() != StringUse) {
            // child3 could be non String (e.g. function). In this case, any side effect can happen.
            clobberWorld();
        }
        setForNode(node, m_vm.stringStructure.get());
        break;

    case Jump:
        break;
            
    case Branch: {
        Node* child = node->child1().node();
        switch (booleanResult(node, forNode(child))) {
        case TriState::True:
            m_state.setBranchDirection(TakeTrue);
            break;
        case TriState::False:
            m_state.setBranchDirection(TakeFalse);
            break;
        case TriState::Indeterminate:
            // FIXME: The above handles the trivial cases of sparse conditional
            // constant propagation, but we can do better:
            // We can specialize the source variable's value on each direction of
            // the branch.
            m_state.setBranchDirection(TakeBoth);
            break;
        }
        break;
    }
        
    case Switch: {
        // Nothing to do for now.
        // FIXME: Do sparse conditional things.
        break;
    }

    case EntrySwitch:
        break;

    case Return:
        m_state.setIsValid(false);
        break;

    case Throw:
    case ThrowStaticError:
    case TailCall:
    case DirectTailCall:
    case TailCallVarargs:
    case TailCallForwardVarargs:
        clobberWorld();
        m_state.setIsValid(false);
        break;
        
    case ToPrimitive: {
        JSValue childConst = forNode(node->child1()).value();
        if (childConst && childConst.isNumber()) {
            didFoldClobberWorld();
            setConstant(node, childConst);
            break;
        }
        
        ASSERT(node->child1().useKind() == UntypedUse);
        
        if (!(forNode(node->child1()).m_type & ~(SpecFullNumber | SpecBoolean | SpecString | SpecSymbol | SpecBigInt))) {
            m_state.setShouldTryConstantFolding(true);
            didFoldClobberWorld();
            setForNode(node, forNode(node->child1()));
            break;
        }
        
        clobberWorld();
        
        setTypeForNode(node, SpecHeapTop & ~SpecObject);
        break;
    }

    case ToPropertyKeyOrNumber: {
        JSValue childConst = forNode(node->child1()).value();
        if (childConst && childConst.isNumber()) {
            didFoldClobberWorld();
            setConstant(node, childConst);
            break;
        }
        FALLTHROUGH;
    }
    case ToPropertyKey: {
        if (!(forNode(node->child1()).m_type & ~(SpecString | SpecSymbol))) {
            m_state.setShouldTryConstantFolding(true);
            didFoldClobberWorld();
            setForNode(node, forNode(node->child1()));
            break;
        }

        clobberWorld();

        SpeculatedType type = SpecString | SpecSymbol;
        if (node->op() == ToPropertyKeyOrNumber)
            type |= SpecBytecodeNumber;
        setTypeForNode(node, type);
        break;
    }

    case ToNumber: {
        if (node->child1().useKind() == StringUse) {
            setNonCellTypeForNode(node, SpecBytecodeNumber);
            break;
        }

        JSValue childConst = forNode(node->child1()).value();

        if (childConst && childConst.isNumber()) {
            didFoldClobberWorld();
            setConstant(node, childConst);
            break;
        }

        ASSERT(node->child1().useKind() == UntypedUse);

        if (!(forNode(node->child1()).m_type & ~SpecBytecodeNumber)) {
            m_state.setShouldTryConstantFolding(true);
            didFoldClobberWorld();
            setForNode(node, forNode(node->child1()));
            break;
        }

        clobberWorld();
        setNonCellTypeForNode(node, SpecBytecodeNumber);
        break;
    }

    case ToNumeric: {
        JSValue childConst = forNode(node->child1()).value();
        if (childConst && (childConst.isNumber() || childConst.isBigInt())) {
            didFoldClobberWorld();
            if (childConst.isCell())
                setConstant(node, *m_graph.freeze(childConst.asCell()));
            else
                setConstant(node, childConst);
            break;
        }

        ASSERT(node->child1().useKind() == UntypedUse);

        if (!(forNode(node->child1()).m_type & ~(SpecBytecodeNumber | SpecBigInt))) {
            m_state.setShouldTryConstantFolding(true);
            didFoldClobberWorld();
            setForNode(node, forNode(node->child1()));
            break;
        }

        clobberWorld();
        setTypeForNode(node, SpecBytecodeNumber | SpecBigInt);
        break;
    }

    case CallNumberConstructor: {
        JSValue childConst = forNode(node->child1()).value();
        if (childConst) {
            if (childConst.isNumber()) {
                if (node->child1().useKind() == UntypedUse)
                    didFoldClobberWorld();
                setConstant(node, childConst);
                break;
            }
#if USE(BIGINT32)
            if (childConst.isBigInt32()) {
                if (node->child1().useKind() == UntypedUse)
                    didFoldClobberWorld();
                setConstant(node, jsNumber(childConst.bigInt32AsInt32()));
                break;
            }
#endif
        }

        ASSERT(node->child1().useKind() == UntypedUse || node->child1().useKind() == BigInt32Use);

        if (!(forNode(node->child1()).m_type & ~SpecBytecodeNumber)) {
            m_state.setShouldTryConstantFolding(true);
            if (node->child1().useKind() == UntypedUse)
                didFoldClobberWorld();
            setForNode(node, forNode(node->child1()));
            break;
        }

        if (node->child1().useKind() == BigInt32Use) {
            setTypeForNode(node, SpecInt32Only);
            break;
        }

        clobberWorld();
        setNonCellTypeForNode(node, SpecBytecodeNumber);
        break;
    }
        
    case ToString:
    case CallStringConstructor: {
        switch (node->child1().useKind()) {
        case StringObjectUse:
        case StringOrOtherUse:
        case StringOrStringObjectUse:
        case Int32Use:
        case Int52RepUse:
        case DoubleRepUse:
        case NotCellUse:
        case KnownPrimitiveUse:
            break;
        case CellUse:
        case UntypedUse:
            clobberWorld();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        setForNode(node, m_vm.stringStructure.get());
        break;
    }

    case FunctionToString: {
        JSValue value = m_state.forNode(node->child1()).value();
        if (value) {
            JSFunction* function = jsDynamicCast<JSFunction*>(value);
            if (JSString* asString = function->asStringConcurrently()) {
                setConstant(node, *m_graph.freeze(asString));
                break;
            }
        }
        setForNode(node, m_vm.stringStructure.get());
        break;
    }

    case FunctionBind: {
        if (m_graph.m_plan.isUnlinked()) {
            clobberWorld();
            setTypeForNode(node, SpecFunction);
            break;
        }

        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
        Edge target = m_graph.child(node, 0);
        AbstractValue& targetValue = forNode(target);
        auto& structureSet = targetValue.m_structure;
        if (!(targetValue.m_type & ~SpecFunction) && structureSet.isFinite() && structureSet.size() == 1) {
            RegisteredStructure structure = structureSet.onlyStructure();
            if (JSBoundFunction::canSkipNameAndLengthMaterialization(globalObject, structure.get())) {
                m_state.setShouldTryConstantFolding(true);
                didFoldClobberWorld();
                setForNode(node, globalObject->boundFunctionStructure());
                break;
            }
        }

        clobberWorld();
        setTypeForNode(node, SpecFunction);
        break;
    }

    case NumberToStringWithRadix: {
        JSValue radixValue = forNode(node->child2()).m_value;
        if (radixValue && radixValue.isInt32()) {
            int32_t radix = radixValue.asInt32();
            if (2 <= radix && radix <= 36) {
                m_state.setShouldTryConstantFolding(true);
                didFoldClobberWorld();
                setForNode(node, m_graph.m_vm.stringStructure.get());
                break;
            }
        }
        clobberWorld();
        setForNode(node, m_graph.m_vm.stringStructure.get());
        break;
    }

    case NumberToStringWithValidRadixConstant: {
        setForNode(node, m_graph.m_vm.stringStructure.get());
        break;
    }
        
    case NewStringObject: {
        ASSERT(node->structure()->classInfoForCells() == StringObject::info());
        setForNode(node, node->structure());
        break;
    }

    case NewSymbol: {
        if (node->child1() && node->child1().useKind() != StringUse)
            clobberWorld();
        setForNode(node, m_vm.symbolStructure.get());
        break;
    }
            
    case NewArray:
        ASSERT(node->indexingMode() == node->indexingType()); // Copy on write arrays should only be created by NewArrayBuffer.
        setForNode(node,
            m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()));
        break;

    case NewArrayWithSpread:
        if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
            // We've compiled assuming we're not having a bad time, so to be consistent
            // with StructureRegisterationPhase we must say we produce an original array
            // allocation structure.
#if USE(JSVALUE64)
            BitVector* bitVector = node->bitVector();
            if (node->numChildren() == 1 && bitVector->get(0)) {
                Edge use = m_graph.varArgChild(node, 0);
                if (use->op() == PhantomSpread) {
                    if (use->child1()->op() == PhantomNewArrayBuffer) {
                        auto* immutableButterfly = use->child1()->castOperand<JSImmutableButterfly*>();
                        if (hasContiguous(immutableButterfly->indexingType())) {
                            m_state.setShouldTryConstantFolding(true);
                            setForNode(node, m_graph.globalObjectFor(node->origin.semantic)->originalArrayStructureForIndexingType(CopyOnWriteArrayWithContiguous));
                            break;
                        }
                    }
                } else {
                    setForNode(node, m_graph.globalObjectFor(node->origin.semantic)->originalArrayStructureForIndexingType(CopyOnWriteArrayWithContiguous));
                    break;
                }
            }
#endif
            setForNode(node, m_graph.globalObjectFor(node->origin.semantic)->originalArrayStructureForIndexingType(ArrayWithContiguous));
        } else {
            setForNode(node, 
                m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous));
        }

        break;

    case Spread:
        switch (node->child1()->op()) {
        case PhantomNewArrayBuffer:
        case PhantomCreateRest:
            break;
        default:
            if (!m_graph.canDoFastSpread(node, forNode(node->child1())))
                clobberWorld();
            else
                didFoldClobberWorld();
            break;
        }

        setForNode(node, m_vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get());
        break;
        
    case NewArrayBuffer:
        setForNode(node,
            m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(node->indexingMode()));
        break;

    case NewArrayWithSpecies:
        clobberWorld();
        setTypeForNode(node, SpecObject);
        break;

    case NewArrayWithSize: {
        bool folding = false;
        if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
            if (node->child1().useKind() == Int32Use && node->child1()->isInt32Constant()) {
                int32_t length = node->child1()->asInt32();
                if (length >= 0 && length < MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH) {
                    switch (node->indexingType()) {
                    case ALL_DOUBLE_INDEXING_TYPES:
                    case ALL_INT32_INDEXING_TYPES:
                    case ALL_CONTIGUOUS_INDEXING_TYPES: {
                        m_state.setShouldTryConstantFolding(true);
                        setForNode(node, m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(node->indexingMode()));
                        folding = true;
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
        if (!folding)
            setTypeForNode(node, SpecArray);
        break;
    }

    case NewArrayWithSizeAndStructure: {
        setTypeForNode(node, SpecArray);
        break;
    }

    case NewArrayWithConstantSize:
        setForNode(node, m_graph.globalObjectFor(node->origin.semantic)->arrayStructureForIndexingTypeDuringAllocation(node->indexingMode()));
        break;

    case NewTypedArray: {
        switch (node->child1().useKind()) {
        case Int32Use:
        case Int52RepUse: {
            bool isResizableOrGrowableShared = false;
            setForNode(node, m_graph.globalObjectFor(node->origin.semantic)->typedArrayStructureConcurrently(node->typedArrayType(), isResizableOrGrowableShared));
            break;
        }
        case UntypedUse: {
            clobberWorld();
            RegisteredStructureSet structureSet;
            structureSet.add(m_graph.registerStructure(m_graph.globalObjectFor(node->origin.semantic)->typedArrayStructureConcurrently(node->typedArrayType(), true)));
            structureSet.add(m_graph.registerStructure(m_graph.globalObjectFor(node->origin.semantic)->typedArrayStructureConcurrently(node->typedArrayType(), false)));
            setForNode(node, structureSet);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }

    case NewRegexp:
        setForNode(node, m_graph.globalObjectFor(node->origin.semantic)->regExpStructure());
        break;

    case NewMap:
        setForNode(node, node->structure());
        break;

    case NewSet:
        setForNode(node, node->structure());
        break;

    case ToThis: {
        AbstractValue& source = forNode(node->child1());
        AbstractValue& destination = forNode(node);
        ECMAMode ecmaMode = node->ecmaMode();

        ToThisResult result = isToThisAnIdentity(ecmaMode, source);
        switch (result) {
        case ToThisResult::Identity:
            m_state.setShouldTryConstantFolding(true);
            destination = source;
            break;
        case ToThisResult::Undefined:
            setConstant(node, jsUndefined());
            break;
        case ToThisResult::GlobalThis:
            m_state.setShouldTryConstantFolding(true);
            destination.setType(m_graph, SpecObject);
            break;
        case ToThisResult::Dynamic:
            if (ecmaMode.isStrict())
                destination.makeHeapTop();
            else {
                destination = source;
                destination.merge(SpecObject);
            }
            break;
        }
        break;
    }

    case CreateThis: {
        if (JSValue base = forNode(node->child1()).m_value) {
            if (auto* function = jsDynamicCast<JSFunction*>(base)) {
                if (FunctionRareData* rareData = function->rareData()) {
                    if (rareData->allocationProfileWatchpointSet().isStillValid() && m_graph.isWatchingStructureCacheClearedWatchpoint(node)) {
                        if (Structure* structure = rareData->objectAllocationStructure()) {
                            m_graph.freeze(rareData);
                            m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                            m_state.setShouldTryConstantFolding(true);
                            didFoldClobberWorld();
                            setForNode(node, structure);
                            break;
                        }
                    }
                }
            }
        }
        clobberWorld();
        setTypeForNode(node, SpecFinalObject);
        break;
    }

    case CreatePromise: {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
        if (JSValue base = forNode(node->child1()).m_value) {
            if (base == (node->isInternalPromise() ? globalObject->internalPromiseConstructor() : globalObject->promiseConstructor())) {
                m_state.setShouldTryConstantFolding(true);
                didFoldClobberWorld();
                setForNode(node, node->isInternalPromise() ? globalObject->internalPromiseStructure() : globalObject->promiseStructure());
                break;
            }
            if (auto* function = jsDynamicCast<JSFunction*>(base)) {
                if (FunctionRareData* rareData = function->rareData()) {
                    if (rareData->allocationProfileWatchpointSet().isStillValid() && m_graph.isWatchingStructureCacheClearedWatchpoint(node)) {
                        Structure* structure = rareData->internalFunctionAllocationStructure();
                        if (structure
                            && structure->classInfoForCells() == (node->isInternalPromise() ? JSInternalPromise::info() : JSPromise::info())
                            && structure->globalObject() == globalObject) {
                            m_graph.freeze(rareData);
                            m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                            m_state.setShouldTryConstantFolding(true);
                            didFoldClobberWorld();
                            setForNode(node, structure);
                            break;
                        }
                    }
                }
            }
        }
        clobberWorld();
        setTypeForNode(node, SpecPromiseObject);
        break;
    }

    case CreateGenerator:
    case CreateAsyncGenerator: {
        auto tryToFold = [&] (const ClassInfo* classInfo) -> bool {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
            if (JSValue base = forNode(node->child1()).m_value) {
                if (auto* function = jsDynamicCast<JSFunction*>(base)) {
                    if (FunctionRareData* rareData = function->rareData()) {
                        if (rareData->allocationProfileWatchpointSet().isStillValid() && m_graph.isWatchingStructureCacheClearedWatchpoint(node)) {
                            Structure* structure = rareData->internalFunctionAllocationStructure();
                            if (structure
                                && structure->classInfoForCells() == classInfo
                                && structure->globalObject() == globalObject) {
                                m_graph.freeze(rareData);
                                m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                                m_state.setShouldTryConstantFolding(true);
                                didFoldClobberWorld();
                                setForNode(node, structure);
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        };

        bool found = false;
        switch (node->op()) {
        case CreateGenerator:
            found = tryToFold(JSGenerator::info());
            break;
        case CreateAsyncGenerator:
            found = tryToFold(JSAsyncGenerator::info());
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        if (found)
            break;
        clobberWorld();
        setTypeForNode(node, SpecObjectOther);
        break;
    }

    case NewGenerator:
    case NewAsyncGenerator:    
    case NewInternalFieldObject:
    case NewObject:
    case MaterializeNewInternalFieldObject:
        ASSERT(!!node->structure().get());
        setForNode(node, node->structure());
        break;

    case ObjectAssign: {
        clobberWorld();
        break;
    }

    case ObjectCreate: {
        if (JSValue base = forNode(node->child1()).m_value) {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
            Structure* structure = nullptr;
            if (base.isNull())
                structure = globalObject->nullPrototypeObjectStructure();
            else if (base.isObject()) {
                // Having a bad time clears the structureCache, and so it should invalidate this structure.
                if (m_graph.isWatchingStructureCacheClearedWatchpoint(node))
                    structure = globalObject->structureCache().emptyObjectStructureConcurrently(base.getObject(), JSFinalObject::defaultInlineCapacity);
            }

            if (structure) {
                m_state.setShouldTryConstantFolding(true);
                didFoldClobberWorld();
                setForNode(node, structure);
                break;
            }
        }
        clobberWorld();
        setTypeForNode(node, SpecFinalObject);
        break;
    }

    case ObjectKeys:
    case ObjectGetOwnPropertyNames:
    case ObjectGetOwnPropertySymbols:
    case ReflectOwnKeys: {
        if (node->child1().useKind() == ObjectUse) {
            auto& structureSet = forNode(node->child1()).m_structure;
            if (structureSet.isFinite() && structureSet.size() == 1) {
                RegisteredStructure structure = structureSet.onlyStructure();
                if (auto* rareData = structure->rareDataConcurrently()) {
                    if (!!rareData->cachedPropertyNamesConcurrently(node->cachedPropertyNamesKind())) {
                        if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
                            m_state.setShouldTryConstantFolding(true);
                            didFoldClobberWorld();
                            setTypeForNode(node, SpecArray);
                            break;
                        }
                    }
                }
            }
        }

        clobberWorld();
        setTypeForNode(node, SpecArray);
        break;
    }

    case ObjectToString: {
        clobberWorld();
        setTypeForNode(node, SpecString);
        break;
    }

    case ToObject:
    case CallObjectConstructor: {
        AbstractValue& source = forNode(node->child1());
        AbstractValue& destination = forNode(node);

        if (!(source.m_type & ~SpecObject)) {
            m_state.setShouldTryConstantFolding(true);
            if (node->op() == ToObject)
                didFoldClobberWorld();
            destination = source;
            break;
        }

        if (node->op() == ToObject)
            clobberWorld();
        setTypeForNode(node, SpecObject);
        break;
    }

    case PhantomNewObject:
    case PhantomNewFunction:
    case PhantomNewGeneratorFunction:
    case PhantomNewAsyncGeneratorFunction:
    case PhantomNewAsyncFunction:
    case PhantomCreateActivation:
    case PhantomDirectArguments:
    case PhantomClonedArguments:
    case PhantomCreateRest:
    case PhantomSpread:
    case PhantomNewArrayWithSpread:
    case PhantomNewArrayBuffer:
    case PhantomNewInternalFieldObject:
    case PhantomNewRegexp:
    case BottomValue: {
        clearForNode(node);
        break;
    }

    case PutHint:
        break;
        
    case MaterializeNewObject: {
        setForNode(node, node->structureSet());
        break;
    }

    case PushWithScope:
        // We don't use the more precise withScopeStructure() here because it is a LazyProperty and may not yet be allocated.
        setTypeForNode(node, SpecObjectOther);
        break;

    case CreateActivation:
    case MaterializeCreateActivation:
        setForNode(node, 
            m_codeBlock->globalObjectFor(node->origin.semantic)->activationStructure());
        break;
        
    case CreateDirectArguments:
        setForNode(node, m_codeBlock->globalObjectFor(node->origin.semantic)->directArgumentsStructure());
        break;
        
    case CreateScopedArguments:
        setForNode(node, m_codeBlock->globalObjectFor(node->origin.semantic)->scopedArgumentsStructure());
        break;
        
    case CreateClonedArguments:
        if (!m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
            setTypeForNode(node, SpecObject);
            break;
        }
        setForNode(node, m_codeBlock->globalObjectFor(node->origin.semantic)->clonedArgumentsStructure());
        break;

    case NewGeneratorFunction:
        setForNode(node, 
            m_codeBlock->globalObjectFor(node->origin.semantic)->generatorFunctionStructure());
        break;

    case NewAsyncGeneratorFunction:
        setForNode(node, 
            m_codeBlock->globalObjectFor(node->origin.semantic)->asyncGeneratorFunctionStructure());
        break;

    case NewAsyncFunction:
        setForNode(node, 
            m_codeBlock->globalObjectFor(node->origin.semantic)->asyncFunctionStructure());
        break;

    case NewBoundFunction: {
        setForNode(node, m_codeBlock->globalObjectFor(node->origin.semantic)->boundFunctionStructure());
        break;
    }

    case NewFunction: {
        JSGlobalObject* globalObject = m_codeBlock->globalObjectFor(node->origin.semantic);
        Structure* structure = JSFunction::selectStructureForNewFuncExp(globalObject, node->castOperand<FunctionExecutable*>());
        setForNode(node, structure);
        break;
    }
        
    case GetCallee:
        if (FunctionExecutable* executable = jsDynamicCast<FunctionExecutable*>(m_codeBlock->ownerExecutable())) {
            if (JSFunction* function = executable->singleton().inferredValue()) {
                m_graph.watchpoints().addLazily(m_graph, executable);
                setConstant(node, *m_graph.freeze(function));
                break;
            }
        }
        setTypeForNode(node, SpecFunction | SpecObjectOther);
        break;
        
    case GetArgumentCountIncludingThis:
        setTypeForNode(node, SpecInt32Only);
        break;

    case SetCallee:
    case SetArgumentCountIncludingThis:
        break;
        
    case GetRestLength:
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
        
    case GetGetter: {
        if (JSValue base = forNode(node->child1()).m_value) {
            GetterSetter* getterSetter = jsDynamicCast<GetterSetter*>(base);
            if (getterSetter && !getterSetter->isGetterNull()) {
                setConstant(node, *m_graph.freeze(getterSetter->getterConcurrently()));
                break;
            }
        }
        
        setTypeForNode(node, SpecObject);
        break;
    }
        
    case GetSetter: {
        if (JSValue base = forNode(node->child1()).m_value) {
            GetterSetter* getterSetter = jsDynamicCast<GetterSetter*>(base);
            if (getterSetter && !getterSetter->isSetterNull()) {
                setConstant(node, *m_graph.freeze(getterSetter->setterConcurrently()));
                break;
            }
        }
        
        setTypeForNode(node, SpecObject);
        break;
    }
        
    case GetScope: {
        JSValue value = forNode(node->child1()).value();
        if (value) {
            if (JSFunction* function = jsDynamicCast<JSFunction*>(value)) {
                setConstant(node, *m_graph.freeze(function->scope()));
                break;
            }
        }

        switch (node->child1()->op()) {
        case NewFunction:
        case NewGeneratorFunction:
        case NewAsyncGeneratorFunction:
        case NewAsyncFunction: {
            m_state.setShouldTryConstantFolding(true);
            forNode(node) = forNode(node->child1()->child1());
            break;
        }
        default:
            setTypeForNode(node, SpecObjectOther);
            break;
        }
        break;
    }

    case SkipScope: {
        if (JSValue child = forNode(node->child1()).value()) {
            if (JSScope* scope = jsDynamicCast<JSScope*>(child)) {
                if (JSScope* nextScope = scope->next()) {
                    setConstant(node, *m_graph.freeze(JSValue(nextScope)));
                    break;
                }
            }
        }
        setTypeForNode(node, SpecObjectOther);
        break;
    }

    case GetGlobalObject: {
        JSValue child = forNode(node->child1()).value();
        if (child) {
            setConstant(node, *m_graph.freeze(JSValue(asObject(child)->globalObject())));
            break;
        }

        if (forNode(node->child1()).m_structure.isFinite()) {
            JSGlobalObject* globalObject = nullptr;
            bool ok = true;
            forNode(node->child1()).m_structure.forEach(
                [&] (RegisteredStructure structure) {
                    if (!globalObject)
                        globalObject = structure->globalObject();
                    else if (globalObject != structure->globalObject())
                        ok = false;
                });
            if (globalObject && ok) {
                setConstant(node, *m_graph.freeze(JSValue(globalObject)));
                break;
            }
        }

        setTypeForNode(node, SpecObjectOther);
        break;
    }

    case UnwrapGlobalProxy: {
        if (forNode(node->child1()).m_structure.isFinite()) {
            JSGlobalObject* globalObject = nullptr;
            bool ok = true;
            forNode(node->child1()).m_structure.forEach(
                [&] (RegisteredStructure structure) {
                    if (!globalObject)
                        globalObject = structure->globalObject();
                    else if (globalObject != structure->globalObject())
                        ok = false;
                });
            if (globalObject && ok) {
                setConstant(node, *m_graph.freeze(JSValue(globalObject)));
                break;
            }
        }

        setTypeForNode(node, SpecObjectOther);
        break;
    }

    case GetGlobalThis: {
        setTypeForNode(node, SpecGlobalProxy);
        break;
    }

    case GetClosureVar: {
        JSValue value = m_graph.tryGetConstantClosureVar(forNode(node->child1()), node->scopeOffset());
        if (node->hasDoubleResult()) {
            if (value && value.isNumber()) {
                if (!std::isnan(value.asNumber())) {
                    setConstant(node, *m_graph.freeze(value));
                    break;
                }
            }
            setTypeForNode(node, SpecBytecodeRealNumber);
            break;
        }

        if (value)
            setConstant(node, *m_graph.freeze(value));
        else
            makeBytecodeTopForNode(node);
        break;
    }
            
    case PutClosureVar:
        break;

    case GetInternalField: {
        AbstractValue& child = forNode(node->child1());
        if (child.m_type && !(child.m_type & ~SpecProxyObject)) {
            if (node->internalFieldIndex() == static_cast<unsigned>(ProxyObject::Field::Target)) {
                setTypeForNode(node, SpecObject);
                break;
            }
            if (node->internalFieldIndex() == static_cast<unsigned>(ProxyObject::Field::Handler)) {
                setTypeForNode(node, SpecObject | SpecOther);
                break;
            }
        }

        makeBytecodeTopForNode(node);
        break;
    }

    case PutInternalField:
        break;


    case GetRegExpObjectLastIndex:
        makeHeapTopForNode(node);
        break;

    case SetRegExpObjectLastIndex:
    case RecordRegExpCachedResult:
        break;
        
    case GetFromArguments:
        makeHeapTopForNode(node);
        break;
        
    case PutToArguments:
        break;

    case GetArgument:
        makeHeapTopForNode(node);
        break;

    case TryGetById: {
        // This is very adhoc, but @tryGetById is not used in user code, and it is used adhocly in very limited places.
        // So adhoc one is fine.
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isFinite()
            && (node->child1().useKind() == CellUse || !(value.m_type & ~SpecCell))) {
            if (RegisteredStructure structure = value.m_structure.onlyStructure()) {
                JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
                if (structure->typeInfo().type() == RegExpObjectType
                    && !structure->hasPolyProto()
                    && structure->storedPrototype() == globalObject->regExpPrototype()
                    && !structure->isDictionary()
                    && structure->propertyAccessesAreCacheable()
                    && structure->propertyAccessesAreCacheableForAbsence()
                    && m_graph.isWatchingRegExpPrimordialPropertiesWatchpoint(node)) {
                    UniquedStringImpl* uid = node->cacheableIdentifier().uid();

                    auto attemptToFold = [&](UniquedStringImpl* name, JSValue constant) -> bool {
                        if (uid != name)
                            return false;
                        unsigned attributes;
                        PropertyOffset offset = structure->getConcurrently(uid, attributes);
                        if (isValidOffset(offset))
                            return false;
                        didFoldClobberWorld();
                        setConstant(node, *m_graph.freeze(constant));
                        return true;
                    };

                    if (attemptToFold(m_vm.propertyNames->exec.impl(), globalObject->regExpProtoExecFunction()))
                        break;

                    if (attemptToFold(m_vm.propertyNames->global.impl(), globalObject->regExpProtoGlobalGetter()))
                        break;

                    if (attemptToFold(m_vm.propertyNames->unicode.impl(), globalObject->regExpProtoUnicodeGetter()))
                        break;

                    if (attemptToFold(m_vm.propertyNames->unicodeSets.impl(), globalObject->regExpProtoUnicodeSetsGetter()))
                        break;

                    if (attemptToFold(m_vm.propertyNames->replaceSymbol.impl(), globalObject->regExpProtoSymbolReplaceFunction()))
                        break;
                }
            }
        }

        // FIXME: This should constant fold at least as well as the normal GetById case.
        // https://bugs.webkit.org/show_bug.cgi?id=156422
        clobberWorld();
        makeHeapTopForNode(node);
        break;
    }

    case GetPrivateNameById:
    case GetByIdDirect:
    case GetByIdDirectFlush:
    case GetById:
    case GetByIdFlush:
    case GetByIdMegamorphic: {
        AbstractValue& value = forNode(node->child1());

        if (Options::useAccessInlining()
            && value.m_structure.isFinite()
            && (node->child1().useKind() == CellUse || !(value.m_type & ~SpecCell))) {
            UniquedStringImpl* uid = node->cacheableIdentifier().uid();
            GetByStatus status = GetByStatus::computeFor(value.m_structure.toStructureSet(), uid);
            if (status.isSimple()) {
                // Figure out what the result is going to be - is it TOP, a constant, or maybe
                // something more subtle?
                AbstractValue result;
                for (unsigned i = status.numVariants(); i--;) {
                    // This thing won't give us a variant that involves prototypes. If it did, we'd
                    // have more work to do here.
                    DFG_ASSERT(m_graph, node, status[i].conditionSet().isEmpty());
                    const auto& variant = status[i];
                    result.merge(m_graph.inferredValueForProperty(value, *m_graph.addStructureSet(variant.structureSet()), variant.offset(), m_state.structureClobberState()));
                }
            
                m_state.setShouldTryConstantFolding(true);
                didFoldClobberWorld();
                forNode(node) = result;
                break;
            }
        }

        clobberWorld();
        makeHeapTopForNode(node);
        break;
    }

    case GetPrivateName:
    case GetByValWithThis:
    case GetByValWithThisMegamorphic:
    case GetByIdWithThis:
    case GetByIdWithThisMegamorphic:
        clobberWorld();
        makeHeapTopForNode(node);
        break;
            
    case GetArrayLength:
    case GetUndetachedTypeArrayLength: {
        ArrayMode arrayMode = node->arrayMode();
        AbstractValue& abstractValue = forNode(node->child1());
        if (JSValue constant = abstractValue.m_value) {
            JSArrayBufferView* view = m_graph.tryGetFoldableView(constant, arrayMode);
            if (view && !view->isResizableOrGrowableShared() && isInBounds<int32_t>(view->length())) {
                setConstant(node, jsNumber(view->length()));
                break;
            }

            if (constant.isString() && node->arrayMode().type() == Array::String) {
                setConstant(node, jsNumber(asString(constant)->length()));
                break;
            }
        }

        if (node->op() == GetArrayLength) {
            if (arrayMode.type() != Array::AnyTypedArray && arrayMode.isSomeTypedArrayView() && !arrayMode.mayBeResizableOrGrowableSharedTypedArray()) {
                if ((abstractValue.m_type && !(abstractValue.m_type & ~SpecObject)) && abstractValue.m_structure.isFinite()) {
                    bool canFold = !abstractValue.m_structure.isClear();
                    JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
                    abstractValue.m_structure.forEach([&](RegisteredStructure structure) {
                        if (!arrayMode.structureWouldPassArrayModeFiltering(structure.get())) {
                            canFold = false;
                            return;
                        }

                        if (structure->globalObject() != globalObject) {
                            canFold = false;
                            return;
                        }
                    });

                    if (canFold) {
                        if (m_graph.isWatchingArrayBufferDetachWatchpoint(node))
                            m_state.setShouldTryConstantFolding(true);
                    }
                }
            }
        }

        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }

    case GetTypedArrayLengthAsInt52: {
        JSArrayBufferView* view = m_graph.tryGetFoldableView(
            forNode(node->child1()).m_value, node->arrayMode());
        if (view && !view->isResizableOrGrowableShared()) {
            setConstant(node, jsNumber(view->length()));
            break;
        }
        setNonCellTypeForNode(node, SpecInt52Any);
        break;
    }

    case GetVectorLength: {
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }

    case DeleteById:
    case DeleteByVal: {
        // FIXME: This could decide if the delete will be successful based on the set of structures that
        // we get from our base value. https://bugs.webkit.org/show_bug.cgi?id=156611
        clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }
        
    case CheckStructure: {
        AbstractValue& value = forNode(node->child1());

        const RegisteredStructureSet& set = node->structureSet();
        
        // It's interesting that we could have proven that the object has a larger structure set
        // that includes the set we're testing. In that case we could make the structure check
        // more efficient. We currently don't.
        
        if (value.m_structure.isSubsetOf(set))
            m_state.setShouldTryConstantFolding(true);
        else if (value.m_type) {
            if (set.onlyStructure().get() == m_vm.stringStructure.get()
                || set.onlyStructure().get() == m_vm.symbolStructure.get()
                || set.onlyStructure().get() == m_vm.bigIntStructure.get())
                m_state.setShouldTryConstantFolding(true);
        }

        SpeculatedType admittedTypes = SpecNone;
        switch (node->child1().useKind()) {
        case CellUse:
        case KnownCellUse:
            admittedTypes = SpecNone;
            break;
        case CellOrOtherUse:
            admittedTypes = SpecOther;
            break;
        default:
            DFG_CRASH(m_graph, node, "Bad use kind");
            break;
        }
        
        filter(value, set, admittedTypes);
        break;
    }

    case CheckStructureOrEmpty: {
        AbstractValue& value = forNode(node->child1());

        bool mayBeEmpty = value.m_type & SpecEmpty;
        if (!mayBeEmpty)
            m_state.setShouldTryConstantFolding(true);

        SpeculatedType admittedTypes = mayBeEmpty ? SpecEmpty : SpecNone;
        filter(value, node->structureSet(), admittedTypes);
        break;
    }
        
    case CheckStructureImmediate: {
        // FIXME: This currently can only reason about one structure at a time.
        // https://bugs.webkit.org/show_bug.cgi?id=136988
        
        AbstractValue& value = forNode(node->child1());
        const RegisteredStructureSet& set = node->structureSet();
        
        if (value.value()) {
            if (Structure* structure = jsDynamicCast<Structure*>(value.value())) {
                if (set.contains(m_graph.registerStructure(structure))) {
                    m_state.setShouldTryConstantFolding(true);
                    break;
                }
            }
            m_state.setIsValid(false);
            break;
        }
        
        if (m_phiChildren) {
            bool allGood = true;
            m_phiChildren->forAllTransitiveIncomingValues(
                node,
                [&] (Node* incoming) {
                    if (Structure* structure = incoming->dynamicCastConstant<Structure*>()) {
                        if (set.contains(m_graph.registerStructure(structure)))
                            return;
                    }
                    allGood = false;
                });
            if (allGood) {
                m_state.setShouldTryConstantFolding(true);
                break;
            }
        }
            
        if (RegisteredStructure structure = set.onlyStructure()) {
            filterByValue(node->child1(), *m_graph.freeze(structure.get()));
            break;
        }
        
        // Aw shucks, we can't do anything!
        break;
    }
        
    case PutStructure:
        if (!forNode(node->child1()).m_structure.isClear()) {
            if (forNode(node->child1()).m_structure.onlyStructure() == node->transition()->next) {
                didFoldClobberStructures();
                m_state.setShouldTryConstantFolding(true);
            } else {
                observeTransition(
                    clobberLimit, node->transition()->previous, node->transition()->next);
                forNode(node->child1()).changeStructure(m_graph, node->transition()->next);
            }
        } else {
            // We're going to exit before we get here, but for the sake of validation, we've folded our write to StructureID.
            didFoldClobberStructures();
        }
        break;
    case GetButterfly:
    case AllocatePropertyStorage:
    case ReallocatePropertyStorage:
    case NukeStructureAndSetButterfly:
        // FIXME: We don't model the fact that the structureID is nuked, simply because currently
        // nobody would currently benefit from having that information. But it's a bug nonetheless.
        if (node->op() == NukeStructureAndSetButterfly)
            didFoldClobberStructures();
        clearForNode(node); // The result is not a JS value.
        break;
    case CheckJSCast: {
        const ClassInfo* classInfo = node->classInfo();
        JSValue constant = forNode(node->child1()).value();
        if (constant) {
            if (constant.isCell() && constant.asCell()->inherits(classInfo)) {
                ASSERT(!classInfo->inheritsJSTypeRange || classInfo->inheritsJSTypeRange->contains(constant.asCell()->type()));
                m_state.setShouldTryConstantFolding(true);
                ASSERT(constant);
                break;
            }
        }

        AbstractValue& value = forNode(node->child1());

        if (value.m_structure.isSubClassOf(classInfo))
            m_state.setShouldTryConstantFolding(true);

        filterClassInfo(value, classInfo);
        break;
    }
    case CheckNotJSCast: {
        const ClassInfo* classInfo = node->classInfo();
        JSValue constant = forNode(node->child1()).value();
        if (constant) {
            if (constant.isCell() && !constant.asCell()->inherits(classInfo)) {
                ASSERT(!classInfo->inheritsJSTypeRange || !classInfo->inheritsJSTypeRange->contains(constant.asCell()->type()));
                m_state.setShouldTryConstantFolding(true);
                ASSERT(constant);
                break;
            }
        }

        AbstractValue& value = forNode(node->child1());

        if (value.m_structure.isNotSubClassOf(classInfo))
            m_state.setShouldTryConstantFolding(true);
        break;
    }

    case CallDOMGetter: {
        CallDOMGetterData* callDOMGetterData = node->callDOMGetterData();
        DOMJIT::CallDOMGetterSnippet* snippet = callDOMGetterData->snippet;
        if (!snippet || snippet->effect.writes)
            clobberWorld();
        if (callDOMGetterData->domJIT)
            setTypeForNode(node, callDOMGetterData->domJIT->resultType());
        else
            makeBytecodeTopForNode(node);
        break;
    }
    case CallDOM: {
        const DOMJIT::Signature* signature = node->signature();
        if (signature->effect.writes)
            clobberWorld();
        setTypeForNode(node, signature->result);
        break;
    }

    case CheckArrayOrEmpty:
    case CheckArray: {
        AbstractValue& value = forNode(node->child1());

        SpeculatedType admittedTypes = SpecNone;
        if (node->op() == CheckArrayOrEmpty) {
            bool mayBeEmpty = value.m_type & SpecEmpty;
            if (!mayBeEmpty)
                m_state.setShouldTryConstantFolding(true);
            else
                admittedTypes = SpecEmpty;
        }

        if (node->arrayMode().alreadyChecked(m_graph, node, value)) {
            m_state.setShouldTryConstantFolding(true);
            break;
        }

        switch (node->arrayMode().type()) {
        case Array::String:
            filter(node->child1(), SpecString | admittedTypes);
            break;
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::Undecided:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            break;
        case Array::DirectArguments:
            filter(node->child1(), SpecDirectArguments | admittedTypes);
            break;
        case Array::ScopedArguments:
            filter(node->child1(), SpecScopedArguments | admittedTypes);
            break;
        case Array::Int8Array:
            filter(node->child1(), SpecInt8Array | admittedTypes);
            break;
        case Array::Int16Array:
            filter(node->child1(), SpecInt16Array | admittedTypes);
            break;
        case Array::Int32Array:
            filter(node->child1(), SpecInt32Array | admittedTypes);
            break;
        case Array::Uint8Array:
            filter(node->child1(), SpecUint8Array | admittedTypes);
            break;
        case Array::Uint8ClampedArray:
            filter(node->child1(), SpecUint8ClampedArray | admittedTypes);
            break;
        case Array::Uint16Array:
            filter(node->child1(), SpecUint16Array | admittedTypes);
            break;
        case Array::Uint32Array:
            filter(node->child1(), SpecUint32Array | admittedTypes);
            break;
        case Array::Float32Array:
            filter(node->child1(), SpecFloat32Array | admittedTypes);
            break;
        case Array::Float64Array:
            filter(node->child1(), SpecFloat64Array | admittedTypes);
            break;
        case Array::BigInt64Array:
            filter(node->child1(), SpecBigInt64Array | admittedTypes);
            break;
        case Array::BigUint64Array:
            filter(node->child1(), SpecBigUint64Array | admittedTypes);
            break;
        case Array::Float16Array:
            filter(node->child1(), SpecFloat16Array | admittedTypes);
            break;
        case Array::AnyTypedArray:
            filter(node->child1(), SpecTypedArrayView | admittedTypes);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        filterArrayModes(node->child1(), node->arrayMode().arrayModesThatPassFiltering(), admittedTypes);
        break;
    }

    case Arrayify: {
        if (node->arrayMode().alreadyChecked(m_graph, node, forNode(node->child1()))) {
            didFoldClobberStructures();
            m_state.setShouldTryConstantFolding(true);
            break;
        }
        ASSERT(node->arrayMode().conversion() == Array::Convert);
        clobberStructures();
        filterArrayModes(node->child1(), node->arrayMode().arrayModesThatPassFiltering());
        break;
    }
    case ArrayifyToStructure: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isSubsetOf(RegisteredStructureSet(node->structure())))
            m_state.setShouldTryConstantFolding(true);
        clobberStructures();
        
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
        forNode(node->child1()).set(m_graph, node->structure());
        break;
    }
    case GetIndexedPropertyStorage: {
        ASSERT(node->arrayMode().type() != Array::String);
        JSArrayBufferView* view = m_graph.tryGetFoldableView(
            forNode(node->child1()).m_value, node->arrayMode());
        if (view)
            m_state.setShouldTryConstantFolding(true);
        clearForNode(node);
        break;
    }
    case ResolveRope: {
        JSValue childConst = forNode(node->child1()).value();
        if (childConst && childConst.isString() && !asString(childConst)->isRope()) {
            setConstant(node, *m_graph.freeze(childConst));
            break;
        }

        if (!(forNode(node->child1()).m_type & ~SpecStringIdent)) {
            m_state.setShouldTryConstantFolding(true);
            setForNode(node, forNode(node->child1()));
            break;
        }

        setTypeForNode(node, SpecString);
        break;
    }
    case ConstantStoragePointer: {
        clearForNode(node);
        break; 
    }
        
    case GetTypedArrayByteOffset: {
        JSArrayBufferView* view = m_graph.tryGetFoldableView(forNode(node->child1()).m_value);
        if (view && !view->isResizableOrGrowableShared()) {
            size_t byteOffset = view->byteOffset();
            if (isInBounds<int32_t>(byteOffset)) {
                setConstant(node, jsNumber(byteOffset));
                break;
            }
        }
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }

    case GetTypedArrayByteOffsetAsInt52: {
        JSArrayBufferView* view = m_graph.tryGetFoldableView(forNode(node->child1()).m_value);
        if (view && !view->isResizableOrGrowableShared()) {
            size_t byteOffset = view->byteOffset();
            setConstant(node, jsNumber(byteOffset));
            break;
        }
        setNonCellTypeForNode(node, SpecInt52Any);
        break;
    }

    case GetPrototypeOf: {
        AbstractValue& value = forNode(node->child1());
        if ((value.m_type && !(value.m_type & ~SpecObject)) && value.m_structure.isFinite()) {
            bool canFold = !value.m_structure.isClear();
            JSValue prototype;
            value.m_structure.forEach([&] (RegisteredStructure structure) {
                if (structure->typeInfo().overridesGetPrototype()) {
                    canFold = false;
                    return;
                }

                if (structure->hasPolyProto()) {
                    canFold = false;
                    return;
                }
                if (!prototype)
                    prototype = structure->storedPrototype();
                else if (prototype != structure->storedPrototype())
                    canFold = false;
            });

            if (prototype && canFold) {
                switch (node->child1().useKind()) {
                case ArrayUse:
                case FunctionUse:
                case FinalObjectUse:
                    break;
                default:
                    didFoldClobberWorld();
                    break;
                }
                setConstant(node, *m_graph.freeze(prototype));
                break;
            }
        }

        switch (node->child1().useKind()) {
        case ArrayUse:
        case FunctionUse:
        case FinalObjectUse:
            break;
        default:
            clobberWorld();
            break;
        }
        setTypeForNode(node, SpecObject | SpecOther);
        break;
    }

    case GetWebAssemblyInstanceExports: {
#if ENABLE(WEBASSEMBLY)
        AbstractValue& base = forNode(node->child1());
        if (base.m_value) {
            if (auto* instance = jsDynamicCast<JSWebAssemblyInstance*>(base.m_value)) {
                if (auto* moduleRecord = instance->moduleRecord()) {
                    if (auto* exportsObject = moduleRecord->exportsObject()) {
                        setConstant(node, *m_graph.freeze(exportsObject));
                        break;
                    }
                }
            }
        }
#endif
        setTypeForNode(node, SpecFinalObject);
        break;
    }

    case GetByOffset: {
        StorageAccessData& data = node->storageAccessData();

        // FIXME: The part of this that handles inferred property types relies on AI knowing the structure
        // right now. That's probably not optimal. In some cases, we may perform an optimization (usually
        // by something other than AI, maybe by CSE for example) that obscures AI's view of the structure
        // at the point where GetByOffset runs. Currently, when that happens, we'll have to rely entirely
        // on the type that ByteCodeParser was able to prove.
        AbstractValue value = m_graph.inferredValueForProperty(forNode(node->child2()), data.offset, m_state.structureClobberState());

        // If we decide that there does not exist any value that this can return, then it's probably
        // because the compilation was already invalidated.
        if (value.isClear())
            m_state.setIsValid(false);

        if (node->hasDoubleResult()) {
            if (value.isType(SpecBytecodeRealNumber))
                setForNode(node, value);
            else
                setTypeForNode(node, SpecBytecodeRealNumber);
        } else
            setForNode(node, value);

        if (value.m_value)
            m_state.setShouldTryConstantFolding(true);
        break;
    }
        
    case GetGetterSetterByOffset: {
        StorageAccessData& data = node->storageAccessData();
        AbstractValue& base = forNode(node->child2());
        JSValue result = m_graph.tryGetConstantProperty(base, data.offset);
        if (result && jsDynamicCast<GetterSetter*>(result)) {
            setConstant(node, *m_graph.freeze(result));
            break;
        }
        
        setForNode(node, m_vm.getterSetterStructure.get());
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
        
        // Ordinarily you have to be careful with calling setShouldTryConstantFolding()
        // because of the effect on compile times, but this node is FTL-only.
        m_state.setShouldTryConstantFolding(true);
        
        AbstractValue& base = forNode(node->child1());
        RegisteredStructureSet baseSet;
        AbstractValue result;
        for (const MultiGetByOffsetCase& getCase : node->multiGetByOffsetData().cases) {
            RegisteredStructureSet set = getCase.set();
            set.filter(base);
            if (set.isEmpty())
                continue;
            baseSet.merge(set);

            switch (getCase.method().kind()) {
            case GetByOffsetMethod::Constant: {
                AbstractValue thisResult;
                thisResult.set(
                    m_graph,
                    *getCase.method().constant(),
                    m_state.structureClobberState());
                result.merge(thisResult);
                break;
            }

            default: {
                result.makeHeapTop();
                break;
            } }
        }
        
        if (forNode(node->child1()).changeStructure(m_graph, baseSet) == Contradiction)
            m_state.setIsValid(false);
        
        if (node->hasDoubleResult()) {
            if (result.isType(SpecBytecodeRealNumber))
                setForNode(node, result);
            else
                setTypeForNode(node, SpecBytecodeRealNumber);
        } else
            setForNode(node, result);
        break;
    }
            
    case PutByOffset: {
        break;
    }
        
    case MultiPutByOffset: {
        RegisteredStructureSet newSet;
        TransitionVector transitions;
        
        // Ordinarily you have to be careful with calling setShouldTryConstantFolding()
        // because of the effect on compile times, but this node is FTL-only.
        m_state.setShouldTryConstantFolding(true);
        
        AbstractValue& base = forNode(node->child1());
        AbstractValue& originalValue = forNode(node->child2());
        AbstractValue resultingValue;
        
        if (node->multiPutByOffsetData().writesStructures())
            didFoldClobberStructures();
            
        for (unsigned i = node->multiPutByOffsetData().variants.size(); i--;) {
            const PutByVariant& variant = node->multiPutByOffsetData().variants[i];
            RegisteredStructureSet thisSet = *m_graph.addStructureSet(variant.oldStructure());
            thisSet.filter(base);
            if (thisSet.isEmpty())
                continue;

            AbstractValue& thisValue = originalValue;
            resultingValue.merge(thisValue);
            
            if (variant.kind() == PutByVariant::Transition) {
                RegisteredStructure newStructure = m_graph.registerStructure(variant.newStructure());
                if (thisSet.onlyStructure() != newStructure) {
                    transitions.append(
                        Transition(m_graph.registerStructure(variant.oldStructureForTransition()), newStructure));
                } // else this is really a replace.
                newSet.add(newStructure);
            } else {
                ASSERT(variant.kind() == PutByVariant::Replace);
                newSet.merge(thisSet);
            }
        }
        
        // We need to order AI executing these effects in the same order as they're executed
        // at runtime. This is critical when you have JS code like `o.f = o;`. We first
        // filter types on o, then transition o. Not the other way around. If we got
        // this ordering wrong, we could end up with the wrong type representing o.
        setForNode(node->child2(), resultingValue);
        if (!!originalValue && !resultingValue)
            m_state.setIsValid(false);

        observeTransitions(clobberLimit, transitions);
        if (forNode(node->child1()).changeStructure(m_graph, newSet) == Contradiction)
            m_state.setIsValid(false);
        break;
    }

    case MultiDeleteByOffset: {
        RegisteredStructureSet newSet;
        TransitionVector transitions;

        // Ordinarily you have to be careful with calling setShouldTryConstantFolding()
        // because of the effect on compile times, but this node is FTL-only.
        m_state.setShouldTryConstantFolding(true);

        AbstractValue& base = forNode(node->child1());

        if (node->multiDeleteByOffsetData().writesStructures())
            didFoldClobberStructures();

        for (unsigned i = node->multiDeleteByOffsetData().variants.size(); i--;) {
            const DeleteByVariant& variant = node->multiDeleteByOffsetData().variants[i];
            RegisteredStructureSet thisSet = *m_graph.addStructureSet(variant.oldStructure());
            thisSet.filter(base);
            if (thisSet.isEmpty())
                continue;

            if (variant.newStructure()) {
                RegisteredStructure newStructure = m_graph.registerStructure(variant.newStructure());
                transitions.append(
                    Transition(m_graph.registerStructure(variant.oldStructure()), newStructure));
                newSet.add(newStructure);
            } else
                newSet.merge(thisSet);
        }

        observeTransitions(clobberLimit, transitions);
        if (forNode(node->child1()).changeStructure(m_graph, newSet) == Contradiction)
            m_state.setIsValid(false);
        setNonCellTypeForNode(node, SpecBoolean);
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

        switch (node->child1()->op()) {
        case NewFunction:
        case NewGeneratorFunction:
        case NewAsyncGeneratorFunction:
        case NewAsyncFunction: {
            setConstant(node, *node->child1()->cellOperand());
            break;
        }
        default:
            setTypeForNode(node, SpecCellOther);
            break;
        }
        break;
    }

    case CheckIsConstant: {
        AbstractValue& value = forNode(node->child1());
        if (value.value() == node->constant()->value() && (value.value() || value.m_type == SpecEmpty)) {
            m_state.setShouldTryConstantFolding(true);
            break;
        }
        filterByValue(node->child1(), *node->constant());
        break;
    }

    case AssertNotEmpty:
    case CheckNotEmpty: {
        AbstractValue& value = forNode(node->child1());
        if (!(value.m_type & SpecEmpty)) {
            m_state.setShouldTryConstantFolding(true);
            break;
        }
        
        filter(value, ~SpecEmpty);
        break;
    }

    case CheckIdent: {
        AbstractValue& value = forNode(node->child1());
        UniquedStringImpl* uid = node->uidOperand();

        JSValue childConstant = value.value();
        if (childConstant) {
            if (childConstant.isString()) {
                if (asString(childConstant)->tryGetValueImpl() == uid) {
                    m_state.setShouldTryConstantFolding(true);
                    break;
                }
            } else if (childConstant.isSymbol()) {
                if (&jsCast<Symbol*>(childConstant)->uid() == uid) {
                    m_state.setShouldTryConstantFolding(true);
                    break;
                }
            }
        }

        if (node->child1().useKind() == StringIdentUse)
            filter(value, SpecStringIdent);
        else
            filter(value, SpecSymbol);
        break;
    }

    case AssertInBounds:
        break;

    case CheckInBounds: {
        JSValue left = forNode(node->child1()).value();
        JSValue right = forNode(node->child2()).value();
        if (left && right && left.isInt32() && right.isInt32() && static_cast<uint32_t>(left.asInt32()) < static_cast<uint32_t>(right.asInt32()))
            m_state.setShouldTryConstantFolding(true);

        // We claim we result in Int32. It's not really important what our result is (though we
        // don't want to claim we may result in the empty value), other nodes with data flow edges
        // to us just do that to maintain the invariant that they can't be hoisted higher than us.
        // So we just arbitrarily pick Int32. In some ways, StorageResult may be the more correct
        // thing to do here. We pick NodeResultJS because it makes converting this to an identity
        // easier.
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }
    case CheckInBoundsInt52: {
        // See the CheckInBounds case, it does not really matter what we put here.
        setNonCellTypeForNode(node, SpecInt32Only);
        break;
    }

    case CheckPrivateBrand:
    case SetPrivateBrand:
    case PutPrivateName: {
        clobberWorld();
        break;
    }

    case PutPrivateNameById:
    case PutById:
    case PutByIdMegamorphic:
    case PutByIdFlush:
    case PutByIdDirect: {
        AbstractValue& value = forNode(node->child1());
        if (Options::useAccessInlining() && value.m_structure.isFinite()) {
            bool isDirect = node->op() == PutByIdDirect || node->op() == PutPrivateNameById;
            auto privateFieldPutKind = node->op() == PutPrivateNameById ? node->privateFieldPutKind() : PrivateFieldPutKind::none();
            PutByStatus status = PutByStatus::computeFor(
                m_graph.globalObjectFor(node->origin.semantic),
                value.m_structure.toStructureSet(),
                node->cacheableIdentifier(),
                isDirect, privateFieldPutKind);

            bool allGood = true;
            if (status.isSimple()) {
                RegisteredStructureSet newSet;
                TransitionVector transitions;
                
                for (const PutByVariant& variant : status.variants()) {
                    for (const ObjectPropertyCondition& condition : variant.conditionSet()) {
                        if (!m_graph.watchCondition(condition)) {
                            allGood = false;
                            break;
                        }
                    }

                    if (!allGood)
                        break;

                    if (variant.kind() == PutByVariant::Transition) {
                        RegisteredStructure newStructure = m_graph.registerStructure(variant.newStructure());
                        transitions.append(
                            Transition(
                                m_graph.registerStructure(variant.oldStructureForTransition()), newStructure));
                        newSet.add(newStructure);
                    } else {
                        ASSERT(variant.kind() == PutByVariant::Replace);
                        newSet.merge(*m_graph.addStructureSet(variant.oldStructure()));
                    }
                }

                if (status.numVariants() == 1 || m_graph.m_plan.isFTL())
                    m_state.setShouldTryConstantFolding(true);
                
                if (allGood) {
                    didFoldClobberWorld();
                    observeTransitions(clobberLimit, transitions);
                    if (forNode(node->child1()).changeStructure(m_graph, newSet) == Contradiction)
                        m_state.setIsValid(false);
                    break;
                }
            }
        }
        
        clobberWorld();
        break;
    }

    case PutByValWithThis:
    case PutByIdWithThis:
        clobberWorld();
        break;

    case PutGetterById:
    case PutSetterById:
    case PutGetterSetterById:
    case PutGetterByVal:
    case PutSetterByVal: {
        clobberWorld();
        break;
    }

    case DefineDataProperty:
    case DefineAccessorProperty:
        clobberWorld();
        break;
        
    case InById:
    case InByIdMegamorphic: {
        // FIXME: We can determine when the property definitely exists based on abstract
        // value information.
        clobberWorld();
        filter(node->child1(), SpecObject);
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case InByVal:
    case InByValMegamorphic: {
        AbstractValue& property = forNode(node->child2());
        if (JSValue constant = property.value()) {
            if (constant.isString()) {
                JSString* string = asString(constant);
                if (CacheableIdentifier::isCacheableIdentifierCell(string) && !parseIndex(CacheableIdentifier::createFromCell(string).uid()))
                    m_state.setShouldTryConstantFolding(true);
            }
        }

        // FIXME: We can determine when the property definitely exists based on abstract
        // value information.
        clobberWorld();
        filter(node->child1(), SpecObject);
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case HasPrivateName:
    case HasPrivateBrand: {
        clobberWorld();
        filter(node->child1(), SpecObject);
        filter(node->child2(), SpecSymbol);
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case HasOwnProperty: {
        clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case HasIndexedProperty: {
        ArrayMode mode = node->arrayMode();
        switch (mode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::ArrayStorage: {
            if (mode.isInBounds())
                break;
            FALLTHROUGH;
        }
        default: {
            clobberWorld();
            break;
        }
        }
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case GetPropertyEnumerator: {
        setTypeForNode(node, SpecCell);
        clobberWorld();
        break;
    }

    case EnumeratorNextUpdateIndexAndMode: {
        ArrayMode arrayMode = node->arrayMode();
        if (node->enumeratorMetadata() == JSPropertyNameEnumerator::OwnStructureMode && m_graph.varArgChild(node, 0).useKind() == CellUse)
            setTupleConstant(node, 1, jsNumber(static_cast<uint8_t>(JSPropertyNameEnumerator::OwnStructureMode)));
        else if (node->enumeratorMetadata() != JSPropertyNameEnumerator::IndexedMode) {
            m_state.setNonCellTypeForTupleNode(node, 1, SpecInt32Only);
            clobberWorld();
        } else {
            setTupleConstant(node, 1, jsNumber(static_cast<uint8_t>(JSPropertyNameEnumerator::IndexedMode)));
            switch (arrayMode.type()) {
            case Array::Int32:
            case Array::Double:
            case Array::Contiguous:
            case Array::ArrayStorage: {
                if (arrayMode.isInBounds())
                    break;
                FALLTHROUGH;
            }
            default: {
                clobberWorld();
                break;
            }
            }
        }
        m_state.setNonCellTypeForTupleNode(node, 0, SpecInt32Only);
        clearForNode(node);
        break;
    }

    case EnumeratorNextUpdatePropertyName: {
        setTypeForNode(node, SpecStringIdent);
        break;
    }

    case EnumeratorInByVal:
    case EnumeratorHasOwnProperty: {
        // FIXME: We can determine when the property definitely exists based on abstract
        // value information.
        clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case GetGlobalVar: {
        if (node->hasDoubleResult())
            setTypeForNode(node, SpecBytecodeRealNumber);
        else
            makeHeapTopForNode(node);
        break;
    }

    case GetGlobalLexicalVariable:
        if (node->hasDoubleResult())
            setTypeForNode(node, SpecBytecodeRealNumber);
        else
            makeBytecodeTopForNode(node);
        break;

    case GetDynamicVar:
        clobberWorld();
        makeBytecodeTopForNode(node);
        break;

    case PutDynamicVar:
        clobberWorld();
        break;

    case ResolveScope:
        clobberWorld();
        setTypeForNode(node, SpecObject);
        break;

    case ResolveScopeForHoistingFuncDeclInEval:
        clobberWorld();
        makeBytecodeTopForNode(node);
        break;

    case PutGlobalVariable:
    case NotifyWrite:
        break;

    case OverridesHasInstance:
        setNonCellTypeForNode(node, SpecBoolean);
        break;

    case InstanceOf:
        clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;

    case InstanceOfMegamorphic:
        clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;

    case InstanceOfCustom:
        clobberWorld();
        setNonCellTypeForNode(node, SpecBoolean);
        break;
        
    case MatchStructure: {
        AbstractValue& base = forNode(node->child1());
        RegisteredStructureSet baseSet;
        
        BooleanLattice result = BooleanLattice::Bottom;
        for (MatchStructureVariant& variant : node->matchStructureData().variants) {
            RegisteredStructure structure = variant.structure;
            if (!base.contains(structure)) {
                m_state.setShouldTryConstantFolding(true);
                continue;
            }
            
            baseSet.add(structure);
            result = leastUpperBoundOfBooleanLattices(
                result, variant.result ? BooleanLattice::True : BooleanLattice::False);
        }
        
        if (forNode(node->child1()).changeStructure(m_graph, baseSet) == Contradiction)
            m_state.setIsValid(false);
        
        switch (result) {
        case BooleanLattice::False:
            setConstant(node, jsBoolean(false));
            break;
        case BooleanLattice::True:
            setConstant(node, jsBoolean(true));
            break;
        default:
            setNonCellTypeForNode(node, SpecBoolean);
            break;
        }
        break;
    }
            
    case Phi:
        RELEASE_ASSERT(m_graph.m_form == SSA);
        setForNode(node, forNode(NodeFlowProjection(node, NodeFlowProjection::Shadow)));
        // The state of this node would have already been decided, but it may have become a
        // constant, in which case we'd like to know.
        if (forNode(node).m_value)
            m_state.setShouldTryConstantFolding(true);
        break;
        
    case Upsilon: {
        NodeFlowProjection shadow(node->phi(), NodeFlowProjection::Shadow);
        if (shadow.isStillValid()) {
            m_state.createValueForNode(shadow);
            setForNode(shadow, forNode(node->child1()));
        }
        break;
    }
        
    case Flush:
    case PhantomLocal:
        break;

    case Construct: {
        Edge calleeNode = m_graph.child(node, 0);
        Edge newTargetNode = m_graph.child(node, 1);
        JSValue calleeValue = forNode(calleeNode).m_value;
        JSValue newTargetValue = forNode(newTargetNode).m_value;
        if (calleeValue && newTargetValue) {
            auto* callee = jsDynamicCast<JSObject*>(calleeValue);
            auto* newTarget = jsDynamicCast<JSFunction*>(newTargetValue);
            if (callee && newTarget) {
                JSGlobalObject* globalObject = m_graph.globalObjectFor(node->origin.semantic);
                if (callee->globalObject() == globalObject) {
                    if (FunctionRareData* rareData = newTarget->rareData()) {
                        if (rareData->allocationProfileWatchpointSet().isStillValid() && globalObject->structureCacheClearedWatchpointSet().isStillValid()) {
                            Structure* structure = rareData->internalFunctionAllocationStructure();
                            if (callee->classInfo() == ObjectConstructor::info() && node->numChildren() == 2) {
                                if (structure && structure->classInfoForCells() == JSFinalObject::info() && structure->hasMonoProto()) {
                                    m_graph.freeze(rareData);
                                    m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                                    m_graph.freeze(globalObject);
                                    m_graph.watchpoints().addLazily(globalObject->structureCacheClearedWatchpointSet());
                                    m_state.setShouldTryConstantFolding(true);
                                    didFoldClobberWorld();
                                    setForNode(node, structure);
                                    break;
                                }
                            }

                            if (callee->classInfo() == ArrayConstructor::info() && node->numChildren() == 3 && !m_graph.hasExitSite(node->origin.semantic, BadType) && !m_graph.hasExitSite(node->origin.semantic, OutOfBounds)) {
                                if (structure && structure->classInfoForCells() == JSArray::info() && structure->hasMonoProto() && !hasAnyArrayStorage(structure->indexingType())) {
                                    if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {

                                        m_graph.freeze(rareData);
                                        m_graph.watchpoints().addLazily(rareData->allocationProfileWatchpointSet());
                                        m_graph.freeze(globalObject);
                                        m_graph.watchpoints().addLazily(globalObject->structureCacheClearedWatchpointSet());
                                        m_state.setShouldTryConstantFolding(true);
                                        didFoldClobberWorld();
                                        setTypeForNode(node, SpecArray);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        clobberWorld();
        makeHeapTopForNode(node);
        break;
    }
            
    case Call:
    case TailCallInlinedCaller:
    case CallVarargs:
    case CallForwardVarargs:
    case TailCallVarargsInlinedCaller:
    case ConstructVarargs:
    case ConstructForwardVarargs:
    case TailCallForwardVarargsInlinedCaller:
    case CallDirectEval:
    case DirectCall:
    case DirectConstruct:
    case DirectTailCallInlinedCaller:
    case CallCustomAccessorGetter:
    case CallCustomAccessorSetter:
        clobberWorld();
        makeHeapTopForNode(node);
        break;

    case CallWasm: {
#if ENABLE(WEBASSEMBLY)
        clobberWorld();

        WebAssemblyFunction* wasmFunction = node->castOperand<WebAssemblyFunction*>();
        const auto& signature = Wasm::TypeInformation::getFunctionSignature(wasmFunction->typeIndex());
        if (signature.returnsVoid()) {
            setConstant(node, jsUndefined());
            break;
        }

        ASSERT(signature.returnCount() == 1);
        auto type = signature.returnType(0);
        switch (type.kind) {
        case Wasm::TypeKind::I32: {
            setNonCellTypeForNode(node, SpecInt32Only);
            break;
        }
        case Wasm::TypeKind::I64: {
            setTypeForNode(node, SpecBigInt);
            break;
        }
        case Wasm::TypeKind::Ref:
        case Wasm::TypeKind::RefNull:
        case Wasm::TypeKind::Funcref:
        case Wasm::TypeKind::Externref: {
            makeHeapTopForNode(node);
            break;
        }
        case Wasm::TypeKind::F32:
        case Wasm::TypeKind::F64: {
            setNonCellTypeForNode(node, SpecBytecodeDouble);
            break;
        }
        case Wasm::TypeKind::V128:
        default: {
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        }
#endif
        break;
    }

    case ForceOSRExit:
    case CheckBadValue:
        m_state.setIsValid(false);
        break;
        
    case InvalidationPoint:
        m_state.setStructureClobberState(StructuresAreWatched);
        m_state.observeInvalidationPoint();
        break;

    case CPUIntrinsic: 
        if (node->intrinsic() == CPURdtscIntrinsic)
            setNonCellTypeForNode(node, SpecInt32Only);
        else
            setNonCellTypeForNode(node, SpecOther);
        break;

    case CheckTraps:
    case LogShadowChickenPrologue:
    case LogShadowChickenTail:
    case ProfileType:
    case ProfileControlFlow:
    case Phantom:
    case CountExecution:
    case CheckTierUpInLoop:
    case CheckTierUpAtReturn:
    case CheckDetached:
    case SuperSamplerBegin:
    case SuperSamplerEnd:
    case CheckTierUpAndOSREnter:
    case LoopHint:
    case ExitOK:
    case FilterCallLinkStatus:
    case FilterGetByStatus:
    case FilterPutByStatus:
    case FilterInByStatus:
    case FilterDeleteByStatus:
    case FilterCheckPrivateBrandStatus:
    case FilterSetPrivateBrandStatus:
    case ClearCatchLocals:
        break;

    case CheckTypeInfoFlags: {
        const AbstractValue& abstractValue = forNode(node->child1());
        unsigned bits = node->typeInfoOperand();
        ASSERT(bits);

        if (JSValue value = abstractValue.value()) {
            if (value.isCell()) {
                // This works because if we see a cell here, we know it's fully constructed
                // and we can read its inline type info flags. These flags don't change over the
                // object's lifetime.
                if ((value.asCell()->inlineTypeFlags() & bits) == bits) {
                    m_state.setShouldTryConstantFolding(true);
                    break;
                }
            }
        }

        if (abstractValue.m_structure.isFinite()) {
            bool ok = true;
            abstractValue.m_structure.forEach([&] (RegisteredStructure structure) {
                ok &= (structure->typeInfo().inlineTypeFlags() & bits) == bits;
            });
            if (ok) {
                m_state.setShouldTryConstantFolding(true);
                break;
            }
        }

        break;
    }

    case HasStructureWithFlags: {
        const AbstractValue& child = forNode(node->child1());
        unsigned flags = node->structureFlags();
        ASSERT(flags);

        if (Structure::bitFieldFlagsCantBeChangedWithoutTransition(flags) && child.m_type && !(child.m_type & ~SpecCell) && child.m_structure.isFinite())
            m_state.setShouldTryConstantFolding(true);

        setNonCellTypeForNode(node, SpecBoolean);
        break;
    }

    case ParseInt: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_type && !(value.m_type & ~SpecInt32Only)) {
            JSValue radix;
            if (!node->child2())
                radix = jsNumber(0);
            else
                radix = forNode(node->child2()).m_value;

            if (radix.isNumber()
                && (radix.asNumber() == 0 || radix.asNumber() == 10)) {
                m_state.setShouldTryConstantFolding(true);
                if (node->child1().useKind() == UntypedUse)
                    didFoldClobberWorld();
                setNonCellTypeForNode(node, SpecInt32Only);
                break;
            }
        }

        if (node->child1().useKind() == UntypedUse)
            clobberWorld();
        setNonCellTypeForNode(node, SpecBytecodeNumber);
        break;
    }

    case ToIntegerOrInfinity: {
        AbstractValue& child = forNode(node->child1());
        if (JSValue value = child.value(); value && value.isNumber()) {
            if (node->child1().useKind() == UntypedUse)
                didFoldClobberWorld();
            double d = value.asNumber();
            setConstant(node, jsNumber(trunc(std::isnan(d) ? 0.0 : d + 0.0)));
            break;
        }
        if (node->child1().useKind() == UntypedUse)
            clobberWorld();
        setNonCellTypeForNode(node, SpecBytecodeNumber);
        break;
    }

    case ToLength: {
        AbstractValue& child = forNode(node->child1());
        if (JSValue value = child.value(); value && value.isNumber()) {
            if (node->child1().useKind() == UntypedUse)
                didFoldClobberWorld();
            double d = value.asNumber();
            d = trunc(std::isnan(d) ? 0.0 : d + 0.0);
            if (d <= 0)
                d = 0.0;
            else
                d = std::min(d, maxSafeInteger());
            setConstant(node, jsNumber(d));
            break;
        }
        if (node->child1().useKind() == UntypedUse)
            clobberWorld();
        if (node->child1().useKind() == Int32Use)
            setNonCellTypeForNode(node, SpecInt32Only);
        else
            setNonCellTypeForNode(node, SpecBytecodeNumber);
        break;
    }

    case CreateRest:
        if (!m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
            // This means we're already having a bad time.
            clobberWorld();
            setTypeForNode(node, SpecArray);
            break;
        }
        setForNode(node,
            m_graph.globalObjectFor(node->origin.semantic)->restParameterStructure());
        break;
            
    case CheckVarargs:
    case Check: {
        // Simplify out checks that don't actually do checking.
        m_graph.doToChildren(node, [&] (Edge edge) {
            if (!edge)
                return;
            if (edge.isProved() || edge.willNotHaveCheck())
                m_state.setShouldTryConstantFolding(true);
        });
        break;
    }

    case SetFunctionName: {
        clobberWorld();
        break;
    }

    case StoreBarrier:
    case FencedStoreBarrier: {
        filter(node->child1(), SpecCell);
        break;
    }

    case DataViewGetInt: {
        DataViewData data = node->dataViewData();
        if (data.byteSize < 4)
            setNonCellTypeForNode(node, SpecInt32Only);
        else {
            ASSERT(data.byteSize == 4);
            if (data.isSigned)
                setNonCellTypeForNode(node, SpecInt32Only);
            else
                setNonCellTypeForNode(node, SpecInt52Any);
        }
        break;
    }

    case DataViewGetFloat: {
        setNonCellTypeForNode(node, SpecFullDouble);
        break;
    }

    case DateGetInt32OrNaN: {
        setNonCellTypeForNode(node, SpecInt32Only | SpecDoublePureNaN);
        break;
    }

    case DateGetTime: {
        setNonCellTypeForNode(node, SpecFullDouble);
        break;
    }

    case DateSetTime: {
        setNonCellTypeForNode(node, SpecFullDouble);
        break;
    }

    case DataViewSet: {
        break;
    }
        
    case Unreachable:
        // It may be that during a previous run of AI we proved that something was unreachable, but
        // during this run of AI we forget that it's unreachable. AI's proofs don't have to get
        // monotonically stronger over time. So, we don't assert that AI doesn't reach the
        // Unreachable. We have no choice but to take our past proof at face value. Otherwise we'll
        // crash whenever AI fails to be as powerful on run K as it was on run K-1.
        m_state.setIsValid(false);
        break;
        
    case LastNodeType:
    case ArithIMul:
    case FiatInt52:
        DFG_CRASH(m_graph, node, "Unexpected node type");
        break;
    }
    
    return m_state.isValid();
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::filterICStatus(Node* node)
{
    switch (node->op()) {
    case FilterCallLinkStatus:
        if (JSValue value = forNode(node->child1()).m_value)
            node->callLinkStatus()->filter(value);
        break;
        
    case FilterGetByStatus: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isFinite())
            node->getByStatus()->filter(value.m_structure.toStructureSet());
        break;
    }
        
    case FilterInByStatus: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isFinite())
            node->inByStatus()->filter(value.m_structure.toStructureSet());
        break;
    }
        
    case FilterPutByStatus: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isFinite())
            node->putByStatus()->filter(value.m_structure.toStructureSet());
        break;
    }

    case FilterDeleteByStatus: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isFinite())
            node->deleteByStatus()->filter(value.m_structure.toStructureSet());
        break;
    }

    case FilterCheckPrivateBrandStatus: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isFinite())
            node->checkPrivateBrandStatus()->filter(value.m_structure.toStructureSet());
        break;
    }

    case FilterSetPrivateBrandStatus: {
        AbstractValue& value = forNode(node->child1());
        if (value.m_structure.isFinite())
            node->setPrivateBrandStatus()->filter(value.m_structure.toStructureSet());
        break;
    }


    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
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

    startExecuting();
    executeEdges(node);
    return executeEffects(indexInBlock, node);
}

template<typename AbstractStateType>
bool AbstractInterpreter<AbstractStateType>::execute(Node* node)
{
    startExecuting();
    executeEdges(node);
    return executeEffects(UINT_MAX, node);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::clobberWorld()
{
    clobberStructures();
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::didFoldClobberWorld()
{
    didFoldClobberStructures();
}

template<typename AbstractStateType>
template<typename Functor>
void AbstractInterpreter<AbstractStateType>::forAllValues(
    unsigned clobberLimit, Functor& functor)
{
    if (clobberLimit >= m_state.block()->size())
        clobberLimit = m_state.block()->size();
    else
        clobberLimit++;
    ASSERT(clobberLimit <= m_state.block()->size());
    for (size_t i = clobberLimit; i--;) {
        NodeFlowProjection::forEach(
            m_state.block()->at(i),
            [&] (NodeFlowProjection nodeProjection) {
                functor(forNode(nodeProjection));
            });
    }
    if (m_graph.m_form == SSA) {
        for (NodeFlowProjection node : m_state.block()->ssa->liveAtHead) {
            if (node.isStillValid())
                functor(forNode(node));
        }
    }
    for (size_t i = m_state.size(); i--;)
        functor(m_state.atIndex(i));
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::clobberStructures()
{
    m_state.clobberStructures();
    m_state.mergeClobberState(AbstractInterpreterClobberState::ClobberedStructures);
    m_state.setStructureClobberState(StructuresAreClobbered);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::didFoldClobberStructures()
{
    m_state.mergeClobberState(AbstractInterpreterClobberState::FoldedClobber);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::observeTransition(
    unsigned clobberLimit, RegisteredStructure from, RegisteredStructure to)
{
    // Stop performing precise structure transition tracking.
    // Precise structure transition tracking shows quadratic complexity for # of nodes in a basic block.
    // If it is too large, we conservatively clobber all the structures.
    if (m_state.block()->size() > Options::maxDFGNodesInBasicBlockForPreciseAnalysis()) {
        clobberStructures();
        return;
    }

    AbstractValue::TransitionObserver transitionObserver(from, to);
    forAllValues(clobberLimit, transitionObserver);
    
    ASSERT(!from->dfgShouldWatch()); // We don't need to claim to be in a clobbered state because 'from' was never watchable (during the time we were compiling), hence no constants ever introduced into the DFG IR that ever had a watchable structure would ever have the same structure as from.
    
    m_state.mergeClobberState(AbstractInterpreterClobberState::ObservedTransitions);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::observeTransitions(
    unsigned clobberLimit, const TransitionVector& vector)
{
    if (vector.isEmpty())
        return;
    
    // Stop performing precise structure transition tracking.
    // Precise structure transition tracking shows quadratic complexity for # of nodes in a basic block.
    // If it is too large, we conservatively clobber all the structures.
    if (m_state.block()->size() > Options::maxDFGNodesInBasicBlockForPreciseAnalysis()) {
        clobberStructures();
        return;
    }

    AbstractValue::TransitionsObserver transitionsObserver(vector);
    forAllValues(clobberLimit, transitionsObserver);
    
    if (ASSERT_ENABLED) {
        // We don't need to claim to be in a clobbered state because none of the Transition::previous structures are watchable.
        for (unsigned i = vector.size(); i--;)
            ASSERT(!vector[i].previous->dfgShouldWatch());
    }

    m_state.mergeClobberState(AbstractInterpreterClobberState::ObservedTransitions);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::dump(PrintStream& out) const
{
    const_cast<AbstractInterpreter<AbstractStateType>*>(this)->dump(out);
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::dump(PrintStream& out)
{
    CommaPrinter comma(" "_s);
    HashSet<NodeFlowProjection> seen;
    if (m_graph.m_form == SSA) {
        for (NodeFlowProjection node : m_state.block()->ssa->liveAtHead) {
            seen.add(node);
            AbstractValue& value = forNode(node);
            if (value.isClear())
                continue;
            out.print(comma, node, ":"_s, value);
        }
    }
    for (size_t i = 0; i < m_state.block()->size(); ++i) {
        NodeFlowProjection::forEach(
            m_state.block()->at(i), [&] (NodeFlowProjection nodeProjection) {
                seen.add(nodeProjection);
                AbstractValue& value = forNode(nodeProjection);
                if (value.isClear())
                    return;
                out.print(comma, nodeProjection, ":"_s, value);
            });
    }
    if (m_graph.m_form == SSA) {
        for (NodeFlowProjection node : m_state.block()->ssa->liveAtTail) {
            if (seen.contains(node))
                continue;
            AbstractValue& value = forNode(node);
            if (value.isClear())
                continue;
            out.print(comma, node, ":"_s, value);
        }
    }
}

template<typename AbstractStateType>
FiltrationResult AbstractInterpreter<AbstractStateType>::filter(
    AbstractValue& value, const RegisteredStructureSet& set, SpeculatedType admittedTypes)
{
    if (value.filter(m_graph, set, admittedTypes) == FiltrationOK)
        return FiltrationOK;
    m_state.setIsValid(false);
    return Contradiction;
}

template<typename AbstractStateType>
FiltrationResult AbstractInterpreter<AbstractStateType>::filterArrayModes(
    AbstractValue& value, ArrayModes arrayModes, SpeculatedType admittedTypes)
{
    if (value.filterArrayModes(arrayModes, admittedTypes) == FiltrationOK)
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

template<typename AbstractStateType>
FiltrationResult AbstractInterpreter<AbstractStateType>::filterClassInfo(
    AbstractValue& value, const ClassInfo* classInfo)
{
    if (value.filterClassInfo(m_graph, classInfo) == FiltrationOK)
        return FiltrationOK;
    m_state.setIsValid(false);
    return Contradiction;
}

template<typename AbstractStateType>
void AbstractInterpreter<AbstractStateType>::executeDoubleUnaryOpEffects(Node* node, const auto& equivalentFunction)
{
    JSValue child = forNode(node->child1()).value();
    if (std::optional<double> number = child.toNumberFromPrimitive()) {
        if (node->child1().useKind() != DoubleRepUse)
            didFoldClobberWorld();
        setConstant(node, jsDoubleNumber(equivalentFunction(*number)));
        return;
    }
    SpeculatedType type;
    if (node->child1().useKind() == DoubleRepUse)
        type = typeOfDoubleUnaryOp(forNode(node->child1()).m_type);
    else {
        clobberWorld();
        type = SpecBytecodeNumber;
    }
    setNonCellTypeForNode(node, type);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
