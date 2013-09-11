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
#include "DFGAbstractInterpreterInlines.h"
#include "DFGInPlaceAbstractState.h"
#include "FTLAbstractHeapRepository.h"
#include "FTLExitThunkGenerator.h"
#include "FTLForOSREntryJITCode.h"
#include "FTLFormattedValue.h"
#include "FTLLoweredNodeValue.h"
#include "FTLOutput.h"
#include "FTLThunks.h"
#include "FTLValueSource.h"
#include "LinkBuffer.h"
#include "OperandsInlines.h"
#include "Operations.h"
#include <wtf/ProcessID.h>

namespace JSC { namespace FTL {

using namespace DFG;

static int compileCounter;

// Using this instead of typeCheck() helps to reduce the load on LLVM, by creating
// significantly less dead code.
#define FTL_TYPE_CHECK(lowValue, highValue, typesPassedThrough, failCondition) do { \
        FormattedValue _ftc_lowValue = (lowValue);                      \
        Edge _ftc_highValue = (highValue);                              \
        SpeculatedType _ftc_typesPassedThrough = (typesPassedThrough);  \
        if (!m_interpreter.needsTypeCheck(_ftc_highValue, _ftc_typesPassedThrough)) \
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
        , m_valueSources(OperandsLike, state.graph.block(0)->variablesAtHead)
        , m_lastSetOperand(std::numeric_limits<int>::max())
        , m_exitThunkGenerator(state)
        , m_state(state.graph)
        , m_interpreter(state.graph, m_state)
    {
    }
    
    void lower()
    {
        CString name;
        if (verboseCompilationEnabled()) {
            name = toCString(
                "jsBody_", atomicIncrement(&compileCounter), "_", codeBlock()->inferredName(),
                "_", codeBlock()->hash());
        } else
            name = "jsBody";
        
        m_graph.m_dominators.computeIfNecessary(m_graph);
        
        m_ftlState.module =
            LLVMModuleCreateWithNameInContext(name.data(), m_ftlState.context);
        
        m_ftlState.function = addFunction(
            m_ftlState.module, name.data(), functionType(m_out.int64, m_out.intPtr));
        setFunctionCallingConv(m_ftlState.function, LLVMCCallConv);
        
        m_out.initialize(m_ftlState.module, m_ftlState.function, m_heaps);
        
        m_prologue = appendBasicBlock(m_ftlState.context, m_ftlState.function);
        m_out.appendTo(m_prologue);
        createPhiVariables();
        
        m_initialization = appendBasicBlock(m_ftlState.context, m_ftlState.function);

        m_callFrame = m_out.param(0);
        m_tagTypeNumber = m_out.constInt64(TagTypeNumber);
        m_tagMask = m_out.constInt64(TagMask);
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            m_highBlock = m_graph.block(blockIndex);
            if (!m_highBlock)
                continue;
            m_blocks.add(m_highBlock, FTL_NEW_BLOCK(m_out, ("Block ", *m_highBlock)));
        }
        
        Vector<BasicBlock*> depthFirst;
        m_graph.getBlocksInDepthFirstOrder(depthFirst);
        for (unsigned i = 0; i < depthFirst.size(); ++i)
            compileBlock(depthFirst[i]);
        
        // And now complete the initialization block.
        linkOSRExitsAndCompleteInitializationBlocks();

        if (Options::dumpLLVMIR())
            dumpModule(m_ftlState.module);
        
        if (verboseCompilationEnabled())
            m_ftlState.dumpState("after lowering");
        if (validationEnabled())
            verifyModule(m_ftlState.module);
    }

private:
    
    void createPhiVariables()
    {
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                Node* node = block->at(nodeIndex);
                if (node->op() != Phi)
                    continue;
                LType type;
                switch (node->flags() & NodeResultMask) {
                case NodeResultNumber:
                    type = m_out.doubleType;
                    break;
                case NodeResultInt32:
                    type = m_out.int32;
                    break;
                case NodeResultBoolean:
                    type = m_out.boolean;
                    break;
                case NodeResultJS:
                    type = m_out.int64;
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
                m_phis.add(node, buildAlloca(m_out.m_builder, type));
            }
        }
    }
    
    void compileBlock(BasicBlock* block)
    {
        if (!block)
            return;
        
        if (verboseCompilationEnabled())
            dataLog("Compiling block ", *block, "\n");
        
        m_highBlock = block;
        
        LBasicBlock lowBlock = m_blocks.get(m_highBlock);
        
        m_nextHighBlock = 0;
        for (BlockIndex nextBlockIndex = m_highBlock->index + 1; nextBlockIndex < m_graph.numBlocks(); ++nextBlockIndex) {
            m_nextHighBlock = m_graph.block(nextBlockIndex);
            if (m_nextHighBlock)
                break;
        }
        m_nextLowBlock = m_nextHighBlock ? m_blocks.get(m_nextHighBlock) : 0;
        
        // All of this effort to find the next block gives us the ability to keep the
        // generated IR in roughly program order. This ought not affect the performance
        // of the generated code (since we expect LLVM to reorder things) but it will
        // make IR dumps easier to read.
        m_out.appendTo(lowBlock, m_nextLowBlock);
        
        if (Options::ftlCrashes())
            m_out.crashNonTerminal();
        
        if (!m_highBlock->cfaHasVisited) {
            m_out.crash();
            return;
        }
        
        initializeOSRExitStateForBlock();
        
        m_live = block->ssa->liveAtHead;
        
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
        m_codeOriginForExitProfile = m_node->codeOrigin;
        m_codeOriginForExitTarget = m_node->codeOriginForExitTarget;
        
        if (verboseCompilationEnabled())
            dataLog("Lowering ", m_node, "\n");
        
        bool shouldExecuteEffects = m_interpreter.startExecuting(m_node);
        
        m_direction = (m_node->flags() & NodeExitsForward) ? ForwardSpeculation : BackwardSpeculation;
        
        switch (m_node->op()) {
        case Upsilon:
            compileUpsilon();
            break;
        case Phi:
            compilePhi();
            break;
        case JSConstant:
            compileJSConstant();
            break;
        case WeakJSConstant:
            compileWeakJSConstant();
            break;
        case GetArgument:
            compileGetArgument();
            break;
        case ExtractOSREntryLocal:
            compileExtractOSREntryLocal();
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
        case LoopHint:
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
        case ArithMod:
            compileArithMod();
            break;
        case ArithMin:
        case ArithMax:
            compileArithMinOrMax();
            break;
        case ArithAbs:
            compileArithAbs();
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
        case ArrayifyToStructure:
            compileArrayifyToStructure();
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
        case GetIndexedPropertyStorage:
            compileGetIndexedPropertyStorage();
            break;
        case CheckArray:
            compileCheckArray();
            break;
        case GetArrayLength:
            compileGetArrayLength();
            break;
        case GetByVal:
            compileGetByVal();
            break;
        case PutByVal:
        case PutByValAlias:
            compilePutByVal();
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
        case GlobalVarWatchpoint:
            compileGlobalVarWatchpoint();
            break;
        case GetMyScope:
            compileGetMyScope();
            break;
        case SkipScope:
            compileSkipScope();
            break;
        case GetClosureRegisters:
            compileGetClosureRegisters();
            break;
        case GetClosureVar:
            compileGetClosureVar();
            break;
        case PutClosureVar:
            compilePutClosureVar();
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
        case Call:
        case Construct:
            compileCallOrConstruct();
            break;
        case Jump:
            compileJump();
            break;
        case Branch:
            compileBranch();
            break;
        case Switch:
            compileSwitch();
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
        
        if (m_node->adjustedRefCount())
            m_live.add(m_node);
        
        if (shouldExecuteEffects)
            m_interpreter.executeEffects(nodeIndex);
        
        return true;
    }
    
    void compileUpsilon()
    {
        LValue destination = m_phis.get(m_node->phi());
        
        switch (m_node->child1().useKind()) {
        case NumberUse:
            m_out.set(lowDouble(m_node->child1()), destination);
            break;
        case Int32Use:
            m_out.set(lowInt32(m_node->child1()), destination);
            break;
        case BooleanUse:
            m_out.set(lowBoolean(m_node->child1()), destination);
            break;
        case CellUse:
            m_out.set(lowCell(m_node->child1()), destination);
            break;
        case UntypedUse:
            m_out.set(lowJSValue(m_node->child1()), destination);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compilePhi()
    {
        LValue source = m_phis.get(m_node);
        
        switch (m_node->flags() & NodeResultMask) {
        case NodeResultNumber:
            setDouble(m_out.get(source));
            break;
        case NodeResultInt32:
            setInt32(m_out.get(source));
            break;
        case NodeResultBoolean:
            setBoolean(m_out.get(source));
            break;
        case NodeResultJS:
            setJSValue(m_out.get(source));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileJSConstant()
    {
        JSValue value = m_graph.valueOfJSConstant(m_node);
        if (value.isDouble())
            setDouble(m_out.constDouble(value.asDouble()));
        else
            setJSValue(m_out.constInt64(JSValue::encode(value)));
    }
    
    void compileWeakJSConstant()
    {
        setJSValue(weakPointer(m_node->weakConstant()));
    }
    
    void compileGetArgument()
    {
        VariableAccessData* variable = m_node->variableAccessData();
        int operand = variable->operand();

        LValue jsValue = m_out.load64(addressFor(operand));

        switch (useKindFor(variable->flushFormat())) {
        case Int32Use:
            speculateBackward(BadType, jsValueValue(jsValue), m_node, isNotInt32(jsValue));
            setInt32(unboxInt32(jsValue));
            break;
        case CellUse:
            speculateBackward(BadType, jsValueValue(jsValue), m_node, isNotCell(jsValue));
            setJSValue(jsValue);
            break;
        case BooleanUse:
            speculateBackward(BadType, jsValueValue(jsValue), m_node, isNotBoolean(jsValue));
            setBoolean(unboxBoolean(jsValue));
            break;
        case UntypedUse:
            setJSValue(jsValue);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileExtractOSREntryLocal()
    {
        EncodedJSValue* buffer = static_cast<EncodedJSValue*>(
            m_ftlState.jitCode->ftlForOSREntry()->entryBuffer()->dataBuffer());
        setJSValue(m_out.load64(m_out.absolute(buffer + m_node->unlinkedLocal())));
    }
    
    void compileGetLocal()
    {
        // GetLocals arise only for captured variables.
        
        VariableAccessData* variable = m_node->variableAccessData();
        AbstractValue& value = m_state.variables().operand(variable->local());
        
        RELEASE_ASSERT(variable->isCaptured());
        
        if (isInt32Speculation(value.m_type))
            setInt32(m_out.load32(payloadFor(variable->local())));
        else
            setJSValue(m_out.load64(addressFor(variable->local())));
    }
    
    void compileSetLocal()
    {
        observeMovHint(m_node);
        
        VariableAccessData* variable = m_node->variableAccessData();
        switch (variable->flushFormat()) {
        case FlushedJSValue: {
            LValue value = lowJSValue(m_node->child1());
            m_out.store64(value, addressFor(variable->local()));
            m_valueSources.operand(variable->local()) = ValueSource(ValueInJSStack);
            return;
        }
            
        case FlushedDouble: {
            LValue value = lowDouble(m_node->child1());
            m_out.storeDouble(value, addressFor(variable->local()));
            m_valueSources.operand(variable->local()) = ValueSource(DoubleInJSStack);
            return;
        }
            
        case FlushedInt32: {
            LValue value = lowInt32(m_node->child1());
            m_out.store32(value, payloadFor(variable->local()));
            m_valueSources.operand(variable->local()) = ValueSource(Int32InJSStack);
            return;
        }
            
        case FlushedCell: {
            LValue value = lowCell(m_node->child1());
            m_out.store64(value, addressFor(variable->local()));
            m_valueSources.operand(variable->local()) = ValueSource(ValueInJSStack);
            return;
        }
            
        case FlushedBoolean: {
            speculateBoolean(m_node->child1());
            m_out.store64(
                lowJSValue(m_node->child1(), ManualOperandSpeculation),
                addressFor(variable->local()));
            m_valueSources.operand(variable->local()) = ValueSource(ValueInJSStack);
            return;
        }
            
        case DeadFlush:
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        RELEASE_ASSERT_NOT_REACHED();
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
            
            if (bytecodeCanTruncateInteger(m_node->arithNodeFlags())) {
                setInt32(m_out.add(left, right));
                break;
            }
            
            LValue result = m_out.addWithOverflow32(left, right);
            speculate(Overflow, noValue(), 0, m_out.extractValue(result, 1));
            setInt32(m_out.extractValue(result, 0));
            break;
        }
            
        case NumberUse: {
            setDouble(
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
            
            if (bytecodeCanTruncateInteger(m_node->arithNodeFlags())) {
                setInt32(m_out.sub(left, right));
                break;
            }
            
            LValue result = m_out.subWithOverflow32(left, right);
            speculate(Overflow, noValue(), 0, m_out.extractValue(result, 1));
            setInt32(m_out.extractValue(result, 0));
            break;
        }
            
        case NumberUse: {
            setDouble(
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
            if (bytecodeCanTruncateInteger(m_node->arithNodeFlags()))
                result = m_out.mul(left, right);
            else {
                LValue overflowResult = m_out.mulWithOverflow32(left, right);
                speculate(Overflow, noValue(), 0, m_out.extractValue(overflowResult, 1));
                result = m_out.extractValue(overflowResult, 0);
            }
            
            if (!bytecodeCanIgnoreNegativeZero(m_node->arithNodeFlags())) {
                LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("ArithMul slow case"));
                LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArithMul continuation"));
                
                m_out.branch(m_out.notZero32(result), continuation, slowCase);
                
                LBasicBlock lastNext = m_out.appendTo(slowCase, continuation);
                speculate(NegativeZero, noValue(), 0, m_out.lessThan(left, m_out.int32Zero));
                speculate(NegativeZero, noValue(), 0, m_out.lessThan(right, m_out.int32Zero));
                m_out.jump(continuation);
                m_out.appendTo(continuation, lastNext);
            }
            
            setInt32(result);
            break;
        }
            
        case NumberUse: {
            setDouble(
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
            
            if (bytecodeUsesAsNumber(m_node->arithNodeFlags())) {
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
            
            if (!bytecodeCanIgnoreNegativeZero(m_node->arithNodeFlags())) {
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
            
            if (bytecodeUsesAsNumber(m_node->arithNodeFlags())) {
                speculate(
                    Overflow, noValue(), 0,
                    m_out.notEqual(m_out.mul(divisionResult, denominator), numerator));
            }
            
            results.append(m_out.anchor(divisionResult));
            m_out.jump(done);
            
            m_out.appendTo(done, lastNext);
            
            setInt32(m_out.phi(m_out.int32, results));
            break;
        }
            
        case NumberUse: {
            setDouble(
                m_out.doubleDiv(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileArithMod()
    {
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue numerator = lowInt32(m_node->child1());
            LValue denominator = lowInt32(m_node->child2());
            
            LBasicBlock unsafeDenominator = FTL_NEW_BLOCK(m_out, ("ArithMod unsafe denominator"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArithMod continuation"));
            LBasicBlock done = FTL_NEW_BLOCK(m_out, ("ArithMod done"));
            
            Vector<ValueFromBlock, 3> results;
            
            LValue adjustedDenominator = m_out.add(denominator, m_out.int32One);
            
            m_out.branch(m_out.above(adjustedDenominator, m_out.int32One), continuation, unsafeDenominator);
            
            LBasicBlock lastNext = m_out.appendTo(unsafeDenominator, continuation);
            
            LValue neg2ToThe31 = m_out.constInt32(-2147483647-1);
            
            // FIXME: -2^31 / -1 will actually yield negative zero, so we could have a
            // separate case for that. But it probably doesn't matter so much.
            if (bytecodeUsesAsNumber(m_node->arithNodeFlags())) {
                speculate(Overflow, noValue(), 0, m_out.isZero32(denominator));
                speculate(Overflow, noValue(), 0, m_out.equal(numerator, neg2ToThe31));
                m_out.jump(continuation);
            } else {
                // This is the case where we convert the result to an int after we're done. So,
                // if the denominator is zero, then the result should be result should be zero.
                // If the denominator is not zero (i.e. it's -1 because we're guarded by the
                // check above) and the numerator is -2^31 then the result should be -2^31.
                
                LBasicBlock modByZero = FTL_NEW_BLOCK(m_out, ("ArithMod modulo by zero"));
                LBasicBlock notModByZero = FTL_NEW_BLOCK(m_out, ("ArithMod not modulo by zero"));
                LBasicBlock neg2ToThe31ByNeg1 = FTL_NEW_BLOCK(m_out, ("ArithMod -2^31/-1"));
                
                m_out.branch(m_out.isZero32(denominator), modByZero, notModByZero);
                
                m_out.appendTo(modByZero, notModByZero);
                results.append(m_out.anchor(m_out.int32Zero));
                m_out.jump(done);
                
                m_out.appendTo(notModByZero, neg2ToThe31ByNeg1);
                m_out.branch(m_out.equal(numerator, neg2ToThe31), neg2ToThe31ByNeg1, continuation);
                
                m_out.appendTo(neg2ToThe31ByNeg1, continuation);
                results.append(m_out.anchor(m_out.int32Zero));
                m_out.jump(done);
            }
            
            m_out.appendTo(continuation, done);
            
            LValue remainder = m_out.rem(numerator, denominator);
            
            if (!bytecodeCanIgnoreNegativeZero(m_node->arithNodeFlags())) {
                LBasicBlock negativeNumerator = FTL_NEW_BLOCK(m_out, ("ArithMod negative numerator"));
                LBasicBlock numeratorContinuation = FTL_NEW_BLOCK(m_out, ("ArithMod numerator continuation"));
                
                m_out.branch(
                    m_out.lessThan(numerator, m_out.int32Zero),
                    negativeNumerator, numeratorContinuation);
                
                LBasicBlock innerLastNext = m_out.appendTo(negativeNumerator, numeratorContinuation);
                
                speculate(NegativeZero, noValue(), 0, m_out.isZero32(remainder));
                
                m_out.jump(numeratorContinuation);
                
                m_out.appendTo(numeratorContinuation, innerLastNext);
            }
            
            results.append(m_out.anchor(remainder));
            m_out.jump(done);
            
            m_out.appendTo(done, lastNext);
            
            setInt32(m_out.phi(m_out.int32, results));
            break;
        }
            
        case NumberUse: {
            setDouble(
                m_out.doubleRem(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    void compileArithMinOrMax()
    {
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue left = lowInt32(m_node->child1());
            LValue right = lowInt32(m_node->child2());
            
            setInt32(
                m_out.select(
                    m_node->op() == ArithMin
                        ? m_out.lessThan(left, right)
                        : m_out.lessThan(right, left),
                    left, right));
            break;
        }
            
        case NumberUse: {
            LValue left = lowDouble(m_node->child1());
            LValue right = lowDouble(m_node->child2());
            
            LBasicBlock notLessThan = FTL_NEW_BLOCK(m_out, ("ArithMin/ArithMax not less than"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArithMin/ArithMax continuation"));
            
            Vector<ValueFromBlock, 2> results;
            
            results.append(m_out.anchor(left));
            m_out.branch(
                m_node->op() == ArithMin
                    ? m_out.doubleLessThan(left, right)
                    : m_out.doubleGreaterThan(left, right),
                continuation, notLessThan);
            
            LBasicBlock lastNext = m_out.appendTo(notLessThan, continuation);
            results.append(m_out.anchor(m_out.select(
                m_node->op() == ArithMin
                    ? m_out.doubleGreaterThanOrEqual(left, right)
                    : m_out.doubleLessThanOrEqual(left, right),
                right, m_out.constDouble(0.0 / 0.0))));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setDouble(m_out.phi(m_out.doubleType, results));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileArithAbs()
    {
        switch (m_node->child1().useKind()) {
        case Int32Use: {
            LValue value = lowInt32(m_node->child1());
            
            LValue mask = m_out.aShr(value, m_out.constInt32(31));
            LValue result = m_out.bitXor(mask, m_out.add(mask, value));
            
            speculate(Overflow, noValue(), 0, m_out.equal(result, m_out.constInt32(1 << 31)));
            
            setInt32(result);
            break;
        }
            
        case NumberUse: {
            setDouble(m_out.doubleAbs(lowDouble(m_node->child1())));
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
            if (bytecodeCanTruncateInteger(m_node->arithNodeFlags()))
                result = m_out.neg(value);
            else {
                // We don't have a negate-with-overflow intrinsic. Hopefully this
                // does the trick, though.
                LValue overflowResult = m_out.subWithOverflow32(m_out.int32Zero, value);
                speculate(Overflow, noValue(), 0, m_out.extractValue(overflowResult, 1));
                result = m_out.extractValue(overflowResult, 0);
                speculate(NegativeZero, noValue(), 0, m_out.isZero32(result));
            }
            
            setInt32(result);
            break;
        }
            
        case NumberUse: {
            setDouble(m_out.doubleNeg(lowDouble(m_node->child1())));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileBitAnd()
    {
        setInt32(m_out.bitAnd(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitOr()
    {
        setInt32(m_out.bitOr(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitXor()
    {
        setInt32(m_out.bitXor(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitRShift()
    {
        setInt32(m_out.aShr(
            lowInt32(m_node->child1()),
            m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileBitLShift()
    {
        setInt32(m_out.shl(
            lowInt32(m_node->child1()),
            m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileBitURShift()
    {
        setInt32(m_out.lShr(
            lowInt32(m_node->child1()),
            m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileUInt32ToNumber()
    {
        LValue value = lowInt32(m_node->child1());

        if (!nodeCanSpeculateInt32(m_node->arithNodeFlags())) {
            setDouble(m_out.unsignedToDouble(value));
            return;
        }
        
        speculateForward(
            Overflow, noValue(), 0, m_out.lessThan(value, m_out.int32Zero),
            FormattedValue(ValueFormatUInt32, value));
        setInt32(value);
    }
    
    void compileInt32ToDouble()
    {
        // This node is tricky to compile in the DFG backend because it tries to
        // avoid converting child1 to a double in-place, as that would make subsequent
        // int uses of of child1 fail. But the FTL needs no such special magic, since
        // unlike the DFG backend, the FTL allows each node to have multiple
        // contemporaneous low-level representations. So, this gives child1 a double
        // representation and then forwards that representation to m_node.
        
        setDouble(lowDouble(m_node->child1()));
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
    
    void compileArrayifyToStructure()
    {
        LValue cell = lowCell(m_node->child1());
        LValue property = !!m_node->child2() ? lowInt32(m_node->child2()) : 0;
        
        LBasicBlock unexpectedStructure = FTL_NEW_BLOCK(m_out, ("ArrayifyToStructure unexpected structure"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArrayifyToStructure continuation"));
        
        LValue structure = m_out.loadPtr(cell, m_heaps.JSCell_structure);
        
        m_out.branch(
            m_out.notEqual(structure, weakPointer(m_node->structure())),
            unexpectedStructure, continuation);
        
        LBasicBlock lastNext = m_out.appendTo(unexpectedStructure, continuation);
        
        if (property) {
            switch (m_node->arrayMode().type()) {
            case Array::Int32:
            case Array::Double:
            case Array::Contiguous:
                speculate(
                    Uncountable, noValue(), 0,
                    m_out.aboveOrEqual(property, m_out.constInt32(MIN_SPARSE_ARRAY_INDEX)));
                break;
            default:
                break;
            }
        }
        
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
            vmCall(m_out.operation(operationEnsureInt32), m_callFrame, cell);
            break;
        case Array::Double:
            vmCall(m_out.operation(operationEnsureDouble), m_callFrame, cell);
            break;
        case Array::Contiguous:
            if (m_node->arrayMode().conversion() == Array::RageConvert)
                vmCall(m_out.operation(operationRageEnsureContiguous), m_callFrame, cell);
            else
                vmCall(m_out.operation(operationEnsureContiguous), m_callFrame, cell);
            break;
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            vmCall(m_out.operation(operationEnsureArrayStorage), m_callFrame, cell);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        structure = m_out.loadPtr(cell, m_heaps.JSCell_structure);
        speculate(
            BadIndexingType, jsValueValue(cell), 0,
            m_out.notEqual(structure, weakPointer(m_node->structure())));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compilePutStructure()
    {
        m_ftlState.jitCode->common.notifyCompilingStructureTransition(m_graph.m_plan, codeBlock(), m_node);
        
        m_out.store64(
            m_out.constIntPtr(m_node->structureTransitionData().newStructure),
            lowCell(m_node->child1()), m_heaps.JSCell_structure);
    }
    
    void compilePhantomPutStructure()
    {
        m_ftlState.jitCode->common.notifyCompilingStructureTransition(m_graph.m_plan, codeBlock(), m_node);
    }
    
    void compileGetButterfly()
    {
        setStorage(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSObject_butterfly));
    }
    
    void compileGetIndexedPropertyStorage()
    {
        setStorage(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSArrayBufferView_vector));
    }
    
    void compileCheckArray()
    {
        Edge edge = m_node->child1();
        LValue cell = lowCell(edge);
        
        if (m_node->arrayMode().alreadyChecked(m_graph, m_node, m_state.forNode(edge)))
            return;
        
        speculate(
            BadIndexingType, jsValueValue(cell), 0,
            m_out.bitNot(isArrayType(cell, m_node->arrayMode())));
    }
    
    void compileGetArrayLength()
    {
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            setInt32(m_out.load32(lowStorage(m_node->child2()), m_heaps.Butterfly_publicLength));
            return;
        }
            
        default:
            if (isTypedView(m_node->arrayMode().typedArrayType())) {
                setInt32(
                    m_out.load32(lowCell(m_node->child1()), m_heaps.JSArrayBufferView_length));
                return;
            }
            
            RELEASE_ASSERT_NOT_REACHED();
            return;
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
                setJSValue(result);
                return;
            }
            
            // FIXME: Implement hole/OOB loads in the FTL.
            // https://bugs.webkit.org/show_bug.cgi?id=118077
            RELEASE_ASSERT_NOT_REACHED();
            return;
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
                setDouble(result);
                break;
            }
            
            // FIXME: Implement hole/OOB loads in the FTL.
            // https://bugs.webkit.org/show_bug.cgi?id=118077
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
            
        default: {
            TypedArrayType type = m_node->arrayMode().typedArrayType();
            
            if (isTypedView(type)) {
                LValue array = lowCell(m_node->child1());
                
                speculate(
                    OutOfBounds, noValue(), 0,
                    m_out.aboveOrEqual(
                        index, m_out.load32(array, m_heaps.JSArrayBufferView_length)));
                
                TypedPointer pointer = TypedPointer(
                    m_heaps.typedArrayProperties,
                    m_out.add(
                        storage,
                        m_out.shl(
                            m_out.zeroExt(index, m_out.intPtr),
                            m_out.constIntPtr(logElementSize(type)))));
                
                if (isInt(type)) {
                    LValue result;
                    switch (elementSize(type)) {
                    case 1:
                        result = m_out.load8(pointer);
                        break;
                    case 2:
                        result = m_out.load16(pointer);
                        break;
                    case 4:
                        result = m_out.load32(pointer);
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                    
                    if (elementSize(type) < 4) {
                        if (isSigned(type))
                            result = m_out.signExt(result, m_out.int32);
                        else
                            result = m_out.zeroExt(result, m_out.int32);
                        setInt32(result);
                        return;
                    }
                    
                    if (isSigned(type)) {
                        setInt32(result);
                        return;
                    }
                    
                    if (m_node->shouldSpeculateInt32()) {
                        speculateForward(
                            Overflow, noValue(), 0, m_out.lessThan(result, m_out.int32Zero),
                            uInt32Value(result));
                        setInt32(result);
                        return;
                    }
                    
                    setDouble(m_out.unsignedToFP(result, m_out.doubleType));
                    return;
                }
            
                ASSERT(isFloat(type));
                
                LValue result;
                switch (type) {
                case TypeFloat32:
                    result = m_out.fpCast(m_out.loadFloat(pointer), m_out.doubleType);
                    break;
                case TypeFloat64:
                    result = m_out.loadDouble(pointer);
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
                
                result = m_out.select(
                    m_out.doubleEqual(result, result), result, m_out.constDouble(QNaN));
                setDouble(result);
                return;
            }
            
            RELEASE_ASSERT_NOT_REACHED();
            return;
        } }
    }
    
    void compilePutByVal()
    {
        Edge child1 = m_graph.varArgChild(m_node, 0);
        Edge child2 = m_graph.varArgChild(m_node, 1);
        Edge child3 = m_graph.varArgChild(m_node, 2);
        Edge child4 = m_graph.varArgChild(m_node, 3);

        LValue base = lowCell(child1);
        LValue index = lowInt32(child2);
        LValue storage = lowStorage(child4);
        
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("PutByVal continuation"));
            LBasicBlock outerLastNext = m_out.appendTo(m_out.m_block, continuation);
            
            switch (m_node->arrayMode().type()) {
            case Array::Int32:
            case Array::Contiguous: {
                LValue value = lowJSValue(child3, ManualOperandSpeculation);
                
                if (m_node->arrayMode().type() == Array::Int32)
                    FTL_TYPE_CHECK(jsValueValue(value), child3, SpecInt32, isNotInt32(value));
                
                TypedPointer elementPointer = m_out.baseIndex(
                    m_node->arrayMode().type() == Array::Int32 ?
                    m_heaps.indexedInt32Properties : m_heaps.indexedContiguousProperties,
                    storage, m_out.zeroExt(index, m_out.intPtr),
                    m_state.forNode(child2).m_value);
                
                if (m_node->op() == PutByValAlias) {
                    m_out.store64(value, elementPointer);
                    break;
                }
                
                contiguousPutByValOutOfBounds(
                    codeBlock()->isStrictMode()
                    ? operationPutByValBeyondArrayBoundsStrict
                    : operationPutByValBeyondArrayBoundsNonStrict,
                    base, storage, index, value, continuation);
                
                m_out.store64(value, elementPointer);
                break;
            }
                
            case Array::Double: {
                LValue value = lowDouble(child3);
                
                FTL_TYPE_CHECK(
                    doubleValue(value), child3, SpecRealNumber,
                    m_out.doubleNotEqualOrUnordered(value, value));
                
                TypedPointer elementPointer = m_out.baseIndex(
                    m_heaps.indexedDoubleProperties,
                    storage, m_out.zeroExt(index, m_out.intPtr),
                    m_state.forNode(child2).m_value);
                
                if (m_node->op() == PutByValAlias) {
                    m_out.storeDouble(value, elementPointer);
                    break;
                }
                
                contiguousPutByValOutOfBounds(
                    codeBlock()->isStrictMode()
                    ? operationPutDoubleByValBeyondArrayBoundsStrict
                    : operationPutDoubleByValBeyondArrayBoundsNonStrict,
                    base, storage, index, value, continuation);
                
                m_out.storeDouble(value, elementPointer);
                break;
            }
                
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            m_out.jump(continuation);
            m_out.appendTo(continuation, outerLastNext);
            return;
        }
            
        default:
            TypedArrayType type = m_node->arrayMode().typedArrayType();
            
            if (isTypedView(type)) {
                if (m_node->op() == PutByVal) {
                    speculate(
                        OutOfBounds, noValue(), 0,
                        m_out.aboveOrEqual(
                            index, m_out.load32(base, m_heaps.JSArrayBufferView_length)));
                }
                
                TypedPointer pointer = TypedPointer(
                    m_heaps.typedArrayProperties,
                    m_out.add(
                        storage,
                        m_out.shl(
                            m_out.zeroExt(index, m_out.intPtr),
                            m_out.constIntPtr(logElementSize(type)))));
                
                if (isInt(type)) {
                    LValue intValue;
                    switch (child3.useKind()) {
                    case Int32Use: {
                        intValue = lowInt32(child3);
                        if (isClamped(type)) {
                            ASSERT(elementSize(type) == 1);
                            
                            LBasicBlock atLeastZero = FTL_NEW_BLOCK(m_out, ("PutByVal int clamp atLeastZero"));
                            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("PutByVal int clamp continuation"));
                            
                            Vector<ValueFromBlock, 2> intValues;
                            intValues.append(m_out.anchor(m_out.int32Zero));
                            m_out.branch(
                                m_out.lessThan(intValue, m_out.int32Zero),
                                continuation, atLeastZero);
                            
                            LBasicBlock lastNext = m_out.appendTo(atLeastZero, continuation);
                            
                            intValues.append(m_out.anchor(m_out.select(
                                m_out.greaterThan(intValue, m_out.constInt32(255)),
                                m_out.constInt32(255),
                                intValue)));
                            m_out.jump(continuation);
                            
                            m_out.appendTo(continuation, lastNext);
                            intValue = m_out.phi(m_out.int32, intValues);
                        }
                        break;
                    }
                        
                    case NumberUse: {
                        LValue doubleValue = lowDouble(child3);
                        
                        if (isClamped(type)) {
                            ASSERT(elementSize(type) == 1);
                            
                            LBasicBlock atLeastZero = FTL_NEW_BLOCK(m_out, ("PutByVal double clamp atLeastZero"));
                            LBasicBlock withinRange = FTL_NEW_BLOCK(m_out, ("PutByVal double clamp withinRange"));
                            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("PutByVal double clamp continuation"));
                            
                            Vector<ValueFromBlock, 3> intValues;
                            intValues.append(m_out.anchor(m_out.int32Zero));
                            m_out.branch(
                                m_out.doubleLessThanOrUnordered(doubleValue, m_out.doubleZero),
                                continuation, atLeastZero);
                            
                            LBasicBlock lastNext = m_out.appendTo(atLeastZero, withinRange);
                            intValues.append(m_out.anchor(m_out.constInt32(255)));
                            m_out.branch(
                                m_out.doubleGreaterThan(doubleValue, m_out.constDouble(255)),
                                continuation, withinRange);
                            
                            m_out.appendTo(withinRange, continuation);
                            intValues.append(m_out.anchor(m_out.fpToInt32(doubleValue)));
                            m_out.jump(continuation);
                            
                            m_out.appendTo(continuation, lastNext);
                            intValue = m_out.phi(m_out.int32, intValues);
                        } else if (isSigned(type))
                            intValue = doubleToInt32(doubleValue);
                        else
                            intValue = doubleToUInt32(doubleValue);
                        break;
                    }
                        
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                    
                    switch (elementSize(type)) {
                    case 1:
                        m_out.store8(m_out.intCast(intValue, m_out.int8), pointer);
                        break;
                    case 2:
                        m_out.store16(m_out.intCast(intValue, m_out.int16), pointer);
                        break;
                    case 4:
                        m_out.store32(intValue, pointer);
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                    
                    return;
                }
                
                ASSERT(isFloat(type));
                
                LValue value = lowDouble(child3);
                switch (type) {
                case TypeFloat32:
                    m_out.storeFloat(m_out.fpCast(value, m_out.floatType), pointer);
                    break;
                case TypeFloat64:
                    m_out.storeDouble(value, pointer);
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
                return;
            }
            
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileGetByOffset()
    {
        StorageAccessData& data =
            m_graph.m_storageAccessData[m_node->storageAccessDataIndex()];
        
        setJSValue(
            m_out.load64(
                m_out.address(
                    m_heaps.properties[data.identifierNumber],
                    lowStorage(m_node->child1()),
                    offsetRelativeToBase(data.offset))));
    }
    
    void compilePutByOffset()
    {
        StorageAccessData& data =
            m_graph.m_storageAccessData[m_node->storageAccessDataIndex()];
        
        m_out.store64(
            lowJSValue(m_node->child3()),
            m_out.address(
                m_heaps.properties[data.identifierNumber],
                lowStorage(m_node->child1()),
                offsetRelativeToBase(data.offset)));
    }
    
    void compileGetGlobalVar()
    {
        setJSValue(m_out.load64(m_out.absolute(m_node->registerPointer())));
    }
    
    void compilePutGlobalVar()
    {
        m_out.store64(
            lowJSValue(m_node->child1()), m_out.absolute(m_node->registerPointer()));
    }
    
    void compileGlobalVarWatchpoint()
    {
        // FIXME: Implement watchpoints.
        // https://bugs.webkit.org/show_bug.cgi?id=113647
    }
    
    void compileGetMyScope()
    {
        setJSValue(m_out.loadPtr(addressFor(
            m_node->codeOrigin.stackOffset() + JSStack::ScopeChain)));
    }
    
    void compileSkipScope()
    {
        setJSValue(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSScope_next));
    }
    
    void compileGetClosureRegisters()
    {
        setStorage(m_out.loadPtr(
            lowCell(m_node->child1()), m_heaps.JSVariableObject_registers));
    }
    
    void compileGetClosureVar()
    {
        setJSValue(m_out.load64(
            addressFor(lowStorage(m_node->child1()), m_node->varNumber())));
    }
    
    void compilePutClosureVar()
    {
        m_out.store64(
            lowJSValue(m_node->child3()),
            addressFor(lowStorage(m_node->child2()), m_node->varNumber()));
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
        setBoolean(
            equalNullOrUndefined(
                m_node->child1(), AllCellsAreFalse, EqualNullOrUndefined));
    }
    
    void compileCompareStrictEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            setBoolean(
                m_out.equal(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            setBoolean(
                m_out.doubleEqual(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(ObjectUse)) {
            masqueradesAsUndefinedWatchpointIfIsStillValid();
            setBoolean(
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
                setBoolean(equalNullOrUndefined(m_node->child1(), AllCellsAreFalse, EqualNull));
                return;
            }
        
            ASSERT(constant.isUndefined());
            setBoolean(equalNullOrUndefined(m_node->child1(), AllCellsAreFalse, EqualUndefined));
            return;
        }
        
        setBoolean(
            m_out.equal(
                lowJSValue(m_node->child1()),
                m_out.constInt64(JSValue::encode(constant))));
    }
    
    void compileCompareLess()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            setBoolean(
                m_out.lessThan(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            setBoolean(
                m_out.doubleLessThan(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareLessEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            setBoolean(
                m_out.lessThanOrEqual(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            setBoolean(
                m_out.doubleLessThanOrEqual(
                    lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareGreater()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            setBoolean(
                m_out.greaterThan(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            setBoolean(
                m_out.doubleGreaterThan(
                    lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareGreaterEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            setBoolean(
                m_out.greaterThanOrEqual(
                    lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            setBoolean(
                m_out.doubleGreaterThanOrEqual(
                    lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileLogicalNot()
    {
        setBoolean(m_out.bitNot(boolify(m_node->child1())));
    }
    
    void compileCallOrConstruct()
    {
        // FIXME: This is unacceptably slow.
        // https://bugs.webkit.org/show_bug.cgi?id=113621
        
        J_DFGOperation_E function =
            m_node->op() == Call ? operationFTLCall : operationFTLConstruct;
        
        int dummyThisArgument = m_node->op() == Call ? 0 : 1;
        
        int numPassedArgs = m_node->numChildren() - 1;
        
        LValue calleeFrame = m_out.add(
            m_callFrame,
            m_out.constIntPtr(sizeof(Register) * codeBlock()->m_numCalleeRegisters));
        
        m_out.store32(
            m_out.constInt32(numPassedArgs + dummyThisArgument),
            payloadFor(calleeFrame, JSStack::ArgumentCount));
        m_out.store64(m_callFrame, addressFor(calleeFrame, JSStack::CallerFrame));
        m_out.store64(
            lowJSValue(m_graph.varArgChild(m_node, 0)),
            addressFor(calleeFrame, JSStack::Callee));
        
        for (int i = 0; i < numPassedArgs; ++i) {
            m_out.store64(
                lowJSValue(m_graph.varArgChild(m_node, 1 + i)),
                addressFor(calleeFrame, argumentToOperand(i + dummyThisArgument)));
        }
        
        setJSValue(vmCall(m_out.operation(function), calleeFrame));
    }
    
    void compileJump()
    {
        m_out.jump(lowBlock(m_node->takenBlock()));
    }
    
    void compileBranch()
    {
        m_out.branch(
            boolify(m_node->child1()),
            lowBlock(m_node->takenBlock()),
            lowBlock(m_node->notTakenBlock()));
    }
    
    void compileSwitch()
    {
        SwitchData* data = m_node->switchData();
        switch (data->kind) {
        case SwitchImm: {
            Vector<ValueFromBlock, 2> intValues;
            LBasicBlock switchOnInts = FTL_NEW_BLOCK(m_out, ("Switch/SwitchImm int case"));
            
            LBasicBlock lastNext = m_out.appendTo(m_out.m_block, switchOnInts);
            
            switch (m_node->child1().useKind()) {
            case Int32Use: {
                intValues.append(m_out.anchor(lowInt32(m_node->child1())));
                m_out.jump(switchOnInts);
                break;
            }
                
            case UntypedUse: {
                LBasicBlock isInt = FTL_NEW_BLOCK(m_out, ("Switch/SwitchImm is int"));
                LBasicBlock isNotInt = FTL_NEW_BLOCK(m_out, ("Switch/SwitchImm is not int"));
                LBasicBlock isDouble = FTL_NEW_BLOCK(m_out, ("Switch/SwitchImm is double"));
                
                LValue boxedValue = lowJSValue(m_node->child1());
                m_out.branch(isNotInt32(boxedValue), isNotInt, isInt);
                
                LBasicBlock innerLastNext = m_out.appendTo(isInt, isNotInt);
                
                intValues.append(m_out.anchor(unboxInt32(boxedValue)));
                m_out.jump(switchOnInts);
                
                m_out.appendTo(isNotInt, isDouble);
                m_out.branch(
                    isCellOrMisc(boxedValue), lowBlock(data->fallThrough), isDouble);
                
                m_out.appendTo(isDouble, innerLastNext);
                LValue doubleValue = unboxDouble(boxedValue);
                LValue intInDouble = m_out.fpToInt32(doubleValue);
                intValues.append(m_out.anchor(intInDouble));
                m_out.branch(
                    m_out.doubleEqual(m_out.intToDouble(intInDouble), doubleValue),
                    switchOnInts, lowBlock(data->fallThrough));
                break;
            }
                
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            
            m_out.appendTo(switchOnInts, lastNext);
            buildSwitch(data, m_out.int32, m_out.phi(m_out.int32, intValues));
            return;
        }
        
        case SwitchChar: {
            LValue stringValue;
            
            switch (m_node->child1().useKind()) {
            case StringUse: {
                stringValue = lowString(m_node->child1());
                break;
            }
                
            case UntypedUse: {
                LValue unboxedValue = lowJSValue(m_node->child1());
                
                LBasicBlock isCellCase = FTL_NEW_BLOCK(m_out, ("Switch/SwitchChar is cell"));
                LBasicBlock isStringCase = FTL_NEW_BLOCK(m_out, ("Switch/SwitchChar is string"));
                
                m_out.branch(
                    isNotCell(unboxedValue), lowBlock(data->fallThrough), isCellCase);
                
                LBasicBlock lastNext = m_out.appendTo(isCellCase, isStringCase);
                LValue cellValue = unboxedValue;
                m_out.branch(isNotString(cellValue), lowBlock(data->fallThrough), isStringCase);
                
                m_out.appendTo(isStringCase, lastNext);
                stringValue = cellValue;
                break;
            }
                
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            
            LBasicBlock lengthIs1 = FTL_NEW_BLOCK(m_out, ("Switch/SwitchChar length is 1"));
            LBasicBlock needResolution = FTL_NEW_BLOCK(m_out, ("Switch/SwitchChar resolution"));
            LBasicBlock resolved = FTL_NEW_BLOCK(m_out, ("Switch/SwitchChar resolved"));
            LBasicBlock is8Bit = FTL_NEW_BLOCK(m_out, ("Switch/SwitchChar 8bit"));
            LBasicBlock is16Bit = FTL_NEW_BLOCK(m_out, ("Switch/SwitchChar 16bit"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Switch/SwitchChar continuation"));
            
            m_out.branch(
                m_out.notEqual(
                    m_out.load32(stringValue, m_heaps.JSString_length),
                    m_out.int32One),
                lowBlock(data->fallThrough), lengthIs1);
            
            LBasicBlock lastNext = m_out.appendTo(lengthIs1, needResolution);
            Vector<ValueFromBlock, 2> values;
            LValue fastValue = m_out.loadPtr(stringValue, m_heaps.JSString_value);
            values.append(m_out.anchor(fastValue));
            m_out.branch(m_out.isNull(fastValue), needResolution, resolved);
            
            m_out.appendTo(needResolution, resolved);
            values.append(m_out.anchor(
                vmCall(m_out.operation(operationResolveRope), m_callFrame, stringValue)));
            m_out.jump(resolved);
            
            m_out.appendTo(resolved, is8Bit);
            LValue value = m_out.phi(m_out.intPtr, values);
            LValue characterData = m_out.loadPtr(value, m_heaps.StringImpl_data);
            m_out.branch(
                m_out.testNonZero32(
                    m_out.load32(value, m_heaps.StringImpl_hashAndFlags),
                    m_out.constInt32(StringImpl::flagIs8Bit())),
                is8Bit, is16Bit);
            
            Vector<ValueFromBlock, 2> characters;
            m_out.appendTo(is8Bit, is16Bit);
            characters.append(m_out.anchor(
                m_out.zeroExt(m_out.load8(characterData, m_heaps.characters8[0]), m_out.int16)));
            m_out.jump(continuation);
            
            m_out.appendTo(is16Bit, continuation);
            characters.append(m_out.anchor(m_out.load16(characterData, m_heaps.characters16[0])));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            buildSwitch(data, m_out.int16, m_out.phi(m_out.int16, characters));
            return;
        }
        
        case SwitchString:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
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
        bool validWatchpoint = masqueradesAsUndefinedWatchpointIfIsStillValid();
        
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
    
    template<typename FunctionType>
    void contiguousPutByValOutOfBounds(
        FunctionType slowPathFunction,
        LValue base, LValue storage, LValue index, LValue value,
        LBasicBlock continuation)
    {
        LValue isNotInBounds = m_out.aboveOrEqual(
            index, m_out.load32(storage, m_heaps.Butterfly_publicLength));
        if (m_node->arrayMode().isInBounds())
            speculate(StoreToHoleOrOutOfBounds, noValue(), 0, isNotInBounds);
        else {
            LBasicBlock notInBoundsCase =
                FTL_NEW_BLOCK(m_out, ("PutByVal not in bounds"));
            LBasicBlock performStore =
                FTL_NEW_BLOCK(m_out, ("PutByVal perform store"));
                
            m_out.branch(isNotInBounds, notInBoundsCase, performStore);
                
            LBasicBlock lastNext = m_out.appendTo(notInBoundsCase, performStore);
                
            LValue isOutOfBounds = m_out.aboveOrEqual(
                index, m_out.load32(storage, m_heaps.Butterfly_vectorLength));
                
            if (!m_node->arrayMode().isOutOfBounds())
                speculate(OutOfBounds, noValue(), 0, isOutOfBounds);
            else {
                LBasicBlock outOfBoundsCase =
                    FTL_NEW_BLOCK(m_out, ("PutByVal out of bounds"));
                LBasicBlock holeCase =
                    FTL_NEW_BLOCK(m_out, ("PutByVal hole case"));
                    
                m_out.branch(isOutOfBounds, outOfBoundsCase, holeCase);
                    
                LBasicBlock innerLastNext = m_out.appendTo(outOfBoundsCase, holeCase);
                    
                vmCall(
                    m_out.operation(slowPathFunction),
                    m_callFrame, base, index, value);
                    
                m_out.jump(continuation);
                    
                m_out.appendTo(holeCase, innerLastNext);
            }
            
            m_out.store32(
                m_out.add(index, m_out.int32One),
                storage, m_heaps.Butterfly_publicLength);
                
            m_out.jump(performStore);
            m_out.appendTo(performStore, lastNext);
        }
    }
    
    void buildSwitch(SwitchData* data, LType type, LValue switchValue)
    {
        Vector<SwitchCase> cases;
        for (unsigned i = 0; i < data->cases.size(); ++i) {
            cases.append(SwitchCase(
                constInt(type, data->cases[i].value.switchLookupValue()),
                lowBlock(data->cases[i].target)));
        }
        
        m_out.switchInstruction(switchValue, cases, lowBlock(data->fallThrough));
    }
    
    LValue doubleToInt32(LValue doubleValue, double low, double high, bool isSigned = true)
    {
        // FIXME: Optimize double-to-int conversions.
        // <rdar://problem/14938465>
        
        LBasicBlock greatEnough = FTL_NEW_BLOCK(m_out, ("doubleToInt32 greatEnough"));
        LBasicBlock withinRange = FTL_NEW_BLOCK(m_out, ("doubleToInt32 withinRange"));
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("doubleToInt32 slowPath"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("doubleToInt32 continuation"));
        
        Vector<ValueFromBlock, 2> results;
        
        m_out.branch(
            m_out.greaterThanOrEqual(doubleValue, m_out.constDouble(low)),
            greatEnough, slowPath);
        
        LBasicBlock lastNext = m_out.appendTo(greatEnough, withinRange);
        m_out.branch(
            m_out.lessThanOrEqual(doubleValue, m_out.constDouble(high)),
            withinRange, slowPath);
        
        m_out.appendTo(withinRange, slowPath);
        LValue fastResult;
        if (isSigned)
            fastResult = m_out.fpToInt32(doubleValue);
        else
            fastResult = m_out.fpToUInt32(doubleValue);
        results.append(m_out.anchor(fastResult));
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        results.append(m_out.anchor(m_out.call(m_out.operation(toInt32), doubleValue)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(m_out.int32, results);
    }
    
    LValue doubleToInt32(LValue doubleValue)
    {
        double limit = pow(2, 31) - 1;
        return doubleToInt32(doubleValue, -limit, limit);
    }
    
    LValue doubleToUInt32(LValue doubleValue)
    {
        return doubleToInt32(doubleValue, 0, pow(2, 32) - 1, false);
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
        if (!m_interpreter.needsTypeCheck(highValue, typesPassedThrough))
            return;
        ASSERT(mayHaveTypeCheck(highValue.useKind()));
        appendOSRExit(BadType, lowValue, highValue.node(), failCondition, direction, recovery);
        m_interpreter.filter(highValue, typesPassedThrough);
    }
    
    LValue lowInt32(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || (edge.useKind() == Int32Use || edge.useKind() == KnownInt32Use));
        
        LoweredNodeValue value = m_int32Values.get(edge.node());
        if (isValid(value))
            return value.value();
        
        value = m_jsValueValues.get(edge.node());
        if (isValid(value)) {
            LValue boxedResult = value.value();
            FTL_TYPE_CHECK(
                jsValueValue(boxedResult), edge, SpecInt32, isNotInt32(boxedResult));
            LValue result = unboxInt32(boxedResult);
            setInt32(edge.node(), result);
            return result;
        }

        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecInt32));
        terminate(Uncountable);
        return m_out.int32Zero;
    }
    
    LValue lowCell(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || isCell(edge.useKind()));
        
        LoweredNodeValue value = m_jsValueValues.get(edge.node());
        if (isValid(value)) {
            LValue uncheckedValue = value.value();
            FTL_TYPE_CHECK(
                jsValueValue(uncheckedValue), edge, SpecCell, isNotCell(uncheckedValue));
            return uncheckedValue;
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
    
    LValue lowString(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == StringUse || edge.useKind() == KnownStringUse);
        
        LValue result = lowCell(edge, mode);
        speculateString(edge, result);
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
        
        LoweredNodeValue value = m_booleanValues.get(edge.node());
        if (isValid(value))
            return value.value();
        
        value = m_jsValueValues.get(edge.node());
        if (isValid(value)) {
            LValue unboxedResult = value.value();
            FTL_TYPE_CHECK(
                jsValueValue(unboxedResult), edge, SpecBoolean, isNotBoolean(unboxedResult));
            LValue result = unboxBoolean(unboxedResult);
            setBoolean(edge.node(), result);
            return result;
        }
        
        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecBoolean));
        terminate(Uncountable);
        return m_out.booleanFalse;
    }
    
    LValue lowDouble(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || isDouble(edge.useKind()));
        
        LoweredNodeValue value = m_doubleValues.get(edge.node());
        if (isValid(value))
            return value.value();
        
        value = m_int32Values.get(edge.node());
        if (isValid(value)) {
            LValue result = m_out.intToDouble(value.value());
            setDouble(edge.node(), result);
            return result;
        }
        
        value = m_jsValueValues.get(edge.node());
        if (isValid(value)) {
            LValue boxedResult = value.value();
            
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
            
            setDouble(edge.node(), result);
            return result;
        }
        
        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecNumber));
        terminate(Uncountable);
        return m_out.doubleZero;
    }
    
    LValue lowJSValue(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == UntypedUse);
        
        LoweredNodeValue value = m_jsValueValues.get(edge.node());
        if (isValid(value))
            return value.value();
        
        value = m_int32Values.get(edge.node());
        if (isValid(value)) {
            LValue result = boxInt32(value.value());
            setJSValue(edge.node(), result);
            return result;
        }
        
        value = m_booleanValues.get(edge.node());
        if (isValid(value)) {
            LValue result = boxBoolean(value.value());
            setJSValue(edge.node(), result);
            return result;
        }
        
        value = m_doubleValues.get(edge.node());
        if (isValid(value)) {
            LValue result = boxDouble(value.value());
            setJSValue(edge.node(), result);
            return result;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
    
    LValue lowStorage(Edge edge)
    {
        LoweredNodeValue value = m_storageValues.get(edge.node());
        if (isValid(value))
            return value.value();
        
        LValue result = lowCell(edge);
        setStorage(edge.node(), result);
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
            ASSERT(!m_interpreter.needsTypeCheck(edge));
            break;
        case Int32Use:
            speculateInt32(edge);
            break;
        case CellUse:
            speculateCell(edge);
            break;
        case KnownCellUse:
            ASSERT(!m_interpreter.needsTypeCheck(edge));
            break;
        case ObjectUse:
            speculateObject(edge);
            break;
        case ObjectOrOtherUse:
            speculateObjectOrOther(edge);
            break;
        case StringUse:
            speculateString(edge);
            break;
        case RealNumberUse:
            speculateRealNumber(edge);
            break;
        case NumberUse:
            speculateNumber(edge);
            break;
        case BooleanUse:
            speculateBoolean(edge);
            break;
        default:
            dataLog("Unsupported speculation use kind: ", edge.useKind(), "\n");
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
    
    LValue isObject(LValue cell)
    {
        return m_out.notEqual(
            m_out.loadPtr(cell, m_heaps.JSCell_structure),
            m_out.constIntPtr(vm().stringStructure.get()));
    }
    
    LValue isNotString(LValue cell)
    {
        return isObject(cell);
    }
    
    LValue isString(LValue cell)
    {
        return m_out.equal(
            m_out.loadPtr(cell, m_heaps.JSCell_structure),
            m_out.constIntPtr(vm().stringStructure.get()));
    }
    
    LValue isNotObject(LValue cell)
    {
        return isString(cell);
    }
    
    LValue isArrayType(LValue cell, ArrayMode arrayMode)
    {
        switch (arrayMode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            LValue indexingType = m_out.load8(
                m_out.loadPtr(cell, m_heaps.JSCell_structure),
                m_heaps.Structure_indexingType);
            
            switch (arrayMode.arrayClass()) {
            case Array::OriginalArray:
                RELEASE_ASSERT_NOT_REACHED();
                return 0;
                
            case Array::Array:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt8(IsArray | IndexingShapeMask)),
                    m_out.constInt8(IsArray | arrayMode.shapeMask()));
                
            default:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt8(IndexingShapeMask)),
                    m_out.constInt8(arrayMode.shapeMask()));
            }
        }
            
        default:
            return hasClassInfo(cell, classInfoForType(arrayMode.typedArrayType()));
        }
    }
    
    LValue hasClassInfo(LValue cell, const ClassInfo* classInfo)
    {
        return m_out.equal(
            m_out.loadPtr(
                m_out.loadPtr(cell, m_heaps.JSCell_structure),
                m_heaps.Structure_classInfo),
            m_out.constIntPtr(classInfo));
    }
    
    void speculateObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecObject, isNotObject(cell));
    }
    
    void speculateObject(Edge edge)
    {
        speculateObject(edge, lowCell(edge));
    }
    
    void speculateObjectOrOther(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowJSValue(edge);
        
        LBasicBlock cellCase = FTL_NEW_BLOCK(m_out, ("speculateObjectOrOther cell case"));
        LBasicBlock primitiveCase = FTL_NEW_BLOCK(m_out, ("speculateObjectOrOther primitive case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("speculateObjectOrOther continuation"));
        
        m_out.branch(isNotCell(value), primitiveCase, cellCase);
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, primitiveCase);
        
        FTL_TYPE_CHECK(
            jsValueValue(value), edge, (~SpecCell) | SpecObject,
            m_out.equal(
                m_out.loadPtr(value, m_heaps.JSCell_structure),
                m_out.constIntPtr(vm().stringStructure.get())));
        
        m_out.jump(continuation);
        
        m_out.appendTo(primitiveCase, continuation);
        
        FTL_TYPE_CHECK(
            jsValueValue(value), edge, SpecCell | SpecOther,
            m_out.notEqual(
                m_out.bitAnd(value, m_out.constInt64(~TagBitUndefined)),
                m_out.constInt64(ValueNull)));
        
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void speculateString(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecString, isNotString(cell));
    }
    
    void speculateString(Edge edge)
    {
        speculateString(edge, lowCell(edge));
    }
    
    void speculateNonNullObject(Edge edge, LValue cell)
    {
        LValue structure = m_out.loadPtr(cell, m_heaps.JSCell_structure);
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecObject, 
            m_out.equal(structure, m_out.constIntPtr(vm().stringStructure.get())));
        if (masqueradesAsUndefinedWatchpointIfIsStillValid())
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
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        lowDouble(edge);
    }
    
    void speculateRealNumber(Edge edge)
    {
        // Do an early return here because lowDouble() can create a lot of control flow.
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowDouble(edge);
        FTL_TYPE_CHECK(
            doubleValue(value), edge, SpecRealNumber,
            m_out.doubleNotEqualOrUnordered(value, value));
    }
    
    void speculateBoolean(Edge edge)
    {
        lowBoolean(edge);
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
    
    enum ExceptionCheckMode { NoExceptions, CheckExceptions };
    
    LValue vmCall(LValue function, ExceptionCheckMode mode = CheckExceptions)
    {
        callPreflight();
        LValue result = m_out.call(function);
        callCheck(mode);
        return result;
    }
    LValue vmCall(LValue function, LValue arg1, ExceptionCheckMode mode = CheckExceptions)
    {
        callPreflight();
        LValue result = m_out.call(function, arg1);
        callCheck(mode);
        return result;
    }
    LValue vmCall(LValue function, LValue arg1, LValue arg2, ExceptionCheckMode mode = CheckExceptions)
    {
        callPreflight();
        LValue result = m_out.call(function, arg1, arg2);
        callCheck(mode);
        return result;
    }
    LValue vmCall(LValue function, LValue arg1, LValue arg2, LValue arg3, ExceptionCheckMode mode = CheckExceptions)
    {
        callPreflight();
        LValue result = m_out.call(function, arg1, arg2, arg3);
        callCheck(mode);
        return result;
    }
    LValue vmCall(LValue function, LValue arg1, LValue arg2, LValue arg3, LValue arg4, ExceptionCheckMode mode = CheckExceptions)
    {
        callPreflight();
        LValue result = m_out.call(function, arg1, arg2, arg3, arg4);
        callCheck(mode);
        return result;
    }
    
    void callPreflight(CodeOrigin codeOrigin)
    {
        m_out.store32(
            m_out.constInt32(
                CallFrame::Location::encodeAsCodeOriginIndex(
                    codeBlock()->addCodeOrigin(codeOrigin))), 
            tagFor(JSStack::ArgumentCount));
    }
    void callPreflight()
    {
        callPreflight(m_node->codeOrigin);
    }
    
    void callCheck(ExceptionCheckMode mode = CheckExceptions)
    {
        if (mode == NoExceptions)
            return;
        
        LBasicBlock didHaveException = FTL_NEW_BLOCK(m_out, ("Did have exception"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Exception check continuation"));
        
        m_out.branch(
            m_out.notZero64(m_out.load64(m_out.absolute(vm().addressOfException()))),
            didHaveException, continuation);
        
        LBasicBlock lastNext = m_out.appendTo(didHaveException, continuation);
        // FIXME: Handle exceptions. https://bugs.webkit.org/show_bug.cgi?id=113622
        m_out.crash();
        
        m_out.appendTo(continuation, lastNext);
    }
    
    bool isLive(Node* node)
    {
        return m_live.contains(node);
    }
    
    void use(Edge edge)
    {
        ASSERT(edge->hasResult());
        if (!edge.doesKill())
            return;
        m_live.remove(edge.node());
    }
    
    // Wrapper used only for DFG_NODE_DO_TO_CHILDREN
    void use(Node*, Edge edge)
    {
        use(edge);
    }
    
    LBasicBlock lowBlock(BasicBlock* block)
    {
        return m_blocks.get(block);
    }
    
    void initializeOSRExitStateForBlock()
    {
        for (unsigned i = m_valueSources.size(); i--;) {
            FlushFormat format = m_highBlock->ssa->flushFormatAtHead[i];
            switch (format) {
            case DeadFlush: {
                // Must consider available nodes instead.
                Node* node = m_highBlock->ssa->availabilityAtHead[i];
                if (!node) {
                    m_valueSources[i] = ValueSource(SourceIsDead);
                    break;
                }
                
                m_valueSources[i] = ValueSource(node);
                break;
            }
                
            case FlushedInt32:
                m_valueSources[i] = ValueSource(Int32InJSStack);
                break;
                
            case FlushedDouble:
                m_valueSources[i] = ValueSource(DoubleInJSStack);
                break;
                
            case FlushedCell:
            case FlushedBoolean:
            case FlushedJSValue:
                m_valueSources[i] = ValueSource(ValueInJSStack);
                break;
            }
        }
    }
    
    void appendOSRExit(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition,
        SpeculationDirection direction, FormattedValue recovery)
    {
        if (Options::ftlTrapsOnOSRExit()) {
            LBasicBlock failCase = FTL_NEW_BLOCK(m_out, ("OSR exit failCase for ", m_node));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("OSR exit continuation for ", m_node));
            
            m_out.branch(failCondition, failCase, continuation);
            
            LBasicBlock lastNext = m_out.appendTo(failCase, continuation);
            m_out.trap();
            m_out.unreachable();
            
            m_out.appendTo(continuation, lastNext);
            return;
        }
        
        if (verboseCompilationEnabled())
            dataLog("    OSR exit with value sources: ", m_valueSources, "\n");
        
        ASSERT(m_ftlState.jitCode->osrExit.size() == m_ftlState.osrExit.size());
        unsigned index = m_ftlState.osrExit.size();
        
        m_ftlState.jitCode->osrExit.append(OSRExit(
            kind, lowValue.format(), m_graph.methodOfGettingAValueProfileFor(highValue),
            m_codeOriginForExitTarget, m_codeOriginForExitProfile, m_lastSetOperand,
            m_valueSources.numberOfArguments(), m_valueSources.numberOfLocals()));
        m_ftlState.osrExit.append(OSRExitCompilationInfo());
        
        OSRExit& exit = m_ftlState.jitCode->osrExit.last();
        OSRExitCompilationInfo& info = m_ftlState.osrExit.last();

        LBasicBlock lastNext = 0;
        LBasicBlock continuation = 0;
        
        if (!Options::useLLVMOSRExitIntrinsic()) {
            LBasicBlock failCase = FTL_NEW_BLOCK(m_out, ("OSR exit failCase for ", m_node));
            continuation = FTL_NEW_BLOCK(m_out, ("OSR exit continuation for ", m_node));
            
            m_out.branch(failCondition, failCase, continuation);

            m_out.appendTo(m_prologue);
            info.m_thunkAddress = buildAlloca(m_out.m_builder, m_out.intPtr);
        
            lastNext = m_out.appendTo(failCase, continuation);
        }
        
        if (Options::ftlOSRExitOmitsMarshalling()) {
            m_out.call(
                m_out.intToPtr(
                    m_out.get(info.m_thunkAddress),
                    pointerType(functionType(m_out.voidType))));
        } else
            emitOSRExitCall(failCondition, index, exit, info, lowValue, direction, recovery);
        
        if (!Options::useLLVMOSRExitIntrinsic()) {
            m_out.unreachable();
            
            m_out.appendTo(continuation, lastNext);
        
            m_exitThunkGenerator.emitThunk(index);
        }
    }
    
    void emitOSRExitCall(
        LValue failCondition, unsigned index, OSRExit& exit, OSRExitCompilationInfo& info,
        FormattedValue lowValue, SpeculationDirection direction, FormattedValue recovery)
    {
        ExitArgumentList arguments;
        
        if (Options::useLLVMOSRExitIntrinsic()) {
            arguments.append(failCondition);
            arguments.append(m_out.constInt32(index));
        }
        
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
        
        if (Options::useLLVMOSRExitIntrinsic()) {
            m_out.call(m_out.osrExitIntrinsic(), arguments);
            return;
        }
        
        m_out.call(
            m_out.intToPtr(
                m_out.get(info.m_thunkAddress),
                pointerType(functionType(m_out.voidType, argumentTypes))),
            arguments);
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
                
                HashSet<Node*>::iterator iter = m_live.begin();
                HashSet<Node*>::iterator end = m_live.end();
                for (; iter != end; ++iter) {
                    Node* candidate = *iter;
                    if (candidate->flags() & NodeHasVarArgs)
                        continue;
                    if (!candidate->child1())
                        continue;
                    if (candidate->child1() != node)
                        continue;
                    switch (candidate->op()) {
                    case Int32ToDouble:
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

        LoweredNodeValue value = m_int32Values.get(node);
        if (isValid(value)) {
            addExitArgument(exit, arguments, index, ValueFormatInt32, value.value());
            return;
        }
        
        value = m_booleanValues.get(node);
        if (isValid(value)) {
            addExitArgument(exit, arguments, index, ValueFormatBoolean, value.value());
            return;
        }
        
        value = m_jsValueValues.get(node);
        if (isValid(value)) {
            addExitArgument(exit, arguments, index, ValueFormatJSValue, value.value());
            return;
        }
        
        value = m_doubleValues.get(node);
        if (isValid(value)) {
            addExitArgument(exit, arguments, index, ValueFormatDouble, value.value());
            return;
        }

        dataLog("Cannot find value for node: ", node, "\n");
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

        m_out.jump(lowBlock(m_graph.block(0)));
    }
    
    void observeMovHint(Node* node)
    {
        ASSERT(node->containsMovHint());
        ASSERT(node->op() != ZombieHint);
        
        int operand = node->local();
        
        m_lastSetOperand = operand;
        m_valueSources.operand(operand) = ValueSource(node->child1().node());
    }
    
    void setInt32(Node* node, LValue value)
    {
        m_int32Values.set(node, LoweredNodeValue(value, m_highBlock));
    }
    void setJSValue(Node* node, LValue value)
    {
        m_jsValueValues.set(node, LoweredNodeValue(value, m_highBlock));
    }
    void setBoolean(Node* node, LValue value)
    {
        m_booleanValues.set(node, LoweredNodeValue(value, m_highBlock));
    }
    void setStorage(Node* node, LValue value)
    {
        m_storageValues.set(node, LoweredNodeValue(value, m_highBlock));
    }
    void setDouble(Node* node, LValue value)
    {
        m_doubleValues.set(node, LoweredNodeValue(value, m_highBlock));
    }

    void setInt32(LValue value)
    {
        setInt32(m_node, value);
    }
    void setJSValue(LValue value)
    {
        setJSValue(m_node, value);
    }
    void setBoolean(LValue value)
    {
        setBoolean(m_node, value);
    }
    void setStorage(LValue value)
    {
        setStorage(m_node, value);
    }
    void setDouble(LValue value)
    {
        setDouble(m_node, value);
    }
    
    bool isValid(const LoweredNodeValue& value)
    {
        if (!value)
            return false;
        if (!m_graph.m_dominators.dominates(value.block(), m_highBlock))
            return false;
        return true;
    }
    
    void addWeakReference(JSCell* target)
    {
        m_graph.m_plan.weakReferences.addLazily(target);
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
    HashMap<BasicBlock*, LBasicBlock> m_blocks;
    
    LValue m_callFrame;
    LValue m_tagTypeNumber;
    LValue m_tagMask;
    
    HashMap<Node*, LoweredNodeValue> m_int32Values;
    HashMap<Node*, LoweredNodeValue> m_jsValueValues;
    HashMap<Node*, LoweredNodeValue> m_booleanValues;
    HashMap<Node*, LoweredNodeValue> m_storageValues;
    HashMap<Node*, LoweredNodeValue> m_doubleValues;
    HashSet<Node*> m_live;
    
    HashMap<Node*, LValue> m_phis;
    
    Operands<ValueSource> m_valueSources;
    int m_lastSetOperand;
    ExitThunkGenerator m_exitThunkGenerator;
    
    InPlaceAbstractState m_state;
    AbstractInterpreter<InPlaceAbstractState> m_interpreter;
    BasicBlock* m_highBlock;
    BasicBlock* m_nextHighBlock;
    LBasicBlock m_nextLowBlock;
    
    CodeOrigin m_codeOriginForExitTarget;
    CodeOrigin m_codeOriginForExitProfile;
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

