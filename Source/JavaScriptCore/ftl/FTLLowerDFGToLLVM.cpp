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
#include "FTLLowerDFGToLLVM.h"

#if ENABLE(FTL_JIT)

#include "CodeBlockWithJITType.h"
#include "DFGAbstractState.h"
#include "FTLAbstractHeapRepository.h"
#include "FTLExitThunkGenerator.h"
#include "FTLFormattedValue.h"
#include "FTLOutput.h"
#include "FTLThunks.h"
#include "FTLValueSource.h"
#include "LinkBuffer.h"
#include "Operations.h"

namespace JSC { namespace FTL {

using namespace DFG;

// Using this instead of typeCheck() helps to reduce the load on LLVM, by creating
// significantly less dead code.
#define FTL_TYPE_CHECK(lowValue, highValue, typesPassedThrough, failCondition) do { \
        FormattedValue _ftc_lowValue = (lowValue);                      \
        Edge _ftc_highValue = (highValue);                              \
        SpeculatedType _ftc_typesPassedThrough = (typesPassedThrough);  \
        if (!m_state.needsTypeCheck(_ftc_highValue, _ftc_typesPassedThrough)) \
            break;                                                      \
        typeCheck(_ftc_lowValue, _ftc_highValue, _ftc_typesPassedThrough, (failCondition)); \
    } while (false)

class LowerDFGToLLVM {
public:
    LowerDFGToLLVM(State& state)
        : m_graph(state.graph)
        , m_ftlState(state)
        , m_heaps(state.context)
        , m_out(state.context)
        , m_localsBoolean(OperandsLike, state.graph.m_blocks[0]->variablesAtHead)
        , m_locals32(OperandsLike, state.graph.m_blocks[0]->variablesAtHead)
        , m_locals64(OperandsLike, state.graph.m_blocks[0]->variablesAtHead)
        , m_localsDouble(OperandsLike, state.graph.m_blocks[0]->variablesAtHead)
        , m_valueSources(OperandsLike, state.graph.m_blocks[0]->variablesAtHead)
        , m_lastSetOperand(std::numeric_limits<int>::max())
        , m_exitThunkGenerator(state)
        , m_state(state.graph)
    {
    }
    
    void lower()
    {
        CString name = toCString(codeBlock()->hash());
        m_ftlState.module =
            LLVMModuleCreateWithNameInContext(name.data(), m_ftlState.context);
        
        m_ftlState.function = addFunction(
            m_ftlState.module, name.data(), functionType(m_out.int64, m_out.intPtr));
        setFunctionCallingConv(m_ftlState.function, LLVMCCallConv);
        
        m_out.initialize(m_ftlState.module, m_ftlState.function, m_heaps);
        
        m_prologue = appendBasicBlock(m_ftlState.context, m_ftlState.function);
        m_out.appendTo(m_prologue);
        for (unsigned index = m_localsBoolean.size(); index--;) {
            m_localsBoolean[index] = buildAlloca(m_out.m_builder, m_out.boolean);
            m_locals32[index] = buildAlloca(m_out.m_builder, m_out.int32);
            m_locals64[index] = buildAlloca(m_out.m_builder, m_out.int64);
            m_localsDouble[index] = buildAlloca(m_out.m_builder, m_out.doubleType);
        }
        
        m_initialization = appendBasicBlock(m_ftlState.context, m_ftlState.function);
        m_argumentChecks = appendBasicBlock(m_ftlState.context, m_ftlState.function);

        m_callFrame = m_out.param(0);
        m_tagTypeNumber = m_out.constInt64(TagTypeNumber);
        m_tagMask = m_out.constInt64(TagMask);
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex) {
            m_highBlock = m_graph.m_blocks[blockIndex].get();
            if (!m_highBlock)
                continue;
            m_blocks.add(m_highBlock, FTL_NEW_BLOCK(m_out, ("Block #", blockIndex)));
            addFlushedLocalOpRoots();
        }
        
        closeOverFlushedLocalOps();
        
        m_out.appendTo(m_argumentChecks, m_blocks.get(m_graph.m_blocks[0].get()));

        transferAndCheckArguments();
        
        m_out.jump(m_blocks.get(m_graph.m_blocks[0].get()));
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.m_blocks.size(); ++blockIndex)
            compileBlock(blockIndex);
        
        // And now complete the initialization block.
        linkOSRExitsAndCompleteInitializationBlocks();

        if (verboseCompilationEnabled())
            m_ftlState.dumpState("after lowering");
        if (validationEnabled())
            verifyModule(m_ftlState.module);
    }

private:
    
    void addFlushedLocalOpRoots()
    {
        for (unsigned nodeIndex = m_highBlock->size(); nodeIndex--;) {
            Node* node = m_highBlock->at(nodeIndex);
            if (node->op() != Flush)
                continue;
            addFlushedLocalOp(node);
        }
    }
    
    void closeOverFlushedLocalOps()
    {
        while (!m_flushedLocalOpWorklist.isEmpty()) {
            Node* node = m_flushedLocalOpWorklist.last();
            m_flushedLocalOpWorklist.removeLast();
            
            ASSERT(m_flushedLocalOps.contains(node));
            
            DFG_NODE_DO_TO_CHILDREN(m_graph, node, addFlushedLocalEdge);
        }
    }
    
    void addFlushedLocalOp(Node* node)
    {
        if (m_flushedLocalOps.contains(node))
            return;
        m_flushedLocalOps.add(node);
        m_flushedLocalOpWorklist.append(node);
    }
    void addFlushedLocalEdge(Node*, Edge edge)
    {
        addFlushedLocalOp(edge.node());
    }
    
    void transferAndCheckArguments()
    {
        // While checking arguments, everything is in the stack.
        for (unsigned i = m_valueSources.size(); i--;)
            m_valueSources[i] = ValueSource(ValueInJSStack);
        
        m_codeOrigin = CodeOrigin(0);
        m_node = 0;
        
        for (int i = 0; i < m_graph.m_codeBlock->numParameters(); ++i) {
            Node* node = m_graph.m_arguments[i];
            if (!node->shouldGenerate())
                continue;
            
            VariableAccessData* variable = node->variableAccessData();
            // If it's captured then we'll be accessing it through loads and stores
            // anyway - there's no point in transfering it out.
            if (variable->isCaptured())
                continue;
            
            LValue jsValue = m_out.load64(payloadFor(variable->local()));
            
            if (variable->shouldUnboxIfPossible()) {
                RELEASE_ASSERT(!variable->shouldUseDoubleFormat());
                
                SpeculatedType prediction = variable->argumentAwarePrediction();
                
                if (isInt32Speculation(prediction)) {
                    speculateBackward(BadType, jsValueValue(jsValue), node, isNotInt32(jsValue));
                    m_out.set(unboxInt32(jsValue), m_locals32.operand(variable->local()));
                    continue;
                }
                
                if (isCellSpeculation(prediction)) {
                    speculateBackward(BadType, jsValueValue(jsValue), node, isNotCell(jsValue));
                    m_out.set(jsValue, m_locals64.operand(variable->local()));
                    continue;
                }
                if (isBooleanSpeculation(prediction)) {
                    speculateBackward(BadType, jsValueValue(jsValue), node, isNotBoolean(jsValue));
                    m_out.set(unboxBoolean(jsValue), m_localsBoolean.operand(variable->local()));
                    continue;
                }
            }
            
            m_out.set(jsValue, m_locals64.operand(variable->local()));
        }
    }
    
    void compileBlock(BlockIndex blockIndex)
    {
        m_highBlock = m_graph.m_blocks[blockIndex].get();
        if (!m_highBlock)
            return;
        
        LBasicBlock lowBlock = m_blocks.get(m_highBlock);
        
        m_nextHighBlock = 0;
        for (BlockIndex nextBlockIndex = blockIndex + 1; nextBlockIndex < m_graph.m_blocks.size(); ++nextBlockIndex) {
            m_nextHighBlock = m_graph.m_blocks[nextBlockIndex].get();
            if (m_nextHighBlock)
                break;
        }
        m_nextLowBlock = m_nextHighBlock ? m_blocks.get(m_nextHighBlock) : 0;
        
        // All of this effort to find the next block gives us the ability to keep the
        // generated IR in roughly program order. This ought not affect the performance
        // of the generated code (since we expect LLVM to reorder things) but it will
        // make IR dumps easier to read.
        m_out.appendTo(lowBlock, m_nextLowBlock);
        
        initializeOSRExitStateForBlock();
        
        m_int32Values.clear();
        m_jsValueValues.clear();
        m_booleanValues.clear();
        m_storageValues.clear();
        m_timeToLive.clear();
        
        m_state.reset();
        m_state.beginBasicBlock(m_highBlock);
        
        for (m_nodeIndex = 0; m_nodeIndex < m_highBlock->size(); ++m_nodeIndex) {
            if (!compileNode(m_nodeIndex))
                break;
        }
    }
    
    bool compileNode(unsigned nodeIndex)
    {
        if (!m_state.isValid()) {
            m_out.unreachable();
            return false;
        }
        
        m_node = m_highBlock->at(nodeIndex);
        m_codeOrigin = m_node->codeOrigin;
        
        if (verboseCompilationEnabled())
            dataLog("Lowering ", m_node, "\n");
        
        bool shouldExecuteEffects = m_state.startExecuting(m_node);
        
        m_direction = (m_node->flags() & NodeExitsForward) ? ForwardSpeculation : BackwardSpeculation;
        
        switch (m_node->op()) {
        case JSConstant:
            compileJSConstant();
            break;
        case WeakJSConstant:
            compileWeakJSConstant();
            break;
        case GetLocal:
            compileGetLocal();
            break;
        case SetLocal:
            compileSetLocal();
            break;
        case MovHint:
            compileMovHint();
            break;
        case ZombieHint:
            compileZombieHint();
            break;
        case MovHintAndCheck:
            compileMovHintAndCheck();
            break;
        case Phantom:
            compilePhantom();
            break;
        case Flush:
        case PhantomLocal:
        case SetArgument:
            break;
        case ArithAdd:
        case ValueAdd:
            compileAdd();
            break;
        case ArithSub:
            compileArithSub();
            break;
        case ArithMul:
            compileArithMul();
            break;
        case ArithDiv:
            compileArithDiv();
            break;
        case ArithNegate:
            compileArithNegate();
            break;
        case BitAnd:
            compileBitAnd();
            break;
        case BitOr:
            compileBitOr();
            break;
        case BitXor:
            compileBitXor();
            break;
        case BitRShift:
            compileBitRShift();
            break;
        case BitLShift:
            compileBitLShift();
            break;
        case BitURShift:
            compileBitURShift();
            break;
        case UInt32ToNumber:
            compileUInt32ToNumber();
            break;
        case Int32ToDouble:
            compileInt32ToDouble();
            break;
        case CheckStructure:
            compileCheckStructure();
            break;
        case StructureTransitionWatchpoint:
            compileStructureTransitionWatchpoint();
            break;
        case PutStructure:
            compilePutStructure();
            break;
        case PhantomPutStructure:
            compilePhantomPutStructure();
            break;
        case GetButterfly:
            compileGetButterfly();
            break;
        case GetArrayLength:
            compileGetArrayLength();
            break;
        case GetByVal:
            compileGetByVal();
            break;
        case GetByOffset:
            compileGetByOffset();
            break;
        case PutByOffset:
            compilePutByOffset();
            break;
        case GetGlobalVar:
            compileGetGlobalVar();
            break;
        case PutGlobalVar:
            compilePutGlobalVar();
            break;
        case CompareEq:
            compileCompareEq();
            break;
        case CompareEqConstant:
            compileCompareEqConstant();
            break;
        case CompareStrictEq:
            compileCompareStrictEq();
            break;
        case CompareStrictEqConstant:
            compileCompareStrictEqConstant();
            break;
        case CompareLess:
            compileCompareLess();
            break;
        case CompareLessEq:
            compileCompareLessEq();
            break;
        case CompareGreater:
            compileCompareGreater();
            break;
        case CompareGreaterEq:
            compileCompareGreaterEq();
            break;
        case LogicalNot:
            compileLogicalNot();
            break;
        case Jump:
            compileJump();
            break;
        case Branch:
            compileBranch();
            break;
        case Return:
            compileReturn();
            break;
        case ForceOSRExit:
            compileForceOSRExit();
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        if (m_node->shouldGenerate())
            DFG_NODE_DO_TO_CHILDREN(m_graph, m_node, use);
        
        if (m_node->hasResult())
            m_timeToLive.add(m_node, m_node->adjustedRefCount());
        
        if (shouldExecuteEffects)
            m_state.executeEffects(nodeIndex);
        
        return true;
    }
    
    void compileJSConstant()
    {
        JSValue value = m_graph.valueOfJSConstant(m_node);
        if (value.isDouble())
            m_doubleValues.add(m_node, m_out.constDouble(value.asDouble()));
        else
            m_jsValueValues.add(m_node, m_out.constInt64(JSValue::encode(value)));
    }
    
    void compileWeakJSConstant()
    {
        m_jsValueValues.add(m_node, weakPointer(m_node->weakConstant()));
    }
    
    void compileGetLocal()
    {
        // GetLocal is one of the few nodes that may be left behind, with !shouldGenerate().
        if (!m_node->shouldGenerate())
            return;
        
        VariableAccessData* variable = m_node->variableAccessData();
        SpeculatedType prediction = variable->argumentAwarePrediction();
        AbstractValue& value = m_state.variables().operand(variable->local());
        
        if (prediction == SpecNone) {
            terminate(InadequateCoverage);
            return;
        }
        
        if (value.isClear()) {
            // FIXME: We should trap instead.
            // https://bugs.webkit.org/show_bug.cgi?id=110383
            terminate(InadequateCoverage);
            return;
        }
        
        if (variable->isCaptured()) {
            if (isInt32Speculation(value.m_value))
                m_int32Values.add(m_node, m_out.load32(payloadFor(variable->local())));
            else
                m_jsValueValues.add(m_node, m_out.load64(addressFor(variable->local())));
            return;
        }
        
        if (variable->shouldUnboxIfPossible()) {
            if (variable->shouldUseDoubleFormat()) {
                m_doubleValues.add(m_node, m_out.get(m_localsDouble.operand(variable->local())));
                return;
            }
            
            // Locals that are marked shouldUnboxIfPossible() that aren't also forced to
            // use double format, and that have the right prediction, will always be
            // speculated on SetLocal and will always be stored into an appropriately
            // typed reference LValue.
            if (isInt32Speculation(prediction)) {
                m_int32Values.add(m_node, m_out.get(m_locals32.operand(variable->local())));
                return;
            }
            if (isBooleanSpeculation(prediction)) {
                m_booleanValues.add(m_node, m_out.get(m_localsBoolean.operand(variable->local())));
                return;
            }
            // We skip the Cell case here because that gets treated identically to
            // JSValues, since cells are stored untagged.
        }
        
        m_jsValueValues.add(m_node, m_out.get(m_locals64.operand(variable->local())));
    }
    
    void compileSetLocal()
    {
        observeMovHint(m_node);
        
        VariableAccessData* variable = m_node->variableAccessData();
        SpeculatedType prediction = variable->argumentAwarePrediction();
        bool needsFlushing = m_flushedLocalOps.contains(m_node);
        
        if (variable->shouldUnboxIfPossible()) {
            if (variable->shouldUseDoubleFormat()) {
                LValue value = lowDouble(m_node->child1());
                m_out.set(value, m_localsDouble.operand(variable->local()));
                if (needsFlushing) {
                    m_out.storeDouble(value, addressFor(variable->local()));
                    m_valueSources.operand(variable->local()) = ValueSource(DoubleInJSStack);
                }
                return;
            }
            
            if (isInt32Speculation(prediction)) {
                LValue value = lowInt32(m_node->child1());
                m_out.set(value, m_locals32.operand(variable->local()));
                if (needsFlushing) {
                    m_out.store32(value, payloadFor(variable->local()));
                    m_valueSources.operand(variable->local()) = ValueSource(Int32InJSStack);
                }
                return;
            }
            if (isCellSpeculation(prediction)) {
                LValue value = lowCell(m_node->child1());
                m_out.set(value, m_locals64.operand(variable->local()));
                if (needsFlushing) {
                    m_out.store64(value, addressFor(variable->local()));
                    m_valueSources.operand(variable->local()) = ValueSource(ValueInJSStack);
                }
                return;
            }
            if (isBooleanSpeculation(prediction)) {
                m_out.set(lowBoolean(m_node->child1()), m_localsBoolean.operand(variable->local()));
                if (needsFlushing) {
                    m_out.store64(lowJSValue(m_node->child1()), addressFor(variable->local()));
                    m_valueSources.operand(variable->local()) = ValueSource(ValueInJSStack);
                }
                return;
            }
        }
        
        LValue value = lowJSValue(m_node->child1());
        if (variable->isCaptured()) {
            m_out.store64(value, addressFor(variable->local()));
            m_valueSources.operand(variable->local()) = ValueSource(ValueInJSStack);
            return;
        }
        
        m_out.set(value, m_locals64.operand(variable->local()));
        if (needsFlushing) {
            m_out.store64(value, addressFor(variable->local()));
            m_valueSources.operand(variable->local()) = ValueSource(ValueInJSStack);
        }
    }
    
    void compileMovHint()
    {
        observeMovHint(m_node);
    }
    
    void compileZombieHint()
    {
        VariableAccessData* data = m_node->variableAccessData();
        m_lastSetOperand = data->local();
        m_valueSources.operand(data->local()) = ValueSource(SourceIsDead);
    }
    
    void compileMovHintAndCheck()
    {
        observeMovHint(m_node);
        speculate(m_node->child1());
    }
    
    void compilePhantom()
    {
        DFG_NODE_DO_TO_CHILDREN(m_graph, m_node, speculate);
    }
    
    void compileAdd()
    {
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue left = lowInt32(m_node->child1());
            LValue right = lowInt32(m_node->child2());
            
            if (nodeCanTruncateInteger(m_node->arithNodeFlags())) {
                m_int32Values.add(m_node, m_out.add(left, right));
                break;
            }
            
            LValue result = m_out.addWithOverflow32(left, right);
            speculate(Overflow, noValue(), 0, m_out.extractValue(result, 1));
            m_int32Values.add(m_node, m_out.extractValue(result, 0));
            break;
        }
            
        case NumberUse: {
            m_doubleValues.add(
                m_node,
                m_out.doubleAdd(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileArithSub()
    {
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue left = lowInt32(m_node->child1());
            LValue right = lowInt32(m_node->child2());
            
            if (nodeCanTruncateInteger(m_node->arithNodeFlags())) {
                m_int32Values.add(m_node, m_out.sub(left, right));
                break;
            }
            
            LValue result = m_out.subWithOverflow32(left, right);
            speculate(Overflow, noValue(), 0, m_out.extractValue(result, 1));
            m_int32Values.add(m_node, m_out.extractValue(result, 0));
            break;
        }
            
        case NumberUse: {
            m_doubleValues.add(
                m_node,
                m_out.doubleSub(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileArithMul()
    {
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue left = lowInt32(m_node->child1());
            LValue right = lowInt32(m_node->child2());
            
            LValue result;
            if (nodeCanTruncateInteger(m_node->arithNodeFlags()))
                result = m_out.mul(left, right);
            else {
                LValue overflowResult = m_out.mulWithOverflow32(left, right);
                speculate(Overflow, noValue(), 0, m_out.extractValue(overflowResult, 1));
                result = m_out.extractValue(overflowResult, 0);
            }
            
            if (!nodeCanIgnoreNegativeZero(m_node->arithNodeFlags())) {
                LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("ArithMul slow case"));
                LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArithMul continuation"));
                
                m_out.branch(m_out.notZero32(result), continuation, slowCase);
                
                LBasicBlock lastNext = m_out.appendTo(slowCase, continuation);
                speculate(NegativeZero, noValue(), 0, m_out.lessThan(left, m_out.int32Zero));
                speculate(NegativeZero, noValue(), 0, m_out.lessThan(right, m_out.int32Zero));
                m_out.jump(continuation);
                m_out.appendTo(continuation, lastNext);
            }
            
            m_int32Values.add(m_node, result);
            break;
        }
            
        case NumberUse: {
            m_doubleValues.add(
                m_node,
                m_out.doubleMul(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileArithDiv()
    {
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue numerator = lowInt32(m_node->child1());
            LValue denominator = lowInt32(m_node->child2());
            
            LBasicBlock unsafeDenominator = FTL_NEW_BLOCK(m_out, ("ArithDiv unsafe denominator"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArithDiv continuation"));
            LBasicBlock done = FTL_NEW_BLOCK(m_out, ("ArithDiv done"));
            
            Vector<ValueFromBlock, 3> results;
            
            LValue adjustedDenominator = m_out.add(denominator, m_out.int32One);
            
            m_out.branch(m_out.above(adjustedDenominator, m_out.int32One), continuation, unsafeDenominator);
            
            LBasicBlock lastNext = m_out.appendTo(unsafeDenominator, continuation);
            
            LValue neg2ToThe31 = m_out.constInt32(-2147483647-1);
            
            if (nodeUsedAsNumber(m_node->arithNodeFlags())) {
                speculate(Overflow, noValue(), 0, m_out.isZero32(denominator));
                speculate(Overflow, noValue(), 0, m_out.equal(numerator, neg2ToThe31));
                m_out.jump(continuation);
            } else {
                // This is the case where we convert the result to an int after we're done. So,
                // if the denominator is zero, then the result should be result should be zero.
                // If the denominator is not zero (i.e. it's -1 because we're guarded by the
                // check above) and the numerator is -2^31 then the result should be -2^31.
                
                LBasicBlock divByZero = FTL_NEW_BLOCK(m_out, ("ArithDiv divide by zero"));
                LBasicBlock notDivByZero = FTL_NEW_BLOCK(m_out, ("ArithDiv not divide by zero"));
                LBasicBlock neg2ToThe31ByNeg1 = FTL_NEW_BLOCK(m_out, ("ArithDiv -2^31/-1"));
                
                m_out.branch(m_out.isZero32(denominator), divByZero, notDivByZero);
                
                m_out.appendTo(divByZero, notDivByZero);
                results.append(m_out.anchor(m_out.int32Zero));
                m_out.jump(done);
                
                m_out.appendTo(notDivByZero, neg2ToThe31ByNeg1);
                m_out.branch(m_out.equal(numerator, neg2ToThe31), neg2ToThe31ByNeg1, continuation);
                
                m_out.appendTo(neg2ToThe31ByNeg1, continuation);
                results.append(m_out.anchor(neg2ToThe31));
                m_out.jump(done);
            }
            
            m_out.appendTo(continuation, done);
            
            if (!nodeCanIgnoreNegativeZero(m_node->arithNodeFlags())) {
                LBasicBlock zeroNumerator = FTL_NEW_BLOCK(m_out, ("ArithDiv zero numerator"));
                LBasicBlock numeratorContinuation = FTL_NEW_BLOCK(m_out, ("ArithDiv numerator continuation"));
                
                m_out.branch(m_out.isZero32(numerator), zeroNumerator, numeratorContinuation);
                
                LBasicBlock innerLastNext = m_out.appendTo(zeroNumerator, numeratorContinuation);
                
                speculate(
                    NegativeZero, noValue(), 0, m_out.lessThan(denominator, m_out.int32Zero));
                
                m_out.jump(numeratorContinuation);
                
                m_out.appendTo(numeratorContinuation, innerLastNext);
            }
            
            LValue divisionResult = m_out.div(numerator, denominator);
            
            if (nodeUsedAsNumber(m_node->arithNodeFlags())) {
                speculate(
                    Overflow, noValue(), 0,
                    m_out.notEqual(m_out.mul(divisionResult, denominator), numerator));
            }
            
            results.append(m_out.anchor(divisionResult));
            m_out.jump(done);
            
            m_out.appendTo(done, lastNext);
            
            m_int32Values.add(m_node, m_out.phi(m_out.int32, results));
            break;
        }
            
        case NumberUse: {
            m_doubleValues.add(
                m_node,
                m_out.doubleDiv(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileArithNegate()
    {
        switch (m_node->child1().useKind()) {
        case Int32Use: {
            LValue value = lowInt32(m_node->child1());
            
            LValue result;
            if (nodeCanTruncateInteger(m_node->arithNodeFlags()))
                result = m_out.neg(value);
            else {
                // We don't have a negate-with-overflow intrinsic. Hopefully this
                // does the trick, though.
                LValue overflowResult = m_out.subWithOverflow32(m_out.int32Zero, value);
                speculate(Overflow, noValue(), 0, m_out.extractValue(overflowResult, 1));
                result = m_out.extractValue(overflowResult, 0);
            }
            
            m_int32Values.add(m_node, result);
            break;
        }
            
        case NumberUse: {
            m_doubleValues.add(m_node, m_out.doubleNeg(lowDouble(m_node->child1())));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileBitAnd()
    {
        m_int32Values.add(
            m_node, m_out.bitAnd(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitOr()
    {
        m_int32Values.add(
            m_node, m_out.bitOr(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitXor()
    {
        m_int32Values.add(
            m_node, m_out.bitXor(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitRShift()
    {
        m_int32Values.add(
            m_node,
            m_out.aShr(
                lowInt32(m_node->child1()),
                m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileBitLShift()
    {
        m_int32Values.add(
            m_node,
            m_out.shl(
                lowInt32(m_node->child1()),
                m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileBitURShift()
    {
        m_int32Values.add(
            m_node,
            m_out.lShr(
                lowInt32(m_node->child1()),
                m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileUInt32ToNumber()
    {
        LValue value = lowInt32(m_node->child1());

        if (!nodeCanSpeculateInteger(m_node->arithNodeFlags())) {
            m_doubleValues.add(m_node, m_out.unsignedToDouble(value));
            return;
        }
        
        speculateForward(
            Overflow, noValue(), 0, m_out.lessThan(value, m_out.int32Zero),
            FormattedValue(ValueFormatUInt32, value));
        m_int32Values.add(m_node, value);
    }
    
    void compileInt32ToDouble()
    {
        // This node is tricky to compile in the DFG backend because it tries to
        // avoid converting child1 to a double in-place, as that would make subsequent
        // int uses of of child1 fail. But the FTL needs no such special magic, since
        // unlike the DFG backend, the FTL allows each node to have multiple
        // contemporaneous low-level representations. So, this gives child1 a double
        // representation and then forwards that representation to m_node.
        
        m_doubleValues.add(m_node, lowDouble(m_node->child1()));
    }
    
    void compileCheckStructure()
    {
        LValue cell = lowCell(m_node->child1());
        
        ExitKind exitKind;
        if (m_node->child1()->op() == WeakJSConstant)
            exitKind = BadWeakConstantCache;
        else
            exitKind = BadCache;
        
        LValue structure = m_out.loadPtr(cell, m_heaps.JSCell_structure);
        
        if (m_node->structureSet().size() == 1) {
            speculate(
                exitKind, jsValueValue(cell), 0,
                m_out.notEqual(structure, weakPointer(m_node->structureSet()[0])));
            return;
        }
        
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("CheckStructure continuation"));
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(continuation);
        for (unsigned i = 0; i < m_node->structureSet().size() - 1; ++i) {
            LBasicBlock nextStructure = FTL_NEW_BLOCK(m_out, ("CheckStructure nextStructure"));
            m_out.branch(
                m_out.equal(structure, weakPointer(m_node->structureSet()[i])),
                continuation, nextStructure);
            m_out.appendTo(nextStructure);
        }
        
        speculate(
            exitKind, jsValueValue(cell), 0,
            m_out.notEqual(structure, weakPointer(m_node->structureSet().last())));
        
        m_out.jump(continuation);
        m_out.appendTo(continuation, lastNext);
    }
    
    void compileStructureTransitionWatchpoint()
    {
        addWeakReference(m_node->structure());
        
        // FIXME: Implement structure transition watchpoints.
        // https://bugs.webkit.org/show_bug.cgi?id=113647
        
        speculateCell(m_node->child1());
    }
    
    void compilePutStructure()
    {
        m_ftlState.jitCode->common.notifyCompilingStructureTransition(codeBlock(), m_node);
        
        m_out.store64(
            m_out.constIntPtr(m_node->structureTransitionData().newStructure),
            lowCell(m_node->child1()), m_heaps.JSCell_structure);
    }
    
    void compilePhantomPutStructure()
    {
        m_ftlState.jitCode->common.notifyCompilingStructureTransition(codeBlock(), m_node);
    }
    
    void compileGetButterfly()
    {
        m_storageValues.add(
            m_node, m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSObject_butterfly));
    }
    
    void compileGetArrayLength()
    {
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            m_int32Values.add(
                m_node, m_out.load32(lowStorage(m_node->child2()), m_heaps.Butterfly_publicLength));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileGetByVal()
    {
        LValue index = lowInt32(m_node->child2());
        LValue storage = lowStorage(m_node->child3());
        
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Contiguous: {
            if (m_node->arrayMode().isInBounds()) {
                speculate(
                    OutOfBounds, noValue(), 0,
                    m_out.aboveOrEqual(
                        index, m_out.load32(storage, m_heaps.Butterfly_publicLength)));
                
                LValue result = m_out.load64(m_out.baseIndex(
                    m_node->arrayMode().type() == Array::Int32 ?
                        m_heaps.indexedInt32Properties : m_heaps.indexedContiguousProperties,
                    storage, m_out.zeroExt(index, m_out.intPtr),
                    m_state.forNode(m_node->child2()).m_value));
                speculate(LoadFromHole, noValue(), 0, m_out.isZero64(result));
                m_jsValueValues.add(m_node, result);
                break;
            }
            
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
            
        case Array::Double: {
            if (m_node->arrayMode().isInBounds()) {
                if (m_node->arrayMode().isSaneChain()) {
                    // FIXME: Implement structure transition watchpoints.
                    // https://bugs.webkit.org/show_bug.cgi?id=113647
                }
            
                speculate(
                    OutOfBounds, noValue(), 0,
                    m_out.aboveOrEqual(
                        index, m_out.load32(storage, m_heaps.Butterfly_publicLength)));
                
                LValue result = m_out.loadDouble(m_out.baseIndex(
                    m_heaps.indexedDoubleProperties,
                    storage, m_out.zeroExt(index, m_out.intPtr),
                    m_state.forNode(m_node->child2()).m_value));
                
                if (!m_node->arrayMode().isSaneChain()) {
                    speculate(
                        LoadFromHole, noValue(), 0,
                        m_out.doubleNotEqualOrUnordered(result, result));
                }
                m_doubleValues.add(m_node, result);
                break;
            }
            
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileGetByOffset()
    {
        StorageAccessData& data =
            m_graph.m_storageAccessData[m_node->storageAccessDataIndex()];
        
        m_jsValueValues.add(
            m_node,
            m_out.load64(
                m_out.address(
                    m_heaps.properties[data.identifierNumber],
                    lowStorage(m_node->child1()),
                    data.offset * sizeof(EncodedJSValue))));
    }
    
    void compilePutByOffset()
    {
        StorageAccessData& data =
            m_graph.m_storageAccessData[m_node->storageAccessDataIndex()];
        
        m_out.store64(
            lowJSValue(m_node->child3()),
            m_out.address(
                m_heaps.properties[data.identifierNumber],
                lowStorage(m_node->child2()),
                data.offset * sizeof(EncodedJSValue)));
    }
    
    void compileGetGlobalVar()
    {
        m_jsValueValues.add(m_node, m_out.load64(m_out.absolute(m_node->registerPointer())));
    }
    
    void compilePutGlobalVar()
    {
        m_out.store64(
            lowJSValue(m_node->child1()), m_out.absolute(m_node->registerPointer()));
    }
    
    void compileCompareEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)
            || m_node->isBinaryUseKind(NumberUse)
            || m_node->isBinaryUseKind(ObjectUse)) {
            compileCompareStrictEq();
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareEqConstant()
    {
        ASSERT(m_graph.valueOfJSConstant(m_node->child2().node()).isNull());
        masqueradesAsUndefinedWatchpointIfIsStillValid();
        m_booleanValues.add(
            m_node, equalNullOrUndefined(
                m_node->child1(), AllCellsAreFalse, EqualNullOrUndefined));
    }
    
    void compileCompareStrictEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            m_booleanValues.add(
                m_node,
                m_out.equal(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            m_booleanValues.add(
                m_node,
                m_out.doubleEqual(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
        }
        
        if (m_node->isBinaryUseKind(ObjectUse)) {
            masqueradesAsUndefinedWatchpointIfIsStillValid();
            m_booleanValues.add(
                m_node,
                m_out.equal(
                    lowNonNullObject(m_node->child1()),
                    lowNonNullObject(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareStrictEqConstant()
    {
        JSValue constant = m_graph.valueOfJSConstant(m_node->child2().node());

        if (constant.isUndefinedOrNull()
            && !masqueradesAsUndefinedWatchpointIfIsStillValid()) {
            if (constant.isNull()) {
                m_booleanValues.add(
                    m_node, equalNullOrUndefined(m_node->child1(), AllCellsAreFalse, EqualNull));
                return;
            }
        
            ASSERT(constant.isUndefined());
            m_booleanValues.add(
                m_node, equalNullOrUndefined(m_node->child1(), AllCellsAreFalse, EqualUndefined));
            return;
        }
        
        m_booleanValues.add(
            m_node,
            m_out.equal(
                lowJSValue(m_node->child1()),
                m_out.constInt64(JSValue::encode(constant))));
    }
    
    void compileCompareLess()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            m_booleanValues.add(
                m_node,
                m_out.lessThan(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            m_booleanValues.add(
                m_node,
                m_out.doubleLessThan(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareLessEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            m_booleanValues.add(
                m_node,
                m_out.lessThanOrEqual(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            m_booleanValues.add(
                m_node,
                m_out.doubleLessThanOrEqual(
                    lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareGreater()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            m_booleanValues.add(
                m_node,
                m_out.greaterThan(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            m_booleanValues.add(
                m_node,
                m_out.doubleGreaterThan(
                    lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareGreaterEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            m_booleanValues.add(
                m_node,
                m_out.greaterThanOrEqual(
                    lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            m_booleanValues.add(
                m_node,
                m_out.doubleGreaterThanOrEqual(
                    lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileLogicalNot()
    {
        m_booleanValues.add(m_node, m_out.bitNot(boolify(m_node->child1())));
    }
    
    void compileJump()
    {
        m_out.jump(m_blocks.get(m_graph.m_blocks[m_node->takenBlockIndex()].get()));
    }
    
    void compileBranch()
    {
        m_out.branch(
            boolify(m_node->child1()),
            m_blocks.get(m_graph.m_blocks[m_node->takenBlockIndex()].get()),
            m_blocks.get(m_graph.m_blocks[m_node->notTakenBlockIndex()].get()));
    }
    
    void compileReturn()
    {
        // FIXME: have a real epilogue when we switch to using our calling convention.
        // https://bugs.webkit.org/show_bug.cgi?id=113621
        m_out.ret(lowJSValue(m_node->child1()));
    }
    
    void compileForceOSRExit()
    {
        terminate(InadequateCoverage);
    }
    
    LValue boolify(Edge edge)
    {
        switch (edge.useKind()) {
        case BooleanUse:
            return lowBoolean(m_node->child1());
        case Int32Use:
            return m_out.notZero32(lowInt32(m_node->child1()));
        case NumberUse:
            return m_out.doubleNotEqual(lowDouble(edge), m_out.doubleZero);
        case ObjectOrOtherUse:
            return m_out.bitNot(
                equalNullOrUndefined(
                    edge, CellCaseSpeculatesObject, SpeculateNullOrUndefined,
                    ManualOperandSpeculation));
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return 0;
        }
    }
    
    enum StringOrObjectMode {
        AllCellsAreFalse,
        CellCaseSpeculatesObject
    };
    enum EqualNullOrUndefinedMode {
        EqualNull,
        EqualUndefined,
        EqualNullOrUndefined,
        SpeculateNullOrUndefined
    };
    LValue equalNullOrUndefined(
        Edge edge, StringOrObjectMode cellMode, EqualNullOrUndefinedMode primitiveMode,
        OperandSpeculationMode operandMode = AutomaticOperandSpeculation)
    {
        bool validWatchpoint = masqueradesAsUndefinedWatchpointIsStillValid();
        
        LValue value = lowJSValue(edge, operandMode);
        
        LBasicBlock cellCase = FTL_NEW_BLOCK(m_out, ("EqualNullOrUndefined cell case"));
        LBasicBlock primitiveCase = FTL_NEW_BLOCK(m_out, ("EqualNullOrUndefined primitive case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("EqualNullOrUndefined continuation"));
        
        m_out.branch(isNotCell(value), primitiveCase, cellCase);
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, primitiveCase);
        
        Vector<ValueFromBlock, 3> results;
        
        switch (cellMode) {
        case AllCellsAreFalse:
            break;
        case CellCaseSpeculatesObject:
            FTL_TYPE_CHECK(
                jsValueValue(value), edge, (~SpecCell) | SpecObject,
                m_out.equal(
                    m_out.loadPtr(value, m_heaps.JSCell_structure),
                    m_out.constIntPtr(vm().stringStructure.get())));
            break;
        }
        
        if (validWatchpoint) {
            results.append(m_out.anchor(m_out.booleanFalse));
            m_out.jump(continuation);
        } else {
            LBasicBlock masqueradesCase =
                FTL_NEW_BLOCK(m_out, ("EqualNullOrUndefined masquerades case"));
                
            LValue structure = m_out.loadPtr(value, m_heaps.JSCell_structure);
            
            results.append(m_out.anchor(m_out.booleanFalse));
            
            m_out.branch(
                m_out.testNonZero8(
                    m_out.load8(structure, m_heaps.Structure_typeInfoFlags),
                    m_out.constInt8(MasqueradesAsUndefined)),
                masqueradesCase, continuation);
            
            m_out.appendTo(masqueradesCase, primitiveCase);
            
            results.append(m_out.anchor(
                m_out.equal(
                    m_out.constIntPtr(m_graph.globalObjectFor(m_node->codeOrigin)),
                    m_out.loadPtr(structure, m_heaps.Structure_globalObject))));
            m_out.jump(continuation);
        }
        
        m_out.appendTo(primitiveCase, continuation);
        
        LValue primitiveResult;
        switch (primitiveMode) {
        case EqualNull:
            primitiveResult = m_out.equal(value, m_out.constInt64(ValueNull));
            break;
        case EqualUndefined:
            primitiveResult = m_out.equal(value, m_out.constInt64(ValueUndefined));
            break;
        case EqualNullOrUndefined:
            primitiveResult = m_out.equal(
                m_out.bitAnd(value, m_out.constInt64(~TagBitUndefined)),
                m_out.constInt64(ValueNull));
            break;
        case SpeculateNullOrUndefined:
            FTL_TYPE_CHECK(
                jsValueValue(value), edge, SpecCell | SpecOther,
                m_out.notEqual(
                    m_out.bitAnd(value, m_out.constInt64(~TagBitUndefined)),
                    m_out.constInt64(ValueNull)));
            primitiveResult = m_out.booleanTrue;
            break;
        }
        results.append(m_out.anchor(primitiveResult));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        return m_out.phi(m_out.boolean, results);
    }
    
    void speculateBackward(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition)
    {
        appendOSRExit(
            kind, lowValue, highValue, failCondition, BackwardSpeculation, FormattedValue());
    }
    
    void speculateForward(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition,
        const FormattedValue& recovery)
    {
        appendOSRExit(
            kind, lowValue, highValue, failCondition, ForwardSpeculation, recovery);
    }
    
    void speculate(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition)
    {
        appendOSRExit(
            kind, lowValue, highValue, failCondition, m_direction, FormattedValue());
    }
    
    void terminate(ExitKind kind)
    {
        speculate(kind, noValue(), 0, m_out.booleanTrue);
    }
    
    void backwardTypeCheck(
        FormattedValue lowValue, Edge highValue, SpeculatedType typesPassedThrough,
        LValue failCondition)
    {
        appendTypeCheck(
            lowValue, highValue, typesPassedThrough, failCondition, BackwardSpeculation,
            FormattedValue());
    }
    
    void forwardTypeCheck(
        FormattedValue lowValue, Edge highValue, SpeculatedType typesPassedThrough,
        LValue failCondition)
    {
        appendTypeCheck(
            lowValue, highValue, typesPassedThrough, failCondition, ForwardSpeculation,
            FormattedValue());
    }
    
    void typeCheck(
        FormattedValue lowValue, Edge highValue, SpeculatedType typesPassedThrough,
        LValue failCondition)
    {
        appendTypeCheck(
            lowValue, highValue, typesPassedThrough, failCondition, m_direction,
            FormattedValue());
    }
    
    void appendTypeCheck(
        FormattedValue lowValue, Edge highValue, SpeculatedType typesPassedThrough,
        LValue failCondition, SpeculationDirection direction, FormattedValue recovery)
    {
        if (!m_state.needsTypeCheck(highValue, typesPassedThrough))
            return;
        ASSERT(mayHaveTypeCheck(highValue.useKind()));
        appendOSRExit(BadType, lowValue, highValue.node(), failCondition, direction, recovery);
        m_state.forNode(highValue).filter(typesPassedThrough);
    }
    
    LValue lowInt32(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || (edge.useKind() == Int32Use || edge.useKind() == KnownInt32Use));
        
        if (LValue result = m_int32Values.get(edge.node()))
            return result;
        
        if (LValue boxedResult = m_jsValueValues.get(edge.node())) {
            FTL_TYPE_CHECK(
                jsValueValue(boxedResult), edge, SpecInt32, isNotInt32(boxedResult));
            LValue result = unboxInt32(boxedResult);
            m_int32Values.add(edge.node(), result);
            return result;
        }

        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecInt32));
        terminate(Uncountable);
        return m_out.int32Zero;
    }
    
    LValue lowCell(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || isCell(edge.useKind()));
        
        if (LValue uncheckedResult = m_jsValueValues.get(edge.node())) {
            FTL_TYPE_CHECK(
                jsValueValue(uncheckedResult), edge, SpecCell, isNotCell(uncheckedResult));
            return uncheckedResult;
        }
        
        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecCell));
        terminate(Uncountable);
        return m_out.intPtrZero;
    }
    
    LValue lowObject(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == ObjectUse);
        
        LValue result = lowCell(edge, mode);
        speculateObject(edge, result);
        return result;
    }
    
    LValue lowNonNullObject(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == ObjectUse);
        
        LValue result = lowCell(edge, mode);
        speculateNonNullObject(edge, result);
        return result;
    }
    
    LValue lowBoolean(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == BooleanUse);
        
        if (LValue result = m_booleanValues.get(edge.node()))
            return result;
        
        if (LValue unboxedResult = m_jsValueValues.get(edge.node())) {
            FTL_TYPE_CHECK(
                jsValueValue(unboxedResult), edge, SpecBoolean, isNotBoolean(unboxedResult));
            LValue result = unboxBoolean(unboxedResult);
            m_booleanValues.add(edge.node(), result);
            return result;
        }
        
        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecBoolean));
        terminate(Uncountable);
        return m_out.booleanFalse;
    }
    
    LValue lowDouble(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || isDouble(edge.useKind()));
        
        if (LValue result = m_doubleValues.get(edge.node()))
            return result;
        
        if (LValue intResult = m_int32Values.get(edge.node())) {
            LValue result = m_out.intToDouble(intResult);
            m_doubleValues.add(edge.node(), result);
            return result;
        }
        
        if (LValue boxedResult = m_jsValueValues.get(edge.node())) {
            LBasicBlock intCase = FTL_NEW_BLOCK(m_out, ("Double unboxing int case"));
            LBasicBlock doubleCase = FTL_NEW_BLOCK(m_out, ("Double unboxing double case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Double unboxing continuation"));
            
            m_out.branch(isNotInt32(boxedResult), doubleCase, intCase);
            
            LBasicBlock lastNext = m_out.appendTo(intCase, doubleCase);
            
            ValueFromBlock intToDouble = m_out.anchor(
                m_out.intToDouble(unboxInt32(boxedResult)));
            m_out.jump(continuation);
            
            m_out.appendTo(doubleCase, continuation);
            
            FTL_TYPE_CHECK(
                jsValueValue(boxedResult), edge, SpecNumber, isCellOrMisc(boxedResult));
            
            ValueFromBlock unboxedDouble = m_out.anchor(unboxDouble(boxedResult));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            
            LValue result = m_out.phi(m_out.doubleType, intToDouble, unboxedDouble);
            
            m_doubleValues.add(edge.node(), result);
            return result;
        }
        
        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecNumber));
        terminate(Uncountable);
        return m_out.doubleZero;
    }
    
    LValue lowJSValue(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == UntypedUse);
        
        if (LValue result = m_jsValueValues.get(edge.node()))
            return result;
        
        if (LValue unboxedResult = m_int32Values.get(edge.node())) {
            LValue result = boxInt32(unboxedResult);
            m_jsValueValues.add(edge.node(), result);
            return result;
        }
        
        if (LValue unboxedResult = m_booleanValues.get(edge.node())) {
            LValue result = boxBoolean(unboxedResult);
            m_jsValueValues.add(edge.node(), result);
            return result;
        }
        
        if (LValue unboxedResult = m_doubleValues.get(edge.node())) {
            LValue result = boxDouble(unboxedResult);
            m_jsValueValues.add(edge.node(), result);
            return result;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
    
    LValue lowStorage(Edge edge)
    {
        if (LValue result = m_storageValues.get(edge.node()))
            return result;
        
        LValue result = lowCell(edge);
        m_storageValues.add(edge.node(), result);
        return result;
    }
    
    LValue isNotInt32(LValue jsValue)
    {
        return m_out.below(jsValue, m_tagTypeNumber);
    }
    LValue unboxInt32(LValue jsValue)
    {
        return m_out.castToInt32(jsValue);
    }
    LValue boxInt32(LValue value)
    {
        return m_out.add(m_out.zeroExt(value, m_out.int64), m_tagTypeNumber);
    }
    
    LValue isCellOrMisc(LValue jsValue)
    {
        return m_out.testIsZero64(jsValue, m_tagTypeNumber);
    }
    LValue unboxDouble(LValue jsValue)
    {
        return m_out.bitCast(m_out.add(jsValue, m_tagTypeNumber), m_out.doubleType);
    }
    LValue boxDouble(LValue doubleValue)
    {
        return m_out.sub(m_out.bitCast(doubleValue, m_out.int64), m_tagTypeNumber);
    }
    
    LValue isNotCell(LValue jsValue)
    {
        return m_out.testNonZero64(jsValue, m_tagMask);
    }
    
    LValue isNotBoolean(LValue jsValue)
    {
        return m_out.testNonZero64(
            m_out.bitXor(jsValue, m_out.constInt64(ValueFalse)),
            m_out.constInt64(~1));
    }
    LValue unboxBoolean(LValue jsValue)
    {
        // We want to use a cast that guarantees that LLVM knows that even the integer
        // value is just 0 or 1. But for now we do it the dumb way.
        return m_out.notZero64(m_out.bitAnd(jsValue, m_out.constInt64(1)));
    }
    LValue boxBoolean(LValue value)
    {
        return m_out.select(
            value, m_out.constInt64(ValueTrue), m_out.constInt64(ValueFalse));
    }

    void speculate(Edge edge)
    {
        switch (edge.useKind()) {
        case UntypedUse:
            break;
        case KnownInt32Use:
        case KnownNumberUse:
            ASSERT(!m_state.needsTypeCheck(edge));
            break;
        case Int32Use:
            speculateInt32(edge);
            break;
        case CellUse:
            speculateCell(edge);
            break;
        case KnownCellUse:
            ASSERT(!m_state.needsTypeCheck(edge));
            break;
        case ObjectUse:
            speculateObject(edge);
            break;
        case RealNumberUse:
            speculateRealNumber(edge);
            break;
        case NumberUse:
            speculateNumber(edge);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    
    void speculate(Node*, Edge edge)
    {
        speculate(edge);
    }
    
    void speculateInt32(Edge edge)
    {
        lowInt32(edge);
    }
    
    void speculateCell(Edge edge)
    {
        lowCell(edge);
    }
    
    void speculateObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecObject,
            m_out.equal(
                m_out.loadPtr(cell, m_heaps.JSCell_structure),
                m_out.constIntPtr(vm().stringStructure.get())));
    }
    
    void speculateObject(Edge edge)
    {
        speculateObject(edge, lowCell(edge));
    }
    
    void speculateNonNullObject(Edge edge, LValue cell)
    {
        LValue structure = m_out.loadPtr(cell, m_heaps.JSCell_structure);
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecObject,
            m_out.equal(structure, m_out.constIntPtr(vm().stringStructure.get())));
        if (masqueradesAsUndefinedWatchpointIsStillValid())
            return;
        
        speculate(
            BadType, jsValueValue(cell), edge.node(),
            m_out.testNonZero8(
                m_out.load8(structure, m_heaps.Structure_typeInfoFlags),
                m_out.constInt8(MasqueradesAsUndefined)));
    }
    
    void speculateNumber(Edge edge)
    {
        // Do an early return here because lowDouble() can create a lot of control flow.
        if (!m_state.needsTypeCheck(edge))
            return;
        
        lowDouble(edge);
    }
    
    void speculateRealNumber(Edge edge)
    {
        // Do an early return here because lowDouble() can create a lot of control flow.
        if (!m_state.needsTypeCheck(edge))
            return;
        
        LValue value = lowDouble(edge);
        FTL_TYPE_CHECK(
            doubleValue(value), edge, SpecRealNumber,
            m_out.doubleNotEqualOrUnordered(value, value));
    }

    bool masqueradesAsUndefinedWatchpointIsStillValid()
    {
        return m_graph.masqueradesAsUndefinedWatchpointIsStillValid(m_node->codeOrigin);
    }
    
    bool masqueradesAsUndefinedWatchpointIfIsStillValid()
    {
        if (!masqueradesAsUndefinedWatchpointIsStillValid())
            return false;
        
        // FIXME: Implement masquerades-as-undefined watchpoints.
        // https://bugs.webkit.org/show_bug.cgi?id=113647
        return true;
    }
    
    bool isLive(Node* node)
    {
        HashMap<Node*, unsigned>::iterator iter = m_timeToLive.find(node);
        ASSERT(iter != m_timeToLive.end());
        return !!iter->value;
    }
    
    void use(Edge edge)
    {
        if (!edge->hasResult()) {
            ASSERT(edge->hasVariableAccessData());
            return;
        }
        
        HashMap<Node*, unsigned>::iterator iter = m_timeToLive.find(edge.node());
        ASSERT(iter != m_timeToLive.end());
        ASSERT(iter->value);
        iter->value--;
    }
    
    // Wrapper used only for DFG_NODE_DO_TO_CHILDREN
    void use(Node*, Edge edge)
    {
        use(edge);
    }
    
    void initializeOSRExitStateForBlock()
    {
        for (unsigned i = m_valueSources.size(); i--;) {
            Node* node = m_highBlock->variablesAtHead[i];
            if (!node) {
                m_valueSources[i] = ValueSource(SourceIsDead);
                continue;
            }
            
            VariableAccessData* variable = node->variableAccessData();
            SpeculatedType prediction = variable->argumentAwarePrediction();
            
            if (variable->isArgumentsAlias()) {
                m_valueSources[i] = ValueSource(ArgumentsSource);
                continue;
            }
            
            if (!node->shouldGenerate()) {
                m_valueSources[i] = ValueSource(SourceIsDead);
                continue;
            }
            
            if (m_flushedLocalOps.contains(node)) {
                // This value will have been flushed to the JSStack in some form or
                // another.
                
                if (variable->shouldUnboxIfPossible()) {
                    if (variable->shouldUseDoubleFormat()) {
                        m_valueSources[i] = ValueSource(DoubleInJSStack);
                        continue;
                    }
                    
                    if (isInt32Speculation(prediction)) {
                        m_valueSources[i] = ValueSource(Int32InJSStack);
                        continue;
                    }
                }
                
                m_valueSources[i] = ValueSource(ValueInJSStack);
                continue;
            }
            
            if (variable->shouldUnboxIfPossible()) {
                if (variable->shouldUseDoubleFormat()) {
                    m_valueSources[i] = ValueSource(ValueInLocals);
                    continue;
                }
                
                if (isInt32Speculation(prediction)) {
                    m_valueSources[i] = ValueSource(Int32InLocals);
                    continue;
                }
                
                if (isBooleanSpeculation(prediction)) {
                    m_valueSources[i] = ValueSource(BooleanInLocals);
                    continue;
                }
            }
            
            m_valueSources[i] = ValueSource(ValueInLocals);
        }
    }
    
    void appendOSRExit(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition,
        SpeculationDirection direction, FormattedValue recovery)
    {
        if (verboseCompilationEnabled())
            dataLog("    OSR exit with value sources: ", m_valueSources, "\n");
        
        ASSERT(m_ftlState.jitCode->osrExit.size() == m_ftlState.osrExit.size());
        unsigned index = m_ftlState.osrExit.size();
        
        m_ftlState.jitCode->osrExit.append(
            OSRExit(
                kind, lowValue.format(), m_graph.methodOfGettingAValueProfileFor(highValue),
                m_codeOrigin, m_lastSetOperand, m_valueSources.numberOfArguments(),
                m_valueSources.numberOfLocals()));
        m_ftlState.osrExit.append(OSRExitCompilationInfo());
        
        OSRExit& exit = m_ftlState.jitCode->osrExit.last();
        OSRExitCompilationInfo& info = m_ftlState.osrExit.last();

        LBasicBlock failCase = FTL_NEW_BLOCK(m_out, ("OSR exit failCase"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("OSR exit continuation"));
        
        m_out.branch(failCondition, failCase, continuation);
        
        m_out.appendTo(m_prologue);
        info.m_thunkAddress = buildAlloca(m_out.m_builder, m_out.intPtr);
        
        LBasicBlock lastNext = m_out.appendTo(failCase, continuation);
        
        ExitArgumentList arguments;
        arguments.append(m_callFrame);
        if (!!lowValue)
            arguments.append(lowValue.value());
        
        for (unsigned i = 0; i < exit.m_values.size(); ++i) {
            ValueSource source = m_valueSources[i];
            
            switch (source.kind()) {
            case ValueInJSStack:
                exit.m_values[i] = ExitValue::inJSStack();
                break;
            case Int32InJSStack:
                exit.m_values[i] = ExitValue::inJSStackAsInt32();
                break;
            case DoubleInJSStack:
                exit.m_values[i] = ExitValue::inJSStackAsDouble();
                break;
            case ValueInLocals:
                addExitArgument(
                    exit, arguments, i, ValueFormatJSValue, m_out.get(m_locals64[i]));
                break;
            case Int32InLocals:
                addExitArgument(
                    exit, arguments, i, ValueFormatInt32, m_out.get(m_locals32[i]));
                break;
            case BooleanInLocals:
                addExitArgument(
                    exit, arguments, i, ValueFormatBoolean, m_out.get(m_localsBoolean[i]));
                break;
            case DoubleInLocals:
                addExitArgument(
                    exit, arguments, i, ValueFormatDouble, m_out.get(m_localsDouble[i]));
                break;
            case ArgumentsSource:
                // FIXME: implement PhantomArguments.
                // https://bugs.webkit.org/show_bug.cgi?id=113986
                RELEASE_ASSERT_NOT_REACHED();
                break;
            case SourceIsDead:
                exit.m_values[i] = ExitValue::dead();
                break;
            case HaveNode:
                addExitArgumentForNode(exit, arguments, i, source.node());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
        
        if (verboseCompilationEnabled())
            dataLog("        Exit values: ", exit.m_values, "\n");
        
        if (direction == ForwardSpeculation) {
            ASSERT(m_node);
            exit.convertToForward(m_highBlock, m_node, m_nodeIndex, recovery, arguments);
        }
        
        // So, the really lame thing here is that we have to build an LLVM function type.
        // Booo.
        Vector<LType, 16> argumentTypes;
        for (unsigned i = 0; i < arguments.size(); ++i)
            argumentTypes.append(typeOf(arguments[i]));
        
        m_out.call(
            m_out.intToPtr(
                m_out.get(info.m_thunkAddress),
                pointerType(functionType(m_out.voidType, argumentTypes))),
            arguments);
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
        
        m_exitThunkGenerator.emitThunk(index);
    }
    
    void addExitArgumentForNode(
        OSRExit& exit, ExitArgumentList& arguments, unsigned index, Node* node)
    {
        ASSERT(node->shouldGenerate());
        ASSERT(node->hasResult());

        if (tryToSetConstantExitArgument(exit, index, node))
            return;
        
        if (!isLive(node)) {
            bool found = false;
            
            if (needsOSRBackwardRewiring(node->op())) {
                node = node->child1().node();
                if (tryToSetConstantExitArgument(exit, index, node))
                    return;
                if (isLive(node))
                    found = true;
            }
            
            if (!found) {
                Node* int32ToDouble = 0;
                Node* valueToInt32 = 0;
                Node* uint32ToNumber = 0;
                Node* doubleAsInt32 = 0;
                
                HashMap<Node*, unsigned>::iterator iter = m_timeToLive.begin();
                HashMap<Node*, unsigned>::iterator end = m_timeToLive.end();
                for (; iter != end; ++iter) {
                    if (!iter->value)
                        continue;
                    Node* candidate = iter->key;
                    if (!candidate->child1())
                        continue;
                    if (candidate->child1() != node)
                        continue;
                    switch (candidate->op()) {
                    case Int32ToDouble:
                    case ForwardInt32ToDouble:
                        int32ToDouble = candidate;
                        break;
                    case ValueToInt32:
                        valueToInt32 = candidate;
                        break;
                    case UInt32ToNumber:
                        uint32ToNumber = candidate;
                        break;
                    case DoubleAsInt32:
                        uint32ToNumber = candidate;
                        break;
                    default:
                        ASSERT(!needsOSRForwardRewiring(candidate->op()));
                        break;
                    }
                }
                
                if (doubleAsInt32)
                    node = doubleAsInt32;
                else if (int32ToDouble)
                    node = int32ToDouble;
                else if (valueToInt32)
                    node = valueToInt32;
                else if (uint32ToNumber)
                    node = uint32ToNumber;
                
                if (isLive(node))
                    found = true;
            }
            
            if (!found) {
                exit.m_values[index] = ExitValue::dead();
                return;
            }
        }
        
        ASSERT(isLive(node));
        
        if (LValue result = m_int32Values.get(node)) {
            addExitArgument(exit, arguments, index, ValueFormatInt32, result);
            return;
        }
        
        if (LValue result = m_booleanValues.get(node)) {
            addExitArgument(exit, arguments, index, ValueFormatBoolean, result);
            return;
        }
        
        if (LValue result = m_jsValueValues.get(node)) {
            addExitArgument(exit, arguments, index, ValueFormatJSValue, result);
            return;
        }
        
        if (LValue result = m_doubleValues.get(node)) {
            addExitArgument(exit, arguments, index, ValueFormatDouble, result);
            return;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }
    
    bool tryToSetConstantExitArgument(OSRExit& exit, unsigned index, Node* node)
    {
        if (!node)
            return false;
        
        switch (node->op()) {
        case JSConstant:
        case WeakJSConstant:
            exit.m_values[index] = ExitValue::constant(m_graph.valueOfJSConstant(node));
            return true;
        case PhantomArguments:
            // FIXME: implement PhantomArguments.
            // https://bugs.webkit.org/show_bug.cgi?id=113986
            RELEASE_ASSERT_NOT_REACHED();
            return true;
        default:
            return false;
        }
    }
    
    void addExitArgument(
        OSRExit& exit, ExitArgumentList& arguments, unsigned index, ValueFormat format,
        LValue value)
    {
        exit.m_values[index] = ExitValue::exitArgument(ExitArgument(format, arguments.size()));
        arguments.append(value);
    }
    
    void linkOSRExitsAndCompleteInitializationBlocks()
    {
        MacroAssemblerCodeRef osrExitThunk =
            vm().getCTIStub(osrExitGenerationThunkGenerator);
        CodeLocationLabel target = CodeLocationLabel(osrExitThunk.code());
        
        m_out.appendTo(m_prologue);
        m_out.jump(m_initialization);
        
        m_out.appendTo(m_initialization);
        
        if (m_exitThunkGenerator.didThings()) {
            OwnPtr<LinkBuffer> linkBuffer = adoptPtr(new LinkBuffer(
                vm(), &m_exitThunkGenerator, m_ftlState.graph.m_codeBlock,
                JITCompilationMustSucceed));
        
            ASSERT(m_ftlState.osrExit.size() == m_ftlState.jitCode->osrExit.size());
        
            for (unsigned i = 0; i < m_ftlState.osrExit.size(); ++i) {
                OSRExitCompilationInfo& info = m_ftlState.osrExit[i];
                OSRExit& exit = m_ftlState.jitCode->osrExit[i];
            
                linkBuffer->link(info.m_thunkJump, target);
            
                m_out.set(
                    m_out.constIntPtr(
                        linkBuffer->locationOf(info.m_thunkLabel).executableAddress()),
                    info.m_thunkAddress);
            
                exit.m_patchableCodeOffset = linkBuffer->offsetOf(info.m_thunkJump);
            }
        
            m_ftlState.finalizer->initializeExitThunksLinkBuffer(linkBuffer.release());
        }

        m_out.jump(m_argumentChecks);
    }
    
    void observeMovHint(Node* node)
    {
        ASSERT(node->containsMovHint());
        ASSERT(node->op() != ZombieHint);
        
        int operand = node->local();
        
        m_lastSetOperand = operand;
        m_valueSources.operand(operand) = ValueSource(node->child1().node());
    }
    
    void addWeakReference(JSCell* target)
    {
        m_ftlState.jitCode->common.weakReferences.append(
            WriteBarrier<JSCell>(vm(), codeBlock()->ownerExecutable(), target));
    }
    
    LValue weakPointer(JSCell* pointer)
    {
        addWeakReference(pointer);
        return m_out.constIntPtr(pointer);
    }
    
    TypedPointer addressFor(LValue base, int operand, ptrdiff_t offset = 0)
    {
        return m_out.address(base, m_heaps.variables[operand], offset);
    }
    TypedPointer payloadFor(LValue base, int operand)
    {
        return addressFor(base, operand, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    }
    TypedPointer tagFor(LValue base, int operand)
    {
        return addressFor(base, operand, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
    }
    TypedPointer addressFor(int operand)
    {
        return addressFor(m_callFrame, operand);
    }
    TypedPointer payloadFor(int operand)
    {
        return payloadFor(m_callFrame, operand);
    }
    TypedPointer tagFor(int operand)
    {
        return tagFor(m_callFrame, operand);
    }
    
    VM& vm() { return m_graph.m_vm; }
    CodeBlock* codeBlock() { return m_graph.m_codeBlock; }
    
    Graph& m_graph;
    State& m_ftlState;
    AbstractHeapRepository m_heaps;
    Output m_out;
    
    LBasicBlock m_prologue;
    LBasicBlock m_initialization;
    LBasicBlock m_argumentChecks;
    HashMap<BasicBlock*, LBasicBlock> m_blocks;
    
    LValue m_callFrame;
    LValue m_tagTypeNumber;
    LValue m_tagMask;
    
    HashSet<Node*> m_flushedLocalOps;
    Vector<Node*> m_flushedLocalOpWorklist;
    
    HashMap<Node*, LValue> m_int32Values;
    HashMap<Node*, LValue> m_jsValueValues;
    HashMap<Node*, LValue> m_booleanValues;
    HashMap<Node*, LValue> m_storageValues;
    HashMap<Node*, LValue> m_doubleValues;
    HashMap<Node*, unsigned> m_timeToLive;
    
    Operands<LValue> m_localsBoolean;
    Operands<LValue> m_locals32;
    Operands<LValue> m_locals64;
    Operands<LValue> m_localsDouble;
    
    Operands<ValueSource> m_valueSources;
    int m_lastSetOperand;
    ExitThunkGenerator m_exitThunkGenerator;
    
    AbstractState m_state;
    BasicBlock* m_highBlock;
    BasicBlock* m_nextHighBlock;
    LBasicBlock m_nextLowBlock;
    
    CodeOrigin m_codeOrigin;
    unsigned m_nodeIndex;
    Node* m_node;
    SpeculationDirection m_direction;
};

void lowerDFGToLLVM(State& state)
{
    LowerDFGToLLVM lowering(state);
    lowering.lower();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

