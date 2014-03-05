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

#include "config.h"
#include "FTLLowerDFGToLLVM.h"

#if ENABLE(FTL_JIT)

#include "CodeBlockWithJITType.h"
#include "DFGAbstractInterpreterInlines.h"
#include "DFGInPlaceAbstractState.h"
#include "FTLAbstractHeapRepository.h"
#include "FTLAvailableRecovery.h"
#include "FTLForOSREntryJITCode.h"
#include "FTLFormattedValue.h"
#include "FTLInlineCacheSize.h"
#include "FTLLoweredNodeValue.h"
#include "FTLOutput.h"
#include "FTLThunks.h"
#include "FTLWeightedTarget.h"
#include "LinkBuffer.h"
#include "OperandsInlines.h"
#include "JSCInlines.h"
#include "VirtualRegister.h"
#include <atomic>
#include <wtf/ProcessID.h>

namespace JSC { namespace FTL {

using namespace DFG;

static std::atomic<int> compileCounter;

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
        , m_availability(OperandsLike, state.graph.block(0)->variablesAtHead)
        , m_state(state.graph)
        , m_interpreter(state.graph, m_state)
        , m_stackmapIDs(0)
    {
    }
    
    void lower()
    {
        CString name;
        if (verboseCompilationEnabled()) {
            name = toCString(
                "jsBody_", ++compileCounter, "_", codeBlock()->inferredName(),
                "_", codeBlock()->hash());
        } else
            name = "jsBody";
        
        m_graph.m_dominators.computeIfNecessary(m_graph);
        
        m_ftlState.module =
            llvm->ModuleCreateWithNameInContext(name.data(), m_ftlState.context);
        
        m_ftlState.function = addFunction(
            m_ftlState.module, name.data(), functionType(m_out.int64));
        setFunctionCallingConv(m_ftlState.function, LLVMCCallConv);
        if (isX86() && Options::llvmDisallowAVX()) {
            // AVX makes V8/raytrace 80% slower. It makes Kraken/audio-oscillator 4.5x
            // slower. It should be disabled.
            addTargetDependentFunctionAttr(m_ftlState.function, "target-features", "-avx");
        }
        
        m_out.initialize(m_ftlState.module, m_ftlState.function, m_heaps);
        
        m_prologue = FTL_NEW_BLOCK(m_out, ("Prologue"));
        LBasicBlock stackOverflow = FTL_NEW_BLOCK(m_out, ("Stack overflow"));
        m_handleExceptions = FTL_NEW_BLOCK(m_out, ("Handle Exceptions"));

        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            m_highBlock = m_graph.block(blockIndex);
            if (!m_highBlock)
                continue;
            m_blocks.add(m_highBlock, FTL_NEW_BLOCK(m_out, ("Block ", *m_highBlock)));
        }
        
        m_out.appendTo(m_prologue, stackOverflow);
        createPhiVariables();
        LValue capturedAlloca = m_out.alloca(arrayType(m_out.int64, m_graph.m_nextMachineLocal));
        m_captured = m_out.add(
            m_out.ptrToInt(capturedAlloca, m_out.intPtr),
            m_out.constIntPtr(m_graph.m_nextMachineLocal * sizeof(Register)));
        
        // We should not create any alloca's after this point, since they will cease to
        // be mem2reg candidates.
        
        m_ftlState.capturedStackmapID = m_stackmapIDs++;
        m_out.call(
            m_out.stackmapIntrinsic(), m_out.constInt64(m_ftlState.capturedStackmapID),
            m_out.int32Zero, capturedAlloca);
        
        m_callFrame = m_out.ptrToInt(
            m_out.call(m_out.frameAddressIntrinsic(), m_out.int32Zero), m_out.intPtr);
        m_tagTypeNumber = m_out.constInt64(TagTypeNumber);
        m_tagMask = m_out.constInt64(TagMask);
        
        m_out.storePtr(m_out.constIntPtr(codeBlock()), addressFor(JSStack::CodeBlock));
        
        m_out.branch(
            didOverflowStack(), rarely(stackOverflow), usually(lowBlock(m_graph.block(0))));
        
        m_out.appendTo(stackOverflow, m_handleExceptions);
        vmCall(m_out.operation(operationThrowStackOverflowError), m_callFrame, m_out.constIntPtr(codeBlock()), NoExceptions);
        m_ftlState.handleStackOverflowExceptionStackmapID = m_stackmapIDs++;
        m_out.call(
            m_out.stackmapIntrinsic(), m_out.constInt64(m_ftlState.handleStackOverflowExceptionStackmapID),
            m_out.constInt32(MacroAssembler::maxJumpReplacementSize()));
        m_out.unreachable();
        
        m_out.appendTo(m_handleExceptions, lowBlock(m_graph.block(0)));
        m_ftlState.handleExceptionStackmapID = m_stackmapIDs++;
        m_out.call(
            m_out.stackmapIntrinsic(), m_out.constInt64(m_ftlState.handleExceptionStackmapID),
            m_out.constInt32(MacroAssembler::maxJumpReplacementSize()));
        m_out.unreachable();
        
        Vector<BasicBlock*> depthFirst;
        m_graph.getBlocksInDepthFirstOrder(depthFirst);
        for (unsigned i = 0; i < depthFirst.size(); ++i)
            compileBlock(depthFirst[i]);
        
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
                case NodeResultInt52:
                    type = m_out.int64;
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
        m_codeOriginForExitProfile = m_node->origin.semantic;
        m_codeOriginForExitTarget = m_node->origin.forExit;
        
        if (verboseCompilationEnabled())
            dataLog("Lowering ", m_node, "\n");
        
        m_availableRecoveries.resize(0);
        
        bool shouldExecuteEffects = m_interpreter.startExecuting(m_node);
        
        switch (m_node->op()) {
        case Upsilon:
            compileUpsilon();
            break;
        case Phi:
            compilePhi();
            break;
        case JSConstant:
            break;
        case WeakJSConstant:
            compileWeakJSConstant();
            break;
        case PhantomArguments:
            compilePhantomArguments();
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
        case GetMyArgumentsLength:
            compileGetMyArgumentsLength();
            break;
        case GetMyArgumentByVal:
            compileGetMyArgumentByVal();
            break;
        case ZombieHint:
            compileZombieHint();
            break;
        case Phantom:
        case HardPhantom:
            compilePhantom();
            break;
        case ToThis:
            compileToThis();
            break;
        case ValueAdd:
            compileValueAdd();
            break;
        case ArithAdd:
        case ArithSub:
            compileArithAddOrSub();
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
        case ArithSin:
            compileArithSin();
            break;
        case ArithCos:
            compileArithCos();
            break;
        case ArithSqrt:
            compileArithSqrt();
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
        case CheckFunction:
            compileCheckFunction();
            break;
        case CheckExecutable:
            compileCheckExecutable();
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
        case GetById:
            compileGetById();
            break;
        case PutByIdDirect:
        case PutById:
            compilePutById();
            break;
        case GetButterfly:
            compileGetButterfly();
            break;
        case ConstantStoragePointer:
            compileConstantStoragePointer();
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
        case CheckInBounds:
            compileCheckInBounds();
            break;
        case GetByVal:
            compileGetByVal();
            break;
        case PutByVal:
        case PutByValAlias:
        case PutByValDirect:
            compilePutByVal();
            break;
        case ArrayPush:
            compileArrayPush();
            break;
        case ArrayPop:
            compileArrayPop();
            break;
        case NewObject:
            compileNewObject();
            break;
        case NewArray:
            compileNewArray();
            break;
        case NewArrayBuffer:
            compileNewArrayBuffer();
            break;
        case NewArrayWithSize:
            compileNewArrayWithSize();
            break;
        case GetTypedArrayByteOffset:
            compileGetTypedArrayByteOffset();
            break;
        case AllocatePropertyStorage:
            compileAllocatePropertyStorage();
            break;
        case ReallocatePropertyStorage:
            compileReallocatePropertyStorage();
            break;
        case ToString:
            compileToString();
            break;
        case ToPrimitive:
            compileToPrimitive();
            break;
        case MakeRope:
            compileMakeRope();
            break;
        case StringCharAt:
            compileStringCharAt();
            break;
        case StringCharCodeAt:
            compileStringCharCodeAt();
            break;
        case GetByOffset:
            compileGetByOffset();
            break;
        case MultiGetByOffset:
            compileMultiGetByOffset();
            break;
        case PutByOffset:
            compilePutByOffset();
            break;
        case MultiPutByOffset:
            compileMultiPutByOffset();
            break;
        case GetGlobalVar:
            compileGetGlobalVar();
            break;
        case PutGlobalVar:
            compilePutGlobalVar();
            break;
        case NotifyWrite:
            compileNotifyWrite();
            break;
        case GetCallee:
            compileGetCallee();
            break;
        case GetScope:
            compileGetScope();
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
        case InvalidationPoint:
            compileInvalidationPoint();
            break;
        case ValueToInt32:
            compileValueToInt32();
            break;
        case Int52ToValue:
            compileInt52ToValue();
            break;
        case CheckArgumentsNotCreated:
            compileCheckArgumentsNotCreated();
            break;
        case CountExecution:
            compileCountExecution();
            break;
        case StoreBarrier:
            compileStoreBarrier();
            break;
        case ConditionalStoreBarrier:
            compileConditionalStoreBarrier();
            break;
        case StoreBarrierWithNullCheck:
            compileStoreBarrierWithNullCheck();
            break;
        case Flush:
        case PhantomLocal:
        case SetArgument:
        case LoopHint:
        case VariableWatchpoint:
        case FunctionReentryWatchpoint:
        case TypedArrayWatchpoint:
        case AllocationProfileWatchpoint:
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        if (shouldExecuteEffects)
            m_interpreter.executeEffects(nodeIndex);
        
        return true;
    }

    void compileValueToInt32()
    {
        switch (m_node->child1().useKind()) {
        case Int32Use:
            setInt32(lowInt32(m_node->child1()));
            break;
            
        case MachineIntUse:
            setInt32(m_out.castToInt32(lowStrictInt52(m_node->child1())));
            break;
            
        case NumberUse:
        case NotCellUse: {
            LoweredNodeValue value = m_int32Values.get(m_node->child1().node());
            if (isValid(value)) {
                setInt32(value.value());
                break;
            }
            
            value = m_jsValueValues.get(m_node->child1().node());
            if (isValid(value)) {
                LBasicBlock intCase = FTL_NEW_BLOCK(m_out, ("ValueToInt32 int case"));
                LBasicBlock notIntCase = FTL_NEW_BLOCK(m_out, ("ValueToInt32 not int case"));
                LBasicBlock doubleCase = 0;
                LBasicBlock notNumberCase = 0;
                if (m_node->child1().useKind() == NotCellUse) {
                    doubleCase = FTL_NEW_BLOCK(m_out, ("ValueToInt32 double case"));
                    notNumberCase = FTL_NEW_BLOCK(m_out, ("ValueToInt32 not number case"));
                }
                LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ValueToInt32 continuation"));
                
                Vector<ValueFromBlock> results;
                
                m_out.branch(
                    isNotInt32(value.value()), unsure(notIntCase), unsure(intCase));
                
                LBasicBlock lastNext = m_out.appendTo(intCase, notIntCase);
                results.append(m_out.anchor(unboxInt32(value.value())));
                m_out.jump(continuation);
                
                if (m_node->child1().useKind() == NumberUse) {
                    m_out.appendTo(notIntCase, continuation);
                    FTL_TYPE_CHECK(
                        jsValueValue(value.value()), m_node->child1(), SpecFullNumber,
                        isCellOrMisc(value.value()));
                    results.append(m_out.anchor(doubleToInt32(unboxDouble(value.value()))));
                    m_out.jump(continuation);
                } else {
                    m_out.appendTo(notIntCase, doubleCase);
                    m_out.branch(
                        isCellOrMisc(value.value()),
                        unsure(notNumberCase), unsure(doubleCase));
                    
                    m_out.appendTo(doubleCase, notNumberCase);
                    results.append(m_out.anchor(doubleToInt32(unboxDouble(value.value()))));
                    m_out.jump(continuation);
                    
                    m_out.appendTo(notNumberCase, continuation);
                    
                    FTL_TYPE_CHECK(
                        jsValueValue(value.value()), m_node->child1(), ~SpecCell,
                        isCell(value.value()));
                    
                    LValue specialResult = m_out.select(
                        m_out.equal(
                            value.value(),
                            m_out.constInt64(JSValue::encode(jsBoolean(true)))),
                        m_out.int32One, m_out.int32Zero);
                    results.append(m_out.anchor(specialResult));
                    m_out.jump(continuation);
                }
                
                m_out.appendTo(continuation, lastNext);
                setInt32(m_out.phi(m_out.int32, results));
                break;
            }
            
            value = m_doubleValues.get(m_node->child1().node());
            if (isValid(value)) {
                setInt32(doubleToInt32(value.value()));
                break;
            }
            
            terminate(Uncountable);
            break;
        }
            
        case BooleanUse:
            setInt32(m_out.zeroExt(lowBoolean(m_node->child1()), m_out.int32));
            break;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    void compileInt52ToValue()
    {
        setJSValue(lowJSValue(m_node->child1()));
    }

    void compileStoreBarrier()
    {
        emitStoreBarrier(lowCell(m_node->child1()));
    }

    void compileConditionalStoreBarrier()
    {
        LValue base = lowCell(m_node->child1());
        LValue value = lowJSValue(m_node->child2());
        emitStoreBarrier(base, value, m_node->child2());
    }

    void compileStoreBarrierWithNullCheck()
    {
#if ENABLE(GGC)
        LBasicBlock isNotNull = FTL_NEW_BLOCK(m_out, ("Store barrier with null check value not null"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Store barrier continuation"));

        LValue base = lowJSValue(m_node->child1());
        m_out.branch(m_out.isZero64(base), unsure(continuation), unsure(isNotNull));
        LBasicBlock lastNext = m_out.appendTo(isNotNull, continuation);
        emitStoreBarrier(base);
        m_out.appendTo(continuation, lastNext);
#else
        speculate(m_node->child1());
#endif
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
        case MachineIntUse:
            m_out.set(lowInt52(m_node->child1()), destination);
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
        case NodeResultInt52:
            setInt52(m_out.get(source));
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

    void compilePhantomArguments()
    {
        setJSValue(m_out.constInt64(JSValue::encode(JSValue())));
    }
    
    void compileWeakJSConstant()
    {
        setJSValue(weakPointer(m_node->weakConstant()));
    }
    
    void compileGetArgument()
    {
        VariableAccessData* variable = m_node->variableAccessData();
        VirtualRegister operand = variable->machineLocal();
        RELEASE_ASSERT(operand.isArgument());

        LValue jsValue = m_out.load64(addressFor(operand));

        switch (useKindFor(variable->flushFormat())) {
        case Int32Use:
            speculate(BadType, jsValueValue(jsValue), m_node, isNotInt32(jsValue));
            setInt32(unboxInt32(jsValue));
            break;
        case CellUse:
            speculate(BadType, jsValueValue(jsValue), m_node, isNotCell(jsValue));
            setJSValue(jsValue);
            break;
        case BooleanUse:
            speculate(BadType, jsValueValue(jsValue), m_node, isNotBoolean(jsValue));
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
        setJSValue(m_out.load64(m_out.absolute(buffer + m_node->unlinkedLocal().toLocal())));
    }
    
    void compileGetLocal()
    {
        // GetLocals arise only for captured variables.
        
        VariableAccessData* variable = m_node->variableAccessData();
        AbstractValue& value = m_state.variables().operand(variable->local());
        
        RELEASE_ASSERT(variable->isCaptured());
        
        if (isInt32Speculation(value.m_type))
            setInt32(m_out.load32(payloadFor(variable->machineLocal())));
        else
            setJSValue(m_out.load64(addressFor(variable->machineLocal())));
    }
    
    void compileSetLocal()
    {
        VariableAccessData* variable = m_node->variableAccessData();
        switch (variable->flushFormat()) {
        case FlushedJSValue: {
            LValue value = lowJSValue(m_node->child1());
            m_out.store64(value, addressFor(variable->machineLocal()));
            break;
        }
            
        case FlushedDouble: {
            LValue value = lowDouble(m_node->child1());
            m_out.storeDouble(value, addressFor(variable->machineLocal()));
            break;
        }
            
        case FlushedInt32: {
            LValue value = lowInt32(m_node->child1());
            m_out.store32(value, payloadFor(variable->machineLocal()));
            break;
        }
            
        case FlushedInt52: {
            LValue value = lowInt52(m_node->child1());
            m_out.store64(value, addressFor(variable->machineLocal()));
            break;
        }
            
        case FlushedCell: {
            LValue value = lowCell(m_node->child1());
            m_out.store64(value, addressFor(variable->machineLocal()));
            break;
        }
            
        case FlushedBoolean: {
            speculateBoolean(m_node->child1());
            m_out.store64(
                lowJSValue(m_node->child1(), ManualOperandSpeculation),
                addressFor(variable->machineLocal()));
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        
        m_availability.operand(variable->local()) = Availability(variable->flushedAt());
    }
    
    void compileMovHint()
    {
        ASSERT(m_node->containsMovHint());
        ASSERT(m_node->op() != ZombieHint);
        
        VirtualRegister operand = m_node->unlinkedLocal();
        m_availability.operand(operand) = Availability(m_node->child1().node());
    }
    
    void compileZombieHint()
    {
        m_availability.operand(m_node->unlinkedLocal()) = Availability::unavailable();
    }
    
    void compilePhantom()
    {
        DFG_NODE_DO_TO_CHILDREN(m_graph, m_node, speculate);
    }
    
    void compileToThis()
    {
        LValue value = lowJSValue(m_node->child1());
        
        LBasicBlock isCellCase = FTL_NEW_BLOCK(m_out, ("ToThis is cell case"));
        LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("ToThis slow case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ToThis continuation"));
        
        m_out.branch(isCell(value), usually(isCellCase), rarely(slowCase));
        
        LBasicBlock lastNext = m_out.appendTo(isCellCase, slowCase);
        ValueFromBlock fastResult = m_out.anchor(value);
        m_out.branch(isType(value, FinalObjectType), usually(continuation), rarely(slowCase));
        
        m_out.appendTo(slowCase, continuation);
        J_JITOperation_EJ function;
        if (m_graph.isStrictModeFor(m_node->origin.semantic))
            function = operationToThisStrict;
        else
            function = operationToThis;
        ValueFromBlock slowResult = m_out.anchor(
            vmCall(m_out.operation(function), m_callFrame, value));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
    }
    
    void compileValueAdd()
    {
        J_JITOperation_EJJ operation;
        if (!(m_state.forNode(m_node->child1()).m_type & SpecFullNumber)
            && !(m_state.forNode(m_node->child2()).m_type & SpecFullNumber))
            operation = operationValueAddNotNumber;
        else
            operation = operationValueAdd;
        setJSValue(vmCall(
            m_out.operation(operation), m_callFrame,
            lowJSValue(m_node->child1()), lowJSValue(m_node->child2())));
    }
    
    void compileArithAddOrSub()
    {
        bool isSub =  m_node->op() == ArithSub;
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue left = lowInt32(m_node->child1());
            LValue right = lowInt32(m_node->child2());

            if (!shouldCheckOverflow(m_node->arithMode())) {
                setInt32(isSub ? m_out.sub(left, right) : m_out.add(left, right));
                break;
            }

            LValue result;
            if (!isSub) {
                result = m_out.addWithOverflow32(left, right);
                
                if (doesKill(m_node->child2())) {
                    addAvailableRecovery(
                        m_node->child2(), SubRecovery,
                        m_out.extractValue(result, 0), left, ValueFormatInt32);
                } else if (doesKill(m_node->child1())) {
                    addAvailableRecovery(
                        m_node->child1(), SubRecovery,
                        m_out.extractValue(result, 0), right, ValueFormatInt32);
                }
            } else {
                result = m_out.subWithOverflow32(left, right);
                
                if (doesKill(m_node->child2())) {
                    // result = left - right
                    // result - left = -right
                    // right = left - result
                    addAvailableRecovery(
                        m_node->child2(), SubRecovery,
                        left, m_out.extractValue(result, 0), ValueFormatInt32);
                } else if (doesKill(m_node->child1())) {
                    // result = left - right
                    // result + right = left
                    addAvailableRecovery(
                        m_node->child1(), AddRecovery,
                        m_out.extractValue(result, 0), right, ValueFormatInt32);
                }
            }

            speculate(Overflow, noValue(), 0, m_out.extractValue(result, 1));
            setInt32(m_out.extractValue(result, 0));
            break;
        }
            
        case MachineIntUse: {
            if (!m_state.forNode(m_node->child1()).couldBeType(SpecInt52)
                && !m_state.forNode(m_node->child2()).couldBeType(SpecInt52)) {
                Int52Kind kind;
                LValue left = lowWhicheverInt52(m_node->child1(), kind);
                LValue right = lowInt52(m_node->child2(), kind);
                setInt52(isSub ? m_out.sub(left, right) : m_out.add(left, right), kind);
                break;
            }
            
            LValue left = lowInt52(m_node->child1());
            LValue right = lowInt52(m_node->child2());

            LValue result;
            if (!isSub) {
                result = m_out.addWithOverflow64(left, right);
                
                if (doesKill(m_node->child2())) {
                    addAvailableRecovery(
                        m_node->child2(), SubRecovery,
                        m_out.extractValue(result, 0), left, ValueFormatInt52);
                } else if (doesKill(m_node->child1())) {
                    addAvailableRecovery(
                        m_node->child1(), SubRecovery,
                        m_out.extractValue(result, 0), right, ValueFormatInt52);
                }
            } else {
                result = m_out.subWithOverflow64(left, right);
                
                if (doesKill(m_node->child2())) {
                    // result = left - right
                    // result - left = -right
                    // right = left - result
                    addAvailableRecovery(
                        m_node->child2(), SubRecovery,
                        left, m_out.extractValue(result, 0), ValueFormatInt52);
                } else if (doesKill(m_node->child1())) {
                    // result = left - right
                    // result + right = left
                    addAvailableRecovery(
                        m_node->child1(), AddRecovery,
                        m_out.extractValue(result, 0), right, ValueFormatInt52);
                }
            }

            speculate(Int52Overflow, noValue(), 0, m_out.extractValue(result, 1));
            setInt52(m_out.extractValue(result, 0));
            break;
        }
            
        case NumberUse: {
            LValue C1 = lowDouble(m_node->child1());
            LValue C2 = lowDouble(m_node->child2());

            setDouble(isSub ? m_out.doubleSub(C1, C2) : m_out.doubleAdd(C1, C2));
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

            if (!shouldCheckOverflow(m_node->arithMode()))
                result = m_out.mul(left, right);
            else {
                LValue overflowResult = m_out.mulWithOverflow32(left, right);
                speculate(Overflow, noValue(), 0, m_out.extractValue(overflowResult, 1));
                result = m_out.extractValue(overflowResult, 0);
            }
            
            if (shouldCheckNegativeZero(m_node->arithMode())) {
                LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("ArithMul slow case"));
                LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArithMul continuation"));
                
                m_out.branch(
                    m_out.notZero32(result), usually(continuation), rarely(slowCase));
                
                LBasicBlock lastNext = m_out.appendTo(slowCase, continuation);
                LValue cond = m_out.bitOr(m_out.lessThan(left, m_out.int32Zero), m_out.lessThan(right, m_out.int32Zero));
                speculate(NegativeZero, noValue(), 0, cond);
                m_out.jump(continuation);
                m_out.appendTo(continuation, lastNext);
            }
            
            setInt32(result);
            break;
        }
            
        case MachineIntUse: {
            Int52Kind kind;
            LValue left = lowWhicheverInt52(m_node->child1(), kind);
            LValue right = lowInt52(m_node->child2(), opposite(kind));

            LValue overflowResult = m_out.mulWithOverflow64(left, right);
            speculate(Int52Overflow, noValue(), 0, m_out.extractValue(overflowResult, 1));
            LValue result = m_out.extractValue(overflowResult, 0);

            if (shouldCheckNegativeZero(m_node->arithMode())) {
                LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("ArithMul slow case"));
                LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArithMul continuation"));
                
                m_out.branch(
                    m_out.notZero64(result), usually(continuation), rarely(slowCase));
                
                LBasicBlock lastNext = m_out.appendTo(slowCase, continuation);
                LValue cond = m_out.bitOr(m_out.lessThan(left, m_out.int64Zero), m_out.lessThan(right, m_out.int64Zero));
                speculate(NegativeZero, noValue(), 0, cond);
                m_out.jump(continuation);
                m_out.appendTo(continuation, lastNext);
            }
            
            setInt52(result);
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
            
            m_out.branch(
                m_out.above(adjustedDenominator, m_out.int32One),
                usually(continuation), rarely(unsafeDenominator));
            
            LBasicBlock lastNext = m_out.appendTo(unsafeDenominator, continuation);
            
            LValue neg2ToThe31 = m_out.constInt32(-2147483647-1);
            
            if (shouldCheckOverflow(m_node->arithMode())) {
                LValue cond = m_out.bitOr(m_out.isZero32(denominator), m_out.equal(numerator, neg2ToThe31));
                speculate(Overflow, noValue(), 0, cond);
                m_out.jump(continuation);
            } else {
                // This is the case where we convert the result to an int after we're done. So,
                // if the denominator is zero, then the result should be zero.
                // If the denominator is not zero (i.e. it's -1 because we're guarded by the
                // check above) and the numerator is -2^31 then the result should be -2^31.
                
                LBasicBlock divByZero = FTL_NEW_BLOCK(m_out, ("ArithDiv divide by zero"));
                LBasicBlock notDivByZero = FTL_NEW_BLOCK(m_out, ("ArithDiv not divide by zero"));
                LBasicBlock neg2ToThe31ByNeg1 = FTL_NEW_BLOCK(m_out, ("ArithDiv -2^31/-1"));
                
                m_out.branch(
                    m_out.isZero32(denominator), rarely(divByZero), usually(notDivByZero));
                
                m_out.appendTo(divByZero, notDivByZero);
                results.append(m_out.anchor(m_out.int32Zero));
                m_out.jump(done);
                
                m_out.appendTo(notDivByZero, neg2ToThe31ByNeg1);
                m_out.branch(
                    m_out.equal(numerator, neg2ToThe31),
                    rarely(neg2ToThe31ByNeg1), usually(continuation));
                
                m_out.appendTo(neg2ToThe31ByNeg1, continuation);
                results.append(m_out.anchor(neg2ToThe31));
                m_out.jump(done);
            }
            
            m_out.appendTo(continuation, done);
            
            if (shouldCheckNegativeZero(m_node->arithMode())) {
                LBasicBlock zeroNumerator = FTL_NEW_BLOCK(m_out, ("ArithDiv zero numerator"));
                LBasicBlock numeratorContinuation = FTL_NEW_BLOCK(m_out, ("ArithDiv numerator continuation"));
                
                m_out.branch(
                    m_out.isZero32(numerator),
                    rarely(zeroNumerator), usually(numeratorContinuation));
                
                LBasicBlock innerLastNext = m_out.appendTo(zeroNumerator, numeratorContinuation);
                
                speculate(
                    NegativeZero, noValue(), 0, m_out.lessThan(denominator, m_out.int32Zero));
                
                m_out.jump(numeratorContinuation);
                
                m_out.appendTo(numeratorContinuation, innerLastNext);
            }
            
            LValue result = m_out.div(numerator, denominator);
            
            if (shouldCheckOverflow(m_node->arithMode())) {
                speculate(
                    Overflow, noValue(), 0,
                    m_out.notEqual(m_out.mul(result, denominator), numerator));
            }
            
            results.append(m_out.anchor(result));
            m_out.jump(done);
            
            m_out.appendTo(done, lastNext);
            
            setInt32(m_out.phi(m_out.int32, results));
            break;
        }
            
        case NumberUse: {
            setDouble(m_out.doubleDiv(
                lowDouble(m_node->child1()), lowDouble(m_node->child2())));
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
            
            m_out.branch(
                m_out.above(adjustedDenominator, m_out.int32One),
                usually(continuation), rarely(unsafeDenominator));
            
            LBasicBlock lastNext = m_out.appendTo(unsafeDenominator, continuation);
            
            LValue neg2ToThe31 = m_out.constInt32(-2147483647-1);
            
            // FIXME: -2^31 / -1 will actually yield negative zero, so we could have a
            // separate case for that. But it probably doesn't matter so much.
            if (shouldCheckOverflow(m_node->arithMode())) {
                LValue cond = m_out.bitOr(m_out.isZero32(denominator), m_out.equal(numerator, neg2ToThe31));
                speculate(Overflow, noValue(), 0, cond);
                m_out.jump(continuation);
            } else {
                // This is the case where we convert the result to an int after we're done. So,
                // if the denominator is zero, then the result should be result should be zero.
                // If the denominator is not zero (i.e. it's -1 because we're guarded by the
                // check above) and the numerator is -2^31 then the result should be -2^31.
                
                LBasicBlock modByZero = FTL_NEW_BLOCK(m_out, ("ArithMod modulo by zero"));
                LBasicBlock notModByZero = FTL_NEW_BLOCK(m_out, ("ArithMod not modulo by zero"));
                LBasicBlock neg2ToThe31ByNeg1 = FTL_NEW_BLOCK(m_out, ("ArithMod -2^31/-1"));
                
                m_out.branch(
                    m_out.isZero32(denominator), rarely(modByZero), usually(notModByZero));
                
                m_out.appendTo(modByZero, notModByZero);
                results.append(m_out.anchor(m_out.int32Zero));
                m_out.jump(done);
                
                m_out.appendTo(notModByZero, neg2ToThe31ByNeg1);
                m_out.branch(
                    m_out.equal(numerator, neg2ToThe31),
                    rarely(neg2ToThe31ByNeg1), usually(continuation));
                
                m_out.appendTo(neg2ToThe31ByNeg1, continuation);
                results.append(m_out.anchor(m_out.int32Zero));
                m_out.jump(done);
            }
            
            m_out.appendTo(continuation, done);
            
            LValue remainder = m_out.rem(numerator, denominator);
            
            if (shouldCheckNegativeZero(m_node->arithMode())) {
                LBasicBlock negativeNumerator = FTL_NEW_BLOCK(m_out, ("ArithMod negative numerator"));
                LBasicBlock numeratorContinuation = FTL_NEW_BLOCK(m_out, ("ArithMod numerator continuation"));
                
                m_out.branch(
                    m_out.lessThan(numerator, m_out.int32Zero),
                    unsure(negativeNumerator), unsure(numeratorContinuation));
                
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
                unsure(continuation), unsure(notLessThan));
            
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

    void compileArithSin() { setDouble(m_out.doubleSin(lowDouble(m_node->child1()))); }

    void compileArithCos() { setDouble(m_out.doubleCos(lowDouble(m_node->child1()))); }

    void compileArithSqrt() { setDouble(m_out.doubleSqrt(lowDouble(m_node->child1()))); }
    
    void compileArithNegate()
    {
        switch (m_node->child1().useKind()) {
        case Int32Use: {
            LValue value = lowInt32(m_node->child1());
            
            LValue result;
            if (!shouldCheckOverflow(m_node->arithMode()))
                result = m_out.neg(value);
            else if (!shouldCheckNegativeZero(m_node->arithMode())) {
                // We don't have a negate-with-overflow intrinsic. Hopefully this
                // does the trick, though.
                LValue overflowResult = m_out.subWithOverflow32(m_out.int32Zero, value);
                speculate(Overflow, noValue(), 0, m_out.extractValue(overflowResult, 1));
                result = m_out.extractValue(overflowResult, 0);
            } else {
                speculate(Overflow, noValue(), 0, m_out.testIsZero32(value, m_out.constInt32(0x7fffffff)));
                result = m_out.neg(value);
            }

            setInt32(result);
            break;
        }
            
        case MachineIntUse: {
            if (!m_state.forNode(m_node->child1()).couldBeType(SpecInt52)) {
                Int52Kind kind;
                LValue value = lowWhicheverInt52(m_node->child1(), kind);
                LValue result = m_out.neg(value);
                if (shouldCheckNegativeZero(m_node->arithMode()))
                    speculate(NegativeZero, noValue(), 0, m_out.isZero64(result));
                setInt52(result, kind);
                break;
            }
            
            LValue value = lowInt52(m_node->child1());
            LValue overflowResult = m_out.subWithOverflow64(m_out.int64Zero, value);
            speculate(Int52Overflow, noValue(), 0, m_out.extractValue(overflowResult, 1));
            LValue result = m_out.extractValue(overflowResult, 0);
            speculate(NegativeZero, noValue(), 0, m_out.isZero64(result));
            setInt52(result);
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

        if (doesOverflow(m_node->arithMode())) {
            setDouble(m_out.unsignedToDouble(value));
            return;
        }
        
        speculate(Overflow, noValue(), 0, m_out.lessThan(value, m_out.int32Zero));
        setInt32(value);
    }
    
    void compileInt32ToDouble()
    {
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
        
        LValue structureID = m_out.load32(cell, m_heaps.JSCell_structureID);
        
        if (m_node->structureSet().size() == 1) {
            speculate(
                exitKind, jsValueValue(cell), 0,
                m_out.notEqual(structureID, weakStructure(m_node->structureSet()[0])));
            return;
        }
        
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("CheckStructure continuation"));
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(continuation);
        for (unsigned i = 0; i < m_node->structureSet().size() - 1; ++i) {
            LBasicBlock nextStructure = FTL_NEW_BLOCK(m_out, ("CheckStructure nextStructure"));
            m_out.branch(
                m_out.equal(structureID, weakStructure(m_node->structureSet()[i])),
                unsure(continuation), unsure(nextStructure));
            m_out.appendTo(nextStructure);
        }
        
        speculate(
            exitKind, jsValueValue(cell), 0,
            m_out.notEqual(structureID, weakStructure(m_node->structureSet().last())));
        
        m_out.jump(continuation);
        m_out.appendTo(continuation, lastNext);
    }
    
    void compileStructureTransitionWatchpoint()
    {
        addWeakReference(m_node->structure());
        speculateCell(m_node->child1());
    }
    
    void compileCheckFunction()
    {
        LValue cell = lowCell(m_node->child1());
        
        speculate(
            BadFunction, jsValueValue(cell), m_node->child1().node(),
            m_out.notEqual(cell, weakPointer(m_node->function())));
    }
    
    void compileCheckExecutable()
    {
        LValue cell = lowCell(m_node->child1());
        
        speculate(
            BadExecutable, jsValueValue(cell), m_node->child1().node(),
            m_out.notEqual(
                m_out.loadPtr(cell, m_heaps.JSFunction_executable),
                weakPointer(m_node->executable())));
    }
    
    void compileArrayifyToStructure()
    {
        LValue cell = lowCell(m_node->child1());
        LValue property = !!m_node->child2() ? lowInt32(m_node->child2()) : 0;
        
        LBasicBlock unexpectedStructure = FTL_NEW_BLOCK(m_out, ("ArrayifyToStructure unexpected structure"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArrayifyToStructure continuation"));
        
        LValue structureID = m_out.load32(cell, m_heaps.JSCell_structureID);
        
        m_out.branch(
            m_out.notEqual(structureID, weakStructure(m_node->structure())),
            rarely(unexpectedStructure), usually(continuation));
        
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
        
        speculate(
            BadIndexingType, jsValueValue(cell), 0,
            m_out.notEqual(structureID, weakStructure(m_node->structure())));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compilePutStructure()
    {
        m_ftlState.jitCode->common.notifyCompilingStructureTransition(m_graph.m_plan, codeBlock(), m_node);

        Structure* oldStructure = m_node->structureTransitionData().previousStructure;
        Structure* newStructure = m_node->structureTransitionData().newStructure;
        ASSERT_UNUSED(oldStructure, oldStructure->indexingType() == newStructure->indexingType());
        ASSERT(oldStructure->typeInfo().inlineTypeFlags() == newStructure->typeInfo().inlineTypeFlags());
        ASSERT(oldStructure->typeInfo().type() == newStructure->typeInfo().type());

        LValue cell = lowCell(m_node->child1()); 
        m_out.store32(
            weakStructure(newStructure),
            cell, m_heaps.JSCell_structureID);
    }
    
    void compilePhantomPutStructure()
    {
        m_ftlState.jitCode->common.notifyCompilingStructureTransition(m_graph.m_plan, codeBlock(), m_node);
    }
    
    void compileGetById()
    {
        // Pretty much the only reason why we don't also support GetByIdFlush is because:
        // https://bugs.webkit.org/show_bug.cgi?id=125711
        
        switch (m_node->child1().useKind()) {
        case CellUse: {
            setJSValue(getById(lowCell(m_node->child1())));
            return;
        }
            
        case UntypedUse: {
            // This is pretty weird, since we duplicate the slow path both here and in the
            // code generated by the IC. We should investigate making this less bad.
            // https://bugs.webkit.org/show_bug.cgi?id=127830
            LValue value = lowJSValue(m_node->child1());
            
            LBasicBlock cellCase = FTL_NEW_BLOCK(m_out, ("GetById untyped cell case"));
            LBasicBlock notCellCase = FTL_NEW_BLOCK(m_out, ("GetById untyped not cell case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("GetById untyped continuation"));
            
            m_out.branch(isCell(value), unsure(cellCase), unsure(notCellCase));
            
            LBasicBlock lastNext = m_out.appendTo(cellCase, notCellCase);
            ValueFromBlock cellResult = m_out.anchor(getById(value));
            m_out.jump(continuation);
            
            m_out.appendTo(notCellCase, continuation);
            ValueFromBlock notCellResult = m_out.anchor(vmCall(
                m_out.operation(operationGetById),
                m_callFrame, getUndef(m_out.intPtr), value,
                m_out.constIntPtr(m_graph.identifiers()[m_node->identifierNumber()])));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, cellResult, notCellResult));
            return;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    }
    
    void compilePutById()
    {
        // See above; CellUse is easier so we do only that for now.
        ASSERT(m_node->child1().useKind() == CellUse);
        
        LValue base = lowCell(m_node->child1());
        LValue value = lowJSValue(m_node->child2());
        StringImpl* uid = m_graph.identifiers()[m_node->identifierNumber()];

        // Arguments: id, bytes, target, numArgs, args...
        unsigned stackmapID = m_stackmapIDs++;

        if (Options::verboseCompilation())
            dataLog("    Emitting PutById patchpoint with stackmap #", stackmapID, "\n");
        
        LValue call = m_out.call(
            m_out.patchpointVoidIntrinsic(),
            m_out.constInt64(stackmapID), m_out.constInt32(sizeOfPutById()),
            constNull(m_out.ref8), m_out.constInt32(2), base, value);
        setInstructionCallingConvention(call, LLVMAnyRegCallConv);
        
        m_ftlState.putByIds.append(PutByIdDescriptor(
            stackmapID, m_node->origin.semantic, uid,
            m_graph.executableFor(m_node->origin.semantic)->ecmaMode(),
            m_node->op() == PutByIdDirect ? Direct : NotDirect));
    }
    
    void compileGetButterfly()
    {
        setStorage(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSObject_butterfly));
    }
    
    void compileConstantStoragePointer()
    {
        setStorage(m_out.constIntPtr(m_node->storagePointer()));
    }
    
    void compileGetIndexedPropertyStorage()
    {
        LValue cell = lowCell(m_node->child1());
        
        if (m_node->arrayMode().type() == Array::String) {
            LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("GetIndexedPropertyStorage String slow case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("GetIndexedPropertyStorage String continuation"));
            
            ValueFromBlock fastResult = m_out.anchor(
                m_out.loadPtr(cell, m_heaps.JSString_value));
            
            m_out.branch(
                m_out.notNull(fastResult.value()), usually(continuation), rarely(slowPath));
            
            LBasicBlock lastNext = m_out.appendTo(slowPath, continuation);
            
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(m_out.operation(operationResolveRope), m_callFrame, cell));
            
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            
            setStorage(m_out.loadPtr(m_out.phi(m_out.intPtr, fastResult, slowResult), m_heaps.StringImpl_data));
            return;
        }
        
        setStorage(m_out.loadPtr(cell, m_heaps.JSArrayBufferView_vector));
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

    void compileGetTypedArrayByteOffset()
    {
        LValue basePtr = lowCell(m_node->child1());    

        LBasicBlock simpleCase = FTL_NEW_BLOCK(m_out, ("wasteless typed array"));
        LBasicBlock wastefulCase = FTL_NEW_BLOCK(m_out, ("wasteful typed array"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("continuation branch"));
        
        LValue baseAddress = m_out.addPtr(basePtr, JSArrayBufferView::offsetOfMode());
        m_out.branch(
            m_out.notEqual(baseAddress , m_out.constIntPtr(WastefulTypedArray)),
            unsure(simpleCase), unsure(wastefulCase));

        // begin simple case        
        LBasicBlock lastNext = m_out.appendTo(simpleCase, wastefulCase);

        ValueFromBlock simpleOut = m_out.anchor(m_out.constIntPtr(0));

        m_out.jump(continuation);

        // begin wasteful case
        m_out.appendTo(wastefulCase, continuation);

        LValue vectorPtr = m_out.loadPtr(basePtr, m_heaps.JSArrayBufferView_vector);
        LValue butterflyPtr = m_out.loadPtr(basePtr, m_heaps.JSObject_butterfly);
        LValue arrayBufferPtr = m_out.loadPtr(butterflyPtr, m_heaps.Butterfly_arrayBuffer);
        LValue dataPtr = m_out.loadPtr(arrayBufferPtr, m_heaps.ArrayBuffer_data);

        ValueFromBlock wastefulOut = m_out.anchor(m_out.sub(dataPtr, vectorPtr));        

        m_out.jump(continuation);
        m_out.appendTo(continuation, lastNext);

        // output
        setInt32(m_out.castToInt32(m_out.phi(m_out.intPtr, simpleOut, wastefulOut)));
    }
    
    void compileGetMyArgumentsLength() 
    {
        checkArgumentsNotCreated();

        RELEASE_ASSERT(!m_node->origin.semantic.inlineCallFrame);
        setInt32(m_out.add(m_out.load32NonNegative(payloadFor(JSStack::ArgumentCount)), m_out.constInt32(-1)));
    }
    
    void compileGetMyArgumentByVal()
    {
        checkArgumentsNotCreated();
        
        CodeOrigin codeOrigin = m_node->origin.semantic;
        
        LValue zeroBasedIndex = lowInt32(m_node->child1());
        LValue oneBasedIndex = m_out.add(zeroBasedIndex, m_out.int32One);
        
        LValue limit;
        if (codeOrigin.inlineCallFrame)
            limit = m_out.constInt32(codeOrigin.inlineCallFrame->arguments.size());
        else
            limit = m_out.load32(payloadFor(JSStack::ArgumentCount));
        
        speculate(Uncountable, noValue(), 0, m_out.aboveOrEqual(oneBasedIndex, limit));
        
        SymbolTable* symbolTable = m_graph.baselineCodeBlockFor(codeOrigin)->symbolTable();
        if (symbolTable->slowArguments()) {
            // FIXME: FTL should support activations.
            // https://bugs.webkit.org/show_bug.cgi?id=129576
            
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        TypedPointer base;
        if (codeOrigin.inlineCallFrame)
            base = addressFor(codeOrigin.inlineCallFrame->arguments[1].virtualRegister());
        else
            base = addressFor(virtualRegisterForArgument(1));
        
        LValue pointer = m_out.baseIndex(
            base.value(), m_out.zeroExt(zeroBasedIndex, m_out.intPtr), ScaleEight);
        setJSValue(m_out.load64(TypedPointer(m_heaps.variables.atAnyIndex(), pointer)));
    }

    void compileGetArrayLength()
    {
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            setInt32(m_out.load32NonNegative(lowStorage(m_node->child2()), m_heaps.Butterfly_publicLength));
            return;
        }
            
        case Array::String: {
            LValue string = lowCell(m_node->child1());
            setInt32(m_out.load32NonNegative(string, m_heaps.JSString_length));
            return;
        }
            
        default:
            if (isTypedView(m_node->arrayMode().typedArrayType())) {
                setInt32(
                    m_out.load32NonNegative(lowCell(m_node->child1()), m_heaps.JSArrayBufferView_length));
                return;
            }
            
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    }
    
    void compileCheckInBounds()
    {
        speculate(
            OutOfBounds, noValue(), 0,
            m_out.aboveOrEqual(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileGetByVal()
    {
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Contiguous: {
            LValue index = lowInt32(m_node->child2());
            LValue storage = lowStorage(m_node->child3());
            
            IndexedAbstractHeap& heap = m_node->arrayMode().type() == Array::Int32 ?
                m_heaps.indexedInt32Properties : m_heaps.indexedContiguousProperties;
            
            if (m_node->arrayMode().isInBounds()) {
                LValue result = m_out.load64(baseIndex(heap, storage, index, m_node->child2()));
                speculate(LoadFromHole, noValue(), 0, m_out.isZero64(result));
                setJSValue(result);
                return;
            }
            
            LValue base = lowCell(m_node->child1());
            
            LBasicBlock fastCase = FTL_NEW_BLOCK(m_out, ("GetByVal int/contiguous fast case"));
            LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("GetByVal int/contiguous slow case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("GetByVal int/contiguous continuation"));
            
            m_out.branch(
                m_out.aboveOrEqual(
                    index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength)),
                rarely(slowCase), usually(fastCase));
            
            LBasicBlock lastNext = m_out.appendTo(fastCase, slowCase);
            
            ValueFromBlock fastResult = m_out.anchor(
                m_out.load64(baseIndex(heap, storage, index, m_node->child2())));
            m_out.branch(
                m_out.isZero64(fastResult.value()), rarely(slowCase), usually(continuation));
            
            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(m_out.operation(operationGetByValArrayInt), m_callFrame, base, index));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
            return;
        }
            
        case Array::Double: {
            LValue index = lowInt32(m_node->child2());
            LValue storage = lowStorage(m_node->child3());
            
            IndexedAbstractHeap& heap = m_heaps.indexedDoubleProperties;
            
            if (m_node->arrayMode().isInBounds()) {
                LValue result = m_out.loadDouble(
                    baseIndex(heap, storage, index, m_node->child2()));
                
                if (!m_node->arrayMode().isSaneChain()) {
                    speculate(
                        LoadFromHole, noValue(), 0,
                        m_out.doubleNotEqualOrUnordered(result, result));
                }
                setDouble(result);
                break;
            }
            
            LValue base = lowCell(m_node->child1());
            
            LBasicBlock inBounds = FTL_NEW_BLOCK(m_out, ("GetByVal double in bounds"));
            LBasicBlock boxPath = FTL_NEW_BLOCK(m_out, ("GetByVal double boxing"));
            LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("GetByVal double slow case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("GetByVal double continuation"));
            
            m_out.branch(
                m_out.aboveOrEqual(
                    index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength)),
                rarely(slowCase), usually(inBounds));
            
            LBasicBlock lastNext = m_out.appendTo(inBounds, boxPath);
            LValue doubleValue = m_out.loadDouble(
                baseIndex(heap, storage, index, m_node->child2()));
            m_out.branch(
                m_out.doubleNotEqualOrUnordered(doubleValue, doubleValue),
                rarely(slowCase), usually(boxPath));
            
            m_out.appendTo(boxPath, slowCase);
            ValueFromBlock fastResult = m_out.anchor(boxDouble(doubleValue));
            m_out.jump(continuation);
            
            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(m_out.operation(operationGetByValArrayInt), m_callFrame, base, index));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
            return;
        }
            
        case Array::Generic: {
            setJSValue(vmCall(
                m_out.operation(operationGetByVal), m_callFrame,
                lowJSValue(m_node->child1()), lowJSValue(m_node->child2())));
            return;
        }
            
        case Array::String: {
            compileStringCharAt();
            return;
        }
            
        default: {
            LValue index = lowInt32(m_node->child2());
            LValue storage = lowStorage(m_node->child3());
            
            TypedArrayType type = m_node->arrayMode().typedArrayType();
            
            if (isTypedView(type)) {
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
                        speculate(
                            Overflow, noValue(), 0, m_out.lessThan(result, m_out.int32Zero));
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
        Edge child5 = m_graph.varArgChild(m_node, 4);
        
        switch (m_node->arrayMode().type()) {
        case Array::Generic: {
            V_JITOperation_EJJJ operation;
            if (m_node->op() == PutByValDirect) {
                if (m_graph.isStrictModeFor(m_node->origin.semantic))
                    operation = operationPutByValDirectStrict;
                else
                    operation = operationPutByValDirectNonStrict;
            } else {
                if (m_graph.isStrictModeFor(m_node->origin.semantic))
                    operation = operationPutByValStrict;
                else
                    operation = operationPutByValNonStrict;
            }
                
            vmCall(
                m_out.operation(operation), m_callFrame,
                lowJSValue(child1), lowJSValue(child2), lowJSValue(child3));
            return;
        }
            
        default:
            break;
        }

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
                    doubleValue(value), child3, SpecFullRealNumber,
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
                TypedPointer pointer = TypedPointer(
                    m_heaps.typedArrayProperties,
                    m_out.add(
                        storage,
                        m_out.shl(
                            m_out.zeroExt(index, m_out.intPtr),
                            m_out.constIntPtr(logElementSize(type)))));
                
                LType refType;
                LValue valueToStore;
                
                if (isInt(type)) {
                    LValue intValue;
                    switch (child3.useKind()) {
                    case MachineIntUse:
                    case Int32Use: {
                        if (child3.useKind() == Int32Use)
                            intValue = lowInt32(child3);
                        else
                            intValue = m_out.castToInt32(lowStrictInt52(child3));

                        if (isClamped(type)) {
                            ASSERT(elementSize(type) == 1);
                            
                            LBasicBlock atLeastZero = FTL_NEW_BLOCK(m_out, ("PutByVal int clamp atLeastZero"));
                            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("PutByVal int clamp continuation"));
                            
                            Vector<ValueFromBlock, 2> intValues;
                            intValues.append(m_out.anchor(m_out.int32Zero));
                            m_out.branch(
                                m_out.lessThan(intValue, m_out.int32Zero),
                                unsure(continuation), unsure(atLeastZero));
                            
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
                                unsure(continuation), unsure(atLeastZero));
                            
                            LBasicBlock lastNext = m_out.appendTo(atLeastZero, withinRange);
                            intValues.append(m_out.anchor(m_out.constInt32(255)));
                            m_out.branch(
                                m_out.doubleGreaterThan(doubleValue, m_out.constDouble(255)),
                                unsure(continuation), unsure(withinRange));
                            
                            m_out.appendTo(withinRange, continuation);
                            intValues.append(m_out.anchor(m_out.fpToInt32(doubleValue)));
                            m_out.jump(continuation);
                            
                            m_out.appendTo(continuation, lastNext);
                            intValue = m_out.phi(m_out.int32, intValues);
                        } else
                            intValue = doubleToInt32(doubleValue);
                        break;
                    }
                        
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                    
                    switch (elementSize(type)) {
                    case 1:
                        valueToStore = m_out.intCast(intValue, m_out.int8);
                        refType = m_out.ref8;
                        break;
                    case 2:
                        valueToStore = m_out.intCast(intValue, m_out.int16);
                        refType = m_out.ref16;
                        break;
                    case 4:
                        valueToStore = intValue;
                        refType = m_out.ref32;
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                } else /* !isInt(type) */ {
                    LValue value = lowDouble(child3);
                    switch (type) {
                    case TypeFloat32:
                        valueToStore = m_out.fpCast(value, m_out.floatType);
                        refType = m_out.refFloat;
                        break;
                    case TypeFloat64:
                        valueToStore = value;
                        refType = m_out.refDouble;
                        break;
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                }
                
                if (m_node->arrayMode().isInBounds() || m_node->op() == PutByValAlias)
                    m_out.store(valueToStore, pointer, refType);
                else {
                    LBasicBlock isInBounds = FTL_NEW_BLOCK(m_out, ("PutByVal typed array in bounds case"));
                    LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("PutByVal typed array continuation"));
                    
                    m_out.branch(
                        m_out.aboveOrEqual(index, lowInt32(child5)),
                        unsure(continuation), unsure(isInBounds));
                    
                    LBasicBlock lastNext = m_out.appendTo(isInBounds, continuation);
                    m_out.store(valueToStore, pointer, refType);
                    m_out.jump(continuation);
                    
                    m_out.appendTo(continuation, lastNext);
                }
                
                return;
            }
            
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileArrayPush()
    {
        LValue base = lowCell(m_node->child1());
        LValue storage = lowStorage(m_node->child3());
        
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Contiguous:
        case Array::Double: {
            LValue value;
            LType refType;
            
            if (m_node->arrayMode().type() != Array::Double) {
                value = lowJSValue(m_node->child2(), ManualOperandSpeculation);
                if (m_node->arrayMode().type() == Array::Int32) {
                    FTL_TYPE_CHECK(
                        jsValueValue(value), m_node->child2(), SpecInt32, isNotInt32(value));
                }
                refType = m_out.ref64;
            } else {
                value = lowDouble(m_node->child2());
                FTL_TYPE_CHECK(
                    doubleValue(value), m_node->child2(), SpecFullRealNumber,
                    m_out.doubleNotEqualOrUnordered(value, value));
                refType = m_out.refDouble;
            }
            
            IndexedAbstractHeap& heap = m_heaps.forArrayType(m_node->arrayMode().type());

            LValue prevLength = m_out.load32(storage, m_heaps.Butterfly_publicLength);
            
            LBasicBlock fastPath = FTL_NEW_BLOCK(m_out, ("ArrayPush fast path"));
            LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("ArrayPush slow path"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArrayPush continuation"));
            
            m_out.branch(
                m_out.aboveOrEqual(
                    prevLength, m_out.load32(storage, m_heaps.Butterfly_vectorLength)),
                rarely(slowPath), usually(fastPath));
            
            LBasicBlock lastNext = m_out.appendTo(fastPath, slowPath);
            m_out.store(
                value,
                m_out.baseIndex(heap, storage, m_out.zeroExt(prevLength, m_out.intPtr)),
                refType);
            LValue newLength = m_out.add(prevLength, m_out.int32One);
            m_out.store32(newLength, storage, m_heaps.Butterfly_publicLength);
            
            ValueFromBlock fastResult = m_out.anchor(boxInt32(newLength));
            m_out.jump(continuation);
            
            m_out.appendTo(slowPath, continuation);
            LValue operation;
            if (m_node->arrayMode().type() != Array::Double)
                operation = m_out.operation(operationArrayPush);
            else
                operation = m_out.operation(operationArrayPushDouble);
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(operation, m_callFrame, value, base));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
            return;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    }
    
    void compileArrayPop()
    {
        LValue base = lowCell(m_node->child1());
        LValue storage = lowStorage(m_node->child2());
        
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            IndexedAbstractHeap& heap = m_heaps.forArrayType(m_node->arrayMode().type());
            
            LBasicBlock fastCase = FTL_NEW_BLOCK(m_out, ("ArrayPop fast case"));
            LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("ArrayPop slow case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ArrayPop continuation"));
            
            LValue prevLength = m_out.load32(storage, m_heaps.Butterfly_publicLength);
            
            Vector<ValueFromBlock, 3> results;
            results.append(m_out.anchor(m_out.constInt64(JSValue::encode(jsUndefined()))));
            m_out.branch(
                m_out.isZero32(prevLength), rarely(continuation), usually(fastCase));
            
            LBasicBlock lastNext = m_out.appendTo(fastCase, slowCase);
            LValue newLength = m_out.sub(prevLength, m_out.int32One);
            m_out.store32(newLength, storage, m_heaps.Butterfly_publicLength);
            TypedPointer pointer = m_out.baseIndex(
                heap, storage, m_out.zeroExt(newLength, m_out.intPtr));
            if (m_node->arrayMode().type() != Array::Double) {
                LValue result = m_out.load64(pointer);
                m_out.store64(m_out.int64Zero, pointer);
                results.append(m_out.anchor(result));
                m_out.branch(
                    m_out.notZero64(result), usually(continuation), rarely(slowCase));
            } else {
                LValue result = m_out.loadDouble(pointer);
                m_out.store64(m_out.constInt64(bitwise_cast<int64_t>(QNaN)), pointer);
                results.append(m_out.anchor(boxDouble(result)));
                m_out.branch(
                    m_out.doubleEqual(result, result),
                    usually(continuation), rarely(slowCase));
            }
            
            m_out.appendTo(slowCase, continuation);
            results.append(m_out.anchor(vmCall(
                m_out.operation(operationArrayPopAndRecoverLength), m_callFrame, base)));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, results));
            return;
        }

        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    }
    
    void compileNewObject()
    {
        Structure* structure = m_node->structure();
        size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
        MarkedAllocator* allocator = &vm().heap.allocatorForObjectWithoutDestructor(allocationSize);
        
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("NewObject slow path"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("NewObject continuation"));
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        ValueFromBlock fastResult = m_out.anchor(allocateObject(
            m_out.constIntPtr(allocator), structure, m_out.intPtrZero, slowPath));
        
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        
        ValueFromBlock slowResult = m_out.anchor(vmCall(
            m_out.operation(operationNewObject), m_callFrame, m_out.constIntPtr(structure)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.intPtr, fastResult, slowResult));
    }
    
    void compileNewArray()
    {
        // First speculate appropriately on all of the children. Do this unconditionally up here
        // because some of the slow paths may otherwise forget to do it. It's sort of arguable
        // that doing the speculations up here might be unprofitable for RA - so we can consider
        // sinking this to below the allocation fast path if we find that this has a lot of
        // register pressure.
        for (unsigned operandIndex = 0; operandIndex < m_node->numChildren(); ++operandIndex)
            speculate(m_graph.varArgChild(m_node, operandIndex));
        
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(
            m_node->indexingType());
        
        RELEASE_ASSERT(structure->indexingType() == m_node->indexingType());
        
        if (!globalObject->isHavingABadTime() && !hasArrayStorage(m_node->indexingType())) {
            unsigned numElements = m_node->numChildren();
            
            ArrayValues arrayValues = allocateJSArray(structure, numElements);
            
            for (unsigned operandIndex = 0; operandIndex < m_node->numChildren(); ++operandIndex) {
                Edge edge = m_graph.varArgChild(m_node, operandIndex);
                
                switch (m_node->indexingType()) {
                case ALL_BLANK_INDEXING_TYPES:
                case ALL_UNDECIDED_INDEXING_TYPES:
                    CRASH();
                    break;
                    
                case ALL_DOUBLE_INDEXING_TYPES:
                    m_out.storeDouble(
                        lowDouble(edge),
                        arrayValues.butterfly, m_heaps.indexedDoubleProperties[operandIndex]);
                    break;
                    
                case ALL_INT32_INDEXING_TYPES:
                case ALL_CONTIGUOUS_INDEXING_TYPES:
                    m_out.store64(
                        lowJSValue(edge, ManualOperandSpeculation),
                        arrayValues.butterfly,
                        m_heaps.forIndexingType(m_node->indexingType())->at(operandIndex));
                    break;
                    
                default:
                    CRASH();
                }
            }
            
            setJSValue(arrayValues.array);
            return;
        }
        
        if (!m_node->numChildren()) {
            setJSValue(vmCall(
                m_out.operation(operationNewEmptyArray), m_callFrame,
                m_out.constIntPtr(structure)));
            return;
        }
        
        size_t scratchSize = sizeof(EncodedJSValue) * m_node->numChildren();
        ASSERT(scratchSize);
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());
        
        for (unsigned operandIndex = 0; operandIndex < m_node->numChildren(); ++operandIndex) {
            Edge edge = m_graph.varArgChild(m_node, operandIndex);
            m_out.store64(
                lowJSValue(edge, ManualOperandSpeculation),
                m_out.absolute(buffer + operandIndex));
        }
        
        m_out.storePtr(
            m_out.constIntPtr(scratchSize), m_out.absolute(scratchBuffer->activeLengthPtr()));
        
        LValue result = vmCall(
            m_out.operation(operationNewArray), m_callFrame,
            m_out.constIntPtr(structure), m_out.constIntPtr(buffer),
            m_out.constIntPtr(m_node->numChildren()));
        
        m_out.storePtr(m_out.intPtrZero, m_out.absolute(scratchBuffer->activeLengthPtr()));
        
        setJSValue(result);
    }
    
    void compileNewArrayBuffer()
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(
            m_node->indexingType());
        
        RELEASE_ASSERT(structure->indexingType() == m_node->indexingType());
        
        if (!globalObject->isHavingABadTime() && !hasArrayStorage(m_node->indexingType())) {
            unsigned numElements = m_node->numConstants();
            
            ArrayValues arrayValues = allocateJSArray(structure, numElements);
            
            JSValue* data = codeBlock()->constantBuffer(m_node->startConstant());
            for (unsigned index = 0; index < m_node->numConstants(); ++index) {
                int64_t value;
                if (hasDouble(m_node->indexingType()))
                    value = bitwise_cast<int64_t>(data[index].asNumber());
                else
                    value = JSValue::encode(data[index]);
                
                m_out.store64(
                    m_out.constInt64(value),
                    arrayValues.butterfly,
                    m_heaps.forIndexingType(m_node->indexingType())->at(index));
            }
            
            setJSValue(arrayValues.array);
            return;
        }
        
        setJSValue(vmCall(
            m_out.operation(operationNewArrayBuffer), m_callFrame,
            m_out.constIntPtr(structure), m_out.constIntPtr(m_node->startConstant()),
            m_out.constIntPtr(m_node->numConstants())));
    }
    
    void compileNewArrayWithSize()
    {
        LValue publicLength = lowInt32(m_node->child1());
        
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(
            m_node->indexingType());
        
        if (!globalObject->isHavingABadTime() && !hasArrayStorage(m_node->indexingType())) {
            ASSERT(
                hasUndecided(structure->indexingType())
                || hasInt32(structure->indexingType())
                || hasDouble(structure->indexingType())
                || hasContiguous(structure->indexingType()));

            LBasicBlock fastCase = FTL_NEW_BLOCK(m_out, ("NewArrayWithSize fast case"));
            LBasicBlock largeCase = FTL_NEW_BLOCK(m_out, ("NewArrayWithSize large case"));
            LBasicBlock failCase = FTL_NEW_BLOCK(m_out, ("NewArrayWithSize fail case"));
            LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("NewArrayWithSize slow case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("NewArrayWithSize continuation"));
            
            m_out.branch(
                m_out.aboveOrEqual(publicLength, m_out.constInt32(MIN_SPARSE_ARRAY_INDEX)),
                rarely(largeCase), usually(fastCase));

            LBasicBlock lastNext = m_out.appendTo(fastCase, largeCase);
            
            // We don't round up to BASE_VECTOR_LEN for new Array(blah).
            LValue vectorLength = publicLength;
            
            LValue payloadSize =
                m_out.shl(m_out.zeroExt(vectorLength, m_out.intPtr), m_out.constIntPtr(3));
            
            LValue butterflySize = m_out.add(
                payloadSize, m_out.constIntPtr(sizeof(IndexingHeader)));
            
            LValue endOfStorage = allocateBasicStorageAndGetEnd(butterflySize, failCase);
            
            LValue butterfly = m_out.sub(endOfStorage, payloadSize);
            
            LValue object = allocateObject<JSArray>(
                structure, butterfly, failCase);
            
            m_out.store32(publicLength, butterfly, m_heaps.Butterfly_publicLength);
            m_out.store32(vectorLength, butterfly, m_heaps.Butterfly_vectorLength);
            
            if (hasDouble(m_node->indexingType())) {
                LBasicBlock initLoop = FTL_NEW_BLOCK(m_out, ("NewArrayWithSize double init loop"));
                LBasicBlock initDone = FTL_NEW_BLOCK(m_out, ("NewArrayWithSize double init done"));
                
                ValueFromBlock originalIndex = m_out.anchor(vectorLength);
                ValueFromBlock originalPointer = m_out.anchor(butterfly);
                m_out.branch(
                    m_out.notZero32(vectorLength), unsure(initLoop), unsure(initDone));
                
                LBasicBlock initLastNext = m_out.appendTo(initLoop, initDone);
                LValue index = m_out.phi(m_out.int32, originalIndex);
                LValue pointer = m_out.phi(m_out.intPtr, originalPointer);
                
                m_out.store64(
                    m_out.constInt64(bitwise_cast<int64_t>(QNaN)),
                    TypedPointer(m_heaps.indexedDoubleProperties.atAnyIndex(), pointer));
                
                LValue nextIndex = m_out.sub(index, m_out.int32One);
                addIncoming(index, m_out.anchor(nextIndex));
                addIncoming(pointer, m_out.anchor(m_out.add(pointer, m_out.intPtrEight)));
                m_out.branch(
                    m_out.notZero32(nextIndex), unsure(initLoop), unsure(initDone));
                
                m_out.appendTo(initDone, initLastNext);
            }
            
            ValueFromBlock fastResult = m_out.anchor(object);
            m_out.jump(continuation);
            
            m_out.appendTo(largeCase, failCase);
            ValueFromBlock largeStructure = m_out.anchor(m_out.constIntPtr(
                globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage)));
            m_out.jump(slowCase);
            
            m_out.appendTo(failCase, slowCase);
            ValueFromBlock failStructure = m_out.anchor(m_out.constIntPtr(structure));
            m_out.jump(slowCase);
            
            m_out.appendTo(slowCase, continuation);
            LValue structureValue = m_out.phi(
                m_out.intPtr, largeStructure, failStructure);
            ValueFromBlock slowResult = m_out.anchor(vmCall(
                m_out.operation(operationNewArrayWithSize),
                m_callFrame, structureValue, publicLength));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.intPtr, fastResult, slowResult));
            return;
        }
        
        LValue structureValue = m_out.select(
            m_out.aboveOrEqual(publicLength, m_out.constInt32(MIN_SPARSE_ARRAY_INDEX)),
            m_out.constIntPtr(
                globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage)),
            m_out.constIntPtr(structure));
        setJSValue(vmCall(m_out.operation(operationNewArrayWithSize), m_callFrame, structureValue, publicLength));
    }
    
    void compileAllocatePropertyStorage()
    {
        StructureTransitionData& data = m_node->structureTransitionData();
        LValue object = lowCell(m_node->child1());
        
        setStorage(allocatePropertyStorage(object, data.previousStructure));
    }

    void compileReallocatePropertyStorage()
    {
        StructureTransitionData& data = m_node->structureTransitionData();
        LValue object = lowCell(m_node->child1());
        LValue oldStorage = lowStorage(m_node->child2());
        
        setStorage(
            reallocatePropertyStorage(
                object, oldStorage, data.previousStructure, data.newStructure));
    }
    
    void compileToString()
    {
        switch (m_node->child1().useKind()) {
        case StringObjectUse: {
            LValue cell = lowCell(m_node->child1());
            speculateStringObjectForCell(m_node->child1(), cell);
            m_interpreter.filter(m_node->child1(), SpecStringObject);
            
            setJSValue(m_out.loadPtr(cell, m_heaps.JSWrapperObject_internalValue));
            return;
        }
            
        case StringOrStringObjectUse: {
            LValue cell = lowCell(m_node->child1());
            LValue structureID = m_out.load32(cell, m_heaps.JSCell_structureID);
            
            LBasicBlock notString = FTL_NEW_BLOCK(m_out, ("ToString StringOrStringObject not string case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ToString StringOrStringObject continuation"));
            
            ValueFromBlock simpleResult = m_out.anchor(cell);
            m_out.branch(
                m_out.equal(structureID, m_out.constInt32(vm().stringStructure->id())),
                unsure(continuation), unsure(notString));
            
            LBasicBlock lastNext = m_out.appendTo(notString, continuation);
            speculateStringObjectForStructureID(m_node->child1(), structureID);
            ValueFromBlock unboxedResult = m_out.anchor(
                m_out.loadPtr(cell, m_heaps.JSWrapperObject_internalValue));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, simpleResult, unboxedResult));
            
            m_interpreter.filter(m_node->child1(), SpecString | SpecStringObject);
            return;
        }
            
        case CellUse:
        case UntypedUse: {
            LValue value;
            if (m_node->child1().useKind() == CellUse)
                value = lowCell(m_node->child1());
            else
                value = lowJSValue(m_node->child1());
            
            LBasicBlock isCell = FTL_NEW_BLOCK(m_out, ("ToString CellUse/UntypedUse is cell"));
            LBasicBlock notString = FTL_NEW_BLOCK(m_out, ("ToString CellUse/UntypedUse not string"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ToString CellUse/UntypedUse continuation"));
            
            LValue isCellPredicate;
            if (m_node->child1().useKind() == CellUse)
                isCellPredicate = m_out.booleanTrue;
            else
                isCellPredicate = this->isCell(value);
            m_out.branch(isCellPredicate, unsure(isCell), unsure(notString));
            
            LBasicBlock lastNext = m_out.appendTo(isCell, notString);
            ValueFromBlock simpleResult = m_out.anchor(value);
            LValue isStringPredicate;
            if (m_node->child1()->prediction() & SpecString) {
                isStringPredicate = m_out.equal(
                    m_out.load32(value, m_heaps.JSCell_structureID),
                    m_out.constInt32(vm().stringStructure->id()));
            } else
                isStringPredicate = m_out.booleanFalse;
            m_out.branch(isStringPredicate, unsure(continuation), unsure(notString));
            
            m_out.appendTo(notString, continuation);
            LValue operation;
            if (m_node->child1().useKind() == CellUse)
                operation = m_out.operation(operationToStringOnCell);
            else
                operation = m_out.operation(operationToString);
            ValueFromBlock convertedResult = m_out.anchor(vmCall(operation, m_callFrame, value));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, simpleResult, convertedResult));
            return;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileToPrimitive()
    {
        LValue value = lowJSValue(m_node->child1());
        
        LBasicBlock isCellCase = FTL_NEW_BLOCK(m_out, ("ToPrimitive cell case"));
        LBasicBlock isObjectCase = FTL_NEW_BLOCK(m_out, ("ToPrimitive object case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("ToPrimitive continuation"));
        
        Vector<ValueFromBlock, 3> results;
        
        results.append(m_out.anchor(value));
        m_out.branch(isCell(value), unsure(isCellCase), unsure(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(isCellCase, isObjectCase);
        results.append(m_out.anchor(value));
        m_out.branch(isObject(value), unsure(isObjectCase), unsure(continuation));
        
        m_out.appendTo(isObjectCase, continuation);
        results.append(m_out.anchor(vmCall(
            m_out.operation(operationToPrimitive), m_callFrame, value)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, results));
    }
    
    void compileMakeRope()
    {
        LValue kids[3];
        unsigned numKids;
        kids[0] = lowCell(m_node->child1());
        kids[1] = lowCell(m_node->child2());
        if (m_node->child3()) {
            kids[2] = lowCell(m_node->child3());
            numKids = 3;
        } else {
            kids[2] = 0;
            numKids = 2;
        }
        
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("MakeRope slow path"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("MakeRope continuation"));
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        MarkedAllocator& allocator =
            vm().heap.allocatorForObjectWithImmortalStructureDestructor(sizeof(JSRopeString));
        
        LValue result = allocateCell(
            m_out.constIntPtr(&allocator),
            vm().stringStructure.get(),
            slowPath);
        
        m_out.storePtr(m_out.intPtrZero, result, m_heaps.JSString_value);
        for (unsigned i = 0; i < numKids; ++i)
            m_out.storePtr(kids[i], result, m_heaps.JSRopeString_fibers[i]);
        for (unsigned i = numKids; i < JSRopeString::s_maxInternalRopeLength; ++i)
            m_out.storePtr(m_out.intPtrZero, result, m_heaps.JSRopeString_fibers[i]);
        LValue flags = m_out.load32(kids[0], m_heaps.JSString_flags);
        LValue length = m_out.load32(kids[0], m_heaps.JSString_length);
        for (unsigned i = 1; i < numKids; ++i) {
            flags = m_out.bitAnd(flags, m_out.load32(kids[i], m_heaps.JSString_flags));
            length = m_out.add(length, m_out.load32(kids[i], m_heaps.JSString_length));
        }
        m_out.store32(
            m_out.bitAnd(m_out.constInt32(JSString::Is8Bit), flags),
            result, m_heaps.JSString_flags);
        m_out.store32(length, result, m_heaps.JSString_length);
        
        ValueFromBlock fastResult = m_out.anchor(result);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        ValueFromBlock slowResult;
        switch (numKids) {
        case 2:
            slowResult = m_out.anchor(vmCall(
                m_out.operation(operationMakeRope2), m_callFrame, kids[0], kids[1]));
            break;
        case 3:
            slowResult = m_out.anchor(vmCall(
                m_out.operation(operationMakeRope3), m_callFrame, kids[0], kids[1], kids[2]));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
    }
    
    void compileStringCharAt()
    {
        LValue base = lowCell(m_node->child1());
        LValue index = lowInt32(m_node->child2());
        LValue storage = lowStorage(m_node->child3());
            
        LBasicBlock fastPath = FTL_NEW_BLOCK(m_out, ("GetByVal String fast path"));
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("GetByVal String slow path"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("GetByVal String continuation"));
            
        m_out.branch(
            m_out.aboveOrEqual(
                index, m_out.load32NonNegative(base, m_heaps.JSString_length)),
            rarely(slowPath), usually(fastPath));
            
        LBasicBlock lastNext = m_out.appendTo(fastPath, slowPath);
            
        LValue stringImpl = m_out.loadPtr(base, m_heaps.JSString_value);
            
        LBasicBlock is8Bit = FTL_NEW_BLOCK(m_out, ("GetByVal String 8-bit case"));
        LBasicBlock is16Bit = FTL_NEW_BLOCK(m_out, ("GetByVal String 16-bit case"));
        LBasicBlock bitsContinuation = FTL_NEW_BLOCK(m_out, ("GetByVal String bitness continuation"));
        LBasicBlock bigCharacter = FTL_NEW_BLOCK(m_out, ("GetByVal String big character"));
            
        m_out.branch(
            m_out.testIsZero32(
                m_out.load32(stringImpl, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIs8Bit())),
            unsure(is16Bit), unsure(is8Bit));
            
        m_out.appendTo(is8Bit, is16Bit);
            
        ValueFromBlock char8Bit = m_out.anchor(m_out.zeroExt(
            m_out.load8(m_out.baseIndex(
                m_heaps.characters8,
                storage, m_out.zeroExt(index, m_out.intPtr),
                m_state.forNode(m_node->child2()).m_value)),
            m_out.int32));
        m_out.jump(bitsContinuation);
            
        m_out.appendTo(is16Bit, bigCharacter);
            
        ValueFromBlock char16Bit = m_out.anchor(m_out.zeroExt(
            m_out.load16(m_out.baseIndex(
                m_heaps.characters16,
                storage, m_out.zeroExt(index, m_out.intPtr),
                m_state.forNode(m_node->child2()).m_value)),
            m_out.int32));
        m_out.branch(
            m_out.aboveOrEqual(char16Bit.value(), m_out.constInt32(0x100)),
            rarely(bigCharacter), usually(bitsContinuation));
            
        m_out.appendTo(bigCharacter, bitsContinuation);
            
        Vector<ValueFromBlock, 4> results;
        results.append(m_out.anchor(vmCall(
            m_out.operation(operationSingleCharacterString),
            m_callFrame, char16Bit.value())));
        m_out.jump(continuation);
            
        m_out.appendTo(bitsContinuation, slowPath);
            
        LValue character = m_out.phi(m_out.int32, char8Bit, char16Bit);
            
        LValue smallStrings = m_out.constIntPtr(vm().smallStrings.singleCharacterStrings());
            
        results.append(m_out.anchor(m_out.loadPtr(m_out.baseIndex(
            m_heaps.singleCharacterStrings, smallStrings,
            m_out.zeroExt(character, m_out.intPtr)))));
        m_out.jump(continuation);
            
        m_out.appendTo(slowPath, continuation);
            
        if (m_node->arrayMode().isInBounds()) {
            speculate(OutOfBounds, noValue(), 0, m_out.booleanTrue);
            results.append(m_out.anchor(m_out.intPtrZero));
        } else {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
                
            if (globalObject->stringPrototypeChainIsSane()) {
                LBasicBlock negativeIndex = FTL_NEW_BLOCK(m_out, ("GetByVal String negative index"));
                    
                results.append(m_out.anchor(m_out.constInt64(JSValue::encode(jsUndefined()))));
                m_out.branch(
                    m_out.lessThan(index, m_out.int32Zero),
                    rarely(negativeIndex), usually(continuation));
                    
                m_out.appendTo(negativeIndex, continuation);
            }
                
            results.append(m_out.anchor(vmCall(
                m_out.operation(operationGetByValStringInt), m_callFrame, base, index)));
        }
            
        m_out.jump(continuation);
            
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, results));
    }
    
    void compileStringCharCodeAt()
    {
        LBasicBlock is8Bit = FTL_NEW_BLOCK(m_out, ("StringCharCodeAt 8-bit case"));
        LBasicBlock is16Bit = FTL_NEW_BLOCK(m_out, ("StringCharCodeAt 16-bit case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("StringCharCodeAt continuation"));

        LValue base = lowCell(m_node->child1());
        LValue index = lowInt32(m_node->child2());
        LValue storage = lowStorage(m_node->child3());
        
        speculate(
            Uncountable, noValue(), 0,
            m_out.aboveOrEqual(
                index, m_out.load32NonNegative(base, m_heaps.JSString_length)));
        
        LValue stringImpl = m_out.loadPtr(base, m_heaps.JSString_value);
        
        m_out.branch(
            m_out.testIsZero32(
                m_out.load32(stringImpl, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIs8Bit())),
            unsure(is16Bit), unsure(is8Bit));
            
        LBasicBlock lastNext = m_out.appendTo(is8Bit, is16Bit);
            
        ValueFromBlock char8Bit = m_out.anchor(m_out.zeroExt(
            m_out.load8(m_out.baseIndex(
                m_heaps.characters8,
                storage, m_out.zeroExt(index, m_out.intPtr),
                m_state.forNode(m_node->child2()).m_value)),
            m_out.int32));
        m_out.jump(continuation);
            
        m_out.appendTo(is16Bit, continuation);
            
        ValueFromBlock char16Bit = m_out.anchor(m_out.zeroExt(
            m_out.load16(m_out.baseIndex(
                m_heaps.characters16,
                storage, m_out.zeroExt(index, m_out.intPtr),
                m_state.forNode(m_node->child2()).m_value)),
            m_out.int32));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        setInt32(m_out.phi(m_out.int32, char8Bit, char16Bit));
    }
    
    void compileGetByOffset()
    {
        StorageAccessData& data =
            m_graph.m_storageAccessData[m_node->storageAccessDataIndex()];
        
        setJSValue(loadProperty(
            lowStorage(m_node->child1()), data.identifierNumber, data.offset));
    }
    
    void compileMultiGetByOffset()
    {
        LValue base = lowCell(m_node->child1());
        
        MultiGetByOffsetData& data = m_node->multiGetByOffsetData();
        
        Vector<LBasicBlock, 2> blocks(data.variants.size());
        for (unsigned i = data.variants.size(); i--;)
            blocks[i] = FTL_NEW_BLOCK(m_out, ("MultiGetByOffset case ", i));
        LBasicBlock exit = FTL_NEW_BLOCK(m_out, ("MultiGetByOffset fail"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("MultiGetByOffset continuation"));
        
        Vector<SwitchCase, 2> cases;
        for (unsigned i = data.variants.size(); i--;) {
            GetByIdVariant variant = data.variants[i];
            for (unsigned j = variant.structureSet().size(); j--;) {
                cases.append(SwitchCase(
                    weakStructure(variant.structureSet()[j]), blocks[i], Weight(1)));
            }
        }
        m_out.switchInstruction(
            m_out.load32(base, m_heaps.JSCell_structureID), cases, exit, Weight(0));
        
        LBasicBlock lastNext = m_out.m_nextBlock;
        
        Vector<ValueFromBlock, 2> results;
        for (unsigned i = data.variants.size(); i--;) {
            m_out.appendTo(blocks[i], i + 1 < data.variants.size() ? blocks[i + 1] : exit);
            
            GetByIdVariant variant = data.variants[i];
            LValue result;
            if (variant.specificValue())
                result = m_out.constInt64(JSValue::encode(variant.specificValue()));
            else {
                LValue propertyBase;
                if (variant.chain())
                    propertyBase = weakPointer(variant.chain()->terminalPrototype());
                else
                    propertyBase = base;
                if (!isInlineOffset(variant.offset()))
                    propertyBase = m_out.loadPtr(propertyBase, m_heaps.JSObject_butterfly);
                result = loadProperty(propertyBase, data.identifierNumber, variant.offset());
            }
            
            results.append(m_out.anchor(result));
            m_out.jump(continuation);
        }
        
        m_out.appendTo(exit, continuation);
        terminate(BadCache);
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, results));
    }
    
    void compilePutByOffset()
    {
        StorageAccessData& data =
            m_graph.m_storageAccessData[m_node->storageAccessDataIndex()];
        
        storeProperty(
            lowJSValue(m_node->child3()),
            lowStorage(m_node->child1()), data.identifierNumber, data.offset);
    }
    
    void compileMultiPutByOffset()
    {
        LValue base = lowCell(m_node->child1());
        LValue value = lowJSValue(m_node->child2());
        
        MultiPutByOffsetData& data = m_node->multiPutByOffsetData();
        
        Vector<LBasicBlock, 2> blocks(data.variants.size());
        for (unsigned i = data.variants.size(); i--;)
            blocks[i] = FTL_NEW_BLOCK(m_out, ("MultiPutByOffset case ", i));
        LBasicBlock exit = FTL_NEW_BLOCK(m_out, ("MultiPutByOffset fail"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("MultiPutByOffset continuation"));
        
        Vector<SwitchCase, 2> cases;
        for (unsigned i = data.variants.size(); i--;) {
            PutByIdVariant variant = data.variants[i];
            cases.append(
                SwitchCase(weakStructure(variant.oldStructure()), blocks[i], Weight(1)));
        }
        m_out.switchInstruction(
            m_out.load32(base, m_heaps.JSCell_structureID), cases, exit, Weight(0));
        
        LBasicBlock lastNext = m_out.m_nextBlock;
        
        for (unsigned i = data.variants.size(); i--;) {
            m_out.appendTo(blocks[i], i  + 1 < data.variants.size() ? blocks[i + 1] : exit);
            
            PutByIdVariant variant = data.variants[i];
            
            LValue storage;
            if (variant.kind() == PutByIdVariant::Replace) {
                if (isInlineOffset(variant.offset()))
                    storage = base;
                else
                    storage = m_out.loadPtr(base, m_heaps.JSObject_butterfly);
            } else {
                m_graph.m_plan.transitions.addLazily(
                    codeBlock(), m_node->origin.semantic.codeOriginOwner(),
                    variant.oldStructure(), variant.newStructure());
                
                storage = storageForTransition(
                    base, variant.offset(), variant.oldStructure(), variant.newStructure());

                ASSERT(variant.oldStructure()->indexingType() == variant.newStructure()->indexingType());
                ASSERT(variant.oldStructure()->typeInfo().inlineTypeFlags() == variant.newStructure()->typeInfo().inlineTypeFlags());
                ASSERT(variant.oldStructure()->typeInfo().type() == variant.newStructure()->typeInfo().type());
                m_out.store32(
                    weakStructure(variant.newStructure()), base, m_heaps.JSCell_structureID);
            }
            
            storeProperty(value, storage, data.identifierNumber, variant.offset());
            m_out.jump(continuation);
        }
        
        m_out.appendTo(exit, continuation);
        terminate(BadCache);
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
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
    
    void compileNotifyWrite()
    {
        VariableWatchpointSet* set = m_node->variableWatchpointSet();
        
        LValue value = lowJSValue(m_node->child1());
        
        LBasicBlock isNotInvalidated = FTL_NEW_BLOCK(m_out, ("NotifyWrite not invalidated case"));
        LBasicBlock isClear = FTL_NEW_BLOCK(m_out, ("NotifyWrite clear case"));
        LBasicBlock isWatched = FTL_NEW_BLOCK(m_out, ("NotifyWrite watched case"));
        LBasicBlock invalidate = FTL_NEW_BLOCK(m_out, ("NotifyWrite invalidate case"));
        LBasicBlock invalidateFast = FTL_NEW_BLOCK(m_out, ("NotifyWrite invalidate fast case"));
        LBasicBlock invalidateSlow = FTL_NEW_BLOCK(m_out, ("NotifyWrite invalidate slow case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("NotifyWrite continuation"));
        
        LValue state = m_out.load8(m_out.absolute(set->addressOfState()));
        
        m_out.branch(
            m_out.equal(state, m_out.constInt8(IsInvalidated)),
            usually(continuation), rarely(isNotInvalidated));
        
        LBasicBlock lastNext = m_out.appendTo(isNotInvalidated, isClear);

        LValue isClearValue;
        if (set->state() == ClearWatchpoint)
            isClearValue = m_out.equal(state, m_out.constInt8(ClearWatchpoint));
        else
            isClearValue = m_out.booleanFalse;
        m_out.branch(isClearValue, unsure(isClear), unsure(isWatched));
        
        m_out.appendTo(isClear, isWatched);
        
        m_out.store64(value, m_out.absolute(set->addressOfInferredValue()));
        m_out.store8(m_out.constInt8(IsWatched), m_out.absolute(set->addressOfState()));
        m_out.jump(continuation);
        
        m_out.appendTo(isWatched, invalidate);
        
        m_out.branch(
            m_out.equal(value, m_out.load64(m_out.absolute(set->addressOfInferredValue()))),
            unsure(continuation), unsure(invalidate));
        
        m_out.appendTo(invalidate, invalidateFast);
        
        m_out.branch(
            m_out.notZero8(m_out.load8(m_out.absolute(set->addressOfSetIsNotEmpty()))),
            rarely(invalidateSlow), usually(invalidateFast));
        
        m_out.appendTo(invalidateFast, invalidateSlow);
        
        m_out.store64(
            m_out.constInt64(JSValue::encode(JSValue())),
            m_out.absolute(set->addressOfInferredValue()));
        m_out.store8(m_out.constInt8(IsInvalidated), m_out.absolute(set->addressOfState()));
        m_out.jump(continuation);
        
        m_out.appendTo(invalidateSlow, continuation);
        
        vmCall(m_out.operation(operationInvalidate), m_callFrame, m_out.constIntPtr(set));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compileGetCallee()
    {
        setJSValue(m_out.loadPtr(addressFor(JSStack::Callee)));
    }
    
    void compileGetScope()
    {
        setJSValue(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSFunction_scope));
    }
    
    void compileGetMyScope()
    {
        setJSValue(m_out.loadPtr(addressFor(
            m_node->origin.semantic.stackOffset() + JSStack::ScopeChain)));
    }
    
    void compileSkipScope()
    {
        setJSValue(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSScope_next));
    }
    
    void compileGetClosureRegisters()
    {
        if (WriteBarrierBase<Unknown>* registers = m_graph.tryGetRegisters(m_node->child1().node())) {
            setStorage(m_out.constIntPtr(registers));
            return;
        }
        
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
            || m_node->isBinaryUseKind(MachineIntUse)
            || m_node->isBinaryUseKind(NumberUse)
            || m_node->isBinaryUseKind(ObjectUse)) {
            compileCompareStrictEq();
            return;
        }
        
        if (m_node->child1().useKind() == ObjectUse
            && m_node->child2().useKind() == ObjectOrOtherUse) {
            compareEqObjectOrOtherToObject(m_node->child2(), m_node->child1());
            return;
        }
        
        if (m_node->child1().useKind() == ObjectOrOtherUse
            && m_node->child2().useKind() == ObjectUse) {
            compareEqObjectOrOtherToObject(m_node->child1(), m_node->child2());
            return;
        }
        
        if (m_node->isBinaryUseKind(UntypedUse)) {
            nonSpeculativeCompare(LLVMIntEQ, operationCompareEq);
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compileCompareEqConstant()
    {
        ASSERT(m_graph.valueOfJSConstant(m_node->child2().node()).isNull());
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
        
        if (m_node->isBinaryUseKind(MachineIntUse)) {
            Int52Kind kind;
            LValue left = lowWhicheverInt52(m_node->child1(), kind);
            LValue right = lowInt52(m_node->child2(), kind);
            setBoolean(m_out.equal(left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            setBoolean(
                m_out.doubleEqual(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(ObjectUse)) {
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
            && !masqueradesAsUndefinedWatchpointIsStillValid()) {
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
        compare(LLVMIntSLT, LLVMRealOLT, operationCompareLess);
    }
    
    void compileCompareLessEq()
    {
        compare(LLVMIntSLE, LLVMRealOLE, operationCompareLessEq);
    }
    
    void compileCompareGreater()
    {
        compare(LLVMIntSGT, LLVMRealOGT, operationCompareGreater);
    }
    
    void compileCompareGreaterEq()
    {
        compare(LLVMIntSGE, LLVMRealOGE, operationCompareGreaterEq);
    }
    
    void compileLogicalNot()
    {
        setBoolean(m_out.bitNot(boolify(m_node->child1())));
    }
    
    void compileCallOrConstruct()
    {
        int dummyThisArgument = m_node->op() == Call ? 0 : 1;
        int numPassedArgs = m_node->numChildren() - 1;
        int numArgs = numPassedArgs + dummyThisArgument;
        
        LValue callee = lowJSValue(m_graph.varArgChild(m_node, 0));
        
        unsigned stackmapID = m_stackmapIDs++;
        
        Vector<LValue> arguments;
        arguments.append(m_out.constInt64(stackmapID));
        arguments.append(m_out.constInt32(sizeOfCall()));
        arguments.append(constNull(m_out.ref8));
        arguments.append(m_out.constInt32(1 + JSStack::CallFrameHeaderSize - JSStack::CallerFrameAndPCSize + numArgs));
        arguments.append(callee); // callee -> %rax
        arguments.append(getUndef(m_out.int64)); // code block
        arguments.append(getUndef(m_out.int64)); // scope chain
        arguments.append(callee); // callee -> stack
        arguments.append(m_out.constInt64(numArgs)); // argument count and zeros for the tag
        if (dummyThisArgument)
            arguments.append(getUndef(m_out.int64));
        for (int i = 0; i < numPassedArgs; ++i)
            arguments.append(lowJSValue(m_graph.varArgChild(m_node, 1 + i)));
        
        callPreflight();
        
        LValue call = m_out.call(m_out.patchpointInt64Intrinsic(), arguments);
        setInstructionCallingConvention(call, LLVMWebKitJSCallConv);
        
        m_ftlState.jsCalls.append(JSCall(stackmapID, m_node));
        
        setJSValue(call);
    }
    
    void compileJump()
    {
        m_out.jump(lowBlock(m_node->targetBlock()));
    }
    
    void compileBranch()
    {
        m_out.branch(
            boolify(m_node->child1()),
            WeightedTarget(
                lowBlock(m_node->branchData()->taken.block),
                m_node->branchData()->taken.count),
            WeightedTarget(
                lowBlock(m_node->branchData()->notTaken.block),
                m_node->branchData()->notTaken.count));
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
                m_out.branch(isNotInt32(boxedValue), unsure(isNotInt), unsure(isInt));
                
                LBasicBlock innerLastNext = m_out.appendTo(isInt, isNotInt);
                
                intValues.append(m_out.anchor(unboxInt32(boxedValue)));
                m_out.jump(switchOnInts);
                
                m_out.appendTo(isNotInt, isDouble);
                m_out.branch(
                    isCellOrMisc(boxedValue),
                    usually(lowBlock(data->fallThrough.block)), rarely(isDouble));
                
                m_out.appendTo(isDouble, innerLastNext);
                LValue doubleValue = unboxDouble(boxedValue);
                LValue intInDouble = m_out.fpToInt32(doubleValue);
                intValues.append(m_out.anchor(intInDouble));
                m_out.branch(
                    m_out.doubleEqual(m_out.intToDouble(intInDouble), doubleValue),
                    unsure(switchOnInts), unsure(lowBlock(data->fallThrough.block)));
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
            
            // FIXME: We should use something other than unsure() for the branch weight
            // of the fallThrough block. The main challenge is just that we have multiple
            // branches to fallThrough but a single count, so we would need to divvy it up
            // among the different lowered branches.
            // https://bugs.webkit.org/show_bug.cgi?id=129082
            
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
                    isNotCell(unboxedValue),
                    unsure(lowBlock(data->fallThrough.block)), unsure(isCellCase));
                
                LBasicBlock lastNext = m_out.appendTo(isCellCase, isStringCase);
                LValue cellValue = unboxedValue;
                m_out.branch(
                    isNotString(cellValue),
                    unsure(lowBlock(data->fallThrough.block)), unsure(isStringCase));
                
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
                    m_out.load32NonNegative(stringValue, m_heaps.JSString_length),
                    m_out.int32One),
                unsure(lowBlock(data->fallThrough.block)), unsure(lengthIs1));
            
            LBasicBlock lastNext = m_out.appendTo(lengthIs1, needResolution);
            Vector<ValueFromBlock, 2> values;
            LValue fastValue = m_out.loadPtr(stringValue, m_heaps.JSString_value);
            values.append(m_out.anchor(fastValue));
            m_out.branch(m_out.isNull(fastValue), rarely(needResolution), usually(resolved));
            
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
                unsure(is8Bit), unsure(is16Bit));
            
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
    
    void compileInvalidationPoint()
    {
        if (verboseCompilationEnabled())
            dataLog("    Invalidation point with availability: ", m_availability, "\n");
        
        m_ftlState.jitCode->osrExit.append(OSRExit(
            UncountableInvalidation, InvalidValueFormat, MethodOfGettingAValueProfile(),
            m_codeOriginForExitTarget, m_codeOriginForExitProfile,
            m_availability.numberOfArguments(), m_availability.numberOfLocals()));
        m_ftlState.finalizer->osrExit.append(OSRExitCompilationInfo());
        
        OSRExit& exit = m_ftlState.jitCode->osrExit.last();
        OSRExitCompilationInfo& info = m_ftlState.finalizer->osrExit.last();
        
        ExitArgumentList arguments;
        
        buildExitArguments(exit, arguments, FormattedValue(), exit.m_codeOrigin);
        callStackmap(exit, arguments);
        
        info.m_isInvalidationPoint = true;
    }
    
    void compileCheckArgumentsNotCreated()
    {
        ASSERT(!isEmptySpeculation(
            m_state.variables().operand(
                m_graph.argumentsRegisterFor(m_node->origin.semantic)).m_type));
        
        checkArgumentsNotCreated();
    }
    
    void compileCountExecution()
    {
        TypedPointer counter = m_out.absolute(m_node->executionCounter()->address());
        m_out.store64(m_out.add(m_out.load64(counter), m_out.constInt64(1)), counter);
    }
    
    LValue didOverflowStack()
    {
        // This does a very simple leaf function analysis. The invariant of FTL call
        // frames is that the caller had already done enough of a stack check to
        // prove that this call frame has enough stack to run, and also enough stack
        // to make runtime calls. So, we only need to stack check when making calls
        // to other JS functions. If we don't find such calls then we don't need to
        // do any stack checks.
        
        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            
            for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                Node* node = block->at(nodeIndex);
                
                switch (node->op()) {
                case GetById:
                case PutById:
                case Call:
                case Construct:
                    return m_out.below(
                        m_callFrame,
                        m_out.loadPtr(
                            m_out.absolute(vm().addressOfFTLStackLimit())));
                    
                default:
                    break;
                }
            }
        }
        
        return m_out.booleanFalse;
    }
    
    LValue loadProperty(LValue storage, unsigned identifierNumber, PropertyOffset offset)
    {
        return m_out.load64(addressOfProperty(storage, identifierNumber, offset));
    }
    
    void storeProperty(
        LValue value, LValue storage, unsigned identifierNumber, PropertyOffset offset)
    {
        m_out.store64(value, addressOfProperty(storage, identifierNumber, offset));
    }
    
    TypedPointer addressOfProperty(
        LValue storage, unsigned identifierNumber, PropertyOffset offset)
    {
        return m_out.address(
            m_heaps.properties[identifierNumber], storage, offsetRelativeToBase(offset));
    }
    
    LValue storageForTransition(
        LValue object, PropertyOffset offset,
        Structure* previousStructure, Structure* nextStructure)
    {
        if (isInlineOffset(offset))
            return object;
        
        if (previousStructure->outOfLineCapacity() == nextStructure->outOfLineCapacity())
            return m_out.loadPtr(object, m_heaps.JSObject_butterfly);
        
        LValue result;
        if (!previousStructure->outOfLineCapacity())
            result = allocatePropertyStorage(object, previousStructure);
        else {
            result = reallocatePropertyStorage(
                object, m_out.loadPtr(object, m_heaps.JSObject_butterfly),
                previousStructure, nextStructure);
        }
        
        emitStoreBarrier(object);
        
        return result;
    }
    
    LValue allocatePropertyStorage(LValue object, Structure* previousStructure)
    {
        if (previousStructure->couldHaveIndexingHeader()) {
            return vmCall(
                m_out.operation(
                    operationReallocateButterflyToHavePropertyStorageWithInitialCapacity),
                m_callFrame, object);
        }
        
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("allocatePropertyStorage slow path"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("allocatePropertyStorage continuation"));
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        LValue endOfStorage = allocateBasicStorageAndGetEnd(
            m_out.constIntPtr(initialOutOfLineCapacity * sizeof(JSValue)), slowPath);
        
        ValueFromBlock fastButterfly = m_out.anchor(
            m_out.add(m_out.constIntPtr(sizeof(IndexingHeader)), endOfStorage));
        
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        
        ValueFromBlock slowButterfly = m_out.anchor(vmCall(
            m_out.operation(operationAllocatePropertyStorageWithInitialCapacity), m_callFrame));
        
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        LValue result = m_out.phi(m_out.intPtr, fastButterfly, slowButterfly);
        m_out.storePtr(result, object, m_heaps.JSObject_butterfly);
        
        return result;
    }
    
    LValue reallocatePropertyStorage(
        LValue object, LValue oldStorage, Structure* previous, Structure* next)
    {
        size_t oldSize = previous->outOfLineCapacity() * sizeof(JSValue);
        size_t newSize = oldSize * outOfLineGrowthFactor; 

        ASSERT_UNUSED(next, newSize == next->outOfLineCapacity() * sizeof(JSValue));
        
        if (previous->couldHaveIndexingHeader()) {
            LValue newAllocSize = m_out.constInt64(newSize / sizeof(JSValue));                    
            return vmCall(m_out.operation(operationReallocateButterflyToGrowPropertyStorage), m_callFrame, object, newAllocSize);
        }
        
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("reallocatePropertyStorage slow path"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("reallocatePropertyStorage continuation"));
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        LValue endOfStorage = 
            allocateBasicStorageAndGetEnd(m_out.constIntPtr(newSize), slowPath);
        
        ValueFromBlock fastButterfly = m_out.anchor(m_out.add(m_out.constIntPtr(sizeof(IndexingHeader)), endOfStorage));
        
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        
        LValue newAllocSize = m_out.constInt64(newSize / sizeof(JSValue));       
        
        LValue storageLocation = vmCall(m_out.operation(operationAllocatePropertyStorage), m_callFrame, newAllocSize);
        
        ValueFromBlock slowButterfly = m_out.anchor(storageLocation);
        
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        LValue result = m_out.phi(m_out.intPtr, fastButterfly, slowButterfly);

        ptrdiff_t headerSize = -sizeof(JSValue) - sizeof(void *);
        ptrdiff_t endStorage = headerSize - static_cast<ptrdiff_t>(oldSize);

        for (ptrdiff_t offset = headerSize; offset > endStorage; offset -= sizeof(void*)) {
            LValue loaded = 
                m_out.loadPtr(m_out.address(m_heaps.properties.atAnyNumber(), oldStorage, offset));
            m_out.storePtr(loaded, m_out.address(m_heaps.properties.atAnyNumber(), result, offset));
        }
        
        m_out.storePtr(result, m_out.address(object, m_heaps.JSObject_butterfly));
        
        return result;
    }
    
    LValue getById(LValue base)
    {
        StringImpl* uid = m_graph.identifiers()[m_node->identifierNumber()];

        // Arguments: id, bytes, target, numArgs, args...
        unsigned stackmapID = m_stackmapIDs++;
        
        if (Options::verboseCompilation())
            dataLog("    Emitting GetById patchpoint with stackmap #", stackmapID, "\n");
        
        LValue call = m_out.call(
            m_out.patchpointInt64Intrinsic(),
            m_out.constInt64(stackmapID), m_out.constInt32(sizeOfGetById()),
            constNull(m_out.ref8), m_out.constInt32(1), base);
        setInstructionCallingConvention(call, LLVMAnyRegCallConv);
        
        m_ftlState.getByIds.append(GetByIdDescriptor(stackmapID, m_node->origin.semantic, uid));
        
        return call;
    }
    
    TypedPointer baseIndex(IndexedAbstractHeap& heap, LValue storage, LValue index, Edge edge)
    {
        return m_out.baseIndex(
            heap, storage, m_out.zeroExt(index, m_out.intPtr),
            m_state.forNode(edge).m_value);
    }
    
    void compare(
        LIntPredicate intCondition, LRealPredicate realCondition,
        S_JITOperation_EJJ helperFunction)
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            LValue left = lowInt32(m_node->child1());
            LValue right = lowInt32(m_node->child2());
            setBoolean(m_out.icmp(intCondition, left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(MachineIntUse)) {
            Int52Kind kind;
            LValue left = lowWhicheverInt52(m_node->child1(), kind);
            LValue right = lowInt52(m_node->child2(), kind);
            setBoolean(m_out.icmp(intCondition, left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(NumberUse)) {
            LValue left = lowDouble(m_node->child1());
            LValue right = lowDouble(m_node->child2());
            setBoolean(m_out.fcmp(realCondition, left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(UntypedUse)) {
            nonSpeculativeCompare(intCondition, helperFunction);
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    void compareEqObjectOrOtherToObject(Edge leftChild, Edge rightChild)
    {
        LValue rightCell = lowCell(rightChild);
        LValue leftValue = lowJSValue(leftChild, ManualOperandSpeculation);
        
        speculateTruthyObject(rightChild, rightCell, SpecObject);
        
        LBasicBlock leftCellCase = FTL_NEW_BLOCK(m_out, ("CompareEqObjectOrOtherToObject left cell case"));
        LBasicBlock leftNotCellCase = FTL_NEW_BLOCK(m_out, ("CompareEqObjectOrOtherToObject left not cell case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("CompareEqObjectOrOtherToObject continuation"));
        
        m_out.branch(isCell(leftValue), unsure(leftCellCase), unsure(leftNotCellCase));
        
        LBasicBlock lastNext = m_out.appendTo(leftCellCase, leftNotCellCase);
        speculateTruthyObject(leftChild, leftValue, SpecObject | (~SpecCell));
        ValueFromBlock cellResult = m_out.anchor(m_out.equal(rightCell, leftValue));
        m_out.jump(continuation);
        
        m_out.appendTo(leftNotCellCase, continuation);
        FTL_TYPE_CHECK(
            jsValueValue(leftValue), leftChild, SpecOther | SpecCell, isNotNully(leftValue));
        ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(m_out.boolean, cellResult, notCellResult));
    }
    
    void speculateTruthyObject(Edge edge, LValue cell, SpeculatedType filter)
    {
        if (masqueradesAsUndefinedWatchpointIsStillValid()) {
            FTL_TYPE_CHECK(jsValueValue(cell), edge, filter, isNotObject(cell));
            return;
        }
        
        LValue structureID = m_out.load32(cell, m_heaps.JSCell_structureID);
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, filter,
            m_out.equal(structureID, m_out.constInt32(vm().stringStructure->id())));
        speculate(
            BadType, jsValueValue(cell), edge.node(),
            m_out.testNonZero8(
                m_out.load8(cell, m_heaps.JSCell_typeInfoFlags),
                m_out.constInt8(MasqueradesAsUndefined)));
    }
    
    void nonSpeculativeCompare(LIntPredicate intCondition, S_JITOperation_EJJ helperFunction)
    {
        LValue left = lowJSValue(m_node->child1());
        LValue right = lowJSValue(m_node->child2());
        
        LBasicBlock leftIsInt = FTL_NEW_BLOCK(m_out, ("CompareEq untyped left is int"));
        LBasicBlock fastPath = FTL_NEW_BLOCK(m_out, ("CompareEq untyped fast path"));
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("CompareEq untyped slow path"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("CompareEq untyped continuation"));
        
        m_out.branch(isNotInt32(left), rarely(slowPath), usually(leftIsInt));
        
        LBasicBlock lastNext = m_out.appendTo(leftIsInt, fastPath);
        m_out.branch(isNotInt32(right), rarely(slowPath), usually(fastPath));
        
        m_out.appendTo(fastPath, slowPath);
        ValueFromBlock fastResult = m_out.anchor(
            m_out.icmp(intCondition, unboxInt32(left), unboxInt32(right)));
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        ValueFromBlock slowResult = m_out.anchor(m_out.notNull(vmCall(
            m_out.operation(helperFunction), m_callFrame, left, right)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(m_out.boolean, fastResult, slowResult));
    }

    LValue allocateCell(LValue allocator, Structure* structure, LBasicBlock slowPath)
    {
        LBasicBlock success = FTL_NEW_BLOCK(m_out, ("object allocation success"));
        
        LValue result = m_out.loadPtr(
            allocator, m_heaps.MarkedAllocator_freeListHead);
        
        m_out.branch(m_out.notNull(result), usually(success), rarely(slowPath));
        
        m_out.appendTo(success);
        
        m_out.storePtr(
            m_out.loadPtr(result, m_heaps.JSCell_freeListNext),
            allocator, m_heaps.MarkedAllocator_freeListHead);
        
        m_out.store32(m_out.constInt32(structure->id()), result, m_heaps.JSCell_structureID);
        m_out.store8(m_out.constInt8(structure->indexingType()), result, m_heaps.JSCell_indexingType);
        m_out.store8(m_out.constInt8(structure->typeInfo().type()), result, m_heaps.JSCell_typeInfoType);
        m_out.store8(m_out.constInt8(structure->typeInfo().inlineTypeFlags()), result, m_heaps.JSCell_typeInfoFlags);
        m_out.store8(m_out.constInt8(0), result, m_heaps.JSCell_gcData);
        
        return result;
    }

    LValue allocateObject(
        LValue allocator, Structure* structure, LValue butterfly, LBasicBlock slowPath)
    {
        LValue result = allocateCell(allocator, structure, slowPath);
        m_out.storePtr(butterfly, result, m_heaps.JSObject_butterfly);
        return result;
    }
    
    template<typename ClassType>
    LValue allocateObject(Structure* structure, LValue butterfly, LBasicBlock slowPath)
    {
        MarkedAllocator* allocator;
        size_t size = ClassType::allocationSize(0);
        if (ClassType::needsDestruction && ClassType::hasImmortalStructure)
            allocator = &vm().heap.allocatorForObjectWithImmortalStructureDestructor(size);
        else if (ClassType::needsDestruction)
            allocator = &vm().heap.allocatorForObjectWithNormalDestructor(size);
        else
            allocator = &vm().heap.allocatorForObjectWithoutDestructor(size);
        return allocateObject(m_out.constIntPtr(allocator), structure, butterfly, slowPath);
    }
    
    // Returns a pointer to the end of the allocation.
    LValue allocateBasicStorageAndGetEnd(LValue size, LBasicBlock slowPath)
    {
        CopiedAllocator& allocator = vm().heap.storageAllocator();
        
        LBasicBlock success = FTL_NEW_BLOCK(m_out, ("storage allocation success"));
        
        LValue remaining = m_out.loadPtr(m_out.absolute(&allocator.m_currentRemaining));
        LValue newRemaining = m_out.sub(remaining, size);
        
        m_out.branch(
            m_out.lessThan(newRemaining, m_out.intPtrZero),
            rarely(slowPath), usually(success));
        
        m_out.appendTo(success);
        
        m_out.storePtr(newRemaining, m_out.absolute(&allocator.m_currentRemaining));
        return m_out.sub(
            m_out.loadPtr(m_out.absolute(&allocator.m_currentPayloadEnd)), newRemaining);
    }
    
    struct ArrayValues {
        ArrayValues()
            : array(0)
            , butterfly(0)
        {
        }
        
        ArrayValues(LValue array, LValue butterfly)
            : array(array)
            , butterfly(butterfly)
        {
        }
        
        LValue array;
        LValue butterfly;
    };
    ArrayValues allocateJSArray(
        Structure* structure, unsigned numElements, LBasicBlock slowPath)
    {
        ASSERT(
            hasUndecided(structure->indexingType())
            || hasInt32(structure->indexingType())
            || hasDouble(structure->indexingType())
            || hasContiguous(structure->indexingType()));
        
        unsigned vectorLength = std::max(BASE_VECTOR_LEN, numElements);
        
        LValue endOfStorage = allocateBasicStorageAndGetEnd(
            m_out.constIntPtr(sizeof(JSValue) * vectorLength + sizeof(IndexingHeader)),
            slowPath);
        
        LValue butterfly = m_out.sub(
            endOfStorage, m_out.constIntPtr(sizeof(JSValue) * vectorLength));
        
        LValue object = allocateObject<JSArray>(
            structure, butterfly, slowPath);
        
        m_out.store32(m_out.constInt32(numElements), butterfly, m_heaps.Butterfly_publicLength);
        m_out.store32(m_out.constInt32(vectorLength), butterfly, m_heaps.Butterfly_vectorLength);
        
        if (hasDouble(structure->indexingType())) {
            for (unsigned i = numElements; i < vectorLength; ++i) {
                m_out.store64(
                    m_out.constInt64(bitwise_cast<int64_t>(QNaN)),
                    butterfly, m_heaps.indexedDoubleProperties[i]);
            }
        }
        
        return ArrayValues(object, butterfly);
    }
    
    ArrayValues allocateJSArray(Structure* structure, unsigned numElements)
    {
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("JSArray allocation slow path"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("JSArray allocation continuation"));
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        ArrayValues fastValues = allocateJSArray(structure, numElements, slowPath);
        ValueFromBlock fastArray = m_out.anchor(fastValues.array);
        ValueFromBlock fastButterfly = m_out.anchor(fastValues.butterfly);
        
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        
        ValueFromBlock slowArray = m_out.anchor(vmCall(
            m_out.operation(operationNewArrayWithSize), m_callFrame,
            m_out.constIntPtr(structure), m_out.constInt32(numElements)));
        ValueFromBlock slowButterfly = m_out.anchor(
            m_out.loadPtr(slowArray.value(), m_heaps.JSObject_butterfly));

        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        return ArrayValues(
            m_out.phi(m_out.intPtr, fastArray, slowArray),
            m_out.phi(m_out.intPtr, fastButterfly, slowButterfly));
    }
    
    LValue typedArrayLength(Edge baseEdge, ArrayMode arrayMode, LValue base)
    {
        if (JSArrayBufferView* view = m_graph.tryGetFoldableView(baseEdge.node(), arrayMode))
            return m_out.constInt32(view->length());
        return m_out.load32NonNegative(base, m_heaps.JSArrayBufferView_length);
    }
    
    LValue typedArrayLength(Edge baseEdge, ArrayMode arrayMode)
    {
        return typedArrayLength(baseEdge, arrayMode, lowCell(baseEdge));
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
        case StringUse: {
            LValue stringValue = lowString(m_node->child1());
            LValue length = m_out.load32NonNegative(stringValue, m_heaps.JSString_length);
            return m_out.notEqual(length, m_out.int32Zero);
        }
        case UntypedUse: {
            LValue value = lowJSValue(m_node->child1());
            
            LBasicBlock slowCase = FTL_NEW_BLOCK(m_out, ("Boolify untyped slow case"));
            LBasicBlock fastCase = FTL_NEW_BLOCK(m_out, ("Boolify untyped fast case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Boolify untyped continuation"));
            
            m_out.branch(isNotBoolean(value), rarely(slowCase), usually(fastCase));
            
            LBasicBlock lastNext = m_out.appendTo(fastCase, slowCase);
            ValueFromBlock fastResult = m_out.anchor(unboxBoolean(value));
            m_out.jump(continuation);
            
            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(m_out.notNull(vmCall(
                m_out.operation(operationConvertJSValueToBoolean), m_callFrame, value)));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            return m_out.phi(m_out.boolean, fastResult, slowResult);
        }
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
        
        m_out.branch(isNotCell(value), unsure(primitiveCase), unsure(cellCase));
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, primitiveCase);
        
        Vector<ValueFromBlock, 3> results;
        
        switch (cellMode) {
        case AllCellsAreFalse:
            break;
        case CellCaseSpeculatesObject:
            FTL_TYPE_CHECK(
                jsValueValue(value), edge, (~SpecCell) | SpecObject,
                m_out.equal(
                    m_out.load32(value, m_heaps.JSCell_structureID),
                    m_out.constInt32(vm().stringStructure->id())));
            break;
        }
        
        if (validWatchpoint) {
            results.append(m_out.anchor(m_out.booleanFalse));
            m_out.jump(continuation);
        } else {
            LBasicBlock masqueradesCase =
                FTL_NEW_BLOCK(m_out, ("EqualNullOrUndefined masquerades case"));
                
            results.append(m_out.anchor(m_out.booleanFalse));
            
            m_out.branch(
                m_out.testNonZero8(
                    m_out.load8(value, m_heaps.JSCell_typeInfoFlags),
                    m_out.constInt8(MasqueradesAsUndefined)),
                rarely(masqueradesCase), usually(continuation));
            
            m_out.appendTo(masqueradesCase, primitiveCase);
            
            LValue structure = loadStructure(value);
            
            results.append(m_out.anchor(
                m_out.equal(
                    m_out.constIntPtr(m_graph.globalObjectFor(m_node->origin.semantic)),
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
            primitiveResult = isNully(value);
            break;
        case SpeculateNullOrUndefined:
            FTL_TYPE_CHECK(
                jsValueValue(value), edge, SpecCell | SpecOther, isNotNully(value));
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
        FunctionType slowPathFunction, LValue base, LValue storage, LValue index, LValue value,
        LBasicBlock continuation)
    {
        LValue isNotInBounds = m_out.aboveOrEqual(
            index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength));
        if (!m_node->arrayMode().isInBounds()) {
            LBasicBlock notInBoundsCase =
                FTL_NEW_BLOCK(m_out, ("PutByVal not in bounds"));
            LBasicBlock performStore =
                FTL_NEW_BLOCK(m_out, ("PutByVal perform store"));
                
            m_out.branch(isNotInBounds, unsure(notInBoundsCase), unsure(performStore));
                
            LBasicBlock lastNext = m_out.appendTo(notInBoundsCase, performStore);
                
            LValue isOutOfBounds = m_out.aboveOrEqual(
                index, m_out.load32NonNegative(storage, m_heaps.Butterfly_vectorLength));
                
            if (!m_node->arrayMode().isOutOfBounds())
                speculate(OutOfBounds, noValue(), 0, isOutOfBounds);
            else {
                LBasicBlock outOfBoundsCase =
                    FTL_NEW_BLOCK(m_out, ("PutByVal out of bounds"));
                LBasicBlock holeCase =
                    FTL_NEW_BLOCK(m_out, ("PutByVal hole case"));
                    
                m_out.branch(isOutOfBounds, unsure(outOfBoundsCase), unsure(holeCase));
                    
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
                lowBlock(data->cases[i].target.block), Weight(data->cases[i].target.count)));
        }
        
        m_out.switchInstruction(
            switchValue, cases,
            lowBlock(data->fallThrough.block), Weight(data->fallThrough.count));
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
            m_out.doubleGreaterThanOrEqual(doubleValue, m_out.constDouble(low)),
            unsure(greatEnough), unsure(slowPath));
        
        LBasicBlock lastNext = m_out.appendTo(greatEnough, withinRange);
        m_out.branch(
            m_out.doubleLessThanOrEqual(doubleValue, m_out.constDouble(high)),
            unsure(withinRange), unsure(slowPath));
        
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
        if (Output::hasSensibleDoubleToInt())
            return sensibleDoubleToInt32(doubleValue);
        
        double limit = pow(2, 31) - 1;
        return doubleToInt32(doubleValue, -limit, limit);
    }
    
    LValue sensibleDoubleToInt32(LValue doubleValue)
    {
        LBasicBlock slowPath = FTL_NEW_BLOCK(m_out, ("sensible doubleToInt32 slow path"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("sensible doubleToInt32 continuation"));
        
        ValueFromBlock fastResult = m_out.anchor(
            m_out.sensibleDoubleToInt(doubleValue));
        m_out.branch(
            m_out.equal(fastResult.value(), m_out.constInt32(0x80000000)),
            rarely(slowPath), usually(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(slowPath, continuation);
        ValueFromBlock slowResult = m_out.anchor(
            m_out.call(m_out.operation(toInt32), doubleValue));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(m_out.int32, fastResult, slowResult);
    }
    
    void checkArgumentsNotCreated()
    {
        CodeOrigin codeOrigin = m_node->origin.semantic;
        VirtualRegister argumentsRegister = m_graph.argumentsRegisterFor(codeOrigin);
        if (isEmptySpeculation(m_state.variables().operand(argumentsRegister).m_type))
            return;
        
        VirtualRegister argsReg = m_graph.machineArgumentsRegisterFor(codeOrigin);
        speculate(
            ArgumentsEscaped, noValue(), 0,
            m_out.notZero64(m_out.load64(addressFor(argsReg))));
    }
    
    void speculate(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition)
    {
        appendOSRExit(kind, lowValue, highValue, failCondition);
    }
    
    void terminate(ExitKind kind)
    {
        speculate(kind, noValue(), 0, m_out.booleanTrue);
    }
    
    void typeCheck(
        FormattedValue lowValue, Edge highValue, SpeculatedType typesPassedThrough,
        LValue failCondition)
    {
        appendTypeCheck(lowValue, highValue, typesPassedThrough, failCondition);
    }
    
    void appendTypeCheck(
        FormattedValue lowValue, Edge highValue, SpeculatedType typesPassedThrough,
        LValue failCondition)
    {
        if (!m_interpreter.needsTypeCheck(highValue, typesPassedThrough))
            return;
        ASSERT(mayHaveTypeCheck(highValue.useKind()));
        appendOSRExit(BadType, lowValue, highValue.node(), failCondition);
        m_interpreter.filter(highValue, typesPassedThrough);
    }
    
    LValue lowInt32(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || (edge.useKind() == Int32Use || edge.useKind() == KnownInt32Use));
        
        if (edge->hasConstant()) {
            JSValue value = m_graph.valueOfJSConstant(edge.node());
            if (!value.isInt32()) {
                terminate(Uncountable);
                return m_out.int32Zero;
            }
            return m_out.constInt32(value.asInt32());
        }
        
        LoweredNodeValue value = m_int32Values.get(edge.node());
        if (isValid(value))
            return value.value();
        
        value = m_strictInt52Values.get(edge.node());
        if (isValid(value))
            return strictInt52ToInt32(edge, value.value());
        
        value = m_int52Values.get(edge.node());
        if (isValid(value))
            return strictInt52ToInt32(edge, int52ToStrictInt52(value.value()));
        
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
    
    enum Int52Kind { StrictInt52, Int52 };
    LValue lowInt52(Edge edge, Int52Kind kind, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == MachineIntUse);
        
        if (edge->hasConstant()) {
            JSValue value = m_graph.valueOfJSConstant(edge.node());
            if (!value.isMachineInt()) {
                terminate(Uncountable);
                return m_out.int64Zero;
            }
            int64_t result = value.asMachineInt();
            if (kind == Int52)
                result <<= JSValue::int52ShiftAmount;
            return m_out.constInt64(result);
        }
        
        LoweredNodeValue value;
        
        switch (kind) {
        case Int52:
            value = m_int52Values.get(edge.node());
            if (isValid(value))
                return value.value();
            
            value = m_strictInt52Values.get(edge.node());
            if (isValid(value))
                return strictInt52ToInt52(value.value());
            break;
            
        case StrictInt52:
            value = m_strictInt52Values.get(edge.node());
            if (isValid(value))
                return value.value();
            
            value = m_int52Values.get(edge.node());
            if (isValid(value))
                return int52ToStrictInt52(value.value());
            break;
        }
        
        value = m_int32Values.get(edge.node());
        if (isValid(value)) {
            return setInt52WithStrictValue(
                edge.node(), m_out.signExt(value.value(), m_out.int64), kind);
        }
        
        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecInt52));
        
        value = m_jsValueValues.get(edge.node());
        if (isValid(value)) {
            LValue boxedResult = value.value();
            FTL_TYPE_CHECK(
                jsValueValue(boxedResult), edge, SpecMachineInt, isNotInt32(boxedResult));
            return setInt52WithStrictValue(
                edge.node(), m_out.signExt(unboxInt32(boxedResult), m_out.int64), kind);
        }
        
        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecMachineInt));
        terminate(Uncountable);
        return m_out.int64Zero;
    }
    
    LValue lowInt52(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        return lowInt52(edge, Int52, mode);
    }
    
    LValue lowStrictInt52(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        return lowInt52(edge, StrictInt52, mode);
    }
    
    bool betterUseStrictInt52(Node* node)
    {
        return !isValid(m_int52Values.get(node));
    }
    bool betterUseStrictInt52(Edge edge)
    {
        return betterUseStrictInt52(edge.node());
    }
    template<typename T>
    Int52Kind bestInt52Kind(T node)
    {
        return betterUseStrictInt52(node) ? StrictInt52 : Int52;
    }
    Int52Kind opposite(Int52Kind kind)
    {
        switch (kind) {
        case Int52:
            return StrictInt52;
        case StrictInt52:
            return Int52;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    LValue lowWhicheverInt52(Edge edge, Int52Kind& kind, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        kind = bestInt52Kind(edge);
        return lowInt52(edge, kind, mode);
    }
    
    LValue lowCell(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || DFG::isCell(edge.useKind()));
        
        if (edge->op() == JSConstant) {
            JSValue value = m_graph.valueOfJSConstant(edge.node());
            if (!value.isCell()) {
                terminate(Uncountable);
                return m_out.intPtrZero;
            }
            return m_out.constIntPtr(value.asCell());
        }
        
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
        
        if (edge->hasConstant()) {
            JSValue value = m_graph.valueOfJSConstant(edge.node());
            if (!value.isBoolean()) {
                terminate(Uncountable);
                return m_out.booleanFalse;
            }
            return m_out.constBool(value.asBoolean());
        }
        
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
        
        if (edge->hasConstant()) {
            JSValue value = m_graph.valueOfJSConstant(edge.node());
            if (!value.isNumber()) {
                terminate(Uncountable);
                return m_out.doubleZero;
            }
            return m_out.constDouble(value.asNumber());
        }
        
        LoweredNodeValue value = m_doubleValues.get(edge.node());
        if (isValid(value))
            return value.value();
        
        value = m_int32Values.get(edge.node());
        if (isValid(value)) {
            LValue result = m_out.intToDouble(value.value());
            setDouble(edge.node(), result);
            return result;
        }
        
        value = m_strictInt52Values.get(edge.node());
        if (isValid(value))
            return strictInt52ToDouble(edge, value.value());
        
        value = m_int52Values.get(edge.node());
        if (isValid(value))
            return strictInt52ToDouble(edge, int52ToStrictInt52(value.value()));
        
        value = m_jsValueValues.get(edge.node());
        if (isValid(value)) {
            LValue boxedResult = value.value();
            
            LBasicBlock intCase = FTL_NEW_BLOCK(m_out, ("Double unboxing int case"));
            LBasicBlock doubleCase = FTL_NEW_BLOCK(m_out, ("Double unboxing double case"));
            LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Double unboxing continuation"));
            
            m_out.branch(isNotInt32(boxedResult), unsure(doubleCase), unsure(intCase));
            
            LBasicBlock lastNext = m_out.appendTo(intCase, doubleCase);
            
            ValueFromBlock intToDouble = m_out.anchor(
                m_out.intToDouble(unboxInt32(boxedResult)));
            m_out.jump(continuation);
            
            m_out.appendTo(doubleCase, continuation);
            
            FTL_TYPE_CHECK(
                jsValueValue(boxedResult), edge, SpecFullNumber, isCellOrMisc(boxedResult));
            
            ValueFromBlock unboxedDouble = m_out.anchor(unboxDouble(boxedResult));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            
            LValue result = m_out.phi(m_out.doubleType, intToDouble, unboxedDouble);
            
            setDouble(edge.node(), result);
            return result;
        }
        
        RELEASE_ASSERT(!(m_state.forNode(edge).m_type & SpecFullNumber));
        terminate(Uncountable);
        return m_out.doubleZero;
    }
    
    LValue lowJSValue(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == UntypedUse);
        
        if (edge->hasConstant())
            return m_out.constInt64(JSValue::encode(m_graph.valueOfJSConstant(edge.node())));
        
        LoweredNodeValue value = m_jsValueValues.get(edge.node());
        if (isValid(value))
            return value.value();
        
        value = m_int32Values.get(edge.node());
        if (isValid(value)) {
            LValue result = boxInt32(value.value());
            setJSValue(edge.node(), result);
            return result;
        }
        
        value = m_strictInt52Values.get(edge.node());
        if (isValid(value))
            return strictInt52ToJSValue(value.value());
        
        value = m_int52Values.get(edge.node());
        if (isValid(value))
            return strictInt52ToJSValue(int52ToStrictInt52(value.value()));
        
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
    
    LValue strictInt52ToInt32(Edge edge, LValue value)
    {
        LValue result = m_out.castToInt32(value);
        FTL_TYPE_CHECK(
            noValue(), edge, SpecInt32,
            m_out.notEqual(m_out.signExt(result, m_out.int64), value));
        setInt32(edge.node(), result);
        return result;
    }
    
    LValue strictInt52ToDouble(Edge edge, LValue value)
    {
        LValue result = m_out.intToDouble(value);
        setDouble(edge.node(), result);
        return result;
    }
    
    LValue strictInt52ToJSValue(LValue value)
    {
        LBasicBlock isInt32 = FTL_NEW_BLOCK(m_out, ("strictInt52ToJSValue isInt32 case"));
        LBasicBlock isDouble = FTL_NEW_BLOCK(m_out, ("strictInt52ToJSValue isDouble case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("strictInt52ToJSValue continuation"));
        
        Vector<ValueFromBlock, 2> results;
            
        LValue int32Value = m_out.castToInt32(value);
        m_out.branch(
            m_out.equal(m_out.signExt(int32Value, m_out.int64), value),
            unsure(isInt32), unsure(isDouble));
        
        LBasicBlock lastNext = m_out.appendTo(isInt32, isDouble);
        
        results.append(m_out.anchor(boxInt32(int32Value)));
        m_out.jump(continuation);
        
        m_out.appendTo(isDouble, continuation);
        
        results.append(m_out.anchor(boxDouble(m_out.intToDouble(value))));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(m_out.int64, results);
    }
    
    LValue setInt52WithStrictValue(Node* node, LValue value, Int52Kind kind)
    {
        switch (kind) {
        case StrictInt52:
            setStrictInt52(node, value);
            return value;
            
        case Int52:
            value = strictInt52ToInt52(value);
            setInt52(node, value);
            return value;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }

    LValue strictInt52ToInt52(LValue value)
    {
        return m_out.shl(value, m_out.constInt64(JSValue::int52ShiftAmount));
    }
    
    LValue int52ToStrictInt52(LValue value)
    {
        return m_out.aShr(value, m_out.constInt64(JSValue::int52ShiftAmount));
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
    
    LValue isCell(LValue jsValue)
    {
        return m_out.testIsZero64(jsValue, m_tagMask);
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
    
    LValue isNotNully(LValue value)
    {
        return m_out.notEqual(
            m_out.bitAnd(value, m_out.constInt64(~TagBitUndefined)),
            m_out.constInt64(ValueNull));
    }
    LValue isNully(LValue value)
    {
        return m_out.equal(
            m_out.bitAnd(value, m_out.constInt64(~TagBitUndefined)),
            m_out.constInt64(ValueNull));
    }

    void speculate(Edge edge)
    {
        switch (edge.useKind()) {
        case UntypedUse:
            break;
        case KnownInt32Use:
        case KnownNumberUse:
        case KnownStringUse:
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
        case FinalObjectUse:
            speculateFinalObject(edge);
            break;
        case StringUse:
            speculateString(edge);
            break;
        case StringObjectUse:
            speculateStringObject(edge);
            break;
        case StringOrStringObjectUse:
            speculateStringOrStringObject(edge);
            break;
        case RealNumberUse:
            speculateRealNumber(edge);
            break;
        case NumberUse:
            speculateNumber(edge);
            break;
        case MachineIntUse:
            speculateMachineInt(edge);
            break;
        case BooleanUse:
            speculateBoolean(edge);
            break;
        case NotCellUse:
            speculateNotCell(edge);
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
            m_out.load32(cell, m_heaps.JSCell_structureID),
            m_out.constInt32(vm().stringStructure->id()));
    }
    
    LValue isNotString(LValue cell)
    {
        return isObject(cell);
    }
    
    LValue isString(LValue cell)
    {
        return m_out.equal(
            m_out.load32(cell, m_heaps.JSCell_structureID),
            m_out.constInt32(vm().stringStructure->id()));
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
            LValue indexingType = m_out.load8(cell, m_heaps.JSCell_indexingType);
            
            switch (arrayMode.arrayClass()) {
            case Array::OriginalArray:
                RELEASE_ASSERT_NOT_REACHED();
                return 0;
                
            case Array::Array:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt8(IsArray | IndexingShapeMask)),
                    m_out.constInt8(IsArray | arrayMode.shapeMask()));
                
            case Array::NonArray:
            case Array::OriginalNonArray:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt8(IsArray | IndexingShapeMask)),
                    m_out.constInt8(arrayMode.shapeMask()));
                
            case Array::PossiblyArray:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt8(IndexingShapeMask)),
                    m_out.constInt8(arrayMode.shapeMask()));
            }
            
            RELEASE_ASSERT_NOT_REACHED();
        }
            
        default:
            return m_out.equal(
                m_out.load8(cell, m_heaps.JSCell_typeInfoType), 
                m_out.constInt8(typeForTypedArrayType(arrayMode.typedArrayType())));
        }
    }
    
    LValue hasClassInfo(LValue cell, const ClassInfo* classInfo)
    {
        return m_out.equal(
            m_out.loadPtr(
                loadStructure(cell),
                m_heaps.Structure_classInfo),
            m_out.constIntPtr(classInfo));
    }
    
    LValue isType(LValue cell, JSType type)
    {
        return m_out.equal(
            m_out.load8(cell, m_heaps.JSCell_typeInfoType),
            m_out.constInt8(type));
    }
    
    LValue isNotType(LValue cell, JSType type)
    {
        return m_out.bitNot(isType(cell, type));
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
        
        m_out.branch(isNotCell(value), unsure(primitiveCase), unsure(cellCase));
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, primitiveCase);
        
        FTL_TYPE_CHECK(
            jsValueValue(value), edge, (~SpecCell) | SpecObject, isNotObject(value));
        
        m_out.jump(continuation);
        
        m_out.appendTo(primitiveCase, continuation);
        
        FTL_TYPE_CHECK(
            jsValueValue(value), edge, SpecCell | SpecOther, isNotNully(value));
        
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void speculateFinalObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecFinalObject, isNotType(cell, FinalObjectType));
    }
    
    void speculateFinalObject(Edge edge)
    {
        speculateFinalObject(edge, lowCell(edge));
    }
    
    void speculateString(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecString, isNotString(cell));
    }
    
    void speculateString(Edge edge)
    {
        speculateString(edge, lowCell(edge));
    }
    
    void speculateStringObject(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge, SpecStringObject))
            return;
        
        speculateStringObjectForCell(edge, lowCell(edge));
        m_interpreter.filter(edge, SpecStringObject);
    }
    
    void speculateStringOrStringObject(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge, SpecString | SpecStringObject))
            return;
        
        LBasicBlock notString = FTL_NEW_BLOCK(m_out, ("Speculate StringOrStringObject not string case"));
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Speculate StringOrStringObject continuation"));
        
        LValue structureID = m_out.load32(lowCell(edge), m_heaps.JSCell_structureID);
        m_out.branch(
            m_out.equal(structureID, m_out.constInt32(vm().stringStructure->id())),
            unsure(continuation), unsure(notString));
        
        LBasicBlock lastNext = m_out.appendTo(notString, continuation);
        speculateStringObjectForStructureID(edge, structureID);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        m_interpreter.filter(edge, SpecString | SpecStringObject);
    }
    
    void speculateStringObjectForCell(Edge edge, LValue cell)
    {
        speculateStringObjectForStructureID(edge, m_out.load32(cell, m_heaps.JSCell_structureID));
    }
    
    void speculateStringObjectForStructureID(Edge edge, LValue structureID)
    {
        Structure* stringObjectStructure =
            m_graph.globalObjectFor(m_node->origin.semantic)->stringObjectStructure();
        
        if (m_state.forNode(edge).m_currentKnownStructure.isSubsetOf(StructureSet(stringObjectStructure)))
            return;
        
        speculate(
            NotStringObject, noValue(), 0,
            m_out.notEqual(structureID, weakStructure(stringObjectStructure)));
    }
    
    void speculateNonNullObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecObject, 
            m_out.equal(
                m_out.load32(cell, m_heaps.JSCell_structureID),
                m_out.constInt32(vm().stringStructure->id())));
        if (masqueradesAsUndefinedWatchpointIsStillValid())
            return;
        
        speculate(
            BadType, jsValueValue(cell), edge.node(),
            m_out.testNonZero8(
                m_out.load8(cell, m_heaps.JSCell_typeInfoFlags),
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
            doubleValue(value), edge, SpecFullRealNumber,
            m_out.doubleNotEqualOrUnordered(value, value));
    }
    
    void speculateMachineInt(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        Int52Kind kind;
        lowWhicheverInt52(edge, kind);
    }
    
    void speculateBoolean(Edge edge)
    {
        lowBoolean(edge);
    }
    
    void speculateNotCell(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowJSValue(edge);
        typeCheck(jsValueValue(value), edge, ~SpecCell, isCell(value));
    }
    
    bool masqueradesAsUndefinedWatchpointIsStillValid()
    {
        return m_graph.masqueradesAsUndefinedWatchpointIsStillValid(m_node->origin.semantic);
    }
    
    LValue loadMarkByte(LValue base)
    {
        return m_out.load8(base, m_heaps.JSCell_gcData);
    }

    void emitStoreBarrier(LValue base, LValue value, Edge valueEdge)
    {
#if ENABLE(GGC)
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Store barrier continuation"));
        LBasicBlock isCell = FTL_NEW_BLOCK(m_out, ("Store barrier is cell block"));

        if (m_state.forNode(valueEdge.node()).couldBeType(SpecCell))
            m_out.branch(isNotCell(value), unsure(continuation), unsure(isCell));
        else
            m_out.jump(isCell);

        LBasicBlock lastNext = m_out.appendTo(isCell, continuation);
        emitStoreBarrier(base);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
#else
        UNUSED_PARAM(base);
        UNUSED_PARAM(value);
        UNUSED_PARAM(valueEdge);
#endif
    }

    void emitStoreBarrier(LValue base)
    {
#if ENABLE(GGC)
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Store barrier continuation"));
        LBasicBlock isMarked = FTL_NEW_BLOCK(m_out, ("Store barrier is marked block"));
        LBasicBlock bufferHasSpace = FTL_NEW_BLOCK(m_out, ("Store barrier buffer is full"));
        LBasicBlock bufferIsFull = FTL_NEW_BLOCK(m_out, ("Store barrier buffer is full"));

        // Check the mark byte. 
        m_out.branch(
            m_out.isZero8(loadMarkByte(base)), usually(continuation), rarely(isMarked));

        // Append to the write barrier buffer.
        LBasicBlock lastNext = m_out.appendTo(isMarked, bufferHasSpace);
        LValue currentBufferIndex = m_out.load32(m_out.absolute(&vm().heap.writeBarrierBuffer().m_currentIndex));
        LValue bufferCapacity = m_out.load32(m_out.absolute(&vm().heap.writeBarrierBuffer().m_capacity));
        m_out.branch(
            m_out.lessThan(currentBufferIndex, bufferCapacity),
            usually(bufferHasSpace), rarely(bufferIsFull));

        // Buffer has space, store to it.
        m_out.appendTo(bufferHasSpace, bufferIsFull);
        LValue writeBarrierBufferBase = m_out.loadPtr(m_out.absolute(&vm().heap.writeBarrierBuffer().m_buffer));
        m_out.storePtr(base, m_out.baseIndex(m_heaps.WriteBarrierBuffer_bufferContents, writeBarrierBufferBase, m_out.zeroExt(currentBufferIndex, m_out.intPtr), ScalePtr));
        m_out.store32(m_out.add(currentBufferIndex, m_out.constInt32(1)), m_out.absolute(&vm().heap.writeBarrierBuffer().m_currentIndex));
        m_out.jump(continuation);

        // Buffer is out of space, flush it.
        m_out.appendTo(bufferIsFull, continuation);
        vmCall(m_out.operation(operationFlushWriteBarrierBuffer), m_callFrame, base);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
#else
        UNUSED_PARAM(base);
#endif
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
                    m_ftlState.jitCode->common.addCodeOrigin(codeOrigin))),
            tagFor(JSStack::ArgumentCount));
    }
    void callPreflight()
    {
        callPreflight(m_node->origin.semantic);
    }
    
    void callCheck(ExceptionCheckMode mode = CheckExceptions)
    {
        if (mode == NoExceptions)
            return;
        
        LBasicBlock continuation = FTL_NEW_BLOCK(m_out, ("Exception check continuation"));
        
        m_out.branch(
            m_out.notZero64(m_out.load64(m_out.absolute(vm().addressOfException()))),
            rarely(m_handleExceptions), usually(continuation));
        
        m_out.appendTo(continuation);
    }
    
    LBasicBlock lowBlock(BasicBlock* block)
    {
        return m_blocks.get(block);
    }
    
    void initializeOSRExitStateForBlock()
    {
        m_availability = m_highBlock->ssa->availabilityAtHead;
    }
    
    void appendOSRExit(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition)
    {
        if (verboseCompilationEnabled()) {
            dataLog("    OSR exit #", m_ftlState.jitCode->osrExit.size(), " with availability: ", m_availability, "\n");
            if (!m_availableRecoveries.isEmpty())
                dataLog("        Available recoveries: ", listDump(m_availableRecoveries), "\n");
        }

        ASSERT(m_ftlState.jitCode->osrExit.size() == m_ftlState.finalizer->osrExit.size());
        
        m_ftlState.jitCode->osrExit.append(OSRExit(
            kind, lowValue.format(), m_graph.methodOfGettingAValueProfileFor(highValue),
            m_codeOriginForExitTarget, m_codeOriginForExitProfile,
            m_availability.numberOfArguments(), m_availability.numberOfLocals()));
        m_ftlState.finalizer->osrExit.append(OSRExitCompilationInfo());
        
        OSRExit& exit = m_ftlState.jitCode->osrExit.last();

        LBasicBlock lastNext = 0;
        LBasicBlock continuation = 0;
        
        LBasicBlock failCase = FTL_NEW_BLOCK(m_out, ("OSR exit failCase for ", m_node));
        continuation = FTL_NEW_BLOCK(m_out, ("OSR exit continuation for ", m_node));
        
        m_out.branch(failCondition, rarely(failCase), usually(continuation));
        
        lastNext = m_out.appendTo(failCase, continuation);
        
        emitOSRExitCall(exit, lowValue);
        
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void emitOSRExitCall(OSRExit& exit, FormattedValue lowValue)
    {
        ExitArgumentList arguments;
        
        CodeOrigin codeOrigin = exit.m_codeOrigin;
        
        buildExitArguments(exit, arguments, lowValue, codeOrigin);
        
        callStackmap(exit, arguments);
    }
    
    void buildExitArguments(
        OSRExit& exit, ExitArgumentList& arguments, FormattedValue lowValue,
        CodeOrigin codeOrigin)
    {
        if (!!lowValue)
            arguments.append(lowValue.value());
        
        for (unsigned i = 0; i < exit.m_values.size(); ++i) {
            int operand = exit.m_values.operandForIndex(i);
            bool isLive = m_graph.isLiveInBytecode(VirtualRegister(operand), codeOrigin);
            if (!isLive) {
                exit.m_values[i] = ExitValue::dead();
                continue;
            }
            
            Availability availability = m_availability[i];
            FlushedAt flush = availability.flushedAt();
            switch (flush.format()) {
            case DeadFlush:
            case ConflictingFlush:
                if (availability.hasNode()) {
                    addExitArgumentForNode(exit, arguments, i, availability.node());
                    break;
                }
                
                if (Options::validateFTLOSRExitLiveness()) {
                    dataLog("Expected r", operand, " to be available but it wasn't.\n");
                    RELEASE_ASSERT_NOT_REACHED();
                }
                
                // This means that the DFG's DCE proved that the value is dead in bytecode
                // even though the bytecode liveness analysis thinks it's live. This is
                // acceptable since the DFG's DCE is by design more aggressive while still
                // being sound.
                exit.m_values[i] = ExitValue::dead();
                break;

            case FlushedJSValue:
            case FlushedCell:
            case FlushedBoolean:
                exit.m_values[i] = ExitValue::inJSStack(flush.virtualRegister());
                break;
                
            case FlushedInt32:
                exit.m_values[i] = ExitValue::inJSStackAsInt32(flush.virtualRegister());
                break;
                
            case FlushedInt52:
                exit.m_values[i] = ExitValue::inJSStackAsInt52(flush.virtualRegister());
                break;
                
            case FlushedDouble:
                exit.m_values[i] = ExitValue::inJSStackAsDouble(flush.virtualRegister());
                break;
                
            case FlushedArguments:
                exit.m_values[i] = ExitValue::argumentsObjectThatWasNotCreated();
                break;
            }
        }
        
        if (verboseCompilationEnabled())
            dataLog("        Exit values: ", exit.m_values, "\n");
    }
    
    void callStackmap(OSRExit& exit, ExitArgumentList& arguments)
    {
        exit.m_stackmapID = m_stackmapIDs++;
        arguments.insert(0, m_out.constInt32(MacroAssembler::maxJumpReplacementSize()));
        arguments.insert(0, m_out.constInt64(exit.m_stackmapID));
        
        m_out.call(m_out.stackmapIntrinsic(), arguments);
    }
    
    void addExitArgumentForNode(
        OSRExit& exit, ExitArgumentList& arguments, unsigned index, Node* node)
    {
        ASSERT(node->shouldGenerate());
        ASSERT(node->hasResult());

        if (tryToSetConstantExitArgument(exit, index, node))
            return;
        
        for (unsigned i = 0; i < m_availableRecoveries.size(); ++i) {
            AvailableRecovery recovery = m_availableRecoveries[i];
            if (recovery.node() != node)
                continue;
            
            exit.m_values[index] = ExitValue::recovery(
                recovery.opcode(), arguments.size(), arguments.size() + 1,
                recovery.format());
            arguments.append(recovery.left());
            arguments.append(recovery.right());
            return;
        }
        
        LoweredNodeValue value = m_int32Values.get(node);
        if (isValid(value)) {
            addExitArgument(exit, arguments, index, ValueFormatInt32, value.value());
            return;
        }
        
        value = m_int52Values.get(node);
        if (isValid(value)) {
            addExitArgument(exit, arguments, index, ValueFormatInt52, value.value());
            return;
        }
        
        value = m_strictInt52Values.get(node);
        if (isValid(value)) {
            addExitArgument(exit, arguments, index, ValueFormatStrictInt52, value.value());
            return;
        }
        
        value = m_booleanValues.get(node);
        if (isValid(value)) {
            LValue valueToPass = m_out.zeroExt(value.value(), m_out.int32);
            addExitArgument(exit, arguments, index, ValueFormatBoolean, valueToPass);
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
            exit.m_values[index] = ExitValue::argumentsObjectThatWasNotCreated();
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
    
    bool doesKill(Edge edge)
    {
        if (edge.doesNotKill())
            return false;
        
        if (edge->hasConstant())
            return false;
        
        return true;
    }

    void addAvailableRecovery(
        Node* node, RecoveryOpcode opcode, LValue left, LValue right, ValueFormat format)
    {
        m_availableRecoveries.append(AvailableRecovery(node, opcode, left, right, format));
    }
    
    void addAvailableRecovery(
        Edge edge, RecoveryOpcode opcode, LValue left, LValue right, ValueFormat format)
    {
        addAvailableRecovery(edge.node(), opcode, left, right, format);
    }
    
    void setInt32(Node* node, LValue value)
    {
        m_int32Values.set(node, LoweredNodeValue(value, m_highBlock));
    }
    void setInt52(Node* node, LValue value)
    {
        m_int52Values.set(node, LoweredNodeValue(value, m_highBlock));
    }
    void setStrictInt52(Node* node, LValue value)
    {
        m_strictInt52Values.set(node, LoweredNodeValue(value, m_highBlock));
    }
    void setInt52(Node* node, LValue value, Int52Kind kind)
    {
        switch (kind) {
        case Int52:
            setInt52(node, value);
            return;
            
        case StrictInt52:
            setStrictInt52(node, value);
            return;
        }
        
        RELEASE_ASSERT_NOT_REACHED();
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
    void setInt52(LValue value)
    {
        setInt52(m_node, value);
    }
    void setStrictInt52(LValue value)
    {
        setStrictInt52(m_node, value);
    }
    void setInt52(LValue value, Int52Kind kind)
    {
        setInt52(m_node, value, kind);
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

    LValue loadStructure(LValue value)
    {
        LValue tableIndex = m_out.load32(value, m_heaps.JSCell_structureID);
        LValue tableBase = m_out.get(m_out.constIntPtr(vm().heap.structureIDTable().base()));
        return m_out.get(m_out.baseIndex(tableBase, tableIndex, ScaleEight));
    }

    LValue weakPointer(JSCell* pointer)
    {
        addWeakReference(pointer);
        return m_out.constIntPtr(pointer);
    }

    LValue weakStructure(Structure* structure)
    {
        addWeakReference(structure);
        return m_out.constInt32(structure->id());
    }
    
    TypedPointer addressFor(LValue base, int operand, ptrdiff_t offset = 0)
    {
        return m_out.address(base, m_heaps.variables[operand], offset);
    }
    TypedPointer payloadFor(LValue base, int operand)
    {
        return addressFor(base, operand, PayloadOffset);
    }
    TypedPointer tagFor(LValue base, int operand)
    {
        return addressFor(base, operand, TagOffset);
    }
    TypedPointer addressFor(int operand, ptrdiff_t offset = 0)
    {
        return addressFor(VirtualRegister(operand), offset);
    }
    TypedPointer addressFor(VirtualRegister operand, ptrdiff_t offset = 0)
    {
        if (operand.isLocal())
            return addressFor(m_captured, operand.offset(), offset);
        return addressFor(m_callFrame, operand.offset(), offset);
    }
    TypedPointer payloadFor(int operand)
    {
        return payloadFor(VirtualRegister(operand));
    }
    TypedPointer payloadFor(VirtualRegister operand)
    {
        return addressFor(operand, PayloadOffset);
    }
    TypedPointer tagFor(int operand)
    {
        return tagFor(VirtualRegister(operand));
    }
    TypedPointer tagFor(VirtualRegister operand)
    {
        return addressFor(operand, TagOffset);
    }
    
    VM& vm() { return m_graph.m_vm; }
    CodeBlock* codeBlock() { return m_graph.m_codeBlock; }
    
    Graph& m_graph;
    State& m_ftlState;
    AbstractHeapRepository m_heaps;
    Output m_out;
    
    LBasicBlock m_prologue;
    LBasicBlock m_handleExceptions;
    HashMap<BasicBlock*, LBasicBlock> m_blocks;
    
    LValue m_callFrame;
    LValue m_captured;
    LValue m_tagTypeNumber;
    LValue m_tagMask;
    
    HashMap<Node*, LoweredNodeValue> m_int32Values;
    HashMap<Node*, LoweredNodeValue> m_strictInt52Values;
    HashMap<Node*, LoweredNodeValue> m_int52Values;
    HashMap<Node*, LoweredNodeValue> m_jsValueValues;
    HashMap<Node*, LoweredNodeValue> m_booleanValues;
    HashMap<Node*, LoweredNodeValue> m_storageValues;
    HashMap<Node*, LoweredNodeValue> m_doubleValues;
    
    HashMap<Node*, LValue> m_phis;
    
    Operands<Availability> m_availability;
    
    Vector<AvailableRecovery, 3> m_availableRecoveries;
    
    InPlaceAbstractState m_state;
    AbstractInterpreter<InPlaceAbstractState> m_interpreter;
    BasicBlock* m_highBlock;
    BasicBlock* m_nextHighBlock;
    LBasicBlock m_nextLowBlock;
    
    CodeOrigin m_codeOriginForExitTarget;
    CodeOrigin m_codeOriginForExitProfile;
    unsigned m_nodeIndex;
    Node* m_node;
    
    uint32_t m_stackmapIDs;
};

void lowerDFGToLLVM(State& state)
{
    LowerDFGToLLVM lowering(state);
    lowering.lower();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

