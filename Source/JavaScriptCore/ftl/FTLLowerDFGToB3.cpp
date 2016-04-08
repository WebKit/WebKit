/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "FTLLowerDFGToB3.h"

#if ENABLE(FTL_JIT)

#include "AirGenerationContext.h"
#include "AllowMacroScratchRegisterUsage.h"
#include "B3StackmapGenerationParams.h"
#include "CallFrameShuffler.h"
#include "CodeBlockWithJITType.h"
#include "DFGAbstractInterpreterInlines.h"
#include "DFGDominators.h"
#include "DFGInPlaceAbstractState.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "DFGOSRExitFuzz.h"
#include "DirectArguments.h"
#include "FTLAbstractHeapRepository.h"
#include "FTLAvailableRecovery.h"
#include "FTLExceptionTarget.h"
#include "FTLForOSREntryJITCode.h"
#include "FTLFormattedValue.h"
#include "FTLLazySlowPathCall.h"
#include "FTLLoweredNodeValue.h"
#include "FTLOperations.h"
#include "FTLOutput.h"
#include "FTLPatchpointExceptionHandle.h"
#include "FTLThunks.h"
#include "FTLWeightedTarget.h"
#include "JITAddGenerator.h"
#include "JITBitAndGenerator.h"
#include "JITBitOrGenerator.h"
#include "JITBitXorGenerator.h"
#include "JITDivGenerator.h"
#include "JITInlineCacheGenerator.h"
#include "JITLeftShiftGenerator.h"
#include "JITMulGenerator.h"
#include "JITRightShiftGenerator.h"
#include "JITSubGenerator.h"
#include "JSCInlines.h"
#include "JSGeneratorFunction.h"
#include "JSLexicalEnvironment.h"
#include "OperandsInlines.h"
#include "ScopedArguments.h"
#include "ScopedArgumentsTable.h"
#include "ScratchRegisterAllocator.h"
#include "ShadowChicken.h"
#include "SetupVarargsFrame.h"
#include "VirtualRegister.h"
#include "Watchdog.h"
#include <atomic>
#if !OS(WINDOWS)
#include <dlfcn.h>
#endif
#include <unordered_set>
#include <wtf/Box.h>
#include <wtf/ProcessID.h>

namespace JSC { namespace FTL {

using namespace B3;
using namespace DFG;

namespace {

std::atomic<int> compileCounter;

#if ASSERT_DISABLED
NO_RETURN_DUE_TO_CRASH static void ftlUnreachable()
{
    CRASH();
}
#else
NO_RETURN_DUE_TO_CRASH static void ftlUnreachable(
    CodeBlock* codeBlock, BlockIndex blockIndex, unsigned nodeIndex)
{
    dataLog("Crashing in thought-to-be-unreachable FTL-generated code for ", pointerDump(codeBlock), " at basic block #", blockIndex);
    if (nodeIndex != UINT_MAX)
        dataLog(", node @", nodeIndex);
    dataLog(".\n");
    CRASH();
}
#endif

// Using this instead of typeCheck() helps to reduce the load on B3, by creating
// significantly less dead code.
#define FTL_TYPE_CHECK_WITH_EXIT_KIND(exitKind, lowValue, highValue, typesPassedThrough, failCondition) do { \
        FormattedValue _ftc_lowValue = (lowValue);                      \
        Edge _ftc_highValue = (highValue);                              \
        SpeculatedType _ftc_typesPassedThrough = (typesPassedThrough);  \
        if (!m_interpreter.needsTypeCheck(_ftc_highValue, _ftc_typesPassedThrough)) \
            break;                                                      \
        typeCheck(_ftc_lowValue, _ftc_highValue, _ftc_typesPassedThrough, (failCondition), exitKind); \
    } while (false)

#define FTL_TYPE_CHECK(lowValue, highValue, typesPassedThrough, failCondition) \
    FTL_TYPE_CHECK_WITH_EXIT_KIND(BadType, lowValue, highValue, typesPassedThrough, failCondition)

class LowerDFGToB3 {
    WTF_MAKE_NONCOPYABLE(LowerDFGToB3);
public:
    LowerDFGToB3(State& state)
        : m_graph(state.graph)
        , m_ftlState(state)
        , m_out(state)
        , m_proc(*state.proc)
        , m_state(state.graph)
        , m_interpreter(state.graph, m_state)
    {
    }
    
    void lower()
    {
        State* state = &m_ftlState;
        
        CString name;
        if (verboseCompilationEnabled()) {
            name = toCString(
                "jsBody_", ++compileCounter, "_", codeBlock()->inferredName(),
                "_", codeBlock()->hash());
        } else
            name = "jsBody";
        
        m_graph.ensureDominators();

        if (verboseCompilationEnabled())
            dataLog("Function ready, beginning lowering.\n");

        m_out.initialize(m_heaps);

        // We use prologue frequency for all of the initialization code.
        m_out.setFrequency(1);
        
        m_prologue = m_out.newBlock();
        LBasicBlock stackOverflow = m_out.newBlock();
        m_handleExceptions = m_out.newBlock();
        
        LBasicBlock checkArguments = m_out.newBlock();

        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            m_highBlock = m_graph.block(blockIndex);
            if (!m_highBlock)
                continue;
            m_out.setFrequency(m_highBlock->executionCount);
            m_blocks.add(m_highBlock, m_out.newBlock());
        }

        // Back to prologue frequency for any bocks that get sneakily created in the initialization code.
        m_out.setFrequency(1);
        
        m_out.appendTo(m_prologue, stackOverflow);
        m_out.initializeConstants(m_proc, m_prologue);
        createPhiVariables();

        size_t sizeOfCaptured = sizeof(JSValue) * m_graph.m_nextMachineLocal;
        B3::SlotBaseValue* capturedBase = m_out.lockedStackSlot(sizeOfCaptured);
        m_captured = m_out.add(capturedBase, m_out.constIntPtr(sizeOfCaptured));
        state->capturedValue = capturedBase->slot();

        auto preOrder = m_graph.blocksInPreOrder();

        // We should not create any alloca's after this point, since they will cease to
        // be mem2reg candidates.
        
        m_callFrame = m_out.framePointer();
        m_tagTypeNumber = m_out.constInt64(TagTypeNumber);
        m_tagMask = m_out.constInt64(TagMask);

        // Make sure that B3 knows that we really care about the mask registers. This forces the
        // constants to be materialized in registers.
        m_proc.addFastConstant(m_tagTypeNumber->key());
        m_proc.addFastConstant(m_tagMask->key());
        
        m_out.storePtr(m_out.constIntPtr(codeBlock()), addressFor(JSStack::CodeBlock));
        
        m_out.branch(
            didOverflowStack(), rarely(stackOverflow), usually(checkArguments));
        
        m_out.appendTo(stackOverflow, m_handleExceptions);
        m_out.call(m_out.voidType, m_out.operation(operationThrowStackOverflowError), m_callFrame, m_out.constIntPtr(codeBlock()));
        m_out.patchpoint(Void)->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams&) {
                // We are terminal, so we can clobber everything. That's why we don't claim to
                // clobber scratch.
                AllowMacroScratchRegisterUsage allowScratch(jit);
                
                jit.copyCalleeSavesToVMCalleeSavesBuffer();
                jit.move(CCallHelpers::TrustedImmPtr(jit.vm()), GPRInfo::argumentGPR0);
                jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
                CCallHelpers::Call call = jit.call();
                jit.jumpToExceptionHandler();

                jit.addLinkTask(
                    [=] (LinkBuffer& linkBuffer) {
                        linkBuffer.link(call, FunctionPtr(lookupExceptionHandlerFromCallerFrame));
                    });
            });
        m_out.unreachable();
        
        m_out.appendTo(m_handleExceptions, checkArguments);
        Box<CCallHelpers::Label> exceptionHandler = state->exceptionHandler;
        m_out.patchpoint(Void)->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams&) {
                CCallHelpers::Jump jump = jit.jump();
                jit.addLinkTask(
                    [=] (LinkBuffer& linkBuffer) {
                        linkBuffer.link(jump, linkBuffer.locationOf(*exceptionHandler));
                    });
            });
        m_out.unreachable();
        
        m_out.appendTo(checkArguments, lowBlock(m_graph.block(0)));
        availabilityMap().clear();
        availabilityMap().m_locals = Operands<Availability>(codeBlock()->numParameters(), 0);
        for (unsigned i = codeBlock()->numParameters(); i--;) {
            availabilityMap().m_locals.argument(i) =
                Availability(FlushedAt(FlushedJSValue, virtualRegisterForArgument(i)));
        }
        m_node = nullptr;
        m_origin = NodeOrigin(CodeOrigin(0), CodeOrigin(0), true);
        for (unsigned i = codeBlock()->numParameters(); i--;) {
            Node* node = m_graph.m_arguments[i];
            VirtualRegister operand = virtualRegisterForArgument(i);
            
            LValue jsValue = m_out.load64(addressFor(operand));
            
            if (node) {
                DFG_ASSERT(m_graph, node, operand == node->stackAccessData()->machineLocal);
                
                // This is a hack, but it's an effective one. It allows us to do CSE on the
                // primordial load of arguments. This assumes that the GetLocal that got put in
                // place of the original SetArgument doesn't have any effects before it. This
                // should hold true.
                m_loadedArgumentValues.add(node, jsValue);
            }
            
            switch (m_graph.m_argumentFormats[i]) {
            case FlushedInt32:
                speculate(BadType, jsValueValue(jsValue), node, isNotInt32(jsValue));
                break;
            case FlushedBoolean:
                speculate(BadType, jsValueValue(jsValue), node, isNotBoolean(jsValue));
                break;
            case FlushedCell:
                speculate(BadType, jsValueValue(jsValue), node, isNotCell(jsValue));
                break;
            case FlushedJSValue:
                break;
            default:
                DFG_CRASH(m_graph, node, "Bad flush format for argument");
                break;
            }
        }
        m_out.jump(lowBlock(m_graph.block(0)));
        
        for (DFG::BasicBlock* block : preOrder)
            compileBlock(block);

        // Make sure everything is decorated. This does a bunch of deferred decorating. This has
        // to happen last because our abstract heaps are generated lazily. They have to be
        // generated lazily because we have an infiniten number of numbered, indexed, and
        // absolute heaps. We only become aware of the ones we actually mention while lowering.
        m_heaps.computeRangesAndDecorateInstructions();

        // We create all Phi's up front, but we may then decide not to compile the basic block
        // that would have contained one of them. So this creates orphans, which triggers B3
        // validation failures. Calling this fixes the issue.
        //
        // Note that you should avoid the temptation to make this call conditional upon
        // validation being enabled. B3 makes no guarantees of any kind of correctness when
        // dealing with IR that would have failed validation. For example, it would be valid to
        // write a B3 phase that so aggressively assumes the lack of orphans that it would crash
        // if any orphans were around. We might even have such phases already.
        m_proc.deleteOrphans();

        // We put the blocks into the B3 procedure in a super weird order. Now we reorder them.
        m_out.applyBlockOrder();
    }

private:
    
    void createPhiVariables()
    {
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            DFG::BasicBlock* block = m_graph.block(blockIndex);
            if (!block)
                continue;
            for (unsigned nodeIndex = block->size(); nodeIndex--;) {
                Node* node = block->at(nodeIndex);
                if (node->op() != DFG::Phi)
                    continue;
                LType type;
                switch (node->flags() & NodeResultMask) {
                case NodeResultDouble:
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
                    DFG_CRASH(m_graph, node, "Bad Phi node result type");
                    break;
                }
                m_phis.add(node, m_proc.add<Value>(B3::Phi, type, Origin(node)));
            }
        }
    }
    
    void compileBlock(DFG::BasicBlock* block)
    {
        if (!block)
            return;
        
        if (verboseCompilationEnabled())
            dataLog("Compiling block ", *block, "\n");
        
        m_highBlock = block;
        
        // Make sure that any blocks created while lowering code in the high block have the frequency of
        // the high block. This is appropriate because B3 doesn't need precise frequencies. It just needs
        // something roughly approximate for things like register allocation.
        m_out.setFrequency(m_highBlock->executionCount);
        
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
        // of the generated code (since we expect B3 to reorder things) but it will
        // make IR dumps easier to read.
        m_out.appendTo(lowBlock, m_nextLowBlock);
        
        if (Options::ftlCrashes())
            m_out.trap();
        
        if (!m_highBlock->cfaHasVisited) {
            if (verboseCompilationEnabled())
                dataLog("Bailing because CFA didn't reach.\n");
            crash(m_highBlock->index, UINT_MAX);
            return;
        }
        
        m_availabilityCalculator.beginBlock(m_highBlock);
        
        m_state.reset();
        m_state.beginBasicBlock(m_highBlock);
        
        for (m_nodeIndex = 0; m_nodeIndex < m_highBlock->size(); ++m_nodeIndex) {
            if (!compileNode(m_nodeIndex))
                break;
        }
    }

    void safelyInvalidateAfterTermination()
    {
        if (verboseCompilationEnabled())
            dataLog("Bailing.\n");
        crash();

        // Invalidate dominated blocks. Under normal circumstances we would expect
        // them to be invalidated already. But you can have the CFA become more
        // precise over time because the structures of objects change on the main
        // thread. Failing to do this would result in weird crashes due to a value
        // being used but not defined. Race conditions FTW!
        for (BlockIndex blockIndex = m_graph.numBlocks(); blockIndex--;) {
            DFG::BasicBlock* target = m_graph.block(blockIndex);
            if (!target)
                continue;
            if (m_graph.m_dominators->dominates(m_highBlock, target)) {
                if (verboseCompilationEnabled())
                    dataLog("Block ", *target, " will bail also.\n");
                target->cfaHasVisited = false;
            }
        }
    }

    bool compileNode(unsigned nodeIndex)
    {
        if (!m_state.isValid()) {
            safelyInvalidateAfterTermination();
            return false;
        }
        
        m_node = m_highBlock->at(nodeIndex);
        m_origin = m_node->origin;
        m_out.setOrigin(m_node);
        
        if (verboseCompilationEnabled())
            dataLog("Lowering ", m_node, "\n");
        
        m_availableRecoveries.resize(0);
        
        m_interpreter.startExecuting();
        
        switch (m_node->op()) {
        case DFG::Upsilon:
            compileUpsilon();
            break;
        case DFG::Phi:
            compilePhi();
            break;
        case JSConstant:
            break;
        case DoubleConstant:
            compileDoubleConstant();
            break;
        case Int52Constant:
            compileInt52Constant();
            break;
        case LazyJSConstant:
            compileLazyJSConstant();
            break;
        case DoubleRep:
            compileDoubleRep();
            break;
        case DoubleAsInt32:
            compileDoubleAsInt32();
            break;
        case DFG::ValueRep:
            compileValueRep();
            break;
        case Int52Rep:
            compileInt52Rep();
            break;
        case ValueToInt32:
            compileValueToInt32();
            break;
        case BooleanToNumber:
            compileBooleanToNumber();
            break;
        case ExtractOSREntryLocal:
            compileExtractOSREntryLocal();
            break;
        case GetStack:
            compileGetStack();
            break;
        case PutStack:
            compilePutStack();
            break;
        case DFG::Check:
            compileNoOp();
            break;
        case ToThis:
            compileToThis();
            break;
        case ValueAdd:
            compileValueAdd();
            break;
        case StrCat:
            compileStrCat();
            break;
        case ArithAdd:
        case ArithSub:
            compileArithAddOrSub();
            break;
        case ArithClz32:
            compileArithClz32();
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
        case ArithPow:
            compileArithPow();
            break;
        case ArithRandom:
            compileArithRandom();
            break;
        case ArithRound:
            compileArithRound();
            break;
        case ArithFloor:
            compileArithFloor();
            break;
        case ArithCeil:
            compileArithCeil();
            break;
        case ArithTrunc:
            compileArithTrunc();
            break;
        case ArithSqrt:
            compileArithSqrt();
            break;
        case ArithLog:
            compileArithLog();
            break;
        case ArithFRound:
            compileArithFRound();
            break;
        case ArithNegate:
            compileArithNegate();
            break;
        case DFG::BitAnd:
            compileBitAnd();
            break;
        case DFG::BitOr:
            compileBitOr();
            break;
        case DFG::BitXor:
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
        case CheckStructure:
            compileCheckStructure();
            break;
        case CheckCell:
            compileCheckCell();
            break;
        case CheckNotEmpty:
            compileCheckNotEmpty();
            break;
        case CheckBadCell:
            compileCheckBadCell();
            break;
        case CheckIdent:
            compileCheckIdent();
            break;
        case GetExecutable:
            compileGetExecutable();
            break;
        case ArrayifyToStructure:
            compileArrayifyToStructure();
            break;
        case PutStructure:
            compilePutStructure();
            break;
        case GetById:
        case GetByIdFlush:
            compileGetById();
            break;
        case In:
            compileIn();
            break;
        case PutById:
        case PutByIdDirect:
        case PutByIdFlush:
            compilePutById();
            break;
        case PutGetterById:
        case PutSetterById:
            compilePutAccessorById();
            break;
        case PutGetterSetterById:
            compilePutGetterSetterById();
            break;
        case PutGetterByVal:
        case PutSetterByVal:
            compilePutAccessorByVal();
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
        case GetMyArgumentByVal:
            compileGetMyArgumentByVal();
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
        case CreateActivation:
            compileCreateActivation();
            break;
        case NewFunction:
        case NewArrowFunction:
        case NewGeneratorFunction:
            compileNewFunction();
            break;
        case CreateDirectArguments:
            compileCreateDirectArguments();
            break;
        case CreateScopedArguments:
            compileCreateScopedArguments();
            break;
        case CreateClonedArguments:
            compileCreateClonedArguments();
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
        case NewTypedArray:
            compileNewTypedArray();
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
        case CallStringConstructor:
            compileToStringOrCallStringConstructor();
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
        case StringFromCharCode:
            compileStringFromCharCode();
            break;
        case GetByOffset:
        case GetGetterSetterByOffset:
            compileGetByOffset();
            break;
        case GetGetter:
            compileGetGetter();
            break;
        case GetSetter:
            compileGetSetter();
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
        case GetGlobalLexicalVariable:
            compileGetGlobalVariable();
            break;
        case PutGlobalVariable:
            compilePutGlobalVariable();
            break;
        case NotifyWrite:
            compileNotifyWrite();
            break;
        case GetCallee:
            compileGetCallee();
            break;
        case GetArgumentCount:
            compileGetArgumentCount();
            break;
        case GetScope:
            compileGetScope();
            break;
        case SkipScope:
            compileSkipScope();
            break;
        case GetGlobalObject:
            compileGetGlobalObject();
            break;
        case GetClosureVar:
            compileGetClosureVar();
            break;
        case PutClosureVar:
            compilePutClosureVar();
            break;
        case GetFromArguments:
            compileGetFromArguments();
            break;
        case PutToArguments:
            compilePutToArguments();
            break;
        case CompareEq:
            compileCompareEq();
            break;
        case CompareStrictEq:
            compileCompareStrictEq();
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
        case TailCallInlinedCaller:
        case Construct:
            compileCallOrConstruct();
            break;
        case TailCall:
            compileTailCall();
            break;
        case CallVarargs:
        case CallForwardVarargs:
        case TailCallVarargs:
        case TailCallVarargsInlinedCaller:
        case TailCallForwardVarargs:
        case TailCallForwardVarargsInlinedCaller:
        case ConstructVarargs:
        case ConstructForwardVarargs:
            compileCallOrConstructVarargs();
            break;
        case LoadVarargs:
            compileLoadVarargs();
            break;
        case ForwardVarargs:
            compileForwardVarargs();
            break;
        case DFG::Jump:
            compileJump();
            break;
        case DFG::Branch:
            compileBranch();
            break;
        case DFG::Switch:
            compileSwitch();
            break;
        case DFG::Return:
            compileReturn();
            break;
        case ForceOSRExit:
            compileForceOSRExit();
            break;
        case Throw:
        case ThrowReferenceError:
            compileThrow();
            break;
        case InvalidationPoint:
            compileInvalidationPoint();
            break;
        case IsUndefined:
            compileIsUndefined();
            break;
        case IsBoolean:
            compileIsBoolean();
            break;
        case IsNumber:
            compileIsNumber();
            break;
        case IsString:
            compileIsString();
            break;
        case IsObject:
            compileIsObject();
            break;
        case IsObjectOrNull:
            compileIsObjectOrNull();
            break;
        case IsFunction:
            compileIsFunction();
            break;
        case TypeOf:
            compileTypeOf();
            break;
        case CheckTypeInfoFlags:
            compileCheckTypeInfoFlags();
            break;
        case OverridesHasInstance:
            compileOverridesHasInstance();
            break;
        case InstanceOf:
            compileInstanceOf();
            break;
        case InstanceOfCustom:
            compileInstanceOfCustom();
            break;
        case CountExecution:
            compileCountExecution();
            break;
        case StoreBarrier:
            compileStoreBarrier();
            break;
        case HasIndexedProperty:
            compileHasIndexedProperty();
            break;
        case HasGenericProperty:
            compileHasGenericProperty();
            break;
        case HasStructureProperty:
            compileHasStructureProperty();
            break;
        case GetDirectPname:
            compileGetDirectPname();
            break;
        case GetEnumerableLength:
            compileGetEnumerableLength();
            break;
        case GetPropertyEnumerator:
            compileGetPropertyEnumerator();
            break;
        case GetEnumeratorStructurePname:
            compileGetEnumeratorStructurePname();
            break;
        case GetEnumeratorGenericPname:
            compileGetEnumeratorGenericPname();
            break;
        case ToIndexString:
            compileToIndexString();
            break;
        case CheckStructureImmediate:
            compileCheckStructureImmediate();
            break;
        case MaterializeNewObject:
            compileMaterializeNewObject();
            break;
        case MaterializeCreateActivation:
            compileMaterializeCreateActivation();
            break;
        case CheckWatchdogTimer:
            compileCheckWatchdogTimer();
            break;
        case CopyRest:
            compileCopyRest();
            break;
        case GetRestLength:
            compileGetRestLength();
            break;
        case RegExpExec:
            compileRegExpExec();
            break;
        case RegExpTest:
            compileRegExpTest();
            break;
        case NewRegexp:
            compileNewRegexp();
            break;
        case SetFunctionName:
            compileSetFunctionName();
            break;
        case StringReplace:
            compileStringReplace();
            break;
        case GetRegExpObjectLastIndex:
            compileGetRegExpObjectLastIndex();
            break;
        case SetRegExpObjectLastIndex:
            compileSetRegExpObjectLastIndex();
            break;
        case LogShadowChickenPrologue:
            compileLogShadowChickenPrologue();
            break;
        case LogShadowChickenTail:
            compileLogShadowChickenTail();
            break;
        case RecordRegExpCachedResult:
            compileRecordRegExpCachedResult();
            break;

        case PhantomLocal:
        case LoopHint:
        case MovHint:
        case ZombieHint:
        case ExitOK:
        case PhantomNewObject:
        case PhantomNewFunction:
        case PhantomNewGeneratorFunction:
        case PhantomCreateActivation:
        case PhantomDirectArguments:
        case PhantomClonedArguments:
        case PutHint:
        case BottomValue:
        case KillStack:
            break;
        default:
            DFG_CRASH(m_graph, m_node, "Unrecognized node in FTL backend");
            break;
        }
        
        if (m_node->isTerminal())
            return false;
        
        if (!m_state.isValid()) {
            safelyInvalidateAfterTermination();
            return false;
        }

        m_availabilityCalculator.executeNode(m_node);
        m_interpreter.executeEffects(nodeIndex);
        
        return true;
    }

    void compileUpsilon()
    {
        LValue upsilonValue = nullptr;
        switch (m_node->child1().useKind()) {
        case DoubleRepUse:
            upsilonValue = lowDouble(m_node->child1());
            break;
        case Int32Use:
        case KnownInt32Use:
            upsilonValue = lowInt32(m_node->child1());
            break;
        case Int52RepUse:
            upsilonValue = lowInt52(m_node->child1());
            break;
        case BooleanUse:
        case KnownBooleanUse:
            upsilonValue = lowBoolean(m_node->child1());
            break;
        case CellUse:
        case KnownCellUse:
            upsilonValue = lowCell(m_node->child1());
            break;
        case UntypedUse:
            upsilonValue = lowJSValue(m_node->child1());
            break;
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
        ValueFromBlock upsilon = m_out.anchor(upsilonValue);
        LValue phiNode = m_phis.get(m_node->phi());
        m_out.addIncomingToPhi(phiNode, upsilon);
    }
    
    void compilePhi()
    {
        LValue phi = m_phis.get(m_node);
        m_out.m_block->append(phi);

        switch (m_node->flags() & NodeResultMask) {
        case NodeResultDouble:
            setDouble(phi);
            break;
        case NodeResultInt32:
            setInt32(phi);
            break;
        case NodeResultInt52:
            setInt52(phi);
            break;
        case NodeResultBoolean:
            setBoolean(phi);
            break;
        case NodeResultJS:
            setJSValue(phi);
            break;
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }
    
    void compileDoubleConstant()
    {
        setDouble(m_out.constDouble(m_node->asNumber()));
    }
    
    void compileInt52Constant()
    {
        int64_t value = m_node->asMachineInt();
        
        setInt52(m_out.constInt64(value << JSValue::int52ShiftAmount));
        setStrictInt52(m_out.constInt64(value));
    }

    void compileLazyJSConstant()
    {
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        LazyJSValue value = m_node->lazyJSValue();
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                value.emit(jit, JSValueRegs(params[0].gpr()));
            });
        patchpoint->effects = Effects::none();
        setJSValue(patchpoint);
    }

    void compileDoubleRep()
    {
        switch (m_node->child1().useKind()) {
        case RealNumberUse: {
            LValue value = lowJSValue(m_node->child1(), ManualOperandSpeculation);
            
            LValue doubleValue = unboxDouble(value);
            
            LBasicBlock intCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            ValueFromBlock fastResult = m_out.anchor(doubleValue);
            m_out.branch(
                m_out.doubleEqual(doubleValue, doubleValue),
                usually(continuation), rarely(intCase));
            
            LBasicBlock lastNext = m_out.appendTo(intCase, continuation);
            
            FTL_TYPE_CHECK(
                jsValueValue(value), m_node->child1(), SpecBytecodeRealNumber,
                isNotInt32(value, provenType(m_node->child1()) & ~SpecFullDouble));
            ValueFromBlock slowResult = m_out.anchor(m_out.intToDouble(unboxInt32(value)));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            
            setDouble(m_out.phi(m_out.doubleType, fastResult, slowResult));
            return;
        }
            
        case NotCellUse:
        case NumberUse: {
            bool shouldConvertNonNumber = m_node->child1().useKind() == NotCellUse;
            
            LValue value = lowJSValue(m_node->child1(), ManualOperandSpeculation);

            LBasicBlock intCase = m_out.newBlock();
            LBasicBlock doubleTesting = m_out.newBlock();
            LBasicBlock doubleCase = m_out.newBlock();
            LBasicBlock nonDoubleCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                isNotInt32(value, provenType(m_node->child1())),
                unsure(doubleTesting), unsure(intCase));
            
            LBasicBlock lastNext = m_out.appendTo(intCase, doubleTesting);
            
            ValueFromBlock intToDouble = m_out.anchor(
                m_out.intToDouble(unboxInt32(value)));
            m_out.jump(continuation);
            
            m_out.appendTo(doubleTesting, doubleCase);
            LValue valueIsNumber = isNumber(value, provenType(m_node->child1()));
            m_out.branch(valueIsNumber, usually(doubleCase), rarely(nonDoubleCase));

            m_out.appendTo(doubleCase, nonDoubleCase);
            ValueFromBlock unboxedDouble = m_out.anchor(unboxDouble(value));
            m_out.jump(continuation);

            if (shouldConvertNonNumber) {
                LBasicBlock undefinedCase = m_out.newBlock();
                LBasicBlock testNullCase = m_out.newBlock();
                LBasicBlock nullCase = m_out.newBlock();
                LBasicBlock testBooleanTrueCase = m_out.newBlock();
                LBasicBlock convertBooleanTrueCase = m_out.newBlock();
                LBasicBlock convertBooleanFalseCase = m_out.newBlock();

                m_out.appendTo(nonDoubleCase, undefinedCase);
                LValue valueIsUndefined = m_out.equal(value, m_out.constInt64(ValueUndefined));
                m_out.branch(valueIsUndefined, unsure(undefinedCase), unsure(testNullCase));

                m_out.appendTo(undefinedCase, testNullCase);
                ValueFromBlock convertedUndefined = m_out.anchor(m_out.constDouble(PNaN));
                m_out.jump(continuation);

                m_out.appendTo(testNullCase, nullCase);
                LValue valueIsNull = m_out.equal(value, m_out.constInt64(ValueNull));
                m_out.branch(valueIsNull, unsure(nullCase), unsure(testBooleanTrueCase));

                m_out.appendTo(nullCase, testBooleanTrueCase);
                ValueFromBlock convertedNull = m_out.anchor(m_out.constDouble(0));
                m_out.jump(continuation);

                m_out.appendTo(testBooleanTrueCase, convertBooleanTrueCase);
                LValue valueIsBooleanTrue = m_out.equal(value, m_out.constInt64(ValueTrue));
                m_out.branch(valueIsBooleanTrue, unsure(convertBooleanTrueCase), unsure(convertBooleanFalseCase));

                m_out.appendTo(convertBooleanTrueCase, convertBooleanFalseCase);
                ValueFromBlock convertedTrue = m_out.anchor(m_out.constDouble(1));
                m_out.jump(continuation);

                m_out.appendTo(convertBooleanFalseCase, continuation);

                LValue valueIsNotBooleanFalse = m_out.notEqual(value, m_out.constInt64(ValueFalse));
                FTL_TYPE_CHECK(jsValueValue(value), m_node->child1(), ~SpecCell, valueIsNotBooleanFalse);
                ValueFromBlock convertedFalse = m_out.anchor(m_out.constDouble(0));
                m_out.jump(continuation);

                m_out.appendTo(continuation, lastNext);
                setDouble(m_out.phi(m_out.doubleType, intToDouble, unboxedDouble, convertedUndefined, convertedNull, convertedTrue, convertedFalse));
                return;
            }
            m_out.appendTo(nonDoubleCase, continuation);
            FTL_TYPE_CHECK(jsValueValue(value), m_node->child1(), SpecBytecodeNumber, m_out.booleanTrue);
            m_out.unreachable();

            m_out.appendTo(continuation, lastNext);

            setDouble(m_out.phi(m_out.doubleType, intToDouble, unboxedDouble));
            return;
        }
            
        case Int52RepUse: {
            setDouble(strictInt52ToDouble(lowStrictInt52(m_node->child1())));
            return;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
        }
    }

    void compileDoubleAsInt32()
    {
        LValue integerValue = convertDoubleToInt32(lowDouble(m_node->child1()), shouldCheckNegativeZero(m_node->arithMode()));
        setInt32(integerValue);
    }

    void compileValueRep()
    {
        switch (m_node->child1().useKind()) {
        case DoubleRepUse: {
            LValue value = lowDouble(m_node->child1());
            
            if (m_interpreter.needsTypeCheck(m_node->child1(), ~SpecDoubleImpureNaN)) {
                value = m_out.select(
                    m_out.doubleEqual(value, value), value, m_out.constDouble(PNaN));
            }
            
            setJSValue(boxDouble(value));
            return;
        }
            
        case Int52RepUse: {
            setJSValue(strictInt52ToJSValue(lowStrictInt52(m_node->child1())));
            return;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
        }
    }
    
    void compileInt52Rep()
    {
        switch (m_node->child1().useKind()) {
        case Int32Use:
            setStrictInt52(m_out.signExt32To64(lowInt32(m_node->child1())));
            return;
            
        case MachineIntUse:
            setStrictInt52(
                jsValueToStrictInt52(
                    m_node->child1(), lowJSValue(m_node->child1(), ManualOperandSpeculation)));
            return;
            
        case DoubleRepMachineIntUse:
            setStrictInt52(
                doubleToStrictInt52(
                    m_node->child1(), lowDouble(m_node->child1())));
            return;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    
    void compileValueToInt32()
    {
        switch (m_node->child1().useKind()) {
        case Int52RepUse:
            setInt32(m_out.castToInt32(lowStrictInt52(m_node->child1())));
            break;
            
        case DoubleRepUse:
            setInt32(doubleToInt32(lowDouble(m_node->child1())));
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
                setInt32(numberOrNotCellToInt32(m_node->child1(), value.value()));
                break;
            }
            
            // We'll basically just get here for constants. But it's good to have this
            // catch-all since we often add new representations into the mix.
            setInt32(
                numberOrNotCellToInt32(
                    m_node->child1(),
                    lowJSValue(m_node->child1(), ManualOperandSpeculation)));
            break;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }
    
    void compileBooleanToNumber()
    {
        switch (m_node->child1().useKind()) {
        case BooleanUse: {
            setInt32(m_out.zeroExt(lowBoolean(m_node->child1()), m_out.int32));
            return;
        }
            
        case UntypedUse: {
            LValue value = lowJSValue(m_node->child1());
            
            if (!m_interpreter.needsTypeCheck(m_node->child1(), SpecBoolInt32 | SpecBoolean)) {
                setInt32(m_out.bitAnd(m_out.castToInt32(value), m_out.int32One));
                return;
            }
            
            LBasicBlock booleanCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            ValueFromBlock notBooleanResult = m_out.anchor(value);
            m_out.branch(
                isBoolean(value, provenType(m_node->child1())),
                unsure(booleanCase), unsure(continuation));
            
            LBasicBlock lastNext = m_out.appendTo(booleanCase, continuation);
            ValueFromBlock booleanResult = m_out.anchor(m_out.bitOr(
                m_out.zeroExt(unboxBoolean(value), m_out.int64), m_tagTypeNumber));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, booleanResult, notBooleanResult));
            return;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    }

    void compileExtractOSREntryLocal()
    {
        EncodedJSValue* buffer = static_cast<EncodedJSValue*>(
            m_ftlState.jitCode->ftlForOSREntry()->entryBuffer()->dataBuffer());
        setJSValue(m_out.load64(m_out.absolute(buffer + m_node->unlinkedLocal().toLocal())));
    }
    
    void compileGetStack()
    {
        // GetLocals arise only for captured variables and arguments. For arguments, we might have
        // already loaded it.
        if (LValue value = m_loadedArgumentValues.get(m_node)) {
            setJSValue(value);
            return;
        }
        
        StackAccessData* data = m_node->stackAccessData();
        AbstractValue& value = m_state.variables().operand(data->local);
        
        DFG_ASSERT(m_graph, m_node, isConcrete(data->format));
        DFG_ASSERT(m_graph, m_node, data->format != FlushedDouble); // This just happens to not arise for GetStacks, right now. It would be trivial to support.
        
        if (isInt32Speculation(value.m_type))
            setInt32(m_out.load32(payloadFor(data->machineLocal)));
        else
            setJSValue(m_out.load64(addressFor(data->machineLocal)));
    }
    
    void compilePutStack()
    {
        StackAccessData* data = m_node->stackAccessData();
        switch (data->format) {
        case FlushedJSValue: {
            LValue value = lowJSValue(m_node->child1());
            m_out.store64(value, addressFor(data->machineLocal));
            break;
        }
            
        case FlushedDouble: {
            LValue value = lowDouble(m_node->child1());
            m_out.storeDouble(value, addressFor(data->machineLocal));
            break;
        }
            
        case FlushedInt32: {
            LValue value = lowInt32(m_node->child1());
            m_out.store32(value, payloadFor(data->machineLocal));
            break;
        }
            
        case FlushedInt52: {
            LValue value = lowInt52(m_node->child1());
            m_out.store64(value, addressFor(data->machineLocal));
            break;
        }
            
        case FlushedCell: {
            LValue value = lowCell(m_node->child1());
            m_out.store64(value, addressFor(data->machineLocal));
            break;
        }
            
        case FlushedBoolean: {
            speculateBoolean(m_node->child1());
            m_out.store64(
                lowJSValue(m_node->child1(), ManualOperandSpeculation),
                addressFor(data->machineLocal));
            break;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad flush format");
            break;
        }
    }
    
    void compileNoOp()
    {
        DFG_NODE_DO_TO_CHILDREN(m_graph, m_node, speculate);
    }
    
    void compileToThis()
    {
        LValue value = lowJSValue(m_node->child1());
        
        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        m_out.branch(
            isCell(value, provenType(m_node->child1())), usually(isCellCase), rarely(slowCase));
        
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
            vmCall(m_out.int64, m_out.operation(function), m_callFrame, value));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
    }
    
    void compileValueAdd()
    {
        emitBinarySnippet<JITAddGenerator>(operationValueAdd);
    }
    
    void compileStrCat()
    {
        LValue result;
        if (m_node->child3()) {
            result = vmCall(
                m_out.int64, m_out.operation(operationStrCat3), m_callFrame,
                lowJSValue(m_node->child1(), ManualOperandSpeculation),
                lowJSValue(m_node->child2(), ManualOperandSpeculation),
                lowJSValue(m_node->child3(), ManualOperandSpeculation));
        } else {
            result = vmCall(
                m_out.int64, m_out.operation(operationStrCat2), m_callFrame,
                lowJSValue(m_node->child1(), ManualOperandSpeculation),
                lowJSValue(m_node->child2(), ManualOperandSpeculation));
        }
        setJSValue(result);
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

            CheckValue* result =
                isSub ? m_out.speculateSub(left, right) : m_out.speculateAdd(left, right);
            blessSpeculation(result, Overflow, noValue(), nullptr, m_origin);
            setInt32(result);
            break;
        }
            
        case Int52RepUse: {
            if (!abstractValue(m_node->child1()).couldBeType(SpecInt52)
                && !abstractValue(m_node->child2()).couldBeType(SpecInt52)) {
                Int52Kind kind;
                LValue left = lowWhicheverInt52(m_node->child1(), kind);
                LValue right = lowInt52(m_node->child2(), kind);
                setInt52(isSub ? m_out.sub(left, right) : m_out.add(left, right), kind);
                break;
            }

            LValue left = lowInt52(m_node->child1());
            LValue right = lowInt52(m_node->child2());
            CheckValue* result =
                isSub ? m_out.speculateSub(left, right) : m_out.speculateAdd(left, right);
            blessSpeculation(result, Overflow, noValue(), nullptr, m_origin);
            setInt52(result);
            break;
        }
            
        case DoubleRepUse: {
            LValue C1 = lowDouble(m_node->child1());
            LValue C2 = lowDouble(m_node->child2());

            setDouble(isSub ? m_out.doubleSub(C1, C2) : m_out.doubleAdd(C1, C2));
            break;
        }

        case UntypedUse: {
            if (!isSub) {
                DFG_CRASH(m_graph, m_node, "Bad use kind");
                break;
            }

            emitBinarySnippet<JITSubGenerator>(operationValueSub);
            break;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }

    void compileArithClz32()
    {
        LValue operand = lowInt32(m_node->child1());
        setInt32(m_out.ctlz32(operand));
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
                CheckValue* speculation = m_out.speculateMul(left, right);
                blessSpeculation(speculation, Overflow, noValue(), nullptr, m_origin);
                result = speculation;
            }
            
            if (shouldCheckNegativeZero(m_node->arithMode())) {
                LBasicBlock slowCase = m_out.newBlock();
                LBasicBlock continuation = m_out.newBlock();
                
                m_out.branch(
                    m_out.notZero32(result), usually(continuation), rarely(slowCase));
                
                LBasicBlock lastNext = m_out.appendTo(slowCase, continuation);
                speculate(NegativeZero, noValue(), nullptr, m_out.lessThan(left, m_out.int32Zero));
                speculate(NegativeZero, noValue(), nullptr, m_out.lessThan(right, m_out.int32Zero));
                m_out.jump(continuation);
                m_out.appendTo(continuation, lastNext);
            }
            
            setInt32(result);
            break;
        }
            
        case Int52RepUse: {
            Int52Kind kind;
            LValue left = lowWhicheverInt52(m_node->child1(), kind);
            LValue right = lowInt52(m_node->child2(), opposite(kind));

            CheckValue* result = m_out.speculateMul(left, right);
            blessSpeculation(result, Overflow, noValue(), nullptr, m_origin);

            if (shouldCheckNegativeZero(m_node->arithMode())) {
                LBasicBlock slowCase = m_out.newBlock();
                LBasicBlock continuation = m_out.newBlock();
                
                m_out.branch(
                    m_out.notZero64(result), usually(continuation), rarely(slowCase));
                
                LBasicBlock lastNext = m_out.appendTo(slowCase, continuation);
                speculate(NegativeZero, noValue(), nullptr, m_out.lessThan(left, m_out.int64Zero));
                speculate(NegativeZero, noValue(), nullptr, m_out.lessThan(right, m_out.int64Zero));
                m_out.jump(continuation);
                m_out.appendTo(continuation, lastNext);
            }
            
            setInt52(result);
            break;
        }
            
        case DoubleRepUse: {
            setDouble(
                m_out.doubleMul(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }

        case UntypedUse: {
            emitBinarySnippet<JITMulGenerator>(operationValueMul);
            break;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }

    void compileArithDiv()
    {
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue numerator = lowInt32(m_node->child1());
            LValue denominator = lowInt32(m_node->child2());

            if (shouldCheckNegativeZero(m_node->arithMode())) {
                LBasicBlock zeroNumerator = m_out.newBlock();
                LBasicBlock numeratorContinuation = m_out.newBlock();

                m_out.branch(
                    m_out.isZero32(numerator),
                    rarely(zeroNumerator), usually(numeratorContinuation));

                LBasicBlock innerLastNext = m_out.appendTo(zeroNumerator, numeratorContinuation);

                speculate(
                    NegativeZero, noValue(), 0, m_out.lessThan(denominator, m_out.int32Zero));

                m_out.jump(numeratorContinuation);

                m_out.appendTo(numeratorContinuation, innerLastNext);
            }
            
            if (shouldCheckOverflow(m_node->arithMode())) {
                LBasicBlock unsafeDenominator = m_out.newBlock();
                LBasicBlock continuation = m_out.newBlock();

                LValue adjustedDenominator = m_out.add(denominator, m_out.int32One);
                m_out.branch(
                    m_out.above(adjustedDenominator, m_out.int32One),
                    usually(continuation), rarely(unsafeDenominator));

                LBasicBlock lastNext = m_out.appendTo(unsafeDenominator, continuation);
                LValue neg2ToThe31 = m_out.constInt32(-2147483647-1);
                speculate(Overflow, noValue(), nullptr, m_out.isZero32(denominator));
                speculate(Overflow, noValue(), nullptr, m_out.equal(numerator, neg2ToThe31));
                m_out.jump(continuation);

                m_out.appendTo(continuation, lastNext);
                LValue result = m_out.div(numerator, denominator);
                speculate(
                    Overflow, noValue(), 0,
                    m_out.notEqual(m_out.mul(result, denominator), numerator));
                setInt32(result);
            } else
                setInt32(m_out.chillDiv(numerator, denominator));

            break;
        }
            
        case DoubleRepUse: {
            setDouble(m_out.doubleDiv(
                lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }

        case UntypedUse: {
            emitBinarySnippet<JITDivGenerator, NeedScratchFPR>(operationValueDiv);
            break;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }
    
    void compileArithMod()
    {
        switch (m_node->binaryUseKind()) {
        case Int32Use: {
            LValue numerator = lowInt32(m_node->child1());
            LValue denominator = lowInt32(m_node->child2());

            LValue remainder;
            if (shouldCheckOverflow(m_node->arithMode())) {
                LBasicBlock unsafeDenominator = m_out.newBlock();
                LBasicBlock continuation = m_out.newBlock();

                LValue adjustedDenominator = m_out.add(denominator, m_out.int32One);
                m_out.branch(
                    m_out.above(adjustedDenominator, m_out.int32One),
                    usually(continuation), rarely(unsafeDenominator));

                LBasicBlock lastNext = m_out.appendTo(unsafeDenominator, continuation);
                LValue neg2ToThe31 = m_out.constInt32(-2147483647-1);
                speculate(Overflow, noValue(), nullptr, m_out.isZero32(denominator));
                speculate(Overflow, noValue(), nullptr, m_out.equal(numerator, neg2ToThe31));
                m_out.jump(continuation);

                m_out.appendTo(continuation, lastNext);
                LValue result = m_out.mod(numerator, denominator);
                remainder = result;
            } else
                remainder = m_out.chillMod(numerator, denominator);

            if (shouldCheckNegativeZero(m_node->arithMode())) {
                LBasicBlock negativeNumerator = m_out.newBlock();
                LBasicBlock numeratorContinuation = m_out.newBlock();

                m_out.branch(
                    m_out.lessThan(numerator, m_out.int32Zero),
                    unsure(negativeNumerator), unsure(numeratorContinuation));

                LBasicBlock innerLastNext = m_out.appendTo(negativeNumerator, numeratorContinuation);

                speculate(NegativeZero, noValue(), 0, m_out.isZero32(remainder));

                m_out.jump(numeratorContinuation);

                m_out.appendTo(numeratorContinuation, innerLastNext);
            }

            setInt32(remainder);
            break;
        }
            
        case DoubleRepUse: {
            setDouble(
                m_out.doubleMod(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            break;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
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
            
        case DoubleRepUse: {
            LValue left = lowDouble(m_node->child1());
            LValue right = lowDouble(m_node->child2());
            
            LBasicBlock notLessThan = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
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
                right, m_out.constDouble(PNaN))));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setDouble(m_out.phi(m_out.doubleType, results));
            break;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
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

            if (shouldCheckOverflow(m_node->arithMode()))
                speculate(Overflow, noValue(), 0, m_out.lessThan(result, m_out.int32Zero));

            setInt32(result);
            break;
        }
            
        case DoubleRepUse: {
            setDouble(m_out.doubleAbs(lowDouble(m_node->child1())));
            break;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }

    void compileArithSin() { setDouble(m_out.doubleSin(lowDouble(m_node->child1()))); }

    void compileArithCos() { setDouble(m_out.doubleCos(lowDouble(m_node->child1()))); }

    void compileArithPow()
    {
        if (m_node->child2().useKind() == Int32Use)
            setDouble(m_out.doublePowi(lowDouble(m_node->child1()), lowInt32(m_node->child2())));
        else {
            LValue base = lowDouble(m_node->child1());
            LValue exponent = lowDouble(m_node->child2());

            LBasicBlock integerExponentIsSmallBlock = m_out.newBlock();
            LBasicBlock integerExponentPowBlock = m_out.newBlock();
            LBasicBlock doubleExponentPowBlockEntry = m_out.newBlock();
            LBasicBlock nanExceptionExponentIsInfinity = m_out.newBlock();
            LBasicBlock nanExceptionBaseIsOne = m_out.newBlock();
            LBasicBlock powBlock = m_out.newBlock();
            LBasicBlock nanExceptionResultIsNaN = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue integerExponent = m_out.doubleToInt(exponent);
            LValue integerExponentConvertedToDouble = m_out.intToDouble(integerExponent);
            LValue exponentIsInteger = m_out.doubleEqual(exponent, integerExponentConvertedToDouble);
            m_out.branch(exponentIsInteger, unsure(integerExponentIsSmallBlock), unsure(doubleExponentPowBlockEntry));

            LBasicBlock lastNext = m_out.appendTo(integerExponentIsSmallBlock, integerExponentPowBlock);
            LValue integerExponentBelow1000 = m_out.below(integerExponent, m_out.constInt32(1000));
            m_out.branch(integerExponentBelow1000, usually(integerExponentPowBlock), rarely(doubleExponentPowBlockEntry));

            m_out.appendTo(integerExponentPowBlock, doubleExponentPowBlockEntry);
            ValueFromBlock powDoubleIntResult = m_out.anchor(m_out.doublePowi(base, integerExponent));
            m_out.jump(continuation);

            // If y is NaN, the result is NaN.
            m_out.appendTo(doubleExponentPowBlockEntry, nanExceptionExponentIsInfinity);
            LValue exponentIsNaN;
            if (provenType(m_node->child2()) & SpecDoubleNaN)
                exponentIsNaN = m_out.doubleNotEqualOrUnordered(exponent, exponent);
            else
                exponentIsNaN = m_out.booleanFalse;
            m_out.branch(exponentIsNaN, rarely(nanExceptionResultIsNaN), usually(nanExceptionExponentIsInfinity));

            // If abs(x) is 1 and y is +infinity, the result is NaN.
            // If abs(x) is 1 and y is -infinity, the result is NaN.
            m_out.appendTo(nanExceptionExponentIsInfinity, nanExceptionBaseIsOne);
            LValue absoluteExponent = m_out.doubleAbs(exponent);
            LValue absoluteExponentIsInfinity = m_out.doubleEqual(absoluteExponent, m_out.constDouble(std::numeric_limits<double>::infinity()));
            m_out.branch(absoluteExponentIsInfinity, rarely(nanExceptionBaseIsOne), usually(powBlock));

            m_out.appendTo(nanExceptionBaseIsOne, powBlock);
            LValue absoluteBase = m_out.doubleAbs(base);
            LValue absoluteBaseIsOne = m_out.doubleEqual(absoluteBase, m_out.constDouble(1));
            m_out.branch(absoluteBaseIsOne, unsure(nanExceptionResultIsNaN), unsure(powBlock));

            m_out.appendTo(powBlock, nanExceptionResultIsNaN);
            ValueFromBlock powResult = m_out.anchor(m_out.doublePow(base, exponent));
            m_out.jump(continuation);

            m_out.appendTo(nanExceptionResultIsNaN, continuation);
            ValueFromBlock pureNan = m_out.anchor(m_out.constDouble(PNaN));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setDouble(m_out.phi(m_out.doubleType, powDoubleIntResult, powResult, pureNan));
        }
    }

    void compileArithRandom()
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);

        // Inlined WeakRandom::advance().
        // uint64_t x = m_low;
        void* lowAddress = reinterpret_cast<uint8_t*>(globalObject) + JSGlobalObject::weakRandomOffset() + WeakRandom::lowOffset();
        LValue low = m_out.load64(m_out.absolute(lowAddress));
        // uint64_t y = m_high;
        void* highAddress = reinterpret_cast<uint8_t*>(globalObject) + JSGlobalObject::weakRandomOffset() + WeakRandom::highOffset();
        LValue high = m_out.load64(m_out.absolute(highAddress));
        // m_low = y;
        m_out.store64(high, m_out.absolute(lowAddress));

        // x ^= x << 23;
        LValue phase1 = m_out.bitXor(m_out.shl(low, m_out.constInt64(23)), low);

        // x ^= x >> 17;
        LValue phase2 = m_out.bitXor(m_out.lShr(phase1, m_out.constInt64(17)), phase1);

        // x ^= y ^ (y >> 26);
        LValue phase3 = m_out.bitXor(m_out.bitXor(high, m_out.lShr(high, m_out.constInt64(26))), phase2);

        // m_high = x;
        m_out.store64(phase3, m_out.absolute(highAddress));

        // return x + y;
        LValue random64 = m_out.add(phase3, high);

        // Extract random 53bit. [0, 53] bit is safe integer number ranges in double representation.
        LValue random53 = m_out.bitAnd(random64, m_out.constInt64((1ULL << 53) - 1));

        LValue double53Integer = m_out.intToDouble(random53);

        // Convert `(53bit double integer value) / (1 << 53)` to `(53bit double integer value) * (1.0 / (1 << 53))`.
        // In latter case, `1.0 / (1 << 53)` will become a double value represented as (mantissa = 0 & exp = 970, it means 1e-(2**54)).
        static const double scale = 1.0 / (1ULL << 53);

        // Multiplying 1e-(2**54) with the double integer does not change anything of the mantissa part of the double integer.
        // It just reduces the exp part of the given 53bit double integer.
        // (Except for 0.0. This is specially handled and in this case, exp just becomes 0.)
        // Now we get 53bit precision random double value in [0, 1).
        LValue result = m_out.doubleMul(double53Integer, m_out.constDouble(scale));

        setDouble(result);
    }

    void compileArithRound()
    {
        LValue result = nullptr;

        if (producesInteger(m_node->arithRoundingMode()) && !shouldCheckNegativeZero(m_node->arithRoundingMode())) {
            LValue value = lowDouble(m_node->child1());
            result = m_out.doubleFloor(m_out.doubleAdd(value, m_out.constDouble(0.5)));
        } else {
            LBasicBlock realPartIsMoreThanHalf = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue value = lowDouble(m_node->child1());
            LValue integerValue = m_out.doubleCeil(value);
            ValueFromBlock integerValueResult = m_out.anchor(integerValue);

            LValue realPart = m_out.doubleSub(integerValue, value);

            m_out.branch(m_out.doubleGreaterThanOrUnordered(realPart, m_out.constDouble(0.5)), unsure(realPartIsMoreThanHalf), unsure(continuation));

            LBasicBlock lastNext = m_out.appendTo(realPartIsMoreThanHalf, continuation);
            LValue integerValueRoundedDown = m_out.doubleSub(integerValue, m_out.constDouble(1));
            ValueFromBlock integerValueRoundedDownResult = m_out.anchor(integerValueRoundedDown);
            m_out.jump(continuation);
            m_out.appendTo(continuation, lastNext);

            result = m_out.phi(m_out.doubleType, integerValueResult, integerValueRoundedDownResult);
        }

        if (producesInteger(m_node->arithRoundingMode())) {
            LValue integerValue = convertDoubleToInt32(result, shouldCheckNegativeZero(m_node->arithRoundingMode()));
            setInt32(integerValue);
        } else
            setDouble(result);
    }

    void compileArithFloor()
    {
        LValue value = lowDouble(m_node->child1());
        LValue integerValue = m_out.doubleFloor(value);
        if (producesInteger(m_node->arithRoundingMode()))
            setInt32(convertDoubleToInt32(integerValue, shouldCheckNegativeZero(m_node->arithRoundingMode())));
        else
            setDouble(integerValue);
    }

    void compileArithCeil()
    {
        LValue value = lowDouble(m_node->child1());
        LValue integerValue = m_out.doubleCeil(value);
        if (producesInteger(m_node->arithRoundingMode()))
            setInt32(convertDoubleToInt32(integerValue, shouldCheckNegativeZero(m_node->arithRoundingMode())));
        else
            setDouble(integerValue);
    }

    void compileArithTrunc()
    {
        LValue value = lowDouble(m_node->child1());
        LValue result = m_out.doubleTrunc(value);
        if (producesInteger(m_node->arithRoundingMode()))
            setInt32(convertDoubleToInt32(result, shouldCheckNegativeZero(m_node->arithRoundingMode())));
        else
            setDouble(result);
    }

    void compileArithSqrt() { setDouble(m_out.doubleSqrt(lowDouble(m_node->child1()))); }

    void compileArithLog() { setDouble(m_out.doubleLog(lowDouble(m_node->child1()))); }
    
    void compileArithFRound()
    {
        setDouble(m_out.fround(lowDouble(m_node->child1())));
    }
    
    void compileArithNegate()
    {
        switch (m_node->child1().useKind()) {
        case Int32Use: {
            LValue value = lowInt32(m_node->child1());
            
            LValue result;
            if (!shouldCheckOverflow(m_node->arithMode()))
                result = m_out.neg(value);
            else if (!shouldCheckNegativeZero(m_node->arithMode())) {
                CheckValue* check = m_out.speculateSub(m_out.int32Zero, value);
                blessSpeculation(check, Overflow, noValue(), nullptr, m_origin);
                result = check;
            } else {
                speculate(Overflow, noValue(), 0, m_out.testIsZero32(value, m_out.constInt32(0x7fffffff)));
                result = m_out.neg(value);
            }

            setInt32(result);
            break;
        }
            
        case Int52RepUse: {
            if (!abstractValue(m_node->child1()).couldBeType(SpecInt52)) {
                Int52Kind kind;
                LValue value = lowWhicheverInt52(m_node->child1(), kind);
                LValue result = m_out.neg(value);
                if (shouldCheckNegativeZero(m_node->arithMode()))
                    speculate(NegativeZero, noValue(), 0, m_out.isZero64(result));
                setInt52(result, kind);
                break;
            }
            
            LValue value = lowInt52(m_node->child1());
            CheckValue* result = m_out.speculateSub(m_out.int64Zero, value);
            blessSpeculation(result, Int52Overflow, noValue(), nullptr, m_origin);
            speculate(NegativeZero, noValue(), 0, m_out.isZero64(result));
            setInt52(result);
            break;
        }
            
        case DoubleRepUse: {
            setDouble(m_out.doubleNeg(lowDouble(m_node->child1())));
            break;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }
    
    void compileBitAnd()
    {
        if (m_node->isBinaryUseKind(UntypedUse)) {
            emitBinaryBitOpSnippet<JITBitAndGenerator>(operationValueBitAnd);
            return;
        }
        setInt32(m_out.bitAnd(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitOr()
    {
        if (m_node->isBinaryUseKind(UntypedUse)) {
            emitBinaryBitOpSnippet<JITBitOrGenerator>(operationValueBitOr);
            return;
        }
        setInt32(m_out.bitOr(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitXor()
    {
        if (m_node->isBinaryUseKind(UntypedUse)) {
            emitBinaryBitOpSnippet<JITBitXorGenerator>(operationValueBitXor);
            return;
        }
        setInt32(m_out.bitXor(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileBitRShift()
    {
        if (m_node->isBinaryUseKind(UntypedUse)) {
            emitRightShiftSnippet(JITRightShiftGenerator::SignedShift);
            return;
        }
        setInt32(m_out.aShr(
            lowInt32(m_node->child1()),
            m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileBitLShift()
    {
        if (m_node->isBinaryUseKind(UntypedUse)) {
            emitBinaryBitOpSnippet<JITLeftShiftGenerator>(operationValueBitLShift);
            return;
        }
        setInt32(m_out.shl(
            lowInt32(m_node->child1()),
            m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileBitURShift()
    {
        if (m_node->isBinaryUseKind(UntypedUse)) {
            emitRightShiftSnippet(JITRightShiftGenerator::UnsignedShift);
            return;
        }
        setInt32(m_out.lShr(
            lowInt32(m_node->child1()),
            m_out.bitAnd(lowInt32(m_node->child2()), m_out.constInt32(31))));
    }
    
    void compileUInt32ToNumber()
    {
        LValue value = lowInt32(m_node->child1());

        if (doesOverflow(m_node->arithMode())) {
            setStrictInt52(m_out.zeroExtPtr(value));
            return;
        }

        speculate(Overflow, noValue(), 0, m_out.lessThan(value, m_out.int32Zero));
        setInt32(value);
    }
    
    void compileCheckStructure()
    {
        ExitKind exitKind;
        if (m_node->child1()->hasConstant())
            exitKind = BadConstantCache;
        else
            exitKind = BadCache;

        switch (m_node->child1().useKind()) {
        case CellUse:
        case KnownCellUse: {
            LValue cell = lowCell(m_node->child1());
            
            checkStructure(
                m_out.load32(cell, m_heaps.JSCell_structureID), jsValueValue(cell),
                exitKind, m_node->structureSet(),
                [&] (Structure* structure) {
                    return weakStructureID(structure);
                });
            return;
        }

        case CellOrOtherUse: {
            LValue value = lowJSValue(m_node->child1(), ManualOperandSpeculation);

            LBasicBlock cellCase = m_out.newBlock();
            LBasicBlock notCellCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            m_out.branch(
                isCell(value, provenType(m_node->child1())), unsure(cellCase), unsure(notCellCase));

            LBasicBlock lastNext = m_out.appendTo(cellCase, notCellCase);
            checkStructure(
                m_out.load32(value, m_heaps.JSCell_structureID), jsValueValue(value),
                exitKind, m_node->structureSet(),
                [&] (Structure* structure) {
                    return weakStructureID(structure);
                });
            m_out.jump(continuation);

            m_out.appendTo(notCellCase, continuation);
            FTL_TYPE_CHECK(jsValueValue(value), m_node->child1(), SpecCell | SpecOther, isNotOther(value));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            return;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            return;
        }
    }
    
    void compileCheckCell()
    {
        LValue cell = lowCell(m_node->child1());
        
        speculate(
            BadCell, jsValueValue(cell), m_node->child1().node(),
            m_out.notEqual(cell, weakPointer(m_node->cellOperand()->cell())));
    }
    
    void compileCheckBadCell()
    {
        terminate(BadCell);
    }

    void compileCheckNotEmpty()
    {
        speculate(TDZFailure, noValue(), nullptr, m_out.isZero64(lowJSValue(m_node->child1())));
    }

    void compileCheckIdent()
    {
        UniquedStringImpl* uid = m_node->uidOperand();
        if (uid->isSymbol()) {
            LValue symbol = lowSymbol(m_node->child1());
            LValue stringImpl = m_out.loadPtr(symbol, m_heaps.Symbol_privateName);
            speculate(BadIdent, noValue(), nullptr, m_out.notEqual(stringImpl, m_out.constIntPtr(uid)));
        } else {
            LValue string = lowStringIdent(m_node->child1());
            LValue stringImpl = m_out.loadPtr(string, m_heaps.JSString_value);
            speculate(BadIdent, noValue(), nullptr, m_out.notEqual(stringImpl, m_out.constIntPtr(uid)));
        }
    }

    void compileGetExecutable()
    {
        LValue cell = lowCell(m_node->child1());
        speculateFunction(m_node->child1(), cell);
        setJSValue(m_out.loadPtr(cell, m_heaps.JSFunction_executable));
    }
    
    void compileArrayifyToStructure()
    {
        LValue cell = lowCell(m_node->child1());
        LValue property = !!m_node->child2() ? lowInt32(m_node->child2()) : 0;
        
        LBasicBlock unexpectedStructure = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue structureID = m_out.load32(cell, m_heaps.JSCell_structureID);
        
        m_out.branch(
            m_out.notEqual(structureID, weakStructureID(m_node->structure())),
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
            vmCall(m_out.voidType, m_out.operation(operationEnsureInt32), m_callFrame, cell);
            break;
        case Array::Double:
            vmCall(m_out.voidType, m_out.operation(operationEnsureDouble), m_callFrame, cell);
            break;
        case Array::Contiguous:
            vmCall(m_out.voidType, m_out.operation(operationEnsureContiguous), m_callFrame, cell);
            break;
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            vmCall(m_out.voidType, m_out.operation(operationEnsureArrayStorage), m_callFrame, cell);
            break;
        default:
            DFG_CRASH(m_graph, m_node, "Bad array type");
            break;
        }
        
        structureID = m_out.load32(cell, m_heaps.JSCell_structureID);
        speculate(
            BadIndexingType, jsValueValue(cell), 0,
            m_out.notEqual(structureID, weakStructureID(m_node->structure())));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compilePutStructure()
    {
        m_ftlState.jitCode->common.notifyCompilingStructureTransition(m_graph.m_plan, codeBlock(), m_node);

        Structure* oldStructure = m_node->transition()->previous;
        Structure* newStructure = m_node->transition()->next;
        ASSERT_UNUSED(oldStructure, oldStructure->indexingType() == newStructure->indexingType());
        ASSERT(oldStructure->typeInfo().inlineTypeFlags() == newStructure->typeInfo().inlineTypeFlags());
        ASSERT(oldStructure->typeInfo().type() == newStructure->typeInfo().type());

        LValue cell = lowCell(m_node->child1()); 
        m_out.store32(
            weakStructureID(newStructure),
            cell, m_heaps.JSCell_structureID);
    }
    
    void compileGetById()
    {
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
            
            LBasicBlock cellCase = m_out.newBlock();
            LBasicBlock notCellCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                isCell(value, provenType(m_node->child1())), unsure(cellCase), unsure(notCellCase));
            
            LBasicBlock lastNext = m_out.appendTo(cellCase, notCellCase);
            ValueFromBlock cellResult = m_out.anchor(getById(value));
            m_out.jump(continuation);
            
            m_out.appendTo(notCellCase, continuation);
            ValueFromBlock notCellResult = m_out.anchor(vmCall(
                m_out.int64, m_out.operation(operationGetByIdGeneric),
                m_callFrame, value,
                m_out.constIntPtr(m_graph.identifiers()[m_node->identifierNumber()])));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, cellResult, notCellResult));
            return;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            return;
        }
    }
    
    void compilePutById()
    {
        Node* node = m_node;
        
        // See above; CellUse is easier so we do only that for now.
        ASSERT(node->child1().useKind() == CellUse);

        LValue base = lowCell(node->child1());
        LValue value = lowJSValue(node->child2());
        auto uid = m_graph.identifiers()[node->identifierNumber()];

        B3::PatchpointValue* patchpoint = m_out.patchpoint(Void);
        patchpoint->appendSomeRegister(base);
        patchpoint->appendSomeRegister(value);
        patchpoint->clobber(RegisterSet::macroScratchRegisters());

        // FIXME: If this is a PutByIdFlush, we might want to late-clobber volatile registers.
        // https://bugs.webkit.org/show_bug.cgi?id=152848

        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);

        State* state = &m_ftlState;
        ECMAMode ecmaMode = m_graph.executableFor(node->origin.semantic)->ecmaMode();
        
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);

                CallSiteIndex callSiteIndex =
                    state->jitCode->common.addUniqueCallSiteIndex(node->origin.semantic);

                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);

                // JS setter call ICs generated by the PutById IC will need this.
                exceptionHandle->scheduleExitCreationForUnwind(params, callSiteIndex);

                auto generator = Box<JITPutByIdGenerator>::create(
                    jit.codeBlock(), node->origin.semantic, callSiteIndex,
                    params.unavailableRegisters(), JSValueRegs(params[0].gpr()),
                    JSValueRegs(params[1].gpr()), GPRInfo::patchpointScratchRegister, ecmaMode,
                    node->op() == PutByIdDirect ? Direct : NotDirect);

                generator->generateFastPath(jit);
                CCallHelpers::Label done = jit.label();

                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);

                        generator->slowPathJump().link(&jit);
                        CCallHelpers::Label slowPathBegin = jit.label();
                        CCallHelpers::Call slowPathCall = callOperation(
                            *state, params.unavailableRegisters(), jit, node->origin.semantic,
                            exceptions.get(), generator->slowPathFunction(), InvalidGPRReg,
                            CCallHelpers::TrustedImmPtr(generator->stubInfo()), params[1].gpr(),
                            params[0].gpr(), CCallHelpers::TrustedImmPtr(uid)).call();
                        jit.jump().linkTo(done, &jit);

                        generator->reportSlowPathCall(slowPathBegin, slowPathCall);

                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                generator->finalize(linkBuffer);
                            });
                    });
            });
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
            LBasicBlock slowPath = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue fastResultValue = m_out.loadPtr(cell, m_heaps.JSString_value);
            ValueFromBlock fastResult = m_out.anchor(fastResultValue);
            
            m_out.branch(
                m_out.notNull(fastResultValue), usually(continuation), rarely(slowPath));
            
            LBasicBlock lastNext = m_out.appendTo(slowPath, continuation);
            
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(m_out.intPtr, m_out.operation(operationResolveRope), m_callFrame, cell));
            
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
        
        if (m_node->arrayMode().alreadyChecked(m_graph, m_node, abstractValue(edge)))
            return;
        
        speculate(
            BadIndexingType, jsValueValue(cell), 0,
            m_out.logicalNot(isArrayType(cell, m_node->arrayMode())));
    }

    void compileGetTypedArrayByteOffset()
    {
        LValue basePtr = lowCell(m_node->child1());    

        LBasicBlock simpleCase = m_out.newBlock();
        LBasicBlock wastefulCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue mode = m_out.load32(basePtr, m_heaps.JSArrayBufferView_mode);
        m_out.branch(
            m_out.notEqual(mode, m_out.constInt32(WastefulTypedArray)),
            unsure(simpleCase), unsure(wastefulCase));

        LBasicBlock lastNext = m_out.appendTo(simpleCase, wastefulCase);

        ValueFromBlock simpleOut = m_out.anchor(m_out.constIntPtr(0));

        m_out.jump(continuation);

        m_out.appendTo(wastefulCase, continuation);

        LValue vectorPtr = m_out.loadPtr(basePtr, m_heaps.JSArrayBufferView_vector);
        LValue butterflyPtr = m_out.loadPtr(basePtr, m_heaps.JSObject_butterfly);
        LValue arrayBufferPtr = m_out.loadPtr(butterflyPtr, m_heaps.Butterfly_arrayBuffer);
        LValue dataPtr = m_out.loadPtr(arrayBufferPtr, m_heaps.ArrayBuffer_data);

        ValueFromBlock wastefulOut = m_out.anchor(m_out.sub(vectorPtr, dataPtr));

        m_out.jump(continuation);
        m_out.appendTo(continuation, lastNext);

        setInt32(m_out.castToInt32(m_out.phi(m_out.intPtr, simpleOut, wastefulOut)));
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
            
        case Array::DirectArguments: {
            LValue arguments = lowCell(m_node->child1());
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.notNull(m_out.loadPtr(arguments, m_heaps.DirectArguments_overrides)));
            setInt32(m_out.load32NonNegative(arguments, m_heaps.DirectArguments_length));
            return;
        }
            
        case Array::ScopedArguments: {
            LValue arguments = lowCell(m_node->child1());
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.notZero32(m_out.load8ZeroExt32(arguments, m_heaps.ScopedArguments_overrodeThings)));
            setInt32(m_out.load32NonNegative(arguments, m_heaps.ScopedArguments_totalLength));
            return;
        }
            
        default:
            if (m_node->arrayMode().isSomeTypedArrayView()) {
                setInt32(
                    m_out.load32NonNegative(lowCell(m_node->child1()), m_heaps.JSArrayBufferView_length));
                return;
            }
            
            DFG_CRASH(m_graph, m_node, "Bad array type");
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
                LValue isHole = m_out.isZero64(result);
                if (m_node->arrayMode().isSaneChain()) {
                    DFG_ASSERT(
                        m_graph, m_node, m_node->arrayMode().type() == Array::Contiguous);
                    result = m_out.select(
                        isHole, m_out.constInt64(JSValue::encode(jsUndefined())), result);
                } else
                    speculate(LoadFromHole, noValue(), 0, isHole);
                setJSValue(result);
                return;
            }
            
            LValue base = lowCell(m_node->child1());
            
            LBasicBlock fastCase = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                m_out.aboveOrEqual(
                    index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength)),
                rarely(slowCase), usually(fastCase));
            
            LBasicBlock lastNext = m_out.appendTo(fastCase, slowCase);

            LValue fastResultValue = m_out.load64(baseIndex(heap, storage, index, m_node->child2()));
            ValueFromBlock fastResult = m_out.anchor(fastResultValue);
            m_out.branch(
                m_out.isZero64(fastResultValue), rarely(slowCase), usually(continuation));
            
            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(m_out.int64, m_out.operation(operationGetByValArrayInt), m_callFrame, base, index));
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
            
            LBasicBlock inBounds = m_out.newBlock();
            LBasicBlock boxPath = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
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
                vmCall(m_out.int64, m_out.operation(operationGetByValArrayInt), m_callFrame, base, index));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
            return;
        }

        case Array::Undecided: {
            LValue index = lowInt32(m_node->child2());

            speculate(OutOfBounds, noValue(), m_node, m_out.lessThan(index, m_out.int32Zero));
            setJSValue(m_out.constInt64(ValueUndefined));
            return;
        }
            
        case Array::DirectArguments: {
            LValue base = lowCell(m_node->child1());
            LValue index = lowInt32(m_node->child2());
            
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.notNull(m_out.loadPtr(base, m_heaps.DirectArguments_overrides)));
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.aboveOrEqual(
                    index,
                    m_out.load32NonNegative(base, m_heaps.DirectArguments_length)));

            TypedPointer address = m_out.baseIndex(
                m_heaps.DirectArguments_storage, base, m_out.zeroExtPtr(index));
            setJSValue(m_out.load64(address));
            return;
        }
            
        case Array::ScopedArguments: {
            LValue base = lowCell(m_node->child1());
            LValue index = lowInt32(m_node->child2());
            
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.aboveOrEqual(
                    index,
                    m_out.load32NonNegative(base, m_heaps.ScopedArguments_totalLength)));
            
            LValue table = m_out.loadPtr(base, m_heaps.ScopedArguments_table);
            LValue namedLength = m_out.load32(table, m_heaps.ScopedArgumentsTable_length);
            
            LBasicBlock namedCase = m_out.newBlock();
            LBasicBlock overflowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                m_out.aboveOrEqual(index, namedLength), unsure(overflowCase), unsure(namedCase));
            
            LBasicBlock lastNext = m_out.appendTo(namedCase, overflowCase);
            
            LValue scope = m_out.loadPtr(base, m_heaps.ScopedArguments_scope);
            LValue arguments = m_out.loadPtr(table, m_heaps.ScopedArgumentsTable_arguments);
            
            TypedPointer address = m_out.baseIndex(
                m_heaps.scopedArgumentsTableArguments, arguments, m_out.zeroExtPtr(index));
            LValue scopeOffset = m_out.load32(address);
            
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.equal(scopeOffset, m_out.constInt32(ScopeOffset::invalidOffset)));
            
            address = m_out.baseIndex(
                m_heaps.JSEnvironmentRecord_variables, scope, m_out.zeroExtPtr(scopeOffset));
            ValueFromBlock namedResult = m_out.anchor(m_out.load64(address));
            m_out.jump(continuation);
            
            m_out.appendTo(overflowCase, continuation);
            
            address = m_out.baseIndex(
                m_heaps.ScopedArguments_overflowStorage, base,
                m_out.zeroExtPtr(m_out.sub(index, namedLength)));
            LValue overflowValue = m_out.load64(address);
            speculate(ExoticObjectMode, noValue(), nullptr, m_out.isZero64(overflowValue));
            ValueFromBlock overflowResult = m_out.anchor(overflowValue);
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, namedResult, overflowResult));
            return;
        }
            
        case Array::Generic: {
            setJSValue(vmCall(
                m_out.int64, m_out.operation(operationGetByVal), m_callFrame,
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
                            m_out.zeroExtPtr(index),
                            m_out.constIntPtr(logElementSize(type)))));
                
                if (isInt(type)) {
                    LValue result;
                    switch (elementSize(type)) {
                    case 1:
                        result = isSigned(type) ? m_out.load8SignExt32(pointer) :  m_out.load8ZeroExt32(pointer);
                        break;
                    case 2:
                        result = isSigned(type) ? m_out.load16SignExt32(pointer) :  m_out.load16ZeroExt32(pointer);
                        break;
                    case 4:
                        result = m_out.load32(pointer);
                        break;
                    default:
                        DFG_CRASH(m_graph, m_node, "Bad element size");
                    }
                    
                    if (elementSize(type) < 4 || isSigned(type)) {
                        setInt32(result);
                        return;
                    }

                    if (m_node->shouldSpeculateInt32()) {
                        speculate(
                            Overflow, noValue(), 0, m_out.lessThan(result, m_out.int32Zero));
                        setInt32(result);
                        return;
                    }
                    
                    if (m_node->shouldSpeculateMachineInt()) {
                        setStrictInt52(m_out.zeroExt(result, m_out.int64));
                        return;
                    }
                    
                    setDouble(m_out.unsignedToDouble(result));
                    return;
                }
            
                ASSERT(isFloat(type));
                
                LValue result;
                switch (type) {
                case TypeFloat32:
                    result = m_out.floatToDouble(m_out.loadFloat(pointer));
                    break;
                case TypeFloat64:
                    result = m_out.loadDouble(pointer);
                    break;
                default:
                    DFG_CRASH(m_graph, m_node, "Bad typed array type");
                }
                
                setDouble(result);
                return;
            }
            
            DFG_CRASH(m_graph, m_node, "Bad array type");
            return;
        } }
    }
    
    void compileGetMyArgumentByVal()
    {
        InlineCallFrame* inlineCallFrame = m_node->child1()->origin.semantic.inlineCallFrame;
        
        LValue index = lowInt32(m_node->child2());
        
        LValue limit;
        if (inlineCallFrame && !inlineCallFrame->isVarargs())
            limit = m_out.constInt32(inlineCallFrame->arguments.size() - 1);
        else {
            VirtualRegister argumentCountRegister;
            if (!inlineCallFrame)
                argumentCountRegister = VirtualRegister(JSStack::ArgumentCount);
            else
                argumentCountRegister = inlineCallFrame->argumentCountRegister;
            limit = m_out.sub(m_out.load32(payloadFor(argumentCountRegister)), m_out.int32One);
        }
        
        speculate(ExoticObjectMode, noValue(), 0, m_out.aboveOrEqual(index, limit));
        
        TypedPointer base;
        if (inlineCallFrame) {
            if (inlineCallFrame->arguments.size() <= 1) {
                // We should have already exited due to the bounds check, above. Just tell the
                // compiler that anything dominated by this instruction is not reachable, so
                // that we don't waste time generating such code. This will also plant some
                // kind of crashing instruction so that if by some fluke the bounds check didn't
                // work, we'll crash in an easy-to-see way.
                didAlreadyTerminate();
                return;
            }
            base = addressFor(inlineCallFrame->arguments[1].virtualRegister());
        } else
            base = addressFor(virtualRegisterForArgument(1));
        
        LValue pointer = m_out.baseIndex(
            base.value(), m_out.zeroExt(index, m_out.intPtr), ScaleEight);
        setJSValue(m_out.load64(TypedPointer(m_heaps.variables.atAnyIndex(), pointer)));
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
                m_out.voidType, m_out.operation(operation), m_callFrame,
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
            LBasicBlock continuation = m_out.newBlock();
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
                    storage, m_out.zeroExtPtr(index), provenValue(child2));
                
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
                    doubleValue(value), child3, SpecDoubleReal,
                    m_out.doubleNotEqualOrUnordered(value, value));
                
                TypedPointer elementPointer = m_out.baseIndex(
                    m_heaps.indexedDoubleProperties, storage, m_out.zeroExtPtr(index),
                    provenValue(child2));
                
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
                DFG_CRASH(m_graph, m_node, "Bad array type");
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
                
                Output::StoreType storeType;
                LValue valueToStore;
                
                if (isInt(type)) {
                    LValue intValue;
                    switch (child3.useKind()) {
                    case Int52RepUse:
                    case Int32Use: {
                        if (child3.useKind() == Int32Use)
                            intValue = lowInt32(child3);
                        else
                            intValue = m_out.castToInt32(lowStrictInt52(child3));

                        if (isClamped(type)) {
                            ASSERT(elementSize(type) == 1);
                            
                            LBasicBlock atLeastZero = m_out.newBlock();
                            LBasicBlock continuation = m_out.newBlock();
                            
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
                        
                    case DoubleRepUse: {
                        LValue doubleValue = lowDouble(child3);
                        
                        if (isClamped(type)) {
                            ASSERT(elementSize(type) == 1);
                            
                            LBasicBlock atLeastZero = m_out.newBlock();
                            LBasicBlock withinRange = m_out.newBlock();
                            LBasicBlock continuation = m_out.newBlock();
                            
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
                            intValues.append(m_out.anchor(m_out.doubleToInt(doubleValue)));
                            m_out.jump(continuation);
                            
                            m_out.appendTo(continuation, lastNext);
                            intValue = m_out.phi(m_out.int32, intValues);
                        } else
                            intValue = doubleToInt32(doubleValue);
                        break;
                    }
                        
                    default:
                        DFG_CRASH(m_graph, m_node, "Bad use kind");
                    }

                    valueToStore = intValue;
                    switch (elementSize(type)) {
                    case 1:
                        storeType = Output::Store32As8;
                        break;
                    case 2:
                        storeType = Output::Store32As16;
                        break;
                    case 4:
                        storeType = Output::Store32;
                        break;
                    default:
                        DFG_CRASH(m_graph, m_node, "Bad element size");
                    }
                } else /* !isInt(type) */ {
                    LValue value = lowDouble(child3);
                    switch (type) {
                    case TypeFloat32:
                        valueToStore = m_out.doubleToFloat(value);
                        storeType = Output::StoreFloat;
                        break;
                    case TypeFloat64:
                        valueToStore = value;
                        storeType = Output::StoreDouble;
                        break;
                    default:
                        DFG_CRASH(m_graph, m_node, "Bad typed array type");
                    }
                }
                
                if (m_node->arrayMode().isInBounds() || m_node->op() == PutByValAlias)
                    m_out.store(valueToStore, pointer, storeType);
                else {
                    LBasicBlock isInBounds = m_out.newBlock();
                    LBasicBlock continuation = m_out.newBlock();
                    
                    m_out.branch(
                        m_out.aboveOrEqual(index, lowInt32(child5)),
                        unsure(continuation), unsure(isInBounds));
                    
                    LBasicBlock lastNext = m_out.appendTo(isInBounds, continuation);
                    m_out.store(valueToStore, pointer, storeType);
                    m_out.jump(continuation);
                    
                    m_out.appendTo(continuation, lastNext);
                }
                
                return;
            }
            
            DFG_CRASH(m_graph, m_node, "Bad array type");
            break;
        }
    }

    void compilePutAccessorById()
    {
        LValue base = lowCell(m_node->child1());
        LValue accessor = lowCell(m_node->child2());
        auto uid = m_graph.identifiers()[m_node->identifierNumber()];
        vmCall(
            m_out.voidType,
            m_out.operation(m_node->op() == PutGetterById ? operationPutGetterById : operationPutSetterById),
            m_callFrame, base, m_out.constIntPtr(uid), m_out.constInt32(m_node->accessorAttributes()), accessor);
    }

    void compilePutGetterSetterById()
    {
        LValue base = lowCell(m_node->child1());
        LValue getter = lowJSValue(m_node->child2());
        LValue setter = lowJSValue(m_node->child3());
        auto uid = m_graph.identifiers()[m_node->identifierNumber()];
        vmCall(
            m_out.voidType, m_out.operation(operationPutGetterSetter),
            m_callFrame, base, m_out.constIntPtr(uid), m_out.constInt32(m_node->accessorAttributes()), getter, setter);

    }

    void compilePutAccessorByVal()
    {
        LValue base = lowCell(m_node->child1());
        LValue subscript = lowJSValue(m_node->child2());
        LValue accessor = lowCell(m_node->child3());
        vmCall(
            m_out.voidType,
            m_out.operation(m_node->op() == PutGetterByVal ? operationPutGetterByVal : operationPutSetterByVal),
            m_callFrame, base, subscript, m_out.constInt32(m_node->accessorAttributes()), accessor);
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
            Output::StoreType storeType;
            
            if (m_node->arrayMode().type() != Array::Double) {
                value = lowJSValue(m_node->child2(), ManualOperandSpeculation);
                if (m_node->arrayMode().type() == Array::Int32) {
                    FTL_TYPE_CHECK(
                        jsValueValue(value), m_node->child2(), SpecInt32, isNotInt32(value));
                }
                storeType = Output::Store64;
            } else {
                value = lowDouble(m_node->child2());
                FTL_TYPE_CHECK(
                    doubleValue(value), m_node->child2(), SpecDoubleReal,
                    m_out.doubleNotEqualOrUnordered(value, value));
                storeType = Output::StoreDouble;
            }
            
            IndexedAbstractHeap& heap = m_heaps.forArrayType(m_node->arrayMode().type());

            LValue prevLength = m_out.load32(storage, m_heaps.Butterfly_publicLength);
            
            LBasicBlock fastPath = m_out.newBlock();
            LBasicBlock slowPath = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                m_out.aboveOrEqual(
                    prevLength, m_out.load32(storage, m_heaps.Butterfly_vectorLength)),
                unsure(slowPath), unsure(fastPath));
            
            LBasicBlock lastNext = m_out.appendTo(fastPath, slowPath);
            m_out.store(
                value, m_out.baseIndex(heap, storage, m_out.zeroExtPtr(prevLength)), storeType);
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
                vmCall(m_out.int64, operation, m_callFrame, value, base));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
            return;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad array type");
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
            
            LBasicBlock fastCase = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            LValue prevLength = m_out.load32(storage, m_heaps.Butterfly_publicLength);
            
            Vector<ValueFromBlock, 3> results;
            results.append(m_out.anchor(m_out.constInt64(JSValue::encode(jsUndefined()))));
            m_out.branch(
                m_out.isZero32(prevLength), rarely(continuation), usually(fastCase));
            
            LBasicBlock lastNext = m_out.appendTo(fastCase, slowCase);
            LValue newLength = m_out.sub(prevLength, m_out.int32One);
            m_out.store32(newLength, storage, m_heaps.Butterfly_publicLength);
            TypedPointer pointer = m_out.baseIndex(heap, storage, m_out.zeroExtPtr(newLength));
            if (m_node->arrayMode().type() != Array::Double) {
                LValue result = m_out.load64(pointer);
                m_out.store64(m_out.int64Zero, pointer);
                results.append(m_out.anchor(result));
                m_out.branch(
                    m_out.notZero64(result), usually(continuation), rarely(slowCase));
            } else {
                LValue result = m_out.loadDouble(pointer);
                m_out.store64(m_out.constInt64(bitwise_cast<int64_t>(PNaN)), pointer);
                results.append(m_out.anchor(boxDouble(result)));
                m_out.branch(
                    m_out.doubleEqual(result, result),
                    usually(continuation), rarely(slowCase));
            }
            
            m_out.appendTo(slowCase, continuation);
            results.append(m_out.anchor(vmCall(
                m_out.int64, m_out.operation(operationArrayPopAndRecoverLength), m_callFrame, base)));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, results));
            return;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad array type");
            return;
        }
    }
    
    void compileCreateActivation()
    {
        LValue scope = lowCell(m_node->child1());
        SymbolTable* table = m_node->castOperand<SymbolTable*>();
        Structure* structure = m_graph.globalObjectFor(m_node->origin.semantic)->activationStructure();
        JSValue initializationValue = m_node->initializationValueForActivation();
        ASSERT(initializationValue.isUndefined() || initializationValue == jsTDZValue());
        if (table->singletonScope()->isStillValid()) {
            LValue callResult = vmCall(
                m_out.int64,
                m_out.operation(operationCreateActivationDirect), m_callFrame, weakPointer(structure),
                scope, weakPointer(table), m_out.constInt64(JSValue::encode(initializationValue)));
            setJSValue(callResult);
            return;
        }
        
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        LValue fastObject = allocateObject<JSLexicalEnvironment>(
            JSLexicalEnvironment::allocationSize(table), structure, m_out.intPtrZero, slowPath);
        
        // We don't need memory barriers since we just fast-created the activation, so the
        // activation must be young.
        m_out.storePtr(scope, fastObject, m_heaps.JSScope_next);
        m_out.storePtr(weakPointer(table), fastObject, m_heaps.JSSymbolTableObject_symbolTable);
        
        for (unsigned i = 0; i < table->scopeSize(); ++i) {
            m_out.store64(
                m_out.constInt64(JSValue::encode(initializationValue)),
                fastObject, m_heaps.JSEnvironmentRecord_variables[i]);
        }
        
        ValueFromBlock fastResult = m_out.anchor(fastObject);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        LValue callResult = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationCreateActivationDirect, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure), locations[1].directGPR(),
                    CCallHelpers::TrustedImmPtr(table),
                    CCallHelpers::TrustedImm64(JSValue::encode(initializationValue)));
            },
            scope);
        ValueFromBlock slowResult = m_out.anchor(callResult);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.intPtr, fastResult, slowResult));
    }
    
    void compileNewFunction()
    {
        ASSERT(m_node->op() == NewFunction || m_node->op() == NewArrowFunction || m_node->op() == NewGeneratorFunction);
        bool isGeneratorFunction = m_node->op() == NewGeneratorFunction;
        
        LValue scope = lowCell(m_node->child1());
        
        FunctionExecutable* executable = m_node->castOperand<FunctionExecutable*>();
        if (executable->singletonFunction()->isStillValid()) {
            LValue callResult =
                isGeneratorFunction ? vmCall(m_out.int64, m_out.operation(operationNewGeneratorFunction), m_callFrame, scope, weakPointer(executable)) :
                vmCall(m_out.int64, m_out.operation(operationNewFunction), m_callFrame, scope, weakPointer(executable));
            setJSValue(callResult);
            return;
        }
        
        Structure* structure =
            isGeneratorFunction ? m_graph.globalObjectFor(m_node->origin.semantic)->generatorFunctionStructure() :
            m_graph.globalObjectFor(m_node->origin.semantic)->functionStructure();
        
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        LValue fastObject =
            isGeneratorFunction ? allocateObject<JSGeneratorFunction>(structure, m_out.intPtrZero, slowPath) :
            allocateObject<JSFunction>(structure, m_out.intPtrZero, slowPath);
        
        
        // We don't need memory barriers since we just fast-created the function, so it
        // must be young.
        m_out.storePtr(scope, fastObject, m_heaps.JSFunction_scope);
        m_out.storePtr(weakPointer(executable), fastObject, m_heaps.JSFunction_executable);
        m_out.storePtr(m_out.intPtrZero, fastObject, m_heaps.JSFunction_rareData);
        
        ValueFromBlock fastResult = m_out.anchor(fastObject);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);

        Vector<LValue> slowPathArguments;
        slowPathArguments.append(scope);
        LValue callResult = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                if (isGeneratorFunction) {
                    return createLazyCallGenerator(
                        operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint,
                        locations[0].directGPR(), locations[1].directGPR(),
                        CCallHelpers::TrustedImmPtr(executable));
                }
                return createLazyCallGenerator(
                    operationNewFunctionWithInvalidatedReallocationWatchpoint,
                    locations[0].directGPR(), locations[1].directGPR(),
                    CCallHelpers::TrustedImmPtr(executable));
            },
            slowPathArguments);
        ValueFromBlock slowResult = m_out.anchor(callResult);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.intPtr, fastResult, slowResult));
    }
    
    void compileCreateDirectArguments()
    {
        // FIXME: A more effective way of dealing with the argument count and callee is to have
        // them be explicit arguments to this node.
        // https://bugs.webkit.org/show_bug.cgi?id=142207
        
        Structure* structure =
            m_graph.globalObjectFor(m_node->origin.semantic)->directArgumentsStructure();
        
        unsigned minCapacity = m_graph.baselineCodeBlockFor(m_node->origin.semantic)->numParameters() - 1;
        
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        ArgumentsLength length = getArgumentsLength();
        
        LValue fastObject;
        if (length.isKnown) {
            fastObject = allocateObject<DirectArguments>(
                DirectArguments::allocationSize(std::max(length.known, minCapacity)), structure,
                m_out.intPtrZero, slowPath);
        } else {
            LValue size = m_out.add(
                m_out.shl(length.value, m_out.constInt32(3)),
                m_out.constInt32(DirectArguments::storageOffset()));
            
            size = m_out.select(
                m_out.aboveOrEqual(length.value, m_out.constInt32(minCapacity)),
                size, m_out.constInt32(DirectArguments::allocationSize(minCapacity)));
            
            fastObject = allocateVariableSizedObject<DirectArguments>(
                size, structure, m_out.intPtrZero, slowPath);
        }
        
        m_out.store32(length.value, fastObject, m_heaps.DirectArguments_length);
        m_out.store32(m_out.constInt32(minCapacity), fastObject, m_heaps.DirectArguments_minCapacity);
        m_out.storePtr(m_out.intPtrZero, fastObject, m_heaps.DirectArguments_overrides);
        
        ValueFromBlock fastResult = m_out.anchor(fastObject);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        LValue callResult = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationCreateDirectArguments, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure), locations[1].directGPR(),
                    CCallHelpers::TrustedImm32(minCapacity));
            }, length.value);
        ValueFromBlock slowResult = m_out.anchor(callResult);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        LValue result = m_out.phi(m_out.intPtr, fastResult, slowResult);

        m_out.storePtr(getCurrentCallee(), result, m_heaps.DirectArguments_callee);
        
        if (length.isKnown) {
            VirtualRegister start = AssemblyHelpers::argumentsStart(m_node->origin.semantic);
            for (unsigned i = 0; i < std::max(length.known, minCapacity); ++i) {
                m_out.store64(
                    m_out.load64(addressFor(start + i)),
                    result, m_heaps.DirectArguments_storage[i]);
            }
        } else {
            LValue stackBase = getArgumentsStart();
            
            LBasicBlock loop = m_out.newBlock();
            LBasicBlock end = m_out.newBlock();

            ValueFromBlock originalLength;
            if (minCapacity) {
                LValue capacity = m_out.select(
                    m_out.aboveOrEqual(length.value, m_out.constInt32(minCapacity)),
                    length.value,
                    m_out.constInt32(minCapacity));
                LValue originalLengthValue = m_out.zeroExtPtr(capacity);
                originalLength = m_out.anchor(originalLengthValue);
                m_out.jump(loop);
            } else {
                LValue originalLengthValue = m_out.zeroExtPtr(length.value);
                originalLength = m_out.anchor(originalLengthValue);
                m_out.branch(m_out.isNull(originalLengthValue), unsure(end), unsure(loop));
            }
            
            lastNext = m_out.appendTo(loop, end);
            LValue previousIndex = m_out.phi(m_out.intPtr, originalLength);
            LValue index = m_out.sub(previousIndex, m_out.intPtrOne);
            m_out.store64(
                m_out.load64(m_out.baseIndex(m_heaps.variables, stackBase, index)),
                m_out.baseIndex(m_heaps.DirectArguments_storage, result, index));
            ValueFromBlock nextIndex = m_out.anchor(index);
            m_out.addIncomingToPhi(previousIndex, nextIndex);
            m_out.branch(m_out.isNull(index), unsure(end), unsure(loop));
            
            m_out.appendTo(end, lastNext);
        }
        
        setJSValue(result);
    }
    
    void compileCreateScopedArguments()
    {
        LValue scope = lowCell(m_node->child1());
        
        LValue result = vmCall(
            m_out.int64, m_out.operation(operationCreateScopedArguments), m_callFrame,
            weakPointer(
                m_graph.globalObjectFor(m_node->origin.semantic)->scopedArgumentsStructure()),
            getArgumentsStart(), getArgumentsLength().value, getCurrentCallee(), scope);
        
        setJSValue(result);
    }
    
    void compileCreateClonedArguments()
    {
        LValue result = vmCall(
            m_out.int64, m_out.operation(operationCreateClonedArguments), m_callFrame,
            weakPointer(
                m_graph.globalObjectFor(m_node->origin.semantic)->clonedArgumentsStructure()),
            getArgumentsStart(), getArgumentsLength().value, getCurrentCallee());
        
        setJSValue(result);
    }

    void compileCopyRest()
    {            
        LBasicBlock doCopyRest = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue arrayLength = lowInt32(m_node->child2());

        m_out.branch(
            m_out.equal(arrayLength, m_out.constInt32(0)),
            unsure(continuation), unsure(doCopyRest));
            
        LBasicBlock lastNext = m_out.appendTo(doCopyRest, continuation);
        // Arguments: 0:exec, 1:JSCell* array, 2:arguments start, 3:number of arguments to skip, 4:array length
        LValue numberOfArgumentsToSkip = m_out.constInt32(m_node->numberOfArgumentsToSkip());
        vmCall(
            m_out.voidType,m_out.operation(operationCopyRest), m_callFrame, lowCell(m_node->child1()),
            getArgumentsStart(), numberOfArgumentsToSkip, arrayLength);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
    }

    void compileGetRestLength()
    {
        LBasicBlock nonZeroLength = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        ValueFromBlock zeroLengthResult = m_out.anchor(m_out.constInt32(0));

        LValue numberOfArgumentsToSkip = m_out.constInt32(m_node->numberOfArgumentsToSkip());
        LValue argumentsLength = getArgumentsLength().value;
        m_out.branch(m_out.above(argumentsLength, numberOfArgumentsToSkip),
            unsure(nonZeroLength), unsure(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(nonZeroLength, continuation);
        ValueFromBlock nonZeroLengthResult = m_out.anchor(m_out.sub(argumentsLength, numberOfArgumentsToSkip));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setInt32(m_out.phi(m_out.int32, zeroLengthResult, nonZeroLengthResult));
    }
    
    void compileNewObject()
    {
        setJSValue(allocateObject(m_node->structure()));
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

        if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(m_node->indexingType())) {
            unsigned numElements = m_node->numChildren();
            
            ArrayValues arrayValues = allocateJSArray(structure, numElements);
            
            for (unsigned operandIndex = 0; operandIndex < m_node->numChildren(); ++operandIndex) {
                Edge edge = m_graph.varArgChild(m_node, operandIndex);
                
                switch (m_node->indexingType()) {
                case ALL_BLANK_INDEXING_TYPES:
                case ALL_UNDECIDED_INDEXING_TYPES:
                    DFG_CRASH(m_graph, m_node, "Bad indexing type");
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
                    DFG_CRASH(m_graph, m_node, "Corrupt indexing type");
                    break;
                }
            }
            
            setJSValue(arrayValues.array);
            return;
        }
        
        if (!m_node->numChildren()) {
            setJSValue(vmCall(
                m_out.int64, m_out.operation(operationNewEmptyArray), m_callFrame,
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
            m_out.int64, m_out.operation(operationNewArray), m_callFrame,
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
        
        if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(m_node->indexingType())) {
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
            m_out.int64, m_out.operation(operationNewArrayBuffer), m_callFrame,
            m_out.constIntPtr(structure), m_out.constIntPtr(m_node->startConstant()),
            m_out.constIntPtr(m_node->numConstants())));
    }
    
    void compileNewArrayWithSize()
    {
        LValue publicLength = lowInt32(m_node->child1());
        
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(
            m_node->indexingType());
        
        if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(m_node->indexingType())) {
            ASSERT(
                hasUndecided(structure->indexingType())
                || hasInt32(structure->indexingType())
                || hasDouble(structure->indexingType())
                || hasContiguous(structure->indexingType()));

            LBasicBlock fastCase = m_out.newBlock();
            LBasicBlock largeCase = m_out.newBlock();
            LBasicBlock failCase = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                m_out.aboveOrEqual(publicLength, m_out.constInt32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)),
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
            
            LValue object = allocateObject<JSArray>(structure, butterfly, failCase);
            
            m_out.store32(publicLength, butterfly, m_heaps.Butterfly_publicLength);
            m_out.store32(vectorLength, butterfly, m_heaps.Butterfly_vectorLength);

            initializeArrayElements(m_node->indexingType(), vectorLength, butterfly);
            
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
            LValue slowResultValue = lazySlowPath(
                [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(
                        operationNewArrayWithSize, locations[0].directGPR(),
                        locations[1].directGPR(), locations[2].directGPR());
                },
                structureValue, publicLength);
            ValueFromBlock slowResult = m_out.anchor(slowResultValue);
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.intPtr, fastResult, slowResult));
            return;
        }
        
        LValue structureValue = m_out.select(
            m_out.aboveOrEqual(publicLength, m_out.constInt32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)),
            m_out.constIntPtr(
                globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage)),
            m_out.constIntPtr(structure));
        setJSValue(vmCall(m_out.int64, m_out.operation(operationNewArrayWithSize), m_callFrame, structureValue, publicLength));
    }

    void compileNewTypedArray()
    {
        TypedArrayType type = m_node->typedArrayType();
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        
        switch (m_node->child1().useKind()) {
        case Int32Use: {
            Structure* structure = globalObject->typedArrayStructure(type);

            LValue size = lowInt32(m_node->child1());

            LBasicBlock smallEnoughCase = m_out.newBlock();
            LBasicBlock nonZeroCase = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            m_out.branch(
                m_out.above(size, m_out.constInt32(JSArrayBufferView::fastSizeLimit)),
                rarely(slowCase), usually(smallEnoughCase));

            LBasicBlock lastNext = m_out.appendTo(smallEnoughCase, nonZeroCase);

            m_out.branch(m_out.notZero32(size), usually(nonZeroCase), rarely(slowCase));

            m_out.appendTo(nonZeroCase, slowCase);

            LValue byteSize =
                m_out.shl(m_out.zeroExtPtr(size), m_out.constInt32(logElementSize(type)));
            if (elementSize(type) < 8) {
                byteSize = m_out.bitAnd(
                    m_out.add(byteSize, m_out.constIntPtr(7)),
                    m_out.constIntPtr(~static_cast<intptr_t>(7)));
            }
        
            LValue storage = allocateBasicStorage(byteSize, slowCase);

            LValue fastResultValue =
                allocateObject<JSArrayBufferView>(structure, m_out.intPtrZero, slowCase);

            m_out.storePtr(storage, fastResultValue, m_heaps.JSArrayBufferView_vector);
            m_out.store32(size, fastResultValue, m_heaps.JSArrayBufferView_length);
            m_out.store32(m_out.constInt32(FastTypedArray), fastResultValue, m_heaps.JSArrayBufferView_mode);

            ValueFromBlock fastResult = m_out.anchor(fastResultValue);
            m_out.jump(continuation);

            m_out.appendTo(slowCase, continuation);

            LValue slowResultValue = lazySlowPath(
                [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(
                        operationNewTypedArrayWithSizeForType(type), locations[0].directGPR(),
                        CCallHelpers::TrustedImmPtr(structure), locations[1].directGPR());
                },
                size);
            ValueFromBlock slowResult = m_out.anchor(slowResultValue);
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.intPtr, fastResult, slowResult));
            return;
        }

        case UntypedUse: {
            LValue argument = lowJSValue(m_node->child1());

            LValue result = vmCall(
                m_out.intPtr, m_out.operation(operationNewTypedArrayWithOneArgumentForType(type)),
                m_callFrame, weakPointer(globalObject->typedArrayStructure(type)), argument);

            setJSValue(result);
            return;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            return;
        }
    }
    
    void compileAllocatePropertyStorage()
    {
        LValue object = lowCell(m_node->child1());
        setStorage(allocatePropertyStorage(object, m_node->transition()->previous));
    }

    void compileReallocatePropertyStorage()
    {
        Transition* transition = m_node->transition();
        LValue object = lowCell(m_node->child1());
        LValue oldStorage = lowStorage(m_node->child2());
        
        setStorage(
            reallocatePropertyStorage(
                object, oldStorage, transition->previous, transition->next));
    }
    
    void compileToStringOrCallStringConstructor()
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
            
            LBasicBlock notString = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
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
            
            LBasicBlock isCell = m_out.newBlock();
            LBasicBlock notString = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            LValue isCellPredicate;
            if (m_node->child1().useKind() == CellUse)
                isCellPredicate = m_out.booleanTrue;
            else
                isCellPredicate = this->isCell(value, provenType(m_node->child1()));
            m_out.branch(isCellPredicate, unsure(isCell), unsure(notString));
            
            LBasicBlock lastNext = m_out.appendTo(isCell, notString);
            ValueFromBlock simpleResult = m_out.anchor(value);
            LValue isStringPredicate;
            if (m_node->child1()->prediction() & SpecString) {
                isStringPredicate = isString(value, provenType(m_node->child1()));
            } else
                isStringPredicate = m_out.booleanFalse;
            m_out.branch(isStringPredicate, unsure(continuation), unsure(notString));
            
            m_out.appendTo(notString, continuation);
            LValue operation;
            if (m_node->child1().useKind() == CellUse)
                operation = m_out.operation(m_node->op() == ToString ? operationToStringOnCell : operationCallStringConstructorOnCell);
            else
                operation = m_out.operation(m_node->op() == ToString ? operationToString : operationCallStringConstructor);
            ValueFromBlock convertedResult = m_out.anchor(vmCall(m_out.int64, operation, m_callFrame, value));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(m_out.int64, simpleResult, convertedResult));
            return;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }
    
    void compileToPrimitive()
    {
        LValue value = lowJSValue(m_node->child1());
        
        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock isObjectCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        Vector<ValueFromBlock, 3> results;
        
        results.append(m_out.anchor(value));
        m_out.branch(
            isCell(value, provenType(m_node->child1())), unsure(isCellCase), unsure(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(isCellCase, isObjectCase);
        results.append(m_out.anchor(value));
        m_out.branch(
            isObject(value, provenType(m_node->child1())),
            unsure(isObjectCase), unsure(continuation));
        
        m_out.appendTo(isObjectCase, continuation);
        results.append(m_out.anchor(vmCall(
            m_out.int64, m_out.operation(operationToPrimitive), m_callFrame, value)));
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
        
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        MarkedAllocator& allocator =
            vm().heap.allocatorForObjectWithDestructor(sizeof(JSRopeString));
        
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
            CheckValue* lengthCheck = m_out.speculateAdd(
                length, m_out.load32(kids[i], m_heaps.JSString_length));
            blessSpeculation(lengthCheck, Uncountable, noValue(), nullptr, m_origin);
            length = lengthCheck;
        }
        m_out.store32(
            m_out.bitAnd(m_out.constInt32(JSString::Is8Bit), flags),
            result, m_heaps.JSString_flags);
        m_out.store32(length, result, m_heaps.JSString_length);
        
        ValueFromBlock fastResult = m_out.anchor(result);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        LValue slowResultValue;
        switch (numKids) {
        case 2:
            slowResultValue = lazySlowPath(
                [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(
                        operationMakeRope2, locations[0].directGPR(), locations[1].directGPR(),
                        locations[2].directGPR());
                }, kids[0], kids[1]);
            break;
        case 3:
            slowResultValue = lazySlowPath(
                [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(
                        operationMakeRope3, locations[0].directGPR(), locations[1].directGPR(),
                        locations[2].directGPR(), locations[3].directGPR());
                }, kids[0], kids[1], kids[2]);
            break;
        default:
            DFG_CRASH(m_graph, m_node, "Bad number of children");
            break;
        }
        ValueFromBlock slowResult = m_out.anchor(slowResultValue);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
    }
    
    void compileStringCharAt()
    {
        LValue base = lowCell(m_node->child1());
        LValue index = lowInt32(m_node->child2());
        LValue storage = lowStorage(m_node->child3());
            
        LBasicBlock fastPath = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
            
        m_out.branch(
            m_out.aboveOrEqual(
                index, m_out.load32NonNegative(base, m_heaps.JSString_length)),
            rarely(slowPath), usually(fastPath));
            
        LBasicBlock lastNext = m_out.appendTo(fastPath, slowPath);
            
        LValue stringImpl = m_out.loadPtr(base, m_heaps.JSString_value);
            
        LBasicBlock is8Bit = m_out.newBlock();
        LBasicBlock is16Bit = m_out.newBlock();
        LBasicBlock bitsContinuation = m_out.newBlock();
        LBasicBlock bigCharacter = m_out.newBlock();
            
        m_out.branch(
            m_out.testIsZero32(
                m_out.load32(stringImpl, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIs8Bit())),
            unsure(is16Bit), unsure(is8Bit));
            
        m_out.appendTo(is8Bit, is16Bit);
            
        ValueFromBlock char8Bit = m_out.anchor(
            m_out.load8ZeroExt32(m_out.baseIndex(
                m_heaps.characters8, storage, m_out.zeroExtPtr(index),
                provenValue(m_node->child2()))));
        m_out.jump(bitsContinuation);
            
        m_out.appendTo(is16Bit, bigCharacter);

        LValue char16BitValue = m_out.load16ZeroExt32(
            m_out.baseIndex(
                m_heaps.characters16, storage, m_out.zeroExtPtr(index),
                provenValue(m_node->child2())));
        ValueFromBlock char16Bit = m_out.anchor(char16BitValue);
        m_out.branch(
            m_out.aboveOrEqual(char16BitValue, m_out.constInt32(0x100)),
            rarely(bigCharacter), usually(bitsContinuation));
            
        m_out.appendTo(bigCharacter, bitsContinuation);
            
        Vector<ValueFromBlock, 4> results;
        results.append(m_out.anchor(vmCall(
            m_out.int64, m_out.operation(operationSingleCharacterString),
            m_callFrame, char16BitValue)));
        m_out.jump(continuation);
            
        m_out.appendTo(bitsContinuation, slowPath);
            
        LValue character = m_out.phi(m_out.int32, char8Bit, char16Bit);
            
        LValue smallStrings = m_out.constIntPtr(vm().smallStrings.singleCharacterStrings());
            
        results.append(m_out.anchor(m_out.loadPtr(m_out.baseIndex(
            m_heaps.singleCharacterStrings, smallStrings, m_out.zeroExtPtr(character)))));
        m_out.jump(continuation);
            
        m_out.appendTo(slowPath, continuation);
            
        if (m_node->arrayMode().isInBounds()) {
            speculate(OutOfBounds, noValue(), 0, m_out.booleanTrue);
            results.append(m_out.anchor(m_out.intPtrZero));
        } else {
            JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
                
            if (globalObject->stringPrototypeChainIsSane()) {
                // FIXME: This could be captured using a Speculation mode that means
                // "out-of-bounds loads return a trivial value", something like
                // SaneChainOutOfBounds.
                // https://bugs.webkit.org/show_bug.cgi?id=144668
                
                m_graph.watchpoints().addLazily(globalObject->stringPrototype()->structure()->transitionWatchpointSet());
                m_graph.watchpoints().addLazily(globalObject->objectPrototype()->structure()->transitionWatchpointSet());
                
                LBasicBlock negativeIndex = m_out.newBlock();
                    
                results.append(m_out.anchor(m_out.constInt64(JSValue::encode(jsUndefined()))));
                m_out.branch(
                    m_out.lessThan(index, m_out.int32Zero),
                    rarely(negativeIndex), usually(continuation));
                    
                m_out.appendTo(negativeIndex, continuation);
            }
                
            results.append(m_out.anchor(vmCall(
                m_out.int64, m_out.operation(operationGetByValStringInt), m_callFrame, base, index)));
        }
            
        m_out.jump(continuation);
            
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, results));
    }
    
    void compileStringCharCodeAt()
    {
        LBasicBlock is8Bit = m_out.newBlock();
        LBasicBlock is16Bit = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

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
            
        ValueFromBlock char8Bit = m_out.anchor(
            m_out.load8ZeroExt32(m_out.baseIndex(
                m_heaps.characters8, storage, m_out.zeroExtPtr(index),
                provenValue(m_node->child2()))));
        m_out.jump(continuation);
            
        m_out.appendTo(is16Bit, continuation);
            
        ValueFromBlock char16Bit = m_out.anchor(
            m_out.load16ZeroExt32(m_out.baseIndex(
                m_heaps.characters16, storage, m_out.zeroExtPtr(index),
                provenValue(m_node->child2()))));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        setInt32(m_out.phi(m_out.int32, char8Bit, char16Bit));
    }

    void compileStringFromCharCode()
    {
        Edge childEdge = m_node->child1();
        
        if (childEdge.useKind() == UntypedUse) {
            LValue result = vmCall(
                m_out.int64, m_out.operation(operationStringFromCharCodeUntyped), m_callFrame,
                lowJSValue(childEdge));
            setJSValue(result);
            return;
        }

        DFG_ASSERT(m_graph, m_node, childEdge.useKind() == Int32Use);

        LValue value = lowInt32(childEdge);
        
        LBasicBlock smallIntCase = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(
            m_out.aboveOrEqual(value, m_out.constInt32(0xff)),
            rarely(slowCase), usually(smallIntCase));

        LBasicBlock lastNext = m_out.appendTo(smallIntCase, slowCase);

        LValue smallStrings = m_out.constIntPtr(vm().smallStrings.singleCharacterStrings());
        LValue fastResultValue = m_out.loadPtr(
            m_out.baseIndex(m_heaps.singleCharacterStrings, smallStrings, m_out.zeroExtPtr(value)));
        ValueFromBlock fastResult = m_out.anchor(fastResultValue);
        m_out.jump(continuation);

        m_out.appendTo(slowCase, continuation);

        LValue slowResultValue = vmCall(
            m_out.intPtr, m_out.operation(operationStringFromCharCode), m_callFrame, value);
        ValueFromBlock slowResult = m_out.anchor(slowResultValue);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);

        setJSValue(m_out.phi(m_out.int64, fastResult, slowResult));
    }
    
    void compileGetByOffset()
    {
        StorageAccessData& data = m_node->storageAccessData();
        
        setJSValue(loadProperty(
            lowStorage(m_node->child1()), data.identifierNumber, data.offset));
    }
    
    void compileGetGetter()
    {
        setJSValue(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.GetterSetter_getter));
    }
    
    void compileGetSetter()
    {
        setJSValue(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.GetterSetter_setter));
    }
    
    void compileMultiGetByOffset()
    {
        LValue base = lowCell(m_node->child1());
        
        MultiGetByOffsetData& data = m_node->multiGetByOffsetData();

        if (data.cases.isEmpty()) {
            // Protect against creating a Phi function with zero inputs. LLVM didn't like that.
            // It's not clear if this is needed anymore.
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=154382
            terminate(BadCache);
            return;
        }
        
        Vector<LBasicBlock, 2> blocks(data.cases.size());
        for (unsigned i = data.cases.size(); i--;)
            blocks[i] = m_out.newBlock();
        LBasicBlock exit = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        Vector<SwitchCase, 2> cases;
        StructureSet baseSet;
        for (unsigned i = data.cases.size(); i--;) {
            MultiGetByOffsetCase getCase = data.cases[i];
            for (unsigned j = getCase.set().size(); j--;) {
                Structure* structure = getCase.set()[j];
                baseSet.add(structure);
                cases.append(SwitchCase(weakStructureID(structure), blocks[i], Weight(1)));
            }
        }
        m_out.switchInstruction(
            m_out.load32(base, m_heaps.JSCell_structureID), cases, exit, Weight(0));
        
        LBasicBlock lastNext = m_out.m_nextBlock;
        
        Vector<ValueFromBlock, 2> results;
        for (unsigned i = data.cases.size(); i--;) {
            MultiGetByOffsetCase getCase = data.cases[i];
            GetByOffsetMethod method = getCase.method();
            
            m_out.appendTo(blocks[i], i + 1 < data.cases.size() ? blocks[i + 1] : exit);
            
            LValue result;
            
            switch (method.kind()) {
            case GetByOffsetMethod::Invalid:
                RELEASE_ASSERT_NOT_REACHED();
                break;
                
            case GetByOffsetMethod::Constant:
                result = m_out.constInt64(JSValue::encode(method.constant()->value()));
                break;
                
            case GetByOffsetMethod::Load:
            case GetByOffsetMethod::LoadFromPrototype: {
                LValue propertyBase;
                if (method.kind() == GetByOffsetMethod::Load)
                    propertyBase = base;
                else
                    propertyBase = weakPointer(method.prototype()->value().asCell());
                if (!isInlineOffset(method.offset()))
                    propertyBase = m_out.loadPtr(propertyBase, m_heaps.JSObject_butterfly);
                result = loadProperty(
                    propertyBase, data.identifierNumber, method.offset());
                break;
            } }
            
            results.append(m_out.anchor(result));
            m_out.jump(continuation);
        }
        
        m_out.appendTo(exit, continuation);
        if (!m_interpreter.forNode(m_node->child1()).m_structure.isSubsetOf(baseSet))
            speculate(BadCache, noValue(), nullptr, m_out.booleanTrue);
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, results));
    }
    
    void compilePutByOffset()
    {
        StorageAccessData& data = m_node->storageAccessData();
        
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
            blocks[i] = m_out.newBlock();
        LBasicBlock exit = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        Vector<SwitchCase, 2> cases;
        StructureSet baseSet;
        for (unsigned i = data.variants.size(); i--;) {
            PutByIdVariant variant = data.variants[i];
            for (unsigned j = variant.oldStructure().size(); j--;) {
                Structure* structure = variant.oldStructure()[j];
                baseSet.add(structure);
                cases.append(SwitchCase(weakStructureID(structure), blocks[i], Weight(1)));
            }
        }
        m_out.switchInstruction(
            m_out.load32(base, m_heaps.JSCell_structureID), cases, exit, Weight(0));
        
        LBasicBlock lastNext = m_out.m_nextBlock;
        
        for (unsigned i = data.variants.size(); i--;) {
            m_out.appendTo(blocks[i], i + 1 < data.variants.size() ? blocks[i + 1] : exit);
            
            PutByIdVariant variant = data.variants[i];

            checkInferredType(m_node->child2(), value, variant.requiredType());
            
            LValue storage;
            if (variant.kind() == PutByIdVariant::Replace) {
                if (isInlineOffset(variant.offset()))
                    storage = base;
                else
                    storage = m_out.loadPtr(base, m_heaps.JSObject_butterfly);
            } else {
                m_graph.m_plan.transitions.addLazily(
                    codeBlock(), m_node->origin.semantic.codeOriginOwner(),
                    variant.oldStructureForTransition(), variant.newStructure());
                
                storage = storageForTransition(
                    base, variant.offset(),
                    variant.oldStructureForTransition(), variant.newStructure());

                ASSERT(variant.oldStructureForTransition()->indexingType() == variant.newStructure()->indexingType());
                ASSERT(variant.oldStructureForTransition()->typeInfo().inlineTypeFlags() == variant.newStructure()->typeInfo().inlineTypeFlags());
                ASSERT(variant.oldStructureForTransition()->typeInfo().type() == variant.newStructure()->typeInfo().type());
                m_out.store32(
                    weakStructureID(variant.newStructure()), base, m_heaps.JSCell_structureID);
            }
            
            storeProperty(value, storage, data.identifierNumber, variant.offset());
            m_out.jump(continuation);
        }
        
        m_out.appendTo(exit, continuation);
        if (!m_interpreter.forNode(m_node->child1()).m_structure.isSubsetOf(baseSet))
            speculate(BadCache, noValue(), nullptr, m_out.booleanTrue);
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compileGetGlobalVariable()
    {
        setJSValue(m_out.load64(m_out.absolute(m_node->variablePointer())));
    }
    
    void compilePutGlobalVariable()
    {
        m_out.store64(
            lowJSValue(m_node->child2()), m_out.absolute(m_node->variablePointer()));
    }
    
    void compileNotifyWrite()
    {
        WatchpointSet* set = m_node->watchpointSet();
        
        LBasicBlock isNotInvalidated = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue state = m_out.load8ZeroExt32(m_out.absolute(set->addressOfState()));
        m_out.branch(
            m_out.equal(state, m_out.constInt32(IsInvalidated)),
            usually(continuation), rarely(isNotInvalidated));
        
        LBasicBlock lastNext = m_out.appendTo(isNotInvalidated, continuation);

        lazySlowPath(
            [=] (const Vector<Location>&) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationNotifyWrite, InvalidGPRReg, CCallHelpers::TrustedImmPtr(set));
            });
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compileGetCallee()
    {
        setJSValue(m_out.loadPtr(addressFor(JSStack::Callee)));
    }
    
    void compileGetArgumentCount()
    {
        setInt32(m_out.load32(payloadFor(JSStack::ArgumentCount)));
    }
    
    void compileGetScope()
    {
        setJSValue(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSFunction_scope));
    }
    
    void compileSkipScope()
    {
        setJSValue(m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSScope_next));
    }

    void compileGetGlobalObject()
    {
        LValue structure = loadStructure(lowCell(m_node->child1()));
        setJSValue(m_out.loadPtr(structure, m_heaps.Structure_globalObject));
    }
    
    void compileGetClosureVar()
    {
        setJSValue(
            m_out.load64(
                lowCell(m_node->child1()),
                m_heaps.JSEnvironmentRecord_variables[m_node->scopeOffset().offset()]));
    }
    
    void compilePutClosureVar()
    {
        m_out.store64(
            lowJSValue(m_node->child2()),
            lowCell(m_node->child1()),
            m_heaps.JSEnvironmentRecord_variables[m_node->scopeOffset().offset()]);
    }
    
    void compileGetFromArguments()
    {
        setJSValue(
            m_out.load64(
                lowCell(m_node->child1()),
                m_heaps.DirectArguments_storage[m_node->capturedArgumentsOffset().offset()]));
    }
    
    void compilePutToArguments()
    {
        m_out.store64(
            lowJSValue(m_node->child2()),
            lowCell(m_node->child1()),
            m_heaps.DirectArguments_storage[m_node->capturedArgumentsOffset().offset()]);
    }
    
    void compileCompareEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)
            || m_node->isBinaryUseKind(Int52RepUse)
            || m_node->isBinaryUseKind(DoubleRepUse)
            || m_node->isBinaryUseKind(ObjectUse)
            || m_node->isBinaryUseKind(BooleanUse)
            || m_node->isBinaryUseKind(SymbolUse)
            || m_node->isBinaryUseKind(StringIdentUse)
            || m_node->isBinaryUseKind(StringUse)) {
            compileCompareStrictEq();
            return;
        }
        
        if (m_node->isBinaryUseKind(ObjectUse, ObjectOrOtherUse)) {
            compareEqObjectOrOtherToObject(m_node->child2(), m_node->child1());
            return;
        }
        
        if (m_node->isBinaryUseKind(ObjectOrOtherUse, ObjectUse)) {
            compareEqObjectOrOtherToObject(m_node->child1(), m_node->child2());
            return;
        }
        
        if (m_node->isBinaryUseKind(UntypedUse)) {
            nonSpeculativeCompare(
                [&] (LValue left, LValue right) {
                    return m_out.equal(left, right);
                },
                operationCompareEq);
            return;
        }

        if (m_node->child1().useKind() == OtherUse) {
            ASSERT(!m_interpreter.needsTypeCheck(m_node->child1(), SpecOther));
            setBoolean(equalNullOrUndefined(m_node->child2(), AllCellsAreFalse, EqualNullOrUndefined, ManualOperandSpeculation));
            return;
        }

        if (m_node->child2().useKind() == OtherUse) {
            ASSERT(!m_interpreter.needsTypeCheck(m_node->child2(), SpecOther));
            setBoolean(equalNullOrUndefined(m_node->child1(), AllCellsAreFalse, EqualNullOrUndefined, ManualOperandSpeculation));
            return;
        }

        DFG_CRASH(m_graph, m_node, "Bad use kinds");
    }
    
    void compileCompareStrictEq()
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            setBoolean(
                m_out.equal(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(Int52RepUse)) {
            Int52Kind kind;
            LValue left = lowWhicheverInt52(m_node->child1(), kind);
            LValue right = lowInt52(m_node->child2(), kind);
            setBoolean(m_out.equal(left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(DoubleRepUse)) {
            setBoolean(
                m_out.doubleEqual(lowDouble(m_node->child1()), lowDouble(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(StringIdentUse)) {
            setBoolean(
                m_out.equal(lowStringIdent(m_node->child1()), lowStringIdent(m_node->child2())));
            return;
        }

        if (m_node->isBinaryUseKind(StringUse)) {
            LValue left = lowCell(m_node->child1());
            LValue right = lowCell(m_node->child2());

            LBasicBlock notTriviallyEqualCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            speculateString(m_node->child1(), left);

            ValueFromBlock fastResult = m_out.anchor(m_out.booleanTrue);
            m_out.branch(
                m_out.equal(left, right), unsure(continuation), unsure(notTriviallyEqualCase));

            LBasicBlock lastNext = m_out.appendTo(notTriviallyEqualCase, continuation);

            speculateString(m_node->child2(), right);
            
            ValueFromBlock slowResult = m_out.anchor(stringsEqual(left, right));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(m_out.boolean, fastResult, slowResult));
            return;
        }

        if (m_node->isBinaryUseKind(ObjectUse, UntypedUse)) {
            setBoolean(
                m_out.equal(
                    lowNonNullObject(m_node->child1()),
                    lowJSValue(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(UntypedUse, ObjectUse)) {
            setBoolean(
                m_out.equal(
                    lowNonNullObject(m_node->child2()),
                    lowJSValue(m_node->child1())));
            return;
        }

        if (m_node->isBinaryUseKind(ObjectUse)) {
            setBoolean(
                m_out.equal(
                    lowNonNullObject(m_node->child1()),
                    lowNonNullObject(m_node->child2())));
            return;
        }
        
        if (m_node->isBinaryUseKind(BooleanUse)) {
            setBoolean(
                m_out.equal(lowBoolean(m_node->child1()), lowBoolean(m_node->child2())));
            return;
        }

        if (m_node->isBinaryUseKind(SymbolUse)) {
            LValue left = lowSymbol(m_node->child1());
            LValue right = lowSymbol(m_node->child2());
            LValue leftStringImpl = m_out.loadPtr(left, m_heaps.Symbol_privateName);
            LValue rightStringImpl = m_out.loadPtr(right, m_heaps.Symbol_privateName);
            setBoolean(m_out.equal(leftStringImpl, rightStringImpl));
            return;
        }
        
        if (m_node->isBinaryUseKind(MiscUse, UntypedUse)
            || m_node->isBinaryUseKind(UntypedUse, MiscUse)) {
            speculate(m_node->child1());
            speculate(m_node->child2());
            LValue left = lowJSValue(m_node->child1(), ManualOperandSpeculation);
            LValue right = lowJSValue(m_node->child2(), ManualOperandSpeculation);
            setBoolean(m_out.equal(left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(StringIdentUse, NotStringVarUse)
            || m_node->isBinaryUseKind(NotStringVarUse, StringIdentUse)) {
            Edge leftEdge = m_node->childFor(StringIdentUse);
            Edge rightEdge = m_node->childFor(NotStringVarUse);
            
            LValue left = lowStringIdent(leftEdge);
            LValue rightValue = lowJSValue(rightEdge, ManualOperandSpeculation);
            
            LBasicBlock isCellCase = m_out.newBlock();
            LBasicBlock isStringCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
            m_out.branch(
                isCell(rightValue, provenType(rightEdge)),
                unsure(isCellCase), unsure(continuation));
            
            LBasicBlock lastNext = m_out.appendTo(isCellCase, isStringCase);
            ValueFromBlock notStringResult = m_out.anchor(m_out.booleanFalse);
            m_out.branch(
                isString(rightValue, provenType(rightEdge)),
                unsure(isStringCase), unsure(continuation));
            
            m_out.appendTo(isStringCase, continuation);
            LValue right = m_out.loadPtr(rightValue, m_heaps.JSString_value);
            speculateStringIdent(rightEdge, rightValue, right);
            ValueFromBlock isStringResult = m_out.anchor(m_out.equal(left, right));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(m_out.boolean, notCellResult, notStringResult, isStringResult));
            return;
        }
        
        DFG_CRASH(m_graph, m_node, "Bad use kinds");
    }
    
    void compileCompareStrictEqConstant()
    {
        JSValue constant = m_node->child2()->asJSValue();

        setBoolean(
            m_out.equal(
                lowJSValue(m_node->child1()),
                m_out.constInt64(JSValue::encode(constant))));
    }
    
    void compileCompareLess()
    {
        compare(
            [&] (LValue left, LValue right) {
                return m_out.lessThan(left, right);
            },
            [&] (LValue left, LValue right) {
                return m_out.doubleLessThan(left, right);
            },
            operationCompareLess);
    }
    
    void compileCompareLessEq()
    {
        compare(
            [&] (LValue left, LValue right) {
                return m_out.lessThanOrEqual(left, right);
            },
            [&] (LValue left, LValue right) {
                return m_out.doubleLessThanOrEqual(left, right);
            },
            operationCompareLessEq);
    }
    
    void compileCompareGreater()
    {
        compare(
            [&] (LValue left, LValue right) {
                return m_out.greaterThan(left, right);
            },
            [&] (LValue left, LValue right) {
                return m_out.doubleGreaterThan(left, right);
            },
            operationCompareGreater);
    }
    
    void compileCompareGreaterEq()
    {
        compare(
            [&] (LValue left, LValue right) {
                return m_out.greaterThanOrEqual(left, right);
            },
            [&] (LValue left, LValue right) {
                return m_out.doubleGreaterThanOrEqual(left, right);
            },
            operationCompareGreaterEq);
    }
    
    void compileLogicalNot()
    {
        setBoolean(m_out.logicalNot(boolify(m_node->child1())));
    }

    void compileCallOrConstruct()
    {
        Node* node = m_node;
        unsigned numArgs = node->numChildren() - 1;

        LValue jsCallee = lowJSValue(m_graph.varArgChild(node, 0));

        unsigned frameSize = JSStack::CallFrameHeaderSize + numArgs;
        unsigned alignedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), frameSize);

        // JS->JS calling convention requires that the caller allows this much space on top of stack to
        // get trashed by the callee, even if not all of that space is used to pass arguments. We tell
        // B3 this explicitly for two reasons:
        //
        // - We will only pass frameSize worth of stuff.
        // - The trashed stack guarantee is logically separate from the act of passing arguments, so we
        //   shouldn't rely on Air to infer the trashed stack property based on the arguments it ends
        //   up seeing.
        m_proc.requestCallArgAreaSize(alignedFrameSize);

        // Collect the arguments, since this can generate code and we want to generate it before we emit
        // the call.
        Vector<ConstrainedValue> arguments;

        // Make sure that the callee goes into GPR0 because that's where the slow path thunks expect the
        // callee to be.
        arguments.append(ConstrainedValue(jsCallee, ValueRep::reg(GPRInfo::regT0)));

        auto addArgument = [&] (LValue value, VirtualRegister reg, int offset) {
            intptr_t offsetFromSP =
                (reg.offset() - JSStack::CallerFrameAndPCSize) * sizeof(EncodedJSValue) + offset;
            arguments.append(ConstrainedValue(value, ValueRep::stackArgument(offsetFromSP)));
        };

        addArgument(jsCallee, VirtualRegister(JSStack::Callee), 0);
        addArgument(m_out.constInt32(numArgs), VirtualRegister(JSStack::ArgumentCount), PayloadOffset);
        for (unsigned i = 0; i < numArgs; ++i)
            addArgument(lowJSValue(m_graph.varArgChild(node, 1 + i)), virtualRegisterForArgument(i), 0);

        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendVector(arguments);

        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());
        patchpoint->resultConstraint = ValueRep::reg(GPRInfo::returnValueGPR);

        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CallSiteIndex callSiteIndex = state->jitCode->common.addUniqueCallSiteIndex(codeOrigin);

                exceptionHandle->scheduleExitCreationForUnwind(params, callSiteIndex);

                jit.store32(
                    CCallHelpers::TrustedImm32(callSiteIndex.bits()),
                    CCallHelpers::tagFor(VirtualRegister(JSStack::ArgumentCount)));

                CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();

                CCallHelpers::DataLabelPtr targetToCheck;
                CCallHelpers::Jump slowPath = jit.branchPtrWithPatch(
                    CCallHelpers::NotEqual, GPRInfo::regT0, targetToCheck,
                    CCallHelpers::TrustedImmPtr(0));

                CCallHelpers::Call fastCall = jit.nearCall();
                CCallHelpers::Jump done = jit.jump();

                slowPath.link(&jit);

                jit.move(CCallHelpers::TrustedImmPtr(callLinkInfo), GPRInfo::regT2);
                CCallHelpers::Call slowCall = jit.nearCall();
                done.link(&jit);

                callLinkInfo->setUpCall(
                    node->op() == Construct ? CallLinkInfo::Construct : CallLinkInfo::Call,
                    node->origin.semantic, GPRInfo::regT0);

                jit.addPtr(
                    CCallHelpers::TrustedImm32(-params.proc().frameSize()),
                    GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

                jit.addLinkTask(
                    [=] (LinkBuffer& linkBuffer) {
                        MacroAssemblerCodePtr linkCall =
                            linkBuffer.vm().getCTIStub(linkCallThunkGenerator).code();
                        linkBuffer.link(slowCall, FunctionPtr(linkCall.executableAddress()));

                        callLinkInfo->setCallLocations(
                            linkBuffer.locationOfNearCall(slowCall),
                            linkBuffer.locationOf(targetToCheck),
                            linkBuffer.locationOfNearCall(fastCall));
                    });
            });

        setJSValue(patchpoint);
    }

    void compileTailCall()
    {
        Node* node = m_node;
        unsigned numArgs = node->numChildren() - 1;

        LValue jsCallee = lowJSValue(m_graph.varArgChild(node, 0));
        
        // We want B3 to give us all of the arguments using whatever mechanism it thinks is
        // convenient. The generator then shuffles those arguments into our own call frame,
        // destroying our frame in the process.

        // Note that we don't have to do anything special for exceptions. A tail call is only a
        // tail call if it is not inside a try block.

        Vector<ConstrainedValue> arguments;

        arguments.append(ConstrainedValue(jsCallee, ValueRep::reg(GPRInfo::regT0)));

        for (unsigned i = 0; i < numArgs; ++i) {
            // Note: we could let the shuffler do boxing for us, but it's not super clear that this
            // would be better. Also, if we wanted to do that, then we'd have to teach the shuffler
            // that 32-bit values could land at 4-byte alignment but not 8-byte alignment.
            
            ConstrainedValue constrainedValue(
                lowJSValue(m_graph.varArgChild(node, 1 + i)),
                ValueRep::WarmAny);
            arguments.append(constrainedValue);
        }

        PatchpointValue* patchpoint = m_out.patchpoint(Void);
        patchpoint->appendVector(arguments);

        // Prevent any of the arguments from using the scratch register.
        patchpoint->clobberEarly(RegisterSet::macroScratchRegisters());

        // We don't have to tell the patchpoint that we will clobber registers, since we won't return
        // anyway.

        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CallSiteIndex callSiteIndex = state->jitCode->common.addUniqueCallSiteIndex(codeOrigin);

                CallFrameShuffleData shuffleData;
                shuffleData.numLocals = state->jitCode->common.frameRegisterCount;
                shuffleData.callee = ValueRecovery::inGPR(GPRInfo::regT0, DataFormatJS);

                for (unsigned i = 0; i < numArgs; ++i)
                    shuffleData.args.append(params[1 + i].recoveryForJSValue());

                shuffleData.setupCalleeSaveRegisters(jit.codeBlock());

                CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();

                CCallHelpers::DataLabelPtr targetToCheck;
                CCallHelpers::Jump slowPath = jit.branchPtrWithPatch(
                    CCallHelpers::NotEqual, GPRInfo::regT0, targetToCheck,
                    CCallHelpers::TrustedImmPtr(0));

                callLinkInfo->setFrameShuffleData(shuffleData);
                CallFrameShuffler(jit, shuffleData).prepareForTailCall();

                CCallHelpers::Call fastCall = jit.nearTailCall();

                slowPath.link(&jit);

                // Yes, this is really necessary. You could throw an exception in a host call on the
                // slow path. That'll route us to lookupExceptionHandler(), which unwinds starting
                // with the call site index of our frame. Bad things happen if it's not set.
                jit.store32(
                    CCallHelpers::TrustedImm32(callSiteIndex.bits()),
                    CCallHelpers::tagFor(VirtualRegister(JSStack::ArgumentCount)));

                CallFrameShuffler slowPathShuffler(jit, shuffleData);
                slowPathShuffler.setCalleeJSValueRegs(JSValueRegs(GPRInfo::regT0));
                slowPathShuffler.prepareForSlowPath();

                jit.move(CCallHelpers::TrustedImmPtr(callLinkInfo), GPRInfo::regT2);
                CCallHelpers::Call slowCall = jit.nearCall();

                jit.abortWithReason(JITDidReturnFromTailCall);

                callLinkInfo->setUpCall(CallLinkInfo::TailCall, codeOrigin, GPRInfo::regT0);

                jit.addLinkTask(
                    [=] (LinkBuffer& linkBuffer) {
                        MacroAssemblerCodePtr linkCall =
                            linkBuffer.vm().getCTIStub(linkCallThunkGenerator).code();
                        linkBuffer.link(slowCall, FunctionPtr(linkCall.executableAddress()));

                        callLinkInfo->setCallLocations(
                            linkBuffer.locationOfNearCall(slowCall),
                            linkBuffer.locationOf(targetToCheck),
                            linkBuffer.locationOfNearCall(fastCall));
                    });
            });
        
        m_out.unreachable();
    }
    
    void compileCallOrConstructVarargs()
    {
        Node* node = m_node;
        LValue jsCallee = lowJSValue(m_node->child1());
        LValue thisArg = lowJSValue(m_node->child3());
        
        LValue jsArguments = nullptr;
        bool forwarding = false;
        
        switch (node->op()) {
        case CallVarargs:
        case TailCallVarargs:
        case TailCallVarargsInlinedCaller:
        case ConstructVarargs:
            jsArguments = lowJSValue(node->child2());
            break;
        case CallForwardVarargs:
        case TailCallForwardVarargs:
        case TailCallForwardVarargsInlinedCaller:
        case ConstructForwardVarargs:
            forwarding = true;
            break;
        default:
            DFG_CRASH(m_graph, node, "bad node type");
            break;
        }
        
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);

        // Append the forms of the arguments that we will use before any clobbering happens.
        patchpoint->append(jsCallee, ValueRep::reg(GPRInfo::regT0));
        if (jsArguments)
            patchpoint->appendSomeRegister(jsArguments);
        patchpoint->appendSomeRegister(thisArg);

        if (!forwarding) {
            // Now append them again for after clobbering. Note that the compiler may ask us to use a
            // different register for the late for the post-clobbering version of the value. This gives
            // the compiler a chance to spill these values without having to burn any callee-saves.
            patchpoint->append(jsCallee, ValueRep::LateColdAny);
            patchpoint->append(jsArguments, ValueRep::LateColdAny);
            patchpoint->append(thisArg, ValueRep::LateColdAny);
        }

        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());
        patchpoint->resultConstraint = ValueRep::reg(GPRInfo::returnValueGPR);

        // This is the minimum amount of call arg area stack space that all JS->JS calls always have.
        unsigned minimumJSCallAreaSize =
            sizeof(CallerFrameAndPC) +
            WTF::roundUpToMultipleOf(stackAlignmentBytes(), 5 * sizeof(EncodedJSValue));

        m_proc.requestCallArgAreaSize(minimumJSCallAreaSize);
        
        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CallSiteIndex callSiteIndex =
                    state->jitCode->common.addUniqueCallSiteIndex(codeOrigin);

                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);

                exceptionHandle->scheduleExitCreationForUnwind(params, callSiteIndex);

                jit.store32(
                    CCallHelpers::TrustedImm32(callSiteIndex.bits()),
                    CCallHelpers::tagFor(VirtualRegister(JSStack::ArgumentCount)));

                CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();
                CallVarargsData* data = node->callVarargsData();

                unsigned argIndex = 1;
                GPRReg calleeGPR = params[argIndex++].gpr();
                ASSERT(calleeGPR == GPRInfo::regT0);
                GPRReg argumentsGPR = jsArguments ? params[argIndex++].gpr() : InvalidGPRReg;
                GPRReg thisGPR = params[argIndex++].gpr();

                B3::ValueRep calleeLateRep;
                B3::ValueRep argumentsLateRep;
                B3::ValueRep thisLateRep;
                if (!forwarding) {
                    // If we're not forwarding then we'll need callee, arguments, and this after we
                    // have potentially clobbered calleeGPR, argumentsGPR, and thisGPR. Our technique
                    // for this is to supply all of those operands as late uses in addition to
                    // specifying them as early uses. It's possible that the late use uses a spill
                    // while the early use uses a register, and it's possible for the late and early
                    // uses to use different registers. We do know that the late uses interfere with
                    // all volatile registers and so won't use those, but the early uses may use
                    // volatile registers and in the case of calleeGPR, it's pinned to regT0 so it
                    // definitely will.
                    //
                    // Note that we have to be super careful with these. It's possible that these
                    // use a shuffling of the registers used for calleeGPR, argumentsGPR, and
                    // thisGPR. If that happens and we do for example:
                    //
                    //     calleeLateRep.emitRestore(jit, calleeGPR);
                    //     argumentsLateRep.emitRestore(jit, calleeGPR);
                    //
                    // Then we might end up with garbage if calleeLateRep.gpr() == argumentsGPR and
                    // argumentsLateRep.gpr() == calleeGPR.
                    //
                    // We do a variety of things to prevent this from happening. For example, we use
                    // argumentsLateRep before needing the other two and after we've already stopped
                    // using the *GPRs. Also, we pin calleeGPR to regT0, and rely on the fact that
                    // the *LateReps cannot use volatile registers (so they cannot be regT0, so
                    // calleeGPR != argumentsLateRep.gpr() and calleeGPR != thisLateRep.gpr()).
                    //
                    // An alternative would have been to just use early uses and early-clobber all
                    // volatile registers. But that would force callee, arguments, and this into
                    // callee-save registers even if we have to spill them. We don't want spilling to
                    // use up three callee-saves.
                    //
                    // TL;DR: The way we use LateReps here is dangerous and barely works but achieves
                    // some desirable performance properties, so don't mistake the cleverness for
                    // elegance.
                    calleeLateRep = params[argIndex++];
                    argumentsLateRep = params[argIndex++];
                    thisLateRep = params[argIndex++];
                }

                // Get some scratch registers.
                RegisterSet usedRegisters;
                usedRegisters.merge(RegisterSet::stackRegisters());
                usedRegisters.merge(RegisterSet::reservedHardwareRegisters());
                usedRegisters.merge(RegisterSet::calleeSaveRegisters());
                usedRegisters.set(calleeGPR);
                if (argumentsGPR != InvalidGPRReg)
                    usedRegisters.set(argumentsGPR);
                usedRegisters.set(thisGPR);
                if (calleeLateRep.isReg())
                    usedRegisters.set(calleeLateRep.reg());
                if (argumentsLateRep.isReg())
                    usedRegisters.set(argumentsLateRep.reg());
                if (thisLateRep.isReg())
                    usedRegisters.set(thisLateRep.reg());
                ScratchRegisterAllocator allocator(usedRegisters);
                GPRReg scratchGPR1 = allocator.allocateScratchGPR();
                GPRReg scratchGPR2 = allocator.allocateScratchGPR();
                GPRReg scratchGPR3 = forwarding ? allocator.allocateScratchGPR() : InvalidGPRReg;
                RELEASE_ASSERT(!allocator.numberOfReusedRegisters());

                auto callWithExceptionCheck = [&] (void* callee) {
                    jit.move(CCallHelpers::TrustedImmPtr(callee), GPRInfo::nonPreservedNonArgumentGPR);
                    jit.call(GPRInfo::nonPreservedNonArgumentGPR);
                    exceptions->append(jit.emitExceptionCheck(AssemblyHelpers::NormalExceptionCheck, AssemblyHelpers::FarJumpWidth));
                };

                auto adjustStack = [&] (GPRReg amount) {
                    jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CallerFrameAndPC)), amount, CCallHelpers::stackPointerRegister);
                };

                unsigned originalStackHeight = params.proc().frameSize();

                if (forwarding) {
                    jit.move(CCallHelpers::TrustedImm32(originalStackHeight / sizeof(EncodedJSValue)), scratchGPR2);
                    
                    CCallHelpers::JumpList slowCase;
                    emitSetupVarargsFrameFastCase(jit, scratchGPR2, scratchGPR1, scratchGPR2, scratchGPR3, node->child2()->origin.semantic.inlineCallFrame, data->firstVarArgOffset, slowCase);

                    CCallHelpers::Jump done = jit.jump();
                    slowCase.link(&jit);
                    jit.setupArgumentsExecState();
                    callWithExceptionCheck(bitwise_cast<void*>(operationThrowStackOverflowForVarargs));
                    jit.abortWithReason(DFGVarargsThrowingPathDidNotThrow);
                    
                    done.link(&jit);

                    adjustStack(scratchGPR2);
                } else {
                    jit.move(CCallHelpers::TrustedImm32(originalStackHeight / sizeof(EncodedJSValue)), scratchGPR1);
                    jit.setupArgumentsWithExecState(argumentsGPR, scratchGPR1, CCallHelpers::TrustedImm32(data->firstVarArgOffset));
                    callWithExceptionCheck(bitwise_cast<void*>(operationSizeFrameForVarargs));

                    jit.move(GPRInfo::returnValueGPR, scratchGPR1);
                    jit.move(CCallHelpers::TrustedImm32(originalStackHeight / sizeof(EncodedJSValue)), scratchGPR2);
                    argumentsLateRep.emitRestore(jit, argumentsGPR);
                    emitSetVarargsFrame(jit, scratchGPR1, false, scratchGPR2, scratchGPR2);
                    jit.addPtr(CCallHelpers::TrustedImm32(-minimumJSCallAreaSize), scratchGPR2, CCallHelpers::stackPointerRegister);
                    jit.setupArgumentsWithExecState(scratchGPR2, argumentsGPR, CCallHelpers::TrustedImm32(data->firstVarArgOffset), scratchGPR1);
                    callWithExceptionCheck(bitwise_cast<void*>(operationSetupVarargsFrame));
                    
                    adjustStack(GPRInfo::returnValueGPR);

                    calleeLateRep.emitRestore(jit, GPRInfo::regT0);

                    // This may not emit code if thisGPR got a callee-save. Also, we're guaranteed
                    // that thisGPR != GPRInfo::regT0 because regT0 interferes with it.
                    thisLateRep.emitRestore(jit, thisGPR);
                }
                
                jit.store64(GPRInfo::regT0, CCallHelpers::calleeFrameSlot(JSStack::Callee));
                jit.store64(thisGPR, CCallHelpers::calleeArgumentSlot(0));
                
                CallLinkInfo::CallType callType;
                if (node->op() == ConstructVarargs || node->op() == ConstructForwardVarargs)
                    callType = CallLinkInfo::ConstructVarargs;
                else if (node->op() == TailCallVarargs || node->op() == TailCallForwardVarargs)
                    callType = CallLinkInfo::TailCallVarargs;
                else
                    callType = CallLinkInfo::CallVarargs;
                
                bool isTailCall = CallLinkInfo::callModeFor(callType) == CallMode::Tail;
                
                CCallHelpers::DataLabelPtr targetToCheck;
                CCallHelpers::Jump slowPath = jit.branchPtrWithPatch(
                    CCallHelpers::NotEqual, GPRInfo::regT0, targetToCheck,
                    CCallHelpers::TrustedImmPtr(0));
                
                CCallHelpers::Call fastCall;
                CCallHelpers::Jump done;
                
                if (isTailCall) {
                    jit.emitRestoreCalleeSaves();
                    jit.prepareForTailCallSlow();
                    fastCall = jit.nearTailCall();
                } else {
                    fastCall = jit.nearCall();
                    done = jit.jump();
                }
                
                slowPath.link(&jit);

                if (isTailCall)
                    jit.emitRestoreCalleeSaves();
                jit.move(CCallHelpers::TrustedImmPtr(callLinkInfo), GPRInfo::regT2);
                CCallHelpers::Call slowCall = jit.nearCall();
                
                if (isTailCall)
                    jit.abortWithReason(JITDidReturnFromTailCall);
                else
                    done.link(&jit);
                
                callLinkInfo->setUpCall(callType, node->origin.semantic, GPRInfo::regT0);
                
                jit.addPtr(
                    CCallHelpers::TrustedImm32(-originalStackHeight),
                    GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
                
                jit.addLinkTask(
                    [=] (LinkBuffer& linkBuffer) {
                        MacroAssemblerCodePtr linkCall =
                            linkBuffer.vm().getCTIStub(linkCallThunkGenerator).code();
                        linkBuffer.link(slowCall, FunctionPtr(linkCall.executableAddress()));
                        
                        callLinkInfo->setCallLocations(
                            linkBuffer.locationOfNearCall(slowCall),
                            linkBuffer.locationOf(targetToCheck),
                            linkBuffer.locationOfNearCall(fastCall));
                    });
            });

        switch (node->op()) {
        case TailCallVarargs:
        case TailCallForwardVarargs:
            m_out.unreachable();
            break;

        default:
            setJSValue(patchpoint);
            break;
        }
    }
    
    void compileLoadVarargs()
    {
        LoadVarargsData* data = m_node->loadVarargsData();
        LValue jsArguments = lowJSValue(m_node->child1());
        
        LValue length = vmCall(
            m_out.int32, m_out.operation(operationSizeOfVarargs), m_callFrame, jsArguments,
            m_out.constInt32(data->offset));
        
        // FIXME: There is a chance that we will call an effectful length property twice. This is safe
        // from the standpoint of the VM's integrity, but it's subtly wrong from a spec compliance
        // standpoint. The best solution would be one where we can exit *into* the op_call_varargs right
        // past the sizing.
        // https://bugs.webkit.org/show_bug.cgi?id=141448
        
        LValue lengthIncludingThis = m_out.add(length, m_out.int32One);
        speculate(
            VarargsOverflow, noValue(), nullptr,
            m_out.above(lengthIncludingThis, m_out.constInt32(data->limit)));
        
        m_out.store32(lengthIncludingThis, payloadFor(data->machineCount));
        
        // FIXME: This computation is rather silly. If operationLaodVarargs just took a pointer instead
        // of a VirtualRegister, we wouldn't have to do this.
        // https://bugs.webkit.org/show_bug.cgi?id=141660
        LValue machineStart = m_out.lShr(
            m_out.sub(addressFor(data->machineStart.offset()).value(), m_callFrame),
            m_out.constIntPtr(3));
        
        vmCall(
            m_out.voidType, m_out.operation(operationLoadVarargs), m_callFrame,
            m_out.castToInt32(machineStart), jsArguments, m_out.constInt32(data->offset),
            length, m_out.constInt32(data->mandatoryMinimum));
    }
    
    void compileForwardVarargs()
    {
        LoadVarargsData* data = m_node->loadVarargsData();
        InlineCallFrame* inlineCallFrame = m_node->child1()->origin.semantic.inlineCallFrame;
        
        LValue length = getArgumentsLength(inlineCallFrame).value;
        LValue lengthIncludingThis = m_out.add(length, m_out.constInt32(1 - data->offset));
        
        speculate(
            VarargsOverflow, noValue(), nullptr,
            m_out.above(lengthIncludingThis, m_out.constInt32(data->limit)));
        
        m_out.store32(lengthIncludingThis, payloadFor(data->machineCount));
        
        LValue sourceStart = getArgumentsStart(inlineCallFrame);
        LValue targetStart = addressFor(data->machineStart).value();

        LBasicBlock undefinedLoop = m_out.newBlock();
        LBasicBlock mainLoopEntry = m_out.newBlock();
        LBasicBlock mainLoop = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue lengthAsPtr = m_out.zeroExtPtr(length);
        LValue loopBoundValue = m_out.constIntPtr(data->mandatoryMinimum);
        ValueFromBlock loopBound = m_out.anchor(loopBoundValue);
        m_out.branch(
            m_out.above(loopBoundValue, lengthAsPtr), unsure(undefinedLoop), unsure(mainLoopEntry));
        
        LBasicBlock lastNext = m_out.appendTo(undefinedLoop, mainLoopEntry);
        LValue previousIndex = m_out.phi(m_out.intPtr, loopBound);
        LValue currentIndex = m_out.sub(previousIndex, m_out.intPtrOne);
        m_out.store64(
            m_out.constInt64(JSValue::encode(jsUndefined())),
            m_out.baseIndex(m_heaps.variables, targetStart, currentIndex));
        ValueFromBlock nextIndex = m_out.anchor(currentIndex);
        m_out.addIncomingToPhi(previousIndex, nextIndex);
        m_out.branch(
            m_out.above(currentIndex, lengthAsPtr), unsure(undefinedLoop), unsure(mainLoopEntry));
        
        m_out.appendTo(mainLoopEntry, mainLoop);
        loopBound = m_out.anchor(lengthAsPtr);
        m_out.branch(m_out.notNull(lengthAsPtr), unsure(mainLoop), unsure(continuation));
        
        m_out.appendTo(mainLoop, continuation);
        previousIndex = m_out.phi(m_out.intPtr, loopBound);
        currentIndex = m_out.sub(previousIndex, m_out.intPtrOne);
        LValue value = m_out.load64(
            m_out.baseIndex(
                m_heaps.variables, sourceStart,
                m_out.add(currentIndex, m_out.constIntPtr(data->offset))));
        m_out.store64(value, m_out.baseIndex(m_heaps.variables, targetStart, currentIndex));
        nextIndex = m_out.anchor(currentIndex);
        m_out.addIncomingToPhi(previousIndex, nextIndex);
        m_out.branch(m_out.isNull(currentIndex), unsure(continuation), unsure(mainLoop));
        
        m_out.appendTo(continuation, lastNext);
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
            LBasicBlock switchOnInts = m_out.newBlock();
            
            LBasicBlock lastNext = m_out.appendTo(m_out.m_block, switchOnInts);
            
            switch (m_node->child1().useKind()) {
            case Int32Use: {
                intValues.append(m_out.anchor(lowInt32(m_node->child1())));
                m_out.jump(switchOnInts);
                break;
            }
                
            case UntypedUse: {
                LBasicBlock isInt = m_out.newBlock();
                LBasicBlock isNotInt = m_out.newBlock();
                LBasicBlock isDouble = m_out.newBlock();
                
                LValue boxedValue = lowJSValue(m_node->child1());
                m_out.branch(isNotInt32(boxedValue), unsure(isNotInt), unsure(isInt));
                
                LBasicBlock innerLastNext = m_out.appendTo(isInt, isNotInt);
                
                intValues.append(m_out.anchor(unboxInt32(boxedValue)));
                m_out.jump(switchOnInts);
                
                m_out.appendTo(isNotInt, isDouble);
                m_out.branch(
                    isCellOrMisc(boxedValue, provenType(m_node->child1())),
                    usually(lowBlock(data->fallThrough.block)), rarely(isDouble));
                
                m_out.appendTo(isDouble, innerLastNext);
                LValue doubleValue = unboxDouble(boxedValue);
                LValue intInDouble = m_out.doubleToInt(doubleValue);
                intValues.append(m_out.anchor(intInDouble));
                m_out.branch(
                    m_out.doubleEqual(m_out.intToDouble(intInDouble), doubleValue),
                    unsure(switchOnInts), unsure(lowBlock(data->fallThrough.block)));
                break;
            }
                
            default:
                DFG_CRASH(m_graph, m_node, "Bad use kind");
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
                
                LBasicBlock isCellCase = m_out.newBlock();
                LBasicBlock isStringCase = m_out.newBlock();
                
                m_out.branch(
                    isNotCell(unboxedValue, provenType(m_node->child1())),
                    unsure(lowBlock(data->fallThrough.block)), unsure(isCellCase));
                
                LBasicBlock lastNext = m_out.appendTo(isCellCase, isStringCase);
                LValue cellValue = unboxedValue;
                m_out.branch(
                    isNotString(cellValue, provenType(m_node->child1())),
                    unsure(lowBlock(data->fallThrough.block)), unsure(isStringCase));
                
                m_out.appendTo(isStringCase, lastNext);
                stringValue = cellValue;
                break;
            }
                
            default:
                DFG_CRASH(m_graph, m_node, "Bad use kind");
                break;
            }
            
            LBasicBlock lengthIs1 = m_out.newBlock();
            LBasicBlock needResolution = m_out.newBlock();
            LBasicBlock resolved = m_out.newBlock();
            LBasicBlock is8Bit = m_out.newBlock();
            LBasicBlock is16Bit = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
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
                vmCall(m_out.intPtr, m_out.operation(operationResolveRope), m_callFrame, stringValue)));
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
            characters.append(m_out.anchor(m_out.load8ZeroExt32(characterData, m_heaps.characters8[0])));
            m_out.jump(continuation);
            
            m_out.appendTo(is16Bit, continuation);
            characters.append(m_out.anchor(m_out.load16ZeroExt32(characterData, m_heaps.characters16[0])));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            buildSwitch(data, m_out.int32, m_out.phi(m_out.int32, characters));
            return;
        }
        
        case SwitchString: {
            switch (m_node->child1().useKind()) {
            case StringIdentUse: {
                LValue stringImpl = lowStringIdent(m_node->child1());
                
                Vector<SwitchCase> cases;
                for (unsigned i = 0; i < data->cases.size(); ++i) {
                    LValue value = m_out.constIntPtr(data->cases[i].value.stringImpl());
                    LBasicBlock block = lowBlock(data->cases[i].target.block);
                    Weight weight = Weight(data->cases[i].target.count);
                    cases.append(SwitchCase(value, block, weight));
                }
                
                m_out.switchInstruction(
                    stringImpl, cases, lowBlock(data->fallThrough.block),
                    Weight(data->fallThrough.count));
                return;
            }
                
            case StringUse: {
                switchString(data, lowString(m_node->child1()));
                return;
            }
                
            case UntypedUse: {
                LValue value = lowJSValue(m_node->child1());
                
                LBasicBlock isCellBlock = m_out.newBlock();
                LBasicBlock isStringBlock = m_out.newBlock();
                
                m_out.branch(
                    isCell(value, provenType(m_node->child1())),
                    unsure(isCellBlock), unsure(lowBlock(data->fallThrough.block)));
                
                LBasicBlock lastNext = m_out.appendTo(isCellBlock, isStringBlock);
                
                m_out.branch(
                    isString(value, provenType(m_node->child1())),
                    unsure(isStringBlock), unsure(lowBlock(data->fallThrough.block)));
                
                m_out.appendTo(isStringBlock, lastNext);
                
                switchString(data, value);
                return;
            }
                
            default:
                DFG_CRASH(m_graph, m_node, "Bad use kind");
                return;
            }
            return;
        }
            
        case SwitchCell: {
            LValue cell;
            switch (m_node->child1().useKind()) {
            case CellUse: {
                cell = lowCell(m_node->child1());
                break;
            }
                
            case UntypedUse: {
                LValue value = lowJSValue(m_node->child1());
                LBasicBlock cellCase = m_out.newBlock();
                m_out.branch(
                    isCell(value, provenType(m_node->child1())),
                    unsure(cellCase), unsure(lowBlock(data->fallThrough.block)));
                m_out.appendTo(cellCase);
                cell = value;
                break;
            }
                
            default:
                DFG_CRASH(m_graph, m_node, "Bad use kind");
                return;
            }
            
            buildSwitch(m_node->switchData(), m_out.intPtr, cell);
            return;
        } }
        
        DFG_CRASH(m_graph, m_node, "Bad switch kind");
    }
    
    void compileReturn()
    {
        m_out.ret(lowJSValue(m_node->child1()));
    }
    
    void compileForceOSRExit()
    {
        terminate(InadequateCoverage);
    }
    
    void compileThrow()
    {
        terminate(Uncountable);
    }
    
    void compileInvalidationPoint()
    {
        if (verboseCompilationEnabled())
            dataLog("    Invalidation point with availability: ", availabilityMap(), "\n");

        DFG_ASSERT(m_graph, m_node, m_origin.exitOK);
        
        PatchpointValue* patchpoint = m_out.patchpoint(Void);
        OSRExitDescriptor* descriptor = appendOSRExitDescriptor(noValue(), nullptr);
        NodeOrigin origin = m_origin;
        patchpoint->appendColdAnys(buildExitArguments(descriptor, origin.forExit, noValue()));
        
        State* state = &m_ftlState;

        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                // The MacroAssembler knows more about this than B3 does. The watchpointLabel() method
                // will ensure that this is followed by a nop shadow but only when this is actually
                // necessary.
                CCallHelpers::Label label = jit.watchpointLabel();

                RefPtr<OSRExitHandle> handle = descriptor->emitOSRExitLater(
                    *state, UncountableInvalidation, origin, params);

                RefPtr<JITCode> jitCode = state->jitCode.get();

                jit.addLinkTask(
                    [=] (LinkBuffer& linkBuffer) {
                        JumpReplacement jumpReplacement(
                            linkBuffer.locationOf(label),
                            linkBuffer.locationOf(handle->label));
                        jitCode->common.jumpReplacements.append(jumpReplacement);
                    });
            });

        // Set some obvious things.
        patchpoint->effects.terminal = false;
        patchpoint->effects.writesLocalState = false;
        patchpoint->effects.readsLocalState = false;
        
        // This is how we tell B3 about the possibility of jump replacement.
        patchpoint->effects.exitsSideways = true;
        
        // It's not possible for some prior branch to determine the safety of this operation. It's always
        // fine to execute this on some path that wouldn't have originally executed it before
        // optimization.
        patchpoint->effects.controlDependent = false;

        // If this falls through then it won't write anything.
        patchpoint->effects.writes = HeapRange();

        // When this abruptly terminates, it could read any heap location.
        patchpoint->effects.reads = HeapRange::top();
    }
    
    void compileIsUndefined()
    {
        setBoolean(equalNullOrUndefined(m_node->child1(), AllCellsAreFalse, EqualUndefined));
    }
    
    void compileIsBoolean()
    {
        setBoolean(isBoolean(lowJSValue(m_node->child1()), provenType(m_node->child1())));
    }
    
    void compileIsNumber()
    {
        setBoolean(isNumber(lowJSValue(m_node->child1()), provenType(m_node->child1())));
    }
    
    void compileIsString()
    {
        LValue value = lowJSValue(m_node->child1());
        
        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(
            isCell(value, provenType(m_node->child1())), unsure(isCellCase), unsure(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(isCellCase, continuation);
        ValueFromBlock cellResult = m_out.anchor(isString(value, provenType(m_node->child1())));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(m_out.boolean, notCellResult, cellResult));
    }

    void compileIsObject()
    {
        LValue value = lowJSValue(m_node->child1());

        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(
            isCell(value, provenType(m_node->child1())), unsure(isCellCase), unsure(continuation));

        LBasicBlock lastNext = m_out.appendTo(isCellCase, continuation);
        ValueFromBlock cellResult = m_out.anchor(isObject(value, provenType(m_node->child1())));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(m_out.boolean, notCellResult, cellResult));
    }

    void compileIsObjectOrNull()
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        
        Edge child = m_node->child1();
        LValue value = lowJSValue(child);
        
        LBasicBlock cellCase = m_out.newBlock();
        LBasicBlock notFunctionCase = m_out.newBlock();
        LBasicBlock objectCase = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock notCellCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        m_out.branch(isCell(value, provenType(child)), unsure(cellCase), unsure(notCellCase));
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, notFunctionCase);
        ValueFromBlock isFunctionResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(
            isFunction(value, provenType(child)),
            unsure(continuation), unsure(notFunctionCase));
        
        m_out.appendTo(notFunctionCase, objectCase);
        ValueFromBlock notObjectResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(
            isObject(value, provenType(child)),
            unsure(objectCase), unsure(continuation));
        
        m_out.appendTo(objectCase, slowPath);
        ValueFromBlock objectResult = m_out.anchor(m_out.booleanTrue);
        m_out.branch(
            isExoticForTypeof(value, provenType(child)),
            rarely(slowPath), usually(continuation));
        
        m_out.appendTo(slowPath, notCellCase);
        LValue slowResultValue = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationObjectIsObject, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(globalObject), locations[1].directGPR());
            }, value);
        ValueFromBlock slowResult = m_out.anchor(m_out.notZero64(slowResultValue));
        m_out.jump(continuation);
        
        m_out.appendTo(notCellCase, continuation);
        LValue notCellResultValue = m_out.equal(value, m_out.constInt64(JSValue::encode(jsNull())));
        ValueFromBlock notCellResult = m_out.anchor(notCellResultValue);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        LValue result = m_out.phi(
            m_out.boolean,
            isFunctionResult, notObjectResult, objectResult, slowResult, notCellResult);
        setBoolean(result);
    }
    
    void compileIsFunction()
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        
        Edge child = m_node->child1();
        LValue value = lowJSValue(child);
        
        LBasicBlock cellCase = m_out.newBlock();
        LBasicBlock notFunctionCase = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(
            isCell(value, provenType(child)), unsure(cellCase), unsure(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, notFunctionCase);
        ValueFromBlock functionResult = m_out.anchor(m_out.booleanTrue);
        m_out.branch(
            isFunction(value, provenType(child)),
            unsure(continuation), unsure(notFunctionCase));
        
        m_out.appendTo(notFunctionCase, slowPath);
        ValueFromBlock objectResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(
            isExoticForTypeof(value, provenType(child)),
            rarely(slowPath), usually(continuation));
        
        m_out.appendTo(slowPath, continuation);
        LValue slowResultValue = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationObjectIsFunction, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(globalObject), locations[1].directGPR());
            }, value);
        ValueFromBlock slowResult = m_out.anchor(m_out.notNull(slowResultValue));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        LValue result = m_out.phi(
            m_out.boolean, notCellResult, functionResult, objectResult, slowResult);
        setBoolean(result);
    }
    
    void compileTypeOf()
    {
        Edge child = m_node->child1();
        LValue value = lowJSValue(child);
        
        LBasicBlock continuation = m_out.newBlock();
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(continuation);
        
        Vector<ValueFromBlock> results;
        
        buildTypeOf(
            child, value,
            [&] (TypeofType type) {
                results.append(m_out.anchor(weakPointer(vm().smallStrings.typeString(type))));
                m_out.jump(continuation);
            });
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, results));
    }
    
    void compileIn()
    {
        Node* node = m_node;
        Edge base = node->child2();
        LValue cell = lowCell(base);
        if (JSString* string = node->child1()->dynamicCastConstant<JSString*>()) {
            if (string->tryGetValueImpl() && string->tryGetValueImpl()->isAtomic()) {
                UniquedStringImpl* str = bitwise_cast<UniquedStringImpl*>(string->tryGetValueImpl());
                B3::PatchpointValue* patchpoint = m_out.patchpoint(Int64);
                patchpoint->appendSomeRegister(cell);
                patchpoint->clobber(RegisterSet::macroScratchRegisters());

                State* state = &m_ftlState;
                patchpoint->setGenerator(
                    [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);

                        GPRReg baseGPR = params[1].gpr();
                        GPRReg resultGPR = params[0].gpr();

                        StructureStubInfo* stubInfo =
                            jit.codeBlock()->addStubInfo(AccessType::In);
                        stubInfo->callSiteIndex =
                            state->jitCode->common.addCodeOrigin(node->origin.semantic);
                        stubInfo->codeOrigin = node->origin.semantic;
                        stubInfo->patch.baseGPR = static_cast<int8_t>(baseGPR);
                        stubInfo->patch.valueGPR = static_cast<int8_t>(resultGPR);
                        stubInfo->patch.usedRegisters = params.unavailableRegisters();

                        CCallHelpers::PatchableJump jump = jit.patchableJump();
                        CCallHelpers::Label done = jit.label();

                        params.addLatePath(
                            [=] (CCallHelpers& jit) {
                                AllowMacroScratchRegisterUsage allowScratch(jit);

                                jump.m_jump.link(&jit);
                                CCallHelpers::Label slowPathBegin = jit.label();
                                CCallHelpers::Call slowPathCall = callOperation(
                                    *state, params.unavailableRegisters(), jit,
                                    node->origin.semantic, nullptr, operationInOptimize,
                                    resultGPR, CCallHelpers::TrustedImmPtr(stubInfo), baseGPR,
                                    CCallHelpers::TrustedImmPtr(str)).call();
                                jit.jump().linkTo(done, &jit);

                                jit.addLinkTask(
                                    [=] (LinkBuffer& linkBuffer) {
                                        CodeLocationCall callReturnLocation =
                                            linkBuffer.locationOf(slowPathCall);
                                        stubInfo->patch.deltaCallToDone =
                                            CCallHelpers::differenceBetweenCodePtr(
                                                callReturnLocation,
                                                linkBuffer.locationOf(done));
                                        stubInfo->patch.deltaCallToJump =
                                            CCallHelpers::differenceBetweenCodePtr(
                                                callReturnLocation,
                                                linkBuffer.locationOf(jump));
                                        stubInfo->callReturnLocation = callReturnLocation;
                                        stubInfo->patch.deltaCallToSlowCase =
                                            CCallHelpers::differenceBetweenCodePtr(
                                                callReturnLocation,
                                                linkBuffer.locationOf(slowPathBegin));
                                    });
                            });
                    });

                setJSValue(patchpoint);
                return;
            }
        } 

        setJSValue(vmCall(m_out.int64, m_out.operation(operationGenericIn), m_callFrame, cell, lowJSValue(m_node->child1())));
    }

    void compileOverridesHasInstance()
    {
        JSFunction* defaultHasInstanceFunction = jsCast<JSFunction*>(m_node->cellOperand()->value());

        LValue constructor = lowCell(m_node->child1());
        LValue hasInstance = lowJSValue(m_node->child2());

        LBasicBlock defaultHasInstance = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        // Unlike in the DFG, we don't worry about cleaning this code up for the case where we have proven the hasInstanceValue is a constant as B3 should fix it for us.

        ASSERT(!m_node->child2().node()->isCellConstant() || defaultHasInstanceFunction == m_node->child2().node()->asCell());

        ValueFromBlock notDefaultHasInstanceResult = m_out.anchor(m_out.booleanTrue);
        m_out.branch(m_out.notEqual(hasInstance, m_out.constIntPtr(defaultHasInstanceFunction)), unsure(continuation), unsure(defaultHasInstance));

        LBasicBlock lastNext = m_out.appendTo(defaultHasInstance, continuation);
        ValueFromBlock implementsDefaultHasInstanceResult = m_out.anchor(m_out.testIsZero32(
            m_out.load8ZeroExt32(constructor, m_heaps.JSCell_typeInfoFlags),
            m_out.constInt32(ImplementsDefaultHasInstance)));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(m_out.boolean, implementsDefaultHasInstanceResult, notDefaultHasInstanceResult));
    }

    void compileCheckTypeInfoFlags()
    {
        speculate(
            BadTypeInfoFlags, noValue(), 0,
            m_out.testIsZero32(
                m_out.load8ZeroExt32(lowCell(m_node->child1()), m_heaps.JSCell_typeInfoFlags),
                m_out.constInt32(m_node->typeInfoOperand())));
    }
    
    void compileInstanceOf()
    {
        LValue cell;
        
        if (m_node->child1().useKind() == UntypedUse)
            cell = lowJSValue(m_node->child1());
        else
            cell = lowCell(m_node->child1());
        
        LValue prototype = lowCell(m_node->child2());
        
        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock loop = m_out.newBlock();
        LBasicBlock notYetInstance = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        LBasicBlock loadPrototypeDirect = m_out.newBlock();
        LBasicBlock defaultHasInstanceSlow = m_out.newBlock();
        
        LValue condition;
        if (m_node->child1().useKind() == UntypedUse)
            condition = isCell(cell, provenType(m_node->child1()));
        else
            condition = m_out.booleanTrue;
        
        ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(condition, unsure(isCellCase), unsure(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(isCellCase, loop);
        
        speculate(BadType, noValue(), 0, isNotObject(prototype, provenType(m_node->child2())));
        
        ValueFromBlock originalValue = m_out.anchor(cell);
        m_out.jump(loop);
        
        m_out.appendTo(loop, loadPrototypeDirect);
        LValue value = m_out.phi(m_out.int64, originalValue);
        LValue type = m_out.load8ZeroExt32(value, m_heaps.JSCell_typeInfoType);
        m_out.branch(
            m_out.notEqual(type, m_out.constInt32(ProxyObjectType)),
            usually(loadPrototypeDirect), rarely(defaultHasInstanceSlow));

        m_out.appendTo(loadPrototypeDirect, notYetInstance);
        LValue structure = loadStructure(value);
        LValue currentPrototype = m_out.load64(structure, m_heaps.Structure_prototype);
        ValueFromBlock isInstanceResult = m_out.anchor(m_out.booleanTrue);
        m_out.branch(
            m_out.equal(currentPrototype, prototype),
            unsure(continuation), unsure(notYetInstance));
        
        m_out.appendTo(notYetInstance, defaultHasInstanceSlow);
        ValueFromBlock notInstanceResult = m_out.anchor(m_out.booleanFalse);
        m_out.addIncomingToPhi(value, m_out.anchor(currentPrototype));
        m_out.branch(isCell(currentPrototype), unsure(loop), unsure(continuation));

        m_out.appendTo(defaultHasInstanceSlow, continuation);
        // We can use the value that we're looping with because we
        // can just continue off from wherever we bailed from the
        // loop.
        ValueFromBlock defaultHasInstanceResult = m_out.anchor(
            vmCall(m_out.boolean, m_out.operation(operationDefaultHasInstance), m_callFrame, value, prototype));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(
            m_out.phi(m_out.boolean, notCellResult, isInstanceResult, notInstanceResult, defaultHasInstanceResult));
    }

    void compileInstanceOfCustom()
    {
        LValue value = lowJSValue(m_node->child1());
        LValue constructor = lowCell(m_node->child2());
        LValue hasInstance = lowJSValue(m_node->child3());

        setBoolean(m_out.logicalNot(m_out.equal(m_out.constInt32(0), vmCall(m_out.int32, m_out.operation(operationInstanceOfCustom), m_callFrame, value, constructor, hasInstance))));
    }
    
    void compileCountExecution()
    {
        TypedPointer counter = m_out.absolute(m_node->executionCounter()->address());
        m_out.store64(m_out.add(m_out.load64(counter), m_out.constInt64(1)), counter);
    }
    
    void compileStoreBarrier()
    {
        emitStoreBarrier(lowCell(m_node->child1()));
    }

    void compileHasIndexedProperty()
    {
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Contiguous: {
            LValue base = lowCell(m_node->child1());
            LValue index = lowInt32(m_node->child2());
            LValue storage = lowStorage(m_node->child3());

            IndexedAbstractHeap& heap = m_node->arrayMode().type() == Array::Int32 ?
                m_heaps.indexedInt32Properties : m_heaps.indexedContiguousProperties;

            LBasicBlock checkHole = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            if (!m_node->arrayMode().isInBounds()) {
                m_out.branch(
                    m_out.aboveOrEqual(
                        index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength)),
                    rarely(slowCase), usually(checkHole));
            } else
                m_out.jump(checkHole);

            LBasicBlock lastNext = m_out.appendTo(checkHole, slowCase);
            LValue checkHoleResultValue =
                m_out.notZero64(m_out.load64(baseIndex(heap, storage, index, m_node->child2())));
            ValueFromBlock checkHoleResult = m_out.anchor(checkHoleResultValue);
            m_out.branch(checkHoleResultValue, usually(continuation), rarely(slowCase));

            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(m_out.equal(
                m_out.constInt64(JSValue::encode(jsBoolean(true))), 
                vmCall(m_out.int64, m_out.operation(operationHasIndexedProperty), m_callFrame, base, index)));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(m_out.boolean, checkHoleResult, slowResult));
            return;
        }
        case Array::Double: {
            LValue base = lowCell(m_node->child1());
            LValue index = lowInt32(m_node->child2());
            LValue storage = lowStorage(m_node->child3());
            
            IndexedAbstractHeap& heap = m_heaps.indexedDoubleProperties;
            
            LBasicBlock checkHole = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            if (!m_node->arrayMode().isInBounds()) {
                m_out.branch(
                    m_out.aboveOrEqual(
                        index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength)),
                    rarely(slowCase), usually(checkHole));
            } else
                m_out.jump(checkHole);

            LBasicBlock lastNext = m_out.appendTo(checkHole, slowCase);
            LValue doubleValue = m_out.loadDouble(baseIndex(heap, storage, index, m_node->child2()));
            LValue checkHoleResultValue = m_out.doubleEqual(doubleValue, doubleValue);
            ValueFromBlock checkHoleResult = m_out.anchor(checkHoleResultValue);
            m_out.branch(checkHoleResultValue, usually(continuation), rarely(slowCase));
            
            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(m_out.equal(
                m_out.constInt64(JSValue::encode(jsBoolean(true))), 
                vmCall(m_out.int64, m_out.operation(operationHasIndexedProperty), m_callFrame, base, index)));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(m_out.boolean, checkHoleResult, slowResult));
            return;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return;
        }
    }

    void compileHasGenericProperty()
    {
        LValue base = lowJSValue(m_node->child1());
        LValue property = lowCell(m_node->child2());
        setJSValue(vmCall(m_out.int64, m_out.operation(operationHasGenericProperty), m_callFrame, base, property));
    }

    void compileHasStructureProperty()
    {
        LValue base = lowJSValue(m_node->child1());
        LValue property = lowString(m_node->child2());
        LValue enumerator = lowCell(m_node->child3());

        LBasicBlock correctStructure = m_out.newBlock();
        LBasicBlock wrongStructure = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(m_out.notEqual(
            m_out.load32(base, m_heaps.JSCell_structureID),
            m_out.load32(enumerator, m_heaps.JSPropertyNameEnumerator_cachedStructureID)),
            rarely(wrongStructure), usually(correctStructure));

        LBasicBlock lastNext = m_out.appendTo(correctStructure, wrongStructure);
        ValueFromBlock correctStructureResult = m_out.anchor(m_out.booleanTrue);
        m_out.jump(continuation);

        m_out.appendTo(wrongStructure, continuation);
        ValueFromBlock wrongStructureResult = m_out.anchor(
            m_out.equal(
                m_out.constInt64(JSValue::encode(jsBoolean(true))), 
                vmCall(m_out.int64, m_out.operation(operationHasGenericProperty), m_callFrame, base, property)));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(m_out.boolean, correctStructureResult, wrongStructureResult));
    }

    void compileGetDirectPname()
    {
        LValue base = lowCell(m_graph.varArgChild(m_node, 0));
        LValue property = lowCell(m_graph.varArgChild(m_node, 1));
        LValue index = lowInt32(m_graph.varArgChild(m_node, 2));
        LValue enumerator = lowCell(m_graph.varArgChild(m_node, 3));

        LBasicBlock checkOffset = m_out.newBlock();
        LBasicBlock inlineLoad = m_out.newBlock();
        LBasicBlock outOfLineLoad = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(m_out.notEqual(
            m_out.load32(base, m_heaps.JSCell_structureID),
            m_out.load32(enumerator, m_heaps.JSPropertyNameEnumerator_cachedStructureID)),
            rarely(slowCase), usually(checkOffset));

        LBasicBlock lastNext = m_out.appendTo(checkOffset, inlineLoad);
        m_out.branch(m_out.aboveOrEqual(index, m_out.load32(enumerator, m_heaps.JSPropertyNameEnumerator_cachedInlineCapacity)),
            unsure(outOfLineLoad), unsure(inlineLoad));

        m_out.appendTo(inlineLoad, outOfLineLoad);
        ValueFromBlock inlineResult = m_out.anchor(
            m_out.load64(m_out.baseIndex(m_heaps.properties.atAnyNumber(), 
                base, m_out.zeroExt(index, m_out.int64), ScaleEight, JSObject::offsetOfInlineStorage())));
        m_out.jump(continuation);

        m_out.appendTo(outOfLineLoad, slowCase);
        LValue storage = m_out.loadPtr(base, m_heaps.JSObject_butterfly);
        LValue realIndex = m_out.signExt32To64(
            m_out.neg(m_out.sub(index, m_out.load32(enumerator, m_heaps.JSPropertyNameEnumerator_cachedInlineCapacity))));
        int32_t offsetOfFirstProperty = static_cast<int32_t>(offsetInButterfly(firstOutOfLineOffset)) * sizeof(EncodedJSValue);
        ValueFromBlock outOfLineResult = m_out.anchor(
            m_out.load64(m_out.baseIndex(m_heaps.properties.atAnyNumber(), storage, realIndex, ScaleEight, offsetOfFirstProperty)));
        m_out.jump(continuation);

        m_out.appendTo(slowCase, continuation);
        ValueFromBlock slowCaseResult = m_out.anchor(
            vmCall(m_out.int64, m_out.operation(operationGetByVal), m_callFrame, base, property));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, inlineResult, outOfLineResult, slowCaseResult));
    }

    void compileGetEnumerableLength()
    {
        LValue enumerator = lowCell(m_node->child1());
        setInt32(m_out.load32(enumerator, m_heaps.JSPropertyNameEnumerator_indexLength));
    }

    void compileGetPropertyEnumerator()
    {
        LValue base = lowCell(m_node->child1());
        setJSValue(vmCall(m_out.int64, m_out.operation(operationGetPropertyEnumerator), m_callFrame, base));
    }

    void compileGetEnumeratorStructurePname()
    {
        LValue enumerator = lowCell(m_node->child1());
        LValue index = lowInt32(m_node->child2());

        LBasicBlock inBounds = m_out.newBlock();
        LBasicBlock outOfBounds = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(m_out.below(index, m_out.load32(enumerator, m_heaps.JSPropertyNameEnumerator_endStructurePropertyIndex)),
            usually(inBounds), rarely(outOfBounds));

        LBasicBlock lastNext = m_out.appendTo(inBounds, outOfBounds);
        LValue storage = m_out.loadPtr(enumerator, m_heaps.JSPropertyNameEnumerator_cachedPropertyNamesVector);
        ValueFromBlock inBoundsResult = m_out.anchor(
            m_out.loadPtr(m_out.baseIndex(m_heaps.JSPropertyNameEnumerator_cachedPropertyNamesVectorContents, storage, m_out.zeroExtPtr(index))));
        m_out.jump(continuation);

        m_out.appendTo(outOfBounds, continuation);
        ValueFromBlock outOfBoundsResult = m_out.anchor(m_out.constInt64(ValueNull));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, inBoundsResult, outOfBoundsResult));
    }

    void compileGetEnumeratorGenericPname()
    {
        LValue enumerator = lowCell(m_node->child1());
        LValue index = lowInt32(m_node->child2());

        LBasicBlock inBounds = m_out.newBlock();
        LBasicBlock outOfBounds = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(m_out.below(index, m_out.load32(enumerator, m_heaps.JSPropertyNameEnumerator_endGenericPropertyIndex)),
            usually(inBounds), rarely(outOfBounds));

        LBasicBlock lastNext = m_out.appendTo(inBounds, outOfBounds);
        LValue storage = m_out.loadPtr(enumerator, m_heaps.JSPropertyNameEnumerator_cachedPropertyNamesVector);
        ValueFromBlock inBoundsResult = m_out.anchor(
            m_out.loadPtr(m_out.baseIndex(m_heaps.JSPropertyNameEnumerator_cachedPropertyNamesVectorContents, storage, m_out.zeroExtPtr(index))));
        m_out.jump(continuation);

        m_out.appendTo(outOfBounds, continuation);
        ValueFromBlock outOfBoundsResult = m_out.anchor(m_out.constInt64(ValueNull));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(m_out.int64, inBoundsResult, outOfBoundsResult));
    }
    
    void compileToIndexString()
    {
        LValue index = lowInt32(m_node->child1());
        setJSValue(vmCall(m_out.int64, m_out.operation(operationToIndexString), m_callFrame, index));
    }
    
    void compileCheckStructureImmediate()
    {
        LValue structure = lowCell(m_node->child1());
        checkStructure(
            structure, noValue(), BadCache, m_node->structureSet(),
            [this] (Structure* structure) {
                return weakStructure(structure);
            });
    }
    
    void compileMaterializeNewObject()
    {
        ObjectMaterializationData& data = m_node->objectMaterializationData();
        
        // Lower the values first, to avoid creating values inside a control flow diamond.
        
        Vector<LValue, 8> values;
        for (unsigned i = 0; i < data.m_properties.size(); ++i) {
            Edge edge = m_graph.varArgChild(m_node, 1 + i);
            switch (data.m_properties[i].kind()) {
            case PublicLengthPLoc:
            case VectorLengthPLoc:
                values.append(lowInt32(edge));
                break;
            default:
                values.append(lowJSValue(edge));
                break;
            }
        }
        
        const StructureSet& set = m_node->structureSet();

        Vector<LBasicBlock, 1> blocks(set.size());
        for (unsigned i = set.size(); i--;)
            blocks[i] = m_out.newBlock();
        LBasicBlock dummyDefault = m_out.newBlock();
        LBasicBlock outerContinuation = m_out.newBlock();
        
        Vector<SwitchCase, 1> cases(set.size());
        for (unsigned i = set.size(); i--;)
            cases[i] = SwitchCase(weakStructure(set[i]), blocks[i], Weight(1));
        m_out.switchInstruction(
            lowCell(m_graph.varArgChild(m_node, 0)), cases, dummyDefault, Weight(0));
        
        LBasicBlock outerLastNext = m_out.m_nextBlock;
        
        Vector<ValueFromBlock, 1> results;
        
        for (unsigned i = set.size(); i--;) {
            m_out.appendTo(blocks[i], i + 1 < set.size() ? blocks[i + 1] : dummyDefault);
            
            Structure* structure = set[i];
            
            LValue object;
            LValue butterfly;
            
            if (structure->outOfLineCapacity() || hasIndexedProperties(structure->indexingType())) {
                size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
                MarkedAllocator* allocator = &vm().heap.allocatorForObjectWithoutDestructor(allocationSize);

                bool hasIndexingHeader = hasIndexedProperties(structure->indexingType());
                unsigned indexingHeaderSize = 0;
                LValue indexingPayloadSizeInBytes = m_out.intPtrZero;
                LValue vectorLength = m_out.int32Zero;
                LValue publicLength = m_out.int32Zero;
                if (hasIndexingHeader) {
                    indexingHeaderSize = sizeof(IndexingHeader);
                    for (unsigned i = data.m_properties.size(); i--;) {
                        PromotedLocationDescriptor descriptor = data.m_properties[i];
                        switch (descriptor.kind()) {
                        case PublicLengthPLoc:
                            publicLength = values[i];
                            break;
                        case VectorLengthPLoc:
                            vectorLength = values[i];
                            break;
                        default:
                            break;
                        }
                    }
                    indexingPayloadSizeInBytes =
                        m_out.mul(m_out.zeroExtPtr(vectorLength), m_out.intPtrEight);
                }

                LValue butterflySize = m_out.add(
                    m_out.constIntPtr(
                        structure->outOfLineCapacity() * sizeof(JSValue) + indexingHeaderSize),
                    indexingPayloadSizeInBytes);
                
                LBasicBlock slowPath = m_out.newBlock();
                LBasicBlock continuation = m_out.newBlock();
                
                LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
                
                LValue endOfStorage = allocateBasicStorageAndGetEnd(butterflySize, slowPath);

                LValue fastButterflyValue = m_out.add(
                    m_out.sub(endOfStorage, indexingPayloadSizeInBytes),
                    m_out.constIntPtr(sizeof(IndexingHeader) - indexingHeaderSize));

                m_out.store32(vectorLength, fastButterflyValue, m_heaps.Butterfly_vectorLength);
                
                LValue fastObjectValue = allocateObject(
                    m_out.constIntPtr(allocator), structure, fastButterflyValue, slowPath);

                ValueFromBlock fastObject = m_out.anchor(fastObjectValue);
                ValueFromBlock fastButterfly = m_out.anchor(fastButterflyValue);
                m_out.jump(continuation);
                
                m_out.appendTo(slowPath, continuation);

                LValue slowObjectValue;
                if (hasIndexingHeader) {
                    slowObjectValue = lazySlowPath(
                        [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                            return createLazyCallGenerator(
                                operationNewObjectWithButterflyWithIndexingHeaderAndVectorLength,
                                locations[0].directGPR(), CCallHelpers::TrustedImmPtr(structure),
                                locations[1].directGPR());
                        },
                        vectorLength);
                } else {
                    slowObjectValue = lazySlowPath(
                        [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                            return createLazyCallGenerator(
                                operationNewObjectWithButterfly, locations[0].directGPR(),
                                CCallHelpers::TrustedImmPtr(structure));
                        });
                }
                ValueFromBlock slowObject = m_out.anchor(slowObjectValue);
                ValueFromBlock slowButterfly = m_out.anchor(
                    m_out.loadPtr(slowObjectValue, m_heaps.JSObject_butterfly));
                
                m_out.jump(continuation);
                
                m_out.appendTo(continuation, lastNext);
                
                object = m_out.phi(m_out.intPtr, fastObject, slowObject);
                butterfly = m_out.phi(m_out.intPtr, fastButterfly, slowButterfly);

                m_out.store32(publicLength, butterfly, m_heaps.Butterfly_publicLength);

                initializeArrayElements(structure->indexingType(), vectorLength, butterfly);

                HashMap<int32_t, LValue, DefaultHash<int32_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<int32_t>> indexMap;
                Vector<int32_t> indices;
                for (unsigned i = data.m_properties.size(); i--;) {
                    PromotedLocationDescriptor descriptor = data.m_properties[i];
                    if (descriptor.kind() != IndexedPropertyPLoc)
                        continue;
                    int32_t index = static_cast<int32_t>(descriptor.info());
                    
                    auto result = indexMap.add(index, values[i]);
                    DFG_ASSERT(m_graph, m_node, result); // Duplicates are illegal.

                    indices.append(index);
                }

                if (!indices.isEmpty()) {
                    std::sort(indices.begin(), indices.end());
                    
                    Vector<LBasicBlock> blocksWithStores(indices.size());
                    Vector<LBasicBlock> blocksWithChecks(indices.size());
                    
                    for (unsigned i = indices.size(); i--;) {
                        blocksWithStores[i] = m_out.newBlock();
                        blocksWithChecks[i] = m_out.newBlock(); // blocksWithChecks[0] is the continuation.
                    }

                    LBasicBlock indexLastNext = m_out.m_nextBlock;
                    
                    for (unsigned i = indices.size(); i--;) {
                        int32_t index = indices[i];
                        LValue value = indexMap.get(index);
                        
                        m_out.branch(
                            m_out.below(m_out.constInt32(index), publicLength),
                            unsure(blocksWithStores[i]), unsure(blocksWithChecks[i]));

                        m_out.appendTo(blocksWithStores[i], blocksWithChecks[i]);

                        // This has to type-check and convert its inputs, but it cannot do so in a
                        // way that updates AI. That's a bit annoying, but if you think about how
                        // sinking works, it's actually not a bad thing. We are virtually guaranteed
                        // that these type checks will not fail, since the type checks that guarded
                        // the original stores to the array are still somewhere above this point.
                        Output::StoreType storeType;
                        IndexedAbstractHeap* heap;
                        switch (structure->indexingType()) {
                        case ALL_INT32_INDEXING_TYPES:
                            // FIXME: This could use the proven type if we had the Edge for the
                            // value. https://bugs.webkit.org/show_bug.cgi?id=155311
                            speculate(BadType, noValue(), nullptr, isNotInt32(value));
                            storeType = Output::Store64;
                            heap = &m_heaps.indexedInt32Properties;
                            break;

                        case ALL_DOUBLE_INDEXING_TYPES: {
                            // FIXME: If the source is ValueRep, we should avoid emitting any
                            // checks. We could also avoid emitting checks if we had the Edge of
                            // this value. https://bugs.webkit.org/show_bug.cgi?id=155311

                            LBasicBlock intCase = m_out.newBlock();
                            LBasicBlock doubleCase = m_out.newBlock();
                            LBasicBlock continuation = m_out.newBlock();

                            m_out.branch(isInt32(value), unsure(intCase), unsure(doubleCase));

                            LBasicBlock lastNext = m_out.appendTo(intCase, doubleCase);

                            ValueFromBlock intResult =
                                m_out.anchor(m_out.intToDouble(unboxInt32(value)));
                            m_out.jump(continuation);

                            m_out.appendTo(doubleCase, continuation);

                            speculate(BadType, noValue(), nullptr, isNumber(value));
                            ValueFromBlock doubleResult = m_out.anchor(unboxDouble(value));
                            m_out.jump(continuation);

                            m_out.appendTo(continuation, lastNext);
                            value = m_out.phi(Double, intResult, doubleResult);
                            storeType = Output::StoreDouble;
                            heap = &m_heaps.indexedDoubleProperties;
                            break;
                        }

                        case ALL_CONTIGUOUS_INDEXING_TYPES:
                            storeType = Output::Store64;
                            heap = &m_heaps.indexedContiguousProperties;
                            break;

                        default:
                            DFG_CRASH(m_graph, m_node, "Invalid indexing type");
                            break;
                        }
                        
                        m_out.store(value, m_out.address(butterfly, heap->at(index)), storeType);

                        m_out.jump(blocksWithChecks[i]);
                        m_out.appendTo(
                            blocksWithChecks[i], i ? blocksWithStores[i - 1] : indexLastNext);
                    }
                }
            } else {
                // In the easy case where we can do a one-shot allocation, we simply allocate the
                // object to directly have the desired structure.
                object = allocateObject(structure);
                butterfly = nullptr; // Don't have one, don't need one.
            }
            
            for (PropertyMapEntry entry : structure->getPropertiesConcurrently()) {
                for (unsigned i = data.m_properties.size(); i--;) {
                    PromotedLocationDescriptor descriptor = data.m_properties[i];
                    if (descriptor.kind() != NamedPropertyPLoc)
                        continue;
                    if (m_graph.identifiers()[descriptor.info()] != entry.key)
                        continue;
                    
                    LValue base = isInlineOffset(entry.offset) ? object : butterfly;
                    storeProperty(values[i], base, descriptor.info(), entry.offset);
                    break;
                }
            }
            
            results.append(m_out.anchor(object));
            m_out.jump(outerContinuation);
        }
        
        m_out.appendTo(dummyDefault, outerContinuation);
        m_out.unreachable();
        
        m_out.appendTo(outerContinuation, outerLastNext);
        setJSValue(m_out.phi(m_out.intPtr, results));
    }

    void compileMaterializeCreateActivation()
    {
        ObjectMaterializationData& data = m_node->objectMaterializationData();

        Vector<LValue, 8> values;
        for (unsigned i = 0; i < data.m_properties.size(); ++i)
            values.append(lowJSValue(m_graph.varArgChild(m_node, 2 + i)));

        LValue scope = lowCell(m_graph.varArgChild(m_node, 1));
        SymbolTable* table = m_node->castOperand<SymbolTable*>();
        ASSERT(table == m_graph.varArgChild(m_node, 0)->castConstant<SymbolTable*>());
        Structure* structure = m_graph.globalObjectFor(m_node->origin.semantic)->activationStructure();

        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);

        LValue fastObject = allocateObject<JSLexicalEnvironment>(
            JSLexicalEnvironment::allocationSize(table), structure, m_out.intPtrZero, slowPath);

        m_out.storePtr(scope, fastObject, m_heaps.JSScope_next);
        m_out.storePtr(weakPointer(table), fastObject, m_heaps.JSSymbolTableObject_symbolTable);


        ValueFromBlock fastResult = m_out.anchor(fastObject);
        m_out.jump(continuation);

        m_out.appendTo(slowPath, continuation);
        // We ensure allocation sinking explictly sets bottom values for all field members. 
        // Therefore, it doesn't matter what JSValue we pass in as the initialization value
        // because all fields will be overwritten.
        // FIXME: It may be worth creating an operation that calls a constructor on JSLexicalEnvironment that 
        // doesn't initialize every slot because we are guaranteed to do that here.
        LValue callResult = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationCreateActivationDirect, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure), locations[1].directGPR(),
                    CCallHelpers::TrustedImmPtr(table),
                    CCallHelpers::TrustedImm64(JSValue::encode(jsUndefined())));
            }, scope);
        ValueFromBlock slowResult =  m_out.anchor(callResult);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        LValue activation = m_out.phi(m_out.intPtr, fastResult, slowResult);
        RELEASE_ASSERT(data.m_properties.size() == table->scopeSize());
        for (unsigned i = 0; i < data.m_properties.size(); ++i) {
            PromotedLocationDescriptor descriptor = data.m_properties[i];
            ASSERT(descriptor.kind() == ClosureVarPLoc);
            m_out.store64(
                values[i], activation,
                m_heaps.JSEnvironmentRecord_variables[descriptor.info()]);
        }

        if (validationEnabled()) {
            // Validate to make sure every slot in the scope has one value.
            ConcurrentJITLocker locker(table->m_lock);
            for (auto iter = table->begin(locker), end = table->end(locker); iter != end; ++iter) {
                bool found = false;
                for (unsigned i = 0; i < data.m_properties.size(); ++i) {
                    PromotedLocationDescriptor descriptor = data.m_properties[i];
                    ASSERT(descriptor.kind() == ClosureVarPLoc);
                    if (iter->value.scopeOffset().offset() == descriptor.info()) {
                        found = true;
                        break;
                    }
                }
                ASSERT_UNUSED(found, found);
            }
        }

        setJSValue(activation);
    }

    void compileCheckWatchdogTimer()
    {
        LBasicBlock timerDidFire = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue state = m_out.load8ZeroExt32(m_out.absolute(vm().watchdog()->timerDidFireAddress()));
        m_out.branch(m_out.isZero32(state),
            usually(continuation), rarely(timerDidFire));

        LBasicBlock lastNext = m_out.appendTo(timerDidFire, continuation);

        lazySlowPath(
            [=] (const Vector<Location>&) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(operationHandleWatchdogTimer, InvalidGPRReg);
            });
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }

    void compileRegExpExec()
    {
        LValue globalObject = lowCell(m_node->child1());
        
        if (m_node->child2().useKind() == RegExpObjectUse) {
            LValue base = lowRegExpObject(m_node->child2());
            
            if (m_node->child3().useKind() == StringUse) {
                LValue argument = lowString(m_node->child3());
                LValue result = vmCall(
                    Int64, m_out.operation(operationRegExpExecString), m_callFrame, globalObject,
                    base, argument);
                setJSValue(result);
                return;
            }
            
            LValue argument = lowJSValue(m_node->child3());
            LValue result = vmCall(
                Int64, m_out.operation(operationRegExpExec), m_callFrame, globalObject, base,
                argument);
            setJSValue(result);
            return;
        }
        
        LValue base = lowJSValue(m_node->child2());
        LValue argument = lowJSValue(m_node->child3());
        LValue result = vmCall(
            Int64, m_out.operation(operationRegExpExecGeneric), m_callFrame, globalObject, base,
            argument);
        setJSValue(result);
    }

    void compileRegExpTest()
    {
        LValue globalObject = lowCell(m_node->child1());
        
        if (m_node->child2().useKind() == RegExpObjectUse) {
            LValue base = lowRegExpObject(m_node->child2());
            
            if (m_node->child3().useKind() == StringUse) {
                LValue argument = lowString(m_node->child3());
                LValue result = vmCall(
                    Int32, m_out.operation(operationRegExpTestString), m_callFrame, globalObject,
                    base, argument);
                setBoolean(result);
                return;
            }

            LValue argument = lowJSValue(m_node->child3());
            LValue result = vmCall(
                Int32, m_out.operation(operationRegExpTest), m_callFrame, globalObject, base,
                argument);
            setBoolean(result);
            return;
        }

        LValue base = lowJSValue(m_node->child2());
        LValue argument = lowJSValue(m_node->child3());
        LValue result = vmCall(
            Int32, m_out.operation(operationRegExpTestGeneric), m_callFrame, globalObject, base,
            argument);
        setBoolean(result);
    }

    void compileNewRegexp()
    {
        // FIXME: We really should be able to inline code that uses NewRegexp. That means not
        // reaching into the CodeBlock here.
        // https://bugs.webkit.org/show_bug.cgi?id=154808

        LValue result = vmCall(
            pointerType(),
            m_out.operation(operationNewRegexp), m_callFrame,
            m_out.constIntPtr(codeBlock()->regexp(m_node->regexpIndex())));
        
        setJSValue(result);
    }

    void compileSetFunctionName()
    {
        vmCall(m_out.voidType, m_out.operation(operationSetFunctionName), m_callFrame,
            lowCell(m_node->child1()), lowJSValue(m_node->child2()));
    }
    
    void compileStringReplace()
    {
        if (m_node->child1().useKind() == StringUse
            && m_node->child2().useKind() == RegExpObjectUse
            && m_node->child3().useKind() == StringUse) {

            if (JSString* replace = m_node->child3()->dynamicCastConstant<JSString*>()) {
                if (!replace->length()) {
                    LValue string = lowString(m_node->child1());
                    LValue regExp = lowRegExpObject(m_node->child2());

                    LValue result = vmCall(
                        Int64, m_out.operation(operationStringProtoFuncReplaceRegExpEmptyStr),
                        m_callFrame, string, regExp);

                    setJSValue(result);
                    return;
                }
            }
            
            LValue string = lowString(m_node->child1());
            LValue regExp = lowRegExpObject(m_node->child2());
            LValue replace = lowString(m_node->child3());

            LValue result = vmCall(
                Int64, m_out.operation(operationStringProtoFuncReplaceRegExpString),
                m_callFrame, string, regExp, replace);

            setJSValue(result);
            return;
        }
        
        LValue result = vmCall(
            Int64, m_out.operation(operationStringProtoFuncReplaceGeneric), m_callFrame,
            lowJSValue(m_node->child1()), lowJSValue(m_node->child2()),
            lowJSValue(m_node->child3()));

        setJSValue(result);
    }

    void compileGetRegExpObjectLastIndex()
    {
        setJSValue(m_out.load64(lowRegExpObject(m_node->child1()), m_heaps.RegExpObject_lastIndex));
    }

    void compileSetRegExpObjectLastIndex()
    {
        LValue regExp = lowRegExpObject(m_node->child1());
        LValue value = lowJSValue(m_node->child2());

        speculate(
            ExoticObjectMode, noValue(), nullptr,
            m_out.isZero32(m_out.load8ZeroExt32(regExp, m_heaps.RegExpObject_lastIndexIsWritable)));
        
        m_out.store64(value, regExp, m_heaps.RegExpObject_lastIndex);
    }
    
    void compileLogShadowChickenPrologue()
    {
        LValue packet = setupShadowChickenPacket();
        
        m_out.storePtr(m_callFrame, packet, m_heaps.ShadowChicken_Packet_frame);
        m_out.storePtr(m_out.loadPtr(addressFor(0)), packet, m_heaps.ShadowChicken_Packet_callerFrame);
        m_out.storePtr(m_out.loadPtr(payloadFor(JSStack::Callee)), packet, m_heaps.ShadowChicken_Packet_callee);
    }
    
    void compileLogShadowChickenTail()
    {
        LValue packet = setupShadowChickenPacket();
        
        m_out.storePtr(m_callFrame, packet, m_heaps.ShadowChicken_Packet_frame);
        m_out.storePtr(m_out.constIntPtr(ShadowChicken::Packet::tailMarker()), packet, m_heaps.ShadowChicken_Packet_callee);
    }

    void compileRecordRegExpCachedResult()
    {
        Edge constructorEdge = m_graph.varArgChild(m_node, 0);
        Edge regExpEdge = m_graph.varArgChild(m_node, 1);
        Edge stringEdge = m_graph.varArgChild(m_node, 2);
        Edge startEdge = m_graph.varArgChild(m_node, 3);
        Edge endEdge = m_graph.varArgChild(m_node, 4);
        
        LValue constructor = lowCell(constructorEdge);
        LValue regExp = lowCell(regExpEdge);
        LValue string = lowCell(stringEdge);
        LValue start = lowInt32(startEdge);
        LValue end = lowInt32(endEdge);

        m_out.storePtr(regExp, constructor, m_heaps.RegExpConstructor_cachedResult_lastRegExp);
        m_out.storePtr(string, constructor, m_heaps.RegExpConstructor_cachedResult_lastInput);
        m_out.store32(start, constructor, m_heaps.RegExpConstructor_cachedResult_result_start);
        m_out.store32(end, constructor, m_heaps.RegExpConstructor_cachedResult_result_end);
        m_out.store32As8(
            m_out.constInt32(0),
            m_out.address(constructor, m_heaps.RegExpConstructor_cachedResult_reified));
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
            DFG::BasicBlock* block = m_graph.block(blockIndex);
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
    
    struct ArgumentsLength {
        ArgumentsLength()
            : isKnown(false)
            , known(UINT_MAX)
            , value(nullptr)
        {
        }
        
        bool isKnown;
        unsigned known;
        LValue value;
    };
    ArgumentsLength getArgumentsLength(InlineCallFrame* inlineCallFrame)
    {
        ArgumentsLength length;

        if (inlineCallFrame && !inlineCallFrame->isVarargs()) {
            length.known = inlineCallFrame->arguments.size() - 1;
            length.isKnown = true;
            length.value = m_out.constInt32(length.known);
        } else {
            length.known = UINT_MAX;
            length.isKnown = false;
            
            VirtualRegister argumentCountRegister;
            if (!inlineCallFrame)
                argumentCountRegister = VirtualRegister(JSStack::ArgumentCount);
            else
                argumentCountRegister = inlineCallFrame->argumentCountRegister;
            length.value = m_out.sub(m_out.load32(payloadFor(argumentCountRegister)), m_out.int32One);
        }
        
        return length;
    }
    
    ArgumentsLength getArgumentsLength()
    {
        return getArgumentsLength(m_node->origin.semantic.inlineCallFrame);
    }
    
    LValue getCurrentCallee()
    {
        if (InlineCallFrame* frame = m_node->origin.semantic.inlineCallFrame) {
            if (frame->isClosureCall)
                return m_out.loadPtr(addressFor(frame->calleeRecovery.virtualRegister()));
            return weakPointer(frame->calleeRecovery.constant().asCell());
        }
        return m_out.loadPtr(addressFor(JSStack::Callee));
    }
    
    LValue getArgumentsStart(InlineCallFrame* inlineCallFrame)
    {
        VirtualRegister start = AssemblyHelpers::argumentsStart(inlineCallFrame);
        return addressFor(start).value();
    }
    
    LValue getArgumentsStart()
    {
        return getArgumentsStart(m_node->origin.semantic.inlineCallFrame);
    }
    
    template<typename Functor>
    void checkStructure(
        LValue structureDiscriminant, const FormattedValue& formattedValue, ExitKind exitKind,
        const StructureSet& set, const Functor& weakStructureDiscriminant)
    {
        if (set.isEmpty()) {
            terminate(exitKind);
            return;
        }

        if (set.size() == 1) {
            speculate(
                exitKind, formattedValue, 0,
                m_out.notEqual(structureDiscriminant, weakStructureDiscriminant(set[0])));
            return;
        }
        
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(continuation);
        for (unsigned i = 0; i < set.size() - 1; ++i) {
            LBasicBlock nextStructure = m_out.newBlock();
            m_out.branch(
                m_out.equal(structureDiscriminant, weakStructureDiscriminant(set[i])),
                unsure(continuation), unsure(nextStructure));
            m_out.appendTo(nextStructure);
        }
        
        speculate(
            exitKind, formattedValue, 0,
            m_out.notEqual(structureDiscriminant, weakStructureDiscriminant(set.last())));
        
        m_out.jump(continuation);
        m_out.appendTo(continuation, lastNext);
    }
    
    LValue numberOrNotCellToInt32(Edge edge, LValue value)
    {
        LBasicBlock intCase = m_out.newBlock();
        LBasicBlock notIntCase = m_out.newBlock();
        LBasicBlock doubleCase = 0;
        LBasicBlock notNumberCase = 0;
        if (edge.useKind() == NotCellUse) {
            doubleCase = m_out.newBlock();
            notNumberCase = m_out.newBlock();
        }
        LBasicBlock continuation = m_out.newBlock();
        
        Vector<ValueFromBlock> results;
        
        m_out.branch(isNotInt32(value), unsure(notIntCase), unsure(intCase));
        
        LBasicBlock lastNext = m_out.appendTo(intCase, notIntCase);
        results.append(m_out.anchor(unboxInt32(value)));
        m_out.jump(continuation);
        
        if (edge.useKind() == NumberUse) {
            m_out.appendTo(notIntCase, continuation);
            FTL_TYPE_CHECK(jsValueValue(value), edge, SpecBytecodeNumber, isCellOrMisc(value));
            results.append(m_out.anchor(doubleToInt32(unboxDouble(value))));
            m_out.jump(continuation);
        } else {
            m_out.appendTo(notIntCase, doubleCase);
            m_out.branch(
                isCellOrMisc(value, provenType(edge)), unsure(notNumberCase), unsure(doubleCase));
            
            m_out.appendTo(doubleCase, notNumberCase);
            results.append(m_out.anchor(doubleToInt32(unboxDouble(value))));
            m_out.jump(continuation);
            
            m_out.appendTo(notNumberCase, continuation);
            
            FTL_TYPE_CHECK(jsValueValue(value), edge, ~SpecCell, isCell(value));
            
            LValue specialResult = m_out.select(
                m_out.equal(value, m_out.constInt64(JSValue::encode(jsBoolean(true)))),
                m_out.int32One, m_out.int32Zero);
            results.append(m_out.anchor(specialResult));
            m_out.jump(continuation);
        }
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(m_out.int32, results);
    }

    void checkInferredType(Edge edge, LValue value, const InferredType::Descriptor& type)
    {
        // This cannot use FTL_TYPE_CHECK or typeCheck() because it is called partially, as in a node like:
        //
        //     MultiPutByOffset(...)
        //
        // may be lowered to:
        //
        //     switch (object->structure) {
        //     case 42:
        //         checkInferredType(..., type1);
        //         ...
        //         break;
        //     case 43:
        //         checkInferredType(..., type2);
        //         ...
        //         break;
        //     }
        //
        // where type1 and type2 are different. Using typeCheck() would mean that the edge would be
        // filtered by type1 & type2, instead of type1 | type2.
        
        switch (type.kind()) {
        case InferredType::Bottom:
            speculate(BadType, jsValueValue(value), edge.node(), m_out.booleanTrue);
            return;

        case InferredType::Boolean:
            speculate(BadType, jsValueValue(value), edge.node(), isNotBoolean(value, provenType(edge)));
            return;

        case InferredType::Other:
            speculate(BadType, jsValueValue(value), edge.node(), isNotOther(value, provenType(edge)));
            return;

        case InferredType::Int32:
            speculate(BadType, jsValueValue(value), edge.node(), isNotInt32(value, provenType(edge)));
            return;

        case InferredType::Number:
            speculate(BadType, jsValueValue(value), edge.node(), isNotNumber(value, provenType(edge)));
            return;

        case InferredType::String:
            speculate(BadType, jsValueValue(value), edge.node(), isNotCell(value, provenType(edge)));
            speculate(BadType, jsValueValue(value), edge.node(), isNotString(value, provenType(edge)));
            return;

        case InferredType::Symbol:
            speculate(BadType, jsValueValue(value), edge.node(), isNotCell(value, provenType(edge)));
            speculate(BadType, jsValueValue(value), edge.node(), isNotSymbol(value, provenType(edge)));
            return;

        case InferredType::ObjectWithStructure:
            speculate(BadType, jsValueValue(value), edge.node(), isNotCell(value, provenType(edge)));
            if (!abstractValue(edge).m_structure.isSubsetOf(StructureSet(type.structure()))) {
                speculate(
                    BadType, jsValueValue(value), edge.node(),
                    m_out.notEqual(
                        m_out.load32(value, m_heaps.JSCell_structureID),
                        weakStructureID(type.structure())));
            }
            return;

        case InferredType::ObjectWithStructureOrOther: {
            LBasicBlock cellCase = m_out.newBlock();
            LBasicBlock notCellCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            m_out.branch(isCell(value, provenType(edge)), unsure(cellCase), unsure(notCellCase));

            LBasicBlock lastNext = m_out.appendTo(cellCase, notCellCase);

            if (!abstractValue(edge).m_structure.isSubsetOf(StructureSet(type.structure()))) {
                speculate(
                    BadType, jsValueValue(value), edge.node(),
                    m_out.notEqual(
                        m_out.load32(value, m_heaps.JSCell_structureID),
                        weakStructureID(type.structure())));
            }

            m_out.jump(continuation);

            m_out.appendTo(notCellCase, continuation);

            speculate(
                BadType, jsValueValue(value), edge.node(),
                isNotOther(value, provenType(edge) & ~SpecCell));

            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            return;
        }

        case InferredType::Object:
            speculate(BadType, jsValueValue(value), edge.node(), isNotCell(value, provenType(edge)));
            speculate(BadType, jsValueValue(value), edge.node(), isNotObject(value, provenType(edge)));
            return;

        case InferredType::ObjectOrOther: {
            LBasicBlock cellCase = m_out.newBlock();
            LBasicBlock notCellCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            m_out.branch(isCell(value, provenType(edge)), unsure(cellCase), unsure(notCellCase));

            LBasicBlock lastNext = m_out.appendTo(cellCase, notCellCase);

            speculate(
                BadType, jsValueValue(value), edge.node(),
                isNotObject(value, provenType(edge) & SpecCell));

            m_out.jump(continuation);

            m_out.appendTo(notCellCase, continuation);

            speculate(
                BadType, jsValueValue(value), edge.node(),
                isNotOther(value, provenType(edge) & ~SpecCell));

            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            return;
        }

        case InferredType::Top:
            return;
        }

        DFG_CRASH(m_graph, m_node, "Bad inferred type");
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

    void initializeArrayElements(IndexingType indexingType, LValue vectorLength, LValue butterfly)
    {
        if (!hasDouble(indexingType)) {
            // The GC already initialized everything to JSValue() for us.
            return;
        }

        // Doubles must be initialized to PNaN.
        LBasicBlock initLoop = m_out.newBlock();
        LBasicBlock initDone = m_out.newBlock();
        
        ValueFromBlock originalIndex = m_out.anchor(vectorLength);
        ValueFromBlock originalPointer = m_out.anchor(butterfly);
        m_out.branch(
            m_out.notZero32(vectorLength), unsure(initLoop), unsure(initDone));
        
        LBasicBlock initLastNext = m_out.appendTo(initLoop, initDone);
        LValue index = m_out.phi(m_out.int32, originalIndex);
        LValue pointer = m_out.phi(m_out.intPtr, originalPointer);
        
        m_out.store64(
            m_out.constInt64(bitwise_cast<int64_t>(PNaN)),
            TypedPointer(m_heaps.indexedDoubleProperties.atAnyIndex(), pointer));
        
        LValue nextIndex = m_out.sub(index, m_out.int32One);
        m_out.addIncomingToPhi(index, m_out.anchor(nextIndex));
        m_out.addIncomingToPhi(pointer, m_out.anchor(m_out.add(pointer, m_out.intPtrEight)));
        m_out.branch(
            m_out.notZero32(nextIndex), unsure(initLoop), unsure(initDone));
        
        m_out.appendTo(initDone, initLastNext);
    }
    
    LValue allocatePropertyStorage(LValue object, Structure* previousStructure)
    {
        if (previousStructure->couldHaveIndexingHeader()) {
            return vmCall(
                m_out.intPtr,
                m_out.operation(
                    operationReallocateButterflyToHavePropertyStorageWithInitialCapacity),
                m_callFrame, object);
        }
        
        LValue result = allocatePropertyStorageWithSizeImpl(initialOutOfLineCapacity);
        m_out.storePtr(result, object, m_heaps.JSObject_butterfly);
        return result;
    }
    
    LValue reallocatePropertyStorage(
        LValue object, LValue oldStorage, Structure* previous, Structure* next)
    {
        size_t oldSize = previous->outOfLineCapacity();
        size_t newSize = oldSize * outOfLineGrowthFactor; 

        ASSERT_UNUSED(next, newSize == next->outOfLineCapacity());
        
        if (previous->couldHaveIndexingHeader()) {
            LValue newAllocSize = m_out.constIntPtr(newSize);                    
            return vmCall(m_out.intPtr, m_out.operation(operationReallocateButterflyToGrowPropertyStorage), m_callFrame, object, newAllocSize);
        }
        
        LValue result = allocatePropertyStorageWithSizeImpl(newSize);

        ptrdiff_t headerSize = -sizeof(IndexingHeader) - sizeof(void*);
        ptrdiff_t endStorage = headerSize - static_cast<ptrdiff_t>(oldSize * sizeof(JSValue));

        for (ptrdiff_t offset = headerSize; offset > endStorage; offset -= sizeof(void*)) {
            LValue loaded = 
                m_out.loadPtr(m_out.address(m_heaps.properties.atAnyNumber(), oldStorage, offset));
            m_out.storePtr(loaded, m_out.address(m_heaps.properties.atAnyNumber(), result, offset));
        }
        
        m_out.storePtr(result, m_out.address(object, m_heaps.JSObject_butterfly));
        
        return result;
    }
    
    LValue allocatePropertyStorageWithSizeImpl(size_t sizeInValues)
    {
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        LValue endOfStorage = allocateBasicStorageAndGetEnd(
            m_out.constIntPtr(sizeInValues * sizeof(JSValue)), slowPath);
        
        ValueFromBlock fastButterfly = m_out.anchor(
            m_out.add(m_out.constIntPtr(sizeof(IndexingHeader)), endOfStorage));
        
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        
        LValue slowButterflyValue;
        if (sizeInValues == initialOutOfLineCapacity) {
            slowButterflyValue = lazySlowPath(
                [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(
                        operationAllocatePropertyStorageWithInitialCapacity,
                        locations[0].directGPR());
                });
        } else {
            slowButterflyValue = lazySlowPath(
                [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(
                        operationAllocatePropertyStorage, locations[0].directGPR(),
                        CCallHelpers::TrustedImmPtr(sizeInValues));
                });
        }
        ValueFromBlock slowButterfly = m_out.anchor(slowButterflyValue);
        
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        return m_out.phi(m_out.intPtr, fastButterfly, slowButterfly);
    }
    
    LValue getById(LValue base)
    {
        Node* node = m_node;
        UniquedStringImpl* uid = m_graph.identifiers()[node->identifierNumber()];

        B3::PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(base);

        // FIXME: If this is a GetByIdFlush, we might get some performance boost if we claim that it
        // clobbers volatile registers late. It's not necessary for correctness, though, since the
        // IC code is super smart about saving registers.
        // https://bugs.webkit.org/show_bug.cgi?id=152848
        
        patchpoint->clobber(RegisterSet::macroScratchRegisters());

        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);

        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);

                CallSiteIndex callSiteIndex =
                    state->jitCode->common.addUniqueCallSiteIndex(node->origin.semantic);

                // This is the direct exit target for operation calls.
                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);

                // This is the exit for call IC's created by the getById for getters. We don't have
                // to do anything weird other than call this, since it will associate the exit with
                // the callsite index.
                exceptionHandle->scheduleExitCreationForUnwind(params, callSiteIndex);

                auto generator = Box<JITGetByIdGenerator>::create(
                    jit.codeBlock(), node->origin.semantic, callSiteIndex,
                    params.unavailableRegisters(), JSValueRegs(params[1].gpr()),
                    JSValueRegs(params[0].gpr()), AccessType::Get);

                generator->generateFastPath(jit);
                CCallHelpers::Label done = jit.label();

                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);

                        generator->slowPathJump().link(&jit);
                        CCallHelpers::Label slowPathBegin = jit.label();
                        CCallHelpers::Call slowPathCall = callOperation(
                            *state, params.unavailableRegisters(), jit, node->origin.semantic,
                            exceptions.get(), operationGetByIdOptimize, params[0].gpr(),
                            CCallHelpers::TrustedImmPtr(generator->stubInfo()), params[1].gpr(),
                            CCallHelpers::TrustedImmPtr(uid)).call();
                        jit.jump().linkTo(done, &jit);

                        generator->reportSlowPathCall(slowPathBegin, slowPathCall);

                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                generator->finalize(linkBuffer);
                            });
                    });
            });

        return patchpoint;
    }

    LValue isFastTypedArray(LValue object)
    {
        return m_out.equal(
            m_out.load32(object, m_heaps.JSArrayBufferView_mode),
            m_out.constInt32(FastTypedArray));
    }
    
    TypedPointer baseIndex(IndexedAbstractHeap& heap, LValue storage, LValue index, Edge edge, ptrdiff_t offset = 0)
    {
        return m_out.baseIndex(
            heap, storage, m_out.zeroExtPtr(index), provenValue(edge), offset);
    }

    template<typename IntFunctor, typename DoubleFunctor>
    void compare(
        const IntFunctor& intFunctor, const DoubleFunctor& doubleFunctor,
        S_JITOperation_EJJ helperFunction)
    {
        if (m_node->isBinaryUseKind(Int32Use)) {
            LValue left = lowInt32(m_node->child1());
            LValue right = lowInt32(m_node->child2());
            setBoolean(intFunctor(left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(Int52RepUse)) {
            Int52Kind kind;
            LValue left = lowWhicheverInt52(m_node->child1(), kind);
            LValue right = lowInt52(m_node->child2(), kind);
            setBoolean(intFunctor(left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(DoubleRepUse)) {
            LValue left = lowDouble(m_node->child1());
            LValue right = lowDouble(m_node->child2());
            setBoolean(doubleFunctor(left, right));
            return;
        }
        
        if (m_node->isBinaryUseKind(UntypedUse)) {
            nonSpeculativeCompare(intFunctor, helperFunction);
            return;
        }
        
        DFG_CRASH(m_graph, m_node, "Bad use kinds");
    }
    
    void compareEqObjectOrOtherToObject(Edge leftChild, Edge rightChild)
    {
        LValue rightCell = lowCell(rightChild);
        LValue leftValue = lowJSValue(leftChild, ManualOperandSpeculation);
        
        speculateTruthyObject(rightChild, rightCell, SpecObject);
        
        LBasicBlock leftCellCase = m_out.newBlock();
        LBasicBlock leftNotCellCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        m_out.branch(
            isCell(leftValue, provenType(leftChild)),
            unsure(leftCellCase), unsure(leftNotCellCase));
        
        LBasicBlock lastNext = m_out.appendTo(leftCellCase, leftNotCellCase);
        speculateTruthyObject(leftChild, leftValue, SpecObject | (~SpecCell));
        ValueFromBlock cellResult = m_out.anchor(m_out.equal(rightCell, leftValue));
        m_out.jump(continuation);
        
        m_out.appendTo(leftNotCellCase, continuation);
        FTL_TYPE_CHECK(
            jsValueValue(leftValue), leftChild, SpecOther | SpecCell, isNotOther(leftValue));
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
        
        FTL_TYPE_CHECK(jsValueValue(cell), edge, filter, isNotObject(cell));
        speculate(
            BadType, jsValueValue(cell), edge.node(),
            m_out.testNonZero32(
                m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoFlags),
                m_out.constInt32(MasqueradesAsUndefined)));
    }

    template<typename IntFunctor>
    void nonSpeculativeCompare(const IntFunctor& intFunctor, S_JITOperation_EJJ helperFunction)
    {
        LValue left = lowJSValue(m_node->child1());
        LValue right = lowJSValue(m_node->child2());
        
        LBasicBlock leftIsInt = m_out.newBlock();
        LBasicBlock fastPath = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        m_out.branch(isNotInt32(left), rarely(slowPath), usually(leftIsInt));
        
        LBasicBlock lastNext = m_out.appendTo(leftIsInt, fastPath);
        m_out.branch(isNotInt32(right), rarely(slowPath), usually(fastPath));
        
        m_out.appendTo(fastPath, slowPath);
        ValueFromBlock fastResult = m_out.anchor(intFunctor(unboxInt32(left), unboxInt32(right)));
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        ValueFromBlock slowResult = m_out.anchor(m_out.notNull(vmCall(
            m_out.intPtr, m_out.operation(helperFunction), m_callFrame, left, right)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(m_out.boolean, fastResult, slowResult));
    }

    LValue stringsEqual(LValue leftJSString, LValue rightJSString)
    {
        LBasicBlock notTriviallyUnequalCase = m_out.newBlock();
        LBasicBlock notEmptyCase = m_out.newBlock();
        LBasicBlock leftReadyCase = m_out.newBlock();
        LBasicBlock rightReadyCase = m_out.newBlock();
        LBasicBlock left8BitCase = m_out.newBlock();
        LBasicBlock right8BitCase = m_out.newBlock();
        LBasicBlock loop = m_out.newBlock();
        LBasicBlock bytesEqual = m_out.newBlock();
        LBasicBlock trueCase = m_out.newBlock();
        LBasicBlock falseCase = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue length = m_out.load32(leftJSString, m_heaps.JSString_length);

        m_out.branch(
            m_out.notEqual(length, m_out.load32(rightJSString, m_heaps.JSString_length)),
            unsure(falseCase), unsure(notTriviallyUnequalCase));

        LBasicBlock lastNext = m_out.appendTo(notTriviallyUnequalCase, notEmptyCase);

        m_out.branch(m_out.isZero32(length), unsure(trueCase), unsure(notEmptyCase));

        m_out.appendTo(notEmptyCase, leftReadyCase);

        LValue left = m_out.loadPtr(leftJSString, m_heaps.JSString_value);
        LValue right = m_out.loadPtr(rightJSString, m_heaps.JSString_value);

        m_out.branch(m_out.notNull(left), usually(leftReadyCase), rarely(slowCase));

        m_out.appendTo(leftReadyCase, rightReadyCase);
        
        m_out.branch(m_out.notNull(right), usually(rightReadyCase), rarely(slowCase));

        m_out.appendTo(rightReadyCase, left8BitCase);

        m_out.branch(
            m_out.testIsZero32(
                m_out.load32(left, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIs8Bit())),
            unsure(slowCase), unsure(left8BitCase));

        m_out.appendTo(left8BitCase, right8BitCase);

        m_out.branch(
            m_out.testIsZero32(
                m_out.load32(right, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIs8Bit())),
            unsure(slowCase), unsure(right8BitCase));

        m_out.appendTo(right8BitCase, loop);

        LValue leftData = m_out.loadPtr(left, m_heaps.StringImpl_data);
        LValue rightData = m_out.loadPtr(right, m_heaps.StringImpl_data);

        ValueFromBlock indexAtStart = m_out.anchor(length);

        m_out.jump(loop);

        m_out.appendTo(loop, bytesEqual);

        LValue indexAtLoopTop = m_out.phi(m_out.int32, indexAtStart);
        LValue indexInLoop = m_out.sub(indexAtLoopTop, m_out.int32One);

        LValue leftByte = m_out.load8ZeroExt32(
            m_out.baseIndex(m_heaps.characters8, leftData, m_out.zeroExtPtr(indexInLoop)));
        LValue rightByte = m_out.load8ZeroExt32(
            m_out.baseIndex(m_heaps.characters8, rightData, m_out.zeroExtPtr(indexInLoop)));

        m_out.branch(m_out.notEqual(leftByte, rightByte), unsure(falseCase), unsure(bytesEqual));

        m_out.appendTo(bytesEqual, trueCase);

        ValueFromBlock indexForNextIteration = m_out.anchor(indexInLoop);
        m_out.addIncomingToPhi(indexAtLoopTop, indexForNextIteration);
        m_out.branch(m_out.notZero32(indexInLoop), unsure(loop), unsure(trueCase));

        m_out.appendTo(trueCase, falseCase);
        
        ValueFromBlock trueResult = m_out.anchor(m_out.booleanTrue);
        m_out.jump(continuation);

        m_out.appendTo(falseCase, slowCase);

        ValueFromBlock falseResult = m_out.anchor(m_out.booleanFalse);
        m_out.jump(continuation);

        m_out.appendTo(slowCase, continuation);

        LValue slowResultValue = vmCall(
            m_out.int64, m_out.operation(operationCompareStringEq), m_callFrame,
            leftJSString, rightJSString);
        ValueFromBlock slowResult = m_out.anchor(unboxBoolean(slowResultValue));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        return m_out.phi(m_out.boolean, trueResult, falseResult, slowResult);
    }

    enum ScratchFPRUsage {
        DontNeedScratchFPR,
        NeedScratchFPR
    };
    template<typename BinaryArithOpGenerator, ScratchFPRUsage scratchFPRUsage = DontNeedScratchFPR>
    void emitBinarySnippet(J_JITOperation_EJJ slowPathFunction)
    {
        Node* node = m_node;
        
        LValue left = lowJSValue(node->child1());
        LValue right = lowJSValue(node->child2());

        SnippetOperand leftOperand(m_state.forNode(node->child1()).resultType());
        SnippetOperand rightOperand(m_state.forNode(node->child2()).resultType());
            
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(left);
        patchpoint->appendSomeRegister(right);
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        patchpoint->numGPScratchRegisters = 1;
        patchpoint->numFPScratchRegisters = 2;
        if (scratchFPRUsage == NeedScratchFPR)
            patchpoint->numFPScratchRegisters++;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);

                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);

                auto generator = Box<BinaryArithOpGenerator>::create(
                    leftOperand, rightOperand, JSValueRegs(params[0].gpr()),
                    JSValueRegs(params[1].gpr()), JSValueRegs(params[2].gpr()),
                    params.fpScratch(0), params.fpScratch(1), params.gpScratch(0),
                    scratchFPRUsage == NeedScratchFPR ? params.fpScratch(2) : InvalidFPRReg);

                generator->generateFastPath(jit);

                if (generator->didEmitFastPath()) {
                    generator->endJumpList().link(&jit);
                    CCallHelpers::Label done = jit.label();
                    
                    params.addLatePath(
                        [=] (CCallHelpers& jit) {
                            AllowMacroScratchRegisterUsage allowScratch(jit);
                            
                            generator->slowPathJumpList().link(&jit);
                            callOperation(
                                *state, params.unavailableRegisters(), jit, node->origin.semantic,
                                exceptions.get(), slowPathFunction, params[0].gpr(),
                                params[1].gpr(), params[2].gpr());
                            jit.jump().linkTo(done, &jit);
                        });
                } else {
                    callOperation(
                        *state, params.unavailableRegisters(), jit, node->origin.semantic,
                        exceptions.get(), slowPathFunction, params[0].gpr(), params[1].gpr(),
                        params[2].gpr());
                }
            });

        setJSValue(patchpoint);
    }

    template<typename BinaryBitOpGenerator>
    void emitBinaryBitOpSnippet(J_JITOperation_EJJ slowPathFunction)
    {
        Node* node = m_node;
        
        // FIXME: Make this do exceptions.
        // https://bugs.webkit.org/show_bug.cgi?id=151686
            
        LValue left = lowJSValue(node->child1());
        LValue right = lowJSValue(node->child2());

        SnippetOperand leftOperand(m_state.forNode(node->child1()).resultType());
        SnippetOperand rightOperand(m_state.forNode(node->child2()).resultType());
            
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(left);
        patchpoint->appendSomeRegister(right);
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        patchpoint->numGPScratchRegisters = 1;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                    
                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);
                    
                auto generator = Box<BinaryBitOpGenerator>::create(
                    leftOperand, rightOperand, JSValueRegs(params[0].gpr()),
                    JSValueRegs(params[1].gpr()), JSValueRegs(params[2].gpr()), params.gpScratch(0));

                generator->generateFastPath(jit);
                generator->endJumpList().link(&jit);
                CCallHelpers::Label done = jit.label();

                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);
                            
                        generator->slowPathJumpList().link(&jit);
                        callOperation(
                            *state, params.unavailableRegisters(), jit, node->origin.semantic,
                            exceptions.get(), slowPathFunction, params[0].gpr(),
                            params[1].gpr(), params[2].gpr());
                        jit.jump().linkTo(done, &jit);
                    });
            });

        setJSValue(patchpoint);
    }

    void emitRightShiftSnippet(JITRightShiftGenerator::ShiftType shiftType)
    {
        Node* node = m_node;
        
        // FIXME: Make this do exceptions.
        // https://bugs.webkit.org/show_bug.cgi?id=151686
            
        LValue left = lowJSValue(node->child1());
        LValue right = lowJSValue(node->child2());

        SnippetOperand leftOperand(m_state.forNode(node->child1()).resultType());
        SnippetOperand rightOperand(m_state.forNode(node->child2()).resultType());
            
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(left);
        patchpoint->appendSomeRegister(right);
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        patchpoint->numGPScratchRegisters = 1;
        patchpoint->numFPScratchRegisters = 1;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                    
                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);
                    
                auto generator = Box<JITRightShiftGenerator>::create(
                    leftOperand, rightOperand, JSValueRegs(params[0].gpr()),
                    JSValueRegs(params[1].gpr()), JSValueRegs(params[2].gpr()),
                    params.fpScratch(0), params.gpScratch(0), InvalidFPRReg, shiftType);

                generator->generateFastPath(jit);
                generator->endJumpList().link(&jit);
                CCallHelpers::Label done = jit.label();

                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);
                            
                        generator->slowPathJumpList().link(&jit);

                        J_JITOperation_EJJ slowPathFunction =
                            shiftType == JITRightShiftGenerator::SignedShift
                            ? operationValueBitRShift : operationValueBitURShift;
                        
                        callOperation(
                            *state, params.unavailableRegisters(), jit, node->origin.semantic,
                            exceptions.get(), slowPathFunction, params[0].gpr(),
                            params[1].gpr(), params[2].gpr());
                        jit.jump().linkTo(done, &jit);
                    });
            });

        setJSValue(patchpoint);
    }

    LValue allocateCell(LValue allocator, LBasicBlock slowPath)
    {
        LBasicBlock success = m_out.newBlock();
    
        LValue result;
        LValue condition;
        if (Options::forceGCSlowPaths()) {
            result = m_out.intPtrZero;
            condition = m_out.booleanFalse;
        } else {
            result = m_out.loadPtr(
                allocator, m_heaps.MarkedAllocator_freeListHead);
            condition = m_out.notNull(result);
        }
        m_out.branch(condition, usually(success), rarely(slowPath));
        
        m_out.appendTo(success);
        
        m_out.storePtr(
            m_out.loadPtr(result, m_heaps.JSCell_freeListNext),
            allocator, m_heaps.MarkedAllocator_freeListHead);

        return result;
    }
    
    void storeStructure(LValue object, Structure* structure)
    {
        m_out.store32(m_out.constInt32(structure->id()), object, m_heaps.JSCell_structureID);
        m_out.store32(
            m_out.constInt32(structure->objectInitializationBlob()),
            object, m_heaps.JSCell_usefulBytes);
    }

    LValue allocateCell(LValue allocator, Structure* structure, LBasicBlock slowPath)
    {
        LValue result = allocateCell(allocator, slowPath);
        storeStructure(result, structure);
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
    LValue allocateObject(
        size_t size, Structure* structure, LValue butterfly, LBasicBlock slowPath)
    {
        MarkedAllocator* allocator = &vm().heap.allocatorForObjectOfType<ClassType>(size);
        return allocateObject(m_out.constIntPtr(allocator), structure, butterfly, slowPath);
    }
    
    template<typename ClassType>
    LValue allocateObject(Structure* structure, LValue butterfly, LBasicBlock slowPath)
    {
        return allocateObject<ClassType>(
            ClassType::allocationSize(0), structure, butterfly, slowPath);
    }
    
    template<typename ClassType>
    LValue allocateVariableSizedObject(
        LValue size, Structure* structure, LValue butterfly, LBasicBlock slowPath)
    {
        static_assert(!(MarkedSpace::preciseStep & (MarkedSpace::preciseStep - 1)), "MarkedSpace::preciseStep must be a power of two.");
        static_assert(!(MarkedSpace::impreciseStep & (MarkedSpace::impreciseStep - 1)), "MarkedSpace::impreciseStep must be a power of two.");

        LValue subspace = m_out.constIntPtr(&vm().heap.subspaceForObjectOfType<ClassType>());
        
        LBasicBlock smallCaseBlock = m_out.newBlock();
        LBasicBlock largeOrOversizeCaseBlock = m_out.newBlock();
        LBasicBlock largeCaseBlock = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue uproundedSize = m_out.add(size, m_out.constInt32(MarkedSpace::preciseStep - 1));
        LValue isSmall = m_out.below(uproundedSize, m_out.constInt32(MarkedSpace::preciseCutoff));
        m_out.branch(isSmall, unsure(smallCaseBlock), unsure(largeOrOversizeCaseBlock));
        
        LBasicBlock lastNext = m_out.appendTo(smallCaseBlock, largeOrOversizeCaseBlock);
        TypedPointer address = m_out.baseIndex(
            m_heaps.MarkedSpace_Subspace_preciseAllocators, subspace,
            m_out.zeroExtPtr(m_out.lShr(uproundedSize, m_out.constInt32(getLSBSet(MarkedSpace::preciseStep)))));
        ValueFromBlock smallAllocator = m_out.anchor(address.value());
        m_out.jump(continuation);
        
        m_out.appendTo(largeOrOversizeCaseBlock, largeCaseBlock);
        m_out.branch(
            m_out.below(uproundedSize, m_out.constInt32(MarkedSpace::impreciseCutoff)),
            usually(largeCaseBlock), rarely(slowPath));
        
        m_out.appendTo(largeCaseBlock, continuation);
        address = m_out.baseIndex(
            m_heaps.MarkedSpace_Subspace_impreciseAllocators, subspace,
            m_out.zeroExtPtr(m_out.lShr(uproundedSize, m_out.constInt32(getLSBSet(MarkedSpace::impreciseStep)))));
        ValueFromBlock largeAllocator = m_out.anchor(address.value());
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        LValue allocator = m_out.phi(m_out.intPtr, smallAllocator, largeAllocator);
        
        return allocateObject(allocator, structure, butterfly, slowPath);
    }
    
    // Returns a pointer to the end of the allocation.
    LValue allocateBasicStorageAndGetEnd(LValue size, LBasicBlock slowPath)
    {
        CopiedAllocator& allocator = vm().heap.storageAllocator();
        
        LBasicBlock success = m_out.newBlock();
        
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

    LValue allocateBasicStorage(LValue size, LBasicBlock slowPath)
    {
        return m_out.sub(allocateBasicStorageAndGetEnd(size, slowPath), size);
    }
    
    LValue allocateObject(Structure* structure)
    {
        size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
        MarkedAllocator* allocator = &vm().heap.allocatorForObjectWithoutDestructor(allocationSize);
        
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        ValueFromBlock fastResult = m_out.anchor(allocateObject(
            m_out.constIntPtr(allocator), structure, m_out.intPtrZero, slowPath));
        
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);

        LValue slowResultValue = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationNewObject, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure));
            });
        ValueFromBlock slowResult = m_out.anchor(slowResultValue);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(m_out.intPtr, fastResult, slowResult);
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
                    m_out.constInt64(bitwise_cast<int64_t>(PNaN)),
                    butterfly, m_heaps.indexedDoubleProperties[i]);
            }
        }
        
        return ArrayValues(object, butterfly);
    }
    
    ArrayValues allocateJSArray(Structure* structure, unsigned numElements)
    {
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        ArrayValues fastValues = allocateJSArray(structure, numElements, slowPath);
        ValueFromBlock fastArray = m_out.anchor(fastValues.array);
        ValueFromBlock fastButterfly = m_out.anchor(fastValues.butterfly);
        
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);

        LValue slowArrayValue = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationNewArrayWithSize, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure), CCallHelpers::TrustedImm32(numElements));
            });
        ValueFromBlock slowArray = m_out.anchor(slowArrayValue);
        ValueFromBlock slowButterfly = m_out.anchor(
            m_out.loadPtr(slowArrayValue, m_heaps.JSObject_butterfly));

        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        return ArrayValues(
            m_out.phi(m_out.intPtr, fastArray, slowArray),
            m_out.phi(m_out.intPtr, fastButterfly, slowButterfly));
    }
    
    LValue setupShadowChickenPacket()
    {
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        TypedPointer addressOfLogCursor = m_out.absolute(vm().shadowChicken().addressOfLogCursor());
        LValue logCursor = m_out.loadPtr(addressOfLogCursor);
        
        ValueFromBlock fastResult = m_out.anchor(logCursor);
        
        m_out.branch(
            m_out.below(logCursor, m_out.constIntPtr(vm().shadowChicken().logEnd())),
            usually(continuation), rarely(slowCase));
        
        LBasicBlock lastNext = m_out.appendTo(slowCase, continuation);
        
        vmCall(Void, m_out.operation(operationProcessShadowChickenLog), m_callFrame);
        
        ValueFromBlock slowResult = m_out.anchor(m_out.loadPtr(addressOfLogCursor));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        LValue result = m_out.phi(pointerType(), fastResult, slowResult);
        
        m_out.storePtr(
            m_out.add(result, m_out.constIntPtr(sizeof(ShadowChicken::Packet))),
            addressOfLogCursor);
        
        return result;
    }
    
    LValue boolify(Edge edge)
    {
        switch (edge.useKind()) {
        case BooleanUse:
        case KnownBooleanUse:
            return lowBoolean(edge);
        case Int32Use:
            return m_out.notZero32(lowInt32(edge));
        case DoubleRepUse:
            return m_out.doubleNotEqualAndOrdered(lowDouble(edge), m_out.doubleZero);
        case ObjectOrOtherUse:
            return m_out.logicalNot(
                equalNullOrUndefined(
                    edge, CellCaseSpeculatesObject, SpeculateNullOrUndefined,
                    ManualOperandSpeculation));
        case StringUse: {
            LValue stringValue = lowString(edge);
            LValue length = m_out.load32NonNegative(stringValue, m_heaps.JSString_length);
            return m_out.notEqual(length, m_out.int32Zero);
        }
        case StringOrOtherUse: {
            LValue value = lowJSValue(edge, ManualOperandSpeculation);

            LBasicBlock cellCase = m_out.newBlock();
            LBasicBlock notCellCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            m_out.branch(isCell(value, provenType(edge)), unsure(cellCase), unsure(notCellCase));
            
            LBasicBlock lastNext = m_out.appendTo(cellCase, notCellCase);
            
            FTL_TYPE_CHECK(jsValueValue(value), edge, (~SpecCell) | SpecString, isNotString(value));
            LValue length = m_out.load32NonNegative(value, m_heaps.JSString_length);
            ValueFromBlock cellResult = m_out.anchor(m_out.notEqual(length, m_out.int32Zero));
            m_out.jump(continuation);
            
            m_out.appendTo(notCellCase, continuation);
            
            FTL_TYPE_CHECK(jsValueValue(value), edge, SpecCell | SpecOther, isNotOther(value));
            ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
            m_out.jump(continuation);
            m_out.appendTo(continuation, lastNext);

            return m_out.phi(Int32, cellResult, notCellResult);
        }
        case UntypedUse: {
            LValue value = lowJSValue(edge);
            
            // Implements the following control flow structure:
            // if (value is cell) {
            //     if (value is string)
            //         result = !!value->length
            //     else {
            //         do evil things for masquerades-as-undefined
            //         result = true
            //     }
            // } else if (value is int32) {
            //     result = !!unboxInt32(value)
            // } else if (value is number) {
            //     result = !!unboxDouble(value)
            // } else {
            //     result = value == jsTrue
            // }
            
            LBasicBlock cellCase = m_out.newBlock();
            LBasicBlock stringCase = m_out.newBlock();
            LBasicBlock notStringCase = m_out.newBlock();
            LBasicBlock notCellCase = m_out.newBlock();
            LBasicBlock int32Case = m_out.newBlock();
            LBasicBlock notInt32Case = m_out.newBlock();
            LBasicBlock doubleCase = m_out.newBlock();
            LBasicBlock notDoubleCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            Vector<ValueFromBlock> results;
            
            m_out.branch(isCell(value, provenType(edge)), unsure(cellCase), unsure(notCellCase));
            
            LBasicBlock lastNext = m_out.appendTo(cellCase, stringCase);
            m_out.branch(
                isString(value, provenType(edge) & SpecCell),
                unsure(stringCase), unsure(notStringCase));
            
            m_out.appendTo(stringCase, notStringCase);
            LValue nonEmptyString = m_out.notZero32(
                m_out.load32NonNegative(value, m_heaps.JSString_length));
            results.append(m_out.anchor(nonEmptyString));
            m_out.jump(continuation);
            
            m_out.appendTo(notStringCase, notCellCase);
            LValue isTruthyObject;
            if (masqueradesAsUndefinedWatchpointIsStillValid())
                isTruthyObject = m_out.booleanTrue;
            else {
                LBasicBlock masqueradesCase = m_out.newBlock();
                
                results.append(m_out.anchor(m_out.booleanTrue));
                
                m_out.branch(
                    m_out.testIsZero32(
                        m_out.load8ZeroExt32(value, m_heaps.JSCell_typeInfoFlags),
                        m_out.constInt32(MasqueradesAsUndefined)),
                    usually(continuation), rarely(masqueradesCase));
                
                m_out.appendTo(masqueradesCase);
                
                isTruthyObject = m_out.notEqual(
                    m_out.constIntPtr(m_graph.globalObjectFor(m_node->origin.semantic)),
                    m_out.loadPtr(loadStructure(value), m_heaps.Structure_globalObject));
            }
            results.append(m_out.anchor(isTruthyObject));
            m_out.jump(continuation);
            
            m_out.appendTo(notCellCase, int32Case);
            m_out.branch(
                isInt32(value, provenType(edge) & ~SpecCell),
                unsure(int32Case), unsure(notInt32Case));
            
            m_out.appendTo(int32Case, notInt32Case);
            results.append(m_out.anchor(m_out.notZero32(unboxInt32(value))));
            m_out.jump(continuation);
            
            m_out.appendTo(notInt32Case, doubleCase);
            m_out.branch(
                isNumber(value, provenType(edge) & ~SpecCell),
                unsure(doubleCase), unsure(notDoubleCase));
            
            m_out.appendTo(doubleCase, notDoubleCase);
            LValue doubleIsTruthy = m_out.doubleNotEqualAndOrdered(
                unboxDouble(value), m_out.constDouble(0));
            results.append(m_out.anchor(doubleIsTruthy));
            m_out.jump(continuation);
            
            m_out.appendTo(notDoubleCase, continuation);
            LValue miscIsTruthy = m_out.equal(
                value, m_out.constInt64(JSValue::encode(jsBoolean(true))));
            results.append(m_out.anchor(miscIsTruthy));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            return m_out.phi(m_out.boolean, results);
        }
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
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
        
        LBasicBlock cellCase = m_out.newBlock();
        LBasicBlock primitiveCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        m_out.branch(isNotCell(value, provenType(edge)), unsure(primitiveCase), unsure(cellCase));
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, primitiveCase);
        
        Vector<ValueFromBlock, 3> results;
        
        switch (cellMode) {
        case AllCellsAreFalse:
            break;
        case CellCaseSpeculatesObject:
            FTL_TYPE_CHECK(
                jsValueValue(value), edge, (~SpecCell) | SpecObject, isNotObject(value));
            break;
        }
        
        if (validWatchpoint) {
            results.append(m_out.anchor(m_out.booleanFalse));
            m_out.jump(continuation);
        } else {
            LBasicBlock masqueradesCase =
                m_out.newBlock();
                
            results.append(m_out.anchor(m_out.booleanFalse));
            
            m_out.branch(
                m_out.testNonZero32(
                    m_out.load8ZeroExt32(value, m_heaps.JSCell_typeInfoFlags),
                    m_out.constInt32(MasqueradesAsUndefined)),
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
            primitiveResult = isOther(value, provenType(edge));
            break;
        case SpeculateNullOrUndefined:
            FTL_TYPE_CHECK(
                jsValueValue(value), edge, SpecCell | SpecOther, isNotOther(value));
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
                m_out.newBlock();
            LBasicBlock performStore =
                m_out.newBlock();
                
            m_out.branch(isNotInBounds, unsure(notInBoundsCase), unsure(performStore));
                
            LBasicBlock lastNext = m_out.appendTo(notInBoundsCase, performStore);
                
            LValue isOutOfBounds = m_out.aboveOrEqual(
                index, m_out.load32NonNegative(storage, m_heaps.Butterfly_vectorLength));
                
            if (!m_node->arrayMode().isOutOfBounds())
                speculate(OutOfBounds, noValue(), 0, isOutOfBounds);
            else {
                LBasicBlock outOfBoundsCase =
                    m_out.newBlock();
                LBasicBlock holeCase =
                    m_out.newBlock();
                    
                m_out.branch(isOutOfBounds, rarely(outOfBoundsCase), usually(holeCase));
                    
                LBasicBlock innerLastNext = m_out.appendTo(outOfBoundsCase, holeCase);
                    
                vmCall(
                    m_out.voidType, m_out.operation(slowPathFunction),
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
        ASSERT(type == m_out.intPtr || type == m_out.int32);

        Vector<SwitchCase> cases;
        for (unsigned i = 0; i < data->cases.size(); ++i) {
            SwitchCase newCase;

            if (type == m_out.intPtr) {
                newCase = SwitchCase(m_out.constIntPtr(data->cases[i].value.switchLookupValue(data->kind)),
                    lowBlock(data->cases[i].target.block), Weight(data->cases[i].target.count));
            } else if (type == m_out.int32) {
                newCase = SwitchCase(m_out.constInt32(data->cases[i].value.switchLookupValue(data->kind)),
                    lowBlock(data->cases[i].target.block), Weight(data->cases[i].target.count));
            } else
                CRASH();

            cases.append(newCase);
        }
        
        m_out.switchInstruction(
            switchValue, cases,
            lowBlock(data->fallThrough.block), Weight(data->fallThrough.count));
    }
    
    void switchString(SwitchData* data, LValue string)
    {
        bool canDoBinarySwitch = true;
        unsigned totalLength = 0;
        
        for (DFG::SwitchCase myCase : data->cases) {
            StringImpl* string = myCase.value.stringImpl();
            if (!string->is8Bit()) {
                canDoBinarySwitch = false;
                break;
            }
            if (string->length() > Options::maximumBinaryStringSwitchCaseLength()) {
                canDoBinarySwitch = false;
                break;
            }
            totalLength += string->length();
        }
        
        if (!canDoBinarySwitch || totalLength > Options::maximumBinaryStringSwitchTotalLength()) {
            switchStringSlow(data, string);
            return;
        }
        
        LValue stringImpl = m_out.loadPtr(string, m_heaps.JSString_value);
        LValue length = m_out.load32(string, m_heaps.JSString_length);
        
        LBasicBlock hasImplBlock = m_out.newBlock();
        LBasicBlock is8BitBlock = m_out.newBlock();
        LBasicBlock slowBlock = m_out.newBlock();
        
        m_out.branch(m_out.isNull(stringImpl), unsure(slowBlock), unsure(hasImplBlock));
        
        LBasicBlock lastNext = m_out.appendTo(hasImplBlock, is8BitBlock);
        
        m_out.branch(
            m_out.testIsZero32(
                m_out.load32(stringImpl, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIs8Bit())),
            unsure(slowBlock), unsure(is8BitBlock));
        
        m_out.appendTo(is8BitBlock, slowBlock);
        
        LValue buffer = m_out.loadPtr(stringImpl, m_heaps.StringImpl_data);
        
        // FIXME: We should propagate branch weight data to the cases of this switch.
        // https://bugs.webkit.org/show_bug.cgi?id=144368
        
        Vector<StringSwitchCase> cases;
        for (DFG::SwitchCase myCase : data->cases)
            cases.append(StringSwitchCase(myCase.value.stringImpl(), lowBlock(myCase.target.block)));
        std::sort(cases.begin(), cases.end());
        switchStringRecurse(data, buffer, length, cases, 0, 0, cases.size(), 0, false);

        m_out.appendTo(slowBlock, lastNext);
        switchStringSlow(data, string);
    }
    
    // The code for string switching is based closely on the same code in the DFG backend. While it
    // would be nice to reduce the amount of similar-looking code, it seems like this is one of
    // those algorithms where factoring out the common bits would result in more code than just
    // duplicating.
    
    struct StringSwitchCase {
        StringSwitchCase() { }
        
        StringSwitchCase(StringImpl* string, LBasicBlock target)
            : string(string)
            , target(target)
        {
        }

        bool operator<(const StringSwitchCase& other) const
        {
            return stringLessThan(*string, *other.string);
        }
        
        StringImpl* string;
        LBasicBlock target;
    };
    
    struct CharacterCase {
        CharacterCase()
            : character(0)
            , begin(0)
            , end(0)
        {
        }
        
        CharacterCase(LChar character, unsigned begin, unsigned end)
            : character(character)
            , begin(begin)
            , end(end)
        {
        }
        
        bool operator<(const CharacterCase& other) const
        {
            return character < other.character;
        }
        
        LChar character;
        unsigned begin;
        unsigned end;
    };
    
    void switchStringRecurse(
        SwitchData* data, LValue buffer, LValue length, const Vector<StringSwitchCase>& cases,
        unsigned numChecked, unsigned begin, unsigned end, unsigned alreadyCheckedLength,
        unsigned checkedExactLength)
    {
        LBasicBlock fallThrough = lowBlock(data->fallThrough.block);
        
        if (begin == end) {
            m_out.jump(fallThrough);
            return;
        }
        
        unsigned minLength = cases[begin].string->length();
        unsigned commonChars = minLength;
        bool allLengthsEqual = true;
        for (unsigned i = begin + 1; i < end; ++i) {
            unsigned myCommonChars = numChecked;
            unsigned limit = std::min(cases[begin].string->length(), cases[i].string->length());
            for (unsigned j = numChecked; j < limit; ++j) {
                if (cases[begin].string->at(j) != cases[i].string->at(j))
                    break;
                myCommonChars++;
            }
            commonChars = std::min(commonChars, myCommonChars);
            if (minLength != cases[i].string->length())
                allLengthsEqual = false;
            minLength = std::min(minLength, cases[i].string->length());
        }
        
        if (checkedExactLength) {
            DFG_ASSERT(m_graph, m_node, alreadyCheckedLength == minLength);
            DFG_ASSERT(m_graph, m_node, allLengthsEqual);
        }
        
        DFG_ASSERT(m_graph, m_node, minLength >= commonChars);
        
        if (!allLengthsEqual && alreadyCheckedLength < minLength)
            m_out.check(m_out.below(length, m_out.constInt32(minLength)), unsure(fallThrough));
        if (allLengthsEqual && (alreadyCheckedLength < minLength || !checkedExactLength))
            m_out.check(m_out.notEqual(length, m_out.constInt32(minLength)), unsure(fallThrough));
        
        for (unsigned i = numChecked; i < commonChars; ++i) {
            m_out.check(
                m_out.notEqual(
                    m_out.load8ZeroExt32(buffer, m_heaps.characters8[i]),
                    m_out.constInt32(static_cast<uint16_t>(cases[begin].string->at(i)))),
                unsure(fallThrough));
        }
        
        if (minLength == commonChars) {
            // This is the case where one of the cases is a prefix of all of the other cases.
            // We've already checked that the input string is a prefix of all of the cases,
            // so we just check length to jump to that case.
            
            DFG_ASSERT(m_graph, m_node, cases[begin].string->length() == commonChars);
            for (unsigned i = begin + 1; i < end; ++i)
                DFG_ASSERT(m_graph, m_node, cases[i].string->length() > commonChars);
            
            if (allLengthsEqual) {
                DFG_ASSERT(m_graph, m_node, end == begin + 1);
                m_out.jump(cases[begin].target);
                return;
            }
            
            m_out.check(
                m_out.equal(length, m_out.constInt32(commonChars)),
                unsure(cases[begin].target));
            
            // We've checked if the length is >= minLength, and then we checked if the length is
            // == commonChars. We get to this point if it is >= minLength but not == commonChars.
            // Hence we know that it now must be > minLength, i.e. that it's >= minLength + 1.
            switchStringRecurse(
                data, buffer, length, cases, commonChars, begin + 1, end, minLength + 1, false);
            return;
        }
        
        // At this point we know that the string is longer than commonChars, and we've only verified
        // commonChars. Use a binary switch on the next unchecked character, i.e.
        // string[commonChars].
        
        DFG_ASSERT(m_graph, m_node, end >= begin + 2);
        
        LValue uncheckedChar = m_out.load8ZeroExt32(buffer, m_heaps.characters8[commonChars]);
        
        Vector<CharacterCase> characterCases;
        CharacterCase currentCase(cases[begin].string->at(commonChars), begin, begin + 1);
        for (unsigned i = begin + 1; i < end; ++i) {
            LChar currentChar = cases[i].string->at(commonChars);
            if (currentChar != currentCase.character) {
                currentCase.end = i;
                characterCases.append(currentCase);
                currentCase = CharacterCase(currentChar, i, i + 1);
            } else
                currentCase.end = i + 1;
        }
        characterCases.append(currentCase);
        
        Vector<LBasicBlock> characterBlocks;
        for (unsigned i = characterCases.size(); i--;)
            characterBlocks.append(m_out.newBlock());
        
        Vector<SwitchCase> switchCases;
        for (unsigned i = 0; i < characterCases.size(); ++i) {
            if (i)
                DFG_ASSERT(m_graph, m_node, characterCases[i - 1].character < characterCases[i].character);
            switchCases.append(SwitchCase(
                m_out.constInt32(characterCases[i].character), characterBlocks[i], Weight()));
        }
        m_out.switchInstruction(uncheckedChar, switchCases, fallThrough, Weight());
        
        LBasicBlock lastNext = m_out.m_nextBlock;
        characterBlocks.append(lastNext); // Makes it convenient to set nextBlock.
        for (unsigned i = 0; i < characterCases.size(); ++i) {
            m_out.appendTo(characterBlocks[i], characterBlocks[i + 1]);
            switchStringRecurse(
                data, buffer, length, cases, commonChars + 1,
                characterCases[i].begin, characterCases[i].end, minLength, allLengthsEqual);
        }
        
        DFG_ASSERT(m_graph, m_node, m_out.m_nextBlock == lastNext);
    }
    
    void switchStringSlow(SwitchData* data, LValue string)
    {
        // FIXME: We ought to be able to use computed gotos here. We would save the labels of the
        // blocks we want to jump to, and then request their addresses after compilation completes.
        // https://bugs.webkit.org/show_bug.cgi?id=144369
        
        LValue branchOffset = vmCall(
            m_out.int32, m_out.operation(operationSwitchStringAndGetBranchOffset),
            m_callFrame, m_out.constIntPtr(data->switchTableIndex), string);
        
        StringJumpTable& table = codeBlock()->stringSwitchJumpTable(data->switchTableIndex);
        
        Vector<SwitchCase> cases;
        std::unordered_set<int32_t> alreadyHandled; // These may be negative, or zero, or probably other stuff, too. We don't want to mess with HashSet's corner cases and we don't really care about throughput here.
        for (unsigned i = 0; i < data->cases.size(); ++i) {
            // FIXME: The fact that we're using the bytecode's switch table means that the
            // following DFG IR transformation would be invalid.
            //
            // Original code:
            //     switch (v) {
            //     case "foo":
            //     case "bar":
            //         things();
            //         break;
            //     default:
            //         break;
            //     }
            //
            // New code:
            //     switch (v) {
            //     case "foo":
            //         instrumentFoo();
            //         goto _things;
            //     case "bar":
            //         instrumentBar();
            //     _things:
            //         things();
            //         break;
            //     default:
            //         break;
            //     }
            //
            // Luckily, we don't currently do any such transformation. But it's kind of silly that
            // this is an issue.
            // https://bugs.webkit.org/show_bug.cgi?id=144635
            
            DFG::SwitchCase myCase = data->cases[i];
            StringJumpTable::StringOffsetTable::iterator iter =
                table.offsetTable.find(myCase.value.stringImpl());
            DFG_ASSERT(m_graph, m_node, iter != table.offsetTable.end());
            
            if (!alreadyHandled.insert(iter->value.branchOffset).second)
                continue;

            cases.append(SwitchCase(
                m_out.constInt32(iter->value.branchOffset),
                lowBlock(myCase.target.block), Weight(myCase.target.count)));
        }
        
        m_out.switchInstruction(
            branchOffset, cases, lowBlock(data->fallThrough.block),
            Weight(data->fallThrough.count));
    }
    
    // Calls the functor at the point of code generation where we know what the result type is.
    // You can emit whatever code you like at that point. Expects you to terminate the basic block.
    // When buildTypeOf() returns, it will have terminated all basic blocks that it created. So, if
    // you aren't using this as the terminator of a high-level block, you should create your own
    // contination and set it as the nextBlock (m_out.insertNewBlocksBefore(continuation)) before
    // calling this. For example:
    //
    // LBasicBlock continuation = m_out.newBlock();
    // LBasicBlock lastNext = m_out.insertNewBlocksBefore(continuation);
    // buildTypeOf(
    //     child, value,
    //     [&] (TypeofType type) {
    //          do things;
    //          m_out.jump(continuation);
    //     });
    // m_out.appendTo(continuation, lastNext);
    template<typename Functor>
    void buildTypeOf(Edge child, LValue value, const Functor& functor)
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        
        // Implements the following branching structure:
        //
        // if (is cell) {
        //     if (is object) {
        //         if (is function) {
        //             return function;
        //         } else if (doesn't have call trap and doesn't masquerade as undefined) {
        //             return object
        //         } else {
        //             return slowPath();
        //         }
        //     } else if (is string) {
        //         return string
        //     } else {
        //         return symbol
        //     }
        // } else if (is number) {
        //     return number
        // } else if (is null) {
        //     return object
        // } else if (is boolean) {
        //     return boolean
        // } else {
        //     return undefined
        // }
        
        LBasicBlock cellCase = m_out.newBlock();
        LBasicBlock objectCase = m_out.newBlock();
        LBasicBlock functionCase = m_out.newBlock();
        LBasicBlock notFunctionCase = m_out.newBlock();
        LBasicBlock reallyObjectCase = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock unreachable = m_out.newBlock();
        LBasicBlock notObjectCase = m_out.newBlock();
        LBasicBlock stringCase = m_out.newBlock();
        LBasicBlock symbolCase = m_out.newBlock();
        LBasicBlock notCellCase = m_out.newBlock();
        LBasicBlock numberCase = m_out.newBlock();
        LBasicBlock notNumberCase = m_out.newBlock();
        LBasicBlock notNullCase = m_out.newBlock();
        LBasicBlock booleanCase = m_out.newBlock();
        LBasicBlock undefinedCase = m_out.newBlock();
        
        m_out.branch(isCell(value, provenType(child)), unsure(cellCase), unsure(notCellCase));
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, objectCase);
        m_out.branch(isObject(value, provenType(child)), unsure(objectCase), unsure(notObjectCase));
        
        m_out.appendTo(objectCase, functionCase);
        m_out.branch(
            isFunction(value, provenType(child) & SpecObject),
            unsure(functionCase), unsure(notFunctionCase));
        
        m_out.appendTo(functionCase, notFunctionCase);
        functor(TypeofType::Function);
        
        m_out.appendTo(notFunctionCase, reallyObjectCase);
        m_out.branch(
            isExoticForTypeof(value, provenType(child) & (SpecObject - SpecFunction)),
            rarely(slowPath), usually(reallyObjectCase));
        
        m_out.appendTo(reallyObjectCase, slowPath);
        functor(TypeofType::Object);
        
        m_out.appendTo(slowPath, unreachable);
        LValue result = lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(
                    operationTypeOfObjectAsTypeofType, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(globalObject), locations[1].directGPR());
            }, value);
        Vector<SwitchCase, 3> cases;
        cases.append(SwitchCase(m_out.constInt32(static_cast<int32_t>(TypeofType::Undefined)), undefinedCase));
        cases.append(SwitchCase(m_out.constInt32(static_cast<int32_t>(TypeofType::Object)), reallyObjectCase));
        cases.append(SwitchCase(m_out.constInt32(static_cast<int32_t>(TypeofType::Function)), functionCase));
        m_out.switchInstruction(m_out.castToInt32(result), cases, unreachable, Weight());
        
        m_out.appendTo(unreachable, notObjectCase);
        m_out.unreachable();
        
        m_out.appendTo(notObjectCase, stringCase);
        m_out.branch(
            isString(value, provenType(child) & (SpecCell - SpecObject)),
            unsure(stringCase), unsure(symbolCase));
        
        m_out.appendTo(stringCase, symbolCase);
        functor(TypeofType::String);
        
        m_out.appendTo(symbolCase, notCellCase);
        functor(TypeofType::Symbol);
        
        m_out.appendTo(notCellCase, numberCase);
        m_out.branch(
            isNumber(value, provenType(child) & ~SpecCell),
            unsure(numberCase), unsure(notNumberCase));
        
        m_out.appendTo(numberCase, notNumberCase);
        functor(TypeofType::Number);
        
        m_out.appendTo(notNumberCase, notNullCase);
        LValue isNull;
        if (provenType(child) & SpecOther)
            isNull = m_out.equal(value, m_out.constInt64(ValueNull));
        else
            isNull = m_out.booleanFalse;
        m_out.branch(isNull, unsure(reallyObjectCase), unsure(notNullCase));
        
        m_out.appendTo(notNullCase, booleanCase);
        m_out.branch(
            isBoolean(value, provenType(child) & ~(SpecCell | SpecFullNumber)),
            unsure(booleanCase), unsure(undefinedCase));
        
        m_out.appendTo(booleanCase, undefinedCase);
        functor(TypeofType::Boolean);
        
        m_out.appendTo(undefinedCase, lastNext);
        functor(TypeofType::Undefined);
    }
    
    LValue doubleToInt32(LValue doubleValue, double low, double high, bool isSigned = true)
    {
        LBasicBlock greatEnough = m_out.newBlock();
        LBasicBlock withinRange = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
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
            fastResult = m_out.doubleToInt(doubleValue);
        else
            fastResult = m_out.doubleToUInt(doubleValue);
        results.append(m_out.anchor(fastResult));
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        results.append(m_out.anchor(m_out.call(m_out.int32, m_out.operation(toInt32), doubleValue)));
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
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue fastResultValue = m_out.doubleToInt(doubleValue);
        ValueFromBlock fastResult = m_out.anchor(fastResultValue);
        m_out.branch(
            m_out.equal(fastResultValue, m_out.constInt32(0x80000000)),
            rarely(slowPath), usually(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(slowPath, continuation);
        ValueFromBlock slowResult = m_out.anchor(
            m_out.call(m_out.int32, m_out.operation(toInt32), doubleValue));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(m_out.int32, fastResult, slowResult);
    }

    // This is a mechanism for creating a code generator that fills in a gap in the code using our
    // own MacroAssembler. This is useful for slow paths that involve a lot of code and we don't want
    // to pay the price of B3 optimizing it. A lazy slow path will only be generated if it actually
    // executes. On the other hand, a lazy slow path always incurs the cost of two additional jumps.
    // Also, the lazy slow path's register allocation state is slaved to whatever B3 did, so you
    // have to use a ScratchRegisterAllocator to try to use some unused registers and you may have
    // to spill to top of stack if there aren't enough registers available.
    //
    // Lazy slow paths involve three different stages of execution. Each stage has unique
    // capabilities and knowledge. The stages are:
    //
    // 1) DFG->B3 lowering, i.e. code that runs in this phase. Lowering is the last time you will
    //    have access to LValues. If there is an LValue that needs to be fed as input to a lazy slow
    //    path, then you must pass it as an argument here (as one of the varargs arguments after the
    //    functor). But, lowering doesn't know which registers will be used for those LValues. Hence
    //    you pass a lambda to lazySlowPath() and that lambda will run during stage (2):
    //
    // 2) FTLCompile.cpp's fixFunctionBasedOnStackMaps. This code is the only stage at which we know
    //    the mapping from arguments passed to this method in (1) and the registers that B3
    //    selected for those arguments. You don't actually want to generate any code here, since then
    //    the slow path wouldn't actually be lazily generated. Instead, you want to save the
    //    registers being used for the arguments and defer code generation to stage (3) by creating
    //    and returning a LazySlowPath::Generator:
    //
    // 3) LazySlowPath's generate() method. This code runs in response to the lazy slow path
    //    executing for the first time. It will call the generator you created in stage (2).
    //
    // Note that each time you invoke stage (1), stage (2) may be invoked zero, one, or many times.
    // Stage (2) will usually be invoked once for stage (1). But, B3 may kill the code, in which
    // case stage (2) won't run. B3 may duplicate the code (for example via tail duplication),
    // leading to many calls to your stage (2) lambda. Stage (3) may be called zero or once for each
    // stage (2). It will be called zero times if the slow path never runs. This is what you hope for
    // whenever you use the lazySlowPath() mechanism.
    //
    // A typical use of lazySlowPath() will look like the example below, which just creates a slow
    // path that adds some value to the input and returns it.
    //
    // // Stage (1) is here. This is your last chance to figure out which LValues to use as inputs.
    // // Notice how we pass "input" as an argument to lazySlowPath().
    // LValue input = ...;
    // int addend = ...;
    // LValue output = lazySlowPath(
    //     [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
    //         // Stage (2) is here. This is your last chance to figure out which registers are used
    //         // for which values. Location zero is always the return value. You can ignore it if
    //         // you don't want to return anything. Location 1 is the register for the first
    //         // argument to the lazySlowPath(), i.e. "input". Note that the Location object could
    //         // also hold an FPR, if you are passing a double.
    //         GPRReg outputGPR = locations[0].directGPR();
    //         GPRReg inputGPR = locations[1].directGPR();
    //         return LazySlowPath::createGenerator(
    //             [=] (CCallHelpers& jit, LazySlowPath::GenerationParams& params) {
    //                 // Stage (3) is here. This is when you generate code. You have access to the
    //                 // registers you collected in stage (2) because this lambda closes over those
    //                 // variables (outputGPR and inputGPR). You also have access to whatever extra
    //                 // data you collected in stage (1), such as the addend in this case.
    //                 jit.add32(TrustedImm32(addend), inputGPR, outputGPR);
    //                 // You have to end by jumping to done. There is nothing to fall through to.
    //                 // You can also jump to the exception handler (see LazySlowPath.h for more
    //                 // info). Note that currently you cannot OSR exit.
    //                 params.doneJumps.append(jit.jump());
    //             });
    //     },
    //     input);
    //
    // You can basically pass as many inputs as you like, either using this varargs form, or by
    // passing a Vector of LValues.
    //
    // Note that if your slow path is only doing a call, you can use the createLazyCallGenerator()
    // helper. For example:
    //
    // LValue input = ...;
    // LValue output = lazySlowPath(
    //     [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
    //         return createLazyCallGenerator(
    //             operationDoThings, locations[0].directGPR(), locations[1].directGPR());
    //     }, input);
    //
    // Finally, note that all of the lambdas - both the stage (2) lambda and the stage (3) lambda -
    // run after the function that created them returns. Hence, you should not use by-reference
    // capture (i.e. [&]) in any of these lambdas.
    template<typename Functor, typename... ArgumentTypes>
    LValue lazySlowPath(const Functor& functor, ArgumentTypes... arguments)
    {
        return lazySlowPath(functor, Vector<LValue>{ arguments... });
    }

    template<typename Functor>
    LValue lazySlowPath(const Functor& functor, const Vector<LValue>& userArguments)
    {
        CodeOrigin origin = m_node->origin.semantic;
        
        PatchpointValue* result = m_out.patchpoint(B3::Int64);
        for (LValue arg : userArguments)
            result->append(ConstrainedValue(arg, B3::ValueRep::SomeRegister));

        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(result);
        
        result->clobber(RegisterSet::macroScratchRegisters());
        State* state = &m_ftlState;

        result->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                Vector<Location> locations;
                for (const B3::ValueRep& rep : params)
                    locations.append(Location::forValueRep(rep));

                RefPtr<LazySlowPath::Generator> generator = functor(locations);
                
                CCallHelpers::PatchableJump patchableJump = jit.patchableJump();
                CCallHelpers::Label done = jit.label();

                RegisterSet usedRegisters = params.unavailableRegisters();

                RefPtr<ExceptionTarget> exceptionTarget =
                    exceptionHandle->scheduleExitCreation(params);

                // FIXME: As part of handling exceptions, we need to create a concrete OSRExit here.
                // Doing so should automagically register late paths that emit exit thunks.

                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);
                        patchableJump.m_jump.link(&jit);
                        unsigned index = state->jitCode->lazySlowPaths.size();
                        state->jitCode->lazySlowPaths.append(nullptr);
                        jit.pushToSaveImmediateWithoutTouchingRegisters(
                            CCallHelpers::TrustedImm32(index));
                        CCallHelpers::Jump generatorJump = jit.jump();

                        // Note that so long as we're here, we don't really know if our late path
                        // runs before or after any other late paths that we might depend on, like
                        // the exception thunk.

                        RefPtr<JITCode> jitCode = state->jitCode;
                        VM* vm = &state->graph.m_vm;

                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                linkBuffer.link(
                                    generatorJump, CodeLocationLabel(
                                        vm->getCTIStub(
                                            lazySlowPathGenerationThunkGenerator).code()));
                                
                                CodeLocationJump linkedPatchableJump = CodeLocationJump(
                                    linkBuffer.locationOf(patchableJump));
                                CodeLocationLabel linkedDone = linkBuffer.locationOf(done);

                                CallSiteIndex callSiteIndex =
                                    jitCode->common.addUniqueCallSiteIndex(origin);
                                    
                                std::unique_ptr<LazySlowPath> lazySlowPath =
                                    std::make_unique<LazySlowPath>(
                                        linkedPatchableJump, linkedDone,
                                        exceptionTarget->label(linkBuffer), usedRegisters,
                                        callSiteIndex, generator);
                                    
                                jitCode->lazySlowPaths[index] = WTFMove(lazySlowPath);
                            });
                    });
            });
        return result;
    }
    
    void speculate(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition)
    {
        appendOSRExit(kind, lowValue, highValue, failCondition, m_origin);
    }
    
    void terminate(ExitKind kind)
    {
        speculate(kind, noValue(), nullptr, m_out.booleanTrue);
        didAlreadyTerminate();
    }
    
    void didAlreadyTerminate()
    {
        m_state.setIsValid(false);
    }
    
    void typeCheck(
        FormattedValue lowValue, Edge highValue, SpeculatedType typesPassedThrough,
        LValue failCondition, ExitKind exitKind = BadType)
    {
        appendTypeCheck(lowValue, highValue, typesPassedThrough, failCondition, exitKind);
    }
    
    void appendTypeCheck(
        FormattedValue lowValue, Edge highValue, SpeculatedType typesPassedThrough,
        LValue failCondition, ExitKind exitKind)
    {
        if (!m_interpreter.needsTypeCheck(highValue, typesPassedThrough))
            return;
        ASSERT(mayHaveTypeCheck(highValue.useKind()));
        appendOSRExit(exitKind, lowValue, highValue.node(), failCondition, m_origin);
        m_interpreter.filter(highValue, typesPassedThrough);
    }
    
    LValue lowInt32(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || (edge.useKind() == Int32Use || edge.useKind() == KnownInt32Use));
        
        if (edge->hasConstant()) {
            JSValue value = edge->asJSValue();
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

        DFG_ASSERT(m_graph, m_node, !(provenType(edge) & SpecInt32));
        terminate(Uncountable);
        return m_out.int32Zero;
    }
    
    enum Int52Kind { StrictInt52, Int52 };
    LValue lowInt52(Edge edge, Int52Kind kind)
    {
        DFG_ASSERT(m_graph, m_node, edge.useKind() == Int52RepUse);
        
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

        DFG_ASSERT(m_graph, m_node, !provenType(edge));
        terminate(Uncountable);
        return m_out.int64Zero;
    }
    
    LValue lowInt52(Edge edge)
    {
        return lowInt52(edge, Int52);
    }
    
    LValue lowStrictInt52(Edge edge)
    {
        return lowInt52(edge, StrictInt52);
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
        DFG_CRASH(m_graph, m_node, "Bad use kind");
        return Int52;
    }
    
    LValue lowWhicheverInt52(Edge edge, Int52Kind& kind)
    {
        kind = bestInt52Kind(edge);
        return lowInt52(edge, kind);
    }
    
    LValue lowCell(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        DFG_ASSERT(m_graph, m_node, mode == ManualOperandSpeculation || DFG::isCell(edge.useKind()));
        
        if (edge->op() == JSConstant) {
            JSValue value = edge->asJSValue();
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
        
        DFG_ASSERT(m_graph, m_node, !(provenType(edge) & SpecCell));
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

    LValue lowRegExpObject(Edge edge)
    {
        LValue result = lowCell(edge);
        speculateRegExpObject(edge, result);
        return result;
    }
    
    LValue lowString(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == StringUse || edge.useKind() == KnownStringUse || edge.useKind() == StringIdentUse);
        
        LValue result = lowCell(edge, mode);
        speculateString(edge, result);
        return result;
    }
    
    LValue lowStringIdent(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == StringIdentUse);
        
        LValue string = lowString(edge, mode);
        LValue stringImpl = m_out.loadPtr(string, m_heaps.JSString_value);
        speculateStringIdent(edge, string, stringImpl);
        return stringImpl;
    }

    LValue lowSymbol(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == SymbolUse);

        LValue result = lowCell(edge, mode);
        speculateSymbol(edge, result);
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
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == BooleanUse || edge.useKind() == KnownBooleanUse);
        
        if (edge->hasConstant()) {
            JSValue value = edge->asJSValue();
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
        
        DFG_ASSERT(m_graph, m_node, !(provenType(edge) & SpecBoolean));
        terminate(Uncountable);
        return m_out.booleanFalse;
    }
    
    LValue lowDouble(Edge edge)
    {
        DFG_ASSERT(m_graph, m_node, isDouble(edge.useKind()));
        
        LoweredNodeValue value = m_doubleValues.get(edge.node());
        if (isValid(value))
            return value.value();
        DFG_ASSERT(m_graph, m_node, !provenType(edge));
        terminate(Uncountable);
        return m_out.doubleZero;
    }
    
    LValue lowJSValue(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        DFG_ASSERT(m_graph, m_node, mode == ManualOperandSpeculation || edge.useKind() == UntypedUse);
        DFG_ASSERT(m_graph, m_node, !isDouble(edge.useKind()));
        DFG_ASSERT(m_graph, m_node, edge.useKind() != Int52RepUse);
        
        if (edge->hasConstant())
            return m_out.constInt64(JSValue::encode(edge->asJSValue()));

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
        
        DFG_CRASH(m_graph, m_node, "Value not defined");
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
            m_out.notEqual(m_out.signExt32To64(result), value));
        setInt32(edge.node(), result);
        return result;
    }
    
    LValue strictInt52ToDouble(LValue value)
    {
        return m_out.intToDouble(value);
    }
    
    LValue strictInt52ToJSValue(LValue value)
    {
        LBasicBlock isInt32 = m_out.newBlock();
        LBasicBlock isDouble = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        Vector<ValueFromBlock, 2> results;
            
        LValue int32Value = m_out.castToInt32(value);
        m_out.branch(
            m_out.equal(m_out.signExt32To64(int32Value), value),
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
    
    LValue strictInt52ToInt52(LValue value)
    {
        return m_out.shl(value, m_out.constInt64(JSValue::int52ShiftAmount));
    }
    
    LValue int52ToStrictInt52(LValue value)
    {
        return m_out.aShr(value, m_out.constInt64(JSValue::int52ShiftAmount));
    }
    
    LValue isInt32(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecInt32))
            return proven;
        return m_out.aboveOrEqual(jsValue, m_tagTypeNumber);
    }
    LValue isNotInt32(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~SpecInt32))
            return proven;
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
    
    LValue isCellOrMisc(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecCell | SpecMisc))
            return proven;
        return m_out.testIsZero64(jsValue, m_tagTypeNumber);
    }
    LValue isNotCellOrMisc(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~(SpecCell | SpecMisc)))
            return proven;
        return m_out.testNonZero64(jsValue, m_tagTypeNumber);
    }

    LValue unboxDouble(LValue jsValue)
    {
        return m_out.bitCast(m_out.add(jsValue, m_tagTypeNumber), m_out.doubleType);
    }
    LValue boxDouble(LValue doubleValue)
    {
        return m_out.sub(m_out.bitCast(doubleValue, m_out.int64), m_tagTypeNumber);
    }
    
    LValue jsValueToStrictInt52(Edge edge, LValue boxedValue)
    {
        LBasicBlock intCase = m_out.newBlock();
        LBasicBlock doubleCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
            
        LValue isNotInt32;
        if (!m_interpreter.needsTypeCheck(edge, SpecInt32))
            isNotInt32 = m_out.booleanFalse;
        else if (!m_interpreter.needsTypeCheck(edge, ~SpecInt32))
            isNotInt32 = m_out.booleanTrue;
        else
            isNotInt32 = this->isNotInt32(boxedValue);
        m_out.branch(isNotInt32, unsure(doubleCase), unsure(intCase));
            
        LBasicBlock lastNext = m_out.appendTo(intCase, doubleCase);
            
        ValueFromBlock intToInt52 = m_out.anchor(
            m_out.signExt32To64(unboxInt32(boxedValue)));
        m_out.jump(continuation);
            
        m_out.appendTo(doubleCase, continuation);
        
        LValue possibleResult = m_out.call(
            m_out.int64, m_out.operation(operationConvertBoxedDoubleToInt52), boxedValue);
        FTL_TYPE_CHECK(
            jsValueValue(boxedValue), edge, SpecInt32 | SpecInt52AsDouble,
            m_out.equal(possibleResult, m_out.constInt64(JSValue::notInt52)));
            
        ValueFromBlock doubleToInt52 = m_out.anchor(possibleResult);
        m_out.jump(continuation);
            
        m_out.appendTo(continuation, lastNext);
            
        return m_out.phi(m_out.int64, intToInt52, doubleToInt52);
    }
    
    LValue doubleToStrictInt52(Edge edge, LValue value)
    {
        LValue possibleResult = m_out.call(
            m_out.int64, m_out.operation(operationConvertDoubleToInt52), value);
        FTL_TYPE_CHECK_WITH_EXIT_KIND(Int52Overflow,
            doubleValue(value), edge, SpecInt52AsDouble,
            m_out.equal(possibleResult, m_out.constInt64(JSValue::notInt52)));
        
        return possibleResult;
    }

    LValue convertDoubleToInt32(LValue value, bool shouldCheckNegativeZero)
    {
        LValue integerValue = m_out.doubleToInt(value);
        LValue integerValueConvertedToDouble = m_out.intToDouble(integerValue);
        LValue valueNotConvertibleToInteger = m_out.doubleNotEqualOrUnordered(value, integerValueConvertedToDouble);
        speculate(Overflow, FormattedValue(DataFormatDouble, value), m_node, valueNotConvertibleToInteger);

        if (shouldCheckNegativeZero) {
            LBasicBlock valueIsZero = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            m_out.branch(m_out.isZero32(integerValue), unsure(valueIsZero), unsure(continuation));

            LBasicBlock lastNext = m_out.appendTo(valueIsZero, continuation);

            LValue doubleBitcastToInt64 = m_out.bitCast(value, m_out.int64);
            LValue signBitSet = m_out.lessThan(doubleBitcastToInt64, m_out.constInt64(0));

            speculate(NegativeZero, FormattedValue(DataFormatDouble, value), m_node, signBitSet);
            m_out.jump(continuation);
            m_out.appendTo(continuation, lastNext);
        }
        return integerValue;
    }

    LValue isNumber(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecFullNumber))
            return proven;
        return isNotCellOrMisc(jsValue);
    }
    LValue isNotNumber(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~SpecFullNumber))
            return proven;
        return isCellOrMisc(jsValue);
    }
    
    LValue isNotCell(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~SpecCell))
            return proven;
        return m_out.testNonZero64(jsValue, m_tagMask);
    }
    
    LValue isCell(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecCell))
            return proven;
        return m_out.testIsZero64(jsValue, m_tagMask);
    }
    
    LValue isNotMisc(LValue value, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~SpecMisc))
            return proven;
        return m_out.above(value, m_out.constInt64(TagBitTypeOther | TagBitBool | TagBitUndefined));
    }
    
    LValue isMisc(LValue value, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecMisc))
            return proven;
        return m_out.logicalNot(isNotMisc(value));
    }
    
    LValue isNotBoolean(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~SpecBoolean))
            return proven;
        return m_out.testNonZero64(
            m_out.bitXor(jsValue, m_out.constInt64(ValueFalse)),
            m_out.constInt64(~1));
    }
    LValue isBoolean(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecBoolean))
            return proven;
        return m_out.logicalNot(isNotBoolean(jsValue));
    }
    LValue unboxBoolean(LValue jsValue)
    {
        // We want to use a cast that guarantees that B3 knows that even the integer
        // value is just 0 or 1. But for now we do it the dumb way.
        return m_out.notZero64(m_out.bitAnd(jsValue, m_out.constInt64(1)));
    }
    LValue boxBoolean(LValue value)
    {
        return m_out.select(
            value, m_out.constInt64(ValueTrue), m_out.constInt64(ValueFalse));
    }
    
    LValue isNotOther(LValue value, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~SpecOther))
            return proven;
        return m_out.notEqual(
            m_out.bitAnd(value, m_out.constInt64(~TagBitUndefined)),
            m_out.constInt64(ValueNull));
    }
    LValue isOther(LValue value, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecOther))
            return proven;
        return m_out.equal(
            m_out.bitAnd(value, m_out.constInt64(~TagBitUndefined)),
            m_out.constInt64(ValueNull));
    }
    
    LValue isProvenValue(SpeculatedType provenType, SpeculatedType wantedType)
    {
        if (!(provenType & ~wantedType))
            return m_out.booleanTrue;
        if (!(provenType & wantedType))
            return m_out.booleanFalse;
        return nullptr;
    }

    void speculate(Edge edge)
    {
        switch (edge.useKind()) {
        case UntypedUse:
            break;
        case KnownInt32Use:
        case KnownStringUse:
        case KnownPrimitiveUse:
        case DoubleRepUse:
        case Int52RepUse:
            ASSERT(!m_interpreter.needsTypeCheck(edge));
            break;
        case Int32Use:
            speculateInt32(edge);
            break;
        case CellUse:
            speculateCell(edge);
            break;
        case CellOrOtherUse:
            speculateCellOrOther(edge);
            break;
        case KnownCellUse:
            ASSERT(!m_interpreter.needsTypeCheck(edge));
            break;
        case MachineIntUse:
            speculateMachineInt(edge);
            break;
        case ObjectUse:
            speculateObject(edge);
            break;
        case FunctionUse:
            speculateFunction(edge);
            break;
        case ObjectOrOtherUse:
            speculateObjectOrOther(edge);
            break;
        case FinalObjectUse:
            speculateFinalObject(edge);
            break;
        case RegExpObjectUse:
            speculateRegExpObject(edge);
            break;
        case StringUse:
            speculateString(edge);
            break;
        case StringOrOtherUse:
            speculateStringOrOther(edge);
            break;
        case StringIdentUse:
            speculateStringIdent(edge);
            break;
        case SymbolUse:
            speculateSymbol(edge);
            break;
        case StringObjectUse:
            speculateStringObject(edge);
            break;
        case StringOrStringObjectUse:
            speculateStringOrStringObject(edge);
            break;
        case NumberUse:
            speculateNumber(edge);
            break;
        case RealNumberUse:
            speculateRealNumber(edge);
            break;
        case DoubleRepRealUse:
            speculateDoubleRepReal(edge);
            break;
        case DoubleRepMachineIntUse:
            speculateDoubleRepMachineInt(edge);
            break;
        case BooleanUse:
            speculateBoolean(edge);
            break;
        case NotStringVarUse:
            speculateNotStringVar(edge);
            break;
        case NotCellUse:
            speculateNotCell(edge);
            break;
        case OtherUse:
            speculateOther(edge);
            break;
        case MiscUse:
            speculateMisc(edge);
            break;
        default:
            DFG_CRASH(m_graph, m_node, "Unsupported speculation use kind");
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
    
    void speculateCellOrOther(Edge edge)
    {
        LValue value = lowJSValue(edge, ManualOperandSpeculation);

        LBasicBlock isNotCell = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(isCell(value, provenType(edge)), unsure(continuation), unsure(isNotCell));

        LBasicBlock lastNext = m_out.appendTo(isNotCell, continuation);
        FTL_TYPE_CHECK(jsValueValue(value), edge, SpecCell | SpecOther, isNotOther(value));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
    }
    
    void speculateMachineInt(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        jsValueToStrictInt52(edge, lowJSValue(edge, ManualOperandSpeculation));
    }
    
    LValue isObject(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, SpecObject))
            return proven;
        return m_out.aboveOrEqual(
            m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType),
            m_out.constInt32(ObjectType));
    }

    LValue isNotObject(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, ~SpecObject))
            return proven;
        return m_out.below(
            m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType),
            m_out.constInt32(ObjectType));
    }

    LValue isNotString(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, ~SpecString))
            return proven;
        return m_out.notEqual(
            m_out.load32(cell, m_heaps.JSCell_structureID),
            m_out.constInt32(vm().stringStructure->id()));
    }
    
    LValue isString(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, SpecString))
            return proven;
        return m_out.equal(
            m_out.load32(cell, m_heaps.JSCell_structureID),
            m_out.constInt32(vm().stringStructure->id()));
    }

    LValue isNotSymbol(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, ~SpecSymbol))
            return proven;
        return m_out.notEqual(
            m_out.load32(cell, m_heaps.JSCell_structureID),
            m_out.constInt32(vm().symbolStructure->id()));
    }

    LValue isArrayType(LValue cell, ArrayMode arrayMode)
    {
        switch (arrayMode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            LValue indexingType = m_out.load8ZeroExt32(cell, m_heaps.JSCell_indexingType);
            
            switch (arrayMode.arrayClass()) {
            case Array::OriginalArray:
                DFG_CRASH(m_graph, m_node, "Unexpected original array");
                return 0;
                
            case Array::Array:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt32(IsArray | IndexingShapeMask)),
                    m_out.constInt32(IsArray | arrayMode.shapeMask()));
                
            case Array::NonArray:
            case Array::OriginalNonArray:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt32(IsArray | IndexingShapeMask)),
                    m_out.constInt32(arrayMode.shapeMask()));
                
            case Array::PossiblyArray:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt32(IndexingShapeMask)),
                    m_out.constInt32(arrayMode.shapeMask()));
            }
            
            DFG_CRASH(m_graph, m_node, "Corrupt array class");
        }
            
        case Array::DirectArguments:
            return m_out.equal(
                m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType),
                m_out.constInt32(DirectArgumentsType));
            
        case Array::ScopedArguments:
            return m_out.equal(
                m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType),
                m_out.constInt32(ScopedArgumentsType));
            
        default:
            return m_out.equal(
                m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType),
                m_out.constInt32(typeForTypedArrayType(arrayMode.typedArrayType())));
        }
    }
    
    LValue isFunction(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, SpecFunction))
            return proven;
        return isType(cell, JSFunctionType);
    }
    LValue isNotFunction(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, ~SpecFunction))
            return proven;
        return isNotType(cell, JSFunctionType);
    }
            
    LValue isExoticForTypeof(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (!(type & SpecObjectOther))
            return m_out.booleanFalse;
        return m_out.testNonZero32(
            m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoFlags),
            m_out.constInt32(MasqueradesAsUndefined | TypeOfShouldCallGetCallData));
    }
    
    LValue isType(LValue cell, JSType type)
    {
        return m_out.equal(
            m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType),
            m_out.constInt32(type));
    }
    
    LValue isNotType(LValue cell, JSType type)
    {
        return m_out.logicalNot(isType(cell, type));
    }
    
    void speculateObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecObject, isNotObject(cell));
    }
    
    void speculateObject(Edge edge)
    {
        speculateObject(edge, lowCell(edge));
    }
    
    void speculateFunction(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecFunction, isNotFunction(cell));
    }
    
    void speculateFunction(Edge edge)
    {
        speculateFunction(edge, lowCell(edge));
    }
    
    void speculateObjectOrOther(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowJSValue(edge, ManualOperandSpeculation);
        
        LBasicBlock cellCase = m_out.newBlock();
        LBasicBlock primitiveCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        m_out.branch(isNotCell(value, provenType(edge)), unsure(primitiveCase), unsure(cellCase));
        
        LBasicBlock lastNext = m_out.appendTo(cellCase, primitiveCase);
        
        FTL_TYPE_CHECK(
            jsValueValue(value), edge, (~SpecCell) | SpecObject, isNotObject(value));
        
        m_out.jump(continuation);
        
        m_out.appendTo(primitiveCase, continuation);
        
        FTL_TYPE_CHECK(
            jsValueValue(value), edge, SpecCell | SpecOther, isNotOther(value));
        
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
    
    void speculateRegExpObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecRegExpObject, isNotType(cell, RegExpObjectType));
    }
    
    void speculateRegExpObject(Edge edge)
    {
        speculateRegExpObject(edge, lowCell(edge));
    }
    
    void speculateString(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecString | ~SpecCell, isNotString(cell));
    }
    
    void speculateString(Edge edge)
    {
        speculateString(edge, lowCell(edge));
    }
    
    void speculateStringOrOther(Edge edge, LValue value)
    {
        LBasicBlock cellCase = m_out.newBlock();
        LBasicBlock notCellCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(isCell(value, provenType(edge)), unsure(cellCase), unsure(notCellCase));

        LBasicBlock lastNext = m_out.appendTo(cellCase, notCellCase);

        FTL_TYPE_CHECK(jsValueValue(value), edge, (~SpecCell) | SpecString, isNotString(value));

        m_out.jump(continuation);
        m_out.appendTo(notCellCase, continuation);

        FTL_TYPE_CHECK(jsValueValue(value), edge, SpecCell | SpecOther, isNotOther(value));

        m_out.jump(continuation);
        m_out.appendTo(continuation, lastNext);
    }
    
    void speculateStringOrOther(Edge edge)
    {
        speculateStringOrOther(edge, lowJSValue(edge, ManualOperandSpeculation));
    }
    
    void speculateStringIdent(Edge edge, LValue string, LValue stringImpl)
    {
        if (!m_interpreter.needsTypeCheck(edge, SpecStringIdent | ~SpecString))
            return;
        
        speculate(BadType, jsValueValue(string), edge.node(), m_out.isNull(stringImpl));
        speculate(
            BadType, jsValueValue(string), edge.node(),
            m_out.testIsZero32(
                m_out.load32(stringImpl, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIsAtomic())));
        m_interpreter.filter(edge, SpecStringIdent | ~SpecString);
    }
    
    void speculateStringIdent(Edge edge)
    {
        lowStringIdent(edge);
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
        
        LBasicBlock notString = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
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
        
        if (abstractStructure(edge).isSubsetOf(StructureSet(stringObjectStructure)))
            return;
        
        speculate(
            NotStringObject, noValue(), 0,
            m_out.notEqual(structureID, weakStructureID(stringObjectStructure)));
    }

    void speculateSymbol(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecSymbol | ~SpecCell, isNotSymbol(cell));
    }

    void speculateSymbol(Edge edge)
    {
        speculateSymbol(edge, lowCell(edge));
    }

    void speculateNonNullObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecObject, isNotObject(cell));
        if (masqueradesAsUndefinedWatchpointIsStillValid())
            return;
        
        speculate(
            BadType, jsValueValue(cell), edge.node(),
            m_out.testNonZero32(
                m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoFlags),
                m_out.constInt32(MasqueradesAsUndefined)));
    }
    
    void speculateNumber(Edge edge)
    {
        LValue value = lowJSValue(edge, ManualOperandSpeculation);
        FTL_TYPE_CHECK(jsValueValue(value), edge, SpecBytecodeNumber, isNotNumber(value));
    }
    
    void speculateRealNumber(Edge edge)
    {
        // Do an early return here because lowDouble() can create a lot of control flow.
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowJSValue(edge, ManualOperandSpeculation);
        LValue doubleValue = unboxDouble(value);
        
        LBasicBlock intCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        m_out.branch(
            m_out.doubleEqual(doubleValue, doubleValue),
            usually(continuation), rarely(intCase));
        
        LBasicBlock lastNext = m_out.appendTo(intCase, continuation);
        
        typeCheck(
            jsValueValue(value), m_node->child1(), SpecBytecodeRealNumber,
            isNotInt32(value, provenType(m_node->child1()) & ~SpecFullDouble));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
    }
    
    void speculateDoubleRepReal(Edge edge)
    {
        // Do an early return here because lowDouble() can create a lot of control flow.
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowDouble(edge);
        FTL_TYPE_CHECK(
            doubleValue(value), edge, SpecDoubleReal,
            m_out.doubleNotEqualOrUnordered(value, value));
    }
    
    void speculateDoubleRepMachineInt(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        doubleToStrictInt52(edge, lowDouble(edge));
    }
    
    void speculateBoolean(Edge edge)
    {
        lowBoolean(edge);
    }
    
    void speculateNotStringVar(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge, ~SpecStringVar))
            return;
        
        LValue value = lowJSValue(edge, ManualOperandSpeculation);
        
        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock isStringCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        m_out.branch(isCell(value, provenType(edge)), unsure(isCellCase), unsure(continuation));
        
        LBasicBlock lastNext = m_out.appendTo(isCellCase, isStringCase);
        m_out.branch(isString(value, provenType(edge)), unsure(isStringCase), unsure(continuation));
        
        m_out.appendTo(isStringCase, continuation);
        speculateStringIdent(edge, value, m_out.loadPtr(value, m_heaps.JSString_value));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void speculateNotCell(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowJSValue(edge, ManualOperandSpeculation);
        typeCheck(jsValueValue(value), edge, ~SpecCell, isCell(value));
    }
    
    void speculateOther(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowJSValue(edge, ManualOperandSpeculation);
        typeCheck(jsValueValue(value), edge, SpecOther, isNotOther(value));
    }
    
    void speculateMisc(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LValue value = lowJSValue(edge, ManualOperandSpeculation);
        typeCheck(jsValueValue(value), edge, SpecMisc, isNotMisc(value));
    }
    
    bool masqueradesAsUndefinedWatchpointIsStillValid()
    {
        return m_graph.masqueradesAsUndefinedWatchpointIsStillValid(m_node->origin.semantic);
    }
    
    LValue loadCellState(LValue base)
    {
        return m_out.load8ZeroExt32(base, m_heaps.JSCell_cellState);
    }

    void emitStoreBarrier(LValue base)
    {
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(
            m_out.notZero32(loadCellState(base)), usually(continuation), rarely(slowPath));

        LBasicBlock lastNext = m_out.appendTo(slowPath, continuation);

        // We emit the store barrier slow path lazily. In a lot of cases, this will never fire. And
        // when it does fire, it makes sense for us to generate this code using our JIT rather than
        // wasting B3's time optimizing it.
        lazySlowPath(
            [=] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                GPRReg baseGPR = locations[1].directGPR();

                return LazySlowPath::createGenerator(
                    [=] (CCallHelpers& jit, LazySlowPath::GenerationParams& params) {
                        RegisterSet usedRegisters = params.lazySlowPath->usedRegisters();
                        ScratchRegisterAllocator scratchRegisterAllocator(usedRegisters);
                        scratchRegisterAllocator.lock(baseGPR);

                        GPRReg scratch1 = scratchRegisterAllocator.allocateScratchGPR();
                        GPRReg scratch2 = scratchRegisterAllocator.allocateScratchGPR();

                        ScratchRegisterAllocator::PreservedState preservedState =
                            scratchRegisterAllocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

                        // We've already saved these, so when we make a slow path call, we don't have
                        // to save them again.
                        usedRegisters.exclude(RegisterSet(scratch1, scratch2));

                        WriteBarrierBuffer& writeBarrierBuffer = jit.vm()->heap.writeBarrierBuffer();
                        jit.load32(writeBarrierBuffer.currentIndexAddress(), scratch2);
                        CCallHelpers::Jump needToFlush = jit.branch32(
                            CCallHelpers::AboveOrEqual, scratch2,
                            CCallHelpers::TrustedImm32(writeBarrierBuffer.capacity()));

                        jit.add32(CCallHelpers::TrustedImm32(1), scratch2);
                        jit.store32(scratch2, writeBarrierBuffer.currentIndexAddress());

                        jit.move(CCallHelpers::TrustedImmPtr(writeBarrierBuffer.buffer()), scratch1);
                        jit.storePtr(
                            baseGPR,
                            CCallHelpers::BaseIndex(
                                scratch1, scratch2, CCallHelpers::ScalePtr,
                                static_cast<int32_t>(-sizeof(void*))));

                        scratchRegisterAllocator.restoreReusedRegistersByPopping(jit, preservedState);

                        params.doneJumps.append(jit.jump());

                        needToFlush.link(&jit);
                        callOperation(
                            usedRegisters, jit, params.lazySlowPath->callSiteIndex(),
                            params.exceptionJumps, operationFlushWriteBarrierBuffer, InvalidGPRReg,
                            baseGPR);
                        scratchRegisterAllocator.restoreReusedRegistersByPopping(jit, preservedState);
                        params.doneJumps.append(jit.jump());
                    });
            },
            base);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
    }

    template<typename... Args>
    LValue vmCall(LType type, LValue function, Args... args)
    {
        callPreflight();
        LValue result = m_out.call(type, function, args...);
        callCheck();
        return result;
    }
    
    void callPreflight(CodeOrigin codeOrigin)
    {
        CallSiteIndex callSiteIndex = m_ftlState.jitCode->common.addCodeOrigin(codeOrigin);
        m_out.store32(
            m_out.constInt32(callSiteIndex.bits()),
            tagFor(JSStack::ArgumentCount));
    }

    void callPreflight()
    {
        callPreflight(codeOriginDescriptionOfCallSite());
    }

    CodeOrigin codeOriginDescriptionOfCallSite() const
    {
        CodeOrigin codeOrigin = m_node->origin.semantic;
        if (m_node->op() == TailCallInlinedCaller
            || m_node->op() == TailCallVarargsInlinedCaller
            || m_node->op() == TailCallForwardVarargsInlinedCaller) {
            // This case arises when you have a situation like this:
            // foo makes a call to bar, bar is inlined in foo. bar makes a call
            // to baz and baz is inlined in bar. And then baz makes a tail-call to jaz,
            // and jaz is inlined in baz. We want the callframe for jaz to appear to 
            // have caller be bar.
            codeOrigin = *codeOrigin.inlineCallFrame->getCallerSkippingTailCalls();
        }

        return codeOrigin;
    }
    
    void callCheck()
    {
        if (Options::useExceptionFuzz())
            m_out.call(m_out.voidType, m_out.operation(operationExceptionFuzz), m_callFrame);
        
        LValue exception = m_out.load64(m_out.absolute(vm().addressOfException()));
        LValue hadException = m_out.notZero64(exception);

        CodeOrigin opCatchOrigin;
        HandlerInfo* exceptionHandler;
        if (m_graph.willCatchExceptionInMachineFrame(m_origin.forExit, opCatchOrigin, exceptionHandler)) {
            bool exitOK = true;
            bool isExceptionHandler = true;
            appendOSRExit(
                ExceptionCheck, noValue(), nullptr, hadException,
                m_origin.withForExitAndExitOK(opCatchOrigin, exitOK), isExceptionHandler);
            return;
        }

        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(
            hadException, rarely(m_handleExceptions), usually(continuation));

        m_out.appendTo(continuation);
    }

    RefPtr<PatchpointExceptionHandle> preparePatchpointForExceptions(PatchpointValue* value)
    {
        CodeOrigin opCatchOrigin;
        HandlerInfo* exceptionHandler;
        bool willCatchException = m_graph.willCatchExceptionInMachineFrame(m_origin.forExit, opCatchOrigin, exceptionHandler);
        if (!willCatchException)
            return PatchpointExceptionHandle::defaultHandle(m_ftlState);

        if (verboseCompilationEnabled()) {
            dataLog("    Patchpoint exception OSR exit #", m_ftlState.jitCode->osrExitDescriptors.size(), " with availability: ", availabilityMap(), "\n");
            if (!m_availableRecoveries.isEmpty())
                dataLog("        Available recoveries: ", listDump(m_availableRecoveries), "\n");
        }

        bool exitOK = true;
        NodeOrigin origin = m_origin.withForExitAndExitOK(opCatchOrigin, exitOK);

        OSRExitDescriptor* exitDescriptor = appendOSRExitDescriptor(noValue(), nullptr);

        // Compute the offset into the StackmapGenerationParams where we will find the exit arguments
        // we are about to append. We need to account for both the children we've already added, and
        // for the possibility of a result value if the patchpoint is not void.
        unsigned offset = value->numChildren();
        if (value->type() != Void)
            offset++;

        // Use LateColdAny to ensure that the stackmap arguments interfere with the patchpoint's
        // result and with any late-clobbered registers.
        value->appendVectorWithRep(
            buildExitArguments(exitDescriptor, opCatchOrigin, noValue()),
            ValueRep::LateColdAny);

        return PatchpointExceptionHandle::create(
            m_ftlState, exitDescriptor, origin, offset, *exceptionHandler);
    }

    LBasicBlock lowBlock(DFG::BasicBlock* block)
    {
        return m_blocks.get(block);
    }

    OSRExitDescriptor* appendOSRExitDescriptor(FormattedValue lowValue, Node* highValue)
    {
        return &m_ftlState.jitCode->osrExitDescriptors.alloc(
            lowValue.format(), m_graph.methodOfGettingAValueProfileFor(highValue),
            availabilityMap().m_locals.numberOfArguments(),
            availabilityMap().m_locals.numberOfLocals());
    }
    
    void appendOSRExit(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition, 
        NodeOrigin origin, bool isExceptionHandler = false)
    {
        if (verboseCompilationEnabled()) {
            dataLog("    OSR exit #", m_ftlState.jitCode->osrExitDescriptors.size(), " with availability: ", availabilityMap(), "\n");
            if (!m_availableRecoveries.isEmpty())
                dataLog("        Available recoveries: ", listDump(m_availableRecoveries), "\n");
        }

        DFG_ASSERT(m_graph, m_node, origin.exitOK);
        
        if (doOSRExitFuzzing() && !isExceptionHandler) {
            LValue numberOfFuzzChecks = m_out.add(
                m_out.load32(m_out.absolute(&g_numberOfOSRExitFuzzChecks)),
                m_out.int32One);
            
            m_out.store32(numberOfFuzzChecks, m_out.absolute(&g_numberOfOSRExitFuzzChecks));
            
            if (unsigned atOrAfter = Options::fireOSRExitFuzzAtOrAfter()) {
                failCondition = m_out.bitOr(
                    failCondition,
                    m_out.aboveOrEqual(numberOfFuzzChecks, m_out.constInt32(atOrAfter)));
            }
            if (unsigned at = Options::fireOSRExitFuzzAt()) {
                failCondition = m_out.bitOr(
                    failCondition,
                    m_out.equal(numberOfFuzzChecks, m_out.constInt32(at)));
            }
        }

        if (failCondition == m_out.booleanFalse)
            return;

        blessSpeculation(
            m_out.speculate(failCondition), kind, lowValue, highValue, origin);
    }

    void blessSpeculation(CheckValue* value, ExitKind kind, FormattedValue lowValue, Node* highValue, NodeOrigin origin)
    {
        OSRExitDescriptor* exitDescriptor = appendOSRExitDescriptor(lowValue, highValue);
        
        value->appendColdAnys(buildExitArguments(exitDescriptor, origin.forExit, lowValue));

        State* state = &m_ftlState;
        value->setGenerator(
            [=] (CCallHelpers& jit, const B3::StackmapGenerationParams& params) {
                exitDescriptor->emitOSRExit(
                    *state, kind, origin, jit, params, 0);
            });
    }

    StackmapArgumentList buildExitArguments(
        OSRExitDescriptor* exitDescriptor, CodeOrigin exitOrigin, FormattedValue lowValue,
        unsigned offsetOfExitArgumentsInStackmapLocations = 0)
    {
        StackmapArgumentList result;
        buildExitArguments(
            exitDescriptor, exitOrigin, result, lowValue, offsetOfExitArgumentsInStackmapLocations);
        return result;
    }
    
    void buildExitArguments(
        OSRExitDescriptor* exitDescriptor, CodeOrigin exitOrigin, StackmapArgumentList& arguments, FormattedValue lowValue,
        unsigned offsetOfExitArgumentsInStackmapLocations = 0)
    {
        if (!!lowValue)
            arguments.append(lowValue.value());
        
        AvailabilityMap availabilityMap = this->availabilityMap();
        availabilityMap.pruneByLiveness(m_graph, exitOrigin);
        
        HashMap<Node*, ExitTimeObjectMaterialization*> map;
        availabilityMap.forEachAvailability(
            [&] (Availability availability) {
                if (!availability.shouldUseNode())
                    return;
                
                Node* node = availability.node();
                if (!node->isPhantomAllocation())
                    return;
                
                auto result = map.add(node, nullptr);
                if (result.isNewEntry) {
                    result.iterator->value =
                        exitDescriptor->m_materializations.add(node->op(), node->origin.semantic);
                }
            });
        
        for (unsigned i = 0; i < exitDescriptor->m_values.size(); ++i) {
            int operand = exitDescriptor->m_values.operandForIndex(i);
            
            Availability availability = availabilityMap.m_locals[i];
            
            if (Options::validateFTLOSRExitLiveness()) {
                DFG_ASSERT(
                    m_graph, m_node,
                    (!(availability.isDead() && m_graph.isLiveInBytecode(VirtualRegister(operand), exitOrigin))) || m_graph.m_plan.mode == FTLForOSREntryMode);
            }
            ExitValue exitValue = exitValueForAvailability(arguments, map, availability);
            if (exitValue.hasIndexInStackmapLocations())
                exitValue.adjustStackmapLocationsIndexByOffset(offsetOfExitArgumentsInStackmapLocations);
            exitDescriptor->m_values[i] = exitValue;
        }
        
        for (auto heapPair : availabilityMap.m_heap) {
            Node* node = heapPair.key.base();
            ExitTimeObjectMaterialization* materialization = map.get(node);
            ExitValue exitValue = exitValueForAvailability(arguments, map, heapPair.value);
            if (exitValue.hasIndexInStackmapLocations())
                exitValue.adjustStackmapLocationsIndexByOffset(offsetOfExitArgumentsInStackmapLocations);
            materialization->add(
                heapPair.key.descriptor(),
                exitValue);
        }
        
        if (verboseCompilationEnabled()) {
            dataLog("        Exit values: ", exitDescriptor->m_values, "\n");
            if (!exitDescriptor->m_materializations.isEmpty()) {
                dataLog("        Materializations: \n");
                for (ExitTimeObjectMaterialization* materialization : exitDescriptor->m_materializations)
                    dataLog("            ", pointerDump(materialization), "\n");
            }
        }
    }

    ExitValue exitValueForAvailability(
        StackmapArgumentList& arguments, const HashMap<Node*, ExitTimeObjectMaterialization*>& map,
        Availability availability)
    {
        FlushedAt flush = availability.flushedAt();
        switch (flush.format()) {
        case DeadFlush:
        case ConflictingFlush:
            if (availability.hasNode())
                return exitValueForNode(arguments, map, availability.node());
            
            // This means that the value is dead. It could be dead in bytecode or it could have
            // been killed by our DCE, which can sometimes kill things even if they were live in
            // bytecode.
            return ExitValue::dead();

        case FlushedJSValue:
        case FlushedCell:
        case FlushedBoolean:
            return ExitValue::inJSStack(flush.virtualRegister());
                
        case FlushedInt32:
            return ExitValue::inJSStackAsInt32(flush.virtualRegister());
                
        case FlushedInt52:
            return ExitValue::inJSStackAsInt52(flush.virtualRegister());
                
        case FlushedDouble:
            return ExitValue::inJSStackAsDouble(flush.virtualRegister());
        }
        
        DFG_CRASH(m_graph, m_node, "Invalid flush format");
        return ExitValue::dead();
    }
    
    ExitValue exitValueForNode(
        StackmapArgumentList& arguments, const HashMap<Node*, ExitTimeObjectMaterialization*>& map,
        Node* node)
    {
        // NOTE: In FTL->B3, we cannot generate code here, because m_output is positioned after the
        // stackmap value. Like all values, the stackmap value cannot use a child that is defined after
        // it.
        
        ASSERT(node->shouldGenerate());
        ASSERT(node->hasResult());

        if (node) {
            switch (node->op()) {
            case BottomValue:
                // This might arise in object materializations. I actually doubt that it would,
                // but it seems worthwhile to be conservative.
                return ExitValue::dead();
                
            case JSConstant:
            case Int52Constant:
            case DoubleConstant:
                return ExitValue::constant(node->asJSValue());
                
            default:
                if (node->isPhantomAllocation())
                    return ExitValue::materializeNewObject(map.get(node));
                break;
            }
        }
        
        for (unsigned i = 0; i < m_availableRecoveries.size(); ++i) {
            AvailableRecovery recovery = m_availableRecoveries[i];
            if (recovery.node() != node)
                continue;
            ExitValue result = ExitValue::recovery(
                recovery.opcode(), arguments.size(), arguments.size() + 1,
                recovery.format());
            arguments.append(recovery.left());
            arguments.append(recovery.right());
            return result;
        }
        
        LoweredNodeValue value = m_int32Values.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatInt32, value.value());
        
        value = m_int52Values.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatInt52, value.value());
        
        value = m_strictInt52Values.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatStrictInt52, value.value());
        
        value = m_booleanValues.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatBoolean, value.value());
        
        value = m_jsValueValues.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatJS, value.value());
        
        value = m_doubleValues.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatDouble, value.value());

        DFG_CRASH(m_graph, m_node, toCString("Cannot find value for node: ", node).data());
        return ExitValue::dead();
    }

    ExitValue exitArgument(StackmapArgumentList& arguments, DataFormat format, LValue value)
    {
        ExitValue result = ExitValue::exitArgument(ExitArgument(format, arguments.size()));
        arguments.append(value);
        return result;
    }

    ExitValue exitValueForTailCall(StackmapArgumentList& arguments, Node* node)
    {
        ASSERT(node->shouldGenerate());
        ASSERT(node->hasResult());

        switch (node->op()) {
        case JSConstant:
        case Int52Constant:
        case DoubleConstant:
            return ExitValue::constant(node->asJSValue());

        default:
            break;
        }

        LoweredNodeValue value = m_jsValueValues.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatJS, value.value());

        value = m_int32Values.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatJS, boxInt32(value.value()));

        value = m_booleanValues.get(node);
        if (isValid(value))
            return exitArgument(arguments, DataFormatJS, boxBoolean(value.value()));

        // Doubles and Int52 have been converted by ValueRep()
        DFG_CRASH(m_graph, m_node, toCString("Cannot find value for node: ", node).data());
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
        Node* node, RecoveryOpcode opcode, LValue left, LValue right, DataFormat format)
    {
        m_availableRecoveries.append(AvailableRecovery(node, opcode, left, right, format));
    }
    
    void addAvailableRecovery(
        Edge edge, RecoveryOpcode opcode, LValue left, LValue right, DataFormat format)
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
        
        DFG_CRASH(m_graph, m_node, "Corrupt int52 kind");
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
        if (!m_graph.m_dominators->dominates(value.block(), m_highBlock))
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
        LValue tableBase = m_out.loadPtr(
            m_out.absolute(vm().heap.structureIDTable().base()));
        TypedPointer address = m_out.baseIndex(
            m_heaps.structureTable, tableBase, m_out.zeroExtPtr(tableIndex));
        return m_out.loadPtr(address);
    }

    LValue weakPointer(JSCell* pointer)
    {
        addWeakReference(pointer);
        return m_out.constIntPtr(pointer);
    }

    LValue weakStructureID(Structure* structure)
    {
        addWeakReference(structure);
        return m_out.constInt32(structure->id());
    }
    
    LValue weakStructure(Structure* structure)
    {
        return weakPointer(structure);
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
    
    AbstractValue abstractValue(Node* node)
    {
        return m_state.forNode(node);
    }
    AbstractValue abstractValue(Edge edge)
    {
        return abstractValue(edge.node());
    }
    
    SpeculatedType provenType(Node* node)
    {
        return abstractValue(node).m_type;
    }
    SpeculatedType provenType(Edge edge)
    {
        return provenType(edge.node());
    }
    
    JSValue provenValue(Node* node)
    {
        return abstractValue(node).m_value;
    }
    JSValue provenValue(Edge edge)
    {
        return provenValue(edge.node());
    }
    
    StructureAbstractValue abstractStructure(Node* node)
    {
        return abstractValue(node).m_structure;
    }
    StructureAbstractValue abstractStructure(Edge edge)
    {
        return abstractStructure(edge.node());
    }
    
#if ENABLE(MASM_PROBE)
    void probe(std::function<void (CCallHelpers::ProbeContext*)> probeFunc)
    {
        UNUSED_PARAM(probeFunc);
    }
#endif

    void crash()
    {
        crash(m_highBlock->index, m_node->index());
    }
    void crash(BlockIndex blockIndex, unsigned nodeIndex)
    {
#if ASSERT_DISABLED
        m_out.call(m_out.voidType, m_out.operation(ftlUnreachable));
        UNUSED_PARAM(blockIndex);
        UNUSED_PARAM(nodeIndex);
#else
        m_out.call(
            m_out.voidType,
            m_out.constIntPtr(ftlUnreachable),
            m_out.constIntPtr(codeBlock()), m_out.constInt32(blockIndex),
            m_out.constInt32(nodeIndex));
#endif
        m_out.unreachable();
    }

    AvailabilityMap& availabilityMap() { return m_availabilityCalculator.m_availability; }
    
    VM& vm() { return m_graph.m_vm; }
    CodeBlock* codeBlock() { return m_graph.m_codeBlock; }
    
    Graph& m_graph;
    State& m_ftlState;
    AbstractHeapRepository m_heaps;
    Output m_out;
    Procedure& m_proc;
    
    LBasicBlock m_prologue;
    LBasicBlock m_handleExceptions;
    HashMap<DFG::BasicBlock*, LBasicBlock> m_blocks;
    
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
    
    // This is a bit of a hack. It prevents B3 from having to do CSE on loading of arguments.
    // It's nice to have these optimizations on our end because we can guarantee them a bit better.
    // Probably also saves B3 compile time.
    HashMap<Node*, LValue> m_loadedArgumentValues;
    
    HashMap<Node*, LValue> m_phis;
    
    LocalOSRAvailabilityCalculator m_availabilityCalculator;
    
    Vector<AvailableRecovery, 3> m_availableRecoveries;
    
    InPlaceAbstractState m_state;
    AbstractInterpreter<InPlaceAbstractState> m_interpreter;
    DFG::BasicBlock* m_highBlock;
    DFG::BasicBlock* m_nextHighBlock;
    LBasicBlock m_nextLowBlock;

    NodeOrigin m_origin;
    unsigned m_nodeIndex;
    Node* m_node;
};

} // anonymous namespace

void lowerDFGToB3(State& state)
{
    LowerDFGToB3 lowering(state);
    lowering.lower();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

