/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "AirCode.h"
#include "AirGenerationContext.h"
#include "AllowMacroScratchRegisterUsage.h"
#include "AllowMacroScratchRegisterUsageIf.h"
#include "AtomicsObject.h"
#include "B3CheckValue.h"
#include "B3FenceValue.h"
#include "B3PatchpointValue.h"
#include "B3SlotBaseValue.h"
#include "B3StackmapGenerationParams.h"
#include "B3ValueInlines.h"
#include "CallFrameShuffler.h"
#include "CodeBlockWithJITType.h"
#include "DFGAbstractInterpreterInlines.h"
#include "DFGCapabilities.h"
#include "DFGDominators.h"
#include "DFGInPlaceAbstractState.h"
#include "DFGMayExit.h"
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
#include "FTLSnippetParams.h"
#include "FTLThunks.h"
#include "FTLWeightedTarget.h"
#include "JITAddGenerator.h"
#include "JITBitAndGenerator.h"
#include "JITBitOrGenerator.h"
#include "JITBitXorGenerator.h"
#include "JITDivGenerator.h"
#include "JITInlineCacheGenerator.h"
#include "JITLeftShiftGenerator.h"
#include "JITMathIC.h"
#include "JITMulGenerator.h"
#include "JITRightShiftGenerator.h"
#include "JITSubGenerator.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSCInlines.h"
#include "JSGeneratorFunction.h"
#include "JSImmutableButterfly.h"
#include "JSLexicalEnvironment.h"
#include "JSMap.h"
#include "OperandsInlines.h"
#include "RegExpObject.h"
#include "ScopedArguments.h"
#include "ScopedArgumentsTable.h"
#include "ScratchRegisterAllocator.h"
#include "SetupVarargsFrame.h"
#include "ShadowChicken.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include "VirtualRegister.h"
#include "Watchdog.h"
#include <atomic>
#include <wtf/Box.h>
#include <wtf/Gigacage.h>
#include <wtf/RecursableLambda.h>
#include <wtf/StdUnorderedSet.h>

#undef RELEASE_ASSERT
#define RELEASE_ASSERT(assertion) do { \
    if (!(assertion)) { \
        WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion); \
        CRASH(); \
    } \
} while (0)

namespace JSC { namespace FTL {

using namespace B3;
using namespace DFG;

namespace {

std::atomic<int> compileCounter;

#if !ASSERT_DISABLED
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
        , m_availabilityCalculator(m_graph)
        , m_state(state.graph)
        , m_interpreter(state.graph, m_state)
        , m_indexMaskingMode(Options::enableSpectreMitigations() ?  IndexMaskingEnabled : IndexMaskingDisabled)
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

        {
            m_proc.setNumEntrypoints(m_graph.m_numberOfEntrypoints);
            CodeBlock* codeBlock = m_graph.m_codeBlock;

            Ref<B3::Air::PrologueGenerator> catchPrologueGenerator = createSharedTask<B3::Air::PrologueGeneratorFunction>(
                [codeBlock] (CCallHelpers& jit, B3::Air::Code& code) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);
                    jit.addPtr(CCallHelpers::TrustedImm32(-code.frameSize()), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
                    if (Options::zeroStackFrame())
                        jit.clearStackFrame(GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister, GPRInfo::regT0, code.frameSize());

                    jit.emitSave(code.calleeSaveRegisterAtOffsetList());
                    jit.emitPutToCallFrameHeader(codeBlock, CallFrameSlot::codeBlock);
                });

            for (unsigned catchEntrypointIndex : m_graph.m_entrypointIndexToCatchBytecodeOffset.keys()) {
                RELEASE_ASSERT(catchEntrypointIndex != 0);
                m_proc.code().setPrologueForEntrypoint(catchEntrypointIndex, catchPrologueGenerator.copyRef());
            }

            if (m_graph.m_maxLocalsForCatchOSREntry) {
                uint32_t numberOfLiveLocals = std::max(*m_graph.m_maxLocalsForCatchOSREntry, 1u); // Make sure we always allocate a non-null catchOSREntryBuffer.
                m_ftlState.jitCode->common.catchOSREntryBuffer = m_graph.m_vm.scratchBufferForSize(sizeof(JSValue) * numberOfLiveLocals);
            }
        }
        
        m_graph.ensureSSADominators();

        if (verboseCompilationEnabled())
            dataLog("Function ready, beginning lowering.\n");

        m_out.initialize(m_heaps);

        // We use prologue frequency for all of the initialization code.
        m_out.setFrequency(1);
        
        bool hasMultipleEntrypoints = m_graph.m_numberOfEntrypoints > 1;
    
        LBasicBlock prologue = m_out.newBlock();
        LBasicBlock callEntrypointArgumentSpeculations = hasMultipleEntrypoints ? m_out.newBlock() : nullptr;
        m_handleExceptions = m_out.newBlock();

        for (BlockIndex blockIndex = 0; blockIndex < m_graph.numBlocks(); ++blockIndex) {
            m_highBlock = m_graph.block(blockIndex);
            if (!m_highBlock)
                continue;
            m_out.setFrequency(m_highBlock->executionCount);
            m_blocks.add(m_highBlock, m_out.newBlock());
        }

        // Back to prologue frequency for any bocks that get sneakily created in the initialization code.
        m_out.setFrequency(1);
        
        m_out.appendTo(prologue, hasMultipleEntrypoints ? callEntrypointArgumentSpeculations : m_handleExceptions);
        m_out.initializeConstants(m_proc, prologue);
        createPhiVariables();

        size_t sizeOfCaptured = sizeof(JSValue) * m_graph.m_nextMachineLocal;
        B3::SlotBaseValue* capturedBase = m_out.lockedStackSlot(sizeOfCaptured);
        m_captured = m_out.add(capturedBase, m_out.constIntPtr(sizeOfCaptured));
        state->capturedValue = capturedBase->slot();

        auto preOrder = m_graph.blocksInPreOrder();

        m_callFrame = m_out.framePointer();
        m_tagTypeNumber = m_out.constInt64(TagTypeNumber);
        m_tagMask = m_out.constInt64(TagMask);

        // Make sure that B3 knows that we really care about the mask registers. This forces the
        // constants to be materialized in registers.
        m_proc.addFastConstant(m_tagTypeNumber->key());
        m_proc.addFastConstant(m_tagMask->key());
        
        // We don't want the CodeBlock to have a weak pointer to itself because
        // that would cause it to always get collected.
        m_out.storePtr(m_out.constIntPtr(bitwise_cast<intptr_t>(codeBlock())), addressFor(CallFrameSlot::codeBlock));

        VM* vm = &this->vm();

        // Stack Overflow Check.
        unsigned exitFrameSize = m_graph.requiredRegisterCountForExit() * sizeof(Register);
        MacroAssembler::AbsoluteAddress addressOfStackLimit(vm->addressOfSoftStackLimit());
        PatchpointValue* stackOverflowHandler = m_out.patchpoint(Void);
        CallSiteIndex callSiteIndex = callSiteIndexForCodeOrigin(m_ftlState, CodeOrigin(0));
        stackOverflowHandler->appendSomeRegister(m_callFrame);
        stackOverflowHandler->clobber(RegisterSet::macroScratchRegisters());
        stackOverflowHandler->numGPScratchRegisters = 1;
        stackOverflowHandler->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                GPRReg fp = params[0].gpr();
                GPRReg scratch = params.gpScratch(0);

                unsigned ftlFrameSize = params.proc().frameSize();
                unsigned maxFrameSize = std::max(exitFrameSize, ftlFrameSize);

                jit.addPtr(MacroAssembler::TrustedImm32(-maxFrameSize), fp, scratch);
                MacroAssembler::JumpList stackOverflow;
                if (UNLIKELY(maxFrameSize > Options::reservedZoneSize()))
                    stackOverflow.append(jit.branchPtr(MacroAssembler::Above, scratch, fp));
                stackOverflow.append(jit.branchPtr(MacroAssembler::Above, addressOfStackLimit, scratch));

                params.addLatePath([=] (CCallHelpers& jit) {
                    AllowMacroScratchRegisterUsage allowScratch(jit);

                    stackOverflow.link(&jit);
                    
                    // FIXME: We would not have to do this if the stack check was part of the Air
                    // prologue. Then, we would know that there is no way for the callee-saves to
                    // get clobbered.
                    // https://bugs.webkit.org/show_bug.cgi?id=172456
                    jit.emitRestore(params.proc().calleeSaveRegisterAtOffsetList());
                    
                    jit.store32(
                        MacroAssembler::TrustedImm32(callSiteIndex.bits()),
                        CCallHelpers::tagFor(VirtualRegister(CallFrameSlot::argumentCount)));
                    jit.copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm->topEntryFrame);

                    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
                    jit.move(CCallHelpers::TrustedImmPtr(jit.codeBlock()), GPRInfo::argumentGPR1);
                    CCallHelpers::Call throwCall = jit.call(OperationPtrTag);

                    jit.move(CCallHelpers::TrustedImmPtr(vm), GPRInfo::argumentGPR0);
                    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
                    CCallHelpers::Call lookupExceptionHandlerCall = jit.call(OperationPtrTag);
                    jit.jumpToExceptionHandler(*vm);

                    jit.addLinkTask(
                        [=] (LinkBuffer& linkBuffer) {
                            linkBuffer.link(throwCall, FunctionPtr<OperationPtrTag>(operationThrowStackOverflowError));
                            linkBuffer.link(lookupExceptionHandlerCall, FunctionPtr<OperationPtrTag>(lookupExceptionHandlerFromCallerFrame));
                    });
                });
            });

        LBasicBlock firstDFGBasicBlock = lowBlock(m_graph.block(0));

        {
            if (hasMultipleEntrypoints) {
                Vector<LBasicBlock> successors(m_graph.m_numberOfEntrypoints);
                successors[0] = callEntrypointArgumentSpeculations;
                for (unsigned i = 1; i < m_graph.m_numberOfEntrypoints; ++i) {
                    // Currently, the only other entrypoint is an op_catch entrypoint.
                    // We do OSR entry at op_catch, and we prove argument formats before
                    // jumping to FTL code, so we don't need to check argument types here
                    // for these entrypoints.
                    successors[i] = firstDFGBasicBlock;
                }
                
                m_out.entrySwitch(successors);
                m_out.appendTo(callEntrypointArgumentSpeculations, m_handleExceptions);
            }

            m_node = nullptr;
            m_origin = NodeOrigin(CodeOrigin(0), CodeOrigin(0), true);

            // Check Arguments.
            availabilityMap().clear();
            availabilityMap().m_locals = Operands<Availability>(codeBlock()->numParameters(), 0);
            for (unsigned i = codeBlock()->numParameters(); i--;) {
                availabilityMap().m_locals.argument(i) =
                    Availability(FlushedAt(FlushedJSValue, virtualRegisterForArgument(i)));
            }

            for (unsigned i = codeBlock()->numParameters(); i--;) {
                MethodOfGettingAValueProfile profile(&m_graph.m_profiledBlock->valueProfileForArgument(i));
                VirtualRegister operand = virtualRegisterForArgument(i);
                LValue jsValue = m_out.load64(addressFor(operand));
                
                switch (m_graph.m_argumentFormats[0][i]) {
                case FlushedInt32:
                    speculate(BadType, jsValueValue(jsValue), profile, isNotInt32(jsValue));
                    break;
                case FlushedBoolean:
                    speculate(BadType, jsValueValue(jsValue), profile, isNotBoolean(jsValue));
                    break;
                case FlushedCell:
                    speculate(BadType, jsValueValue(jsValue), profile, isNotCell(jsValue));
                    break;
                case FlushedJSValue:
                    break;
                default:
                    DFG_CRASH(m_graph, nullptr, "Bad flush format for argument");
                    break;
                }
            }
            m_out.jump(firstDFGBasicBlock);
        }


        m_out.appendTo(m_handleExceptions, firstDFGBasicBlock);
        Box<CCallHelpers::Label> exceptionHandler = state->exceptionHandler;
        m_out.patchpoint(Void)->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams&) {
                CCallHelpers::Jump jump = jit.jump();
                jit.addLinkTask(
                    [=] (LinkBuffer& linkBuffer) {
                        linkBuffer.link(jump, linkBuffer.locationOf<ExceptionHandlerPtrTag>(*exceptionHandler));
                    });
            });
        m_out.unreachable();

        for (DFG::BasicBlock* block : preOrder)
            compileBlock(block);

        // Make sure everything is decorated. This does a bunch of deferred decorating. This has
        // to happen last because our abstract heaps are generated lazily. They have to be
        // generated lazily because we have an infinite number of numbered, indexed, and
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
                    type = Double;
                    break;
                case NodeResultInt32:
                    type = Int32;
                    break;
                case NodeResultInt52:
                    type = Int64;
                    break;
                case NodeResultBoolean:
                    type = Int32;
                    break;
                case NodeResultJS:
                    type = Int64;
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
            crash(m_highBlock, nullptr);
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
            if (m_graph.m_ssaDominators->dominates(m_highBlock, target)) {
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
        
        m_availableRecoveries.shrink(0);
        
        m_interpreter.startExecuting();
        m_interpreter.executeKnownEdgeTypes(m_node);
        
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
        case ExtractCatchLocal:
            compileExtractCatchLocal();
            break;
        case ClearCatchLocals:
            compileClearCatchLocals();
            break;
        case GetStack:
            compileGetStack();
            break;
        case PutStack:
            compilePutStack();
            break;
        case DFG::Check:
        case CheckVarargs:
            compileNoOp();
            break;
        case ToObject:
        case CallObjectConstructor:
            compileToObjectOrCallObjectConstructor();
            break;
        case ToThis:
            compileToThis();
            break;
        case ValueNegate:
            compileValueNegate();
            break;
        case ValueAdd:
            compileValueAdd();
            break;
        case ValueSub:
            compileValueSub();
            break;
        case ValueMul:
            compileValueMul();
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
        case ValueDiv:
            compileValueDiv();
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
        case ArithFRound:
            compileArithFRound();
            break;
        case ArithNegate:
            compileArithNegate();
            break;
        case ArithUnary:
            compileArithUnary();
            break;
        case ArithBitNot:
            compileArithBitNot();
            break;
        case ValueBitAnd:
            compileValueBitAnd();
            break;
        case ArithBitAnd:
            compileArithBitAnd();
            break;
        case ValueBitOr:
            compileValueBitOr();
            break;
        case ArithBitOr:
            compileArithBitOr();
            break;
        case ArithBitXor:
            compileArithBitXor();
            break;
        case ValueBitXor:
            compileValueBitXor();
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
        case CheckStructureOrEmpty:
            compileCheckStructureOrEmpty();
            break;
        case CheckCell:
            compileCheckCell();
            break;
        case CheckNotEmpty:
            compileCheckNotEmpty();
            break;
        case AssertNotEmpty:
            compileAssertNotEmpty();
            break;
        case CheckBadCell:
            compileCheckBadCell();
            break;
        case CheckStringIdent:
            compileCheckStringIdent();
            break;
        case GetExecutable:
            compileGetExecutable();
            break;
        case Arrayify:
        case ArrayifyToStructure:
            compileArrayify();
            break;
        case PutStructure:
            compilePutStructure();
            break;
        case TryGetById:
            compileGetById(AccessType::TryGet);
            break;
        case GetById:
        case GetByIdFlush:
            compileGetById(AccessType::Get);
            break;
        case GetByIdWithThis:
            compileGetByIdWithThis();
            break;
        case GetByIdDirect:
        case GetByIdDirectFlush:
            compileGetById(AccessType::GetDirect);
            break;
        case InById:
            compileInById();
            break;
        case InByVal:
            compileInByVal();
            break;
        case HasOwnProperty:
            compileHasOwnProperty();
            break;
        case PutById:
        case PutByIdDirect:
        case PutByIdFlush:
            compilePutById();
            break;
        case PutByIdWithThis:
            compilePutByIdWithThis();
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
        case DeleteById:
            compileDeleteById();
            break;
        case DeleteByVal:
            compileDeleteByVal();
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
        case GetVectorLength:
            compileGetVectorLength();
            break;
        case CheckInBounds:
            compileCheckInBounds();
            break;
        case GetByVal:
            compileGetByVal();
            break;
        case GetMyArgumentByVal:
        case GetMyArgumentByValOutOfBounds:
            compileGetMyArgumentByVal();
            break;
        case GetByValWithThis:
            compileGetByValWithThis();
            break;
        case PutByVal:
        case PutByValAlias:
        case PutByValDirect:
            compilePutByVal();
            break;
        case PutByValWithThis:
            compilePutByValWithThis();
            break;
        case AtomicsAdd:
        case AtomicsAnd:
        case AtomicsCompareExchange:
        case AtomicsExchange:
        case AtomicsLoad:
        case AtomicsOr:
        case AtomicsStore:
        case AtomicsSub:
        case AtomicsXor:
            compileAtomicsReadModifyWrite();
            break;
        case AtomicsIsLockFree:
            compileAtomicsIsLockFree();
            break;
        case DefineDataProperty:
            compileDefineDataProperty();
            break;
        case DefineAccessorProperty:
            compileDefineAccessorProperty();
            break;
        case ArrayPush:
            compileArrayPush();
            break;
        case ArrayPop:
            compileArrayPop();
            break;
        case ArraySlice:
            compileArraySlice();
            break;
        case ArrayIndexOf:
            compileArrayIndexOf();
            break;
        case CreateActivation:
            compileCreateActivation();
            break;
        case PushWithScope:
            compilePushWithScope();
            break;
        case NewFunction:
        case NewGeneratorFunction:
        case NewAsyncGeneratorFunction:
        case NewAsyncFunction:
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
        case ObjectCreate:
            compileObjectCreate();
            break;
        case ObjectKeys:
            compileObjectKeys();
            break;
        case NewObject:
            compileNewObject();
            break;
        case NewStringObject:
            compileNewStringObject();
            break;
        case NewSymbol:
            compileNewSymbol();
            break;
        case NewArray:
            compileNewArray();
            break;
        case NewArrayWithSpread:
            compileNewArrayWithSpread();
            break;
        case CreateThis:
            compileCreateThis();
            break;
        case Spread:
            compileSpread();
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
        case GetPrototypeOf:
            compileGetPrototypeOf();
            break;
        case AllocatePropertyStorage:
            compileAllocatePropertyStorage();
            break;
        case ReallocatePropertyStorage:
            compileReallocatePropertyStorage();
            break;
        case NukeStructureAndSetButterfly:
            compileNukeStructureAndSetButterfly();
            break;
        case ToNumber:
            compileToNumber();
            break;
        case ToString:
        case CallStringConstructor:
        case StringValueOf:
            compileToStringOrCallStringConstructorOrStringValueOf();
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
        case MatchStructure:
            compileMatchStructure();
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
        case SetCallee:
            compileSetCallee();
            break;
        case GetArgumentCountIncludingThis:
            compileGetArgumentCountIncludingThis();
            break;
        case SetArgumentCountIncludingThis:
            compileSetArgumentCountIncludingThis();
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
        case GetGlobalThis:
            compileGetGlobalThis();
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
        case GetArgument:
            compileGetArgument();
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
        case CompareBelow:
            compileCompareBelow();
            break;
        case CompareBelowEq:
            compileCompareBelowEq();
            break;
        case CompareEqPtr:
            compileCompareEqPtr();
            break;
        case SameValue:
            compileSameValue();
            break;
        case LogicalNot:
            compileLogicalNot();
            break;
        case Call:
        case TailCallInlinedCaller:
        case Construct:
            compileCallOrConstruct();
            break;
        case DirectCall:
        case DirectTailCallInlinedCaller:
        case DirectConstruct:
        case DirectTailCall:
            compileDirectCallOrConstruct();
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
        case CallEval:
            compileCallEval();
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
        case DFG::EntrySwitch:
            compileEntrySwitch();
            break;
        case DFG::Return:
            compileReturn();
            break;
        case ForceOSRExit:
            compileForceOSRExit();
            break;
        case CPUIntrinsic:
#if CPU(X86_64)
            compileCPUIntrinsic();
#else
            RELEASE_ASSERT_NOT_REACHED();
#endif
            break;
        case Throw:
            compileThrow();
            break;
        case ThrowStaticError:
            compileThrowStaticError();
            break;
        case InvalidationPoint:
            compileInvalidationPoint();
            break;
        case IsEmpty:
            compileIsEmpty();
            break;
        case IsUndefined:
            compileIsUndefined();
            break;
        case IsUndefinedOrNull:
            compileIsUndefinedOrNull();
            break;
        case IsBoolean:
            compileIsBoolean();
            break;
        case IsNumber:
            compileIsNumber();
            break;
        case NumberIsInteger:
            compileNumberIsInteger();
            break;
        case IsCellWithType:
            compileIsCellWithType();
            break;
        case MapHash:
            compileMapHash();
            break;
        case NormalizeMapKey:
            compileNormalizeMapKey();
            break;
        case GetMapBucket:
            compileGetMapBucket();
            break;
        case GetMapBucketHead:
            compileGetMapBucketHead();
            break;
        case GetMapBucketNext:
            compileGetMapBucketNext();
            break;
        case LoadKeyFromMapBucket:
            compileLoadKeyFromMapBucket();
            break;
        case LoadValueFromMapBucket:
            compileLoadValueFromMapBucket();
            break;
        case ExtractValueFromWeakMapGet:
            compileExtractValueFromWeakMapGet();
            break;
        case SetAdd:
            compileSetAdd();
            break;
        case MapSet:
            compileMapSet();
            break;
        case WeakMapGet:
            compileWeakMapGet();
            break;
        case WeakSetAdd:
            compileWeakSetAdd();
            break;
        case WeakMapSet:
            compileWeakMapSet();
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
        case IsTypedArrayView:
            compileIsTypedArrayView();
            break;
        case ParseInt:
            compileParseInt();
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
        case SuperSamplerBegin:
            compileSuperSamplerBegin();
            break;
        case SuperSamplerEnd:
            compileSuperSamplerEnd();
            break;
        case StoreBarrier:
        case FencedStoreBarrier:
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
        case CheckTraps:
            compileCheckTraps();
            break;
        case CreateRest:
            compileCreateRest();
            break;
        case GetRestLength:
            compileGetRestLength();
            break;
        case RegExpExec:
            compileRegExpExec();
            break;
        case RegExpExecNonGlobalOrSticky:
            compileRegExpExecNonGlobalOrSticky();
            break;
        case RegExpTest:
            compileRegExpTest();
            break;
        case RegExpMatchFast:
            compileRegExpMatchFast();
            break;
        case RegExpMatchFastGlobal:
            compileRegExpMatchFastGlobal();
            break;
        case NewRegexp:
            compileNewRegexp();
            break;
        case SetFunctionName:
            compileSetFunctionName();
            break;
        case StringReplace:
        case StringReplaceRegExp:
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
        case ResolveScopeForHoistingFuncDeclInEval:
            compileResolveScopeForHoistingFuncDeclInEval();
            break;
        case ResolveScope:
            compileResolveScope();
            break;
        case GetDynamicVar:
            compileGetDynamicVar();
            break;
        case PutDynamicVar:
            compilePutDynamicVar();
            break;
        case Unreachable:
            compileUnreachable();
            break;
        case StringSlice:
            compileStringSlice();
            break;
        case ToLowerCase:
            compileToLowerCase();
            break;
        case NumberToStringWithRadix:
            compileNumberToStringWithRadix();
            break;
        case NumberToStringWithValidRadixConstant:
            compileNumberToStringWithValidRadixConstant();
            break;
        case CheckSubClass:
            compileCheckSubClass();
            break;
        case CallDOM:
            compileCallDOM();
            break;
        case CallDOMGetter:
            compileCallDOMGetter();
            break;
        case FilterCallLinkStatus:
        case FilterGetByIdStatus:
        case FilterPutByIdStatus:
        case FilterInByIdStatus:
            compileFilterICStatus();
            break;
        case DataViewGetInt:
        case DataViewGetFloat:
            compileDataViewGet();
            break;
        case DataViewSet:
            compileDataViewSet();
            break;

        case PhantomLocal:
        case LoopHint:
        case MovHint:
        case ZombieHint:
        case ExitOK:
        case PhantomNewObject:
        case PhantomNewFunction:
        case PhantomNewGeneratorFunction:
        case PhantomNewAsyncGeneratorFunction:
        case PhantomNewAsyncFunction:
        case PhantomCreateActivation:
        case PhantomDirectArguments:
        case PhantomCreateRest:
        case PhantomSpread:
        case PhantomNewArrayWithSpread:
        case PhantomNewArrayBuffer:
        case PhantomClonedArguments:
        case PhantomNewRegexp:
        case PutHint:
        case BottomValue:
        case KillStack:
        case InitializeEntrypointArguments:
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
            DFG_CRASH(m_graph, m_node, "Bad result type");
            break;
        }
    }
    
    void compileDoubleConstant()
    {
        setDouble(m_out.constDouble(m_node->asNumber()));
    }
    
    void compileInt52Constant()
    {
        int64_t value = m_node->asAnyInt();
        
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
                isNotInt32(value, provenType(m_node->child1()) & ~SpecDoubleReal));
            ValueFromBlock slowResult = m_out.anchor(m_out.intToDouble(unboxInt32(value)));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            
            setDouble(m_out.phi(Double, fastResult, slowResult));
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
                FTL_TYPE_CHECK(jsValueValue(value), m_node->child1(), ~SpecCellCheck, valueIsNotBooleanFalse);
                ValueFromBlock convertedFalse = m_out.anchor(m_out.constDouble(0));
                m_out.jump(continuation);

                m_out.appendTo(continuation, lastNext);
                setDouble(m_out.phi(Double, intToDouble, unboxedDouble, convertedUndefined, convertedNull, convertedTrue, convertedFalse));
                return;
            }
            m_out.appendTo(nonDoubleCase, continuation);
            FTL_TYPE_CHECK(jsValueValue(value), m_node->child1(), SpecBytecodeNumber, m_out.booleanTrue);
            m_out.unreachable();

            m_out.appendTo(continuation, lastNext);

            setDouble(m_out.phi(Double, intToDouble, unboxedDouble));
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
            
        case AnyIntUse:
            setStrictInt52(
                jsValueToStrictInt52(
                    m_node->child1(), lowJSValue(m_node->child1(), ManualOperandSpeculation)));
            return;
            
        case DoubleRepAnyIntUse:
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
            setInt32(m_out.zeroExt(lowBoolean(m_node->child1()), Int32));
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
                m_out.zeroExt(unboxBoolean(value), Int64), m_tagTypeNumber));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, booleanResult, notBooleanResult));
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

    void compileExtractCatchLocal()
    {
        EncodedJSValue* buffer = static_cast<EncodedJSValue*>(m_ftlState.jitCode->common.catchOSREntryBuffer->dataBuffer());
        setJSValue(m_out.load64(m_out.absolute(buffer + m_node->catchOSREntryIndex())));
    }

    void compileClearCatchLocals()
    {
        ScratchBuffer* scratchBuffer = m_ftlState.jitCode->common.catchOSREntryBuffer;
        ASSERT(scratchBuffer);
        m_out.storePtr(m_out.constIntPtr(0), m_out.absolute(scratchBuffer->addressOfActiveLength()));
    }
    
    void compileGetStack()
    {
        StackAccessData* data = m_node->stackAccessData();
        AbstractValue& value = m_state.operand(data->local);
        
        DFG_ASSERT(m_graph, m_node, isConcrete(data->format), data->format);
        
        if (data->format == FlushedDouble)
            setDouble(m_out.loadDouble(addressFor(data->machineLocal)));
        else if (isInt32Speculation(value.m_type))
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

    void compileToObjectOrCallObjectConstructor()
    {
        LValue value = lowJSValue(m_node->child1());

        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(isCell(value, provenType(m_node->child1())), usually(isCellCase), rarely(slowCase));

        LBasicBlock lastNext = m_out.appendTo(isCellCase, slowCase);
        ValueFromBlock fastResult = m_out.anchor(value);
        m_out.branch(isObject(value), usually(continuation), rarely(slowCase));

        m_out.appendTo(slowCase, continuation);

        ValueFromBlock slowResult;
        if (m_node->op() == ToObject) {
            auto* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
            slowResult = m_out.anchor(vmCall(Int64, m_out.operation(operationToObject), m_callFrame, weakPointer(globalObject), value, m_out.constIntPtr(m_graph.identifiers()[m_node->identifierNumber()])));
        } else
            slowResult = m_out.anchor(vmCall(Int64, m_out.operation(operationCallObjectConstructor), m_callFrame, frozenPointer(m_node->cellOperand()), value));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(Int64, fastResult, slowResult));
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
        m_out.branch(
            m_out.testIsZero32(
                m_out.load8ZeroExt32(value, m_heaps.JSCell_typeInfoFlags),
                m_out.constInt32(OverridesToThis)),
            usually(continuation), rarely(slowCase));
        
        m_out.appendTo(slowCase, continuation);
        J_JITOperation_EJ function;
        if (m_graph.isStrictModeFor(m_node->origin.semantic))
            function = operationToThisStrict;
        else
            function = operationToThis;
        ValueFromBlock slowResult = m_out.anchor(
            vmCall(Int64, m_out.operation(function), m_callFrame, value));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(Int64, fastResult, slowResult));
    }

    void compileValueAdd()
    {
        if (m_node->isBinaryUseKind(BigIntUse)) {
            LValue left = lowBigInt(m_node->child1());
            LValue right = lowBigInt(m_node->child2());

            LValue result = vmCall(pointerType(), m_out.operation(operationAddBigInt), m_callFrame, left, right);
            setJSValue(result);
            return;
        }

        CodeBlock* baselineCodeBlock = m_ftlState.graph.baselineCodeBlockFor(m_node->origin.semantic);
        ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(m_node->origin.semantic.bytecodeIndex);
        const Instruction* instruction = baselineCodeBlock->instructions().at(m_node->origin.semantic.bytecodeIndex).ptr();
        auto repatchingFunction = operationValueAddOptimize;
        auto nonRepatchingFunction = operationValueAdd;
        compileBinaryMathIC<JITAddGenerator>(arithProfile, instruction, repatchingFunction, nonRepatchingFunction);
    }

    void compileValueSub()
    {
        if (m_node->isBinaryUseKind(BigIntUse)) {
            LValue left = lowBigInt(m_node->child1());
            LValue right = lowBigInt(m_node->child2());
            
            LValue result = vmCall(pointerType(), m_out.operation(operationSubBigInt), m_callFrame, left, right);
            setJSValue(result);
            return;
        }

        CodeBlock* baselineCodeBlock = m_ftlState.graph.baselineCodeBlockFor(m_node->origin.semantic);
        ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(m_node->origin.semantic.bytecodeIndex);
        const Instruction* instruction = baselineCodeBlock->instructions().at(m_node->origin.semantic.bytecodeIndex).ptr();
        auto repatchingFunction = operationValueSubOptimize;
        auto nonRepatchingFunction = operationValueSub;
        compileBinaryMathIC<JITSubGenerator>(arithProfile, instruction, repatchingFunction, nonRepatchingFunction);
    }

    void compileValueMul()
    {
        if (m_node->isBinaryUseKind(BigIntUse)) {
            LValue left = lowBigInt(m_node->child1());
            LValue right = lowBigInt(m_node->child2());
            
            LValue result = vmCall(Int64, m_out.operation(operationMulBigInt), m_callFrame, left, right);
            setJSValue(result);
            return;
        }

        CodeBlock* baselineCodeBlock = m_ftlState.graph.baselineCodeBlockFor(m_node->origin.semantic);
        ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(m_node->origin.semantic.bytecodeIndex);
        const Instruction* instruction = baselineCodeBlock->instructions().at(m_node->origin.semantic.bytecodeIndex).ptr();
        auto repatchingFunction = operationValueMulOptimize;
        auto nonRepatchingFunction = operationValueMul;
        compileBinaryMathIC<JITMulGenerator>(arithProfile, instruction, repatchingFunction, nonRepatchingFunction);
    }

    template <typename Generator, typename Func1, typename Func2,
        typename = std::enable_if_t<std::is_function<typename std::remove_pointer<Func1>::type>::value && std::is_function<typename std::remove_pointer<Func2>::type>::value>>
    void compileUnaryMathIC(ArithProfile* arithProfile, const Instruction* instruction, Func1 repatchingFunction, Func2 nonRepatchingFunction)
    {
        Node* node = m_node;

        LValue operand = lowJSValue(node->child1());

        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(operand);
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle = preparePatchpointForExceptions(patchpoint);
        patchpoint->numGPScratchRegisters = 1;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);

                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);

#if ENABLE(MATH_IC_STATS)
                auto inlineStart = jit.label();
#endif

                Box<MathICGenerationState> mathICGenerationState = Box<MathICGenerationState>::create();
                JITUnaryMathIC<Generator>* mathIC = jit.codeBlock()->addMathIC<Generator>(arithProfile, instruction);
                mathIC->m_generator = Generator(JSValueRegs(params[0].gpr()), JSValueRegs(params[1].gpr()), params.gpScratch(0));

                bool shouldEmitProfiling = false;
                bool generatedInline = mathIC->generateInline(jit, *mathICGenerationState, shouldEmitProfiling);

                if (generatedInline) {
                    ASSERT(!mathICGenerationState->slowPathJumps.empty());
                    auto done = jit.label();
                    params.addLatePath([=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);
                        mathICGenerationState->slowPathJumps.link(&jit);
                        mathICGenerationState->slowPathStart = jit.label();
#if ENABLE(MATH_IC_STATS)
                        auto slowPathStart = jit.label();
#endif

                        if (mathICGenerationState->shouldSlowPathRepatch) {
                            SlowPathCall call = callOperation(*state, params.unavailableRegisters(), jit, node->origin.semantic, exceptions.get(),
                                repatchingFunction, params[0].gpr(), params[1].gpr(), CCallHelpers::TrustedImmPtr(mathIC));
                            mathICGenerationState->slowPathCall = call.call();
                        } else {
                            SlowPathCall call = callOperation(*state, params.unavailableRegisters(), jit, node->origin.semantic,
                                exceptions.get(), nonRepatchingFunction, params[0].gpr(), params[1].gpr());
                            mathICGenerationState->slowPathCall = call.call();
                        }
                        jit.jump().linkTo(done, &jit);

                        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                            mathIC->finalizeInlineCode(*mathICGenerationState, linkBuffer);
                        });

#if ENABLE(MATH_IC_STATS)
                        auto slowPathEnd = jit.label();
                        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                            size_t size = linkBuffer.locationOf(slowPathEnd).executableAddress<char*>() - linkBuffer.locationOf(slowPathStart).executableAddress<char*>();
                            mathIC->m_generatedCodeSize += size;
                        });
#endif
                    });
                } else {
                    callOperation(
                        *state, params.unavailableRegisters(), jit, node->origin.semantic, exceptions.get(),
                        nonRepatchingFunction, params[0].gpr(), params[1].gpr());
                }

#if ENABLE(MATH_IC_STATS)
                auto inlineEnd = jit.label();
                jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                    size_t size = linkBuffer.locationOf(inlineEnd).executableAddress<char*>() - linkBuffer.locationOf(inlineStart).executableAddress<char*>();
                    mathIC->m_generatedCodeSize += size;
                });
#endif
            });

        setJSValue(patchpoint);
    }

    template <typename Generator, typename Func1, typename Func2,
        typename = std::enable_if_t<std::is_function<typename std::remove_pointer<Func1>::type>::value && std::is_function<typename std::remove_pointer<Func2>::type>::value>>
    void compileBinaryMathIC(ArithProfile* arithProfile, const Instruction* instruction, Func1 repatchingFunction, Func2 nonRepatchingFunction)
    {
        Node* node = m_node;
        
        LValue left = lowJSValue(node->child1());
        LValue right = lowJSValue(node->child2());

        SnippetOperand leftOperand(m_state.forNode(node->child1()).resultType());
        SnippetOperand rightOperand(m_state.forNode(node->child2()).resultType());
            
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(left);
        patchpoint->appendSomeRegister(right);
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        patchpoint->numGPScratchRegisters = 1;
        patchpoint->numFPScratchRegisters = 2;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);


                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);

#if ENABLE(MATH_IC_STATS)
                auto inlineStart = jit.label();
#endif

                Box<MathICGenerationState> mathICGenerationState = Box<MathICGenerationState>::create();
                JITBinaryMathIC<Generator>* mathIC = jit.codeBlock()->addMathIC<Generator>(arithProfile, instruction);
                mathIC->m_generator = Generator(leftOperand, rightOperand, JSValueRegs(params[0].gpr()),
                    JSValueRegs(params[1].gpr()), JSValueRegs(params[2].gpr()), params.fpScratch(0),
                    params.fpScratch(1), params.gpScratch(0), InvalidFPRReg);

                bool shouldEmitProfiling = false;
                bool generatedInline = mathIC->generateInline(jit, *mathICGenerationState, shouldEmitProfiling);

                if (generatedInline) {
                    ASSERT(!mathICGenerationState->slowPathJumps.empty());
                    auto done = jit.label();
                    params.addLatePath([=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);
                        mathICGenerationState->slowPathJumps.link(&jit);
                        mathICGenerationState->slowPathStart = jit.label();
#if ENABLE(MATH_IC_STATS)
                        auto slowPathStart = jit.label();
#endif

                        if (mathICGenerationState->shouldSlowPathRepatch) {
                            SlowPathCall call = callOperation(*state, params.unavailableRegisters(), jit, node->origin.semantic, exceptions.get(),
                                repatchingFunction, params[0].gpr(), params[1].gpr(), params[2].gpr(), CCallHelpers::TrustedImmPtr(mathIC));
                            mathICGenerationState->slowPathCall = call.call();
                        } else {
                            SlowPathCall call = callOperation(*state, params.unavailableRegisters(), jit, node->origin.semantic,
                                exceptions.get(), nonRepatchingFunction, params[0].gpr(), params[1].gpr(), params[2].gpr());
                            mathICGenerationState->slowPathCall = call.call();
                        }
                        jit.jump().linkTo(done, &jit);

                        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                            mathIC->finalizeInlineCode(*mathICGenerationState, linkBuffer);
                        });

#if ENABLE(MATH_IC_STATS)
                        auto slowPathEnd = jit.label();
                        jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                            size_t size = linkBuffer.locationOf(slowPathEnd).executableAddress<char*>() - linkBuffer.locationOf(slowPathStart).executableAddress<char*>();
                            mathIC->m_generatedCodeSize += size;
                        });
#endif
                    });
                } else {
                    callOperation(
                        *state, params.unavailableRegisters(), jit, node->origin.semantic, exceptions.get(),
                        nonRepatchingFunction, params[0].gpr(), params[1].gpr(), params[2].gpr());
                }

#if ENABLE(MATH_IC_STATS)
                auto inlineEnd = jit.label();
                jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                    size_t size = linkBuffer.locationOf(inlineEnd).executableAddress<char*>() - linkBuffer.locationOf(inlineStart).executableAddress<char*>();
                    mathIC->m_generatedCodeSize += size;
                });
#endif
            });

        setJSValue(patchpoint);
    }
    
    void compileStrCat()
    {
        LValue result;
        if (m_node->child3()) {
            result = vmCall(
                Int64, m_out.operation(operationStrCat3), m_callFrame,
                lowJSValue(m_node->child1(), ManualOperandSpeculation),
                lowJSValue(m_node->child2(), ManualOperandSpeculation),
                lowJSValue(m_node->child3(), ManualOperandSpeculation));
        } else {
            result = vmCall(
                Int64, m_out.operation(operationStrCat2), m_callFrame,
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
            if (!abstractValue(m_node->child1()).couldBeType(SpecInt52Only)
                && !abstractValue(m_node->child2()).couldBeType(SpecInt52Only)) {
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

            CodeBlock* baselineCodeBlock = m_ftlState.graph.baselineCodeBlockFor(m_node->origin.semantic);
            ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(m_node->origin.semantic.bytecodeIndex);
            const Instruction* instruction = baselineCodeBlock->instructions().at(m_node->origin.semantic.bytecodeIndex).ptr();
            auto repatchingFunction = operationValueSubOptimize;
            auto nonRepatchingFunction = operationValueSub;
            compileBinaryMathIC<JITSubGenerator>(arithProfile, instruction, repatchingFunction, nonRepatchingFunction);
            break;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }

    void compileArithClz32()
    {
        if (m_node->child1().useKind() == Int32Use || m_node->child1().useKind() == KnownInt32Use) {
            LValue operand = lowInt32(m_node->child1());
            setInt32(m_out.ctlz32(operand));
            return;
        }
        DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == UntypedUse, m_node->child1().useKind());
        LValue argument = lowJSValue(m_node->child1());
        LValue result = vmCall(Int32, m_out.operation(operationArithClz32), m_callFrame, argument);
        setInt32(result);
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

        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            break;
        }
    }

    void compileValueDiv()
    {
        if (m_node->isBinaryUseKind(BigIntUse)) {
            LValue left = lowBigInt(m_node->child1());
            LValue right = lowBigInt(m_node->child2());
            
            LValue result = vmCall(pointerType(), m_out.operation(operationDivBigInt), m_callFrame, left, right);
            setJSValue(result);
            return;
        }

        emitBinarySnippet<JITDivGenerator, NeedScratchFPR>(operationValueDiv);
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
            setDouble(m_out.phi(Double, results));
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
            
        default: {
            DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == UntypedUse, m_node->child1().useKind());
            LValue argument = lowJSValue(m_node->child1());
            LValue result = vmCall(Double, m_out.operation(operationArithAbs), m_callFrame, argument);
            setDouble(result);
            break;
        }
        }
    }

    void compileArithUnary()
    {
        if (m_node->child1().useKind() == DoubleRepUse) {
            setDouble(m_out.doubleUnary(m_node->arithUnaryType(), lowDouble(m_node->child1())));
            return;
        }
        LValue argument = lowJSValue(m_node->child1());
        LValue result = vmCall(Double, m_out.operation(DFG::arithUnaryOperation(m_node->arithUnaryType())), m_callFrame, argument);
        setDouble(result);
    }

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
            LBasicBlock nanExceptionBaseIsOne = m_out.newBlock();
            LBasicBlock nanExceptionExponentIsInfinity = m_out.newBlock();
            LBasicBlock testExponentIsOneHalf = m_out.newBlock();
            LBasicBlock handleBaseZeroExponentIsOneHalf = m_out.newBlock();
            LBasicBlock handleInfinityForExponentIsOneHalf = m_out.newBlock();
            LBasicBlock exponentIsOneHalfNormal = m_out.newBlock();
            LBasicBlock exponentIsOneHalfInfinity = m_out.newBlock();
            LBasicBlock testExponentIsNegativeOneHalf = m_out.newBlock();
            LBasicBlock testBaseZeroExponentIsNegativeOneHalf = m_out.newBlock();
            LBasicBlock handleBaseZeroExponentIsNegativeOneHalf = m_out.newBlock();
            LBasicBlock handleInfinityForExponentIsNegativeOneHalf = m_out.newBlock();
            LBasicBlock exponentIsNegativeOneHalfNormal = m_out.newBlock();
            LBasicBlock exponentIsNegativeOneHalfInfinity = m_out.newBlock();
            LBasicBlock powBlock = m_out.newBlock();
            LBasicBlock nanExceptionResultIsNaN = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue integerExponent = m_out.doubleToInt(exponent);
            LValue integerExponentConvertedToDouble = m_out.intToDouble(integerExponent);
            LValue exponentIsInteger = m_out.doubleEqual(exponent, integerExponentConvertedToDouble);
            m_out.branch(exponentIsInteger, unsure(integerExponentIsSmallBlock), unsure(doubleExponentPowBlockEntry));

            LBasicBlock lastNext = m_out.appendTo(integerExponentIsSmallBlock, integerExponentPowBlock);
            LValue integerExponentBelowMax = m_out.belowOrEqual(integerExponent, m_out.constInt32(maxExponentForIntegerMathPow));
            m_out.branch(integerExponentBelowMax, usually(integerExponentPowBlock), rarely(doubleExponentPowBlockEntry));

            m_out.appendTo(integerExponentPowBlock, doubleExponentPowBlockEntry);
            ValueFromBlock powDoubleIntResult = m_out.anchor(m_out.doublePowi(base, integerExponent));
            m_out.jump(continuation);

            // If y is NaN, the result is NaN.
            m_out.appendTo(doubleExponentPowBlockEntry, nanExceptionBaseIsOne);
            LValue exponentIsNaN;
            if (provenType(m_node->child2()) & SpecDoubleNaN)
                exponentIsNaN = m_out.doubleNotEqualOrUnordered(exponent, exponent);
            else
                exponentIsNaN = m_out.booleanFalse;
            m_out.branch(exponentIsNaN, rarely(nanExceptionResultIsNaN), usually(nanExceptionBaseIsOne));

            // If abs(x) is 1 and y is +infinity, the result is NaN.
            // If abs(x) is 1 and y is -infinity, the result is NaN.

            //     Test if base == 1.
            m_out.appendTo(nanExceptionBaseIsOne, nanExceptionExponentIsInfinity);
            LValue absoluteBase = m_out.doubleAbs(base);
            LValue absoluteBaseIsOne = m_out.doubleEqual(absoluteBase, m_out.constDouble(1));
            m_out.branch(absoluteBaseIsOne, rarely(nanExceptionExponentIsInfinity), usually(testExponentIsOneHalf));

            //     Test if abs(y) == Infinity.
            m_out.appendTo(nanExceptionExponentIsInfinity, testExponentIsOneHalf);
            LValue absoluteExponent = m_out.doubleAbs(exponent);
            LValue absoluteExponentIsInfinity = m_out.doubleEqual(absoluteExponent, m_out.constDouble(std::numeric_limits<double>::infinity()));
            m_out.branch(absoluteExponentIsInfinity, rarely(nanExceptionResultIsNaN), usually(testExponentIsOneHalf));

            // If y == 0.5 or y == -0.5, handle it through SQRT.
            // We have be carefuly with -0 and -Infinity.

            //     Test if y == 0.5
            m_out.appendTo(testExponentIsOneHalf, handleBaseZeroExponentIsOneHalf);
            LValue exponentIsOneHalf = m_out.doubleEqual(exponent, m_out.constDouble(0.5));
            m_out.branch(exponentIsOneHalf, rarely(handleBaseZeroExponentIsOneHalf), usually(testExponentIsNegativeOneHalf));

            //     Handle x == -0.
            m_out.appendTo(handleBaseZeroExponentIsOneHalf, handleInfinityForExponentIsOneHalf);
            LValue baseIsZeroExponentIsOneHalf = m_out.doubleEqual(base, m_out.doubleZero);
            ValueFromBlock zeroResultExponentIsOneHalf = m_out.anchor(m_out.doubleZero);
            m_out.branch(baseIsZeroExponentIsOneHalf, rarely(continuation), usually(handleInfinityForExponentIsOneHalf));

            //     Test if abs(x) == Infinity.
            m_out.appendTo(handleInfinityForExponentIsOneHalf, exponentIsOneHalfNormal);
            LValue absoluteBaseIsInfinityOneHalf = m_out.doubleEqual(absoluteBase, m_out.constDouble(std::numeric_limits<double>::infinity()));
            m_out.branch(absoluteBaseIsInfinityOneHalf, rarely(exponentIsOneHalfInfinity), usually(exponentIsOneHalfNormal));

            //     The exponent is 0.5, the base is finite or NaN, we can use SQRT.
            m_out.appendTo(exponentIsOneHalfNormal, exponentIsOneHalfInfinity);
            ValueFromBlock sqrtResult = m_out.anchor(m_out.doubleSqrt(base));
            m_out.jump(continuation);

            //     The exponent is 0.5, the base is infinite, the result is always infinite.
            m_out.appendTo(exponentIsOneHalfInfinity, testExponentIsNegativeOneHalf);
            ValueFromBlock sqrtInfinityResult = m_out.anchor(m_out.constDouble(std::numeric_limits<double>::infinity()));
            m_out.jump(continuation);

            //     Test if y == -0.5
            m_out.appendTo(testExponentIsNegativeOneHalf, testBaseZeroExponentIsNegativeOneHalf);
            LValue exponentIsNegativeOneHalf = m_out.doubleEqual(exponent, m_out.constDouble(-0.5));
            m_out.branch(exponentIsNegativeOneHalf, rarely(testBaseZeroExponentIsNegativeOneHalf), usually(powBlock));

            //     Handle x == -0.
            m_out.appendTo(testBaseZeroExponentIsNegativeOneHalf, handleBaseZeroExponentIsNegativeOneHalf);
            LValue baseIsZeroExponentIsNegativeOneHalf = m_out.doubleEqual(base, m_out.doubleZero);
            m_out.branch(baseIsZeroExponentIsNegativeOneHalf, rarely(handleBaseZeroExponentIsNegativeOneHalf), usually(handleInfinityForExponentIsNegativeOneHalf));

            m_out.appendTo(handleBaseZeroExponentIsNegativeOneHalf, handleInfinityForExponentIsNegativeOneHalf);
            ValueFromBlock oneOverSqrtZeroResult = m_out.anchor(m_out.constDouble(std::numeric_limits<double>::infinity()));
            m_out.jump(continuation);

            //     Test if abs(x) == Infinity.
            m_out.appendTo(handleInfinityForExponentIsNegativeOneHalf, exponentIsNegativeOneHalfNormal);
            LValue absoluteBaseIsInfinityNegativeOneHalf = m_out.doubleEqual(absoluteBase, m_out.constDouble(std::numeric_limits<double>::infinity()));
            m_out.branch(absoluteBaseIsInfinityNegativeOneHalf, rarely(exponentIsNegativeOneHalfInfinity), usually(exponentIsNegativeOneHalfNormal));

            //     The exponent is -0.5, the base is finite or NaN, we can use 1/SQRT.
            m_out.appendTo(exponentIsNegativeOneHalfNormal, exponentIsNegativeOneHalfInfinity);
            LValue sqrtBase = m_out.doubleSqrt(base);
            ValueFromBlock oneOverSqrtResult = m_out.anchor(m_out.div(m_out.constDouble(1.), sqrtBase));
            m_out.jump(continuation);

            //     The exponent is -0.5, the base is infinite, the result is always zero.
            m_out.appendTo(exponentIsNegativeOneHalfInfinity, powBlock);
            ValueFromBlock oneOverSqrtInfinityResult = m_out.anchor(m_out.doubleZero);
            m_out.jump(continuation);

            m_out.appendTo(powBlock, nanExceptionResultIsNaN);
            ValueFromBlock powResult = m_out.anchor(m_out.doublePow(base, exponent));
            m_out.jump(continuation);

            m_out.appendTo(nanExceptionResultIsNaN, continuation);
            ValueFromBlock pureNan = m_out.anchor(m_out.constDouble(PNaN));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setDouble(m_out.phi(Double, powDoubleIntResult, zeroResultExponentIsOneHalf, sqrtResult, sqrtInfinityResult, oneOverSqrtZeroResult, oneOverSqrtResult, oneOverSqrtInfinityResult, powResult, pureNan));
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
        if (m_node->child1().useKind() == DoubleRepUse) {
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

                result = m_out.phi(Double, integerValueResult, integerValueRoundedDownResult);
            }

            if (producesInteger(m_node->arithRoundingMode())) {
                LValue integerValue = convertDoubleToInt32(result, shouldCheckNegativeZero(m_node->arithRoundingMode()));
                setInt32(integerValue);
            } else
                setDouble(result);
            return;
        }

        DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == UntypedUse, m_node->child1().useKind());
        LValue argument = lowJSValue(m_node->child1());
        setJSValue(vmCall(Int64, m_out.operation(operationArithRound), m_callFrame, argument));
    }

    void compileArithFloor()
    {
        if (m_node->child1().useKind() == DoubleRepUse) {
            LValue value = lowDouble(m_node->child1());
            LValue integerValue = m_out.doubleFloor(value);
            if (producesInteger(m_node->arithRoundingMode()))
                setInt32(convertDoubleToInt32(integerValue, shouldCheckNegativeZero(m_node->arithRoundingMode())));
            else
                setDouble(integerValue);
            return;
        }
        DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == UntypedUse, m_node->child1().useKind());
        LValue argument = lowJSValue(m_node->child1());
        setJSValue(vmCall(Int64, m_out.operation(operationArithFloor), m_callFrame, argument));
    }

    void compileArithCeil()
    {
        if (m_node->child1().useKind() == DoubleRepUse) {
            LValue value = lowDouble(m_node->child1());
            LValue integerValue = m_out.doubleCeil(value);
            if (producesInteger(m_node->arithRoundingMode()))
                setInt32(convertDoubleToInt32(integerValue, shouldCheckNegativeZero(m_node->arithRoundingMode())));
            else
                setDouble(integerValue);
            return;
        }
        DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == UntypedUse, m_node->child1().useKind());
        LValue argument = lowJSValue(m_node->child1());
        setJSValue(vmCall(Int64, m_out.operation(operationArithCeil), m_callFrame, argument));
    }

    void compileArithTrunc()
    {
        if (m_node->child1().useKind() == DoubleRepUse) {
            LValue value = lowDouble(m_node->child1());
            LValue result = m_out.doubleTrunc(value);
            if (producesInteger(m_node->arithRoundingMode()))
                setInt32(convertDoubleToInt32(result, shouldCheckNegativeZero(m_node->arithRoundingMode())));
            else
                setDouble(result);
            return;
        }
        DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == UntypedUse, m_node->child1().useKind());
        LValue argument = lowJSValue(m_node->child1());
        setJSValue(vmCall(Int64, m_out.operation(operationArithTrunc), m_callFrame, argument));
    }

    void compileArithSqrt()
    {
        if (m_node->child1().useKind() == DoubleRepUse) {
            setDouble(m_out.doubleSqrt(lowDouble(m_node->child1())));
            return;
        }
        LValue argument = lowJSValue(m_node->child1());
        LValue result = vmCall(Double, m_out.operation(operationArithSqrt), m_callFrame, argument);
        setDouble(result);
    }

    void compileArithFRound()
    {
        if (m_node->child1().useKind() == DoubleRepUse) {
            setDouble(m_out.fround(lowDouble(m_node->child1())));
            return;
        }
        LValue argument = lowJSValue(m_node->child1());
        LValue result = vmCall(Double, m_out.operation(operationArithFRound), m_callFrame, argument);
        setDouble(result);
    }

    void compileValueNegate()
    {
        DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == UntypedUse);
        CodeBlock* baselineCodeBlock = m_ftlState.graph.baselineCodeBlockFor(m_node->origin.semantic);
        ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(m_node->origin.semantic.bytecodeIndex);
        const Instruction* instruction = baselineCodeBlock->instructions().at(m_node->origin.semantic.bytecodeIndex).ptr();
        auto repatchingFunction = operationArithNegateOptimize;
        auto nonRepatchingFunction = operationArithNegate;
        compileUnaryMathIC<JITNegGenerator>(arithProfile, instruction, repatchingFunction, nonRepatchingFunction);
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
            if (!abstractValue(m_node->child1()).couldBeType(SpecInt52Only)) {
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
            if (shouldCheckNegativeZero(m_node->arithMode()))
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
    
    void compileArithBitNot()
    {
        if (m_node->child1().useKind() == UntypedUse) {
            LValue operand = lowJSValue(m_node->child1());
            LValue result = vmCall(Int64, m_out.operation(operationValueBitNot), m_callFrame, operand);
            setJSValue(result);
            return;
        }

        setInt32(m_out.bitNot(lowInt32(m_node->child1())));
    }

    void compileValueBitAnd()
    {
        if (m_node->isBinaryUseKind(BigIntUse)) {
            LValue left = lowBigInt(m_node->child1());
            LValue right = lowBigInt(m_node->child2());
            
            LValue result = vmCall(pointerType(), m_out.operation(operationBitAndBigInt), m_callFrame, left, right);
            setJSValue(result);
            return;
        }
        
        emitBinaryBitOpSnippet<JITBitAndGenerator>(operationValueBitAnd);
    }
    
    void compileArithBitAnd()
    {
        setInt32(m_out.bitAnd(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileValueBitOr()
    {
        if (m_node->isBinaryUseKind(BigIntUse)) {
            LValue left = lowBigInt(m_node->child1());
            LValue right = lowBigInt(m_node->child2());

            LValue result = vmCall(pointerType(), m_out.operation(operationBitOrBigInt), m_callFrame, left, right);
            setJSValue(result);
            return;
        }

        emitBinaryBitOpSnippet<JITBitOrGenerator>(operationValueBitOr);
    }

    void compileArithBitOr()
    {
        setInt32(m_out.bitOr(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }
    
    void compileValueBitXor()
    {
        if (m_node->isBinaryUseKind(BigIntUse)) {
            LValue left = lowBigInt(m_node->child1());
            LValue right = lowBigInt(m_node->child2());

            LValue result = vmCall(pointerType(), m_out.operation(operationBitXorBigInt), m_callFrame, left, right);
            setJSValue(result);
            return;
        }

        emitBinaryBitOpSnippet<JITBitXorGenerator>(operationValueBitXor);
    }

    void compileArithBitXor()
    {
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
                [&] (RegisteredStructure structure) {
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
                [&] (RegisteredStructure structure) {
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

    void compileCheckStructureOrEmpty()
    {
        ExitKind exitKind;
        if (m_node->child1()->hasConstant())
            exitKind = BadConstantCache;
        else
            exitKind = BadCache;

        LValue cell = lowCell(m_node->child1());
        bool maySeeEmptyValue = m_interpreter.forNode(m_node->child1()).m_type & SpecEmpty;
        LBasicBlock notEmpty;
        LBasicBlock continuation;
        LBasicBlock lastNext;
        if (maySeeEmptyValue) {
            notEmpty = m_out.newBlock();
            continuation = m_out.newBlock();
            m_out.branch(m_out.isZero64(cell), unsure(continuation), unsure(notEmpty));
            lastNext = m_out.appendTo(notEmpty, continuation);
        }

        checkStructure(
            m_out.load32(cell, m_heaps.JSCell_structureID), jsValueValue(cell),
            exitKind, m_node->structureSet(),
            [&] (RegisteredStructure structure) {
                return weakStructureID(structure);
            });

        if (maySeeEmptyValue) {
            m_out.jump(continuation);
            m_out.appendTo(continuation, lastNext);
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

    void compileAssertNotEmpty()
    {
        if (!validationEnabled())
            return;

        LValue val = lowJSValue(m_node->child1());
        PatchpointValue* patchpoint = m_out.patchpoint(Void);
        patchpoint->appendSomeRegister(val);
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                GPRReg input =  params[0].gpr();
                CCallHelpers::Jump done = jit.branchIfNotEmpty(input);
                jit.breakpoint();
                done.link(&jit);
            });
    }

    void compileCheckStringIdent()
    {
        UniquedStringImpl* uid = m_node->uidOperand();
        LValue stringImpl = lowStringIdent(m_node->child1());
        speculate(BadIdent, noValue(), nullptr, m_out.notEqual(stringImpl, m_out.constIntPtr(uid)));
    }

    void compileGetExecutable()
    {
        LValue cell = lowCell(m_node->child1());
        speculateFunction(m_node->child1(), cell);
        setJSValue(
            m_out.bitXor(
                m_out.loadPtr(cell, m_heaps.JSFunction_executable),
                m_out.constIntPtr(JSFunctionPoison::key())));
    }
    
    void compileArrayify()
    {
        LValue cell = lowCell(m_node->child1());
        LValue property = !!m_node->child2() ? lowInt32(m_node->child2()) : 0;
        
        LBasicBlock unexpectedStructure = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        auto isUnexpectedArray = [&] (LValue cell) {
            if (m_node->op() == Arrayify)
                return m_out.logicalNot(isArrayTypeForArrayify(cell, m_node->arrayMode()));

            ASSERT(m_node->op() == ArrayifyToStructure);
            return m_out.notEqual(m_out.load32(cell, m_heaps.JSCell_structureID), weakStructureID(m_node->structure()));
        };

        m_out.branch(isUnexpectedArray(cell), rarely(unexpectedStructure), usually(continuation));

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
            vmCall(Void, m_out.operation(operationEnsureInt32), m_callFrame, cell);
            break;
        case Array::Double:
            vmCall(Void, m_out.operation(operationEnsureDouble), m_callFrame, cell);
            break;
        case Array::Contiguous:
            vmCall(Void, m_out.operation(operationEnsureContiguous), m_callFrame, cell);
            break;
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            vmCall(Void, m_out.operation(operationEnsureArrayStorage), m_callFrame, cell);
            break;
        default:
            DFG_CRASH(m_graph, m_node, "Bad array type");
            break;
        }
        
        speculate(BadIndexingType, jsValueValue(cell), 0, isUnexpectedArray(cell));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compilePutStructure()
    {
        m_ftlState.jitCode->common.notifyCompilingStructureTransition(m_graph.m_plan, codeBlock(), m_node);
        
        RegisteredStructure oldStructure = m_node->transition()->previous;
        RegisteredStructure newStructure = m_node->transition()->next;
        ASSERT_UNUSED(oldStructure, oldStructure->indexingMode() == newStructure->indexingMode());
        ASSERT(oldStructure->typeInfo().inlineTypeFlags() == newStructure->typeInfo().inlineTypeFlags());
        ASSERT(oldStructure->typeInfo().type() == newStructure->typeInfo().type());

        LValue cell = lowCell(m_node->child1()); 
        m_out.store32(
            weakStructureID(newStructure),
            cell, m_heaps.JSCell_structureID);
    }
    
    void compileGetById(AccessType type)
    {
        ASSERT(type == AccessType::Get || type == AccessType::TryGet || type == AccessType::GetDirect);
        switch (m_node->child1().useKind()) {
        case CellUse: {
            setJSValue(getById(lowCell(m_node->child1()), type));
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
            ValueFromBlock cellResult = m_out.anchor(getById(value, type));
            m_out.jump(continuation);

            J_JITOperation_EJI getByIdFunction = appropriateGenericGetByIdFunction(type);

            m_out.appendTo(notCellCase, continuation);
            ValueFromBlock notCellResult = m_out.anchor(vmCall(
                Int64, m_out.operation(getByIdFunction),
                m_callFrame, value,
                m_out.constIntPtr(m_graph.identifiers()[m_node->identifierNumber()])));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, cellResult, notCellResult));
            return;
        }
            
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
            return;
        }
    }

    void compileGetByIdWithThis()
    {
        if (m_node->child1().useKind() == CellUse && m_node->child2().useKind() == CellUse)
            setJSValue(getByIdWithThis(lowCell(m_node->child1()), lowCell(m_node->child2())));
        else {
            LValue base = lowJSValue(m_node->child1());
            LValue thisValue = lowJSValue(m_node->child2());
            
            LBasicBlock baseCellCase = m_out.newBlock();
            LBasicBlock notCellCase = m_out.newBlock();
            LBasicBlock thisValueCellCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                isCell(base, provenType(m_node->child1())), unsure(baseCellCase), unsure(notCellCase));
            
            LBasicBlock lastNext = m_out.appendTo(baseCellCase, thisValueCellCase);
            
            m_out.branch(
                isCell(thisValue, provenType(m_node->child2())), unsure(thisValueCellCase), unsure(notCellCase));
            
            m_out.appendTo(thisValueCellCase, notCellCase);
            ValueFromBlock cellResult = m_out.anchor(getByIdWithThis(base, thisValue));
            m_out.jump(continuation);

            m_out.appendTo(notCellCase, continuation);
            ValueFromBlock notCellResult = m_out.anchor(vmCall(
                Int64, m_out.operation(operationGetByIdWithThisGeneric),
                m_callFrame, base, thisValue,
                m_out.constIntPtr(m_graph.identifiers()[m_node->identifierNumber()])));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, cellResult, notCellResult));
        }
        
    }

    void compileGetByValWithThis()
    {
        LValue base = lowJSValue(m_node->child1());
        LValue thisValue = lowJSValue(m_node->child2());
        LValue subscript = lowJSValue(m_node->child3());

        LValue result = vmCall(Int64, m_out.operation(operationGetByValWithThis), m_callFrame, base, thisValue, subscript);
        setJSValue(result);
    }

    void compilePutByIdWithThis()
    {
        LValue base = lowJSValue(m_node->child1());
        LValue thisValue = lowJSValue(m_node->child2());
        LValue value = lowJSValue(m_node->child3());

        vmCall(Void, m_out.operation(m_graph.isStrictModeFor(m_node->origin.semantic) ? operationPutByIdWithThisStrict : operationPutByIdWithThis),
            m_callFrame, base, thisValue, value, m_out.constIntPtr(m_graph.identifiers()[m_node->identifierNumber()]));
    }

    void compilePutByValWithThis()
    {
        LValue base = lowJSValue(m_graph.varArgChild(m_node, 0));
        LValue thisValue = lowJSValue(m_graph.varArgChild(m_node, 1));
        LValue property = lowJSValue(m_graph.varArgChild(m_node, 2));
        LValue value = lowJSValue(m_graph.varArgChild(m_node, 3));

        vmCall(Void, m_out.operation(m_graph.isStrictModeFor(m_node->origin.semantic) ? operationPutByValWithThisStrict : operationPutByValWithThis),
            m_callFrame, base, thisValue, property, value);
    }
    
    void compileAtomicsReadModifyWrite()
    {
        TypedArrayType type = m_node->arrayMode().typedArrayType();
        unsigned numExtraArgs = numExtraAtomicsArgs(m_node->op());
        Edge baseEdge = m_graph.child(m_node, 0);
        Edge indexEdge = m_graph.child(m_node, 1);
        Edge argEdges[maxNumExtraAtomicsArgs];
        for (unsigned i = numExtraArgs; i--;)
            argEdges[i] = m_graph.child(m_node, 2 + i);
        Edge storageEdge = m_graph.child(m_node, 2 + numExtraArgs);
        
        auto operation = [&] () -> LValue {
            switch (m_node->op()) {
            case AtomicsAdd:
                return m_out.operation(operationAtomicsAdd);
            case AtomicsAnd:
                return m_out.operation(operationAtomicsAnd);
            case AtomicsCompareExchange:
                return m_out.operation(operationAtomicsCompareExchange);
            case AtomicsExchange:
                return m_out.operation(operationAtomicsExchange);
            case AtomicsLoad:
                return m_out.operation(operationAtomicsLoad);
            case AtomicsOr:
                return m_out.operation(operationAtomicsOr);
            case AtomicsStore:
                return m_out.operation(operationAtomicsStore);
            case AtomicsSub:
                return m_out.operation(operationAtomicsSub);
            case AtomicsXor:
                return m_out.operation(operationAtomicsXor);
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        };
        
        if (!storageEdge) {
            Vector<LValue> args;
            args.append(m_callFrame);
            args.append(lowJSValue(baseEdge));
            args.append(lowJSValue(indexEdge));
            for (unsigned i = 0; i < numExtraArgs; ++i)
                args.append(lowJSValue(argEdges[i]));
            LValue result = vmCall(Int64, operation(), args);
            setJSValue(result);
            return;
        }

        LValue index = lowInt32(indexEdge);
        LValue args[2];
        for (unsigned i = numExtraArgs; i--;)
            args[i] = getIntTypedArrayStoreOperand(argEdges[i]);
        LValue storage = lowStorage(storageEdge);

        TypedPointer pointer = pointerIntoTypedArray(storage, index, type);
        Width width = widthForBytes(elementSize(type));
        
        LValue atomicValue;
        LValue result;
        
        auto sanitizeResult = [&] (LValue value) -> LValue {
            if (isSigned(type)) {
                switch (elementSize(type)) {
                case 1:
                    value = m_out.bitAnd(value, m_out.constInt32(0xff));
                    break;
                case 2:
                    value = m_out.bitAnd(value, m_out.constInt32(0xffff));
                    break;
                case 4:
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
            }
            return value;
        };
        
        switch (m_node->op()) {
        case AtomicsAdd:
            atomicValue = m_out.atomicXchgAdd(args[0], pointer, width);
            result = sanitizeResult(atomicValue);
            break;
        case AtomicsAnd:
            atomicValue = m_out.atomicXchgAnd(args[0], pointer, width);
            result = sanitizeResult(atomicValue);
            break;
        case AtomicsCompareExchange:
            atomicValue = m_out.atomicStrongCAS(args[0], args[1], pointer, width);
            result = sanitizeResult(atomicValue);
            break;
        case AtomicsExchange:
            atomicValue = m_out.atomicXchg(args[0], pointer, width);
            result = sanitizeResult(atomicValue);
            break;
        case AtomicsLoad:
            atomicValue = m_out.atomicXchgAdd(m_out.int32Zero, pointer, width);
            result = sanitizeResult(atomicValue);
            break;
        case AtomicsOr:
            atomicValue = m_out.atomicXchgOr(args[0], pointer, width);
            result = sanitizeResult(atomicValue);
            break;
        case AtomicsStore:
            atomicValue = m_out.atomicXchg(args[0], pointer, width);
            result = args[0];
            break;
        case AtomicsSub:
            atomicValue = m_out.atomicXchgSub(args[0], pointer, width);
            result = sanitizeResult(atomicValue);
            break;
        case AtomicsXor:
            atomicValue = m_out.atomicXchgXor(args[0], pointer, width);
            result = sanitizeResult(atomicValue);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        // Signify that the state against which the atomic operations are serialized is confined to just
        // the typed array storage, since that's as precise of an abstraction as we can have of shared
        // array buffer storage.
        m_heaps.decorateFencedAccess(&m_heaps.typedArrayProperties, atomicValue);
        
        setIntTypedArrayLoadResult(result, type);
    }
    
    void compileAtomicsIsLockFree()
    {
        if (m_node->child1().useKind() != Int32Use) {
            setJSValue(vmCall(Int64, m_out.operation(operationAtomicsIsLockFree), m_callFrame, lowJSValue(m_node->child1())));
            return;
        }
        
        LValue bytes = lowInt32(m_node->child1());
        
        LBasicBlock trueCase = m_out.newBlock();
        LBasicBlock falseCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(trueCase);
        
        Vector<SwitchCase> cases;
        cases.append(SwitchCase(m_out.constInt32(1), trueCase, Weight()));
        cases.append(SwitchCase(m_out.constInt32(2), trueCase, Weight()));
        cases.append(SwitchCase(m_out.constInt32(4), trueCase, Weight()));
        m_out.switchInstruction(bytes, cases, falseCase, Weight());
        
        m_out.appendTo(trueCase, falseCase);
        ValueFromBlock trueValue = m_out.anchor(m_out.booleanTrue);
        m_out.jump(continuation);
        m_out.appendTo(falseCase, continuation);
        ValueFromBlock falseValue = m_out.anchor(m_out.booleanFalse);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, trueValue, falseValue));
    }

    void compileDefineDataProperty()
    {
        LValue base = lowCell(m_graph.varArgChild(m_node, 0));
        LValue value  = lowJSValue(m_graph.varArgChild(m_node, 2));
        LValue attributes = lowInt32(m_graph.varArgChild(m_node, 3));
        Edge& propertyEdge = m_graph.varArgChild(m_node, 1);
        switch (propertyEdge.useKind()) {
        case StringUse: {
            LValue property = lowString(propertyEdge);
            vmCall(Void, m_out.operation(operationDefineDataPropertyString), m_callFrame, base, property, value, attributes);
            break;
        }
        case StringIdentUse: {
            LValue property = lowStringIdent(propertyEdge);
            vmCall(Void, m_out.operation(operationDefineDataPropertyStringIdent), m_callFrame, base, property, value, attributes);
            break;
        }
        case SymbolUse: {
            LValue property = lowSymbol(propertyEdge);
            vmCall(Void, m_out.operation(operationDefineDataPropertySymbol), m_callFrame, base, property, value, attributes);
            break;
        }
        case UntypedUse: {
            LValue property = lowJSValue(propertyEdge);
            vmCall(Void, m_out.operation(operationDefineDataProperty), m_callFrame, base, property, value, attributes);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void compileDefineAccessorProperty()
    {
        LValue base = lowCell(m_graph.varArgChild(m_node, 0));
        LValue getter = lowCell(m_graph.varArgChild(m_node, 2));
        LValue setter = lowCell(m_graph.varArgChild(m_node, 3));
        LValue attributes = lowInt32(m_graph.varArgChild(m_node, 4));
        Edge& propertyEdge = m_graph.varArgChild(m_node, 1);
        switch (propertyEdge.useKind()) {
        case StringUse: {
            LValue property = lowString(propertyEdge);
            vmCall(Void, m_out.operation(operationDefineAccessorPropertyString), m_callFrame, base, property, getter, setter, attributes);
            break;
        }
        case StringIdentUse: {
            LValue property = lowStringIdent(propertyEdge);
            vmCall(Void, m_out.operation(operationDefineAccessorPropertyStringIdent), m_callFrame, base, property, getter, setter, attributes);
            break;
        }
        case SymbolUse: {
            LValue property = lowSymbol(propertyEdge);
            vmCall(Void, m_out.operation(operationDefineAccessorPropertySymbol), m_callFrame, base, property, getter, setter, attributes);
            break;
        }
        case UntypedUse: {
            LValue property = lowJSValue(propertyEdge);
            vmCall(Void, m_out.operation(operationDefineAccessorProperty), m_callFrame, base, property, getter, setter, attributes);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    
    void compilePutById()
    {
        DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == CellUse, m_node->child1().useKind());

        Node* node = m_node;
        LValue base = lowCell(node->child1());
        LValue value = lowJSValue(node->child2());
        auto uid = m_graph.identifiers()[node->identifierNumber()];

        PatchpointValue* patchpoint = m_out.patchpoint(Void);
        patchpoint->appendSomeRegister(base);
        patchpoint->appendSomeRegister(value);
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));
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
                                generator->finalize(linkBuffer, linkBuffer);
                            });
                    });
            });
    }
    
    void compileGetButterfly()
    {
        LValue butterfly = m_out.loadPtr(lowCell(m_node->child1()), m_heaps.JSObject_butterfly);
        setStorage(butterfly);
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
                vmCall(pointerType(), m_out.operation(operationResolveRope), m_callFrame, cell));
            
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            
            setStorage(m_out.loadPtr(m_out.phi(pointerType(), fastResult, slowResult), m_heaps.StringImpl_data));
            return;
        }

        DFG_ASSERT(m_graph, m_node, isTypedView(m_node->arrayMode().typedArrayType()), m_node->arrayMode().typedArrayType());
        LValue vector = m_out.loadPtr(cell, m_heaps.JSArrayBufferView_vector);
        setStorage(caged(Gigacage::Primitive, vector));
    }
    
    void compileCheckArray()
    {
        Edge edge = m_node->child1();
        LValue cell = lowCell(edge);
        
        if (m_node->arrayMode().alreadyChecked(m_graph, m_node, abstractValue(edge)))
            return;
        
        speculate(
            BadIndexingType, jsValueValue(cell), 0,
            m_out.logicalNot(isArrayTypeForCheckArray(cell, m_node->arrayMode())));
    }

    void compileGetTypedArrayByteOffset()
    {
        LValue basePtr = lowCell(m_node->child1());    

        LBasicBlock simpleCase = m_out.newBlock();
        LBasicBlock wastefulCase = m_out.newBlock();
        LBasicBlock notNull = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue mode = m_out.load32(basePtr, m_heaps.JSArrayBufferView_mode);
        m_out.branch(
            m_out.notEqual(mode, m_out.constInt32(WastefulTypedArray)),
            unsure(simpleCase), unsure(wastefulCase));

        LBasicBlock lastNext = m_out.appendTo(simpleCase, wastefulCase);

        ValueFromBlock simpleOut = m_out.anchor(m_out.constIntPtr(0));

        m_out.jump(continuation);

        m_out.appendTo(wastefulCase, notNull);

        LValue vector = m_out.loadPtr(basePtr, m_heaps.JSArrayBufferView_vector);
        ValueFromBlock nullVectorOut = m_out.anchor(vector);
        m_out.branch(vector, unsure(notNull), unsure(continuation));

        m_out.appendTo(notNull, continuation);

        LValue butterflyPtr = caged(Gigacage::JSValue, m_out.loadPtr(basePtr, m_heaps.JSObject_butterfly));
        LValue arrayBufferPtr = m_out.loadPtr(butterflyPtr, m_heaps.Butterfly_arrayBuffer);

        LValue vectorPtr = caged(Gigacage::Primitive, vector);

        // FIXME: This needs caging.
        // https://bugs.webkit.org/show_bug.cgi?id=175515
        LValue dataPtr = m_out.loadPtr(arrayBufferPtr, m_heaps.ArrayBuffer_data);

        ValueFromBlock wastefulOut = m_out.anchor(m_out.sub(vectorPtr, dataPtr));

        m_out.jump(continuation);
        m_out.appendTo(continuation, lastNext);

        setInt32(m_out.castToInt32(m_out.phi(pointerType(), simpleOut, nullVectorOut, wastefulOut)));
    }

    void compileGetPrototypeOf()
    {
        switch (m_node->child1().useKind()) {
        case ArrayUse:
        case FunctionUse:
        case FinalObjectUse: {
            LValue object = lowCell(m_node->child1());
            switch (m_node->child1().useKind()) {
            case ArrayUse:
                speculateArray(m_node->child1(), object);
                break;
            case FunctionUse:
                speculateFunction(m_node->child1(), object);
                break;
            case FinalObjectUse:
                speculateFinalObject(m_node->child1(), object);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            LValue structure = loadStructure(object);

            AbstractValue& value = m_state.forNode(m_node->child1());
            if ((value.m_type && !(value.m_type & ~SpecObject)) && value.m_structure.isFinite()) {
                bool hasPolyProto = false;
                bool hasMonoProto = false;
                value.m_structure.forEach([&] (RegisteredStructure structure) {
                    if (structure->hasPolyProto())
                        hasPolyProto = true;
                    else
                        hasMonoProto = true;
                });

                if (hasMonoProto && !hasPolyProto) {
                    setJSValue(m_out.load64(structure, m_heaps.Structure_prototype));
                    return;
                }

                if (hasPolyProto && !hasMonoProto) {
                    setJSValue(m_out.load64(m_out.baseIndex(m_heaps.properties.atAnyNumber(), object, m_out.constInt64(knownPolyProtoOffset), ScaleEight, JSObject::offsetOfInlineStorage())));
                    return;
                }
            }

            LBasicBlock continuation = m_out.newBlock();
            LBasicBlock loadPolyProto = m_out.newBlock();

            LValue prototypeBits = m_out.load64(structure, m_heaps.Structure_prototype);
            ValueFromBlock directPrototype = m_out.anchor(prototypeBits);
            m_out.branch(m_out.isZero64(prototypeBits), unsure(loadPolyProto), unsure(continuation));

            LBasicBlock lastNext = m_out.appendTo(loadPolyProto, continuation);
            ValueFromBlock polyProto = m_out.anchor(
                m_out.load64(m_out.baseIndex(m_heaps.properties.atAnyNumber(), object, m_out.constInt64(knownPolyProtoOffset), ScaleEight, JSObject::offsetOfInlineStorage())));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, directPrototype, polyProto));
            return;
        }
        case ObjectUse: {
            setJSValue(vmCall(Int64, m_out.operation(operationGetPrototypeOfObject), m_callFrame, lowObject(m_node->child1())));
            return;
        }
        default: {
            setJSValue(vmCall(Int64, m_out.operation(operationGetPrototypeOf), m_callFrame, lowJSValue(m_node->child1())));
            return;
        }
        }
    }
    
    void compileGetArrayLength()
    {
        switch (m_node->arrayMode().type()) {
        case Array::Undecided:
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            setInt32(m_out.load32NonNegative(lowStorage(m_node->child2()), m_heaps.Butterfly_publicLength));
            return;
        }

        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage: {
            LValue length = m_out.load32(lowStorage(m_node->child2()), m_heaps.ArrayStorage_publicLength);
            speculate(Uncountable, noValue(), nullptr, m_out.lessThan(length, m_out.int32Zero));
            setInt32(length);
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
                m_out.notNull(m_out.loadPtr(arguments, m_heaps.DirectArguments_mappedArguments)));
            setInt32(m_out.load32NonNegative(arguments, m_heaps.DirectArguments_length));
            return;
        }
            
        case Array::ScopedArguments: {
            LValue arguments = lowCell(m_node->child1());
            LValue storage = m_out.bitXor(
                m_out.loadPtr(arguments, m_heaps.ScopedArguments_storage),
                m_out.constIntPtr(ScopedArgumentsPoison::key()));
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.notZero32(m_out.load8ZeroExt32(storage, m_heaps.ScopedArguments_Storage_overrodeThings)));
            setInt32(m_out.load32NonNegative(storage, m_heaps.ScopedArguments_Storage_totalLength));
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

    void compileGetVectorLength()
    {
        switch (m_node->arrayMode().type()) {
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            setInt32(m_out.load32NonNegative(lowStorage(m_node->child2()), m_heaps.ArrayStorage_vectorLength));
            return;
        default:
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
            LValue index = lowInt32(m_graph.varArgChild(m_node, 1));
            LValue storage = lowStorage(m_graph.varArgChild(m_node, 2));
            
            IndexedAbstractHeap& heap = m_node->arrayMode().type() == Array::Int32 ?
                m_heaps.indexedInt32Properties : m_heaps.indexedContiguousProperties;

            LValue base = lowCell(m_graph.varArgChild(m_node, 0));

            if (m_node->arrayMode().isInBounds()) {
                LValue result = m_out.load64(baseIndex(heap, storage, index, m_graph.varArgChild(m_node, 1)));
                LValue isHole = m_out.isZero64(result);
                if (m_node->arrayMode().isSaneChain()) {
                    DFG_ASSERT(
                        m_graph, m_node, m_node->arrayMode().type() == Array::Contiguous, m_node->arrayMode().type());
                    result = m_out.select(
                        isHole, m_out.constInt64(JSValue::encode(jsUndefined())), result);
                } else
                    speculate(LoadFromHole, noValue(), 0, isHole);
                setJSValue(result);
                return;
            }
            
            LBasicBlock fastCase = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                m_out.aboveOrEqual(
                    index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength)),
                rarely(slowCase), usually(fastCase));
            
            LBasicBlock lastNext = m_out.appendTo(fastCase, slowCase);

            LValue fastResultValue = m_out.load64(baseIndex(heap, storage, index, m_graph.varArgChild(m_node, 1)));
            ValueFromBlock fastResult = m_out.anchor(fastResultValue);
            m_out.branch(
                m_out.isZero64(fastResultValue), rarely(slowCase), usually(continuation));
            
            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(Int64, m_out.operation(operationGetByValObjectInt), m_callFrame, base, index));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, fastResult, slowResult));
            return;
        }
            
        case Array::Double: {
            LValue base = lowCell(m_graph.varArgChild(m_node, 0));
            LValue index = lowInt32(m_graph.varArgChild(m_node, 1));
            LValue storage = lowStorage(m_graph.varArgChild(m_node, 2));
            
            IndexedAbstractHeap& heap = m_heaps.indexedDoubleProperties;
            
            if (m_node->arrayMode().isInBounds()) {
                LValue result = m_out.loadDouble(
                    baseIndex(heap, storage, index, m_graph.varArgChild(m_node, 1)));
                
                if (!m_node->arrayMode().isSaneChain()) {
                    speculate(
                        LoadFromHole, noValue(), 0,
                        m_out.doubleNotEqualOrUnordered(result, result));
                }
                setDouble(result);
                break;
            }

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
                baseIndex(heap, storage, index, m_graph.varArgChild(m_node, 1)));
            m_out.branch(
                m_out.doubleNotEqualOrUnordered(doubleValue, doubleValue),
                rarely(slowCase), usually(boxPath));
            
            m_out.appendTo(boxPath, slowCase);
            ValueFromBlock fastResult = m_out.anchor(boxDouble(doubleValue));
            m_out.jump(continuation);
            
            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(Int64, m_out.operation(operationGetByValObjectInt), m_callFrame, base, index));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, fastResult, slowResult));
            return;
        }

        case Array::Undecided: {
            LValue index = lowInt32(m_graph.varArgChild(m_node, 1));

            speculate(OutOfBounds, noValue(), m_node, m_out.lessThan(index, m_out.int32Zero));
            setJSValue(m_out.constInt64(ValueUndefined));
            return;
        }
            
        case Array::DirectArguments: {
            LValue base = lowCell(m_graph.varArgChild(m_node, 0));
            LValue index = lowInt32(m_graph.varArgChild(m_node, 1));
            
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.notNull(m_out.loadPtr(base, m_heaps.DirectArguments_mappedArguments)));

            LValue length = m_out.load32NonNegative(base, m_heaps.DirectArguments_length);
            auto isOutOfBounds = m_out.aboveOrEqual(index, length);
            if (m_node->arrayMode().isInBounds()) {
                speculate(OutOfBounds, noValue(), nullptr, isOutOfBounds);
                TypedPointer address = m_out.baseIndex(
                    m_heaps.DirectArguments_storage, base, m_out.zeroExtPtr(index));
                setJSValue(m_out.load64(address));
                return;
            }

            LBasicBlock inBounds = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            m_out.branch(isOutOfBounds, rarely(slowCase), usually(inBounds));

            LBasicBlock lastNext = m_out.appendTo(inBounds, slowCase);
            TypedPointer address = m_out.baseIndex(
                m_heaps.DirectArguments_storage,
                base,
                m_out.zeroExt(index, pointerType()));
            ValueFromBlock fastResult = m_out.anchor(m_out.load64(address));
            m_out.jump(continuation);

            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(Int64, m_out.operation(operationGetByValObjectInt), m_callFrame, base, index));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, fastResult, slowResult));
            return;
        }
            
        case Array::ScopedArguments: {
            LValue base = lowCell(m_graph.varArgChild(m_node, 0));
            LValue index = lowInt32(m_graph.varArgChild(m_node, 1));
            
            LValue storage = m_out.loadPtr(base, m_heaps.ScopedArguments_storage);
            storage = m_out.bitXor(storage, m_out.constIntPtr(ScopedArgumentsPoison::key()));
            
            LValue totalLength = m_out.load32NonNegative(
                storage, m_heaps.ScopedArguments_Storage_totalLength);
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.aboveOrEqual(index, totalLength));
            
            LValue table = m_out.loadPtr(base, m_heaps.ScopedArguments_table);
            table = m_out.bitXor(table, m_out.constIntPtr(ScopedArgumentsPoison::key()));
            
            LValue namedLength = m_out.load32(table, m_heaps.ScopedArgumentsTable_length);
            
            LBasicBlock namedCase = m_out.newBlock();
            LBasicBlock overflowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            m_out.branch(
                m_out.aboveOrEqual(index, namedLength), unsure(overflowCase), unsure(namedCase));
            
            LBasicBlock lastNext = m_out.appendTo(namedCase, overflowCase);
            
            LValue scope = m_out.loadPtr(base, m_heaps.ScopedArguments_scope);
            scope = m_out.bitXor(scope, m_out.constIntPtr(ScopedArgumentsPoison::key()));
            
            LValue arguments = m_out.loadPtr(table, m_heaps.ScopedArgumentsTable_arguments);
            
            TypedPointer address = m_out.baseIndex(
                m_heaps.scopedArgumentsTableArguments, arguments, m_out.zeroExtPtr(index));
            LValue scopeOffset = m_out.load32(address);
            
            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.equal(scopeOffset, m_out.constInt32(ScopeOffset::invalidOffset)));
            
            address = m_out.baseIndex(
                m_heaps.JSLexicalEnvironment_variables, scope, m_out.zeroExtPtr(scopeOffset));
            ValueFromBlock namedResult = m_out.anchor(m_out.load64(address));
            m_out.jump(continuation);
            
            m_out.appendTo(overflowCase, continuation);
            
            address = m_out.baseIndex(
                m_heaps.ScopedArguments_Storage_storage, storage,
                m_out.zeroExtPtr(m_out.sub(index, namedLength)));
            LValue overflowValue = m_out.load64(address);
            speculate(ExoticObjectMode, noValue(), nullptr, m_out.isZero64(overflowValue));
            ValueFromBlock overflowResult = m_out.anchor(overflowValue);
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            
            LValue result = m_out.phi(Int64, namedResult, overflowResult);
            result = preciseIndexMask32(result, index, totalLength);
            
            setJSValue(result);
            return;
        }
            
        case Array::Generic: {
            if (m_graph.varArgChild(m_node, 0).useKind() == ObjectUse) {
                if (m_graph.varArgChild(m_node, 1).useKind() == StringUse) {
                    setJSValue(vmCall(
                        Int64, m_out.operation(operationGetByValObjectString), m_callFrame,
                        lowObject(m_graph.varArgChild(m_node, 0)), lowString(m_graph.varArgChild(m_node, 1))));
                    return;
                }

                if (m_graph.varArgChild(m_node, 1).useKind() == SymbolUse) {
                    setJSValue(vmCall(
                        Int64, m_out.operation(operationGetByValObjectSymbol), m_callFrame,
                        lowObject(m_graph.varArgChild(m_node, 0)), lowSymbol(m_graph.varArgChild(m_node, 1))));
                    return;
                }
            }
            setJSValue(vmCall(
                Int64, m_out.operation(operationGetByVal), m_callFrame,
                lowJSValue(m_graph.varArgChild(m_node, 0)), lowJSValue(m_graph.varArgChild(m_node, 1))));
            return;
        }

        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage: {
            LValue base = lowCell(m_graph.varArgChild(m_node, 0));
            LValue index = lowInt32(m_graph.varArgChild(m_node, 1));
            LValue storage = lowStorage(m_graph.varArgChild(m_node, 2));

            IndexedAbstractHeap& heap = m_heaps.ArrayStorage_vector;

            if (m_node->arrayMode().isInBounds()) {
                LValue result = m_out.load64(baseIndex(heap, storage, index, m_graph.varArgChild(m_node, 1)));
                speculate(LoadFromHole, noValue(), 0, m_out.isZero64(result));
                setJSValue(result);
                break;
            }

            LBasicBlock inBounds = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            m_out.branch(
                m_out.aboveOrEqual(index, m_out.load32NonNegative(storage, m_heaps.ArrayStorage_vectorLength)),
                rarely(slowCase), usually(inBounds));

            LBasicBlock lastNext = m_out.appendTo(inBounds, slowCase);
            LValue result = m_out.load64(baseIndex(heap, storage, index, m_graph.varArgChild(m_node, 1)));
            ValueFromBlock fastResult = m_out.anchor(result);
            m_out.branch(
                m_out.isZero64(result),
                rarely(slowCase), usually(continuation));

            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                vmCall(Int64, m_out.operation(operationGetByValObjectInt), m_callFrame, base, index));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, fastResult, slowResult));
            return;
        }
            
        case Array::String: {
            compileStringCharAt();
            return;
        }
            
        case Array::Int8Array:
        case Array::Int16Array:
        case Array::Int32Array:
        case Array::Uint8Array:
        case Array::Uint8ClampedArray:
        case Array::Uint16Array:
        case Array::Uint32Array:
        case Array::Float32Array:
        case Array::Float64Array: {
            LValue index = lowInt32(m_graph.varArgChild(m_node, 1));
            LValue storage = lowStorage(m_graph.varArgChild(m_node, 2));
            
            TypedArrayType type = m_node->arrayMode().typedArrayType();
            ASSERT(isTypedView(type));
            {
                TypedPointer pointer = pointerIntoTypedArray(storage, index, type);
                
                if (isInt(type)) {
                    LValue result = loadFromIntTypedArray(pointer, type);
                    bool canSpeculate = true;
                    setIntTypedArrayLoadResult(result, type, canSpeculate);
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
        }

        case Array::AnyTypedArray:
        case Array::ForceExit:
        case Array::SelectUsingArguments:
        case Array::SelectUsingPredictions:
        case Array::Unprofiled:
            DFG_CRASH(m_graph, m_node, "Bad array type");
            return;
        }
    }
    
    void compileGetMyArgumentByVal()
    {
        InlineCallFrame* inlineCallFrame = m_node->child1()->origin.semantic.inlineCallFrame;
        
        LValue originalIndex = lowInt32(m_node->child2());
        
        LValue numberOfArgsIncludingThis;
        if (inlineCallFrame && !inlineCallFrame->isVarargs())
            numberOfArgsIncludingThis = m_out.constInt32(inlineCallFrame->argumentCountIncludingThis);
        else {
            VirtualRegister argumentCountRegister = AssemblyHelpers::argumentCount(inlineCallFrame);
            numberOfArgsIncludingThis = m_out.load32(payloadFor(argumentCountRegister));
        }
        
        LValue numberOfArgs = m_out.sub(numberOfArgsIncludingThis, m_out.int32One);
        LValue indexToCheck = originalIndex;
        if (m_node->numberOfArgumentsToSkip()) {
            CheckValue* check = m_out.speculateAdd(indexToCheck, m_out.constInt32(m_node->numberOfArgumentsToSkip()));
            blessSpeculation(check, Overflow, noValue(), nullptr, m_origin);
            indexToCheck = check;
        }

        LValue isOutOfBounds = m_out.aboveOrEqual(indexToCheck, numberOfArgs);
        LBasicBlock continuation = nullptr;
        LBasicBlock lastNext = nullptr;
        ValueFromBlock slowResult;
        if (m_node->op() == GetMyArgumentByValOutOfBounds) {
            LBasicBlock normalCase = m_out.newBlock();
            continuation = m_out.newBlock();
            
            slowResult = m_out.anchor(m_out.constInt64(JSValue::encode(jsUndefined())));
            m_out.branch(isOutOfBounds, unsure(continuation), unsure(normalCase));
            
            lastNext = m_out.appendTo(normalCase, continuation);
        } else
            speculate(OutOfBounds, noValue(), nullptr, isOutOfBounds);
        
        LValue index = m_out.add(indexToCheck, m_out.int32One);

        TypedPointer base;
        if (inlineCallFrame) {
            if (inlineCallFrame->argumentCountIncludingThis > 1)
                base = addressFor(inlineCallFrame->argumentsWithFixup[0].virtualRegister());
        } else
            base = addressFor(virtualRegisterForArgument(0));
        
        LValue result;
        if (base) {
            LValue pointer = m_out.baseIndex(
                base.value(), m_out.zeroExt(index, pointerType()), ScaleEight);
            result = m_out.load64(TypedPointer(m_heaps.variables.atAnyIndex(), pointer));
            result = preciseIndexMask32(result, indexToCheck, numberOfArgs);
        } else
            result = m_out.constInt64(JSValue::encode(jsUndefined()));
        
        if (m_node->op() == GetMyArgumentByValOutOfBounds) {
            ValueFromBlock normalResult = m_out.anchor(result);
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            result = m_out.phi(Int64, slowResult, normalResult);
        }
        
        setJSValue(result);
    }
    
    void compilePutByVal()
    {
        Edge child1 = m_graph.varArgChild(m_node, 0);
        Edge child2 = m_graph.varArgChild(m_node, 1);
        Edge child3 = m_graph.varArgChild(m_node, 2);
        Edge child4 = m_graph.varArgChild(m_node, 3);
        Edge child5 = m_graph.varArgChild(m_node, 4);
        
        ArrayMode arrayMode = m_node->arrayMode().modeForPut();
        switch (arrayMode.type()) {
        case Array::Generic: {
            if (child1.useKind() == CellUse) {
                V_JITOperation_ECCJ operation = nullptr;
                if (child2.useKind() == StringUse) {
                    if (m_node->op() == PutByValDirect) {
                        if (m_graph.isStrictModeFor(m_node->origin.semantic))
                            operation = operationPutByValDirectCellStringStrict;
                        else
                            operation = operationPutByValDirectCellStringNonStrict;
                    } else {
                        if (m_graph.isStrictModeFor(m_node->origin.semantic))
                            operation = operationPutByValCellStringStrict;
                        else
                            operation = operationPutByValCellStringNonStrict;
                    }
                    vmCall(Void, m_out.operation(operation), m_callFrame, lowCell(child1), lowString(child2), lowJSValue(child3));
                    return;
                }

                if (child2.useKind() == SymbolUse) {
                    if (m_node->op() == PutByValDirect) {
                        if (m_graph.isStrictModeFor(m_node->origin.semantic))
                            operation = operationPutByValDirectCellSymbolStrict;
                        else
                            operation = operationPutByValDirectCellSymbolNonStrict;
                    } else {
                        if (m_graph.isStrictModeFor(m_node->origin.semantic))
                            operation = operationPutByValCellSymbolStrict;
                        else
                            operation = operationPutByValCellSymbolNonStrict;
                    }
                    vmCall(Void, m_out.operation(operation), m_callFrame, lowCell(child1), lowSymbol(child2), lowJSValue(child3));
                    return;
                }
            }

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
                Void, m_out.operation(operation), m_callFrame,
                lowJSValue(child1), lowJSValue(child2), lowJSValue(child3));
            return;
        }
            
        default:
            break;
        }

        LValue base = lowCell(child1);
        LValue index = lowInt32(child2);
        LValue storage = lowStorage(child4);
        
        switch (arrayMode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous: {
            LBasicBlock continuation = m_out.newBlock();
            LBasicBlock outerLastNext = m_out.appendTo(m_out.m_block, continuation);
            
            switch (arrayMode.type()) {
            case Array::Int32:
            case Array::Contiguous: {
                LValue value = lowJSValue(child3, ManualOperandSpeculation);
                
                if (arrayMode.type() == Array::Int32)
                    FTL_TYPE_CHECK(jsValueValue(value), child3, SpecInt32Only, isNotInt32(value));
                
                TypedPointer elementPointer = m_out.baseIndex(
                    arrayMode.type() == Array::Int32 ?
                    m_heaps.indexedInt32Properties : m_heaps.indexedContiguousProperties,
                    storage, m_out.zeroExtPtr(index), provenValue(child2));
                
                if (m_node->op() == PutByValAlias) {
                    m_out.store64(value, elementPointer);
                    break;
                }
                
                contiguousPutByValOutOfBounds(
                    codeBlock()->isStrictMode()
                        ? (m_node->op() == PutByValDirect ? operationPutByValDirectBeyondArrayBoundsStrict : operationPutByValBeyondArrayBoundsStrict)
                        : (m_node->op() == PutByValDirect ? operationPutByValDirectBeyondArrayBoundsNonStrict : operationPutByValBeyondArrayBoundsNonStrict),
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
                        ? (m_node->op() == PutByValDirect ? operationPutDoubleByValDirectBeyondArrayBoundsStrict : operationPutDoubleByValBeyondArrayBoundsStrict)
                        : (m_node->op() == PutByValDirect ? operationPutDoubleByValDirectBeyondArrayBoundsNonStrict : operationPutDoubleByValBeyondArrayBoundsNonStrict),
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

        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage: {
            LValue value = lowJSValue(child3);

            TypedPointer elementPointer = m_out.baseIndex(
                m_heaps.ArrayStorage_vector, storage, m_out.zeroExtPtr(index),
                provenValue(child2));

            if (m_node->op() == PutByValAlias) {
                m_out.store64(value, elementPointer);
                return;
            }

            if (arrayMode.isInBounds()) {
                speculate(StoreToHole, noValue(), 0, m_out.isZero64(m_out.load64(elementPointer)));
                m_out.store64(value, elementPointer);
                return;
            }

            LValue isOutOfBounds = m_out.aboveOrEqual(
                index, m_out.load32NonNegative(storage, m_heaps.ArrayStorage_vectorLength));

            auto slowPathFunction = codeBlock()->isStrictMode()
                ? (m_node->op() == PutByValDirect ? operationPutByValDirectBeyondArrayBoundsStrict : operationPutByValBeyondArrayBoundsStrict)
                : (m_node->op() == PutByValDirect ? operationPutByValDirectBeyondArrayBoundsNonStrict : operationPutByValBeyondArrayBoundsNonStrict);
            if (!arrayMode.isOutOfBounds()) {
                speculate(OutOfBounds, noValue(), 0, isOutOfBounds);
                isOutOfBounds = m_out.booleanFalse;
            }

            LBasicBlock inBoundCase = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock holeCase = m_out.newBlock();
            LBasicBlock doStoreCase = m_out.newBlock();
            LBasicBlock lengthUpdateCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            m_out.branch(isOutOfBounds, rarely(slowCase), usually(inBoundCase));

            LBasicBlock lastNext = m_out.appendTo(slowCase, inBoundCase);
            vmCall(
                Void, m_out.operation(slowPathFunction),
                m_callFrame, base, index, value);
            m_out.jump(continuation);


            if (arrayMode.isSlowPut()) {
                m_out.appendTo(inBoundCase, doStoreCase);
                m_out.branch(m_out.isZero64(m_out.load64(elementPointer)), rarely(slowCase), usually(doStoreCase));
            } else {
                m_out.appendTo(inBoundCase, holeCase);
                m_out.branch(m_out.isZero64(m_out.load64(elementPointer)), rarely(holeCase), usually(doStoreCase));

                m_out.appendTo(holeCase, lengthUpdateCase);
                m_out.store32(
                    m_out.add(m_out.load32(storage, m_heaps.ArrayStorage_numValuesInVector), m_out.int32One),
                    storage, m_heaps.ArrayStorage_numValuesInVector);
                m_out.branch(
                    m_out.below(
                        index, m_out.load32NonNegative(storage, m_heaps.ArrayStorage_publicLength)),
                    unsure(doStoreCase), unsure(lengthUpdateCase));

                m_out.appendTo(lengthUpdateCase, doStoreCase);
                m_out.store32(
                    m_out.add(index, m_out.int32One),
                    storage, m_heaps.ArrayStorage_publicLength);
                m_out.jump(doStoreCase);
            }

            m_out.appendTo(doStoreCase, continuation);
            m_out.store64(value, elementPointer);
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            return;
        }
            
        case Array::Int8Array:
        case Array::Int16Array:
        case Array::Int32Array:
        case Array::Uint8Array:
        case Array::Uint8ClampedArray:
        case Array::Uint16Array:
        case Array::Uint32Array:
        case Array::Float32Array:
        case Array::Float64Array: {
            TypedArrayType type = arrayMode.typedArrayType();
            
            ASSERT(isTypedView(type));
            {
                TypedPointer pointer = TypedPointer(
                    m_heaps.typedArrayProperties,
                    m_out.add(
                        storage,
                        m_out.shl(
                            m_out.zeroExt(index, pointerType()),
                            m_out.constIntPtr(logElementSize(type)))));
                
                LValue valueToStore;
                
                if (isInt(type)) {
                    LValue intValue = getIntTypedArrayStoreOperand(child3, isClamped(type));

                    valueToStore = intValue;
                } else /* !isInt(type) */ {
                    LValue value = lowDouble(child3);
                    switch (type) {
                    case TypeFloat32:
                        valueToStore = m_out.doubleToFloat(value);
                        break;
                    case TypeFloat64:
                        valueToStore = value;
                        break;
                    default:
                        DFG_CRASH(m_graph, m_node, "Bad typed array type");
                    }
                }

                if (arrayMode.isInBounds() || m_node->op() == PutByValAlias)
                    m_out.store(valueToStore, pointer, storeType(type));
                else {
                    LBasicBlock isInBounds = m_out.newBlock();
                    LBasicBlock isOutOfBounds = m_out.newBlock();
                    LBasicBlock continuation = m_out.newBlock();
                    
                    m_out.branch(
                        m_out.aboveOrEqual(index, lowInt32(child5)),
                        unsure(isOutOfBounds), unsure(isInBounds));
                    
                    LBasicBlock lastNext = m_out.appendTo(isInBounds, isOutOfBounds);
                    m_out.store(valueToStore, pointer, storeType(type));
                    m_out.jump(continuation);

                    m_out.appendTo(isOutOfBounds, continuation);
                    speculateTypedArrayIsNotNeutered(base);
                    m_out.jump(continuation);
                    
                    m_out.appendTo(continuation, lastNext);
                }
                
                return;
            }
        }

        case Array::AnyTypedArray:
        case Array::String:
        case Array::DirectArguments:
        case Array::ForceExit:
        case Array::Generic:
        case Array::ScopedArguments:
        case Array::SelectUsingArguments:
        case Array::SelectUsingPredictions:
        case Array::Undecided:
        case Array::Unprofiled:
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
            Void,
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
            Void, m_out.operation(operationPutGetterSetter),
            m_callFrame, base, m_out.constIntPtr(uid), m_out.constInt32(m_node->accessorAttributes()), getter, setter);

    }

    void compilePutAccessorByVal()
    {
        LValue base = lowCell(m_node->child1());
        LValue subscript = lowJSValue(m_node->child2());
        LValue accessor = lowCell(m_node->child3());
        vmCall(
            Void,
            m_out.operation(m_node->op() == PutGetterByVal ? operationPutGetterByVal : operationPutSetterByVal),
            m_callFrame, base, subscript, m_out.constInt32(m_node->accessorAttributes()), accessor);
    }

    void compileDeleteById()
    {
        LValue base = lowJSValue(m_node->child1());
        auto uid = m_graph.identifiers()[m_node->identifierNumber()];
        setBoolean(m_out.notZero64(vmCall(Int64, m_out.operation(operationDeleteById), m_callFrame, base, m_out.constIntPtr(uid))));
    }

    void compileDeleteByVal()
    {
        LValue base = lowJSValue(m_node->child1());
        LValue subscript = lowJSValue(m_node->child2());
        setBoolean(m_out.notZero64(vmCall(Int64, m_out.operation(operationDeleteByVal), m_callFrame, base, subscript)));
    }
    
    void compileArrayPush()
    {
        LValue base = lowCell(m_graph.varArgChild(m_node, 1));
        LValue storage = lowStorage(m_graph.varArgChild(m_node, 0));
        unsigned elementOffset = 2;
        unsigned elementCount = m_node->numChildren() - elementOffset;

        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Contiguous:
        case Array::Double: {
            IndexedAbstractHeap& heap = m_heaps.forArrayType(m_node->arrayMode().type());

            if (elementCount == 1) {
                LValue value;
                Output::StoreType storeType;
                
                Edge& element = m_graph.varArgChild(m_node, elementOffset);
                if (m_node->arrayMode().type() != Array::Double) {
                    value = lowJSValue(element, ManualOperandSpeculation);
                    if (m_node->arrayMode().type() == Array::Int32)
                        DFG_ASSERT(m_graph, m_node, !m_interpreter.needsTypeCheck(element, SpecInt32Only));
                    storeType = Output::Store64;
                } else {
                    value = lowDouble(element);
                    DFG_ASSERT(m_graph, m_node, !m_interpreter.needsTypeCheck(element, SpecDoubleReal));
                    storeType = Output::StoreDouble;
                }

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
                    vmCall(Int64, operation, m_callFrame, value, base));
                m_out.jump(continuation);
                
                m_out.appendTo(continuation, lastNext);
                setJSValue(m_out.phi(Int64, fastResult, slowResult));
                return;
            }

            LValue prevLength = m_out.load32(storage, m_heaps.Butterfly_publicLength);
            LValue newLength = m_out.add(prevLength, m_out.constInt32(elementCount));

            LBasicBlock fastPath = m_out.newBlock();
            LBasicBlock slowPath = m_out.newBlock();
            LBasicBlock setup = m_out.newBlock();
            LBasicBlock slowCallPath = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue beyondVectorLength = m_out.above(newLength, m_out.load32(storage, m_heaps.Butterfly_vectorLength));

            m_out.branch(beyondVectorLength, unsure(slowPath), unsure(fastPath));

            LBasicBlock lastNext = m_out.appendTo(fastPath, slowPath);
            m_out.store32(newLength, storage, m_heaps.Butterfly_publicLength);
            ValueFromBlock fastBufferResult = m_out.anchor(m_out.baseIndex(storage, m_out.zeroExtPtr(prevLength), ScaleEight));
            m_out.jump(setup);

            m_out.appendTo(slowPath, setup);
            size_t scratchSize = sizeof(EncodedJSValue) * elementCount;
            static_assert(sizeof(EncodedJSValue) == sizeof(double), "");
            ASSERT(scratchSize);
            ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
            m_out.storePtr(m_out.constIntPtr(scratchSize), m_out.absolute(scratchBuffer->addressOfActiveLength()));
            ValueFromBlock slowBufferResult = m_out.anchor(m_out.constIntPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())));
            m_out.jump(setup);

            m_out.appendTo(setup, slowCallPath);
            LValue buffer = m_out.phi(pointerType(), fastBufferResult, slowBufferResult);
            for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
                Edge& element = m_graph.varArgChild(m_node, elementIndex + elementOffset);

                LValue value;
                Output::StoreType storeType;
                if (m_node->arrayMode().type() != Array::Double) {
                    value = lowJSValue(element, ManualOperandSpeculation);
                    if (m_node->arrayMode().type() == Array::Int32)
                        DFG_ASSERT(m_graph, m_node, !m_interpreter.needsTypeCheck(element, SpecInt32Only));
                    storeType = Output::Store64;
                } else {
                    value = lowDouble(element);
                    DFG_ASSERT(m_graph, m_node, !m_interpreter.needsTypeCheck(element, SpecDoubleReal));
                    storeType = Output::StoreDouble;
                }

                m_out.store(value, m_out.baseIndex(heap, buffer, m_out.constInt32(elementIndex), jsNumber(elementIndex)), storeType);
            }
            ValueFromBlock fastResult = m_out.anchor(boxInt32(newLength));

            m_out.branch(beyondVectorLength, unsure(slowCallPath), unsure(continuation));

            m_out.appendTo(slowCallPath, continuation);
            LValue operation;
            if (m_node->arrayMode().type() != Array::Double)
                operation = m_out.operation(operationArrayPushMultiple);
            else
                operation = m_out.operation(operationArrayPushDoubleMultiple);
            ValueFromBlock slowResult = m_out.anchor(vmCall(Int64, operation, m_callFrame, base, buffer, m_out.constInt32(elementCount)));
            m_out.storePtr(m_out.constIntPtr(0), m_out.absolute(scratchBuffer->addressOfActiveLength()));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, fastResult, slowResult));
            return;
        }

        case Array::ArrayStorage: {
            // This ensures that the result of ArrayPush is Int32 in AI.
            int32_t largestPositiveInt32Length = 0x7fffffff - elementCount;

            LValue prevLength = m_out.load32(storage, m_heaps.ArrayStorage_publicLength);
            // Refuse to handle bizarre lengths.
            speculate(Uncountable, noValue(), nullptr, m_out.above(prevLength, m_out.constInt32(largestPositiveInt32Length)));

            if (elementCount == 1) {
                Edge& element = m_graph.varArgChild(m_node, elementOffset);

                LValue value = lowJSValue(element);

                LBasicBlock fastPath = m_out.newBlock();
                LBasicBlock slowPath = m_out.newBlock();
                LBasicBlock continuation = m_out.newBlock();

                m_out.branch(
                    m_out.aboveOrEqual(
                        prevLength, m_out.load32(storage, m_heaps.ArrayStorage_vectorLength)),
                    rarely(slowPath), usually(fastPath));

                LBasicBlock lastNext = m_out.appendTo(fastPath, slowPath);
                m_out.store64(
                    value, m_out.baseIndex(m_heaps.ArrayStorage_vector, storage, m_out.zeroExtPtr(prevLength)));
                LValue newLength = m_out.add(prevLength, m_out.int32One);
                m_out.store32(newLength, storage, m_heaps.ArrayStorage_publicLength);
                m_out.store32(
                    m_out.add(m_out.load32(storage, m_heaps.ArrayStorage_numValuesInVector), m_out.int32One),
                    storage, m_heaps.ArrayStorage_numValuesInVector);

                ValueFromBlock fastResult = m_out.anchor(boxInt32(newLength));
                m_out.jump(continuation);

                m_out.appendTo(slowPath, continuation);
                ValueFromBlock slowResult = m_out.anchor(
                    vmCall(Int64, m_out.operation(operationArrayPush), m_callFrame, value, base));
                m_out.jump(continuation);

                m_out.appendTo(continuation, lastNext);
                setJSValue(m_out.phi(Int64, fastResult, slowResult));
                return;
            }

            LValue newLength = m_out.add(prevLength, m_out.constInt32(elementCount));

            LBasicBlock fastPath = m_out.newBlock();
            LBasicBlock slowPath = m_out.newBlock();
            LBasicBlock setup = m_out.newBlock();
            LBasicBlock slowCallPath = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue beyondVectorLength = m_out.above(newLength, m_out.load32(storage, m_heaps.ArrayStorage_vectorLength));

            m_out.branch(beyondVectorLength, rarely(slowPath), usually(fastPath));

            LBasicBlock lastNext = m_out.appendTo(fastPath, slowPath);
            m_out.store32(newLength, storage, m_heaps.ArrayStorage_publicLength);
            m_out.store32(
                m_out.add(m_out.load32(storage, m_heaps.ArrayStorage_numValuesInVector), m_out.constInt32(elementCount)),
                storage, m_heaps.ArrayStorage_numValuesInVector);
            ValueFromBlock fastBufferResult = m_out.anchor(m_out.baseIndex(storage, m_out.zeroExtPtr(prevLength), ScaleEight, ArrayStorage::vectorOffset()));
            m_out.jump(setup);

            m_out.appendTo(slowPath, setup);
            size_t scratchSize = sizeof(EncodedJSValue) * elementCount;
            ASSERT(scratchSize);
            ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
            m_out.storePtr(m_out.constIntPtr(scratchSize), m_out.absolute(scratchBuffer->addressOfActiveLength()));
            ValueFromBlock slowBufferResult = m_out.anchor(m_out.constIntPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())));
            m_out.jump(setup);

            m_out.appendTo(setup, slowCallPath);
            LValue buffer = m_out.phi(pointerType(), fastBufferResult, slowBufferResult);
            for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
                Edge& element = m_graph.varArgChild(m_node, elementIndex + elementOffset);

                LValue value = lowJSValue(element);
                m_out.store64(value, m_out.baseIndex(m_heaps.ArrayStorage_vector.atAnyIndex(), buffer, m_out.constIntPtr(elementIndex), ScaleEight));
            }
            ValueFromBlock fastResult = m_out.anchor(boxInt32(newLength));

            m_out.branch(beyondVectorLength, rarely(slowCallPath), usually(continuation));

            m_out.appendTo(slowCallPath, continuation);
            ValueFromBlock slowResult = m_out.anchor(vmCall(Int64, m_out.operation(operationArrayPushMultiple), m_callFrame, base, buffer, m_out.constInt32(elementCount)));
            m_out.storePtr(m_out.constIntPtr(0), m_out.absolute(scratchBuffer->addressOfActiveLength()));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, fastResult, slowResult));
            return;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad array type");
            return;
        }
    }

    std::pair<LValue, LValue> populateSliceRange(LValue start, LValue end, LValue length)
    {
        // end can be nullptr.
        ASSERT(start);
        ASSERT(length);

        auto pickIndex = [&] (LValue index) {
            return m_out.select(m_out.greaterThanOrEqual(index, m_out.int32Zero),
                m_out.select(m_out.above(index, length), length, index),
                m_out.select(m_out.lessThan(m_out.add(length, index), m_out.int32Zero), m_out.int32Zero, m_out.add(length, index)));
        };

        LValue endBoundary = length;
        if (end)
            endBoundary = pickIndex(end);
        LValue startIndex = pickIndex(start);
        return std::make_pair(startIndex, endBoundary);
    }

    void compileArraySlice()
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);

        LValue sourceStorage = lowStorage(m_graph.varArgChild(m_node, m_node->numChildren() - 1));
        LValue inputLength = m_out.load32(sourceStorage, m_heaps.Butterfly_publicLength);

        LValue startIndex = nullptr;
        LValue resultLength = nullptr;
        if (m_node->numChildren() == 2) {
            startIndex = m_out.constInt32(0);
            resultLength = inputLength;
        } else {
            LValue start = lowInt32(m_graph.varArgChild(m_node, 1));
            LValue end = nullptr;
            if (m_node->numChildren() != 3)
                end = lowInt32(m_graph.varArgChild(m_node, 2));

            auto range = populateSliceRange(start, end, inputLength);
            startIndex = range.first;
            LValue endBoundary = range.second;

            resultLength = m_out.select(m_out.belowOrEqual(startIndex, endBoundary),
                m_out.sub(endBoundary, startIndex),
                m_out.constInt32(0));
        }

        ArrayValues arrayResult;
        {
            LValue indexingType = m_out.load8ZeroExt32(lowCell(m_graph.varArgChild(m_node, 0)), m_heaps.JSCell_indexingTypeAndMisc);
            // We can ignore the writability of the cell since we won't write to the source.
            indexingType = m_out.bitAnd(indexingType, m_out.constInt32(AllWritableArrayTypesAndHistory));
            // When we emit an ArraySlice, we dominate the use of the array by a CheckStructure
            // to ensure the incoming array is one to be one of the original array structures
            // with one of the following indexing shapes: Int32, Contiguous, Double.
            LValue structure = m_out.select(
                m_out.equal(indexingType, m_out.constInt32(ArrayWithInt32)),
                weakStructure(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithInt32))),
                m_out.select(m_out.equal(indexingType, m_out.constInt32(ArrayWithContiguous)),
                    weakStructure(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous))),
                    weakStructure(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithDouble)))));
            arrayResult = allocateJSArray(resultLength, resultLength, structure, indexingType, false, false);
        }

        LBasicBlock loop = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        resultLength = m_out.zeroExtPtr(resultLength);
        ValueFromBlock startLoadIndex = m_out.anchor(m_out.zeroExtPtr(startIndex));
        ValueFromBlock startStoreIndex = m_out.anchor(m_out.constIntPtr(0));

        m_out.branch(
            m_out.below(m_out.constIntPtr(0), resultLength), unsure(loop), unsure(continuation));

        LBasicBlock lastNext = m_out.appendTo(loop, continuation);
        LValue storeIndex = m_out.phi(pointerType(), startStoreIndex);
        LValue loadIndex = m_out.phi(pointerType(), startLoadIndex);
        LValue value = m_out.load64(m_out.baseIndex(m_heaps.root, sourceStorage, loadIndex, ScaleEight));
        m_out.store64(value, m_out.baseIndex(m_heaps.root, arrayResult.butterfly, storeIndex, ScaleEight));
        LValue nextStoreIndex = m_out.add(storeIndex, m_out.constIntPtr(1));
        m_out.addIncomingToPhi(storeIndex, m_out.anchor(nextStoreIndex));
        m_out.addIncomingToPhi(loadIndex, m_out.anchor(m_out.add(loadIndex, m_out.constIntPtr(1))));
        m_out.branch(
            m_out.below(nextStoreIndex, resultLength), unsure(loop), unsure(continuation));

        m_out.appendTo(continuation, lastNext);

        mutatorFence();
        setJSValue(arrayResult.array);
    }

    void compileArrayIndexOf()
    {
        LValue storage = lowStorage(m_node->numChildren() == 3 ? m_graph.varArgChild(m_node, 2) : m_graph.varArgChild(m_node, 3));
        LValue length = m_out.load32(storage, m_heaps.Butterfly_publicLength);

        LValue startIndex;
        if (m_node->numChildren() == 4) {
            startIndex = lowInt32(m_graph.varArgChild(m_node, 2));
            startIndex = m_out.select(m_out.greaterThanOrEqual(startIndex, m_out.int32Zero),
                m_out.select(m_out.above(startIndex, length), length, startIndex),
                m_out.select(m_out.lessThan(m_out.add(length, startIndex), m_out.int32Zero), m_out.int32Zero, m_out.add(length, startIndex)));
        } else
            startIndex = m_out.int32Zero;

        Edge& searchElementEdge = m_graph.varArgChild(m_node, 1);
        switch (searchElementEdge.useKind()) {
        case Int32Use:
        case ObjectUse:
        case SymbolUse:
        case OtherUse:
        case DoubleRepUse: {
            LBasicBlock loopHeader = m_out.newBlock();
            LBasicBlock loopBody = m_out.newBlock();
            LBasicBlock loopNext = m_out.newBlock();
            LBasicBlock notFound = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue searchElement;
            switch (searchElementEdge.useKind()) {
            case Int32Use:
                ASSERT(m_node->arrayMode().type() == Array::Int32);
                speculate(searchElementEdge);
                searchElement = lowJSValue(searchElementEdge, ManualOperandSpeculation);
                break;
            case ObjectUse:
                ASSERT(m_node->arrayMode().type() == Array::Contiguous);
                searchElement = lowObject(searchElementEdge);
                break;
            case SymbolUse:
                ASSERT(m_node->arrayMode().type() == Array::Contiguous);
                searchElement = lowSymbol(searchElementEdge);
                break;
            case OtherUse:
                ASSERT(m_node->arrayMode().type() == Array::Contiguous);
                speculate(searchElementEdge);
                searchElement = lowJSValue(searchElementEdge, ManualOperandSpeculation);
                break;
            case DoubleRepUse:
                ASSERT(m_node->arrayMode().type() == Array::Double);
                searchElement = lowDouble(searchElementEdge);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            startIndex = m_out.zeroExtPtr(startIndex);
            length = m_out.zeroExtPtr(length);

            ValueFromBlock initialStartIndex = m_out.anchor(startIndex);
            m_out.jump(loopHeader);

            LBasicBlock lastNext = m_out.appendTo(loopHeader, loopBody);
            LValue index = m_out.phi(pointerType(), initialStartIndex);
            m_out.branch(m_out.notEqual(index, length), unsure(loopBody), unsure(notFound));

            m_out.appendTo(loopBody, loopNext);
            ValueFromBlock foundResult = m_out.anchor(index);
            switch (searchElementEdge.useKind()) {
            case Int32Use: {
                // Empty value is ignored because of TagTypeNumber.
                LValue value = m_out.load64(m_out.baseIndex(m_heaps.indexedInt32Properties, storage, index));
                m_out.branch(m_out.equal(value, searchElement), unsure(continuation), unsure(loopNext));
                break;
            }
            case ObjectUse:
            case SymbolUse:
            case OtherUse: {
                // Empty value never matches against non-empty JS values.
                LValue value = m_out.load64(m_out.baseIndex(m_heaps.indexedContiguousProperties, storage, index));
                m_out.branch(m_out.equal(value, searchElement), unsure(continuation), unsure(loopNext));
                break;
            }
            case DoubleRepUse: {
                // Empty value is ignored because of NaN.
                LValue value = m_out.loadDouble(m_out.baseIndex(m_heaps.indexedDoubleProperties, storage, index));
                m_out.branch(m_out.doubleEqual(value, searchElement), unsure(continuation), unsure(loopNext));
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            m_out.appendTo(loopNext, notFound);
            LValue nextIndex = m_out.add(index, m_out.intPtrOne);
            m_out.addIncomingToPhi(index, m_out.anchor(nextIndex));
            m_out.jump(loopHeader);

            m_out.appendTo(notFound, continuation);
            ValueFromBlock notFoundResult = m_out.anchor(m_out.constIntPtr(-1));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setInt32(m_out.castToInt32(m_out.phi(pointerType(), notFoundResult, foundResult)));
            break;
        }

        case StringUse:
            ASSERT(m_node->arrayMode().type() == Array::Contiguous);
            setInt32(vmCall(Int32, m_out.operation(operationArrayIndexOfString), m_callFrame, storage, lowString(searchElementEdge), startIndex));
            break;

        case UntypedUse:
            switch (m_node->arrayMode().type()) {
            case Array::Double:
                setInt32(vmCall(Int32, m_out.operation(operationArrayIndexOfValueDouble), m_callFrame, storage, lowJSValue(searchElementEdge), startIndex));
                break;
            case Array::Int32:
            case Array::Contiguous:
                setInt32(vmCall(Int32, m_out.operation(operationArrayIndexOfValueInt32OrContiguous), m_callFrame, storage, lowJSValue(searchElementEdge), startIndex));
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            break;

        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
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
                Int64, m_out.operation(operationArrayPopAndRecoverLength), m_callFrame, base)));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, results));
            return;
        }

        case Array::ArrayStorage: {
            LBasicBlock vectorLengthCheckCase = m_out.newBlock();
            LBasicBlock popCheckCase = m_out.newBlock();
            LBasicBlock fastCase = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue prevLength = m_out.load32(storage, m_heaps.ArrayStorage_publicLength);

            Vector<ValueFromBlock, 3> results;
            results.append(m_out.anchor(m_out.constInt64(JSValue::encode(jsUndefined()))));
            m_out.branch(
                m_out.isZero32(prevLength), rarely(continuation), usually(vectorLengthCheckCase));

            LBasicBlock lastNext = m_out.appendTo(vectorLengthCheckCase, popCheckCase);
            LValue newLength = m_out.sub(prevLength, m_out.int32One);
            m_out.branch(
                m_out.aboveOrEqual(newLength, m_out.load32(storage, m_heaps.ArrayStorage_vectorLength)), rarely(slowCase), usually(popCheckCase));

            m_out.appendTo(popCheckCase, fastCase);
            TypedPointer pointer = m_out.baseIndex(m_heaps.ArrayStorage_vector, storage, m_out.zeroExtPtr(newLength));
            LValue result = m_out.load64(pointer);
            m_out.branch(m_out.notZero64(result), usually(fastCase), rarely(slowCase));

            m_out.appendTo(fastCase, slowCase);
            m_out.store32(newLength, storage, m_heaps.ArrayStorage_publicLength);
            m_out.store64(m_out.int64Zero, pointer);
            m_out.store32(
                m_out.sub(m_out.load32(storage, m_heaps.ArrayStorage_numValuesInVector), m_out.int32One),
                storage, m_heaps.ArrayStorage_numValuesInVector);
            results.append(m_out.anchor(result));
            m_out.jump(continuation);

            m_out.appendTo(slowCase, continuation);
            results.append(m_out.anchor(vmCall(
                Int64, m_out.operation(operationArrayPop), m_callFrame, base)));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, results));
            return;
        }

        default:
            DFG_CRASH(m_graph, m_node, "Bad array type");
            return;
        }
    }

    void compilePushWithScope()
    {
        LValue parentScope = lowCell(m_node->child1());
        auto objectEdge = m_node->child2();
        if (objectEdge.useKind() == ObjectUse) {
            LValue object = lowNonNullObject(objectEdge);
            LValue result = vmCall(Int64, m_out.operation(operationPushWithScopeObject), m_callFrame, parentScope, object);
            setJSValue(result);
        } else {
            ASSERT(objectEdge.useKind() == UntypedUse);
            LValue object = lowJSValue(m_node->child2());
            LValue result = vmCall(Int64, m_out.operation(operationPushWithScope), m_callFrame, parentScope, object);
            setJSValue(result);
        }
    }

    void compileCreateActivation()
    {
        LValue scope = lowCell(m_node->child1());
        SymbolTable* table = m_node->castOperand<SymbolTable*>();
        RegisteredStructure structure = m_graph.registerStructure(m_graph.globalObjectFor(m_node->origin.semantic)->activationStructure());
        JSValue initializationValue = m_node->initializationValueForActivation();
        ASSERT(initializationValue.isUndefined() || initializationValue == jsTDZValue());
        if (table->singletonScope()->isStillValid()) {
            LValue callResult = vmCall(
                Int64,
                m_out.operation(operationCreateActivationDirect), m_callFrame, weakStructure(structure),
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
                fastObject, m_heaps.JSLexicalEnvironment_variables[i]);
        }
        
        mutatorFence();
        
        ValueFromBlock fastResult = m_out.anchor(fastObject);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        VM& vm = this->vm();
        LValue callResult = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
                    operationCreateActivationDirect, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure.get()), locations[1].directGPR(),
                    CCallHelpers::TrustedImmPtr(table),
                    CCallHelpers::TrustedImm64(JSValue::encode(initializationValue)));
            },
            scope);
        ValueFromBlock slowResult = m_out.anchor(callResult);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(pointerType(), fastResult, slowResult));
    }
    
    void compileNewFunction()
    {
        ASSERT(m_node->op() == NewFunction || m_node->op() == NewGeneratorFunction || m_node->op() == NewAsyncGeneratorFunction || m_node->op() == NewAsyncFunction);
        bool isGeneratorFunction = m_node->op() == NewGeneratorFunction;
        bool isAsyncFunction = m_node->op() == NewAsyncFunction;
        bool isAsyncGeneratorFunction =  m_node->op() == NewAsyncGeneratorFunction;
        
        LValue scope = lowCell(m_node->child1());
        
        FunctionExecutable* executable = m_node->castOperand<FunctionExecutable*>();
        if (executable->singletonFunction()->isStillValid()) {
            LValue callResult =
                isGeneratorFunction ? vmCall(Int64, m_out.operation(operationNewGeneratorFunction), m_callFrame, scope, weakPointer(executable)) :
                isAsyncFunction ? vmCall(Int64, m_out.operation(operationNewAsyncFunction), m_callFrame, scope, weakPointer(executable)) :
                isAsyncGeneratorFunction ? vmCall(Int64, m_out.operation(operationNewAsyncGeneratorFunction), m_callFrame, scope, weakPointer(executable)) :
                vmCall(Int64, m_out.operation(operationNewFunction), m_callFrame, scope, weakPointer(executable));
            setJSValue(callResult);
            return;
        }

        RegisteredStructure structure = m_graph.registerStructure(
            [&] () {
                JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
                switch (m_node->op()) {
                case NewGeneratorFunction:
                    return globalObject->generatorFunctionStructure();
                case NewAsyncFunction:
                    return globalObject->asyncFunctionStructure();
                case NewAsyncGeneratorFunction:
                    return globalObject->asyncGeneratorFunctionStructure();
                case NewFunction:
                    return JSFunction::selectStructureForNewFuncExp(globalObject, m_node->castOperand<FunctionExecutable*>());
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }());
        
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        LValue fastObject =
            isGeneratorFunction ? allocateObject<JSGeneratorFunction>(structure, m_out.intPtrZero, slowPath) :
            isAsyncFunction ? allocateObject<JSAsyncFunction>(structure, m_out.intPtrZero, slowPath) :
            isAsyncGeneratorFunction ? allocateObject<JSAsyncGeneratorFunction>(structure, m_out.intPtrZero, slowPath) :
            allocateObject<JSFunction>(structure, m_out.intPtrZero, slowPath);
        
        
        // We don't need memory barriers since we just fast-created the function, so it
        // must be young.
        m_out.storePtr(scope, fastObject, m_heaps.JSFunction_scope);
        m_out.storePtr(weakPoisonedPointer<JSFunctionPoison>(executable), fastObject, m_heaps.JSFunction_executable);
        m_out.storePtr(m_out.intPtrZero, fastObject, m_heaps.JSFunction_rareData);
        
        mutatorFence();
        
        ValueFromBlock fastResult = m_out.anchor(fastObject);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);

        Vector<LValue> slowPathArguments;
        slowPathArguments.append(scope);
        VM& vm = this->vm();
        LValue callResult = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                auto* operation = operationNewFunctionWithInvalidatedReallocationWatchpoint;
                if (isGeneratorFunction)
                    operation = operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint;
                else if (isAsyncFunction)
                    operation = operationNewAsyncFunctionWithInvalidatedReallocationWatchpoint;
                else if (isAsyncGeneratorFunction)
                    operation = operationNewAsyncGeneratorFunctionWithInvalidatedReallocationWatchpoint;

                return createLazyCallGenerator(vm, operation,
                    locations[0].directGPR(), locations[1].directGPR(),
                    CCallHelpers::TrustedImmPtr(executable));
            },
            slowPathArguments);
        ValueFromBlock slowResult = m_out.anchor(callResult);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(pointerType(), fastResult, slowResult));
    }
    
    void compileCreateDirectArguments()
    {
        // FIXME: A more effective way of dealing with the argument count and callee is to have
        // them be explicit arguments to this node.
        // https://bugs.webkit.org/show_bug.cgi?id=142207
        
        RegisteredStructure structure =
            m_graph.registerStructure(m_graph.globalObjectFor(m_node->origin.semantic)->directArgumentsStructure());
        
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
                m_out.zeroExtPtr(size), structure, m_out.intPtrZero, slowPath);
        }
        
        m_out.store32(length.value, fastObject, m_heaps.DirectArguments_length);
        m_out.store32(m_out.constInt32(minCapacity), fastObject, m_heaps.DirectArguments_minCapacity);
        m_out.storePtr(m_out.intPtrZero, fastObject, m_heaps.DirectArguments_mappedArguments);
        m_out.storePtr(m_out.intPtrZero, fastObject, m_heaps.DirectArguments_modifiedArgumentsDescriptor);
        
        ValueFromBlock fastResult = m_out.anchor(fastObject);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        VM& vm = this->vm();
        LValue callResult = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
                    operationCreateDirectArguments, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure.get()), locations[1].directGPR(),
                    CCallHelpers::TrustedImm32(minCapacity));
            }, length.value);
        ValueFromBlock slowResult = m_out.anchor(callResult);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        LValue result = m_out.phi(pointerType(), fastResult, slowResult);

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
            LValue previousIndex = m_out.phi(pointerType(), originalLength);
            LValue index = m_out.sub(previousIndex, m_out.intPtrOne);
            m_out.store64(
                m_out.load64(m_out.baseIndex(m_heaps.variables, stackBase, index)),
                m_out.baseIndex(m_heaps.DirectArguments_storage, result, index));
            ValueFromBlock nextIndex = m_out.anchor(index);
            m_out.addIncomingToPhi(previousIndex, nextIndex);
            m_out.branch(m_out.isNull(index), unsure(end), unsure(loop));
            
            m_out.appendTo(end, lastNext);
        }
        
        mutatorFence();
        
        setJSValue(result);
    }
    
    void compileCreateScopedArguments()
    {
        LValue scope = lowCell(m_node->child1());
        
        LValue result = vmCall(
            Int64, m_out.operation(operationCreateScopedArguments), m_callFrame,
            weakPointer(
                m_graph.globalObjectFor(m_node->origin.semantic)->scopedArgumentsStructure()),
            getArgumentsStart(), getArgumentsLength().value, getCurrentCallee(), scope);
        
        setJSValue(result);
    }
    
    void compileCreateClonedArguments()
    {
        LValue result = vmCall(
            Int64, m_out.operation(operationCreateClonedArguments), m_callFrame,
            weakPointer(
                m_graph.globalObjectFor(m_node->origin.semantic)->clonedArgumentsStructure()),
            getArgumentsStart(), getArgumentsLength().value, getCurrentCallee());
        
        setJSValue(result);
    }

    void compileCreateRest()
    {
        if (m_graph.isWatchingHavingABadTimeWatchpoint(m_node)) {
            LBasicBlock continuation = m_out.newBlock();
            LValue arrayLength = lowInt32(m_node->child1());
            LBasicBlock loopStart = m_out.newBlock();
            JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
            RegisteredStructure structure = m_graph.registerStructure(globalObject->originalRestParameterStructure());
            ArrayValues arrayValues = allocateUninitializedContiguousJSArray(arrayLength, structure);
            LValue array = arrayValues.array;
            LValue butterfly = arrayValues.butterfly;
            ValueFromBlock startLength = m_out.anchor(arrayLength);
            LValue argumentRegion = m_out.add(getArgumentsStart(), m_out.constInt64(sizeof(Register) * m_node->numberOfArgumentsToSkip()));
            m_out.branch(m_out.equal(arrayLength, m_out.constInt32(0)),
                unsure(continuation), unsure(loopStart));

            LBasicBlock lastNext = m_out.appendTo(loopStart, continuation);
            LValue phiOffset = m_out.phi(Int32, startLength);
            LValue currentOffset = m_out.sub(phiOffset, m_out.int32One);
            m_out.addIncomingToPhi(phiOffset, m_out.anchor(currentOffset));
            LValue loadedValue = m_out.load64(m_out.baseIndex(m_heaps.variables, argumentRegion, m_out.zeroExtPtr(currentOffset)));
            IndexedAbstractHeap& heap = m_heaps.indexedContiguousProperties;
            m_out.store64(loadedValue, m_out.baseIndex(heap, butterfly, m_out.zeroExtPtr(currentOffset)));
            m_out.branch(m_out.equal(currentOffset, m_out.constInt32(0)), unsure(continuation), unsure(loopStart));

            m_out.appendTo(continuation, lastNext);
            mutatorFence();
            setJSValue(array);
            return;
        }

        LValue arrayLength = lowInt32(m_node->child1());
        LValue argumentStart = getArgumentsStart();
        LValue numberOfArgumentsToSkip = m_out.constInt32(m_node->numberOfArgumentsToSkip());
        setJSValue(vmCall(
            Int64, m_out.operation(operationCreateRest), m_callFrame, argumentStart, numberOfArgumentsToSkip, arrayLength));
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
        setInt32(m_out.phi(Int32, zeroLengthResult, nonZeroLengthResult));
    }

    void compileObjectKeys()
    {
        switch (m_node->child1().useKind()) {
        case ObjectUse: {
            if (m_graph.isWatchingHavingABadTimeWatchpoint(m_node)) {
                LBasicBlock notNullCase = m_out.newBlock();
                LBasicBlock rareDataCase = m_out.newBlock();
                LBasicBlock useCacheCase = m_out.newBlock();
                LBasicBlock slowButArrayBufferCase = m_out.newBlock();
                LBasicBlock slowCase = m_out.newBlock();
                LBasicBlock continuation = m_out.newBlock();

                LValue object = lowObject(m_node->child1());
                LValue structure = loadStructure(object);
                LValue previousOrRareData = m_out.loadPtr(structure, m_heaps.Structure_previousOrRareData);
                m_out.branch(m_out.notNull(previousOrRareData), unsure(notNullCase), unsure(slowCase));

                LBasicBlock lastNext = m_out.appendTo(notNullCase, rareDataCase);
                m_out.branch(
                    m_out.notEqual(m_out.load32(previousOrRareData, m_heaps.JSCell_structureID), m_out.constInt32(m_graph.m_vm.structureStructure->structureID())),
                    unsure(rareDataCase), unsure(slowCase));

                m_out.appendTo(rareDataCase, useCacheCase);
                ASSERT(bitwise_cast<uintptr_t>(StructureRareData::cachedOwnKeysSentinel()) == 1);
                LValue cachedOwnKeys = m_out.loadPtr(previousOrRareData, m_heaps.StructureRareData_cachedOwnKeys);
                m_out.branch(m_out.belowOrEqual(cachedOwnKeys, m_out.constIntPtr(bitwise_cast<void*>(StructureRareData::cachedOwnKeysSentinel()))), unsure(slowCase), unsure(useCacheCase));

                m_out.appendTo(useCacheCase, slowButArrayBufferCase);
                JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
                RegisteredStructure arrayStructure = m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(CopyOnWriteArrayWithContiguous));
                LValue fastArray = allocateObject<JSArray>(arrayStructure, m_out.addPtr(cachedOwnKeys, JSImmutableButterfly::offsetOfData()), slowButArrayBufferCase);
                ValueFromBlock fastResult = m_out.anchor(fastArray);
                m_out.jump(continuation);

                m_out.appendTo(slowButArrayBufferCase, slowCase);
                LValue slowArray = vmCall(Int64, m_out.operation(operationNewArrayBuffer), m_callFrame, weakStructure(arrayStructure), cachedOwnKeys);
                ValueFromBlock slowButArrayBufferResult = m_out.anchor(slowArray);
                m_out.jump(continuation);

                m_out.appendTo(slowCase, continuation);
                VM& vm = this->vm();
                LValue slowResultValue = lazySlowPath(
                    [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                        return createLazyCallGenerator(vm,
                            operationObjectKeysObject, locations[0].directGPR(), locations[1].directGPR());
                    },
                    object);
                ValueFromBlock slowResult = m_out.anchor(slowResultValue);
                m_out.jump(continuation);

                m_out.appendTo(continuation, lastNext);
                setJSValue(m_out.phi(pointerType(), fastResult, slowButArrayBufferResult, slowResult));
                break;
            }
            setJSValue(vmCall(Int64, m_out.operation(operationObjectKeysObject), m_callFrame, lowObject(m_node->child1())));
            break;
        }
        case UntypedUse:
            setJSValue(vmCall(Int64, m_out.operation(operationObjectKeys), m_callFrame, lowJSValue(m_node->child1())));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    void compileObjectCreate()
    {
        switch (m_node->child1().useKind()) {
        case ObjectUse:
            setJSValue(vmCall(Int64, m_out.operation(operationObjectCreateObject), m_callFrame, lowObject(m_node->child1())));
            break;
        case UntypedUse:
            setJSValue(vmCall(Int64, m_out.operation(operationObjectCreate), m_callFrame, lowJSValue(m_node->child1())));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    
    void compileNewObject()
    {
        setJSValue(allocateObject(m_node->structure()));
        mutatorFence();
    }

    void compileNewStringObject()
    {
        RegisteredStructure structure = m_node->structure();
        LValue string = lowString(m_node->child1());

        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowCase);

        LValue fastResultValue = allocateObject<StringObject>(structure, m_out.intPtrZero, slowCase);
        m_out.storePtr(m_out.constIntPtr(PoisonedClassInfoPtr(StringObject::info()).bits()), fastResultValue, m_heaps.JSDestructibleObject_classInfo);
        m_out.store64(string, fastResultValue, m_heaps.JSWrapperObject_internalValue);
        mutatorFence();
        ValueFromBlock fastResult = m_out.anchor(fastResultValue);
        m_out.jump(continuation);

        m_out.appendTo(slowCase, continuation);
        VM& vm = this->vm();
        LValue slowResultValue = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
                    operationNewStringObject, locations[0].directGPR(), locations[1].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure.get()));
            },
            string);
        ValueFromBlock slowResult = m_out.anchor(slowResultValue);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(pointerType(), fastResult, slowResult));
    }

    void compileNewSymbol()
    {
        if (!m_node->child1()) {
            setJSValue(vmCall(pointerType(), m_out.operation(operationNewSymbol), m_callFrame));
            return;
        }
        ASSERT(m_node->child1().useKind() == KnownStringUse);
        setJSValue(vmCall(pointerType(), m_out.operation(operationNewSymbolWithDescription), m_callFrame, lowString(m_node->child1())));
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
        RegisteredStructure structure = m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(
            m_node->indexingType()));

        if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(m_node->indexingType())) {
            unsigned numElements = m_node->numChildren();
            unsigned vectorLengthHint = m_node->vectorLengthHint();
            ASSERT(vectorLengthHint >= numElements);
            
            ArrayValues arrayValues =
                allocateUninitializedContiguousJSArray(numElements, vectorLengthHint, structure);
            
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
            mutatorFence();
            return;
        }
        
        if (!m_node->numChildren()) {
            setJSValue(vmCall(
                Int64, m_out.operation(operationNewEmptyArray), m_callFrame,
                weakStructure(structure)));
            return;
        }
        
        size_t scratchSize = sizeof(EncodedJSValue) * m_node->numChildren();
        ASSERT(scratchSize);
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());
        
        for (unsigned operandIndex = 0; operandIndex < m_node->numChildren(); ++operandIndex) {
            Edge edge = m_graph.varArgChild(m_node, operandIndex);
            LValue valueToStore;
            switch (m_node->indexingType()) {
            case ALL_DOUBLE_INDEXING_TYPES:
                valueToStore = boxDouble(lowDouble(edge));
                break;
            default:
                valueToStore = lowJSValue(edge, ManualOperandSpeculation);
                break;
            }
            m_out.store64(valueToStore, m_out.absolute(buffer + operandIndex));
        }
        
        m_out.storePtr(
            m_out.constIntPtr(scratchSize), m_out.absolute(scratchBuffer->addressOfActiveLength()));
        
        LValue result = vmCall(
            Int64, m_out.operation(operationNewArray), m_callFrame,
            weakStructure(structure), m_out.constIntPtr(buffer),
            m_out.constIntPtr(m_node->numChildren()));
        
        m_out.storePtr(m_out.intPtrZero, m_out.absolute(scratchBuffer->addressOfActiveLength()));
        
        setJSValue(result);
    }

    void compileNewArrayWithSpread()
    {
        if (m_graph.isWatchingHavingABadTimeWatchpoint(m_node)) {
            CheckedInt32 startLength = 0;
            BitVector* bitVector = m_node->bitVector();
            HashMap<InlineCallFrame*, LValue, WTF::DefaultHash<InlineCallFrame*>::Hash, WTF::NullableHashTraits<InlineCallFrame*>> cachedSpreadLengths;

            for (unsigned i = 0; i < m_node->numChildren(); ++i) {
                if (!bitVector->get(i))
                    ++startLength;
                else {
                    Edge& child = m_graph.varArgChild(m_node, i);
                    if (child->op() == PhantomSpread && child->child1()->op() == PhantomNewArrayBuffer)
                        startLength += child->child1()->castOperand<JSImmutableButterfly*>()->length();
                }
            }

            if (startLength.hasOverflowed()) {
                terminate(Overflow);
                return;
            }

            LValue length = m_out.constInt32(startLength.unsafeGet());

            for (unsigned i = 0; i < m_node->numChildren(); ++i) {
                if (bitVector->get(i)) {
                    Edge use = m_graph.varArgChild(m_node, i);
                    CheckValue* lengthCheck = nullptr;
                    if (use->op() == PhantomSpread) {
                        if (use->child1()->op() == PhantomCreateRest) {
                            InlineCallFrame* inlineCallFrame = use->child1()->origin.semantic.inlineCallFrame;
                            unsigned numberOfArgumentsToSkip = use->child1()->numberOfArgumentsToSkip();
                            LValue spreadLength = cachedSpreadLengths.ensure(inlineCallFrame, [&] () {
                                return getSpreadLengthFromInlineCallFrame(inlineCallFrame, numberOfArgumentsToSkip);
                            }).iterator->value;
                            lengthCheck = m_out.speculateAdd(length, spreadLength);
                        }
                    } else {
                        LValue fixedArray = lowCell(use);
                        lengthCheck = m_out.speculateAdd(length, m_out.load32(fixedArray, m_heaps.JSFixedArray_size));
                    }

                    if (lengthCheck) {
                        blessSpeculation(lengthCheck, Overflow, noValue(), nullptr, m_origin);
                        length = lengthCheck;
                    }
                }
            }

            RegisteredStructure structure = m_graph.registerStructure(m_graph.globalObjectFor(m_node->origin.semantic)->originalArrayStructureForIndexingType(ArrayWithContiguous));
            ArrayValues arrayValues = allocateUninitializedContiguousJSArray(length, structure);
            LValue result = arrayValues.array;
            LValue storage = arrayValues.butterfly;
            LValue index = m_out.constIntPtr(0);

            for (unsigned i = 0; i < m_node->numChildren(); ++i) {
                Edge use = m_graph.varArgChild(m_node, i);
                if (bitVector->get(i)) {
                    if (use->op() == PhantomSpread) {
                        if (use->child1()->op() == PhantomNewArrayBuffer) {
                            IndexedAbstractHeap& heap = m_heaps.indexedContiguousProperties;
                            auto* array = use->child1()->castOperand<JSImmutableButterfly*>();
                            for (unsigned i = 0; i < array->length(); ++i) {
                                // Because resulted array from NewArrayWithSpread is always contiguous, we should not generate value
                                // in Double form even if PhantomNewArrayBuffer's indexingType is ArrayWithDouble.
                                int64_t value = JSValue::encode(array->get(i));
                                m_out.store64(m_out.constInt64(value), m_out.baseIndex(heap, storage, index, JSValue(), (Checked<int32_t>(sizeof(JSValue)) * i).unsafeGet()));
                            }
                            index = m_out.add(index, m_out.constIntPtr(array->length()));
                        } else {
                            RELEASE_ASSERT(use->child1()->op() == PhantomCreateRest);
                            InlineCallFrame* inlineCallFrame = use->child1()->origin.semantic.inlineCallFrame;
                            unsigned numberOfArgumentsToSkip = use->child1()->numberOfArgumentsToSkip();

                            LValue length = m_out.zeroExtPtr(cachedSpreadLengths.get(inlineCallFrame));
                            LValue sourceStart = getArgumentsStart(inlineCallFrame, numberOfArgumentsToSkip);

                            LBasicBlock loopStart = m_out.newBlock();
                            LBasicBlock continuation = m_out.newBlock();

                            ValueFromBlock loadIndexStart = m_out.anchor(m_out.constIntPtr(0));
                            ValueFromBlock arrayIndexStart = m_out.anchor(index);
                            ValueFromBlock arrayIndexStartForFinish = m_out.anchor(index);

                            m_out.branch(
                                m_out.isZero64(length),
                                unsure(continuation), unsure(loopStart));

                            LBasicBlock lastNext = m_out.appendTo(loopStart, continuation);

                            LValue arrayIndex = m_out.phi(pointerType(), arrayIndexStart);
                            LValue loadIndex = m_out.phi(pointerType(), loadIndexStart);

                            LValue item = m_out.load64(m_out.baseIndex(m_heaps.variables, sourceStart, loadIndex));
                            m_out.store64(item, m_out.baseIndex(m_heaps.indexedContiguousProperties, storage, arrayIndex));

                            LValue nextArrayIndex = m_out.add(arrayIndex, m_out.constIntPtr(1));
                            LValue nextLoadIndex = m_out.add(loadIndex, m_out.constIntPtr(1));
                            ValueFromBlock arrayIndexLoopForFinish = m_out.anchor(nextArrayIndex);

                            m_out.addIncomingToPhi(loadIndex, m_out.anchor(nextLoadIndex));
                            m_out.addIncomingToPhi(arrayIndex, m_out.anchor(nextArrayIndex));

                            m_out.branch(
                                m_out.below(nextLoadIndex, length),
                                unsure(loopStart), unsure(continuation));

                            m_out.appendTo(continuation, lastNext);
                            index = m_out.phi(pointerType(), arrayIndexStartForFinish, arrayIndexLoopForFinish);
                        }
                    } else {
                        LBasicBlock loopStart = m_out.newBlock();
                        LBasicBlock continuation = m_out.newBlock();

                        LValue fixedArray = lowCell(use);

                        ValueFromBlock fixedIndexStart = m_out.anchor(m_out.constIntPtr(0));
                        ValueFromBlock arrayIndexStart = m_out.anchor(index);
                        ValueFromBlock arrayIndexStartForFinish = m_out.anchor(index);

                        LValue fixedArraySize = m_out.zeroExtPtr(m_out.load32(fixedArray, m_heaps.JSFixedArray_size));

                        m_out.branch(
                            m_out.isZero64(fixedArraySize),
                            unsure(continuation), unsure(loopStart));

                        LBasicBlock lastNext = m_out.appendTo(loopStart, continuation);

                        LValue arrayIndex = m_out.phi(pointerType(), arrayIndexStart);
                        LValue fixedArrayIndex = m_out.phi(pointerType(), fixedIndexStart);

                        LValue item = m_out.load64(m_out.baseIndex(m_heaps.JSFixedArray_buffer, fixedArray, fixedArrayIndex));
                        m_out.store64(item, m_out.baseIndex(m_heaps.indexedContiguousProperties, storage, arrayIndex));

                        LValue nextArrayIndex = m_out.add(arrayIndex, m_out.constIntPtr(1));
                        LValue nextFixedArrayIndex = m_out.add(fixedArrayIndex, m_out.constIntPtr(1));
                        ValueFromBlock arrayIndexLoopForFinish = m_out.anchor(nextArrayIndex);

                        m_out.addIncomingToPhi(fixedArrayIndex, m_out.anchor(nextFixedArrayIndex));
                        m_out.addIncomingToPhi(arrayIndex, m_out.anchor(nextArrayIndex));

                        m_out.branch(
                            m_out.below(nextFixedArrayIndex, fixedArraySize),
                            unsure(loopStart), unsure(continuation));

                        m_out.appendTo(continuation, lastNext);
                        index = m_out.phi(pointerType(), arrayIndexStartForFinish, arrayIndexLoopForFinish);
                    }
                } else {
                    IndexedAbstractHeap& heap = m_heaps.indexedContiguousProperties;
                    LValue item = lowJSValue(use);
                    m_out.store64(item, m_out.baseIndex(heap, storage, index));
                    index = m_out.add(index, m_out.constIntPtr(1));
                }
            }

            mutatorFence();
            setJSValue(result);
            return;
        }

        ASSERT(m_node->numChildren());
        size_t scratchSize = sizeof(EncodedJSValue) * m_node->numChildren();
        ScratchBuffer* scratchBuffer = vm().scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());
        BitVector* bitVector = m_node->bitVector();
        for (unsigned i = 0; i < m_node->numChildren(); ++i) {
            Edge use = m_graph.m_varArgChildren[m_node->firstChild() + i];
            LValue value;
            if (bitVector->get(i))
                value = lowCell(use);
            else
                value = lowJSValue(use);
            m_out.store64(value, m_out.absolute(&buffer[i]));
        }

        m_out.storePtr(m_out.constIntPtr(scratchSize), m_out.absolute(scratchBuffer->addressOfActiveLength()));
        LValue result = vmCall(Int64, m_out.operation(operationNewArrayWithSpreadSlow), m_callFrame, m_out.constIntPtr(buffer), m_out.constInt32(m_node->numChildren()));
        m_out.storePtr(m_out.constIntPtr(0), m_out.absolute(scratchBuffer->addressOfActiveLength()));

        setJSValue(result);
    }
    
    void compileCreateThis()
    {
        LValue callee = lowCell(m_node->child1());

        LBasicBlock isFunctionBlock = m_out.newBlock();
        LBasicBlock hasRareData = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(isFunction(callee, provenType(m_node->child1())), usually(isFunctionBlock), rarely(slowPath));

        LBasicBlock lastNext = m_out.appendTo(isFunctionBlock, hasRareData);
        LValue rareData = m_out.loadPtr(callee, m_heaps.JSFunction_rareData);
        m_out.branch(m_out.isZero64(rareData), rarely(slowPath), usually(hasRareData));

        m_out.appendTo(hasRareData, slowPath);
        LValue allocator = m_out.loadPtr(rareData, m_heaps.FunctionRareData_allocator);
        LValue structure = m_out.loadPtr(rareData, m_heaps.FunctionRareData_structure);
        LValue butterfly = m_out.constIntPtr(0);
        ValueFromBlock fastResult = m_out.anchor(allocateObject(allocator, structure, butterfly, slowPath));
        m_out.jump(continuation);

        m_out.appendTo(slowPath, continuation);
        ValueFromBlock slowResult = m_out.anchor(vmCall(
            Int64, m_out.operation(operationCreateThis), m_callFrame, callee, m_out.constInt32(m_node->inlineCapacity())));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        LValue result = m_out.phi(Int64, fastResult, slowResult);

        mutatorFence();
        setJSValue(result);
    }

    void compileSpread()
    {
        if (m_node->child1()->op() == PhantomNewArrayBuffer) {
            LBasicBlock slowAllocation = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            auto* immutableButterfly = m_node->child1()->castOperand<JSImmutableButterfly*>();

            LValue fastFixedArrayValue = allocateVariableSizedCell<JSFixedArray>(
                m_out.constIntPtr(JSFixedArray::allocationSize(immutableButterfly->length()).unsafeGet()),
                m_graph.m_vm.fixedArrayStructure.get(), slowAllocation);
            m_out.store32(m_out.constInt32(immutableButterfly->length()), fastFixedArrayValue, m_heaps.JSFixedArray_size);
            ValueFromBlock fastFixedArray = m_out.anchor(fastFixedArrayValue);
            m_out.jump(continuation);

            LBasicBlock lastNext = m_out.appendTo(slowAllocation, continuation);
            ValueFromBlock slowFixedArray = m_out.anchor(vmCall(pointerType(), m_out.operation(operationCreateFixedArray), m_callFrame, m_out.constInt32(immutableButterfly->length())));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            LValue fixedArray = m_out.phi(pointerType(), fastFixedArray, slowFixedArray);
            for (unsigned i = 0; i < immutableButterfly->length(); i++) {
                // Because forwarded values are drained as JSValue, we should not generate value
                // in Double form even if PhantomNewArrayBuffer's indexingType is ArrayWithDouble.
                int64_t value = JSValue::encode(immutableButterfly->get(i));
                m_out.store64(
                    m_out.constInt64(value),
                    m_out.baseIndex(m_heaps.JSFixedArray_buffer, fixedArray, m_out.constIntPtr(i), jsNumber(i)));
            }
            mutatorFence();
            setJSValue(fixedArray);
            return;
        }

        if (m_node->child1()->op() == PhantomCreateRest) {
            // This IR is rare to generate since it requires escaping the Spread
            // but not the CreateRest. In bytecode, we have only few operations that
            // accept Spread's result as input. This usually leads to the Spread node not
            // escaping. However, this can happen if for example we generate a PutStack on
            // the Spread but nothing escapes the CreateRest.
            LBasicBlock loopHeader = m_out.newBlock();
            LBasicBlock loopBody = m_out.newBlock();
            LBasicBlock slowAllocation = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            LBasicBlock lastNext = m_out.insertNewBlocksBefore(loopHeader);

            InlineCallFrame* inlineCallFrame = m_node->child1()->origin.semantic.inlineCallFrame;
            unsigned numberOfArgumentsToSkip = m_node->child1()->numberOfArgumentsToSkip();
            LValue sourceStart = getArgumentsStart(inlineCallFrame, numberOfArgumentsToSkip);
            LValue length = getSpreadLengthFromInlineCallFrame(inlineCallFrame, numberOfArgumentsToSkip);
            static_assert(sizeof(JSValue) == 8 && 1 << 3 == 8, "Assumed in the code below.");
            LValue size = m_out.add(
                m_out.shl(m_out.zeroExtPtr(length), m_out.constInt32(3)),
                m_out.constIntPtr(JSFixedArray::offsetOfData()));

            LValue fastArrayValue = allocateVariableSizedCell<JSFixedArray>(size, m_graph.m_vm.fixedArrayStructure.get(), slowAllocation);
            m_out.store32(length, fastArrayValue, m_heaps.JSFixedArray_size);
            ValueFromBlock fastArray = m_out.anchor(fastArrayValue);
            m_out.jump(loopHeader);

            m_out.appendTo(slowAllocation, loopHeader);
            ValueFromBlock slowArray = m_out.anchor(vmCall(pointerType(), m_out.operation(operationCreateFixedArray), m_callFrame, length));
            m_out.jump(loopHeader);

            m_out.appendTo(loopHeader, loopBody);
            LValue fixedArray = m_out.phi(pointerType(), fastArray, slowArray);
            ValueFromBlock startIndex = m_out.anchor(m_out.constIntPtr(0));
            m_out.branch(m_out.isZero32(length), unsure(continuation), unsure(loopBody));

            m_out.appendTo(loopBody, continuation);
            LValue index = m_out.phi(pointerType(), startIndex);
            LValue value = m_out.load64(
                m_out.baseIndex(m_heaps.variables, sourceStart, index));
            m_out.store64(value, m_out.baseIndex(m_heaps.JSFixedArray_buffer, fixedArray, index));
            LValue nextIndex = m_out.add(m_out.constIntPtr(1), index);
            m_out.addIncomingToPhi(index, m_out.anchor(nextIndex));
            m_out.branch(m_out.below(nextIndex, m_out.zeroExtPtr(length)), unsure(loopBody), unsure(continuation));

            m_out.appendTo(continuation, lastNext);
            mutatorFence();
            setJSValue(fixedArray);
            return;
        }

        LValue argument = lowCell(m_node->child1());

        LValue result;

        if (m_node->child1().useKind() == ArrayUse)
            speculateArray(m_node->child1());

        if (m_graph.canDoFastSpread(m_node, m_state.forNode(m_node->child1()))) {
            LBasicBlock preLoop = m_out.newBlock();
            LBasicBlock loopSelection = m_out.newBlock();
            LBasicBlock contiguousLoopStart = m_out.newBlock();
            LBasicBlock doubleLoopStart = m_out.newBlock();
            LBasicBlock slowPath = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue indexingShape = m_out.load8ZeroExt32(argument, m_heaps.JSCell_indexingTypeAndMisc);
            indexingShape = m_out.bitAnd(indexingShape, m_out.constInt32(IndexingShapeMask));
            LValue isOKIndexingType = m_out.belowOrEqual(
                m_out.sub(indexingShape, m_out.constInt32(Int32Shape)),
                m_out.constInt32(ContiguousShape - Int32Shape));

            m_out.branch(isOKIndexingType, unsure(preLoop), unsure(slowPath));
            LBasicBlock lastNext = m_out.appendTo(preLoop, loopSelection);

            LValue butterfly = m_out.loadPtr(argument, m_heaps.JSObject_butterfly);
            LValue length = m_out.load32NonNegative(butterfly, m_heaps.Butterfly_publicLength);
            static_assert(sizeof(JSValue) == 8 && 1 << 3 == 8, "Assumed in the code below.");
            LValue size = m_out.add(
                m_out.shl(m_out.zeroExtPtr(length), m_out.constInt32(3)),
                m_out.constIntPtr(JSFixedArray::offsetOfData()));

            LValue fastAllocation = allocateVariableSizedCell<JSFixedArray>(size, m_graph.m_vm.fixedArrayStructure.get(), slowPath);
            ValueFromBlock fastResult = m_out.anchor(fastAllocation);
            m_out.store32(length, fastAllocation, m_heaps.JSFixedArray_size);

            ValueFromBlock startIndexForContiguous = m_out.anchor(m_out.constIntPtr(0));
            ValueFromBlock startIndexForDouble = m_out.anchor(m_out.constIntPtr(0));

            m_out.branch(m_out.isZero32(length), unsure(continuation), unsure(loopSelection));

            m_out.appendTo(loopSelection, contiguousLoopStart);
            m_out.branch(m_out.equal(indexingShape, m_out.constInt32(DoubleShape)),
                unsure(doubleLoopStart), unsure(contiguousLoopStart));

            {
                m_out.appendTo(contiguousLoopStart, doubleLoopStart);
                LValue index = m_out.phi(pointerType(), startIndexForContiguous);

                TypedPointer loadSite = m_out.baseIndex(m_heaps.root, butterfly, index, ScaleEight); // We read TOP here since we can be reading either int32 or contiguous properties.
                LValue value = m_out.load64(loadSite);
                value = m_out.select(m_out.isZero64(value), m_out.constInt64(JSValue::encode(jsUndefined())), value);
                m_out.store64(value, m_out.baseIndex(m_heaps.JSFixedArray_buffer, fastAllocation, index));

                LValue nextIndex = m_out.add(index, m_out.constIntPtr(1));
                m_out.addIncomingToPhi(index, m_out.anchor(nextIndex));

                m_out.branch(m_out.below(nextIndex, m_out.zeroExtPtr(length)),
                    unsure(contiguousLoopStart), unsure(continuation));
            }

            {
                m_out.appendTo(doubleLoopStart, slowPath);
                LValue index = m_out.phi(pointerType(), startIndexForDouble);

                LValue value = m_out.loadDouble(m_out.baseIndex(m_heaps.indexedDoubleProperties, butterfly, index));
                LValue isNaN = m_out.doubleNotEqualOrUnordered(value, value);
                LValue holeResult = m_out.constInt64(JSValue::encode(jsUndefined()));
                LValue normalResult = boxDouble(value);
                value = m_out.select(isNaN, holeResult, normalResult);
                m_out.store64(value, m_out.baseIndex(m_heaps.JSFixedArray_buffer, fastAllocation, index));

                LValue nextIndex = m_out.add(index, m_out.constIntPtr(1));
                m_out.addIncomingToPhi(index, m_out.anchor(nextIndex));

                m_out.branch(m_out.below(nextIndex, m_out.zeroExtPtr(length)),
                    unsure(doubleLoopStart), unsure(continuation));
            }

            m_out.appendTo(slowPath, continuation);
            ValueFromBlock slowResult = m_out.anchor(vmCall(pointerType(), m_out.operation(operationSpreadFastArray), m_callFrame, argument));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            result = m_out.phi(pointerType(), fastResult, slowResult);
            mutatorFence();
        } else
            result = vmCall(pointerType(), m_out.operation(operationSpreadGeneric), m_callFrame, argument);

        setJSValue(result);
    }
    
    void compileNewArrayBuffer()
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        RegisteredStructure structure = m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(
            m_node->indexingMode()));
        auto* immutableButterfly = m_node->castOperand<JSImmutableButterfly*>();

        if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(m_node->indexingMode())) {
            LBasicBlock slowPath = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue fastArray = allocateObject<JSArray>(structure, m_out.constIntPtr(immutableButterfly->toButterfly()), slowPath);
            ValueFromBlock fastResult = m_out.anchor(fastArray);
            m_out.jump(continuation);

            m_out.appendTo(slowPath, continuation);
            LValue slowArray = vmCall(Int64, m_out.operation(operationNewArrayBuffer), m_callFrame, weakStructure(structure), m_out.weakPointer(m_node->cellOperand()));
            ValueFromBlock slowResult = m_out.anchor(slowArray);
            m_out.jump(continuation);

            m_out.appendTo(continuation);

            mutatorFence();
            setJSValue(m_out.phi(pointerType(), slowResult, fastResult));
            return;
        }
        
        setJSValue(vmCall(
            Int64, m_out.operation(operationNewArrayBuffer), m_callFrame,
            weakStructure(structure), m_out.weakPointer(m_node->cellOperand())));
    }

    void compileNewArrayWithSize()
    {
        LValue publicLength = lowInt32(m_node->child1());
        
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        RegisteredStructure structure = m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(
            m_node->indexingType()));
        
        if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(m_node->indexingType())) {
            IndexingType indexingType = m_node->indexingType();
            setJSValue(
                allocateJSArray(
                    publicLength, publicLength, weakPointer(globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType)), m_out.constInt32(indexingType)).array);
            mutatorFence();
            return;
        }
        
        LValue structureValue = m_out.select(
            m_out.aboveOrEqual(publicLength, m_out.constInt32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)),
            weakStructure(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage))),
            weakStructure(structure));
        setJSValue(vmCall(Int64, m_out.operation(operationNewArrayWithSize), m_callFrame, structureValue, publicLength, m_out.intPtrZero));
    }

    void compileNewTypedArray()
    {
        TypedArrayType typedArrayType = m_node->typedArrayType();
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        
        switch (m_node->child1().useKind()) {
        case Int32Use: {
            RegisteredStructure structure = m_graph.registerStructure(globalObject->typedArrayStructureConcurrently(typedArrayType));

            LValue size = lowInt32(m_node->child1());

            LBasicBlock smallEnoughCase = m_out.newBlock();
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            ValueFromBlock noStorage = m_out.anchor(m_out.intPtrZero);

            m_out.branch(
                m_out.above(size, m_out.constInt32(JSArrayBufferView::fastSizeLimit)),
                rarely(slowCase), usually(smallEnoughCase));

            LBasicBlock lastNext = m_out.appendTo(smallEnoughCase, slowCase);

            LValue byteSize =
                m_out.shl(m_out.zeroExtPtr(size), m_out.constInt32(logElementSize(typedArrayType)));
            if (elementSize(typedArrayType) < 8) {
                byteSize = m_out.bitAnd(
                    m_out.add(byteSize, m_out.constIntPtr(7)),
                    m_out.constIntPtr(~static_cast<intptr_t>(7)));
            }
        
            LValue allocator = allocatorForSize(vm().primitiveGigacageAuxiliarySpace, byteSize, slowCase);
            LValue storage = allocateHeapCell(allocator, slowCase);
            
            splatWords(
                storage,
                m_out.int32Zero,
                m_out.castToInt32(m_out.lShr(byteSize, m_out.constIntPtr(3))),
                m_out.int64Zero,
                m_heaps.typedArrayProperties);

            ValueFromBlock haveStorage = m_out.anchor(storage);

            LValue fastResultValue =
                allocateObject<JSArrayBufferView>(structure, m_out.intPtrZero, slowCase);

            m_out.storePtr(storage, fastResultValue, m_heaps.JSArrayBufferView_vector);
            m_out.store32(size, fastResultValue, m_heaps.JSArrayBufferView_length);
            m_out.store32(m_out.constInt32(FastTypedArray), fastResultValue, m_heaps.JSArrayBufferView_mode);
            
            mutatorFence();
            ValueFromBlock fastResult = m_out.anchor(fastResultValue);
            m_out.jump(continuation);

            m_out.appendTo(slowCase, continuation);
            LValue storageValue = m_out.phi(pointerType(), noStorage, haveStorage);

            VM& vm = this->vm();
            LValue slowResultValue = lazySlowPath(
                [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(vm,
                        operationNewTypedArrayWithSizeForType(typedArrayType), locations[0].directGPR(),
                        CCallHelpers::TrustedImmPtr(structure.get()), locations[1].directGPR(),
                        locations[2].directGPR());
                },
                size, storageValue);
            ValueFromBlock slowResult = m_out.anchor(slowResultValue);
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(pointerType(), fastResult, slowResult));
            return;
        }

        case UntypedUse: {
            LValue argument = lowJSValue(m_node->child1());

            LValue result = vmCall(
                pointerType(), m_out.operation(operationNewTypedArrayWithOneArgumentForType(typedArrayType)),
                m_callFrame, weakPointer(globalObject->typedArrayStructureConcurrently(typedArrayType)), argument);

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
        setStorage(allocatePropertyStorage(object, m_node->transition()->previous.get()));
    }

    void compileReallocatePropertyStorage()
    {
        Transition* transition = m_node->transition();
        LValue object = lowCell(m_node->child1());
        LValue oldStorage = lowStorage(m_node->child2());
        
        setStorage(
            reallocatePropertyStorage(
                object, oldStorage, transition->previous.get(), transition->next.get()));
    }
    
    void compileNukeStructureAndSetButterfly()
    {
        nukeStructureAndSetButterfly(lowStorage(m_node->child2()), lowCell(m_node->child1()));
    }

    void compileToNumber()
    {
        LValue value = lowJSValue(m_node->child1());

        if (!(abstractValue(m_node->child1()).m_type & SpecBytecodeNumber))
            setJSValue(vmCall(Int64, m_out.operation(operationToNumber), m_callFrame, value));
        else {
            LBasicBlock notNumber = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            ValueFromBlock fastResult = m_out.anchor(value);
            m_out.branch(isNumber(value, provenType(m_node->child1())), unsure(continuation), unsure(notNumber));

            // notNumber case.
            LBasicBlock lastNext = m_out.appendTo(notNumber, continuation);
            // We have several attempts to remove ToNumber. But ToNumber still exists.
            // It means that converting non-numbers to numbers by this ToNumber is not rare.
            // Instead of the lazy slow path generator, we call the operation here.
            ValueFromBlock slowResult = m_out.anchor(vmCall(Int64, m_out.operation(operationToNumber), m_callFrame, value));
            m_out.jump(continuation);

            // continuation case.
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, fastResult, slowResult));
        }
    }
    
    void compileToStringOrCallStringConstructorOrStringValueOf()
    {
        ASSERT(m_node->op() != StringValueOf || m_node->child1().useKind() == UntypedUse);
        switch (m_node->child1().useKind()) {
        case StringObjectUse: {
            LValue cell = lowCell(m_node->child1());
            speculateStringObjectForCell(m_node->child1(), cell);
            setJSValue(m_out.loadPtr(cell, m_heaps.JSWrapperObject_internalValue));
            return;
        }
            
        case StringOrStringObjectUse: {
            LValue cell = lowCell(m_node->child1());
            LValue type = m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType);
            
            LBasicBlock notString = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            ValueFromBlock simpleResult = m_out.anchor(cell);
            m_out.branch(
                m_out.equal(type, m_out.constInt32(StringType)),
                unsure(continuation), unsure(notString));
            
            LBasicBlock lastNext = m_out.appendTo(notString, continuation);
            speculate(
                BadType, jsValueValue(cell), m_node->child1().node(),
                m_out.notEqual(type, m_out.constInt32(StringObjectType)));
            ValueFromBlock unboxedResult = m_out.anchor(
                m_out.loadPtr(cell, m_heaps.JSWrapperObject_internalValue));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, simpleResult, unboxedResult));
            
            m_interpreter.filter(m_node->child1(), SpecString | SpecStringObject);
            return;
        }
            
        case CellUse:
        case NotCellUse:
        case UntypedUse: {
            LValue value;
            if (m_node->child1().useKind() == CellUse)
                value = lowCell(m_node->child1());
            else if (m_node->child1().useKind() == NotCellUse)
                value = lowNotCell(m_node->child1());
            else
                value = lowJSValue(m_node->child1());
            
            LBasicBlock isCell = m_out.newBlock();
            LBasicBlock notString = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            LValue isCellPredicate;
            if (m_node->child1().useKind() == CellUse)
                isCellPredicate = m_out.booleanTrue;
            else if (m_node->child1().useKind() == NotCellUse)
                isCellPredicate = m_out.booleanFalse;
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
            if (m_node->child1().useKind() == CellUse) {
                ASSERT(m_node->op() != StringValueOf);
                operation = m_out.operation(m_node->op() == ToString ? operationToStringOnCell : operationCallStringConstructorOnCell);
            } else {
                operation = m_out.operation(m_node->op() == ToString
                    ? operationToString : m_node->op() == StringValueOf
                    ? operationStringValueOf : operationCallStringConstructor);
            }
            ValueFromBlock convertedResult = m_out.anchor(vmCall(Int64, operation, m_callFrame, value));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setJSValue(m_out.phi(Int64, simpleResult, convertedResult));
            return;
        }

        case Int32Use:
            setJSValue(vmCall(Int64, m_out.operation(operationInt32ToStringWithValidRadix), m_callFrame, lowInt32(m_node->child1()), m_out.constInt32(10)));
            return;

        case Int52RepUse:
            setJSValue(vmCall(Int64, m_out.operation(operationInt52ToStringWithValidRadix), m_callFrame, lowStrictInt52(m_node->child1()), m_out.constInt32(10)));
            return;

        case DoubleRepUse:
            setJSValue(vmCall(Int64, m_out.operation(operationDoubleToStringWithValidRadix), m_callFrame, lowDouble(m_node->child1()), m_out.constInt32(10)));
            return;
            
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
            Int64, m_out.operation(operationToPrimitive), m_callFrame, value)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(Int64, results));
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
        
        Allocator allocator = allocatorForNonVirtualConcurrently<JSRopeString>(vm(), sizeof(JSRopeString), AllocatorForMode::AllocatorIfExists);
        
        LValue result = allocateCell(
            m_out.constIntPtr(allocator.localAllocator()), vm().stringStructure.get(), slowPath);
        
        m_out.storePtr(m_out.intPtrZero, result, m_heaps.JSString_value);
        for (unsigned i = 0; i < numKids; ++i)
            m_out.storePtr(kids[i], result, m_heaps.JSRopeString_fibers[i]);
        for (unsigned i = numKids; i < JSRopeString::s_maxInternalRopeLength; ++i)
            m_out.storePtr(m_out.intPtrZero, result, m_heaps.JSRopeString_fibers[i]);
        LValue flags = m_out.load16ZeroExt32(kids[0], m_heaps.JSString_flags);
        LValue length = m_out.load32(kids[0], m_heaps.JSString_length);
        for (unsigned i = 1; i < numKids; ++i) {
            flags = m_out.bitAnd(flags, m_out.load16ZeroExt32(kids[i], m_heaps.JSString_flags));
            CheckValue* lengthCheck = m_out.speculateAdd(
                length, m_out.load32(kids[i], m_heaps.JSString_length));
            blessSpeculation(lengthCheck, Uncountable, noValue(), nullptr, m_origin);
            length = lengthCheck;
        }
        m_out.store32As16(
            m_out.bitAnd(m_out.constInt32(JSString::Is8Bit), flags),
            result, m_heaps.JSString_flags);
        m_out.store32(length, result, m_heaps.JSString_length);
        
        mutatorFence();
        ValueFromBlock fastResult = m_out.anchor(result);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        LValue slowResultValue;
        VM& vm = this->vm();
        switch (numKids) {
        case 2:
            slowResultValue = lazySlowPath(
                [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(vm,
                        operationMakeRope2, locations[0].directGPR(), locations[1].directGPR(),
                        locations[2].directGPR());
                }, kids[0], kids[1]);
            break;
        case 3:
            slowResultValue = lazySlowPath(
                [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(vm,
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
        setJSValue(m_out.phi(Int64, fastResult, slowResult));
    }
    
    void compileStringCharAt()
    {
        LValue base = lowString(m_graph.child(m_node, 0));
        LValue index = lowInt32(m_graph.child(m_node, 1));
        LValue storage = lowStorage(m_graph.child(m_node, 2));
            
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
            
        // FIXME: Need to cage strings!
        // https://bugs.webkit.org/show_bug.cgi?id=174924
        ValueFromBlock char8Bit = m_out.anchor(
            m_out.load8ZeroExt32(m_out.baseIndex(
                m_heaps.characters8, storage, m_out.zeroExtPtr(index),
                provenValue(m_graph.child(m_node, 1)))));
        m_out.jump(bitsContinuation);
            
        m_out.appendTo(is16Bit, bigCharacter);

        LValue char16BitValue = m_out.load16ZeroExt32(
            m_out.baseIndex(
                m_heaps.characters16, storage, m_out.zeroExtPtr(index),
                provenValue(m_graph.child(m_node, 1))));
        ValueFromBlock char16Bit = m_out.anchor(char16BitValue);
        m_out.branch(
            m_out.aboveOrEqual(char16BitValue, m_out.constInt32(0x100)),
            rarely(bigCharacter), usually(bitsContinuation));
            
        m_out.appendTo(bigCharacter, bitsContinuation);
            
        Vector<ValueFromBlock, 4> results;
        results.append(m_out.anchor(vmCall(
            Int64, m_out.operation(operationSingleCharacterString),
            m_callFrame, char16BitValue)));
        m_out.jump(continuation);
            
        m_out.appendTo(bitsContinuation, slowPath);
            
        LValue character = m_out.phi(Int32, char8Bit, char16Bit);
            
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
            
            bool prototypeChainIsSane = false;
            if (globalObject->stringPrototypeChainIsSane()) {
                // FIXME: This could be captured using a Speculation mode that means
                // "out-of-bounds loads return a trivial value", something like
                // SaneChainOutOfBounds.
                // https://bugs.webkit.org/show_bug.cgi?id=144668
                
                m_graph.registerAndWatchStructureTransition(globalObject->stringPrototype()->structure(vm()));
                m_graph.registerAndWatchStructureTransition(globalObject->objectPrototype()->structure(vm()));

                prototypeChainIsSane = globalObject->stringPrototypeChainIsSane();
            }
            if (prototypeChainIsSane) {
                LBasicBlock negativeIndex = m_out.newBlock();
                    
                results.append(m_out.anchor(m_out.constInt64(JSValue::encode(jsUndefined()))));
                m_out.branch(
                    m_out.lessThan(index, m_out.int32Zero),
                    rarely(negativeIndex), usually(continuation));
                    
                m_out.appendTo(negativeIndex, continuation);
            }
                
            results.append(m_out.anchor(vmCall(
                Int64, m_out.operation(operationGetByValStringInt), m_callFrame, base, index)));
        }
            
        m_out.jump(continuation);
            
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(Int64, results));
    }
    
    void compileStringCharCodeAt()
    {
        LBasicBlock is8Bit = m_out.newBlock();
        LBasicBlock is16Bit = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue base = lowString(m_node->child1());
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
            
        // FIXME: need to cage strings!
        // https://bugs.webkit.org/show_bug.cgi?id=174924
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
        
        setInt32(m_out.phi(Int32, char8Bit, char16Bit));
    }

    void compileStringFromCharCode()
    {
        Edge childEdge = m_node->child1();
        
        if (childEdge.useKind() == UntypedUse) {
            LValue result = vmCall(
                Int64, m_out.operation(operationStringFromCharCodeUntyped), m_callFrame,
                lowJSValue(childEdge));
            setJSValue(result);
            return;
        }

        DFG_ASSERT(m_graph, m_node, childEdge.useKind() == Int32Use, childEdge.useKind());

        LValue value = lowInt32(childEdge);
        
        LBasicBlock smallIntCase = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(
            m_out.aboveOrEqual(value, m_out.constInt32(maxSingleCharacterString)),
            rarely(slowCase), usually(smallIntCase));

        LBasicBlock lastNext = m_out.appendTo(smallIntCase, slowCase);

        LValue smallStrings = m_out.constIntPtr(vm().smallStrings.singleCharacterStrings());
        LValue fastResultValue = m_out.loadPtr(
            m_out.baseIndex(m_heaps.singleCharacterStrings, smallStrings, m_out.zeroExtPtr(value)));
        ValueFromBlock fastResult = m_out.anchor(fastResultValue);
        m_out.jump(continuation);

        m_out.appendTo(slowCase, continuation);

        LValue slowResultValue = vmCall(
            pointerType(), m_out.operation(operationStringFromCharCode), m_callFrame, value);
        ValueFromBlock slowResult = m_out.anchor(slowResultValue);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);

        setJSValue(m_out.phi(Int64, fastResult, slowResult));
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

        Vector<LBasicBlock, 2> blocks(data.cases.size());
        for (unsigned i = data.cases.size(); i--;)
            blocks[i] = m_out.newBlock();
        LBasicBlock exit = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        Vector<SwitchCase, 2> cases;
        RegisteredStructureSet baseSet;
        for (unsigned i = data.cases.size(); i--;) {
            MultiGetByOffsetCase getCase = data.cases[i];
            for (unsigned j = getCase.set().size(); j--;) {
                RegisteredStructure structure = getCase.set()[j];
                baseSet.add(structure);
                cases.append(SwitchCase(weakStructureID(structure), blocks[i], Weight(1)));
            }
        }
        bool structuresChecked = m_interpreter.forNode(m_node->child1()).m_structure.isSubsetOf(baseSet);
        emitSwitchForMultiByOffset(base, structuresChecked, cases, exit);
        
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
        if (!structuresChecked)
            speculate(BadCache, noValue(), nullptr, m_out.booleanTrue);
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(Int64, results));
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
        RegisteredStructureSet baseSet;
        for (unsigned i = data.variants.size(); i--;) {
            PutByIdVariant variant = data.variants[i];
            for (unsigned j = variant.oldStructure().size(); j--;) {
                RegisteredStructure structure = m_graph.registerStructure(variant.oldStructure()[j]);
                baseSet.add(structure);
                cases.append(SwitchCase(weakStructureID(structure), blocks[i], Weight(1)));
            }
        }
        bool structuresChecked = m_interpreter.forNode(m_node->child1()).m_structure.isSubsetOf(baseSet);
        emitSwitchForMultiByOffset(base, structuresChecked, cases, exit);
        
        LBasicBlock lastNext = m_out.m_nextBlock;
        
        for (unsigned i = data.variants.size(); i--;) {
            m_out.appendTo(blocks[i], i + 1 < data.variants.size() ? blocks[i + 1] : exit);
            
            PutByIdVariant variant = data.variants[i];

            LValue storage;
            if (variant.kind() == PutByIdVariant::Replace) {
                if (isInlineOffset(variant.offset()))
                    storage = base;
                else
                    storage = m_out.loadPtr(base, m_heaps.JSObject_butterfly);
            } else {
                DFG_ASSERT(m_graph, m_node, variant.kind() == PutByIdVariant::Transition, variant.kind());
                m_graph.m_plan.transitions().addLazily(
                    codeBlock(), m_node->origin.semantic.codeOriginOwner(),
                    variant.oldStructureForTransition(), variant.newStructure());
                
                storage = storageForTransition(
                    base, variant.offset(),
                    variant.oldStructureForTransition(), variant.newStructure());
            }
            
            storeProperty(value, storage, data.identifierNumber, variant.offset());
            
            if (variant.kind() == PutByIdVariant::Transition) {
                ASSERT(variant.oldStructureForTransition()->indexingType() == variant.newStructure()->indexingType());
                ASSERT(variant.oldStructureForTransition()->typeInfo().inlineTypeFlags() == variant.newStructure()->typeInfo().inlineTypeFlags());
                ASSERT(variant.oldStructureForTransition()->typeInfo().type() == variant.newStructure()->typeInfo().type());
                m_out.store32(
                    weakStructureID(m_graph.registerStructure(variant.newStructure())), base, m_heaps.JSCell_structureID);
            }
            
            m_out.jump(continuation);
        }
        
        m_out.appendTo(exit, continuation);
        if (!structuresChecked)
            speculate(BadCache, noValue(), nullptr, m_out.booleanTrue);
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compileMatchStructure()
    {
        LValue base = lowCell(m_node->child1());
        
        MatchStructureData& data = m_node->matchStructureData();
        
        LBasicBlock trueBlock = m_out.newBlock();
        LBasicBlock falseBlock = m_out.newBlock();
        LBasicBlock exitBlock = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(trueBlock);
        
        Vector<SwitchCase, 2> cases;
        RegisteredStructureSet baseSet;
        for (MatchStructureVariant& variant : data.variants) {
            baseSet.add(variant.structure);
            cases.append(SwitchCase(
                weakStructureID(variant.structure),
                variant.result ? trueBlock : falseBlock, Weight(1)));
        }
        bool structuresChecked = m_interpreter.forNode(m_node->child1()).m_structure.isSubsetOf(baseSet);
        emitSwitchForMultiByOffset(base, structuresChecked, cases, exitBlock);
        
        m_out.appendTo(trueBlock, falseBlock);
        ValueFromBlock trueResult = m_out.anchor(m_out.booleanTrue);
        m_out.jump(continuation);
        
        m_out.appendTo(falseBlock, exitBlock);
        ValueFromBlock falseResult = m_out.anchor(m_out.booleanFalse);
        m_out.jump(continuation);
        
        m_out.appendTo(exitBlock, continuation);
        if (!structuresChecked)
            speculate(BadCache, noValue(), nullptr, m_out.booleanTrue);
        m_out.unreachable();
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, trueResult, falseResult));
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

        VM& vm = this->vm();
        lazySlowPath(
            [=, &vm] (const Vector<Location>&) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
                    operationNotifyWrite, InvalidGPRReg, CCallHelpers::TrustedImmPtr(set));
            });
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void compileGetCallee()
    {
        setJSValue(m_out.loadPtr(addressFor(CallFrameSlot::callee)));
    }

    void compileSetCallee()
    {
        auto callee = lowCell(m_node->child1());
        m_out.storePtr(callee, payloadFor(CallFrameSlot::callee));
    }
    
    void compileGetArgumentCountIncludingThis()
    {
        VirtualRegister argumentCountRegister;
        if (InlineCallFrame* inlineCallFrame = m_node->argumentsInlineCallFrame())
            argumentCountRegister = inlineCallFrame->argumentCountRegister;
        else
            argumentCountRegister = VirtualRegister(CallFrameSlot::argumentCount);
        setInt32(m_out.load32(payloadFor(argumentCountRegister)));
    }

    void compileSetArgumentCountIncludingThis()
    {
        m_out.store32(m_out.constInt32(m_node->argumentCountIncludingThis()), payloadFor(CallFrameSlot::argumentCount));
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

    void compileGetGlobalThis()
    {
        auto* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        setJSValue(m_out.loadPtr(m_out.absolute(globalObject->addressOfGlobalThis())));
    }
    
    void compileGetClosureVar()
    {
        setJSValue(
            m_out.load64(
                lowCell(m_node->child1()),
                m_heaps.JSLexicalEnvironment_variables[m_node->scopeOffset().offset()]));
    }
    
    void compilePutClosureVar()
    {
        m_out.store64(
            lowJSValue(m_node->child2()),
            lowCell(m_node->child1()),
            m_heaps.JSLexicalEnvironment_variables[m_node->scopeOffset().offset()]);
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

    void compileGetArgument()
    {
        LValue argumentCount = m_out.load32(payloadFor(AssemblyHelpers::argumentCount(m_node->origin.semantic)));

        LBasicBlock inBounds = m_out.newBlock();
        LBasicBlock outOfBounds = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(m_out.lessThanOrEqual(argumentCount, m_out.constInt32(m_node->argumentIndex())), unsure(outOfBounds), unsure(inBounds));

        LBasicBlock lastNext = m_out.appendTo(inBounds, outOfBounds);
        VirtualRegister arg = AssemblyHelpers::argumentsStart(m_node->origin.semantic) + m_node->argumentIndex() - 1;
        ValueFromBlock inBoundsResult = m_out.anchor(m_out.load64(addressFor(arg)));
        m_out.jump(continuation);

        m_out.appendTo(outOfBounds, continuation);
        ValueFromBlock outOfBoundsResult = m_out.anchor(m_out.constInt64(ValueUndefined));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(Int64, inBoundsResult, outOfBoundsResult));
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

        if (m_node->child1().useKind() == KnownOtherUse) {
            ASSERT(!m_interpreter.needsTypeCheck(m_node->child1(), SpecOther));
            setBoolean(equalNullOrUndefined(m_node->child2(), AllCellsAreFalse, EqualNullOrUndefined, ManualOperandSpeculation));
            return;
        }

        if (m_node->child2().useKind() == KnownOtherUse) {
            ASSERT(!m_interpreter.needsTypeCheck(m_node->child2(), SpecOther));
            setBoolean(equalNullOrUndefined(m_node->child1(), AllCellsAreFalse, EqualNullOrUndefined, ManualOperandSpeculation));
            return;
        }

        DFG_ASSERT(m_graph, m_node, m_node->isBinaryUseKind(UntypedUse), m_node->child1().useKind(), m_node->child2().useKind());
        nonSpeculativeCompare(
            [&] (LValue left, LValue right) {
                return m_out.equal(left, right);
            },
            operationCompareEq);
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
            setBoolean(m_out.phi(Int32, fastResult, slowResult));
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
            LValue leftSymbol = lowSymbol(m_node->child1());
            LValue rightSymbol = lowSymbol(m_node->child2());
            setBoolean(m_out.equal(leftSymbol, rightSymbol));
            return;
        }
        
        if (m_node->isBinaryUseKind(BigIntUse)) {
            // FIXME: [ESNext][BigInt] Create specialized version of strict equals for BigIntUse
            // https://bugs.webkit.org/show_bug.cgi?id=182895
            LValue left = lowBigInt(m_node->child1());
            LValue right = lowBigInt(m_node->child2());

            LBasicBlock notTriviallyEqualCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            ValueFromBlock fastResult = m_out.anchor(m_out.booleanTrue);
            m_out.branch(m_out.equal(left, right), rarely(continuation), usually(notTriviallyEqualCase));

            LBasicBlock lastNext = m_out.appendTo(notTriviallyEqualCase, continuation);

            ValueFromBlock slowResult = m_out.anchor(m_out.notNull(vmCall(
                pointerType(), m_out.operation(operationCompareStrictEq), m_callFrame, left, right)));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(Int32, fastResult, slowResult));
            return;
        }

        if (m_node->isBinaryUseKind(SymbolUse, UntypedUse)
            || m_node->isBinaryUseKind(UntypedUse, SymbolUse)) {
            Edge symbolEdge = m_node->child1();
            Edge untypedEdge = m_node->child2();
            if (symbolEdge.useKind() != SymbolUse)
                std::swap(symbolEdge, untypedEdge);
            
            LValue leftSymbol = lowSymbol(symbolEdge);
            LValue untypedValue = lowJSValue(untypedEdge);

            setBoolean(m_out.equal(leftSymbol, untypedValue));
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
            setBoolean(m_out.phi(Int32, notCellResult, notStringResult, isStringResult));
            return;
        }

        if (m_node->isBinaryUseKind(StringUse, UntypedUse)) {
            compileStringToUntypedStrictEquality(m_node->child1(), m_node->child2());
            return;
        }
        if (m_node->isBinaryUseKind(UntypedUse, StringUse)) {
            compileStringToUntypedStrictEquality(m_node->child2(), m_node->child1());
            return;
        }

        DFG_ASSERT(m_graph, m_node, m_node->isBinaryUseKind(UntypedUse), m_node->child1().useKind(), m_node->child2().useKind());
        nonSpeculativeCompare(
            [&] (LValue left, LValue right) {
                return m_out.equal(left, right);
            },
            operationCompareStrictEq);
    }

    void compileStringToUntypedStrictEquality(Edge stringEdge, Edge untypedEdge)
    {
        ASSERT(stringEdge.useKind() == StringUse);
        ASSERT(untypedEdge.useKind() == UntypedUse);

        LValue leftString = lowCell(stringEdge);
        LValue rightValue = lowJSValue(untypedEdge);
        SpeculatedType rightValueType = provenType(untypedEdge);

        // Verify left is string.
        speculateString(stringEdge, leftString);

        LBasicBlock testUntypedEdgeIsCell = m_out.newBlock();
        LBasicBlock testUntypedEdgeIsString = m_out.newBlock();
        LBasicBlock testStringEquality = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        // Given left is string. If the value are strictly equal, rightValue has to be the same string.
        ValueFromBlock fastTrue = m_out.anchor(m_out.booleanTrue);
        m_out.branch(m_out.equal(leftString, rightValue), unsure(continuation), unsure(testUntypedEdgeIsCell));

        LBasicBlock lastNext = m_out.appendTo(testUntypedEdgeIsCell, testUntypedEdgeIsString);
        ValueFromBlock fastFalse = m_out.anchor(m_out.booleanFalse);
        m_out.branch(isNotCell(rightValue, rightValueType), unsure(continuation), unsure(testUntypedEdgeIsString));

        // Check if the untyped edge is a string.
        m_out.appendTo(testUntypedEdgeIsString, testStringEquality);
        m_out.branch(isNotString(rightValue, rightValueType), unsure(continuation), unsure(testStringEquality));

        // Full String compare.
        m_out.appendTo(testStringEquality, continuation);
        ValueFromBlock slowResult = m_out.anchor(stringsEqual(leftString, rightValue));
        m_out.jump(continuation);

        // Continuation.
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, fastTrue, fastFalse, slowResult));
    }

    void compileCompareEqPtr()
    {
        setBoolean(
            m_out.equal(
                lowJSValue(m_node->child1()),
                weakPointer(m_node->cellOperand()->cell())));
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
            operationCompareStringImplLess,
            operationCompareStringLess,
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
            operationCompareStringImplLessEq,
            operationCompareStringLessEq,
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
            operationCompareStringImplGreater,
            operationCompareStringGreater,
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
            operationCompareStringImplGreaterEq,
            operationCompareStringGreaterEq,
            operationCompareGreaterEq);
    }

    void compileCompareBelow()
    {
        setBoolean(m_out.below(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }

    void compileCompareBelowEq()
    {
        setBoolean(m_out.belowOrEqual(lowInt32(m_node->child1()), lowInt32(m_node->child2())));
    }

    void compileSameValue()
    {
        if (m_node->isBinaryUseKind(DoubleRepUse)) {
            LValue arg1 = lowDouble(m_node->child1());
            LValue arg2 = lowDouble(m_node->child2());

            LBasicBlock numberCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            PatchpointValue* patchpoint = m_out.patchpoint(Int32);
            patchpoint->append(arg1, ValueRep::SomeRegister);
            patchpoint->append(arg2, ValueRep::SomeRegister);
            patchpoint->numGPScratchRegisters = 1;
            patchpoint->setGenerator(
                [] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                    GPRReg scratchGPR = params.gpScratch(0);
                    jit.moveDoubleTo64(params[1].fpr(), scratchGPR);
                    jit.moveDoubleTo64(params[2].fpr(), params[0].gpr());
                    jit.compare64(CCallHelpers::Equal, scratchGPR, params[0].gpr(), params[0].gpr());
                });
            patchpoint->effects = Effects::none();
            ValueFromBlock compareResult = m_out.anchor(patchpoint);
            m_out.branch(patchpoint, unsure(continuation), unsure(numberCase));

            LBasicBlock lastNext = m_out.appendTo(numberCase, continuation);
            LValue isArg1NaN = m_out.doubleNotEqualOrUnordered(arg1, arg1);
            LValue isArg2NaN = m_out.doubleNotEqualOrUnordered(arg2, arg2);
            ValueFromBlock nanResult = m_out.anchor(m_out.bitAnd(isArg1NaN, isArg2NaN));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(Int32, compareResult, nanResult));
            return;
        }

        ASSERT(m_node->isBinaryUseKind(UntypedUse));
        setBoolean(vmCall(Int32, m_out.operation(operationSameValue), m_callFrame, lowJSValue(m_node->child1()), lowJSValue(m_node->child2())));
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

        unsigned frameSize = (CallFrame::headerSizeInRegisters + numArgs) * sizeof(EncodedJSValue);
        unsigned alignedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), frameSize);

        // JS->JS calling convention requires that the caller allows this much space on top of stack to
        // get trashed by the callee, even if not all of that space is used to pass arguments. We tell
        // B3 this explicitly for two reasons:
        //
        // - We will only pass frameSize worth of stuff.
        // - The trashed stack guarantee is logically separate from the act of passing arguments, so we
        //   shouldn't rely on Air to infer the trashed stack property based on the arguments it ends
        //   up seeing.
        m_proc.requestCallArgAreaSizeInBytes(alignedFrameSize);

        // Collect the arguments, since this can generate code and we want to generate it before we emit
        // the call.
        Vector<ConstrainedValue> arguments;

        // Make sure that the callee goes into GPR0 because that's where the slow path thunks expect the
        // callee to be.
        arguments.append(ConstrainedValue(jsCallee, ValueRep::reg(GPRInfo::regT0)));

        auto addArgument = [&] (LValue value, VirtualRegister reg, int offset) {
            intptr_t offsetFromSP =
                (reg.offset() - CallerFrameAndPC::sizeInRegisters) * sizeof(EncodedJSValue) + offset;
            arguments.append(ConstrainedValue(value, ValueRep::stackArgument(offsetFromSP)));
        };

        addArgument(jsCallee, VirtualRegister(CallFrameSlot::callee), 0);
        addArgument(m_out.constInt32(numArgs), VirtualRegister(CallFrameSlot::argumentCount), PayloadOffset);
        for (unsigned i = 0; i < numArgs; ++i)
            addArgument(lowJSValue(m_graph.varArgChild(node, 1 + i)), virtualRegisterForArgument(i), 0);

        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendVector(arguments);

        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());
        patchpoint->resultConstraint = ValueRep::reg(GPRInfo::returnValueGPR);

        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        VM* vm = &this->vm();
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CallSiteIndex callSiteIndex = state->jitCode->common.addUniqueCallSiteIndex(codeOrigin);

                exceptionHandle->scheduleExitCreationForUnwind(params, callSiteIndex);

                jit.store32(
                    CCallHelpers::TrustedImm32(callSiteIndex.bits()),
                    CCallHelpers::tagFor(VirtualRegister(CallFrameSlot::argumentCount)));

                CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();

                CCallHelpers::DataLabelPtr targetToCheck;
                CCallHelpers::Jump slowPath = jit.branchPtrWithPatch(
                    CCallHelpers::NotEqual, GPRInfo::regT0, targetToCheck,
                    CCallHelpers::TrustedImmPtr(nullptr));

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
                        MacroAssemblerCodePtr<JITThunkPtrTag> linkCall = vm->getCTIStub(linkCallThunkGenerator).code();
                        linkBuffer.link(slowCall, FunctionPtr<JITThunkPtrTag>(linkCall));

                        callLinkInfo->setCallLocations(
                            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOfNearCall<JSInternalPtrTag>(slowCall)),
                            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOf<JSInternalPtrTag>(targetToCheck)),
                            linkBuffer.locationOfNearCall<JSInternalPtrTag>(fastCall));
                    });
            });

        setJSValue(patchpoint);
    }
    
    void compileDirectCallOrConstruct()
    {
        Node* node = m_node;
        bool isTail = node->op() == DirectTailCall;
        bool isConstruct = node->op() == DirectConstruct;
        
        ExecutableBase* executable = node->castOperand<ExecutableBase*>();
        FunctionExecutable* functionExecutable = jsDynamicCast<FunctionExecutable*>(vm(), executable);
        
        unsigned numPassedArgs = node->numChildren() - 1;
        unsigned numAllocatedArgs = numPassedArgs;
        
        if (functionExecutable) {
            numAllocatedArgs = std::max(
                numAllocatedArgs,
                std::min(
                    static_cast<unsigned>(functionExecutable->parameterCount()) + 1,
                    Options::maximumDirectCallStackSize()));
        }
        
        LValue jsCallee = lowJSValue(m_graph.varArgChild(node, 0));
        
        if (!isTail) {
            unsigned frameSize = (CallFrame::headerSizeInRegisters + numAllocatedArgs) * sizeof(EncodedJSValue);
            unsigned alignedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), frameSize);
            
            m_proc.requestCallArgAreaSizeInBytes(alignedFrameSize);
        }
        
        Vector<ConstrainedValue> arguments;
        
        arguments.append(ConstrainedValue(jsCallee, ValueRep::SomeRegister));
        if (!isTail) {
            auto addArgument = [&] (LValue value, VirtualRegister reg, int offset) {
                intptr_t offsetFromSP =
                    (reg.offset() - CallerFrameAndPC::sizeInRegisters) * sizeof(EncodedJSValue) + offset;
                arguments.append(ConstrainedValue(value, ValueRep::stackArgument(offsetFromSP)));
            };
            
            addArgument(jsCallee, VirtualRegister(CallFrameSlot::callee), 0);
            addArgument(m_out.constInt32(numPassedArgs), VirtualRegister(CallFrameSlot::argumentCount), PayloadOffset);
            for (unsigned i = 0; i < numPassedArgs; ++i)
                addArgument(lowJSValue(m_graph.varArgChild(node, 1 + i)), virtualRegisterForArgument(i), 0);
            for (unsigned i = numPassedArgs; i < numAllocatedArgs; ++i)
                addArgument(m_out.constInt64(JSValue::encode(jsUndefined())), virtualRegisterForArgument(i), 0);
        } else {
            for (unsigned i = 0; i < numPassedArgs; ++i)
                arguments.append(ConstrainedValue(lowJSValue(m_graph.varArgChild(node, 1 + i)), ValueRep::WarmAny));
        }
        
        PatchpointValue* patchpoint = m_out.patchpoint(isTail ? Void : Int64);
        patchpoint->appendVector(arguments);
        
        RefPtr<PatchpointExceptionHandle> exceptionHandle = preparePatchpointForExceptions(patchpoint);
        
        if (isTail) {
            // The shuffler needs tags.
            patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
            patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));
        }
        
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        if (!isTail) {
            patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());
            patchpoint->resultConstraint = ValueRep::reg(GPRInfo::returnValueGPR);
        }
        
        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CallSiteIndex callSiteIndex = state->jitCode->common.addUniqueCallSiteIndex(codeOrigin);
                
                GPRReg calleeGPR = params[!isTail].gpr();

                exceptionHandle->scheduleExitCreationForUnwind(params, callSiteIndex);
                
                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);
                
                if (isTail) {
                    CallFrameShuffleData shuffleData;
                    shuffleData.numLocals = state->jitCode->common.frameRegisterCount;
                    
                    RegisterSet toSave = params.unavailableRegisters();
                    shuffleData.callee = ValueRecovery::inGPR(calleeGPR, DataFormatCell);
                    toSave.set(calleeGPR);
                    for (unsigned i = 0; i < numPassedArgs; ++i) {
                        ValueRecovery recovery = params[1 + i].recoveryForJSValue();
                        shuffleData.args.append(recovery);
                        recovery.forEachReg(
                            [&] (Reg reg) {
                                toSave.set(reg);
                            });
                    }
                    for (unsigned i = numPassedArgs; i < numAllocatedArgs; ++i)
                        shuffleData.args.append(ValueRecovery::constant(jsUndefined()));
                    shuffleData.numPassedArgs = numPassedArgs;
                    shuffleData.setupCalleeSaveRegisters(jit.codeBlock());
                    
                    CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();
                    
                    CCallHelpers::PatchableJump patchableJump = jit.patchableJump();
                    CCallHelpers::Label mainPath = jit.label();
                    
                    jit.store32(
                        CCallHelpers::TrustedImm32(callSiteIndex.bits()),
                        CCallHelpers::tagFor(VirtualRegister(CallFrameSlot::argumentCount)));
                
                    callLinkInfo->setFrameShuffleData(shuffleData);
                    CallFrameShuffler(jit, shuffleData).prepareForTailCall();
                    
                    CCallHelpers::Call call = jit.nearTailCall();
                    
                    jit.abortWithReason(JITDidReturnFromTailCall);
                    
                    CCallHelpers::Label slowPath = jit.label();
                    patchableJump.m_jump.linkTo(slowPath, &jit);
                    callOperation(
                        *state, toSave, jit,
                        node->origin.semantic, exceptions.get(), operationLinkDirectCall,
                        InvalidGPRReg, CCallHelpers::TrustedImmPtr(callLinkInfo), calleeGPR).call();
                    jit.jump().linkTo(mainPath, &jit);
                    
                    callLinkInfo->setUpCall(
                        CallLinkInfo::DirectTailCall, node->origin.semantic, InvalidGPRReg);
                    callLinkInfo->setExecutableDuringCompilation(executable);
                    if (numAllocatedArgs > numPassedArgs)
                        callLinkInfo->setMaxNumArguments(numAllocatedArgs);
                    
                    jit.addLinkTask(
                        [=] (LinkBuffer& linkBuffer) {
                            CodeLocationLabel<JSInternalPtrTag> patchableJumpLocation = linkBuffer.locationOf<JSInternalPtrTag>(patchableJump);
                            CodeLocationNearCall<JSInternalPtrTag> callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
                            CodeLocationLabel<JSInternalPtrTag> slowPathLocation = linkBuffer.locationOf<JSInternalPtrTag>(slowPath);

                            callLinkInfo->setCallLocations(
                                patchableJumpLocation,
                                slowPathLocation,
                                callLocation);
                        });
                    return;
                }
                
                CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();
                
                CCallHelpers::Label mainPath = jit.label();

                jit.store32(
                    CCallHelpers::TrustedImm32(callSiteIndex.bits()),
                    CCallHelpers::tagFor(VirtualRegister(CallFrameSlot::argumentCount)));
                
                CCallHelpers::Call call = jit.nearCall();
                jit.addPtr(
                    CCallHelpers::TrustedImm32(-params.proc().frameSize()),
                    GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
                
                callLinkInfo->setUpCall(
                    isConstruct ? CallLinkInfo::DirectConstruct : CallLinkInfo::DirectCall,
                    node->origin.semantic, InvalidGPRReg);
                callLinkInfo->setExecutableDuringCompilation(executable);
                if (numAllocatedArgs > numPassedArgs)
                    callLinkInfo->setMaxNumArguments(numAllocatedArgs);
                
                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);
                        
                        CCallHelpers::Label slowPath = jit.label();
                        if (isX86())
                            jit.pop(CCallHelpers::selectScratchGPR(calleeGPR));
                        
                        callOperation(
                            *state, params.unavailableRegisters(), jit,
                            node->origin.semantic, exceptions.get(), operationLinkDirectCall,
                            InvalidGPRReg, CCallHelpers::TrustedImmPtr(callLinkInfo),
                            calleeGPR).call();
                        jit.jump().linkTo(mainPath, &jit);
                        
                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                CodeLocationNearCall<JSInternalPtrTag> callLocation = linkBuffer.locationOfNearCall<JSInternalPtrTag>(call);
                                CodeLocationLabel<JSInternalPtrTag> slowPathLocation = linkBuffer.locationOf<JSInternalPtrTag>(slowPath);

                                linkBuffer.link(call, slowPathLocation);

                                callLinkInfo->setCallLocations(
                                    CodeLocationLabel<JSInternalPtrTag>(),
                                    slowPathLocation,
                                    callLocation);
                            });
                    });
            });
        
        if (isTail)
            patchpoint->effects.terminal = true;
        else
            setJSValue(patchpoint);
    }

    void compileTailCall()
    {
        Node* node = m_node;
        unsigned numArgs = node->numChildren() - 1;

        // It seems counterintuitive that this is needed given that tail calls don't create a new frame
        // on the stack. However, the tail call slow path builds the frame at SP instead of FP before
        // calling into the slow path C code. This slow path may decide to throw an exception because
        // the callee we're trying to call is not callable. Throwing an exception will cause us to walk
        // the stack, which may read, for the sake of the correctness of this code, arbitrary slots on the
        // stack to recover state. This call arg area ensures the call frame shuffler does not overwrite
        // any of the slots the stack walking code requires when on the slow path.
        m_proc.requestCallArgAreaSizeInBytes(
            WTF::roundUpToMultipleOf(stackAlignmentBytes(), (CallFrame::headerSizeInRegisters + numArgs) * sizeof(EncodedJSValue)));

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

        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));

        // Prevent any of the arguments from using the scratch register.
        patchpoint->clobberEarly(RegisterSet::macroScratchRegisters());
        
        patchpoint->effects.terminal = true;

        // We don't have to tell the patchpoint that we will clobber registers, since we won't return
        // anyway.

        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        VM* vm = &this->vm();
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CallSiteIndex callSiteIndex = state->jitCode->common.addUniqueCallSiteIndex(codeOrigin);

                // Yes, this is really necessary. You could throw an exception in a host call on the
                // slow path. That'll route us to lookupExceptionHandler(), which unwinds starting
                // with the call site index of our frame. Bad things happen if it's not set.
                jit.store32(
                    CCallHelpers::TrustedImm32(callSiteIndex.bits()),
                    CCallHelpers::tagFor(VirtualRegister(CallFrameSlot::argumentCount)));

                CallFrameShuffleData shuffleData;
                shuffleData.numLocals = state->jitCode->common.frameRegisterCount;
                shuffleData.callee = ValueRecovery::inGPR(GPRInfo::regT0, DataFormatJS);

                for (unsigned i = 0; i < numArgs; ++i)
                    shuffleData.args.append(params[1 + i].recoveryForJSValue());

                shuffleData.numPassedArgs = numArgs;
                
                shuffleData.setupCalleeSaveRegisters(jit.codeBlock());

                CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();

                CCallHelpers::DataLabelPtr targetToCheck;
                CCallHelpers::Jump slowPath = jit.branchPtrWithPatch(
                    CCallHelpers::NotEqual, GPRInfo::regT0, targetToCheck,
                    CCallHelpers::TrustedImmPtr(nullptr));

                callLinkInfo->setFrameShuffleData(shuffleData);
                CallFrameShuffler(jit, shuffleData).prepareForTailCall();

                CCallHelpers::Call fastCall = jit.nearTailCall();

                slowPath.link(&jit);

                CallFrameShuffler slowPathShuffler(jit, shuffleData);
                slowPathShuffler.setCalleeJSValueRegs(JSValueRegs(GPRInfo::regT0));
                slowPathShuffler.prepareForSlowPath();

                jit.move(CCallHelpers::TrustedImmPtr(callLinkInfo), GPRInfo::regT2);
                CCallHelpers::Call slowCall = jit.nearCall();

                jit.abortWithReason(JITDidReturnFromTailCall);

                callLinkInfo->setUpCall(CallLinkInfo::TailCall, codeOrigin, GPRInfo::regT0);

                jit.addLinkTask(
                    [=] (LinkBuffer& linkBuffer) {
                        MacroAssemblerCodePtr<JITThunkPtrTag> linkCall = vm->getCTIStub(linkCallThunkGenerator).code();
                        linkBuffer.link(slowCall, FunctionPtr<JITThunkPtrTag>(linkCall));

                        callLinkInfo->setCallLocations(
                            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOfNearCall<JSInternalPtrTag>(slowCall)),
                            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOf<JSInternalPtrTag>(targetToCheck)),
                            linkBuffer.locationOfNearCall<JSInternalPtrTag>(fastCall));
                    });
            });
    }
    
    void compileCallOrConstructVarargsSpread()
    {
        Node* node = m_node;
        Node* arguments = node->child3().node();

        LValue jsCallee = lowJSValue(m_node->child1());
        LValue thisArg = lowJSValue(m_node->child2());

        RELEASE_ASSERT(arguments->op() == PhantomNewArrayWithSpread || arguments->op() == PhantomSpread || arguments->op() == PhantomNewArrayBuffer);

        unsigned staticArgumentCount = 0;
        Vector<LValue, 2> spreadLengths;
        Vector<LValue, 8> patchpointArguments;
        HashMap<InlineCallFrame*, LValue, WTF::DefaultHash<InlineCallFrame*>::Hash, WTF::NullableHashTraits<InlineCallFrame*>> cachedSpreadLengths;
        auto pushAndCountArgumentsFromRightToLeft = recursableLambda([&](auto self, Node* target) -> void {
            if (target->op() == PhantomSpread) {
                self(target->child1().node());
                return;
            }

            if (target->op() == PhantomNewArrayWithSpread) {
                BitVector* bitVector = target->bitVector();
                for (unsigned i = target->numChildren(); i--; ) {
                    if (bitVector->get(i))
                        self(m_graph.varArgChild(target, i).node());
                    else {
                        ++staticArgumentCount;
                        LValue argument = this->lowJSValue(m_graph.varArgChild(target, i));
                        patchpointArguments.append(argument);
                    }
                }
                return;
            }

            if (target->op() == PhantomNewArrayBuffer) {
                staticArgumentCount += target->castOperand<JSImmutableButterfly*>()->length();
                return;
            }

            RELEASE_ASSERT(target->op() == PhantomCreateRest);
            InlineCallFrame* inlineCallFrame = target->origin.semantic.inlineCallFrame;
            unsigned numberOfArgumentsToSkip = target->numberOfArgumentsToSkip();
            LValue length = cachedSpreadLengths.ensure(inlineCallFrame, [&] () {
                return m_out.zeroExtPtr(this->getSpreadLengthFromInlineCallFrame(inlineCallFrame, numberOfArgumentsToSkip));
            }).iterator->value;
            patchpointArguments.append(length);
            spreadLengths.append(length);
        });

        pushAndCountArgumentsFromRightToLeft(arguments);
        LValue argumentCountIncludingThis = m_out.constIntPtr(staticArgumentCount + 1);
        for (LValue length : spreadLengths)
            argumentCountIncludingThis = m_out.add(length, argumentCountIncludingThis);
        
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);

        patchpoint->append(jsCallee, ValueRep::reg(GPRInfo::regT0));
        patchpoint->append(thisArg, ValueRep::WarmAny);
        patchpoint->append(argumentCountIncludingThis, ValueRep::WarmAny);
        patchpoint->appendVectorWithRep(patchpointArguments, ValueRep::WarmAny);
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));

        RefPtr<PatchpointExceptionHandle> exceptionHandle = preparePatchpointForExceptions(patchpoint);

        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->clobber(RegisterSet::volatileRegistersForJSCall()); // No inputs will be in a volatile register.
        patchpoint->resultConstraint = ValueRep::reg(GPRInfo::returnValueGPR);

        patchpoint->numGPScratchRegisters = 0;

        // This is the minimum amount of call arg area stack space that all JS->JS calls always have.
        unsigned minimumJSCallAreaSize =
            sizeof(CallerFrameAndPC) +
            WTF::roundUpToMultipleOf(stackAlignmentBytes(), 5 * sizeof(EncodedJSValue));

        m_proc.requestCallArgAreaSizeInBytes(minimumJSCallAreaSize);
        
        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        VM* vm = &this->vm();
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
                    CCallHelpers::tagFor(VirtualRegister(CallFrameSlot::argumentCount)));

                CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();

                RegisterSet usedRegisters = RegisterSet::allRegisters();
                usedRegisters.exclude(RegisterSet::volatileRegistersForJSCall());
                GPRReg calleeGPR = params[1].gpr();
                usedRegisters.set(calleeGPR);

                ScratchRegisterAllocator allocator(usedRegisters);
                GPRReg scratchGPR1 = allocator.allocateScratchGPR();
                GPRReg scratchGPR2 = allocator.allocateScratchGPR();
                GPRReg scratchGPR3 = allocator.allocateScratchGPR();
                GPRReg scratchGPR4 = allocator.allocateScratchGPR();
                RELEASE_ASSERT(!allocator.numberOfReusedRegisters());

                auto getValueFromRep = [&] (B3::ValueRep rep, GPRReg result) {
                    ASSERT(!usedRegisters.get(result));

                    if (rep.isConstant()) {
                        jit.move(CCallHelpers::Imm64(rep.value()), result);
                        return;
                    }

                    // Note: in this function, we only request 64 bit values.
                    if (rep.isStack()) {
                        jit.load64(
                            CCallHelpers::Address(GPRInfo::callFrameRegister, rep.offsetFromFP()),
                            result);
                        return;
                    }

                    RELEASE_ASSERT(rep.isGPR());
                    ASSERT(usedRegisters.get(rep.gpr()));
                    jit.move(rep.gpr(), result);
                };

                auto callWithExceptionCheck = [&] (void* callee) {
                    jit.move(CCallHelpers::TrustedImmPtr(tagCFunctionPtr<OperationPtrTag>(callee)), GPRInfo::nonPreservedNonArgumentGPR0);
                    jit.call(GPRInfo::nonPreservedNonArgumentGPR0, OperationPtrTag);
                    exceptions->append(jit.emitExceptionCheck(*vm, AssemblyHelpers::NormalExceptionCheck, AssemblyHelpers::FarJumpWidth));
                };

                CCallHelpers::JumpList slowCase;
                unsigned originalStackHeight = params.proc().frameSize();

                {
                    unsigned numUsedSlots = WTF::roundUpToMultipleOf(stackAlignmentRegisters(), originalStackHeight / sizeof(EncodedJSValue));
                    B3::ValueRep argumentCountIncludingThisRep = params[3];
                    getValueFromRep(argumentCountIncludingThisRep, scratchGPR2);
                    slowCase.append(jit.branch32(CCallHelpers::Above, scratchGPR2, CCallHelpers::TrustedImm32(JSC::maxArguments + 1)));
                    
                    jit.move(scratchGPR2, scratchGPR1);
                    jit.addPtr(CCallHelpers::TrustedImmPtr(static_cast<size_t>(numUsedSlots + CallFrame::headerSizeInRegisters)), scratchGPR1);
                    // scratchGPR1 now has the required frame size in Register units
                    // Round scratchGPR1 to next multiple of stackAlignmentRegisters()
                    jit.addPtr(CCallHelpers::TrustedImm32(stackAlignmentRegisters() - 1), scratchGPR1);
                    jit.andPtr(CCallHelpers::TrustedImm32(~(stackAlignmentRegisters() - 1)), scratchGPR1);
                    jit.negPtr(scratchGPR1);
                    jit.getEffectiveAddress(CCallHelpers::BaseIndex(GPRInfo::callFrameRegister, scratchGPR1, CCallHelpers::TimesEight), scratchGPR1);

                    // Before touching stack values, we should update the stack pointer to protect them from signal stack.
                    jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CallerFrameAndPC)), scratchGPR1, CCallHelpers::stackPointerRegister);

                    jit.store32(scratchGPR2, CCallHelpers::Address(scratchGPR1, CallFrameSlot::argumentCount * static_cast<int>(sizeof(Register)) + PayloadOffset));

                    int storeOffset = CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register));

                    unsigned paramsOffset = 4;
                    unsigned index = 0;
                    auto emitArgumentsFromRightToLeft = recursableLambda([&](auto self, Node* target) -> void {
                        if (target->op() == PhantomSpread) {
                            self(target->child1().node());
                            return;
                        }

                        if (target->op() == PhantomNewArrayWithSpread) {
                            BitVector* bitVector = target->bitVector();
                            for (unsigned i = target->numChildren(); i--; ) {
                                if (bitVector->get(i))
                                    self(state->graph.varArgChild(target, i).node());
                                else {
                                    jit.subPtr(CCallHelpers::TrustedImmPtr(static_cast<size_t>(1)), scratchGPR2);
                                    getValueFromRep(params[paramsOffset + (index++)], scratchGPR3);
                                    jit.store64(scratchGPR3,
                                        CCallHelpers::BaseIndex(scratchGPR1, scratchGPR2, CCallHelpers::TimesEight, storeOffset));
                                }
                            }
                            return;
                        }

                        if (target->op() == PhantomNewArrayBuffer) {
                            auto* array = target->castOperand<JSImmutableButterfly*>();
                            Checked<int32_t> offsetCount { 1 };
                            for (unsigned i = array->length(); i--; ++offsetCount) {
                                // Because varargs values are drained as JSValue, we should not generate value
                                // in Double form even if PhantomNewArrayBuffer's indexingType is ArrayWithDouble.
                                int64_t value = JSValue::encode(array->get(i));
                                jit.move(CCallHelpers::TrustedImm64(value), scratchGPR3);
                                Checked<int32_t> currentStoreOffset { storeOffset };
                                currentStoreOffset -= (offsetCount * static_cast<int32_t>(sizeof(Register)));
                                jit.store64(scratchGPR3,
                                    CCallHelpers::BaseIndex(scratchGPR1, scratchGPR2, CCallHelpers::TimesEight, currentStoreOffset.unsafeGet()));
                            }
                            jit.subPtr(CCallHelpers::TrustedImmPtr(static_cast<size_t>(array->length())), scratchGPR2);
                            return;
                        }

                        RELEASE_ASSERT(target->op() == PhantomCreateRest);
                        InlineCallFrame* inlineCallFrame = target->origin.semantic.inlineCallFrame;

                        unsigned numberOfArgumentsToSkip = target->numberOfArgumentsToSkip();

                        B3::ValueRep numArgumentsToCopy = params[paramsOffset + (index++)];
                        getValueFromRep(numArgumentsToCopy, scratchGPR3);
                        int loadOffset = (AssemblyHelpers::argumentsStart(inlineCallFrame).offset() + numberOfArgumentsToSkip) * static_cast<int>(sizeof(Register));

                        auto done = jit.branchTestPtr(MacroAssembler::Zero, scratchGPR3);
                        auto loopStart = jit.label();
                        jit.subPtr(CCallHelpers::TrustedImmPtr(static_cast<size_t>(1)), scratchGPR3);
                        jit.subPtr(CCallHelpers::TrustedImmPtr(static_cast<size_t>(1)), scratchGPR2);
                        jit.load64(CCallHelpers::BaseIndex(GPRInfo::callFrameRegister, scratchGPR3, CCallHelpers::TimesEight, loadOffset), scratchGPR4);
                        jit.store64(scratchGPR4,
                            CCallHelpers::BaseIndex(scratchGPR1, scratchGPR2, CCallHelpers::TimesEight, storeOffset));
                        jit.branchTestPtr(CCallHelpers::NonZero, scratchGPR3).linkTo(loopStart, &jit);
                        done.link(&jit);
                    });
                    emitArgumentsFromRightToLeft(arguments);
                }

                {
                    CCallHelpers::Jump dontThrow = jit.jump();
                    slowCase.link(&jit);
                    jit.setupArguments<decltype(operationThrowStackOverflowForVarargs)>();
                    callWithExceptionCheck(bitwise_cast<void*>(operationThrowStackOverflowForVarargs));
                    jit.abortWithReason(DFGVarargsThrowingPathDidNotThrow);
                    
                    dontThrow.link(&jit);
                }
                
                ASSERT(calleeGPR == GPRInfo::regT0);
                jit.store64(calleeGPR, CCallHelpers::calleeFrameSlot(CallFrameSlot::callee));
                getValueFromRep(params[2], scratchGPR3);
                jit.store64(scratchGPR3, CCallHelpers::calleeArgumentSlot(0));
                
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
                    CCallHelpers::TrustedImmPtr(nullptr));
                
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
                ASSERT(!usedRegisters.get(GPRInfo::regT2));
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
                        MacroAssemblerCodePtr<JITThunkPtrTag> linkCall = vm->getCTIStub(linkCallThunkGenerator).code();
                        linkBuffer.link(slowCall, FunctionPtr<JITThunkPtrTag>(linkCall));
                        
                        callLinkInfo->setCallLocations(
                            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOfNearCall<JSInternalPtrTag>(slowCall)),
                            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOf<JSInternalPtrTag>(targetToCheck)),
                            linkBuffer.locationOfNearCall<JSInternalPtrTag>(fastCall));
                    });
            });

        switch (node->op()) {
        case TailCallForwardVarargs:
            m_out.unreachable();
            break;

        default:
            setJSValue(patchpoint);
            break;
        }
    }

    void compileCallOrConstructVarargs()
    {
        Node* node = m_node;
        LValue jsCallee = lowJSValue(m_node->child1());
        LValue thisArg = lowJSValue(m_node->child2());
        
        LValue jsArguments = nullptr;
        bool forwarding = false;
        
        switch (node->op()) {
        case CallVarargs:
        case TailCallVarargs:
        case TailCallVarargsInlinedCaller:
        case ConstructVarargs:
            jsArguments = lowJSValue(node->child3());
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

        if (forwarding && m_node->child3()) {
            Node* arguments = m_node->child3().node();
            if (arguments->op() == PhantomNewArrayWithSpread || arguments->op() == PhantomNewArrayBuffer || arguments->op() == PhantomSpread) {
                compileCallOrConstructVarargsSpread();
                return;
            }
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
        
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));

        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());
        patchpoint->resultConstraint = ValueRep::reg(GPRInfo::returnValueGPR);

        // This is the minimum amount of call arg area stack space that all JS->JS calls always have.
        unsigned minimumJSCallAreaSize =
            sizeof(CallerFrameAndPC) +
            WTF::roundUpToMultipleOf(stackAlignmentBytes(), 5 * sizeof(EncodedJSValue));

        m_proc.requestCallArgAreaSizeInBytes(minimumJSCallAreaSize);
        
        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        VM* vm = &this->vm();
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
                    CCallHelpers::tagFor(VirtualRegister(CallFrameSlot::argumentCount)));

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
                    jit.move(CCallHelpers::TrustedImmPtr(tagCFunctionPtr<OperationPtrTag>(callee)), GPRInfo::nonPreservedNonArgumentGPR0);
                    jit.call(GPRInfo::nonPreservedNonArgumentGPR0, OperationPtrTag);
                    exceptions->append(jit.emitExceptionCheck(*vm, AssemblyHelpers::NormalExceptionCheck, AssemblyHelpers::FarJumpWidth));
                };

                unsigned originalStackHeight = params.proc().frameSize();

                if (forwarding) {
                    jit.move(CCallHelpers::TrustedImm32(originalStackHeight / sizeof(EncodedJSValue)), scratchGPR2);
                    
                    CCallHelpers::JumpList slowCase;
                    InlineCallFrame* inlineCallFrame;
                    if (node->child3())
                        inlineCallFrame = node->child3()->origin.semantic.inlineCallFrame;
                    else
                        inlineCallFrame = node->origin.semantic.inlineCallFrame;

                    // emitSetupVarargsFrameFastCase modifies the stack pointer if it succeeds.
                    emitSetupVarargsFrameFastCase(*vm, jit, scratchGPR2, scratchGPR1, scratchGPR2, scratchGPR3, inlineCallFrame, data->firstVarArgOffset, slowCase);

                    CCallHelpers::Jump done = jit.jump();
                    slowCase.link(&jit);
                    jit.setupArguments<decltype(operationThrowStackOverflowForVarargs)>();
                    callWithExceptionCheck(bitwise_cast<void*>(operationThrowStackOverflowForVarargs));
                    jit.abortWithReason(DFGVarargsThrowingPathDidNotThrow);
                    
                    done.link(&jit);
                } else {
                    jit.move(CCallHelpers::TrustedImm32(originalStackHeight / sizeof(EncodedJSValue)), scratchGPR1);
                    jit.setupArguments<decltype(operationSizeFrameForVarargs)>(argumentsGPR, scratchGPR1, CCallHelpers::TrustedImm32(data->firstVarArgOffset));
                    callWithExceptionCheck(bitwise_cast<void*>(operationSizeFrameForVarargs));

                    jit.move(GPRInfo::returnValueGPR, scratchGPR1);
                    jit.move(CCallHelpers::TrustedImm32(originalStackHeight / sizeof(EncodedJSValue)), scratchGPR2);
                    argumentsLateRep.emitRestore(jit, argumentsGPR);
                    emitSetVarargsFrame(jit, scratchGPR1, false, scratchGPR2, scratchGPR2);
                    jit.addPtr(CCallHelpers::TrustedImm32(-minimumJSCallAreaSize), scratchGPR2, CCallHelpers::stackPointerRegister);
                    jit.setupArguments<decltype(operationSetupVarargsFrame)>(scratchGPR2, argumentsGPR, CCallHelpers::TrustedImm32(data->firstVarArgOffset), scratchGPR1);
                    callWithExceptionCheck(bitwise_cast<void*>(operationSetupVarargsFrame));
                    
                    jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CallerFrameAndPC)), GPRInfo::returnValueGPR, CCallHelpers::stackPointerRegister);

                    calleeLateRep.emitRestore(jit, GPRInfo::regT0);

                    // This may not emit code if thisGPR got a callee-save. Also, we're guaranteed
                    // that thisGPR != GPRInfo::regT0 because regT0 interferes with it.
                    thisLateRep.emitRestore(jit, thisGPR);
                }
                
                jit.store64(GPRInfo::regT0, CCallHelpers::calleeFrameSlot(CallFrameSlot::callee));
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
                    CCallHelpers::TrustedImmPtr(nullptr));
                
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
                        MacroAssemblerCodePtr<JITThunkPtrTag> linkCall = vm->getCTIStub(linkCallThunkGenerator).code();
                        linkBuffer.link(slowCall, FunctionPtr<JITThunkPtrTag>(linkCall));
                        
                        callLinkInfo->setCallLocations(
                            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOfNearCall<JSInternalPtrTag>(slowCall)),
                            CodeLocationLabel<JSInternalPtrTag>(linkBuffer.locationOf<JSInternalPtrTag>(targetToCheck)),
                            linkBuffer.locationOfNearCall<JSInternalPtrTag>(fastCall));
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
    
    void compileCallEval()
    {
        Node* node = m_node;
        unsigned numArgs = node->numChildren() - 1;
        
        LValue jsCallee = lowJSValue(m_graph.varArgChild(node, 0));
        
        unsigned frameSize = (CallFrame::headerSizeInRegisters + numArgs) * sizeof(EncodedJSValue);
        unsigned alignedFrameSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), frameSize);
        
        m_proc.requestCallArgAreaSizeInBytes(alignedFrameSize);
        
        Vector<ConstrainedValue> arguments;
        arguments.append(ConstrainedValue(jsCallee, ValueRep::reg(GPRInfo::regT0)));
        
        auto addArgument = [&] (LValue value, VirtualRegister reg, int offset) {
            intptr_t offsetFromSP = 
                (reg.offset() - CallerFrameAndPC::sizeInRegisters) * sizeof(EncodedJSValue) + offset;
            arguments.append(ConstrainedValue(value, ValueRep::stackArgument(offsetFromSP)));
        };
        
        addArgument(jsCallee, VirtualRegister(CallFrameSlot::callee), 0);
        addArgument(m_out.constInt32(numArgs), VirtualRegister(CallFrameSlot::argumentCount), PayloadOffset);
        for (unsigned i = 0; i < numArgs; ++i)
            addArgument(lowJSValue(m_graph.varArgChild(node, 1 + i)), virtualRegisterForArgument(i), 0);
        
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendVector(arguments);
        
        RefPtr<PatchpointExceptionHandle> exceptionHandle = preparePatchpointForExceptions(patchpoint);
        
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->clobberLate(RegisterSet::volatileRegistersForJSCall());
        patchpoint->resultConstraint = ValueRep::reg(GPRInfo::returnValueGPR);
        
        CodeOrigin codeOrigin = codeOriginDescriptionOfCallSite();
        State* state = &m_ftlState;
        VM& vm = this->vm();
        patchpoint->setGenerator(
            [=, &vm] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CallSiteIndex callSiteIndex = state->jitCode->common.addUniqueCallSiteIndex(codeOrigin);
                
                Box<CCallHelpers::JumpList> exceptions = exceptionHandle->scheduleExitCreation(params)->jumps(jit);
                
                exceptionHandle->scheduleExitCreationForUnwind(params, callSiteIndex);
                
                jit.store32(
                    CCallHelpers::TrustedImm32(callSiteIndex.bits()),
                    CCallHelpers::tagFor(VirtualRegister(CallFrameSlot::argumentCount)));
                
                CallLinkInfo* callLinkInfo = jit.codeBlock()->addCallLinkInfo();
                callLinkInfo->setUpCall(CallLinkInfo::Call, node->origin.semantic, GPRInfo::regT0);
                
                jit.addPtr(CCallHelpers::TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC))), CCallHelpers::stackPointerRegister, GPRInfo::regT1);
                jit.storePtr(GPRInfo::callFrameRegister, CCallHelpers::Address(GPRInfo::regT1, CallFrame::callerFrameOffset()));
                
                // Now we need to make room for:
                // - The caller frame and PC for a call to operationCallEval.
                // - Potentially two arguments on the stack.
                unsigned requiredBytes = sizeof(CallerFrameAndPC) + sizeof(ExecState*) * 2;
                requiredBytes = WTF::roundUpToMultipleOf(stackAlignmentBytes(), requiredBytes);
                jit.subPtr(CCallHelpers::TrustedImm32(requiredBytes), CCallHelpers::stackPointerRegister);
                jit.setupArguments<decltype(operationCallEval)>(GPRInfo::regT1);
                jit.move(CCallHelpers::TrustedImmPtr(tagCFunctionPtr<OperationPtrTag>(operationCallEval)), GPRInfo::nonPreservedNonArgumentGPR0);
                jit.call(GPRInfo::nonPreservedNonArgumentGPR0, OperationPtrTag);
                exceptions->append(jit.emitExceptionCheck(state->vm(), AssemblyHelpers::NormalExceptionCheck, AssemblyHelpers::FarJumpWidth));
                
                CCallHelpers::Jump done = jit.branchTest64(CCallHelpers::NonZero, GPRInfo::returnValueGPR);
                
                jit.addPtr(CCallHelpers::TrustedImm32(requiredBytes), CCallHelpers::stackPointerRegister);
                jit.load64(CCallHelpers::calleeFrameSlot(CallFrameSlot::callee), GPRInfo::regT0);
                jit.emitDumbVirtualCall(vm, callLinkInfo);
                
                done.link(&jit);
                jit.addPtr(
                    CCallHelpers::TrustedImm32(-params.proc().frameSize()),
                    GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
            });
        
        setJSValue(patchpoint);
    }
    
    void compileLoadVarargs()
    {
        LoadVarargsData* data = m_node->loadVarargsData();
        LValue jsArguments = lowJSValue(m_node->child1());
        
        LValue length = vmCall(
            Int32, m_out.operation(operationSizeOfVarargs), m_callFrame, jsArguments,
            m_out.constInt32(data->offset));
        
        // FIXME: There is a chance that we will call an effectful length property twice. This is safe
        // from the standpoint of the VM's integrity, but it's subtly wrong from a spec compliance
        // standpoint. The best solution would be one where we can exit *into* the op_call_varargs right
        // past the sizing.
        // https://bugs.webkit.org/show_bug.cgi?id=141448
        
        LValue lengthIncludingThis = m_out.add(length, m_out.int32One);

        speculate(
            VarargsOverflow, noValue(), nullptr,
            m_out.above(length, lengthIncludingThis));

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
            Void, m_out.operation(operationLoadVarargs), m_callFrame,
            m_out.castToInt32(machineStart), jsArguments, m_out.constInt32(data->offset),
            length, m_out.constInt32(data->mandatoryMinimum));
    }
    
    void compileForwardVarargs()
    {
        if (m_node->child1()) {
            Node* arguments = m_node->child1().node();
            if (arguments->op() == PhantomNewArrayWithSpread || arguments->op() == PhantomNewArrayBuffer || arguments->op() == PhantomSpread) {
                compileForwardVarargsWithSpread();
                return;
            }
        }

        LoadVarargsData* data = m_node->loadVarargsData();
        InlineCallFrame* inlineCallFrame;
        if (m_node->child1())
            inlineCallFrame = m_node->child1()->origin.semantic.inlineCallFrame;
        else
            inlineCallFrame = m_node->origin.semantic.inlineCallFrame;

        LValue length = nullptr; 
        LValue lengthIncludingThis = nullptr;
        ArgumentsLength argumentsLength = getArgumentsLength(inlineCallFrame);
        if (argumentsLength.isKnown) {
            unsigned knownLength = argumentsLength.known;
            if (knownLength >= data->offset)
                knownLength = knownLength - data->offset;
            else
                knownLength = 0;
            length = m_out.constInt32(knownLength);
            lengthIncludingThis = m_out.constInt32(knownLength + 1);
        } else {
            // We need to perform the same logical operation as the code above, but through dynamic operations.
            if (!data->offset)
                length = argumentsLength.value;
            else {
                LBasicBlock isLarger = m_out.newBlock();
                LBasicBlock continuation = m_out.newBlock();

                ValueFromBlock smallerOrEqualLengthResult = m_out.anchor(m_out.constInt32(0));
                m_out.branch(
                    m_out.above(argumentsLength.value, m_out.constInt32(data->offset)), unsure(isLarger), unsure(continuation));
                LBasicBlock lastNext = m_out.appendTo(isLarger, continuation);
                ValueFromBlock largerLengthResult = m_out.anchor(m_out.sub(argumentsLength.value, m_out.constInt32(data->offset)));
                m_out.jump(continuation);

                m_out.appendTo(continuation, lastNext);
                length = m_out.phi(Int32, smallerOrEqualLengthResult, largerLengthResult);
            }
            lengthIncludingThis = m_out.add(length, m_out.constInt32(1));
        }

        speculate(
            VarargsOverflow, noValue(), nullptr,
            m_out.above(lengthIncludingThis, m_out.constInt32(data->limit)));
        
        m_out.store32(lengthIncludingThis, payloadFor(data->machineCount));
        
        unsigned numberOfArgumentsToSkip = data->offset;
        LValue sourceStart = getArgumentsStart(inlineCallFrame, numberOfArgumentsToSkip);
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
        LValue previousIndex = m_out.phi(pointerType(), loopBound);
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
        previousIndex = m_out.phi(pointerType(), loopBound);
        currentIndex = m_out.sub(previousIndex, m_out.intPtrOne);
        LValue value = m_out.load64(
            m_out.baseIndex(m_heaps.variables, sourceStart, currentIndex));
        m_out.store64(value, m_out.baseIndex(m_heaps.variables, targetStart, currentIndex));
        nextIndex = m_out.anchor(currentIndex);
        m_out.addIncomingToPhi(previousIndex, nextIndex);
        m_out.branch(m_out.isNull(currentIndex), unsure(continuation), unsure(mainLoop));
        
        m_out.appendTo(continuation, lastNext);
    }

    LValue getSpreadLengthFromInlineCallFrame(InlineCallFrame* inlineCallFrame, unsigned numberOfArgumentsToSkip)
    {
        ArgumentsLength argumentsLength = getArgumentsLength(inlineCallFrame);
        if (argumentsLength.isKnown) {
            unsigned knownLength = argumentsLength.known;
            if (knownLength >= numberOfArgumentsToSkip)
                knownLength = knownLength - numberOfArgumentsToSkip;
            else
                knownLength = 0;
            return m_out.constInt32(knownLength);
        }


        // We need to perform the same logical operation as the code above, but through dynamic operations.
        if (!numberOfArgumentsToSkip)
            return argumentsLength.value;

        LBasicBlock isLarger = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        ValueFromBlock smallerOrEqualLengthResult = m_out.anchor(m_out.constInt32(0));
        m_out.branch(
            m_out.above(argumentsLength.value, m_out.constInt32(numberOfArgumentsToSkip)), unsure(isLarger), unsure(continuation));
        LBasicBlock lastNext = m_out.appendTo(isLarger, continuation);
        ValueFromBlock largerLengthResult = m_out.anchor(m_out.sub(argumentsLength.value, m_out.constInt32(numberOfArgumentsToSkip)));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        return m_out.phi(Int32, smallerOrEqualLengthResult, largerLengthResult);
    }

    void compileForwardVarargsWithSpread()
    {
        HashMap<InlineCallFrame*, LValue, WTF::DefaultHash<InlineCallFrame*>::Hash, WTF::NullableHashTraits<InlineCallFrame*>> cachedSpreadLengths;

        Node* arguments = m_node->child1().node();
        RELEASE_ASSERT(arguments->op() == PhantomNewArrayWithSpread || arguments->op() == PhantomNewArrayBuffer || arguments->op() == PhantomSpread);

        unsigned numberOfStaticArguments = 0;
        Vector<LValue, 2> spreadLengths;

        auto collectArgumentCount = recursableLambda([&](auto self, Node* target) -> void {
            if (target->op() == PhantomSpread) {
                self(target->child1().node());
                return;
            }

            if (target->op() == PhantomNewArrayWithSpread) {
                BitVector* bitVector = target->bitVector();
                for (unsigned i = 0; i < target->numChildren(); i++) {
                    if (bitVector->get(i))
                        self(m_graph.varArgChild(target, i).node());
                    else
                        ++numberOfStaticArguments;
                }
                return;
            }

            if (target->op() == PhantomNewArrayBuffer) {
                numberOfStaticArguments += target->castOperand<JSImmutableButterfly*>()->length();
                return;
            }

            ASSERT(target->op() == PhantomCreateRest);
            InlineCallFrame* inlineCallFrame = target->origin.semantic.inlineCallFrame;
            unsigned numberOfArgumentsToSkip = target->numberOfArgumentsToSkip();
            spreadLengths.append(cachedSpreadLengths.ensure(inlineCallFrame, [&] () {
                return this->getSpreadLengthFromInlineCallFrame(inlineCallFrame, numberOfArgumentsToSkip);
            }).iterator->value);
        });

        collectArgumentCount(arguments);
        LValue lengthIncludingThis = m_out.constInt32(1 + numberOfStaticArguments);
        for (LValue length : spreadLengths)
            lengthIncludingThis = m_out.add(lengthIncludingThis, length);

        LoadVarargsData* data = m_node->loadVarargsData();
        speculate(
            VarargsOverflow, noValue(), nullptr,
            m_out.above(lengthIncludingThis, m_out.constInt32(data->limit)));
        
        m_out.store32(lengthIncludingThis, payloadFor(data->machineCount));

        LValue targetStart = addressFor(data->machineStart).value();

        auto forwardSpread = recursableLambda([this, &cachedSpreadLengths, &targetStart](auto self, Node* target, LValue storeIndex) -> LValue {
            if (target->op() == PhantomSpread)
                return self(target->child1().node(), storeIndex);

            if (target->op() == PhantomNewArrayWithSpread) {
                BitVector* bitVector = target->bitVector();
                for (unsigned i = 0; i < target->numChildren(); i++) {
                    if (bitVector->get(i))
                        storeIndex = self(m_graph.varArgChild(target, i).node(), storeIndex);
                    else {
                        LValue value = this->lowJSValue(m_graph.varArgChild(target, i));
                        m_out.store64(value, m_out.baseIndex(m_heaps.variables, targetStart, storeIndex));
                        storeIndex = m_out.add(m_out.constIntPtr(1), storeIndex);
                    }
                }
                return storeIndex;
            }

            if (target->op() == PhantomNewArrayBuffer) {
                auto* array = target->castOperand<JSImmutableButterfly*>();
                for (unsigned i = 0; i < array->length(); i++) {
                    // Because forwarded values are drained as JSValue, we should not generate value
                    // in Double form even if PhantomNewArrayBuffer's indexingType is ArrayWithDouble.
                    int64_t value = JSValue::encode(array->get(i));
                    m_out.store64(m_out.constInt64(value), m_out.baseIndex(m_heaps.variables, targetStart, storeIndex, JSValue(), (Checked<int32_t>(sizeof(Register)) * i).unsafeGet()));
                }
                return m_out.add(m_out.constIntPtr(array->length()), storeIndex);
            }

            RELEASE_ASSERT(target->op() == PhantomCreateRest);
            InlineCallFrame* inlineCallFrame = target->origin.semantic.inlineCallFrame;

            LValue sourceStart = this->getArgumentsStart(inlineCallFrame, target->numberOfArgumentsToSkip());
            LValue spreadLength = m_out.zeroExtPtr(cachedSpreadLengths.get(inlineCallFrame));

            LBasicBlock loop = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            ValueFromBlock startLoadIndex = m_out.anchor(m_out.constIntPtr(0));
            ValueFromBlock startStoreIndex = m_out.anchor(storeIndex);
            ValueFromBlock startStoreIndexForEnd = m_out.anchor(storeIndex);

            m_out.branch(m_out.isZero64(spreadLength), unsure(continuation), unsure(loop));

            LBasicBlock lastNext = m_out.appendTo(loop, continuation);
            LValue loopStoreIndex = m_out.phi(Int64, startStoreIndex);
            LValue loadIndex = m_out.phi(Int64, startLoadIndex);
            LValue value = m_out.load64(
                m_out.baseIndex(m_heaps.variables, sourceStart, loadIndex));
            m_out.store64(value, m_out.baseIndex(m_heaps.variables, targetStart, loopStoreIndex));
            LValue nextLoadIndex = m_out.add(m_out.constIntPtr(1), loadIndex);
            m_out.addIncomingToPhi(loadIndex, m_out.anchor(nextLoadIndex));
            LValue nextStoreIndex = m_out.add(m_out.constIntPtr(1), loopStoreIndex);
            m_out.addIncomingToPhi(loopStoreIndex, m_out.anchor(nextStoreIndex));
            ValueFromBlock loopStoreIndexForEnd = m_out.anchor(nextStoreIndex);
            m_out.branch(m_out.below(nextLoadIndex, spreadLength), unsure(loop), unsure(continuation));

            m_out.appendTo(continuation, lastNext);
            return m_out.phi(Int64, startStoreIndexForEnd, loopStoreIndexForEnd);
        });

        LValue storeIndex = forwardSpread(arguments, m_out.constIntPtr(0));

        LBasicBlock undefinedLoop = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        ValueFromBlock startStoreIndex = m_out.anchor(storeIndex);
        LValue loopBoundValue = m_out.constIntPtr(data->mandatoryMinimum);
        m_out.branch(m_out.below(storeIndex, loopBoundValue),
            unsure(undefinedLoop), unsure(continuation));

        LBasicBlock lastNext = m_out.appendTo(undefinedLoop, continuation);
        LValue loopStoreIndex = m_out.phi(Int64, startStoreIndex);
        m_out.store64(
            m_out.constInt64(JSValue::encode(jsUndefined())),
            m_out.baseIndex(m_heaps.variables, targetStart, loopStoreIndex));
        LValue nextIndex = m_out.add(loopStoreIndex, m_out.constIntPtr(1));
        m_out.addIncomingToPhi(loopStoreIndex, m_out.anchor(nextIndex));
        m_out.branch(
            m_out.below(nextIndex, loopBoundValue), unsure(undefinedLoop), unsure(continuation));

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
            buildSwitch(data, Int32, m_out.phi(Int32, intValues));
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
                vmCall(pointerType(), m_out.operation(operationResolveRope), m_callFrame, stringValue)));
            m_out.jump(resolved);
            
            m_out.appendTo(resolved, is8Bit);
            LValue value = m_out.phi(pointerType(), values);
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
            buildSwitch(data, Int32, m_out.phi(Int32, characters));
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
            
            buildSwitch(m_node->switchData(), pointerType(), cell);
            return;
        } }
        
        DFG_CRASH(m_graph, m_node, "Bad switch kind");
    }

    void compileEntrySwitch()
    {
        Vector<LBasicBlock> successors;
        for (DFG::BasicBlock* successor : m_node->entrySwitchData()->cases)
            successors.append(lowBlock(successor));
        m_out.entrySwitch(successors);
    }
    
    void compileReturn()
    {
        m_out.ret(lowJSValue(m_node->child1()));
    }
    
    void compileForceOSRExit()
    {
        terminate(InadequateCoverage);
    }

    void compileCPUIntrinsic()
    {
#if CPU(X86_64)
        Intrinsic intrinsic = m_node->intrinsic();
        switch (intrinsic) {
        case CPUMfenceIntrinsic:
        case CPUCpuidIntrinsic:
        case CPUPauseIntrinsic: {
            PatchpointValue* patchpoint = m_out.patchpoint(Void);
            patchpoint->effects = Effects::forCall();
            if (intrinsic == CPUCpuidIntrinsic)
                patchpoint->clobber(RegisterSet { X86Registers::eax, X86Registers::ebx, X86Registers::ecx, X86Registers::edx });

            patchpoint->setGenerator([=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                switch (intrinsic) {
                case CPUMfenceIntrinsic:
                    jit.mfence();
                    break;
                case CPUCpuidIntrinsic:
                    jit.cpuid();
                    break;
                case CPUPauseIntrinsic:
                    jit.pause();
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
            });
            setJSValue(m_out.constInt64(JSValue::encode(jsUndefined())));
            break;
        }
        case CPURdtscIntrinsic: {
            PatchpointValue* patchpoint = m_out.patchpoint(Int32);
            patchpoint->effects = Effects::forCall();
            patchpoint->clobber(RegisterSet { X86Registers::eax, X86Registers::edx });
            // The low 32-bits of rdtsc go into rax.
            patchpoint->resultConstraint = ValueRep::reg(X86Registers::eax);
            patchpoint->setGenerator( [=] (CCallHelpers& jit, const B3::StackmapGenerationParams&) {
                jit.rdtsc();
            });
            setJSValue(boxInt32(patchpoint));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();

        }
#endif
    }
    
    void compileThrow()
    {
        LValue error = lowJSValue(m_node->child1());
        vmCall(Void, m_out.operation(operationThrowDFG), m_callFrame, error); 
        // vmCall() does an exception check so we should never reach this.
        m_out.unreachable();
    }

    void compileThrowStaticError()
    {
        LValue errorMessage = lowString(m_node->child1());
        LValue errorType = m_out.constInt32(m_node->errorType());
        vmCall(Void, m_out.operation(operationThrowStaticError), m_callFrame, errorMessage, errorType);
        // vmCall() does an exception check so we should never reach this.
        m_out.unreachable();
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
                            linkBuffer.locationOf<JSInternalPtrTag>(label),
                            linkBuffer.locationOf<OSRExitPtrTag>(handle->label));
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

    void compileIsEmpty()
    {
        setBoolean(m_out.isZero64(lowJSValue(m_node->child1())));
    }
    
    void compileIsUndefined()
    {
        setBoolean(equalNullOrUndefined(m_node->child1(), AllCellsAreFalse, EqualUndefined));
    }

    void compileIsUndefinedOrNull()
    {
        setBoolean(isOther(lowJSValue(m_node->child1()), provenType(m_node->child1())));
    }
    
    void compileIsBoolean()
    {
        setBoolean(isBoolean(lowJSValue(m_node->child1()), provenType(m_node->child1())));
    }
    
    void compileIsNumber()
    {
        setBoolean(isNumber(lowJSValue(m_node->child1()), provenType(m_node->child1())));
    }

    void compileNumberIsInteger()
    {
        LBasicBlock notInt32 = m_out.newBlock();
        LBasicBlock doubleCase = m_out.newBlock();
        LBasicBlock doubleNotNanOrInf = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue input = lowJSValue(m_node->child1());

        ValueFromBlock trueResult = m_out.anchor(m_out.booleanTrue);
        m_out.branch(
            isInt32(input, provenType(m_node->child1())), unsure(continuation), unsure(notInt32));

        LBasicBlock lastNext = m_out.appendTo(notInt32, doubleCase);
        ValueFromBlock falseResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(
            isNotNumber(input, provenType(m_node->child1())), unsure(continuation), unsure(doubleCase));

        m_out.appendTo(doubleCase, doubleNotNanOrInf);
        LValue doubleAsInt;
        LValue asDouble = unboxDouble(input, &doubleAsInt);
        LValue expBits = m_out.bitAnd(m_out.lShr(doubleAsInt, m_out.constInt32(52)), m_out.constInt64(0x7ff));
        m_out.branch(
            m_out.equal(expBits, m_out.constInt64(0x7ff)),
            unsure(continuation), unsure(doubleNotNanOrInf));

        m_out.appendTo(doubleNotNanOrInf, continuation);
        PatchpointValue* patchpoint = m_out.patchpoint(Int32);
        patchpoint->appendSomeRegister(asDouble);
        patchpoint->numFPScratchRegisters = 1;
        patchpoint->effects = Effects::none();
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            GPRReg result = params[0].gpr();
            FPRReg input = params[1].fpr();
            FPRReg temp = params.fpScratch(0);
            jit.roundTowardZeroDouble(input, temp);
            jit.compareDouble(MacroAssembler::DoubleEqual, input, temp, result);
        });
        ValueFromBlock patchpointResult = m_out.anchor(patchpoint);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, trueResult, falseResult, patchpointResult));
    }
    
    void compileIsCellWithType()
    {
        if (m_node->child1().useKind() == UntypedUse) {
            LValue value = lowJSValue(m_node->child1());

            LBasicBlock isCellCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
            m_out.branch(
                isCell(value, provenType(m_node->child1())), unsure(isCellCase), unsure(continuation));

            LBasicBlock lastNext = m_out.appendTo(isCellCase, continuation);
            ValueFromBlock cellResult = m_out.anchor(isCellWithType(value, m_node->queriedType(), m_node->speculatedTypeForQuery(), provenType(m_node->child1())));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(Int32, notCellResult, cellResult));
        } else {
            ASSERT(m_node->child1().useKind() == CellUse);
            setBoolean(isCellWithType(lowCell(m_node->child1()), m_node->queriedType(), m_node->speculatedTypeForQuery(), provenType(m_node->child1())));
        }
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
        setBoolean(m_out.phi(Int32, notCellResult, cellResult));
    }

    LValue wangsInt64Hash(LValue input)
    {
        // key += ~(key << 32);
        LValue key = input;
        LValue temp = key;
        temp = m_out.shl(temp, m_out.constInt32(32));
        temp = m_out.bitNot(temp);
        key = m_out.add(key, temp);
        // key ^= (key >> 22);
        temp = key;
        temp = m_out.lShr(temp, m_out.constInt32(22));
        key = m_out.bitXor(key, temp);
        // key += ~(key << 13);
        temp = key;
        temp = m_out.shl(temp, m_out.constInt32(13));
        temp = m_out.bitNot(temp);
        key = m_out.add(key, temp);
        // key ^= (key >> 8);
        temp = key;
        temp = m_out.lShr(temp, m_out.constInt32(8));
        key = m_out.bitXor(key, temp);
        // key += (key << 3);
        temp = key;
        temp = m_out.shl(temp, m_out.constInt32(3));
        key = m_out.add(key, temp);
        // key ^= (key >> 15);
        temp = key;
        temp = m_out.lShr(temp, m_out.constInt32(15));
        key = m_out.bitXor(key, temp);
        // key += ~(key << 27);
        temp = key;
        temp = m_out.shl(temp, m_out.constInt32(27));
        temp = m_out.bitNot(temp);
        key = m_out.add(key, temp);
        // key ^= (key >> 31);
        temp = key;
        temp = m_out.lShr(temp, m_out.constInt32(31));
        key = m_out.bitXor(key, temp);
        key = m_out.castToInt32(key);

        return key;
    }

    LValue mapHashString(LValue string)
    {
        LBasicBlock nonEmptyStringCase = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue stringImpl = m_out.loadPtr(string, m_heaps.JSString_value);
        m_out.branch(
            m_out.equal(stringImpl, m_out.constIntPtr(0)), unsure(slowCase), unsure(nonEmptyStringCase));

        LBasicBlock lastNext = m_out.appendTo(nonEmptyStringCase, slowCase);
        LValue hash = m_out.lShr(m_out.load32(stringImpl, m_heaps.StringImpl_hashAndFlags), m_out.constInt32(StringImpl::s_flagCount));
        ValueFromBlock nonEmptyStringHashResult = m_out.anchor(hash);
        m_out.branch(m_out.equal(hash, m_out.constInt32(0)),
            unsure(slowCase), unsure(continuation));

        m_out.appendTo(slowCase, continuation);
        ValueFromBlock slowResult = m_out.anchor(
            vmCall(Int32, m_out.operation(operationMapHash), m_callFrame, string));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        return m_out.phi(Int32, slowResult, nonEmptyStringHashResult);
    }

    void compileMapHash()
    {
        switch (m_node->child1().useKind()) {
        case BooleanUse:
        case Int32Use:
        case SymbolUse:
        case ObjectUse: {
            LValue key = lowJSValue(m_node->child1(), ManualOperandSpeculation);
            speculate(m_node->child1());
            setInt32(wangsInt64Hash(key));
            return;
        }

        case CellUse: {
            LBasicBlock isString = m_out.newBlock();
            LBasicBlock notString = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue value = lowCell(m_node->child1());
            LValue isStringValue = m_out.equal(m_out.load8ZeroExt32(value, m_heaps.JSCell_typeInfoType), m_out.constInt32(StringType));
            m_out.branch(
                isStringValue, unsure(isString), unsure(notString));

            LBasicBlock lastNext = m_out.appendTo(isString, notString);
            ValueFromBlock stringResult = m_out.anchor(mapHashString(value));
            m_out.jump(continuation);

            m_out.appendTo(notString, continuation);
            ValueFromBlock notStringResult = m_out.anchor(wangsInt64Hash(value));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setInt32(m_out.phi(Int32, stringResult, notStringResult));
            return;
        }

        case StringUse: {
            LValue string = lowString(m_node->child1());
            setInt32(mapHashString(string));
            return;
        }

        default:
            RELEASE_ASSERT(m_node->child1().useKind() == UntypedUse);
            break;
        }

        LValue value = lowJSValue(m_node->child1());

        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock straightHash = m_out.newBlock();
        LBasicBlock isStringCase = m_out.newBlock();
        LBasicBlock nonEmptyStringCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(
            isCell(value, provenType(m_node->child1())), unsure(isCellCase), unsure(straightHash));

        LBasicBlock lastNext = m_out.appendTo(isCellCase, isStringCase);
        LValue isString = m_out.equal(m_out.load8ZeroExt32(value, m_heaps.JSCell_typeInfoType), m_out.constInt32(StringType));
        m_out.branch(
            isString, unsure(isStringCase), unsure(straightHash));

        m_out.appendTo(isStringCase, nonEmptyStringCase);
        LValue stringImpl = m_out.loadPtr(value, m_heaps.JSString_value);
        m_out.branch(
            m_out.equal(stringImpl, m_out.constIntPtr(0)), rarely(slowCase), usually(nonEmptyStringCase));

        m_out.appendTo(nonEmptyStringCase, straightHash);
        LValue hash = m_out.lShr(m_out.load32(stringImpl, m_heaps.StringImpl_hashAndFlags), m_out.constInt32(StringImpl::s_flagCount));
        ValueFromBlock nonEmptyStringHashResult = m_out.anchor(hash);
        m_out.branch(m_out.equal(hash, m_out.constInt32(0)),
            unsure(slowCase), unsure(continuation));

        m_out.appendTo(straightHash, slowCase);
        ValueFromBlock fastResult = m_out.anchor(wangsInt64Hash(value));
        m_out.jump(continuation);

        m_out.appendTo(slowCase, continuation);
        ValueFromBlock slowResult = m_out.anchor(
            vmCall(Int32, m_out.operation(operationMapHash), m_callFrame, value));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setInt32(m_out.phi(Int32, fastResult, slowResult, nonEmptyStringHashResult));
    }

    void compileNormalizeMapKey()
    {
        ASSERT(m_node->child1().useKind() == UntypedUse);

        LBasicBlock isNumberCase = m_out.newBlock();
        LBasicBlock notInt32NumberCase = m_out.newBlock();
        LBasicBlock notNaNCase = m_out.newBlock();
        LBasicBlock convertibleCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LBasicBlock lastNext = m_out.insertNewBlocksBefore(isNumberCase);

        LValue key = lowJSValue(m_node->child1());
        ValueFromBlock fastResult = m_out.anchor(key);
        m_out.branch(isNotNumber(key), unsure(continuation), unsure(isNumberCase));

        m_out.appendTo(isNumberCase, notInt32NumberCase);
        m_out.branch(isInt32(key), unsure(continuation), unsure(notInt32NumberCase));

        m_out.appendTo(notInt32NumberCase, notNaNCase);
        LValue doubleValue = unboxDouble(key);
        m_out.branch(m_out.doubleNotEqualOrUnordered(doubleValue, doubleValue), unsure(continuation), unsure(notNaNCase));

        m_out.appendTo(notNaNCase, convertibleCase);
        LValue integerValue = m_out.doubleToInt(doubleValue);
        LValue integerValueConvertedToDouble = m_out.intToDouble(integerValue);
        m_out.branch(m_out.doubleNotEqualOrUnordered(doubleValue, integerValueConvertedToDouble), unsure(continuation), unsure(convertibleCase));

        m_out.appendTo(convertibleCase, continuation);
        ValueFromBlock slowResult = m_out.anchor(boxInt32(integerValue));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(Int64, fastResult, slowResult));
    }

    void compileGetMapBucket()
    {
        LBasicBlock loopStart = m_out.newBlock();
        LBasicBlock loopAround = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock notPresentInTable = m_out.newBlock();
        LBasicBlock notEmptyValue = m_out.newBlock();
        LBasicBlock notDeletedValue = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LBasicBlock lastNext = m_out.insertNewBlocksBefore(loopStart);

        LValue map;
        if (m_node->child1().useKind() == MapObjectUse)
            map = lowMapObject(m_node->child1());
        else if (m_node->child1().useKind() == SetObjectUse)
            map = lowSetObject(m_node->child1());
        else
            RELEASE_ASSERT_NOT_REACHED();

        LValue key = lowJSValue(m_node->child2(), ManualOperandSpeculation);
        if (m_node->child2().useKind() != UntypedUse)
            speculate(m_node->child2());

        LValue hash = lowInt32(m_node->child3());

        LValue buffer = m_out.loadPtr(map, m_heaps.HashMapImpl_buffer);
        LValue mask = m_out.sub(m_out.load32(map, m_heaps.HashMapImpl_capacity), m_out.int32One);

        ValueFromBlock indexStart = m_out.anchor(hash);
        m_out.jump(loopStart);

        m_out.appendTo(loopStart, notEmptyValue);
        LValue unmaskedIndex = m_out.phi(Int32, indexStart);
        LValue index = m_out.bitAnd(mask, unmaskedIndex);
        // FIXME: I think these buffers are caged?
        // https://bugs.webkit.org/show_bug.cgi?id=174925
        LValue hashMapBucket = m_out.load64(m_out.baseIndex(m_heaps.properties.atAnyNumber(), buffer, m_out.zeroExt(index, Int64), ScaleEight));
        ValueFromBlock bucketResult = m_out.anchor(hashMapBucket);
        m_out.branch(m_out.equal(hashMapBucket, m_out.constIntPtr(bitwise_cast<intptr_t>(HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::emptyValue()))),
            unsure(notPresentInTable), unsure(notEmptyValue));

        m_out.appendTo(notEmptyValue, notDeletedValue);
        m_out.branch(m_out.equal(hashMapBucket, m_out.constIntPtr(bitwise_cast<intptr_t>(HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::deletedValue()))),
            unsure(loopAround), unsure(notDeletedValue));

        m_out.appendTo(notDeletedValue, loopAround);
        LValue bucketKey = m_out.load64(hashMapBucket, m_heaps.HashMapBucket_key);

        // Perform Object.is()
        switch (m_node->child2().useKind()) {
        case BooleanUse:
        case Int32Use:
        case SymbolUse:
        case ObjectUse: {
            m_out.branch(m_out.equal(key, bucketKey),
                unsure(continuation), unsure(loopAround));
            break;
        }
        case StringUse: {
            LBasicBlock notBitEqual = m_out.newBlock();
            LBasicBlock bucketKeyIsCell = m_out.newBlock();

            m_out.branch(m_out.equal(key, bucketKey),
                unsure(continuation), unsure(notBitEqual));

            m_out.appendTo(notBitEqual, bucketKeyIsCell);
            m_out.branch(isCell(bucketKey),
                unsure(bucketKeyIsCell), unsure(loopAround));

            m_out.appendTo(bucketKeyIsCell, loopAround);
            m_out.branch(isString(bucketKey),
                unsure(slowPath), unsure(loopAround));
            break;
        }
        case CellUse: {
            LBasicBlock notBitEqual = m_out.newBlock();
            LBasicBlock bucketKeyIsCell = m_out.newBlock();
            LBasicBlock bucketKeyIsString = m_out.newBlock();

            m_out.branch(m_out.equal(key, bucketKey),
                unsure(continuation), unsure(notBitEqual));

            m_out.appendTo(notBitEqual, bucketKeyIsCell);
            m_out.branch(isCell(bucketKey),
                unsure(bucketKeyIsCell), unsure(loopAround));

            m_out.appendTo(bucketKeyIsCell, bucketKeyIsString);
            m_out.branch(isString(bucketKey),
                unsure(bucketKeyIsString), unsure(loopAround));

            m_out.appendTo(bucketKeyIsString, loopAround);
            m_out.branch(isString(key),
                unsure(slowPath), unsure(loopAround));
            break;
        }
        case UntypedUse: {
            LBasicBlock notBitEqual = m_out.newBlock();
            LBasicBlock bucketKeyIsCell = m_out.newBlock();
            LBasicBlock bothAreCells = m_out.newBlock();
            LBasicBlock bucketKeyIsString = m_out.newBlock();

            m_out.branch(m_out.equal(key, bucketKey),
                unsure(continuation), unsure(notBitEqual));

            m_out.appendTo(notBitEqual, bucketKeyIsCell);
            m_out.branch(isCell(bucketKey),
                unsure(bucketKeyIsCell), unsure(loopAround));

            m_out.appendTo(bucketKeyIsCell, bothAreCells);
            m_out.branch(isCell(key),
                unsure(bothAreCells), unsure(loopAround));

            m_out.appendTo(bothAreCells, bucketKeyIsString);
            m_out.branch(isString(bucketKey),
                unsure(bucketKeyIsString), unsure(loopAround));

            m_out.appendTo(bucketKeyIsString, loopAround);
            m_out.branch(isString(key),
                unsure(slowPath), unsure(loopAround));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        m_out.appendTo(loopAround, slowPath);
        m_out.addIncomingToPhi(unmaskedIndex, m_out.anchor(m_out.add(index, m_out.int32One)));
        m_out.jump(loopStart);

        m_out.appendTo(slowPath, notPresentInTable);
        ValueFromBlock slowPathResult = m_out.anchor(vmCall(pointerType(),
            m_out.operation(m_node->child1().useKind() == MapObjectUse ? operationJSMapFindBucket : operationJSSetFindBucket), m_callFrame, map, key, hash));
        m_out.jump(continuation);

        m_out.appendTo(notPresentInTable, continuation);
        ValueFromBlock notPresentResult;
        if (m_node->child1().useKind() == MapObjectUse)
            notPresentResult = m_out.anchor(weakPointer(vm().sentinelMapBucket.get()));
        else if (m_node->child1().useKind() == SetObjectUse)
            notPresentResult = m_out.anchor(weakPointer(vm().sentinelSetBucket.get()));
        else
            RELEASE_ASSERT_NOT_REACHED();
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(pointerType(), bucketResult, slowPathResult, notPresentResult));
    }

    void compileGetMapBucketHead()
    {
        LValue map;
        if (m_node->child1().useKind() == MapObjectUse)
            map = lowMapObject(m_node->child1());
        else if (m_node->child1().useKind() == SetObjectUse)
            map = lowSetObject(m_node->child1());
        else
            RELEASE_ASSERT_NOT_REACHED();

        ASSERT(HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::offsetOfHead() == HashMapImpl<HashMapBucket<HashMapBucketDataKeyValue>>::offsetOfHead());
        setJSValue(m_out.loadPtr(map, m_heaps.HashMapImpl_head));
    }

    void compileGetMapBucketNext()
    {
        LBasicBlock loopStart = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        LBasicBlock noBucket = m_out.newBlock();
        LBasicBlock hasBucket = m_out.newBlock();
        LBasicBlock nextBucket = m_out.newBlock();

        LBasicBlock lastNext = m_out.insertNewBlocksBefore(loopStart);

        ASSERT(HashMapBucket<HashMapBucketDataKey>::offsetOfNext() == HashMapBucket<HashMapBucketDataKeyValue>::offsetOfNext());
        ASSERT(HashMapBucket<HashMapBucketDataKey>::offsetOfKey() == HashMapBucket<HashMapBucketDataKeyValue>::offsetOfKey());
        LValue mapBucketPrev = lowCell(m_node->child1());
        ValueFromBlock mapBucketStart = m_out.anchor(m_out.loadPtr(mapBucketPrev, m_heaps.HashMapBucket_next));
        m_out.jump(loopStart);

        m_out.appendTo(loopStart, noBucket);
        LValue mapBucket = m_out.phi(pointerType(), mapBucketStart);
        m_out.branch(m_out.isNull(mapBucket), unsure(noBucket), unsure(hasBucket));

        m_out.appendTo(noBucket, hasBucket);
        ValueFromBlock noBucketResult;
        if (m_node->bucketOwnerType() == BucketOwnerType::Map)
            noBucketResult = m_out.anchor(weakPointer(vm().sentinelMapBucket.get()));
        else {
            ASSERT(m_node->bucketOwnerType() == BucketOwnerType::Set);
            noBucketResult = m_out.anchor(weakPointer(vm().sentinelSetBucket.get()));
        }
        m_out.jump(continuation);

        m_out.appendTo(hasBucket, nextBucket);
        ValueFromBlock bucketResult = m_out.anchor(mapBucket);
        m_out.branch(m_out.isZero64(m_out.load64(mapBucket, m_heaps.HashMapBucket_key)), unsure(nextBucket), unsure(continuation));

        m_out.appendTo(nextBucket, continuation);
        m_out.addIncomingToPhi(mapBucket, m_out.anchor(m_out.loadPtr(mapBucket, m_heaps.HashMapBucket_next)));
        m_out.jump(loopStart);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(pointerType(), noBucketResult, bucketResult));
    }

    void compileLoadValueFromMapBucket()
    {
        LValue mapBucket = lowCell(m_node->child1());
        setJSValue(m_out.load64(mapBucket, m_heaps.HashMapBucket_value));
    }

    void compileExtractValueFromWeakMapGet()
    {
        LValue value = lowJSValue(m_node->child1());
        setJSValue(m_out.select(m_out.isZero64(value),
            m_out.constInt64(JSValue::encode(jsUndefined())),
            value));
    }

    void compileLoadKeyFromMapBucket()
    {
        LValue mapBucket = lowCell(m_node->child1());
        setJSValue(m_out.load64(mapBucket, m_heaps.HashMapBucket_key));
    }

    void compileSetAdd()
    {
        LValue set = lowSetObject(m_node->child1());
        LValue key = lowJSValue(m_node->child2());
        LValue hash = lowInt32(m_node->child3());

        setJSValue(vmCall(pointerType(), m_out.operation(operationSetAdd), m_callFrame, set, key, hash));
    }

    void compileMapSet()
    {
        LValue map = lowMapObject(m_graph.varArgChild(m_node, 0));
        LValue key = lowJSValue(m_graph.varArgChild(m_node, 1));
        LValue value = lowJSValue(m_graph.varArgChild(m_node, 2));
        LValue hash = lowInt32(m_graph.varArgChild(m_node, 3));

        setJSValue(vmCall(pointerType(), m_out.operation(operationMapSet), m_callFrame, map, key, value, hash));
    }

    void compileWeakMapGet()
    {
        LBasicBlock loopStart = m_out.newBlock();
        LBasicBlock loopAround = m_out.newBlock();
        LBasicBlock notEqualValue = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LBasicBlock lastNext = m_out.insertNewBlocksBefore(loopStart);

        LValue weakMap;
        if (m_node->child1().useKind() == WeakMapObjectUse)
            weakMap = lowWeakMapObject(m_node->child1());
        else if (m_node->child1().useKind() == WeakSetObjectUse)
            weakMap = lowWeakSetObject(m_node->child1());
        else
            RELEASE_ASSERT_NOT_REACHED();
        LValue key = lowObject(m_node->child2());
        LValue hash = lowInt32(m_node->child3());

        LValue buffer = m_out.loadPtr(weakMap, m_heaps.WeakMapImpl_buffer);
        LValue mask = m_out.sub(m_out.load32(weakMap, m_heaps.WeakMapImpl_capacity), m_out.int32One);

        ValueFromBlock indexStart = m_out.anchor(hash);
        m_out.jump(loopStart);

        m_out.appendTo(loopStart, notEqualValue);
        LValue unmaskedIndex = m_out.phi(Int32, indexStart);
        LValue index = m_out.bitAnd(mask, unmaskedIndex);

        LValue bucket;

        if (m_node->child1().useKind() == WeakMapObjectUse) {
            static_assert(hasOneBitSet(sizeof(WeakMapBucket<WeakMapBucketDataKeyValue>)), "Should be a power of 2");
            bucket = m_out.add(buffer, m_out.shl(m_out.zeroExt(index, Int64), m_out.constInt32(getLSBSet(sizeof(WeakMapBucket<WeakMapBucketDataKeyValue>)))));
        } else {
            static_assert(hasOneBitSet(sizeof(WeakMapBucket<WeakMapBucketDataKey>)), "Should be a power of 2");
            bucket = m_out.add(buffer, m_out.shl(m_out.zeroExt(index, Int64), m_out.constInt32(getLSBSet(sizeof(WeakMapBucket<WeakMapBucketDataKey>)))));
        }

        LValue bucketKey = m_out.load64(bucket, m_heaps.WeakMapBucket_key);
        m_out.branch(m_out.equal(key, bucketKey), unsure(continuation), unsure(notEqualValue));

        m_out.appendTo(notEqualValue, loopAround);
        m_out.branch(m_out.isNull(bucketKey), unsure(continuation), unsure(loopAround));

        m_out.appendTo(loopAround, continuation);
        m_out.addIncomingToPhi(unmaskedIndex, m_out.anchor(m_out.add(index, m_out.int32One)));
        m_out.jump(loopStart);

        m_out.appendTo(continuation, lastNext);
        LValue result;
        if (m_node->child1().useKind() == WeakMapObjectUse)
            result = m_out.load64(bucket, m_heaps.WeakMapBucket_value);
        else
            result = bucketKey;
        setJSValue(result);
    }

    void compileWeakSetAdd()
    {
        LValue set = lowWeakSetObject(m_node->child1());
        LValue key = lowObject(m_node->child2());
        LValue hash = lowInt32(m_node->child3());

        vmCall(Void, m_out.operation(operationWeakSetAdd), m_callFrame, set, key, hash);
    }

    void compileWeakMapSet()
    {
        LValue map = lowWeakMapObject(m_graph.varArgChild(m_node, 0));
        LValue key = lowObject(m_graph.varArgChild(m_node, 1));
        LValue value = lowJSValue(m_graph.varArgChild(m_node, 2));
        LValue hash = lowInt32(m_graph.varArgChild(m_node, 3));

        vmCall(Void, m_out.operation(operationWeakMapSet), m_callFrame, map, key, value, hash);
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
        VM& vm = this->vm();
        LValue slowResultValue = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
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
            Int32,
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
        VM& vm = this->vm();
        LValue slowResultValue = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
                    operationObjectIsFunction, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(globalObject), locations[1].directGPR());
            }, value);
        ValueFromBlock slowResult = m_out.anchor(m_out.notNull(slowResultValue));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        LValue result = m_out.phi(
            Int32, notCellResult, functionResult, objectResult, slowResult);
        setBoolean(result);
    }

    void compileIsTypedArrayView()
    {
        LValue value = lowJSValue(m_node->child1());

        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
        m_out.branch(isCell(value, provenType(m_node->child1())), unsure(isCellCase), unsure(continuation));

        LBasicBlock lastNext = m_out.appendTo(isCellCase, continuation);
        ValueFromBlock cellResult = m_out.anchor(isTypedArrayView(value, provenType(m_node->child1())));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, notCellResult, cellResult));
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
        setJSValue(m_out.phi(Int64, results));
    }
    
    void compileInByVal()
    {
        setJSValue(vmCall(Int64, m_out.operation(operationInByVal), m_callFrame, lowCell(m_node->child1()), lowJSValue(m_node->child2())));
    }

    void compileInById()
    {
        Node* node = m_node;
        UniquedStringImpl* uid = m_graph.identifiers()[node->identifierNumber()];
        LValue base = lowCell(m_node->child1());

        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(base);
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));

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

                auto generator = Box<JITInByIdGenerator>::create(
                    jit.codeBlock(), node->origin.semantic, callSiteIndex,
                    params.unavailableRegisters(), uid, JSValueRegs(params[1].gpr()),
                    JSValueRegs(params[0].gpr()));

                generator->generateFastPath(jit);
                CCallHelpers::Label done = jit.label();

                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);

                        generator->slowPathJump().link(&jit);
                        CCallHelpers::Label slowPathBegin = jit.label();
                        CCallHelpers::Call slowPathCall = callOperation(
                            *state, params.unavailableRegisters(), jit, node->origin.semantic,
                            exceptions.get(), operationInByIdOptimize, params[0].gpr(),
                            CCallHelpers::TrustedImmPtr(generator->stubInfo()), params[1].gpr(),
                            CCallHelpers::TrustedImmPtr(uid)).call();
                        jit.jump().linkTo(done, &jit);

                        generator->reportSlowPathCall(slowPathBegin, slowPathCall);

                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                generator->finalize(linkBuffer, linkBuffer);
                            });
                    });
            });

        setJSValue(patchpoint);
    }

    void compileHasOwnProperty()
    {
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        LBasicBlock lastNext = nullptr;

        LValue object = lowObject(m_node->child1());
        LValue uniquedStringImpl;
        LValue keyAsValue = nullptr;
        switch (m_node->child2().useKind()) {
        case StringUse: {
            LBasicBlock isNonEmptyString = m_out.newBlock();
            LBasicBlock isAtomicString = m_out.newBlock();

            keyAsValue = lowString(m_node->child2());
            uniquedStringImpl = m_out.loadPtr(keyAsValue, m_heaps.JSString_value);
            m_out.branch(m_out.notNull(uniquedStringImpl), usually(isNonEmptyString), rarely(slowCase));

            lastNext = m_out.appendTo(isNonEmptyString, isAtomicString);
            LValue isNotAtomic = m_out.testIsZero32(m_out.load32(uniquedStringImpl, m_heaps.StringImpl_hashAndFlags), m_out.constInt32(StringImpl::flagIsAtomic()));
            m_out.branch(isNotAtomic, rarely(slowCase), usually(isAtomicString));

            m_out.appendTo(isAtomicString, slowCase);
            break;
        }
        case SymbolUse: {
            keyAsValue = lowSymbol(m_node->child2());
            uniquedStringImpl = m_out.loadPtr(keyAsValue, m_heaps.Symbol_symbolImpl);
            lastNext = m_out.insertNewBlocksBefore(slowCase);
            break;
        }
        case UntypedUse: {
            LBasicBlock isCellCase = m_out.newBlock();
            LBasicBlock isStringCase = m_out.newBlock();
            LBasicBlock notStringCase = m_out.newBlock();
            LBasicBlock isNonEmptyString = m_out.newBlock();
            LBasicBlock isSymbolCase = m_out.newBlock();
            LBasicBlock hasUniquedStringImpl = m_out.newBlock();

            keyAsValue = lowJSValue(m_node->child2());
            m_out.branch(isCell(keyAsValue), usually(isCellCase), rarely(slowCase));

            lastNext = m_out.appendTo(isCellCase, isStringCase);
            m_out.branch(isString(keyAsValue), unsure(isStringCase), unsure(notStringCase));

            m_out.appendTo(isStringCase, isNonEmptyString);
            LValue implFromString = m_out.loadPtr(keyAsValue, m_heaps.JSString_value);
            ValueFromBlock stringResult = m_out.anchor(implFromString);
            m_out.branch(m_out.notNull(implFromString), usually(isNonEmptyString), rarely(slowCase));

            m_out.appendTo(isNonEmptyString, notStringCase);
            LValue isNotAtomic = m_out.testIsZero32(m_out.load32(implFromString, m_heaps.StringImpl_hashAndFlags), m_out.constInt32(StringImpl::flagIsAtomic()));
            m_out.branch(isNotAtomic, rarely(slowCase), usually(hasUniquedStringImpl));

            m_out.appendTo(notStringCase, isSymbolCase);
            m_out.branch(isSymbol(keyAsValue), unsure(isSymbolCase), unsure(slowCase));

            m_out.appendTo(isSymbolCase, hasUniquedStringImpl);
            ValueFromBlock symbolResult = m_out.anchor(m_out.loadPtr(keyAsValue, m_heaps.Symbol_symbolImpl));
            m_out.jump(hasUniquedStringImpl);

            m_out.appendTo(hasUniquedStringImpl, slowCase);
            uniquedStringImpl = m_out.phi(pointerType(), stringResult, symbolResult);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        ASSERT(keyAsValue);

        // Note that we don't test if the hash is zero here. AtomicStringImpl's can't have a zero
        // hash, however, a SymbolImpl may. But, because this is a cache, we don't care. We only
        // ever load the result from the cache if the cache entry matches what we are querying for.
        // So we either get super lucky and use zero for the hash and somehow collide with the entity
        // we're looking for, or we realize we're comparing against another entity, and go to the
        // slow path anyways.
        LValue hash = m_out.lShr(m_out.load32(uniquedStringImpl, m_heaps.StringImpl_hashAndFlags), m_out.constInt32(StringImpl::s_flagCount));

        LValue structureID = m_out.load32(object, m_heaps.JSCell_structureID);
        LValue index = m_out.add(hash, structureID);
        index = m_out.zeroExtPtr(m_out.bitAnd(index, m_out.constInt32(HasOwnPropertyCache::mask)));
        ASSERT(vm().hasOwnPropertyCache());
        LValue cache = m_out.constIntPtr(vm().hasOwnPropertyCache());

        IndexedAbstractHeap& heap = m_heaps.HasOwnPropertyCache;
        LValue sameStructureID = m_out.equal(structureID, m_out.load32(m_out.baseIndex(heap, cache, index, JSValue(), HasOwnPropertyCache::Entry::offsetOfStructureID())));
        LValue sameImpl = m_out.equal(uniquedStringImpl, m_out.loadPtr(m_out.baseIndex(heap, cache, index, JSValue(), HasOwnPropertyCache::Entry::offsetOfImpl())));
        ValueFromBlock fastResult = m_out.anchor(m_out.load8ZeroExt32(m_out.baseIndex(heap, cache, index, JSValue(), HasOwnPropertyCache::Entry::offsetOfResult())));
        LValue cacheHit = m_out.bitAnd(sameStructureID, sameImpl);

        m_out.branch(m_out.notZero32(cacheHit), usually(continuation), rarely(slowCase));

        m_out.appendTo(slowCase, continuation);
        ValueFromBlock slowResult;
        slowResult = m_out.anchor(vmCall(Int32, m_out.operation(operationHasOwnProperty), m_callFrame, object, keyAsValue));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, fastResult, slowResult));
    }

    void compileParseInt()
    {
        RELEASE_ASSERT(m_node->child1().useKind() == UntypedUse || m_node->child1().useKind() == StringUse);
        LValue result;
        if (m_node->child2()) {
            LValue radix = lowInt32(m_node->child2());
            if (m_node->child1().useKind() == UntypedUse)
                result = vmCall(Int64, m_out.operation(operationParseIntGeneric), m_callFrame, lowJSValue(m_node->child1()), radix);
            else
                result = vmCall(Int64, m_out.operation(operationParseIntString), m_callFrame, lowString(m_node->child1()), radix);
        } else {
            if (m_node->child1().useKind() == UntypedUse)
                result = vmCall(Int64, m_out.operation(operationParseIntNoRadixGeneric), m_callFrame, lowJSValue(m_node->child1()));
            else
                result = vmCall(Int64, m_out.operation(operationParseIntStringNoRadix), m_callFrame, lowString(m_node->child1()));
        }
        setJSValue(result);
    }

    void compileOverridesHasInstance()
    {
        FrozenValue* defaultHasInstanceFunction = m_node->cellOperand();
        ASSERT(defaultHasInstanceFunction->cell()->inherits<JSFunction>(vm()));

        LValue constructor = lowCell(m_node->child1());
        LValue hasInstance = lowJSValue(m_node->child2());

        LBasicBlock defaultHasInstance = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        // Unlike in the DFG, we don't worry about cleaning this code up for the case where we have proven the hasInstanceValue is a constant as B3 should fix it for us.

        ValueFromBlock notDefaultHasInstanceResult = m_out.anchor(m_out.booleanTrue);
        m_out.branch(m_out.notEqual(hasInstance, frozenPointer(defaultHasInstanceFunction)), unsure(continuation), unsure(defaultHasInstance));

        LBasicBlock lastNext = m_out.appendTo(defaultHasInstance, continuation);
        ValueFromBlock implementsDefaultHasInstanceResult = m_out.anchor(m_out.testIsZero32(
            m_out.load8ZeroExt32(constructor, m_heaps.JSCell_typeInfoFlags),
            m_out.constInt32(ImplementsDefaultHasInstance)));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, implementsDefaultHasInstanceResult, notDefaultHasInstanceResult));
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
        Node* node = m_node;
        State* state = &m_ftlState;
        
        LValue value;
        LValue prototype;
        bool valueIsCell;
        bool prototypeIsCell;
        if (m_node->child1().useKind() == CellUse
            && m_node->child2().useKind() == CellUse) {
            value = lowCell(m_node->child1());
            prototype = lowCell(m_node->child2());
            
            valueIsCell = true;
            prototypeIsCell = true;
        } else {
            DFG_ASSERT(m_graph, m_node, m_node->child1().useKind() == UntypedUse);
            DFG_ASSERT(m_graph, m_node, m_node->child2().useKind() == UntypedUse);
            
            value = lowJSValue(m_node->child1());
            prototype = lowJSValue(m_node->child2());
            
            valueIsCell = abstractValue(m_node->child1()).isType(SpecCell);
            prototypeIsCell = abstractValue(m_node->child2()).isType(SpecCell);
        }
        
        bool prototypeIsObject = abstractValue(m_node->child2()).isType(SpecObject | ~SpecCell);
        
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(value);
        patchpoint->appendSomeRegister(prototype);
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));
        patchpoint->numGPScratchRegisters = 2;
        patchpoint->resultConstraint = ValueRep::SomeEarlyRegister;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        
        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);

        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                
                GPRReg resultGPR = params[0].gpr();
                GPRReg valueGPR = params[1].gpr();
                GPRReg prototypeGPR = params[2].gpr();
                GPRReg scratchGPR = params.gpScratch(0);
                GPRReg scratch2GPR = params.gpScratch(1);
                
                CCallHelpers::Jump doneJump;
                if (!valueIsCell) {
                    CCallHelpers::Jump isCell = jit.branchIfCell(valueGPR);
                    jit.boxBooleanPayload(false, resultGPR);
                    doneJump = jit.jump();
                    isCell.link(&jit);
                }
                
                CCallHelpers::JumpList slowCases;
                if (!prototypeIsCell)
                    slowCases.append(jit.branchIfNotCell(prototypeGPR));
                
                CallSiteIndex callSiteIndex =
                    state->jitCode->common.addUniqueCallSiteIndex(node->origin.semantic);
                
                // This is the direct exit target for operation calls.
                Box<CCallHelpers::JumpList> exceptions =
                    exceptionHandle->scheduleExitCreation(params)->jumps(jit);
                
                auto generator = Box<JITInstanceOfGenerator>::create(
                    jit.codeBlock(), node->origin.semantic, callSiteIndex,
                    params.unavailableRegisters(), resultGPR, valueGPR, prototypeGPR, scratchGPR,
                    scratch2GPR, prototypeIsObject);
                generator->generateFastPath(jit);
                CCallHelpers::Label done = jit.label();
                
                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);
                        
                        J_JITOperation_ESsiJJ optimizationFunction = operationInstanceOfOptimize;
                        
                        slowCases.link(&jit);
                        CCallHelpers::Label slowPathBegin = jit.label();
                        CCallHelpers::Call slowPathCall = callOperation(
                            *state, params.unavailableRegisters(), jit, node->origin.semantic,
                            exceptions.get(), optimizationFunction, resultGPR,
                            CCallHelpers::TrustedImmPtr(generator->stubInfo()), valueGPR,
                            prototypeGPR).call();
                        jit.jump().linkTo(done, &jit);
                        
                        generator->reportSlowPathCall(slowPathBegin, slowPathCall);
                        
                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                generator->finalize(linkBuffer, linkBuffer);
                            });
                    });
                
                if (doneJump.isSet())
                    doneJump.link(&jit);
            });
        
        // This returns a boxed boolean.
        setJSValue(patchpoint);
    }

    void compileInstanceOfCustom()
    {
        LValue value = lowJSValue(m_node->child1());
        LValue constructor = lowCell(m_node->child2());
        LValue hasInstance = lowJSValue(m_node->child3());

        setBoolean(m_out.logicalNot(m_out.equal(m_out.constInt32(0), vmCall(Int32, m_out.operation(operationInstanceOfCustom), m_callFrame, value, constructor, hasInstance))));
    }
    
    void compileCountExecution()
    {
        TypedPointer counter = m_out.absolute(m_node->executionCounter()->address());
        m_out.store64(m_out.add(m_out.load64(counter), m_out.constInt64(1)), counter);
    }

    void compileSuperSamplerBegin()
    {
        TypedPointer counter = m_out.absolute(bitwise_cast<void*>(&g_superSamplerCount));
        m_out.store32(m_out.add(m_out.load32(counter), m_out.constInt32(1)), counter);
    }

    void compileSuperSamplerEnd()
    {
        TypedPointer counter = m_out.absolute(bitwise_cast<void*>(&g_superSamplerCount));
        m_out.store32(m_out.sub(m_out.load32(counter), m_out.constInt32(1)), counter);
    }

    void compileStoreBarrier()
    {
        emitStoreBarrier(lowCell(m_node->child1()), m_node->op() == FencedStoreBarrier);
    }
    
    void compileHasIndexedProperty()
    {
        switch (m_node->arrayMode().type()) {
        case Array::Int32:
        case Array::Contiguous: {
            LValue base = lowCell(m_node->child1());
            LValue index = lowInt32(m_node->child2());
            LValue storage = lowStorage(m_node->child3());
            LValue internalMethodType = m_out.constInt32(static_cast<int32_t>(m_node->internalMethodType()));

            IndexedAbstractHeap& heap = m_node->arrayMode().type() == Array::Int32 ?
                m_heaps.indexedInt32Properties : m_heaps.indexedContiguousProperties;

            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            LBasicBlock lastNext = nullptr;

            if (!m_node->arrayMode().isInBounds()) {
                LBasicBlock checkHole = m_out.newBlock();
                m_out.branch(
                    m_out.aboveOrEqual(
                        index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength)),
                    rarely(slowCase), usually(checkHole));
                lastNext = m_out.appendTo(checkHole, slowCase);
            } else
                lastNext = m_out.insertNewBlocksBefore(slowCase);

            LValue checkHoleResultValue =
                m_out.notZero64(m_out.load64(baseIndex(heap, storage, index, m_node->child2())));
            ValueFromBlock checkHoleResult = m_out.anchor(checkHoleResultValue);
            m_out.branch(checkHoleResultValue, usually(continuation), rarely(slowCase));

            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                m_out.notZero64(vmCall(Int64, m_out.operation(operationHasIndexedPropertyByInt), m_callFrame, base, index, internalMethodType)));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(Int32, checkHoleResult, slowResult));
            return;
        }
        case Array::Double: {
            LValue base = lowCell(m_node->child1());
            LValue index = lowInt32(m_node->child2());
            LValue storage = lowStorage(m_node->child3());
            LValue internalMethodType = m_out.constInt32(static_cast<int32_t>(m_node->internalMethodType()));
            
            IndexedAbstractHeap& heap = m_heaps.indexedDoubleProperties;
            
            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            LBasicBlock lastNext = nullptr;
            
            if (!m_node->arrayMode().isInBounds()) {
                LBasicBlock checkHole = m_out.newBlock();
                m_out.branch(
                    m_out.aboveOrEqual(
                        index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength)),
                    rarely(slowCase), usually(checkHole));
                lastNext = m_out.appendTo(checkHole, slowCase);
            } else
                lastNext = m_out.insertNewBlocksBefore(slowCase);

            LValue doubleValue = m_out.loadDouble(baseIndex(heap, storage, index, m_node->child2()));
            LValue checkHoleResultValue = m_out.doubleEqual(doubleValue, doubleValue);
            ValueFromBlock checkHoleResult = m_out.anchor(checkHoleResultValue);
            m_out.branch(checkHoleResultValue, usually(continuation), rarely(slowCase));
            
            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                m_out.notZero64(vmCall(Int64, m_out.operation(operationHasIndexedPropertyByInt), m_callFrame, base, index, internalMethodType)));
            m_out.jump(continuation);
            
            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(Int32, checkHoleResult, slowResult));
            return;
        }

        case Array::ArrayStorage: {
            LValue base = lowCell(m_node->child1());
            LValue index = lowInt32(m_node->child2());
            LValue storage = lowStorage(m_node->child3());
            LValue internalMethodType = m_out.constInt32(static_cast<int32_t>(m_node->internalMethodType()));

            LBasicBlock slowCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            LBasicBlock lastNext = nullptr;

            if (!m_node->arrayMode().isInBounds()) {
                LBasicBlock checkHole = m_out.newBlock();
                m_out.branch(
                    m_out.aboveOrEqual(
                        index, m_out.load32NonNegative(storage, m_heaps.ArrayStorage_vectorLength)),
                    rarely(slowCase), usually(checkHole));
                lastNext = m_out.appendTo(checkHole, slowCase);
            } else
                lastNext = m_out.insertNewBlocksBefore(slowCase);

            LValue checkHoleResultValue =
                m_out.notZero64(m_out.load64(baseIndex(m_heaps.ArrayStorage_vector, storage, index, m_node->child2())));
            ValueFromBlock checkHoleResult = m_out.anchor(checkHoleResultValue);
            m_out.branch(checkHoleResultValue, usually(continuation), rarely(slowCase));

            m_out.appendTo(slowCase, continuation);
            ValueFromBlock slowResult = m_out.anchor(
                m_out.notZero64(vmCall(Int64, m_out.operation(operationHasIndexedPropertyByInt), m_callFrame, base, index, internalMethodType)));
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            setBoolean(m_out.phi(Int32, checkHoleResult, slowResult));
            break;
        }

        default: {
            LValue base = lowCell(m_node->child1());
            LValue index = lowInt32(m_node->child2());
            LValue internalMethodType = m_out.constInt32(static_cast<int32_t>(m_node->internalMethodType()));
            setBoolean(m_out.notZero64(vmCall(Int64, m_out.operation(operationHasIndexedPropertyByInt), m_callFrame, base, index, internalMethodType)));
            break;
        }
        }
    }

    void compileHasGenericProperty()
    {
        LValue base = lowJSValue(m_node->child1());
        LValue property = lowCell(m_node->child2());
        setJSValue(vmCall(Int64, m_out.operation(operationHasGenericProperty), m_callFrame, base, property));
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
                vmCall(Int64, m_out.operation(operationHasGenericProperty), m_callFrame, base, property)));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, correctStructureResult, wrongStructureResult));
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
                base, m_out.zeroExt(index, Int64), ScaleEight, JSObject::offsetOfInlineStorage())));
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
            vmCall(Int64, m_out.operation(operationGetByVal), m_callFrame, base, property));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(Int64, inlineResult, outOfLineResult, slowCaseResult));
    }

    void compileGetEnumerableLength()
    {
        LValue enumerator = lowCell(m_node->child1());
        setInt32(m_out.load32(enumerator, m_heaps.JSPropertyNameEnumerator_indexLength));
    }

    void compileGetPropertyEnumerator()
    {
        if (m_node->child1().useKind() == CellUse)
            setJSValue(vmCall(Int64, m_out.operation(operationGetPropertyEnumeratorCell), m_callFrame, lowCell(m_node->child1())));
        else
            setJSValue(vmCall(Int64, m_out.operation(operationGetPropertyEnumerator), m_callFrame, lowJSValue(m_node->child1())));
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
        setJSValue(m_out.phi(Int64, inBoundsResult, outOfBoundsResult));
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
        setJSValue(m_out.phi(Int64, inBoundsResult, outOfBoundsResult));
    }
    
    void compileToIndexString()
    {
        LValue index = lowInt32(m_node->child1());
        setJSValue(vmCall(Int64, m_out.operation(operationToIndexString), m_callFrame, index));
    }
    
    void compileCheckStructureImmediate()
    {
        LValue structure = lowCell(m_node->child1());
        checkStructure(
            structure, noValue(), BadCache, m_node->structureSet(),
            [this] (RegisteredStructure structure) {
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
        
        RegisteredStructureSet set = m_node->structureSet();

        Vector<LBasicBlock, 1> blocks(set.size());
        for (unsigned i = set.size(); i--;)
            blocks[i] = m_out.newBlock();
        LBasicBlock dummyDefault = m_out.newBlock();
        LBasicBlock outerContinuation = m_out.newBlock();
        
        Vector<SwitchCase, 1> cases(set.size());
        for (unsigned i = set.size(); i--;)
            cases[i] = SwitchCase(weakStructure(set.at(i)), blocks[i], Weight(1));
        m_out.switchInstruction(
            lowCell(m_graph.varArgChild(m_node, 0)), cases, dummyDefault, Weight(0));
        
        LBasicBlock outerLastNext = m_out.m_nextBlock;
        
        Vector<ValueFromBlock, 1> results;
        
        for (unsigned i = set.size(); i--;) {
            m_out.appendTo(blocks[i], i + 1 < set.size() ? blocks[i + 1] : dummyDefault);
            
            RegisteredStructure structure = set.at(i);
            
            LValue object;
            LValue butterfly;
            
            if (structure->outOfLineCapacity() || hasIndexedProperties(structure->indexingType())) {
                size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
                Allocator cellAllocator = allocatorForNonVirtualConcurrently<JSFinalObject>(vm(), allocationSize, AllocatorForMode::AllocatorIfExists);

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
                
                ValueFromBlock noButterfly = m_out.anchor(m_out.intPtrZero);
                
                LValue startOfStorage = allocateHeapCell(
                    allocatorForSize(vm().jsValueGigacageAuxiliarySpace, butterflySize, slowPath),
                    slowPath);

                LValue fastButterflyValue = m_out.add(
                    startOfStorage,
                    m_out.constIntPtr(
                        structure->outOfLineCapacity() * sizeof(JSValue) + sizeof(IndexingHeader)));
                
                ValueFromBlock haveButterfly = m_out.anchor(fastButterflyValue);
                
                splatWords(
                    fastButterflyValue,
                    m_out.constInt32(-structure->outOfLineCapacity() - 1),
                    m_out.constInt32(-1),
                    m_out.int64Zero, m_heaps.properties.atAnyNumber());

                m_out.store32(vectorLength, fastButterflyValue, m_heaps.Butterfly_vectorLength);

                LValue fastObjectValue = allocateObject(
                    m_out.constIntPtr(cellAllocator.localAllocator()), structure, fastButterflyValue,
                    slowPath);

                ValueFromBlock fastObject = m_out.anchor(fastObjectValue);
                ValueFromBlock fastButterfly = m_out.anchor(fastButterflyValue);
                m_out.jump(continuation);
                
                m_out.appendTo(slowPath, continuation);
                
                LValue butterflyValue = m_out.phi(pointerType(), noButterfly, haveButterfly);

                VM& vm = this->vm();
                LValue slowObjectValue;
                if (hasIndexingHeader) {
                    slowObjectValue = lazySlowPath(
                        [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                            return createLazyCallGenerator(vm,
                                operationNewObjectWithButterflyWithIndexingHeaderAndVectorLength,
                                locations[0].directGPR(), CCallHelpers::TrustedImmPtr(structure.get()),
                                locations[1].directGPR(), locations[2].directGPR());
                        },
                        vectorLength, butterflyValue);
                } else {
                    slowObjectValue = lazySlowPath(
                        [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                            return createLazyCallGenerator(vm,
                                operationNewObjectWithButterfly, locations[0].directGPR(),
                                CCallHelpers::TrustedImmPtr(structure.get()), locations[1].directGPR());
                        },
                        butterflyValue);
                }
                ValueFromBlock slowObject = m_out.anchor(slowObjectValue);
                ValueFromBlock slowButterfly = m_out.anchor(
                    m_out.loadPtr(slowObjectValue, m_heaps.JSObject_butterfly));
                
                m_out.jump(continuation);
                
                m_out.appendTo(continuation, lastNext);
                
                object = m_out.phi(pointerType(), fastObject, slowObject);
                butterfly = m_out.phi(pointerType(), fastButterfly, slowButterfly);

                m_out.store32(publicLength, butterfly, m_heaps.Butterfly_publicLength);

                initializeArrayElements(m_out.constInt32(structure->indexingType()), m_out.int32Zero, vectorLength, butterfly);

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

            BitVector setInlineOffsets;
            for (PropertyMapEntry entry : structure->getPropertiesConcurrently()) {
                for (unsigned i = data.m_properties.size(); i--;) {
                    PromotedLocationDescriptor descriptor = data.m_properties[i];
                    if (descriptor.kind() != NamedPropertyPLoc)
                        continue;
                    if (m_graph.identifiers()[descriptor.info()] != entry.key)
                        continue;
                    
                    LValue base;
                    if (isInlineOffset(entry.offset)) {
                        setInlineOffsets.set(entry.offset);
                        base = object;
                    } else
                        base = butterfly;
                    storeProperty(values[i], base, descriptor.info(), entry.offset);
                    break;
                }
            }
            for (unsigned i = structure->inlineCapacity(); i--;) {
                if (!setInlineOffsets.get(i))
                    m_out.store64(m_out.int64Zero, m_out.address(m_heaps.properties.atAnyNumber(), object, offsetRelativeToBase(i)));
            }
            
            results.append(m_out.anchor(object));
            m_out.jump(outerContinuation);
        }
        
        m_out.appendTo(dummyDefault, outerContinuation);
        m_out.unreachable();
        
        m_out.appendTo(outerContinuation, outerLastNext);
        setJSValue(m_out.phi(pointerType(), results));
        mutatorFence();
    }

    void compileMaterializeCreateActivation()
    {
        ObjectMaterializationData& data = m_node->objectMaterializationData();

        Vector<LValue, 8> values;
        for (unsigned i = 0; i < data.m_properties.size(); ++i)
            values.append(lowJSValue(m_graph.varArgChild(m_node, 2 + i)));

        LValue scope = lowCell(m_graph.varArgChild(m_node, 1));
        SymbolTable* table = m_node->castOperand<SymbolTable*>();
        ASSERT(table == m_graph.varArgChild(m_node, 0)->castConstant<SymbolTable*>(vm()));
        RegisteredStructure structure = m_graph.registerStructure(m_graph.globalObjectFor(m_node->origin.semantic)->activationStructure());

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
        VM& vm = this->vm();
        LValue callResult = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
                    operationCreateActivationDirect, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure.get()), locations[1].directGPR(),
                    CCallHelpers::TrustedImmPtr(table),
                    CCallHelpers::TrustedImm64(JSValue::encode(jsUndefined())));
            }, scope);
        ValueFromBlock slowResult =  m_out.anchor(callResult);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        LValue activation = m_out.phi(pointerType(), fastResult, slowResult);
        RELEASE_ASSERT(data.m_properties.size() == table->scopeSize());
        for (unsigned i = 0; i < data.m_properties.size(); ++i) {
            PromotedLocationDescriptor descriptor = data.m_properties[i];
            ASSERT(descriptor.kind() == ClosureVarPLoc);
            m_out.store64(
                values[i], activation,
                m_heaps.JSLexicalEnvironment_variables[descriptor.info()]);
        }

        if (validationEnabled()) {
            // Validate to make sure every slot in the scope has one value.
            ConcurrentJSLocker locker(table->m_lock);
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

        mutatorFence();
        setJSValue(activation);
    }

    void compileCheckTraps()
    {
        ASSERT(Options::usePollingTraps());
        LBasicBlock needTrapHandling = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue state = m_out.load8ZeroExt32(m_out.absolute(vm().needTrapHandlingAddress()));
        m_out.branch(m_out.isZero32(state),
            usually(continuation), rarely(needTrapHandling));

        LBasicBlock lastNext = m_out.appendTo(needTrapHandling, continuation);

        VM& vm = this->vm();
        lazySlowPath(
            [=, &vm] (const Vector<Location>&) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm, operationHandleTraps, InvalidGPRReg);
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

    void compileRegExpExecNonGlobalOrSticky()
    {
        LValue globalObject = lowCell(m_node->child1());
        LValue argument = lowString(m_node->child2());
        LValue result = vmCall(
            Int64, m_out.operation(operationRegExpExecNonGlobalOrSticky), m_callFrame, globalObject, frozenPointer(m_node->cellOperand()), argument);
        setJSValue(result);
    }

    void compileRegExpMatchFastGlobal()
    {
        LValue globalObject = lowCell(m_node->child1());
        LValue argument = lowString(m_node->child2());
        LValue result = vmCall(
            Int64, m_out.operation(operationRegExpMatchFastGlobalString), m_callFrame, globalObject, frozenPointer(m_node->cellOperand()), argument);
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

    void compileRegExpMatchFast()
    {
        LValue globalObject = lowCell(m_node->child1());
        LValue base = lowRegExpObject(m_node->child2());
        LValue argument = lowString(m_node->child3());
        LValue result = vmCall(
            Int64, m_out.operation(operationRegExpMatchFastString), m_callFrame, globalObject,
            base, argument);
        setJSValue(result);
    }

    void compileNewRegexp()
    {
        FrozenValue* regexp = m_node->cellOperand();
        LValue lastIndex = lowJSValue(m_node->child1());
        ASSERT(regexp->cell()->inherits<RegExp>(vm()));
        ASSERT(m_node->castOperand<RegExp*>()->isValid());

        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowCase);

        auto structure = m_graph.registerStructure(m_graph.globalObjectFor(m_node->origin.semantic)->regExpStructure());
        LValue fastResultValue = allocateObject<RegExpObject>(structure, m_out.intPtrZero, slowCase);
        m_out.storePtr(frozenPointer(regexp), fastResultValue, m_heaps.RegExpObject_regExp);
        m_out.store64(lastIndex, fastResultValue, m_heaps.RegExpObject_lastIndex);
        m_out.store32As8(m_out.constInt32(true), m_out.address(fastResultValue, m_heaps.RegExpObject_lastIndexIsWritable));
        mutatorFence();
        ValueFromBlock fastResult = m_out.anchor(fastResultValue);
        m_out.jump(continuation);

        m_out.appendTo(slowCase, continuation);
        VM& vm = this->vm();
        RegExp* regexpCell = regexp->cast<RegExp*>();
        LValue slowResultValue = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
                    operationNewRegexpWithLastIndex, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(regexpCell), locations[1].directGPR());
            }, lastIndex);
        ValueFromBlock slowResult = m_out.anchor(slowResultValue);
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(pointerType(), fastResult, slowResult));
    }

    void compileSetFunctionName()
    {
        vmCall(Void, m_out.operation(operationSetFunctionName), m_callFrame,
            lowCell(m_node->child1()), lowJSValue(m_node->child2()));
    }
    
    void compileStringReplace()
    {
        if (m_node->child1().useKind() == StringUse
            && m_node->child2().useKind() == RegExpObjectUse
            && m_node->child3().useKind() == StringUse) {

            if (JSString* replace = m_node->child3()->dynamicCastConstant<JSString*>(vm())) {
                if (!replace->length()) {
                    LValue string = lowString(m_node->child1());
                    LValue regExp = lowRegExpObject(m_node->child2());

                    LValue result = vmCall(
                        pointerType(), m_out.operation(operationStringProtoFuncReplaceRegExpEmptyStr),
                        m_callFrame, string, regExp);

                    setJSValue(result);
                    return;
                }
            }
            
            LValue string = lowString(m_node->child1());
            LValue regExp = lowRegExpObject(m_node->child2());
            LValue replace = lowString(m_node->child3());

            LValue result = vmCall(
                pointerType(), m_out.operation(operationStringProtoFuncReplaceRegExpString),
                m_callFrame, string, regExp, replace);

            setJSValue(result);
            return;
        }

        LValue search;
        if (m_node->child2().useKind() == StringUse)
            search = lowString(m_node->child2());
        else
            search = lowJSValue(m_node->child2());

        LValue result = vmCall(
            pointerType(), m_out.operation(operationStringProtoFuncReplaceGeneric), m_callFrame,
            lowJSValue(m_node->child1()), search,
            lowJSValue(m_node->child3()));

        setJSValue(result);
    }

    void compileGetRegExpObjectLastIndex()
    {
        setJSValue(m_out.load64(lowRegExpObject(m_node->child1()), m_heaps.RegExpObject_lastIndex));
    }

    void compileSetRegExpObjectLastIndex()
    {
        if (!m_node->ignoreLastIndexIsWritable()) {
            LValue regExp = lowRegExpObject(m_node->child1());
            LValue value = lowJSValue(m_node->child2());

            speculate(
                ExoticObjectMode, noValue(), nullptr,
                m_out.isZero32(m_out.load8ZeroExt32(regExp, m_heaps.RegExpObject_lastIndexIsWritable)));

            m_out.store64(value, regExp, m_heaps.RegExpObject_lastIndex);
            return;
        }
        
        m_out.store64(lowJSValue(m_node->child2()), lowCell(m_node->child1()), m_heaps.RegExpObject_lastIndex);
    }
    
    void compileLogShadowChickenPrologue()
    {
        LValue packet = ensureShadowChickenPacket();
        LValue scope = lowCell(m_node->child1());

        m_out.storePtr(m_callFrame, packet, m_heaps.ShadowChicken_Packet_frame);
        m_out.storePtr(m_out.loadPtr(addressFor(0)), packet, m_heaps.ShadowChicken_Packet_callerFrame);
        m_out.storePtr(m_out.loadPtr(payloadFor(CallFrameSlot::callee)), packet, m_heaps.ShadowChicken_Packet_callee);
        m_out.storePtr(scope, packet, m_heaps.ShadowChicken_Packet_scope);
    }
    
    void compileLogShadowChickenTail()
    {
        LValue packet = ensureShadowChickenPacket();
        LValue thisValue = lowJSValue(m_node->child1());
        LValue scope = lowCell(m_node->child2());
        CallSiteIndex callSiteIndex = m_ftlState.jitCode->common.addCodeOrigin(m_node->origin.semantic);
        
        m_out.storePtr(m_callFrame, packet, m_heaps.ShadowChicken_Packet_frame);
        m_out.storePtr(m_out.constIntPtr(bitwise_cast<intptr_t>(ShadowChicken::Packet::tailMarker())), packet, m_heaps.ShadowChicken_Packet_callee);
        m_out.store64(thisValue, packet, m_heaps.ShadowChicken_Packet_thisValue);
        m_out.storePtr(scope, packet, m_heaps.ShadowChicken_Packet_scope);
        // We don't want the CodeBlock to have a weak pointer to itself because
        // that would cause it to always get collected.
        m_out.storePtr(m_out.constIntPtr(bitwise_cast<intptr_t>(codeBlock())), packet, m_heaps.ShadowChicken_Packet_codeBlock);
        m_out.store32(m_out.constInt32(callSiteIndex.bits()), packet, m_heaps.ShadowChicken_Packet_callSiteIndex);
    }

    void compileRecordRegExpCachedResult()
    {
        Edge globalObjectEdge = m_graph.varArgChild(m_node, 0);
        Edge regExpEdge = m_graph.varArgChild(m_node, 1);
        Edge stringEdge = m_graph.varArgChild(m_node, 2);
        Edge startEdge = m_graph.varArgChild(m_node, 3);
        Edge endEdge = m_graph.varArgChild(m_node, 4);
        
        LValue globalObject = lowCell(globalObjectEdge);
        LValue regExp = lowCell(regExpEdge);
        LValue string = lowCell(stringEdge);
        LValue start = lowInt32(startEdge);
        LValue end = lowInt32(endEdge);

        m_out.storePtr(regExp, globalObject, m_heaps.JSGlobalObject_regExpGlobalData_cachedResult_lastRegExp);
        m_out.storePtr(string, globalObject, m_heaps.JSGlobalObject_regExpGlobalData_cachedResult_lastInput);
        m_out.store32(start, globalObject, m_heaps.JSGlobalObject_regExpGlobalData_cachedResult_result_start);
        m_out.store32(end, globalObject, m_heaps.JSGlobalObject_regExpGlobalData_cachedResult_result_end);
        m_out.store32As8(
            m_out.constInt32(0),
            m_out.address(globalObject, m_heaps.JSGlobalObject_regExpGlobalData_cachedResult_reified));
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
            length.known = inlineCallFrame->argumentCountIncludingThis - 1;
            length.isKnown = true;
            length.value = m_out.constInt32(length.known);
        } else {
            length.known = UINT_MAX;
            length.isKnown = false;
            
            VirtualRegister argumentCountRegister;
            if (!inlineCallFrame)
                argumentCountRegister = VirtualRegister(CallFrameSlot::argumentCount);
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
        return m_out.loadPtr(addressFor(CallFrameSlot::callee));
    }
    
    LValue getArgumentsStart(InlineCallFrame* inlineCallFrame, unsigned offset = 0)
    {
        VirtualRegister start = AssemblyHelpers::argumentsStart(inlineCallFrame) + offset;
        return addressFor(start).value();
    }
    
    LValue getArgumentsStart()
    {
        return getArgumentsStart(m_node->origin.semantic.inlineCallFrame);
    }
    
    template<typename Functor>
    void checkStructure(
        LValue structureDiscriminant, const FormattedValue& formattedValue, ExitKind exitKind,
        const RegisteredStructureSet& set, const Functor& weakStructureDiscriminant)
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
            
            FTL_TYPE_CHECK(jsValueValue(value), edge, ~SpecCellCheck, isCell(value));
            
            LValue specialResult = m_out.select(
                m_out.equal(value, m_out.constInt64(JSValue::encode(jsBoolean(true)))),
                m_out.int32One, m_out.int32Zero);
            results.append(m_out.anchor(specialResult));
            m_out.jump(continuation);
        }
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(Int32, results);
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
        
        nukeStructureAndSetButterfly(result, object);
        return result;
    }

    void initializeArrayElements(LValue indexingType, LValue begin, LValue end, LValue butterfly)
    {
        
        if (begin == end)
            return;
        
        if (indexingType->hasInt32()) {
            IndexingType rawIndexingType = static_cast<IndexingType>(indexingType->asInt32());
            if (hasUndecided(rawIndexingType))
                return;
            IndexedAbstractHeap* heap = m_heaps.forIndexingType(rawIndexingType);
            DFG_ASSERT(m_graph, m_node, heap);
            
            LValue hole;
            if (hasDouble(rawIndexingType))
                hole = m_out.constInt64(bitwise_cast<int64_t>(PNaN));
            else
                hole = m_out.constInt64(JSValue::encode(JSValue()));
            
            splatWords(butterfly, begin, end, hole, heap->atAnyIndex());
        } else {
            LValue hole = m_out.select(
                m_out.equal(m_out.bitAnd(indexingType, m_out.constInt32(IndexingShapeMask)), m_out.constInt32(DoubleShape)),
                m_out.constInt64(bitwise_cast<int64_t>(PNaN)),
                m_out.constInt64(JSValue::encode(JSValue())));
            splatWords(butterfly, begin, end, hole, m_heaps.root);
        }
    }

    void splatWords(LValue base, LValue begin, LValue end, LValue value, const AbstractHeap& heap)
    {
        const uint64_t unrollingLimit = 10;
        if (begin->hasInt() && end->hasInt()) {
            uint64_t beginConst = static_cast<uint64_t>(begin->asInt());
            uint64_t endConst = static_cast<uint64_t>(end->asInt());
            
            if (endConst - beginConst <= unrollingLimit) {
                for (uint64_t i = beginConst; i < endConst; ++i) {
                    LValue pointer = m_out.add(base, m_out.constIntPtr(i * sizeof(uint64_t)));
                    m_out.store64(value, TypedPointer(heap, pointer));
                }
                return;
            }
        }

        LBasicBlock initLoop = m_out.newBlock();
        LBasicBlock initDone = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(initLoop);
        
        ValueFromBlock originalIndex = m_out.anchor(end);
        ValueFromBlock originalPointer = m_out.anchor(
            m_out.add(base, m_out.shl(m_out.signExt32ToPtr(begin), m_out.constInt32(3))));
        m_out.branch(m_out.notEqual(end, begin), unsure(initLoop), unsure(initDone));
        
        m_out.appendTo(initLoop, initDone);
        LValue index = m_out.phi(Int32, originalIndex);
        LValue pointer = m_out.phi(pointerType(), originalPointer);
        
        m_out.store64(value, TypedPointer(heap, pointer));
        
        LValue nextIndex = m_out.sub(index, m_out.int32One);
        m_out.addIncomingToPhi(index, m_out.anchor(nextIndex));
        m_out.addIncomingToPhi(pointer, m_out.anchor(m_out.add(pointer, m_out.intPtrEight)));
        m_out.branch(
            m_out.notEqual(nextIndex, begin), unsure(initLoop), unsure(initDone));
        
        m_out.appendTo(initDone, lastNext);
    }
    
    LValue allocatePropertyStorage(LValue object, Structure* previousStructure)
    {
        if (previousStructure->couldHaveIndexingHeader()) {
            return vmCall(
                pointerType(),
                m_out.operation(operationAllocateComplexPropertyStorageWithInitialCapacity),
                m_callFrame, object);
        }
        
        LValue result = allocatePropertyStorageWithSizeImpl(initialOutOfLineCapacity);

        splatWords(
            result,
            m_out.constInt32(-initialOutOfLineCapacity - 1), m_out.constInt32(-1),
            m_out.int64Zero, m_heaps.properties.atAnyNumber());
        
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
            return vmCall(pointerType(), m_out.operation(operationAllocateComplexPropertyStorage), m_callFrame, object, newAllocSize);
        }
        
        LValue result = allocatePropertyStorageWithSizeImpl(newSize);

        ptrdiff_t headerSize = -sizeof(IndexingHeader) - sizeof(void*);
        ptrdiff_t endStorage = headerSize - static_cast<ptrdiff_t>(oldSize * sizeof(JSValue));

        for (ptrdiff_t offset = headerSize; offset > endStorage; offset -= sizeof(void*)) {
            LValue loaded = 
                m_out.loadPtr(m_out.address(m_heaps.properties.atAnyNumber(), oldStorage, offset));
            m_out.storePtr(loaded, m_out.address(m_heaps.properties.atAnyNumber(), result, offset));
        }
        
        splatWords(
            result,
            m_out.constInt32(-newSize - 1), m_out.constInt32(-oldSize - 1),
            m_out.int64Zero, m_heaps.properties.atAnyNumber());
        
        return result;
    }
    
    LValue allocatePropertyStorageWithSizeImpl(size_t sizeInValues)
    {
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);

        size_t sizeInBytes = sizeInValues * sizeof(JSValue);
        Allocator allocator = vm().jsValueGigacageAuxiliarySpace.allocatorForNonVirtual(sizeInBytes, AllocatorForMode::AllocatorIfExists);
        LValue startOfStorage = allocateHeapCell(
            m_out.constIntPtr(allocator.localAllocator()), slowPath);
        ValueFromBlock fastButterfly = m_out.anchor(
            m_out.add(m_out.constIntPtr(sizeInBytes + sizeof(IndexingHeader)), startOfStorage));
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        
        LValue slowButterflyValue;
        VM& vm = this->vm();
        if (sizeInValues == initialOutOfLineCapacity) {
            slowButterflyValue = lazySlowPath(
                [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(vm,
                        operationAllocateSimplePropertyStorageWithInitialCapacity,
                        locations[0].directGPR());
                });
        } else {
            slowButterflyValue = lazySlowPath(
                [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(vm,
                        operationAllocateSimplePropertyStorage, locations[0].directGPR(),
                        CCallHelpers::TrustedImmPtr(sizeInValues));
                });
        }
        ValueFromBlock slowButterfly = m_out.anchor(slowButterflyValue);
        
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        return m_out.phi(pointerType(), fastButterfly, slowButterfly);
    }
    
    LValue getById(LValue base, AccessType type)
    {
        Node* node = m_node;
        UniquedStringImpl* uid = m_graph.identifiers()[node->identifierNumber()];

        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(base);
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));

        // FIXME: If this is a GetByIdFlush/GetByIdDirectFlush, we might get some performance boost if we claim that it
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
                    params.unavailableRegisters(), uid, JSValueRegs(params[1].gpr()),
                    JSValueRegs(params[0].gpr()), type);

                generator->generateFastPath(jit);
                CCallHelpers::Label done = jit.label();

                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);

                        J_JITOperation_ESsiJI optimizationFunction = appropriateOptimizingGetByIdFunction(type);

                        generator->slowPathJump().link(&jit);
                        CCallHelpers::Label slowPathBegin = jit.label();
                        CCallHelpers::Call slowPathCall = callOperation(
                            *state, params.unavailableRegisters(), jit, node->origin.semantic,
                            exceptions.get(), optimizationFunction, params[0].gpr(),
                            CCallHelpers::TrustedImmPtr(generator->stubInfo()), params[1].gpr(),
                            CCallHelpers::TrustedImmPtr(uid)).call();
                        jit.jump().linkTo(done, &jit);

                        generator->reportSlowPathCall(slowPathBegin, slowPathCall);

                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                generator->finalize(linkBuffer, linkBuffer);
                            });
                    });
            });

        return patchpoint;
    }
    
    LValue getByIdWithThis(LValue base, LValue thisValue)
    {
        Node* node = m_node;
        UniquedStringImpl* uid = m_graph.identifiers()[node->identifierNumber()];

        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(base);
        patchpoint->appendSomeRegister(thisValue);
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));

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

                auto generator = Box<JITGetByIdWithThisGenerator>::create(
                    jit.codeBlock(), node->origin.semantic, callSiteIndex,
                    params.unavailableRegisters(), uid, JSValueRegs(params[0].gpr()),
                    JSValueRegs(params[1].gpr()), JSValueRegs(params[2].gpr()), AccessType::GetWithThis);

                generator->generateFastPath(jit);
                CCallHelpers::Label done = jit.label();

                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);

                        J_JITOperation_ESsiJJI optimizationFunction = operationGetByIdWithThisOptimize;

                        generator->slowPathJump().link(&jit);
                        CCallHelpers::Label slowPathBegin = jit.label();
                        CCallHelpers::Call slowPathCall = callOperation(
                            *state, params.unavailableRegisters(), jit, node->origin.semantic,
                            exceptions.get(), optimizationFunction, params[0].gpr(),
                            CCallHelpers::TrustedImmPtr(generator->stubInfo()), params[1].gpr(),
                            params[2].gpr(), CCallHelpers::TrustedImmPtr(uid)).call();
                        jit.jump().linkTo(done, &jit);

                        generator->reportSlowPathCall(slowPathBegin, slowPathCall);

                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                generator->finalize(linkBuffer, linkBuffer);
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
        C_JITOperation_TT stringIdentFunction,
        C_JITOperation_B_EJssJss stringFunction,
        S_JITOperation_EJJ fallbackFunction)
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

        if (m_node->isBinaryUseKind(StringIdentUse)) {
            LValue left = lowStringIdent(m_node->child1());
            LValue right = lowStringIdent(m_node->child2());
            setBoolean(m_out.callWithoutSideEffects(Int32, stringIdentFunction, left, right));
            return;
        }

        if (m_node->isBinaryUseKind(StringUse)) {
            LValue left = lowCell(m_node->child1());
            LValue right = lowCell(m_node->child2());
            speculateString(m_node->child1(), left);
            speculateString(m_node->child2(), right);

            LValue result = vmCall(
                Int32, m_out.operation(stringFunction),
                m_callFrame, left, right);
            setBoolean(result);
            return;
        }

        DFG_ASSERT(m_graph, m_node, m_node->isBinaryUseKind(UntypedUse), m_node->child1().useKind(), m_node->child2().useKind());
        nonSpeculativeCompare(intFunctor, fallbackFunction);
    }

    void compileStringSlice()
    {
        LBasicBlock emptyCase = m_out.newBlock();
        LBasicBlock notEmptyCase = m_out.newBlock();
        LBasicBlock oneCharCase = m_out.newBlock();
        LBasicBlock bitCheckCase = m_out.newBlock();
        LBasicBlock is8Bit = m_out.newBlock();
        LBasicBlock is16Bit = m_out.newBlock();
        LBasicBlock bitsContinuation = m_out.newBlock();
        LBasicBlock bigCharacter = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue string = lowString(m_node->child1());
        LValue length = m_out.load32NonNegative(string, m_heaps.JSString_length);
        LValue start = lowInt32(m_node->child2());
        LValue end = nullptr;
        if (m_node->child3())
            end = lowInt32(m_node->child3());

        auto range = populateSliceRange(start, end, length);
        LValue from = range.first;
        LValue to = range.second;

        LValue span = m_out.sub(to, from);
        m_out.branch(m_out.lessThanOrEqual(span, m_out.int32Zero), unsure(emptyCase), unsure(notEmptyCase));

        Vector<ValueFromBlock, 4> results;

        LBasicBlock lastNext = m_out.appendTo(emptyCase, notEmptyCase);
        results.append(m_out.anchor(weakPointer(jsEmptyString(&vm()))));
        m_out.jump(continuation);

        m_out.appendTo(notEmptyCase, oneCharCase);
        m_out.branch(m_out.equal(span, m_out.int32One), unsure(oneCharCase), unsure(slowCase));

        m_out.appendTo(oneCharCase, bitCheckCase);
        LValue stringImpl = m_out.loadPtr(string, m_heaps.JSString_value);
        m_out.branch(m_out.isNull(stringImpl), unsure(slowCase), unsure(bitCheckCase));

        m_out.appendTo(bitCheckCase, is8Bit);
        LValue storage = m_out.loadPtr(stringImpl, m_heaps.StringImpl_data);
        m_out.branch(
            m_out.testIsZero32(
                m_out.load32(stringImpl, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIs8Bit())),
            unsure(is16Bit), unsure(is8Bit));

        m_out.appendTo(is8Bit, is16Bit);
        // FIXME: Need to cage strings!
        // https://bugs.webkit.org/show_bug.cgi?id=174924
        ValueFromBlock char8Bit = m_out.anchor(m_out.load8ZeroExt32(m_out.baseIndex(m_heaps.characters8, storage, m_out.zeroExtPtr(from))));
        m_out.jump(bitsContinuation);

        m_out.appendTo(is16Bit, bigCharacter);
        LValue char16BitValue = m_out.load16ZeroExt32(m_out.baseIndex(m_heaps.characters16, storage, m_out.zeroExtPtr(from)));
        ValueFromBlock char16Bit = m_out.anchor(char16BitValue);
        m_out.branch(
            m_out.aboveOrEqual(char16BitValue, m_out.constInt32(0x100)),
            rarely(bigCharacter), usually(bitsContinuation));

        m_out.appendTo(bigCharacter, bitsContinuation);
        results.append(m_out.anchor(vmCall(
            Int64, m_out.operation(operationSingleCharacterString),
            m_callFrame, char16BitValue)));
        m_out.jump(continuation);

        m_out.appendTo(bitsContinuation, slowCase);
        LValue character = m_out.phi(Int32, char8Bit, char16Bit);
        LValue smallStrings = m_out.constIntPtr(vm().smallStrings.singleCharacterStrings());
        results.append(m_out.anchor(m_out.loadPtr(m_out.baseIndex(
            m_heaps.singleCharacterStrings, smallStrings, m_out.zeroExtPtr(character)))));
        m_out.jump(continuation);

        m_out.appendTo(slowCase, continuation);
        results.append(m_out.anchor(vmCall(pointerType(), m_out.operation(operationStringSubstr), m_callFrame, string, from, span)));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(pointerType(), results));
    }

    void compileToLowerCase()
    {
        LBasicBlock notRope = m_out.newBlock();
        LBasicBlock is8Bit = m_out.newBlock();
        LBasicBlock loopTop = m_out.newBlock();
        LBasicBlock loopBody = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue string = lowString(m_node->child1());
        ValueFromBlock startIndex = m_out.anchor(m_out.constInt32(0));
        ValueFromBlock startIndexForCall = m_out.anchor(m_out.constInt32(0));
        LValue impl = m_out.loadPtr(string, m_heaps.JSString_value);
        m_out.branch(m_out.isZero64(impl),
            unsure(slowPath), unsure(notRope));

        LBasicBlock lastNext = m_out.appendTo(notRope, is8Bit);

        m_out.branch(
            m_out.testIsZero32(
                m_out.load32(impl, m_heaps.StringImpl_hashAndFlags),
                m_out.constInt32(StringImpl::flagIs8Bit())),
            unsure(slowPath), unsure(is8Bit));

        m_out.appendTo(is8Bit, loopTop);
        LValue length = m_out.load32(impl, m_heaps.StringImpl_length);
        LValue buffer = m_out.loadPtr(impl, m_heaps.StringImpl_data);
        ValueFromBlock fastResult = m_out.anchor(string);
        m_out.jump(loopTop);

        m_out.appendTo(loopTop, loopBody);
        LValue index = m_out.phi(Int32, startIndex);
        ValueFromBlock indexFromBlock = m_out.anchor(index);
        m_out.branch(m_out.below(index, length),
            unsure(loopBody), unsure(continuation));

        m_out.appendTo(loopBody, slowPath);

        // FIXME: Strings needs to be caged.
        // https://bugs.webkit.org/show_bug.cgi?id=174924
        LValue byte = m_out.load8ZeroExt32(m_out.baseIndex(m_heaps.characters8, buffer, m_out.zeroExtPtr(index)));
        LValue isInvalidAsciiRange = m_out.bitAnd(byte, m_out.constInt32(~0x7F));
        LValue isUpperCase = m_out.belowOrEqual(m_out.sub(byte, m_out.constInt32('A')), m_out.constInt32('Z' - 'A'));
        LValue isBadCharacter = m_out.bitOr(isInvalidAsciiRange, isUpperCase);
        m_out.addIncomingToPhi(index, m_out.anchor(m_out.add(index, m_out.int32One)));
        m_out.branch(isBadCharacter, unsure(slowPath), unsure(loopTop));

        m_out.appendTo(slowPath, continuation);
        LValue slowPathIndex = m_out.phi(Int32, startIndexForCall, indexFromBlock);
        ValueFromBlock slowResult = m_out.anchor(vmCall(pointerType(), m_out.operation(operationToLowerCase), m_callFrame, string, slowPathIndex));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        setJSValue(m_out.phi(pointerType(), fastResult, slowResult));
    }

    void compileNumberToStringWithRadix()
    {
        bool validRadixIsGuaranteed = false;
        if (m_node->child2()->isInt32Constant()) {
            int32_t radix = m_node->child2()->asInt32();
            if (radix >= 2 && radix <= 36)
                validRadixIsGuaranteed = true;
        }

        switch (m_node->child1().useKind()) {
        case Int32Use:
            setJSValue(vmCall(pointerType(), m_out.operation(validRadixIsGuaranteed ? operationInt32ToStringWithValidRadix : operationInt32ToString), m_callFrame, lowInt32(m_node->child1()), lowInt32(m_node->child2())));
            break;
        case Int52RepUse:
            setJSValue(vmCall(pointerType(), m_out.operation(validRadixIsGuaranteed ? operationInt52ToStringWithValidRadix : operationInt52ToString), m_callFrame, lowStrictInt52(m_node->child1()), lowInt32(m_node->child2())));
            break;
        case DoubleRepUse:
            setJSValue(vmCall(pointerType(), m_out.operation(validRadixIsGuaranteed ? operationDoubleToStringWithValidRadix : operationDoubleToString), m_callFrame, lowDouble(m_node->child1()), lowInt32(m_node->child2())));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void compileNumberToStringWithValidRadixConstant()
    {
        switch (m_node->child1().useKind()) {
        case Int32Use:
            setJSValue(vmCall(pointerType(), m_out.operation(operationInt32ToStringWithValidRadix), m_callFrame, lowInt32(m_node->child1()), m_out.constInt32(m_node->validRadixConstant())));
            break;
        case Int52RepUse:
            setJSValue(vmCall(pointerType(), m_out.operation(operationInt52ToStringWithValidRadix), m_callFrame, lowStrictInt52(m_node->child1()), m_out.constInt32(m_node->validRadixConstant())));
            break;
        case DoubleRepUse:
            setJSValue(vmCall(pointerType(), m_out.operation(operationDoubleToStringWithValidRadix), m_callFrame, lowDouble(m_node->child1()), m_out.constInt32(m_node->validRadixConstant())));
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void compileResolveScopeForHoistingFuncDeclInEval()
    {
        UniquedStringImpl* uid = m_graph.identifiers()[m_node->identifierNumber()];
        setJSValue(vmCall(pointerType(), m_out.operation(operationResolveScopeForHoistingFuncDeclInEval), m_callFrame, lowCell(m_node->child1()), m_out.constIntPtr(uid)));
    }

    void compileResolveScope()
    {
        UniquedStringImpl* uid = m_graph.identifiers()[m_node->identifierNumber()];
        setJSValue(vmCall(pointerType(), m_out.operation(operationResolveScope),
            m_callFrame, lowCell(m_node->child1()), m_out.constIntPtr(uid)));
    }

    void compileGetDynamicVar()
    {
        UniquedStringImpl* uid = m_graph.identifiers()[m_node->identifierNumber()];
        setJSValue(vmCall(Int64, m_out.operation(operationGetDynamicVar),
            m_callFrame, lowCell(m_node->child1()), m_out.constIntPtr(uid), m_out.constInt32(m_node->getPutInfo())));
    }

    void compilePutDynamicVar()
    {
        UniquedStringImpl* uid = m_graph.identifiers()[m_node->identifierNumber()];
        setJSValue(vmCall(Void, m_out.operation(operationPutDynamicVar),
            m_callFrame, lowCell(m_node->child1()), lowJSValue(m_node->child2()), m_out.constIntPtr(uid), m_out.constInt32(m_node->getPutInfo())));
    }
    
    void compileUnreachable()
    {
        // It's so tempting to assert that AI has proved that this is unreachable. But that's
        // simply not a requirement of the Unreachable opcode at all. If you emit an opcode that
        // *you* know will not return, then it's fine to end the basic block with Unreachable
        // after that opcode. You don't have to also prove to AI that your opcode does not return.
        // Hence, there is nothing to do here but emit code that will crash, so that we catch
        // cases where you said Unreachable but you lied.
        //
        // It's also also worth noting that some clients emit this opcode because they're not 100% sure
        // if the code is unreachable, but they would really prefer if we crashed rather than kept going
        // if it did turn out to be reachable. Hence, this needs to deterministically crash.
        
        crash();
    }

    void compileCheckSubClass()
    {
        LValue cell = lowCell(m_node->child1());

        const ClassInfo* classInfo = m_node->classInfo();
        if (!classInfo->checkSubClassSnippet) {
            LBasicBlock loop = m_out.newBlock();
            LBasicBlock parentClass = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue structure = loadStructure(cell);
            LValue poisonedClassInfo = m_out.loadPtr(structure, m_heaps.Structure_classInfo);
            LValue classInfo = m_out.bitXor(poisonedClassInfo, m_out.constInt64(GlobalDataPoison::key()));
            ValueFromBlock otherAtStart = m_out.anchor(classInfo);
            m_out.jump(loop);

            LBasicBlock lastNext = m_out.appendTo(loop, parentClass);
            LValue other = m_out.phi(pointerType(), otherAtStart);
            m_out.branch(m_out.equal(other, m_out.constIntPtr(classInfo)), unsure(continuation), unsure(parentClass));

            m_out.appendTo(parentClass, continuation);
            LValue parent = m_out.loadPtr(other, m_heaps.ClassInfo_parentClass);
            speculate(BadType, jsValueValue(cell), m_node->child1().node(), m_out.isNull(parent));
            m_out.addIncomingToPhi(other, m_out.anchor(parent));
            m_out.jump(loop);

            m_out.appendTo(continuation, lastNext);
            return;
        }

        RefPtr<Snippet> domJIT = classInfo->checkSubClassSnippet();
        PatchpointValue* patchpoint = m_out.patchpoint(Void);
        patchpoint->appendSomeRegister(cell);
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));

        NodeOrigin origin = m_origin;
        unsigned osrExitArgumentOffset = patchpoint->numChildren();
        OSRExitDescriptor* exitDescriptor = appendOSRExitDescriptor(jsValueValue(cell), m_node->child1().node());
        patchpoint->appendColdAnys(buildExitArguments(exitDescriptor, origin.forExit, jsValueValue(cell)));

        patchpoint->numGPScratchRegisters = domJIT->numGPScratchRegisters;
        patchpoint->numFPScratchRegisters = domJIT->numFPScratchRegisters;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());

        State* state = &m_ftlState;
        Node* node = m_node;
        JSValue child1Constant = m_state.forNode(m_node->child1()).value();

        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);

                Vector<GPRReg> gpScratch;
                Vector<FPRReg> fpScratch;
                Vector<SnippetParams::Value> regs;

                regs.append(SnippetParams::Value(params[0].gpr(), child1Constant));

                for (unsigned i = 0; i < domJIT->numGPScratchRegisters; ++i)
                    gpScratch.append(params.gpScratch(i));

                for (unsigned i = 0; i < domJIT->numFPScratchRegisters; ++i)
                    fpScratch.append(params.fpScratch(i));

                RefPtr<OSRExitHandle> handle = exitDescriptor->emitOSRExitLater(*state, BadType, origin, params, osrExitArgumentOffset);

                SnippetParams domJITParams(*state, params, node, nullptr, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
                CCallHelpers::JumpList failureCases = domJIT->generator()->run(jit, domJITParams);

                jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                    linkBuffer.link(failureCases, linkBuffer.locationOf<NoPtrTag>(handle->label));
                });
            });
        patchpoint->effects = Effects::forCheck();
    }

    void compileCallDOM()
    {
        const DOMJIT::Signature* signature = m_node->signature();

        // FIXME: We should have a way to call functions with the vector of registers.
        // https://bugs.webkit.org/show_bug.cgi?id=163099
        Vector<LValue, JSC_DOMJIT_SIGNATURE_MAX_ARGUMENTS_INCLUDING_THIS> operands;

        unsigned index = 0;
        DFG_NODE_DO_TO_CHILDREN(m_graph, m_node, [&](Node*, Edge edge) {
            if (!index)
                operands.append(lowCell(edge));
            else {
                switch (signature->arguments[index - 1]) {
                case SpecString:
                    operands.append(lowString(edge));
                    break;
                case SpecInt32Only:
                    operands.append(lowInt32(edge));
                    break;
                case SpecBoolean:
                    operands.append(lowBoolean(edge));
                    break;
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                    break;
                }
            }
            ++index;
        });

        unsigned argumentCountIncludingThis = signature->argumentCount + 1;
        LValue result;
        assertIsTaggedWith(reinterpret_cast<void*>(signature->unsafeFunction), CFunctionPtrTag);
        switch (argumentCountIncludingThis) {
        case 1:
            result = vmCall(Int64, m_out.operation(reinterpret_cast<J_JITOperation_EP>(signature->unsafeFunction)), m_callFrame, operands[0]);
            break;
        case 2:
            result = vmCall(Int64, m_out.operation(reinterpret_cast<J_JITOperation_EPP>(signature->unsafeFunction)), m_callFrame, operands[0], operands[1]);
            break;
        case 3:
            result = vmCall(Int64, m_out.operation(reinterpret_cast<J_JITOperation_EPPP>(signature->unsafeFunction)), m_callFrame, operands[0], operands[1], operands[2]);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        setJSValue(result);
    }

    void compileCallDOMGetter()
    {
        DOMJIT::CallDOMGetterSnippet* domJIT = m_node->callDOMGetterData()->snippet;
        if (!domJIT) {
            // The following function is not an operation: we directly call a custom accessor getter.
            // Since the getter does not have code setting topCallFrame, As is the same to IC, we should set topCallFrame in caller side.
            m_out.storePtr(m_callFrame, m_out.absolute(&vm().topCallFrame));
            setJSValue(
                vmCall(Int64, m_out.operation(m_node->callDOMGetterData()->customAccessorGetter.retaggedExecutableAddress<CFunctionPtrTag>()),
                    m_callFrame, lowCell(m_node->child1()), m_out.constIntPtr(m_graph.identifiers()[m_node->callDOMGetterData()->identifierNumber])));
            return;
        }

        Edge& baseEdge = m_node->child1();
        LValue base = lowCell(baseEdge);
        JSValue baseConstant = m_state.forNode(baseEdge).value();

        LValue globalObject;
        JSValue globalObjectConstant;
        if (domJIT->requireGlobalObject) {
            Edge& globalObjectEdge = m_node->child2();
            globalObject = lowCell(globalObjectEdge);
            globalObjectConstant = m_state.forNode(globalObjectEdge).value();
        }

        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(base);
        if (domJIT->requireGlobalObject)
            patchpoint->appendSomeRegister(globalObject);
        patchpoint->append(m_tagMask, ValueRep::reg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::reg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle = preparePatchpointForExceptions(patchpoint);
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->numGPScratchRegisters = domJIT->numGPScratchRegisters;
        patchpoint->numFPScratchRegisters = domJIT->numFPScratchRegisters;
        patchpoint->resultConstraint = ValueRep::SomeEarlyRegister;

        State* state = &m_ftlState;
        Node* node = m_node;
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);

                Vector<GPRReg> gpScratch;
                Vector<FPRReg> fpScratch;
                Vector<SnippetParams::Value> regs;

                regs.append(JSValueRegs(params[0].gpr()));
                regs.append(SnippetParams::Value(params[1].gpr(), baseConstant));
                if (domJIT->requireGlobalObject)
                    regs.append(SnippetParams::Value(params[2].gpr(), globalObjectConstant));

                for (unsigned i = 0; i < domJIT->numGPScratchRegisters; ++i)
                    gpScratch.append(params.gpScratch(i));

                for (unsigned i = 0; i < domJIT->numFPScratchRegisters; ++i)
                    fpScratch.append(params.fpScratch(i));

                Box<CCallHelpers::JumpList> exceptions = exceptionHandle->scheduleExitCreation(params)->jumps(jit);

                SnippetParams domJITParams(*state, params, node, exceptions, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
                domJIT->generator()->run(jit, domJITParams);
            });
        patchpoint->effects = Effects::forCall();
        setJSValue(patchpoint);
    }
    
    void compileFilterICStatus()
    {
        m_interpreter.filterICStatus(m_node);
    }

    LValue byteSwap32(LValue value)
    {
        // FIXME: teach B3 byteswap
        // https://bugs.webkit.org/show_bug.cgi?id=188759

        RELEASE_ASSERT(value->type() == Int32);
        PatchpointValue* patchpoint = m_out.patchpoint(Int32);
        patchpoint->appendSomeRegister(value);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.move(params[1].gpr(), params[0].gpr());
            jit.byteSwap32(params[0].gpr());
        });
        patchpoint->effects = Effects::none();
        return patchpoint;
    }

    LValue byteSwap64(LValue value)
    {
        // FIXME: teach B3 byteswap
        // https://bugs.webkit.org/show_bug.cgi?id=188759

        RELEASE_ASSERT(value->type() == Int64);
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(value);
        patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.move(params[1].gpr(), params[0].gpr());
            jit.byteSwap64(params[0].gpr());
        });
        patchpoint->effects = Effects::none();
        return patchpoint;
    }

    template <typename F1, typename F2>
    LValue emitCodeBasedOnEndiannessBranch(LValue isLittleEndian, const F1& emitLittleEndianCode, const F2& emitBigEndianCode)
    {
        LType type;

        LBasicBlock bigEndianCase = m_out.newBlock();
        LBasicBlock littleEndianCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(m_out.testIsZero32(isLittleEndian, m_out.constInt32(1)),
            unsure(bigEndianCase), unsure(littleEndianCase));

        LBasicBlock lastNext = m_out.appendTo(bigEndianCase, littleEndianCase);
        LValue bigEndianValue = emitBigEndianCode();
        type = bigEndianValue ? bigEndianValue->type() : Void;
        ValueFromBlock bigEndianResult = bigEndianValue ? m_out.anchor(bigEndianValue) : ValueFromBlock();
        m_out.jump(continuation);

        m_out.appendTo(littleEndianCase, continuation);
        LValue littleEndianValue = emitLittleEndianCode();
        ValueFromBlock littleEndianResult = littleEndianValue ? m_out.anchor(littleEndianValue) : ValueFromBlock();
        RELEASE_ASSERT((!littleEndianValue && !bigEndianValue) || type == littleEndianValue->type());
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        RELEASE_ASSERT(!!bigEndianResult == !!littleEndianResult);
        if (bigEndianResult)
            return m_out.phi(type, bigEndianResult, littleEndianResult);
        return nullptr;
    }

    void compileDataViewGet()
    {
        LValue dataView = lowDataViewObject(m_node->child1());
        LValue index = lowInt32(m_node->child2());
        LValue isLittleEndian = nullptr;
        if (m_node->child3())
            isLittleEndian = lowBoolean(m_node->child3());

        DataViewData data = m_node->dataViewData();

        LValue length = m_out.zeroExtPtr(m_out.load32NonNegative(dataView, m_heaps.JSArrayBufferView_length));
        LValue indexToCheck = m_out.zeroExtPtr(index);
        if (data.byteSize > 1)
            indexToCheck = m_out.add(indexToCheck, m_out.constInt64(data.byteSize - 1));
        speculate(OutOfBounds, noValue(), nullptr, m_out.aboveOrEqual(indexToCheck, length));

        LValue vector = caged(Gigacage::Primitive, m_out.loadPtr(dataView, m_heaps.JSArrayBufferView_vector));

        TypedPointer pointer(m_heaps.typedArrayProperties, m_out.add(vector, m_out.zeroExtPtr(index)));

        if (m_node->op() == DataViewGetInt) {
            switch (data.byteSize) {
            case 1:
                if (data.isSigned)
                    setInt32(m_out.load8SignExt32(pointer));
                else
                    setInt32(m_out.load8ZeroExt32(pointer));
                break;
            case 2: {
                auto emitLittleEndianLoad = [&] {
                    if (data.isSigned)
                        return m_out.load16SignExt32(pointer);
                    return m_out.load16ZeroExt32(pointer);
                };

                auto emitBigEndianLoad = [&] {
                    LValue val = m_out.load16ZeroExt32(pointer);

                    PatchpointValue* patchpoint = m_out.patchpoint(Int32);
                    patchpoint->appendSomeRegister(val);
                    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                        jit.move(params[1].gpr(), params[0].gpr());
                        jit.byteSwap16(params[0].gpr());
                        if (data.isSigned)
                            jit.signExtend16To32(params[0].gpr(), params[0].gpr());
                    });
                    patchpoint->effects = Effects::none();

                    return patchpoint;
                };

                if (data.isLittleEndian == FalseTriState)
                    setInt32(emitBigEndianLoad());
                else if (data.isLittleEndian == TrueTriState)
                    setInt32(emitLittleEndianLoad());
                else
                    setInt32(emitCodeBasedOnEndiannessBranch(isLittleEndian, emitLittleEndianLoad, emitBigEndianLoad));

                break;
            }
            case 4: {
                LValue loadedValue = m_out.load32(pointer);

                if (data.isLittleEndian == FalseTriState)
                    loadedValue = byteSwap32(loadedValue);
                else if (data.isLittleEndian == MixedTriState) {
                    auto emitLittleEndianCode = [&] {
                        return loadedValue;
                    };
                    auto emitBigEndianCode = [&] {
                        return byteSwap32(loadedValue);
                    };

                    loadedValue = emitCodeBasedOnEndiannessBranch(isLittleEndian, emitLittleEndianCode, emitBigEndianCode);
                }

                if (data.isSigned)
                    setInt32(loadedValue);
                else
                    setStrictInt52(m_out.zeroExt(loadedValue, Int64));

                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            switch (data.byteSize) {
            case 4: {
                auto emitLittleEndianCode = [&] {
                    return m_out.floatToDouble(m_out.loadFloat(pointer));
                };

                auto emitBigEndianCode = [&] {
                    LValue loadedValue = m_out.load32(pointer);
                    PatchpointValue* patchpoint = m_out.patchpoint(Double);
                    patchpoint->appendSomeRegister(loadedValue);
                    patchpoint->numGPScratchRegisters = 1;
                    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                        jit.move(params[1].gpr(), params.gpScratch(0));
                        jit.byteSwap32(params.gpScratch(0));
                        jit.move32ToFloat(params.gpScratch(0), params[0].fpr());
                        jit.convertFloatToDouble(params[0].fpr(), params[0].fpr());
                    });
                    patchpoint->effects = Effects::none();
                    return patchpoint;
                };

                if (data.isLittleEndian == TrueTriState)
                    setDouble(emitLittleEndianCode());
                else if (data.isLittleEndian == FalseTriState)
                    setDouble(emitBigEndianCode());
                else
                    setDouble(emitCodeBasedOnEndiannessBranch(isLittleEndian, emitLittleEndianCode, emitBigEndianCode));

                break;
            }
            case 8: {
                auto emitLittleEndianCode = [&] {
                    return m_out.loadDouble(pointer);
                };

                auto emitBigEndianCode = [&] {
                    LValue loadedValue = m_out.load64(pointer);
                    loadedValue = byteSwap64(loadedValue);
                    return m_out.bitCast(loadedValue, Double);
                };

                if (data.isLittleEndian == TrueTriState)
                    setDouble(emitLittleEndianCode());
                else if (data.isLittleEndian == FalseTriState)
                    setDouble(emitBigEndianCode());
                else
                    setDouble(emitCodeBasedOnEndiannessBranch(isLittleEndian, emitLittleEndianCode, emitBigEndianCode));

                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
    }

    void compileDataViewSet()
    {
        LValue dataView = lowDataViewObject(m_graph.varArgChild(m_node, 0));
        LValue index = lowInt32(m_graph.varArgChild(m_node, 1));
        LValue isLittleEndian = nullptr;
        if (m_graph.varArgChild(m_node, 3))
            isLittleEndian = lowBoolean(m_graph.varArgChild(m_node, 3));

        DataViewData data = m_node->dataViewData();

        LValue length = m_out.zeroExtPtr(m_out.load32NonNegative(dataView, m_heaps.JSArrayBufferView_length));
        LValue indexToCheck = m_out.zeroExtPtr(index);
        if (data.byteSize > 1)
            indexToCheck = m_out.add(indexToCheck, m_out.constInt64(data.byteSize - 1));
        speculate(OutOfBounds, noValue(), nullptr, m_out.aboveOrEqual(indexToCheck, length));

        Edge& valueEdge = m_graph.varArgChild(m_node, 2);
        LValue valueToStore;
        switch (valueEdge.useKind()) {
        case Int32Use:
            valueToStore = lowInt32(valueEdge);
            break;
        case DoubleRepUse:
            valueToStore = lowDouble(valueEdge);
            break;
        case Int52RepUse:
            valueToStore = lowStrictInt52(valueEdge);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        LValue vector = caged(Gigacage::Primitive, m_out.loadPtr(dataView, m_heaps.JSArrayBufferView_vector));
        TypedPointer pointer(m_heaps.typedArrayProperties, m_out.add(vector, m_out.zeroExtPtr(index)));

        if (data.isFloatingPoint) {
            if (data.byteSize == 4) {
                valueToStore = m_out.doubleToFloat(valueToStore);

                auto emitLittleEndianCode = [&] () -> LValue {
                    m_out.storeFloat(valueToStore, pointer);
                    return nullptr;
                };

                auto emitBigEndianCode = [&] () -> LValue {
                    PatchpointValue* patchpoint = m_out.patchpoint(Int32);
                    patchpoint->appendSomeRegister(valueToStore);
                    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                        jit.moveFloatTo32(params[1].fpr(), params[0].gpr());
                        jit.byteSwap32(params[0].gpr());
                    });
                    patchpoint->effects = Effects::none();
                    m_out.store32(patchpoint, pointer);
                    return nullptr;
                };

                if (data.isLittleEndian == FalseTriState)
                    emitBigEndianCode();
                else if (data.isLittleEndian == TrueTriState)
                    emitLittleEndianCode();
                else
                    emitCodeBasedOnEndiannessBranch(isLittleEndian, emitLittleEndianCode, emitBigEndianCode);

            } else {
                RELEASE_ASSERT(data.byteSize == 8);
                auto emitLittleEndianCode = [&] () -> LValue {
                    m_out.storeDouble(valueToStore, pointer);
                    return nullptr;
                };
                auto emitBigEndianCode = [&] () -> LValue {
                    m_out.store64(byteSwap64(m_out.bitCast(valueToStore, Int64)), pointer);
                    return nullptr;
                };

                if (data.isLittleEndian == FalseTriState)
                    emitBigEndianCode();
                else if (data.isLittleEndian == TrueTriState)
                    emitLittleEndianCode();
                else
                    emitCodeBasedOnEndiannessBranch(isLittleEndian, emitLittleEndianCode, emitBigEndianCode);
            }
        } else {
            switch (data.byteSize) {
            case 1:
                RELEASE_ASSERT(valueEdge.useKind() == Int32Use);
                m_out.store32As8(valueToStore, pointer);
                break;
            case 2: {
                RELEASE_ASSERT(valueEdge.useKind() == Int32Use);

                auto emitLittleEndianCode = [&] () -> LValue {
                    m_out.store32As16(valueToStore, pointer);
                    return nullptr;
                };
                auto emitBigEndianCode = [&] () -> LValue {
                    PatchpointValue* patchpoint = m_out.patchpoint(Int32);
                    patchpoint->appendSomeRegister(valueToStore);
                    patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                        jit.move(params[1].gpr(), params[0].gpr());
                        jit.byteSwap16(params[0].gpr());
                    });
                    patchpoint->effects = Effects::none();

                    m_out.store32As16(patchpoint, pointer);
                    return nullptr;
                };

                if (data.isLittleEndian == FalseTriState)
                    emitBigEndianCode();
                else if (data.isLittleEndian == TrueTriState)
                    emitLittleEndianCode();
                else
                    emitCodeBasedOnEndiannessBranch(isLittleEndian, emitLittleEndianCode, emitBigEndianCode);
                break;
            }
            case 4: {
                RELEASE_ASSERT(valueEdge.useKind() == Int32Use || valueEdge.useKind() == Int52RepUse);

                if (valueEdge.useKind() == Int52RepUse)
                    valueToStore = m_out.castToInt32(valueToStore);

                auto emitLittleEndianCode = [&] () -> LValue {
                    m_out.store32(valueToStore, pointer);
                    return nullptr;
                };
                auto emitBigEndianCode = [&] () -> LValue {
                    m_out.store32(byteSwap32(valueToStore), pointer);
                    return nullptr;
                };

                if (data.isLittleEndian == FalseTriState)
                    emitBigEndianCode();
                else if (data.isLittleEndian == TrueTriState)
                    emitLittleEndianCode();
                else
                    emitCodeBasedOnEndiannessBranch(isLittleEndian, emitLittleEndianCode, emitBigEndianCode);

                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
    }
    
    void emitSwitchForMultiByOffset(LValue base, bool structuresChecked, Vector<SwitchCase, 2>& cases, LBasicBlock exit)
    {
        if (cases.isEmpty()) {
            m_out.jump(exit);
            return;
        }
        
        if (structuresChecked) {
            std::sort(
                cases.begin(), cases.end(),
                [&] (const SwitchCase& a, const SwitchCase& b) -> bool {
                    return a.value()->asInt() < b.value()->asInt();
                });
            SwitchCase last = cases.takeLast();
            m_out.switchInstruction(
                m_out.load32(base, m_heaps.JSCell_structureID), cases, last.target(), Weight(0));
            return;
        }
        
        m_out.switchInstruction(
            m_out.load32(base, m_heaps.JSCell_structureID), cases, exit, Weight(0));
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
        speculateTruthyObject(leftChild, leftValue, SpecObject | (~SpecCellCheck));
        ValueFromBlock cellResult = m_out.anchor(m_out.equal(rightCell, leftValue));
        m_out.jump(continuation);
        
        m_out.appendTo(leftNotCellCase, continuation);
        FTL_TYPE_CHECK(
            jsValueValue(leftValue), leftChild, SpecOther | SpecCellCheck, isNotOther(leftValue));
        ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, cellResult, notCellResult));
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
        
        m_out.branch(isNotInt32(left, provenType(m_node->child1())), rarely(slowPath), usually(leftIsInt));
        
        LBasicBlock lastNext = m_out.appendTo(leftIsInt, fastPath);
        m_out.branch(isNotInt32(right, provenType(m_node->child2())), rarely(slowPath), usually(fastPath));
        
        m_out.appendTo(fastPath, slowPath);
        ValueFromBlock fastResult = m_out.anchor(intFunctor(unboxInt32(left), unboxInt32(right)));
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        ValueFromBlock slowResult = m_out.anchor(m_out.notNull(vmCall(
            pointerType(), m_out.operation(helperFunction), m_callFrame, left, right)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        setBoolean(m_out.phi(Int32, fastResult, slowResult));
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

        LValue indexAtLoopTop = m_out.phi(Int32, indexAtStart);
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
            Int64, m_out.operation(operationCompareStringEq), m_callFrame,
            leftJSString, rightJSString);
        ValueFromBlock slowResult = m_out.anchor(unboxBoolean(slowResultValue));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
        return m_out.phi(Int32, trueResult, falseResult, slowResult);
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
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        patchpoint->numGPScratchRegisters = 1;
        patchpoint->numFPScratchRegisters = 2;
        if (scratchFPRUsage == NeedScratchFPR)
            patchpoint->numFPScratchRegisters++;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->resultConstraint = ValueRep::SomeEarlyRegister;
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
        
        LValue left = lowJSValue(node->child1());
        LValue right = lowJSValue(node->child2());

        SnippetOperand leftOperand(m_state.forNode(node->child1()).resultType());
        SnippetOperand rightOperand(m_state.forNode(node->child2()).resultType());
            
        PatchpointValue* patchpoint = m_out.patchpoint(Int64);
        patchpoint->appendSomeRegister(left);
        patchpoint->appendSomeRegister(right);
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        patchpoint->numGPScratchRegisters = 1;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->resultConstraint = ValueRep::SomeEarlyRegister;
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
        patchpoint->append(m_tagMask, ValueRep::lateReg(GPRInfo::tagMaskRegister));
        patchpoint->append(m_tagTypeNumber, ValueRep::lateReg(GPRInfo::tagTypeNumberRegister));
        RefPtr<PatchpointExceptionHandle> exceptionHandle =
            preparePatchpointForExceptions(patchpoint);
        patchpoint->numGPScratchRegisters = 1;
        patchpoint->numFPScratchRegisters = 1;
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        patchpoint->resultConstraint = ValueRep::SomeEarlyRegister;
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

    LValue allocateHeapCell(LValue allocator, LBasicBlock slowPath)
    {
        JITAllocator actualAllocator;
        if (allocator->hasIntPtr())
            actualAllocator = JITAllocator::constant(Allocator(bitwise_cast<LocalAllocator*>(allocator->asIntPtr())));
        else
            actualAllocator = JITAllocator::variable();
        
        if (actualAllocator.isConstant()) {
            if (!actualAllocator.allocator()) {
                LBasicBlock haveAllocator = m_out.newBlock();
                LBasicBlock lastNext = m_out.insertNewBlocksBefore(haveAllocator);
                m_out.jump(slowPath);
                m_out.appendTo(haveAllocator, lastNext);
                return m_out.intPtrZero;
            }
        } else {
            // This means that either we know that the allocator is null or we don't know what the
            // allocator is. In either case, we need the null check.
            LBasicBlock haveAllocator = m_out.newBlock();
            LBasicBlock lastNext = m_out.insertNewBlocksBefore(haveAllocator);
            m_out.branch(
                m_out.notEqual(allocator, m_out.intPtrZero),
                usually(haveAllocator), rarely(slowPath));
            m_out.appendTo(haveAllocator, lastNext);
        }
        
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(continuation);
        
        PatchpointValue* patchpoint = m_out.patchpoint(pointerType());
        if (isARM64()) {
            // emitAllocateWithNonNullAllocator uses the scratch registers on ARM.
            patchpoint->clobber(RegisterSet::macroScratchRegisters());
        }
        patchpoint->effects.terminal = true;
        if (actualAllocator.isConstant())
            patchpoint->numGPScratchRegisters++;
        else
            patchpoint->appendSomeRegisterWithClobber(allocator);
        patchpoint->numGPScratchRegisters++;
        patchpoint->resultConstraint = ValueRep::SomeEarlyRegister;
        
        m_out.appendSuccessor(usually(continuation));
        m_out.appendSuccessor(rarely(slowPath));
        
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsageIf allowScratchIf(jit, isARM64());
                CCallHelpers::JumpList jumpToSlowPath;
                
                GPRReg allocatorGPR;
                if (actualAllocator.isConstant())
                    allocatorGPR = params.gpScratch(1);
                else
                    allocatorGPR = params[1].gpr();
                
                // We use a patchpoint to emit the allocation path because whenever we mess with
                // allocation paths, we already reason about them at the machine code level. We know
                // exactly what instruction sequence we want. We're confident that no compiler
                // optimization could make this code better. So, it's best to have the code in
                // AssemblyHelpers::emitAllocate(). That way, the same optimized path is shared by
                // all of the compiler tiers.
                jit.emitAllocateWithNonNullAllocator(
                    params[0].gpr(), actualAllocator, allocatorGPR, params.gpScratch(0),
                    jumpToSlowPath);
                
                CCallHelpers::Jump jumpToSuccess;
                if (!params.fallsThroughToSuccessor(0))
                    jumpToSuccess = jit.jump();
                
                Vector<Box<CCallHelpers::Label>> labels = params.successorLabels();
                
                params.addLatePath(
                    [=] (CCallHelpers& jit) {
                        jumpToSlowPath.linkTo(*labels[1], &jit);
                        if (jumpToSuccess.isSet())
                            jumpToSuccess.linkTo(*labels[0], &jit);
                    });
            });
        
        m_out.appendTo(continuation, lastNext);
        return patchpoint;
    }
    
    void storeStructure(LValue object, Structure* structure)
    {
        m_out.store32(m_out.constInt32(structure->id()), object, m_heaps.JSCell_structureID);
        m_out.store32(
            m_out.constInt32(structure->objectInitializationBlob()),
            object, m_heaps.JSCell_usefulBytes);
    }

    void storeStructure(LValue object, LValue structure)
    {
        if (structure->hasIntPtr()) {
            storeStructure(object, bitwise_cast<Structure*>(structure->asIntPtr()));
            return;
        }

        LValue id = m_out.load32(structure, m_heaps.Structure_structureID);
        m_out.store32(id, object, m_heaps.JSCell_structureID);

        LValue blob = m_out.load32(structure, m_heaps.Structure_indexingModeIncludingHistory);
        m_out.store32(blob, object, m_heaps.JSCell_usefulBytes);
    }

    template <typename StructureType>
    LValue allocateCell(LValue allocator, StructureType structure, LBasicBlock slowPath)
    {
        LValue result = allocateHeapCell(allocator, slowPath);
        storeStructure(result, structure);
        return result;
    }

    LValue allocateObject(LValue allocator, RegisteredStructure structure, LValue butterfly, LBasicBlock slowPath)
    {
        return allocateObject(allocator, weakStructure(structure), butterfly, slowPath);
    }

    LValue allocateObject(LValue allocator, LValue structure, LValue butterfly, LBasicBlock slowPath)
    {
        LValue result = allocateCell(allocator, structure, slowPath);
        if (structure->hasIntPtr()) {
            splatWords(
                result,
                m_out.constInt32(JSFinalObject::offsetOfInlineStorage() / 8),
                m_out.constInt32(JSFinalObject::offsetOfInlineStorage() / 8 + bitwise_cast<Structure*>(structure->asIntPtr())->inlineCapacity()),
                m_out.int64Zero,
                m_heaps.properties.atAnyNumber());
        } else {
            LValue end = m_out.add(
                m_out.constInt32(JSFinalObject::offsetOfInlineStorage() / 8),
                m_out.load8ZeroExt32(structure, m_heaps.Structure_inlineCapacity));
            splatWords(
                result,
                m_out.constInt32(JSFinalObject::offsetOfInlineStorage() / 8),
                end,
                m_out.int64Zero,
                m_heaps.properties.atAnyNumber());
        }
        
        m_out.storePtr(butterfly, result, m_heaps.JSObject_butterfly);
        return result;
    }
    
    template<typename ClassType, typename StructureType>
    LValue allocateObject(
        size_t size, StructureType structure, LValue butterfly, LBasicBlock slowPath)
    {
        Allocator allocator = allocatorForNonVirtualConcurrently<ClassType>(vm(), size, AllocatorForMode::AllocatorIfExists);
        return allocateObject(
            m_out.constIntPtr(allocator.localAllocator()), structure, butterfly, slowPath);
    }
    
    template<typename ClassType, typename StructureType>
    LValue allocateObject(StructureType structure, LValue butterfly, LBasicBlock slowPath)
    {
        return allocateObject<ClassType>(
            ClassType::allocationSize(0), structure, butterfly, slowPath);
    }
    
    LValue allocatorForSize(LValue subspace, LValue size, LBasicBlock slowPath)
    {
        static_assert(!(MarkedSpace::sizeStep & (MarkedSpace::sizeStep - 1)), "MarkedSpace::sizeStep must be a power of two.");
        
        // Try to do some constant-folding here.
        if (subspace->hasIntPtr() && size->hasIntPtr()) {
            CompleteSubspace* actualSubspace = bitwise_cast<CompleteSubspace*>(subspace->asIntPtr());
            size_t actualSize = size->asIntPtr();

            Allocator actualAllocator = actualSubspace->allocatorForNonVirtual(actualSize, AllocatorForMode::AllocatorIfExists);
            if (!actualAllocator) {
                LBasicBlock continuation = m_out.newBlock();
                LBasicBlock lastNext = m_out.insertNewBlocksBefore(continuation);
                m_out.jump(slowPath);
                m_out.appendTo(continuation, lastNext);
                return m_out.intPtrZero;
            }

            return m_out.constIntPtr(actualAllocator.localAllocator());
        }
        
        unsigned stepShift = getLSBSet(MarkedSpace::sizeStep);
        
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(continuation);
        
        LValue sizeClassIndex = m_out.lShr(
            m_out.add(size, m_out.constIntPtr(MarkedSpace::sizeStep - 1)),
            m_out.constInt32(stepShift));
        
        m_out.branch(
            m_out.above(sizeClassIndex, m_out.constIntPtr(MarkedSpace::largeCutoff >> stepShift)),
            rarely(slowPath), usually(continuation));
        
        m_out.appendTo(continuation, lastNext);
        
        return m_out.loadPtr(
            m_out.baseIndex(
                m_heaps.CompleteSubspace_allocatorForSizeStep,
                subspace, sizeClassIndex));
    }
    
    LValue allocatorForSize(CompleteSubspace& subspace, LValue size, LBasicBlock slowPath)
    {
        return allocatorForSize(m_out.constIntPtr(&subspace), size, slowPath);
    }
    
    template<typename ClassType>
    LValue allocateVariableSizedObject(
        LValue size, RegisteredStructure structure, LValue butterfly, LBasicBlock slowPath)
    {
        CompleteSubspace* subspace = subspaceForConcurrently<ClassType>(vm());
        RELEASE_ASSERT_WITH_MESSAGE(subspace, "CompleteSubspace is always allocated");
        LValue allocator = allocatorForSize(*subspace, size, slowPath);
        return allocateObject(allocator, structure, butterfly, slowPath);
    }

    template<typename ClassType>
    LValue allocateVariableSizedCell(
        LValue size, Structure* structure, LBasicBlock slowPath)
    {
        CompleteSubspace* subspace = subspaceForConcurrently<ClassType>(vm());
        RELEASE_ASSERT_WITH_MESSAGE(subspace, "CompleteSubspace is always allocated");
        LValue allocator = allocatorForSize(*subspace, size, slowPath);
        return allocateCell(allocator, structure, slowPath);
    }
    
    LValue allocateObject(RegisteredStructure structure)
    {
        size_t allocationSize = JSFinalObject::allocationSize(structure.get()->inlineCapacity());
        Allocator allocator = allocatorForNonVirtualConcurrently<JSFinalObject>(vm(), allocationSize, AllocatorForMode::AllocatorIfExists);
        
        // FIXME: If the allocator is null, we could simply emit a normal C call to the allocator
        // instead of putting it on the slow path.
        // https://bugs.webkit.org/show_bug.cgi?id=161062
        
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        ValueFromBlock fastResult = m_out.anchor(allocateObject(
            m_out.constIntPtr(allocator.localAllocator()), structure, m_out.intPtrZero, slowPath));
        
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);

        VM& vm = this->vm();
        LValue slowResultValue = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
                    operationNewObject, locations[0].directGPR(),
                    CCallHelpers::TrustedImmPtr(structure.get()));
            });
        ValueFromBlock slowResult = m_out.anchor(slowResultValue);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(pointerType(), fastResult, slowResult);
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

    ArrayValues allocateJSArray(LValue publicLength, LValue vectorLength, LValue structure, LValue indexingType, bool shouldInitializeElements = true, bool shouldLargeArraySizeCreateArrayStorage = true)
    {
        JSGlobalObject* globalObject = m_graph.globalObjectFor(m_node->origin.semantic);
        if (indexingType->hasInt32()) {
            IndexingType type = static_cast<IndexingType>(indexingType->asInt32());
            ASSERT_UNUSED(type,
                hasUndecided(type)
                || hasInt32(type)
                || hasDouble(type)
                || hasContiguous(type));
        }

        LBasicBlock fastCase = m_out.newBlock();
        LBasicBlock largeCase = m_out.newBlock();
        LBasicBlock failCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        LBasicBlock slowCase = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(fastCase);

        Optional<unsigned> staticVectorLength;
        Optional<unsigned> staticVectorLengthFromPublicLength;
        if (structure->hasIntPtr()) {
            if (publicLength->hasInt32()) {
                unsigned publicLengthConst = static_cast<unsigned>(publicLength->asInt32());
                if (publicLengthConst <= MAX_STORAGE_VECTOR_LENGTH) {
                    publicLengthConst = Butterfly::optimalContiguousVectorLength(
                        bitwise_cast<Structure*>(structure->asIntPtr())->outOfLineCapacity(), publicLengthConst);
                    staticVectorLengthFromPublicLength = publicLengthConst;
                }

            }
            if (vectorLength->hasInt32()) {
                unsigned vectorLengthConst = static_cast<unsigned>(vectorLength->asInt32());
                if (vectorLengthConst <= MAX_STORAGE_VECTOR_LENGTH) {
                    vectorLengthConst = Butterfly::optimalContiguousVectorLength(
                        bitwise_cast<Structure*>(structure->asIntPtr())->outOfLineCapacity(), vectorLengthConst);
                    vectorLength = m_out.constInt32(vectorLengthConst);
                    staticVectorLength = vectorLengthConst;
                }
            }
        } else {
            // We don't compute the optimal vector length for new Array(blah) where blah is not
            // statically known, since the compute effort of doing it here is probably not worth it.
        }
        
        ValueFromBlock noButterfly = m_out.anchor(m_out.intPtrZero);
        
        LValue predicate;
        if (shouldLargeArraySizeCreateArrayStorage)
            predicate = m_out.aboveOrEqual(publicLength, m_out.constInt32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH));
        else
            predicate = m_out.booleanFalse;
        
        m_out.branch(predicate, rarely(largeCase), usually(fastCase));
        
        m_out.appendTo(fastCase, largeCase);
            
        LValue payloadSize =
            m_out.shl(m_out.zeroExt(vectorLength, pointerType()), m_out.constIntPtr(3));
            
        LValue butterflySize = m_out.add(
            payloadSize, m_out.constIntPtr(sizeof(IndexingHeader)));
            
        LValue allocator = allocatorForSize(vm().jsValueGigacageAuxiliarySpace, butterflySize, failCase);
        LValue startOfStorage = allocateHeapCell(allocator, failCase);
            
        LValue butterfly = m_out.add(startOfStorage, m_out.constIntPtr(sizeof(IndexingHeader)));
        
        m_out.store32(publicLength, butterfly, m_heaps.Butterfly_publicLength);
        m_out.store32(vectorLength, butterfly, m_heaps.Butterfly_vectorLength);
    
        initializeArrayElements(
            indexingType,
            shouldInitializeElements ? m_out.int32Zero : publicLength, vectorLength,
            butterfly);
        
        ValueFromBlock haveButterfly = m_out.anchor(butterfly);

        LValue object = allocateObject<JSArray>(structure, butterfly, failCase);

        ValueFromBlock fastResult = m_out.anchor(object);
        ValueFromBlock fastButterfly = m_out.anchor(butterfly);
        m_out.jump(continuation);
        
        m_out.appendTo(largeCase, failCase);
        ValueFromBlock largeStructure = m_out.anchor(
            weakStructure(m_graph.registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage))));
        m_out.jump(slowCase);
        
        m_out.appendTo(failCase, slowCase);
        ValueFromBlock failStructure = m_out.anchor(structure);
        m_out.jump(slowCase);
        
        m_out.appendTo(slowCase, continuation);
        LValue structureValue = m_out.phi(pointerType(), largeStructure, failStructure);
        LValue butterflyValue = m_out.phi(pointerType(), noButterfly, haveButterfly);

        VM& vm = this->vm();
        LValue slowResultValue = nullptr;
        if (vectorLength == publicLength
            || (staticVectorLengthFromPublicLength && staticVectorLength && staticVectorLength.value() == staticVectorLengthFromPublicLength.value())) {
            slowResultValue = lazySlowPath(
                [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(vm,
                        operationNewArrayWithSize, locations[0].directGPR(),
                        locations[1].directGPR(), locations[2].directGPR(), locations[3].directGPR());
                },
                structureValue, publicLength, butterflyValue);
        } else {
            slowResultValue = lazySlowPath(
                [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                    return createLazyCallGenerator(vm,
                        operationNewArrayWithSizeAndHint, locations[0].directGPR(),
                        locations[1].directGPR(), locations[2].directGPR(), locations[3].directGPR(), locations[4].directGPR());
                },
                structureValue, publicLength, vectorLength, butterflyValue);
        }

        ValueFromBlock slowResult = m_out.anchor(slowResultValue);
        ValueFromBlock slowButterfly = m_out.anchor(
            m_out.loadPtr(slowResultValue, m_heaps.JSObject_butterfly));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return ArrayValues(
            m_out.phi(pointerType(), fastResult, slowResult),
            m_out.phi(pointerType(), fastButterfly, slowButterfly));
    }
    
    ArrayValues allocateUninitializedContiguousJSArrayInternal(LValue publicLength, LValue vectorLength, RegisteredStructure structure)
    {
        bool shouldInitializeElements = false;
        bool shouldLargeArraySizeCreateArrayStorage = false;
        return allocateJSArray(
            publicLength, vectorLength, weakStructure(structure), m_out.constInt32(structure->indexingType()), shouldInitializeElements,
            shouldLargeArraySizeCreateArrayStorage);
    }

    ArrayValues allocateUninitializedContiguousJSArray(LValue publicLength, RegisteredStructure structure)
    {
        return allocateUninitializedContiguousJSArrayInternal(publicLength, publicLength, structure);
    }

    ArrayValues allocateUninitializedContiguousJSArray(unsigned publicLength, unsigned vectorLength, RegisteredStructure structure)
    {
        ASSERT(vectorLength >= publicLength);
        return allocateUninitializedContiguousJSArrayInternal(m_out.constInt32(publicLength), m_out.constInt32(vectorLength), structure);
    }
    
    LValue ensureShadowChickenPacket()
    {
        ShadowChicken* shadowChicken = vm().shadowChicken();
        RELEASE_ASSERT(shadowChicken);
        LBasicBlock slowCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        TypedPointer addressOfLogCursor = m_out.absolute(shadowChicken->addressOfLogCursor());
        LValue logCursor = m_out.loadPtr(addressOfLogCursor);
        
        ValueFromBlock fastResult = m_out.anchor(logCursor);
        
        m_out.branch(
            m_out.below(logCursor, m_out.constIntPtr(shadowChicken->logEnd())),
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
            
            FTL_TYPE_CHECK(jsValueValue(value), edge, (~SpecCellCheck) | SpecString, isNotString(value));
            LValue length = m_out.load32NonNegative(value, m_heaps.JSString_length);
            ValueFromBlock cellResult = m_out.anchor(m_out.notEqual(length, m_out.int32Zero));
            m_out.jump(continuation);
            
            m_out.appendTo(notCellCase, continuation);
            
            FTL_TYPE_CHECK(jsValueValue(value), edge, SpecCellCheck | SpecOther, isNotOther(value));
            ValueFromBlock notCellResult = m_out.anchor(m_out.booleanFalse);
            m_out.jump(continuation);
            m_out.appendTo(continuation, lastNext);

            return m_out.phi(Int32, cellResult, notCellResult);
        }
        case UntypedUse: {
            LValue value = lowJSValue(edge);
            
            // Implements the following control flow structure:
            // if (value is cell) {
            //     if (value is string or value is BigInt)
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
            LBasicBlock notStringCase = m_out.newBlock();
            LBasicBlock stringOrBigIntCase = m_out.newBlock();
            LBasicBlock notStringOrBigIntCase = m_out.newBlock();
            LBasicBlock notCellCase = m_out.newBlock();
            LBasicBlock int32Case = m_out.newBlock();
            LBasicBlock notInt32Case = m_out.newBlock();
            LBasicBlock doubleCase = m_out.newBlock();
            LBasicBlock notDoubleCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();
            
            Vector<ValueFromBlock> results;
            
            m_out.branch(isCell(value, provenType(edge)), unsure(cellCase), unsure(notCellCase));
            
            LBasicBlock lastNext = m_out.appendTo(cellCase, notStringCase);
            m_out.branch(
                isString(value, provenType(edge) & SpecCell),
                unsure(stringOrBigIntCase), unsure(notStringCase));
            
            m_out.appendTo(notStringCase, stringOrBigIntCase);
            m_out.branch(
                isBigInt(value, provenType(edge) & (SpecCell - SpecString)),
                unsure(stringOrBigIntCase), unsure(notStringOrBigIntCase));

            m_out.appendTo(stringOrBigIntCase, notStringOrBigIntCase);
            LValue nonZeroCell = m_out.notZero32(
                m_out.load32NonNegative(value, m_heaps.JSBigIntOrString_length));
            results.append(m_out.anchor(nonZeroCell));
            m_out.jump(continuation);
            
            m_out.appendTo(notStringOrBigIntCase, notCellCase);
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
                    weakPointer(m_graph.globalObjectFor(m_node->origin.semantic)),
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
            return m_out.phi(Int32, results);
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
                jsValueValue(value), edge, (~SpecCellCheck) | SpecObject, isNotObject(value));
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
                    weakPointer(m_graph.globalObjectFor(m_node->origin.semantic)),
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
                jsValueValue(value), edge, SpecCellCheck | SpecOther, isNotOther(value));
            primitiveResult = m_out.booleanTrue;
            break;
        }
        results.append(m_out.anchor(primitiveResult));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        
        return m_out.phi(Int32, results);
    }
    
    template<typename FunctionType>
    void contiguousPutByValOutOfBounds(
        FunctionType slowPathFunction, LValue base, LValue storage, LValue index, LValue value,
        LBasicBlock continuation)
    {
        if (!m_node->arrayMode().isInBounds()) {
            LBasicBlock notInBoundsCase =
                m_out.newBlock();
            LBasicBlock performStore =
                m_out.newBlock();
                
            LValue isNotInBounds = m_out.aboveOrEqual(
                index, m_out.load32NonNegative(storage, m_heaps.Butterfly_publicLength));
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
                    Void, m_out.operation(slowPathFunction),
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
    
    LValue caged(Gigacage::Kind kind, LValue ptr)
    {
#if GIGACAGE_ENABLED
        if (!Gigacage::isEnabled(kind))
            return ptr;
        
        if (kind == Gigacage::Primitive && Gigacage::canPrimitiveGigacageBeDisabled()) {
            if (vm().primitiveGigacageEnabled().isStillValid())
                m_graph.watchpoints().addLazily(vm().primitiveGigacageEnabled());
            else
                return ptr;
        }
        
        LValue basePtr = m_out.constIntPtr(Gigacage::basePtr(kind));
        LValue mask = m_out.constIntPtr(Gigacage::mask(kind));
        
        LValue masked = m_out.bitAnd(ptr, mask);
        LValue result = m_out.add(masked, basePtr);

        // Make sure that B3 doesn't try to do smart reassociation of these pointer bits.
        // FIXME: In an ideal world, B3 would not do harmful reassociations, and if it did, it would be able
        // to undo them during constant hoisting and regalloc. As it stands, if you remove this then Octane
        // gets 1.6% slower and Kraken gets 5% slower. It's all because the basePtr, which is a constant,
        // gets reassociated out of the add above and into the address arithmetic. This disables hoisting of
        // the basePtr constant. Hoisting that constant is worth a lot more perf than the reassociation. One
        // way to make this all work happily is to combine offset legalization with constant hoisting, and
        // then teach it reassociation. So, Add(Add(a, b), const) where a is loop-invariant while b isn't
        // will turn into Add(Add(a, const), b) by the constant hoister. We would have to teach B3 to do this
        // and possibly other smart things if we want to be able to remove this opaque.
        // https://bugs.webkit.org/show_bug.cgi?id=175493
        return m_out.opaque(result);
#else
        UNUSED_PARAM(kind);
        return ptr;
#endif
    }
    
    void buildSwitch(SwitchData* data, LType type, LValue switchValue)
    {
        ASSERT(type == pointerType() || type == Int32);

        Vector<SwitchCase> cases;
        for (unsigned i = 0; i < data->cases.size(); ++i) {
            SwitchCase newCase;

            if (type == pointerType()) {
                newCase = SwitchCase(m_out.constIntPtr(data->cases[i].value.switchLookupValue(data->kind)),
                    lowBlock(data->cases[i].target.block), Weight(data->cases[i].target.count));
            } else if (type == Int32) {
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
            DFG_ASSERT(m_graph, m_node, alreadyCheckedLength == minLength, alreadyCheckedLength, minLength);
            DFG_ASSERT(m_graph, m_node, allLengthsEqual);
        }
        
        DFG_ASSERT(m_graph, m_node, minLength >= commonChars, minLength, commonChars);
        
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
            
            DFG_ASSERT(m_graph, m_node, cases[begin].string->length() == commonChars, cases[begin].string->length(), commonChars);
            for (unsigned i = begin + 1; i < end; ++i)
                DFG_ASSERT(m_graph, m_node, cases[i].string->length() > commonChars, cases[i].string->length(), commonChars);
            
            if (allLengthsEqual) {
                DFG_ASSERT(m_graph, m_node, end == begin + 1, end, begin);
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
        
        DFG_ASSERT(m_graph, m_node, end >= begin + 2, end, begin);
        
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
            Int32, m_out.operation(operationSwitchStringAndGetBranchOffset),
            m_callFrame, m_out.constIntPtr(data->switchTableIndex), string);
        
        StringJumpTable& table = codeBlock()->stringSwitchJumpTable(data->switchTableIndex);
        
        Vector<SwitchCase> cases;
        // These may be negative, or zero, or probably other stuff, too. We don't want to mess with HashSet's corner cases and we don't really care about throughput here.
        StdUnorderedSet<int32_t> alreadyHandled;
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
        //     } else if (is bigint) {
        //         return bigint
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
        //
        // FIXME: typeof Symbol should be more frequently seen than BigInt.
        // We should change the order of type detection based on this frequency.
        // https://bugs.webkit.org/show_bug.cgi?id=192650
        
        LBasicBlock cellCase = m_out.newBlock();
        LBasicBlock objectCase = m_out.newBlock();
        LBasicBlock functionCase = m_out.newBlock();
        LBasicBlock notFunctionCase = m_out.newBlock();
        LBasicBlock reallyObjectCase = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock unreachable = m_out.newBlock();
        LBasicBlock notObjectCase = m_out.newBlock();
        LBasicBlock stringCase = m_out.newBlock();
        LBasicBlock notStringCase = m_out.newBlock();
        LBasicBlock bigIntCase = m_out.newBlock();
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
        VM& vm = this->vm();
        LValue result = lazySlowPath(
            [=, &vm] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
                return createLazyCallGenerator(vm,
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
            unsure(stringCase), unsure(notStringCase));
        
        m_out.appendTo(stringCase, notStringCase);
        functor(TypeofType::String);

        m_out.appendTo(notStringCase, bigIntCase);
        m_out.branch(
            isBigInt(value, provenType(child) & (SpecCell - SpecObject - SpecString)),
            unsure(bigIntCase), unsure(symbolCase));

        m_out.appendTo(bigIntCase, symbolCase);
        functor(TypeofType::BigInt);
        
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
    
    TypedPointer pointerIntoTypedArray(LValue storage, LValue index, TypedArrayType type)
    {
        LValue offset = m_out.shl(m_out.zeroExtPtr(index), m_out.constIntPtr(logElementSize(type)));

        return TypedPointer(
            m_heaps.typedArrayProperties,
            m_out.add(
                storage,
                offset
            ));
    }
    
    LValue loadFromIntTypedArray(TypedPointer pointer, TypedArrayType type)
    {
        switch (elementSize(type)) {
        case 1:
            return isSigned(type) ? m_out.load8SignExt32(pointer) : m_out.load8ZeroExt32(pointer);
        case 2:
            return isSigned(type) ? m_out.load16SignExt32(pointer) : m_out.load16ZeroExt32(pointer);
        case 4:
            return m_out.load32(pointer);
        default:
            DFG_CRASH(m_graph, m_node, "Bad element size");
        }
    }
    
    Output::StoreType storeType(TypedArrayType type)
    {
        if (isInt(type)) {
            switch (elementSize(type)) {
            case 1:
                return Output::Store32As8;
            case 2:
                return Output::Store32As16;
            case 4:
                return Output::Store32;
            default:
                DFG_CRASH(m_graph, m_node, "Bad element size");
                return Output::Store32;
            }
        }
        switch (type) {
        case TypeFloat32:
            return Output::StoreFloat;
        case TypeFloat64:
            return Output::StoreDouble;
        default:
            DFG_CRASH(m_graph, m_node, "Bad typed array type");
        }
    }
    
    void setIntTypedArrayLoadResult(LValue result, TypedArrayType type, bool canSpeculate = false)
    {
        if (elementSize(type) < 4 || isSigned(type)) {
            setInt32(result);
            return;
        }
        
        if (m_node->shouldSpeculateInt32() && canSpeculate) {
            speculate(
                Overflow, noValue(), 0, m_out.lessThan(result, m_out.int32Zero));
            setInt32(result);
            return;
        }
        
        if (m_node->shouldSpeculateAnyInt()) {
            setStrictInt52(m_out.zeroExt(result, Int64));
            return;
        }
        
        setDouble(m_out.unsignedToDouble(result));
    }
    
    LValue getIntTypedArrayStoreOperand(Edge edge, bool isClamped = false)
    {
        LValue intValue;
        switch (edge.useKind()) {
        case Int52RepUse:
        case Int32Use: {
            if (edge.useKind() == Int32Use)
                intValue = lowInt32(edge);
            else
                intValue = m_out.castToInt32(lowStrictInt52(edge));

            if (isClamped) {
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
                intValue = m_out.phi(Int32, intValues);
            }
            break;
        }
                        
        case DoubleRepUse: {
            LValue doubleValue = lowDouble(edge);
                        
            if (isClamped) {
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
                intValue = m_out.phi(Int32, intValues);
            } else
                intValue = doubleToInt32(doubleValue);
            break;
        }
                        
        default:
            DFG_CRASH(m_graph, m_node, "Bad use kind");
        }
        
        return intValue;
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
        results.append(m_out.anchor(m_out.call(Int32, m_out.operation(operationToInt32), doubleValue)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(Int32, results);
    }
    
    LValue doubleToInt32(LValue doubleValue)
    {
#if CPU(ARM64)
        if (MacroAssemblerARM64::supportsDoubleToInt32ConversionUsingJavaScriptSemantics()) {
            PatchpointValue* patchpoint = m_out.patchpoint(Int32);
            patchpoint->append(ConstrainedValue(doubleValue, B3::ValueRep::SomeRegister));
            patchpoint->setGenerator([=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                jit.convertDoubleToInt32UsingJavaScriptSemantics(params[1].fpr(), params[0].gpr());
            });
            patchpoint->effects = Effects::none();
            return patchpoint;
        }
#endif

        if (hasSensibleDoubleToInt())
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
            m_out.call(Int32, m_out.operation(operationToInt32SensibleSlow), doubleValue));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        return m_out.phi(Int32, fastResult, slowResult);
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
    PatchpointValue* lazySlowPath(const Functor& functor, ArgumentTypes... arguments)
    {
        return lazySlowPath(functor, Vector<LValue>{ arguments... });
    }

    template<typename Functor>
    PatchpointValue* lazySlowPath(const Functor& functor, const Vector<LValue>& userArguments)
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
                                linkBuffer.link(generatorJump,
                                    CodeLocationLabel<JITThunkPtrTag>(vm->getCTIStub(lazySlowPathGenerationThunkGenerator).code()));
                                
                                std::unique_ptr<LazySlowPath> lazySlowPath = std::make_unique<LazySlowPath>();

                                auto linkedPatchableJump = CodeLocationJump<JSInternalPtrTag>(linkBuffer.locationOf<JSInternalPtrTag>(patchableJump));

                                CodeLocationLabel<JSInternalPtrTag> linkedDone = linkBuffer.locationOf<JSInternalPtrTag>(done);

                                CallSiteIndex callSiteIndex =
                                    jitCode->common.addUniqueCallSiteIndex(origin);
                                    
                                lazySlowPath->initialize(
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

    void speculate(
        ExitKind kind, FormattedValue lowValue, const MethodOfGettingAValueProfile& profile, LValue failCondition)
    {
        appendOSRExit(kind, lowValue, profile, failCondition, m_origin);
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

    void simulatedTypeCheck(Edge highValue, SpeculatedType typesPassedThrough)
    {
        m_interpreter.filter(highValue, typesPassedThrough);
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
            simulatedTypeCheck(edge, SpecInt32Only);
            if (!value.isInt32()) {
                if (mayHaveTypeCheck(edge.useKind()))
                    terminate(Uncountable);
                return m_out.int32Zero;
            }
            LValue result = m_out.constInt32(value.asInt32());
            result->setOrigin(B3::Origin(edge.node()));
            return result;
        }
        
        LoweredNodeValue value = m_int32Values.get(edge.node());
        if (isValid(value)) {
            simulatedTypeCheck(edge, SpecInt32Only);
            return value.value();
        }
        
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
                jsValueValue(boxedResult), edge, SpecInt32Only, isNotInt32(boxedResult));
            LValue result = unboxInt32(boxedResult);
            setInt32(edge.node(), result);
            return result;
        }

        DFG_ASSERT(m_graph, m_node, !(provenType(edge) & SpecInt32Only), provenType(edge));
        if (mayHaveTypeCheck(edge.useKind()))
            terminate(Uncountable);
        return m_out.int32Zero;
    }
    
    enum Int52Kind { StrictInt52, Int52 };
    LValue lowInt52(Edge edge, Int52Kind kind)
    {
        DFG_ASSERT(m_graph, m_node, edge.useKind() == Int52RepUse, edge.useKind());
        
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

        DFG_ASSERT(m_graph, m_node, !provenType(edge), provenType(edge));
        if (mayHaveTypeCheck(edge.useKind()))
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
        DFG_ASSERT(m_graph, m_node, mode == ManualOperandSpeculation || DFG::isCell(edge.useKind()), edge.useKind());
        
        if (edge->op() == JSConstant) {
            FrozenValue* value = edge->constant();
            simulatedTypeCheck(edge, SpecCellCheck);
            if (!value->value().isCell()) {
                if (mayHaveTypeCheck(edge.useKind()))
                    terminate(Uncountable);
                return m_out.intPtrZero;
            }
            LValue result = frozenPointer(value);
            result->setOrigin(B3::Origin(edge.node()));
            return result;
        }
        
        LoweredNodeValue value = m_jsValueValues.get(edge.node());
        if (isValid(value)) {
            LValue uncheckedValue = value.value();
            FTL_TYPE_CHECK(
                jsValueValue(uncheckedValue), edge, SpecCellCheck, isNotCell(uncheckedValue));
            return uncheckedValue;
        }
        
        DFG_ASSERT(m_graph, m_node, !(provenType(edge) & SpecCellCheck), provenType(edge));
        if (mayHaveTypeCheck(edge.useKind()))
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

    LValue lowMapObject(Edge edge)
    {
        LValue result = lowCell(edge);
        speculateMapObject(edge, result);
        return result;
    }

    LValue lowSetObject(Edge edge)
    {
        LValue result = lowCell(edge);
        speculateSetObject(edge, result);
        return result;
    }

    LValue lowWeakMapObject(Edge edge)
    {
        LValue result = lowCell(edge);
        speculateWeakMapObject(edge, result);
        return result;
    }

    LValue lowWeakSetObject(Edge edge)
    {
        LValue result = lowCell(edge);
        speculateWeakSetObject(edge, result);
        return result;
    }

    LValue lowDataViewObject(Edge edge)
    {
        LValue result = lowCell(edge);
        speculateDataViewObject(edge, result);
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

    LValue lowBigInt(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        ASSERT_UNUSED(mode, mode == ManualOperandSpeculation || edge.useKind() == BigIntUse);

        LValue result = lowCell(edge, mode);
        speculateBigInt(edge, result);
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
            simulatedTypeCheck(edge, SpecBoolean);
            if (!value.isBoolean()) {
                if (mayHaveTypeCheck(edge.useKind()))
                    terminate(Uncountable);
                return m_out.booleanFalse;
            }
            LValue result = m_out.constBool(value.asBoolean());
            result->setOrigin(B3::Origin(edge.node()));
            return result;
        }
        
        LoweredNodeValue value = m_booleanValues.get(edge.node());
        if (isValid(value)) {
            simulatedTypeCheck(edge, SpecBoolean);
            return value.value();
        }
        
        value = m_jsValueValues.get(edge.node());
        if (isValid(value)) {
            LValue unboxedResult = value.value();
            FTL_TYPE_CHECK(
                jsValueValue(unboxedResult), edge, SpecBoolean, isNotBoolean(unboxedResult));
            LValue result = unboxBoolean(unboxedResult);
            setBoolean(edge.node(), result);
            return result;
        }

        DFG_ASSERT(m_graph, m_node, !(provenType(edge) & SpecBoolean), provenType(edge));
        if (mayHaveTypeCheck(edge.useKind()))
            terminate(Uncountable);
        return m_out.booleanFalse;
    }
    
    LValue lowDouble(Edge edge)
    {
        DFG_ASSERT(m_graph, m_node, isDouble(edge.useKind()), edge.useKind());
        
        LoweredNodeValue value = m_doubleValues.get(edge.node());
        if (isValid(value))
            return value.value();
        DFG_ASSERT(m_graph, m_node, !provenType(edge), provenType(edge));
        if (mayHaveTypeCheck(edge.useKind()))
            terminate(Uncountable);
        return m_out.doubleZero;
    }
    
    LValue lowJSValue(Edge edge, OperandSpeculationMode mode = AutomaticOperandSpeculation)
    {
        DFG_ASSERT(m_graph, m_node, mode == ManualOperandSpeculation || edge.useKind() == UntypedUse, m_node->op(), edge.useKind());
        DFG_ASSERT(m_graph, m_node, !isDouble(edge.useKind()), m_node->op(), edge.useKind());
        DFG_ASSERT(m_graph, m_node, edge.useKind() != Int52RepUse, m_node->op(), edge.useKind());
        
        if (edge->hasConstant()) {
            LValue result = m_out.constInt64(JSValue::encode(edge->asJSValue()));
            result->setOrigin(B3::Origin(edge.node()));
            return result;
        }

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

    LValue lowNotCell(Edge edge)
    {
        LValue result = lowJSValue(edge, ManualOperandSpeculation);
        FTL_TYPE_CHECK(jsValueValue(result), edge, ~SpecCellCheck, isCell(result));
        return result;
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
            noValue(), edge, SpecInt32Only,
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
        return m_out.phi(Int64, results);
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
        if (LValue proven = isProvenValue(type, SpecInt32Only))
            return proven;
        return m_out.aboveOrEqual(jsValue, m_tagTypeNumber);
    }
    LValue isNotInt32(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~SpecInt32Only))
            return proven;
        return m_out.below(jsValue, m_tagTypeNumber);
    }
    LValue unboxInt32(LValue jsValue)
    {
        return m_out.castToInt32(jsValue);
    }
    LValue boxInt32(LValue value)
    {
        return m_out.add(m_out.zeroExt(value, Int64), m_tagTypeNumber);
    }
    
    LValue isCellOrMisc(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecCellCheck | SpecMisc))
            return proven;
        return m_out.testIsZero64(jsValue, m_tagTypeNumber);
    }
    LValue isNotCellOrMisc(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, ~(SpecCellCheck | SpecMisc)))
            return proven;
        return m_out.testNonZero64(jsValue, m_tagTypeNumber);
    }

    LValue unboxDouble(LValue jsValue, LValue* unboxedAsInt = nullptr)
    {
        LValue asInt = m_out.add(jsValue, m_tagTypeNumber);
        if (unboxedAsInt)
            *unboxedAsInt = asInt;
        return m_out.bitCast(asInt, Double);
    }
    LValue boxDouble(LValue doubleValue)
    {
        return m_out.sub(m_out.bitCast(doubleValue, Int64), m_tagTypeNumber);
    }
    
    LValue jsValueToStrictInt52(Edge edge, LValue boxedValue)
    {
        LBasicBlock intCase = m_out.newBlock();
        LBasicBlock doubleCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
            
        LValue isNotInt32;
        if (!m_interpreter.needsTypeCheck(edge, SpecInt32Only))
            isNotInt32 = m_out.booleanFalse;
        else if (!m_interpreter.needsTypeCheck(edge, ~SpecInt32Only))
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
            Int64, m_out.operation(operationConvertBoxedDoubleToInt52), boxedValue);
        FTL_TYPE_CHECK(
            jsValueValue(boxedValue), edge, SpecInt32Only | SpecAnyIntAsDouble,
            m_out.equal(possibleResult, m_out.constInt64(JSValue::notInt52)));
            
        ValueFromBlock doubleToInt52 = m_out.anchor(possibleResult);
        m_out.jump(continuation);
            
        m_out.appendTo(continuation, lastNext);
            
        return m_out.phi(Int64, intToInt52, doubleToInt52);
    }
    
    LValue doubleToStrictInt52(Edge edge, LValue value)
    {
        LValue possibleResult = m_out.call(
            Int64, m_out.operation(operationConvertDoubleToInt52), value);
        FTL_TYPE_CHECK_WITH_EXIT_KIND(Int52Overflow,
            doubleValue(value), edge, SpecAnyIntAsDouble,
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

            LValue doubleBitcastToInt64 = m_out.bitCast(value, Int64);
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
        if (LValue proven = isProvenValue(type, ~SpecCellCheck))
            return proven;
        return m_out.testNonZero64(jsValue, m_tagMask);
    }
    
    LValue isCell(LValue jsValue, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type, SpecCellCheck))
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
        case KnownOtherUse:
        case DoubleRepUse:
        case Int52RepUse:
        case KnownCellUse:
        case KnownBooleanUse:
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
        case AnyIntUse:
            speculateAnyInt(edge);
            break;
        case ObjectUse:
            speculateObject(edge);
            break;
        case ArrayUse:
            speculateArray(edge);
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
        case ProxyObjectUse:
            speculateProxyObject(edge);
            break;
        case DerivedArrayUse:
            speculateDerivedArray(edge);
            break;
        case MapObjectUse:
            speculateMapObject(edge);
            break;
        case SetObjectUse:
            speculateSetObject(edge);
            break;
        case WeakMapObjectUse:
            speculateWeakMapObject(edge);
            break;
        case WeakSetObjectUse:
            speculateWeakSetObject(edge);
            break;
        case DataViewObjectUse:
            speculateDataViewObject(edge);
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
        case DoubleRepAnyIntUse:
            speculateDoubleRepAnyInt(edge);
            break;
        case BooleanUse:
            speculateBoolean(edge);
            break;
        case BigIntUse:
            speculateBigInt(edge);
            break;
        case NotStringVarUse:
            speculateNotStringVar(edge);
            break;
        case NotSymbolUse:
            speculateNotSymbol(edge);
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

    void speculateNotCell(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        lowNotCell(edge);
    }
    
    void speculateCellOrOther(Edge edge)
    {
        if (shouldNotHaveTypeCheck(edge.useKind()))
            return;
        
        LValue value = lowJSValue(edge, ManualOperandSpeculation);

        LBasicBlock isNotCell = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(isCell(value, provenType(edge)), unsure(continuation), unsure(isNotCell));

        LBasicBlock lastNext = m_out.appendTo(isNotCell, continuation);
        FTL_TYPE_CHECK(jsValueValue(value), edge, SpecCellCheck | SpecOther, isNotOther(value));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
    }
    
    void speculateAnyInt(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        jsValueToStrictInt52(edge, lowJSValue(edge, ManualOperandSpeculation));
    }

    LValue isCellWithType(LValue cell, JSType queriedType, SpeculatedType speculatedTypeForQuery, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, speculatedTypeForQuery))
            return proven;
        return m_out.equal(
            m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType),
            m_out.constInt32(queriedType));
    }

    LValue isTypedArrayView(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, SpecTypedArrayView))
            return proven;
        LValue jsType = m_out.sub(
            m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType),
            m_out.constInt32(FirstTypedArrayType));
        return m_out.below(
            jsType,
            m_out.constInt32(NumberOfTypedArrayTypesExcludingDataView));
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
    
    LValue isSymbol(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, SpecSymbol))
            return proven;
        return m_out.equal(
            m_out.load32(cell, m_heaps.JSCell_structureID),
            m_out.constInt32(vm().symbolStructure->id()));
    }

    LValue isNotBigInt(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, ~SpecBigInt))
            return proven;
        return m_out.notEqual(
            m_out.load32(cell, m_heaps.JSCell_structureID),
            m_out.constInt32(vm().bigIntStructure->id()));
    }

    LValue isBigInt(LValue cell, SpeculatedType type = SpecFullTop)
    {
        if (LValue proven = isProvenValue(type & SpecCell, SpecBigInt))
            return proven;
        return m_out.equal(
            m_out.load32(cell, m_heaps.JSCell_structureID),
            m_out.constInt32(vm().bigIntStructure->id()));
    }

    LValue isArrayTypeForArrayify(LValue cell, ArrayMode arrayMode)
    {
        switch (arrayMode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::Undecided:
        case Array::ArrayStorage: {
            IndexingType indexingModeMask = IsArray | IndexingShapeMask;
            if (arrayMode.action() == Array::Write)
                indexingModeMask |= CopyOnWrite;

            IndexingType shape = arrayMode.shapeMask();
            LValue indexingType = m_out.load8ZeroExt32(cell, m_heaps.JSCell_indexingTypeAndMisc);

            switch (arrayMode.arrayClass()) {
            case Array::OriginalArray:
            case Array::OriginalCopyOnWriteArray:
                DFG_CRASH(m_graph, m_node, "Unexpected original array");
                return nullptr;

            case Array::Array:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt32(indexingModeMask)),
                    m_out.constInt32(IsArray | shape));

            case Array::NonArray:
            case Array::OriginalNonArray:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt32(indexingModeMask)),
                    m_out.constInt32(shape));

            case Array::PossiblyArray:
                return m_out.equal(
                    m_out.bitAnd(indexingType, m_out.constInt32(indexingModeMask & ~IsArray)),
                    m_out.constInt32(shape));
            }
            break;
        }

        case Array::SlowPutArrayStorage: {
            ASSERT(!arrayMode.isJSArrayWithOriginalStructure());
            LValue indexingType = m_out.load8ZeroExt32(cell, m_heaps.JSCell_indexingTypeAndMisc);

            LBasicBlock trueCase = m_out.newBlock();
            LBasicBlock checkCase = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            ValueFromBlock falseValue = m_out.anchor(m_out.booleanFalse);
            LValue isAnArrayStorageShape = m_out.belowOrEqual(
                m_out.sub(
                    m_out.bitAnd(indexingType, m_out.constInt32(IndexingShapeMask)),
                    m_out.constInt32(ArrayStorageShape)),
                m_out.constInt32(SlowPutArrayStorageShape - ArrayStorageShape));
            m_out.branch(isAnArrayStorageShape, unsure(checkCase), unsure(continuation));

            LBasicBlock lastNext = m_out.appendTo(checkCase, trueCase);
            switch (arrayMode.arrayClass()) {
            case Array::OriginalArray:
            case Array::OriginalCopyOnWriteArray:
                DFG_CRASH(m_graph, m_node, "Unexpected original array");
                return nullptr;

            case Array::Array:
                m_out.branch(
                    m_out.testNonZero32(indexingType, m_out.constInt32(IsArray)),
                    unsure(trueCase), unsure(continuation));
                break;

            case Array::NonArray:
            case Array::OriginalNonArray:
                m_out.branch(
                    m_out.testIsZero32(indexingType, m_out.constInt32(IsArray)),
                    unsure(trueCase), unsure(continuation));
                break;

            case Array::PossiblyArray:
                m_out.jump(trueCase);
                break;
            }

            m_out.appendTo(trueCase, continuation);
            ValueFromBlock trueValue = m_out.anchor(m_out.booleanTrue);
            m_out.jump(continuation);

            m_out.appendTo(continuation, lastNext);
            return m_out.phi(Int32, falseValue, trueValue);
        }

        default:
            break;
        }
        DFG_CRASH(m_graph, m_node, "Corrupt array class");
    }

    LValue isArrayTypeForCheckArray(LValue cell, ArrayMode arrayMode)
    {
        switch (arrayMode.type()) {
        case Array::Int32:
        case Array::Double:
        case Array::Contiguous:
        case Array::Undecided:
        case Array::ArrayStorage:
        case Array::SlowPutArrayStorage:
            return isArrayTypeForArrayify(cell, arrayMode);
            
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
            m_out.constInt32(MasqueradesAsUndefined | OverridesGetCallData));
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

    void speculateArray(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecArray, isNotType(cell, ArrayType));
    }

    void speculateArray(Edge edge)
    {
        speculateArray(edge, lowCell(edge));
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
            jsValueValue(value), edge, (~SpecCellCheck) | SpecObject, isNotObject(value));
        
        m_out.jump(continuation);
        
        m_out.appendTo(primitiveCase, continuation);
        
        FTL_TYPE_CHECK(
            jsValueValue(value), edge, SpecCellCheck | SpecOther, isNotOther(value));
        
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

    void speculateProxyObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecProxyObject, isNotType(cell, ProxyObjectType));
    }

    void speculateProxyObject(Edge edge)
    {
        speculateProxyObject(edge, lowCell(edge));
    }

    void speculateDerivedArray(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecDerivedArray, isNotType(cell, DerivedArrayType));
    }

    void speculateDerivedArray(Edge edge)
    {
        speculateDerivedArray(edge, lowCell(edge));
    }

    void speculateMapObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecMapObject, isNotType(cell, JSMapType));
    }

    void speculateMapObject(Edge edge)
    {
        speculateMapObject(edge, lowCell(edge));
    }

    void speculateSetObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecSetObject, isNotType(cell, JSSetType));
    }

    void speculateSetObject(Edge edge)
    {
        speculateSetObject(edge, lowCell(edge));
    }

    void speculateWeakMapObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecWeakMapObject, isNotType(cell, JSWeakMapType));
    }

    void speculateWeakMapObject(Edge edge)
    {
        speculateWeakMapObject(edge, lowCell(edge));
    }

    void speculateWeakSetObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecWeakSetObject, isNotType(cell, JSWeakSetType));
    }

    void speculateWeakSetObject(Edge edge)
    {
        speculateWeakSetObject(edge, lowCell(edge));
    }

    void speculateDataViewObject(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(
            jsValueValue(cell), edge, SpecDataViewObject, isNotType(cell, DataViewType));
    }

    void speculateDataViewObject(Edge edge)
    {
        speculateDataViewObject(edge, lowCell(edge));
    }
    
    void speculateString(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecString, isNotString(cell));
    }
    
    void speculateString(Edge edge)
    {
        speculateString(edge, lowCell(edge));
    }
    
    void speculateStringOrOther(Edge edge, LValue value)
    {
        if (!m_interpreter.needsTypeCheck(edge))
            return;
        
        LBasicBlock cellCase = m_out.newBlock();
        LBasicBlock notCellCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(isCell(value, provenType(edge)), unsure(cellCase), unsure(notCellCase));

        LBasicBlock lastNext = m_out.appendTo(cellCase, notCellCase);

        FTL_TYPE_CHECK(jsValueValue(value), edge, (~SpecCellCheck) | SpecString, isNotString(value));

        m_out.jump(continuation);
        m_out.appendTo(notCellCase, continuation);

        FTL_TYPE_CHECK(jsValueValue(value), edge, SpecCellCheck | SpecOther, isNotOther(value));

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
    }
    
    void speculateStringOrStringObject(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge, SpecString | SpecStringObject))
            return;

        LValue cellBase = lowCell(edge);
        if (!m_interpreter.needsTypeCheck(edge, SpecString | SpecStringObject))
            return;
        
        LBasicBlock notString = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LValue type = m_out.load8ZeroExt32(cellBase, m_heaps.JSCell_typeInfoType);
        m_out.branch(
            m_out.equal(type, m_out.constInt32(StringType)),
            unsure(continuation), unsure(notString));
        
        LBasicBlock lastNext = m_out.appendTo(notString, continuation);
        speculate(
            BadType, jsValueValue(cellBase), edge.node(),
            m_out.notEqual(type, m_out.constInt32(StringObjectType)));
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
        m_interpreter.filter(edge, SpecString | SpecStringObject);
    }
    
    void speculateStringObjectForCell(Edge edge, LValue cell)
    {
        if (!m_interpreter.needsTypeCheck(edge, SpecStringObject))
            return;

        LValue type = m_out.load8ZeroExt32(cell, m_heaps.JSCell_typeInfoType);
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecStringObject, m_out.notEqual(type, m_out.constInt32(StringObjectType)));
    }
    
    void speculateSymbol(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecSymbol, isNotSymbol(cell));
    }

    void speculateSymbol(Edge edge)
    {
        speculateSymbol(edge, lowCell(edge));
    }

    void speculateBigInt(Edge edge, LValue cell)
    {
        FTL_TYPE_CHECK(jsValueValue(cell), edge, SpecBigInt, isNotBigInt(cell));
    }

    void speculateBigInt(Edge edge)
    {
        speculateBigInt(edge, lowCell(edge));
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
    
    void speculateDoubleRepAnyInt(Edge edge)
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
    
    void speculateNotSymbol(Edge edge)
    {
        if (!m_interpreter.needsTypeCheck(edge, ~SpecSymbol))
            return;

        ASSERT(mayHaveTypeCheck(edge.useKind()));
        LValue value = lowJSValue(edge, ManualOperandSpeculation);

        LBasicBlock isCellCase = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        m_out.branch(isCell(value, provenType(edge)), unsure(isCellCase), unsure(continuation));

        LBasicBlock lastNext = m_out.appendTo(isCellCase, continuation);
        speculate(BadType, jsValueValue(value), edge.node(), isSymbol(value));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);

        m_interpreter.filter(edge, ~SpecSymbol);
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

    void speculateTypedArrayIsNotNeutered(LValue base)
    {
        LBasicBlock isWasteful = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();

        LValue mode = m_out.load32(base, m_heaps.JSArrayBufferView_mode);
        m_out.branch(m_out.equal(mode, m_out.constInt32(WastefulTypedArray)),
            unsure(isWasteful), unsure(continuation));

        LBasicBlock lastNext = m_out.appendTo(isWasteful, continuation);
        LValue vector = m_out.loadPtr(base, m_heaps.JSArrayBufferView_vector);
        speculate(Uncountable, jsValueValue(vector), m_node, m_out.isZero64(vector));
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
    }

    bool masqueradesAsUndefinedWatchpointIsStillValid()
    {
        return m_graph.masqueradesAsUndefinedWatchpointIsStillValid(m_node->origin.semantic);
    }
    
    LValue loadCellState(LValue base)
    {
        return m_out.load8ZeroExt32(base, m_heaps.JSCell_cellState);
    }

    void emitStoreBarrier(LValue base, bool isFenced)
    {
        LBasicBlock recheckPath = nullptr;
        if (isFenced)
            recheckPath = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(isFenced ? recheckPath : slowPath);

        LValue threshold;
        if (isFenced)
            threshold = m_out.load32(m_out.absolute(vm().heap.addressOfBarrierThreshold()));
        else
            threshold = m_out.constInt32(blackThreshold);
        
        m_out.branch(
            m_out.above(loadCellState(base), threshold),
            usually(continuation), rarely(isFenced ? recheckPath : slowPath));
        
        if (isFenced) {
            m_out.appendTo(recheckPath, slowPath);
            
            m_out.fence(&m_heaps.root, &m_heaps.JSCell_cellState);
            
            m_out.branch(
                m_out.above(loadCellState(base), m_out.constInt32(blackThreshold)),
                usually(continuation), rarely(slowPath));
        }

        m_out.appendTo(slowPath, continuation);
        
        LValue call = vmCall(Void, m_out.operation(operationWriteBarrierSlowPath), m_callFrame, base);
        m_heaps.decorateCCallRead(&m_heaps.root, call);
        m_heaps.decorateCCallWrite(&m_heaps.JSCell_cellState, call);
        
        m_out.jump(continuation);

        m_out.appendTo(continuation, lastNext);
    }
    
    void mutatorFence()
    {
        if (isX86()) {
            m_out.fence(&m_heaps.root, nullptr);
            return;
        }
        
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(slowPath);
        
        m_out.branch(
            m_out.load8ZeroExt32(m_out.absolute(vm().heap.addressOfMutatorShouldBeFenced())),
            rarely(slowPath), usually(continuation));
        
        m_out.appendTo(slowPath, continuation);
        
        m_out.fence(&m_heaps.root, nullptr);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    void nukeStructureAndSetButterfly(LValue butterfly, LValue object)
    {
        if (isX86()) {
            m_out.store32(
                m_out.bitOr(
                    m_out.load32(object, m_heaps.JSCell_structureID),
                    m_out.constInt32(nukedStructureIDBit())),
                object, m_heaps.JSCell_structureID);
            m_out.fence(&m_heaps.root, nullptr);
            m_out.storePtr(butterfly, object, m_heaps.JSObject_butterfly);
            m_out.fence(&m_heaps.root, nullptr);
            return;
        }
        
        LBasicBlock fastPath = m_out.newBlock();
        LBasicBlock slowPath = m_out.newBlock();
        LBasicBlock continuation = m_out.newBlock();
        
        LBasicBlock lastNext = m_out.insertNewBlocksBefore(fastPath);
        
        m_out.branch(
            m_out.load8ZeroExt32(m_out.absolute(vm().heap.addressOfMutatorShouldBeFenced())),
            rarely(slowPath), usually(fastPath));

        m_out.appendTo(fastPath, slowPath);
        
        m_out.storePtr(butterfly, object, m_heaps.JSObject_butterfly);
        m_out.jump(continuation);
        
        m_out.appendTo(slowPath, continuation);
        
        m_out.store32(
            m_out.bitOr(
                m_out.load32(object, m_heaps.JSCell_structureID),
                m_out.constInt32(nukedStructureIDBit())),
            object, m_heaps.JSCell_structureID);
        m_out.fence(&m_heaps.root, nullptr);
        m_out.storePtr(butterfly, object, m_heaps.JSObject_butterfly);
        m_out.fence(&m_heaps.root, nullptr);
        m_out.jump(continuation);
        
        m_out.appendTo(continuation, lastNext);
    }
    
    LValue preciseIndexMask64(LValue value, LValue index, LValue limit)
    {
        return m_out.bitAnd(
            value,
            m_out.aShr(
                m_out.sub(
                    index,
                    m_out.opaque(limit)),
                m_out.constInt32(63)));
    }
    
    LValue preciseIndexMask32(LValue value, LValue index, LValue limit)
    {
        return preciseIndexMask64(value, m_out.zeroExt(index, Int64), m_out.zeroExt(limit, Int64));
    }
    
    LValue dynamicPoison(LValue value, LValue poison)
    {
        return m_out.add(
            value,
            m_out.shl(
                m_out.zeroExt(poison, pointerType()),
                m_out.constInt32(40)));
    }
    
    LValue dynamicPoisonOnLoadedType(LValue value, LValue actualType, JSType expectedType)
    {
        return dynamicPoison(
            value,
            m_out.bitXor(
                m_out.opaque(actualType),
                m_out.constInt32(expectedType)));
    }
    
    LValue dynamicPoisonOnType(LValue value, JSType expectedType)
    {
        return dynamicPoisonOnLoadedType(
            value,
            m_out.load8ZeroExt32(value, m_heaps.JSCell_typeInfoType),
            expectedType);
    }

    template<typename... Args>
    LValue vmCall(LType type, LValue function, Args&&... args)
    {
        callPreflight();
        LValue result = m_out.call(type, function, std::forward<Args>(args)...);
        if (mayExit(m_graph, m_node))
            callCheck();
        else {
            // We can't exit due to an exception, so we also can't throw an exception.
#ifndef NDEBUG
            LBasicBlock crash = m_out.newBlock();
            LBasicBlock continuation = m_out.newBlock();

            LValue exception = m_out.load64(m_out.absolute(vm().addressOfException()));
            LValue hadException = m_out.notZero64(exception);

            m_out.branch(
                hadException, rarely(crash), usually(continuation));

            LBasicBlock lastNext = m_out.appendTo(crash, continuation);
            m_out.unreachable();

            m_out.appendTo(continuation, lastNext);
#endif
        }
        return result;
    }
    
    void callPreflight(CodeOrigin codeOrigin)
    {
        CallSiteIndex callSiteIndex = m_ftlState.jitCode->common.addCodeOrigin(codeOrigin);
        m_out.store32(
            m_out.constInt32(callSiteIndex.bits()),
            tagFor(CallFrameSlot::argumentCount));
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
            || m_node->op() == TailCallForwardVarargsInlinedCaller
            || m_node->op() == DirectTailCallInlinedCaller) {
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
            m_out.call(Void, m_out.operation(operationExceptionFuzz), m_callFrame);
        
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
        return appendOSRExitDescriptor(lowValue, m_graph.methodOfGettingAValueProfileFor(m_node, highValue));
    }

    OSRExitDescriptor* appendOSRExitDescriptor(FormattedValue lowValue, const MethodOfGettingAValueProfile& profile)
    {
        return &m_ftlState.jitCode->osrExitDescriptors.alloc(
            lowValue.format(), profile,
            availabilityMap().m_locals.numberOfArguments(),
            availabilityMap().m_locals.numberOfLocals());
    }

    void appendOSRExit(
        ExitKind kind, FormattedValue lowValue, Node* highValue, LValue failCondition, 
        NodeOrigin origin, bool isExceptionHandler = false)
    {
        return appendOSRExit(kind, lowValue, m_graph.methodOfGettingAValueProfileFor(m_node, highValue),
            failCondition, origin, isExceptionHandler);
    }
    
    void appendOSRExit(
        ExitKind kind, FormattedValue lowValue, const MethodOfGettingAValueProfile& profile, LValue failCondition, 
        NodeOrigin origin, bool isExceptionHandler = false)
    {
        if (verboseCompilationEnabled()) {
            dataLog("    OSR exit #", m_ftlState.jitCode->osrExitDescriptors.size(), " with availability: ", availabilityMap(), "\n");
            if (!m_availableRecoveries.isEmpty())
                dataLog("        Available recoveries: ", listDump(m_availableRecoveries), "\n");
        }

        DFG_ASSERT(m_graph, m_node, origin.exitOK);

        if (!isExceptionHandler
            && Options::useOSRExitFuzz()
            && canUseOSRExitFuzzing(m_graph.baselineCodeBlockFor(m_node->origin.semantic))
            && doOSRExitFuzzing()) {
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
            m_out.speculate(failCondition), kind, lowValue, profile, origin);
    }

    void blessSpeculation(CheckValue* value, ExitKind kind, FormattedValue lowValue, Node* highValue, NodeOrigin origin)
    {
        blessSpeculation(value, kind, lowValue, m_graph.methodOfGettingAValueProfileFor(m_node, highValue), origin);
    }

    void blessSpeculation(CheckValue* value, ExitKind kind, FormattedValue lowValue, const MethodOfGettingAValueProfile& profile, NodeOrigin origin)
    {
        OSRExitDescriptor* exitDescriptor = appendOSRExitDescriptor(lowValue, profile);
        
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
            
            if (Options::validateFTLOSRExitLiveness()
                && m_graph.m_plan.mode() != FTLForOSREntryMode) {

                if (availability.isDead() && m_graph.isLiveInBytecode(VirtualRegister(operand), exitOrigin))
                    DFG_CRASH(m_graph, m_node, toCString("Live bytecode local not available: operand = ", VirtualRegister(operand), ", availability = ", availability, ", origin = ", exitOrigin).data());
            }
            ExitValue exitValue = exitValueForAvailability(arguments, map, availability);
            if (exitValue.hasIndexInStackmapLocations())
                exitValue.adjustStackmapLocationsIndexByOffset(offsetOfExitArgumentsInStackmapLocations);
            exitDescriptor->m_values[i] = exitValue;
        }
        
        for (auto heapPair : availabilityMap.m_heap) {
            Node* node = heapPair.key.base();
            ExitTimeObjectMaterialization* materialization = map.get(node);
            if (!materialization)
                DFG_CRASH(m_graph, m_node, toCString("Could not find materialization for ", node, " in ", availabilityMap).data());
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
        if (!m_graph.m_ssaDominators->dominates(value.block(), m_highBlock))
            return false;
        return true;
    }
    
    void addWeakReference(JSCell* target)
    {
        m_graph.m_plan.weakReferences().addLazily(target);
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
        return m_out.weakPointer(m_graph, pointer);
    }
    
    template<typename Key>
    LValue weakPoisonedPointer(JSCell* pointer)
    {
        addWeakReference(pointer);
        return m_out.weakPoisonedPointer<Key>(m_graph, pointer);
    }

    LValue frozenPointer(FrozenValue* value)
    {
        return m_out.weakPointer(value);
    }

    LValue weakStructureID(RegisteredStructure structure)
    {
        return m_out.constInt32(structure->id());
    }
    
    LValue weakStructure(RegisteredStructure structure)
    {
        ASSERT(!!structure.get());
        return m_out.weakPointer(m_graph, structure.get());
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

    void crash()
    {
        crash(m_highBlock, m_node);
    }
    void crash(DFG::BasicBlock* block, Node* node)
    {
        BlockIndex blockIndex = block->index;
        unsigned nodeIndex = node ? node->index() : UINT_MAX;
#if ASSERT_DISABLED
        m_out.patchpoint(Void)->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams&) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                
                jit.move(CCallHelpers::TrustedImm32(blockIndex), GPRInfo::regT0);
                jit.move(CCallHelpers::TrustedImm32(nodeIndex), GPRInfo::regT1);
                if (node)
                    jit.move(CCallHelpers::TrustedImm32(node->op()), GPRInfo::regT2);
                jit.abortWithReason(FTLCrash);
            });
#else
        m_out.call(
            Void,
            m_out.constIntPtr(ftlUnreachable),
            // We don't want the CodeBlock to have a weak pointer to itself because
            // that would cause it to always get collected.
            m_out.constIntPtr(bitwise_cast<intptr_t>(codeBlock())), m_out.constInt32(blockIndex),
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
    
    HashMap<Node*, LValue> m_phis;
    
    LocalOSRAvailabilityCalculator m_availabilityCalculator;
    
    Vector<AvailableRecovery, 3> m_availableRecoveries;
    
    InPlaceAbstractState m_state;
    AbstractInterpreter<InPlaceAbstractState> m_interpreter;
    DFG::BasicBlock* m_highBlock;
    DFG::BasicBlock* m_nextHighBlock;
    LBasicBlock m_nextLowBlock;

    enum IndexMaskingMode { IndexMaskingDisabled, IndexMaskingEnabled };

    IndexMaskingMode m_indexMaskingMode;

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

