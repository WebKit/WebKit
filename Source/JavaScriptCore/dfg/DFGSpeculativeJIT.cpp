/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
#include "DFGSpeculativeJIT.h"

#if ENABLE(DFG_JIT)

#include "BinarySwitch.h"
#include "DFGAbstractInterpreterInlines.h"
#include "DFGArrayifySlowPathGenerator.h"
#include "DFGCallArrayAllocatorSlowPathGenerator.h"
#include "DFGCallCreateDirectArgumentsSlowPathGenerator.h"
#include "DFGCapabilities.h"
#include "DFGMayExit.h"
#include "DFGOSRExitFuzz.h"
#include "DFGSaneStringGetByValSlowPathGenerator.h"
#include "DFGSlowPathGenerator.h"
#include "DFGSnippetParams.h"
#include "DirectArguments.h"
#include "JITAddGenerator.h"
#include "JITBitAndGenerator.h"
#include "JITBitOrGenerator.h"
#include "JITBitXorGenerator.h"
#include "JITDivGenerator.h"
#include "JITLeftShiftGenerator.h"
#include "JITMulGenerator.h"
#include "JITRightShiftGenerator.h"
#include "JITSubGenerator.h"
#include "JSAsyncFunction.h"
#include "JSAsyncGeneratorFunction.h"
#include "JSCInlines.h"
#include "JSFixedArray.h"
#include "JSGeneratorFunction.h"
#include "JSImmutableButterfly.h"
#include "JSLexicalEnvironment.h"
#include "JSPropertyNameEnumerator.h"
#include "LinkBuffer.h"
#include "RegExpObject.h"
#include "ScopedArguments.h"
#include "ScratchRegisterAllocator.h"
#include "SuperSampler.h"
#include "TypeProfilerLog.h"
#include "WeakMapImpl.h"
#include <wtf/BitVector.h>
#include <wtf/Box.h>
#include <wtf/MathExtras.h>

namespace JSC { namespace DFG {

SpeculativeJIT::SpeculativeJIT(JITCompiler& jit)
    : m_jit(jit)
    , m_graph(m_jit.graph())
    , m_currentNode(0)
    , m_lastGeneratedNode(LastNodeType)
    , m_indexInBlock(0)
    , m_generationInfo(m_jit.graph().frameRegisterCount())
    , m_compileOkay(true)
    , m_state(m_jit.graph())
    , m_interpreter(m_jit.graph(), m_state)
    , m_stream(&jit.jitCode()->variableEventStream)
    , m_minifiedGraph(&jit.jitCode()->minifiedDFG)
{
}

SpeculativeJIT::~SpeculativeJIT()
{
}

void SpeculativeJIT::emitAllocateRawObject(GPRReg resultGPR, RegisteredStructure structure, GPRReg storageGPR, unsigned numElements, unsigned vectorLength)
{
    ASSERT(!isCopyOnWrite(structure->indexingMode()));
    IndexingType indexingType = structure->indexingType();
    bool hasIndexingHeader = hasIndexedProperties(indexingType);

    unsigned inlineCapacity = structure->inlineCapacity();
    unsigned outOfLineCapacity = structure->outOfLineCapacity();
    
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);
    GPRReg scratchGPR = scratch.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

    ASSERT(vectorLength >= numElements);
    vectorLength = Butterfly::optimalContiguousVectorLength(structure.get(), vectorLength);
    
    JITCompiler::JumpList slowCases;

    size_t size = 0;
    if (hasIndexingHeader)
        size += vectorLength * sizeof(JSValue) + sizeof(IndexingHeader);
    size += outOfLineCapacity * sizeof(JSValue);

    m_jit.move(TrustedImmPtr(nullptr), storageGPR);

    if (size) {
        if (Allocator allocator = m_jit.vm()->jsValueGigacageAuxiliarySpace.allocatorForNonVirtual(size, AllocatorForMode::AllocatorIfExists)) {
            m_jit.emitAllocate(storageGPR, JITAllocator::constant(allocator), scratchGPR, scratch2GPR, slowCases);
            
            m_jit.addPtr(
                TrustedImm32(outOfLineCapacity * sizeof(JSValue) + sizeof(IndexingHeader)),
                storageGPR);
            
            if (hasIndexingHeader)
                m_jit.store32(TrustedImm32(vectorLength), MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));
        } else
            slowCases.append(m_jit.jump());
    }

    size_t allocationSize = JSFinalObject::allocationSize(inlineCapacity);
    Allocator allocator = allocatorForNonVirtualConcurrently<JSFinalObject>(*m_jit.vm(), allocationSize, AllocatorForMode::AllocatorIfExists);
    if (allocator) {
        emitAllocateJSObject(resultGPR, JITAllocator::constant(allocator), scratchGPR, TrustedImmPtr(structure), storageGPR, scratch2GPR, slowCases);
        m_jit.emitInitializeInlineStorage(resultGPR, structure->inlineCapacity());
    } else
        slowCases.append(m_jit.jump());

    // I want a slow path that also loads out the storage pointer, and that's
    // what this custom CallArrayAllocatorSlowPathGenerator gives me. It's a lot
    // of work for a very small piece of functionality. :-/
    addSlowPathGenerator(std::make_unique<CallArrayAllocatorSlowPathGenerator>(
        slowCases, this, operationNewRawObject, resultGPR, storageGPR,
        structure, vectorLength));

    if (numElements < vectorLength) {
#if USE(JSVALUE64)
        if (hasDouble(structure->indexingType()))
            m_jit.move(TrustedImm64(bitwise_cast<int64_t>(PNaN)), scratchGPR);
        else
            m_jit.move(TrustedImm64(JSValue::encode(JSValue())), scratchGPR);
        for (unsigned i = numElements; i < vectorLength; ++i)
            m_jit.store64(scratchGPR, MacroAssembler::Address(storageGPR, sizeof(double) * i));
#else
        EncodedValueDescriptor value;
        if (hasDouble(structure->indexingType()))
            value.asInt64 = JSValue::encode(JSValue(JSValue::EncodeAsDouble, PNaN));
        else
            value.asInt64 = JSValue::encode(JSValue());
        for (unsigned i = numElements; i < vectorLength; ++i) {
            m_jit.store32(TrustedImm32(value.asBits.tag), MacroAssembler::Address(storageGPR, sizeof(double) * i + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
            m_jit.store32(TrustedImm32(value.asBits.payload), MacroAssembler::Address(storageGPR, sizeof(double) * i + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
        }
#endif
    }
    
    if (hasIndexingHeader)
        m_jit.store32(TrustedImm32(numElements), MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
    
    m_jit.emitInitializeOutOfLineStorage(storageGPR, structure->outOfLineCapacity());
    
    m_jit.mutatorFence(*m_jit.vm());
}

void SpeculativeJIT::emitGetLength(InlineCallFrame* inlineCallFrame, GPRReg lengthGPR, bool includeThis)
{
    if (inlineCallFrame && !inlineCallFrame->isVarargs())
        m_jit.move(TrustedImm32(inlineCallFrame->argumentCountIncludingThis - !includeThis), lengthGPR);
    else {
        VirtualRegister argumentCountRegister = m_jit.argumentCount(inlineCallFrame);
        m_jit.load32(JITCompiler::payloadFor(argumentCountRegister), lengthGPR);
        if (!includeThis)
            m_jit.sub32(TrustedImm32(1), lengthGPR);
    }
}

void SpeculativeJIT::emitGetLength(CodeOrigin origin, GPRReg lengthGPR, bool includeThis)
{
    emitGetLength(origin.inlineCallFrame, lengthGPR, includeThis);
}

void SpeculativeJIT::emitGetCallee(CodeOrigin origin, GPRReg calleeGPR)
{
    if (origin.inlineCallFrame) {
        if (origin.inlineCallFrame->isClosureCall) {
            m_jit.loadPtr(
                JITCompiler::addressFor(origin.inlineCallFrame->calleeRecovery.virtualRegister()),
                calleeGPR);
        } else {
            m_jit.move(
                TrustedImmPtr::weakPointer(m_jit.graph(), origin.inlineCallFrame->calleeRecovery.constant().asCell()),
                calleeGPR);
        }
    } else
        m_jit.loadPtr(JITCompiler::addressFor(CallFrameSlot::callee), calleeGPR);
}

void SpeculativeJIT::emitGetArgumentStart(CodeOrigin origin, GPRReg startGPR)
{
    m_jit.addPtr(
        TrustedImm32(
            JITCompiler::argumentsStart(origin).offset() * static_cast<int>(sizeof(Register))),
        GPRInfo::callFrameRegister, startGPR);
}

MacroAssembler::Jump SpeculativeJIT::emitOSRExitFuzzCheck()
{
    if (!Options::useOSRExitFuzz()
        || !canUseOSRExitFuzzing(m_jit.graph().baselineCodeBlockFor(m_origin.semantic))
        || !doOSRExitFuzzing())
        return MacroAssembler::Jump();
    
    MacroAssembler::Jump result;
    
    m_jit.pushToSave(GPRInfo::regT0);
    m_jit.load32(&g_numberOfOSRExitFuzzChecks, GPRInfo::regT0);
    m_jit.add32(TrustedImm32(1), GPRInfo::regT0);
    m_jit.store32(GPRInfo::regT0, &g_numberOfOSRExitFuzzChecks);
    unsigned atOrAfter = Options::fireOSRExitFuzzAtOrAfter();
    unsigned at = Options::fireOSRExitFuzzAt();
    if (at || atOrAfter) {
        unsigned threshold;
        MacroAssembler::RelationalCondition condition;
        if (atOrAfter) {
            threshold = atOrAfter;
            condition = MacroAssembler::Below;
        } else {
            threshold = at;
            condition = MacroAssembler::NotEqual;
        }
        MacroAssembler::Jump ok = m_jit.branch32(
            condition, GPRInfo::regT0, MacroAssembler::TrustedImm32(threshold));
        m_jit.popToRestore(GPRInfo::regT0);
        result = m_jit.jump();
        ok.link(&m_jit);
    }
    m_jit.popToRestore(GPRInfo::regT0);
    
    return result;
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Node* node, MacroAssembler::Jump jumpToFail)
{
    if (!m_compileOkay)
        return;
    JITCompiler::Jump fuzzJump = emitOSRExitFuzzCheck();
    if (fuzzJump.isSet()) {
        JITCompiler::JumpList jumpsToFail;
        jumpsToFail.append(fuzzJump);
        jumpsToFail.append(jumpToFail);
        m_jit.appendExitInfo(jumpsToFail);
    } else
        m_jit.appendExitInfo(jumpToFail);
    m_jit.jitCode()->appendOSRExit(OSRExit(kind, jsValueSource, m_jit.graph().methodOfGettingAValueProfileFor(m_currentNode, node), this, m_stream->size()));
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Node* node, const MacroAssembler::JumpList& jumpsToFail)
{
    if (!m_compileOkay)
        return;
    JITCompiler::Jump fuzzJump = emitOSRExitFuzzCheck();
    if (fuzzJump.isSet()) {
        JITCompiler::JumpList myJumpsToFail;
        myJumpsToFail.append(jumpsToFail);
        myJumpsToFail.append(fuzzJump);
        m_jit.appendExitInfo(myJumpsToFail);
    } else
        m_jit.appendExitInfo(jumpsToFail);
    m_jit.jitCode()->appendOSRExit(OSRExit(kind, jsValueSource, m_jit.graph().methodOfGettingAValueProfileFor(m_currentNode, node), this, m_stream->size()));
}

OSRExitJumpPlaceholder SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Node* node)
{
    if (!m_compileOkay)
        return OSRExitJumpPlaceholder();
    unsigned index = m_jit.jitCode()->osrExit.size();
    m_jit.appendExitInfo();
    m_jit.jitCode()->appendOSRExit(OSRExit(kind, jsValueSource, m_jit.graph().methodOfGettingAValueProfileFor(m_currentNode, node), this, m_stream->size()));
    return OSRExitJumpPlaceholder(index);
}

OSRExitJumpPlaceholder SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Edge nodeUse)
{
    return speculationCheck(kind, jsValueSource, nodeUse.node());
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Edge nodeUse, MacroAssembler::Jump jumpToFail)
{
    speculationCheck(kind, jsValueSource, nodeUse.node(), jumpToFail);
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Edge nodeUse, const MacroAssembler::JumpList& jumpsToFail)
{
    speculationCheck(kind, jsValueSource, nodeUse.node(), jumpsToFail);
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Node* node, MacroAssembler::Jump jumpToFail, const SpeculationRecovery& recovery)
{
    if (!m_compileOkay)
        return;
    unsigned recoveryIndex = m_jit.jitCode()->appendSpeculationRecovery(recovery);
    m_jit.appendExitInfo(jumpToFail);
    m_jit.jitCode()->appendOSRExit(OSRExit(kind, jsValueSource, m_jit.graph().methodOfGettingAValueProfileFor(m_currentNode, node), this, m_stream->size(), recoveryIndex));
}

void SpeculativeJIT::speculationCheck(ExitKind kind, JSValueSource jsValueSource, Edge nodeUse, MacroAssembler::Jump jumpToFail, const SpeculationRecovery& recovery)
{
    speculationCheck(kind, jsValueSource, nodeUse.node(), jumpToFail, recovery);
}

void SpeculativeJIT::emitInvalidationPoint(Node* node)
{
    if (!m_compileOkay)
        return;
    OSRExitCompilationInfo& info = m_jit.appendExitInfo(JITCompiler::JumpList());
    m_jit.jitCode()->appendOSRExit(OSRExit(
        UncountableInvalidation, JSValueSource(), MethodOfGettingAValueProfile(),
        this, m_stream->size()));
    info.m_replacementSource = m_jit.watchpointLabel();
    ASSERT(info.m_replacementSource.isSet());
    noResult(node);
}

void SpeculativeJIT::unreachable(Node* node)
{
    m_compileOkay = false;
    m_jit.abortWithReason(DFGUnreachableNode, node->op());
}

void SpeculativeJIT::terminateSpeculativeExecution(ExitKind kind, JSValueRegs jsValueRegs, Node* node)
{
    if (!m_compileOkay)
        return;
    speculationCheck(kind, jsValueRegs, node, m_jit.jump());
    m_compileOkay = false;
    if (verboseCompilationEnabled())
        dataLog("Bailing compilation.\n");
}

void SpeculativeJIT::terminateSpeculativeExecution(ExitKind kind, JSValueRegs jsValueRegs, Edge nodeUse)
{
    terminateSpeculativeExecution(kind, jsValueRegs, nodeUse.node());
}

void SpeculativeJIT::typeCheck(JSValueSource source, Edge edge, SpeculatedType typesPassedThrough, MacroAssembler::Jump jumpToFail, ExitKind exitKind)
{
    ASSERT(needsTypeCheck(edge, typesPassedThrough));
    m_interpreter.filter(edge, typesPassedThrough);
    speculationCheck(exitKind, source, edge.node(), jumpToFail);
}

RegisterSet SpeculativeJIT::usedRegisters()
{
    RegisterSet result;
    
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        GPRReg gpr = GPRInfo::toRegister(i);
        if (m_gprs.isInUse(gpr))
            result.set(gpr);
    }
    for (unsigned i = FPRInfo::numberOfRegisters; i--;) {
        FPRReg fpr = FPRInfo::toRegister(i);
        if (m_fprs.isInUse(fpr))
            result.set(fpr);
    }
    
    // FIXME: This is overly conservative. We could subtract out those callee-saves that we
    // actually saved.
    // https://bugs.webkit.org/show_bug.cgi?id=185686
    result.merge(RegisterSet::stubUnavailableRegisters());
    
    return result;
}

void SpeculativeJIT::addSlowPathGenerator(std::unique_ptr<SlowPathGenerator> slowPathGenerator)
{
    m_slowPathGenerators.append(WTFMove(slowPathGenerator));
}

void SpeculativeJIT::addSlowPathGeneratorLambda(Function<void()>&& lambda)
{
    m_slowPathLambdas.append(SlowPathLambda{ WTFMove(lambda), m_currentNode, static_cast<unsigned>(m_stream->size()) });
}

void SpeculativeJIT::runSlowPathGenerators(PCToCodeOriginMapBuilder& pcToCodeOriginMapBuilder)
{
    for (auto& slowPathGenerator : m_slowPathGenerators) {
        pcToCodeOriginMapBuilder.appendItem(m_jit.labelIgnoringWatchpoints(), slowPathGenerator->origin().semantic);
        slowPathGenerator->generate(this);
    }
    for (auto& slowPathLambda : m_slowPathLambdas) {
        Node* currentNode = slowPathLambda.currentNode;
        m_currentNode = currentNode;
        m_outOfLineStreamIndex = slowPathLambda.streamIndex;
        pcToCodeOriginMapBuilder.appendItem(m_jit.labelIgnoringWatchpoints(), currentNode->origin.semantic);
        slowPathLambda.generator();
        m_outOfLineStreamIndex = WTF::nullopt;
    }
}

void SpeculativeJIT::clearGenerationInfo()
{
    for (unsigned i = 0; i < m_generationInfo.size(); ++i)
        m_generationInfo[i] = GenerationInfo();
    m_gprs = RegisterBank<GPRInfo>();
    m_fprs = RegisterBank<FPRInfo>();
}

SilentRegisterSavePlan SpeculativeJIT::silentSavePlanForGPR(VirtualRegister spillMe, GPRReg source)
{
    GenerationInfo& info = generationInfoFromVirtualRegister(spillMe);
    Node* node = info.node();
    DataFormat registerFormat = info.registerFormat();
    ASSERT(registerFormat != DataFormatNone);
    ASSERT(registerFormat != DataFormatDouble);
        
    SilentSpillAction spillAction;
    SilentFillAction fillAction;
        
    if (!info.needsSpill())
        spillAction = DoNothingForSpill;
    else {
#if USE(JSVALUE64)
        ASSERT(info.gpr() == source);
        if (registerFormat == DataFormatInt32)
            spillAction = Store32Payload;
        else if (registerFormat == DataFormatCell || registerFormat == DataFormatStorage)
            spillAction = StorePtr;
        else if (registerFormat == DataFormatInt52 || registerFormat == DataFormatStrictInt52)
            spillAction = Store64;
        else {
            ASSERT(registerFormat & DataFormatJS);
            spillAction = Store64;
        }
#elif USE(JSVALUE32_64)
        if (registerFormat & DataFormatJS) {
            ASSERT(info.tagGPR() == source || info.payloadGPR() == source);
            spillAction = source == info.tagGPR() ? Store32Tag : Store32Payload;
        } else {
            ASSERT(info.gpr() == source);
            spillAction = Store32Payload;
        }
#endif
    }
        
    if (registerFormat == DataFormatInt32) {
        ASSERT(info.gpr() == source);
        ASSERT(isJSInt32(info.registerFormat()));
        if (node->hasConstant()) {
            ASSERT(node->isInt32Constant());
            fillAction = SetInt32Constant;
        } else
            fillAction = Load32Payload;
    } else if (registerFormat == DataFormatBoolean) {
#if USE(JSVALUE64)
        RELEASE_ASSERT_NOT_REACHED();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
        fillAction = DoNothingForFill;
#endif
#elif USE(JSVALUE32_64)
        ASSERT(info.gpr() == source);
        if (node->hasConstant()) {
            ASSERT(node->isBooleanConstant());
            fillAction = SetBooleanConstant;
        } else
            fillAction = Load32Payload;
#endif
    } else if (registerFormat == DataFormatCell) {
        ASSERT(info.gpr() == source);
        if (node->hasConstant()) {
            DFG_ASSERT(m_jit.graph(), m_currentNode, node->isCellConstant());
            node->asCell(); // To get the assertion.
            fillAction = SetCellConstant;
        } else {
#if USE(JSVALUE64)
            fillAction = LoadPtr;
#else
            fillAction = Load32Payload;
#endif
        }
    } else if (registerFormat == DataFormatStorage) {
        ASSERT(info.gpr() == source);
        fillAction = LoadPtr;
    } else if (registerFormat == DataFormatInt52) {
        if (node->hasConstant())
            fillAction = SetInt52Constant;
        else if (info.spillFormat() == DataFormatInt52)
            fillAction = Load64;
        else if (info.spillFormat() == DataFormatStrictInt52)
            fillAction = Load64ShiftInt52Left;
        else if (info.spillFormat() == DataFormatNone)
            fillAction = Load64;
        else {
            RELEASE_ASSERT_NOT_REACHED();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
            fillAction = Load64; // Make GCC happy.
#endif
        }
    } else if (registerFormat == DataFormatStrictInt52) {
        if (node->hasConstant())
            fillAction = SetStrictInt52Constant;
        else if (info.spillFormat() == DataFormatInt52)
            fillAction = Load64ShiftInt52Right;
        else if (info.spillFormat() == DataFormatStrictInt52)
            fillAction = Load64;
        else if (info.spillFormat() == DataFormatNone)
            fillAction = Load64;
        else {
            RELEASE_ASSERT_NOT_REACHED();
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
            fillAction = Load64; // Make GCC happy.
#endif
        }
    } else {
        ASSERT(registerFormat & DataFormatJS);
#if USE(JSVALUE64)
        ASSERT(info.gpr() == source);
        if (node->hasConstant()) {
            if (node->isCellConstant())
                fillAction = SetTrustedJSConstant;
            else
                fillAction = SetJSConstant;
        } else if (info.spillFormat() == DataFormatInt32) {
            ASSERT(registerFormat == DataFormatJSInt32);
            fillAction = Load32PayloadBoxInt;
        } else
            fillAction = Load64;
#else
        ASSERT(info.tagGPR() == source || info.payloadGPR() == source);
        if (node->hasConstant())
            fillAction = info.tagGPR() == source ? SetJSConstantTag : SetJSConstantPayload;
        else if (info.payloadGPR() == source)
            fillAction = Load32Payload;
        else { // Fill the Tag
            switch (info.spillFormat()) {
            case DataFormatInt32:
                ASSERT(registerFormat == DataFormatJSInt32);
                fillAction = SetInt32Tag;
                break;
            case DataFormatCell:
                ASSERT(registerFormat == DataFormatJSCell);
                fillAction = SetCellTag;
                break;
            case DataFormatBoolean:
                ASSERT(registerFormat == DataFormatJSBoolean);
                fillAction = SetBooleanTag;
                break;
            default:
                fillAction = Load32Tag;
                break;
            }
        }
#endif
    }
        
    return SilentRegisterSavePlan(spillAction, fillAction, node, source);
}
    
SilentRegisterSavePlan SpeculativeJIT::silentSavePlanForFPR(VirtualRegister spillMe, FPRReg source)
{
    GenerationInfo& info = generationInfoFromVirtualRegister(spillMe);
    Node* node = info.node();
    ASSERT(info.registerFormat() == DataFormatDouble);

    SilentSpillAction spillAction;
    SilentFillAction fillAction;
        
    if (!info.needsSpill())
        spillAction = DoNothingForSpill;
    else {
        ASSERT(!node->hasConstant());
        ASSERT(info.spillFormat() == DataFormatNone);
        ASSERT(info.fpr() == source);
        spillAction = StoreDouble;
    }
        
#if USE(JSVALUE64)
    if (node->hasConstant()) {
        node->asNumber(); // To get the assertion.
        fillAction = SetDoubleConstant;
    } else {
        ASSERT(info.spillFormat() == DataFormatNone || info.spillFormat() == DataFormatDouble);
        fillAction = LoadDouble;
    }
#elif USE(JSVALUE32_64)
    ASSERT(info.registerFormat() == DataFormatDouble);
    if (node->hasConstant()) {
        node->asNumber(); // To get the assertion.
        fillAction = SetDoubleConstant;
    } else
        fillAction = LoadDouble;
#endif

    return SilentRegisterSavePlan(spillAction, fillAction, node, source);
}
    
void SpeculativeJIT::silentSpill(const SilentRegisterSavePlan& plan)
{
    switch (plan.spillAction()) {
    case DoNothingForSpill:
        break;
    case Store32Tag:
        m_jit.store32(plan.gpr(), JITCompiler::tagFor(plan.node()->virtualRegister()));
        break;
    case Store32Payload:
        m_jit.store32(plan.gpr(), JITCompiler::payloadFor(plan.node()->virtualRegister()));
        break;
    case StorePtr:
        m_jit.storePtr(plan.gpr(), JITCompiler::addressFor(plan.node()->virtualRegister()));
        break;
#if USE(JSVALUE64)
    case Store64:
        m_jit.store64(plan.gpr(), JITCompiler::addressFor(plan.node()->virtualRegister()));
        break;
#endif
    case StoreDouble:
        m_jit.storeDouble(plan.fpr(), JITCompiler::addressFor(plan.node()->virtualRegister()));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}
    
void SpeculativeJIT::silentFill(const SilentRegisterSavePlan& plan)
{
    switch (plan.fillAction()) {
    case DoNothingForFill:
        break;
    case SetInt32Constant:
        m_jit.move(Imm32(plan.node()->asInt32()), plan.gpr());
        break;
#if USE(JSVALUE64)
    case SetInt52Constant:
        m_jit.move(Imm64(plan.node()->asAnyInt() << JSValue::int52ShiftAmount), plan.gpr());
        break;
    case SetStrictInt52Constant:
        m_jit.move(Imm64(plan.node()->asAnyInt()), plan.gpr());
        break;
#endif // USE(JSVALUE64)
    case SetBooleanConstant:
        m_jit.move(TrustedImm32(plan.node()->asBoolean()), plan.gpr());
        break;
    case SetCellConstant:
        ASSERT(plan.node()->constant()->value().isCell());
        m_jit.move(TrustedImmPtr(plan.node()->constant()), plan.gpr());
        break;
#if USE(JSVALUE64)
    case SetTrustedJSConstant:
        m_jit.move(valueOfJSConstantAsImm64(plan.node()).asTrustedImm64(), plan.gpr());
        break;
    case SetJSConstant:
        m_jit.move(valueOfJSConstantAsImm64(plan.node()), plan.gpr());
        break;
    case SetDoubleConstant:
        m_jit.moveDouble(Imm64(reinterpretDoubleToInt64(plan.node()->asNumber())), plan.fpr());
        break;
    case Load32PayloadBoxInt:
        m_jit.load32(JITCompiler::payloadFor(plan.node()->virtualRegister()), plan.gpr());
        m_jit.or64(GPRInfo::tagTypeNumberRegister, plan.gpr());
        break;
    case Load32PayloadConvertToInt52:
        m_jit.load32(JITCompiler::payloadFor(plan.node()->virtualRegister()), plan.gpr());
        m_jit.signExtend32ToPtr(plan.gpr(), plan.gpr());
        m_jit.lshift64(TrustedImm32(JSValue::int52ShiftAmount), plan.gpr());
        break;
    case Load32PayloadSignExtend:
        m_jit.load32(JITCompiler::payloadFor(plan.node()->virtualRegister()), plan.gpr());
        m_jit.signExtend32ToPtr(plan.gpr(), plan.gpr());
        break;
#else
    case SetJSConstantTag:
        m_jit.move(Imm32(plan.node()->asJSValue().tag()), plan.gpr());
        break;
    case SetJSConstantPayload:
        m_jit.move(Imm32(plan.node()->asJSValue().payload()), plan.gpr());
        break;
    case SetInt32Tag:
        m_jit.move(TrustedImm32(JSValue::Int32Tag), plan.gpr());
        break;
    case SetCellTag:
        m_jit.move(TrustedImm32(JSValue::CellTag), plan.gpr());
        break;
    case SetBooleanTag:
        m_jit.move(TrustedImm32(JSValue::BooleanTag), plan.gpr());
        break;
    case SetDoubleConstant:
        m_jit.loadDouble(TrustedImmPtr(m_jit.addressOfDoubleConstant(plan.node())), plan.fpr());
        break;
#endif
    case Load32Tag:
        m_jit.load32(JITCompiler::tagFor(plan.node()->virtualRegister()), plan.gpr());
        break;
    case Load32Payload:
        m_jit.load32(JITCompiler::payloadFor(plan.node()->virtualRegister()), plan.gpr());
        break;
    case LoadPtr:
        m_jit.loadPtr(JITCompiler::addressFor(plan.node()->virtualRegister()), plan.gpr());
        break;
#if USE(JSVALUE64)
    case Load64:
        m_jit.load64(JITCompiler::addressFor(plan.node()->virtualRegister()), plan.gpr());
        break;
    case Load64ShiftInt52Right:
        m_jit.load64(JITCompiler::addressFor(plan.node()->virtualRegister()), plan.gpr());
        m_jit.rshift64(TrustedImm32(JSValue::int52ShiftAmount), plan.gpr());
        break;
    case Load64ShiftInt52Left:
        m_jit.load64(JITCompiler::addressFor(plan.node()->virtualRegister()), plan.gpr());
        m_jit.lshift64(TrustedImm32(JSValue::int52ShiftAmount), plan.gpr());
        break;
#endif
    case LoadDouble:
        m_jit.loadDouble(JITCompiler::addressFor(plan.node()->virtualRegister()), plan.fpr());
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

JITCompiler::JumpList SpeculativeJIT::jumpSlowForUnwantedArrayMode(GPRReg tempGPR, ArrayMode arrayMode)
{
    JITCompiler::JumpList result;

    IndexingType indexingModeMask = IsArray | IndexingShapeMask;
    if (arrayMode.action() == Array::Write)
        indexingModeMask |= CopyOnWrite;

    switch (arrayMode.type()) {
    case Array::Int32:
    case Array::Double:
    case Array::Contiguous:
    case Array::Undecided:
    case Array::ArrayStorage: {
        IndexingType shape = arrayMode.shapeMask();
        switch (arrayMode.arrayClass()) {
        case Array::OriginalArray:
        case Array::OriginalCopyOnWriteArray:
            RELEASE_ASSERT_NOT_REACHED();
            return result;

        case Array::Array:
            m_jit.and32(TrustedImm32(indexingModeMask), tempGPR);
            result.append(m_jit.branch32(
                MacroAssembler::NotEqual, tempGPR, TrustedImm32(IsArray | shape)));
            return result;

        case Array::NonArray:
        case Array::OriginalNonArray:
            m_jit.and32(TrustedImm32(indexingModeMask), tempGPR);
            result.append(m_jit.branch32(
                MacroAssembler::NotEqual, tempGPR, TrustedImm32(shape)));
            return result;

        case Array::PossiblyArray:
            m_jit.and32(TrustedImm32(indexingModeMask & ~IsArray), tempGPR);
            result.append(m_jit.branch32(MacroAssembler::NotEqual, tempGPR, TrustedImm32(shape)));
            return result;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return result;
    }

    case Array::SlowPutArrayStorage: {
        ASSERT(!arrayMode.isJSArrayWithOriginalStructure());

        switch (arrayMode.arrayClass()) {
        case Array::OriginalArray:
        case Array::OriginalCopyOnWriteArray:
            RELEASE_ASSERT_NOT_REACHED();
            return result;

        case Array::Array:
            result.append(
                m_jit.branchTest32(
                    MacroAssembler::Zero, tempGPR, MacroAssembler::TrustedImm32(IsArray)));
            break;

        case Array::NonArray:
        case Array::OriginalNonArray:
            result.append(
                m_jit.branchTest32(
                    MacroAssembler::NonZero, tempGPR, MacroAssembler::TrustedImm32(IsArray)));
            break;

        case Array::PossiblyArray:
            break;
        }

        m_jit.and32(TrustedImm32(IndexingShapeMask), tempGPR);
        m_jit.sub32(TrustedImm32(ArrayStorageShape), tempGPR);
        result.append(
            m_jit.branch32(
                MacroAssembler::Above, tempGPR,
                TrustedImm32(SlowPutArrayStorageShape - ArrayStorageShape)));
        return result;
    }
    default:
        CRASH();
        break;
    }
    
    return result;
}

void SpeculativeJIT::checkArray(Node* node)
{
    ASSERT(node->arrayMode().isSpecific());
    ASSERT(!node->arrayMode().doesConversion());
    
    SpeculateCellOperand base(this, node->child1());
    GPRReg baseReg = base.gpr();
    
    if (node->arrayMode().alreadyChecked(m_jit.graph(), node, m_state.forNode(node->child1()))) {
        noResult(m_currentNode);
        return;
    }
    
    switch (node->arrayMode().type()) {
    case Array::AnyTypedArray:
    case Array::String:
        RELEASE_ASSERT_NOT_REACHED(); // Should have been a Phantom(String:)
        return;
    case Array::Int32:
    case Array::Double:
    case Array::Contiguous:
    case Array::Undecided:
    case Array::ArrayStorage:
    case Array::SlowPutArrayStorage: {
        GPRTemporary temp(this);
        GPRReg tempGPR = temp.gpr();
        m_jit.load8(MacroAssembler::Address(baseReg, JSCell::indexingTypeAndMiscOffset()), tempGPR);
        speculationCheck(
            BadIndexingType, JSValueSource::unboxedCell(baseReg), 0,
            jumpSlowForUnwantedArrayMode(tempGPR, node->arrayMode()));
        
        noResult(m_currentNode);
        return;
    }
    case Array::DirectArguments:
        speculateCellTypeWithoutTypeFiltering(node->child1(), baseReg, DirectArgumentsType);
        noResult(m_currentNode);
        return;
    case Array::ScopedArguments:
        speculateCellTypeWithoutTypeFiltering(node->child1(), baseReg, ScopedArgumentsType);
        noResult(m_currentNode);
        return;
    default:
        speculateCellTypeWithoutTypeFiltering(
            node->child1(), baseReg,
            typeForTypedArrayType(node->arrayMode().typedArrayType()));
        noResult(m_currentNode);
        return;
    }
}

void SpeculativeJIT::arrayify(Node* node, GPRReg baseReg, GPRReg propertyReg)
{
    ASSERT(node->arrayMode().doesConversion());
    
    GPRTemporary temp(this);
    GPRTemporary structure;
    GPRReg tempGPR = temp.gpr();
    GPRReg structureGPR = InvalidGPRReg;
    
    if (node->op() != ArrayifyToStructure) {
        GPRTemporary realStructure(this);
        structure.adopt(realStructure);
        structureGPR = structure.gpr();
    }
        
    // We can skip all that comes next if we already have array storage.
    MacroAssembler::JumpList slowPath;
    
    if (node->op() == ArrayifyToStructure) {
        ASSERT(!isCopyOnWrite(node->structure()->indexingMode()));
        ASSERT((node->structure()->indexingType() & IndexingShapeMask) == node->arrayMode().shapeMask());
        slowPath.append(m_jit.branchWeakStructure(
            JITCompiler::NotEqual,
            JITCompiler::Address(baseReg, JSCell::structureIDOffset()),
            node->structure()));
    } else {
        m_jit.load8(
            MacroAssembler::Address(baseReg, JSCell::indexingTypeAndMiscOffset()), tempGPR);
        
        slowPath.append(jumpSlowForUnwantedArrayMode(tempGPR, node->arrayMode()));
    }
    
    addSlowPathGenerator(std::make_unique<ArrayifySlowPathGenerator>(
        slowPath, this, node, baseReg, propertyReg, tempGPR, structureGPR));
    
    noResult(m_currentNode);
}

void SpeculativeJIT::arrayify(Node* node)
{
    ASSERT(node->arrayMode().isSpecific());
    
    SpeculateCellOperand base(this, node->child1());
    
    if (!node->child2()) {
        arrayify(node, base.gpr(), InvalidGPRReg);
        return;
    }
    
    SpeculateInt32Operand property(this, node->child2());
    
    arrayify(node, base.gpr(), property.gpr());
}

GPRReg SpeculativeJIT::fillStorage(Edge edge)
{
    VirtualRegister virtualRegister = edge->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
    
    switch (info.registerFormat()) {
    case DataFormatNone: {
        if (info.spillFormat() == DataFormatStorage) {
            GPRReg gpr = allocate();
            m_gprs.retain(gpr, virtualRegister, SpillOrderSpilled);
            m_jit.loadPtr(JITCompiler::addressFor(virtualRegister), gpr);
            info.fillStorage(*m_stream, gpr);
            return gpr;
        }
        
        // Must be a cell; fill it as a cell and then return the pointer.
        return fillSpeculateCell(edge);
    }
        
    case DataFormatStorage: {
        GPRReg gpr = info.gpr();
        m_gprs.lock(gpr);
        return gpr;
    }
        
    default:
        return fillSpeculateCell(edge);
    }
}

void SpeculativeJIT::useChildren(Node* node)
{
    if (node->flags() & NodeHasVarArgs) {
        for (unsigned childIdx = node->firstChild(); childIdx < node->firstChild() + node->numChildren(); childIdx++) {
            if (!!m_jit.graph().m_varArgChildren[childIdx])
                use(m_jit.graph().m_varArgChildren[childIdx]);
        }
    } else {
        Edge child1 = node->child1();
        if (!child1) {
            ASSERT(!node->child2() && !node->child3());
            return;
        }
        use(child1);
        
        Edge child2 = node->child2();
        if (!child2) {
            ASSERT(!node->child3());
            return;
        }
        use(child2);
        
        Edge child3 = node->child3();
        if (!child3)
            return;
        use(child3);
    }
}

void SpeculativeJIT::compileGetById(Node* node, AccessType accessType)
{
    ASSERT(accessType == AccessType::Get || accessType == AccessType::GetDirect || accessType == AccessType::TryGet);

    switch (node->child1().useKind()) {
    case CellUse: {
        SpeculateCellOperand base(this, node->child1());
        JSValueRegsTemporary result(this, Reuse, base);

        JSValueRegs baseRegs = JSValueRegs::payloadOnly(base.gpr());
        JSValueRegs resultRegs = result.regs();

        base.use();

        cachedGetById(node->origin.semantic, baseRegs, resultRegs, node->identifierNumber(), JITCompiler::Jump(), NeedToSpill, accessType);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        break;
    }

    case UntypedUse: {
        JSValueOperand base(this, node->child1());
        JSValueRegsTemporary result(this, Reuse, base);

        JSValueRegs baseRegs = base.jsValueRegs();
        JSValueRegs resultRegs = result.regs();

        base.use();

        JITCompiler::Jump notCell = m_jit.branchIfNotCell(baseRegs);

        cachedGetById(node->origin.semantic, baseRegs, resultRegs, node->identifierNumber(), notCell, NeedToSpill, accessType);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        break;
    }

    default:
        DFG_CRASH(m_jit.graph(), node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::compileGetByIdFlush(Node* node, AccessType accessType)
{
    switch (node->child1().useKind()) {
    case CellUse: {
        SpeculateCellOperand base(this, node->child1());
        JSValueRegs baseRegs = JSValueRegs::payloadOnly(base.gpr());

        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        base.use();

        flushRegisters();

        cachedGetById(node->origin.semantic, baseRegs, resultRegs, node->identifierNumber(), JITCompiler::Jump(), DontSpill, accessType);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        break;
    }

    case UntypedUse: {
        JSValueOperand base(this, node->child1());
        JSValueRegs baseRegs = base.jsValueRegs();

        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();

        base.use();

        flushRegisters();

        JITCompiler::Jump notCell = m_jit.branchIfNotCell(baseRegs);

        cachedGetById(node->origin.semantic, baseRegs, resultRegs, node->identifierNumber(), notCell, DontSpill, accessType);

        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        break;
    }

    default:
        DFG_CRASH(m_jit.graph(), node, "Bad use kind");
        break;
    }
}

void SpeculativeJIT::compileInById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, base, PayloadWord);

    GPRReg baseGPR = base.gpr();
    JSValueRegs resultRegs = result.regs();

    base.use();

    CodeOrigin codeOrigin = node->origin.semantic;
    CallSiteIndex callSite = m_jit.recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream->size());
    RegisterSet usedRegisters = this->usedRegisters();
    JITInByIdGenerator gen(
        m_jit.codeBlock(), codeOrigin, callSite, usedRegisters, identifierUID(node->identifierNumber()),
        JSValueRegs::payloadOnly(baseGPR), resultRegs);
    gen.generateFastPath(m_jit);

    auto slowPath = slowPathCall(
        gen.slowPathJump(), this, operationInByIdOptimize,
        NeedToSpill, ExceptionCheckRequirement::CheckNeeded,
        resultRegs, gen.stubInfo(), CCallHelpers::CellValue(baseGPR), identifierUID(node->identifierNumber()));

    m_jit.addInById(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));

    blessedBooleanResult(resultRegs.payloadGPR(), node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileInByVal(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand key(this, node->child2());

    GPRReg baseGPR = base.gpr();
    JSValueRegs regs = key.jsValueRegs();

    base.use();
    key.use();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationInByVal, resultRegs, baseGPR, regs);
    m_jit.exceptionCheck();
    blessedBooleanResult(resultRegs.payloadGPR(), node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileDeleteById(Node* node)
{
    JSValueOperand value(this, node->child1());
    GPRFlushedCallResult result(this);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    value.use();

    flushRegisters();
    callOperation(operationDeleteById, resultGPR, valueRegs, identifierUID(node->identifierNumber()));
    m_jit.exceptionCheck();

    unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileDeleteByVal(Node* node)
{
    JSValueOperand base(this, node->child1());
    JSValueOperand key(this, node->child2());
    GPRFlushedCallResult result(this);

    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueRegs keyRegs = key.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    base.use();
    key.use();

    flushRegisters();
    callOperation(operationDeleteByVal, resultGPR, baseRegs, keyRegs);
    m_jit.exceptionCheck();

    unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compilePushWithScope(Node* node)
{
    SpeculateCellOperand currentScope(this, node->child1());
    GPRReg currentScopeGPR = currentScope.gpr();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    auto objectEdge = node->child2();
    if (objectEdge.useKind() == ObjectUse) {
        SpeculateCellOperand object(this, objectEdge);
        GPRReg objectGPR = object.gpr();
        speculateObject(objectEdge, objectGPR);

        flushRegisters();
        callOperation(operationPushWithScopeObject, resultGPR, currentScopeGPR, objectGPR);
        // No exception check here as we did not have to call toObject().
    } else {
        ASSERT(objectEdge.useKind() == UntypedUse);
        JSValueOperand object(this, objectEdge);
        JSValueRegs objectRegs = object.jsValueRegs();

        flushRegisters();
        callOperation(operationPushWithScope, resultGPR, currentScopeGPR, objectRegs);
        m_jit.exceptionCheck();
    }
    
    cellResult(resultGPR, node);
}

bool SpeculativeJIT::nonSpeculativeStrictEq(Node* node, bool invert)
{
    unsigned branchIndexInBlock = detectPeepHoleBranch();
    if (branchIndexInBlock != UINT_MAX) {
        Node* branchNode = m_block->at(branchIndexInBlock);

        ASSERT(node->adjustedRefCount() == 1);
        
        nonSpeculativePeepholeStrictEq(node, branchNode, invert);
    
        m_indexInBlock = branchIndexInBlock;
        m_currentNode = branchNode;
        
        return true;
    }
    
    nonSpeculativeNonPeepholeStrictEq(node, invert);
    
    return false;
}

static const char* dataFormatString(DataFormat format)
{
    // These values correspond to the DataFormat enum.
    const char* strings[] = {
        "[  ]",
        "[ i]",
        "[ d]",
        "[ c]",
        "Err!",
        "Err!",
        "Err!",
        "Err!",
        "[J ]",
        "[Ji]",
        "[Jd]",
        "[Jc]",
        "Err!",
        "Err!",
        "Err!",
        "Err!",
    };
    return strings[format];
}

void SpeculativeJIT::dump(const char* label)
{
    if (label)
        dataLogF("<%s>\n", label);

    dataLogF("  gprs:\n");
    m_gprs.dump();
    dataLogF("  fprs:\n");
    m_fprs.dump();
    dataLogF("  VirtualRegisters:\n");
    for (unsigned i = 0; i < m_generationInfo.size(); ++i) {
        GenerationInfo& info = m_generationInfo[i];
        if (info.alive())
            dataLogF("    % 3d:%s%s", i, dataFormatString(info.registerFormat()), dataFormatString(info.spillFormat()));
        else
            dataLogF("    % 3d:[__][__]", i);
        if (info.registerFormat() == DataFormatDouble)
            dataLogF(":fpr%d\n", info.fpr());
        else if (info.registerFormat() != DataFormatNone
#if USE(JSVALUE32_64)
            && !(info.registerFormat() & DataFormatJS)
#endif
            ) {
            ASSERT(info.gpr() != InvalidGPRReg);
            dataLogF(":%s\n", GPRInfo::debugName(info.gpr()));
        } else
            dataLogF("\n");
    }
    if (label)
        dataLogF("</%s>\n", label);
}

GPRTemporary::GPRTemporary()
    : m_jit(0)
    , m_gpr(InvalidGPRReg)
{
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    m_gpr = m_jit->allocate();
}

GPRTemporary::GPRTemporary(SpeculativeJIT* jit, GPRReg specific)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    m_gpr = m_jit->allocate(specific);
}

#if USE(JSVALUE32_64)
GPRTemporary::GPRTemporary(
    SpeculativeJIT* jit, ReuseTag, JSValueOperand& op1, WhichValueWord which)
    : m_jit(jit)
    , m_gpr(InvalidGPRReg)
{
    if (!op1.isDouble() && m_jit->canReuse(op1.node()))
        m_gpr = m_jit->reuse(op1.gpr(which));
    else
        m_gpr = m_jit->allocate();
}
#else // USE(JSVALUE32_64)
GPRTemporary::GPRTemporary(SpeculativeJIT* jit, ReuseTag, JSValueOperand& op1, WhichValueWord)
    : GPRTemporary(jit, Reuse, op1)
{
}
#endif

JSValueRegsTemporary::JSValueRegsTemporary() { }

JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit)
#if USE(JSVALUE64)
    : m_gpr(jit)
#else
    : m_payloadGPR(jit)
    , m_tagGPR(jit)
#endif
{
}

#if USE(JSVALUE64)
template<typename T>
JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit, ReuseTag, T& operand, WhichValueWord)
    : m_gpr(jit, Reuse, operand)
{
}
#else
template<typename T>
JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit, ReuseTag, T& operand, WhichValueWord resultWord)
{
    if (resultWord == PayloadWord) {
        m_payloadGPR = GPRTemporary(jit, Reuse, operand);
        m_tagGPR = GPRTemporary(jit);
    } else {
        m_payloadGPR = GPRTemporary(jit);
        m_tagGPR = GPRTemporary(jit, Reuse, operand);
    }
}
#endif

#if USE(JSVALUE64)
JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit, ReuseTag, JSValueOperand& operand)
{
    m_gpr = GPRTemporary(jit, Reuse, operand);
}
#else
JSValueRegsTemporary::JSValueRegsTemporary(SpeculativeJIT* jit, ReuseTag, JSValueOperand& operand)
{
    if (jit->canReuse(operand.node())) {
        m_payloadGPR = GPRTemporary(jit, Reuse, operand, PayloadWord);
        m_tagGPR = GPRTemporary(jit, Reuse, operand, TagWord);
    } else {
        m_payloadGPR = GPRTemporary(jit);
        m_tagGPR = GPRTemporary(jit);
    }
}
#endif

JSValueRegsTemporary::~JSValueRegsTemporary() { }

JSValueRegs JSValueRegsTemporary::regs()
{
#if USE(JSVALUE64)
    return JSValueRegs(m_gpr.gpr());
#else
    return JSValueRegs(m_tagGPR.gpr(), m_payloadGPR.gpr());
#endif
}

void GPRTemporary::adopt(GPRTemporary& other)
{
    ASSERT(!m_jit);
    ASSERT(m_gpr == InvalidGPRReg);
    ASSERT(other.m_jit);
    ASSERT(other.m_gpr != InvalidGPRReg);
    m_jit = other.m_jit;
    m_gpr = other.m_gpr;
    other.m_jit = 0;
    other.m_gpr = InvalidGPRReg;
}

FPRTemporary::FPRTemporary(FPRTemporary&& other)
{
    ASSERT(other.m_jit);
    ASSERT(other.m_fpr != InvalidFPRReg);
    m_jit = other.m_jit;
    m_fpr = other.m_fpr;

    other.m_jit = nullptr;
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit, SpeculateDoubleOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.node()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

FPRTemporary::FPRTemporary(SpeculativeJIT* jit, SpeculateDoubleOperand& op1, SpeculateDoubleOperand& op2)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (m_jit->canReuse(op1.node()))
        m_fpr = m_jit->reuse(op1.fpr());
    else if (m_jit->canReuse(op2.node()))
        m_fpr = m_jit->reuse(op2.fpr());
    else if (m_jit->canReuse(op1.node(), op2.node()) && op1.fpr() == op2.fpr())
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}

#if USE(JSVALUE32_64)
FPRTemporary::FPRTemporary(SpeculativeJIT* jit, JSValueOperand& op1)
    : m_jit(jit)
    , m_fpr(InvalidFPRReg)
{
    if (op1.isDouble() && m_jit->canReuse(op1.node()))
        m_fpr = m_jit->reuse(op1.fpr());
    else
        m_fpr = m_jit->fprAllocate();
}
#endif

void SpeculativeJIT::compilePeepHoleDoubleBranch(Node* node, Node* branchNode, JITCompiler::DoubleCondition condition)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    if (taken == nextBlock()) {
        condition = MacroAssembler::invert(condition);
        std::swap(taken, notTaken);
    }

    SpeculateDoubleOperand op1(this, node->child1());
    SpeculateDoubleOperand op2(this, node->child2());
    
    branchDouble(condition, op1.fpr(), op2.fpr(), taken);
    jump(notTaken);
}

void SpeculativeJIT::compilePeepHoleObjectEquality(Node* node, Node* branchNode)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    MacroAssembler::RelationalCondition condition = MacroAssembler::Equal;
    
    if (taken == nextBlock()) {
        condition = MacroAssembler::NotEqual;
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    SpeculateCellOperand op1(this, node->child1());
    SpeculateCellOperand op2(this, node->child2());
    
    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    
    if (masqueradesAsUndefinedWatchpointIsStillValid()) {
        if (m_state.forNode(node->child1()).m_type & ~SpecObject) {
            speculationCheck(
                BadType, JSValueSource::unboxedCell(op1GPR), node->child1(), m_jit.branchIfNotObject(op1GPR));
        }
        if (m_state.forNode(node->child2()).m_type & ~SpecObject) {
            speculationCheck(
                BadType, JSValueSource::unboxedCell(op2GPR), node->child2(), m_jit.branchIfNotObject(op2GPR));
        }
    } else {
        if (m_state.forNode(node->child1()).m_type & ~SpecObject) {
            speculationCheck(
                BadType, JSValueSource::unboxedCell(op1GPR), node->child1(),
                m_jit.branchIfNotObject(op1GPR));
        }
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node->child1(),
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(op1GPR, JSCell::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));

        if (m_state.forNode(node->child2()).m_type & ~SpecObject) {
            speculationCheck(
                BadType, JSValueSource::unboxedCell(op2GPR), node->child2(),
                m_jit.branchIfNotObject(op2GPR));
        }
        speculationCheck(BadType, JSValueSource::unboxedCell(op2GPR), node->child2(),
            m_jit.branchTest8(
                MacroAssembler::NonZero, 
                MacroAssembler::Address(op2GPR, JSCell::typeInfoFlagsOffset()), 
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }

    branchPtr(condition, op1GPR, op2GPR, taken);
    jump(notTaken);
}

void SpeculativeJIT::compilePeepHoleBooleanBranch(Node* node, Node* branchNode, JITCompiler::RelationalCondition condition)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        condition = JITCompiler::invert(condition);
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    if (node->child1()->isInt32Constant()) {
        int32_t imm = node->child1()->asInt32();
        SpeculateBooleanOperand op2(this, node->child2());
        branch32(condition, JITCompiler::Imm32(imm), op2.gpr(), taken);
    } else if (node->child2()->isInt32Constant()) {
        SpeculateBooleanOperand op1(this, node->child1());
        int32_t imm = node->child2()->asInt32();
        branch32(condition, op1.gpr(), JITCompiler::Imm32(imm), taken);
    } else {
        SpeculateBooleanOperand op1(this, node->child1());
        SpeculateBooleanOperand op2(this, node->child2());
        branch32(condition, op1.gpr(), op2.gpr(), taken);
    }

    jump(notTaken);
}

void SpeculativeJIT::compileStringSlice(Node* node)
{
    SpeculateCellOperand string(this, node->child1());

    GPRReg stringGPR = string.gpr();

    speculateString(node->child1(), stringGPR);

    SpeculateInt32Operand start(this, node->child2());
    GPRReg startGPR = start.gpr();

    Optional<SpeculateInt32Operand> end;
    Optional<GPRReg> endGPR;
    if (node->child3()) {
        end.emplace(this, node->child3());
        endGPR.emplace(end->gpr());
    }

    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();

    m_jit.loadPtr(CCallHelpers::Address(stringGPR, JSString::offsetOfValue()), tempGPR);
    auto isRope = m_jit.branchIfRopeStringImpl(tempGPR);

    GPRTemporary temp2(this);
    GPRTemporary startIndex(this);

    GPRReg temp2GPR = temp2.gpr();
    GPRReg startIndexGPR = startIndex.gpr();
    {
        m_jit.load32(MacroAssembler::Address(tempGPR, StringImpl::lengthMemoryOffset()), temp2GPR);

        emitPopulateSliceIndex(node->child2(), startGPR, temp2GPR, startIndexGPR);

        if (node->child3())
            emitPopulateSliceIndex(node->child3(), endGPR.value(), temp2GPR, tempGPR);
        else
            m_jit.move(temp2GPR, tempGPR);
    }

    CCallHelpers::JumpList doneCases;
    CCallHelpers::JumpList slowCases;

    auto nonEmptyCase = m_jit.branch32(MacroAssembler::Below, startIndexGPR, tempGPR);
    m_jit.move(TrustedImmPtr::weakPointer(m_jit.graph(), jsEmptyString(&vm())), tempGPR);
    doneCases.append(m_jit.jump());

    nonEmptyCase.link(&m_jit);
    m_jit.sub32(startIndexGPR, tempGPR); // the size of the sliced string.
    slowCases.append(m_jit.branch32(MacroAssembler::NotEqual, tempGPR, TrustedImm32(1)));

    // Refill StringImpl* here.
    m_jit.loadPtr(MacroAssembler::Address(stringGPR, JSString::offsetOfValue()), temp2GPR);
    m_jit.loadPtr(MacroAssembler::Address(temp2GPR, StringImpl::dataOffset()), tempGPR);

    // Load the character into scratchReg
    m_jit.zeroExtend32ToPtr(startIndexGPR, startIndexGPR);
    auto is16Bit = m_jit.branchTest32(MacroAssembler::Zero, MacroAssembler::Address(temp2GPR, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    m_jit.load8(MacroAssembler::BaseIndex(tempGPR, startIndexGPR, MacroAssembler::TimesOne, 0), tempGPR);
    auto cont8Bit = m_jit.jump();

    is16Bit.link(&m_jit);
    m_jit.load16(MacroAssembler::BaseIndex(tempGPR, startIndexGPR, MacroAssembler::TimesTwo, 0), tempGPR);

    auto bigCharacter = m_jit.branch32(MacroAssembler::Above, tempGPR, TrustedImm32(maxSingleCharacterString));

    // 8 bit string values don't need the isASCII check.
    cont8Bit.link(&m_jit);

    m_jit.lshift32(MacroAssembler::TrustedImm32(sizeof(void*) == 4 ? 2 : 3), tempGPR);
    m_jit.addPtr(TrustedImmPtr(m_jit.vm()->smallStrings.singleCharacterStrings()), tempGPR);
    m_jit.loadPtr(tempGPR, tempGPR);

    addSlowPathGenerator(slowPathCall(bigCharacter, this, operationSingleCharacterString, tempGPR, tempGPR));

    addSlowPathGenerator(slowPathCall(slowCases, this, operationStringSubstr, tempGPR, stringGPR, startIndexGPR, tempGPR));

    if (endGPR)
        addSlowPathGenerator(slowPathCall(isRope, this, operationStringSlice, tempGPR, stringGPR, startGPR, *endGPR));
    else
        addSlowPathGenerator(slowPathCall(isRope, this, operationStringSlice, tempGPR, stringGPR, startGPR, TrustedImm32(std::numeric_limits<int32_t>::max())));

    doneCases.link(&m_jit);
    cellResult(tempGPR, node);
}

void SpeculativeJIT::compileToLowerCase(Node* node)
{
    ASSERT(node->op() == ToLowerCase);
    SpeculateCellOperand string(this, node->child1());
    GPRTemporary temp(this);
    GPRTemporary index(this);
    GPRTemporary charReg(this);
    GPRTemporary length(this);

    GPRReg stringGPR = string.gpr();
    GPRReg tempGPR = temp.gpr();
    GPRReg indexGPR = index.gpr();
    GPRReg charGPR = charReg.gpr();
    GPRReg lengthGPR = length.gpr();

    speculateString(node->child1(), stringGPR);

    CCallHelpers::JumpList slowPath;

    m_jit.move(TrustedImmPtr(nullptr), indexGPR);

    m_jit.loadPtr(MacroAssembler::Address(stringGPR, JSString::offsetOfValue()), tempGPR);
    slowPath.append(m_jit.branchIfRopeStringImpl(tempGPR));
    slowPath.append(m_jit.branchTest32(
        MacroAssembler::Zero, MacroAssembler::Address(tempGPR, StringImpl::flagsOffset()),
        MacroAssembler::TrustedImm32(StringImpl::flagIs8Bit())));
    m_jit.load32(MacroAssembler::Address(tempGPR, StringImpl::lengthMemoryOffset()), lengthGPR);
    m_jit.loadPtr(MacroAssembler::Address(tempGPR, StringImpl::dataOffset()), tempGPR);

    auto loopStart = m_jit.label();
    auto loopDone = m_jit.branch32(CCallHelpers::AboveOrEqual, indexGPR, lengthGPR);
    m_jit.load8(MacroAssembler::BaseIndex(tempGPR, indexGPR, MacroAssembler::TimesOne), charGPR);
    slowPath.append(m_jit.branchTest32(CCallHelpers::NonZero, charGPR, TrustedImm32(~0x7F)));
    m_jit.sub32(TrustedImm32('A'), charGPR);
    slowPath.append(m_jit.branch32(CCallHelpers::BelowOrEqual, charGPR, TrustedImm32('Z' - 'A')));

    m_jit.add32(TrustedImm32(1), indexGPR);
    m_jit.jump().linkTo(loopStart, &m_jit);
    
    slowPath.link(&m_jit);
    silentSpillAllRegisters(lengthGPR);
    callOperation(operationToLowerCase, lengthGPR, stringGPR, indexGPR);
    silentFillAllRegisters();
    m_jit.exceptionCheck();
    auto done = m_jit.jump();

    loopDone.link(&m_jit);
    m_jit.move(stringGPR, lengthGPR);

    done.link(&m_jit);
    cellResult(lengthGPR, node);
}

void SpeculativeJIT::compilePeepHoleInt32Branch(Node* node, Node* branchNode, JITCompiler::RelationalCondition condition)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        condition = JITCompiler::invert(condition);
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    if (node->child1()->isInt32Constant()) {
        int32_t imm = node->child1()->asInt32();
        SpeculateInt32Operand op2(this, node->child2());
        branch32(condition, JITCompiler::Imm32(imm), op2.gpr(), taken);
    } else if (node->child2()->isInt32Constant()) {
        SpeculateInt32Operand op1(this, node->child1());
        int32_t imm = node->child2()->asInt32();
        branch32(condition, op1.gpr(), JITCompiler::Imm32(imm), taken);
    } else {
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        branch32(condition, op1.gpr(), op2.gpr(), taken);
    }

    jump(notTaken);
}

// Returns true if the compare is fused with a subsequent branch.
bool SpeculativeJIT::compilePeepHoleBranch(Node* node, MacroAssembler::RelationalCondition condition, MacroAssembler::DoubleCondition doubleCondition, S_JITOperation_EJJ operation)
{
    // Fused compare & branch.
    unsigned branchIndexInBlock = detectPeepHoleBranch();
    if (branchIndexInBlock != UINT_MAX) {
        Node* branchNode = m_block->at(branchIndexInBlock);

        // detectPeepHoleBranch currently only permits the branch to be the very next node,
        // so can be no intervening nodes to also reference the compare. 
        ASSERT(node->adjustedRefCount() == 1);

        if (node->isBinaryUseKind(Int32Use))
            compilePeepHoleInt32Branch(node, branchNode, condition);
#if USE(JSVALUE64)
        else if (node->isBinaryUseKind(Int52RepUse))
            compilePeepHoleInt52Branch(node, branchNode, condition);
#endif // USE(JSVALUE64)
        else if (node->isBinaryUseKind(StringUse) || node->isBinaryUseKind(StringIdentUse)) {
            // Use non-peephole comparison, for now.
            return false;
        } else if (node->isBinaryUseKind(DoubleRepUse))
            compilePeepHoleDoubleBranch(node, branchNode, doubleCondition);
        else if (node->op() == CompareEq) {
            if (node->isBinaryUseKind(BooleanUse))
                compilePeepHoleBooleanBranch(node, branchNode, condition);
            else if (node->isBinaryUseKind(SymbolUse))
                compilePeepHoleSymbolEquality(node, branchNode);
            else if (node->isBinaryUseKind(ObjectUse))
                compilePeepHoleObjectEquality(node, branchNode);
            else if (node->isBinaryUseKind(ObjectUse, ObjectOrOtherUse))
                compilePeepHoleObjectToObjectOrOtherEquality(node->child1(), node->child2(), branchNode);
            else if (node->isBinaryUseKind(ObjectOrOtherUse, ObjectUse))
                compilePeepHoleObjectToObjectOrOtherEquality(node->child2(), node->child1(), branchNode);
            else if (!needsTypeCheck(node->child1(), SpecOther))
                nonSpeculativePeepholeBranchNullOrUndefined(node->child2(), branchNode);
            else if (!needsTypeCheck(node->child2(), SpecOther))
                nonSpeculativePeepholeBranchNullOrUndefined(node->child1(), branchNode);
            else {
                nonSpeculativePeepholeBranch(node, branchNode, condition, operation);
                return true;
            }
        } else {
            nonSpeculativePeepholeBranch(node, branchNode, condition, operation);
            return true;
        }

        use(node->child1());
        use(node->child2());
        m_indexInBlock = branchIndexInBlock;
        m_currentNode = branchNode;
        return true;
    }
    return false;
}

void SpeculativeJIT::noticeOSRBirth(Node* node)
{
    if (!node->hasVirtualRegister())
        return;
    
    VirtualRegister virtualRegister = node->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);
    
    info.noticeOSRBirth(*m_stream, node, virtualRegister);
}

void SpeculativeJIT::compileMovHint(Node* node)
{
    ASSERT(node->containsMovHint() && node->op() != ZombieHint);
    
    Node* child = node->child1().node();
    noticeOSRBirth(child);
    
    m_stream->appendAndLog(VariableEvent::movHint(MinifiedID(child), node->unlinkedLocal()));
}

void SpeculativeJIT::bail(AbortReason reason)
{
    if (verboseCompilationEnabled())
        dataLog("Bailing compilation.\n");
    m_compileOkay = true;
    m_jit.abortWithReason(reason, m_lastGeneratedNode);
    clearGenerationInfo();
}

void SpeculativeJIT::compileCurrentBlock()
{
    ASSERT(m_compileOkay);
    
    if (!m_block)
        return;
    
    ASSERT(m_block->isReachable);
    
    m_jit.blockHeads()[m_block->index] = m_jit.label();

    if (!m_block->intersectionOfCFAHasVisited) {
        // Don't generate code for basic blocks that are unreachable according to CFA.
        // But to be sure that nobody has generated a jump to this block, drop in a
        // breakpoint here.
        m_jit.abortWithReason(DFGUnreachableBasicBlock);
        return;
    }

    if (m_block->isCatchEntrypoint) {
        m_jit.addPtr(CCallHelpers::TrustedImm32(-(m_jit.graph().frameRegisterCount() * sizeof(Register))), GPRInfo::callFrameRegister,  CCallHelpers::stackPointerRegister);
        if (Options::zeroStackFrame())
            m_jit.clearStackFrame(GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister, GPRInfo::regT0, m_jit.graph().frameRegisterCount() * sizeof(Register));
        m_jit.emitSaveCalleeSaves();
        m_jit.emitMaterializeTagCheckRegisters();
        m_jit.emitPutToCallFrameHeader(m_jit.codeBlock(), CallFrameSlot::codeBlock);
    }

    m_stream->appendAndLog(VariableEvent::reset());
    
    m_jit.jitAssertHasValidCallFrame();
    m_jit.jitAssertTagsInPlace();
    m_jit.jitAssertArgumentCountSane();

    m_state.reset();
    m_state.beginBasicBlock(m_block);
    
    for (size_t i = m_block->variablesAtHead.size(); i--;) {
        int operand = m_block->variablesAtHead.operandForIndex(i);
        Node* node = m_block->variablesAtHead[i];
        if (!node)
            continue; // No need to record dead SetLocal's.
        
        VariableAccessData* variable = node->variableAccessData();
        DataFormat format;
        if (!node->refCount())
            continue; // No need to record dead SetLocal's.
        format = dataFormatFor(variable->flushFormat());
        m_stream->appendAndLog(
            VariableEvent::setLocal(
                VirtualRegister(operand),
                variable->machineLocal(),
                format));
    }

    m_origin = NodeOrigin();
    
    for (m_indexInBlock = 0; m_indexInBlock < m_block->size(); ++m_indexInBlock) {
        m_currentNode = m_block->at(m_indexInBlock);
        
        // We may have hit a contradiction that the CFA was aware of but that the JIT
        // didn't cause directly.
        if (!m_state.isValid()) {
            bail(DFGBailedAtTopOfBlock);
            return;
        }

        m_interpreter.startExecuting();
        m_interpreter.executeKnownEdgeTypes(m_currentNode);
        m_jit.setForNode(m_currentNode);
        m_origin = m_currentNode->origin;
        m_lastGeneratedNode = m_currentNode->op();
        
        ASSERT(m_currentNode->shouldGenerate());
        
        if (verboseCompilationEnabled()) {
            dataLogF(
                "SpeculativeJIT generating Node @%d (bc#%u) at JIT offset 0x%x",
                (int)m_currentNode->index(),
                m_currentNode->origin.semantic.bytecodeIndex, m_jit.debugOffset());
            dataLog("\n");
        }

        if (Options::validateDFGExceptionHandling() && (mayExit(m_jit.graph(), m_currentNode) != DoesNotExit || m_currentNode->isTerminal()))
            m_jit.jitReleaseAssertNoException(*m_jit.vm());

        m_jit.pcToCodeOriginMapBuilder().appendItem(m_jit.labelIgnoringWatchpoints(), m_origin.semantic);

        compile(m_currentNode);
        
        if (belongsInMinifiedGraph(m_currentNode->op()))
            m_minifiedGraph->append(MinifiedNode::fromNode(m_currentNode));
        
#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
        m_jit.clearRegisterAllocationOffsets();
#endif
        
        if (!m_compileOkay) {
            bail(DFGBailedAtEndOfNode);
            return;
        }
        
        // Make sure that the abstract state is rematerialized for the next node.
        m_interpreter.executeEffects(m_indexInBlock);
    }
    
    // Perform the most basic verification that children have been used correctly.
    if (!ASSERT_DISABLED) {
        for (auto& info : m_generationInfo)
            RELEASE_ASSERT(!info.alive());
    }
}

// If we are making type predictions about our arguments then
// we need to check that they are correct on function entry.
void SpeculativeJIT::checkArgumentTypes()
{
    ASSERT(!m_currentNode);
    m_origin = NodeOrigin(CodeOrigin(0), CodeOrigin(0), true);

    auto& arguments = m_jit.graph().m_rootToArguments.find(m_jit.graph().block(0))->value;
    for (int i = 0; i < m_jit.codeBlock()->numParameters(); ++i) {
        Node* node = arguments[i];
        if (!node) {
            // The argument is dead. We don't do any checks for such arguments.
            continue;
        }
        
        ASSERT(node->op() == SetArgument);
        ASSERT(node->shouldGenerate());

        VariableAccessData* variableAccessData = node->variableAccessData();
        FlushFormat format = variableAccessData->flushFormat();
        
        if (format == FlushedJSValue)
            continue;
        
        VirtualRegister virtualRegister = variableAccessData->local();

        JSValueSource valueSource = JSValueSource(JITCompiler::addressFor(virtualRegister));
        
#if USE(JSVALUE64)
        switch (format) {
        case FlushedInt32: {
            speculationCheck(BadType, valueSource, node, m_jit.branch64(MacroAssembler::Below, JITCompiler::addressFor(virtualRegister), GPRInfo::tagTypeNumberRegister));
            break;
        }
        case FlushedBoolean: {
            GPRTemporary temp(this);
            m_jit.load64(JITCompiler::addressFor(virtualRegister), temp.gpr());
            m_jit.xor64(TrustedImm32(static_cast<int32_t>(ValueFalse)), temp.gpr());
            speculationCheck(BadType, valueSource, node, m_jit.branchTest64(MacroAssembler::NonZero, temp.gpr(), TrustedImm32(static_cast<int32_t>(~1))));
            break;
        }
        case FlushedCell: {
            speculationCheck(BadType, valueSource, node, m_jit.branchTest64(MacroAssembler::NonZero, JITCompiler::addressFor(virtualRegister), GPRInfo::tagMaskRegister));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
#else
        switch (format) {
        case FlushedInt32: {
            speculationCheck(BadType, valueSource, node, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::Int32Tag)));
            break;
        }
        case FlushedBoolean: {
            speculationCheck(BadType, valueSource, node, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::BooleanTag)));
            break;
        }
        case FlushedCell: {
            speculationCheck(BadType, valueSource, node, m_jit.branch32(MacroAssembler::NotEqual, JITCompiler::tagFor(virtualRegister), TrustedImm32(JSValue::CellTag)));
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
#endif
    }

    m_origin = NodeOrigin();
}

bool SpeculativeJIT::compile()
{
    checkArgumentTypes();
    
    ASSERT(!m_currentNode);
    for (BlockIndex blockIndex = 0; blockIndex < m_jit.graph().numBlocks(); ++blockIndex) {
        m_jit.setForBlockIndex(blockIndex);
        m_block = m_jit.graph().block(blockIndex);
        compileCurrentBlock();
    }
    linkBranches();
    return true;
}

void SpeculativeJIT::createOSREntries()
{
    for (BlockIndex blockIndex = 0; blockIndex < m_jit.graph().numBlocks(); ++blockIndex) {
        BasicBlock* block = m_jit.graph().block(blockIndex);
        if (!block)
            continue;
        if (block->isOSRTarget || block->isCatchEntrypoint) {
            // Currently we don't have OSR entry trampolines. We could add them
            // here if need be.
            m_osrEntryHeads.append(m_jit.blockHeads()[blockIndex]);
        }
    }
}

void SpeculativeJIT::linkOSREntries(LinkBuffer& linkBuffer)
{
    unsigned osrEntryIndex = 0;
    for (BlockIndex blockIndex = 0; blockIndex < m_jit.graph().numBlocks(); ++blockIndex) {
        BasicBlock* block = m_jit.graph().block(blockIndex);
        if (!block)
            continue;
        if (!block->isOSRTarget && !block->isCatchEntrypoint)
            continue;
        if (block->isCatchEntrypoint) {
            auto& argumentsVector = m_jit.graph().m_rootToArguments.find(block)->value;
            Vector<FlushFormat> argumentFormats;
            argumentFormats.reserveInitialCapacity(argumentsVector.size());
            for (Node* setArgument : argumentsVector) {
                if (setArgument) {
                    FlushFormat flushFormat = setArgument->variableAccessData()->flushFormat();
                    ASSERT(flushFormat == FlushedInt32 || flushFormat == FlushedCell || flushFormat == FlushedBoolean || flushFormat == FlushedJSValue);
                    argumentFormats.uncheckedAppend(flushFormat);
                } else
                    argumentFormats.uncheckedAppend(DeadFlush);
            }
            m_jit.noticeCatchEntrypoint(*block, m_osrEntryHeads[osrEntryIndex++], linkBuffer, WTFMove(argumentFormats));
        } else {
            ASSERT(block->isOSRTarget);
            m_jit.noticeOSREntry(*block, m_osrEntryHeads[osrEntryIndex++], linkBuffer);
        }
    }

    m_jit.jitCode()->finalizeOSREntrypoints();
    m_jit.jitCode()->common.finalizeCatchEntrypoints();

    ASSERT(osrEntryIndex == m_osrEntryHeads.size());
    
    if (verboseCompilationEnabled()) {
        DumpContext dumpContext;
        dataLog("OSR Entries:\n");
        for (OSREntryData& entryData : m_jit.jitCode()->osrEntry)
            dataLog("    ", inContext(entryData, &dumpContext), "\n");
        if (!dumpContext.isEmpty())
            dumpContext.dump(WTF::dataFile());
    }
}
    
void SpeculativeJIT::compileCheckTraps(Node* node)
{
    ASSERT(Options::usePollingTraps());
    GPRTemporary unused(this);
    GPRReg unusedGPR = unused.gpr();

    JITCompiler::Jump needTrapHandling = m_jit.branchTest8(JITCompiler::NonZero,
        JITCompiler::AbsoluteAddress(m_jit.vm()->needTrapHandlingAddress()));

    addSlowPathGenerator(slowPathCall(needTrapHandling, this, operationHandleTraps, unusedGPR));
    noResult(node);
}

void SpeculativeJIT::compileDoublePutByVal(Node* node, SpeculateCellOperand& base, SpeculateStrictInt32Operand& property)
{
    Edge child3 = m_jit.graph().varArgChild(node, 2);
    Edge child4 = m_jit.graph().varArgChild(node, 3);

    ArrayMode arrayMode = node->arrayMode();
    
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    
    SpeculateDoubleOperand value(this, child3);

    FPRReg valueReg = value.fpr();
    
    DFG_TYPE_CHECK(
        JSValueRegs(), child3, SpecFullRealNumber,
        m_jit.branchIfNaN(valueReg));
    
    if (!m_compileOkay)
        return;
    
    StorageOperand storage(this, child4);
    GPRReg storageReg = storage.gpr();

    if (node->op() == PutByValAlias) {
        // Store the value to the array.
        GPRReg propertyReg = property.gpr();
        FPRReg valueReg = value.fpr();
        m_jit.storeDouble(valueReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight));
        
        noResult(m_currentNode);
        return;
    }
    
    GPRTemporary temporary;
    GPRReg temporaryReg = temporaryRegisterForPutByVal(temporary, node);

    MacroAssembler::Jump slowCase;
    
    if (arrayMode.isInBounds()) {
        speculationCheck(
            OutOfBounds, JSValueRegs(), 0,
            m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength())));
    } else {
        MacroAssembler::Jump inBounds = m_jit.branch32(MacroAssembler::Below, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength()));
        
        slowCase = m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfVectorLength()));
        
        if (!arrayMode.isOutOfBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), 0, slowCase);
        
        m_jit.add32(TrustedImm32(1), propertyReg, temporaryReg);
        m_jit.store32(temporaryReg, MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength()));

        inBounds.link(&m_jit);
    }
    
    m_jit.storeDouble(valueReg, MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight));

    base.use();
    property.use();
    value.use();
    storage.use();
    
    if (arrayMode.isOutOfBounds()) {
        addSlowPathGenerator(
            slowPathCall(
                slowCase, this,
                m_jit.codeBlock()->isStrictMode()
                    ? (node->op() == PutByValDirect ? operationPutDoubleByValDirectBeyondArrayBoundsStrict : operationPutDoubleByValBeyondArrayBoundsStrict)
                    : (node->op() == PutByValDirect ? operationPutDoubleByValDirectBeyondArrayBoundsNonStrict : operationPutDoubleByValBeyondArrayBoundsNonStrict),
                NoResult, baseReg, propertyReg, valueReg));
    }

    noResult(m_currentNode, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileGetCharCodeAt(Node* node)
{
    SpeculateCellOperand string(this, node->child1());
    SpeculateStrictInt32Operand index(this, node->child2());
    StorageOperand storage(this, node->child3());

    GPRReg stringReg = string.gpr();
    GPRReg indexReg = index.gpr();
    GPRReg storageReg = storage.gpr();
    
    ASSERT(speculationChecked(m_state.forNode(node->child1()).m_type, SpecString));

    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();

    m_jit.loadPtr(MacroAssembler::Address(stringReg, JSString::offsetOfValue()), scratchReg);
    
    // unsigned comparison so we can filter out negative indices and indices that are too large
    speculationCheck(Uncountable, JSValueRegs(), 0, m_jit.branch32(MacroAssembler::AboveOrEqual, indexReg, CCallHelpers::Address(scratchReg, StringImpl::lengthMemoryOffset())));

    // Load the character into scratchReg
    JITCompiler::Jump is16Bit = m_jit.branchTest32(MacroAssembler::Zero, MacroAssembler::Address(scratchReg, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    m_jit.load8(MacroAssembler::BaseIndex(storageReg, indexReg, MacroAssembler::TimesOne, 0), scratchReg);
    JITCompiler::Jump cont8Bit = m_jit.jump();

    is16Bit.link(&m_jit);

    m_jit.load16(MacroAssembler::BaseIndex(storageReg, indexReg, MacroAssembler::TimesTwo, 0), scratchReg);

    cont8Bit.link(&m_jit);

    int32Result(scratchReg, m_currentNode);
}

void SpeculativeJIT::compileGetByValOnString(Node* node)
{
    SpeculateCellOperand base(this, m_graph.child(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.child(node, 1));
    StorageOperand storage(this, m_graph.child(node, 2));
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg storageReg = storage.gpr();

    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();
#if USE(JSVALUE32_64)
    GPRTemporary resultTag;
    GPRReg resultTagReg = InvalidGPRReg;
    if (node->arrayMode().isOutOfBounds()) {
        GPRTemporary realResultTag(this);
        resultTag.adopt(realResultTag);
        resultTagReg = resultTag.gpr();
    }
#endif

    ASSERT(ArrayMode(Array::String, Array::Read).alreadyChecked(m_jit.graph(), node, m_state.forNode(m_graph.child(node, 0))));

    // unsigned comparison so we can filter out negative indices and indices that are too large
    m_jit.loadPtr(MacroAssembler::Address(baseReg, JSString::offsetOfValue()), scratchReg);
    JITCompiler::Jump outOfBounds = m_jit.branch32(
        MacroAssembler::AboveOrEqual, propertyReg,
        MacroAssembler::Address(scratchReg, StringImpl::lengthMemoryOffset()));
    if (node->arrayMode().isInBounds())
        speculationCheck(OutOfBounds, JSValueRegs(), 0, outOfBounds);

    // Load the character into scratchReg
    JITCompiler::Jump is16Bit = m_jit.branchTest32(MacroAssembler::Zero, MacroAssembler::Address(scratchReg, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));

    m_jit.load8(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesOne, 0), scratchReg);
    JITCompiler::Jump cont8Bit = m_jit.jump();

    is16Bit.link(&m_jit);

    m_jit.load16(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesTwo, 0), scratchReg);

    JITCompiler::Jump bigCharacter =
        m_jit.branch32(MacroAssembler::Above, scratchReg, TrustedImm32(maxSingleCharacterString));

    // 8 bit string values don't need the isASCII check.
    cont8Bit.link(&m_jit);

    m_jit.lshift32(MacroAssembler::TrustedImm32(sizeof(void*) == 4 ? 2 : 3), scratchReg);
    m_jit.addPtr(TrustedImmPtr(m_jit.vm()->smallStrings.singleCharacterStrings()), scratchReg);
    m_jit.loadPtr(scratchReg, scratchReg);

    addSlowPathGenerator(
        slowPathCall(
            bigCharacter, this, operationSingleCharacterString, scratchReg, scratchReg));

    if (node->arrayMode().isOutOfBounds()) {
#if USE(JSVALUE32_64)
        m_jit.move(TrustedImm32(JSValue::CellTag), resultTagReg);
#endif

        JSGlobalObject* globalObject = m_jit.globalObjectFor(node->origin.semantic);
        bool prototypeChainIsSane = false;
        if (globalObject->stringPrototypeChainIsSane()) {
            // FIXME: This could be captured using a Speculation mode that means "out-of-bounds
            // loads return a trivial value". Something like SaneChainOutOfBounds. This should
            // speculate that we don't take negative out-of-bounds, or better yet, it should rely
            // on a stringPrototypeChainIsSane() guaranteeing that the prototypes have no negative
            // indexed properties either.
            // https://bugs.webkit.org/show_bug.cgi?id=144668
            m_jit.graph().registerAndWatchStructureTransition(globalObject->stringPrototype()->structure(*m_jit.vm()));
            m_jit.graph().registerAndWatchStructureTransition(globalObject->objectPrototype()->structure(*m_jit.vm()));
            prototypeChainIsSane = globalObject->stringPrototypeChainIsSane();
        }
        if (prototypeChainIsSane) {
#if USE(JSVALUE64)
            addSlowPathGenerator(std::make_unique<SaneStringGetByValSlowPathGenerator>(
                outOfBounds, this, JSValueRegs(scratchReg), baseReg, propertyReg));
#else
            addSlowPathGenerator(std::make_unique<SaneStringGetByValSlowPathGenerator>(
                outOfBounds, this, JSValueRegs(resultTagReg, scratchReg),
                baseReg, propertyReg));
#endif
        } else {
#if USE(JSVALUE64)
            addSlowPathGenerator(
                slowPathCall(
                    outOfBounds, this, operationGetByValStringInt,
                    scratchReg, baseReg, propertyReg));
#else
            addSlowPathGenerator(
                slowPathCall(
                    outOfBounds, this, operationGetByValStringInt,
                    JSValueRegs(resultTagReg, scratchReg), baseReg, propertyReg));
#endif
        }
        
#if USE(JSVALUE64)
        jsValueResult(scratchReg, m_currentNode);
#else
        jsValueResult(resultTagReg, scratchReg, m_currentNode);
#endif
    } else
        cellResult(scratchReg, m_currentNode);
}

void SpeculativeJIT::compileFromCharCode(Node* node)
{
    Edge& child = node->child1();
    if (child.useKind() == UntypedUse) {
        JSValueOperand opr(this, child);
        JSValueRegs oprRegs = opr.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationStringFromCharCodeUntyped, resultRegs, oprRegs);
        m_jit.exceptionCheck();
        
        jsValueResult(resultRegs, node);
        return;
    }

    SpeculateStrictInt32Operand property(this, child);
    GPRReg propertyReg = property.gpr();
    GPRTemporary smallStrings(this);
    GPRTemporary scratch(this);
    GPRReg scratchReg = scratch.gpr();
    GPRReg smallStringsReg = smallStrings.gpr();

    JITCompiler::JumpList slowCases;
    slowCases.append(m_jit.branch32(MacroAssembler::Above, propertyReg, TrustedImm32(maxSingleCharacterString)));
    m_jit.move(TrustedImmPtr(m_jit.vm()->smallStrings.singleCharacterStrings()), smallStringsReg);
    m_jit.loadPtr(MacroAssembler::BaseIndex(smallStringsReg, propertyReg, MacroAssembler::ScalePtr, 0), scratchReg);

    slowCases.append(m_jit.branchTest32(MacroAssembler::Zero, scratchReg));
    addSlowPathGenerator(slowPathCall(slowCases, this, operationStringFromCharCode, scratchReg, propertyReg));
    cellResult(scratchReg, m_currentNode);
}

GeneratedOperandType SpeculativeJIT::checkGeneratedTypeForToInt32(Node* node)
{
    VirtualRegister virtualRegister = node->virtualRegister();
    GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

    switch (info.registerFormat()) {
    case DataFormatStorage:
        RELEASE_ASSERT_NOT_REACHED();

    case DataFormatBoolean:
    case DataFormatCell:
        terminateSpeculativeExecution(Uncountable, JSValueRegs(), 0);
        return GeneratedOperandTypeUnknown;

    case DataFormatNone:
    case DataFormatJSCell:
    case DataFormatJS:
    case DataFormatJSBoolean:
    case DataFormatJSDouble:
        return GeneratedOperandJSValue;

    case DataFormatJSInt32:
    case DataFormatInt32:
        return GeneratedOperandInteger;

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return GeneratedOperandTypeUnknown;
    }
}

void SpeculativeJIT::compileValueToInt32(Node* node)
{
    switch (node->child1().useKind()) {
#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand op1(this, node->child1());
        GPRTemporary result(this, Reuse, op1);
        GPRReg op1GPR = op1.gpr();
        GPRReg resultGPR = result.gpr();
        m_jit.zeroExtend32ToPtr(op1GPR, resultGPR);
        int32Result(resultGPR, node, DataFormatInt32);
        return;
    }
#endif // USE(JSVALUE64)
        
    case DoubleRepUse: {
        GPRTemporary result(this);
        SpeculateDoubleOperand op1(this, node->child1());
        FPRReg fpr = op1.fpr();
        GPRReg gpr = result.gpr();
#if CPU(ARM64)
        if (MacroAssemblerARM64::supportsDoubleToInt32ConversionUsingJavaScriptSemantics())
            m_jit.convertDoubleToInt32UsingJavaScriptSemantics(fpr, gpr);
        else
#endif
        {
            JITCompiler::Jump notTruncatedToInteger = m_jit.branchTruncateDoubleToInt32(fpr, gpr, JITCompiler::BranchIfTruncateFailed);
            addSlowPathGenerator(slowPathCall(notTruncatedToInteger, this,
                hasSensibleDoubleToInt() ? operationToInt32SensibleSlow : operationToInt32, NeedToSpill, ExceptionCheckRequirement::CheckNotNeeded, gpr, fpr));
        }
        int32Result(gpr, node);
        return;
    }
    
    case NumberUse:
    case NotCellUse: {
        switch (checkGeneratedTypeForToInt32(node->child1().node())) {
        case GeneratedOperandInteger: {
            SpeculateInt32Operand op1(this, node->child1(), ManualOperandSpeculation);
            GPRTemporary result(this, Reuse, op1);
            m_jit.move(op1.gpr(), result.gpr());
            int32Result(result.gpr(), node, op1.format());
            return;
        }
        case GeneratedOperandJSValue: {
            GPRTemporary result(this);
#if USE(JSVALUE64)
            JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);

            GPRReg gpr = op1.gpr();
            GPRReg resultGpr = result.gpr();
            FPRTemporary tempFpr(this);
            FPRReg fpr = tempFpr.fpr();

            JITCompiler::Jump isInteger = m_jit.branchIfInt32(gpr);
            JITCompiler::JumpList converted;

            if (node->child1().useKind() == NumberUse) {
                DFG_TYPE_CHECK(
                    JSValueRegs(gpr), node->child1(), SpecBytecodeNumber,
                    m_jit.branchIfNotNumber(gpr));
            } else {
                JITCompiler::Jump isNumber = m_jit.branchIfNumber(gpr);
                
                DFG_TYPE_CHECK(
                    JSValueRegs(gpr), node->child1(), ~SpecCellCheck, m_jit.branchIfCell(JSValueRegs(gpr)));
                
                // It's not a cell: so true turns into 1 and all else turns into 0.
                m_jit.compare64(JITCompiler::Equal, gpr, TrustedImm32(ValueTrue), resultGpr);
                converted.append(m_jit.jump());
                
                isNumber.link(&m_jit);
            }

            // First, if we get here we have a double encoded as a JSValue
            unboxDouble(gpr, resultGpr, fpr);
#if CPU(ARM64)
            if (MacroAssemblerARM64::supportsDoubleToInt32ConversionUsingJavaScriptSemantics())
                m_jit.convertDoubleToInt32UsingJavaScriptSemantics(fpr, resultGpr);
            else
#endif
            {
                silentSpillAllRegisters(resultGpr);
                callOperation(operationToInt32, resultGpr, fpr);
                silentFillAllRegisters();
            }

            converted.append(m_jit.jump());

            isInteger.link(&m_jit);
            m_jit.zeroExtend32ToPtr(gpr, resultGpr);

            converted.link(&m_jit);
#else
            Node* childNode = node->child1().node();
            VirtualRegister virtualRegister = childNode->virtualRegister();
            GenerationInfo& info = generationInfoFromVirtualRegister(virtualRegister);

            JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);

            GPRReg payloadGPR = op1.payloadGPR();
            GPRReg resultGpr = result.gpr();
        
            JITCompiler::JumpList converted;

            if (info.registerFormat() == DataFormatJSInt32)
                m_jit.move(payloadGPR, resultGpr);
            else {
                GPRReg tagGPR = op1.tagGPR();
                FPRTemporary tempFpr(this);
                FPRReg fpr = tempFpr.fpr();
                FPRTemporary scratch(this);

                JITCompiler::Jump isInteger = m_jit.branchIfInt32(tagGPR);

                if (node->child1().useKind() == NumberUse) {
                    DFG_TYPE_CHECK(
                        op1.jsValueRegs(), node->child1(), SpecBytecodeNumber,
                        m_jit.branch32(
                            MacroAssembler::AboveOrEqual, tagGPR,
                            TrustedImm32(JSValue::LowestTag)));
                } else {
                    JITCompiler::Jump isNumber = m_jit.branch32(MacroAssembler::Below, tagGPR, TrustedImm32(JSValue::LowestTag));
                    
                    DFG_TYPE_CHECK(
                        op1.jsValueRegs(), node->child1(), ~SpecCell,
                        m_jit.branchIfCell(op1.jsValueRegs()));
                    
                    // It's not a cell: so true turns into 1 and all else turns into 0.
                    JITCompiler::Jump isBoolean = m_jit.branchIfBoolean(tagGPR, InvalidGPRReg);
                    m_jit.move(TrustedImm32(0), resultGpr);
                    converted.append(m_jit.jump());
                    
                    isBoolean.link(&m_jit);
                    m_jit.move(payloadGPR, resultGpr);
                    converted.append(m_jit.jump());
                    
                    isNumber.link(&m_jit);
                }

                unboxDouble(tagGPR, payloadGPR, fpr, scratch.fpr());

                silentSpillAllRegisters(resultGpr);
                callOperation(operationToInt32, resultGpr, fpr);
                silentFillAllRegisters();

                converted.append(m_jit.jump());

                isInteger.link(&m_jit);
                m_jit.move(payloadGPR, resultGpr);

                converted.link(&m_jit);
            }
#endif
            int32Result(resultGpr, node);
            return;
        }
        case GeneratedOperandTypeUnknown:
            RELEASE_ASSERT(!m_compileOkay);
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
    
    default:
        ASSERT(!m_compileOkay);
        return;
    }
}

void SpeculativeJIT::compileUInt32ToNumber(Node* node)
{
    if (doesOverflow(node->arithMode())) {
        if (enableInt52()) {
            SpeculateInt32Operand op1(this, node->child1());
            GPRTemporary result(this, Reuse, op1);
            m_jit.zeroExtend32ToPtr(op1.gpr(), result.gpr());
            strictInt52Result(result.gpr(), node);
            return;
        }
        SpeculateInt32Operand op1(this, node->child1());
        FPRTemporary result(this);
            
        GPRReg inputGPR = op1.gpr();
        FPRReg outputFPR = result.fpr();
            
        m_jit.convertInt32ToDouble(inputGPR, outputFPR);
            
        JITCompiler::Jump positive = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, inputGPR, TrustedImm32(0));
        m_jit.addDouble(JITCompiler::AbsoluteAddress(&AssemblyHelpers::twoToThe32), outputFPR);
        positive.link(&m_jit);
            
        doubleResult(outputFPR, node);
        return;
    }
    
    RELEASE_ASSERT(node->arithMode() == Arith::CheckOverflow);

    SpeculateInt32Operand op1(this, node->child1());
    GPRTemporary result(this);

    m_jit.move(op1.gpr(), result.gpr());

    speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branch32(MacroAssembler::LessThan, result.gpr(), TrustedImm32(0)));

    int32Result(result.gpr(), node, op1.format());
}

void SpeculativeJIT::compileDoubleAsInt32(Node* node)
{
    SpeculateDoubleOperand op1(this, node->child1());
    FPRTemporary scratch(this);
    GPRTemporary result(this);
    
    FPRReg valueFPR = op1.fpr();
    FPRReg scratchFPR = scratch.fpr();
    GPRReg resultGPR = result.gpr();

    JITCompiler::JumpList failureCases;
    RELEASE_ASSERT(shouldCheckOverflow(node->arithMode()));
    m_jit.branchConvertDoubleToInt32(
        valueFPR, resultGPR, failureCases, scratchFPR,
        shouldCheckNegativeZero(node->arithMode()));
    speculationCheck(Overflow, JSValueRegs(), 0, failureCases);

    int32Result(resultGPR, node);
}

void SpeculativeJIT::compileDoubleRep(Node* node)
{
    switch (node->child1().useKind()) {
    case RealNumberUse: {
        JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
        FPRTemporary result(this);
        
        JSValueRegs op1Regs = op1.jsValueRegs();
        FPRReg resultFPR = result.fpr();
        
#if USE(JSVALUE64)
        GPRTemporary temp(this);
        GPRReg tempGPR = temp.gpr();
        m_jit.unboxDoubleWithoutAssertions(op1Regs.gpr(), tempGPR, resultFPR);
#else
        FPRTemporary temp(this);
        FPRReg tempFPR = temp.fpr();
        unboxDouble(op1Regs.tagGPR(), op1Regs.payloadGPR(), resultFPR, tempFPR);
#endif
        
        JITCompiler::Jump done = m_jit.branchIfNotNaN(resultFPR);
        
        DFG_TYPE_CHECK(
            op1Regs, node->child1(), SpecBytecodeRealNumber, m_jit.branchIfNotInt32(op1Regs));
        m_jit.convertInt32ToDouble(op1Regs.payloadGPR(), resultFPR);
        
        done.link(&m_jit);
        
        doubleResult(resultFPR, node);
        return;
    }
    
    case NotCellUse:
    case NumberUse: {
        SpeculatedType possibleTypes = m_state.forNode(node->child1()).m_type;
        if (isInt32Speculation(possibleTypes)) {
            SpeculateInt32Operand op1(this, node->child1(), ManualOperandSpeculation);
            FPRTemporary result(this);
            m_jit.convertInt32ToDouble(op1.gpr(), result.fpr());
            doubleResult(result.fpr(), node);
            return;
        }

        JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
        FPRTemporary result(this);

#if USE(JSVALUE64)
        GPRTemporary temp(this);

        GPRReg op1GPR = op1.gpr();
        GPRReg tempGPR = temp.gpr();
        FPRReg resultFPR = result.fpr();
        JITCompiler::JumpList done;

        JITCompiler::Jump isInteger = m_jit.branchIfInt32(op1GPR);

        if (node->child1().useKind() == NotCellUse) {
            JITCompiler::Jump isNumber = m_jit.branchIfNumber(op1GPR);
            JITCompiler::Jump isUndefined = m_jit.branchIfUndefined(op1GPR);

            static const double zero = 0;
            m_jit.loadDouble(TrustedImmPtr(&zero), resultFPR);

            JITCompiler::Jump isNull = m_jit.branchIfNull(op1GPR);
            done.append(isNull);

            DFG_TYPE_CHECK(JSValueRegs(op1GPR), node->child1(), ~SpecCellCheck,
                m_jit.branchTest64(JITCompiler::Zero, op1GPR, TrustedImm32(static_cast<int32_t>(TagBitBool))));

            JITCompiler::Jump isFalse = m_jit.branch64(JITCompiler::Equal, op1GPR, TrustedImm64(ValueFalse));
            static const double one = 1;
            m_jit.loadDouble(TrustedImmPtr(&one), resultFPR);
            done.append(m_jit.jump());
            done.append(isFalse);

            isUndefined.link(&m_jit);
            static const double NaN = PNaN;
            m_jit.loadDouble(TrustedImmPtr(&NaN), resultFPR);
            done.append(m_jit.jump());

            isNumber.link(&m_jit);
        } else if (needsTypeCheck(node->child1(), SpecBytecodeNumber)) {
            typeCheck(
                JSValueRegs(op1GPR), node->child1(), SpecBytecodeNumber,
                m_jit.branchIfNotNumber(op1GPR));
        }

        unboxDouble(op1GPR, tempGPR, resultFPR);
        done.append(m_jit.jump());
    
        isInteger.link(&m_jit);
        m_jit.convertInt32ToDouble(op1GPR, resultFPR);
        done.link(&m_jit);
#else // USE(JSVALUE64) -> this is the 32_64 case
        FPRTemporary temp(this);
    
        GPRReg op1TagGPR = op1.tagGPR();
        GPRReg op1PayloadGPR = op1.payloadGPR();
        FPRReg tempFPR = temp.fpr();
        FPRReg resultFPR = result.fpr();
        JITCompiler::JumpList done;
    
        JITCompiler::Jump isInteger = m_jit.branchIfInt32(op1TagGPR);

        if (node->child1().useKind() == NotCellUse) {
            JITCompiler::Jump isNumber = m_jit.branch32(JITCompiler::Below, op1TagGPR, JITCompiler::TrustedImm32(JSValue::LowestTag + 1));
            JITCompiler::Jump isUndefined = m_jit.branchIfUndefined(op1TagGPR);

            static const double zero = 0;
            m_jit.loadDouble(TrustedImmPtr(&zero), resultFPR);

            JITCompiler::Jump isNull = m_jit.branchIfNull(op1TagGPR);
            done.append(isNull);

            DFG_TYPE_CHECK(JSValueRegs(op1TagGPR, op1PayloadGPR), node->child1(), ~SpecCell, m_jit.branchIfNotBoolean(op1TagGPR, InvalidGPRReg));

            JITCompiler::Jump isFalse = m_jit.branchTest32(JITCompiler::Zero, op1PayloadGPR, TrustedImm32(1));
            static const double one = 1;
            m_jit.loadDouble(TrustedImmPtr(&one), resultFPR);
            done.append(m_jit.jump());
            done.append(isFalse);

            isUndefined.link(&m_jit);
            static const double NaN = PNaN;
            m_jit.loadDouble(TrustedImmPtr(&NaN), resultFPR);
            done.append(m_jit.jump());

            isNumber.link(&m_jit);
        } else if (needsTypeCheck(node->child1(), SpecBytecodeNumber)) {
            // This check fails with Int32Tag, but it is OK since Int32 case is already excluded.
            typeCheck(
                JSValueRegs(op1TagGPR, op1PayloadGPR), node->child1(), SpecBytecodeNumber,
                m_jit.branch32(MacroAssembler::AboveOrEqual, op1TagGPR, TrustedImm32(JSValue::LowestTag)));
        }

        unboxDouble(op1TagGPR, op1PayloadGPR, resultFPR, tempFPR);
        done.append(m_jit.jump());
    
        isInteger.link(&m_jit);
        m_jit.convertInt32ToDouble(op1PayloadGPR, resultFPR);
        done.link(&m_jit);
#endif // USE(JSVALUE64)
    
        doubleResult(resultFPR, node);
        return;
    }
        
#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand value(this, node->child1());
        FPRTemporary result(this);
        
        GPRReg valueGPR = value.gpr();
        FPRReg resultFPR = result.fpr();

        m_jit.convertInt64ToDouble(valueGPR, resultFPR);
        
        doubleResult(resultFPR, node);
        return;
    }
#endif // USE(JSVALUE64)
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileValueRep(Node* node)
{
    switch (node->child1().useKind()) {
    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        JSValueRegsTemporary result(this);
        
        FPRReg valueFPR = value.fpr();
        JSValueRegs resultRegs = result.regs();
        
        // It's very tempting to in-place filter the value to indicate that it's not impure NaN
        // anymore. Unfortunately, this would be unsound. If it's a GetLocal or if the value was
        // subject to a prior SetLocal, filtering the value would imply that the corresponding
        // local was purified.
        if (needsTypeCheck(node->child1(), ~SpecDoubleImpureNaN))
            m_jit.purifyNaN(valueFPR);

        boxDouble(valueFPR, resultRegs);
        
        jsValueResult(resultRegs, node);
        return;
    }
        
#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand value(this, node->child1());
        GPRTemporary result(this);
        
        GPRReg valueGPR = value.gpr();
        GPRReg resultGPR = result.gpr();
        
        boxInt52(valueGPR, resultGPR, DataFormatStrictInt52);
        
        jsValueResult(resultGPR, node);
        return;
    }
#endif // USE(JSVALUE64)
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

static double clampDoubleToByte(double d)
{
    d += 0.5;
    if (!(d > 0))
        d = 0;
    else if (d > 255)
        d = 255;
    return d;
}

static void compileClampIntegerToByte(JITCompiler& jit, GPRReg result)
{
    MacroAssembler::Jump inBounds = jit.branch32(MacroAssembler::BelowOrEqual, result, JITCompiler::TrustedImm32(0xff));
    MacroAssembler::Jump tooBig = jit.branch32(MacroAssembler::GreaterThan, result, JITCompiler::TrustedImm32(0xff));
    jit.xorPtr(result, result);
    MacroAssembler::Jump clamped = jit.jump();
    tooBig.link(&jit);
    jit.move(JITCompiler::TrustedImm32(255), result);
    clamped.link(&jit);
    inBounds.link(&jit);
}

static void compileClampDoubleToByte(JITCompiler& jit, GPRReg result, FPRReg source, FPRReg scratch)
{
    // Unordered compare so we pick up NaN
    static const double zero = 0;
    static const double byteMax = 255;
    static const double half = 0.5;
    jit.loadDouble(JITCompiler::TrustedImmPtr(&zero), scratch);
    MacroAssembler::Jump tooSmall = jit.branchDouble(MacroAssembler::DoubleLessThanOrEqualOrUnordered, source, scratch);
    jit.loadDouble(JITCompiler::TrustedImmPtr(&byteMax), scratch);
    MacroAssembler::Jump tooBig = jit.branchDouble(MacroAssembler::DoubleGreaterThan, source, scratch);
    
    jit.loadDouble(JITCompiler::TrustedImmPtr(&half), scratch);
    // FIXME: This should probably just use a floating point round!
    // https://bugs.webkit.org/show_bug.cgi?id=72054
    jit.addDouble(source, scratch);
    jit.truncateDoubleToInt32(scratch, result);   
    MacroAssembler::Jump truncatedInt = jit.jump();
    
    tooSmall.link(&jit);
    jit.xorPtr(result, result);
    MacroAssembler::Jump zeroed = jit.jump();
    
    tooBig.link(&jit);
    jit.move(JITCompiler::TrustedImm32(255), result);
    
    truncatedInt.link(&jit);
    zeroed.link(&jit);

}

JITCompiler::Jump SpeculativeJIT::jumpForTypedArrayOutOfBounds(Node* node, GPRReg baseGPR, GPRReg indexGPR)
{
    if (node->op() == PutByValAlias)
        return JITCompiler::Jump();
    JSArrayBufferView* view = m_jit.graph().tryGetFoldableView(
        m_state.forNode(m_jit.graph().child(node, 0)).m_value, node->arrayMode());
    if (view) {
        uint32_t length = view->length();
        Node* indexNode = m_jit.graph().child(node, 1).node();
        if (indexNode->isInt32Constant() && indexNode->asUInt32() < length)
            return JITCompiler::Jump();
        return m_jit.branch32(
            MacroAssembler::AboveOrEqual, indexGPR, MacroAssembler::Imm32(length));
    }
    return m_jit.branch32(
        MacroAssembler::AboveOrEqual, indexGPR,
        MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()));
}

void SpeculativeJIT::emitTypedArrayBoundsCheck(Node* node, GPRReg baseGPR, GPRReg indexGPR)
{
    JITCompiler::Jump jump = jumpForTypedArrayOutOfBounds(node, baseGPR, indexGPR);
    if (!jump.isSet())
        return;
    speculationCheck(OutOfBounds, JSValueRegs(), 0, jump);
}

JITCompiler::Jump SpeculativeJIT::jumpForTypedArrayIsNeuteredIfOutOfBounds(Node* node, GPRReg base, JITCompiler::Jump outOfBounds)
{
    JITCompiler::Jump done;
    if (outOfBounds.isSet()) {
        done = m_jit.jump();
        if (node->arrayMode().isInBounds())
            speculationCheck(OutOfBounds, JSValueSource(), 0, outOfBounds);
        else {
            outOfBounds.link(&m_jit);

            JITCompiler::Jump notWasteful = m_jit.branch32(
                MacroAssembler::NotEqual,
                MacroAssembler::Address(base, JSArrayBufferView::offsetOfMode()),
                TrustedImm32(WastefulTypedArray));

            JITCompiler::Jump hasNullVector = m_jit.branchTestPtr(
                MacroAssembler::Zero,
                MacroAssembler::Address(base, JSArrayBufferView::offsetOfVector()));
            speculationCheck(Uncountable, JSValueSource(), node, hasNullVector);
            notWasteful.link(&m_jit);
        }
    }
    return done;
}

void SpeculativeJIT::loadFromIntTypedArray(GPRReg storageReg, GPRReg propertyReg, GPRReg resultReg, TypedArrayType type)
{
    switch (elementSize(type)) {
    case 1:
        if (isSigned(type))
            m_jit.load8SignedExtendTo32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesOne), resultReg);
        else
            m_jit.load8(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesOne), resultReg);
        break;
    case 2:
        if (isSigned(type))
            m_jit.load16SignedExtendTo32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesTwo), resultReg);
        else
            m_jit.load16(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesTwo), resultReg);
        break;
    case 4:
        m_jit.load32(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesFour), resultReg);
        break;
    default:
        CRASH();
    }
}

void SpeculativeJIT::setIntTypedArrayLoadResult(Node* node, GPRReg resultReg, TypedArrayType type, bool canSpeculate)
{
    if (elementSize(type) < 4 || isSigned(type)) {
        int32Result(resultReg, node);
        return;
    }
    
    ASSERT(elementSize(type) == 4 && !isSigned(type));
    if (node->shouldSpeculateInt32() && canSpeculate) {
        speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branch32(MacroAssembler::LessThan, resultReg, TrustedImm32(0)));
        int32Result(resultReg, node);
        return;
    }
    
#if USE(JSVALUE64)
    if (node->shouldSpeculateAnyInt()) {
        m_jit.zeroExtend32ToPtr(resultReg, resultReg);
        strictInt52Result(resultReg, node);
        return;
    }
#endif
    
    FPRTemporary fresult(this);
    m_jit.convertInt32ToDouble(resultReg, fresult.fpr());
    JITCompiler::Jump positive = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, resultReg, TrustedImm32(0));
    m_jit.addDouble(JITCompiler::AbsoluteAddress(&AssemblyHelpers::twoToThe32), fresult.fpr());
    positive.link(&m_jit);
    doubleResult(fresult.fpr(), node);
}

void SpeculativeJIT::compileGetByValOnIntTypedArray(Node* node, TypedArrayType type)
{
    ASSERT(isInt(type));
    
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    StorageOperand storage(this, m_graph.varArgChild(node, 2));

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg storageReg = storage.gpr();

    GPRTemporary result(this);
    GPRReg resultReg = result.gpr();

    ASSERT(node->arrayMode().alreadyChecked(m_jit.graph(), node, m_state.forNode(m_graph.varArgChild(node, 0))));

    emitTypedArrayBoundsCheck(node, baseReg, propertyReg);
    loadFromIntTypedArray(storageReg, propertyReg, resultReg, type);
    bool canSpeculate = true;
    setIntTypedArrayLoadResult(node, resultReg, type, canSpeculate);
}

bool SpeculativeJIT::getIntTypedArrayStoreOperand(
    GPRTemporary& value,
    GPRReg property,
#if USE(JSVALUE32_64)
    GPRTemporary& propertyTag,
    GPRTemporary& valueTag,
#endif
    Edge valueUse, JITCompiler::JumpList& slowPathCases, bool isClamped)
{
    bool isAppropriateConstant = false;
    if (valueUse->isConstant()) {
        JSValue jsValue = valueUse->asJSValue();
        SpeculatedType expectedType = typeFilterFor(valueUse.useKind());
        SpeculatedType actualType = speculationFromValue(jsValue);
        isAppropriateConstant = (expectedType | actualType) == expectedType;
    }
    
    if (isAppropriateConstant) {
        JSValue jsValue = valueUse->asJSValue();
        if (!jsValue.isNumber()) {
            terminateSpeculativeExecution(Uncountable, JSValueRegs(), 0);
            return false;
        }
        double d = jsValue.asNumber();
        if (isClamped)
            d = clampDoubleToByte(d);
        GPRTemporary scratch(this);
        GPRReg scratchReg = scratch.gpr();
        m_jit.move(Imm32(toInt32(d)), scratchReg);
        value.adopt(scratch);
    } else {
        switch (valueUse.useKind()) {
        case Int32Use: {
            SpeculateInt32Operand valueOp(this, valueUse);
            GPRTemporary scratch(this);
            GPRReg scratchReg = scratch.gpr();
            m_jit.move(valueOp.gpr(), scratchReg);
            if (isClamped)
                compileClampIntegerToByte(m_jit, scratchReg);
            value.adopt(scratch);
            break;
        }
            
#if USE(JSVALUE64)
        case Int52RepUse: {
            SpeculateStrictInt52Operand valueOp(this, valueUse);
            GPRTemporary scratch(this);
            GPRReg scratchReg = scratch.gpr();
            m_jit.move(valueOp.gpr(), scratchReg);
            if (isClamped) {
                MacroAssembler::Jump inBounds = m_jit.branch64(
                    MacroAssembler::BelowOrEqual, scratchReg, JITCompiler::TrustedImm64(0xff));
                MacroAssembler::Jump tooBig = m_jit.branch64(
                    MacroAssembler::GreaterThan, scratchReg, JITCompiler::TrustedImm64(0xff));
                m_jit.move(TrustedImm32(0), scratchReg);
                MacroAssembler::Jump clamped = m_jit.jump();
                tooBig.link(&m_jit);
                m_jit.move(JITCompiler::TrustedImm32(255), scratchReg);
                clamped.link(&m_jit);
                inBounds.link(&m_jit);
            }
            value.adopt(scratch);
            break;
        }
#endif // USE(JSVALUE64)
            
        case DoubleRepUse: {
            RELEASE_ASSERT(!isAtomicsIntrinsic(m_currentNode->op()));
            if (isClamped) {
                SpeculateDoubleOperand valueOp(this, valueUse);
                GPRTemporary result(this);
                FPRTemporary floatScratch(this);
                FPRReg fpr = valueOp.fpr();
                GPRReg gpr = result.gpr();
                compileClampDoubleToByte(m_jit, gpr, fpr, floatScratch.fpr());
                value.adopt(result);
            } else {
#if USE(JSVALUE32_64)
                GPRTemporary realPropertyTag(this);
                propertyTag.adopt(realPropertyTag);
                GPRReg propertyTagGPR = propertyTag.gpr();

                GPRTemporary realValueTag(this);
                valueTag.adopt(realValueTag);
                GPRReg valueTagGPR = valueTag.gpr();
#endif
                SpeculateDoubleOperand valueOp(this, valueUse);
                GPRTemporary result(this);
                FPRReg fpr = valueOp.fpr();
                GPRReg gpr = result.gpr();
                MacroAssembler::Jump notNaN = m_jit.branchIfNotNaN(fpr);
                m_jit.xorPtr(gpr, gpr);
                MacroAssembler::JumpList fixed(m_jit.jump());
                notNaN.link(&m_jit);

                fixed.append(m_jit.branchTruncateDoubleToInt32(
                    fpr, gpr, MacroAssembler::BranchIfTruncateSuccessful));

#if USE(JSVALUE64)
                m_jit.or64(GPRInfo::tagTypeNumberRegister, property);
                boxDouble(fpr, gpr);
#else
                UNUSED_PARAM(property);
                m_jit.move(TrustedImm32(JSValue::Int32Tag), propertyTagGPR);
                boxDouble(fpr, valueTagGPR, gpr);
#endif
                slowPathCases.append(m_jit.jump());

                fixed.link(&m_jit);
                value.adopt(result);
            }
            break;
        }
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
    return true;
}

void SpeculativeJIT::compilePutByValForIntTypedArray(GPRReg base, GPRReg property, Node* node, TypedArrayType type)
{
    ASSERT(isInt(type));
    
    StorageOperand storage(this, m_jit.graph().varArgChild(node, 3));
    GPRReg storageReg = storage.gpr();
    
    Edge valueUse = m_jit.graph().varArgChild(node, 2);
    
    GPRTemporary value;
#if USE(JSVALUE32_64)
    GPRTemporary propertyTag;
    GPRTemporary valueTag;
#endif

    JITCompiler::JumpList slowPathCases;
    
    bool result = getIntTypedArrayStoreOperand(
        value, property,
#if USE(JSVALUE32_64)
        propertyTag, valueTag,
#endif
        valueUse, slowPathCases, isClamped(type));
    if (!result) {
        noResult(node);
        return;
    }

    GPRReg valueGPR = value.gpr();
#if USE(JSVALUE32_64)
    GPRReg propertyTagGPR = propertyTag.gpr();
    GPRReg valueTagGPR = valueTag.gpr();
#endif

    ASSERT_UNUSED(valueGPR, valueGPR != property);
    ASSERT(valueGPR != base);
    ASSERT(valueGPR != storageReg);
    JITCompiler::Jump outOfBounds = jumpForTypedArrayOutOfBounds(node, base, property);

    switch (elementSize(type)) {
    case 1:
        m_jit.store8(value.gpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesOne));
        break;
    case 2:
        m_jit.store16(value.gpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesTwo));
        break;
    case 4:
        m_jit.store32(value.gpr(), MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesFour));
        break;
    default:
        CRASH();
    }

    JITCompiler::Jump done = jumpForTypedArrayIsNeuteredIfOutOfBounds(node, base, outOfBounds);
    if (done.isSet())
        done.link(&m_jit);

    if (!slowPathCases.empty()) {
#if USE(JSVALUE64)
        if (node->op() == PutByValDirect) {
            addSlowPathGenerator(slowPathCall(
                slowPathCases, this,
                m_jit.isStrictModeFor(node->origin.semantic) ? operationPutByValDirectStrict : operationPutByValDirectNonStrict,
                NoResult, base, property, valueGPR));
        } else {
            addSlowPathGenerator(slowPathCall(
                slowPathCases, this,
                m_jit.isStrictModeFor(node->origin.semantic) ? operationPutByValStrict : operationPutByValNonStrict,
                NoResult, base, property, valueGPR));
        }
#else // not USE(JSVALUE64)
        if (node->op() == PutByValDirect) {
            addSlowPathGenerator(slowPathCall(
                slowPathCases, this,
                m_jit.codeBlock()->isStrictMode() ? operationPutByValDirectCellStrict : operationPutByValDirectCellNonStrict,
                NoResult, base, JSValueRegs(propertyTagGPR, property), JSValueRegs(valueTagGPR, valueGPR)));
        } else {
            addSlowPathGenerator(slowPathCall(
                slowPathCases, this,
                m_jit.codeBlock()->isStrictMode() ? operationPutByValCellStrict : operationPutByValCellNonStrict,
                NoResult, base, JSValueRegs(propertyTagGPR, property), JSValueRegs(valueTagGPR, valueGPR)));
        }
#endif
    }
    
    noResult(node);
}

void SpeculativeJIT::compileGetByValOnFloatTypedArray(Node* node, TypedArrayType type)
{
    ASSERT(isFloat(type));
    
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    StorageOperand storage(this, m_graph.varArgChild(node, 2));

    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    GPRReg storageReg = storage.gpr();

    ASSERT(node->arrayMode().alreadyChecked(m_jit.graph(), node, m_state.forNode(m_graph.varArgChild(node, 0))));

    FPRTemporary result(this);
    FPRReg resultReg = result.fpr();
    emitTypedArrayBoundsCheck(node, baseReg, propertyReg);
    switch (elementSize(type)) {
    case 4:
        m_jit.loadFloat(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesFour), resultReg);
        m_jit.convertFloatToDouble(resultReg, resultReg);
        break;
    case 8: {
        m_jit.loadDouble(MacroAssembler::BaseIndex(storageReg, propertyReg, MacroAssembler::TimesEight), resultReg);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    doubleResult(resultReg, node);
}

void SpeculativeJIT::compilePutByValForFloatTypedArray(GPRReg base, GPRReg property, Node* node, TypedArrayType type)
{
    ASSERT(isFloat(type));
    
    StorageOperand storage(this, m_jit.graph().varArgChild(node, 3));
    GPRReg storageReg = storage.gpr();
    
    Edge baseUse = m_jit.graph().varArgChild(node, 0);
    Edge valueUse = m_jit.graph().varArgChild(node, 2);

    SpeculateDoubleOperand valueOp(this, valueUse);
    FPRTemporary scratch(this);
    FPRReg valueFPR = valueOp.fpr();
    FPRReg scratchFPR = scratch.fpr();

    ASSERT_UNUSED(baseUse, node->arrayMode().alreadyChecked(m_jit.graph(), node, m_state.forNode(baseUse)));
    
    MacroAssembler::Jump outOfBounds = jumpForTypedArrayOutOfBounds(node, base, property);
    
    switch (elementSize(type)) {
    case 4: {
        m_jit.moveDouble(valueFPR, scratchFPR);
        m_jit.convertDoubleToFloat(valueFPR, scratchFPR);
        m_jit.storeFloat(scratchFPR, MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesFour));
        break;
    }
    case 8:
        m_jit.storeDouble(valueFPR, MacroAssembler::BaseIndex(storageReg, property, MacroAssembler::TimesEight));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    JITCompiler::Jump done = jumpForTypedArrayIsNeuteredIfOutOfBounds(node, base, outOfBounds);
    if (done.isSet())
        done.link(&m_jit);
    noResult(node);
}

void SpeculativeJIT::compileGetByValForObjectWithString(Node* node)
{
    SpeculateCellOperand arg1(this, m_graph.varArgChild(node, 0));
    SpeculateCellOperand arg2(this, m_graph.varArgChild(node, 1));

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();

    speculateObject(m_graph.varArgChild(node, 0), arg1GPR);
    speculateString(m_graph.varArgChild(node, 1), arg2GPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationGetByValObjectString, resultRegs, arg1GPR, arg2GPR);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetByValForObjectWithSymbol(Node* node)
{
    SpeculateCellOperand arg1(this, m_graph.varArgChild(node, 0));
    SpeculateCellOperand arg2(this, m_graph.varArgChild(node, 1));

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();

    speculateObject(m_graph.varArgChild(node, 0), arg1GPR);
    speculateSymbol(m_graph.varArgChild(node, 1), arg2GPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationGetByValObjectSymbol, resultRegs, arg1GPR, arg2GPR);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutByValForCellWithString(Node* node, Edge& child1, Edge& child2, Edge& child3)
{
    SpeculateCellOperand arg1(this, child1);
    SpeculateCellOperand arg2(this, child2);
    JSValueOperand arg3(this, child3);

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    JSValueRegs arg3Regs = arg3.jsValueRegs();

    speculateString(child2, arg2GPR);

    flushRegisters();
    callOperation(m_jit.isStrictModeFor(node->origin.semantic) ? operationPutByValCellStringStrict : operationPutByValCellStringNonStrict, arg1GPR, arg2GPR, arg3Regs);
    m_jit.exceptionCheck();

    noResult(node);
}

void SpeculativeJIT::compilePutByValForCellWithSymbol(Node* node, Edge& child1, Edge& child2, Edge& child3)
{
    SpeculateCellOperand arg1(this, child1);
    SpeculateCellOperand arg2(this, child2);
    JSValueOperand arg3(this, child3);

    GPRReg arg1GPR = arg1.gpr();
    GPRReg arg2GPR = arg2.gpr();
    JSValueRegs arg3Regs = arg3.jsValueRegs();

    speculateSymbol(child2, arg2GPR);

    flushRegisters();
    callOperation(m_jit.isStrictModeFor(node->origin.semantic) ? operationPutByValCellSymbolStrict : operationPutByValCellSymbolNonStrict, arg1GPR, arg2GPR, arg3Regs);
    m_jit.exceptionCheck();

    noResult(node);
}

void SpeculativeJIT::compileGetByValWithThis(Node* node)
{
    JSValueOperand base(this, node->child1());
    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueOperand thisValue(this, node->child2());
    JSValueRegs thisValueRegs = thisValue.jsValueRegs();
    JSValueOperand subscript(this, node->child3());
    JSValueRegs subscriptRegs = subscript.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationGetByValWithThis, resultRegs, baseRegs, thisValueRegs, subscriptRegs);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileCheckTypeInfoFlags(Node* node)
{
    SpeculateCellOperand base(this, node->child1());

    GPRReg baseGPR = base.gpr();

    // FIXME: This only works for checking if a single bit is set. If we want to check more
    // than one bit at once, we'll need to fix this:
    // https://bugs.webkit.org/show_bug.cgi?id=185705
    speculationCheck(BadTypeInfoFlags, JSValueRegs(), 0, m_jit.branchTest8(MacroAssembler::Zero, MacroAssembler::Address(baseGPR, JSCell::typeInfoFlagsOffset()), MacroAssembler::TrustedImm32(node->typeInfoOperand())));

    noResult(node);
}

void SpeculativeJIT::compileParseInt(Node* node)
{
    RELEASE_ASSERT(node->child1().useKind() == UntypedUse || node->child1().useKind() == StringUse);
    if (node->child2()) {
        SpeculateInt32Operand radix(this, node->child2());
        GPRReg radixGPR = radix.gpr();
        if (node->child1().useKind() == UntypedUse) {
            JSValueOperand value(this, node->child1());
            JSValueRegs valueRegs = value.jsValueRegs();

            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationParseIntGeneric, resultRegs, valueRegs, radixGPR);
            m_jit.exceptionCheck();
            jsValueResult(resultRegs, node);
            return;
        }

        SpeculateCellOperand value(this, node->child1());
        GPRReg valueGPR = value.gpr();
        speculateString(node->child1(), valueGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationParseIntString, resultRegs, valueGPR, radixGPR);
        m_jit.exceptionCheck();
        jsValueResult(resultRegs, node);
        return;
    }

    if (node->child1().useKind() == UntypedUse) {
        JSValueOperand value(this, node->child1());
        JSValueRegs valueRegs = value.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationParseIntNoRadixGeneric, resultRegs, valueRegs);
        m_jit.exceptionCheck();
        jsValueResult(resultRegs, node);
        return;
    }

    SpeculateCellOperand value(this, node->child1());
    GPRReg valueGPR = value.gpr();
    speculateString(node->child1(), valueGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationParseIntStringNoRadix, resultRegs, valueGPR);
    m_jit.exceptionCheck();
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileOverridesHasInstance(Node* node)
{
    Node* hasInstanceValueNode = node->child2().node();
    JSFunction* defaultHasInstanceFunction = jsCast<JSFunction*>(node->cellOperand()->value());

    MacroAssembler::JumpList notDefault;
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand hasInstanceValue(this, node->child2());
    GPRTemporary result(this);

    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();

    // It would be great if constant folding handled automatically the case where we knew the hasInstance function
    // was a constant. Unfortunately, the folding rule for OverridesHasInstance is in the strength reduction phase
    // since it relies on OSR information. https://bugs.webkit.org/show_bug.cgi?id=154832
    if (!hasInstanceValueNode->isCellConstant() || defaultHasInstanceFunction != hasInstanceValueNode->asCell()) {
        JSValueRegs hasInstanceValueRegs = hasInstanceValue.jsValueRegs();
#if USE(JSVALUE64)
        notDefault.append(m_jit.branchPtr(MacroAssembler::NotEqual, hasInstanceValueRegs.gpr(), TrustedImmPtr(node->cellOperand())));
#else
        notDefault.append(m_jit.branchIfNotCell(hasInstanceValueRegs));
        notDefault.append(m_jit.branchPtr(MacroAssembler::NotEqual, hasInstanceValueRegs.payloadGPR(), TrustedImmPtr(node->cellOperand())));
#endif
    }

    // Check that base 'ImplementsDefaultHasInstance'.
    m_jit.test8(MacroAssembler::Zero, MacroAssembler::Address(baseGPR, JSCell::typeInfoFlagsOffset()), MacroAssembler::TrustedImm32(ImplementsDefaultHasInstance), resultGPR);
    MacroAssembler::Jump done = m_jit.jump();

    if (!notDefault.empty()) {
        notDefault.link(&m_jit);
        m_jit.move(TrustedImm32(1), resultGPR);
    }

    done.link(&m_jit);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileInstanceOfForCells(Node* node, JSValueRegs valueRegs, JSValueRegs prototypeRegs, GPRReg resultGPR, GPRReg scratchGPR, GPRReg scratch2GPR, JITCompiler::Jump slowCase)
{
    CallSiteIndex callSiteIndex = m_jit.addCallSite(node->origin.semantic);
    
    JITInstanceOfGenerator gen(
        m_jit.codeBlock(), node->origin.semantic, callSiteIndex, usedRegisters(), resultGPR,
        valueRegs.payloadGPR(), prototypeRegs.payloadGPR(), scratchGPR, scratch2GPR,
        m_state.forNode(node->child2()).isType(SpecObject | ~SpecCell));
    gen.generateFastPath(m_jit);
    
    JITCompiler::JumpList slowCases;
    slowCases.append(slowCase);
    
    std::unique_ptr<SlowPathGenerator> slowPath = slowPathCall(
        slowCases, this, operationInstanceOfOptimize, resultGPR, gen.stubInfo(), valueRegs,
        prototypeRegs);
    
    m_jit.addInstanceOf(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::compileInstanceOf(Node* node)
{
#if USE(JSVALUE64)
    if (node->child1().useKind() == CellUse
        && node->child2().useKind() == CellUse) {
        SpeculateCellOperand value(this, node->child1());
        SpeculateCellOperand prototype(this, node->child2());
        
        GPRTemporary result(this);
        GPRTemporary scratch(this);
        GPRTemporary scratch2(this);
        
        GPRReg valueGPR = value.gpr();
        GPRReg prototypeGPR = prototype.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg scratchGPR = scratch.gpr();
        GPRReg scratch2GPR = scratch2.gpr();
        
        compileInstanceOfForCells(node, JSValueRegs(valueGPR), JSValueRegs(prototypeGPR), resultGPR, scratchGPR, scratch2GPR);
        
        blessedBooleanResult(resultGPR, node);
        return;
    }
#endif
    
    DFG_ASSERT(m_jit.graph(), node, node->child1().useKind() == UntypedUse);
    DFG_ASSERT(m_jit.graph(), node, node->child2().useKind() == UntypedUse);
    
    JSValueOperand value(this, node->child1());
    JSValueOperand prototype(this, node->child2());
    
    GPRTemporary result(this);
    GPRTemporary scratch(this);
    
    JSValueRegs valueRegs = value.jsValueRegs();
    JSValueRegs prototypeRegs = prototype.jsValueRegs();
    
    GPRReg resultGPR = result.gpr();
    GPRReg scratchGPR = scratch.gpr();
    
    JITCompiler::Jump isCell = m_jit.branchIfCell(valueRegs);
    moveFalseTo(resultGPR);
    
    JITCompiler::Jump done = m_jit.jump();
    
    isCell.link(&m_jit);
    
    JITCompiler::Jump slowCase = m_jit.branchIfNotCell(prototypeRegs);
    
    compileInstanceOfForCells(node, valueRegs, prototypeRegs, resultGPR, scratchGPR, InvalidGPRReg, slowCase);
    
    done.link(&m_jit);
    blessedBooleanResult(resultGPR, node);
    return;
}

void SpeculativeJIT::compileValueBitNot(Node* node)
{
    Edge& child1 = node->child1();

    if (child1.useKind() == BigIntUse) {
        SpeculateCellOperand operand(this, child1);
        GPRReg operandGPR = operand.gpr();

        speculateBigInt(child1, operandGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        callOperation(operationBitNotBigInt, resultGPR, operandGPR);
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);

        return;
    }

    JSValueOperand operand(this, child1);
    JSValueRegs operandRegs = operand.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationValueBitNot, resultRegs, operandRegs);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileBitwiseNot(Node* node)
{
    Edge& child1 = node->child1();

    SpeculateInt32Operand operand(this, child1);
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();

    m_jit.move(operand.gpr(), resultGPR);

    m_jit.not32(resultGPR);

    int32Result(resultGPR, node);
}

template<typename SnippetGenerator, J_JITOperation_EJJ snippetSlowPathFunction>
void SpeculativeJIT::emitUntypedBitOp(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node())) {
        JSValueOperand left(this, leftChild);
        JSValueOperand right(this, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(snippetSlowPathFunction, resultRegs, leftRegs, rightRegs);
        m_jit.exceptionCheck();

        jsValueResult(resultRegs, node);
        return;
    }

    Optional<JSValueOperand> left;
    Optional<JSValueOperand> right;

    JSValueRegs leftRegs;
    JSValueRegs rightRegs;

#if USE(JSVALUE64)
    GPRTemporary result(this);
    JSValueRegs resultRegs = JSValueRegs(result.gpr());
    GPRTemporary scratch(this);
    GPRReg scratchGPR = scratch.gpr();
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs = JSValueRegs(resultPayload.gpr(), resultTag.gpr());
    GPRReg scratchGPR = resultTag.gpr();
#endif

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    // The snippet generator does not support both operands being constant. If the left
    // operand is already const, we'll ignore the right operand's constness.
    if (leftChild->isInt32Constant())
        leftOperand.setConstInt32(leftChild->asInt32());
    else if (rightChild->isInt32Constant())
        rightOperand.setConstInt32(rightChild->asInt32());

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst()) {
        left.emplace(this, leftChild);
        leftRegs = left->jsValueRegs();
    }
    if (!rightOperand.isConst()) {
        right.emplace(this, rightChild);
        rightRegs = right->jsValueRegs();
    }

    SnippetGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, scratchGPR);
    gen.generateFastPath(m_jit);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().append(m_jit.jump());

    gen.slowPathJumpList().link(&m_jit);
    silentSpillAllRegisters(resultRegs);

    if (leftOperand.isConst()) {
        leftRegs = resultRegs;
        m_jit.moveValue(leftChild->asJSValue(), leftRegs);
    } else if (rightOperand.isConst()) {
        rightRegs = resultRegs;
        m_jit.moveValue(rightChild->asJSValue(), rightRegs);
    }

    callOperation(snippetSlowPathFunction, resultRegs, leftRegs, rightRegs);

    silentFillAllRegisters();
    m_jit.exceptionCheck();

    gen.endJumpList().link(&m_jit);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileValueBitwiseOp(Node* node)
{
    NodeType op = node->op();
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (leftChild.useKind() == UntypedUse || rightChild.useKind() == UntypedUse) {
        switch (op) {
        case ValueBitAnd:
            emitUntypedBitOp<JITBitAndGenerator, operationValueBitAnd>(node);
            return;
        case ValueBitXor:
            emitUntypedBitOp<JITBitXorGenerator, operationValueBitXor>(node);
            return;
        case ValueBitOr:
            emitUntypedBitOp<JITBitOrGenerator, operationValueBitOr>(node);
            return;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    
    ASSERT(leftChild.useKind() == BigIntUse && rightChild.useKind() == BigIntUse);
    
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();

    speculateBigInt(leftChild, leftGPR);
    speculateBigInt(rightChild, rightGPR);

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    switch (op) {
    case ValueBitAnd:
        callOperation(operationBitAndBigInt, resultGPR, leftGPR, rightGPR);
        break;
    case ValueBitXor:
        callOperation(operationBitXorBigInt, resultGPR, leftGPR, rightGPR);
        break;
    case ValueBitOr:
        callOperation(operationBitOrBigInt, resultGPR, leftGPR, rightGPR);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileBitwiseOp(Node* node)
{
    NodeType op = node->op();
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (leftChild->isInt32Constant()) {
        SpeculateInt32Operand op2(this, rightChild);
        GPRTemporary result(this, Reuse, op2);

        bitOp(op, leftChild->asInt32(), op2.gpr(), result.gpr());

        int32Result(result.gpr(), node);
        return;
    }

    if (rightChild->isInt32Constant()) {
        SpeculateInt32Operand op1(this, leftChild);
        GPRTemporary result(this, Reuse, op1);

        bitOp(op, rightChild->asInt32(), op1.gpr(), result.gpr());

        int32Result(result.gpr(), node);
        return;
    }

    SpeculateInt32Operand op1(this, leftChild);
    SpeculateInt32Operand op2(this, rightChild);
    GPRTemporary result(this, Reuse, op1, op2);

    GPRReg reg1 = op1.gpr();
    GPRReg reg2 = op2.gpr();
    bitOp(op, reg1, reg2, result.gpr());

    int32Result(result.gpr(), node);
}

void SpeculativeJIT::emitUntypedRightShiftBitOp(Node* node)
{
    J_JITOperation_EJJ snippetSlowPathFunction = node->op() == BitRShift
        ? operationValueBitRShift : operationValueBitURShift;
    JITRightShiftGenerator::ShiftType shiftType = node->op() == BitRShift
        ? JITRightShiftGenerator::SignedShift : JITRightShiftGenerator::UnsignedShift;

    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node())) {
        JSValueOperand left(this, leftChild);
        JSValueOperand right(this, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(snippetSlowPathFunction, resultRegs, leftRegs, rightRegs);
        m_jit.exceptionCheck();

        jsValueResult(resultRegs, node);
        return;
    }

    Optional<JSValueOperand> left;
    Optional<JSValueOperand> right;

    JSValueRegs leftRegs;
    JSValueRegs rightRegs;

    FPRTemporary leftNumber(this);
    FPRReg leftFPR = leftNumber.fpr();

#if USE(JSVALUE64)
    GPRTemporary result(this);
    JSValueRegs resultRegs = JSValueRegs(result.gpr());
    GPRTemporary scratch(this);
    GPRReg scratchGPR = scratch.gpr();
    FPRReg scratchFPR = InvalidFPRReg;
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs = JSValueRegs(resultPayload.gpr(), resultTag.gpr());
    GPRReg scratchGPR = resultTag.gpr();
    FPRTemporary fprScratch(this);
    FPRReg scratchFPR = fprScratch.fpr();
#endif

    SnippetOperand leftOperand;
    SnippetOperand rightOperand;

    // The snippet generator does not support both operands being constant. If the left
    // operand is already const, we'll ignore the right operand's constness.
    if (leftChild->isInt32Constant())
        leftOperand.setConstInt32(leftChild->asInt32());
    else if (rightChild->isInt32Constant())
        rightOperand.setConstInt32(rightChild->asInt32());

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst()) {
        left.emplace(this, leftChild);
        leftRegs = left->jsValueRegs();
    }
    if (!rightOperand.isConst()) {
        right.emplace(this, rightChild);
        rightRegs = right->jsValueRegs();
    }

    JITRightShiftGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs,
        leftFPR, scratchGPR, scratchFPR, shiftType);
    gen.generateFastPath(m_jit);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().append(m_jit.jump());

    gen.slowPathJumpList().link(&m_jit);
    silentSpillAllRegisters(resultRegs);

    if (leftOperand.isConst()) {
        leftRegs = resultRegs;
        m_jit.moveValue(leftChild->asJSValue(), leftRegs);
    } else if (rightOperand.isConst()) {
        rightRegs = resultRegs;
        m_jit.moveValue(rightChild->asJSValue(), rightRegs);
    }

    callOperation(snippetSlowPathFunction, resultRegs, leftRegs, rightRegs);

    silentFillAllRegisters();
    m_jit.exceptionCheck();

    gen.endJumpList().link(&m_jit);
    jsValueResult(resultRegs, node);
    return;
}

void SpeculativeJIT::compileShiftOp(Node* node)
{
    NodeType op = node->op();
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (leftChild.useKind() == UntypedUse || rightChild.useKind() == UntypedUse) {
        switch (op) {
        case BitLShift:
            emitUntypedBitOp<JITLeftShiftGenerator, operationValueBitLShift>(node);
            return;
        case BitRShift:
        case BitURShift:
            emitUntypedRightShiftBitOp(node);
            return;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    if (rightChild->isInt32Constant()) {
        SpeculateInt32Operand op1(this, leftChild);
        GPRTemporary result(this, Reuse, op1);

        shiftOp(op, op1.gpr(), rightChild->asInt32() & 0x1f, result.gpr());

        int32Result(result.gpr(), node);
    } else {
        // Do not allow shift amount to be used as the result, MacroAssembler does not permit this.
        SpeculateInt32Operand op1(this, leftChild);
        SpeculateInt32Operand op2(this, rightChild);
        GPRTemporary result(this, Reuse, op1);

        GPRReg reg1 = op1.gpr();
        GPRReg reg2 = op2.gpr();
        shiftOp(op, reg1, reg2, result.gpr());

        int32Result(result.gpr(), node);
    }
}

void SpeculativeJIT::compileValueAdd(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (node->isBinaryUseKind(BigIntUse)) {
        SpeculateCellOperand left(this, node->child1());
        SpeculateCellOperand right(this, node->child2());
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateBigInt(leftChild, leftGPR);
        speculateBigInt(rightChild, rightGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationAddBigInt, resultGPR, leftGPR, rightGPR);
        m_jit.exceptionCheck();

        cellResult(resultGPR, node);
        return;
    }

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node())) {
        JSValueOperand left(this, leftChild);
        JSValueOperand right(this, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationValueAddNotNumber, resultRegs, leftRegs, rightRegs);
        m_jit.exceptionCheck();
    
        jsValueResult(resultRegs, node);
        return;
    }

#if USE(JSVALUE64)
    bool needsScratchGPRReg = true;
    bool needsScratchFPRReg = false;
#else
    bool needsScratchGPRReg = true;
    bool needsScratchFPRReg = true;
#endif

    CodeBlock* baselineCodeBlock = m_jit.graph().baselineCodeBlockFor(node->origin.semantic);
    ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(node->origin.semantic.bytecodeIndex);
    const Instruction* instruction = baselineCodeBlock->instructions().at(node->origin.semantic.bytecodeIndex).ptr();
    JITAddIC* addIC = m_jit.codeBlock()->addJITAddIC(arithProfile, instruction);
    auto repatchingFunction = operationValueAddOptimize;
    auto nonRepatchingFunction = operationValueAdd;
    
    compileMathIC(node, addIC, needsScratchGPRReg, needsScratchFPRReg, repatchingFunction, nonRepatchingFunction);
}

void SpeculativeJIT::compileValueSub(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (node->binaryUseKind() == UntypedUse) {
#if USE(JSVALUE64)
        bool needsScratchGPRReg = true;
        bool needsScratchFPRReg = false;
#else
        bool needsScratchGPRReg = true;
        bool needsScratchFPRReg = true;
#endif

        CodeBlock* baselineCodeBlock = m_jit.graph().baselineCodeBlockFor(node->origin.semantic);
        ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(node->origin.semantic.bytecodeIndex);
        const Instruction* instruction = baselineCodeBlock->instructions().at(node->origin.semantic.bytecodeIndex).ptr();
        JITSubIC* subIC = m_jit.codeBlock()->addJITSubIC(arithProfile, instruction);
        auto repatchingFunction = operationValueSubOptimize;
        auto nonRepatchingFunction = operationValueSub;

        compileMathIC(node, subIC, needsScratchGPRReg, needsScratchFPRReg, repatchingFunction, nonRepatchingFunction);
        return;
    }

    ASSERT(leftChild.useKind() == BigIntUse && rightChild.useKind() == BigIntUse);

    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();

    speculateBigInt(leftChild, leftGPR);
    speculateBigInt(rightChild, rightGPR);

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    callOperation(operationSubBigInt, resultGPR, leftGPR, rightGPR);

    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

template <typename Generator, typename RepatchingFunction, typename NonRepatchingFunction>
void SpeculativeJIT::compileMathIC(Node* node, JITBinaryMathIC<Generator>* mathIC, bool needsScratchGPRReg, bool needsScratchFPRReg, RepatchingFunction repatchingFunction, NonRepatchingFunction nonRepatchingFunction)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    Optional<JSValueOperand> left;
    Optional<JSValueOperand> right;

    JSValueRegs leftRegs;
    JSValueRegs rightRegs;

    FPRTemporary leftNumber(this);
    FPRTemporary rightNumber(this);
    FPRReg leftFPR = leftNumber.fpr();
    FPRReg rightFPR = rightNumber.fpr();

    GPRReg scratchGPR = InvalidGPRReg;
    FPRReg scratchFPR = InvalidFPRReg;

    Optional<FPRTemporary> fprScratch;
    if (needsScratchFPRReg) {
        fprScratch.emplace(this);
        scratchFPR = fprScratch->fpr();
    }

#if USE(JSVALUE64)
    Optional<GPRTemporary> gprScratch;
    if (needsScratchGPRReg) {
        gprScratch.emplace(this);
        scratchGPR = gprScratch->gpr();
    }
    GPRTemporary result(this);
    JSValueRegs resultRegs = JSValueRegs(result.gpr());
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs = JSValueRegs(resultPayload.gpr(), resultTag.gpr());
    if (needsScratchGPRReg)
        scratchGPR = resultRegs.tagGPR();
#endif

    SnippetOperand leftOperand(m_state.forNode(leftChild).resultType());
    SnippetOperand rightOperand(m_state.forNode(rightChild).resultType());

    // The snippet generator does not support both operands being constant. If the left
    // operand is already const, we'll ignore the right operand's constness.
    if (leftChild->isInt32Constant())
        leftOperand.setConstInt32(leftChild->asInt32());
    else if (rightChild->isInt32Constant())
        rightOperand.setConstInt32(rightChild->asInt32());

    ASSERT(!leftOperand.isConst() || !rightOperand.isConst());
    ASSERT(!(Generator::isLeftOperandValidConstant(leftOperand) && Generator::isRightOperandValidConstant(rightOperand)));

    if (!Generator::isLeftOperandValidConstant(leftOperand)) {
        left.emplace(this, leftChild);
        leftRegs = left->jsValueRegs();
    }
    if (!Generator::isRightOperandValidConstant(rightOperand)) {
        right.emplace(this, rightChild);
        rightRegs = right->jsValueRegs();
    }

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = m_jit.label();
#endif

    Box<MathICGenerationState> addICGenerationState = Box<MathICGenerationState>::create();
    mathIC->m_generator = Generator(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs, leftFPR, rightFPR, scratchGPR, scratchFPR);

    bool shouldEmitProfiling = false;
    bool generatedInline = mathIC->generateInline(m_jit, *addICGenerationState, shouldEmitProfiling);
    if (generatedInline) {
        ASSERT(!addICGenerationState->slowPathJumps.empty());

        Vector<SilentRegisterSavePlan> savePlans;
        silentSpillAllRegistersImpl(false, savePlans, resultRegs);

        auto done = m_jit.label();

        addSlowPathGeneratorLambda([=, savePlans = WTFMove(savePlans)] () {
            addICGenerationState->slowPathJumps.link(&m_jit);
            addICGenerationState->slowPathStart = m_jit.label();
#if ENABLE(MATH_IC_STATS)
            auto slowPathStart = m_jit.label();
#endif

            silentSpill(savePlans);

            auto innerLeftRegs = leftRegs;
            auto innerRightRegs = rightRegs;
            if (Generator::isLeftOperandValidConstant(leftOperand)) {
                innerLeftRegs = resultRegs;
                m_jit.moveValue(leftChild->asJSValue(), innerLeftRegs);
            } else if (Generator::isRightOperandValidConstant(rightOperand)) {
                innerRightRegs = resultRegs;
                m_jit.moveValue(rightChild->asJSValue(), innerRightRegs);
            }

            if (addICGenerationState->shouldSlowPathRepatch)
                addICGenerationState->slowPathCall = callOperation(bitwise_cast<J_JITOperation_EJJMic>(repatchingFunction), resultRegs, innerLeftRegs, innerRightRegs, TrustedImmPtr(mathIC));
            else
                addICGenerationState->slowPathCall = callOperation(nonRepatchingFunction, resultRegs, innerLeftRegs, innerRightRegs);

            silentFill(savePlans);
            m_jit.exceptionCheck();
            m_jit.jump().linkTo(done, &m_jit);

            m_jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                mathIC->finalizeInlineCode(*addICGenerationState, linkBuffer);
            });

#if ENABLE(MATH_IC_STATS)
            auto slowPathEnd = m_jit.label();
            m_jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                size_t size = static_cast<char*>(linkBuffer.locationOf(slowPathEnd).executableAddress()) - static_cast<char*>(linkBuffer.locationOf(slowPathStart).executableAddress());
                mathIC->m_generatedCodeSize += size;
            });
#endif

        });
    } else {
        if (Generator::isLeftOperandValidConstant(leftOperand)) {
            left.emplace(this, leftChild);
            leftRegs = left->jsValueRegs();
        } else if (Generator::isRightOperandValidConstant(rightOperand)) {
            right.emplace(this, rightChild);
            rightRegs = right->jsValueRegs();
        }

        flushRegisters();
        callOperation(nonRepatchingFunction, resultRegs, leftRegs, rightRegs);
        m_jit.exceptionCheck();
    }

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = m_jit.label();
    m_jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = static_cast<char*>(linkBuffer.locationOf(inlineEnd).executableAddress()) - static_cast<char*>(linkBuffer.locationOf(inlineStart).executableAddress());
        mathIC->m_generatedCodeSize += size;
    });
#endif

    jsValueResult(resultRegs, node);
    return;
}

void SpeculativeJIT::compileInstanceOfCustom(Node* node)
{
    // We could do something smarter here but this case is currently super rare and unless
    // Symbol.hasInstance becomes popular will likely remain that way.

    JSValueOperand value(this, node->child1());
    SpeculateCellOperand constructor(this, node->child2());
    JSValueOperand hasInstanceValue(this, node->child3());
    GPRTemporary result(this);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg constructorGPR = constructor.gpr();
    JSValueRegs hasInstanceRegs = hasInstanceValue.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    MacroAssembler::Jump slowCase = m_jit.jump();

    addSlowPathGenerator(slowPathCall(slowCase, this, operationInstanceOfCustom, resultGPR, valueRegs, constructorGPR, hasInstanceRegs));

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileIsCellWithType(Node* node)
{
    switch (node->child1().useKind()) {
    case UntypedUse: {
        JSValueOperand value(this, node->child1());
        GPRTemporary result(this, Reuse, value, PayloadWord);

        JSValueRegs valueRegs = value.jsValueRegs();
        GPRReg resultGPR = result.gpr();

        JITCompiler::Jump isNotCell = m_jit.branchIfNotCell(valueRegs);

        m_jit.compare8(JITCompiler::Equal,
            JITCompiler::Address(valueRegs.payloadGPR(), JSCell::typeInfoTypeOffset()),
            TrustedImm32(node->queriedType()),
            resultGPR);
        blessBoolean(resultGPR);
        JITCompiler::Jump done = m_jit.jump();

        isNotCell.link(&m_jit);
        moveFalseTo(resultGPR);

        done.link(&m_jit);
        blessedBooleanResult(resultGPR, node);
        return;
    }

    case CellUse: {
        SpeculateCellOperand cell(this, node->child1());
        GPRTemporary result(this, Reuse, cell);

        GPRReg cellGPR = cell.gpr();
        GPRReg resultGPR = result.gpr();

        m_jit.compare8(JITCompiler::Equal,
            JITCompiler::Address(cellGPR, JSCell::typeInfoTypeOffset()),
            TrustedImm32(node->queriedType()),
            resultGPR);
        blessBoolean(resultGPR);
        blessedBooleanResult(resultGPR, node);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileIsTypedArrayView(Node* node)
{
    JSValueOperand value(this, node->child1());
    GPRTemporary result(this, Reuse, value, PayloadWord);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    JITCompiler::Jump isNotCell = m_jit.branchIfNotCell(valueRegs);

    m_jit.load8(JITCompiler::Address(valueRegs.payloadGPR(), JSCell::typeInfoTypeOffset()), resultGPR);
    m_jit.sub32(TrustedImm32(FirstTypedArrayType), resultGPR);
    m_jit.compare32(JITCompiler::Below,
        resultGPR,
        TrustedImm32(NumberOfTypedArrayTypesExcludingDataView),
        resultGPR);
    blessBoolean(resultGPR);
    JITCompiler::Jump done = m_jit.jump();

    isNotCell.link(&m_jit);
    moveFalseTo(resultGPR);

    done.link(&m_jit);
    blessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileToObjectOrCallObjectConstructor(Node* node)
{
    RELEASE_ASSERT(node->child1().useKind() == UntypedUse);

    JSValueOperand value(this, node->child1());
    GPRTemporary result(this, Reuse, value, PayloadWord);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    MacroAssembler::JumpList slowCases;
    slowCases.append(m_jit.branchIfNotCell(valueRegs));
    slowCases.append(m_jit.branchIfNotObject(valueRegs.payloadGPR()));
    m_jit.move(valueRegs.payloadGPR(), resultGPR);

    if (node->op() == ToObject)
        addSlowPathGenerator(slowPathCall(slowCases, this, operationToObject, resultGPR, m_jit.graph().globalObjectFor(node->origin.semantic), valueRegs, identifierUID(node->identifierNumber())));
    else
        addSlowPathGenerator(slowPathCall(slowCases, this, operationCallObjectConstructor, resultGPR, TrustedImmPtr(node->cellOperand()), valueRegs));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileArithAdd(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        ASSERT(!shouldCheckNegativeZero(node->arithMode()));

        if (node->child2()->isInt32Constant()) {
            SpeculateInt32Operand op1(this, node->child1());
            GPRTemporary result(this, Reuse, op1);

            GPRReg gpr1 = op1.gpr();
            int32_t imm2 = node->child2()->asInt32();
            GPRReg gprResult = result.gpr();

            if (!shouldCheckOverflow(node->arithMode())) {
                m_jit.add32(Imm32(imm2), gpr1, gprResult);
                int32Result(gprResult, node);
                return;
            }

            MacroAssembler::Jump check = m_jit.branchAdd32(MacroAssembler::Overflow, gpr1, Imm32(imm2), gprResult);
            if (gpr1 == gprResult) {
                speculationCheck(Overflow, JSValueRegs(), 0, check,
                    SpeculationRecovery(SpeculativeAddImmediate, gpr1, imm2));
            } else
                speculationCheck(Overflow, JSValueRegs(), 0, check);

            int32Result(gprResult, node);
            return;
        }
                
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this, Reuse, op1, op2);

        GPRReg gpr1 = op1.gpr();
        GPRReg gpr2 = op2.gpr();
        GPRReg gprResult = result.gpr();

        if (!shouldCheckOverflow(node->arithMode()))
            m_jit.add32(gpr1, gpr2, gprResult);
        else {
            MacroAssembler::Jump check = m_jit.branchAdd32(MacroAssembler::Overflow, gpr1, gpr2, gprResult);
                
            if (gpr1 == gprResult)
                speculationCheck(Overflow, JSValueRegs(), 0, check, SpeculationRecovery(SpeculativeAdd, gprResult, gpr2));
            else if (gpr2 == gprResult)
                speculationCheck(Overflow, JSValueRegs(), 0, check, SpeculationRecovery(SpeculativeAdd, gprResult, gpr1));
            else
                speculationCheck(Overflow, JSValueRegs(), 0, check);
        }

        int32Result(gprResult, node);
        return;
    }
        
#if USE(JSVALUE64)
    case Int52RepUse: {
        ASSERT(shouldCheckOverflow(node->arithMode()));
        ASSERT(!shouldCheckNegativeZero(node->arithMode()));

        // Will we need an overflow check? If we can prove that neither input can be
        // Int52 then the overflow check will not be necessary.
        if (!m_state.forNode(node->child1()).couldBeType(SpecInt52Only)
            && !m_state.forNode(node->child2()).couldBeType(SpecInt52Only)) {
            SpeculateWhicheverInt52Operand op1(this, node->child1());
            SpeculateWhicheverInt52Operand op2(this, node->child2(), op1);
            GPRTemporary result(this, Reuse, op1);
            m_jit.add64(op1.gpr(), op2.gpr(), result.gpr());
            int52Result(result.gpr(), node, op1.format());
            return;
        }
        
        SpeculateInt52Operand op1(this, node->child1());
        SpeculateInt52Operand op2(this, node->child2());
        GPRTemporary result(this);
        m_jit.move(op1.gpr(), result.gpr());
        speculationCheck(
            Int52Overflow, JSValueRegs(), 0,
            m_jit.branchAdd64(MacroAssembler::Overflow, op2.gpr(), result.gpr()));
        int52Result(result.gpr(), node);
        return;
    }
#endif // USE(JSVALUE64)
    
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1, op2);

        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        m_jit.addDouble(reg1, reg2, result.fpr());

        doubleResult(result.fpr(), node);
        return;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileArithAbs(Node* node)
{
    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateStrictInt32Operand op1(this, node->child1());
        GPRTemporary result(this, Reuse, op1);
        GPRTemporary scratch(this);

        m_jit.move(op1.gpr(), result.gpr());
        m_jit.rshift32(result.gpr(), MacroAssembler::TrustedImm32(31), scratch.gpr());
        m_jit.add32(scratch.gpr(), result.gpr());
        m_jit.xor32(scratch.gpr(), result.gpr());
        if (shouldCheckOverflow(node->arithMode()))
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(MacroAssembler::Signed, result.gpr()));
        int32Result(result.gpr(), node);
        break;
    }

    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRTemporary result(this);

        m_jit.absDouble(op1.fpr(), result.fpr());
        doubleResult(result.fpr(), node);
        break;
    }

    default: {
        DFG_ASSERT(m_jit.graph(), node, node->child1().useKind() == UntypedUse, node->child1().useKind());
        JSValueOperand op1(this, node->child1());
        JSValueRegs op1Regs = op1.jsValueRegs();
        flushRegisters();
        FPRResult result(this);
        callOperation(operationArithAbs, result.fpr(), op1Regs);
        m_jit.exceptionCheck();
        doubleResult(result.fpr(), node);
        break;
    }
    }
}

void SpeculativeJIT::compileArithClz32(Node* node)
{
    if (node->child1().useKind() == Int32Use || node->child1().useKind() == KnownInt32Use) {
        SpeculateInt32Operand value(this, node->child1());
        GPRTemporary result(this, Reuse, value);
        GPRReg valueReg = value.gpr();
        GPRReg resultReg = result.gpr();
        m_jit.countLeadingZeros32(valueReg, resultReg);
        int32Result(resultReg, node);
        return;
    }
    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    GPRTemporary result(this);
    GPRReg resultReg = result.gpr();
    flushRegisters();
    callOperation(operationArithClz32, resultReg, op1Regs);
    m_jit.exceptionCheck();
    int32Result(resultReg, node);
}

void SpeculativeJIT::compileArithDoubleUnaryOp(Node* node, double (*doubleFunction)(double), double (*operation)(ExecState*, EncodedJSValue))
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRReg op1FPR = op1.fpr();

        flushRegisters();

        FPRResult result(this);
        callOperation(doubleFunction, result.fpr(), op1FPR);

        doubleResult(result.fpr(), node);
        return;
    }

    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    flushRegisters();
    FPRResult result(this);
    callOperation(operation, result.fpr(), op1Regs);
    m_jit.exceptionCheck();
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileArithSub(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        ASSERT(!shouldCheckNegativeZero(node->arithMode()));
        
        if (node->child2()->isInt32Constant()) {
            SpeculateInt32Operand op1(this, node->child1());
            int32_t imm2 = node->child2()->asInt32();
            GPRTemporary result(this);

            if (!shouldCheckOverflow(node->arithMode())) {
                m_jit.move(op1.gpr(), result.gpr());
                m_jit.sub32(Imm32(imm2), result.gpr());
            } else {
                GPRTemporary scratch(this);
                speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchSub32(MacroAssembler::Overflow, op1.gpr(), Imm32(imm2), result.gpr(), scratch.gpr()));
            }

            int32Result(result.gpr(), node);
            return;
        }
            
        if (node->child1()->isInt32Constant()) {
            int32_t imm1 = node->child1()->asInt32();
            SpeculateInt32Operand op2(this, node->child2());
            GPRTemporary result(this);
                
            m_jit.move(Imm32(imm1), result.gpr());
            if (!shouldCheckOverflow(node->arithMode()))
                m_jit.sub32(op2.gpr(), result.gpr());
            else
                speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchSub32(MacroAssembler::Overflow, op2.gpr(), result.gpr()));
                
            int32Result(result.gpr(), node);
            return;
        }
            
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this);

        if (!shouldCheckOverflow(node->arithMode())) {
            m_jit.move(op1.gpr(), result.gpr());
            m_jit.sub32(op2.gpr(), result.gpr());
        } else
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchSub32(MacroAssembler::Overflow, op1.gpr(), op2.gpr(), result.gpr()));

        int32Result(result.gpr(), node);
        return;
    }
        
#if USE(JSVALUE64)
    case Int52RepUse: {
        ASSERT(shouldCheckOverflow(node->arithMode()));
        ASSERT(!shouldCheckNegativeZero(node->arithMode()));

        // Will we need an overflow check? If we can prove that neither input can be
        // Int52 then the overflow check will not be necessary.
        if (!m_state.forNode(node->child1()).couldBeType(SpecInt52Only)
            && !m_state.forNode(node->child2()).couldBeType(SpecInt52Only)) {
            SpeculateWhicheverInt52Operand op1(this, node->child1());
            SpeculateWhicheverInt52Operand op2(this, node->child2(), op1);
            GPRTemporary result(this, Reuse, op1);
            m_jit.move(op1.gpr(), result.gpr());
            m_jit.sub64(op2.gpr(), result.gpr());
            int52Result(result.gpr(), node, op1.format());
            return;
        }
        
        SpeculateInt52Operand op1(this, node->child1());
        SpeculateInt52Operand op2(this, node->child2());
        GPRTemporary result(this);
        m_jit.move(op1.gpr(), result.gpr());
        speculationCheck(
            Int52Overflow, JSValueRegs(), 0,
            m_jit.branchSub64(MacroAssembler::Overflow, op2.gpr(), result.gpr()));
        int52Result(result.gpr(), node);
        return;
    }
#endif // USE(JSVALUE64)

    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1);

        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        m_jit.subDouble(reg1, reg2, result.fpr());

        doubleResult(result.fpr(), node);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileValueNegate(Node* node)
{
    CodeBlock* baselineCodeBlock = m_jit.graph().baselineCodeBlockFor(node->origin.semantic);
    ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(node->origin.semantic.bytecodeIndex);
    const Instruction* instruction = baselineCodeBlock->instructions().at(node->origin.semantic.bytecodeIndex).ptr();
    JITNegIC* negIC = m_jit.codeBlock()->addJITNegIC(arithProfile, instruction);
    auto repatchingFunction = operationArithNegateOptimize;
    auto nonRepatchingFunction = operationArithNegate;
    bool needsScratchGPRReg = true;
    compileMathIC(node, negIC, needsScratchGPRReg, repatchingFunction, nonRepatchingFunction);
}

void SpeculativeJIT::compileArithNegate(Node* node)
{
    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateInt32Operand op1(this, node->child1());
        GPRTemporary result(this);

        m_jit.move(op1.gpr(), result.gpr());

        // Note: there is no notion of being not used as a number, but someone
        // caring about negative zero.
        
        if (!shouldCheckOverflow(node->arithMode()))
            m_jit.neg32(result.gpr());
        else if (!shouldCheckNegativeZero(node->arithMode()))
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchNeg32(MacroAssembler::Overflow, result.gpr()));
        else {
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(MacroAssembler::Zero, result.gpr(), TrustedImm32(0x7fffffff)));
            m_jit.neg32(result.gpr());
        }

        int32Result(result.gpr(), node);
        return;
    }

#if USE(JSVALUE64)
    case Int52RepUse: {
        ASSERT(shouldCheckOverflow(node->arithMode()));
        
        if (!m_state.forNode(node->child1()).couldBeType(SpecInt52Only)) {
            SpeculateWhicheverInt52Operand op1(this, node->child1());
            GPRTemporary result(this);
            GPRReg op1GPR = op1.gpr();
            GPRReg resultGPR = result.gpr();
            m_jit.move(op1GPR, resultGPR);
            m_jit.neg64(resultGPR);
            if (shouldCheckNegativeZero(node->arithMode())) {
                speculationCheck(
                    NegativeZero, JSValueRegs(), 0,
                    m_jit.branchTest64(MacroAssembler::Zero, resultGPR));
            }
            int52Result(resultGPR, node, op1.format());
            return;
        }
        
        SpeculateInt52Operand op1(this, node->child1());
        GPRTemporary result(this);
        GPRReg op1GPR = op1.gpr();
        GPRReg resultGPR = result.gpr();
        m_jit.move(op1GPR, resultGPR);
        speculationCheck(
            Int52Overflow, JSValueRegs(), 0,
            m_jit.branchNeg64(MacroAssembler::Overflow, resultGPR));
        if (shouldCheckNegativeZero(node->arithMode())) {
            speculationCheck(
                NegativeZero, JSValueRegs(), 0,
                m_jit.branchTest64(MacroAssembler::Zero, resultGPR));
        }
        int52Result(resultGPR, node);
        return;
    }
#endif // USE(JSVALUE64)
        
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRTemporary result(this);
        
        m_jit.negateDouble(op1.fpr(), result.fpr());
        
        doubleResult(result.fpr(), node);
        return;
    }
        
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }
}

template <typename Generator, typename RepatchingFunction, typename NonRepatchingFunction>
void SpeculativeJIT::compileMathIC(Node* node, JITUnaryMathIC<Generator>* mathIC, bool needsScratchGPRReg, RepatchingFunction repatchingFunction, NonRepatchingFunction nonRepatchingFunction)
{
    GPRReg scratchGPR = InvalidGPRReg;
    Optional<GPRTemporary> gprScratch;
    if (needsScratchGPRReg) {
        gprScratch.emplace(this);
        scratchGPR = gprScratch->gpr();
    }
    JSValueOperand childOperand(this, node->child1());
    JSValueRegs childRegs = childOperand.jsValueRegs();
#if USE(JSVALUE64)
    GPRTemporary result(this, Reuse, childOperand);
    JSValueRegs resultRegs(result.gpr());
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs(resultPayload.gpr(), resultTag.gpr());
#endif

#if ENABLE(MATH_IC_STATS)
    auto inlineStart = m_jit.label();
#endif

    Box<MathICGenerationState> icGenerationState = Box<MathICGenerationState>::create();
    mathIC->m_generator = Generator(resultRegs, childRegs, scratchGPR);

    bool shouldEmitProfiling = false;
    bool generatedInline = mathIC->generateInline(m_jit, *icGenerationState, shouldEmitProfiling);
    if (generatedInline) {
        ASSERT(!icGenerationState->slowPathJumps.empty());

        Vector<SilentRegisterSavePlan> savePlans;
        silentSpillAllRegistersImpl(false, savePlans, resultRegs);

        auto done = m_jit.label();

        addSlowPathGeneratorLambda([=, savePlans = WTFMove(savePlans)] () {
            icGenerationState->slowPathJumps.link(&m_jit);
            icGenerationState->slowPathStart = m_jit.label();
#if ENABLE(MATH_IC_STATS)
            auto slowPathStart = m_jit.label();
#endif

            silentSpill(savePlans);

            if (icGenerationState->shouldSlowPathRepatch)
                icGenerationState->slowPathCall = callOperation(bitwise_cast<J_JITOperation_EJMic>(repatchingFunction), resultRegs, childRegs, TrustedImmPtr(mathIC));
            else
                icGenerationState->slowPathCall = callOperation(nonRepatchingFunction, resultRegs, childRegs);

            silentFill(savePlans);
            m_jit.exceptionCheck();
            m_jit.jump().linkTo(done, &m_jit);

            m_jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                mathIC->finalizeInlineCode(*icGenerationState, linkBuffer);
            });

#if ENABLE(MATH_IC_STATS)
            auto slowPathEnd = m_jit.label();
            m_jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                size_t size = static_cast<char*>(linkBuffer.locationOf(slowPathEnd).executableAddress()) - static_cast<char*>(linkBuffer.locationOf(slowPathStart).executableAddress());
                mathIC->m_generatedCodeSize += size;
            });
#endif

        });
    } else {
        flushRegisters();
        callOperation(nonRepatchingFunction, resultRegs, childRegs);
        m_jit.exceptionCheck();
    }

#if ENABLE(MATH_IC_STATS)
    auto inlineEnd = m_jit.label();
    m_jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        size_t size = static_cast<char*>(linkBuffer.locationOf(inlineEnd).executableAddress()) - static_cast<char*>(linkBuffer.locationOf(inlineStart).executableAddress());
        mathIC->m_generatedCodeSize += size;
    });
#endif

    jsValueResult(resultRegs, node);
    return;
}

void SpeculativeJIT::compileValueMul(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (leftChild.useKind() == BigIntUse && rightChild.useKind() == BigIntUse) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateBigInt(leftChild, leftGPR);
        speculateBigInt(rightChild, rightGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        callOperation(operationMulBigInt, resultGPR, leftGPR, rightGPR);

        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
        return;
    }

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node())) {
        JSValueOperand left(this, leftChild);
        JSValueOperand right(this, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationValueMul, resultRegs, leftRegs, rightRegs);
        m_jit.exceptionCheck();

        jsValueResult(resultRegs, node);
        return;
    }

    bool needsScratchGPRReg = true;
#if USE(JSVALUE64)
    bool needsScratchFPRReg = false;
#else
    bool needsScratchFPRReg = true;
#endif

    CodeBlock* baselineCodeBlock = m_jit.graph().baselineCodeBlockFor(node->origin.semantic);
    ArithProfile* arithProfile = baselineCodeBlock->arithProfileForBytecodeOffset(node->origin.semantic.bytecodeIndex);
    const Instruction* instruction = baselineCodeBlock->instructions().at(node->origin.semantic.bytecodeIndex).ptr();
    JITMulIC* mulIC = m_jit.codeBlock()->addJITMulIC(arithProfile, instruction);
    auto repatchingFunction = operationValueMulOptimize;
    auto nonRepatchingFunction = operationValueMul;

    compileMathIC(node, mulIC, needsScratchGPRReg, needsScratchFPRReg, repatchingFunction, nonRepatchingFunction);
}

void SpeculativeJIT::compileArithMul(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        if (node->child2()->isInt32Constant()) {
            SpeculateInt32Operand op1(this, node->child1());
            GPRTemporary result(this);

            int32_t imm = node->child2()->asInt32();
            GPRReg op1GPR = op1.gpr();
            GPRReg resultGPR = result.gpr();

            if (!shouldCheckOverflow(node->arithMode()))
                m_jit.mul32(Imm32(imm), op1GPR, resultGPR);
            else {
                speculationCheck(Overflow, JSValueRegs(), 0,
                    m_jit.branchMul32(MacroAssembler::Overflow, op1GPR, Imm32(imm), resultGPR));
            }

            // The only way to create negative zero with a constant is:
            // -negative-op1 * 0.
            // -zero-op1 * negative constant.
            if (shouldCheckNegativeZero(node->arithMode())) {
                if (!imm)
                    speculationCheck(NegativeZero, JSValueRegs(), 0, m_jit.branchTest32(MacroAssembler::Signed, op1GPR));
                else if (imm < 0) {
                    if (shouldCheckOverflow(node->arithMode()))
                        speculationCheck(NegativeZero, JSValueRegs(), 0, m_jit.branchTest32(MacroAssembler::Zero, resultGPR));
                    else
                        speculationCheck(NegativeZero, JSValueRegs(), 0, m_jit.branchTest32(MacroAssembler::Zero, op1GPR));
                }
            }

            int32Result(resultGPR, node);
            return;
        }
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this);

        GPRReg reg1 = op1.gpr();
        GPRReg reg2 = op2.gpr();

        // We can perform truncated multiplications if we get to this point, because if the
        // fixup phase could not prove that it would be safe, it would have turned us into
        // a double multiplication.
        if (!shouldCheckOverflow(node->arithMode()))
            m_jit.mul32(reg1, reg2, result.gpr());
        else {
            speculationCheck(
                Overflow, JSValueRegs(), 0,
                m_jit.branchMul32(MacroAssembler::Overflow, reg1, reg2, result.gpr()));
        }
            
        // Check for negative zero, if the users of this node care about such things.
        if (shouldCheckNegativeZero(node->arithMode())) {
            MacroAssembler::Jump resultNonZero = m_jit.branchTest32(MacroAssembler::NonZero, result.gpr());
            speculationCheck(NegativeZero, JSValueRegs(), 0, m_jit.branchTest32(MacroAssembler::Signed, reg1));
            speculationCheck(NegativeZero, JSValueRegs(), 0, m_jit.branchTest32(MacroAssembler::Signed, reg2));
            resultNonZero.link(&m_jit);
        }

        int32Result(result.gpr(), node);
        return;
    }

#if USE(JSVALUE64)
    case Int52RepUse: {
        ASSERT(shouldCheckOverflow(node->arithMode()));
        
        // This is super clever. We want to do an int52 multiplication and check the
        // int52 overflow bit. There is no direct hardware support for this, but we do
        // have the ability to do an int64 multiplication and check the int64 overflow
        // bit. We leverage that. Consider that a, b are int52 numbers inside int64
        // registers, with the high 12 bits being sign-extended. We can do:
        //
        //     (a * (b << 12))
        //
        // This will give us a left-shifted int52 (value is in high 52 bits, low 16
        // bits are zero) plus the int52 overflow bit. I.e. whether this 64-bit
        // multiplication overflows is identical to whether the 'a * b' 52-bit
        // multiplication overflows.
        //
        // In our nomenclature, this is:
        //
        //     strictInt52(a) * int52(b) => int52
        //
        // That is "strictInt52" means unshifted and "int52" means left-shifted by 16
        // bits.
        //
        // We don't care which of op1 or op2 serves as the left-shifted operand, so
        // we just do whatever is more convenient for op1 and have op2 do the
        // opposite. This ensures that we do at most one shift.

        SpeculateWhicheverInt52Operand op1(this, node->child1());
        SpeculateWhicheverInt52Operand op2(this, node->child2(), OppositeShift, op1);
        GPRTemporary result(this);
        
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
        GPRReg resultGPR = result.gpr();
        
        m_jit.move(op1GPR, resultGPR);
        speculationCheck(
            Int52Overflow, JSValueRegs(), 0,
            m_jit.branchMul64(MacroAssembler::Overflow, op2GPR, resultGPR));
        
        if (shouldCheckNegativeZero(node->arithMode())) {
            MacroAssembler::Jump resultNonZero = m_jit.branchTest64(
                MacroAssembler::NonZero, resultGPR);
            speculationCheck(
                NegativeZero, JSValueRegs(), 0,
                m_jit.branch64(MacroAssembler::LessThan, op1GPR, TrustedImm32(0)));
            speculationCheck(
                NegativeZero, JSValueRegs(), 0,
                m_jit.branch64(MacroAssembler::LessThan, op2GPR, TrustedImm32(0)));
            resultNonZero.link(&m_jit);
        }
        
        int52Result(resultGPR, node);
        return;
    }
#endif // USE(JSVALUE64)
        
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1, op2);
        
        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        
        m_jit.mulDouble(reg1, reg2, result.fpr());
        
        doubleResult(result.fpr(), node);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileValueDiv(Node* node)
{
    Edge& leftChild = node->child1();
    Edge& rightChild = node->child2();

    if (leftChild.useKind() == BigIntUse && rightChild.useKind() == BigIntUse) {
        SpeculateCellOperand left(this, leftChild);
        SpeculateCellOperand right(this, rightChild);
        GPRReg leftGPR = left.gpr();
        GPRReg rightGPR = right.gpr();

        speculateBigInt(leftChild, leftGPR);
        speculateBigInt(rightChild, rightGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        callOperation(operationDivBigInt, resultGPR, leftGPR, rightGPR);

        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
        return;
    }

    if (isKnownNotNumber(leftChild.node()) || isKnownNotNumber(rightChild.node())) {
        JSValueOperand left(this, leftChild);
        JSValueOperand right(this, rightChild);
        JSValueRegs leftRegs = left.jsValueRegs();
        JSValueRegs rightRegs = right.jsValueRegs();

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationValueDiv, resultRegs, leftRegs, rightRegs);
        m_jit.exceptionCheck();

        jsValueResult(resultRegs, node);
        return;
    }

    Optional<JSValueOperand> left;
    Optional<JSValueOperand> right;

    JSValueRegs leftRegs;
    JSValueRegs rightRegs;

    FPRTemporary leftNumber(this);
    FPRTemporary rightNumber(this);
    FPRReg leftFPR = leftNumber.fpr();
    FPRReg rightFPR = rightNumber.fpr();
    FPRTemporary fprScratch(this);
    FPRReg scratchFPR = fprScratch.fpr();

#if USE(JSVALUE64)
    GPRTemporary result(this);
    JSValueRegs resultRegs = JSValueRegs(result.gpr());
    GPRTemporary scratch(this);
    GPRReg scratchGPR = scratch.gpr();
#else
    GPRTemporary resultTag(this);
    GPRTemporary resultPayload(this);
    JSValueRegs resultRegs = JSValueRegs(resultPayload.gpr(), resultTag.gpr());
    GPRReg scratchGPR = resultTag.gpr();
#endif

    SnippetOperand leftOperand(m_state.forNode(leftChild).resultType());
    SnippetOperand rightOperand(m_state.forNode(rightChild).resultType());

    if (leftChild->isInt32Constant())
        leftOperand.setConstInt32(leftChild->asInt32());
#if USE(JSVALUE64)
    else if (leftChild->isDoubleConstant())
        leftOperand.setConstDouble(leftChild->asNumber());
#endif

    if (leftOperand.isConst()) {
        // The snippet generator only supports 1 argument as a constant.
        // Ignore the rightChild's const-ness.
    } else if (rightChild->isInt32Constant())
        rightOperand.setConstInt32(rightChild->asInt32());
#if USE(JSVALUE64)
    else if (rightChild->isDoubleConstant())
        rightOperand.setConstDouble(rightChild->asNumber());
#endif

    RELEASE_ASSERT(!leftOperand.isConst() || !rightOperand.isConst());

    if (!leftOperand.isConst()) {
        left.emplace(this, leftChild);
        leftRegs = left->jsValueRegs();
    }
    if (!rightOperand.isConst()) {
        right.emplace(this, rightChild);
        rightRegs = right->jsValueRegs();
    }

    JITDivGenerator gen(leftOperand, rightOperand, resultRegs, leftRegs, rightRegs,
        leftFPR, rightFPR, scratchGPR, scratchFPR);
    gen.generateFastPath(m_jit);

    ASSERT(gen.didEmitFastPath());
    gen.endJumpList().append(m_jit.jump());

    gen.slowPathJumpList().link(&m_jit);
    silentSpillAllRegisters(resultRegs);

    if (leftOperand.isConst()) {
        leftRegs = resultRegs;
        m_jit.moveValue(leftChild->asJSValue(), leftRegs);
    }
    if (rightOperand.isConst()) {
        rightRegs = resultRegs;
        m_jit.moveValue(rightChild->asJSValue(), rightRegs);
    }

    callOperation(operationValueDiv, resultRegs, leftRegs, rightRegs);

    silentFillAllRegisters();
    m_jit.exceptionCheck();

    gen.endJumpList().link(&m_jit);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileArithDiv(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
#if CPU(X86) || CPU(X86_64)
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary eax(this, X86Registers::eax);
        GPRTemporary edx(this, X86Registers::edx);
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
    
        GPRReg op2TempGPR;
        GPRReg temp;
        if (op2GPR == X86Registers::eax || op2GPR == X86Registers::edx) {
            op2TempGPR = allocate();
            temp = op2TempGPR;
        } else {
            op2TempGPR = InvalidGPRReg;
            if (op1GPR == X86Registers::eax)
                temp = X86Registers::edx;
            else
                temp = X86Registers::eax;
        }
    
        ASSERT(temp != op1GPR);
        ASSERT(temp != op2GPR);
    
        m_jit.add32(JITCompiler::TrustedImm32(1), op2GPR, temp);
    
        JITCompiler::Jump safeDenominator = m_jit.branch32(JITCompiler::Above, temp, JITCompiler::TrustedImm32(1));
    
        JITCompiler::JumpList done;
        if (shouldCheckOverflow(node->arithMode())) {
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(JITCompiler::Zero, op2GPR));
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branch32(JITCompiler::Equal, op1GPR, TrustedImm32(-2147483647-1)));
        } else {
            // This is the case where we convert the result to an int after we're done, and we
            // already know that the denominator is either -1 or 0. So, if the denominator is
            // zero, then the result should be zero. If the denominator is not zero (i.e. it's
            // -1) and the numerator is -2^31 then the result should be -2^31. Otherwise we
            // are happy to fall through to a normal division, since we're just dividing
            // something by negative 1.
        
            JITCompiler::Jump notZero = m_jit.branchTest32(JITCompiler::NonZero, op2GPR);
            m_jit.move(TrustedImm32(0), eax.gpr());
            done.append(m_jit.jump());
        
            notZero.link(&m_jit);
            JITCompiler::Jump notNeg2ToThe31 =
                m_jit.branch32(JITCompiler::NotEqual, op1GPR, TrustedImm32(-2147483647-1));
            m_jit.zeroExtend32ToPtr(op1GPR, eax.gpr());
            done.append(m_jit.jump());
        
            notNeg2ToThe31.link(&m_jit);
        }
    
        safeDenominator.link(&m_jit);
    
        // If the user cares about negative zero, then speculate that we're not about
        // to produce negative zero.
        if (shouldCheckNegativeZero(node->arithMode())) {
            MacroAssembler::Jump numeratorNonZero = m_jit.branchTest32(MacroAssembler::NonZero, op1GPR);
            speculationCheck(NegativeZero, JSValueRegs(), 0, m_jit.branch32(MacroAssembler::LessThan, op2GPR, TrustedImm32(0)));
            numeratorNonZero.link(&m_jit);
        }
    
        if (op2TempGPR != InvalidGPRReg) {
            m_jit.move(op2GPR, op2TempGPR);
            op2GPR = op2TempGPR;
        }
            
        m_jit.move(op1GPR, eax.gpr());
        m_jit.x86ConvertToDoubleWord32();
        m_jit.x86Div32(op2GPR);
            
        if (op2TempGPR != InvalidGPRReg)
            unlock(op2TempGPR);

        // Check that there was no remainder. If there had been, then we'd be obligated to
        // produce a double result instead.
        if (shouldCheckOverflow(node->arithMode()))
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(JITCompiler::NonZero, edx.gpr()));
        
        done.link(&m_jit);
        int32Result(eax.gpr(), node);
#elif HAVE(ARM_IDIV_INSTRUCTIONS) || CPU(ARM64)
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
        GPRTemporary quotient(this);
        GPRTemporary multiplyAnswer(this);

        // If the user cares about negative zero, then speculate that we're not about
        // to produce negative zero.
        if (shouldCheckNegativeZero(node->arithMode())) {
            MacroAssembler::Jump numeratorNonZero = m_jit.branchTest32(MacroAssembler::NonZero, op1GPR);
            speculationCheck(NegativeZero, JSValueRegs(), 0, m_jit.branch32(MacroAssembler::LessThan, op2GPR, TrustedImm32(0)));
            numeratorNonZero.link(&m_jit);
        }

        if (shouldCheckOverflow(node->arithMode()))
            speculationCheck(Overflow, JSValueRegs(), nullptr, m_jit.branchTest32(MacroAssembler::Zero, op2GPR));

        m_jit.assembler().sdiv<32>(quotient.gpr(), op1GPR, op2GPR);

        // Check that there was no remainder. If there had been, then we'd be obligated to
        // produce a double result instead.
        if (shouldCheckOverflow(node->arithMode())) {
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchMul32(JITCompiler::Overflow, quotient.gpr(), op2GPR, multiplyAnswer.gpr()));
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branch32(JITCompiler::NotEqual, multiplyAnswer.gpr(), op1GPR));
        }

        int32Result(quotient.gpr(), node);
#else
        RELEASE_ASSERT_NOT_REACHED();
#endif
        break;
    }
        
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1);
        
        FPRReg reg1 = op1.fpr();
        FPRReg reg2 = op2.fpr();
        m_jit.divDouble(reg1, reg2, result.fpr());
        
        doubleResult(result.fpr(), node);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileArithFRound(Node* node)
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRTemporary result(this, op1);
        m_jit.convertDoubleToFloat(op1.fpr(), result.fpr());
        m_jit.convertFloatToDouble(result.fpr(), result.fpr());
        doubleResult(result.fpr(), node);
        return;
    }

    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    flushRegisters();
    FPRResult result(this);
    callOperation(operationArithFRound, result.fpr(), op1Regs);
    m_jit.exceptionCheck();
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileArithMod(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        // In the fast path, the dividend value could be the final result
        // (in case of |dividend| < |divisor|), so we speculate it as strict int32.
        SpeculateStrictInt32Operand op1(this, node->child1());
        
        if (node->child2()->isInt32Constant()) {
            int32_t divisor = node->child2()->asInt32();
            if (divisor > 1 && hasOneBitSet(divisor)) {
                unsigned logarithm = WTF::fastLog2(static_cast<uint32_t>(divisor));
                GPRReg dividendGPR = op1.gpr();
                GPRTemporary result(this);
                GPRReg resultGPR = result.gpr();

                // This is what LLVM generates. It's pretty crazy. Here's my
                // attempt at understanding it.
                
                // First, compute either divisor - 1, or 0, depending on whether
                // the dividend is negative:
                //
                // If dividend < 0:  resultGPR = divisor - 1
                // If dividend >= 0: resultGPR = 0
                m_jit.move(dividendGPR, resultGPR);
                m_jit.rshift32(TrustedImm32(31), resultGPR);
                m_jit.urshift32(TrustedImm32(32 - logarithm), resultGPR);
                
                // Add in the dividend, so that:
                //
                // If dividend < 0:  resultGPR = dividend + divisor - 1
                // If dividend >= 0: resultGPR = dividend
                m_jit.add32(dividendGPR, resultGPR);
                
                // Mask so as to only get the *high* bits. This rounds down
                // (towards negative infinity) resultGPR to the nearest multiple
                // of divisor, so that:
                //
                // If dividend < 0:  resultGPR = floor((dividend + divisor - 1) / divisor)
                // If dividend >= 0: resultGPR = floor(dividend / divisor)
                //
                // Note that this can be simplified to:
                //
                // If dividend < 0:  resultGPR = ceil(dividend / divisor)
                // If dividend >= 0: resultGPR = floor(dividend / divisor)
                //
                // Note that if the dividend is negative, resultGPR will also be negative.
                // Regardless of the sign of dividend, resultGPR will be rounded towards
                // zero, because of how things are conditionalized.
                m_jit.and32(TrustedImm32(-divisor), resultGPR);
                
                // Subtract resultGPR from dividendGPR, which yields the remainder:
                //
                // resultGPR = dividendGPR - resultGPR
                m_jit.neg32(resultGPR);
                m_jit.add32(dividendGPR, resultGPR);
                
                if (shouldCheckNegativeZero(node->arithMode())) {
                    // Check that we're not about to create negative zero.
                    JITCompiler::Jump numeratorPositive = m_jit.branch32(JITCompiler::GreaterThanOrEqual, dividendGPR, TrustedImm32(0));
                    speculationCheck(NegativeZero, JSValueRegs(), 0, m_jit.branchTest32(JITCompiler::Zero, resultGPR));
                    numeratorPositive.link(&m_jit);
                }

                int32Result(resultGPR, node);
                return;
            }
        }
        
#if CPU(X86) || CPU(X86_64)
        if (node->child2()->isInt32Constant()) {
            int32_t divisor = node->child2()->asInt32();
            if (divisor && divisor != -1) {
                GPRReg op1Gpr = op1.gpr();

                GPRTemporary eax(this, X86Registers::eax);
                GPRTemporary edx(this, X86Registers::edx);
                GPRTemporary scratch(this);
                GPRReg scratchGPR = scratch.gpr();

                GPRReg op1SaveGPR;
                if (op1Gpr == X86Registers::eax || op1Gpr == X86Registers::edx) {
                    op1SaveGPR = allocate();
                    ASSERT(op1Gpr != op1SaveGPR);
                    m_jit.move(op1Gpr, op1SaveGPR);
                } else
                    op1SaveGPR = op1Gpr;
                ASSERT(op1SaveGPR != X86Registers::eax);
                ASSERT(op1SaveGPR != X86Registers::edx);

                m_jit.move(op1Gpr, eax.gpr());
                m_jit.move(TrustedImm32(divisor), scratchGPR);
                m_jit.x86ConvertToDoubleWord32();
                m_jit.x86Div32(scratchGPR);
                if (shouldCheckNegativeZero(node->arithMode())) {
                    JITCompiler::Jump numeratorPositive = m_jit.branch32(JITCompiler::GreaterThanOrEqual, op1SaveGPR, TrustedImm32(0));
                    speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(JITCompiler::Zero, edx.gpr()));
                    numeratorPositive.link(&m_jit);
                }
            
                if (op1SaveGPR != op1Gpr)
                    unlock(op1SaveGPR);

                int32Result(edx.gpr(), node);
                return;
            }
        }
#endif

        SpeculateInt32Operand op2(this, node->child2());
#if CPU(X86) || CPU(X86_64)
        GPRTemporary eax(this, X86Registers::eax);
        GPRTemporary edx(this, X86Registers::edx);
        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
    
        GPRReg op2TempGPR;
        GPRReg temp;
        GPRReg op1SaveGPR;
    
        if (op2GPR == X86Registers::eax || op2GPR == X86Registers::edx) {
            op2TempGPR = allocate();
            temp = op2TempGPR;
        } else {
            op2TempGPR = InvalidGPRReg;
            if (op1GPR == X86Registers::eax)
                temp = X86Registers::edx;
            else
                temp = X86Registers::eax;
        }
    
        if (op1GPR == X86Registers::eax || op1GPR == X86Registers::edx) {
            op1SaveGPR = allocate();
            ASSERT(op1GPR != op1SaveGPR);
            m_jit.move(op1GPR, op1SaveGPR);
        } else
            op1SaveGPR = op1GPR;
    
        ASSERT(temp != op1GPR);
        ASSERT(temp != op2GPR);
        ASSERT(op1SaveGPR != X86Registers::eax);
        ASSERT(op1SaveGPR != X86Registers::edx);
    
        m_jit.add32(JITCompiler::TrustedImm32(1), op2GPR, temp);
    
        JITCompiler::Jump safeDenominator = m_jit.branch32(JITCompiler::Above, temp, JITCompiler::TrustedImm32(1));
    
        JITCompiler::JumpList done;
        
        // FIXME: -2^31 / -1 will actually yield negative zero, so we could have a
        // separate case for that. But it probably doesn't matter so much.
        if (shouldCheckOverflow(node->arithMode())) {
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(JITCompiler::Zero, op2GPR));
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branch32(JITCompiler::Equal, op1GPR, TrustedImm32(-2147483647-1)));
        } else {
            // This is the case where we convert the result to an int after we're done, and we
            // already know that the denominator is either -1 or 0. So, if the denominator is
            // zero, then the result should be zero. If the denominator is not zero (i.e. it's
            // -1) and the numerator is -2^31 then the result should be 0. Otherwise we are
            // happy to fall through to a normal division, since we're just dividing something
            // by negative 1.
        
            JITCompiler::Jump notZero = m_jit.branchTest32(JITCompiler::NonZero, op2GPR);
            m_jit.move(TrustedImm32(0), edx.gpr());
            done.append(m_jit.jump());
        
            notZero.link(&m_jit);
            JITCompiler::Jump notNeg2ToThe31 =
                m_jit.branch32(JITCompiler::NotEqual, op1GPR, TrustedImm32(-2147483647-1));
            m_jit.move(TrustedImm32(0), edx.gpr());
            done.append(m_jit.jump());
        
            notNeg2ToThe31.link(&m_jit);
        }
        
        safeDenominator.link(&m_jit);
            
        if (op2TempGPR != InvalidGPRReg) {
            m_jit.move(op2GPR, op2TempGPR);
            op2GPR = op2TempGPR;
        }
            
        m_jit.move(op1GPR, eax.gpr());
        m_jit.x86ConvertToDoubleWord32();
        m_jit.x86Div32(op2GPR);
            
        if (op2TempGPR != InvalidGPRReg)
            unlock(op2TempGPR);

        // Check that we're not about to create negative zero.
        if (shouldCheckNegativeZero(node->arithMode())) {
            JITCompiler::Jump numeratorPositive = m_jit.branch32(JITCompiler::GreaterThanOrEqual, op1SaveGPR, TrustedImm32(0));
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(JITCompiler::Zero, edx.gpr()));
            numeratorPositive.link(&m_jit);
        }
    
        if (op1SaveGPR != op1GPR)
            unlock(op1SaveGPR);
            
        done.link(&m_jit);
        int32Result(edx.gpr(), node);

#elif HAVE(ARM_IDIV_INSTRUCTIONS) || CPU(ARM64)
        GPRTemporary temp(this);
        GPRTemporary quotientThenRemainder(this);
        GPRTemporary multiplyAnswer(this);
        GPRReg dividendGPR = op1.gpr();
        GPRReg divisorGPR = op2.gpr();
        GPRReg quotientThenRemainderGPR = quotientThenRemainder.gpr();
        GPRReg multiplyAnswerGPR = multiplyAnswer.gpr();

        JITCompiler::JumpList done;
    
        if (shouldCheckOverflow(node->arithMode()))
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(JITCompiler::Zero, divisorGPR));
        else {
            JITCompiler::Jump denominatorNotZero = m_jit.branchTest32(JITCompiler::NonZero, divisorGPR);
            // We know that the low 32-bit of divisorGPR is 0, but we don't know if the high bits are.
            // So, use TrustedImm32(0) on ARM instead because done expects the result to be in DataFormatInt32.
            // Using an immediate 0 doesn't cost anything extra on ARM.
            m_jit.move(TrustedImm32(0), quotientThenRemainderGPR);
            done.append(m_jit.jump());
            denominatorNotZero.link(&m_jit);
        }

        m_jit.assembler().sdiv<32>(quotientThenRemainderGPR, dividendGPR, divisorGPR);
        // FIXME: It seems like there are cases where we don't need this? What if we have
        // arithMode() == Arith::Unchecked?
        // https://bugs.webkit.org/show_bug.cgi?id=126444
        speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchMul32(JITCompiler::Overflow, quotientThenRemainderGPR, divisorGPR, multiplyAnswerGPR));
#if HAVE(ARM_IDIV_INSTRUCTIONS)
        m_jit.assembler().sub(quotientThenRemainderGPR, dividendGPR, multiplyAnswerGPR);
#else
        m_jit.assembler().sub<32>(quotientThenRemainderGPR, dividendGPR, multiplyAnswerGPR);
#endif

        // If the user cares about negative zero, then speculate that we're not about
        // to produce negative zero.
        if (shouldCheckNegativeZero(node->arithMode())) {
            // Check that we're not about to create negative zero.
            JITCompiler::Jump numeratorPositive = m_jit.branch32(JITCompiler::GreaterThanOrEqual, dividendGPR, TrustedImm32(0));
            speculationCheck(Overflow, JSValueRegs(), 0, m_jit.branchTest32(JITCompiler::Zero, quotientThenRemainderGPR));
            numeratorPositive.link(&m_jit);
        }

        done.link(&m_jit);

        int32Result(quotientThenRemainderGPR, node);
#else // not architecture that can do integer division
        RELEASE_ASSERT_NOT_REACHED();
#endif
        return;
    }
        
    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        
        FPRReg op1FPR = op1.fpr();
        FPRReg op2FPR = op2.fpr();
        
        flushRegisters();
        
        FPRResult result(this);

        using OperationType = D_JITOperation_DD;
        callOperation<OperationType>(jsMod, result.fpr(), op1FPR, op2FPR);
        
        doubleResult(result.fpr(), node);
        return;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileArithRounding(Node* node)
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand value(this, node->child1());
        FPRReg valueFPR = value.fpr();

        auto setResult = [&] (FPRReg resultFPR) {
            if (producesInteger(node->arithRoundingMode())) {
                GPRTemporary roundedResultAsInt32(this);
                FPRTemporary scratch(this);
                FPRReg scratchFPR = scratch.fpr();
                GPRReg resultGPR = roundedResultAsInt32.gpr();
                JITCompiler::JumpList failureCases;
                m_jit.branchConvertDoubleToInt32(resultFPR, resultGPR, failureCases, scratchFPR, shouldCheckNegativeZero(node->arithRoundingMode()));
                speculationCheck(Overflow, JSValueRegs(), node, failureCases);

                int32Result(resultGPR, node);
            } else
                doubleResult(resultFPR, node);
        };

        if (m_jit.supportsFloatingPointRounding()) {
            switch (node->op()) {
            case ArithRound: {
                FPRTemporary result(this);
                FPRReg resultFPR = result.fpr();
                if (producesInteger(node->arithRoundingMode()) && !shouldCheckNegativeZero(node->arithRoundingMode())) {
                    static const double halfConstant = 0.5;
                    m_jit.loadDouble(TrustedImmPtr(&halfConstant), resultFPR);
                    m_jit.addDouble(valueFPR, resultFPR);
                    m_jit.floorDouble(resultFPR, resultFPR);
                } else {
                    m_jit.ceilDouble(valueFPR, resultFPR);
                    FPRTemporary realPart(this);
                    FPRReg realPartFPR = realPart.fpr();
                    m_jit.subDouble(resultFPR, valueFPR, realPartFPR);

                    FPRTemporary scratch(this);
                    FPRReg scratchFPR = scratch.fpr();
                    static const double halfConstant = 0.5;
                    m_jit.loadDouble(TrustedImmPtr(&halfConstant), scratchFPR);

                    JITCompiler::Jump shouldUseCeiled = m_jit.branchDouble(JITCompiler::DoubleLessThanOrEqual, realPartFPR, scratchFPR);
                    static const double oneConstant = -1.0;
                    m_jit.loadDouble(TrustedImmPtr(&oneConstant), scratchFPR);
                    m_jit.addDouble(scratchFPR, resultFPR);
                    shouldUseCeiled.link(&m_jit);
                }
                setResult(resultFPR);
                return;
            }

            case ArithFloor: {
                FPRTemporary rounded(this);
                FPRReg resultFPR = rounded.fpr();
                m_jit.floorDouble(valueFPR, resultFPR);
                setResult(resultFPR);
                return;
            }

            case ArithCeil: {
                FPRTemporary rounded(this);
                FPRReg resultFPR = rounded.fpr();
                m_jit.ceilDouble(valueFPR, resultFPR);
                setResult(resultFPR);
                return;
            }

            case ArithTrunc: {
                FPRTemporary rounded(this);
                FPRReg resultFPR = rounded.fpr();
                m_jit.roundTowardZeroDouble(valueFPR, resultFPR);
                setResult(resultFPR);
                return;
            }

            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            flushRegisters();
            FPRResult roundedResultAsDouble(this);
            FPRReg resultFPR = roundedResultAsDouble.fpr();
            using OperationType = D_JITOperation_D;
            if (node->op() == ArithRound)
                callOperation<OperationType>(jsRound, resultFPR, valueFPR);
            else if (node->op() == ArithFloor)
                callOperation<OperationType>(floor, resultFPR, valueFPR);
            else if (node->op() == ArithCeil)
                callOperation<OperationType>(ceil, resultFPR, valueFPR);
            else {
                ASSERT(node->op() == ArithTrunc);
                callOperation<OperationType>(trunc, resultFPR, valueFPR);
            }
            setResult(resultFPR);
        }
        return;
    }

    DFG_ASSERT(m_jit.graph(), node, node->child1().useKind() == UntypedUse, node->child1().useKind());

    JSValueOperand argument(this, node->child1());
    JSValueRegs argumentRegs = argument.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    J_JITOperation_EJ operation = nullptr;
    if (node->op() == ArithRound)
        operation = operationArithRound;
    else if (node->op() == ArithFloor)
        operation = operationArithFloor;
    else if (node->op() == ArithCeil)
        operation = operationArithCeil;
    else {
        ASSERT(node->op() == ArithTrunc);
        operation = operationArithTrunc;
    }
    callOperation(operation, resultRegs, argumentRegs);
    m_jit.exceptionCheck();
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileArithUnary(Node* node)
{
    compileArithDoubleUnaryOp(node, arithUnaryFunction(node->arithUnaryType()), arithUnaryOperation(node->arithUnaryType()));
}

void SpeculativeJIT::compileArithSqrt(Node* node)
{
    if (node->child1().useKind() == DoubleRepUse) {
        SpeculateDoubleOperand op1(this, node->child1());
        FPRReg op1FPR = op1.fpr();

        if (!MacroAssembler::supportsFloatingPointSqrt() || !Options::useArchitectureSpecificOptimizations()) {
            flushRegisters();
            FPRResult result(this);
            callOperation<D_JITOperation_D>(sqrt, result.fpr(), op1FPR);
            doubleResult(result.fpr(), node);
        } else {
            FPRTemporary result(this, op1);
            m_jit.sqrtDouble(op1.fpr(), result.fpr());
            doubleResult(result.fpr(), node);
        }
        return;
    }

    JSValueOperand op1(this, node->child1());
    JSValueRegs op1Regs = op1.jsValueRegs();
    flushRegisters();
    FPRResult result(this);
    callOperation(operationArithSqrt, result.fpr(), op1Regs);
    m_jit.exceptionCheck();
    doubleResult(result.fpr(), node);
}

void SpeculativeJIT::compileArithMinMax(Node* node)
{
    switch (node->binaryUseKind()) {
    case Int32Use: {
        SpeculateStrictInt32Operand op1(this, node->child1());
        SpeculateStrictInt32Operand op2(this, node->child2());
        GPRTemporary result(this, Reuse, op1);

        GPRReg op1GPR = op1.gpr();
        GPRReg op2GPR = op2.gpr();
        GPRReg resultGPR = result.gpr();

        MacroAssembler::Jump op1Less = m_jit.branch32(node->op() == ArithMin ? MacroAssembler::LessThan : MacroAssembler::GreaterThan, op1GPR, op2GPR);
        m_jit.move(op2GPR, resultGPR);
        if (op1GPR != resultGPR) {
            MacroAssembler::Jump done = m_jit.jump();
            op1Less.link(&m_jit);
            m_jit.move(op1GPR, resultGPR);
            done.link(&m_jit);
        } else
            op1Less.link(&m_jit);

        int32Result(resultGPR, node);
        break;
    }

    case DoubleRepUse: {
        SpeculateDoubleOperand op1(this, node->child1());
        SpeculateDoubleOperand op2(this, node->child2());
        FPRTemporary result(this, op1);

        FPRReg op1FPR = op1.fpr();
        FPRReg op2FPR = op2.fpr();
        FPRReg resultFPR = result.fpr();

        MacroAssembler::JumpList done;

        MacroAssembler::Jump op1Less = m_jit.branchDouble(node->op() == ArithMin ? MacroAssembler::DoubleLessThan : MacroAssembler::DoubleGreaterThan, op1FPR, op2FPR);

        // op2 is eather the lesser one or one of then is NaN
        MacroAssembler::Jump op2Less = m_jit.branchDouble(node->op() == ArithMin ? MacroAssembler::DoubleGreaterThanOrEqual : MacroAssembler::DoubleLessThanOrEqual, op1FPR, op2FPR);

        // Unordered case. We don't know which of op1, op2 is NaN. Manufacture NaN by adding 
        // op1 + op2 and putting it into result.
        m_jit.addDouble(op1FPR, op2FPR, resultFPR);
        done.append(m_jit.jump());

        op2Less.link(&m_jit);
        m_jit.moveDouble(op2FPR, resultFPR);

        if (op1FPR != resultFPR) {
            done.append(m_jit.jump());

            op1Less.link(&m_jit);
            m_jit.moveDouble(op1FPR, resultFPR);
        } else
            op1Less.link(&m_jit);

        done.link(&m_jit);

        doubleResult(resultFPR, node);
        break;
    }

    default:
        DFG_CRASH(m_jit.graph(), node, "Bad use kind");
        break;
    }
}

// For small positive integers , it is worth doing a tiny inline loop to exponentiate the base.
// Every register is clobbered by this helper.
static MacroAssembler::Jump compileArithPowIntegerFastPath(JITCompiler& assembler, FPRReg xOperand, GPRReg yOperand, FPRReg result)
{
    MacroAssembler::JumpList skipFastPath;
    skipFastPath.append(assembler.branch32(MacroAssembler::Above, yOperand, MacroAssembler::TrustedImm32(maxExponentForIntegerMathPow)));

    static const double oneConstant = 1.0;
    assembler.loadDouble(MacroAssembler::TrustedImmPtr(&oneConstant), result);

    MacroAssembler::Label startLoop(assembler.label());
    MacroAssembler::Jump exponentIsEven = assembler.branchTest32(MacroAssembler::Zero, yOperand, MacroAssembler::TrustedImm32(1));
    assembler.mulDouble(xOperand, result);
    exponentIsEven.link(&assembler);
    assembler.mulDouble(xOperand, xOperand);
    assembler.rshift32(MacroAssembler::TrustedImm32(1), yOperand);
    assembler.branchTest32(MacroAssembler::NonZero, yOperand).linkTo(startLoop, &assembler);

    MacroAssembler::Jump skipSlowPath = assembler.jump();
    skipFastPath.link(&assembler);

    return skipSlowPath;
}

void SpeculativeJIT::compileArithPow(Node* node)
{
    if (node->child2().useKind() == Int32Use) {
        SpeculateDoubleOperand xOperand(this, node->child1());
        SpeculateInt32Operand yOperand(this, node->child2());
        FPRReg xOperandfpr = xOperand.fpr();
        GPRReg yOperandGpr = yOperand.gpr();
        FPRTemporary yOperandfpr(this);

        flushRegisters();

        FPRResult result(this);
        FPRReg resultFpr = result.fpr();

        FPRTemporary xOperandCopy(this);
        FPRReg xOperandCopyFpr = xOperandCopy.fpr();
        m_jit.moveDouble(xOperandfpr, xOperandCopyFpr);

        GPRTemporary counter(this);
        GPRReg counterGpr = counter.gpr();
        m_jit.move(yOperandGpr, counterGpr);

        MacroAssembler::Jump skipFallback = compileArithPowIntegerFastPath(m_jit, xOperandCopyFpr, counterGpr, resultFpr);
        m_jit.convertInt32ToDouble(yOperandGpr, yOperandfpr.fpr());
        callOperation(operationMathPow, resultFpr, xOperandfpr, yOperandfpr.fpr());

        skipFallback.link(&m_jit);
        doubleResult(resultFpr, node);
        return;
    }

    if (node->child2()->isDoubleConstant()) {
        double exponent = node->child2()->asNumber();
        static const double infinityConstant = std::numeric_limits<double>::infinity();
        static const double minusInfinityConstant = -std::numeric_limits<double>::infinity();
        if (exponent == 0.5) {
            SpeculateDoubleOperand xOperand(this, node->child1());
            FPRTemporary result(this);
            FPRReg xOperandFpr = xOperand.fpr();
            FPRReg resultFpr = result.fpr();

            m_jit.moveZeroToDouble(resultFpr);
            MacroAssembler::Jump xIsZeroOrNegativeZero = m_jit.branchDouble(MacroAssembler::DoubleEqual, xOperandFpr, resultFpr);

            m_jit.loadDouble(TrustedImmPtr(&minusInfinityConstant), resultFpr);
            MacroAssembler::Jump xIsMinusInfinity = m_jit.branchDouble(MacroAssembler::DoubleEqual, xOperandFpr, resultFpr);
            m_jit.sqrtDouble(xOperandFpr, resultFpr);
            MacroAssembler::Jump doneWithSqrt = m_jit.jump();

            xIsMinusInfinity.link(&m_jit);
            if (isX86())
                m_jit.loadDouble(TrustedImmPtr(&infinityConstant), resultFpr);
            else
                m_jit.absDouble(resultFpr, resultFpr);

            xIsZeroOrNegativeZero.link(&m_jit);
            doneWithSqrt.link(&m_jit);
            doubleResult(resultFpr, node);
            return;
        }
        if (exponent == -0.5) {
            SpeculateDoubleOperand xOperand(this, node->child1());
            FPRTemporary scratch(this);
            FPRTemporary result(this);
            FPRReg xOperandFpr = xOperand.fpr();
            FPRReg scratchFPR = scratch.fpr();
            FPRReg resultFpr = result.fpr();

            m_jit.moveZeroToDouble(resultFpr);
            MacroAssembler::Jump xIsZeroOrNegativeZero = m_jit.branchDouble(MacroAssembler::DoubleEqual, xOperandFpr, resultFpr);

            m_jit.loadDouble(TrustedImmPtr(&minusInfinityConstant), resultFpr);
            MacroAssembler::Jump xIsMinusInfinity = m_jit.branchDouble(MacroAssembler::DoubleEqual, xOperandFpr, resultFpr);

            static const double oneConstant = 1.;
            m_jit.loadDouble(TrustedImmPtr(&oneConstant), resultFpr);
            m_jit.sqrtDouble(xOperandFpr, scratchFPR);
            m_jit.divDouble(resultFpr, scratchFPR, resultFpr);
            MacroAssembler::Jump doneWithSqrt = m_jit.jump();

            xIsZeroOrNegativeZero.link(&m_jit);
            m_jit.loadDouble(TrustedImmPtr(&infinityConstant), resultFpr);
            MacroAssembler::Jump doneWithBaseZero = m_jit.jump();

            xIsMinusInfinity.link(&m_jit);
            m_jit.moveZeroToDouble(resultFpr);

            doneWithBaseZero.link(&m_jit);
            doneWithSqrt.link(&m_jit);
            doubleResult(resultFpr, node);
            return;
        }
    }

    SpeculateDoubleOperand xOperand(this, node->child1());
    SpeculateDoubleOperand yOperand(this, node->child2());
    FPRReg xOperandfpr = xOperand.fpr();
    FPRReg yOperandfpr = yOperand.fpr();

    flushRegisters();

    FPRResult result(this);
    FPRReg resultFpr = result.fpr();

    FPRTemporary xOperandCopy(this);
    FPRReg xOperandCopyFpr = xOperandCopy.fpr();

    FPRTemporary scratch(this);
    FPRReg scratchFpr = scratch.fpr();

    GPRTemporary yOperandInteger(this);
    GPRReg yOperandIntegerGpr = yOperandInteger.gpr();
    MacroAssembler::JumpList failedExponentConversionToInteger;
    m_jit.branchConvertDoubleToInt32(yOperandfpr, yOperandIntegerGpr, failedExponentConversionToInteger, scratchFpr, false);

    m_jit.moveDouble(xOperandfpr, xOperandCopyFpr);
    MacroAssembler::Jump skipFallback = compileArithPowIntegerFastPath(m_jit, xOperandCopyFpr, yOperandInteger.gpr(), resultFpr);
    failedExponentConversionToInteger.link(&m_jit);

    callOperation(operationMathPow, resultFpr, xOperandfpr, yOperandfpr);
    skipFallback.link(&m_jit);
    doubleResult(resultFpr, node);
}

// Returns true if the compare is fused with a subsequent branch.
bool SpeculativeJIT::compare(Node* node, MacroAssembler::RelationalCondition condition, MacroAssembler::DoubleCondition doubleCondition, S_JITOperation_EJJ operation)
{
    if (compilePeepHoleBranch(node, condition, doubleCondition, operation))
        return true;

    if (node->isBinaryUseKind(Int32Use)) {
        compileInt32Compare(node, condition);
        return false;
    }
    
#if USE(JSVALUE64)
    if (node->isBinaryUseKind(Int52RepUse)) {
        compileInt52Compare(node, condition);
        return false;
    }
#endif // USE(JSVALUE64)
    
    if (node->isBinaryUseKind(DoubleRepUse)) {
        compileDoubleCompare(node, doubleCondition);
        return false;
    }

    if (node->isBinaryUseKind(StringUse)) {
        if (node->op() == CompareEq)
            compileStringEquality(node);
        else
            compileStringCompare(node, condition);
        return false;
    }

    if (node->isBinaryUseKind(StringIdentUse)) {
        if (node->op() == CompareEq)
            compileStringIdentEquality(node);
        else
            compileStringIdentCompare(node, condition);
        return false;
    }

    if (node->op() == CompareEq) {
        if (node->isBinaryUseKind(BooleanUse)) {
            compileBooleanCompare(node, condition);
            return false;
        }

        if (node->isBinaryUseKind(SymbolUse)) {
            compileSymbolEquality(node);
            return false;
        }
        
        if (node->isBinaryUseKind(ObjectUse)) {
            compileObjectEquality(node);
            return false;
        }
        
        if (node->isBinaryUseKind(ObjectUse, ObjectOrOtherUse)) {
            compileObjectToObjectOrOtherEquality(node->child1(), node->child2());
            return false;
        }
        
        if (node->isBinaryUseKind(ObjectOrOtherUse, ObjectUse)) {
            compileObjectToObjectOrOtherEquality(node->child2(), node->child1());
            return false;
        }

        if (!needsTypeCheck(node->child1(), SpecOther)) {
            nonSpeculativeNonPeepholeCompareNullOrUndefined(node->child2());
            return false;
        }

        if (!needsTypeCheck(node->child2(), SpecOther)) {
            nonSpeculativeNonPeepholeCompareNullOrUndefined(node->child1());
            return false;
        }
    }

    nonSpeculativeNonPeepholeCompare(node, condition, operation);
    return false;
}

void SpeculativeJIT::compileCompareUnsigned(Node* node, MacroAssembler::RelationalCondition condition)
{
    compileInt32Compare(node, condition);
}

bool SpeculativeJIT::compileStrictEq(Node* node)
{
    if (node->isBinaryUseKind(BooleanUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleBooleanBranch(node, branchNode, MacroAssembler::Equal);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileBooleanCompare(node, MacroAssembler::Equal);
        return false;
    }

    if (node->isBinaryUseKind(Int32Use)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleInt32Branch(node, branchNode, MacroAssembler::Equal);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileInt32Compare(node, MacroAssembler::Equal);
        return false;
    }
    
#if USE(JSVALUE64)   
    if (node->isBinaryUseKind(Int52RepUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleInt52Branch(node, branchNode, MacroAssembler::Equal);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileInt52Compare(node, MacroAssembler::Equal);
        return false;
    }
#endif // USE(JSVALUE64)

    if (node->isBinaryUseKind(DoubleRepUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleDoubleBranch(node, branchNode, MacroAssembler::DoubleEqual);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileDoubleCompare(node, MacroAssembler::DoubleEqual);
        return false;
    }

    if (node->isBinaryUseKind(SymbolUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleSymbolEquality(node, branchNode);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileSymbolEquality(node);
        return false;
    }
    
    if (node->isBinaryUseKind(BigIntUse)) {
        compileBigIntEquality(node);
        return false;
    }
    
    if (node->isBinaryUseKind(SymbolUse, UntypedUse)) {
        compileSymbolUntypedEquality(node, node->child1(), node->child2());
        return false;
    }
    
    if (node->isBinaryUseKind(UntypedUse, SymbolUse)) {
        compileSymbolUntypedEquality(node, node->child2(), node->child1());
        return false;
    }
    
    if (node->isBinaryUseKind(StringUse)) {
        compileStringEquality(node);
        return false;
    }
    
    if (node->isBinaryUseKind(StringIdentUse)) {
        compileStringIdentEquality(node);
        return false;
    }

    if (node->isBinaryUseKind(ObjectUse, UntypedUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleObjectStrictEquality(node->child1(), node->child2(), branchNode);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileObjectStrictEquality(node->child1(), node->child2());
        return false;
    }
    
    if (node->isBinaryUseKind(UntypedUse, ObjectUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleObjectStrictEquality(node->child2(), node->child1(), branchNode);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileObjectStrictEquality(node->child2(), node->child1());
        return false;
    }

    if (node->isBinaryUseKind(ObjectUse)) {
        unsigned branchIndexInBlock = detectPeepHoleBranch();
        if (branchIndexInBlock != UINT_MAX) {
            Node* branchNode = m_block->at(branchIndexInBlock);
            compilePeepHoleObjectEquality(node, branchNode);
            use(node->child1());
            use(node->child2());
            m_indexInBlock = branchIndexInBlock;
            m_currentNode = branchNode;
            return true;
        }
        compileObjectEquality(node);
        return false;
    }

    if (node->isBinaryUseKind(MiscUse, UntypedUse)
        || node->isBinaryUseKind(UntypedUse, MiscUse)) {
        compileMiscStrictEq(node);
        return false;
    }
    
    if (node->isBinaryUseKind(StringIdentUse, NotStringVarUse)) {
        compileStringIdentToNotStringVarEquality(node, node->child1(), node->child2());
        return false;
    }
    
    if (node->isBinaryUseKind(NotStringVarUse, StringIdentUse)) {
        compileStringIdentToNotStringVarEquality(node, node->child2(), node->child1());
        return false;
    }
    
    if (node->isBinaryUseKind(StringUse, UntypedUse)) {
        compileStringToUntypedEquality(node, node->child1(), node->child2());
        return false;
    }
    
    if (node->isBinaryUseKind(UntypedUse, StringUse)) {
        compileStringToUntypedEquality(node, node->child2(), node->child1());
        return false;
    }
    
    RELEASE_ASSERT(node->isBinaryUseKind(UntypedUse));
    return nonSpeculativeStrictEq(node);
}

void SpeculativeJIT::compileBooleanCompare(Node* node, MacroAssembler::RelationalCondition condition)
{
    SpeculateBooleanOperand op1(this, node->child1());
    SpeculateBooleanOperand op2(this, node->child2());
    GPRTemporary result(this);
    
    m_jit.compare32(condition, op1.gpr(), op2.gpr(), result.gpr());
    
    unblessedBooleanResult(result.gpr(), node);
}

void SpeculativeJIT::compileInt32Compare(Node* node, MacroAssembler::RelationalCondition condition)
{
    if (node->child1()->isInt32Constant()) {
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this, Reuse, op2);
        int32_t imm = node->child1()->asInt32();
        m_jit.compare32(condition, JITCompiler::Imm32(imm), op2.gpr(), result.gpr());

        unblessedBooleanResult(result.gpr(), node);
    } else if (node->child2()->isInt32Constant()) {
        SpeculateInt32Operand op1(this, node->child1());
        GPRTemporary result(this, Reuse, op1);
        int32_t imm = node->child2()->asInt32();
        m_jit.compare32(condition, op1.gpr(), JITCompiler::Imm32(imm), result.gpr());

        unblessedBooleanResult(result.gpr(), node);
    } else {
        SpeculateInt32Operand op1(this, node->child1());
        SpeculateInt32Operand op2(this, node->child2());
        GPRTemporary result(this, Reuse, op1, op2);
        m_jit.compare32(condition, op1.gpr(), op2.gpr(), result.gpr());

        unblessedBooleanResult(result.gpr(), node);
    }
}

void SpeculativeJIT::compileDoubleCompare(Node* node, MacroAssembler::DoubleCondition condition)
{
    SpeculateDoubleOperand op1(this, node->child1());
    SpeculateDoubleOperand op2(this, node->child2());
    GPRTemporary result(this);

    FPRReg op1FPR = op1.fpr();
    FPRReg op2FPR = op2.fpr();
    GPRReg resultGPR = result.gpr();

    m_jit.compareDouble(condition, op1FPR, op2FPR, resultGPR);

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileObjectEquality(Node* node)
{
    SpeculateCellOperand op1(this, node->child1());
    SpeculateCellOperand op2(this, node->child2());
    GPRTemporary result(this, Reuse, op1);

    GPRReg op1GPR = op1.gpr();
    GPRReg op2GPR = op2.gpr();
    GPRReg resultGPR = result.gpr();

    if (masqueradesAsUndefinedWatchpointIsStillValid()) {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), node->child1(), SpecObject, m_jit.branchIfNotObject(op1GPR));
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op2GPR), node->child2(), SpecObject, m_jit.branchIfNotObject(op2GPR));
    } else {
        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op1GPR), node->child1(), SpecObject, m_jit.branchIfNotObject(op1GPR));
        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node->child1(),
            m_jit.branchTest8(
                MacroAssembler::NonZero,
                MacroAssembler::Address(op1GPR, JSCell::typeInfoFlagsOffset()),
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));

        DFG_TYPE_CHECK(
            JSValueSource::unboxedCell(op2GPR), node->child2(), SpecObject, m_jit.branchIfNotObject(op2GPR));
        speculationCheck(BadType, JSValueSource::unboxedCell(op2GPR), node->child2(),
            m_jit.branchTest8(
                MacroAssembler::NonZero,
                MacroAssembler::Address(op2GPR, JSCell::typeInfoFlagsOffset()),
                MacroAssembler::TrustedImm32(MasqueradesAsUndefined)));
    }

    m_jit.comparePtr(MacroAssembler::Equal, op1GPR, op2GPR, resultGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileSymbolEquality(Node* node)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRTemporary result(this, Reuse, left, right);

    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg resultGPR = result.gpr();

    speculateSymbol(node->child1(), leftGPR);
    speculateSymbol(node->child2(), rightGPR);

    m_jit.comparePtr(JITCompiler::Equal, leftGPR, rightGPR, resultGPR);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compilePeepHoleSymbolEquality(Node* node, Node* branchNode)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());

    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();

    speculateSymbol(node->child1(), leftGPR);
    speculateSymbol(node->child2(), rightGPR);

    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    if (taken == nextBlock()) {
        branchPtr(JITCompiler::NotEqual, leftGPR, rightGPR, notTaken);
        jump(taken);
    } else {
        branchPtr(JITCompiler::Equal, leftGPR, rightGPR, taken);
        jump(notTaken);
    }
}

void SpeculativeJIT::compileStringEquality(
    Node* node, GPRReg leftGPR, GPRReg rightGPR, GPRReg lengthGPR, GPRReg leftTempGPR,
    GPRReg rightTempGPR, GPRReg leftTemp2GPR, GPRReg rightTemp2GPR,
    const JITCompiler::JumpList& fastTrue, const JITCompiler::JumpList& fastFalse)
{
    JITCompiler::JumpList trueCase;
    JITCompiler::JumpList falseCase;
    JITCompiler::JumpList slowCase;
    
    trueCase.append(fastTrue);
    falseCase.append(fastFalse);

    m_jit.loadPtr(MacroAssembler::Address(leftGPR, JSString::offsetOfValue()), leftTempGPR);
    m_jit.loadPtr(MacroAssembler::Address(rightGPR, JSString::offsetOfValue()), rightTempGPR);

    slowCase.append(m_jit.branchIfRopeStringImpl(leftTempGPR));
    slowCase.append(m_jit.branchIfRopeStringImpl(rightTempGPR));

    m_jit.load32(MacroAssembler::Address(leftTempGPR, StringImpl::lengthMemoryOffset()), lengthGPR);
    
    falseCase.append(m_jit.branch32(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(rightTempGPR, StringImpl::lengthMemoryOffset()),
        lengthGPR));
    
    trueCase.append(m_jit.branchTest32(MacroAssembler::Zero, lengthGPR));
    
    slowCase.append(m_jit.branchTest32(
        MacroAssembler::Zero,
        MacroAssembler::Address(leftTempGPR, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit())));
    slowCase.append(m_jit.branchTest32(
        MacroAssembler::Zero,
        MacroAssembler::Address(rightTempGPR, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit())));
    
    m_jit.loadPtr(MacroAssembler::Address(leftTempGPR, StringImpl::dataOffset()), leftTempGPR);
    m_jit.loadPtr(MacroAssembler::Address(rightTempGPR, StringImpl::dataOffset()), rightTempGPR);
    
    MacroAssembler::Label loop = m_jit.label();
    
    m_jit.sub32(TrustedImm32(1), lengthGPR);

    // This isn't going to generate the best code on x86. But that's OK, it's still better
    // than not inlining.
    m_jit.load8(MacroAssembler::BaseIndex(leftTempGPR, lengthGPR, MacroAssembler::TimesOne), leftTemp2GPR);
    m_jit.load8(MacroAssembler::BaseIndex(rightTempGPR, lengthGPR, MacroAssembler::TimesOne), rightTemp2GPR);
    falseCase.append(m_jit.branch32(MacroAssembler::NotEqual, leftTemp2GPR, rightTemp2GPR));
    
    m_jit.branchTest32(MacroAssembler::NonZero, lengthGPR).linkTo(loop, &m_jit);
    
    trueCase.link(&m_jit);
    moveTrueTo(leftTempGPR);
    
    JITCompiler::Jump done = m_jit.jump();

    falseCase.link(&m_jit);
    moveFalseTo(leftTempGPR);
    
    done.link(&m_jit);
    addSlowPathGenerator(
        slowPathCall(
            slowCase, this, operationCompareStringEq, leftTempGPR, leftGPR, rightGPR));
    
    blessedBooleanResult(leftTempGPR, node);
}

void SpeculativeJIT::compileStringEquality(Node* node)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRTemporary length(this);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    GPRTemporary leftTemp2(this, Reuse, left);
    GPRTemporary rightTemp2(this, Reuse, right);
    
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg lengthGPR = length.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();
    GPRReg leftTemp2GPR = leftTemp2.gpr();
    GPRReg rightTemp2GPR = rightTemp2.gpr();
    
    speculateString(node->child1(), leftGPR);
    
    // It's safe to branch around the type check below, since proving that the values are
    // equal does indeed prove that the right value is a string.
    JITCompiler::Jump fastTrue = m_jit.branchPtr(MacroAssembler::Equal, leftGPR, rightGPR);
    
    speculateString(node->child2(), rightGPR);
    
    compileStringEquality(
        node, leftGPR, rightGPR, lengthGPR, leftTempGPR, rightTempGPR, leftTemp2GPR,
        rightTemp2GPR, fastTrue, JITCompiler::Jump());
}

void SpeculativeJIT::compileStringToUntypedEquality(Node* node, Edge stringEdge, Edge untypedEdge)
{
    SpeculateCellOperand left(this, stringEdge);
    JSValueOperand right(this, untypedEdge, ManualOperandSpeculation);
    GPRTemporary length(this);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    GPRTemporary leftTemp2(this, Reuse, left);
    GPRTemporary rightTemp2(this);
    
    GPRReg leftGPR = left.gpr();
    JSValueRegs rightRegs = right.jsValueRegs();
    GPRReg lengthGPR = length.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();
    GPRReg leftTemp2GPR = leftTemp2.gpr();
    GPRReg rightTemp2GPR = rightTemp2.gpr();
    
    speculateString(stringEdge, leftGPR);
    
    JITCompiler::JumpList fastTrue;
    JITCompiler::JumpList fastFalse;
    
    fastFalse.append(m_jit.branchIfNotCell(rightRegs));
    
    // It's safe to branch around the type check below, since proving that the values are
    // equal does indeed prove that the right value is a string.
    fastTrue.append(m_jit.branchPtr(
        MacroAssembler::Equal, leftGPR, rightRegs.payloadGPR()));
    
    fastFalse.append(m_jit.branchIfNotString(rightRegs.payloadGPR()));
    
    compileStringEquality(
        node, leftGPR, rightRegs.payloadGPR(), lengthGPR, leftTempGPR, rightTempGPR, leftTemp2GPR,
        rightTemp2GPR, fastTrue, fastFalse);
}

void SpeculativeJIT::compileStringIdentEquality(Node* node)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();

    speculateString(node->child1(), leftGPR);
    speculateString(node->child2(), rightGPR);
    
    speculateStringIdentAndLoadStorage(node->child1(), leftGPR, leftTempGPR);
    speculateStringIdentAndLoadStorage(node->child2(), rightGPR, rightTempGPR);
    
    m_jit.comparePtr(MacroAssembler::Equal, leftTempGPR, rightTempGPR, leftTempGPR);
    
    unblessedBooleanResult(leftTempGPR, node);
}

void SpeculativeJIT::compileStringIdentToNotStringVarEquality(
    Node* node, Edge stringEdge, Edge notStringVarEdge)
{
    SpeculateCellOperand left(this, stringEdge);
    JSValueOperand right(this, notStringVarEdge, ManualOperandSpeculation);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();
    GPRReg leftGPR = left.gpr();
    JSValueRegs rightRegs = right.jsValueRegs();
    
    speculateString(stringEdge, leftGPR);
    speculateStringIdentAndLoadStorage(stringEdge, leftGPR, leftTempGPR);

    moveFalseTo(rightTempGPR);
    JITCompiler::JumpList notString;
    notString.append(m_jit.branchIfNotCell(rightRegs));
    notString.append(m_jit.branchIfNotString(rightRegs.payloadGPR()));
    
    speculateStringIdentAndLoadStorage(notStringVarEdge, rightRegs.payloadGPR(), rightTempGPR);
    
    m_jit.comparePtr(MacroAssembler::Equal, leftTempGPR, rightTempGPR, rightTempGPR);
    notString.link(&m_jit);
    
    unblessedBooleanResult(rightTempGPR, node);
}

void SpeculativeJIT::compileStringCompare(Node* node, MacroAssembler::RelationalCondition condition)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();

    speculateString(node->child1(), leftGPR);
    speculateString(node->child2(), rightGPR);

    C_JITOperation_B_EJssJss compareFunction = nullptr;
    if (condition == MacroAssembler::LessThan)
        compareFunction = operationCompareStringLess;
    else if (condition == MacroAssembler::LessThanOrEqual)
        compareFunction = operationCompareStringLessEq;
    else if (condition == MacroAssembler::GreaterThan)
        compareFunction = operationCompareStringGreater;
    else if (condition == MacroAssembler::GreaterThanOrEqual)
        compareFunction = operationCompareStringGreaterEq;
    else
        RELEASE_ASSERT_NOT_REACHED();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    flushRegisters();
    callOperation(compareFunction, resultGPR, leftGPR, rightGPR);
    m_jit.exceptionCheck();

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileStringIdentCompare(Node* node, MacroAssembler::RelationalCondition condition)
{
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRFlushedCallResult result(this);
    GPRTemporary leftTemp(this);
    GPRTemporary rightTemp(this);

    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg leftTempGPR = leftTemp.gpr();
    GPRReg rightTempGPR = rightTemp.gpr();

    speculateString(node->child1(), leftGPR);
    speculateString(node->child2(), rightGPR);

    C_JITOperation_TT compareFunction = nullptr;
    if (condition == MacroAssembler::LessThan)
        compareFunction = operationCompareStringImplLess;
    else if (condition == MacroAssembler::LessThanOrEqual)
        compareFunction = operationCompareStringImplLessEq;
    else if (condition == MacroAssembler::GreaterThan)
        compareFunction = operationCompareStringImplGreater;
    else if (condition == MacroAssembler::GreaterThanOrEqual)
        compareFunction = operationCompareStringImplGreaterEq;
    else
        RELEASE_ASSERT_NOT_REACHED();

    speculateStringIdentAndLoadStorage(node->child1(), leftGPR, leftTempGPR);
    speculateStringIdentAndLoadStorage(node->child2(), rightGPR, rightTempGPR);

    flushRegisters();
    callOperation(compareFunction, resultGPR, leftTempGPR, rightTempGPR);

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileSameValue(Node* node)
{
    if (node->isBinaryUseKind(DoubleRepUse)) {
        SpeculateDoubleOperand arg1(this, node->child1());
        SpeculateDoubleOperand arg2(this, node->child2());
        GPRTemporary result(this);
        GPRTemporary temp(this);
        GPRTemporary temp2(this);

        FPRReg arg1FPR = arg1.fpr();
        FPRReg arg2FPR = arg2.fpr();
        GPRReg resultGPR = result.gpr();
        GPRReg tempGPR = temp.gpr();
        GPRReg temp2GPR = temp2.gpr();

#if USE(JSVALUE64)
        m_jit.moveDoubleTo64(arg1FPR, tempGPR);
        m_jit.moveDoubleTo64(arg2FPR, temp2GPR);
        auto trueCase = m_jit.branch64(CCallHelpers::Equal, tempGPR, temp2GPR);
#else
        GPRTemporary temp3(this);
        GPRReg temp3GPR = temp3.gpr();

        m_jit.moveDoubleToInts(arg1FPR, tempGPR, temp2GPR);
        m_jit.moveDoubleToInts(arg2FPR, temp3GPR, resultGPR);
        auto notEqual = m_jit.branch32(CCallHelpers::NotEqual, tempGPR, temp3GPR);
        auto trueCase = m_jit.branch32(CCallHelpers::Equal, temp2GPR, resultGPR);
        notEqual.link(&m_jit);
#endif

        m_jit.compareDouble(CCallHelpers::DoubleNotEqualOrUnordered, arg1FPR, arg1FPR, tempGPR);
        m_jit.compareDouble(CCallHelpers::DoubleNotEqualOrUnordered, arg2FPR, arg2FPR, temp2GPR);
        m_jit.and32(tempGPR, temp2GPR, resultGPR);
        auto done = m_jit.jump();

        trueCase.link(&m_jit);
        m_jit.move(CCallHelpers::TrustedImm32(1), resultGPR);
        done.link(&m_jit);

        unblessedBooleanResult(resultGPR, node);
        return;
    }

    ASSERT(node->isBinaryUseKind(UntypedUse));

    JSValueOperand arg1(this, node->child1());
    JSValueOperand arg2(this, node->child2());
    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();

    arg1.use();
    arg2.use();

    flushRegisters();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationSameValue, resultGPR, arg1Regs, arg2Regs);
    m_jit.exceptionCheck();

    unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileStringZeroLength(Node* node)
{
    SpeculateCellOperand str(this, node->child1());
    GPRReg strGPR = str.gpr();

    // Make sure that this is a string.
    speculateString(node->child1(), strGPR);

    GPRTemporary eq(this);
    GPRReg eqGPR = eq.gpr();

    m_jit.move(TrustedImmPtr::weakPointer(m_jit.graph(), jsEmptyString(m_jit.vm())), eqGPR);
    m_jit.comparePtr(CCallHelpers::Equal, strGPR, eqGPR, eqGPR);
    unblessedBooleanResult(eqGPR, node);
}

void SpeculativeJIT::compileLogicalNotStringOrOther(Node* node)
{
    JSValueOperand value(this, node->child1(), ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg tempGPR = temp.gpr();

    JITCompiler::Jump notCell = m_jit.branchIfNotCell(valueRegs);
    GPRReg cellGPR = valueRegs.payloadGPR();
    DFG_TYPE_CHECK(
        valueRegs, node->child1(), (~SpecCellCheck) | SpecString, m_jit.branchIfNotString(cellGPR));

    m_jit.move(TrustedImmPtr::weakPointer(m_jit.graph(), jsEmptyString(m_jit.vm())), tempGPR);
    m_jit.comparePtr(CCallHelpers::Equal, cellGPR, tempGPR, tempGPR);
    auto done = m_jit.jump();

    notCell.link(&m_jit);
    DFG_TYPE_CHECK(
        valueRegs, node->child1(), SpecCellCheck | SpecOther, m_jit.branchIfNotOther(valueRegs, tempGPR));
    m_jit.move(TrustedImm32(1), tempGPR);

    done.link(&m_jit);
    unblessedBooleanResult(tempGPR, node);

}

void SpeculativeJIT::emitStringBranch(Edge nodeUse, BasicBlock* taken, BasicBlock* notTaken)
{
    SpeculateCellOperand str(this, nodeUse);

    GPRReg strGPR = str.gpr();

    speculateString(nodeUse, strGPR);

    branchPtr(CCallHelpers::Equal, strGPR, TrustedImmPtr::weakPointer(m_jit.graph(), jsEmptyString(m_jit.vm())), notTaken);
    jump(taken);

    noResult(m_currentNode);
}

void SpeculativeJIT::emitStringOrOtherBranch(Edge nodeUse, BasicBlock* taken, BasicBlock* notTaken)
{
    JSValueOperand value(this, nodeUse, ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg tempGPR = temp.gpr();
    
    JITCompiler::Jump notCell = m_jit.branchIfNotCell(valueRegs);
    GPRReg cellGPR = valueRegs.payloadGPR();
    DFG_TYPE_CHECK(valueRegs, nodeUse, (~SpecCellCheck) | SpecString, m_jit.branchIfNotString(cellGPR));

    branchPtr(CCallHelpers::Equal, cellGPR, TrustedImmPtr::weakPointer(m_jit.graph(), jsEmptyString(m_jit.vm())), notTaken);
    jump(taken, ForceJump);

    notCell.link(&m_jit);
    DFG_TYPE_CHECK(
        valueRegs, nodeUse, SpecCellCheck | SpecOther, m_jit.branchIfNotOther(valueRegs, tempGPR));
    jump(notTaken);
    noResult(m_currentNode);
}

void SpeculativeJIT::compileConstantStoragePointer(Node* node)
{
    GPRTemporary storage(this);
    GPRReg storageGPR = storage.gpr();
    m_jit.move(TrustedImmPtr(node->storagePointer()), storageGPR);
    storageResult(storageGPR, node);
}

void SpeculativeJIT::cageTypedArrayStorage(GPRReg storageReg)
{
#if GIGACAGE_ENABLED
    if (!Gigacage::shouldBeEnabled())
        return;
    
    if (Gigacage::canPrimitiveGigacageBeDisabled()) {
        if (m_jit.vm()->primitiveGigacageEnabled().isStillValid())
            m_jit.graph().watchpoints().addLazily(m_jit.vm()->primitiveGigacageEnabled());
        else
            return;
    }
    
    m_jit.cage(Gigacage::Primitive, storageReg);
#else
    UNUSED_PARAM(storageReg);
#endif
}

void SpeculativeJIT::compileGetIndexedPropertyStorage(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRReg baseReg = base.gpr();
    
    GPRTemporary storage(this);
    GPRReg storageReg = storage.gpr();
    
    switch (node->arrayMode().type()) {
    case Array::String:
        m_jit.loadPtr(MacroAssembler::Address(baseReg, JSString::offsetOfValue()), storageReg);
        
        addSlowPathGenerator(
            slowPathCall(
                m_jit.branchIfRopeStringImpl(storageReg),
                this, operationResolveRope, storageReg, baseReg));

        m_jit.loadPtr(MacroAssembler::Address(storageReg, StringImpl::dataOffset()), storageReg);
        break;
        
    default:
        auto typedArrayType = node->arrayMode().typedArrayType();
        ASSERT_UNUSED(typedArrayType, isTypedView(typedArrayType));

        m_jit.loadPtr(JITCompiler::Address(baseReg, JSArrayBufferView::offsetOfVector()), storageReg);
        cageTypedArrayStorage(storageReg);
        break;
    }
    
    storageResult(storageReg, node);
}

void SpeculativeJIT::compileGetTypedArrayByteOffset(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary vector(this);
    GPRTemporary data(this);
    
    GPRReg baseGPR = base.gpr();
    GPRReg vectorGPR = vector.gpr();
    GPRReg dataGPR = data.gpr();
    ASSERT(baseGPR != vectorGPR);
    ASSERT(baseGPR != dataGPR);
    ASSERT(vectorGPR != dataGPR);

    GPRReg arrayBufferGPR = dataGPR;

    JITCompiler::Jump emptyByteOffset = m_jit.branch32(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfMode()),
        TrustedImm32(WastefulTypedArray));

    m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfVector()), vectorGPR);
    JITCompiler::Jump nullVector = m_jit.branchTestPtr(JITCompiler::Zero, vectorGPR);

    m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), dataGPR);
    m_jit.cage(Gigacage::JSValue, dataGPR);

    cageTypedArrayStorage(vectorGPR);

    m_jit.loadPtr(MacroAssembler::Address(dataGPR, Butterfly::offsetOfArrayBuffer()), arrayBufferGPR);
    // FIXME: This needs caging.
    // https://bugs.webkit.org/show_bug.cgi?id=175515
    m_jit.loadPtr(MacroAssembler::Address(arrayBufferGPR, ArrayBuffer::offsetOfData()), dataGPR);
    m_jit.subPtr(dataGPR, vectorGPR);
    
    JITCompiler::Jump done = m_jit.jump();
    
    emptyByteOffset.link(&m_jit);
    m_jit.move(TrustedImmPtr(nullptr), vectorGPR);
    
    done.link(&m_jit);
    nullVector.link(&m_jit);

    int32Result(vectorGPR, node);
}

void SpeculativeJIT::compileGetByValOnDirectArguments(Node* node)
{
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    JSValueRegsTemporary result(this);
    GPRTemporary scratch(this);
    
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    JSValueRegs resultRegs = result.regs();
    GPRReg scratchReg = scratch.gpr();
    
    if (!m_compileOkay)
        return;
    
    ASSERT(ArrayMode(Array::DirectArguments, Array::Read).alreadyChecked(m_jit.graph(), node, m_state.forNode(m_graph.varArgChild(node, 0))));
    
    speculationCheck(
        ExoticObjectMode, JSValueSource(), 0,
        m_jit.branchTestPtr(
            MacroAssembler::NonZero,
            MacroAssembler::Address(baseReg, DirectArguments::offsetOfMappedArguments())));

    m_jit.load32(CCallHelpers::Address(baseReg, DirectArguments::offsetOfLength()), scratchReg);
    auto isOutOfBounds = m_jit.branch32(CCallHelpers::AboveOrEqual, propertyReg, scratchReg);
    if (node->arrayMode().isInBounds())
        speculationCheck(OutOfBounds, JSValueSource(), 0, isOutOfBounds);
    
    m_jit.loadValue(
        MacroAssembler::BaseIndex(
            baseReg, propertyReg, MacroAssembler::TimesEight, DirectArguments::storageOffset()),
        resultRegs);
    
    if (!node->arrayMode().isInBounds()) {
        addSlowPathGenerator(
            slowPathCall(
                isOutOfBounds, this, operationGetByValObjectInt,
                extractResult(resultRegs), baseReg, propertyReg));
    }
    
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetByValOnScopedArguments(Node* node)
{
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand property(this, m_graph.varArgChild(node, 1));
    JSValueRegsTemporary result(this);
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);
    GPRTemporary indexMask(this);
    
    GPRReg baseReg = base.gpr();
    GPRReg propertyReg = property.gpr();
    JSValueRegs resultRegs = result.regs();
    GPRReg scratchReg = scratch.gpr();
    GPRReg scratch2Reg = scratch2.gpr();
    GPRReg indexMaskReg = indexMask.gpr();
    
    if (!m_compileOkay)
        return;
    
    ASSERT(ArrayMode(Array::ScopedArguments, Array::Read).alreadyChecked(m_jit.graph(), node, m_state.forNode(m_graph.varArgChild(node, 0))));
    
    m_jit.loadPtr(
        MacroAssembler::Address(baseReg, ScopedArguments::offsetOfStorage()), resultRegs.payloadGPR());
    m_jit.load32(
        MacroAssembler::Address(resultRegs.payloadGPR(), ScopedArguments::offsetOfTotalLengthInStorage()),
        scratchReg);
    
    speculationCheck(
        ExoticObjectMode, JSValueSource(), nullptr,
        m_jit.branch32(MacroAssembler::AboveOrEqual, propertyReg, scratchReg));
    
    m_jit.emitPreparePreciseIndexMask32(propertyReg, scratchReg, indexMaskReg);
    
    m_jit.loadPtr(MacroAssembler::Address(baseReg, ScopedArguments::offsetOfTable()), scratchReg);
    m_jit.load32(
        MacroAssembler::Address(scratchReg, ScopedArgumentsTable::offsetOfLength()), scratch2Reg);
    
    MacroAssembler::Jump overflowArgument = m_jit.branch32(
        MacroAssembler::AboveOrEqual, propertyReg, scratch2Reg);
    
    m_jit.loadPtr(MacroAssembler::Address(baseReg, ScopedArguments::offsetOfScope()), scratch2Reg);

    m_jit.loadPtr(
        MacroAssembler::Address(scratchReg, ScopedArgumentsTable::offsetOfArguments()),
        scratchReg);
    m_jit.load32(
        MacroAssembler::BaseIndex(scratchReg, propertyReg, MacroAssembler::TimesFour),
        scratchReg);
    
    speculationCheck(
        ExoticObjectMode, JSValueSource(), nullptr,
        m_jit.branch32(
            MacroAssembler::Equal, scratchReg, TrustedImm32(ScopeOffset::invalidOffset)));
    
    m_jit.loadValue(
        MacroAssembler::BaseIndex(
            scratch2Reg, propertyReg, MacroAssembler::TimesEight,
            JSLexicalEnvironment::offsetOfVariables()),
        resultRegs);
    
    MacroAssembler::Jump done = m_jit.jump();
    overflowArgument.link(&m_jit);
    
    m_jit.sub32(propertyReg, scratch2Reg);
    m_jit.neg32(scratch2Reg);
    
    m_jit.loadValue(
        MacroAssembler::BaseIndex(
            resultRegs.payloadGPR(), scratch2Reg, MacroAssembler::TimesEight),
        resultRegs);
    speculationCheck(ExoticObjectMode, JSValueSource(), nullptr, m_jit.branchIfEmpty(resultRegs));
    
    done.link(&m_jit);
    
    m_jit.andPtr(indexMaskReg, resultRegs.payloadGPR());
    
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetScope(Node* node)
{
    SpeculateCellOperand function(this, node->child1());
    GPRTemporary result(this, Reuse, function);
    m_jit.loadPtr(JITCompiler::Address(function.gpr(), JSFunction::offsetOfScopeChain()), result.gpr());
    cellResult(result.gpr(), node);
}
    
void SpeculativeJIT::compileSkipScope(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRTemporary result(this, Reuse, scope);
    m_jit.loadPtr(JITCompiler::Address(scope.gpr(), JSScope::offsetOfNext()), result.gpr());
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileGetGlobalObject(Node* node)
{
    SpeculateCellOperand object(this, node->child1());
    GPRTemporary result(this);
    GPRTemporary scratch(this);
    m_jit.emitLoadStructure(*m_jit.vm(), object.gpr(), result.gpr(), scratch.gpr());
    m_jit.loadPtr(JITCompiler::Address(result.gpr(), Structure::globalObjectOffset()), result.gpr());
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileGetGlobalThis(Node* node)
{
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    auto* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
    m_jit.loadPtr(globalObject->addressOfGlobalThis(), resultGPR);
    cellResult(resultGPR, node);
}

bool SpeculativeJIT::canBeRope(Edge& edge)
{
    if (m_state.forNode(edge).isType(SpecStringIdent))
        return false;
    // If this value is LazyValue, it will be converted to JSString, and the result must be non-rope string.
    String string = edge->tryGetString(m_graph);
    if (!string.isNull())
        return false;
    return true;
}

void SpeculativeJIT::compileGetArrayLength(Node* node)
{
    switch (node->arrayMode().type()) {
    case Array::Undecided:
    case Array::Int32:
    case Array::Double:
    case Array::Contiguous: {
        StorageOperand storage(this, node->child2());
        GPRTemporary result(this, Reuse, storage);
        GPRReg storageReg = storage.gpr();
        GPRReg resultReg = result.gpr();
        m_jit.load32(MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength()), resultReg);
            
        int32Result(resultReg, node);
        break;
    }
    case Array::ArrayStorage:
    case Array::SlowPutArrayStorage: {
        StorageOperand storage(this, node->child2());
        GPRTemporary result(this, Reuse, storage);
        GPRReg storageReg = storage.gpr();
        GPRReg resultReg = result.gpr();
        m_jit.load32(MacroAssembler::Address(storageReg, Butterfly::offsetOfPublicLength()), resultReg);
            
        speculationCheck(Uncountable, JSValueRegs(), 0, m_jit.branch32(MacroAssembler::LessThan, resultReg, MacroAssembler::TrustedImm32(0)));
            
        int32Result(resultReg, node);
        break;
    }
    case Array::String: {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary result(this, Reuse, base);
        GPRTemporary temp(this);
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        GPRReg tempGPR = temp.gpr();

        bool needsRopeCase = canBeRope(node->child1());

        m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSString::offsetOfValue()), tempGPR);
        CCallHelpers::Jump isRope;
        if (needsRopeCase)
            isRope = m_jit.branchIfRopeStringImpl(tempGPR);
        m_jit.load32(MacroAssembler::Address(tempGPR, StringImpl::lengthMemoryOffset()), resultGPR);
        if (needsRopeCase) {
            auto done = m_jit.jump();

            isRope.link(&m_jit);
            m_jit.load32(CCallHelpers::Address(baseGPR, JSRopeString::offsetOfLength()), resultGPR);

            done.link(&m_jit);
        }
        int32Result(resultGPR, node);
        break;
    }
    case Array::DirectArguments: {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary result(this, Reuse, base);
        
        GPRReg baseReg = base.gpr();
        GPRReg resultReg = result.gpr();
        
        if (!m_compileOkay)
            return;
        
        ASSERT(ArrayMode(Array::DirectArguments, Array::Read).alreadyChecked(m_jit.graph(), node, m_state.forNode(node->child1())));
        
        speculationCheck(
            ExoticObjectMode, JSValueSource(), 0,
            m_jit.branchTestPtr(
                MacroAssembler::NonZero,
                MacroAssembler::Address(baseReg, DirectArguments::offsetOfMappedArguments())));
        
        m_jit.load32(
            MacroAssembler::Address(baseReg, DirectArguments::offsetOfLength()), resultReg);
        
        int32Result(resultReg, node);
        break;
    }
    case Array::ScopedArguments: {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary result(this);
        
        GPRReg baseReg = base.gpr();
        GPRReg resultReg = result.gpr();
        
        if (!m_compileOkay)
            return;
        
        ASSERT(ArrayMode(Array::ScopedArguments, Array::Read).alreadyChecked(m_jit.graph(), node, m_state.forNode(node->child1())));
        
        m_jit.loadPtr(
            MacroAssembler::Address(baseReg, ScopedArguments::offsetOfStorage()), resultReg);

        speculationCheck(
            ExoticObjectMode, JSValueSource(), 0,
            m_jit.branchTest8(
                MacroAssembler::NonZero,
                MacroAssembler::Address(resultReg, ScopedArguments::offsetOfOverrodeThingsInStorage())));
        
        m_jit.load32(
            MacroAssembler::Address(resultReg, ScopedArguments::offsetOfTotalLengthInStorage()), resultReg);
        
        int32Result(resultReg, node);
        break;
    }
    default: {
        ASSERT(node->arrayMode().isSomeTypedArrayView());
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary result(this, Reuse, base);
        GPRReg baseGPR = base.gpr();
        GPRReg resultGPR = result.gpr();
        m_jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), resultGPR);
        int32Result(resultGPR, node);
        break;
    } }
}

void SpeculativeJIT::compileCheckStringIdent(Node* node)
{
    SpeculateCellOperand string(this, node->child1());
    GPRTemporary storage(this);

    GPRReg stringGPR = string.gpr();
    GPRReg storageGPR = storage.gpr();

    speculateString(node->child1(), stringGPR);
    speculateStringIdentAndLoadStorage(node->child1(), stringGPR, storageGPR);

    UniquedStringImpl* uid = node->uidOperand();
    speculationCheck(
        BadIdent, JSValueSource(), nullptr,
        m_jit.branchPtr(JITCompiler::NotEqual, storageGPR, TrustedImmPtr(uid)));
    noResult(node);
}

template <typename ClassType>
void SpeculativeJIT::compileNewFunctionCommon(GPRReg resultGPR, RegisteredStructure structure, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scopeGPR, MacroAssembler::JumpList& slowPath, size_t size, FunctionExecutable* executable)
{
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObjectWithKnownSize<ClassType>(resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR, slowPath, size);
    
    m_jit.storePtr(scopeGPR, JITCompiler::Address(resultGPR, JSFunction::offsetOfScopeChain()));
    m_jit.storePtr(TrustedImmPtr::weakPointer(m_jit.graph(), executable), JITCompiler::Address(resultGPR, JSFunction::offsetOfExecutable()));
    m_jit.storePtr(TrustedImmPtr(nullptr), JITCompiler::Address(resultGPR, JSFunction::offsetOfRareData()));
    
    m_jit.mutatorFence(*m_jit.vm());
}

void SpeculativeJIT::compileNewFunction(Node* node)
{
    NodeType nodeType = node->op();
    ASSERT(nodeType == NewFunction || nodeType == NewGeneratorFunction || nodeType == NewAsyncFunction || nodeType == NewAsyncGeneratorFunction);
    
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();

    FunctionExecutable* executable = node->castOperand<FunctionExecutable*>();

    if (executable->singletonFunction()->isStillValid()) {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        
        flushRegisters();

        if (nodeType == NewGeneratorFunction)
            callOperation(operationNewGeneratorFunction, resultGPR, scopeGPR, executable);
        else if (nodeType == NewAsyncFunction)
            callOperation(operationNewAsyncFunction, resultGPR, scopeGPR, executable);
        else if (nodeType == NewAsyncGeneratorFunction)
            callOperation(operationNewAsyncGeneratorFunction, resultGPR, scopeGPR, executable);
        else
            callOperation(operationNewFunction, resultGPR, scopeGPR, executable);
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
        return;
    }

    RegisteredStructure structure = m_jit.graph().registerStructure(
        [&] () {
            JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
            switch (nodeType) {
            case NewGeneratorFunction:
                return globalObject->generatorFunctionStructure();
            case NewAsyncFunction:
                return globalObject->asyncFunctionStructure();
            case NewAsyncGeneratorFunction:
                return globalObject->asyncGeneratorFunctionStructure();
            case NewFunction:
                return JSFunction::selectStructureForNewFuncExp(globalObject, node->castOperand<FunctionExecutable*>());
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }());
    
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    
    JITCompiler::JumpList slowPath;
    
    if (nodeType == NewFunction) {
        compileNewFunctionCommon<JSFunction>(resultGPR, structure, scratch1GPR, scratch2GPR, scopeGPR, slowPath, JSFunction::allocationSize(0), executable);
            
        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewFunctionWithInvalidatedReallocationWatchpoint, resultGPR, scopeGPR, executable));
    }

    if (nodeType == NewGeneratorFunction) {
        compileNewFunctionCommon<JSGeneratorFunction>(resultGPR, structure, scratch1GPR, scratch2GPR, scopeGPR, slowPath, JSGeneratorFunction::allocationSize(0), executable);

        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint, resultGPR, scopeGPR, executable));
    }

    if (nodeType == NewAsyncFunction) {
        compileNewFunctionCommon<JSAsyncFunction>(resultGPR, structure, scratch1GPR, scratch2GPR, scopeGPR, slowPath, JSAsyncFunction::allocationSize(0), executable);

        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewAsyncFunctionWithInvalidatedReallocationWatchpoint, resultGPR, scopeGPR, executable));
    }

    if (nodeType == NewAsyncGeneratorFunction) {
        compileNewFunctionCommon<JSAsyncGeneratorFunction>(resultGPR, structure, scratch1GPR, scratch2GPR, scopeGPR, slowPath, JSAsyncGeneratorFunction::allocationSize(0), executable);
        
        addSlowPathGenerator(slowPathCall(slowPath, this, operationNewAsyncGeneratorFunctionWithInvalidatedReallocationWatchpoint, resultGPR, scopeGPR, executable));
    }
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileSetFunctionName(Node* node)
{
    SpeculateCellOperand func(this, node->child1());
    GPRReg funcGPR = func.gpr();
    JSValueOperand nameValue(this, node->child2());
    JSValueRegs nameValueRegs = nameValue.jsValueRegs();

    flushRegisters();
    callOperation(operationSetFunctionName, funcGPR, nameValueRegs);
    m_jit.exceptionCheck();

    noResult(node);
}

void SpeculativeJIT::compileLoadVarargs(Node* node)
{
    LoadVarargsData* data = node->loadVarargsData();

    JSValueRegs argumentsRegs;
    {
        JSValueOperand arguments(this, node->child1());
        argumentsRegs = arguments.jsValueRegs();
        flushRegisters();
    }

    callOperation(operationSizeOfVarargs, GPRInfo::returnValueGPR, argumentsRegs, data->offset);
    m_jit.exceptionCheck();

    lock(GPRInfo::returnValueGPR);
    {
        JSValueOperand arguments(this, node->child1());
        argumentsRegs = arguments.jsValueRegs();
        flushRegisters();
    }
    unlock(GPRInfo::returnValueGPR);

    // FIXME: There is a chance that we will call an effectful length property twice. This is safe
    // from the standpoint of the VM's integrity, but it's subtly wrong from a spec compliance
    // standpoint. The best solution would be one where we can exit *into* the op_call_varargs right
    // past the sizing.
    // https://bugs.webkit.org/show_bug.cgi?id=141448

    GPRReg argCountIncludingThisGPR =
        JITCompiler::selectScratchGPR(GPRInfo::returnValueGPR, argumentsRegs);

    m_jit.add32(TrustedImm32(1), GPRInfo::returnValueGPR, argCountIncludingThisGPR);

    speculationCheck(
        VarargsOverflow, JSValueSource(), Edge(), m_jit.branch32(
            MacroAssembler::Above,
            GPRInfo::returnValueGPR,
            argCountIncludingThisGPR));

    speculationCheck(
        VarargsOverflow, JSValueSource(), Edge(), m_jit.branch32(
            MacroAssembler::Above,
            argCountIncludingThisGPR,
            TrustedImm32(data->limit)));

    m_jit.store32(argCountIncludingThisGPR, JITCompiler::payloadFor(data->machineCount));

    callOperation(operationLoadVarargs, data->machineStart.offset(), argumentsRegs, data->offset, GPRInfo::returnValueGPR, data->mandatoryMinimum);
    m_jit.exceptionCheck();

    noResult(node);
}

void SpeculativeJIT::compileForwardVarargs(Node* node)
{
    LoadVarargsData* data = node->loadVarargsData();
    InlineCallFrame* inlineCallFrame;
    if (node->child1())
        inlineCallFrame = node->child1()->origin.semantic.inlineCallFrame;
    else
        inlineCallFrame = node->origin.semantic.inlineCallFrame;

    GPRTemporary length(this);
    JSValueRegsTemporary temp(this);
    GPRReg lengthGPR = length.gpr();
    JSValueRegs tempRegs = temp.regs();
        
    emitGetLength(inlineCallFrame, lengthGPR, /* includeThis = */ true);
    if (data->offset)
        m_jit.sub32(TrustedImm32(data->offset), lengthGPR);
        
    speculationCheck(
        VarargsOverflow, JSValueSource(), Edge(), m_jit.branch32(
            MacroAssembler::Above,
            lengthGPR, TrustedImm32(data->limit)));
        
    m_jit.store32(lengthGPR, JITCompiler::payloadFor(data->machineCount));
        
    VirtualRegister sourceStart = JITCompiler::argumentsStart(inlineCallFrame) + data->offset;
    VirtualRegister targetStart = data->machineStart;

    m_jit.sub32(TrustedImm32(1), lengthGPR);
        
    // First have a loop that fills in the undefined slots in case of an arity check failure.
    m_jit.move(TrustedImm32(data->mandatoryMinimum), tempRegs.payloadGPR());
    JITCompiler::Jump done = m_jit.branch32(JITCompiler::BelowOrEqual, tempRegs.payloadGPR(), lengthGPR);
        
    JITCompiler::Label loop = m_jit.label();
    m_jit.sub32(TrustedImm32(1), tempRegs.payloadGPR());
    m_jit.storeTrustedValue(
        jsUndefined(),
        JITCompiler::BaseIndex(
            GPRInfo::callFrameRegister, tempRegs.payloadGPR(), JITCompiler::TimesEight,
            targetStart.offset() * sizeof(EncodedJSValue)));
    m_jit.branch32(JITCompiler::Above, tempRegs.payloadGPR(), lengthGPR).linkTo(loop, &m_jit);
    done.link(&m_jit);
        
    // And then fill in the actual argument values.
    done = m_jit.branchTest32(JITCompiler::Zero, lengthGPR);
        
    loop = m_jit.label();
    m_jit.sub32(TrustedImm32(1), lengthGPR);
    m_jit.loadValue(
        JITCompiler::BaseIndex(
            GPRInfo::callFrameRegister, lengthGPR, JITCompiler::TimesEight,
            sourceStart.offset() * sizeof(EncodedJSValue)),
        tempRegs);
    m_jit.storeValue(
        tempRegs,
        JITCompiler::BaseIndex(
            GPRInfo::callFrameRegister, lengthGPR, JITCompiler::TimesEight,
            targetStart.offset() * sizeof(EncodedJSValue)));
    m_jit.branchTest32(JITCompiler::NonZero, lengthGPR).linkTo(loop, &m_jit);
        
    done.link(&m_jit);
        
    noResult(node);
}

void SpeculativeJIT::compileCreateActivation(Node* node)
{
    SymbolTable* table = node->castOperand<SymbolTable*>();
    RegisteredStructure structure = m_jit.graph().registerStructure(m_jit.graph().globalObjectFor(
        node->origin.semantic)->activationStructure());
        
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    JSValue initializationValue = node->initializationValueForActivation();
    ASSERT(initializationValue == jsUndefined() || initializationValue == jsTDZValue());
    
    if (table->singletonScope()->isStillValid()) {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

#if USE(JSVALUE32_64)
        JSValueRegsTemporary initialization(this);
        JSValueRegs initializationRegs = initialization.regs();
        m_jit.moveTrustedValue(initializationValue, initializationRegs);
#endif

        flushRegisters();

#if USE(JSVALUE64)
        callOperation(operationCreateActivationDirect,
            resultGPR, structure, scopeGPR, table, TrustedImm64(JSValue::encode(initializationValue)));
#else
        callOperation(operationCreateActivationDirect,
            resultGPR, structure, scopeGPR, table, initializationRegs);
#endif
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
        return;
    }
    
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

#if USE(JSVALUE32_64)
    JSValueRegsTemporary initialization(this);
    JSValueRegs initializationRegs = initialization.regs();
    m_jit.moveTrustedValue(initializationValue, initializationRegs);
#endif

    JITCompiler::JumpList slowPath;
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObjectWithKnownSize<JSLexicalEnvironment>(
        resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR,
        slowPath, JSLexicalEnvironment::allocationSize(table));
        
    // Don't need a memory barriers since we just fast-created the activation, so the
    // activation must be young.
    m_jit.storePtr(scopeGPR, JITCompiler::Address(resultGPR, JSScope::offsetOfNext()));
    m_jit.storePtr(
        TrustedImmPtr(node->cellOperand()),
        JITCompiler::Address(resultGPR, JSLexicalEnvironment::offsetOfSymbolTable()));
        
    // Must initialize all members to undefined or the TDZ empty value.
    for (unsigned i = 0; i < table->scopeSize(); ++i) {
        m_jit.storeTrustedValue(
            initializationValue,
            JITCompiler::Address(
                resultGPR, JSLexicalEnvironment::offsetOfVariable(ScopeOffset(i))));
    }
    
    m_jit.mutatorFence(*m_jit.vm());

#if USE(JSVALUE64)
    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationCreateActivationDirect, resultGPR, structure, scopeGPR, table, TrustedImm64(JSValue::encode(initializationValue))));
#else
    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationCreateActivationDirect, resultGPR, structure, scopeGPR, table, initializationRegs));
#endif

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileCreateDirectArguments(Node* node)
{
    // FIXME: A more effective way of dealing with the argument count and callee is to have
    // them be explicit arguments to this node.
    // https://bugs.webkit.org/show_bug.cgi?id=142207
    
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary length;
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg lengthGPR = InvalidGPRReg;
    JSValueRegs valueRegs = JSValueRegs::withTwoAvailableRegs(scratch1GPR, scratch2GPR);
        
    unsigned minCapacity = m_jit.graph().baselineCodeBlockFor(node->origin.semantic)->numParameters() - 1;
        
    unsigned knownLength;
    bool lengthIsKnown; // if false, lengthGPR will have the length.
    if (node->origin.semantic.inlineCallFrame
        && !node->origin.semantic.inlineCallFrame->isVarargs()) {
        knownLength = node->origin.semantic.inlineCallFrame->argumentCountIncludingThis - 1;
        lengthIsKnown = true;
    } else {
        knownLength = UINT_MAX;
        lengthIsKnown = false;
            
        GPRTemporary realLength(this);
        length.adopt(realLength);
        lengthGPR = length.gpr();

        VirtualRegister argumentCountRegister = m_jit.argumentCount(node->origin.semantic);
        m_jit.load32(JITCompiler::payloadFor(argumentCountRegister), lengthGPR);
        m_jit.sub32(TrustedImm32(1), lengthGPR);
    }
        
    RegisteredStructure structure =
        m_jit.graph().registerStructure(m_jit.graph().globalObjectFor(node->origin.semantic)->directArgumentsStructure());
        
    // Use a different strategy for allocating the object depending on whether we know its
    // size statically.
    JITCompiler::JumpList slowPath;
    if (lengthIsKnown) {
        auto butterfly = TrustedImmPtr(nullptr);
        emitAllocateJSObjectWithKnownSize<DirectArguments>(
            resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR,
            slowPath, DirectArguments::allocationSize(std::max(knownLength, minCapacity)));
            
        m_jit.store32(
            TrustedImm32(knownLength),
            JITCompiler::Address(resultGPR, DirectArguments::offsetOfLength()));
    } else {
        JITCompiler::Jump tooFewArguments;
        if (minCapacity) {
            tooFewArguments =
                m_jit.branch32(JITCompiler::Below, lengthGPR, TrustedImm32(minCapacity));
        }
        m_jit.lshift32(lengthGPR, TrustedImm32(3), scratch1GPR);
        m_jit.add32(TrustedImm32(DirectArguments::storageOffset()), scratch1GPR);
        if (minCapacity) {
            JITCompiler::Jump done = m_jit.jump();
            tooFewArguments.link(&m_jit);
            m_jit.move(TrustedImm32(DirectArguments::allocationSize(minCapacity)), scratch1GPR);
            done.link(&m_jit);
        }
        
        emitAllocateVariableSizedJSObject<DirectArguments>(
            resultGPR, TrustedImmPtr(structure), scratch1GPR, scratch1GPR, scratch2GPR,
            slowPath);
            
        m_jit.store32(
            lengthGPR, JITCompiler::Address(resultGPR, DirectArguments::offsetOfLength()));
    }
        
    m_jit.store32(
        TrustedImm32(minCapacity),
        JITCompiler::Address(resultGPR, DirectArguments::offsetOfMinCapacity()));
        
    m_jit.storePtr(
        TrustedImmPtr(nullptr), JITCompiler::Address(resultGPR, DirectArguments::offsetOfMappedArguments()));

    m_jit.storePtr(
        TrustedImmPtr(nullptr), JITCompiler::Address(resultGPR, DirectArguments::offsetOfModifiedArgumentsDescriptor()));
    
    if (lengthIsKnown) {
        addSlowPathGenerator(
            slowPathCall(
                slowPath, this, operationCreateDirectArguments, resultGPR, structure,
                knownLength, minCapacity));
    } else {
        auto generator = std::make_unique<CallCreateDirectArgumentsSlowPathGenerator>(
            slowPath, this, resultGPR, structure, lengthGPR, minCapacity);
        addSlowPathGenerator(WTFMove(generator));
    }
        
    if (node->origin.semantic.inlineCallFrame) {
        if (node->origin.semantic.inlineCallFrame->isClosureCall) {
            m_jit.loadPtr(
                JITCompiler::addressFor(
                    node->origin.semantic.inlineCallFrame->calleeRecovery.virtualRegister()),
                scratch1GPR);
        } else {
            m_jit.move(
                TrustedImmPtr::weakPointer(
                    m_jit.graph(), node->origin.semantic.inlineCallFrame->calleeRecovery.constant().asCell()),
                scratch1GPR);
        }
    } else
        m_jit.loadPtr(JITCompiler::addressFor(CallFrameSlot::callee), scratch1GPR);

    // Don't need a memory barriers since we just fast-created the activation, so the
    // activation must be young.
    m_jit.storePtr(
        scratch1GPR, JITCompiler::Address(resultGPR, DirectArguments::offsetOfCallee()));
        
    VirtualRegister start = m_jit.argumentsStart(node->origin.semantic);
    if (lengthIsKnown) {
        for (unsigned i = 0; i < std::max(knownLength, minCapacity); ++i) {
            m_jit.loadValue(JITCompiler::addressFor(start + i), valueRegs);
            m_jit.storeValue(
                valueRegs, JITCompiler::Address(resultGPR, DirectArguments::offsetOfSlot(i)));
        }
    } else {
        JITCompiler::Jump done;
        if (minCapacity) {
            JITCompiler::Jump startLoop = m_jit.branch32(
                JITCompiler::AboveOrEqual, lengthGPR, TrustedImm32(minCapacity));
            m_jit.move(TrustedImm32(minCapacity), lengthGPR);
            startLoop.link(&m_jit);
        } else
            done = m_jit.branchTest32(MacroAssembler::Zero, lengthGPR);
        JITCompiler::Label loop = m_jit.label();
        m_jit.sub32(TrustedImm32(1), lengthGPR);
        m_jit.loadValue(
            JITCompiler::BaseIndex(
                GPRInfo::callFrameRegister, lengthGPR, JITCompiler::TimesEight,
                start.offset() * static_cast<int>(sizeof(Register))),
            valueRegs);
        m_jit.storeValue(
            valueRegs,
            JITCompiler::BaseIndex(
                resultGPR, lengthGPR, JITCompiler::TimesEight,
                DirectArguments::storageOffset()));
        m_jit.branchTest32(MacroAssembler::NonZero, lengthGPR).linkTo(loop, &m_jit);
        if (done.isSet())
            done.link(&m_jit);
    }
        
    m_jit.mutatorFence(*m_jit.vm());
        
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetFromArguments(Node* node)
{
    SpeculateCellOperand arguments(this, node->child1());
    JSValueRegsTemporary result(this);
    
    GPRReg argumentsGPR = arguments.gpr();
    JSValueRegs resultRegs = result.regs();
    
    m_jit.loadValue(JITCompiler::Address(argumentsGPR, DirectArguments::offsetOfSlot(node->capturedArgumentsOffset().offset())), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutToArguments(Node* node)
{
    SpeculateCellOperand arguments(this, node->child1());
    JSValueOperand value(this, node->child2());
    
    GPRReg argumentsGPR = arguments.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    
    m_jit.storeValue(valueRegs, JITCompiler::Address(argumentsGPR, DirectArguments::offsetOfSlot(node->capturedArgumentsOffset().offset())));
    noResult(node);
}

void SpeculativeJIT::compileGetArgument(Node* node)
{
    GPRTemporary argumentCount(this);
    JSValueRegsTemporary result(this);
    GPRReg argumentCountGPR = argumentCount.gpr();
    JSValueRegs resultRegs = result.regs();
    m_jit.load32(CCallHelpers::payloadFor(m_jit.argumentCount(node->origin.semantic)), argumentCountGPR);
    auto argumentOutOfBounds = m_jit.branch32(CCallHelpers::LessThanOrEqual, argumentCountGPR, CCallHelpers::TrustedImm32(node->argumentIndex()));
    m_jit.loadValue(CCallHelpers::addressFor(CCallHelpers::argumentsStart(node->origin.semantic) + node->argumentIndex() - 1), resultRegs);
    auto done = m_jit.jump();

    argumentOutOfBounds.link(&m_jit);
    m_jit.moveValue(jsUndefined(), resultRegs);

    done.link(&m_jit);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileCreateScopedArguments(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    flushRegisters();
    
    // We set up the arguments ourselves, because we have the whole register file and we can
    // set them up directly into the argument registers. This also means that we don't have to
    // invent a four-argument-register shuffle.
    
    // Arguments: 0:exec, 1:structure, 2:start, 3:length, 4:callee, 5:scope
    
    // Do the scopeGPR first, since it might alias an argument register.
    m_jit.setupArgument(5, [&] (GPRReg destGPR) { m_jit.move(scopeGPR, destGPR); });
    
    // These other things could be done in any order.
    m_jit.setupArgument(4, [&] (GPRReg destGPR) { emitGetCallee(node->origin.semantic, destGPR); });
    m_jit.setupArgument(3, [&] (GPRReg destGPR) { emitGetLength(node->origin.semantic, destGPR); });
    m_jit.setupArgument(2, [&] (GPRReg destGPR) { emitGetArgumentStart(node->origin.semantic, destGPR); });
    m_jit.setupArgument(
        1, [&] (GPRReg destGPR) {
            m_jit.move(
                TrustedImmPtr::weakPointer(m_jit.graph(), m_jit.globalObjectFor(node->origin.semantic)->scopedArgumentsStructure()),
                destGPR);
        });
    m_jit.setupArgument(0, [&] (GPRReg destGPR) { m_jit.move(GPRInfo::callFrameRegister, destGPR); });
    
    appendCallSetResult(operationCreateScopedArguments, resultGPR);
    m_jit.exceptionCheck();
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileCreateClonedArguments(Node* node)
{
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    flushRegisters();
    
    // We set up the arguments ourselves, because we have the whole register file and we can
    // set them up directly into the argument registers.
    
    // Arguments: 0:exec, 1:structure, 2:start, 3:length, 4:callee
    m_jit.setupArgument(4, [&] (GPRReg destGPR) { emitGetCallee(node->origin.semantic, destGPR); });
    m_jit.setupArgument(3, [&] (GPRReg destGPR) { emitGetLength(node->origin.semantic, destGPR); });
    m_jit.setupArgument(2, [&] (GPRReg destGPR) { emitGetArgumentStart(node->origin.semantic, destGPR); });
    m_jit.setupArgument(
        1, [&] (GPRReg destGPR) {
            m_jit.move(
                TrustedImmPtr::weakPointer(
                    m_jit.graph(), m_jit.globalObjectFor(node->origin.semantic)->clonedArgumentsStructure()),
                destGPR);
        });
    m_jit.setupArgument(0, [&] (GPRReg destGPR) { m_jit.move(GPRInfo::callFrameRegister, destGPR); });
    
    appendCallSetResult(operationCreateClonedArguments, resultGPR);
    m_jit.exceptionCheck();
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileCreateRest(Node* node)
{
    ASSERT(node->op() == CreateRest);

#if !CPU(X86)
    if (m_jit.graph().isWatchingHavingABadTimeWatchpoint(node)) {
        SpeculateStrictInt32Operand arrayLength(this, node->child1());
        GPRTemporary arrayResult(this);

        GPRReg arrayLengthGPR = arrayLength.gpr();
        GPRReg arrayResultGPR = arrayResult.gpr();

        bool shouldAllowForArrayStorageStructureForLargeArrays = false;
        ASSERT(m_jit.graph().globalObjectFor(node->origin.semantic)->restParameterStructure()->indexingMode() == ArrayWithContiguous || m_jit.graph().globalObjectFor(node->origin.semantic)->isHavingABadTime());
        compileAllocateNewArrayWithSize(m_jit.graph().globalObjectFor(node->origin.semantic), arrayResultGPR, arrayLengthGPR, ArrayWithContiguous, shouldAllowForArrayStorageStructureForLargeArrays);

        GPRTemporary argumentsStart(this);
        GPRReg argumentsStartGPR = argumentsStart.gpr();

        emitGetArgumentStart(node->origin.semantic, argumentsStartGPR);

        GPRTemporary butterfly(this);
        GPRTemporary currentLength(this);
        JSValueRegsTemporary value(this);

        JSValueRegs valueRegs = value.regs();
        GPRReg currentLengthGPR = currentLength.gpr();
        GPRReg butterflyGPR = butterfly.gpr();

        m_jit.loadPtr(MacroAssembler::Address(arrayResultGPR, JSObject::butterflyOffset()), butterflyGPR);

        CCallHelpers::Jump skipLoop = m_jit.branch32(MacroAssembler::Equal, arrayLengthGPR, TrustedImm32(0));
        m_jit.zeroExtend32ToPtr(arrayLengthGPR, currentLengthGPR);
        m_jit.addPtr(Imm32(sizeof(Register) * node->numberOfArgumentsToSkip()), argumentsStartGPR);

        auto loop = m_jit.label();
        m_jit.sub32(TrustedImm32(1), currentLengthGPR);
        m_jit.loadValue(JITCompiler::BaseIndex(argumentsStartGPR, currentLengthGPR, MacroAssembler::TimesEight), valueRegs);
        m_jit.storeValue(valueRegs, MacroAssembler::BaseIndex(butterflyGPR, currentLengthGPR, MacroAssembler::TimesEight));
        m_jit.branch32(MacroAssembler::NotEqual, currentLengthGPR, TrustedImm32(0)).linkTo(loop, &m_jit);

        skipLoop.link(&m_jit);
        cellResult(arrayResultGPR, node);
        return;
    }
#endif // !CPU(X86)

    SpeculateStrictInt32Operand arrayLength(this, node->child1());
    GPRTemporary argumentsStart(this);
    GPRTemporary numberOfArgumentsToSkip(this);

    GPRReg arrayLengthGPR = arrayLength.gpr();
    GPRReg argumentsStartGPR = argumentsStart.gpr();

    emitGetArgumentStart(node->origin.semantic, argumentsStartGPR);

    flushRegisters();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationCreateRest, resultGPR, argumentsStartGPR, Imm32(node->numberOfArgumentsToSkip()), arrayLengthGPR);
    m_jit.exceptionCheck();

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileSpread(Node* node)
{
    ASSERT(node->op() == Spread);

    SpeculateCellOperand operand(this, node->child1());
    GPRReg argument = operand.gpr();

    if (node->child1().useKind() == ArrayUse)
        speculateArray(node->child1(), argument);

    if (m_jit.graph().canDoFastSpread(node, m_state.forNode(node->child1()))) {
#if USE(JSVALUE64)
        GPRTemporary result(this);
        GPRTemporary scratch1(this);
        GPRTemporary scratch2(this);
        GPRTemporary length(this);
        FPRTemporary doubleRegister(this);

        GPRReg resultGPR = result.gpr();
        GPRReg scratch1GPR = scratch1.gpr();
        GPRReg scratch2GPR = scratch2.gpr();
        GPRReg lengthGPR = length.gpr();
        FPRReg doubleFPR = doubleRegister.fpr();

        MacroAssembler::JumpList slowPath;

        m_jit.load8(MacroAssembler::Address(argument, JSCell::indexingTypeAndMiscOffset()), scratch1GPR);
        m_jit.and32(TrustedImm32(IndexingShapeMask), scratch1GPR);
        m_jit.sub32(TrustedImm32(Int32Shape), scratch1GPR);

        slowPath.append(m_jit.branch32(MacroAssembler::Above, scratch1GPR, TrustedImm32(ContiguousShape - Int32Shape)));

        m_jit.loadPtr(MacroAssembler::Address(argument, JSObject::butterflyOffset()), lengthGPR);
        m_jit.load32(MacroAssembler::Address(lengthGPR, Butterfly::offsetOfPublicLength()), lengthGPR);
        static_assert(sizeof(JSValue) == 8 && 1 << 3 == 8, "This is strongly assumed in the code below.");
        m_jit.move(lengthGPR, scratch1GPR);
        m_jit.lshift32(TrustedImm32(3), scratch1GPR);
        m_jit.add32(TrustedImm32(JSFixedArray::offsetOfData()), scratch1GPR);

        m_jit.emitAllocateVariableSizedCell<JSFixedArray>(*m_jit.vm(), resultGPR, TrustedImmPtr(m_jit.graph().registerStructure(m_jit.graph().m_vm.fixedArrayStructure.get())), scratch1GPR, scratch1GPR, scratch2GPR, slowPath);
        m_jit.store32(lengthGPR, MacroAssembler::Address(resultGPR, JSFixedArray::offsetOfSize()));

        m_jit.loadPtr(MacroAssembler::Address(argument, JSObject::butterflyOffset()), scratch1GPR);

        MacroAssembler::JumpList done;

        m_jit.load8(MacroAssembler::Address(argument, JSCell::indexingTypeAndMiscOffset()), scratch2GPR);
        m_jit.and32(TrustedImm32(IndexingShapeMask), scratch2GPR);
        auto isDoubleArray = m_jit.branch32(MacroAssembler::Equal, scratch2GPR, TrustedImm32(DoubleShape));

        {
            done.append(m_jit.branchTest32(MacroAssembler::Zero, lengthGPR));
            auto loopStart = m_jit.label();
            m_jit.sub32(TrustedImm32(1), lengthGPR);
            m_jit.load64(MacroAssembler::BaseIndex(scratch1GPR, lengthGPR, MacroAssembler::TimesEight), scratch2GPR);
            auto notEmpty = m_jit.branchIfNotEmpty(scratch2GPR);
            m_jit.move(TrustedImm64(JSValue::encode(jsUndefined())), scratch2GPR);
            notEmpty.link(&m_jit);
            m_jit.store64(scratch2GPR, MacroAssembler::BaseIndex(resultGPR, lengthGPR, MacroAssembler::TimesEight, JSFixedArray::offsetOfData()));
            m_jit.branchTest32(MacroAssembler::NonZero, lengthGPR).linkTo(loopStart, &m_jit);
            done.append(m_jit.jump());
        }

        isDoubleArray.link(&m_jit);
        {
            done.append(m_jit.branchTest32(MacroAssembler::Zero, lengthGPR));
            auto loopStart = m_jit.label();
            m_jit.sub32(TrustedImm32(1), lengthGPR);
            m_jit.loadDouble(MacroAssembler::BaseIndex(scratch1GPR, lengthGPR, MacroAssembler::TimesEight), doubleFPR);
            auto notEmpty = m_jit.branchIfNotNaN(doubleFPR);
            m_jit.move(TrustedImm64(JSValue::encode(jsUndefined())), scratch2GPR);
            auto doStore = m_jit.jump();
            notEmpty.link(&m_jit);
            m_jit.boxDouble(doubleFPR, scratch2GPR);
            doStore.link(&m_jit);
            m_jit.store64(scratch2GPR, MacroAssembler::BaseIndex(resultGPR, lengthGPR, MacroAssembler::TimesEight, JSFixedArray::offsetOfData()));
            m_jit.branchTest32(MacroAssembler::NonZero, lengthGPR).linkTo(loopStart, &m_jit);
            done.append(m_jit.jump());
        }
        
        m_jit.mutatorFence(*m_jit.vm());

        slowPath.link(&m_jit);
        addSlowPathGenerator(slowPathCall(m_jit.jump(), this, operationSpreadFastArray, resultGPR, argument));

        done.link(&m_jit);
        cellResult(resultGPR, node);
#else
        flushRegisters();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationSpreadFastArray, resultGPR, argument);
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
#endif // USE(JSVALUE64)
    } else {
        flushRegisters();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationSpreadGeneric, resultGPR, argument);
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
    }
}

void SpeculativeJIT::compileNewArray(Node* node)
{
    JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
    if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(node->indexingType())) {
        RegisteredStructure structure = m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()));
        DFG_ASSERT(m_jit.graph(), node, structure->indexingType() == node->indexingType(), structure->indexingType(), node->indexingType());
        ASSERT(
            hasUndecided(structure->indexingType())
            || hasInt32(structure->indexingType())
            || hasDouble(structure->indexingType())
            || hasContiguous(structure->indexingType()));

        unsigned numElements = node->numChildren();
        unsigned vectorLengthHint = node->vectorLengthHint();
        ASSERT(vectorLengthHint >= numElements);

        GPRTemporary result(this);
        GPRTemporary storage(this);

        GPRReg resultGPR = result.gpr();
        GPRReg storageGPR = storage.gpr();

        emitAllocateRawObject(resultGPR, structure, storageGPR, numElements, vectorLengthHint);

        // At this point, one way or another, resultGPR and storageGPR have pointers to
        // the JSArray and the Butterfly, respectively.

        ASSERT(!hasUndecided(structure->indexingType()) || !node->numChildren());

        for (unsigned operandIdx = 0; operandIdx < node->numChildren(); ++operandIdx) {
            Edge use = m_jit.graph().m_varArgChildren[node->firstChild() + operandIdx];
            switch (node->indexingType()) {
            case ALL_BLANK_INDEXING_TYPES:
            case ALL_UNDECIDED_INDEXING_TYPES:
                CRASH();
                break;
            case ALL_DOUBLE_INDEXING_TYPES: {
                SpeculateDoubleOperand operand(this, use);
                FPRReg opFPR = operand.fpr();
                DFG_TYPE_CHECK(
                    JSValueRegs(), use, SpecDoubleReal,
                    m_jit.branchIfNaN(opFPR));
                m_jit.storeDouble(opFPR, MacroAssembler::Address(storageGPR, sizeof(double) * operandIdx));
                break;
            }
            case ALL_INT32_INDEXING_TYPES:
            case ALL_CONTIGUOUS_INDEXING_TYPES: {
                JSValueOperand operand(this, use, ManualOperandSpeculation);
                JSValueRegs operandRegs = operand.jsValueRegs();
                if (hasInt32(node->indexingType())) {
                    DFG_TYPE_CHECK(
                        operandRegs, use, SpecInt32Only,
                        m_jit.branchIfNotInt32(operandRegs));
                }
                m_jit.storeValue(operandRegs, MacroAssembler::Address(storageGPR, sizeof(JSValue) * operandIdx));
                break;
            }
            default:
                CRASH();
                break;
            }
        }

        // Yuck, we should *really* have a way of also returning the storageGPR. But
        // that's the least of what's wrong with this code. We really shouldn't be
        // allocating the array after having computed - and probably spilled to the
        // stack - all of the things that will go into the array. The solution to that
        // bigger problem will also likely fix the redundancy in reloading the storage
        // pointer that we currently have.

        cellResult(resultGPR, node);
        return;
    }

    if (!node->numChildren()) {
        flushRegisters();
        GPRFlushedCallResult result(this);
        callOperation(operationNewEmptyArray, result.gpr(), m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(node->indexingType())));
        m_jit.exceptionCheck();
        cellResult(result.gpr(), node);
        return;
    }

    size_t scratchSize = sizeof(EncodedJSValue) * node->numChildren();
    ScratchBuffer* scratchBuffer = m_jit.vm()->scratchBufferForSize(scratchSize);
    EncodedJSValue* buffer = scratchBuffer ? static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) : nullptr;

    for (unsigned operandIdx = 0; operandIdx < node->numChildren(); ++operandIdx) {
        // Need to perform the speculations that this node promises to perform. If we're
        // emitting code here and the indexing type is not array storage then there is
        // probably something hilarious going on and we're already failing at all the
        // things, but at least we're going to be sound.
        Edge use = m_jit.graph().m_varArgChildren[node->firstChild() + operandIdx];
        switch (node->indexingType()) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            CRASH();
            break;
        case ALL_DOUBLE_INDEXING_TYPES: {
            SpeculateDoubleOperand operand(this, use);
            FPRReg opFPR = operand.fpr();
            DFG_TYPE_CHECK(
                JSValueRegs(), use, SpecDoubleReal,
                m_jit.branchIfNaN(opFPR));
#if USE(JSVALUE64)
            JSValueRegsTemporary scratch(this);
            JSValueRegs scratchRegs = scratch.regs();
            m_jit.boxDouble(opFPR, scratchRegs);
            m_jit.storeValue(scratchRegs, buffer + operandIdx);
#else
            m_jit.storeDouble(opFPR, TrustedImmPtr(buffer + operandIdx));
#endif
            operand.use();
            break;
        }
        case ALL_INT32_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
        case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
            JSValueOperand operand(this, use, ManualOperandSpeculation);
            JSValueRegs operandRegs = operand.jsValueRegs();
            if (hasInt32(node->indexingType())) {
                DFG_TYPE_CHECK(
                    operandRegs, use, SpecInt32Only,
                    m_jit.branchIfNotInt32(operandRegs));
            }
            m_jit.storeValue(operandRegs, buffer + operandIdx);
            operand.use();
            break;
        }
        default:
            CRASH();
            break;
        }
    }

    flushRegisters();

    if (scratchSize) {
        GPRTemporary scratch(this);

        // Tell GC mark phase how much of the scratch buffer is active during call.
        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), scratch.gpr());
        m_jit.storePtr(TrustedImmPtr(scratchSize), scratch.gpr());
    }

    GPRFlushedCallResult result(this);

    callOperation(
        operationNewArray, result.gpr(), m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(node->indexingType())),
        static_cast<void*>(buffer), size_t(node->numChildren()));
    m_jit.exceptionCheck();

    if (scratchSize) {
        GPRTemporary scratch(this);

        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), scratch.gpr());
        m_jit.storePtr(TrustedImmPtr(nullptr), scratch.gpr());
    }

    cellResult(result.gpr(), node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileNewArrayWithSpread(Node* node)
{
    ASSERT(node->op() == NewArrayWithSpread);

#if USE(JSVALUE64)
    if (m_jit.graph().isWatchingHavingABadTimeWatchpoint(node)) {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        BitVector* bitVector = node->bitVector();
        {
            unsigned startLength = 0;
            for (unsigned i = 0; i < node->numChildren(); ++i) {
                if (!bitVector->get(i))
                    ++startLength;
            }

            GPRTemporary length(this);
            GPRReg lengthGPR = length.gpr();
            m_jit.move(TrustedImm32(startLength), lengthGPR);

            for (unsigned i = 0; i < node->numChildren(); ++i) {
                if (bitVector->get(i)) {
                    Edge use = m_jit.graph().varArgChild(node, i);
                    SpeculateCellOperand fixedArray(this, use);
                    GPRReg fixedArrayGPR = fixedArray.gpr();
                    speculationCheck(Overflow, JSValueRegs(), nullptr, m_jit.branchAdd32(MacroAssembler::Overflow, MacroAssembler::Address(fixedArrayGPR, JSFixedArray::offsetOfSize()), lengthGPR));
                }
            }


            bool shouldAllowForArrayStorageStructureForLargeArrays = false;
            ASSERT(m_jit.graph().globalObjectFor(node->origin.semantic)->restParameterStructure()->indexingType() == ArrayWithContiguous || m_jit.graph().globalObjectFor(node->origin.semantic)->isHavingABadTime());
            compileAllocateNewArrayWithSize(m_jit.graph().globalObjectFor(node->origin.semantic), resultGPR, lengthGPR, ArrayWithContiguous, shouldAllowForArrayStorageStructureForLargeArrays);
        }

        GPRTemporary index(this);
        GPRReg indexGPR = index.gpr();

        GPRTemporary storage(this);
        GPRReg storageGPR = storage.gpr();

        m_jit.move(TrustedImm32(0), indexGPR);
        m_jit.loadPtr(MacroAssembler::Address(resultGPR, JSObject::butterflyOffset()), storageGPR);

        for (unsigned i = 0; i < node->numChildren(); ++i) {
            Edge use = m_jit.graph().varArgChild(node, i);
            if (bitVector->get(i)) {
                SpeculateCellOperand fixedArray(this, use);
                GPRReg fixedArrayGPR = fixedArray.gpr();

                GPRTemporary fixedIndex(this);
                GPRReg fixedIndexGPR = fixedIndex.gpr();

                GPRTemporary item(this);
                GPRReg itemGPR = item.gpr();

                GPRTemporary fixedLength(this);
                GPRReg fixedLengthGPR = fixedLength.gpr();

                m_jit.load32(MacroAssembler::Address(fixedArrayGPR, JSFixedArray::offsetOfSize()), fixedLengthGPR);
                m_jit.move(TrustedImm32(0), fixedIndexGPR);
                auto done = m_jit.branchPtr(MacroAssembler::AboveOrEqual, fixedIndexGPR, fixedLengthGPR);
                auto loopStart = m_jit.label();
                m_jit.load64(
                    MacroAssembler::BaseIndex(fixedArrayGPR, fixedIndexGPR, MacroAssembler::TimesEight, JSFixedArray::offsetOfData()),
                    itemGPR);

                m_jit.store64(itemGPR, MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight));
                m_jit.addPtr(TrustedImm32(1), fixedIndexGPR);
                m_jit.addPtr(TrustedImm32(1), indexGPR);
                m_jit.branchPtr(MacroAssembler::Below, fixedIndexGPR, fixedLengthGPR).linkTo(loopStart, &m_jit);

                done.link(&m_jit);
            } else {
                JSValueOperand item(this, use);
                GPRReg itemGPR = item.gpr();
                m_jit.store64(itemGPR, MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight));
                m_jit.addPtr(TrustedImm32(1), indexGPR);
            }
        }

        cellResult(resultGPR, node);
        return;
    }
#endif // USE(JSVALUE64)

    ASSERT(node->numChildren());
    size_t scratchSize = sizeof(EncodedJSValue) * node->numChildren();
    ScratchBuffer* scratchBuffer = m_jit.vm()->scratchBufferForSize(scratchSize);
    EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());

    BitVector* bitVector = node->bitVector();
    for (unsigned i = 0; i < node->numChildren(); ++i) {
        Edge use = m_jit.graph().m_varArgChildren[node->firstChild() + i];
        if (bitVector->get(i)) {
            SpeculateCellOperand fixedArray(this, use);
            GPRReg arrayGPR = fixedArray.gpr();
#if USE(JSVALUE64)
            m_jit.store64(arrayGPR, &buffer[i]);
#else
            char* pointer = static_cast<char*>(static_cast<void*>(&buffer[i]));
            m_jit.store32(arrayGPR, pointer + PayloadOffset);
            m_jit.store32(TrustedImm32(JSValue::CellTag), pointer + TagOffset);
#endif
        } else {
            JSValueOperand input(this, use);
            JSValueRegs inputRegs = input.jsValueRegs();
            m_jit.storeValue(inputRegs, &buffer[i]);
        }
    }

    {
        GPRTemporary scratch(this);
        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), scratch.gpr());
        m_jit.storePtr(TrustedImmPtr(scratchSize), MacroAssembler::Address(scratch.gpr()));
    }

    flushRegisters();

    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    callOperation(operationNewArrayWithSpreadSlow, resultGPR, buffer, node->numChildren());
    m_jit.exceptionCheck();
    {
        GPRTemporary scratch(this);
        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), scratch.gpr());
        m_jit.storePtr(TrustedImmPtr(nullptr), MacroAssembler::Address(scratch.gpr()));
    }

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetRestLength(Node* node)
{
    ASSERT(node->op() == GetRestLength);

    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();

    emitGetLength(node->origin.semantic, resultGPR);
    CCallHelpers::Jump hasNonZeroLength = m_jit.branch32(MacroAssembler::Above, resultGPR, Imm32(node->numberOfArgumentsToSkip()));
    m_jit.move(TrustedImm32(0), resultGPR);
    CCallHelpers::Jump done = m_jit.jump();
    hasNonZeroLength.link(&m_jit);
    if (node->numberOfArgumentsToSkip())
        m_jit.sub32(TrustedImm32(node->numberOfArgumentsToSkip()), resultGPR);
    done.link(&m_jit);
    int32Result(resultGPR, node);
}

void SpeculativeJIT::emitPopulateSliceIndex(Edge& target, Optional<GPRReg> indexGPR, GPRReg lengthGPR, GPRReg resultGPR)
{
    if (target->isInt32Constant()) {
        int32_t value = target->asInt32();
        if (value == 0) {
            m_jit.move(TrustedImm32(0), resultGPR);
            return;
        }

        MacroAssembler::JumpList done;
        if (value > 0) {
            m_jit.move(TrustedImm32(value), resultGPR);
            done.append(m_jit.branch32(MacroAssembler::BelowOrEqual, resultGPR, lengthGPR));
            m_jit.move(lengthGPR, resultGPR);
        } else {
            ASSERT(value != 0);
            m_jit.move(lengthGPR, resultGPR);
            done.append(m_jit.branchAdd32(MacroAssembler::PositiveOrZero, TrustedImm32(value), resultGPR));
            m_jit.move(TrustedImm32(0), resultGPR);
        }
        done.link(&m_jit);
        return;
    }

    Optional<SpeculateInt32Operand> index;
    if (!indexGPR) {
        index.emplace(this, target);
        indexGPR = index->gpr();
    }
    MacroAssembler::JumpList done;

    auto isPositive = m_jit.branch32(MacroAssembler::GreaterThanOrEqual, indexGPR.value(), TrustedImm32(0));
    m_jit.move(lengthGPR, resultGPR);
    done.append(m_jit.branchAdd32(MacroAssembler::PositiveOrZero, indexGPR.value(), resultGPR));
    m_jit.move(TrustedImm32(0), resultGPR);
    done.append(m_jit.jump());

    isPositive.link(&m_jit);
    m_jit.move(indexGPR.value(), resultGPR);
    done.append(m_jit.branch32(MacroAssembler::BelowOrEqual, resultGPR, lengthGPR));
    m_jit.move(lengthGPR, resultGPR);

    done.link(&m_jit);
}

void SpeculativeJIT::compileArraySlice(Node* node)
{
    ASSERT(node->op() == ArraySlice);

    JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);

    GPRTemporary temp(this);
    StorageOperand storage(this, m_jit.graph().varArgChild(node, node->numChildren() - 1));
    GPRTemporary result(this);
    
    GPRReg storageGPR = storage.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg tempGPR = temp.gpr();

    if (node->numChildren() == 2)
        m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), tempGPR);
    else {
        ASSERT(node->numChildren() == 3 || node->numChildren() == 4);
        GPRTemporary tempLength(this);
        GPRReg lengthGPR = tempLength.gpr();
        m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), lengthGPR);

        if (node->numChildren() == 4)
            emitPopulateSliceIndex(m_jit.graph().varArgChild(node, 2), WTF::nullopt, lengthGPR, tempGPR);
        else
            m_jit.move(lengthGPR, tempGPR);

        if (m_jit.graph().varArgChild(node, 1)->isInt32Constant() && m_jit.graph().varArgChild(node, 1)->asInt32() == 0) {
            // Do nothing for array.slice(0, end) or array.slice(0) cases.
            // `tempGPR` already points to the size of a newly created array.
        } else {
            GPRTemporary tempStartIndex(this);
            GPRReg startGPR = tempStartIndex.gpr();
            emitPopulateSliceIndex(m_jit.graph().varArgChild(node, 1), WTF::nullopt, lengthGPR, startGPR);

            auto tooBig = m_jit.branch32(MacroAssembler::Above, startGPR, tempGPR);
            m_jit.sub32(startGPR, tempGPR); // the size of the array we'll make.
            auto done = m_jit.jump();

            tooBig.link(&m_jit);
            m_jit.move(TrustedImm32(0), tempGPR);
            done.link(&m_jit);
        }
    }


    GPRTemporary temp3(this);
    GPRReg tempValue = temp3.gpr();
    {
        SpeculateCellOperand cell(this, m_jit.graph().varArgChild(node, 0));
        m_jit.load8(MacroAssembler::Address(cell.gpr(), JSCell::indexingTypeAndMiscOffset()), tempValue);
        // We can ignore the writability of the cell since we won't write to the source.
        m_jit.and32(TrustedImm32(AllWritableArrayTypesAndHistory), tempValue);
    }

    {
        JSValueRegsTemporary emptyValue(this);
        JSValueRegs emptyValueRegs = emptyValue.regs();

        GPRTemporary storage(this);
        GPRReg storageResultGPR = storage.gpr();

        GPRReg sizeGPR = tempGPR; 

        CCallHelpers::JumpList done;

        auto emitMoveEmptyValue = [&] (JSValue v) {
            m_jit.moveValue(v, emptyValueRegs);
        };

        auto isContiguous = m_jit.branch32(MacroAssembler::Equal, tempValue, TrustedImm32(ArrayWithContiguous));
        auto isInt32 = m_jit.branch32(MacroAssembler::Equal, tempValue, TrustedImm32(ArrayWithInt32));
        // When we emit an ArraySlice, we dominate the use of the array by a CheckStructure
        // to ensure the incoming array is one to be one of the original array structures
        // with one of the following indexing shapes: Int32, Contiguous, Double. Therefore,
        // we're a double array here.
        m_jit.move(TrustedImmPtr(m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithDouble))), tempValue);
        emitMoveEmptyValue(jsNaN());
        done.append(m_jit.jump());

        isContiguous.link(&m_jit);
        m_jit.move(TrustedImmPtr(m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous))), tempValue);
        emitMoveEmptyValue(JSValue());
        done.append(m_jit.jump());

        isInt32.link(&m_jit);
        m_jit.move(TrustedImmPtr(m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithInt32))), tempValue);
        emitMoveEmptyValue(JSValue());

        done.link(&m_jit);

        MacroAssembler::JumpList slowCases;
        m_jit.move(TrustedImmPtr(nullptr), storageResultGPR);
        // Enable the fast case on 64-bit platforms, where a sufficient amount of GP registers should be available.
        // Other platforms could support the same approach with custom code, but that is not currently worth the extra code maintenance.
        if (is64Bit()) {
            GPRTemporary scratch(this);
            GPRTemporary scratch2(this);
            GPRReg scratchGPR = scratch.gpr();
            GPRReg scratch2GPR = scratch2.gpr();

            emitAllocateButterfly(storageResultGPR, sizeGPR, scratchGPR, scratch2GPR, resultGPR, slowCases);
            emitInitializeButterfly(storageResultGPR, sizeGPR, emptyValueRegs, scratchGPR);
            emitAllocateJSObject<JSArray>(resultGPR, tempValue, storageResultGPR, scratchGPR, scratch2GPR, slowCases);
            m_jit.mutatorFence(*m_jit.vm());
        } else {
            slowCases.append(m_jit.jump());
        }

        addSlowPathGenerator(std::make_unique<CallArrayAllocatorWithVariableStructureVariableSizeSlowPathGenerator>(
            slowCases, this, operationNewArrayWithSize, resultGPR, tempValue, sizeGPR, storageResultGPR));
    }

    GPRTemporary temp4(this);
    GPRReg loadIndex = temp4.gpr();

    if (node->numChildren() == 2) {
        m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), tempGPR);
        m_jit.move(TrustedImm32(0), loadIndex);
    } else {
        m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), tempValue);
        if (node->numChildren() == 4)
            emitPopulateSliceIndex(m_jit.graph().varArgChild(node, 2), WTF::nullopt, tempValue, tempGPR);
        else
            m_jit.move(tempValue, tempGPR);
        emitPopulateSliceIndex(m_jit.graph().varArgChild(node, 1), WTF::nullopt, tempValue, loadIndex);
    }

    GPRTemporary temp5(this);
    GPRReg storeIndex = temp5.gpr();
    m_jit.move(TrustedImmPtr(nullptr), storeIndex);

    GPRTemporary temp2(this);
    GPRReg resultButterfly = temp2.gpr();

    m_jit.loadPtr(MacroAssembler::Address(resultGPR, JSObject::butterflyOffset()), resultButterfly);
    m_jit.zeroExtend32ToPtr(tempGPR, tempGPR);
    m_jit.zeroExtend32ToPtr(loadIndex, loadIndex);
    auto done = m_jit.branchPtr(MacroAssembler::AboveOrEqual, loadIndex, tempGPR);

    auto loop = m_jit.label();
#if USE(JSVALUE64)
    m_jit.load64(
        MacroAssembler::BaseIndex(storageGPR, loadIndex, MacroAssembler::TimesEight), tempValue);
    m_jit.store64(
        tempValue, MacroAssembler::BaseIndex(resultButterfly, storeIndex, MacroAssembler::TimesEight));
#else
    m_jit.load32(
        MacroAssembler::BaseIndex(storageGPR, loadIndex, MacroAssembler::TimesEight, PayloadOffset), tempValue);
    m_jit.store32(
        tempValue, MacroAssembler::BaseIndex(resultButterfly, storeIndex, MacroAssembler::TimesEight, PayloadOffset));
    m_jit.load32(
        MacroAssembler::BaseIndex(storageGPR, loadIndex, MacroAssembler::TimesEight, TagOffset), tempValue);
    m_jit.store32(
        tempValue, MacroAssembler::BaseIndex(resultButterfly, storeIndex, MacroAssembler::TimesEight, TagOffset));
#endif // USE(JSVALUE64)
    m_jit.addPtr(TrustedImm32(1), loadIndex);
    m_jit.addPtr(TrustedImm32(1), storeIndex);
    m_jit.branchPtr(MacroAssembler::Below, loadIndex, tempGPR).linkTo(loop, &m_jit);

    done.link(&m_jit);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileArrayIndexOf(Node* node)
{
    ASSERT(node->op() == ArrayIndexOf);

    StorageOperand storage(this, m_jit.graph().varArgChild(node, node->numChildren() == 3 ? 2 : 3));
    GPRTemporary index(this);
    GPRTemporary tempLength(this);

    GPRReg storageGPR = storage.gpr();
    GPRReg indexGPR = index.gpr();
    GPRReg lengthGPR = tempLength.gpr();

    m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), lengthGPR);

    if (node->numChildren() == 4)
        emitPopulateSliceIndex(m_jit.graph().varArgChild(node, 2), WTF::nullopt, lengthGPR, indexGPR);
    else
        m_jit.move(TrustedImm32(0), indexGPR);

    Edge& searchElementEdge = m_jit.graph().varArgChild(node, 1);
    switch (searchElementEdge.useKind()) {
    case Int32Use:
    case ObjectUse:
    case SymbolUse:
    case OtherUse: {
        auto emitLoop = [&] (auto emitCompare) {
#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
            m_jit.clearRegisterAllocationOffsets();
#endif

            m_jit.zeroExtend32ToPtr(lengthGPR, lengthGPR);
            m_jit.zeroExtend32ToPtr(indexGPR, indexGPR);

            auto loop = m_jit.label();
            auto notFound = m_jit.branch32(CCallHelpers::Equal, indexGPR, lengthGPR);

            auto found = emitCompare();

            m_jit.add32(TrustedImm32(1), indexGPR);
            m_jit.jump().linkTo(loop, &m_jit);

            notFound.link(&m_jit);
            m_jit.move(TrustedImm32(-1), indexGPR);
            found.link(&m_jit);
            int32Result(indexGPR, node);
        };

        if (searchElementEdge.useKind() == Int32Use) {
            ASSERT(node->arrayMode().type() == Array::Int32);
#if USE(JSVALUE64)
            JSValueOperand searchElement(this, searchElementEdge, ManualOperandSpeculation);
            JSValueRegs searchElementRegs = searchElement.jsValueRegs();
            speculateInt32(searchElementEdge, searchElementRegs);
            GPRReg searchElementGPR = searchElementRegs.payloadGPR();
#else
            SpeculateInt32Operand searchElement(this, searchElementEdge);
            GPRReg searchElementGPR = searchElement.gpr();

            GPRTemporary temp(this);
            GPRReg tempGPR = temp.gpr();
#endif
            emitLoop([&] () {
#if USE(JSVALUE64)
                auto found = m_jit.branch64(CCallHelpers::Equal, MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight), searchElementGPR);
#else
                auto skip = m_jit.branch32(CCallHelpers::NotEqual, MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, TagOffset), TrustedImm32(JSValue::Int32Tag));
                m_jit.load32(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, PayloadOffset), tempGPR);
                auto found = m_jit.branch32(CCallHelpers::Equal, tempGPR, searchElementGPR);
                skip.link(&m_jit);
#endif
                return found;
            });
            return;
        }

        if (searchElementEdge.useKind() == OtherUse) {
            ASSERT(node->arrayMode().type() == Array::Contiguous);
            JSValueOperand searchElement(this, searchElementEdge, ManualOperandSpeculation);
            GPRTemporary temp(this);

            JSValueRegs searchElementRegs = searchElement.jsValueRegs();
            GPRReg tempGPR = temp.gpr();
            speculateOther(searchElementEdge, searchElementRegs, tempGPR);

            emitLoop([&] () {
#if USE(JSVALUE64)
                auto found = m_jit.branch64(CCallHelpers::Equal, MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight), searchElementRegs.payloadGPR());
#else
                m_jit.load32(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, TagOffset), tempGPR);
                auto found = m_jit.branch32(CCallHelpers::Equal, tempGPR, searchElementRegs.tagGPR());
#endif
                return found;
            });
            return;
        }

        ASSERT(node->arrayMode().type() == Array::Contiguous);
        SpeculateCellOperand searchElement(this, searchElementEdge);
        GPRReg searchElementGPR = searchElement.gpr();

        if (searchElementEdge.useKind() == ObjectUse)
            speculateObject(searchElementEdge, searchElementGPR);
        else {
            ASSERT(searchElementEdge.useKind() == SymbolUse);
            speculateSymbol(searchElementEdge, searchElementGPR);
        }

#if USE(JSVALUE32_64)
        GPRTemporary temp(this);
        GPRReg tempGPR = temp.gpr();
#endif

        emitLoop([&] () {
#if USE(JSVALUE64)
            auto found = m_jit.branch64(CCallHelpers::Equal, MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight), searchElementGPR);
#else
            auto skip = m_jit.branch32(CCallHelpers::NotEqual, MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, TagOffset), TrustedImm32(JSValue::CellTag));
            m_jit.load32(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, PayloadOffset), tempGPR);
            auto found = m_jit.branch32(CCallHelpers::Equal, tempGPR, searchElementGPR);
            skip.link(&m_jit);
#endif
            return found;
        });
        return;
    }

    case DoubleRepUse: {
        ASSERT(node->arrayMode().type() == Array::Double);
        SpeculateDoubleOperand searchElement(this, searchElementEdge);
        FPRTemporary tempDouble(this);

        FPRReg searchElementFPR = searchElement.fpr();
        FPRReg tempFPR = tempDouble.fpr();

#if ENABLE(DFG_REGISTER_ALLOCATION_VALIDATION)
        m_jit.clearRegisterAllocationOffsets();
#endif

        m_jit.zeroExtend32ToPtr(lengthGPR, lengthGPR);
        m_jit.zeroExtend32ToPtr(indexGPR, indexGPR);

        auto loop = m_jit.label();
        auto notFound = m_jit.branch32(CCallHelpers::Equal, indexGPR, lengthGPR);
        m_jit.loadDouble(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight), tempFPR);
        auto found = m_jit.branchDouble(CCallHelpers::DoubleEqual, tempFPR, searchElementFPR);
        m_jit.add32(TrustedImm32(1), indexGPR);
        m_jit.jump().linkTo(loop, &m_jit);

        notFound.link(&m_jit);
        m_jit.move(TrustedImm32(-1), indexGPR);
        found.link(&m_jit);
        int32Result(indexGPR, node);
        return;
    }

    case StringUse: {
        ASSERT(node->arrayMode().type() == Array::Contiguous);
        SpeculateCellOperand searchElement(this, searchElementEdge);

        GPRReg searchElementGPR = searchElement.gpr();

        speculateString(searchElementEdge, searchElementGPR);

        flushRegisters();

        callOperation(operationArrayIndexOfString, lengthGPR, storageGPR, searchElementGPR, indexGPR);
        m_jit.exceptionCheck();

        int32Result(lengthGPR, node);
        return;
    }

    case UntypedUse: {
        JSValueOperand searchElement(this, searchElementEdge);

        JSValueRegs searchElementRegs = searchElement.jsValueRegs();

        flushRegisters();
        switch (node->arrayMode().type()) {
        case Array::Double:
            callOperation(operationArrayIndexOfValueDouble, lengthGPR, storageGPR, searchElementRegs, indexGPR);
            break;
        case Array::Int32:
        case Array::Contiguous:
            callOperation(operationArrayIndexOfValueInt32OrContiguous, lengthGPR, storageGPR, searchElementRegs, indexGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        m_jit.exceptionCheck();

        int32Result(lengthGPR, node);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
}

void SpeculativeJIT::compileArrayPush(Node* node)
{
    ASSERT(node->arrayMode().isJSArray());

    Edge& storageEdge = m_jit.graph().varArgChild(node, 0);
    Edge& arrayEdge = m_jit.graph().varArgChild(node, 1);

    SpeculateCellOperand base(this, arrayEdge);
    GPRTemporary storageLength(this);

    GPRReg baseGPR = base.gpr();
    GPRReg storageLengthGPR = storageLength.gpr();

    StorageOperand storage(this, storageEdge);
    GPRReg storageGPR = storage.gpr();
    unsigned elementOffset = 2;
    unsigned elementCount = node->numChildren() - elementOffset;

#if USE(JSVALUE32_64)
    GPRTemporary tag(this);
    GPRReg tagGPR = tag.gpr();
    JSValueRegs resultRegs { tagGPR, storageLengthGPR };
#else
    JSValueRegs resultRegs { storageLengthGPR };
#endif

    auto getStorageBufferAddress = [&] (GPRReg storageGPR, GPRReg indexGPR, int32_t offset, GPRReg bufferGPR) {
        static_assert(sizeof(JSValue) == 8 && 1 << 3 == 8, "This is strongly assumed in the code below.");
        m_jit.getEffectiveAddress(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, offset), bufferGPR);
    };

    switch (node->arrayMode().type()) {
    case Array::Int32:
    case Array::Contiguous: {
        if (elementCount == 1) {
            Edge& element = m_jit.graph().varArgChild(node, elementOffset);
            JSValueOperand value(this, element, ManualOperandSpeculation);
            JSValueRegs valueRegs = value.jsValueRegs();

            if (node->arrayMode().type() == Array::Int32)
                DFG_ASSERT(m_jit.graph(), node, !needsTypeCheck(element, SpecInt32Only));

            m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
            MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));
            m_jit.storeValue(valueRegs, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight));
            m_jit.add32(TrustedImm32(1), storageLengthGPR);
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
            m_jit.boxInt32(storageLengthGPR, resultRegs);

            addSlowPathGenerator(
                slowPathCall(slowPath, this, operationArrayPush, resultRegs, valueRegs, baseGPR));

            jsValueResult(resultRegs, node);
            return;
        }

        GPRTemporary buffer(this);
        GPRReg bufferGPR = buffer.gpr();

        m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
        m_jit.move(storageLengthGPR, bufferGPR);
        m_jit.add32(TrustedImm32(elementCount), bufferGPR);
        MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::Above, bufferGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));

        m_jit.store32(bufferGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
        getStorageBufferAddress(storageGPR, storageLengthGPR, 0, bufferGPR);
        m_jit.add32(TrustedImm32(elementCount), storageLengthGPR);
        m_jit.boxInt32(storageLengthGPR, resultRegs);
        auto storageDone = m_jit.jump();

        slowPath.link(&m_jit);

        size_t scratchSize = sizeof(EncodedJSValue) * elementCount;
        ScratchBuffer* scratchBuffer = m_jit.vm()->scratchBufferForSize(scratchSize);
        m_jit.move(TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())), bufferGPR);
        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), storageLengthGPR);
        m_jit.storePtr(TrustedImmPtr(scratchSize), MacroAssembler::Address(storageLengthGPR));

        storageDone.link(&m_jit);
        for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Edge& element = m_jit.graph().varArgChild(node, elementIndex + elementOffset);
            JSValueOperand value(this, element, ManualOperandSpeculation);
            JSValueRegs valueRegs = value.jsValueRegs();

            if (node->arrayMode().type() == Array::Int32)
                DFG_ASSERT(m_jit.graph(), node, !needsTypeCheck(element, SpecInt32Only));

            m_jit.storeValue(valueRegs, MacroAssembler::Address(bufferGPR, sizeof(EncodedJSValue) * elementIndex));
            value.use();
        }

        MacroAssembler::Jump fastPath = m_jit.branchPtr(MacroAssembler::NotEqual, bufferGPR, TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())));

        addSlowPathGenerator(slowPathCall(m_jit.jump(), this, operationArrayPushMultiple, resultRegs, baseGPR, bufferGPR, TrustedImm32(elementCount)));

        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), bufferGPR);
        m_jit.storePtr(TrustedImmPtr(nullptr), MacroAssembler::Address(bufferGPR));

        base.use();
        storage.use();

        fastPath.link(&m_jit);
        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        return;
    }

    case Array::Double: {
        if (elementCount == 1) {
            Edge& element = m_jit.graph().varArgChild(node, elementOffset);
            SpeculateDoubleOperand value(this, element);
            FPRReg valueFPR = value.fpr();

            DFG_ASSERT(m_jit.graph(), node, !needsTypeCheck(element, SpecDoubleReal));

            m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
            MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));
            m_jit.storeDouble(valueFPR, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight));
            m_jit.add32(TrustedImm32(1), storageLengthGPR);
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
            m_jit.boxInt32(storageLengthGPR, resultRegs);

            addSlowPathGenerator(
                slowPathCall(slowPath, this, operationArrayPushDouble, resultRegs, valueFPR, baseGPR));

            jsValueResult(resultRegs, node);
            return;
        }

        GPRTemporary buffer(this);
        GPRReg bufferGPR = buffer.gpr();

        m_jit.load32(MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()), storageLengthGPR);
        m_jit.move(storageLengthGPR, bufferGPR);
        m_jit.add32(TrustedImm32(elementCount), bufferGPR);
        MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::Above, bufferGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfVectorLength()));

        m_jit.store32(bufferGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
        getStorageBufferAddress(storageGPR, storageLengthGPR, 0, bufferGPR);
        m_jit.add32(TrustedImm32(elementCount), storageLengthGPR);
        m_jit.boxInt32(storageLengthGPR, resultRegs);
        auto storageDone = m_jit.jump();

        slowPath.link(&m_jit);

        size_t scratchSize = sizeof(double) * elementCount;
        ScratchBuffer* scratchBuffer = m_jit.vm()->scratchBufferForSize(scratchSize);
        m_jit.move(TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())), bufferGPR);
        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), storageLengthGPR);
        m_jit.storePtr(TrustedImmPtr(scratchSize), MacroAssembler::Address(storageLengthGPR));

        storageDone.link(&m_jit);
        for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Edge& element = m_jit.graph().varArgChild(node, elementIndex + elementOffset);
            SpeculateDoubleOperand value(this, element);
            FPRReg valueFPR = value.fpr();

            DFG_ASSERT(m_jit.graph(), node, !needsTypeCheck(element, SpecDoubleReal));

            m_jit.storeDouble(valueFPR, MacroAssembler::Address(bufferGPR, sizeof(double) * elementIndex));
            value.use();
        }

        MacroAssembler::Jump fastPath = m_jit.branchPtr(MacroAssembler::NotEqual, bufferGPR, TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())));

        addSlowPathGenerator(slowPathCall(m_jit.jump(), this, operationArrayPushDoubleMultiple, resultRegs, baseGPR, bufferGPR, TrustedImm32(elementCount)));

        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), bufferGPR);
        m_jit.storePtr(TrustedImmPtr(nullptr), MacroAssembler::Address(bufferGPR));

        base.use();
        storage.use();

        fastPath.link(&m_jit);
        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        return;
    }

    case Array::ArrayStorage: {
        // This ensures that the result of ArrayPush is Int32 in AI.
        int32_t largestPositiveInt32Length = 0x7fffffff - elementCount;
        if (elementCount == 1) {
            Edge& element = m_jit.graph().varArgChild(node, elementOffset);
            JSValueOperand value(this, element);
            JSValueRegs valueRegs = value.jsValueRegs();

            m_jit.load32(MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()), storageLengthGPR);

            // Refuse to handle bizarre lengths.
            speculationCheck(Uncountable, JSValueRegs(), 0, m_jit.branch32(MacroAssembler::Above, storageLengthGPR, TrustedImm32(largestPositiveInt32Length)));

            MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::AboveOrEqual, storageLengthGPR, MacroAssembler::Address(storageGPR, ArrayStorage::vectorLengthOffset()));

            m_jit.storeValue(valueRegs, MacroAssembler::BaseIndex(storageGPR, storageLengthGPR, MacroAssembler::TimesEight, ArrayStorage::vectorOffset()));

            m_jit.add32(TrustedImm32(1), storageLengthGPR);
            m_jit.store32(storageLengthGPR, MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()));
            m_jit.add32(TrustedImm32(1), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
            m_jit.boxInt32(storageLengthGPR, resultRegs);

            addSlowPathGenerator(
                slowPathCall(slowPath, this, operationArrayPush, resultRegs, valueRegs, baseGPR));

            jsValueResult(resultRegs, node);
            return;
        }

        GPRTemporary buffer(this);
        GPRReg bufferGPR = buffer.gpr();

        m_jit.load32(MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()), storageLengthGPR);

        // Refuse to handle bizarre lengths.
        speculationCheck(Uncountable, JSValueRegs(), 0, m_jit.branch32(MacroAssembler::Above, storageLengthGPR, TrustedImm32(largestPositiveInt32Length)));

        m_jit.move(storageLengthGPR, bufferGPR);
        m_jit.add32(TrustedImm32(elementCount), bufferGPR);
        MacroAssembler::Jump slowPath = m_jit.branch32(MacroAssembler::Above, bufferGPR, MacroAssembler::Address(storageGPR, ArrayStorage::vectorLengthOffset()));

        m_jit.store32(bufferGPR, MacroAssembler::Address(storageGPR, ArrayStorage::lengthOffset()));
        getStorageBufferAddress(storageGPR, storageLengthGPR, ArrayStorage::vectorOffset(), bufferGPR);
        m_jit.add32(TrustedImm32(elementCount), MacroAssembler::Address(storageGPR, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
        m_jit.add32(TrustedImm32(elementCount), storageLengthGPR);
        m_jit.boxInt32(storageLengthGPR, resultRegs);
        auto storageDone = m_jit.jump();

        slowPath.link(&m_jit);

        size_t scratchSize = sizeof(EncodedJSValue) * elementCount;
        ScratchBuffer* scratchBuffer = m_jit.vm()->scratchBufferForSize(scratchSize);
        m_jit.move(TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())), bufferGPR);
        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), storageLengthGPR);
        m_jit.storePtr(TrustedImmPtr(scratchSize), MacroAssembler::Address(storageLengthGPR));

        storageDone.link(&m_jit);
        for (unsigned elementIndex = 0; elementIndex < elementCount; ++elementIndex) {
            Edge& element = m_jit.graph().varArgChild(node, elementIndex + elementOffset);
            JSValueOperand value(this, element);
            JSValueRegs valueRegs = value.jsValueRegs();

            m_jit.storeValue(valueRegs, MacroAssembler::Address(bufferGPR, sizeof(EncodedJSValue) * elementIndex));
            value.use();
        }

        MacroAssembler::Jump fastPath = m_jit.branchPtr(MacroAssembler::NotEqual, bufferGPR, TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer())));

        addSlowPathGenerator(
            slowPathCall(m_jit.jump(), this, operationArrayPushMultiple, resultRegs, baseGPR, bufferGPR, TrustedImm32(elementCount)));

        m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), bufferGPR);
        m_jit.storePtr(TrustedImmPtr(nullptr), MacroAssembler::Address(bufferGPR));

        base.use();
        storage.use();

        fastPath.link(&m_jit);
        jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
        return;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileNotifyWrite(Node* node)
{
    WatchpointSet* set = node->watchpointSet();
    
    JITCompiler::Jump slowCase = m_jit.branch8(
        JITCompiler::NotEqual,
        JITCompiler::AbsoluteAddress(set->addressOfState()),
        TrustedImm32(IsInvalidated));
    
    addSlowPathGenerator(
        slowPathCall(slowCase, this, operationNotifyWrite, NeedToSpill, ExceptionCheckRequirement::CheckNotNeeded, NoResult, set));
    
    noResult(node);
}

void SpeculativeJIT::compileIsObject(Node* node)
{
    JSValueOperand value(this, node->child1());
    GPRTemporary result(this, Reuse, value, TagWord);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg resultGPR = result.gpr();

    JITCompiler::Jump isNotCell = m_jit.branchIfNotCell(valueRegs);

    m_jit.compare8(JITCompiler::AboveOrEqual,
        JITCompiler::Address(valueRegs.payloadGPR(), JSCell::typeInfoTypeOffset()),
        TrustedImm32(ObjectType),
        resultGPR);
    JITCompiler::Jump done = m_jit.jump();

    isNotCell.link(&m_jit);
    m_jit.move(TrustedImm32(0), resultGPR);

    done.link(&m_jit);
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileIsObjectOrNull(Node* node)
{
    JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
    
    JSValueOperand value(this, node->child1());
    JSValueRegs valueRegs = value.jsValueRegs();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    JITCompiler::Jump isCell = m_jit.branchIfCell(valueRegs);
    
    JITCompiler::Jump isNull = m_jit.branchIfEqual(valueRegs, jsNull());
    JITCompiler::Jump isNonNullNonCell = m_jit.jump();
    
    isCell.link(&m_jit);
    JITCompiler::Jump isFunction = m_jit.branchIfFunction(valueRegs.payloadGPR());
    JITCompiler::Jump notObject = m_jit.branchIfNotObject(valueRegs.payloadGPR());
    
    JITCompiler::Jump slowPath = m_jit.branchTest8(
        JITCompiler::NonZero,
        JITCompiler::Address(valueRegs.payloadGPR(), JSCell::typeInfoFlagsOffset()),
        TrustedImm32(MasqueradesAsUndefined | OverridesGetCallData));
    
    isNull.link(&m_jit);
    m_jit.move(TrustedImm32(1), resultGPR);
    JITCompiler::Jump done = m_jit.jump();
    
    isNonNullNonCell.link(&m_jit);
    isFunction.link(&m_jit);
    notObject.link(&m_jit);
    m_jit.move(TrustedImm32(0), resultGPR);
    
    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationObjectIsObject, resultGPR, globalObject,
            valueRegs.payloadGPR()));
    
    done.link(&m_jit);
    
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileIsFunction(Node* node)
{
    JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
    
    JSValueOperand value(this, node->child1());
    JSValueRegs valueRegs = value.jsValueRegs();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    JITCompiler::Jump notCell = m_jit.branchIfNotCell(valueRegs);
    JITCompiler::Jump isFunction = m_jit.branchIfFunction(valueRegs.payloadGPR());
    JITCompiler::Jump notObject = m_jit.branchIfNotObject(valueRegs.payloadGPR());
    
    JITCompiler::Jump slowPath = m_jit.branchTest8(
        JITCompiler::NonZero,
        JITCompiler::Address(valueRegs.payloadGPR(), JSCell::typeInfoFlagsOffset()),
        TrustedImm32(MasqueradesAsUndefined | OverridesGetCallData));
    
    notCell.link(&m_jit);
    notObject.link(&m_jit);
    m_jit.move(TrustedImm32(0), resultGPR);
    JITCompiler::Jump done = m_jit.jump();
    
    isFunction.link(&m_jit);
    m_jit.move(TrustedImm32(1), resultGPR);
    
    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationObjectIsFunction, resultGPR, globalObject,
            valueRegs.payloadGPR()));
    
    done.link(&m_jit);
    
    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileTypeOf(Node* node)
{
    JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
    
    JSValueOperand value(this, node->child1());
    JSValueRegs valueRegs = value.jsValueRegs();
    
    GPRTemporary result(this);
    GPRReg resultGPR = result.gpr();
    
    JITCompiler::JumpList done;
    JITCompiler::Jump slowPath;
    m_jit.emitTypeOf(
        valueRegs, resultGPR,
        [&] (TypeofType type, bool fallsThrough) {
            m_jit.move(TrustedImmPtr::weakPointer(m_jit.graph(), m_jit.vm()->smallStrings.typeString(type)), resultGPR);
            if (!fallsThrough)
                done.append(m_jit.jump());
        },
        [&] (JITCompiler::Jump theSlowPath) {
            slowPath = theSlowPath;
        });
    done.link(&m_jit);

    addSlowPathGenerator(
        slowPathCall(
            slowPath, this, operationTypeOfObject, resultGPR, globalObject,
            valueRegs.payloadGPR()));
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::emitStructureCheck(Node* node, GPRReg cellGPR, GPRReg tempGPR)
{
    ASSERT(node->structureSet().size());
    
    if (node->structureSet().size() == 1) {
        speculationCheck(
            BadCache, JSValueSource::unboxedCell(cellGPR), 0,
            m_jit.branchWeakStructure(
                JITCompiler::NotEqual,
                JITCompiler::Address(cellGPR, JSCell::structureIDOffset()),
                node->structureSet()[0]));
    } else {
        std::unique_ptr<GPRTemporary> structure;
        GPRReg structureGPR;
        
        if (tempGPR == InvalidGPRReg) {
            structure = std::make_unique<GPRTemporary>(this);
            structureGPR = structure->gpr();
        } else
            structureGPR = tempGPR;
        
        m_jit.load32(JITCompiler::Address(cellGPR, JSCell::structureIDOffset()), structureGPR);
        
        JITCompiler::JumpList done;
        
        for (size_t i = 0; i < node->structureSet().size() - 1; ++i) {
            done.append(
                m_jit.branchWeakStructure(JITCompiler::Equal, structureGPR, node->structureSet()[i]));
        }
        
        speculationCheck(
            BadCache, JSValueSource::unboxedCell(cellGPR), 0,
            m_jit.branchWeakStructure(
                JITCompiler::NotEqual, structureGPR, node->structureSet().last()));
        
        done.link(&m_jit);
    }
}

void SpeculativeJIT::compileCheckCell(Node* node)
{
    SpeculateCellOperand cell(this, node->child1());
    speculationCheck(BadCell, JSValueSource::unboxedCell(cell.gpr()), node->child1(), m_jit.branchWeakPtr(JITCompiler::NotEqual, cell.gpr(), node->cellOperand()->cell()));
    noResult(node);
}

void SpeculativeJIT::compileCheckNotEmpty(Node* node)
{
    JSValueOperand operand(this, node->child1());
    JSValueRegs regs = operand.jsValueRegs();
    speculationCheck(TDZFailure, JSValueSource(), nullptr, m_jit.branchIfEmpty(regs));
    noResult(node);
}

void SpeculativeJIT::compileCheckStructure(Node* node)
{
    switch (node->child1().useKind()) {
    case CellUse:
    case KnownCellUse: {
        SpeculateCellOperand cell(this, node->child1());
        emitStructureCheck(node, cell.gpr(), InvalidGPRReg);
        noResult(node);
        return;
    }

    case CellOrOtherUse: {
        JSValueOperand value(this, node->child1(), ManualOperandSpeculation);
        GPRTemporary temp(this);

        JSValueRegs valueRegs = value.jsValueRegs();
        GPRReg tempGPR = temp.gpr();

        JITCompiler::Jump cell = m_jit.branchIfCell(valueRegs);
        DFG_TYPE_CHECK(
            valueRegs, node->child1(), SpecCell | SpecOther,
            m_jit.branchIfNotOther(valueRegs, tempGPR));
        JITCompiler::Jump done = m_jit.jump();
        cell.link(&m_jit);
        emitStructureCheck(node, valueRegs.payloadGPR(), tempGPR);
        done.link(&m_jit);
        noResult(node);
        return;
    }

    default:
        DFG_CRASH(m_jit.graph(), node, "Bad use kind");
        return;
    }
}

void SpeculativeJIT::compileAllocatePropertyStorage(Node* node)
{
    ASSERT(!node->transition()->previous->outOfLineCapacity());
    ASSERT(initialOutOfLineCapacity == node->transition()->next->outOfLineCapacity());
    
    size_t size = initialOutOfLineCapacity * sizeof(JSValue);

    Allocator allocator = m_jit.vm()->jsValueGigacageAuxiliarySpace.allocatorForNonVirtual(size, AllocatorForMode::AllocatorIfExists);

    if (!allocator || node->transition()->previous->couldHaveIndexingHeader()) {
        SpeculateCellOperand base(this, node->child1());
        
        GPRReg baseGPR = base.gpr();
        
        flushRegisters();

        GPRFlushedCallResult result(this);
        callOperation(operationAllocateComplexPropertyStorageWithInitialCapacity, result.gpr(), baseGPR);
        m_jit.exceptionCheck();
        
        storageResult(result.gpr(), node);
        return;
    }
    
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);
        
    GPRReg scratchGPR1 = scratch1.gpr();
    GPRReg scratchGPR2 = scratch2.gpr();
    GPRReg scratchGPR3 = scratch3.gpr();
        
    JITCompiler::JumpList slowPath;
    m_jit.emitAllocate(scratchGPR1, JITAllocator::constant(allocator), scratchGPR2, scratchGPR3, slowPath);
    m_jit.addPtr(JITCompiler::TrustedImm32(size + sizeof(IndexingHeader)), scratchGPR1);

    addSlowPathGenerator(
        slowPathCall(slowPath, this, operationAllocateSimplePropertyStorageWithInitialCapacity, scratchGPR1));

    for (ptrdiff_t offset = 0; offset < static_cast<ptrdiff_t>(size); offset += sizeof(void*))
        m_jit.storePtr(TrustedImmPtr(nullptr), JITCompiler::Address(scratchGPR1, -(offset + sizeof(JSValue) + sizeof(void*))));

    storageResult(scratchGPR1, node);
}

void SpeculativeJIT::compileReallocatePropertyStorage(Node* node)
{
    size_t oldSize = node->transition()->previous->outOfLineCapacity() * sizeof(JSValue);
    size_t newSize = oldSize * outOfLineGrowthFactor;
    ASSERT(newSize == node->transition()->next->outOfLineCapacity() * sizeof(JSValue));
    
    Allocator allocator = m_jit.vm()->jsValueGigacageAuxiliarySpace.allocatorForNonVirtual(newSize, AllocatorForMode::AllocatorIfExists);

    if (!allocator || node->transition()->previous->couldHaveIndexingHeader()) {
        SpeculateCellOperand base(this, node->child1());
        
        GPRReg baseGPR = base.gpr();
        
        flushRegisters();

        GPRFlushedCallResult result(this);
        callOperation(operationAllocateComplexPropertyStorage, result.gpr(), baseGPR, newSize / sizeof(JSValue));
        m_jit.exceptionCheck();

        storageResult(result.gpr(), node);
        return;
    }
    
    StorageOperand oldStorage(this, node->child2());
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);

    GPRReg oldStorageGPR = oldStorage.gpr();
    GPRReg scratchGPR1 = scratch1.gpr();
    GPRReg scratchGPR2 = scratch2.gpr();
    GPRReg scratchGPR3 = scratch3.gpr();
    
    JITCompiler::JumpList slowPath;
    m_jit.emitAllocate(scratchGPR1, JITAllocator::constant(allocator), scratchGPR2, scratchGPR3, slowPath);
    
    m_jit.addPtr(JITCompiler::TrustedImm32(newSize + sizeof(IndexingHeader)), scratchGPR1);

    addSlowPathGenerator(
        slowPathCall(slowPath, this, operationAllocateSimplePropertyStorage, scratchGPR1, newSize / sizeof(JSValue)));

    for (ptrdiff_t offset = oldSize; offset < static_cast<ptrdiff_t>(newSize); offset += sizeof(void*))
        m_jit.storePtr(TrustedImmPtr(nullptr), JITCompiler::Address(scratchGPR1, -(offset + sizeof(JSValue) + sizeof(void*))));

    // We have scratchGPR1 = new storage, scratchGPR2 = scratch
    for (ptrdiff_t offset = 0; offset < static_cast<ptrdiff_t>(oldSize); offset += sizeof(void*)) {
        m_jit.loadPtr(JITCompiler::Address(oldStorageGPR, -(offset + sizeof(JSValue) + sizeof(void*))), scratchGPR2);
        m_jit.storePtr(scratchGPR2, JITCompiler::Address(scratchGPR1, -(offset + sizeof(JSValue) + sizeof(void*))));
    }

    storageResult(scratchGPR1, node);
}

void SpeculativeJIT::compileNukeStructureAndSetButterfly(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    StorageOperand storage(this, node->child2());

    GPRReg baseGPR = base.gpr();
    GPRReg storageGPR = storage.gpr();

    m_jit.nukeStructureAndStoreButterfly(*m_jit.vm(), storageGPR, baseGPR);
    
    noResult(node);
}

void SpeculativeJIT::compileGetButterfly(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary result(this, Reuse, base);
    
    GPRReg baseGPR = base.gpr();
    GPRReg resultGPR = result.gpr();
    
    m_jit.loadPtr(JITCompiler::Address(baseGPR, JSObject::butterflyOffset()), resultGPR);

    storageResult(resultGPR, node);
}

static void allocateTemporaryRegistersForSnippet(SpeculativeJIT* jit, Vector<GPRTemporary>& gpHolders, Vector<FPRTemporary>& fpHolders, Vector<GPRReg>& gpScratch, Vector<FPRReg>& fpScratch, Snippet& snippet)
{
    for (unsigned i = 0; i < snippet.numGPScratchRegisters; ++i) {
        GPRTemporary temporary(jit);
        gpScratch.append(temporary.gpr());
        gpHolders.append(WTFMove(temporary));
    }

    for (unsigned i = 0; i < snippet.numFPScratchRegisters; ++i) {
        FPRTemporary temporary(jit);
        fpScratch.append(temporary.fpr());
        fpHolders.append(WTFMove(temporary));
    }
}

void SpeculativeJIT::compileCallDOM(Node* node)
{
    const DOMJIT::Signature* signature = node->signature();

    // FIXME: We should have a way to call functions with the vector of registers.
    // https://bugs.webkit.org/show_bug.cgi?id=163099
    Vector<Variant<SpeculateCellOperand, SpeculateInt32Operand, SpeculateBooleanOperand>, JSC_DOMJIT_SIGNATURE_MAX_ARGUMENTS_INCLUDING_THIS> operands;
    Vector<GPRReg, JSC_DOMJIT_SIGNATURE_MAX_ARGUMENTS_INCLUDING_THIS> regs;

    auto appendCell = [&](Edge& edge) {
        SpeculateCellOperand operand(this, edge);
        regs.append(operand.gpr());
        operands.append(WTFMove(operand));
    };

    auto appendString = [&](Edge& edge) {
        SpeculateCellOperand operand(this, edge);
        GPRReg gpr = operand.gpr();
        regs.append(gpr);
        speculateString(edge, gpr);
        operands.append(WTFMove(operand));
    };

    auto appendInt32 = [&](Edge& edge) {
        SpeculateInt32Operand operand(this, edge);
        regs.append(operand.gpr());
        operands.append(WTFMove(operand));
    };

    auto appendBoolean = [&](Edge& edge) {
        SpeculateBooleanOperand operand(this, edge);
        regs.append(operand.gpr());
        operands.append(WTFMove(operand));
    };

    unsigned index = 0;
    m_jit.graph().doToChildren(node, [&](Edge edge) {
        if (!index)
            appendCell(edge);
        else {
            switch (signature->arguments[index - 1]) {
            case SpecString:
                appendString(edge);
                break;
            case SpecInt32Only:
                appendInt32(edge);
                break;
            case SpecBoolean:
                appendBoolean(edge);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
        ++index;
    });

    JSValueRegsTemporary result(this);
    JSValueRegs resultRegs = result.regs();

    flushRegisters();
    assertIsTaggedWith(reinterpret_cast<void*>(signature->unsafeFunction), CFunctionPtrTag);
    unsigned argumentCountIncludingThis = signature->argumentCount + 1;
    switch (argumentCountIncludingThis) {
    case 1:
        callOperation(reinterpret_cast<J_JITOperation_EP>(signature->unsafeFunction), extractResult(resultRegs), regs[0]);
        break;
    case 2:
        callOperation(reinterpret_cast<J_JITOperation_EPP>(signature->unsafeFunction), extractResult(resultRegs), regs[0], regs[1]);
        break;
    case 3:
        callOperation(reinterpret_cast<J_JITOperation_EPPP>(signature->unsafeFunction), extractResult(resultRegs), regs[0], regs[1], regs[2]);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    m_jit.exceptionCheck();
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileCallDOMGetter(Node* node)
{
    DOMJIT::CallDOMGetterSnippet* snippet = node->callDOMGetterData()->snippet;
    if (!snippet) {
        FunctionPtr<OperationPtrTag> getter = node->callDOMGetterData()->customAccessorGetter;
        SpeculateCellOperand base(this, node->child1());
        JSValueRegsTemporary result(this);

        JSValueRegs resultRegs = result.regs();
        GPRReg baseGPR = base.gpr();

        flushRegisters();
        m_jit.setupArguments<J_JITOperation_EJI>(CCallHelpers::CellValue(baseGPR), identifierUID(node->callDOMGetterData()->identifierNumber));
        m_jit.storePtr(GPRInfo::callFrameRegister, &m_jit.vm()->topCallFrame);
        m_jit.emitStoreCodeOrigin(m_currentNode->origin.semantic);
        m_jit.appendCall(getter.retagged<CFunctionPtrTag>());
        m_jit.setupResults(resultRegs);

        m_jit.exceptionCheck();
        jsValueResult(resultRegs, node);
        return;
    }

    Vector<GPRReg> gpScratch;
    Vector<FPRReg> fpScratch;
    Vector<SnippetParams::Value> regs;

    JSValueRegsTemporary result(this);
    regs.append(result.regs());

    Edge& baseEdge = node->child1();
    SpeculateCellOperand base(this, baseEdge);
    regs.append(SnippetParams::Value(base.gpr(), m_state.forNode(baseEdge).value()));

    Optional<SpeculateCellOperand> globalObject;
    if (snippet->requireGlobalObject) {
        Edge& globalObjectEdge = node->child2();
        globalObject.emplace(this, globalObjectEdge);
        regs.append(SnippetParams::Value(globalObject->gpr(), m_state.forNode(globalObjectEdge).value()));
    }

    Vector<GPRTemporary> gpTempraries;
    Vector<FPRTemporary> fpTempraries;
    allocateTemporaryRegistersForSnippet(this, gpTempraries, fpTempraries, gpScratch, fpScratch, *snippet);
    SnippetParams params(this, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
    snippet->generator()->run(m_jit, params);
    jsValueResult(result.regs(), node);
}

void SpeculativeJIT::compileCheckSubClass(Node* node)
{
    const ClassInfo* classInfo = node->classInfo();
    if (!classInfo->checkSubClassSnippet) {
        SpeculateCellOperand base(this, node->child1());
        GPRTemporary other(this);
        GPRTemporary specified(this);

        GPRReg baseGPR = base.gpr();
        GPRReg otherGPR = other.gpr();
        GPRReg specifiedGPR = specified.gpr();

        m_jit.emitLoadStructure(*m_jit.vm(), baseGPR, otherGPR, specifiedGPR);
        m_jit.loadPtr(CCallHelpers::Address(otherGPR, Structure::classInfoOffset()), otherGPR);
        m_jit.move(CCallHelpers::TrustedImmPtr(node->classInfo()), specifiedGPR);

        CCallHelpers::Label loop = m_jit.label();
        auto done = m_jit.branchPtr(CCallHelpers::Equal, otherGPR, specifiedGPR);
        m_jit.loadPtr(CCallHelpers::Address(otherGPR, ClassInfo::offsetOfParentClass()), otherGPR);
        m_jit.branchTestPtr(CCallHelpers::NonZero, otherGPR).linkTo(loop, &m_jit);
        speculationCheck(BadType, JSValueSource::unboxedCell(baseGPR), node->child1(), m_jit.jump());
        done.link(&m_jit);
        noResult(node);
        return;
    }

    Ref<Snippet> snippet = classInfo->checkSubClassSnippet();

    Vector<GPRReg> gpScratch;
    Vector<FPRReg> fpScratch;
    Vector<SnippetParams::Value> regs;

    SpeculateCellOperand base(this, node->child1());
    GPRReg baseGPR = base.gpr();
    regs.append(SnippetParams::Value(baseGPR, m_state.forNode(node->child1()).value()));

    Vector<GPRTemporary> gpTempraries;
    Vector<FPRTemporary> fpTempraries;
    allocateTemporaryRegistersForSnippet(this, gpTempraries, fpTempraries, gpScratch, fpScratch, snippet.get());

    SnippetParams params(this, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
    CCallHelpers::JumpList failureCases = snippet->generator()->run(m_jit, params);
    speculationCheck(BadType, JSValueSource::unboxedCell(baseGPR), node->child1(), failureCases);
    noResult(node);
}

GPRReg SpeculativeJIT::temporaryRegisterForPutByVal(GPRTemporary& temporary, ArrayMode arrayMode)
{
    if (!putByValWillNeedExtraRegister(arrayMode))
        return InvalidGPRReg;
    
    GPRTemporary realTemporary(this);
    temporary.adopt(realTemporary);
    return temporary.gpr();
}

void SpeculativeJIT::compileToStringOrCallStringConstructorOrStringValueOf(Node* node)
{
    ASSERT(node->op() != StringValueOf || node->child1().useKind() == UntypedUse);
    switch (node->child1().useKind()) {
    case NotCellUse: {
        JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
        JSValueRegs op1Regs = op1.jsValueRegs();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        speculateNotCell(node->child1(), op1Regs);

        flushRegisters();

        if (node->op() == ToString)
            callOperation(operationToString, resultGPR, op1Regs);
        else {
            ASSERT(node->op() == CallStringConstructor);
            callOperation(operationCallStringConstructor, resultGPR, op1Regs);
        }
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
        return;
    }

    case UntypedUse: {
        JSValueOperand op1(this, node->child1());
        JSValueRegs op1Regs = op1.jsValueRegs();
        GPRReg op1PayloadGPR = op1Regs.payloadGPR();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        flushRegisters();

        JITCompiler::Jump done;
        if (node->child1()->prediction() & SpecString) {
            JITCompiler::Jump slowPath1 = m_jit.branchIfNotCell(op1.jsValueRegs());
            JITCompiler::Jump slowPath2 = m_jit.branchIfNotString(op1PayloadGPR);
            m_jit.move(op1PayloadGPR, resultGPR);
            done = m_jit.jump();
            slowPath1.link(&m_jit);
            slowPath2.link(&m_jit);
        }
        if (node->op() == ToString)
            callOperation(operationToString, resultGPR, op1Regs);
        else if (node->op() == StringValueOf)
            callOperation(operationStringValueOf, resultGPR, op1Regs);
        else {
            ASSERT(node->op() == CallStringConstructor);
            callOperation(operationCallStringConstructor, resultGPR, op1Regs);
        }
        m_jit.exceptionCheck();
        if (done.isSet())
            done.link(&m_jit);
        cellResult(resultGPR, node);
        return;
    }

    case Int32Use:
    case Int52RepUse:
    case DoubleRepUse:
        compileNumberToStringWithValidRadixConstant(node, 10);
        return;

    default:
        break;
    }

    SpeculateCellOperand op1(this, node->child1());
    GPRReg op1GPR = op1.gpr();
    
    switch (node->child1().useKind()) {
    case StringObjectUse: {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();
        
        speculateStringObject(node->child1(), op1GPR);

        m_jit.loadPtr(JITCompiler::Address(op1GPR, JSWrapperObject::internalValueCellOffset()), resultGPR);
        cellResult(resultGPR, node);
        break;
    }
        
    case StringOrStringObjectUse: {
        GPRTemporary result(this);
        GPRReg resultGPR = result.gpr();

        m_jit.load8(JITCompiler::Address(op1GPR, JSCell::typeInfoTypeOffset()), resultGPR);
        JITCompiler::Jump isString = m_jit.branch32(JITCompiler::Equal, resultGPR, TrustedImm32(StringType));

        speculationCheck(BadType, JSValueSource::unboxedCell(op1GPR), node->child1().node(), m_jit.branch32(JITCompiler::NotEqual, resultGPR, TrustedImm32(StringObjectType)));
        m_jit.loadPtr(JITCompiler::Address(op1GPR, JSWrapperObject::internalValueCellOffset()), resultGPR);
        JITCompiler::Jump done = m_jit.jump();

        isString.link(&m_jit);
        m_jit.move(op1GPR, resultGPR);
        done.link(&m_jit);
        
        m_interpreter.filter(node->child1(), SpecString | SpecStringObject);
        
        cellResult(resultGPR, node);
        break;
    }
        
    case CellUse: {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        
        // We flush registers instead of silent spill/fill because in this mode we
        // believe that most likely the input is not a string, and we need to take
        // slow path.
        flushRegisters();
        JITCompiler::Jump done;
        if (node->child1()->prediction() & SpecString) {
            JITCompiler::Jump needCall = m_jit.branchIfNotString(op1GPR);
            m_jit.move(op1GPR, resultGPR);
            done = m_jit.jump();
            needCall.link(&m_jit);
        }
        if (node->op() == ToString)
            callOperation(operationToStringOnCell, resultGPR, op1GPR);
        else {
            ASSERT(node->op() == CallStringConstructor);
            callOperation(operationCallStringConstructorOnCell, resultGPR, op1GPR);
        }
        m_jit.exceptionCheck();
        if (done.isSet())
            done.link(&m_jit);
        cellResult(resultGPR, node);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileNumberToStringWithValidRadixConstant(Node* node)
{
    compileNumberToStringWithValidRadixConstant(node, node->validRadixConstant());
}

void SpeculativeJIT::compileNumberToStringWithValidRadixConstant(Node* node, int32_t radix)
{
    auto callToString = [&] (auto operation, GPRReg resultGPR, auto valueReg) {
        flushRegisters();
        callOperation(operation, resultGPR, valueReg, TrustedImm32(radix));
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
    };

    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateStrictInt32Operand value(this, node->child1());
        GPRFlushedCallResult result(this);
        callToString(operationInt32ToStringWithValidRadix, result.gpr(), value.gpr());
        break;
    }

#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand value(this, node->child1());
        GPRFlushedCallResult result(this);
        callToString(operationInt52ToStringWithValidRadix, result.gpr(), value.gpr());
        break;
    }
#endif

    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        GPRFlushedCallResult result(this);
        callToString(operationDoubleToStringWithValidRadix, result.gpr(), value.fpr());
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileNumberToStringWithRadix(Node* node)
{
    bool validRadixIsGuaranteed = false;
    if (node->child2()->isInt32Constant()) {
        int32_t radix = node->child2()->asInt32();
        if (radix >= 2 && radix <= 36)
            validRadixIsGuaranteed = true;
    }

    auto callToString = [&] (auto operation, GPRReg resultGPR, auto valueReg, GPRReg radixGPR) {
        flushRegisters();
        callOperation(operation, resultGPR, valueReg, radixGPR);
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
    };

    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateStrictInt32Operand value(this, node->child1());
        SpeculateStrictInt32Operand radix(this, node->child2());
        GPRFlushedCallResult result(this);
        callToString(validRadixIsGuaranteed ? operationInt32ToStringWithValidRadix : operationInt32ToString, result.gpr(), value.gpr(), radix.gpr());
        break;
    }

#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateStrictInt52Operand value(this, node->child1());
        SpeculateStrictInt32Operand radix(this, node->child2());
        GPRFlushedCallResult result(this);
        callToString(validRadixIsGuaranteed ? operationInt52ToStringWithValidRadix : operationInt52ToString, result.gpr(), value.gpr(), radix.gpr());
        break;
    }
#endif

    case DoubleRepUse: {
        SpeculateDoubleOperand value(this, node->child1());
        SpeculateStrictInt32Operand radix(this, node->child2());
        GPRFlushedCallResult result(this);
        callToString(validRadixIsGuaranteed ? operationDoubleToStringWithValidRadix : operationDoubleToString, result.gpr(), value.fpr(), radix.gpr());
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void SpeculativeJIT::compileNewStringObject(Node* node)
{
    SpeculateCellOperand operand(this, node->child1());
    
    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);

    GPRReg operandGPR = operand.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    
    JITCompiler::JumpList slowPath;

    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject<StringObject>(
        resultGPR, TrustedImmPtr(node->structure()), butterfly, scratch1GPR, scratch2GPR,
        slowPath);
    
    m_jit.storePtr(
        TrustedImmPtr(StringObject::info()),
        JITCompiler::Address(resultGPR, JSDestructibleObject::classInfoOffset()));
#if USE(JSVALUE64)
    m_jit.store64(
        operandGPR, JITCompiler::Address(resultGPR, JSWrapperObject::internalValueOffset()));
#else
    m_jit.store32(
        TrustedImm32(JSValue::CellTag),
        JITCompiler::Address(resultGPR, JSWrapperObject::internalValueOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
    m_jit.store32(
        operandGPR,
        JITCompiler::Address(resultGPR, JSWrapperObject::internalValueOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
#endif
    
    m_jit.mutatorFence(*m_jit.vm());
    
    addSlowPathGenerator(slowPathCall(
        slowPath, this, operationNewStringObject, resultGPR, operandGPR, node->structure()));
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewSymbol(Node* node)
{
    if (!node->child1()) {
        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationNewSymbol, resultGPR);
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
        return;
    }


    ASSERT(node->child1().useKind() == KnownStringUse);
    SpeculateCellOperand operand(this, node->child1());

    GPRReg stringGPR = operand.gpr();

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationNewSymbolWithDescription, resultGPR, stringGPR);
    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewTypedArrayWithSize(Node* node)
{
    JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
    auto typedArrayType = node->typedArrayType();
    RegisteredStructure structure = m_jit.graph().registerStructure(globalObject->typedArrayStructureConcurrently(typedArrayType));
    RELEASE_ASSERT(structure.get());
    
    SpeculateInt32Operand size(this, node->child1());
    GPRReg sizeGPR = size.gpr();
    
    GPRTemporary result(this);
    GPRTemporary storage(this);
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);
    GPRReg resultGPR = result.gpr();
    GPRReg storageGPR = storage.gpr();
    GPRReg scratchGPR = scratch.gpr();
    GPRReg scratchGPR2 = scratch2.gpr();
    
    JITCompiler::JumpList slowCases;
    
    m_jit.move(TrustedImmPtr(nullptr), storageGPR);

    slowCases.append(m_jit.branch32(
        MacroAssembler::Above, sizeGPR, TrustedImm32(JSArrayBufferView::fastSizeLimit)));
    
    m_jit.move(sizeGPR, scratchGPR);
    m_jit.lshift32(TrustedImm32(logElementSize(typedArrayType)), scratchGPR);
    if (elementSize(typedArrayType) < 8) {
        m_jit.add32(TrustedImm32(7), scratchGPR);
        m_jit.and32(TrustedImm32(~7), scratchGPR);
    }
    m_jit.emitAllocateVariableSized(
        storageGPR, m_jit.vm()->primitiveGigacageAuxiliarySpace, scratchGPR, scratchGPR,
        scratchGPR2, slowCases);
    
    MacroAssembler::Jump done = m_jit.branchTest32(MacroAssembler::Zero, sizeGPR);
    m_jit.move(sizeGPR, scratchGPR);
    if (elementSize(typedArrayType) != 4) {
        if (elementSize(typedArrayType) > 4)
            m_jit.lshift32(TrustedImm32(logElementSize(typedArrayType) - 2), scratchGPR);
        else {
            if (elementSize(typedArrayType) > 1)
                m_jit.lshift32(TrustedImm32(logElementSize(typedArrayType)), scratchGPR);
            m_jit.add32(TrustedImm32(3), scratchGPR);
            m_jit.urshift32(TrustedImm32(2), scratchGPR);
        }
    }
    MacroAssembler::Label loop = m_jit.label();
    m_jit.sub32(TrustedImm32(1), scratchGPR);
    m_jit.store32(
        TrustedImm32(0),
        MacroAssembler::BaseIndex(storageGPR, scratchGPR, MacroAssembler::TimesFour));
    m_jit.branchTest32(MacroAssembler::NonZero, scratchGPR).linkTo(loop, &m_jit);
    done.link(&m_jit);

    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject<JSArrayBufferView>(
        resultGPR, TrustedImmPtr(structure), butterfly, scratchGPR, scratchGPR2,
        slowCases);

    m_jit.storePtr(
        storageGPR,
        MacroAssembler::Address(resultGPR, JSArrayBufferView::offsetOfVector()));
    m_jit.store32(
        sizeGPR,
        MacroAssembler::Address(resultGPR, JSArrayBufferView::offsetOfLength()));
    m_jit.store32(
        TrustedImm32(FastTypedArray),
        MacroAssembler::Address(resultGPR, JSArrayBufferView::offsetOfMode()));
    
    m_jit.mutatorFence(*m_jit.vm());
    
    addSlowPathGenerator(slowPathCall(
        slowCases, this, operationNewTypedArrayWithSizeForType(typedArrayType),
        resultGPR, structure, sizeGPR, storageGPR));
    
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewRegexp(Node* node)
{
    RegExp* regexp = node->castOperand<RegExp*>();
    ASSERT(regexp->isValid());

    GPRTemporary result(this);
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    JSValueOperand lastIndex(this, node->child1());

    GPRReg resultGPR = result.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    JSValueRegs lastIndexRegs = lastIndex.jsValueRegs();

    JITCompiler::JumpList slowPath;

    auto structure = m_jit.graph().registerStructure(m_jit.graph().globalObjectFor(node->origin.semantic)->regExpStructure());
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject<RegExpObject>(resultGPR, TrustedImmPtr(structure), butterfly, scratch1GPR, scratch2GPR, slowPath);

    m_jit.storePtr(
        TrustedImmPtr(node->cellOperand()),
        CCallHelpers::Address(resultGPR, RegExpObject::offsetOfRegExp()));
    m_jit.storeValue(lastIndexRegs, CCallHelpers::Address(resultGPR, RegExpObject::offsetOfLastIndex()));
    m_jit.store8(TrustedImm32(true), CCallHelpers::Address(resultGPR, RegExpObject::offsetOfLastIndexIsWritable()));
    m_jit.mutatorFence(*m_jit.vm());

    addSlowPathGenerator(slowPathCall(slowPath, this, operationNewRegexpWithLastIndex, resultGPR, regexp, lastIndexRegs));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::speculateCellTypeWithoutTypeFiltering(
    Edge edge, GPRReg cellGPR, JSType jsType)
{
    speculationCheck(
        BadType, JSValueSource::unboxedCell(cellGPR), edge,
        m_jit.branchIfNotType(cellGPR, jsType));
}

void SpeculativeJIT::speculateCellType(
    Edge edge, GPRReg cellGPR, SpeculatedType specType, JSType jsType)
{
    DFG_TYPE_CHECK(
        JSValueSource::unboxedCell(cellGPR), edge, specType,
        m_jit.branchIfNotType(cellGPR, jsType));
}

void SpeculativeJIT::speculateInt32(Edge edge)
{
    if (!needsTypeCheck(edge, SpecInt32Only))
        return;
    
    (SpeculateInt32Operand(this, edge)).gpr();
}

void SpeculativeJIT::speculateNumber(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBytecodeNumber))
        return;
    
    JSValueOperand value(this, edge, ManualOperandSpeculation);
#if USE(JSVALUE64)
    GPRReg gpr = value.gpr();
    typeCheck(
        JSValueRegs(gpr), edge, SpecBytecodeNumber,
        m_jit.branchIfNotNumber(gpr));
#else
    IGNORE_WARNINGS_BEGIN("enum-compare")
    static_assert(JSValue::Int32Tag >= JSValue::LowestTag, "Int32Tag is included in >= JSValue::LowestTag range.");
    IGNORE_WARNINGS_END
    GPRReg tagGPR = value.tagGPR();
    DFG_TYPE_CHECK(
        value.jsValueRegs(), edge, ~SpecInt32Only,
        m_jit.branchIfInt32(tagGPR));
    DFG_TYPE_CHECK(
        value.jsValueRegs(), edge, SpecBytecodeNumber,
        m_jit.branch32(MacroAssembler::AboveOrEqual, tagGPR, TrustedImm32(JSValue::LowestTag)));
#endif
}

void SpeculativeJIT::speculateRealNumber(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBytecodeRealNumber))
        return;
    
    JSValueOperand op1(this, edge, ManualOperandSpeculation);
    FPRTemporary result(this);
    
    JSValueRegs op1Regs = op1.jsValueRegs();
    FPRReg resultFPR = result.fpr();
    
#if USE(JSVALUE64)
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    m_jit.unboxDoubleWithoutAssertions(op1Regs.gpr(), tempGPR, resultFPR);
#else
    FPRTemporary temp(this);
    FPRReg tempFPR = temp.fpr();
    unboxDouble(op1Regs.tagGPR(), op1Regs.payloadGPR(), resultFPR, tempFPR);
#endif
    
    JITCompiler::Jump done = m_jit.branchIfNotNaN(resultFPR);

    typeCheck(op1Regs, edge, SpecBytecodeRealNumber, m_jit.branchIfNotInt32(op1Regs));
    
    done.link(&m_jit);
}

void SpeculativeJIT::speculateDoubleRepReal(Edge edge)
{
    if (!needsTypeCheck(edge, SpecDoubleReal))
        return;
    
    SpeculateDoubleOperand operand(this, edge);
    FPRReg fpr = operand.fpr();
    typeCheck(
        JSValueRegs(), edge, SpecDoubleReal,
        m_jit.branchIfNaN(fpr));
}

void SpeculativeJIT::speculateBoolean(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBoolean))
        return;
    
    (SpeculateBooleanOperand(this, edge)).gpr();
}

void SpeculativeJIT::speculateCell(Edge edge)
{
    if (!needsTypeCheck(edge, SpecCellCheck))
        return;
    
    (SpeculateCellOperand(this, edge)).gpr();
}

void SpeculativeJIT::speculateCellOrOther(Edge edge)
{
    if (!needsTypeCheck(edge, SpecCellCheck | SpecOther))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();

    MacroAssembler::Jump ok = m_jit.branchIfCell(operand.jsValueRegs());
    DFG_TYPE_CHECK(
        operand.jsValueRegs(), edge, SpecCellCheck | SpecOther,
        m_jit.branchIfNotOther(operand.jsValueRegs(), tempGPR));
    ok.link(&m_jit);
}

void SpeculativeJIT::speculateObject(Edge edge, GPRReg cell)
{
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(cell), edge, SpecObject, m_jit.branchIfNotObject(cell));
}

void SpeculativeJIT::speculateObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateFunction(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecFunction, JSFunctionType);
}

void SpeculativeJIT::speculateFunction(Edge edge)
{
    if (!needsTypeCheck(edge, SpecFunction))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateFunction(edge, operand.gpr());
}

void SpeculativeJIT::speculateFinalObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecFinalObject, FinalObjectType);
}

void SpeculativeJIT::speculateFinalObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecFinalObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateFinalObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateRegExpObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecRegExpObject, RegExpObjectType);
}

void SpeculativeJIT::speculateRegExpObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecRegExpObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateRegExpObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateArray(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecArray, ArrayType);
}

void SpeculativeJIT::speculateArray(Edge edge)
{
    if (!needsTypeCheck(edge, SpecArray))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateArray(edge, operand.gpr());
}

void SpeculativeJIT::speculateProxyObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecProxyObject, ProxyObjectType);
}

void SpeculativeJIT::speculateProxyObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecProxyObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateProxyObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateDerivedArray(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecDerivedArray, DerivedArrayType);
}

void SpeculativeJIT::speculateDerivedArray(Edge edge)
{
    if (!needsTypeCheck(edge, SpecDerivedArray))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateDerivedArray(edge, operand.gpr());
}

void SpeculativeJIT::speculateMapObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecMapObject, JSMapType);
}

void SpeculativeJIT::speculateMapObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecMapObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateMapObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateSetObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecSetObject, JSSetType);
}

void SpeculativeJIT::speculateSetObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecSetObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateSetObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateWeakMapObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecWeakMapObject, JSWeakMapType);
}

void SpeculativeJIT::speculateWeakMapObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecWeakMapObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateWeakMapObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateWeakSetObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecWeakSetObject, JSWeakSetType);
}

void SpeculativeJIT::speculateWeakSetObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecWeakSetObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateWeakSetObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateDataViewObject(Edge edge, GPRReg cell)
{
    speculateCellType(edge, cell, SpecDataViewObject, DataViewType);
}

void SpeculativeJIT::speculateDataViewObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecDataViewObject))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateDataViewObject(edge, operand.gpr());
}

void SpeculativeJIT::speculateObjectOrOther(Edge edge)
{
    if (!needsTypeCheck(edge, SpecObject | SpecOther))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    MacroAssembler::Jump notCell = m_jit.branchIfNotCell(operand.jsValueRegs());
    GPRReg gpr = operand.jsValueRegs().payloadGPR();
    DFG_TYPE_CHECK(
        operand.jsValueRegs(), edge, (~SpecCellCheck) | SpecObject, m_jit.branchIfNotObject(gpr));
    MacroAssembler::Jump done = m_jit.jump();
    notCell.link(&m_jit);
    DFG_TYPE_CHECK(
        operand.jsValueRegs(), edge, SpecCellCheck | SpecOther,
        m_jit.branchIfNotOther(operand.jsValueRegs(), tempGPR));
    done.link(&m_jit);
}

void SpeculativeJIT::speculateString(Edge edge, GPRReg cell)
{
    DFG_TYPE_CHECK(
        JSValueSource::unboxedCell(cell), edge, SpecString | ~SpecCellCheck, m_jit.branchIfNotString(cell));
}

void SpeculativeJIT::speculateStringOrOther(Edge edge, JSValueRegs regs, GPRReg scratch)
{
    JITCompiler::Jump notCell = m_jit.branchIfNotCell(regs);
    GPRReg cell = regs.payloadGPR();
    DFG_TYPE_CHECK(regs, edge, (~SpecCellCheck) | SpecString, m_jit.branchIfNotString(cell));
    JITCompiler::Jump done = m_jit.jump();
    notCell.link(&m_jit);
    DFG_TYPE_CHECK(regs, edge, SpecCellCheck | SpecOther, m_jit.branchIfNotOther(regs, scratch));
    done.link(&m_jit);
}

void SpeculativeJIT::speculateStringOrOther(Edge edge)
{
    if (!needsTypeCheck(edge, SpecString | SpecOther))
        return;

    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    JSValueRegs regs = operand.jsValueRegs();
    GPRReg tempGPR = temp.gpr();
    speculateStringOrOther(edge, regs, tempGPR);
}

void SpeculativeJIT::speculateStringIdentAndLoadStorage(Edge edge, GPRReg string, GPRReg storage)
{
    m_jit.loadPtr(MacroAssembler::Address(string, JSString::offsetOfValue()), storage);
    
    if (!needsTypeCheck(edge, SpecStringIdent | ~SpecString))
        return;

    speculationCheck(
        BadType, JSValueSource::unboxedCell(string), edge,
        m_jit.branchIfRopeStringImpl(storage));
    speculationCheck(
        BadType, JSValueSource::unboxedCell(string), edge, m_jit.branchTest32(
            MacroAssembler::Zero,
            MacroAssembler::Address(storage, StringImpl::flagsOffset()),
            MacroAssembler::TrustedImm32(StringImpl::flagIsAtomic())));
    
    m_interpreter.filter(edge, SpecStringIdent | ~SpecString);
}

void SpeculativeJIT::speculateStringIdent(Edge edge, GPRReg string)
{
    if (!needsTypeCheck(edge, SpecStringIdent))
        return;

    GPRTemporary temp(this);
    speculateStringIdentAndLoadStorage(edge, string, temp.gpr());
}

void SpeculativeJIT::speculateStringIdent(Edge edge)
{
    if (!needsTypeCheck(edge, SpecStringIdent))
        return;
    
    SpeculateCellOperand operand(this, edge);
    GPRReg gpr = operand.gpr();
    speculateString(edge, gpr);
    speculateStringIdent(edge, gpr);
}

void SpeculativeJIT::speculateString(Edge edge)
{
    if (!needsTypeCheck(edge, SpecString))
        return;
    
    SpeculateCellOperand operand(this, edge);
    speculateString(edge, operand.gpr());
}

void SpeculativeJIT::speculateStringObject(Edge edge, GPRReg cellGPR)
{
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(cellGPR), edge, ~SpecCellCheck | SpecStringObject, m_jit.branchIfNotType(cellGPR, StringObjectType));
}

void SpeculativeJIT::speculateStringObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecStringObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    GPRReg gpr = operand.gpr();
    speculateStringObject(edge, gpr);
}

void SpeculativeJIT::speculateStringOrStringObject(Edge edge)
{
    if (!needsTypeCheck(edge, SpecString | SpecStringObject))
        return;
    
    SpeculateCellOperand operand(this, edge);
    GPRReg gpr = operand.gpr();
    if (!needsTypeCheck(edge, SpecString | SpecStringObject))
        return;

    GPRTemporary typeTemp(this);
    GPRReg typeGPR = typeTemp.gpr();

    m_jit.load8(JITCompiler::Address(gpr, JSCell::typeInfoTypeOffset()), typeGPR);

    JITCompiler::Jump isString = m_jit.branch32(JITCompiler::Equal, typeGPR, TrustedImm32(StringType));
    speculationCheck(BadType, JSValueSource::unboxedCell(gpr), edge.node(), m_jit.branch32(JITCompiler::NotEqual, typeGPR, TrustedImm32(StringObjectType)));
    isString.link(&m_jit);
    
    m_interpreter.filter(edge, SpecString | SpecStringObject);
}

void SpeculativeJIT::speculateNotStringVar(Edge edge)
{
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    
    JITCompiler::Jump notCell = m_jit.branchIfNotCell(operand.jsValueRegs());
    GPRReg cell = operand.jsValueRegs().payloadGPR();
    
    JITCompiler::Jump notString = m_jit.branchIfNotString(cell);
    
    speculateStringIdentAndLoadStorage(edge, cell, tempGPR);
    
    notString.link(&m_jit);
    notCell.link(&m_jit);
}

void SpeculativeJIT::speculateNotSymbol(Edge edge)
{
    if (!needsTypeCheck(edge, ~SpecSymbol))
        return;

    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    auto valueRegs = operand.jsValueRegs();
    GPRReg value = valueRegs.payloadGPR();
    JITCompiler::Jump notCell;

    bool needsCellCheck = needsTypeCheck(edge, SpecCell);
    if (needsCellCheck)
        notCell = m_jit.branchIfNotCell(valueRegs);

    speculationCheck(BadType, JSValueSource::unboxedCell(value), edge.node(), m_jit.branchIfSymbol(value));

    if (needsCellCheck)
        notCell.link(&m_jit);

    m_interpreter.filter(edge, ~SpecSymbol);
}

void SpeculativeJIT::speculateSymbol(Edge edge, GPRReg cell)
{
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(cell), edge, ~SpecCellCheck | SpecSymbol, m_jit.branchIfNotSymbol(cell));
}

void SpeculativeJIT::speculateSymbol(Edge edge)
{
    if (!needsTypeCheck(edge, SpecSymbol))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateSymbol(edge, operand.gpr());
}

void SpeculativeJIT::speculateBigInt(Edge edge, GPRReg cell)
{
    DFG_TYPE_CHECK(JSValueSource::unboxedCell(cell), edge, ~SpecCellCheck | SpecBigInt, m_jit.branchIfNotBigInt(cell));
}

void SpeculativeJIT::speculateBigInt(Edge edge)
{
    if (!needsTypeCheck(edge, SpecBigInt))
        return;

    SpeculateCellOperand operand(this, edge);
    speculateBigInt(edge, operand.gpr());
}

void SpeculativeJIT::speculateNotCell(Edge edge, JSValueRegs regs)
{
    DFG_TYPE_CHECK(regs, edge, ~SpecCellCheck, m_jit.branchIfCell(regs));
}

void SpeculativeJIT::speculateNotCell(Edge edge)
{
    if (!needsTypeCheck(edge, ~SpecCellCheck))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation); 
    speculateNotCell(edge, operand.jsValueRegs());
}

void SpeculativeJIT::speculateOther(Edge edge, JSValueRegs regs, GPRReg tempGPR)
{
    DFG_TYPE_CHECK(regs, edge, SpecOther, m_jit.branchIfNotOther(regs, tempGPR));
}

void SpeculativeJIT::speculateOther(Edge edge, JSValueRegs regs)
{
    if (!needsTypeCheck(edge, SpecOther))
        return;

    GPRTemporary temp(this);
    GPRReg tempGPR = temp.gpr();
    speculateOther(edge, regs, tempGPR);
}

void SpeculativeJIT::speculateOther(Edge edge)
{
    if (!needsTypeCheck(edge, SpecOther))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    speculateOther(edge, operand.jsValueRegs());
}

void SpeculativeJIT::speculateMisc(Edge edge, JSValueRegs regs)
{
#if USE(JSVALUE64)
    DFG_TYPE_CHECK(
        regs, edge, SpecMisc,
        m_jit.branch64(MacroAssembler::Above, regs.gpr(), MacroAssembler::TrustedImm64(TagBitTypeOther | TagBitBool | TagBitUndefined)));
#else
    IGNORE_WARNINGS_BEGIN("enum-compare")
    static_assert(JSValue::Int32Tag >= JSValue::UndefinedTag, "Int32Tag is included in >= JSValue::UndefinedTag range.");
    IGNORE_WARNINGS_END
    DFG_TYPE_CHECK(
        regs, edge, ~SpecInt32Only,
        m_jit.branchIfInt32(regs.tagGPR()));
    DFG_TYPE_CHECK(
        regs, edge, SpecMisc,
        m_jit.branch32(MacroAssembler::Below, regs.tagGPR(), MacroAssembler::TrustedImm32(JSValue::UndefinedTag)));
#endif
}

void SpeculativeJIT::speculateMisc(Edge edge)
{
    if (!needsTypeCheck(edge, SpecMisc))
        return;
    
    JSValueOperand operand(this, edge, ManualOperandSpeculation);
    speculateMisc(edge, operand.jsValueRegs());
}

void SpeculativeJIT::speculate(Node*, Edge edge)
{
    switch (edge.useKind()) {
    case UntypedUse:
        break;
    case DoubleRepUse:
    case Int52RepUse:
    case KnownInt32Use:
    case KnownCellUse:
    case KnownStringUse:
    case KnownPrimitiveUse:
    case KnownOtherUse:
    case KnownBooleanUse:
        ASSERT(!m_interpreter.needsTypeCheck(edge));
        break;
    case Int32Use:
        speculateInt32(edge);
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
#if USE(JSVALUE64)
    case AnyIntUse:
        speculateAnyInt(edge);
        break;
    case DoubleRepAnyIntUse:
        speculateDoubleRepAnyInt(edge);
        break;
#endif
    case BooleanUse:
        speculateBoolean(edge);
        break;
    case CellUse:
        speculateCell(edge);
        break;
    case CellOrOtherUse:
        speculateCellOrOther(edge);
        break;
    case ObjectUse:
        speculateObject(edge);
        break;
    case FunctionUse:
        speculateFunction(edge);
        break;
    case ArrayUse:
        speculateArray(edge);
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
    case ObjectOrOtherUse:
        speculateObjectOrOther(edge);
        break;
    case StringIdentUse:
        speculateStringIdent(edge);
        break;
    case StringUse:
        speculateString(edge);
        break;
    case StringOrOtherUse:
        speculateStringOrOther(edge);
        break;
    case SymbolUse:
        speculateSymbol(edge);
        break;
    case BigIntUse:
        speculateBigInt(edge);
        break;
    case StringObjectUse:
        speculateStringObject(edge);
        break;
    case StringOrStringObjectUse:
        speculateStringOrStringObject(edge);
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
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::emitSwitchIntJump(
    SwitchData* data, GPRReg value, GPRReg scratch)
{
    SimpleJumpTable& table = m_jit.codeBlock()->switchJumpTable(data->switchTableIndex);
    table.ensureCTITable();
    m_jit.sub32(Imm32(table.min), value);
    addBranch(
        m_jit.branch32(JITCompiler::AboveOrEqual, value, Imm32(table.ctiOffsets.size())),
        data->fallThrough.block);
    m_jit.move(TrustedImmPtr(table.ctiOffsets.begin()), scratch);
    m_jit.loadPtr(JITCompiler::BaseIndex(scratch, value, JITCompiler::timesPtr()), scratch);
    
    m_jit.jump(scratch, JSSwitchPtrTag);
    data->didUseJumpTable = true;
}

void SpeculativeJIT::emitSwitchImm(Node* node, SwitchData* data)
{
    switch (node->child1().useKind()) {
    case Int32Use: {
        SpeculateInt32Operand value(this, node->child1());
        GPRTemporary temp(this);
        emitSwitchIntJump(data, value.gpr(), temp.gpr());
        noResult(node);
        break;
    }
        
    case UntypedUse: {
        JSValueOperand value(this, node->child1());
        GPRTemporary temp(this);
        JSValueRegs valueRegs = value.jsValueRegs();
        GPRReg scratch = temp.gpr();

        value.use();

        auto notInt32 = m_jit.branchIfNotInt32(valueRegs);
        emitSwitchIntJump(data, valueRegs.payloadGPR(), scratch);
        notInt32.link(&m_jit);
        addBranch(m_jit.branchIfNotNumber(valueRegs, scratch), data->fallThrough.block);
        silentSpillAllRegisters(scratch);
        callOperation(operationFindSwitchImmTargetForDouble, scratch, valueRegs, data->switchTableIndex);
        silentFillAllRegisters();

        m_jit.jump(scratch, JSSwitchPtrTag);
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::emitSwitchCharStringJump(
    SwitchData* data, GPRReg value, GPRReg scratch)
{
    m_jit.loadPtr(MacroAssembler::Address(value, JSString::offsetOfValue()), scratch);
    auto isRope = m_jit.branchIfRopeStringImpl(scratch);

    addBranch(
        m_jit.branch32(
            MacroAssembler::NotEqual,
            MacroAssembler::Address(scratch, StringImpl::lengthMemoryOffset()),
            TrustedImm32(1)),
        data->fallThrough.block);
    
    addSlowPathGenerator(slowPathCall(isRope, this, operationResolveRope, scratch, value));
    
    m_jit.loadPtr(MacroAssembler::Address(scratch, StringImpl::dataOffset()), value);
    
    JITCompiler::Jump is8Bit = m_jit.branchTest32(
        MacroAssembler::NonZero,
        MacroAssembler::Address(scratch, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit()));
    
    m_jit.load16(MacroAssembler::Address(value), scratch);
    
    JITCompiler::Jump ready = m_jit.jump();
    
    is8Bit.link(&m_jit);
    m_jit.load8(MacroAssembler::Address(value), scratch);
    
    ready.link(&m_jit);
    emitSwitchIntJump(data, scratch, value);
}

void SpeculativeJIT::emitSwitchChar(Node* node, SwitchData* data)
{
    switch (node->child1().useKind()) {
    case StringUse: {
        SpeculateCellOperand op1(this, node->child1());
        GPRTemporary temp(this);

        GPRReg op1GPR = op1.gpr();
        GPRReg tempGPR = temp.gpr();

        op1.use();

        speculateString(node->child1(), op1GPR);
        emitSwitchCharStringJump(data, op1GPR, tempGPR);
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    case UntypedUse: {
        JSValueOperand op1(this, node->child1());
        GPRTemporary temp(this);

        JSValueRegs op1Regs = op1.jsValueRegs();
        GPRReg tempGPR = temp.gpr();

        op1.use();
        
        addBranch(m_jit.branchIfNotCell(op1Regs), data->fallThrough.block);
        
        addBranch(m_jit.branchIfNotString(op1Regs.payloadGPR()), data->fallThrough.block);
        
        emitSwitchCharStringJump(data, op1Regs.payloadGPR(), tempGPR);
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

namespace {

struct CharacterCase {
    bool operator<(const CharacterCase& other) const
    {
        return character < other.character;
    }
    
    LChar character;
    unsigned begin;
    unsigned end;
};

} // anonymous namespace

void SpeculativeJIT::emitBinarySwitchStringRecurse(
    SwitchData* data, const Vector<SpeculativeJIT::StringSwitchCase>& cases,
    unsigned numChecked, unsigned begin, unsigned end, GPRReg buffer, GPRReg length,
    GPRReg temp, unsigned alreadyCheckedLength, bool checkedExactLength)
{
    static const bool verbose = false;
    
    if (verbose) {
        dataLog("We're down to the following cases, alreadyCheckedLength = ", alreadyCheckedLength, ":\n");
        for (unsigned i = begin; i < end; ++i) {
            dataLog("    ", cases[i].string, "\n");
        }
    }
    
    if (begin == end) {
        jump(data->fallThrough.block, ForceJump);
        return;
    }
    
    unsigned minLength = cases[begin].string->length();
    unsigned commonChars = minLength;
    bool allLengthsEqual = true;
    for (unsigned i = begin + 1; i < end; ++i) {
        unsigned myCommonChars = numChecked;
        for (unsigned j = numChecked;
            j < std::min(cases[begin].string->length(), cases[i].string->length());
            ++j) {
            if (cases[begin].string->at(j) != cases[i].string->at(j)) {
                if (verbose)
                    dataLog("string(", cases[i].string, ")[", j, "] != string(", cases[begin].string, ")[", j, "]\n");
                break;
            }
            myCommonChars++;
        }
        commonChars = std::min(commonChars, myCommonChars);
        if (minLength != cases[i].string->length())
            allLengthsEqual = false;
        minLength = std::min(minLength, cases[i].string->length());
    }
    
    if (checkedExactLength) {
        RELEASE_ASSERT(alreadyCheckedLength == minLength);
        RELEASE_ASSERT(allLengthsEqual);
    }
    
    RELEASE_ASSERT(minLength >= commonChars);
    
    if (verbose)
        dataLog("length = ", minLength, ", commonChars = ", commonChars, ", allLengthsEqual = ", allLengthsEqual, "\n");
    
    if (!allLengthsEqual && alreadyCheckedLength < minLength)
        branch32(MacroAssembler::Below, length, Imm32(minLength), data->fallThrough.block);
    if (allLengthsEqual && (alreadyCheckedLength < minLength || !checkedExactLength))
        branch32(MacroAssembler::NotEqual, length, Imm32(minLength), data->fallThrough.block);
    
    for (unsigned i = numChecked; i < commonChars; ++i) {
        branch8(
            MacroAssembler::NotEqual, MacroAssembler::Address(buffer, i),
            TrustedImm32(cases[begin].string->at(i)), data->fallThrough.block);
    }
    
    if (minLength == commonChars) {
        // This is the case where one of the cases is a prefix of all of the other cases.
        // We've already checked that the input string is a prefix of all of the cases,
        // so we just check length to jump to that case.
        
        if (!ASSERT_DISABLED) {
            ASSERT(cases[begin].string->length() == commonChars);
            for (unsigned i = begin + 1; i < end; ++i)
                ASSERT(cases[i].string->length() > commonChars);
        }
        
        if (allLengthsEqual) {
            RELEASE_ASSERT(end == begin + 1);
            jump(cases[begin].target, ForceJump);
            return;
        }
        
        branch32(MacroAssembler::Equal, length, Imm32(commonChars), cases[begin].target);
        
        // We've checked if the length is >= minLength, and then we checked if the
        // length is == commonChars. We get to this point if it is >= minLength but not
        // == commonChars. Hence we know that it now must be > minLength, i.e., that
        // it's >= minLength + 1.
        emitBinarySwitchStringRecurse(
            data, cases, commonChars, begin + 1, end, buffer, length, temp, minLength + 1, false);
        return;
    }
    
    // At this point we know that the string is longer than commonChars, and we've only
    // verified commonChars. Use a binary switch on the next unchecked character, i.e.
    // string[commonChars].
    
    RELEASE_ASSERT(end >= begin + 2);
    
    m_jit.load8(MacroAssembler::Address(buffer, commonChars), temp);
    
    Vector<CharacterCase> characterCases;
    CharacterCase currentCase;
    currentCase.character = cases[begin].string->at(commonChars);
    currentCase.begin = begin;
    currentCase.end = begin + 1;
    for (unsigned i = begin + 1; i < end; ++i) {
        if (cases[i].string->at(commonChars) != currentCase.character) {
            if (verbose)
                dataLog("string(", cases[i].string, ")[", commonChars, "] != string(", cases[begin].string, ")[", commonChars, "]\n");
            currentCase.end = i;
            characterCases.append(currentCase);
            currentCase.character = cases[i].string->at(commonChars);
            currentCase.begin = i;
            currentCase.end = i + 1;
        } else
            currentCase.end = i + 1;
    }
    characterCases.append(currentCase);
    
    Vector<int64_t> characterCaseValues;
    for (unsigned i = 0; i < characterCases.size(); ++i)
        characterCaseValues.append(characterCases[i].character);
    
    BinarySwitch binarySwitch(temp, characterCaseValues, BinarySwitch::Int32);
    while (binarySwitch.advance(m_jit)) {
        const CharacterCase& myCase = characterCases[binarySwitch.caseIndex()];
        emitBinarySwitchStringRecurse(
            data, cases, commonChars + 1, myCase.begin, myCase.end, buffer, length,
            temp, minLength, allLengthsEqual);
    }
    
    addBranch(binarySwitch.fallThrough(), data->fallThrough.block);
}

void SpeculativeJIT::emitSwitchStringOnString(SwitchData* data, GPRReg string)
{
    data->didUseJumpTable = true;
    
    bool canDoBinarySwitch = true;
    unsigned totalLength = 0;
    
    for (unsigned i = data->cases.size(); i--;) {
        StringImpl* string = data->cases[i].value.stringImpl();
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
        flushRegisters();
        callOperation(
            operationSwitchString, string, static_cast<size_t>(data->switchTableIndex), string);
        m_jit.exceptionCheck();
        m_jit.jump(string, JSSwitchPtrTag);
        return;
    }
    
    GPRTemporary length(this);
    GPRTemporary temp(this);
    
    GPRReg lengthGPR = length.gpr();
    GPRReg tempGPR = temp.gpr();
    
    MacroAssembler::JumpList slowCases;
    m_jit.loadPtr(MacroAssembler::Address(string, JSString::offsetOfValue()), tempGPR);
    slowCases.append(m_jit.branchIfRopeStringImpl(tempGPR));
    m_jit.load32(MacroAssembler::Address(tempGPR, StringImpl::lengthMemoryOffset()), lengthGPR);
    
    slowCases.append(m_jit.branchTest32(
        MacroAssembler::Zero,
        MacroAssembler::Address(tempGPR, StringImpl::flagsOffset()),
        TrustedImm32(StringImpl::flagIs8Bit())));
    
    m_jit.loadPtr(MacroAssembler::Address(tempGPR, StringImpl::dataOffset()), string);
    
    Vector<StringSwitchCase> cases;
    for (unsigned i = 0; i < data->cases.size(); ++i) {
        cases.append(
            StringSwitchCase(data->cases[i].value.stringImpl(), data->cases[i].target.block));
    }
    
    std::sort(cases.begin(), cases.end());
    
    emitBinarySwitchStringRecurse(
        data, cases, 0, 0, cases.size(), string, lengthGPR, tempGPR, 0, false);
    
    slowCases.link(&m_jit);
    silentSpillAllRegisters(string);
    callOperation(operationSwitchString, string, static_cast<size_t>(data->switchTableIndex), string);
    silentFillAllRegisters();
    m_jit.exceptionCheck();
    m_jit.jump(string, JSSwitchPtrTag);
}

void SpeculativeJIT::emitSwitchString(Node* node, SwitchData* data)
{
    switch (node->child1().useKind()) {
    case StringIdentUse: {
        SpeculateCellOperand op1(this, node->child1());
        GPRTemporary temp(this);
        
        GPRReg op1GPR = op1.gpr();
        GPRReg tempGPR = temp.gpr();
        
        speculateString(node->child1(), op1GPR);
        speculateStringIdentAndLoadStorage(node->child1(), op1GPR, tempGPR);
        
        Vector<int64_t> identifierCaseValues;
        for (unsigned i = 0; i < data->cases.size(); ++i) {
            identifierCaseValues.append(
                static_cast<int64_t>(bitwise_cast<intptr_t>(data->cases[i].value.stringImpl())));
        }
        
        BinarySwitch binarySwitch(tempGPR, identifierCaseValues, BinarySwitch::IntPtr);
        while (binarySwitch.advance(m_jit))
            jump(data->cases[binarySwitch.caseIndex()].target.block, ForceJump);
        addBranch(binarySwitch.fallThrough(), data->fallThrough.block);
        
        noResult(node);
        break;
    }
        
    case StringUse: {
        SpeculateCellOperand op1(this, node->child1());
        
        GPRReg op1GPR = op1.gpr();
        
        op1.use();

        speculateString(node->child1(), op1GPR);
        emitSwitchStringOnString(data, op1GPR);
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    case UntypedUse: {
        JSValueOperand op1(this, node->child1());
        
        JSValueRegs op1Regs = op1.jsValueRegs();
        
        op1.use();
        
        addBranch(m_jit.branchIfNotCell(op1Regs), data->fallThrough.block);
        
        addBranch(m_jit.branchIfNotString(op1Regs.payloadGPR()), data->fallThrough.block);
        
        emitSwitchStringOnString(data, op1Regs.payloadGPR());
        noResult(node, UseChildrenCalledExplicitly);
        break;
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::emitSwitch(Node* node)
{
    SwitchData* data = node->switchData();
    switch (data->kind) {
    case SwitchImm: {
        emitSwitchImm(node, data);
        return;
    }
    case SwitchChar: {
        emitSwitchChar(node, data);
        return;
    }
    case SwitchString: {
        emitSwitchString(node, data);
        return;
    }
    case SwitchCell: {
        DFG_CRASH(m_jit.graph(), node, "Bad switch kind");
        return;
    } }
    RELEASE_ASSERT_NOT_REACHED();
}

void SpeculativeJIT::addBranch(const MacroAssembler::JumpList& jump, BasicBlock* destination)
{
    for (unsigned i = jump.jumps().size(); i--;)
        addBranch(jump.jumps()[i], destination);
}

void SpeculativeJIT::linkBranches()
{
    for (auto& branch : m_branches)
        branch.jump.linkTo(m_jit.blockHeads()[branch.destination->index], &m_jit);
}

void SpeculativeJIT::compileStoreBarrier(Node* node)
{
    ASSERT(node->op() == StoreBarrier || node->op() == FencedStoreBarrier);
    
    bool isFenced = node->op() == FencedStoreBarrier;
    
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary scratch1(this);
    
    GPRReg baseGPR = base.gpr();
    GPRReg scratch1GPR = scratch1.gpr();
    
    JITCompiler::JumpList ok;
    
    if (isFenced) {
        ok.append(m_jit.barrierBranch(*m_jit.vm(), baseGPR, scratch1GPR));
        
        JITCompiler::Jump noFence = m_jit.jumpIfMutatorFenceNotNeeded(*m_jit.vm());
        m_jit.memoryFence();
        ok.append(m_jit.barrierBranchWithoutFence(baseGPR));
        noFence.link(&m_jit);
    } else
        ok.append(m_jit.barrierBranchWithoutFence(baseGPR));

    silentSpillAllRegisters(InvalidGPRReg);
    callOperation(operationWriteBarrierSlowPath, baseGPR);
    silentFillAllRegisters();

    ok.link(&m_jit);

    noResult(node);
}

void SpeculativeJIT::compilePutAccessorById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    SpeculateCellOperand accessor(this, node->child2());

    GPRReg baseGPR = base.gpr();
    GPRReg accessorGPR = accessor.gpr();

    flushRegisters();
    callOperation(node->op() == PutGetterById ? operationPutGetterById : operationPutSetterById, NoResult, baseGPR, identifierUID(node->identifierNumber()), node->accessorAttributes(), accessorGPR);
    m_jit.exceptionCheck();

    noResult(node);
}

void SpeculativeJIT::compilePutGetterSetterById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand getter(this, node->child2());
    JSValueOperand setter(this, node->child3());

#if USE(JSVALUE64)
    GPRReg baseGPR = base.gpr();
    GPRReg getterGPR = getter.gpr();
    GPRReg setterGPR = setter.gpr();

    flushRegisters();
    callOperation(operationPutGetterSetter, NoResult, baseGPR, identifierUID(node->identifierNumber()), node->accessorAttributes(), getterGPR, setterGPR);
#else
    // These JSValues may be JSUndefined OR JSFunction*.
    // At that time,
    // 1. If the JSValue is JSUndefined, its payload becomes nullptr.
    // 2. If the JSValue is JSFunction*, its payload becomes JSFunction*.
    // So extract payload and pass it to operationPutGetterSetter. This hack is used as the same way in baseline JIT.
    GPRReg baseGPR = base.gpr();
    JSValueRegs getterRegs = getter.jsValueRegs();
    JSValueRegs setterRegs = setter.jsValueRegs();

    flushRegisters();
    callOperation(operationPutGetterSetter, NoResult, baseGPR, identifierUID(node->identifierNumber()), node->accessorAttributes(), getterRegs.payloadGPR(), setterRegs.payloadGPR());
#endif
    m_jit.exceptionCheck();

    noResult(node);
}

void SpeculativeJIT::compileResolveScope(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    flushRegisters();
    callOperation(operationResolveScope, resultGPR, scopeGPR, identifierUID(node->identifierNumber()));
    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileResolveScopeForHoistingFuncDeclInEval(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationResolveScopeForHoistingFuncDeclInEval, resultRegs, scopeGPR, identifierUID(node->identifierNumber()));
    m_jit.exceptionCheck();
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetGlobalVariable(Node* node)
{
    JSValueRegsTemporary result(this);
    JSValueRegs resultRegs = result.regs();
    m_jit.loadValue(node->variablePointer(), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutGlobalVariable(Node* node)
{
    JSValueOperand value(this, node->child2());
    JSValueRegs valueRegs = value.jsValueRegs();
    m_jit.storeValue(valueRegs, node->variablePointer());
    noResult(node);
}

void SpeculativeJIT::compileGetDynamicVar(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeGPR = scope.gpr();
    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationGetDynamicVar, resultRegs, scopeGPR, identifierUID(node->identifierNumber()), node->getPutInfo());
    m_jit.exceptionCheck();
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutDynamicVar(Node* node)
{
    SpeculateCellOperand scope(this, node->child1());
    JSValueOperand value(this, node->child2());

    GPRReg scopeGPR = scope.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    flushRegisters();
    callOperation(operationPutDynamicVar, NoResult, scopeGPR, valueRegs, identifierUID(node->identifierNumber()), node->getPutInfo());
    m_jit.exceptionCheck();
    noResult(node);
}

void SpeculativeJIT::compileGetClosureVar(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueRegsTemporary result(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs resultRegs = result.regs();

    m_jit.loadValue(JITCompiler::Address(baseGPR, JSLexicalEnvironment::offsetOfVariable(node->scopeOffset())), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutClosureVar(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    m_jit.storeValue(valueRegs, JITCompiler::Address(baseGPR, JSLexicalEnvironment::offsetOfVariable(node->scopeOffset())));
    noResult(node);
}

void SpeculativeJIT::compilePutAccessorByVal(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand subscript(this, node->child2());
    SpeculateCellOperand accessor(this, node->child3());

    auto operation = node->op() == PutGetterByVal ? operationPutGetterByVal : operationPutSetterByVal;

    GPRReg baseGPR = base.gpr();
    JSValueRegs subscriptRegs = subscript.jsValueRegs();
    GPRReg accessorGPR = accessor.gpr();

    flushRegisters();
    callOperation(operation, NoResult, baseGPR, subscriptRegs, node->accessorAttributes(), accessorGPR);
    m_jit.exceptionCheck();

    noResult(node);
}

void SpeculativeJIT::compileGetRegExpObjectLastIndex(Node* node)
{
    SpeculateCellOperand regExp(this, node->child1());
    JSValueRegsTemporary result(this);
    GPRReg regExpGPR = regExp.gpr();
    JSValueRegs resultRegs = result.regs();
    speculateRegExpObject(node->child1(), regExpGPR);
    m_jit.loadValue(JITCompiler::Address(regExpGPR, RegExpObject::offsetOfLastIndex()), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileSetRegExpObjectLastIndex(Node* node)
{
    SpeculateCellOperand regExp(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRReg regExpGPR = regExp.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    if (!node->ignoreLastIndexIsWritable()) {
        speculateRegExpObject(node->child1(), regExpGPR);
        speculationCheck(
            ExoticObjectMode, JSValueRegs(), nullptr,
            m_jit.branchTest8(
                JITCompiler::Zero,
                JITCompiler::Address(regExpGPR, RegExpObject::offsetOfLastIndexIsWritable())));
    }

    m_jit.storeValue(valueRegs, JITCompiler::Address(regExpGPR, RegExpObject::offsetOfLastIndex()));
    noResult(node);
}

void SpeculativeJIT::compileRegExpExec(Node* node)
{
    bool sample = false;
    if (sample)
        m_jit.incrementSuperSamplerCount();

    SpeculateCellOperand globalObject(this, node->child1());
    GPRReg globalObjectGPR = globalObject.gpr();

    if (node->child2().useKind() == RegExpObjectUse) {
        if (node->child3().useKind() == StringUse) {
            SpeculateCellOperand base(this, node->child2());
            SpeculateCellOperand argument(this, node->child3());
            GPRReg baseGPR = base.gpr();
            GPRReg argumentGPR = argument.gpr();
            speculateRegExpObject(node->child2(), baseGPR);
            speculateString(node->child3(), argumentGPR);

            flushRegisters();
            JSValueRegsFlushedCallResult result(this);
            JSValueRegs resultRegs = result.regs();
            callOperation(operationRegExpExecString, resultRegs, globalObjectGPR, baseGPR, argumentGPR);
            m_jit.exceptionCheck();

            jsValueResult(resultRegs, node);

            if (sample)
                m_jit.decrementSuperSamplerCount();
            return;
        }

        SpeculateCellOperand base(this, node->child2());
        JSValueOperand argument(this, node->child3());
        GPRReg baseGPR = base.gpr();
        JSValueRegs argumentRegs = argument.jsValueRegs();
        speculateRegExpObject(node->child2(), baseGPR);

        flushRegisters();
        JSValueRegsFlushedCallResult result(this);
        JSValueRegs resultRegs = result.regs();
        callOperation(operationRegExpExec, resultRegs, globalObjectGPR, baseGPR, argumentRegs);
        m_jit.exceptionCheck();

        jsValueResult(resultRegs, node);

        if (sample)
            m_jit.decrementSuperSamplerCount();
        return;
    }

    JSValueOperand base(this, node->child2());
    JSValueOperand argument(this, node->child3());
    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueRegs argumentRegs = argument.jsValueRegs();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationRegExpExecGeneric, resultRegs, globalObjectGPR, baseRegs, argumentRegs);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);

    if (sample)
        m_jit.decrementSuperSamplerCount();
}

void SpeculativeJIT::compileRegExpTest(Node* node)
{
    SpeculateCellOperand globalObject(this, node->child1());
    GPRReg globalObjectGPR = globalObject.gpr();

    if (node->child2().useKind() == RegExpObjectUse) {
        if (node->child3().useKind() == StringUse) {
            SpeculateCellOperand base(this, node->child2());
            SpeculateCellOperand argument(this, node->child3());
            GPRReg baseGPR = base.gpr();
            GPRReg argumentGPR = argument.gpr();
            speculateRegExpObject(node->child2(), baseGPR);
            speculateString(node->child3(), argumentGPR);

            flushRegisters();
            GPRFlushedCallResult result(this);
            callOperation(operationRegExpTestString, result.gpr(), globalObjectGPR, baseGPR, argumentGPR);
            m_jit.exceptionCheck();

            unblessedBooleanResult(result.gpr(), node);
            return;
        }

        SpeculateCellOperand base(this, node->child2());
        JSValueOperand argument(this, node->child3());
        GPRReg baseGPR = base.gpr();
        JSValueRegs argumentRegs = argument.jsValueRegs();
        speculateRegExpObject(node->child2(), baseGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        callOperation(operationRegExpTest, result.gpr(), globalObjectGPR, baseGPR, argumentRegs);
        m_jit.exceptionCheck();

        unblessedBooleanResult(result.gpr(), node);
        return;
    }

    JSValueOperand base(this, node->child2());
    JSValueOperand argument(this, node->child3());
    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueRegs argumentRegs = argument.jsValueRegs();

    flushRegisters();
    GPRFlushedCallResult result(this);
    callOperation(operationRegExpTestGeneric, result.gpr(), globalObjectGPR, baseRegs, argumentRegs);
    m_jit.exceptionCheck();

    unblessedBooleanResult(result.gpr(), node);
}

void SpeculativeJIT::compileStringReplace(Node* node)
{
    ASSERT(node->op() == StringReplace || node->op() == StringReplaceRegExp);
    bool sample = false;
    if (sample)
        m_jit.incrementSuperSamplerCount();

    if (node->child1().useKind() == StringUse
        && node->child2().useKind() == RegExpObjectUse
        && node->child3().useKind() == StringUse) {
        if (JSString* replace = node->child3()->dynamicCastConstant<JSString*>(*m_jit.vm())) {
            if (!replace->length()) {
                SpeculateCellOperand string(this, node->child1());
                SpeculateCellOperand regExp(this, node->child2());
                GPRReg stringGPR = string.gpr();
                GPRReg regExpGPR = regExp.gpr();
                speculateString(node->child1(), stringGPR);
                speculateRegExpObject(node->child2(), regExpGPR);

                flushRegisters();
                GPRFlushedCallResult result(this);
                callOperation(operationStringProtoFuncReplaceRegExpEmptyStr, result.gpr(), stringGPR, regExpGPR);
                m_jit.exceptionCheck();
                cellResult(result.gpr(), node);
                if (sample)
                    m_jit.decrementSuperSamplerCount();
                return;
            }
        }

        SpeculateCellOperand string(this, node->child1());
        SpeculateCellOperand regExp(this, node->child2());
        SpeculateCellOperand replace(this, node->child3());
        GPRReg stringGPR = string.gpr();
        GPRReg regExpGPR = regExp.gpr();
        GPRReg replaceGPR = replace.gpr();
        speculateString(node->child1(), stringGPR);
        speculateRegExpObject(node->child2(), regExpGPR);
        speculateString(node->child3(), replaceGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        callOperation(operationStringProtoFuncReplaceRegExpString, result.gpr(), stringGPR, regExpGPR, replaceGPR);
        m_jit.exceptionCheck();
        cellResult(result.gpr(), node);
        if (sample)
            m_jit.decrementSuperSamplerCount();
        return;
    }

    // If we fixed up the edge of child2, we inserted a Check(@child2, String).
    OperandSpeculationMode child2SpeculationMode = AutomaticOperandSpeculation;
    if (node->child2().useKind() == StringUse)
        child2SpeculationMode = ManualOperandSpeculation;

    JSValueOperand string(this, node->child1());
    JSValueOperand search(this, node->child2(), child2SpeculationMode);
    JSValueOperand replace(this, node->child3());
    JSValueRegs stringRegs = string.jsValueRegs();
    JSValueRegs searchRegs = search.jsValueRegs();
    JSValueRegs replaceRegs = replace.jsValueRegs();

    flushRegisters();
    GPRFlushedCallResult result(this);
    callOperation(operationStringProtoFuncReplaceGeneric, result.gpr(), stringRegs, searchRegs, replaceRegs);
    m_jit.exceptionCheck();
    cellResult(result.gpr(), node);
    if (sample)
        m_jit.decrementSuperSamplerCount();
}

void SpeculativeJIT::compileRegExpExecNonGlobalOrSticky(Node* node)
{
    SpeculateCellOperand globalObject(this, node->child1());
    SpeculateCellOperand argument(this, node->child2());
    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg argumentGPR = argument.gpr();

    speculateString(node->child2(), argumentGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(
        operationRegExpExecNonGlobalOrSticky, resultRegs,
        globalObjectGPR, TrustedImmPtr(node->cellOperand()), argumentGPR);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileRegExpMatchFastGlobal(Node* node)
{
    SpeculateCellOperand globalObject(this, node->child1());
    SpeculateCellOperand argument(this, node->child2());
    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg argumentGPR = argument.gpr();

    speculateString(node->child2(), argumentGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(
        operationRegExpMatchFastGlobalString, resultRegs,
        globalObjectGPR, TrustedImmPtr(node->cellOperand()), argumentGPR);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileRegExpMatchFast(Node* node)
{
    SpeculateCellOperand globalObject(this, node->child1());
    SpeculateCellOperand base(this, node->child2());
    SpeculateCellOperand argument(this, node->child3());
    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg baseGPR = base.gpr();
    GPRReg argumentGPR = argument.gpr();
    speculateRegExpObject(node->child2(), baseGPR);
    speculateString(node->child3(), argumentGPR);

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(
        operationRegExpMatchFastString, resultRegs,
        globalObjectGPR, baseGPR, argumentGPR);
    m_jit.exceptionCheck();

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileLazyJSConstant(Node* node)
{
    JSValueRegsTemporary result(this);
    JSValueRegs resultRegs = result.regs();
    node->lazyJSValue().emit(m_jit, resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileMaterializeNewObject(Node* node)
{
    RegisteredStructure structure = node->structureSet().at(0);
    ASSERT(m_jit.graph().varArgChild(node, 0)->dynamicCastConstant<Structure*>(*m_jit.vm()) == structure.get());

    ObjectMaterializationData& data = node->objectMaterializationData();
        
    IndexingType indexingType = structure->indexingType();
    bool hasIndexingHeader = hasIndexedProperties(indexingType);
    int32_t publicLength = 0;
    int32_t vectorLength = 0;

    if (hasIndexingHeader) {
        for (unsigned i = data.m_properties.size(); i--;) {
            Edge edge = m_jit.graph().varArgChild(node, 1 + i);
            switch (data.m_properties[i].kind()) {
            case PublicLengthPLoc:
                publicLength = edge->asInt32();
                break;
            case VectorLengthPLoc:
                vectorLength = edge->asInt32();
                break;
            default:
                break;
            }
        }
    }

    GPRTemporary result(this);
    GPRTemporary storage(this);
    GPRReg resultGPR = result.gpr();
    GPRReg storageGPR = storage.gpr();
    
    emitAllocateRawObject(resultGPR, structure, storageGPR, 0, vectorLength);
    
    m_jit.store32(
        JITCompiler::TrustedImm32(publicLength),
        JITCompiler::Address(storageGPR, Butterfly::offsetOfPublicLength()));

    for (unsigned i = data.m_properties.size(); i--;) {
        Edge edge = m_jit.graph().varArgChild(node, 1 + i);
        PromotedLocationDescriptor descriptor = data.m_properties[i];
        switch (descriptor.kind()) {
        case IndexedPropertyPLoc: {
            JSValueOperand value(this, edge);
            m_jit.storeValue(
                value.jsValueRegs(),
                JITCompiler::Address(storageGPR, sizeof(EncodedJSValue) * descriptor.info()));
            break;
        }

        case NamedPropertyPLoc: {
            StringImpl* uid = m_jit.graph().identifiers()[descriptor.info()];
            for (PropertyMapEntry entry : structure->getPropertiesConcurrently()) {
                if (uid != entry.key)
                    continue;

                JSValueOperand value(this, edge);
                GPRReg baseGPR = isInlineOffset(entry.offset) ? resultGPR : storageGPR;
                m_jit.storeValue(
                    value.jsValueRegs(),
                    JITCompiler::Address(baseGPR, offsetRelativeToBase(entry.offset)));
            }
            break;
        }
            
        default:
            break;
        }
    }

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileRecordRegExpCachedResult(Node* node)
{
    Edge globalObjectEdge = m_jit.graph().varArgChild(node, 0);
    Edge regExpEdge = m_jit.graph().varArgChild(node, 1);
    Edge stringEdge = m_jit.graph().varArgChild(node, 2);
    Edge startEdge = m_jit.graph().varArgChild(node, 3);
    Edge endEdge = m_jit.graph().varArgChild(node, 4);

    SpeculateCellOperand globalObject(this, globalObjectEdge);
    SpeculateCellOperand regExp(this, regExpEdge);
    SpeculateCellOperand string(this, stringEdge);
    SpeculateInt32Operand start(this, startEdge);
    SpeculateInt32Operand end(this, endEdge);

    GPRReg globalObjectGPR = globalObject.gpr();
    GPRReg regExpGPR = regExp.gpr();
    GPRReg stringGPR = string.gpr();
    GPRReg startGPR = start.gpr();
    GPRReg endGPR = end.gpr();

    ptrdiff_t offset = JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult();

    m_jit.storePtr(
        regExpGPR,
        JITCompiler::Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfLastRegExp()));
    m_jit.storePtr(
        stringGPR,
        JITCompiler::Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfLastInput()));
    m_jit.store32(
        startGPR,
        JITCompiler::Address(
            globalObjectGPR,
            offset + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, start)));
    m_jit.store32(
        endGPR,
        JITCompiler::Address(
            globalObjectGPR,
            offset + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, end)));
    m_jit.store8(
        TrustedImm32(0),
        JITCompiler::Address(globalObjectGPR, offset + RegExpCachedResult::offsetOfReified()));

    noResult(node);
}

void SpeculativeJIT::compileDefineDataProperty(Node* node)
{
#if USE(JSVALUE64)
    static_assert(GPRInfo::numberOfRegisters >= 5, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");
#else
    static_assert(GPRInfo::numberOfRegisters >= 6, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");
#endif

    SpeculateCellOperand base(this, m_jit.graph().varArgChild(node, 0));
    GPRReg baseGPR = base.gpr();

    JSValueOperand value(this, m_jit.graph().varArgChild(node, 2));
    JSValueRegs valueRegs = value.jsValueRegs();

    SpeculateInt32Operand attributes(this, m_jit.graph().varArgChild(node, 3));
    GPRReg attributesGPR = attributes.gpr();

    Edge& propertyEdge = m_jit.graph().varArgChild(node, 1);
    switch (propertyEdge.useKind()) {
    case StringUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRReg propertyGPR = property.gpr();
        speculateString(propertyEdge, propertyGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineDataPropertyString, NoResult, baseGPR, propertyGPR, valueRegs, attributesGPR);
        m_jit.exceptionCheck();
        break;
    }
    case StringIdentUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRTemporary ident(this);

        GPRReg propertyGPR = property.gpr();
        GPRReg identGPR = ident.gpr();

        speculateString(propertyEdge, propertyGPR);
        speculateStringIdentAndLoadStorage(propertyEdge, propertyGPR, identGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineDataPropertyStringIdent, NoResult, baseGPR, identGPR, valueRegs, attributesGPR);
        m_jit.exceptionCheck();
        break;
    }
    case SymbolUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRReg propertyGPR = property.gpr();
        speculateSymbol(propertyEdge, propertyGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineDataPropertySymbol, NoResult, baseGPR, propertyGPR, valueRegs, attributesGPR);
        m_jit.exceptionCheck();
        break;
    }
    case UntypedUse: {
        JSValueOperand property(this, propertyEdge);
        JSValueRegs propertyRegs = property.jsValueRegs();

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineDataProperty, NoResult, baseGPR, propertyRegs, valueRegs, attributesGPR);
        m_jit.exceptionCheck();
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    noResult(node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileDefineAccessorProperty(Node* node)
{
#if USE(JSVALUE64)
    static_assert(GPRInfo::numberOfRegisters >= 5, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");
#else
    static_assert(GPRInfo::numberOfRegisters >= 6, "We are assuming we have enough registers to make this call without incrementally setting up the arguments.");
#endif

    SpeculateCellOperand base(this, m_jit.graph().varArgChild(node, 0));
    GPRReg baseGPR = base.gpr();

    SpeculateCellOperand getter(this, m_jit.graph().varArgChild(node, 2));
    GPRReg getterGPR = getter.gpr();

    SpeculateCellOperand setter(this, m_jit.graph().varArgChild(node, 3));
    GPRReg setterGPR = setter.gpr();

    SpeculateInt32Operand attributes(this, m_jit.graph().varArgChild(node, 4));
    GPRReg attributesGPR = attributes.gpr();

    Edge& propertyEdge = m_jit.graph().varArgChild(node, 1);
    switch (propertyEdge.useKind()) {
    case StringUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRReg propertyGPR = property.gpr();
        speculateString(propertyEdge, propertyGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineAccessorPropertyString, NoResult, baseGPR, propertyGPR, getterGPR, setterGPR, attributesGPR);
        m_jit.exceptionCheck();
        break;
    }
    case StringIdentUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRTemporary ident(this);

        GPRReg propertyGPR = property.gpr();
        GPRReg identGPR = ident.gpr();

        speculateString(propertyEdge, propertyGPR);
        speculateStringIdentAndLoadStorage(propertyEdge, propertyGPR, identGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineAccessorPropertyStringIdent, NoResult, baseGPR, identGPR, getterGPR, setterGPR, attributesGPR);
        m_jit.exceptionCheck();
        break;
    }
    case SymbolUse: {
        SpeculateCellOperand property(this, propertyEdge);
        GPRReg propertyGPR = property.gpr();
        speculateSymbol(propertyEdge, propertyGPR);

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineAccessorPropertySymbol, NoResult, baseGPR, propertyGPR, getterGPR, setterGPR, attributesGPR);
        m_jit.exceptionCheck();
        break;
    }
    case UntypedUse: {
        JSValueOperand property(this, propertyEdge);
        JSValueRegs propertyRegs = property.jsValueRegs();

        useChildren(node);

        flushRegisters();
        callOperation(operationDefineAccessorProperty, NoResult, baseGPR, propertyRegs, getterGPR, setterGPR, attributesGPR);
        m_jit.exceptionCheck();
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    noResult(node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::emitAllocateButterfly(GPRReg storageResultGPR, GPRReg sizeGPR, GPRReg scratch1, GPRReg scratch2, GPRReg scratch3, MacroAssembler::JumpList& slowCases)
{
    RELEASE_ASSERT(RegisterSet(storageResultGPR, sizeGPR, scratch1, scratch2, scratch3).numberOfSetGPRs() == 5);
    ASSERT((1 << 3) == sizeof(JSValue));
    m_jit.zeroExtend32ToPtr(sizeGPR, scratch1);
    m_jit.lshift32(TrustedImm32(3), scratch1);
    m_jit.add32(TrustedImm32(sizeof(IndexingHeader)), scratch1, scratch2);
    m_jit.emitAllocateVariableSized(
        storageResultGPR, m_jit.vm()->jsValueGigacageAuxiliarySpace, scratch2, scratch1, scratch3, slowCases);
    m_jit.addPtr(TrustedImm32(sizeof(IndexingHeader)), storageResultGPR);

    m_jit.store32(sizeGPR, MacroAssembler::Address(storageResultGPR, Butterfly::offsetOfPublicLength()));
    m_jit.store32(sizeGPR, MacroAssembler::Address(storageResultGPR, Butterfly::offsetOfVectorLength()));
}

void SpeculativeJIT::compileNormalizeMapKey(Node* node)
{
    ASSERT(node->child1().useKind() == UntypedUse);
    JSValueOperand key(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, key);
    GPRTemporary scratch(this);
    FPRTemporary doubleValue(this);
    FPRTemporary temp(this);

    JSValueRegs keyRegs = key.jsValueRegs();
    JSValueRegs resultRegs = result.regs();
    GPRReg scratchGPR = scratch.gpr();
    FPRReg doubleValueFPR = doubleValue.fpr();
    FPRReg tempFPR = temp.fpr();

    CCallHelpers::JumpList passThroughCases;

    passThroughCases.append(m_jit.branchIfNotNumber(keyRegs, scratchGPR));
    passThroughCases.append(m_jit.branchIfInt32(keyRegs));

#if USE(JSVALUE64)
    m_jit.unboxDoubleWithoutAssertions(keyRegs.gpr(), scratchGPR, doubleValueFPR);
#else
    unboxDouble(keyRegs.tagGPR(), keyRegs.payloadGPR(), doubleValueFPR, tempFPR);
#endif
    passThroughCases.append(m_jit.branchIfNaN(doubleValueFPR));

    m_jit.truncateDoubleToInt32(doubleValueFPR, scratchGPR);
    m_jit.convertInt32ToDouble(scratchGPR, tempFPR);
    passThroughCases.append(m_jit.branchDouble(JITCompiler::DoubleNotEqual, doubleValueFPR, tempFPR));

    m_jit.boxInt32(scratchGPR, resultRegs);
    auto done = m_jit.jump();

    passThroughCases.link(&m_jit);
    m_jit.moveValueRegs(keyRegs, resultRegs);

    done.link(&m_jit);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetMapBucketHead(Node* node)
{
    SpeculateCellOperand map(this, node->child1());
    GPRTemporary bucket(this);

    GPRReg mapGPR = map.gpr();
    GPRReg bucketGPR = bucket.gpr();

    if (node->child1().useKind() == MapObjectUse)
        speculateMapObject(node->child1(), mapGPR);
    else if (node->child1().useKind() == SetObjectUse)
        speculateSetObject(node->child1(), mapGPR);
    else
        RELEASE_ASSERT_NOT_REACHED();

    ASSERT(HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::offsetOfHead() == HashMapImpl<HashMapBucket<HashMapBucketDataKeyValue>>::offsetOfHead());
    m_jit.loadPtr(MacroAssembler::Address(mapGPR, HashMapImpl<HashMapBucket<HashMapBucketDataKey>>::offsetOfHead()), bucketGPR);
    cellResult(bucketGPR, node);
}

void SpeculativeJIT::compileGetMapBucketNext(Node* node)
{
    SpeculateCellOperand bucket(this, node->child1());
    GPRTemporary result(this);

    GPRReg bucketGPR = bucket.gpr();
    GPRReg resultGPR = result.gpr();

    ASSERT(HashMapBucket<HashMapBucketDataKey>::offsetOfNext() == HashMapBucket<HashMapBucketDataKeyValue>::offsetOfNext());
    ASSERT(HashMapBucket<HashMapBucketDataKey>::offsetOfKey() == HashMapBucket<HashMapBucketDataKeyValue>::offsetOfKey());
    m_jit.loadPtr(MacroAssembler::Address(bucketGPR, HashMapBucket<HashMapBucketDataKeyValue>::offsetOfNext()), resultGPR);

    MacroAssembler::Label loop = m_jit.label();
    auto notBucket = m_jit.branchTestPtr(MacroAssembler::Zero, resultGPR);
#if USE(JSVALUE32_64)
    auto done = m_jit.branch32(MacroAssembler::NotEqual, MacroAssembler::Address(resultGPR, HashMapBucket<HashMapBucketDataKeyValue>::offsetOfKey() + TagOffset), TrustedImm32(JSValue::EmptyValueTag));
#else
    auto done = m_jit.branchTest64(MacroAssembler::NonZero, MacroAssembler::Address(resultGPR, HashMapBucket<HashMapBucketDataKeyValue>::offsetOfKey()));
#endif
    m_jit.loadPtr(MacroAssembler::Address(resultGPR, HashMapBucket<HashMapBucketDataKeyValue>::offsetOfNext()), resultGPR);
    m_jit.jump().linkTo(loop, &m_jit);

    notBucket.link(&m_jit);
    JSCell* sentinel = nullptr;
    if (node->bucketOwnerType() == BucketOwnerType::Map)
        sentinel = m_jit.vm()->sentinelMapBucket();
    else {
        ASSERT(node->bucketOwnerType() == BucketOwnerType::Set);
        sentinel = m_jit.vm()->sentinelSetBucket();
    }
    m_jit.move(TrustedImmPtr::weakPointer(m_jit.graph(), sentinel), resultGPR);
    done.link(&m_jit);

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileLoadKeyFromMapBucket(Node* node)
{
    SpeculateCellOperand bucket(this, node->child1());
    JSValueRegsTemporary result(this);

    GPRReg bucketGPR = bucket.gpr();
    JSValueRegs resultRegs = result.regs();

    m_jit.loadValue(MacroAssembler::Address(bucketGPR, HashMapBucket<HashMapBucketDataKeyValue>::offsetOfKey()), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileLoadValueFromMapBucket(Node* node)
{
    SpeculateCellOperand bucket(this, node->child1());
    JSValueRegsTemporary result(this);

    GPRReg bucketGPR = bucket.gpr();
    JSValueRegs resultRegs = result.regs();

    m_jit.loadValue(MacroAssembler::Address(bucketGPR, HashMapBucket<HashMapBucketDataKeyValue>::offsetOfValue()), resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileExtractValueFromWeakMapGet(Node* node)
{
    JSValueOperand value(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, value);

    JSValueRegs valueRegs = value.jsValueRegs();
    JSValueRegs resultRegs = result.regs();

#if USE(JSVALUE64)
    m_jit.moveValueRegs(valueRegs, resultRegs);
    auto done = m_jit.branchTestPtr(CCallHelpers::NonZero, resultRegs.payloadGPR());
    m_jit.moveValue(jsUndefined(), resultRegs);
    done.link(&m_jit);
#else
    auto isEmpty = m_jit.branchIfEmpty(valueRegs.tagGPR());
    m_jit.moveValueRegs(valueRegs, resultRegs);
    auto done = m_jit.jump();

    isEmpty.link(&m_jit);
    m_jit.moveValue(jsUndefined(), resultRegs);

    done.link(&m_jit);
#endif

    jsValueResult(resultRegs, node, DataFormatJS);
}

void SpeculativeJIT::compileThrow(Node* node)
{
    JSValueOperand value(this, node->child1());
    JSValueRegs valueRegs = value.jsValueRegs();
    flushRegisters();
    callOperation(operationThrowDFG, valueRegs);
    m_jit.exceptionCheck();
    m_jit.breakpoint();
    noResult(node);
}

void SpeculativeJIT::compileThrowStaticError(Node* node)
{
    SpeculateCellOperand message(this, node->child1());
    GPRReg messageGPR = message.gpr();
    speculateString(node->child1(), messageGPR);
    flushRegisters();
    callOperation(operationThrowStaticError, messageGPR, node->errorType());
    m_jit.exceptionCheck();
    m_jit.breakpoint();
    noResult(node);
}

void SpeculativeJIT::compileGetEnumerableLength(Node* node)
{
    SpeculateCellOperand enumerator(this, node->child1());
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();

    m_jit.load32(MacroAssembler::Address(enumerator.gpr(), JSPropertyNameEnumerator::indexedLengthOffset()), resultGPR);
    int32Result(resultGPR, node);
}

void SpeculativeJIT::compileHasGenericProperty(Node* node)
{
    JSValueOperand base(this, node->child1());
    SpeculateCellOperand property(this, node->child2());

    JSValueRegs baseRegs = base.jsValueRegs();
    GPRReg propertyGPR = property.gpr();

    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationHasGenericProperty, resultRegs, baseRegs, propertyGPR);
    m_jit.exceptionCheck();
    blessedBooleanResult(resultRegs.payloadGPR(), node);
}

void SpeculativeJIT::compileToIndexString(Node* node)
{
    SpeculateInt32Operand index(this, node->child1());
    GPRReg indexGPR = index.gpr();

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationToIndexString, resultGPR, indexGPR);
    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compilePutByIdFlush(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRTemporary scratch(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratchGPR = scratch.gpr();
    flushRegisters();

    cachedPutById(node->origin.semantic, baseGPR, valueRegs, scratchGPR, node->identifierNumber(), NotDirect, MacroAssembler::Jump(), DontSpill);

    noResult(node);
}

void SpeculativeJIT::compilePutById(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRTemporary scratch(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratchGPR = scratch.gpr();

    cachedPutById(node->origin.semantic, baseGPR, valueRegs, scratchGPR, node->identifierNumber(), NotDirect);

    noResult(node);
}

void SpeculativeJIT::compilePutByIdDirect(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    JSValueOperand value(this, node->child2());
    GPRTemporary scratch(this);

    GPRReg baseGPR = base.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratchGPR = scratch.gpr();

    cachedPutById(node->origin.semantic, baseGPR, valueRegs, scratchGPR, node->identifierNumber(), Direct);

    noResult(node);
}

void SpeculativeJIT::compilePutByIdWithThis(Node* node)
{
    JSValueOperand base(this, node->child1());
    JSValueRegs baseRegs = base.jsValueRegs();
    JSValueOperand thisValue(this, node->child2());
    JSValueRegs thisRegs = thisValue.jsValueRegs();
    JSValueOperand value(this, node->child3());
    JSValueRegs valueRegs = value.jsValueRegs();

    flushRegisters();
    callOperation(m_jit.isStrictModeFor(node->origin.semantic) ? operationPutByIdWithThisStrict : operationPutByIdWithThis,
        NoResult, baseRegs, thisRegs, valueRegs, identifierUID(node->identifierNumber()));
    m_jit.exceptionCheck();

    noResult(node);
}

void SpeculativeJIT::compileGetByOffset(Node* node)
{
    StorageOperand storage(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, storage);

    GPRReg storageGPR = storage.gpr();
    JSValueRegs resultRegs = result.regs();

    StorageAccessData& storageAccessData = node->storageAccessData();

    m_jit.loadValue(JITCompiler::Address(storageGPR, offsetRelativeToBase(storageAccessData.offset)), resultRegs);

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compilePutByOffset(Node* node)
{
    StorageOperand storage(this, node->child1());
    JSValueOperand value(this, node->child3());

    GPRReg storageGPR = storage.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();

    speculate(node, node->child2());

    StorageAccessData& storageAccessData = node->storageAccessData();

    m_jit.storeValue(valueRegs, JITCompiler::Address(storageGPR, offsetRelativeToBase(storageAccessData.offset)));

    noResult(node);
}

void SpeculativeJIT::compileMatchStructure(Node* node)
{
    SpeculateCellOperand base(this, node->child1());
    GPRTemporary temp(this);
    GPRReg baseGPR = base.gpr();
    GPRReg tempGPR = temp.gpr();
    
    m_jit.load32(JITCompiler::Address(baseGPR, JSCell::structureIDOffset()), tempGPR);
    
    auto& variants = node->matchStructureData().variants;
    Vector<int64_t> cases;
    for (MatchStructureVariant& variant : variants)
        cases.append(bitwise_cast<int32_t>(variant.structure->id()));
    
    BinarySwitch binarySwitch(tempGPR, cases, BinarySwitch::Int32);
    JITCompiler::JumpList done;
    while (binarySwitch.advance(m_jit)) {
        m_jit.boxBooleanPayload(variants[binarySwitch.caseIndex()].result, tempGPR);
        done.append(m_jit.jump());
    }
    speculationCheck(BadCache, JSValueRegs(), node, binarySwitch.fallThrough());
    
    done.link(&m_jit);
    
    blessedBooleanResult(tempGPR, node);
}

void SpeculativeJIT::compileHasStructureProperty(Node* node)
{
    JSValueOperand base(this, node->child1());
    SpeculateCellOperand property(this, node->child2());
    SpeculateCellOperand enumerator(this, node->child3());
    JSValueRegsTemporary result(this);

    JSValueRegs baseRegs = base.jsValueRegs();
    GPRReg propertyGPR = property.gpr();
    JSValueRegs resultRegs = result.regs();

    CCallHelpers::JumpList wrongStructure;

    wrongStructure.append(m_jit.branchIfNotCell(baseRegs));

    m_jit.load32(MacroAssembler::Address(baseRegs.payloadGPR(), JSCell::structureIDOffset()), resultRegs.payloadGPR());
    wrongStructure.append(m_jit.branch32(MacroAssembler::NotEqual,
        resultRegs.payloadGPR(),
        MacroAssembler::Address(enumerator.gpr(), JSPropertyNameEnumerator::cachedStructureIDOffset())));

    moveTrueTo(resultRegs.payloadGPR());
    MacroAssembler::Jump done = m_jit.jump();

    done.link(&m_jit);

    addSlowPathGenerator(slowPathCall(wrongStructure, this, operationHasGenericProperty, resultRegs, baseRegs, propertyGPR));
    blessedBooleanResult(resultRegs.payloadGPR(), node);
}

void SpeculativeJIT::compileGetPropertyEnumerator(Node* node)
{
    if (node->child1().useKind() == CellUse) {
        SpeculateCellOperand base(this, node->child1());
        GPRReg baseGPR = base.gpr();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationGetPropertyEnumeratorCell, resultGPR, baseGPR);
        m_jit.exceptionCheck();
        cellResult(resultGPR, node);
        return;
    }

    JSValueOperand base(this, node->child1());
    JSValueRegs baseRegs = base.jsValueRegs();

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationGetPropertyEnumerator, resultGPR, baseRegs);
    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetEnumeratorPname(Node* node)
{
    ASSERT(node->op() == GetEnumeratorStructurePname || node->op() == GetEnumeratorGenericPname);
    SpeculateCellOperand enumerator(this, node->child1());
    SpeculateStrictInt32Operand index(this, node->child2());
    GPRTemporary scratch(this);
    JSValueRegsTemporary result(this);

    GPRReg enumeratorGPR = enumerator.gpr();
    GPRReg indexGPR = index.gpr();
    GPRReg scratchGPR = scratch.gpr();
    JSValueRegs resultRegs = result.regs();

    MacroAssembler::Jump inBounds = m_jit.branch32(MacroAssembler::Below, indexGPR,
        MacroAssembler::Address(enumeratorGPR, (node->op() == GetEnumeratorStructurePname)
            ? JSPropertyNameEnumerator::endStructurePropertyIndexOffset()
            : JSPropertyNameEnumerator::endGenericPropertyIndexOffset()));

    m_jit.moveValue(jsNull(), resultRegs);

    MacroAssembler::Jump done = m_jit.jump();
    inBounds.link(&m_jit);

    m_jit.loadPtr(MacroAssembler::Address(enumeratorGPR, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset()), scratchGPR);
    m_jit.loadPtr(MacroAssembler::BaseIndex(scratchGPR, indexGPR, MacroAssembler::ScalePtr), resultRegs.payloadGPR());
#if USE(JSVALUE32_64)
    m_jit.move(MacroAssembler::TrustedImm32(JSValue::CellTag), resultRegs.tagGPR());
#endif

    done.link(&m_jit);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileGetExecutable(Node* node)
{
    SpeculateCellOperand function(this, node->child1());
    GPRTemporary result(this, Reuse, function);
    GPRReg functionGPR = function.gpr();
    GPRReg resultGPR = result.gpr();
    speculateCellType(node->child1(), functionGPR, SpecFunction, JSFunctionType);
    m_jit.loadPtr(JITCompiler::Address(functionGPR, JSFunction::offsetOfExecutable()), resultGPR);
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetGetter(Node* node)
{
    SpeculateCellOperand op1(this, node->child1());
    GPRTemporary result(this, Reuse, op1);

    GPRReg op1GPR = op1.gpr();
    GPRReg resultGPR = result.gpr();

    m_jit.loadPtr(JITCompiler::Address(op1GPR, GetterSetter::offsetOfGetter()), resultGPR);

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetSetter(Node* node)
{
    SpeculateCellOperand op1(this, node->child1());
    GPRTemporary result(this, Reuse, op1);

    GPRReg op1GPR = op1.gpr();
    GPRReg resultGPR = result.gpr();

    m_jit.loadPtr(JITCompiler::Address(op1GPR, GetterSetter::offsetOfSetter()), resultGPR);

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileGetCallee(Node* node)
{
    GPRTemporary result(this);
    m_jit.loadPtr(JITCompiler::payloadFor(CallFrameSlot::callee), result.gpr());
    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileSetCallee(Node* node)
{
    SpeculateCellOperand callee(this, node->child1());
    m_jit.storeCell(callee.gpr(), JITCompiler::payloadFor(CallFrameSlot::callee));
    noResult(node);
}

void SpeculativeJIT::compileGetArgumentCountIncludingThis(Node* node)
{
    GPRTemporary result(this);
    VirtualRegister argumentCountRegister;
    if (InlineCallFrame* inlineCallFrame = node->argumentsInlineCallFrame())
        argumentCountRegister = inlineCallFrame->argumentCountRegister;
    else
        argumentCountRegister = VirtualRegister(CallFrameSlot::argumentCount);
    m_jit.load32(JITCompiler::payloadFor(argumentCountRegister), result.gpr());
    int32Result(result.gpr(), node);
}

void SpeculativeJIT::compileSetArgumentCountIncludingThis(Node* node)
{
    m_jit.store32(TrustedImm32(node->argumentCountIncludingThis()), JITCompiler::payloadFor(CallFrameSlot::argumentCount));
    noResult(node);
}

void SpeculativeJIT::compileStrCat(Node* node)
{
    JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand op2(this, node->child2(), ManualOperandSpeculation);
    JSValueOperand op3(this, node->child3(), ManualOperandSpeculation);

    JSValueRegs op1Regs = op1.jsValueRegs();
    JSValueRegs op2Regs = op2.jsValueRegs();
    JSValueRegs op3Regs;

    if (node->child3())
        op3Regs = op3.jsValueRegs();

    flushRegisters();

    GPRFlushedCallResult result(this);
    if (node->child3())
        callOperation(operationStrCat3, result.gpr(), op1Regs, op2Regs, op3Regs);
    else
        callOperation(operationStrCat2, result.gpr(), op1Regs, op2Regs);
    m_jit.exceptionCheck();

    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileNewArrayBuffer(Node* node)
{
    JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
    auto* array = node->castOperand<JSImmutableButterfly*>();

    IndexingType indexingMode = node->indexingMode();
    RegisteredStructure structure = m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingMode));

    if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(indexingMode)) {
        GPRTemporary result(this);
        GPRTemporary scratch1(this);
        GPRTemporary scratch2(this);

        GPRReg resultGPR = result.gpr();
        GPRReg scratch1GPR = scratch1.gpr();
        GPRReg scratch2GPR = scratch2.gpr();

        MacroAssembler::JumpList slowCases;

        emitAllocateJSObject<JSArray>(resultGPR, TrustedImmPtr(structure), TrustedImmPtr(array->toButterfly()), scratch1GPR, scratch2GPR, slowCases);

        addSlowPathGenerator(slowPathCall(slowCases, this, operationNewArrayBuffer, result.gpr(), structure, array));

        DFG_ASSERT(m_jit.graph(), node, indexingMode & IsArray, indexingMode);
        cellResult(resultGPR, node);
        return;
    }

    flushRegisters();
    GPRFlushedCallResult result(this);

    callOperation(operationNewArrayBuffer, result.gpr(), structure, TrustedImmPtr(node->cellOperand()));
    m_jit.exceptionCheck();

    cellResult(result.gpr(), node);
}

void SpeculativeJIT::compileNewArrayWithSize(Node* node)
{
    JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
    if (!globalObject->isHavingABadTime() && !hasAnyArrayStorage(node->indexingType())) {
        SpeculateStrictInt32Operand size(this, node->child1());
        GPRTemporary result(this);

        GPRReg sizeGPR = size.gpr();
        GPRReg resultGPR = result.gpr();

        compileAllocateNewArrayWithSize(globalObject, resultGPR, sizeGPR, node->indexingType());
        cellResult(resultGPR, node);
        return;
    }

    SpeculateStrictInt32Operand size(this, node->child1());
    GPRReg sizeGPR = size.gpr();
    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    GPRReg structureGPR = AssemblyHelpers::selectScratchGPR(sizeGPR);
    MacroAssembler::Jump bigLength = m_jit.branch32(MacroAssembler::AboveOrEqual, sizeGPR, TrustedImm32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH));
    m_jit.move(TrustedImmPtr(m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(node->indexingType()))), structureGPR);
    MacroAssembler::Jump done = m_jit.jump();
    bigLength.link(&m_jit);
    m_jit.move(TrustedImmPtr(m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage))), structureGPR);
    done.link(&m_jit);
    callOperation(operationNewArrayWithSize, resultGPR, structureGPR, sizeGPR, nullptr);
    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewTypedArray(Node* node)
{
    switch (node->child1().useKind()) {
    case Int32Use:
        compileNewTypedArrayWithSize(node);
        break;
    case UntypedUse: {
        JSValueOperand argument(this, node->child1());
        JSValueRegs argumentRegs = argument.jsValueRegs();

        flushRegisters();

        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
        callOperation(
            operationNewTypedArrayWithOneArgumentForType(node->typedArrayType()),
            resultGPR, m_jit.graph().registerStructure(globalObject->typedArrayStructureConcurrently(node->typedArrayType())), argumentRegs);
        m_jit.exceptionCheck();

        cellResult(resultGPR, node);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileToThis(Node* node)
{
    ASSERT(node->child1().useKind() == UntypedUse);
    JSValueOperand thisValue(this, node->child1());
    JSValueRegsTemporary temp(this);

    JSValueRegs thisValueRegs = thisValue.jsValueRegs();
    JSValueRegs tempRegs = temp.regs();

    MacroAssembler::JumpList slowCases;
    slowCases.append(m_jit.branchIfNotCell(thisValueRegs));
    slowCases.append(
        m_jit.branchTest8(
            MacroAssembler::NonZero,
            MacroAssembler::Address(thisValueRegs.payloadGPR(), JSCell::typeInfoFlagsOffset()),
            MacroAssembler::TrustedImm32(OverridesToThis)));
    m_jit.moveValueRegs(thisValueRegs, tempRegs);

    J_JITOperation_EJ function;
    if (m_jit.graph().executableFor(node->origin.semantic)->isStrictMode())
        function = operationToThisStrict;
    else
        function = operationToThis;
    addSlowPathGenerator(slowPathCall(slowCases, this, function, tempRegs, thisValueRegs));

    jsValueResult(tempRegs, node);
}

void SpeculativeJIT::compileObjectKeys(Node* node)
{
    switch (node->child1().useKind()) {
    case ObjectUse: {
        if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
            SpeculateCellOperand object(this, node->child1());
            GPRTemporary structure(this);
            GPRTemporary scratch(this);
            GPRTemporary scratch2(this);
            GPRTemporary scratch3(this);
            GPRTemporary result(this);

            GPRReg objectGPR = object.gpr();
            GPRReg structureGPR = structure.gpr();
            GPRReg scratchGPR = scratch.gpr();
            GPRReg scratch2GPR = scratch2.gpr();
            GPRReg scratch3GPR = scratch3.gpr();
            GPRReg resultGPR = result.gpr();

            speculateObject(node->child1(), objectGPR);

            CCallHelpers::JumpList slowCases;
            m_jit.emitLoadStructure(*m_jit.vm(), objectGPR, structureGPR, scratchGPR);
            m_jit.loadPtr(CCallHelpers::Address(structureGPR, Structure::previousOrRareDataOffset()), scratchGPR);

            slowCases.append(m_jit.branchTestPtr(CCallHelpers::Zero, scratchGPR));
            slowCases.append(m_jit.branch32(CCallHelpers::Equal, CCallHelpers::Address(scratchGPR, JSCell::structureIDOffset()), TrustedImm32(bitwise_cast<int32_t>(m_jit.vm()->structureStructure->structureID()))));

            m_jit.loadPtr(CCallHelpers::Address(scratchGPR, StructureRareData::offsetOfCachedOwnKeys()), scratchGPR);

            ASSERT(bitwise_cast<uintptr_t>(StructureRareData::cachedOwnKeysSentinel()) == 1);
            slowCases.append(m_jit.branchPtr(CCallHelpers::BelowOrEqual, scratchGPR, TrustedImmPtr(bitwise_cast<void*>(StructureRareData::cachedOwnKeysSentinel()))));

            MacroAssembler::JumpList slowButArrayBufferCases;

            JSGlobalObject* globalObject = m_jit.graph().globalObjectFor(node->origin.semantic);
            RegisteredStructure arrayStructure = m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(CopyOnWriteArrayWithContiguous));

            m_jit.move(scratchGPR, scratch3GPR);
            m_jit.addPtr(TrustedImmPtr(JSImmutableButterfly::offsetOfData()), scratchGPR);

            emitAllocateJSObject<JSArray>(resultGPR, TrustedImmPtr(arrayStructure), scratchGPR, structureGPR, scratch2GPR, slowButArrayBufferCases);

            addSlowPathGenerator(slowPathCall(slowButArrayBufferCases, this, operationNewArrayBuffer, resultGPR, arrayStructure, scratch3GPR));

            addSlowPathGenerator(slowPathCall(slowCases, this, operationObjectKeysObject, resultGPR, objectGPR));

            cellResult(resultGPR, node);
            break;
        }

        SpeculateCellOperand object(this, node->child1());

        GPRReg objectGPR = object.gpr();

        speculateObject(node->child1(), objectGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationObjectKeysObject, resultGPR, objectGPR);
        m_jit.exceptionCheck();

        cellResult(resultGPR, node);
        break;
    }

    case UntypedUse: {
        JSValueOperand object(this, node->child1());

        JSValueRegs objectRegs = object.jsValueRegs();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationObjectKeys, resultGPR, objectRegs);
        m_jit.exceptionCheck();

        cellResult(resultGPR, node);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileObjectCreate(Node* node)
{
    switch (node->child1().useKind()) {
    case ObjectUse: {
        SpeculateCellOperand prototype(this, node->child1());

        GPRReg prototypeGPR = prototype.gpr();

        speculateObject(node->child1(), prototypeGPR);

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationObjectCreateObject, resultGPR, prototypeGPR);
        m_jit.exceptionCheck();

        cellResult(resultGPR, node);
        break;
    }

    case UntypedUse: {
        JSValueOperand prototype(this, node->child1());

        JSValueRegs prototypeRegs = prototype.jsValueRegs();

        flushRegisters();
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();
        callOperation(operationObjectCreate, resultGPR, prototypeRegs);
        m_jit.exceptionCheck();

        cellResult(resultGPR, node);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void SpeculativeJIT::compileCreateThis(Node* node)
{
    // Note that there is not so much profit to speculate here. The only things we
    // speculate on are (1) that it's a cell, since that eliminates cell checks
    // later if the proto is reused, and (2) if we have a FinalObject prediction
    // then we speculate because we want to get recompiled if it isn't (since
    // otherwise we'd start taking slow path a lot).

    SpeculateCellOperand callee(this, node->child1());
    GPRTemporary result(this);
    GPRTemporary allocator(this);
    GPRTemporary structure(this);
    GPRTemporary scratch(this);

    GPRReg calleeGPR = callee.gpr();
    GPRReg resultGPR = result.gpr();
    GPRReg allocatorGPR = allocator.gpr();
    GPRReg structureGPR = structure.gpr();
    GPRReg scratchGPR = scratch.gpr();
    // Rare data is only used to access the allocator & structure
    // We can avoid using an additional GPR this way
    GPRReg rareDataGPR = structureGPR;
    GPRReg inlineCapacityGPR = rareDataGPR;

    MacroAssembler::JumpList slowPath;

    slowPath.append(m_jit.branchIfNotFunction(calleeGPR));
    m_jit.loadPtr(JITCompiler::Address(calleeGPR, JSFunction::offsetOfRareData()), rareDataGPR);
    slowPath.append(m_jit.branchTestPtr(MacroAssembler::Zero, rareDataGPR));
    m_jit.loadPtr(JITCompiler::Address(rareDataGPR, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfile::offsetOfAllocator()), allocatorGPR);
    m_jit.loadPtr(JITCompiler::Address(rareDataGPR, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfile::offsetOfStructure()), structureGPR);

    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject(resultGPR, JITAllocator::variable(), allocatorGPR, structureGPR, butterfly, scratchGPR, slowPath);

    m_jit.loadPtr(JITCompiler::Address(calleeGPR, JSFunction::offsetOfRareData()), rareDataGPR);
    m_jit.load32(JITCompiler::Address(rareDataGPR, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfile::offsetOfInlineCapacity()), inlineCapacityGPR);
    m_jit.emitInitializeInlineStorage(resultGPR, inlineCapacityGPR);
    m_jit.mutatorFence(*m_jit.vm());

    addSlowPathGenerator(slowPathCall(slowPath, this, operationCreateThis, resultGPR, calleeGPR, node->inlineCapacity()));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileNewObject(Node* node)
{
    GPRTemporary result(this);
    GPRTemporary allocator(this);
    GPRTemporary scratch(this);

    GPRReg resultGPR = result.gpr();
    GPRReg allocatorGPR = allocator.gpr();
    GPRReg scratchGPR = scratch.gpr();

    MacroAssembler::JumpList slowPath;

    RegisteredStructure structure = node->structure();
    size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
    Allocator allocatorValue = allocatorForNonVirtualConcurrently<JSFinalObject>(*m_jit.vm(), allocationSize, AllocatorForMode::AllocatorIfExists);
    if (!allocatorValue)
        slowPath.append(m_jit.jump());
    else {
        auto butterfly = TrustedImmPtr(nullptr);
        emitAllocateJSObject(resultGPR, JITAllocator::constant(allocatorValue), allocatorGPR, TrustedImmPtr(structure), butterfly, scratchGPR, slowPath);
        m_jit.emitInitializeInlineStorage(resultGPR, structure->inlineCapacity());
        m_jit.mutatorFence(*m_jit.vm());
    }

    addSlowPathGenerator(slowPathCall(slowPath, this, operationNewObject, resultGPR, structure));

    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileToPrimitive(Node* node)
{
    DFG_ASSERT(m_jit.graph(), node, node->child1().useKind() == UntypedUse, node->child1().useKind());
    JSValueOperand argument(this, node->child1());
    JSValueRegsTemporary result(this, Reuse, argument);

    JSValueRegs argumentRegs = argument.jsValueRegs();
    JSValueRegs resultRegs = result.regs();

    argument.use();

    MacroAssembler::Jump alreadyPrimitive = m_jit.branchIfNotCell(argumentRegs);
    MacroAssembler::Jump notPrimitive = m_jit.branchIfObject(argumentRegs.payloadGPR());

    alreadyPrimitive.link(&m_jit);
    m_jit.moveValueRegs(argumentRegs, resultRegs);

    addSlowPathGenerator(slowPathCall(notPrimitive, this, operationToPrimitive, resultRegs, argumentRegs));

    jsValueResult(resultRegs, node, DataFormatJS, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileLogShadowChickenPrologue(Node* node)
{
    flushRegisters();
    prepareForExternalCall();
    m_jit.emitStoreCodeOrigin(node->origin.semantic);

    GPRTemporary scratch1(this, GPRInfo::nonArgGPR0); // This must be a non-argument GPR.
    GPRReg scratch1Reg = scratch1.gpr();
    GPRTemporary scratch2(this);
    GPRReg scratch2Reg = scratch2.gpr();
    GPRTemporary shadowPacket(this);
    GPRReg shadowPacketReg = shadowPacket.gpr();

    m_jit.ensureShadowChickenPacket(*m_jit.vm(), shadowPacketReg, scratch1Reg, scratch2Reg);

    SpeculateCellOperand scope(this, node->child1());
    GPRReg scopeReg = scope.gpr();

    m_jit.logShadowChickenProloguePacket(shadowPacketReg, scratch1Reg, scopeReg);
    noResult(node);
}

void SpeculativeJIT::compileLogShadowChickenTail(Node* node)
{
    flushRegisters();
    prepareForExternalCall();
    CallSiteIndex callSiteIndex = m_jit.emitStoreCodeOrigin(node->origin.semantic);

    GPRTemporary scratch1(this, GPRInfo::nonArgGPR0); // This must be a non-argument GPR.
    GPRReg scratch1Reg = scratch1.gpr();
    GPRTemporary scratch2(this);
    GPRReg scratch2Reg = scratch2.gpr();
    GPRTemporary shadowPacket(this);
    GPRReg shadowPacketReg = shadowPacket.gpr();

    m_jit.ensureShadowChickenPacket(*m_jit.vm(), shadowPacketReg, scratch1Reg, scratch2Reg);

    JSValueOperand thisValue(this, node->child1());
    JSValueRegs thisRegs = thisValue.jsValueRegs();
    SpeculateCellOperand scope(this, node->child2());
    GPRReg scopeReg = scope.gpr();

    m_jit.logShadowChickenTailPacket(shadowPacketReg, thisRegs, scopeReg, m_jit.codeBlock(), callSiteIndex);
    noResult(node);
}

void SpeculativeJIT::compileSetAdd(Node* node)
{
    SpeculateCellOperand set(this, node->child1());
    JSValueOperand key(this, node->child2());
    SpeculateInt32Operand hash(this, node->child3());

    GPRReg setGPR = set.gpr();
    JSValueRegs keyRegs = key.jsValueRegs();
    GPRReg hashGPR = hash.gpr();

    speculateSetObject(node->child1(), setGPR);

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationSetAdd, resultGPR, setGPR, keyRegs, hashGPR);
    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileMapSet(Node* node)
{
    SpeculateCellOperand map(this, m_jit.graph().varArgChild(node, 0));
    JSValueOperand key(this, m_jit.graph().varArgChild(node, 1));
    JSValueOperand value(this, m_jit.graph().varArgChild(node, 2));
    SpeculateInt32Operand hash(this, m_jit.graph().varArgChild(node, 3));

    GPRReg mapGPR = map.gpr();
    JSValueRegs keyRegs = key.jsValueRegs();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg hashGPR = hash.gpr();

    speculateMapObject(m_jit.graph().varArgChild(node, 0), mapGPR);

    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    callOperation(operationMapSet, resultGPR, mapGPR, keyRegs, valueRegs, hashGPR);
    m_jit.exceptionCheck();
    cellResult(resultGPR, node);
}

void SpeculativeJIT::compileWeakMapGet(Node* node)
{
    GPRTemporary mask(this);
    GPRTemporary buffer(this);
    JSValueRegsTemporary result(this);

    GPRReg maskGPR = mask.gpr();
    GPRReg bufferGPR = buffer.gpr();
    JSValueRegs resultRegs = result.regs();

    GPRTemporary index;
    GPRReg indexGPR { InvalidGPRReg };
    {
        SpeculateInt32Operand hash(this, node->child3());
        GPRReg hashGPR = hash.gpr();
        index = GPRTemporary(this, Reuse, hash);
        indexGPR = index.gpr();
        m_jit.move(hashGPR, indexGPR);
    }

    {
        SpeculateCellOperand weakMap(this, node->child1());
        GPRReg weakMapGPR = weakMap.gpr();
        if (node->child1().useKind() == WeakMapObjectUse)
            speculateWeakMapObject(node->child1(), weakMapGPR);
        else
            speculateWeakSetObject(node->child1(), weakMapGPR);

        ASSERT(WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfCapacity() == WeakMapImpl<WeakMapBucket<WeakMapBucketDataKeyValue>>::offsetOfCapacity());
        ASSERT(WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfBuffer() == WeakMapImpl<WeakMapBucket<WeakMapBucketDataKeyValue>>::offsetOfBuffer());
        m_jit.load32(MacroAssembler::Address(weakMapGPR, WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfCapacity()), maskGPR);
        m_jit.loadPtr(MacroAssembler::Address(weakMapGPR, WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfBuffer()), bufferGPR);
    }

    SpeculateCellOperand key(this, node->child2());
    GPRReg keyGPR = key.gpr();
    speculateObject(node->child2(), keyGPR);

#if USE(JSVALUE32_64)
    GPRReg bucketGPR = resultRegs.tagGPR();
#else
    GPRTemporary bucket(this);
    GPRReg bucketGPR = bucket.gpr();
#endif

    m_jit.sub32(TrustedImm32(1), maskGPR);

    MacroAssembler::Label loop = m_jit.label();
    m_jit.and32(maskGPR, indexGPR);
    if (node->child1().useKind() == WeakSetObjectUse) {
        static_assert(sizeof(WeakMapBucket<WeakMapBucketDataKey>) == sizeof(void*), "");
        m_jit.zeroExtend32ToPtr(indexGPR, bucketGPR);
        m_jit.lshiftPtr(MacroAssembler::Imm32(sizeof(void*) == 4 ? 2 : 3), bucketGPR);
        m_jit.addPtr(bufferGPR, bucketGPR);
    } else {
        ASSERT(node->child1().useKind() == WeakMapObjectUse);
        static_assert(sizeof(WeakMapBucket<WeakMapBucketDataKeyValue>) == 16, "");
        m_jit.zeroExtend32ToPtr(indexGPR, bucketGPR);
        m_jit.lshiftPtr(MacroAssembler::Imm32(4), bucketGPR);
        m_jit.addPtr(bufferGPR, bucketGPR);
    }

    m_jit.loadPtr(MacroAssembler::Address(bucketGPR, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfKey()), resultRegs.payloadGPR());

    // They're definitely the same value, we found the bucket we were looking for!
    // The deleted key comparison is also done with this.
    auto found = m_jit.branchPtr(MacroAssembler::Equal, resultRegs.payloadGPR(), keyGPR);

    auto notPresentInTable = m_jit.branchTestPtr(MacroAssembler::Zero, resultRegs.payloadGPR());

    m_jit.add32(TrustedImm32(1), indexGPR);
    m_jit.jump().linkTo(loop, &m_jit);

#if USE(JSVALUE32_64)
    notPresentInTable.link(&m_jit);
    m_jit.moveValue(JSValue(), resultRegs);
    auto notPresentInTableDone = m_jit.jump();

    found.link(&m_jit);
    if (node->child1().useKind() == WeakSetObjectUse)
        m_jit.move(TrustedImm32(JSValue::CellTag), resultRegs.tagGPR());
    else
        m_jit.loadValue(MacroAssembler::Address(bucketGPR, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfValue()), resultRegs);

    notPresentInTableDone.link(&m_jit);
#else
    notPresentInTable.link(&m_jit);
    found.link(&m_jit);

    // In 64bit environment, Empty bucket has JSEmpty value. Empty key is JSEmpty.
    // If empty bucket is found, we can use the same path used for the case of finding a bucket.
    if (node->child1().useKind() == WeakMapObjectUse)
        m_jit.loadValue(MacroAssembler::Address(bucketGPR, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfValue()), resultRegs);
#endif

    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileWeakSetAdd(Node* node)
{
    SpeculateCellOperand set(this, node->child1());
    SpeculateCellOperand key(this, node->child2());
    SpeculateInt32Operand hash(this, node->child3());

    GPRReg setGPR = set.gpr();
    GPRReg keyGPR = key.gpr();
    GPRReg hashGPR = hash.gpr();

    speculateWeakSetObject(node->child1(), setGPR);
    speculateObject(node->child2(), keyGPR);

    flushRegisters();
    callOperation(operationWeakSetAdd, setGPR, keyGPR, hashGPR);
    m_jit.exceptionCheck();
    noResult(node);
}

void SpeculativeJIT::compileWeakMapSet(Node* node)
{
    SpeculateCellOperand map(this, m_jit.graph().varArgChild(node, 0));
    SpeculateCellOperand key(this, m_jit.graph().varArgChild(node, 1));
    JSValueOperand value(this, m_jit.graph().varArgChild(node, 2));
    SpeculateInt32Operand hash(this, m_jit.graph().varArgChild(node, 3));

    GPRReg mapGPR = map.gpr();
    GPRReg keyGPR = key.gpr();
    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg hashGPR = hash.gpr();

    speculateWeakMapObject(m_jit.graph().varArgChild(node, 0), mapGPR);
    speculateObject(m_jit.graph().varArgChild(node, 1), keyGPR);

    flushRegisters();
    callOperation(operationWeakMapSet, mapGPR, keyGPR, valueRegs, hashGPR);
    m_jit.exceptionCheck();
    noResult(node);
}

void SpeculativeJIT::compileGetPrototypeOf(Node* node)
{
    switch (node->child1().useKind()) {
    case ArrayUse:
    case FunctionUse:
    case FinalObjectUse: {
        SpeculateCellOperand object(this, node->child1());
        GPRTemporary temp(this);
        GPRTemporary temp2(this);

        GPRReg objectGPR = object.gpr();
        GPRReg tempGPR = temp.gpr();
        GPRReg temp2GPR = temp2.gpr();

        switch (node->child1().useKind()) {
        case ArrayUse:
            speculateArray(node->child1(), objectGPR);
            break;
        case FunctionUse:
            speculateFunction(node->child1(), objectGPR);
            break;
        case FinalObjectUse:
            speculateFinalObject(node->child1(), objectGPR);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }

        m_jit.emitLoadStructure(*m_jit.vm(), objectGPR, tempGPR, temp2GPR);

        AbstractValue& value = m_state.forNode(node->child1());
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
#if USE(JSVALUE64)
                m_jit.load64(MacroAssembler::Address(tempGPR, Structure::prototypeOffset()), tempGPR);
                jsValueResult(tempGPR, node);
#else
                m_jit.load32(MacroAssembler::Address(tempGPR, Structure::prototypeOffset() + TagOffset), temp2GPR);
                m_jit.load32(MacroAssembler::Address(tempGPR, Structure::prototypeOffset() + PayloadOffset), tempGPR);
                jsValueResult(temp2GPR, tempGPR, node);
#endif
                return;
            }

            if (hasPolyProto && !hasMonoProto) {
#if USE(JSVALUE64)
                m_jit.load64(JITCompiler::Address(objectGPR, offsetRelativeToBase(knownPolyProtoOffset)), tempGPR);
                jsValueResult(tempGPR, node);
#else
                m_jit.load32(JITCompiler::Address(objectGPR, offsetRelativeToBase(knownPolyProtoOffset) + TagOffset), temp2GPR);
                m_jit.load32(JITCompiler::Address(objectGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset), tempGPR);
                jsValueResult(temp2GPR, tempGPR, node);
#endif
                return;
            }
        }

#if USE(JSVALUE64)
        m_jit.load64(MacroAssembler::Address(tempGPR, Structure::prototypeOffset()), tempGPR);
        auto hasMonoProto = m_jit.branchIfNotEmpty(tempGPR);
        m_jit.load64(JITCompiler::Address(objectGPR, offsetRelativeToBase(knownPolyProtoOffset)), tempGPR);
        hasMonoProto.link(&m_jit);
        jsValueResult(tempGPR, node);
#else
        m_jit.load32(MacroAssembler::Address(tempGPR, Structure::prototypeOffset() + TagOffset), temp2GPR);
        m_jit.load32(MacroAssembler::Address(tempGPR, Structure::prototypeOffset() + PayloadOffset), tempGPR);
        auto hasMonoProto = m_jit.branchIfNotEmpty(temp2GPR);
        m_jit.load32(JITCompiler::Address(objectGPR, offsetRelativeToBase(knownPolyProtoOffset) + TagOffset), temp2GPR);
        m_jit.load32(JITCompiler::Address(objectGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset), tempGPR);
        hasMonoProto.link(&m_jit);
        jsValueResult(temp2GPR, tempGPR, node);
#endif
        return;
    }
    case ObjectUse: {
        SpeculateCellOperand value(this, node->child1());
        JSValueRegsTemporary result(this);

        GPRReg valueGPR = value.gpr();
        JSValueRegs resultRegs = result.regs();

        speculateObject(node->child1(), valueGPR);

        flushRegisters();
        callOperation(operationGetPrototypeOfObject, resultRegs, valueGPR);
        m_jit.exceptionCheck();
        jsValueResult(resultRegs, node);
        return;
    }
    default: {
        JSValueOperand value(this, node->child1());
        JSValueRegsTemporary result(this);

        JSValueRegs valueRegs = value.jsValueRegs();
        JSValueRegs resultRegs = result.regs();

        flushRegisters();
        callOperation(operationGetPrototypeOf, resultRegs, valueRegs);
        m_jit.exceptionCheck();
        jsValueResult(resultRegs, node);
        return;
    }
    }
}

void SpeculativeJIT::compileIdentity(Node* node)
{
    speculate(node, node->child1());
    switch (node->child1().useKind()) {
#if USE(JSVALUE64)
    case DoubleRepAnyIntUse:
#endif
    case DoubleRepUse:
    case DoubleRepRealUse: {
        SpeculateDoubleOperand op(this, node->child1());
        FPRTemporary scratch(this, op);
        m_jit.moveDouble(op.fpr(), scratch.fpr());
        doubleResult(scratch.fpr(), node);
        break;
    }
#if USE(JSVALUE64)
    case Int52RepUse: {
        SpeculateInt52Operand op(this, node->child1());
        GPRTemporary result(this, Reuse, op);
        m_jit.move(op.gpr(), result.gpr());
        int52Result(result.gpr(), node);
        break;
    }
#endif
    default: {
        JSValueOperand op(this, node->child1(), ManualOperandSpeculation);
        JSValueRegsTemporary result(this, Reuse, op);
        JSValueRegs opRegs = op.jsValueRegs();
        JSValueRegs resultRegs = result.regs();
        m_jit.moveValueRegs(opRegs, resultRegs);
        jsValueResult(resultRegs, node);
        break;
    }
    }
}

void SpeculativeJIT::compileMiscStrictEq(Node* node)
{
    JSValueOperand op1(this, node->child1(), ManualOperandSpeculation);
    JSValueOperand op2(this, node->child2(), ManualOperandSpeculation);
    GPRTemporary result(this);

    if (node->child1().useKind() == MiscUse)
        speculateMisc(node->child1(), op1.jsValueRegs());
    if (node->child2().useKind() == MiscUse)
        speculateMisc(node->child2(), op2.jsValueRegs());

#if USE(JSVALUE64)
    m_jit.compare64(JITCompiler::Equal, op1.gpr(), op2.gpr(), result.gpr());
#else
    m_jit.move(TrustedImm32(0), result.gpr());
    JITCompiler::Jump notEqual = m_jit.branch32(JITCompiler::NotEqual, op1.tagGPR(), op2.tagGPR());
    m_jit.compare32(JITCompiler::Equal, op1.payloadGPR(), op2.payloadGPR(), result.gpr());
    notEqual.link(&m_jit);
#endif
    unblessedBooleanResult(result.gpr(), node);
}

void SpeculativeJIT::emitInitializeButterfly(GPRReg storageGPR, GPRReg sizeGPR, JSValueRegs emptyValueRegs, GPRReg scratchGPR)
{
    m_jit.zeroExtend32ToPtr(sizeGPR, scratchGPR);
    MacroAssembler::Jump done = m_jit.branchTest32(MacroAssembler::Zero, scratchGPR);
    MacroAssembler::Label loop = m_jit.label();
    m_jit.sub32(TrustedImm32(1), scratchGPR);
    m_jit.storeValue(emptyValueRegs, MacroAssembler::BaseIndex(storageGPR, scratchGPR, MacroAssembler::TimesEight));
    m_jit.branchTest32(MacroAssembler::NonZero, scratchGPR).linkTo(loop, &m_jit);
    done.link(&m_jit);
}

void SpeculativeJIT::compileAllocateNewArrayWithSize(JSGlobalObject* globalObject, GPRReg resultGPR, GPRReg sizeGPR, IndexingType indexingType, bool shouldConvertLargeSizeToArrayStorage)
{
    GPRTemporary storage(this);
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);

    GPRReg storageGPR = storage.gpr();
    GPRReg scratchGPR = scratch.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

    m_jit.move(TrustedImmPtr(nullptr), storageGPR);

    MacroAssembler::JumpList slowCases;
    if (shouldConvertLargeSizeToArrayStorage)
        slowCases.append(m_jit.branch32(MacroAssembler::AboveOrEqual, sizeGPR, TrustedImm32(MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH)));

    // We can use resultGPR as a scratch right now.
    emitAllocateButterfly(storageGPR, sizeGPR, scratchGPR, scratch2GPR, resultGPR, slowCases);

#if USE(JSVALUE64)
    JSValueRegs emptyValueRegs(scratchGPR);
    if (hasDouble(indexingType))
        m_jit.move(TrustedImm64(bitwise_cast<int64_t>(PNaN)), emptyValueRegs.gpr());
    else
        m_jit.move(TrustedImm64(JSValue::encode(JSValue())), emptyValueRegs.gpr());
#else
    JSValueRegs emptyValueRegs(scratchGPR, scratch2GPR);
    if (hasDouble(indexingType))
        m_jit.moveValue(JSValue(JSValue::EncodeAsDouble, PNaN), emptyValueRegs);
    else
        m_jit.moveValue(JSValue(), emptyValueRegs);
#endif
    emitInitializeButterfly(storageGPR, sizeGPR, emptyValueRegs, resultGPR);

    RegisteredStructure structure = m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingType));

    emitAllocateJSObject<JSArray>(resultGPR, TrustedImmPtr(structure), storageGPR, scratchGPR, scratch2GPR, slowCases);

    m_jit.mutatorFence(*m_jit.vm());

    addSlowPathGenerator(std::make_unique<CallArrayAllocatorWithVariableSizeSlowPathGenerator>(
        slowCases, this, operationNewArrayWithSize, resultGPR,
        structure,
        shouldConvertLargeSizeToArrayStorage ? m_jit.graph().registerStructure(globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithArrayStorage)) : structure,
        sizeGPR, storageGPR));
}

void SpeculativeJIT::compileHasIndexedProperty(Node* node)
{
    SpeculateCellOperand base(this, m_graph.varArgChild(node, 0));
    SpeculateStrictInt32Operand index(this, m_graph.varArgChild(node, 1));
    GPRTemporary result(this);

    GPRReg baseGPR = base.gpr();
    GPRReg indexGPR = index.gpr();
    GPRReg resultGPR = result.gpr();

    MacroAssembler::JumpList slowCases;
    ArrayMode mode = node->arrayMode();
    switch (mode.type()) {
    case Array::Int32:
    case Array::Contiguous: {
        ASSERT(!!m_graph.varArgChild(node, 2));
        StorageOperand storage(this, m_graph.varArgChild(node, 2));
        GPRTemporary scratch(this);

        GPRReg storageGPR = storage.gpr();
        GPRReg scratchGPR = scratch.gpr();

        MacroAssembler::Jump outOfBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, indexGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
        if (mode.isInBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, outOfBounds);
        else
            slowCases.append(outOfBounds);

#if USE(JSVALUE64)
        m_jit.load64(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight), scratchGPR);
        slowCases.append(m_jit.branchIfEmpty(scratchGPR));
#else
        m_jit.load32(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), scratchGPR);
        slowCases.append(m_jit.branchIfEmpty(scratchGPR));
#endif
        m_jit.move(TrustedImm32(1), resultGPR);
        break;
    }
    case Array::Double: {
        ASSERT(!!m_graph.varArgChild(node, 2));
        StorageOperand storage(this, m_graph.varArgChild(node, 2));
        FPRTemporary scratch(this);
        FPRReg scratchFPR = scratch.fpr();
        GPRReg storageGPR = storage.gpr();

        MacroAssembler::Jump outOfBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, indexGPR, MacroAssembler::Address(storageGPR, Butterfly::offsetOfPublicLength()));
        if (mode.isInBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, outOfBounds);
        else
            slowCases.append(outOfBounds);

        m_jit.loadDouble(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight), scratchFPR);
        slowCases.append(m_jit.branchIfNaN(scratchFPR));
        m_jit.move(TrustedImm32(1), resultGPR);
        break;
    }
    case Array::ArrayStorage: {
        ASSERT(!!m_graph.varArgChild(node, 2));
        StorageOperand storage(this, m_graph.varArgChild(node, 2));
        GPRTemporary scratch(this);

        GPRReg storageGPR = storage.gpr();
        GPRReg scratchGPR = scratch.gpr();

        MacroAssembler::Jump outOfBounds = m_jit.branch32(MacroAssembler::AboveOrEqual, indexGPR, MacroAssembler::Address(storageGPR, ArrayStorage::vectorLengthOffset()));
        if (mode.isInBounds())
            speculationCheck(OutOfBounds, JSValueRegs(), nullptr, outOfBounds);
        else
            slowCases.append(outOfBounds);

#if USE(JSVALUE64)
        m_jit.load64(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, ArrayStorage::vectorOffset()), scratchGPR);
        slowCases.append(m_jit.branchIfEmpty(scratchGPR));
#else
        m_jit.load32(MacroAssembler::BaseIndex(storageGPR, indexGPR, MacroAssembler::TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), scratchGPR);
        slowCases.append(m_jit.branchIfEmpty(scratchGPR));
#endif
        m_jit.move(TrustedImm32(1), resultGPR);
        break;
    }
    default: {
        slowCases.append(m_jit.jump());
        break;
    }
    }

    addSlowPathGenerator(slowPathCall(slowCases, this, operationHasIndexedPropertyByInt, resultGPR, baseGPR, indexGPR, static_cast<int32_t>(node->internalMethodType())));

    unblessedBooleanResult(resultGPR, node);
}

void SpeculativeJIT::compileGetDirectPname(Node* node)
{
    Edge& baseEdge = m_jit.graph().varArgChild(node, 0);
    Edge& propertyEdge = m_jit.graph().varArgChild(node, 1);

    SpeculateCellOperand base(this, baseEdge);
    SpeculateCellOperand property(this, propertyEdge);
    GPRReg baseGPR = base.gpr();
    GPRReg propertyGPR = property.gpr();

#if CPU(X86)
    // Not enough registers on X86 for this code, so always use the slow path.
    flushRegisters();
    JSValueRegsFlushedCallResult result(this);
    JSValueRegs resultRegs = result.regs();
    callOperation(operationGetByValCell, resultRegs, baseGPR, CCallHelpers::CellValue(propertyGPR));
    m_jit.exceptionCheck();
    jsValueResult(resultRegs, node);
#else
    Edge& indexEdge = m_jit.graph().varArgChild(node, 2);
    Edge& enumeratorEdge = m_jit.graph().varArgChild(node, 3);
    SpeculateStrictInt32Operand index(this, indexEdge);
    SpeculateCellOperand enumerator(this, enumeratorEdge);
    GPRTemporary scratch(this);
    JSValueRegsTemporary result(this);

    GPRReg indexGPR = index.gpr();
    GPRReg enumeratorGPR = enumerator.gpr();
    GPRReg scratchGPR = scratch.gpr();
    JSValueRegs resultRegs = result.regs();

    MacroAssembler::JumpList slowPath;

    // Check the structure
    m_jit.load32(MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()), scratchGPR);
    slowPath.append(
        m_jit.branch32(
            MacroAssembler::NotEqual,
            scratchGPR,
            MacroAssembler::Address(
                enumeratorGPR, JSPropertyNameEnumerator::cachedStructureIDOffset())));

    // Compute the offset
    // If index is less than the enumerator's cached inline storage, then it's an inline access
    MacroAssembler::Jump outOfLineAccess = m_jit.branch32(MacroAssembler::AboveOrEqual,
        indexGPR, MacroAssembler::Address(enumeratorGPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()));

    m_jit.loadValue(MacroAssembler::BaseIndex(baseGPR, indexGPR, MacroAssembler::TimesEight, JSObject::offsetOfInlineStorage()), resultRegs);

    MacroAssembler::Jump done = m_jit.jump();

    // Otherwise it's out of line
    outOfLineAccess.link(&m_jit);
    m_jit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), resultRegs.payloadGPR());
    m_jit.move(indexGPR, scratchGPR);
    m_jit.sub32(MacroAssembler::Address(enumeratorGPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()), scratchGPR);
    m_jit.neg32(scratchGPR);
    m_jit.signExtend32ToPtr(scratchGPR, scratchGPR);
    int32_t offsetOfFirstProperty = static_cast<int32_t>(offsetInButterfly(firstOutOfLineOffset)) * sizeof(EncodedJSValue);
    m_jit.loadValue(MacroAssembler::BaseIndex(resultRegs.payloadGPR(), scratchGPR, MacroAssembler::TimesEight, offsetOfFirstProperty), resultRegs);

    done.link(&m_jit);

    addSlowPathGenerator(slowPathCall(slowPath, this, operationGetByValCell, resultRegs, baseGPR, CCallHelpers::CellValue(propertyGPR)));

    jsValueResult(resultRegs, node);
#endif
}

void SpeculativeJIT::compileExtractCatchLocal(Node* node)
{
    JSValueRegsTemporary result(this);
    JSValueRegs resultRegs = result.regs();

    JSValue* ptr = &reinterpret_cast<JSValue*>(m_jit.jitCode()->common.catchOSREntryBuffer->dataBuffer())[node->catchOSREntryIndex()];
    m_jit.loadValue(ptr, resultRegs);
    jsValueResult(resultRegs, node);
}

void SpeculativeJIT::compileClearCatchLocals(Node* node)
{
    ScratchBuffer* scratchBuffer = m_jit.jitCode()->common.catchOSREntryBuffer;
    ASSERT(scratchBuffer);
    GPRTemporary scratch(this);
    GPRReg scratchGPR = scratch.gpr();
    m_jit.move(TrustedImmPtr(scratchBuffer->addressOfActiveLength()), scratchGPR);
    m_jit.storePtr(TrustedImmPtr(nullptr), scratchGPR);
    noResult(node);
}

void SpeculativeJIT::compileProfileType(Node* node)
{
    JSValueOperand value(this, node->child1());
    GPRTemporary scratch1(this);
    GPRTemporary scratch2(this);
    GPRTemporary scratch3(this);

    JSValueRegs valueRegs = value.jsValueRegs();
    GPRReg scratch1GPR = scratch1.gpr();
    GPRReg scratch2GPR = scratch2.gpr();
    GPRReg scratch3GPR = scratch3.gpr();

    MacroAssembler::JumpList jumpToEnd;

    jumpToEnd.append(m_jit.branchIfEmpty(valueRegs));

    TypeLocation* cachedTypeLocation = node->typeLocation();
    // Compile in a predictive type check, if possible, to see if we can skip writing to the log.
    // These typechecks are inlined to match those of the 64-bit JSValue type checks.
    if (cachedTypeLocation->m_lastSeenType == TypeUndefined)
        jumpToEnd.append(m_jit.branchIfUndefined(valueRegs));
    else if (cachedTypeLocation->m_lastSeenType == TypeNull)
        jumpToEnd.append(m_jit.branchIfNull(valueRegs));
    else if (cachedTypeLocation->m_lastSeenType == TypeBoolean)
        jumpToEnd.append(m_jit.branchIfBoolean(valueRegs, scratch1GPR));
    else if (cachedTypeLocation->m_lastSeenType == TypeAnyInt)
        jumpToEnd.append(m_jit.branchIfInt32(valueRegs));
    else if (cachedTypeLocation->m_lastSeenType == TypeNumber)
        jumpToEnd.append(m_jit.branchIfNumber(valueRegs, scratch1GPR));
    else if (cachedTypeLocation->m_lastSeenType == TypeString) {
        MacroAssembler::Jump isNotCell = m_jit.branchIfNotCell(valueRegs);
        jumpToEnd.append(m_jit.branchIfString(valueRegs.payloadGPR()));
        isNotCell.link(&m_jit);
    }

    // Load the TypeProfilerLog into Scratch2.
    TypeProfilerLog* cachedTypeProfilerLog = m_jit.vm()->typeProfilerLog();
    m_jit.move(TrustedImmPtr(cachedTypeProfilerLog), scratch2GPR);

    // Load the next LogEntry into Scratch1.
    m_jit.loadPtr(MacroAssembler::Address(scratch2GPR, TypeProfilerLog::currentLogEntryOffset()), scratch1GPR);

    // Store the JSValue onto the log entry.
    m_jit.storeValue(valueRegs, MacroAssembler::Address(scratch1GPR, TypeProfilerLog::LogEntry::valueOffset()));

    // Store the structureID of the cell if valueRegs is a cell, otherwise, store 0 on the log entry.
    MacroAssembler::Jump isNotCell = m_jit.branchIfNotCell(valueRegs);
    m_jit.load32(MacroAssembler::Address(valueRegs.payloadGPR(), JSCell::structureIDOffset()), scratch3GPR);
    m_jit.store32(scratch3GPR, MacroAssembler::Address(scratch1GPR, TypeProfilerLog::LogEntry::structureIDOffset()));
    MacroAssembler::Jump skipIsCell = m_jit.jump();
    isNotCell.link(&m_jit);
    m_jit.store32(TrustedImm32(0), MacroAssembler::Address(scratch1GPR, TypeProfilerLog::LogEntry::structureIDOffset()));
    skipIsCell.link(&m_jit);

    // Store the typeLocation on the log entry.
    m_jit.move(TrustedImmPtr(cachedTypeLocation), scratch3GPR);
    m_jit.storePtr(scratch3GPR, MacroAssembler::Address(scratch1GPR, TypeProfilerLog::LogEntry::locationOffset()));

    // Increment the current log entry.
    m_jit.addPtr(TrustedImm32(sizeof(TypeProfilerLog::LogEntry)), scratch1GPR);
    m_jit.storePtr(scratch1GPR, MacroAssembler::Address(scratch2GPR, TypeProfilerLog::currentLogEntryOffset()));
    MacroAssembler::Jump clearLog = m_jit.branchPtr(MacroAssembler::Equal, scratch1GPR, TrustedImmPtr(cachedTypeProfilerLog->logEndPtr()));
    addSlowPathGenerator(
        slowPathCall(clearLog, this, operationProcessTypeProfilerLogDFG, NoResult));

    jumpToEnd.link(&m_jit);

    noResult(node);
}

void SpeculativeJIT::cachedPutById(CodeOrigin codeOrigin, GPRReg baseGPR, JSValueRegs valueRegs, GPRReg scratchGPR, unsigned identifierNumber, PutKind putKind, JITCompiler::Jump slowPathTarget, SpillRegistersMode spillMode)
{
    RegisterSet usedRegisters = this->usedRegisters();
    if (spillMode == DontSpill) {
        // We've already flushed registers to the stack, we don't need to spill these.
        usedRegisters.set(baseGPR, false);
        usedRegisters.set(valueRegs, false);
    }
    CallSiteIndex callSite = m_jit.recordCallSiteAndGenerateExceptionHandlingOSRExitIfNeeded(codeOrigin, m_stream->size());
    JITPutByIdGenerator gen(
        m_jit.codeBlock(), codeOrigin, callSite, usedRegisters,
        JSValueRegs::payloadOnly(baseGPR), valueRegs,
        scratchGPR, m_jit.ecmaModeFor(codeOrigin), putKind);

    gen.generateFastPath(m_jit);

    JITCompiler::JumpList slowCases;
    if (slowPathTarget.isSet())
        slowCases.append(slowPathTarget);
    slowCases.append(gen.slowPathJump());

    auto slowPath = slowPathCall(
        slowCases, this, gen.slowPathFunction(), NoResult, gen.stubInfo(), valueRegs,
        CCallHelpers::CellValue(baseGPR), identifierUID(identifierNumber));

    m_jit.addPutById(gen, slowPath.get());
    addSlowPathGenerator(WTFMove(slowPath));
}

void SpeculativeJIT::nonSpeculativeNonPeepholeCompare(Node* node, MacroAssembler::RelationalCondition cond, S_JITOperation_EJJ helperFunction)
{
    ASSERT(node->isBinaryUseKind(UntypedUse));
    JSValueOperand arg1(this, node->child1());
    JSValueOperand arg2(this, node->child2());

    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();

    JITCompiler::JumpList slowPath;

    if (isKnownNotInteger(node->child1().node()) || isKnownNotInteger(node->child2().node())) {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();

        flushRegisters();
        callOperation(helperFunction, resultGPR, arg1Regs, arg2Regs);
        m_jit.exceptionCheck();

        unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
        return;
    }

    GPRTemporary result(this, Reuse, arg1, TagWord);
    GPRReg resultGPR = result.gpr();

    arg1.use();
    arg2.use();

    if (!isKnownInteger(node->child1().node()))
        slowPath.append(m_jit.branchIfNotInt32(arg1Regs));
    if (!isKnownInteger(node->child2().node()))
        slowPath.append(m_jit.branchIfNotInt32(arg2Regs));

    m_jit.compare32(cond, arg1Regs.payloadGPR(), arg2Regs.payloadGPR(), resultGPR);

    if (!isKnownInteger(node->child1().node()) || !isKnownInteger(node->child2().node()))
        addSlowPathGenerator(slowPathCall(slowPath, this, helperFunction, resultGPR, arg1Regs, arg2Regs));

    unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::nonSpeculativePeepholeBranch(Node* node, Node* branchNode, MacroAssembler::RelationalCondition cond, S_JITOperation_EJJ helperFunction)
{
    BasicBlock* taken = branchNode->branchData()->taken.block;
    BasicBlock* notTaken = branchNode->branchData()->notTaken.block;

    JITCompiler::ResultCondition callResultCondition = JITCompiler::NonZero;

    // The branch instruction will branch to the taken block.
    // If taken is next, switch taken with notTaken & invert the branch condition so we can fall through.
    if (taken == nextBlock()) {
        cond = JITCompiler::invert(cond);
        callResultCondition = JITCompiler::Zero;
        BasicBlock* tmp = taken;
        taken = notTaken;
        notTaken = tmp;
    }

    JSValueOperand arg1(this, node->child1());
    JSValueOperand arg2(this, node->child2());
    JSValueRegs arg1Regs = arg1.jsValueRegs();
    JSValueRegs arg2Regs = arg2.jsValueRegs();

    JITCompiler::JumpList slowPath;

    if (isKnownNotInteger(node->child1().node()) || isKnownNotInteger(node->child2().node())) {
        GPRFlushedCallResult result(this);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();

        flushRegisters();
        callOperation(helperFunction, resultGPR, arg1Regs, arg2Regs);
        m_jit.exceptionCheck();

        branchTest32(callResultCondition, resultGPR, taken);
    } else {
        GPRTemporary result(this, Reuse, arg2, TagWord);
        GPRReg resultGPR = result.gpr();

        arg1.use();
        arg2.use();

        if (!isKnownInteger(node->child1().node()))
            slowPath.append(m_jit.branchIfNotInt32(arg1Regs));
        if (!isKnownInteger(node->child2().node()))
            slowPath.append(m_jit.branchIfNotInt32(arg2Regs));

        branch32(cond, arg1Regs.payloadGPR(), arg2Regs.payloadGPR(), taken);

        if (!isKnownInteger(node->child1().node()) || !isKnownInteger(node->child2().node())) {
            jump(notTaken, ForceJump);

            slowPath.link(&m_jit);

            silentSpillAllRegisters(resultGPR);
            callOperation(helperFunction, resultGPR, arg1Regs, arg2Regs);
            silentFillAllRegisters();
            m_jit.exceptionCheck();

            branchTest32(callResultCondition, resultGPR, taken);
        }
    }

    jump(notTaken);

    m_indexInBlock = m_block->size() - 1;
    m_currentNode = branchNode;
}

void SpeculativeJIT::compileBigIntEquality(Node* node)
{
    // FIXME: [ESNext][BigInt] Create specialized version of strict equals for BigIntUse
    // https://bugs.webkit.org/show_bug.cgi?id=182895
    SpeculateCellOperand left(this, node->child1());
    SpeculateCellOperand right(this, node->child2());
    GPRTemporary result(this, Reuse, left);
    GPRReg leftGPR = left.gpr();
    GPRReg rightGPR = right.gpr();
    GPRReg resultGPR = result.gpr();

    left.use();
    right.use();

    speculateBigInt(node->child1(), leftGPR);
    speculateBigInt(node->child2(), rightGPR);

    JITCompiler::Jump notEqualCase = m_jit.branchPtr(JITCompiler::NotEqual, leftGPR, rightGPR);

    m_jit.move(JITCompiler::TrustedImm32(1), resultGPR);

    JITCompiler::Jump done = m_jit.jump();

    notEqualCase.link(&m_jit);

    silentSpillAllRegisters(resultGPR);
    callOperation(operationCompareStrictEqCell, resultGPR, leftGPR, rightGPR);
    silentFillAllRegisters();

    done.link(&m_jit);

    unblessedBooleanResult(resultGPR, node, UseChildrenCalledExplicitly);
}

void SpeculativeJIT::compileMakeRope(Node* node)
{
    ASSERT(node->child1().useKind() == KnownStringUse);
    ASSERT(node->child2().useKind() == KnownStringUse);
    ASSERT(!node->child3() || node->child3().useKind() == KnownStringUse);

    SpeculateCellOperand op1(this, node->child1());
    SpeculateCellOperand op2(this, node->child2());
    SpeculateCellOperand op3(this, node->child3());
    GPRReg opGPRs[3];
    unsigned numOpGPRs;
    opGPRs[0] = op1.gpr();
    opGPRs[1] = op2.gpr();
    if (node->child3()) {
        opGPRs[2] = op3.gpr();
        numOpGPRs = 3;
    } else {
        opGPRs[2] = InvalidGPRReg;
        numOpGPRs = 2;
    }

#if CPU(ADDRESS64)
    Edge edges[3] = {
        node->child1(),
        node->child2(),
        node->child3()
    };

    GPRTemporary result(this);
    GPRTemporary allocator(this);
    GPRTemporary scratch(this);
    GPRTemporary scratch2(this);
    GPRReg resultGPR = result.gpr();
    GPRReg allocatorGPR = allocator.gpr();
    GPRReg scratchGPR = scratch.gpr();
    GPRReg scratch2GPR = scratch2.gpr();

    CCallHelpers::JumpList slowPath;
    Allocator allocatorValue = allocatorForNonVirtualConcurrently<JSRopeString>(*m_jit.vm(), sizeof(JSRopeString), AllocatorForMode::AllocatorIfExists);
    emitAllocateJSCell(resultGPR, JITAllocator::constant(allocatorValue), allocatorGPR, TrustedImmPtr(m_jit.graph().registerStructure(m_jit.vm()->stringStructure.get())), scratchGPR, slowPath);

    // This puts nullptr for the first fiber. It makes visitChildren safe even if this JSRopeString is discarded due to the speculation failure in the following path.
    m_jit.storePtr(TrustedImmPtr(JSString::isRopeInPointer), CCallHelpers::Address(resultGPR, JSRopeString::offsetOfFiber0()));

    {
        if (JSString* string = edges[0]->dynamicCastConstant<JSString*>(*m_jit.vm())) {
            m_jit.move(TrustedImm32(string->is8Bit() ? StringImpl::flagIs8Bit() : 0), scratchGPR);
            m_jit.move(TrustedImm32(string->length()), allocatorGPR);
        } else {
            bool needsRopeCase = canBeRope(edges[0]);
            m_jit.loadPtr(CCallHelpers::Address(opGPRs[0], JSString::offsetOfValue()), scratch2GPR);
            CCallHelpers::Jump isRope;
            if (needsRopeCase)
                isRope = m_jit.branchIfRopeStringImpl(scratch2GPR);

            m_jit.load32(CCallHelpers::Address(scratch2GPR, StringImpl::flagsOffset()), scratchGPR);
            m_jit.load32(CCallHelpers::Address(scratch2GPR, StringImpl::lengthMemoryOffset()), allocatorGPR);

            if (needsRopeCase) {
                auto done = m_jit.jump();

                isRope.link(&m_jit);
                m_jit.load32(CCallHelpers::Address(opGPRs[0], JSRopeString::offsetOfFlags()), scratchGPR);
                m_jit.load32(CCallHelpers::Address(opGPRs[0], JSRopeString::offsetOfLength()), allocatorGPR);
                done.link(&m_jit);
            }
        }

        if (!ASSERT_DISABLED) {
            CCallHelpers::Jump ok = m_jit.branch32(
                CCallHelpers::GreaterThanOrEqual, allocatorGPR, TrustedImm32(0));
            m_jit.abortWithReason(DFGNegativeStringLength);
            ok.link(&m_jit);
        }
    }

    for (unsigned i = 1; i < numOpGPRs; ++i) {
        if (JSString* string = edges[i]->dynamicCastConstant<JSString*>(*m_jit.vm())) {
            m_jit.and32(TrustedImm32(string->is8Bit() ? StringImpl::flagIs8Bit() : 0), scratchGPR);
            speculationCheck(
                Uncountable, JSValueSource(), nullptr,
                m_jit.branchAdd32(
                    CCallHelpers::Overflow,
                    TrustedImm32(string->length()), allocatorGPR));
        } else {
            bool needsRopeCase = canBeRope(edges[i]);
            m_jit.loadPtr(CCallHelpers::Address(opGPRs[i], JSString::offsetOfValue()), scratch2GPR);
            CCallHelpers::Jump isRope;
            if (needsRopeCase)
                isRope = m_jit.branchIfRopeStringImpl(scratch2GPR);

            m_jit.and32(CCallHelpers::Address(scratch2GPR, StringImpl::flagsOffset()), scratchGPR);
            speculationCheck(
                Uncountable, JSValueSource(), nullptr,
                m_jit.branchAdd32(
                    CCallHelpers::Overflow,
                    CCallHelpers::Address(scratch2GPR, StringImpl::lengthMemoryOffset()), allocatorGPR));
            if (needsRopeCase) {
                auto done = m_jit.jump();

                isRope.link(&m_jit);
                m_jit.and32(CCallHelpers::Address(opGPRs[i], JSRopeString::offsetOfFlags()), scratchGPR);
                m_jit.load32(CCallHelpers::Address(opGPRs[i], JSRopeString::offsetOfLength()), scratch2GPR);
                speculationCheck(
                    Uncountable, JSValueSource(), nullptr,
                    m_jit.branchAdd32(
                        CCallHelpers::Overflow, scratch2GPR, allocatorGPR));
                done.link(&m_jit);
            }
        }
    }

    if (!ASSERT_DISABLED) {
        CCallHelpers::Jump ok = m_jit.branch32(
            CCallHelpers::GreaterThanOrEqual, allocatorGPR, TrustedImm32(0));
        m_jit.abortWithReason(DFGNegativeStringLength);
        ok.link(&m_jit);
    }

    static_assert(StringImpl::flagIs8Bit() == JSRopeString::is8BitInPointer, "");
    m_jit.and32(TrustedImm32(StringImpl::flagIs8Bit()), scratchGPR);
    m_jit.orPtr(opGPRs[0], scratchGPR);
    m_jit.orPtr(TrustedImmPtr(JSString::isRopeInPointer), scratchGPR);
    m_jit.storePtr(scratchGPR, CCallHelpers::Address(resultGPR, JSRopeString::offsetOfFiber0()));

    m_jit.move(opGPRs[1], scratchGPR);
    m_jit.lshiftPtr(TrustedImm32(32), scratchGPR);
    m_jit.orPtr(allocatorGPR, scratchGPR);
    m_jit.storePtr(scratchGPR, CCallHelpers::Address(resultGPR, JSRopeString::offsetOfFiber1()));

    if (numOpGPRs == 2) {
        m_jit.move(opGPRs[1], scratchGPR);
        m_jit.rshiftPtr(TrustedImm32(32), scratchGPR);
        m_jit.storePtr(scratchGPR, CCallHelpers::Address(resultGPR, JSRopeString::offsetOfFiber2()));
    } else {
        m_jit.move(opGPRs[1], scratchGPR);
        m_jit.rshiftPtr(TrustedImm32(32), scratchGPR);
        m_jit.move(opGPRs[2], scratch2GPR);
        m_jit.lshiftPtr(TrustedImm32(16), scratch2GPR);
        m_jit.orPtr(scratch2GPR, scratchGPR);
        m_jit.storePtr(scratchGPR, CCallHelpers::Address(resultGPR, JSRopeString::offsetOfFiber2()));
    }

    auto isNonEmptyString = m_jit.branchTest32(CCallHelpers::NonZero, allocatorGPR);

    m_jit.move(TrustedImmPtr::weakPointer(m_jit.graph(), jsEmptyString(&m_jit.graph().m_vm)), resultGPR);

    isNonEmptyString.link(&m_jit);
    m_jit.mutatorFence(*m_jit.vm());

    switch (numOpGPRs) {
    case 2:
        addSlowPathGenerator(slowPathCall(
            slowPath, this, operationMakeRope2, resultGPR, opGPRs[0], opGPRs[1]));
        break;
    case 3:
        addSlowPathGenerator(slowPathCall(
            slowPath, this, operationMakeRope3, resultGPR, opGPRs[0], opGPRs[1], opGPRs[2]));
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    cellResult(resultGPR, node);
#else
    flushRegisters();
    GPRFlushedCallResult result(this);
    GPRReg resultGPR = result.gpr();
    switch (numOpGPRs) {
    case 2:
        callOperation(operationMakeRope2, resultGPR, opGPRs[0], opGPRs[1]);
        m_jit.exceptionCheck();
        break;
    case 3:
        callOperation(operationMakeRope3, resultGPR, opGPRs[0], opGPRs[1], opGPRs[2]);
        m_jit.exceptionCheck();
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    cellResult(resultGPR, node);
#endif
}

} } // namespace JSC::DFG

#endif
