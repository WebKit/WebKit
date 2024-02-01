/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#include "InlineCacheCompiler.h"

#if ENABLE(JIT)

#include "AccessCaseSnippetParams.h"
#include "BaselineJITCode.h"
#include "BinarySwitch.h"
#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "DOMJITCallDOMGetterSnippet.h"
#include "DOMJITGetterSetter.h"
#include "DirectArguments.h"
#include "FullCodeOrigin.h"
#include "GetterSetter.h"
#include "GetterSetterAccessCase.h"
#include "Heap.h"
#include "InstanceOfAccessCase.h"
#include "IntrinsicGetterAccessCase.h"
#include "JIT.h"
#include "JITOperations.h"
#include "JITThunks.h"
#include "JSModuleEnvironment.h"
#include "JSModuleNamespaceObject.h"
#include "JSTypedArrays.h"
#include "JSWebAssemblyInstance.h"
#include "LLIntThunks.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "MegamorphicCache.h"
#include "ModuleNamespaceAccessCase.h"
#include "ProxyObjectAccessCase.h"
#include "ScopedArguments.h"
#include "ScratchRegisterAllocator.h"
#include "StructureInlines.h"
#include "StructureStubClearingWatchpoint.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include "WebAssemblyModuleRecord.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>

namespace JSC {

namespace InlineCacheCompilerInternal {
static constexpr bool verbose = false;
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PolymorphicAccess);

void AccessGenerationResult::dump(PrintStream& out) const
{
    out.print(m_kind);
    if (m_handler)
        out.print(":", *m_handler);
}

void InlineCacheHandler::dump(PrintStream& out) const
{
    if (m_callTarget)
        out.print(m_callTarget);
}

static TypedArrayType toTypedArrayType(AccessCase::AccessType accessType)
{
    switch (accessType) {
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayInt8InHit:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayInt8InHit:
        return TypeInt8;
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8InHit:
        return TypeUint8;
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedInHit:
        return TypeUint8Clamped;
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayInt16InHit:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayInt16InHit:
        return TypeInt16;
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayUint16InHit:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayUint16InHit:
        return TypeUint16;
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayInt32InHit:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayInt32InHit:
        return TypeInt32;
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayUint32InHit:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayUint32InHit:
        return TypeUint32;
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat32InHit:
        return TypeFloat32;
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedTypedArrayFloat64InHit:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayFloat64InHit:
        return TypeFloat64;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static bool forResizableTypedArray(AccessCase::AccessType accessType)
{
    switch (accessType) {
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedResizableTypedArrayInt16InHit:
    case AccessCase::IndexedResizableTypedArrayUint16InHit:
    case AccessCase::IndexedResizableTypedArrayInt32InHit:
    case AccessCase::IndexedResizableTypedArrayUint32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat64InHit:
        return true;
    default:
        return false;
    }
}

static bool needsScratchFPR(AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Load:
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::Transition:
    case AccessCase::Delete:
    case AccessCase::DeleteNonConfigurable:
    case AccessCase::DeleteMiss:
    case AccessCase::Replace:
    case AccessCase::Miss:
    case AccessCase::GetGetter:
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::InHit:
    case AccessCase::InMiss:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::SetPrivateBrand:
    case AccessCase::ArrayLength:
    case AccessCase::StringLength:
    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::ProxyObjectHas:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
    case AccessCase::InstanceOfGeneric:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8InHit:
    case AccessCase::IndexedTypedArrayUint8InHit:
    case AccessCase::IndexedTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedTypedArrayInt16InHit:
    case AccessCase::IndexedTypedArrayUint16InHit:
    case AccessCase::IndexedTypedArrayInt32InHit:
    case AccessCase::IndexedResizableTypedArrayInt8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedResizableTypedArrayInt16InHit:
    case AccessCase::IndexedResizableTypedArrayUint16InHit:
    case AccessCase::IndexedResizableTypedArrayInt32InHit:
    case AccessCase::IndexedStringInHit:
    case AccessCase::IndexedNoIndexingInMiss:
    // Indexed TypedArray InHit does not need FPR since it does not load a value.
    case AccessCase::IndexedTypedArrayUint32InHit:
    case AccessCase::IndexedTypedArrayFloat32InHit:
    case AccessCase::IndexedTypedArrayFloat64InHit:
    case AccessCase::IndexedResizableTypedArrayUint32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat64InHit:
        return false;
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedDoubleInHit:
    // Used by TypedArrayLength/TypedArrayByteOffset in the process of boxing their result as a double
    case AccessCase::IntrinsicGetter:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static bool forInBy(AccessCase::AccessType type)
{
    switch (type) {
    case AccessCase::Load:
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::Transition:
    case AccessCase::Delete:
    case AccessCase::DeleteNonConfigurable:
    case AccessCase::DeleteMiss:
    case AccessCase::Replace:
    case AccessCase::Miss:
    case AccessCase::GetGetter:
    case AccessCase::ArrayLength:
    case AccessCase::StringLength:
    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::SetPrivateBrand:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::InstanceOfGeneric:
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::ProxyObjectHas:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::IntrinsicGetter:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
        return false;
    case AccessCase::InHit:
    case AccessCase::InMiss:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8InHit:
    case AccessCase::IndexedTypedArrayUint8InHit:
    case AccessCase::IndexedTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedTypedArrayInt16InHit:
    case AccessCase::IndexedTypedArrayUint16InHit:
    case AccessCase::IndexedTypedArrayInt32InHit:
    case AccessCase::IndexedTypedArrayUint32InHit:
    case AccessCase::IndexedTypedArrayFloat32InHit:
    case AccessCase::IndexedTypedArrayFloat64InHit:
    case AccessCase::IndexedResizableTypedArrayInt8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedResizableTypedArrayInt16InHit:
    case AccessCase::IndexedResizableTypedArrayUint16InHit:
    case AccessCase::IndexedResizableTypedArrayInt32InHit:
    case AccessCase::IndexedResizableTypedArrayUint32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat64InHit:
    case AccessCase::IndexedStringInHit:
    case AccessCase::IndexedNoIndexingInMiss:
        return true;
    }
    return false;
}



void InlineCacheCompiler::installWatchpoint(CodeBlock* codeBlock, const ObjectPropertyCondition& condition)
{
    WatchpointsOnStructureStubInfo::ensureReferenceAndInstallWatchpoint(m_watchpoints, codeBlock, m_stubInfo, condition);
}

void InlineCacheCompiler::restoreScratch()
{
    m_allocator->restoreReusedRegistersByPopping(*m_jit, m_preservedReusedRegisterState);
}

inline bool InlineCacheCompiler::useHandlerIC() const
{
    return JITCode::isBaselineCode(m_jitType) && Options::useHandlerIC();
}

void InlineCacheCompiler::succeed()
{
    restoreScratch();
    if (useHandlerIC()) {
        emitDataICEpilogue(*m_jit);
        m_jit->ret();
        return;
    }
    if (m_jit->codeBlock()->useDataIC()) {
        m_jit->farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfDoneLocation()), JSInternalPtrTag);
        return;
    }
    m_success.append(m_jit->jump());
}

const ScalarRegisterSet& InlineCacheCompiler::liveRegistersForCall()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();
    return m_liveRegistersForCall;
}

const ScalarRegisterSet& InlineCacheCompiler::liveRegistersToPreserveAtExceptionHandlingCallSite()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();
    return m_liveRegistersToPreserveAtExceptionHandlingCallSite;
}

static RegisterSetBuilder calleeSaveRegisters()
{
    return RegisterSetBuilder(RegisterSetBuilder::vmCalleeSaveRegisters())
        .filter(RegisterSetBuilder::calleeSaveRegisters())
        .merge(RegisterSetBuilder::reservedHardwareRegisters())
        .merge(RegisterSetBuilder::stackRegisters());
}

const ScalarRegisterSet& InlineCacheCompiler::calculateLiveRegistersForCallAndExceptionHandling()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling) {
        m_calculatedRegistersForCallAndExceptionHandling = true;

        m_liveRegistersToPreserveAtExceptionHandlingCallSite = m_jit->codeBlock()->jitCode()->liveRegistersToPreserveAtExceptionHandlingCallSite(m_jit->codeBlock(), m_stubInfo->callSiteIndex).buildScalarRegisterSet();
        m_needsToRestoreRegistersIfException = m_liveRegistersToPreserveAtExceptionHandlingCallSite.numberOfSetRegisters() > 0;
        if (m_needsToRestoreRegistersIfException)
            RELEASE_ASSERT(JSC::JITCode::isOptimizingJIT(m_jit->codeBlock()->jitType()));

        auto liveRegistersForCall = RegisterSetBuilder(m_liveRegistersToPreserveAtExceptionHandlingCallSite.toRegisterSet(), m_allocator->usedRegisters());
        if (m_jit->codeBlock()->useDataIC())
            liveRegistersForCall.add(m_stubInfo->m_stubInfoGPR, IgnoreVectors);
        liveRegistersForCall.exclude(calleeSaveRegisters().buildAndValidate().includeWholeRegisterWidth());
        m_liveRegistersForCall = liveRegistersForCall.buildScalarRegisterSet();
    }
    return m_liveRegistersForCall;
}

auto InlineCacheCompiler::preserveLiveRegistersToStackForCall(const RegisterSet& extra) -> SpillState
{
    RegisterSetBuilder liveRegisters = liveRegistersForCall().toRegisterSet();
    liveRegisters.merge(extra);
    liveRegisters.filter(RegisterSetBuilder::allScalarRegisters());

    unsigned extraStackPadding = 0;
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(*m_jit, liveRegisters.buildAndValidate(), extraStackPadding);
    RELEASE_ASSERT(liveRegisters.buildAndValidate().numberOfSetRegisters() == liveRegisters.buildScalarRegisterSet().numberOfSetRegisters(),
        liveRegisters.buildAndValidate().numberOfSetRegisters(),
        liveRegisters.buildScalarRegisterSet().numberOfSetRegisters());
    RELEASE_ASSERT(liveRegisters.buildScalarRegisterSet().numberOfSetRegisters() || !numberOfStackBytesUsedForRegisterPreservation,
        liveRegisters.buildScalarRegisterSet().numberOfSetRegisters(),
        numberOfStackBytesUsedForRegisterPreservation);
    return SpillState {
        liveRegisters.buildScalarRegisterSet(),
        numberOfStackBytesUsedForRegisterPreservation
    };
}

auto InlineCacheCompiler::preserveLiveRegistersToStackForCallWithoutExceptions() -> SpillState
{
    RegisterSetBuilder liveRegisters = m_allocator->usedRegisters();
    if (m_jit->codeBlock()->useDataIC())
        liveRegisters.add(m_stubInfo->m_stubInfoGPR, IgnoreVectors);
    liveRegisters.exclude(calleeSaveRegisters().buildAndValidate().includeWholeRegisterWidth());
    liveRegisters.filter(RegisterSetBuilder::allScalarRegisters());

    constexpr unsigned extraStackPadding = 0;
    unsigned numberOfStackBytesUsedForRegisterPreservation = ScratchRegisterAllocator::preserveRegistersToStackForCall(*m_jit, liveRegisters.buildAndValidate(), extraStackPadding);
    RELEASE_ASSERT(liveRegisters.buildAndValidate().numberOfSetRegisters() == liveRegisters.buildScalarRegisterSet().numberOfSetRegisters(),
        liveRegisters.buildAndValidate().numberOfSetRegisters(),
        liveRegisters.buildScalarRegisterSet().numberOfSetRegisters());
    RELEASE_ASSERT(liveRegisters.buildScalarRegisterSet().numberOfSetRegisters() || !numberOfStackBytesUsedForRegisterPreservation,
        liveRegisters.buildScalarRegisterSet().numberOfSetRegisters(),
        numberOfStackBytesUsedForRegisterPreservation);
    return SpillState {
        liveRegisters.buildScalarRegisterSet(),
        numberOfStackBytesUsedForRegisterPreservation
    };
}

void InlineCacheCompiler::restoreLiveRegistersFromStackForCallWithThrownException(const SpillState& spillState)
{
    // Even if we're a getter, we don't want to ignore the result value like we normally do
    // because the getter threw, and therefore, didn't return a value that means anything.
    // Instead, we want to restore that register to what it was upon entering the getter
    // inline cache. The subtlety here is if the base and the result are the same register,
    // and the getter threw, we want OSR exit to see the original base value, not the result
    // of the getter call.
    RegisterSetBuilder dontRestore = spillState.spilledRegisters.toRegisterSet().includeWholeRegisterWidth();
    // As an optimization here, we only need to restore what is live for exception handling.
    // We can construct the dontRestore set to accomplish this goal by having it contain only
    // what is live for call but not live for exception handling. By ignoring things that are
    // only live at the call but not the exception handler, we will only restore things live
    // at the exception handler.
    dontRestore.exclude(liveRegistersToPreserveAtExceptionHandlingCallSite().toRegisterSet().includeWholeRegisterWidth());
    restoreLiveRegistersFromStackForCall(spillState, dontRestore.buildAndValidate());
}

void InlineCacheCompiler::restoreLiveRegistersFromStackForCall(const SpillState& spillState, const RegisterSet& dontRestore)
{
    unsigned extraStackPadding = 0;
    ScratchRegisterAllocator::restoreRegistersFromStackForCall(*m_jit, spillState.spilledRegisters.toRegisterSet(), dontRestore, spillState.numberOfStackBytesUsedForRegisterPreservation, extraStackPadding);
}

CallSiteIndex InlineCacheCompiler::callSiteIndexForExceptionHandlingOrOriginal()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();

    if (!m_calculatedCallSiteIndex) {
        m_calculatedCallSiteIndex = true;

        if (m_needsToRestoreRegistersIfException)
            m_callSiteIndex = m_jit->codeBlock()->newExceptionHandlingCallSiteIndex(m_stubInfo->callSiteIndex);
        else
            m_callSiteIndex = originalCallSiteIndex();
    }

    return m_callSiteIndex;
}

DisposableCallSiteIndex InlineCacheCompiler::callSiteIndexForExceptionHandling()
{
    RELEASE_ASSERT(m_calculatedRegistersForCallAndExceptionHandling);
    RELEASE_ASSERT(m_needsToRestoreRegistersIfException);
    RELEASE_ASSERT(m_calculatedCallSiteIndex);
    return DisposableCallSiteIndex::fromCallSiteIndex(m_callSiteIndex);
}

const HandlerInfo& InlineCacheCompiler::originalExceptionHandler()
{
    if (!m_calculatedRegistersForCallAndExceptionHandling)
        calculateLiveRegistersForCallAndExceptionHandling();

    RELEASE_ASSERT(m_needsToRestoreRegistersIfException);
    HandlerInfo* exceptionHandler = m_jit->codeBlock()->handlerForIndex(m_stubInfo->callSiteIndex.bits());
    RELEASE_ASSERT(exceptionHandler);
    return *exceptionHandler;
}

CallSiteIndex InlineCacheCompiler::originalCallSiteIndex() const { return m_stubInfo->callSiteIndex; }

void InlineCacheCompiler::emitExplicitExceptionHandler()
{
    restoreScratch();
    m_jit->pushToSave(GPRInfo::regT0);
    m_jit->loadPtr(&m_vm.topEntryFrame, GPRInfo::regT0);
    m_jit->copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRInfo::regT0);
    m_jit->popToRestore(GPRInfo::regT0);

    if (needsToRestoreRegistersIfException()) {
        // To the JIT that produces the original exception handling
        // call site, they will expect the OSR exit to be arrived
        // at from genericUnwind. Therefore we must model what genericUnwind
        // does here. I.e, set callFrameForCatch and copy callee saves.

        m_jit->storePtr(GPRInfo::callFrameRegister, m_vm.addressOfCallFrameForCatch());
        // We don't need to insert a new exception handler in the table
        // because we're doing a manual exception check here. i.e, we'll
        // never arrive here from genericUnwind().
        HandlerInfo originalHandler = originalExceptionHandler();
        m_jit->jumpThunk(originalHandler.nativeCode);
    } else
        m_jit->jumpThunk(CodeLocationLabel(m_vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()));
}

ScratchRegisterAllocator InlineCacheCompiler::makeDefaultScratchAllocator(GPRReg extraToLock)
{
    ScratchRegisterAllocator allocator(m_stubInfo->usedRegisters.toRegisterSet());
    allocator.lock(m_stubInfo->baseRegs());
    allocator.lock(m_stubInfo->valueRegs());
    allocator.lock(m_stubInfo->m_extraGPR);
    allocator.lock(m_stubInfo->m_extra2GPR);
#if USE(JSVALUE32_64)
    allocator.lock(m_stubInfo->m_extraTagGPR);
    allocator.lock(m_stubInfo->m_extra2TagGPR);
#endif
    allocator.lock(m_stubInfo->m_stubInfoGPR);
    allocator.lock(m_stubInfo->m_arrayProfileGPR);
    allocator.lock(extraToLock);

    return allocator;
}

#if CPU(X86_64) && OS(WINDOWS)
static constexpr size_t prologueSizeInBytesDataIC = 5;
#elif CPU(X86_64)
static constexpr size_t prologueSizeInBytesDataIC = 1;
#elif CPU(ARM64E)
static constexpr size_t prologueSizeInBytesDataIC = 8;
#elif CPU(ARM64)
static constexpr size_t prologueSizeInBytesDataIC = 4;
#elif CPU(ARM_THUMB2)
static constexpr size_t prologueSizeInBytesDataIC = 6;
#elif CPU(RISCV64)
static constexpr size_t prologueSizeInBytesDataIC = 12;
#else
#error "unsupported architecture"
#endif

void InlineCacheCompiler::emitDataICPrologue(CCallHelpers& jit)
{
    // Important difference from the normal emitPrologue is that DataIC handler does not change callFrameRegister.
    // callFrameRegister is an original one of the caller JS function. This removes necessity of complicated handling
    // of exception unwinding, and it allows operations to access to CallFrame* via callFrameRegister.
#if ASSERT_ENABLED
    size_t startOffset = jit.debugOffset();
#endif

#if CPU(X86_64) && OS(WINDOWS)
    static_assert(maxFrameExtentForSlowPathCall);
    jit.push(CCallHelpers::framePointerRegister);
    jit.subPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
#elif CPU(X86_64)
    static_assert(!maxFrameExtentForSlowPathCall);
    jit.push(CCallHelpers::framePointerRegister);
#elif CPU(ARM64)
    static_assert(!maxFrameExtentForSlowPathCall);
    jit.tagReturnAddress();
    jit.pushPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
#elif CPU(ARM_THUMB2)
    static_assert(maxFrameExtentForSlowPathCall);
    jit.pushPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
    jit.subPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
#elif CPU(RISCV64)
    static_assert(!maxFrameExtentForSlowPathCall);
    jit.pushPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
#else
#error "unsupported architecture"
#endif

#if ASSERT_ENABLED
    ASSERT(prologueSizeInBytesDataIC == (jit.debugOffset() - startOffset));
#endif
}

void InlineCacheCompiler::emitDataICEpilogue(CCallHelpers& jit)
{
    if constexpr (!!maxFrameExtentForSlowPathCall)
        jit.addPtr(CCallHelpers::TrustedImm32(maxFrameExtentForSlowPathCall), CCallHelpers::stackPointerRegister);
    jit.emitFunctionEpilogueWithEmptyFrame();
}

static MacroAssemblerCodeRef<JITThunkPtrTag> getByIdSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByIdOptimize);

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::globalObjectGPR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(baseJSR, globalObjectGPR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 2>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC get_by_id_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> getByIdWithThisSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByIdWithThisOptimize);

    using BaselineJITRegisters::GetByIdWithThis::baseJSR;
    using BaselineJITRegisters::GetByIdWithThis::thisJSR;
    using BaselineJITRegisters::GetByIdWithThis::globalObjectGPR;
    using BaselineJITRegisters::GetByIdWithThis::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(baseJSR, thisJSR, globalObjectGPR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 3>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC get_by_id_with_this_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> getByValSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByValOptimize);

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::globalObjectGPR;
    using BaselineJITRegisters::GetByVal::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::profileGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(baseJSR, propertyJSR, globalObjectGPR, stubInfoGPR, profileGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 3>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC get_by_val_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> getPrivateNameSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetPrivateNameOptimize);

    using BaselineJITRegisters::PrivateBrand::baseJSR;
    using BaselineJITRegisters::PrivateBrand::propertyJSR;
    using BaselineJITRegisters::PrivateBrand::globalObjectGPR;
    using BaselineJITRegisters::PrivateBrand::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(baseJSR, propertyJSR, globalObjectGPR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 3>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC get_private_name_slow");
}

#if USE(JSVALUE64)
static MacroAssemblerCodeRef<JITThunkPtrTag> getByValWithThisSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByValWithThisOptimize);

    using BaselineJITRegisters::GetByValWithThis::baseJSR;
    using BaselineJITRegisters::GetByValWithThis::propertyJSR;
    using BaselineJITRegisters::GetByValWithThis::thisJSR;
    using BaselineJITRegisters::GetByValWithThis::globalObjectGPR;
    using BaselineJITRegisters::GetByValWithThis::stubInfoGPR;
    using BaselineJITRegisters::GetByValWithThis::profileGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(baseJSR, propertyJSR, thisJSR, globalObjectGPR, stubInfoGPR, profileGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 4>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC get_by_val_with_this_slow");
}
#endif

static MacroAssemblerCodeRef<JITThunkPtrTag> putByIdSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationPutByIdStrictOptimize);

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::globalObjectGPR;
    using BaselineJITRegisters::PutById::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(valueJSR, baseJSR, globalObjectGPR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 3>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC put_by_id_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> putByValSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperatoin = decltype(operationPutByValStrictOptimize);

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::profileGPR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::globalObjectGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperatoin>(baseJSR, propertyJSR, valueJSR, globalObjectGPR, stubInfoGPR, profileGPR);
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
#if CPU(ARM_THUMB2)
    // ARMv7 clobbers metadataTable register. Thus we need to restore them back here.
    JIT::emitMaterializeMetadataAndConstantPoolRegisters(jit);
#endif

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC put_by_val_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> instanceOfSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationInstanceOfOptimize);

    using BaselineJITRegisters::Instanceof::valueJSR;
    using BaselineJITRegisters::Instanceof::protoJSR;
    using BaselineJITRegisters::Instanceof::globalObjectGPR;
    using BaselineJITRegisters::Instanceof::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(valueJSR, protoJSR, globalObjectGPR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 3>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC instanceof_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> delByIdSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationDeleteByIdStrictOptimize);

    using BaselineJITRegisters::DelById::baseJSR;
    using BaselineJITRegisters::DelById::globalObjectGPR;
    using BaselineJITRegisters::DelById::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(baseJSR, globalObjectGPR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 2>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC del_by_id_slow");
}

static MacroAssemblerCodeRef<JITThunkPtrTag> delByValSlowPathCodeGenerator(VM& vm)
{
    CCallHelpers jit;

    using SlowOperation = decltype(operationDeleteByValStrictOptimize);

    using BaselineJITRegisters::DelByVal::baseJSR;
    using BaselineJITRegisters::DelByVal::propertyJSR;
    using BaselineJITRegisters::DelByVal::globalObjectGPR;
    using BaselineJITRegisters::DelByVal::stubInfoGPR;

    InlineCacheCompiler::emitDataICPrologue(jit);

    // Call slow operation
    jit.prepareCallOperation(vm);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfGlobalObject()), globalObjectGPR);
    jit.setupArguments<SlowOperation>(baseJSR, propertyJSR, globalObjectGPR, stubInfoGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 3>() == stubInfoGPR, "Needed for branch to slow operation via StubInfo");
    jit.call(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitNonPatchableExceptionCheck(vm).linkThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::HandleException).retaggedCode<NoPtrTag>()), &jit);

    InlineCacheCompiler::emitDataICEpilogue(jit);
    jit.ret();

    // While sp is extended, it is OK. Jump target will adjust it.
    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "DataIC del_by_val_slow");
}

MacroAssemblerCodeRef<JITThunkPtrTag> InlineCacheCompiler::generateSlowPathCode(VM& vm, AccessType type)
{
    switch (type) {
    case AccessType::GetById:
    case AccessType::TryGetById:
    case AccessType::GetByIdDirect:
    case AccessType::InById:
    case AccessType::GetPrivateNameById: {
        using ArgumentTypes = FunctionTraits<decltype(operationGetByIdOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationTryGetByIdOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationGetByIdDirectOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationInByIdOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationGetPrivateNameByIdOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(getByIdSlowPathCodeGenerator);
    }

    case AccessType::GetByIdWithThis:
        return vm.getCTIStub(getByIdWithThisSlowPathCodeGenerator);

    case AccessType::GetByVal:
    case AccessType::InByVal: {
        using ArgumentTypes = FunctionTraits<decltype(operationGetByValOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationInByValOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(getByValSlowPathCodeGenerator);
    }

    case AccessType::GetByValWithThis: {
#if USE(JSVALUE64)
        return vm.getCTIStub(getByValWithThisSlowPathCodeGenerator);
#else
        RELEASE_ASSERT_NOT_REACHED();
        return { };
#endif
    }

    case AccessType::GetPrivateName:
    case AccessType::HasPrivateBrand:
    case AccessType::HasPrivateName:
    case AccessType::CheckPrivateBrand:
    case AccessType::SetPrivateBrand: {
        using ArgumentTypes = FunctionTraits<decltype(operationGetPrivateNameOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationHasPrivateBrandOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationHasPrivateNameOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationCheckPrivateBrandOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationSetPrivateBrandOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(getPrivateNameSlowPathCodeGenerator);
    }

    case AccessType::PutByIdStrict:
    case AccessType::PutByIdSloppy:
    case AccessType::PutByIdDirectStrict:
    case AccessType::PutByIdDirectSloppy:
    case AccessType::DefinePrivateNameById:
    case AccessType::SetPrivateNameById: {
        using ArgumentTypes = FunctionTraits<decltype(operationPutByIdStrictOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdDirectStrictOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdDirectSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdDefinePrivateFieldStrictOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByIdSetPrivateFieldStrictOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(putByIdSlowPathCodeGenerator);
    }

    case AccessType::PutByValStrict:
    case AccessType::PutByValSloppy:
    case AccessType::PutByValDirectStrict:
    case AccessType::PutByValDirectSloppy:
    case AccessType::DefinePrivateNameByVal:
    case AccessType::SetPrivateNameByVal: {
        using ArgumentTypes = FunctionTraits<decltype(operationPutByValStrictOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByValSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationDirectPutByValSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationDirectPutByValStrictOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByValDefinePrivateFieldOptimize)>::ArgumentTypes, ArgumentTypes>);
        static_assert(std::is_same_v<FunctionTraits<decltype(operationPutByValSetPrivateFieldOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(putByValSlowPathCodeGenerator);
    }

    case AccessType::InstanceOf:
        return vm.getCTIStub(instanceOfSlowPathCodeGenerator);

    case AccessType::DeleteByIdStrict:
    case AccessType::DeleteByIdSloppy: {
        using ArgumentTypes = FunctionTraits<decltype(operationDeleteByIdStrictOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationDeleteByIdSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(delByIdSlowPathCodeGenerator);
    }

    case AccessType::DeleteByValStrict:
    case AccessType::DeleteByValSloppy: {
        using ArgumentTypes = FunctionTraits<decltype(operationDeleteByValStrictOptimize)>::ArgumentTypes;
        static_assert(std::is_same_v<FunctionTraits<decltype(operationDeleteByValSloppyOptimize)>::ArgumentTypes, ArgumentTypes>);
        return vm.getCTIStub(delByValSlowPathCodeGenerator);
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

InlineCacheHandler::InlineCacheHandler(Ref<PolymorphicAccessJITStubRoutine>&& stubRoutine, std::unique_ptr<WatchpointsOnStructureStubInfo>&& watchpoints)
    : m_callTarget(stubRoutine->code().code().template retagged<JITStubRoutinePtrTag>())
    , m_jumpTarget(CodePtr<NoPtrTag> { m_callTarget.retagged<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC }.template retagged<JITStubRoutinePtrTag>())
    , m_stubRoutine(WTFMove(stubRoutine))
    , m_watchpoints(WTFMove(watchpoints))
{
}

Ref<InlineCacheHandler> InlineCacheHandler::createNonHandlerSlowPath(CodePtr<JITStubRoutinePtrTag> slowPath)
{
    auto result = adoptRef(*new InlineCacheHandler);
    result->m_callTarget = slowPath;
    result->m_jumpTarget = slowPath;
    return result;
}

Ref<InlineCacheHandler> InlineCacheHandler::createSlowPath(VM& vm, AccessType accessType)
{
    auto result = adoptRef(*new InlineCacheHandler);
    auto codeRef = InlineCacheCompiler::generateSlowPathCode(vm, accessType);
    result->m_callTarget = codeRef.code().template retagged<JITStubRoutinePtrTag>();
    result->m_jumpTarget = CodePtr<NoPtrTag> { codeRef.retaggedCode<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC }.template retagged<JITStubRoutinePtrTag>();
    return result;
}

Ref<InlineCacheHandler> InlineCacheCompiler::generateSlowPathHandler(VM& vm, AccessType accessType)
{
    ASSERT(!isCompilationThread());
    ASSERT(Options::useHandlerIC());
    if (auto handler = vm.m_sharedJITStubs->getSlowPathHandler(accessType))
        return handler.releaseNonNull();
    auto handler = InlineCacheHandler::createSlowPath(vm, accessType);
    vm.m_sharedJITStubs->setSlowPathHandler(accessType, handler);
    return handler;
}

void InlineCacheCompiler::generateWithGuard(AccessCase& accessCase, CCallHelpers::JumpList& fallThrough)
{
    SuperSamplerScope superSamplerScope(false);

    accessCase.checkConsistency(*m_stubInfo);

    RELEASE_ASSERT(accessCase.m_state == AccessCase::Committed);
    accessCase.m_state = AccessCase::Generated;

    JSGlobalObject* globalObject = m_globalObject;
    CCallHelpers& jit = *m_jit;
    JIT_COMMENT(jit, "Begin generateWithGuard");
    StructureStubInfo& stubInfo = *m_stubInfo;
    VM& vm = m_vm;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg scratchGPR = m_scratchGPR;

    if (accessCase.requiresIdentifierNameMatch() && !stubInfo.hasConstantIdentifier) {
        RELEASE_ASSERT(accessCase.m_identifier);
        GPRReg propertyGPR = stubInfo.propertyGPR();
        // non-rope string check done inside polymorphic access.

        if (accessCase.uid()->isSymbol())
            jit.loadPtr(MacroAssembler::Address(propertyGPR, Symbol::offsetOfSymbolImpl()), scratchGPR);
        else
            jit.loadPtr(MacroAssembler::Address(propertyGPR, JSString::offsetOfValue()), scratchGPR);
        fallThrough.append(jit.branchPtr(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImmPtr(accessCase.uid())));
    }

    auto emitDefaultGuard = [&] () {
        if (accessCase.m_polyProtoAccessChain) {
            ASSERT(!accessCase.viaGlobalProxy());
            GPRReg baseForAccessGPR = m_scratchGPR;
            jit.move(baseGPR, baseForAccessGPR);
            accessCase.m_polyProtoAccessChain->forEach(vm, accessCase.structure(), [&] (Structure* structure, bool atEnd) {
                fallThrough.append(
                    jit.branchStructure(
                        CCallHelpers::NotEqual,
                        CCallHelpers::Address(baseForAccessGPR, JSCell::structureIDOffset()),
                        structure));
                if (atEnd) {
                    if ((accessCase.m_type == AccessCase::Miss || accessCase.m_type == AccessCase::InMiss || accessCase.m_type == AccessCase::Transition) && structure->hasPolyProto()) {
                        // For a Miss/InMiss/Transition, we must ensure we're at the end when the last item is poly proto.
                        // Transitions must do this because they need to verify there isn't a setter in the chain.
                        // Miss/InMiss need to do this to ensure there isn't a new item at the end of the chain that
                        // has the property.
#if USE(JSVALUE64)
                        jit.load64(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset)), baseForAccessGPR);
                        fallThrough.append(jit.branch64(CCallHelpers::NotEqual, baseForAccessGPR, CCallHelpers::TrustedImm64(JSValue::ValueNull)));
#else
                        jit.load32(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset), baseForAccessGPR);
                        fallThrough.append(jit.branchTestPtr(CCallHelpers::NonZero, baseForAccessGPR));
#endif
                    }
                } else {
                    if (structure->hasMonoProto()) {
                        JSValue prototype = structure->prototypeForLookup(globalObject);
                        RELEASE_ASSERT(prototype.isObject());
                        jit.move(CCallHelpers::TrustedImmPtr(asObject(prototype)), baseForAccessGPR);
                    } else {
                        RELEASE_ASSERT(structure->isObject()); // Primitives must have a stored prototype. We use prototypeForLookup for them.
#if USE(JSVALUE64)
                        jit.load64(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset)), baseForAccessGPR);
                        fallThrough.append(jit.branch64(CCallHelpers::Equal, baseForAccessGPR, CCallHelpers::TrustedImm64(JSValue::ValueNull)));
#else
                        jit.load32(MacroAssembler::Address(baseForAccessGPR, offsetRelativeToBase(knownPolyProtoOffset) + PayloadOffset), baseForAccessGPR);
                        fallThrough.append(jit.branchTestPtr(CCallHelpers::Zero, baseForAccessGPR));
#endif
                    }
                }
            });
            return;
        }

        if (accessCase.viaGlobalProxy()) {
            fallThrough.append(
                jit.branchIfNotType(baseGPR, GlobalProxyType));

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);

            fallThrough.append(
                jit.branchStructure(
                    CCallHelpers::NotEqual,
                    CCallHelpers::Address(scratchGPR, JSCell::structureIDOffset()),
                    accessCase.structure()));
            return;
        }

        fallThrough.append(
            jit.branchStructure(
                CCallHelpers::NotEqual,
                CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()),
                accessCase.structure()));
    };

    switch (accessCase.m_type) {
    case AccessCase::ArrayLength: {
        ASSERT(!accessCase.viaGlobalProxy());
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        fallThrough.append(
            jit.branchTest32(
                CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(IsArray)));
        fallThrough.append(
            jit.branchTest32(
                CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(IndexingShapeMask)));
        break;
    }

    case AccessCase::StringLength: {
        ASSERT(!accessCase.viaGlobalProxy());
        fallThrough.append(
            jit.branchIfNotString(baseGPR));
        break;
    }

    case AccessCase::DirectArgumentsLength: {
        ASSERT(!accessCase.viaGlobalProxy());
        fallThrough.append(
            jit.branchIfNotType(baseGPR, DirectArgumentsType));

        fallThrough.append(
            jit.branchTestPtr(
                CCallHelpers::NonZero,
                CCallHelpers::Address(baseGPR, DirectArguments::offsetOfMappedArguments())));
        jit.load32(
            CCallHelpers::Address(baseGPR, DirectArguments::offsetOfLength()),
            valueRegs.payloadGPR());
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        succeed();
        return;
    }

    case AccessCase::ScopedArgumentsLength: {
        ASSERT(!accessCase.viaGlobalProxy());
        fallThrough.append(
            jit.branchIfNotType(baseGPR, ScopedArgumentsType));

        fallThrough.append(
            jit.branchTest8(
                CCallHelpers::NonZero,
                CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfOverrodeThings())));
        jit.load32(
            CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTotalLength()),
            valueRegs.payloadGPR());
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        succeed();
        return;
    }

    case AccessCase::ModuleNamespaceLoad: {
        ASSERT(!accessCase.viaGlobalProxy());
        emitModuleNamespaceLoad(accessCase.as<ModuleNamespaceAccessCase>(), fallThrough);
        return;
    }

    case AccessCase::ProxyObjectHas:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::IndexedProxyObjectLoad: {
        ASSERT(!accessCase.viaGlobalProxy());
        emitProxyObjectAccess(accessCase.as<ProxyObjectAccessCase>(), fallThrough);
        return;
    }

    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedScopedArgumentsInHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = stubInfo.propertyGPR();

        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ScopedArgumentsType)));

        auto allocator = makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList failAndIgnore;

        failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTotalLength())));

        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfTable()), scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, ScopedArgumentsTable::offsetOfLength()), scratch2GPR);
        auto overflowCase = jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratch2GPR);

        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfScope()), scratch2GPR);
        jit.loadPtr(CCallHelpers::Address(scratchGPR, ScopedArgumentsTable::offsetOfArguments()), scratchGPR);
        jit.zeroExtend32ToWord(propertyGPR, scratch3GPR);
        jit.load32(CCallHelpers::BaseIndex(scratchGPR, scratch3GPR, CCallHelpers::TimesFour), scratchGPR);
        failAndIgnore.append(jit.branch32(CCallHelpers::Equal, scratchGPR, CCallHelpers::TrustedImm32(ScopeOffset::invalidOffset)));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else
            jit.loadValue(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight, JSLexicalEnvironment::offsetOfVariables()), valueRegs);
        auto done = jit.jump();

        overflowCase.link(&jit);
        jit.sub32(propertyGPR, scratch2GPR);
        jit.neg32(scratch2GPR);
        jit.loadPtr(CCallHelpers::Address(baseGPR, ScopedArguments::offsetOfStorage()), scratch3GPR);
#if USE(JSVALUE64)
        jit.loadValue(CCallHelpers::BaseIndex(scratch3GPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratchGPR));
        failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else
            jit.move(scratchGPR, valueRegs.payloadGPR());
#else
        jit.loadValue(CCallHelpers::BaseIndex(scratch3GPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratch2GPR, scratchGPR));
        failAndIgnore.append(jit.branchIfEmpty(scratch2GPR));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else {
            jit.move(scratchGPR, valueRegs.payloadGPR());
            jit.move(scratch2GPR, valueRegs.tagGPR());
        }
#endif

        done.link(&jit);

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndIgnore.append(jit.jump());
        } else
            m_failAndIgnore.append(failAndIgnore);

        return;
    }

    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsInHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = stubInfo.propertyGPR();
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
        fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(DirectArgumentsType)));

        jit.load32(CCallHelpers::Address(baseGPR, DirectArguments::offsetOfLength()), scratchGPR);
        m_failAndRepatch.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratchGPR));
        m_failAndRepatch.append(jit.branchTestPtr(CCallHelpers::NonZero, CCallHelpers::Address(baseGPR, DirectArguments::offsetOfMappedArguments())));
        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else {
            jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
            jit.loadValue(CCallHelpers::BaseIndex(baseGPR, scratchGPR, CCallHelpers::TimesEight, DirectArguments::storageOffset()), valueRegs);
        }
        succeed();
        return;
    }

    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedTypedArrayInt8InHit:
    case AccessCase::IndexedTypedArrayUint8InHit:
    case AccessCase::IndexedTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedTypedArrayInt16InHit:
    case AccessCase::IndexedTypedArrayUint16InHit:
    case AccessCase::IndexedTypedArrayInt32InHit:
    case AccessCase::IndexedTypedArrayUint32InHit:
    case AccessCase::IndexedTypedArrayFloat32InHit:
    case AccessCase::IndexedTypedArrayFloat64InHit:
    case AccessCase::IndexedResizableTypedArrayInt8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedResizableTypedArrayInt16InHit:
    case AccessCase::IndexedResizableTypedArrayUint16InHit:
    case AccessCase::IndexedResizableTypedArrayInt32InHit:
    case AccessCase::IndexedResizableTypedArrayUint32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat64InHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.

        TypedArrayType type = toTypedArrayType(accessCase.m_type);
        bool isResizableOrGrowableShared = forResizableTypedArray(accessCase.m_type);

        GPRReg propertyGPR = stubInfo.propertyGPR();

        fallThrough.append(jit.branch8(CCallHelpers::NotEqual, CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), CCallHelpers::TrustedImm32(typeForTypedArrayType(type))));

        if (!isResizableOrGrowableShared) {
            fallThrough.append(jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfMode()), CCallHelpers::TrustedImm32(isResizableOrGrowableSharedMode)));
            CCallHelpers::Address addressOfLength = CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength());
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
            // The length is a size_t, so either 32 or 64 bits depending on the platform.
            m_failAndRepatch.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, addressOfLength));
#else
            m_failAndRepatch.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, addressOfLength));
#endif
        }

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList failAndRepatchAfterRestore;
        if (isResizableOrGrowableShared) {
            jit.loadTypedArrayLength(baseGPR, scratch2GPR, scratchGPR, scratch2GPR, type);
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
            // The length is a size_t, so either 32 or 64 bits depending on the platform.
            failAndRepatchAfterRestore.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, scratch2GPR));
#else
            failAndRepatchAfterRestore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratch2GPR));
#endif
        }

        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else {
#if USE(LARGE_TYPED_ARRAYS)
            jit.load64(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
#else
            jit.load32(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
#endif
            jit.loadPtr(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfVector()), scratch2GPR);
            jit.cageConditionally(Gigacage::Primitive, scratch2GPR, scratchGPR, scratchGPR);
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
            if (isInt(type)) {
                switch (elementSize(type)) {
                case 1:
                    if (JSC::isSigned(type))
                        jit.load8SignedExtendTo32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), valueRegs.payloadGPR());
                    else
                        jit.load8(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), valueRegs.payloadGPR());
                    break;
                case 2:
                    if (JSC::isSigned(type))
                        jit.load16SignedExtendTo32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo), valueRegs.payloadGPR());
                    else
                        jit.load16(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo), valueRegs.payloadGPR());
                    break;
                case 4:
                    jit.load32(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour), valueRegs.payloadGPR());
                    break;
                default:
                    CRASH();
                }

                CCallHelpers::Jump done;
                if (type == TypeUint32) {
                    RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
                    auto canBeInt = jit.branch32(CCallHelpers::GreaterThanOrEqual, valueRegs.payloadGPR(), CCallHelpers::TrustedImm32(0));

                    jit.convertInt32ToDouble(valueRegs.payloadGPR(), m_scratchFPR);
                    jit.addDouble(CCallHelpers::AbsoluteAddress(&CCallHelpers::twoToThe32), m_scratchFPR);
                    jit.boxDouble(m_scratchFPR, valueRegs);
                    done = jit.jump();
                    canBeInt.link(&jit);
                }

                jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
                if (done.isSet())
                    done.link(&jit);
            } else {
                ASSERT(isFloat(type));
                RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
                switch (elementSize(type)) {
                case 4:
                    jit.loadFloat(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour), m_scratchFPR);
                    jit.convertFloatToDouble(m_scratchFPR, m_scratchFPR);
                    break;
                case 8: {
                    jit.loadDouble(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight), m_scratchFPR);
                    break;
                }
                default:
                    CRASH();
                }

                jit.purifyNaN(m_scratchFPR);
                jit.boxDouble(m_scratchFPR, valueRegs);
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (isResizableOrGrowableShared) {
            failAndRepatchAfterRestore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        }
        return;
    }

    case AccessCase::IndexedStringLoad:
    case AccessCase::IndexedStringInHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = stubInfo.propertyGPR();

        fallThrough.append(jit.branchIfNotString(baseGPR));

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        CCallHelpers::JumpList failAndIgnore;

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        jit.loadPtr(CCallHelpers::Address(baseGPR, JSString::offsetOfValue()), scratch2GPR);
        failAndIgnore.append(jit.branchIfRopeStringImpl(scratch2GPR));
        jit.load32(CCallHelpers::Address(scratch2GPR, StringImpl::lengthMemoryOffset()), scratchGPR);

        failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratchGPR));

        if (forInBy(accessCase.m_type))
            jit.moveTrustedValue(jsBoolean(true), valueRegs);
        else {
            jit.load32(CCallHelpers::Address(scratch2GPR, StringImpl::flagsOffset()), scratchGPR);
            jit.loadPtr(CCallHelpers::Address(scratch2GPR, StringImpl::dataOffset()), scratch2GPR);
            auto is16Bit = jit.branchTest32(CCallHelpers::Zero, scratchGPR, CCallHelpers::TrustedImm32(StringImpl::flagIs8Bit()));
            jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
            jit.load8(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne, 0), scratch2GPR);
            auto is8BitLoadDone = jit.jump();
            is16Bit.link(&jit);
            jit.zeroExtend32ToWord(propertyGPR, scratchGPR);
            jit.load16(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo, 0), scratch2GPR);
            is8BitLoadDone.link(&jit);

            failAndIgnore.append(jit.branch32(CCallHelpers::Above, scratch2GPR, CCallHelpers::TrustedImm32(maxSingleCharacterString)));
            jit.move(CCallHelpers::TrustedImmPtr(vm.smallStrings.singleCharacterStrings()), scratchGPR);
            jit.loadPtr(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::ScalePtr, 0), valueRegs.payloadGPR());
            jit.boxCell(valueRegs.payloadGPR(), valueRegs);
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndIgnore.append(jit.jump());
        } else
            m_failAndIgnore.append(failAndIgnore);

        return;
    }

    case AccessCase::IndexedNoIndexingMiss:
    case AccessCase::IndexedNoIndexingInMiss: {
        emitDefaultGuard();
        GPRReg propertyGPR = stubInfo.propertyGPR();
        m_failAndIgnore.append(jit.branch32(CCallHelpers::LessThan, propertyGPR, CCallHelpers::TrustedImm32(0)));
        break;
    }

    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.
        GPRReg propertyGPR = stubInfo.propertyGPR();

        // int32 check done in polymorphic access.
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        jit.and32(CCallHelpers::TrustedImm32(IndexingShapeMask), scratchGPR);

        auto allocator = makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratch2GPR = allocator.allocateScratchGPR();
#if USE(JSVALUE32_64)
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
#endif
        ScratchRegisterAllocator::PreservedState preservedState;

        CCallHelpers::JumpList failAndIgnore;
        auto preserveReusedRegisters = [&] {
            preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        };

        if (accessCase.m_type == AccessCase::IndexedArrayStorageLoad || accessCase.m_type == AccessCase::IndexedArrayStorageInHit) {
            jit.add32(CCallHelpers::TrustedImm32(-ArrayStorageShape), scratchGPR, scratchGPR);
            fallThrough.append(jit.branch32(CCallHelpers::Above, scratchGPR, CCallHelpers::TrustedImm32(SlowPutArrayStorageShape - ArrayStorageShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, ArrayStorage::vectorLengthOffset())));

            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
#if USE(JSVALUE64)
            jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs(scratchGPR));
            failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
            if (forInBy(accessCase.m_type))
                jit.moveTrustedValue(jsBoolean(true), valueRegs);
            else
                jit.move(scratchGPR, valueRegs.payloadGPR());
#else
            jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()), JSValueRegs(scratch3GPR, scratchGPR));
            failAndIgnore.append(jit.branchIfEmpty(scratch3GPR));
            if (forInBy(accessCase.m_type))
                jit.moveTrustedValue(jsBoolean(true), valueRegs);
            else {
                jit.move(scratchGPR, valueRegs.payloadGPR());
                jit.move(scratch3GPR, valueRegs.tagGPR());
            }
#endif
        } else {
            IndexingType expectedShape;
            switch (accessCase.m_type) {
            case AccessCase::IndexedInt32Load:
            case AccessCase::IndexedInt32InHit:
                expectedShape = Int32Shape;
                break;
            case AccessCase::IndexedDoubleLoad:
            case AccessCase::IndexedDoubleInHit:
                ASSERT(Options::allowDoubleShape());
                expectedShape = DoubleShape;
                break;
            case AccessCase::IndexedContiguousLoad:
            case AccessCase::IndexedContiguousInHit:
                expectedShape = ContiguousShape;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(expectedShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength())));
            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
            if (accessCase.m_type == AccessCase::IndexedDoubleLoad || accessCase.m_type == AccessCase::IndexedDoubleInHit) {
                RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
                jit.loadDouble(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), m_scratchFPR);
                failAndIgnore.append(jit.branchIfNaN(m_scratchFPR));
                if (forInBy(accessCase.m_type))
                    jit.moveTrustedValue(jsBoolean(true), valueRegs);
                else
                    jit.boxDouble(m_scratchFPR, valueRegs);
            } else {
#if USE(JSVALUE64)
                jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratchGPR));
                failAndIgnore.append(jit.branchIfEmpty(scratchGPR));
                if (forInBy(accessCase.m_type))
                    jit.moveTrustedValue(jsBoolean(true), valueRegs);
                else
                    jit.move(scratchGPR, valueRegs.payloadGPR());
#else
                jit.loadValue(CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight), JSValueRegs(scratch3GPR, scratchGPR));
                failAndIgnore.append(jit.branchIfEmpty(scratch3GPR));
                if (forInBy(accessCase.m_type))
                    jit.moveTrustedValue(jsBoolean(true), valueRegs);
                else {
                    jit.move(scratchGPR, valueRegs.payloadGPR());
                    jit.move(scratch3GPR, valueRegs.tagGPR());
                }
#endif
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters() && !failAndIgnore.empty()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndIgnore.append(jit.jump());
        } else
            m_failAndIgnore.append(failAndIgnore);
        return;
    }

    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore: {
        ASSERT(!accessCase.viaGlobalProxy());
        GPRReg propertyGPR = stubInfo.propertyGPR();

        // int32 check done in polymorphic access.
        jit.load8(CCallHelpers::Address(baseGPR, JSCell::indexingTypeAndMiscOffset()), scratchGPR);
        fallThrough.append(jit.branchTest32(CCallHelpers::NonZero, scratchGPR, CCallHelpers::TrustedImm32(CopyOnWrite)));
        jit.and32(CCallHelpers::TrustedImm32(IndexingShapeMask), scratchGPR);

        CCallHelpers::JumpList isOutOfBounds;

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        ScratchRegisterAllocator::PreservedState preservedState;

        CCallHelpers::JumpList failAndIgnore;
        CCallHelpers::JumpList failAndRepatch;
        auto preserveReusedRegisters = [&] {
            preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        };

        CCallHelpers::Label storeResult;
        if (accessCase.m_type == AccessCase::IndexedArrayStorageStore) {
            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ArrayStorageShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, ArrayStorage::vectorLengthOffset())));

            jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);

#if USE(JSVALUE64)
            isOutOfBounds.append(jit.branchTest64(CCallHelpers::Zero, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset())));
#else
            isOutOfBounds.append(jit.branch32(CCallHelpers::Equal, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset() + JSValue::offsetOfTag()), CCallHelpers::TrustedImm32(JSValue::EmptyValueTag)));
#endif

            storeResult = jit.label();
            jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight, ArrayStorage::vectorOffset()));
        } else {
            IndexingType expectedShape;
            switch (accessCase.m_type) {
            case AccessCase::IndexedInt32Store:
                expectedShape = Int32Shape;
                break;
            case AccessCase::IndexedDoubleStore:
                ASSERT(Options::allowDoubleShape());
                expectedShape = DoubleShape;
                break;
            case AccessCase::IndexedContiguousStore:
                expectedShape = ContiguousShape;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }

            fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(expectedShape)));

            preserveReusedRegisters();

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            isOutOfBounds.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength())));
            storeResult = jit.label();
            switch (accessCase.m_type) {
            case AccessCase::IndexedDoubleStore: {
                RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
                auto notInt = jit.branchIfNotInt32(valueRegs);
                jit.convertInt32ToDouble(valueRegs.payloadGPR(), m_scratchFPR);
                auto ready = jit.jump();
                notInt.link(&jit);
#if USE(JSVALUE64)
                jit.unboxDoubleWithoutAssertions(valueRegs.payloadGPR(), scratch2GPR, m_scratchFPR);
#else
                jit.unboxDouble(valueRegs, m_scratchFPR);
#endif
                failAndRepatch.append(jit.branchIfNaN(m_scratchFPR));
                ready.link(&jit);

                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                jit.storeDouble(m_scratchFPR, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                break;
            }
            case AccessCase::IndexedInt32Store:
                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                failAndRepatch.append(jit.branchIfNotInt32(valueRegs));
                jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                break;
            case AccessCase::IndexedContiguousStore:
                jit.zeroExtend32ToWord(propertyGPR, scratch2GPR);
                jit.storeValue(valueRegs, CCallHelpers::BaseIndex(scratchGPR, scratch2GPR, CCallHelpers::TimesEight));
                // WriteBarrier must be emitted in the embedder side.
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (accessCase.m_type == AccessCase::IndexedArrayStorageStore) {
            isOutOfBounds.link(&jit);
            if (stubInfo.m_arrayProfileGPR != InvalidGPRReg)
                jit.or32(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ArrayProfileFlag::MayStoreHole)), CCallHelpers::Address(stubInfo.m_arrayProfileGPR, ArrayProfile::offsetOfArrayProfileFlags()));
            jit.add32(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(scratchGPR, ArrayStorage::numValuesInVectorOffset()));
            jit.branch32(CCallHelpers::Below, scratch2GPR, CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset())).linkTo(storeResult, &jit);

            jit.add32(CCallHelpers::TrustedImm32(1), scratch2GPR);
            jit.store32(scratch2GPR, CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset()));
            jit.sub32(CCallHelpers::TrustedImm32(1), scratch2GPR);
            jit.jump().linkTo(storeResult, &jit);
        } else {
            isOutOfBounds.link(&jit);
            failAndIgnore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfVectorLength())));
            if (stubInfo.m_arrayProfileGPR != InvalidGPRReg)
                jit.or32(CCallHelpers::TrustedImm32(static_cast<uint32_t>(ArrayProfileFlag::MayStoreHole)), CCallHelpers::Address(stubInfo.m_arrayProfileGPR, ArrayProfile::offsetOfArrayProfileFlags()));
            jit.add32(CCallHelpers::TrustedImm32(1), propertyGPR, scratch2GPR);
            jit.store32(scratch2GPR, CCallHelpers::Address(scratchGPR, Butterfly::offsetOfPublicLength()));
            jit.jump().linkTo(storeResult, &jit);

        }

        if (allocator.didReuseRegisters()) {
            if (!failAndIgnore.empty()) {
                failAndIgnore.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                m_failAndIgnore.append(jit.jump());
            }
            if (!failAndRepatch.empty()) {
                failAndRepatch.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                m_failAndRepatch.append(jit.jump());
            }
        } else {
            m_failAndIgnore.append(failAndIgnore);
            m_failAndRepatch.append(failAndRepatch);
        }
        return;
    }

    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store: {
        ASSERT(!accessCase.viaGlobalProxy());
        // This code is written such that the result could alias with the base or the property.

        TypedArrayType type = toTypedArrayType(accessCase.m_type);
        bool isResizableOrGrowableShared = forResizableTypedArray(accessCase.m_type);

        GPRReg propertyGPR = stubInfo.propertyGPR();

        fallThrough.append(jit.branch8(CCallHelpers::NotEqual, CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), CCallHelpers::TrustedImm32(typeForTypedArrayType(type))));
        if (!isResizableOrGrowableShared)
            fallThrough.append(jit.branchTest8(CCallHelpers::NonZero, CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfMode()), CCallHelpers::TrustedImm32(isResizableOrGrowableSharedMode)));

        if (isInt(type))
            m_failAndRepatch.append(jit.branchIfNotInt32(valueRegs));
        else {
            ASSERT(isFloat(type));
            RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
            auto doubleCase = jit.branchIfNotInt32(valueRegs);
            jit.convertInt32ToDouble(valueRegs.payloadGPR(), m_scratchFPR);
            auto ready = jit.jump();
            doubleCase.link(&jit);
#if USE(JSVALUE64)
            m_failAndRepatch.append(jit.branchIfNotNumber(valueRegs.payloadGPR()));
            jit.unboxDoubleWithoutAssertions(valueRegs.payloadGPR(), scratchGPR, m_scratchFPR);
#else
            m_failAndRepatch.append(jit.branch32(CCallHelpers::Above, valueRegs.tagGPR(), CCallHelpers::TrustedImm32(JSValue::LowestTag)));
            jit.unboxDouble(valueRegs, m_scratchFPR);
#endif
            ready.link(&jit);
        }

        if (!isResizableOrGrowableShared) {
            CCallHelpers::Address addressOfLength = CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength());
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
            // The length is a UCPURegister, so either 32 or 64 bits depending on the platform.
            m_failAndRepatch.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, addressOfLength));
#else
            m_failAndRepatch.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, addressOfLength));
#endif
        }

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(
            jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList failAndRepatchAfterRestore;
        if (isResizableOrGrowableShared) {
            jit.loadTypedArrayLength(baseGPR, scratch2GPR, scratchGPR, scratch2GPR, type);
            jit.signExtend32ToPtr(propertyGPR, scratchGPR);
#if USE(LARGE_TYPED_ARRAYS)
            // The length is a size_t, so either 32 or 64 bits depending on the platform.
            failAndRepatchAfterRestore.append(jit.branch64(CCallHelpers::AboveOrEqual, scratchGPR, scratch2GPR));
#else
            failAndRepatchAfterRestore.append(jit.branch32(CCallHelpers::AboveOrEqual, propertyGPR, scratch2GPR));
#endif
        }

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
#else
        jit.load32(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfLength()), scratchGPR);
#endif
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfVector()), scratch2GPR);
        jit.cageConditionally(Gigacage::Primitive, scratch2GPR, scratchGPR, scratchGPR);
        jit.signExtend32ToPtr(propertyGPR, scratchGPR);
        if (isInt(type)) {
            if (isClamped(type)) {
                ASSERT(elementSize(type) == 1);
                ASSERT(!JSC::isSigned(type));
                jit.getEffectiveAddress(CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne), scratch2GPR);
                jit.move(valueRegs.payloadGPR(), scratchGPR);
                auto inBounds = jit.branch32(CCallHelpers::BelowOrEqual, scratchGPR, CCallHelpers::TrustedImm32(0xff));
                auto tooBig = jit.branch32(CCallHelpers::GreaterThan, scratchGPR, CCallHelpers::TrustedImm32(0xff));
                jit.xor32(scratchGPR, scratchGPR);
                auto clamped = jit.jump();
                tooBig.link(&jit);
                jit.move(CCallHelpers::TrustedImm32(0xff), scratchGPR);
                clamped.link(&jit);
                inBounds.link(&jit);
                jit.store8(scratchGPR, CCallHelpers::Address(scratch2GPR));
            } else {
                switch (elementSize(type)) {
                case 1:
                    jit.store8(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesOne));
                    break;
                case 2:
                    jit.store16(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesTwo));
                    break;
                case 4:
                    jit.store32(valueRegs.payloadGPR(), CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour));
                    break;
                default:
                    CRASH();
                }
            }
        } else {
            ASSERT(isFloat(type));
            RELEASE_ASSERT(m_scratchFPR != InvalidFPRReg);
            switch (elementSize(type)) {
            case 4:
                jit.convertDoubleToFloat(m_scratchFPR, m_scratchFPR);
                jit.storeFloat(m_scratchFPR, CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesFour));
                break;
            case 8: {
                jit.storeDouble(m_scratchFPR, CCallHelpers::BaseIndex(scratch2GPR, scratchGPR, CCallHelpers::TimesEight));
                break;
            }
            default:
                CRASH();
            }
        }

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (isResizableOrGrowableShared) {
            failAndRepatchAfterRestore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        }
        return;
    }

    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
        emitDefaultGuard();

        fallThrough.append(
            jit.branchPtr(
                CCallHelpers::NotEqual, stubInfo.prototypeGPR(),
                CCallHelpers::TrustedImmPtr(accessCase.as<InstanceOfAccessCase>().prototype())));
        break;

    case AccessCase::InstanceOfGeneric: {
        ASSERT(!accessCase.viaGlobalProxy());
        GPRReg prototypeGPR = stubInfo.prototypeGPR();
        // Legend: value = `base instanceof prototypeGPR`.

        GPRReg valueGPR = valueRegs.payloadGPR();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratch2GPR = allocator.allocateScratchGPR();

        if (!stubInfo.prototypeIsKnownObject)
            m_failAndIgnore.append(jit.branchIfNotObject(prototypeGPR));

        ScratchRegisterAllocator::PreservedState preservedState =
            allocator.preserveReusedRegistersByPushing(
                jit,
                ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
        CCallHelpers::JumpList failAndIgnore;

        jit.move(baseGPR, valueGPR);

        CCallHelpers::Label loop(&jit);

#if USE(JSVALUE64)
        JSValueRegs resultRegs(scratch2GPR);
#else
        JSValueRegs resultRegs(scratchGPR, scratch2GPR);
#endif

        jit.emitLoadPrototype(vm, valueGPR, resultRegs, failAndIgnore);
        jit.move(scratch2GPR, valueGPR);

        CCallHelpers::Jump isInstance = jit.branchPtr(CCallHelpers::Equal, valueGPR, prototypeGPR);

#if USE(JSVALUE64)
        jit.branchIfCell(JSValueRegs(valueGPR)).linkTo(loop, &jit);
#else
        jit.branchTestPtr(CCallHelpers::NonZero, valueGPR).linkTo(loop, &jit);
#endif

        jit.boxBooleanPayload(false, valueGPR);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        isInstance.link(&jit);
        jit.boxBooleanPayload(true, valueGPR);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            failAndIgnore.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndIgnore.append(jit.jump());
        } else
            m_failAndIgnore.append(failAndIgnore);
        return;
    }

    case AccessCase::CheckPrivateBrand: {
        emitDefaultGuard();
        succeed();
        return;
    }

    case AccessCase::IndexedMegamorphicLoad: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());

        CCallHelpers::JumpList slowCases;

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        GPRReg scratch4GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList notString;
        GPRReg propertyGPR = m_stubInfo->propertyGPR();
        if (!m_stubInfo->propertyIsString) {
            slowCases.append(jit.branchIfNotCell(propertyGPR));
            slowCases.append(jit.branchIfNotString(propertyGPR));
        }

        jit.loadPtr(CCallHelpers::Address(propertyGPR, JSString::offsetOfValue()), scratch4GPR);
        slowCases.append(jit.branchIfRopeStringImpl(scratch4GPR));
        slowCases.append(jit.branchTest32(CCallHelpers::Zero, CCallHelpers::Address(scratch4GPR, StringImpl::flagsOffset()), CCallHelpers::TrustedImm32(StringImpl::flagIsAtom())));

        slowCases.append(jit.loadMegamorphicProperty(vm, baseGPR, scratch4GPR, nullptr, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR));

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);
#endif
        return;
    }

    case AccessCase::LoadMegamorphic: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());
        CCallHelpers::JumpList failAndRepatch;
        auto* uid = accessCase.m_identifier.uid();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        auto slowCases = jit.loadMegamorphicProperty(vm, baseGPR, InvalidGPRReg, uid, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR);

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);
#endif
        return;
    }

    case AccessCase::StoreMegamorphic: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());
        CCallHelpers::JumpList failAndRepatch;
        auto* uid = accessCase.m_identifier.uid();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        auto slowCases = jit.storeMegamorphicProperty(vm, baseGPR, InvalidGPRReg, uid, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR);

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);
#endif
        return;
    }

    case AccessCase::IndexedMegamorphicStore: {
#if USE(JSVALUE64)
        ASSERT(!accessCase.viaGlobalProxy());

        CCallHelpers::JumpList slowCases;

        auto allocator = makeDefaultScratchAllocator(scratchGPR);
        GPRReg scratch2GPR = allocator.allocateScratchGPR();
        GPRReg scratch3GPR = allocator.allocateScratchGPR();
        GPRReg scratch4GPR = allocator.allocateScratchGPR();

        ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

        CCallHelpers::JumpList notString;
        GPRReg propertyGPR = m_stubInfo->propertyGPR();
        if (!m_stubInfo->propertyIsString) {
            slowCases.append(jit.branchIfNotCell(propertyGPR));
            slowCases.append(jit.branchIfNotString(propertyGPR));
        }

        jit.loadPtr(CCallHelpers::Address(propertyGPR, JSString::offsetOfValue()), scratch4GPR);
        slowCases.append(jit.branchIfRopeStringImpl(scratch4GPR));
        slowCases.append(jit.branchTest32(CCallHelpers::Zero, CCallHelpers::Address(scratch4GPR, StringImpl::flagsOffset()), CCallHelpers::TrustedImm32(StringImpl::flagIsAtom())));

        slowCases.append(jit.storeMegamorphicProperty(vm, baseGPR, scratch4GPR, nullptr, valueRegs.payloadGPR(), scratchGPR, scratch2GPR, scratch3GPR));

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        if (allocator.didReuseRegisters()) {
            slowCases.link(&jit);
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            m_failAndRepatch.append(jit.jump());
        } else
            m_failAndRepatch.append(slowCases);
#endif
        return;
    }

    default:
        emitDefaultGuard();
        break;
    }

    generateImpl(accessCase);
}

void InlineCacheCompiler::generate(AccessCase& accessCase)
{
    RELEASE_ASSERT(accessCase.state() == AccessCase::Committed);
    RELEASE_ASSERT(m_stubInfo->hasConstantIdentifier);
    accessCase.m_state = AccessCase::Generated;

    accessCase.checkConsistency(*m_stubInfo);

    generateImpl(accessCase);
}

void InlineCacheCompiler::generateImpl(AccessCase& accessCase)
{
    SuperSamplerScope superSamplerScope(false);
    dataLogLnIf(InlineCacheCompilerInternal::verbose, "\n\nGenerating code for: ", accessCase);

    ASSERT(accessCase.m_state == AccessCase::Generated); // We rely on the callers setting this for us.

    CCallHelpers& jit = *m_jit;
    VM& vm = m_vm;
    CodeBlock* codeBlock = jit.codeBlock();
    ECMAMode ecmaMode = m_ecmaMode;
    StructureStubInfo& stubInfo = *m_stubInfo;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg thisGPR = stubInfo.thisValueIsInExtraGPR() ? stubInfo.thisGPR() : baseGPR;
    GPRReg scratchGPR = m_scratchGPR;

    for (const ObjectPropertyCondition& condition : accessCase.m_conditionSet) {
        RELEASE_ASSERT(!accessCase.m_polyProtoAccessChain);

        if (condition.isWatchableAssumingImpurePropertyWatchpoint(PropertyCondition::WatchabilityEffort::EnsureWatchability)) {
            installWatchpoint(codeBlock, condition);
            continue;
        }

        // For now, we only allow equivalence when it's watchable.
        RELEASE_ASSERT(condition.condition().kind() != PropertyCondition::Equivalence);

        if (!condition.structureEnsuresValidityAssumingImpurePropertyWatchpoint(Concurrency::MainThread)) {
            // The reason why this cannot happen is that we require that PolymorphicAccess calls
            // AccessCase::generate() only after it has verified that
            // AccessCase::couldStillSucceed() returned true.

            dataLog("This condition is no longer met: ", condition, "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }

        // We will emit code that has a weak reference that isn't otherwise listed anywhere.
        Structure* structure = condition.object()->structure();
        m_weakStructures.append(structure->id());

        jit.move(CCallHelpers::TrustedImmPtr(condition.object()), scratchGPR);
        m_failAndRepatch.append(
            jit.branchStructure(
                CCallHelpers::NotEqual,
                CCallHelpers::Address(scratchGPR, JSCell::structureIDOffset()),
                structure));
    }

    switch (accessCase.m_type) {
    case AccessCase::InHit:
    case AccessCase::InMiss:
        jit.boxBoolean(accessCase.m_type == AccessCase::InHit, valueRegs);
        succeed();
        return;

    case AccessCase::Miss:
        jit.moveTrustedValue(jsUndefined(), valueRegs);
        succeed();
        return;

    case AccessCase::InstanceOfHit:
    case AccessCase::InstanceOfMiss:
        jit.boxBooleanPayload(accessCase.m_type == AccessCase::InstanceOfHit, valueRegs.payloadGPR());
        succeed();
        return;

    case AccessCase::Load:
    case AccessCase::GetGetter:
    case AccessCase::Getter:
    case AccessCase::Setter:
    case AccessCase::CustomValueGetter:
    case AccessCase::CustomAccessorGetter:
    case AccessCase::CustomValueSetter:
    case AccessCase::CustomAccessorSetter:
    case AccessCase::IntrinsicGetter: {
        GPRReg valueRegsPayloadGPR = valueRegs.payloadGPR();

        Structure* currStructure = accessCase.hasAlternateBase() ? accessCase.alternateBase()->structure() : accessCase.structure();
        if (isValidOffset(accessCase.m_offset))
            currStructure->startWatchingPropertyForReplacements(vm, accessCase.offset());

        bool doesPropertyStorageLoads = accessCase.m_type == AccessCase::Load
            || accessCase.m_type == AccessCase::GetGetter
            || accessCase.m_type == AccessCase::Getter
            || accessCase.m_type == AccessCase::Setter
            || accessCase.m_type == AccessCase::IntrinsicGetter;

        bool takesPropertyOwnerAsCFunctionArgument = accessCase.m_type == AccessCase::CustomValueGetter || accessCase.m_type == AccessCase::CustomValueSetter;

        GPRReg receiverGPR = baseGPR;
        GPRReg propertyOwnerGPR;

        if (accessCase.m_polyProtoAccessChain) {
            // This isn't pretty, but we know we got here via generateWithGuard,
            // and it left the baseForAccess inside scratchGPR. We could re-derive the base,
            // but it'd require emitting the same code to load the base twice.
            propertyOwnerGPR = scratchGPR;
        } else if (accessCase.hasAlternateBase()) {
            jit.move(
                CCallHelpers::TrustedImmPtr(accessCase.alternateBase()), scratchGPR);
            propertyOwnerGPR = scratchGPR;
        } else if (accessCase.viaGlobalProxy() && doesPropertyStorageLoads) {
            // We only need this when loading an inline or out of line property. For customs accessors,
            // we can invoke with a receiver value that is a JSGlobalProxy. For custom values, we unbox to the
            // JSGlobalProxy's target. For getters/setters, we'll also invoke them with the JSGlobalProxy as |this|,
            // but we need to load the actual GetterSetter cell from the JSGlobalProxy's target.

            if (accessCase.m_type == AccessCase::Getter || accessCase.m_type == AccessCase::Setter || accessCase.m_type == AccessCase::IntrinsicGetter)
                propertyOwnerGPR = scratchGPR;
            else
                propertyOwnerGPR = valueRegsPayloadGPR;

            jit.loadPtr(
                CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), propertyOwnerGPR);
        } else if (accessCase.viaGlobalProxy() && takesPropertyOwnerAsCFunctionArgument) {
            propertyOwnerGPR = scratchGPR;
            jit.loadPtr(
                CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), propertyOwnerGPR);
        } else
            propertyOwnerGPR = receiverGPR;

        GPRReg loadedValueGPR = InvalidGPRReg;
        if (doesPropertyStorageLoads) {
            if (accessCase.m_type == AccessCase::Load || accessCase.m_type == AccessCase::GetGetter)
                loadedValueGPR = valueRegsPayloadGPR;
            else
                loadedValueGPR = scratchGPR;

            ASSERT((accessCase.m_type != AccessCase::Getter && accessCase.m_type != AccessCase::Setter && accessCase.m_type != AccessCase::IntrinsicGetter) || loadedValueGPR != baseGPR);
            ASSERT(accessCase.m_type != AccessCase::Setter || loadedValueGPR != valueRegsPayloadGPR);

            GPRReg storageGPR;
            if (isInlineOffset(accessCase.m_offset))
                storageGPR = propertyOwnerGPR;
            else {
                jit.loadPtr(
                    CCallHelpers::Address(propertyOwnerGPR, JSObject::butterflyOffset()),
                    loadedValueGPR);
                storageGPR = loadedValueGPR;
            }

#if USE(JSVALUE64)
            jit.load64(
                CCallHelpers::Address(storageGPR, offsetRelativeToBase(accessCase.m_offset)), loadedValueGPR);
#else
            if (accessCase.m_type == AccessCase::Load || accessCase.m_type == AccessCase::GetGetter) {
                jit.loadValue(
                    CCallHelpers::Address(storageGPR, offsetRelativeToBase(accessCase.m_offset)),
                    JSValueRegs { valueRegs.tagGPR(), loadedValueGPR });

            } else {
                jit.load32(
                    CCallHelpers::Address(storageGPR, offsetRelativeToBase(accessCase.m_offset) + PayloadOffset),
                    loadedValueGPR);
            }
#endif
        }

        if (accessCase.m_type == AccessCase::Load || accessCase.m_type == AccessCase::GetGetter) {
            succeed();
            return;
        }

        if (accessCase.m_type == AccessCase::CustomAccessorGetter && accessCase.as<GetterSetterAccessCase>().domAttribute()) {
            auto& access = accessCase.as<GetterSetterAccessCase>();
            // We do not need to emit CheckDOM operation since structure check ensures
            // that the structure of the given base value is accessCase.structure()! So all we should
            // do is performing the CheckDOM thingy in IC compiling time here.
            if (!accessCase.structure()->classInfoForCells()->isSubClassOf(access.domAttribute()->classInfo)) {
                m_failAndIgnore.append(jit.jump());
                return;
            }

            if (Options::useDOMJIT() && access.domAttribute()->domJIT) {
                emitDOMJITGetter(access, access.domAttribute()->domJIT, receiverGPR);
                return;
            }
        }

        if (accessCase.m_type == AccessCase::IntrinsicGetter) {
            jit.loadPtr(CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfGetter()), loadedValueGPR);
            m_failAndIgnore.append(jit.branchPtr(CCallHelpers::NotEqual, loadedValueGPR, CCallHelpers::TrustedImmPtr(accessCase.as<IntrinsicGetterAccessCase>().intrinsicFunction())));
            emitIntrinsicGetter(accessCase.as<IntrinsicGetterAccessCase>());
            return;
        }

        // Stuff for custom getters/setters.
        CCallHelpers::Call operationCall;


        // This also does the necessary calculations of whether or not we're an
        // exception handling call site.
        InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall();

        auto restoreLiveRegistersFromStackForCall = [&](InlineCacheCompiler::SpillState& spillState, bool callHasReturnValue) {
            RegisterSet dontRestore;
            if (callHasReturnValue) {
                // This is the result value. We don't want to overwrite the result with what we stored to the stack.
                // We sometimes have to store it to the stack just in case we throw an exception and need the original value.
                dontRestore.add(valueRegs, IgnoreVectors);
            }
            this->restoreLiveRegistersFromStackForCall(spillState, dontRestore);
        };

        if (codeBlock->useDataIC()) {
            callSiteIndexForExceptionHandlingOrOriginal();
            jit.transfer32(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
        } else
            jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

        if (accessCase.m_type == AccessCase::Getter || accessCase.m_type == AccessCase::Setter) {
            auto& access = accessCase.as<GetterSetterAccessCase>();
            ASSERT(baseGPR != loadedValueGPR);
            ASSERT(accessCase.m_type != AccessCase::Setter || valueRegsPayloadGPR != loadedValueGPR);

            JSGlobalObject* globalObject = m_globalObject;

            // Create a JS call using a JS call inline cache. Assume that:
            //
            // - SP is aligned and represents the extent of the calling compiler's stack usage.
            //
            // - FP is set correctly (i.e. it points to the caller's call frame header).
            //
            // - SP - FP is an aligned difference.
            //
            // - Any byte between FP (exclusive) and SP (inclusive) could be live in the calling
            //   code.
            //
            // Therefore, we temporarily grow the stack for the purpose of the call and then
            // shrink it after.

            setSpillStateForJSCall(spillState);

            RELEASE_ASSERT(!access.callLinkInfo());
            auto* callLinkInfo = m_callLinkInfos.add(stubInfo.codeOrigin, codeBlock->useDataIC() ? CallLinkInfo::UseDataIC::Yes : CallLinkInfo::UseDataIC::No, nullptr);
            access.m_callLinkInfo = callLinkInfo;

            // FIXME: If we generated a polymorphic call stub that jumped back to the getter
            // stub, which then jumped back to the main code, then we'd have a reachability
            // situation that the GC doesn't know about. The GC would ensure that the polymorphic
            // call stub stayed alive, and it would ensure that the main code stayed alive, but
            // it wouldn't know that the getter stub was alive. Ideally JIT stub routines would
            // be GC objects, and then we'd be able to say that the polymorphic call stub has a
            // reference to the getter stub.
            // https://bugs.webkit.org/show_bug.cgi?id=148914
            callLinkInfo->disallowStubs();

            callLinkInfo->setUpCall(CallLinkInfo::Call);

            CCallHelpers::JumpList done;

            // There is a "this" argument.
            unsigned numberOfParameters = 1;
            // ... and a value argument if we're calling a setter.
            if (accessCase.m_type == AccessCase::Setter)
                numberOfParameters++;

            // Get the accessor; if there ain't one then the result is jsUndefined().
            // Note that GetterSetter always has cells for both. If it is not set (like, getter exits, but setter is not set), Null{Getter,Setter}Function is stored.
            std::optional<CCallHelpers::Jump> returnUndefined;
            if (accessCase.m_type == AccessCase::Setter) {
                jit.loadPtr(
                    CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfSetter()),
                    loadedValueGPR);
                if (ecmaMode.isStrict()) {
                    CCallHelpers::Jump shouldNotThrowError = jit.branchIfNotType(loadedValueGPR, NullSetterFunctionType);
                    // We replace setter with this AccessCase's JSGlobalObject::nullSetterStrictFunction, which will throw an error with the right JSGlobalObject.
                    jit.move(CCallHelpers::TrustedImmPtr(globalObject->nullSetterStrictFunction()), loadedValueGPR);
                    shouldNotThrowError.link(&jit);
                }
            } else {
                jit.loadPtr(
                    CCallHelpers::Address(loadedValueGPR, GetterSetter::offsetOfGetter()),
                    loadedValueGPR);
                returnUndefined = jit.branchIfType(loadedValueGPR, NullSetterFunctionType);
            }

            unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
            ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
            unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);

            unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf(stackAlignmentBytes(), numberOfBytesForCall);

            jit.subPtr(
                CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall),
                CCallHelpers::stackPointerRegister);

            CCallHelpers::Address calleeFrame = CCallHelpers::Address(
                CCallHelpers::stackPointerRegister,
                -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

            jit.store32(
                CCallHelpers::TrustedImm32(numberOfParameters),
                calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));

            jit.storeCell(
                loadedValueGPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));

            jit.storeCell(
                thisGPR,
                calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));

            if (accessCase.m_type == AccessCase::Setter) {
                jit.storeValue(
                    valueRegs,
                    calleeFrame.withOffset(
                        virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));
            }

            jit.move(loadedValueGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
#if USE(JSVALUE32_64)
            // We *always* know that the getter/setter, if non-null, is a cell.
            jit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), BaselineJITRegisters::Call::calleeJSR.tagGPR());
#endif
            auto [slowCase, dispatchLabel] = CallLinkInfo::emitFastPath(jit, callLinkInfo);
            auto doneLocation = jit.label();

            if (accessCase.m_type == AccessCase::Getter)
                jit.setupResults(valueRegs);
            done.append(jit.jump());

            if (!slowCase.empty()) {
                slowCase.link(&jit);
                CallLinkInfo::emitSlowPath(vm, jit, callLinkInfo);

                if (accessCase.m_type == AccessCase::Getter)
                    jit.setupResults(valueRegs);
                done.append(jit.jump());
            }

            if (returnUndefined) {
                ASSERT(accessCase.m_type == AccessCase::Getter);
                returnUndefined.value().link(&jit);
                jit.moveTrustedValue(jsUndefined(), valueRegs);
            }
            done.link(&jit);

            if (codeBlock->useDataIC()) {
                jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), m_scratchGPR);
                if (useHandlerIC())
                    jit.addPtr(CCallHelpers::TrustedImm32(-(sizeof(CallerFrameAndPC) + maxFrameExtentForSlowPathCall + m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), m_scratchGPR);
                else
                    jit.addPtr(CCallHelpers::TrustedImm32(-(m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), m_scratchGPR);
                jit.addPtr(m_scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
            } else {
                int stackPointerOffset = (codeBlock->stackPointerOffset() * sizeof(Register)) - m_preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation;
                jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
            }

            bool callHasReturnValue = accessCase.isGetter();
            restoreLiveRegistersFromStackForCall(spillState, callHasReturnValue);

            jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
                callLinkInfo->setCodeLocations(linkBuffer.locationOf<JSInternalPtrTag>(doneLocation));
            });
        } else {
            ASSERT(accessCase.m_type == AccessCase::CustomValueGetter || accessCase.m_type == AccessCase::CustomAccessorGetter || accessCase.m_type == AccessCase::CustomValueSetter || accessCase.m_type == AccessCase::CustomAccessorSetter);
            ASSERT(!doesPropertyStorageLoads); // Or we need an extra register. We rely on propertyOwnerGPR being correct here.

            // We do not need to keep globalObject alive since
            // 1. if it is CustomValue, the owner CodeBlock (even if JSGlobalObject* is one of CodeBlock that is inlined and held by DFG CodeBlock) must keep it alive.
            // 2. if it is CustomAccessor, structure should hold it.
            JSGlobalObject* globalObject = currStructure->globalObject();

            // Need to make room for the C call so any of our stack spillage isn't overwritten. It's
            // hard to track if someone did spillage or not, so we just assume that we always need
            // to make some space here.
            jit.makeSpaceOnStackForCCall();

            // Check if it is a super access
            GPRReg receiverForCustomGetGPR = baseGPR != thisGPR ? thisGPR : receiverGPR;

            // getter: EncodedJSValue (*GetValueFunc)(JSGlobalObject*, EncodedJSValue thisValue, PropertyName);
            // setter: bool (*PutValueFunc)(JSGlobalObject*, EncodedJSValue thisObject, EncodedJSValue value, PropertyName);
            // Custom values are passed the slotBase (the property holder), custom accessors are passed the thisValue (receiver).
            GPRReg baseForCustom = takesPropertyOwnerAsCFunctionArgument ? propertyOwnerGPR : receiverForCustomGetGPR;
            // We do not need to keep globalObject alive since the owner CodeBlock (even if JSGlobalObject* is one of CodeBlock that is inlined and held by DFG CodeBlock)
            // must keep it alive.
            if (accessCase.m_type == AccessCase::CustomValueGetter || accessCase.m_type == AccessCase::CustomAccessorGetter) {
                RELEASE_ASSERT(accessCase.m_identifier);
                if (Options::useJITCage()) {
                    jit.setupArguments<GetValueFuncWithPtr>(
                        CCallHelpers::TrustedImmPtr(globalObject),
                        CCallHelpers::CellValue(baseForCustom),
                        CCallHelpers::TrustedImmPtr(accessCase.uid()),
                        CCallHelpers::TrustedImmPtr(accessCase.as<GetterSetterAccessCase>().m_customAccessor.taggedPtr()));
                } else {
                    jit.setupArguments<GetValueFunc>(
                        CCallHelpers::TrustedImmPtr(globalObject),
                        CCallHelpers::CellValue(baseForCustom),
                        CCallHelpers::TrustedImmPtr(accessCase.uid()));
                }
            } else {
                if (Options::useJITCage()) {
                    jit.setupArguments<PutValueFuncWithPtr>(
                        CCallHelpers::TrustedImmPtr(globalObject),
                        CCallHelpers::CellValue(baseForCustom),
                        valueRegs,
                        CCallHelpers::TrustedImmPtr(accessCase.uid()),
                        CCallHelpers::TrustedImmPtr(accessCase.as<GetterSetterAccessCase>().m_customAccessor.taggedPtr()));
                } else {
                    jit.setupArguments<PutValueFunc>(
                        CCallHelpers::TrustedImmPtr(globalObject),
                        CCallHelpers::CellValue(baseForCustom),
                        valueRegs,
                        CCallHelpers::TrustedImmPtr(accessCase.uid()));
                }
            }
            jit.storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);

            auto type = accessCase.m_type;
            auto customAccessor = accessCase.as<GetterSetterAccessCase>().m_customAccessor;
            if (Options::useJITCage()) {
                if (type == AccessCase::CustomValueGetter || type == AccessCase::CustomAccessorGetter)
                    jit.callOperation<OperationPtrTag>(vmEntryCustomGetter);
                else
                    jit.callOperation<OperationPtrTag>(vmEntryCustomSetter);
            } else
                jit.callOperation<CustomAccessorPtrTag>(customAccessor);

            if (accessCase.m_type == AccessCase::CustomValueGetter || accessCase.m_type == AccessCase::CustomAccessorGetter)
                jit.setupResults(valueRegs);
            jit.reclaimSpaceOnStackForCCall();

            CCallHelpers::Jump noException =
            jit.emitExceptionCheck(vm, CCallHelpers::InvertedExceptionCheck);

            restoreLiveRegistersFromStackForCallWithThrownException(spillState);
            emitExplicitExceptionHandler();

            noException.link(&jit);
            bool callHasReturnValue = accessCase.isGetter();
            restoreLiveRegistersFromStackForCall(spillState, callHasReturnValue);
        }
        succeed();
        return;
    }

    case AccessCase::Replace: {
        GPRReg base = baseGPR;
        if (accessCase.viaGlobalProxy()) {
            // This aint pretty, but the path that structure checks loads the real base into scratchGPR.
            base = scratchGPR;
        }

        if (isInlineOffset(accessCase.m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    base,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(accessCase.m_offset) * sizeof(JSValue)));
        } else {
            jit.loadPtr(CCallHelpers::Address(base, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    scratchGPR, offsetInButterfly(accessCase.m_offset) * sizeof(JSValue)));
        }

        if (accessCase.viaGlobalProxy()) {
            CCallHelpers::JumpList skipBarrier;
            skipBarrier.append(jit.branchIfNotCell(valueRegs));
            if (!isInlineOffset(accessCase.m_offset))
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);
            skipBarrier.append(jit.barrierBranch(vm, scratchGPR, scratchGPR));

            jit.loadPtr(CCallHelpers::Address(baseGPR, JSGlobalProxy::targetOffset()), scratchGPR);

            auto spillState = preserveLiveRegistersToStackForCallWithoutExceptions();

            jit.setupArguments<decltype(operationWriteBarrierSlowPath)>(CCallHelpers::TrustedImmPtr(&vm), scratchGPR);
            jit.prepareCallOperation(vm);
            jit.callOperation<OperationPtrTag>(operationWriteBarrierSlowPath);
            restoreLiveRegistersFromStackForCall(spillState);

            skipBarrier.link(&jit);
        }

        succeed();
        return;
    }

    case AccessCase::Transition: {
        ASSERT(!accessCase.viaGlobalProxy());
        // AccessCase::createTransition() should have returned null if this wasn't true.
        RELEASE_ASSERT(GPRInfo::numberOfRegisters >= 6 || !accessCase.structure()->outOfLineCapacity() || accessCase.structure()->outOfLineCapacity() == accessCase.newStructure()->outOfLineCapacity());

        // NOTE: This logic is duplicated in AccessCase::doesCalls(). It's important that doesCalls() knows
        // exactly when this would make calls.
        bool allocating = accessCase.newStructure()->outOfLineCapacity() != accessCase.structure()->outOfLineCapacity();
        bool reallocating = allocating && accessCase.structure()->outOfLineCapacity();
        bool allocatingInline = allocating && !accessCase.structure()->couldHaveIndexingHeader();

        auto allocator = makeDefaultScratchAllocator(scratchGPR);

        GPRReg scratchGPR2 = InvalidGPRReg;
        GPRReg scratchGPR3 = InvalidGPRReg;
        if (allocatingInline) {
            scratchGPR2 = allocator.allocateScratchGPR();
            scratchGPR3 = allocator.allocateScratchGPR();
        }

        ScratchRegisterAllocator::PreservedState preservedState =
            allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

        CCallHelpers::JumpList slowPath;

        ASSERT(accessCase.structure()->transitionWatchpointSetHasBeenInvalidated());

        if (allocating) {
            size_t newSize = accessCase.newStructure()->outOfLineCapacity() * sizeof(JSValue);

            if (allocatingInline) {
                Allocator allocator = vm.jsValueGigacageAuxiliarySpace().allocatorForNonInline(newSize, AllocatorForMode::AllocatorIfExists);

                jit.emitAllocate(scratchGPR, JITAllocator::constant(allocator), scratchGPR2, scratchGPR3, slowPath);
                jit.addPtr(CCallHelpers::TrustedImm32(newSize + sizeof(IndexingHeader)), scratchGPR);

                size_t oldSize = accessCase.structure()->outOfLineCapacity() * sizeof(JSValue);
                ASSERT(newSize > oldSize);

                if (reallocating) {
                    // Handle the case where we are reallocating (i.e. the old structure/butterfly
                    // already had out-of-line property storage).

                    jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR3);

                    // We have scratchGPR = new storage, scratchGPR3 = old storage,
                    // scratchGPR2 = available
                    for (size_t offset = 0; offset < oldSize; offset += sizeof(void*)) {
                        jit.loadPtr(
                            CCallHelpers::Address(
                                scratchGPR3,
                                -static_cast<ptrdiff_t>(
                                    offset + sizeof(JSValue) + sizeof(void*))),
                            scratchGPR2);
                        jit.storePtr(
                            scratchGPR2,
                            CCallHelpers::Address(
                                scratchGPR,
                                -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))));
                    }
                }

                for (size_t offset = oldSize; offset < newSize; offset += sizeof(void*))
                    jit.storePtr(CCallHelpers::TrustedImmPtr(nullptr), CCallHelpers::Address(scratchGPR, -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))));
            } else {
                // Handle the case where we are allocating out-of-line using an operation.
                RegisterSet extraRegistersToPreserve;
                extraRegistersToPreserve.add(baseGPR, IgnoreVectors);
                extraRegistersToPreserve.add(valueRegs, IgnoreVectors);
                InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall(extraRegistersToPreserve);

                if (codeBlock->useDataIC()) {
                    callSiteIndexForExceptionHandlingOrOriginal();
                    jit.transfer32(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
                } else
                    jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

                jit.makeSpaceOnStackForCCall();

                if (!reallocating) {
                    jit.setupArguments<decltype(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR);
                    jit.prepareCallOperation(vm);
                    jit.callOperation<OperationPtrTag>(operationReallocateButterflyToHavePropertyStorageWithInitialCapacity);
                } else {
                    // Handle the case where we are reallocating (i.e. the old structure/butterfly
                    // already had out-of-line property storage).
                    jit.setupArguments<decltype(operationReallocateButterflyToGrowPropertyStorage)>(CCallHelpers::TrustedImmPtr(&vm), baseGPR, CCallHelpers::TrustedImm32(newSize / sizeof(JSValue)));
                    jit.prepareCallOperation(vm);
                    jit.callOperation<OperationPtrTag>(operationReallocateButterflyToGrowPropertyStorage);
                }

                jit.reclaimSpaceOnStackForCCall();
                jit.move(GPRInfo::returnValueGPR, scratchGPR);

                CCallHelpers::Jump noException = jit.emitExceptionCheck(vm, CCallHelpers::InvertedExceptionCheck);

                restoreLiveRegistersFromStackForCallWithThrownException(spillState);
                emitExplicitExceptionHandler();

                noException.link(&jit);
                RegisterSet resultRegisterToExclude;
                resultRegisterToExclude.add(scratchGPR, IgnoreVectors);
                restoreLiveRegistersFromStackForCall(spillState, resultRegisterToExclude);
            }
        }

        if (isInlineOffset(accessCase.m_offset)) {
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(
                    baseGPR,
                    JSObject::offsetOfInlineStorage() +
                    offsetInInlineStorage(accessCase.m_offset) * sizeof(JSValue)));
        } else {
            if (!allocating)
                jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            jit.storeValue(
                valueRegs,
                CCallHelpers::Address(scratchGPR, offsetInButterfly(accessCase.m_offset) * sizeof(JSValue)));
        }

        if (allocatingInline) {
            // If we were to have any indexed properties, then we would need to update the indexing mask on the base object.
            RELEASE_ASSERT(!accessCase.newStructure()->couldHaveIndexingHeader());
            // We set the new butterfly and the structure last. Doing it this way ensures that
            // whatever we had done up to this point is forgotten if we choose to branch to slow
            // path.
            jit.nukeStructureAndStoreButterfly(vm, scratchGPR, baseGPR);
        }

        uint32_t structureBits = bitwise_cast<uint32_t>(accessCase.newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        succeed();

        // We will have a slow path if we were allocating without the help of an operation.
        if (allocatingInline) {
            if (allocator.didReuseRegisters()) {
                slowPath.link(&jit);
                allocator.restoreReusedRegistersByPopping(jit, preservedState);
                m_failAndIgnore.append(jit.jump());
            } else
                m_failAndIgnore.append(slowPath);
        } else
            RELEASE_ASSERT(slowPath.empty());
        return;
    }

    case AccessCase::Delete: {
        ASSERT(accessCase.structure()->transitionWatchpointSetHasBeenInvalidated());
        ASSERT(accessCase.newStructure()->transitionKind() == TransitionKind::PropertyDeletion);
        ASSERT(baseGPR != scratchGPR);

        if (isInlineOffset(accessCase.m_offset))
            jit.storeTrustedValue(JSValue(), CCallHelpers::Address(baseGPR, JSObject::offsetOfInlineStorage() + offsetInInlineStorage(accessCase.m_offset) * sizeof(JSValue)));
        else {
            jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
            jit.storeTrustedValue(JSValue(), CCallHelpers::Address(scratchGPR, offsetInButterfly(accessCase.m_offset) * sizeof(JSValue)));
        }

        uint32_t structureBits = bitwise_cast<uint32_t>(accessCase.newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        jit.move(MacroAssembler::TrustedImm32(true), valueRegs.payloadGPR());

        succeed();
        return;
    }

    case AccessCase::SetPrivateBrand: {
        ASSERT(accessCase.structure()->transitionWatchpointSetHasBeenInvalidated());
        ASSERT(accessCase.newStructure()->transitionKind() == TransitionKind::SetBrand);

        uint32_t structureBits = bitwise_cast<uint32_t>(accessCase.newStructure()->id());
        jit.store32(
            CCallHelpers::TrustedImm32(structureBits),
            CCallHelpers::Address(baseGPR, JSCell::structureIDOffset()));

        succeed();
        return;
    }

    case AccessCase::DeleteNonConfigurable: {
        jit.move(MacroAssembler::TrustedImm32(false), valueRegs.payloadGPR());
        succeed();
        return;
    }

    case AccessCase::DeleteMiss: {
        jit.move(MacroAssembler::TrustedImm32(true), valueRegs.payloadGPR());
        succeed();
        return;
    }

    case AccessCase::ArrayLength: {
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, ArrayStorage::lengthOffset()), scratchGPR);
        m_failAndIgnore.append(
            jit.branch32(CCallHelpers::LessThan, scratchGPR, CCallHelpers::TrustedImm32(0)));
        jit.boxInt32(scratchGPR, valueRegs);
        succeed();
        return;
    }

    case AccessCase::StringLength: {
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSString::offsetOfValue()), scratchGPR);
        auto isRope = jit.branchIfRopeStringImpl(scratchGPR);
        jit.load32(CCallHelpers::Address(scratchGPR, StringImpl::lengthMemoryOffset()), valueRegs.payloadGPR());
        auto done = jit.jump();

        isRope.link(&jit);
        jit.load32(CCallHelpers::Address(baseGPR, JSRopeString::offsetOfLength()), valueRegs.payloadGPR());

        done.link(&jit);
        jit.boxInt32(valueRegs.payloadGPR(), valueRegs);
        succeed();
        return;
    }

    case AccessCase::IndexedNoIndexingMiss:
        jit.moveTrustedValue(jsUndefined(), valueRegs);
        succeed();
        return;

    case AccessCase::IndexedNoIndexingInMiss:
        jit.moveTrustedValue(jsBoolean(false), valueRegs);
        succeed();
        return;

    case AccessCase::DirectArgumentsLength:
    case AccessCase::ScopedArgumentsLength:
    case AccessCase::ModuleNamespaceLoad:
    case AccessCase::ProxyObjectHas:
    case AccessCase::ProxyObjectLoad:
    case AccessCase::ProxyObjectStore:
    case AccessCase::InstanceOfGeneric:
    case AccessCase::LoadMegamorphic:
    case AccessCase::StoreMegamorphic:
    case AccessCase::IndexedProxyObjectLoad:
    case AccessCase::IndexedMegamorphicLoad:
    case AccessCase::IndexedMegamorphicStore:
    case AccessCase::IndexedInt32Load:
    case AccessCase::IndexedDoubleLoad:
    case AccessCase::IndexedContiguousLoad:
    case AccessCase::IndexedArrayStorageLoad:
    case AccessCase::IndexedScopedArgumentsLoad:
    case AccessCase::IndexedDirectArgumentsLoad:
    case AccessCase::IndexedTypedArrayInt8Load:
    case AccessCase::IndexedTypedArrayUint8Load:
    case AccessCase::IndexedTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedTypedArrayInt16Load:
    case AccessCase::IndexedTypedArrayUint16Load:
    case AccessCase::IndexedTypedArrayInt32Load:
    case AccessCase::IndexedTypedArrayUint32Load:
    case AccessCase::IndexedTypedArrayFloat32Load:
    case AccessCase::IndexedTypedArrayFloat64Load:
    case AccessCase::IndexedResizableTypedArrayInt8Load:
    case AccessCase::IndexedResizableTypedArrayUint8Load:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedLoad:
    case AccessCase::IndexedResizableTypedArrayInt16Load:
    case AccessCase::IndexedResizableTypedArrayUint16Load:
    case AccessCase::IndexedResizableTypedArrayInt32Load:
    case AccessCase::IndexedResizableTypedArrayUint32Load:
    case AccessCase::IndexedResizableTypedArrayFloat32Load:
    case AccessCase::IndexedResizableTypedArrayFloat64Load:
    case AccessCase::IndexedStringLoad:
    case AccessCase::CheckPrivateBrand:
    case AccessCase::IndexedInt32Store:
    case AccessCase::IndexedDoubleStore:
    case AccessCase::IndexedContiguousStore:
    case AccessCase::IndexedArrayStorageStore:
    case AccessCase::IndexedTypedArrayInt8Store:
    case AccessCase::IndexedTypedArrayUint8Store:
    case AccessCase::IndexedTypedArrayUint8ClampedStore:
    case AccessCase::IndexedTypedArrayInt16Store:
    case AccessCase::IndexedTypedArrayUint16Store:
    case AccessCase::IndexedTypedArrayInt32Store:
    case AccessCase::IndexedTypedArrayUint32Store:
    case AccessCase::IndexedTypedArrayFloat32Store:
    case AccessCase::IndexedTypedArrayFloat64Store:
    case AccessCase::IndexedResizableTypedArrayInt8Store:
    case AccessCase::IndexedResizableTypedArrayUint8Store:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedStore:
    case AccessCase::IndexedResizableTypedArrayInt16Store:
    case AccessCase::IndexedResizableTypedArrayUint16Store:
    case AccessCase::IndexedResizableTypedArrayInt32Store:
    case AccessCase::IndexedResizableTypedArrayUint32Store:
    case AccessCase::IndexedResizableTypedArrayFloat32Store:
    case AccessCase::IndexedResizableTypedArrayFloat64Store:
    case AccessCase::IndexedInt32InHit:
    case AccessCase::IndexedDoubleInHit:
    case AccessCase::IndexedContiguousInHit:
    case AccessCase::IndexedArrayStorageInHit:
    case AccessCase::IndexedScopedArgumentsInHit:
    case AccessCase::IndexedDirectArgumentsInHit:
    case AccessCase::IndexedTypedArrayInt8InHit:
    case AccessCase::IndexedTypedArrayUint8InHit:
    case AccessCase::IndexedTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedTypedArrayInt16InHit:
    case AccessCase::IndexedTypedArrayUint16InHit:
    case AccessCase::IndexedTypedArrayInt32InHit:
    case AccessCase::IndexedTypedArrayUint32InHit:
    case AccessCase::IndexedTypedArrayFloat32InHit:
    case AccessCase::IndexedTypedArrayFloat64InHit:
    case AccessCase::IndexedResizableTypedArrayInt8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8InHit:
    case AccessCase::IndexedResizableTypedArrayUint8ClampedInHit:
    case AccessCase::IndexedResizableTypedArrayInt16InHit:
    case AccessCase::IndexedResizableTypedArrayUint16InHit:
    case AccessCase::IndexedResizableTypedArrayInt32InHit:
    case AccessCase::IndexedResizableTypedArrayUint32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat32InHit:
    case AccessCase::IndexedResizableTypedArrayFloat64InHit:
    case AccessCase::IndexedStringInHit:
        // These need to be handled by generateWithGuard(), since the guard is part of the
        // algorithm. We can be sure that nobody will call generate() directly for these since they
        // are not guarded by structure checks.
        RELEASE_ASSERT_NOT_REACHED();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void InlineCacheCompiler::emitDOMJITGetter(GetterSetterAccessCase& accessCase, const DOMJIT::GetterSetter* domJIT, GPRReg baseForGetGPR)
{
    CCallHelpers& jit = *m_jit;
    StructureStubInfo& stubInfo = *m_stubInfo;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg scratchGPR = m_scratchGPR;

    if (jit.codeBlock()->useDataIC()) {
        callSiteIndexForExceptionHandlingOrOriginal();
        jit.transfer32(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
    } else
        jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    // We construct the environment that can execute the DOMJIT::Snippet here.
    Ref<DOMJIT::CallDOMGetterSnippet> snippet = domJIT->compiler()();

    Vector<GPRReg> gpScratch;
    Vector<FPRReg> fpScratch;
    Vector<SnippetParams::Value> regs;

    auto allocator = makeDefaultScratchAllocator(scratchGPR);

    GPRReg paramBaseGPR = InvalidGPRReg;
    GPRReg paramGlobalObjectGPR = InvalidGPRReg;
    JSValueRegs paramValueRegs = valueRegs;
    GPRReg remainingScratchGPR = InvalidGPRReg;

    // valueRegs and baseForGetGPR may be the same. For example, in Baseline JIT, we pass the same regT0 for baseGPR and valueRegs.
    // In FTL, there is no constraint that the baseForGetGPR interferes with the result. To make implementation simple in
    // Snippet, Snippet assumes that result registers always early interfere with input registers, in this case,
    // baseForGetGPR. So we move baseForGetGPR to the other register if baseForGetGPR == valueRegs.
    if (baseForGetGPR != valueRegs.payloadGPR()) {
        paramBaseGPR = baseForGetGPR;
        if (!snippet->requireGlobalObject)
            remainingScratchGPR = scratchGPR;
        else
            paramGlobalObjectGPR = scratchGPR;
    } else {
        jit.move(valueRegs.payloadGPR(), scratchGPR);
        paramBaseGPR = scratchGPR;
        if (snippet->requireGlobalObject)
            paramGlobalObjectGPR = allocator.allocateScratchGPR();
    }

    JSGlobalObject* globalObjectForDOMJIT = accessCase.structure()->globalObject();

    regs.append(paramValueRegs);
    regs.append(paramBaseGPR);
    if (snippet->requireGlobalObject) {
        ASSERT(paramGlobalObjectGPR != InvalidGPRReg);
        regs.append(SnippetParams::Value(paramGlobalObjectGPR, globalObjectForDOMJIT));
    }

    if (snippet->numGPScratchRegisters) {
        unsigned i = 0;
        if (remainingScratchGPR != InvalidGPRReg) {
            gpScratch.append(remainingScratchGPR);
            ++i;
        }
        for (; i < snippet->numGPScratchRegisters; ++i)
            gpScratch.append(allocator.allocateScratchGPR());
    }

    for (unsigned i = 0; i < snippet->numFPScratchRegisters; ++i)
        fpScratch.append(allocator.allocateScratchFPR());

    // Let's store the reused registers to the stack. After that, we can use allocated scratch registers.
    ScratchRegisterAllocator::PreservedState preservedState =
        allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::SpaceForCCall);

    if (InlineCacheCompilerInternal::verbose) {
        dataLog("baseGPR = ", baseGPR, "\n");
        dataLog("valueRegs = ", valueRegs, "\n");
        dataLog("scratchGPR = ", scratchGPR, "\n");
        dataLog("paramBaseGPR = ", paramBaseGPR, "\n");
        if (paramGlobalObjectGPR != InvalidGPRReg)
            dataLog("paramGlobalObjectGPR = ", paramGlobalObjectGPR, "\n");
        dataLog("paramValueRegs = ", paramValueRegs, "\n");
        for (unsigned i = 0; i < snippet->numGPScratchRegisters; ++i)
            dataLog("gpScratch[", i, "] = ", gpScratch[i], "\n");
    }

    if (snippet->requireGlobalObject)
        jit.move(CCallHelpers::TrustedImmPtr(globalObjectForDOMJIT), paramGlobalObjectGPR);

    // We just spill the registers used in Snippet here. For not spilled registers here explicitly,
    // they must be in the used register set passed by the callers (Baseline, DFG, and FTL) if they need to be kept.
    // Some registers can be locked, but not in the used register set. For example, the caller could make baseGPR
    // same to valueRegs, and not include it in the used registers since it will be changed.
    RegisterSetBuilder usedRegisters;
    for (auto& value : regs) {
        SnippetReg reg = value.reg();
        if (reg.isJSValueRegs())
            usedRegisters.add(reg.jsValueRegs(), IgnoreVectors);
        else if (reg.isGPR())
            usedRegisters.add(reg.gpr(), IgnoreVectors);
        else
            usedRegisters.add(reg.fpr(), IgnoreVectors);
    }
    for (GPRReg reg : gpScratch)
        usedRegisters.add(reg, IgnoreVectors);
    for (FPRReg reg : fpScratch)
        usedRegisters.add(reg, IgnoreVectors);
    if (jit.codeBlock()->useDataIC())
        usedRegisters.add(stubInfo.m_stubInfoGPR, IgnoreVectors);
    auto registersToSpillForCCall = RegisterSetBuilder::registersToSaveForCCall(usedRegisters);

    AccessCaseSnippetParams params(m_vm, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch));
    snippet->generator()->run(jit, params);
    allocator.restoreReusedRegistersByPopping(jit, preservedState);
    succeed();

    CCallHelpers::JumpList exceptions = params.emitSlowPathCalls(*this, registersToSpillForCCall, jit);
    if (!exceptions.empty()) {
        exceptions.link(&jit);
        allocator.restoreReusedRegistersByPopping(jit, preservedState);
        emitExplicitExceptionHandler();
    }
}

void InlineCacheCompiler::emitModuleNamespaceLoad(ModuleNamespaceAccessCase& accessCase, MacroAssembler::JumpList& fallThrough)
{
    CCallHelpers& jit = *m_jit;
    StructureStubInfo& stubInfo = *m_stubInfo;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;

    fallThrough.append(
        jit.branchPtr(
            CCallHelpers::NotEqual,
            baseGPR,
            CCallHelpers::TrustedImmPtr(accessCase.moduleNamespaceObject())));

    jit.loadValue(&accessCase.moduleEnvironment()->variableAt(accessCase.scopeOffset()), valueRegs);
    m_failAndIgnore.append(jit.branchIfEmpty(valueRegs));
    succeed();
}

void InlineCacheCompiler::emitProxyObjectAccess(ProxyObjectAccessCase& accessCase, MacroAssembler::JumpList& fallThrough)
{
    CCallHelpers& jit = *m_jit;
    CodeBlock* codeBlock = jit.codeBlock();
    VM& vm = m_vm;
    ECMAMode ecmaMode = m_ecmaMode;
    StructureStubInfo& stubInfo = *m_stubInfo;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg scratchGPR = m_scratchGPR;
    GPRReg thisGPR = stubInfo.thisValueIsInExtraGPR() ? stubInfo.thisGPR() : baseGPR;

    JSGlobalObject* globalObject = m_globalObject;

    jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
    fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ProxyObjectType)));

    InlineCacheCompiler::SpillState spillState = preserveLiveRegistersToStackForCall();

    if (codeBlock->useDataIC()) {
        callSiteIndexForExceptionHandlingOrOriginal();
        jit.transfer32(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCallSiteIndex()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
    } else
        jit.store32(CCallHelpers::TrustedImm32(callSiteIndexForExceptionHandlingOrOriginal().bits()), CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    setSpillStateForJSCall(spillState);

    ASSERT(!accessCase.callLinkInfo());
    auto* callLinkInfo = m_callLinkInfos.add(stubInfo.codeOrigin, codeBlock->useDataIC() ? CallLinkInfo::UseDataIC::Yes : CallLinkInfo::UseDataIC::No, nullptr);
    accessCase.m_callLinkInfo = callLinkInfo;

    callLinkInfo->disallowStubs();

    callLinkInfo->setUpCall(CallLinkInfo::Call);

    unsigned numberOfParameters;
    JSFunction* proxyInternalMethod = nullptr;

    switch (accessCase.m_type) {
    case AccessCase::ProxyObjectHas:
        numberOfParameters = 2;
        proxyInternalMethod = globalObject->performProxyObjectHasFunction();
        break;
    case AccessCase::ProxyObjectLoad:
        numberOfParameters = 3;
        proxyInternalMethod = globalObject->performProxyObjectGetFunction();
        break;
    case AccessCase::IndexedProxyObjectLoad:
        numberOfParameters = 3;
        proxyInternalMethod = globalObject->performProxyObjectGetByValFunction();
        break;
    case AccessCase::ProxyObjectStore:
        numberOfParameters = 4;
        proxyInternalMethod = ecmaMode.isStrict() ? globalObject->performProxyObjectSetStrictFunction() : globalObject->performProxyObjectSetSloppyFunction();
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
    unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);

    unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf(stackAlignmentBytes(), numberOfBytesForCall);

    jit.subPtr(
        CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall),
        CCallHelpers::stackPointerRegister);

    CCallHelpers::Address calleeFrame = CCallHelpers::Address(CCallHelpers::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

    jit.store32(CCallHelpers::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));

    jit.storeCell(baseGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));

    if (!stubInfo.hasConstantIdentifier) {
        if (accessCase.m_type != AccessCase::IndexedProxyObjectLoad)
            RELEASE_ASSERT(accessCase.identifier());
        jit.storeValue(stubInfo.propertyRegs(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));
    } else
        jit.storeTrustedValue(accessCase.identifier().cell(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));

    switch (accessCase.m_type) {
    case AccessCase::ProxyObjectLoad:
    case AccessCase::IndexedProxyObjectLoad:
        jit.storeCell(thisGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(2).offset() * sizeof(Register)));
        break;
    case AccessCase::ProxyObjectStore:
        jit.storeCell(thisGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(2).offset() * sizeof(Register)));
        jit.storeValue(valueRegs, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(3).offset() * sizeof(Register)));
        break;
    default:
        break;
    }

    ASSERT(proxyInternalMethod);
    jit.move(CCallHelpers::TrustedImmPtr(proxyInternalMethod), scratchGPR);
    jit.storeCell(scratchGPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));

    jit.move(scratchGPR, BaselineJITRegisters::Call::calleeJSR.payloadGPR());
#if USE(JSVALUE32_64)
    // We *always* know that the proxy function, if non-null, is a cell.
    jit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), BaselineJITRegisters::Call::calleeJSR.tagGPR());
#endif
    auto [slowCase, dispatchLabel] = CallLinkInfo::emitFastPath(jit, callLinkInfo);
    auto doneLocation = jit.label();

    if (accessCase.m_type != AccessCase::ProxyObjectStore)
        jit.setupResults(valueRegs);

    if (!slowCase.empty()) {
        auto done = jit.jump();

        slowCase.link(&jit);
        CallLinkInfo::emitSlowPath(vm, jit, callLinkInfo);

        if (accessCase.m_type != AccessCase::ProxyObjectStore)
            jit.setupResults(valueRegs);

        done.link(&jit);
    }

    if (codeBlock->useDataIC()) {
        jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), m_scratchGPR);
        if (useHandlerIC())
            jit.addPtr(CCallHelpers::TrustedImm32(-(sizeof(CallerFrameAndPC) + maxFrameExtentForSlowPathCall + m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), m_scratchGPR);
        else
            jit.addPtr(CCallHelpers::TrustedImm32(-(m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), m_scratchGPR);
        jit.addPtr(m_scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
    } else {
        int stackPointerOffset = (codeBlock->stackPointerOffset() * sizeof(Register)) - m_preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation;
        jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
    }

    RegisterSet dontRestore;
    if (accessCase.m_type != AccessCase::ProxyObjectStore) {
        // This is the result value. We don't want to overwrite the result with what we stored to the stack.
        // We sometimes have to store it to the stack just in case we throw an exception and need the original value.
        dontRestore.add(valueRegs, IgnoreVectors);
    }
    restoreLiveRegistersFromStackForCall(spillState, dontRestore);

    jit.addLinkTask([=] (LinkBuffer& linkBuffer) {
        callLinkInfo->setCodeLocations(linkBuffer.locationOf<JSInternalPtrTag>(doneLocation));
    });
    succeed();
}

bool InlineCacheCompiler::canEmitIntrinsicGetter(StructureStubInfo& stubInfo, JSFunction* getter, Structure* structure)
{
    // We aren't structure checking the this value, so we don't know:
    // - For type array loads, that it's a typed array.
    // - For __proto__ getter, that the incoming value is an object,
    //   and if it overrides getPrototype structure flags.
    // So for these cases, it's simpler to just call the getter directly.
    if (stubInfo.thisValueIsInExtraGPR())
        return false;

    switch (getter->intrinsic()) {
    case TypedArrayByteOffsetIntrinsic:
    case TypedArrayByteLengthIntrinsic:
    case TypedArrayLengthIntrinsic: {
        if (!isTypedView(structure->typeInfo().type()))
            return false;
#if USE(JSVALUE32_64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(structure->classInfoForCells()))
            return false;
#endif
        return true;
    }
    case UnderscoreProtoIntrinsic: {
        TypeInfo info = structure->typeInfo();
        return info.isObject() && !info.overridesGetPrototype();
    }
    case SpeciesGetterIntrinsic: {
#if USE(JSVALUE32_64)
        return false;
#else
        return !structure->classInfoForCells()->isSubClassOf(JSScope::info());
#endif
    }
    case WebAssemblyInstanceExportsIntrinsic:
        return structure->typeInfo().type() == WebAssemblyInstanceType;
    default:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void InlineCacheCompiler::emitIntrinsicGetter(IntrinsicGetterAccessCase& accessCase)
{
    CCallHelpers& jit = *m_jit;
    StructureStubInfo& stubInfo = *m_stubInfo;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg valueGPR = valueRegs.payloadGPR();

    switch (accessCase.intrinsic()) {
    case TypedArrayLengthIntrinsic: {
#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(accessCase.structure()->classInfoForCells())) {
            auto allocator = makeDefaultScratchAllocator(m_scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

            jit.loadTypedArrayLength(baseGPR, valueGPR, m_scratchGPR, scratch2GPR, typedArrayType(accessCase.structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        jit.boxInt32(valueGPR, valueRegs);
#endif
        succeed();
        return;
    }

    case TypedArrayByteLengthIntrinsic: {
        TypedArrayType type = typedArrayType(accessCase.structure()->typeInfo().type());

#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(accessCase.structure()->classInfoForCells())) {
            auto allocator = makeDefaultScratchAllocator(m_scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

            jit.loadTypedArrayByteLength(baseGPR, valueGPR, m_scratchGPR, scratch2GPR, typedArrayType(accessCase.structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        if (elementSize(type) > 1)
            jit.lshift64(CCallHelpers::TrustedImm32(logElementSize(type)), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        if (elementSize(type) > 1) {
            // We can use a bitshift here since on ADDRESS32 platforms TypedArrays cannot have byteLength that overflows an int32.
            jit.lshift32(CCallHelpers::TrustedImm32(logElementSize(type)), valueGPR);
        }
        jit.boxInt32(valueGPR, valueRegs);
#endif
        succeed();
        return;
    }

    case TypedArrayByteOffsetIntrinsic: {
#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(accessCase.structure()->classInfoForCells())) {
            auto allocator = makeDefaultScratchAllocator(m_scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
            auto outOfBounds = jit.branchIfResizableOrGrowableSharedTypedArrayIsOutOfBounds(baseGPR, m_scratchGPR, scratch2GPR, typedArrayType(accessCase.structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.load64(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
#else
            jit.load32(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
#endif
            auto done = jit.jump();

            outOfBounds.link(&jit);
            jit.move(CCallHelpers::TrustedImm32(0), valueGPR);

            done.link(&jit);
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, m_scratchGPR, m_scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
        jit.boxInt32(valueGPR, valueRegs);
#endif
        succeed();
        return;
    }

    case UnderscoreProtoIntrinsic: {
        if (accessCase.structure()->hasPolyProto())
            jit.loadValue(CCallHelpers::Address(baseGPR, offsetRelativeToBase(knownPolyProtoOffset)), valueRegs);
        else
            jit.moveValue(accessCase.structure()->storedPrototype(), valueRegs);
        succeed();
        return;
    }

    case SpeciesGetterIntrinsic: {
        jit.moveValueRegs(stubInfo.baseRegs(), valueRegs);
        succeed();
        return;
    }

    case WebAssemblyInstanceExportsIntrinsic: {
#if ENABLE(WEBASSEMBLY)
        jit.loadPtr(CCallHelpers::Address(baseGPR, JSWebAssemblyInstance::offsetOfModuleRecord()), valueGPR);
        jit.loadPtr(CCallHelpers::Address(valueGPR, WebAssemblyModuleRecord::offsetOfExportsObject()), valueGPR);
        jit.boxCell(valueGPR, valueRegs);
        succeed();
#endif
        return;
    }

    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static void commit(const GCSafeConcurrentJSLocker&, VM& vm, std::unique_ptr<WatchpointsOnStructureStubInfo>& watchpoints, CodeBlock* codeBlock, StructureStubInfo& stubInfo, AccessCase& accessCase)
{
    // NOTE: We currently assume that this is relatively rare. It mainly arises for accesses to
    // properties on DOM nodes. For sure we cache many DOM node accesses, but even in
    // Real Pages (TM), we appear to spend most of our time caching accesses to properties on
    // vanilla objects or exotic objects from within JSC (like Arguments, those are super popular).
    // Those common kinds of JSC object accesses don't hit this case.

    for (WatchpointSet* set : accessCase.commit(vm)) {
        Watchpoint* watchpoint = WatchpointsOnStructureStubInfo::ensureReferenceAndAddWatchpoint(watchpoints, codeBlock, &stubInfo);
        set->add(watchpoint);
    }
}

static inline bool canUseMegamorphicPutFastPath(Structure* structure)
{
    while (true) {
        if (structure->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || structure->typeInfo().overridesGetPrototype() || structure->typeInfo().overridesPut() || structure->hasPolyProto())
            return false;
        JSValue prototype = structure->storedPrototype();
        if (prototype.isNull())
            return true;
        structure = asObject(prototype)->structure();
    }
}

AccessGenerationResult InlineCacheCompiler::regenerate(const GCSafeConcurrentJSLocker& locker, PolymorphicAccess& poly, CodeBlock* codeBlock)
{
    SuperSamplerScope superSamplerScope(false);

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "Regenerate with m_list: ", listDump(poly.m_list));

    // Regenerating is our opportunity to figure out what our list of cases should look like. We
    // do this here. The newly produced 'cases' list may be smaller than m_list. We don't edit
    // m_list in-place because we may still fail, in which case we want the PolymorphicAccess object
    // to be unmutated. For sure, we want it to hang onto any data structures that may be referenced
    // from the code of the current stub (aka previous).
    PolymorphicAccess::ListType cases;
    unsigned srcIndex = 0;
    unsigned dstIndex = 0;
    while (srcIndex < poly.m_list.size()) {
        RefPtr<AccessCase> someCase = WTFMove(poly.m_list[srcIndex++]);

        // If the case had been generated, then we have to keep the original in m_list in case we
        // fail to regenerate. That case may have data structures that are used by the code that it
        // had generated. If the case had not been generated, then we want to remove it from m_list.
        bool isGenerated = someCase->state() == AccessCase::Generated;

        [&] () {
            if (!someCase->couldStillSucceed())
                return;

            // Figure out if this is replaced by any later case. Given two cases A and B where A
            // comes first in the case list, we know that A would have triggered first if we had
            // generated the cases in a cascade. That's why this loop asks B->canReplace(A) but not
            // A->canReplace(B). If A->canReplace(B) was true then A would never have requested
            // repatching in cases where Repatch.cpp would have then gone on to generate B. If that
            // did happen by some fluke, then we'd just miss the redundancy here, which wouldn't be
            // incorrect - just slow. However, if A's checks failed and Repatch.cpp concluded that
            // this new condition could be handled by B and B->canReplace(A), then this says that we
            // don't need A anymore.
            //
            // If we can generate a binary switch, then A->canReplace(B) == B->canReplace(A). So,
            // it doesn't matter that we only do the check in one direction.
            for (unsigned j = srcIndex; j < poly.m_list.size(); ++j) {
                if (poly.m_list[j]->canReplace(*someCase))
                    return;
            }

            if (isGenerated)
                cases.append(someCase->clone());
            else
                cases.append(WTFMove(someCase));
        }();

        if (isGenerated)
            poly.m_list[dstIndex++] = WTFMove(someCase);
    }
    poly.m_list.shrink(dstIndex);

    bool generatedMegamorphicCode = false;

    // If the resulting set of cases is so big that we would stop caching and this is InstanceOf,
    // then we want to generate the generic InstanceOf and then stop.
    if (cases.size() >= Options::maxAccessVariantListSize() || m_stubInfo->canBeMegamorphic) {
        switch (m_stubInfo->accessType) {
        case AccessType::InstanceOf: {
            while (!cases.isEmpty())
                poly.m_list.append(cases.takeLast());
            cases.append(AccessCase::create(vm(), codeBlock, AccessCase::InstanceOfGeneric, nullptr));
            generatedMegamorphicCode = true;
            break;
        }
        case AccessType::GetById:
        case AccessType::GetByIdWithThis: {
            auto identifier = cases.last()->m_identifier;
            bool allAreSimpleLoadOrMiss = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::Load && accessCase->type() != AccessCase::Miss) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
            }

            // Currently, we do not apply megamorphic cache for "length" property since Array#length and String#length are too common.
            if (!canUseMegamorphicGetById(vm(), identifier.uid()))
                allAreSimpleLoadOrMiss = false;

#if USE(JSVALUE32_64)
            allAreSimpleLoadOrMiss = false;
#endif

            if (allAreSimpleLoadOrMiss) {
                while (!cases.isEmpty())
                    poly.m_list.append(cases.takeLast());
                cases.append(AccessCase::create(vm(), codeBlock, AccessCase::LoadMegamorphic, identifier));
                generatedMegamorphicCode = true;
            }
            break;
        }
        case AccessType::GetByVal:
        case AccessType::GetByValWithThis: {
            bool allAreSimpleLoadOrMiss = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::Load && accessCase->type() != AccessCase::Miss) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleLoadOrMiss = false;
                    break;
                }
            }

#if USE(JSVALUE32_64)
            allAreSimpleLoadOrMiss = false;
#endif
            if (m_stubInfo->isEnumerator)
                allAreSimpleLoadOrMiss = false;

            if (allAreSimpleLoadOrMiss) {
                while (!cases.isEmpty())
                    poly.m_list.append(cases.takeLast());
                cases.append(AccessCase::create(vm(), codeBlock, AccessCase::IndexedMegamorphicLoad, nullptr));
                generatedMegamorphicCode = true;
            }
            break;
        }
        case AccessType::PutByIdStrict:
        case AccessType::PutByIdSloppy: {
            auto identifier = cases.last()->m_identifier;
            bool allAreSimpleReplaceOrTransition = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::Replace && accessCase->type() != AccessCase::Transition) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (!canUseMegamorphicPutFastPath(accessCase->structure())) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->type() == AccessCase::Transition) {
                    if (accessCase->newStructure()->outOfLineCapacity() != accessCase->structure()->outOfLineCapacity()) {
                        allAreSimpleReplaceOrTransition = false;
                        break;
                    }
                }
            }

            // Currently, we do not apply megamorphic cache for "length" property since Array#length and String#length are too common.
            if (!canUseMegamorphicPutById(vm(), identifier.uid()))
                allAreSimpleReplaceOrTransition = false;

#if USE(JSVALUE32_64)
            allAreSimpleReplaceOrTransition = false;
#endif

            if (allAreSimpleReplaceOrTransition) {
                while (!cases.isEmpty())
                    poly.m_list.append(cases.takeLast());
                cases.append(AccessCase::create(vm(), codeBlock, AccessCase::StoreMegamorphic, identifier));
                generatedMegamorphicCode = true;
            }
            break;
        }
        case AccessType::PutByValStrict:
        case AccessType::PutByValSloppy: {
            bool allAreSimpleReplaceOrTransition = true;
            for (auto& accessCase : cases) {
                if (accessCase->type() != AccessCase::Replace && accessCase->type() != AccessCase::Transition) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->usesPolyProto()) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->viaGlobalProxy()) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (!canUseMegamorphicPutFastPath(accessCase->structure())) {
                    allAreSimpleReplaceOrTransition = false;
                    break;
                }
                if (accessCase->type() == AccessCase::Transition) {
                    if (accessCase->newStructure()->outOfLineCapacity() != accessCase->structure()->outOfLineCapacity()) {
                        allAreSimpleReplaceOrTransition = false;
                        break;
                    }
                }
            }

#if USE(JSVALUE32_64)
            allAreSimpleReplaceOrTransition = false;
#endif

            if (allAreSimpleReplaceOrTransition) {
                while (!cases.isEmpty())
                    poly.m_list.append(cases.takeLast());
                cases.append(AccessCase::create(vm(), codeBlock, AccessCase::IndexedMegamorphicStore, nullptr));
                generatedMegamorphicCode = true;
            }
            break;
        }
        default:
            break;
        }
    }

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "Optimized cases: ", listDump(cases));

    auto finishCodeGeneration = [&](Ref<PolymorphicAccessJITStubRoutine>&& stub) {
        auto handler = InlineCacheHandler::create(WTFMove(stub), WTFMove(m_watchpoints));
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Returning: ", handler->callTarget());

        poly.m_list = WTFMove(cases);
        poly.m_list.shrinkToFit();

        AccessGenerationResult::Kind resultKind;
        if (generatedMegamorphicCode)
            resultKind = AccessGenerationResult::GeneratedMegamorphicCode;
        else if (poly.m_list.size() >= Options::maxAccessVariantListSize())
            resultKind = AccessGenerationResult::GeneratedFinalCode;
        else
            resultKind = AccessGenerationResult::GeneratedNewCode;

        return AccessGenerationResult(resultKind, WTFMove(handler));
    };

    if (generatedMegamorphicCode && useHandlerIC()) {
        ASSERT(codeBlock->useDataIC());
        auto stub = vm().m_sharedJITStubs->getMegamorphic(m_stubInfo->accessType);
        if (stub)
            return finishCodeGeneration(stub.releaseNonNull());
    }

    auto allocator = makeDefaultScratchAllocator();
    m_allocator = &allocator;
    m_scratchGPR = allocator.allocateScratchGPR();

    for (auto& accessCase : cases) {
        if (needsScratchFPR(accessCase->m_type)) {
            m_scratchFPR = allocator.allocateScratchFPR();
            break;
        }
    }

    bool doesCalls = false;
    bool doesJSCalls = false;
    bool canBeShared = Options::useDataICSharing();
    Vector<JSCell*> cellsToMark;
    FixedVector<RefPtr<AccessCase>> keys(cases.size());
    unsigned index = 0;
    for (auto& entry : cases) {
        doesCalls |= entry->doesCalls(vm(), &cellsToMark);
        switch (entry->type()) {
        case AccessCase::Getter:
        case AccessCase::Setter:
        case AccessCase::ProxyObjectHas:
        case AccessCase::ProxyObjectLoad:
        case AccessCase::ProxyObjectStore:
        case AccessCase::IndexedProxyObjectLoad:
            // Getter / Setter / ProxyObjectLoad / ProxyObjectStore / IndexedProxyObjectLoad rely on stack-pointer adjustment, which is tied to the linked CodeBlock, which makes this code unshareable.
            canBeShared = false;
            doesJSCalls = true;
            break;
        case AccessCase::CustomValueGetter:
        case AccessCase::CustomAccessorGetter:
        case AccessCase::CustomValueSetter:
        case AccessCase::CustomAccessorSetter:
            // Custom getter / setter emits JSGlobalObject pointer, which is tied to the linked CodeBlock.
            canBeShared = false;
            break;
        default:
            break;
        }
        keys[index] = entry;
        ++index;
    }
    m_doesCalls = doesCalls;
    m_doesJSCalls = doesJSCalls;

    // At this point we're convinced that 'cases' contains the cases that we want to JIT now and we
    // won't change that set anymore.

    bool allGuardedByStructureCheck = true;
    bool needsInt32PropertyCheck = false;
    bool needsStringPropertyCheck = false;
    bool needsSymbolPropertyCheck = false;
    bool acceptValueProperty = false;
    for (auto& newCase : cases) {
        if (!m_stubInfo->hasConstantIdentifier) {
            if (newCase->requiresIdentifierNameMatch()) {
                if (newCase->uid()->isSymbol())
                    needsSymbolPropertyCheck = true;
                else
                    needsStringPropertyCheck = true;
            } else if (newCase->requiresInt32PropertyCheck())
                needsInt32PropertyCheck = true;
            else
                acceptValueProperty = true;
        }
        commit(locker, vm(), m_watchpoints, codeBlock, *m_stubInfo, *newCase);
        allGuardedByStructureCheck &= newCase->guardedByStructureCheck(*m_stubInfo);
        if (newCase->usesPolyProto())
            canBeShared = false;
    }
    if (needsSymbolPropertyCheck || needsStringPropertyCheck || needsInt32PropertyCheck)
        canBeShared = false;

    CCallHelpers jit(codeBlock);
    m_jit = &jit;

    if (useHandlerIC())
        emitDataICPrologue(*m_jit);

    if (!canBeShared && ASSERT_ENABLED) {
        if (codeBlock->useDataIC()) {
            jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), jit.scratchRegister());
            jit.addPtr(jit.scratchRegister(), GPRInfo::callFrameRegister, jit.scratchRegister());
            if (useHandlerIC())
                jit.addPtr(CCallHelpers::TrustedImm32(-static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC) + maxFrameExtentForSlowPathCall)), jit.scratchRegister());
        } else
            jit.addPtr(CCallHelpers::TrustedImm32(codeBlock->stackPointerOffset() * sizeof(Register)), GPRInfo::callFrameRegister, jit.scratchRegister());
        auto ok = jit.branchPtr(CCallHelpers::Equal, CCallHelpers::stackPointerRegister, jit.scratchRegister());
        jit.breakpoint();
        ok.link(&jit);
    }

    m_preservedReusedRegisterState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

    if (cases.isEmpty()) {
        // This is super unlikely, but we make it legal anyway.
        m_failAndRepatch.append(jit.jump());
    } else if (!allGuardedByStructureCheck || cases.size() == 1) {
        // If there are any proxies in the list, we cannot just use a binary switch over the structure.
        // We need to resort to a cascade. A cascade also happens to be optimal if we only have just
        // one case.
        CCallHelpers::JumpList fallThrough;
        if (needsInt32PropertyCheck || needsStringPropertyCheck || needsSymbolPropertyCheck) {
            if (needsInt32PropertyCheck) {
                CCallHelpers::JumpList notInt32;

                if (!m_stubInfo->propertyIsInt32) {
#if USE(JSVALUE64)
                    notInt32.append(jit.branchIfNotInt32(m_stubInfo->propertyGPR()));
#else
                    notInt32.append(jit.branchIfNotInt32(m_stubInfo->propertyTagGPR()));
#endif
                }
                JIT_COMMENT(jit, "Cases start (needsInt32PropertyCheck)");
                for (unsigned i = cases.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.clear();
                    if (cases[i]->requiresInt32PropertyCheck())
                        generateWithGuard(*cases[i], fallThrough);
                }

                if (needsStringPropertyCheck || needsSymbolPropertyCheck || acceptValueProperty) {
                    notInt32.link(&jit);
                    fallThrough.link(&jit);
                    fallThrough.clear();
                } else
                    m_failAndRepatch.append(notInt32);
            }

            if (needsStringPropertyCheck) {
                CCallHelpers::JumpList notString;
                GPRReg propertyGPR = m_stubInfo->propertyGPR();
                if (!m_stubInfo->propertyIsString) {
#if USE(JSVALUE32_64)
                    GPRReg propertyTagGPR = m_stubInfo->propertyTagGPR();
                    notString.append(jit.branchIfNotCell(propertyTagGPR));
#else
                    notString.append(jit.branchIfNotCell(propertyGPR));
#endif
                    notString.append(jit.branchIfNotString(propertyGPR));
                }

                jit.loadPtr(MacroAssembler::Address(propertyGPR, JSString::offsetOfValue()), m_scratchGPR);

                m_failAndRepatch.append(jit.branchIfRopeStringImpl(m_scratchGPR));

                JIT_COMMENT(jit, "Cases start (needsStringPropertyCheck)");
                for (unsigned i = cases.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.clear();
                    if (cases[i]->requiresIdentifierNameMatch() && !cases[i]->uid()->isSymbol())
                        generateWithGuard(*cases[i], fallThrough);
                }

                if (needsSymbolPropertyCheck || acceptValueProperty) {
                    notString.link(&jit);
                    fallThrough.link(&jit);
                    fallThrough.clear();
                } else
                    m_failAndRepatch.append(notString);
            }

            if (needsSymbolPropertyCheck) {
                CCallHelpers::JumpList notSymbol;
                if (!m_stubInfo->propertyIsSymbol) {
                    GPRReg propertyGPR = m_stubInfo->propertyGPR();
#if USE(JSVALUE32_64)
                    GPRReg propertyTagGPR = m_stubInfo->propertyTagGPR();
                    notSymbol.append(jit.branchIfNotCell(propertyTagGPR));
#else
                    notSymbol.append(jit.branchIfNotCell(propertyGPR));
#endif
                    notSymbol.append(jit.branchIfNotSymbol(propertyGPR));
                }

                JIT_COMMENT(jit, "Cases start (needsSymbolPropertyCheck)");
                for (unsigned i = cases.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.clear();
                    if (cases[i]->requiresIdentifierNameMatch() && cases[i]->uid()->isSymbol())
                        generateWithGuard(*cases[i], fallThrough);
                }

                if (acceptValueProperty) {
                    notSymbol.link(&jit);
                    fallThrough.link(&jit);
                    fallThrough.clear();
                } else
                    m_failAndRepatch.append(notSymbol);
            }

            if (acceptValueProperty) {
                JIT_COMMENT(jit, "Cases start (remaining)");
                for (unsigned i = cases.size(); i--;) {
                    fallThrough.link(&jit);
                    fallThrough.clear();
                    if (!cases[i]->requiresIdentifierNameMatch() && !cases[i]->requiresInt32PropertyCheck())
                        generateWithGuard(*cases[i], fallThrough);
                }
            }
        } else {
            // Cascade through the list, preferring newer entries.
            JIT_COMMENT(jit, "Cases start !(needsInt32PropertyCheck || needsStringPropertyCheck || needsSymbolPropertyCheck)");
            for (unsigned i = cases.size(); i--;) {
                fallThrough.link(&jit);
                fallThrough.clear();
                generateWithGuard(*cases[i], fallThrough);
            }
        }

        m_failAndRepatch.append(fallThrough);

    } else {
        JIT_COMMENT(jit, "Cases start (allGuardedByStructureCheck)");
        jit.load32(
            CCallHelpers::Address(m_stubInfo->m_baseGPR, JSCell::structureIDOffset()),
            m_scratchGPR);

        Vector<int64_t> caseValues(cases.size());
        for (unsigned i = 0; i < cases.size(); ++i)
            caseValues[i] = bitwise_cast<int32_t>(cases[i]->structure()->id());

        BinarySwitch binarySwitch(m_scratchGPR, caseValues, BinarySwitch::Int32);
        while (binarySwitch.advance(jit))
            generate(*cases[binarySwitch.caseIndex()]);
        m_failAndRepatch.append(binarySwitch.fallThrough());
    }

    if (!m_failAndIgnore.empty()) {
        m_failAndIgnore.link(&jit);
        JIT_COMMENT(jit, "failAndIgnore");

        // Make sure that the inline cache optimization code knows that we are taking slow path because
        // of something that isn't patchable. The slow path will decrement "countdown" and will only
        // patch things if the countdown reaches zero. We increment the slow path count here to ensure
        // that the slow path does not try to patch.
        if (codeBlock->useDataIC()) {
#if CPU(X86) || CPU(X86_64) || CPU(ARM64)
            jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()));
#else
            jit.load8(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()), m_scratchGPR);
            jit.add32(CCallHelpers::TrustedImm32(1), m_scratchGPR);
            jit.store8(m_scratchGPR, CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCountdown()));
#endif
        } else {
#if CPU(X86) || CPU(X86_64) || CPU(ARM64)
            jit.move(CCallHelpers::TrustedImmPtr(&m_stubInfo->countdown), m_scratchGPR);
            jit.add8(CCallHelpers::TrustedImm32(1), CCallHelpers::Address(m_scratchGPR));
#else
            jit.load8(&m_stubInfo->countdown, m_scratchGPR);
            jit.add32(CCallHelpers::TrustedImm32(1), m_scratchGPR);
            jit.store8(m_scratchGPR, &m_stubInfo->countdown);
#endif
        }
    }

    CCallHelpers::JumpList failure;
    if (allocator.didReuseRegisters()) {
        m_failAndRepatch.link(&jit);
        restoreScratch();
    } else
        failure = m_failAndRepatch;
    failure.append(jit.jump());

    CodeBlock* codeBlockThatOwnsExceptionHandlers = nullptr;
    DisposableCallSiteIndex callSiteIndexForExceptionHandling;
    if (needsToRestoreRegistersIfException() && doesJSCalls) {
        // Emit the exception handler.
        // Note that this code is only reachable when doing genericUnwind from a pure JS getter/setter .
        // Note also that this is not reachable from custom getter/setter. Custom getter/setters will have
        // their own exception handling logic that doesn't go through genericUnwind.
        MacroAssembler::Label makeshiftCatchHandler = jit.label();
        JIT_COMMENT(jit, "exception handler");

        InlineCacheCompiler::SpillState spillState = this->spillStateForJSCall();
        ASSERT(!spillState.isEmpty());
        jit.loadPtr(vm().addressOfCallFrameForCatch(), GPRInfo::callFrameRegister);
        if (codeBlock->useDataIC()) {
            ASSERT(!JITCode::isBaselineCode(m_jitType));
            jit.loadPtr(CCallHelpers::Address(GPRInfo::jitDataRegister, BaselineJITData::offsetOfStackOffset()), m_scratchGPR);
            jit.addPtr(CCallHelpers::TrustedImm32(-(m_preservedReusedRegisterState.numberOfBytesPreserved + spillState.numberOfStackBytesUsedForRegisterPreservation)), m_scratchGPR);
            jit.addPtr(m_scratchGPR, GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
        } else {
            int stackPointerOffset = (codeBlock->stackPointerOffset() * sizeof(Register)) - m_preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation;
            jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);
        }

        restoreLiveRegistersFromStackForCallWithThrownException(spillState);
        restoreScratch();
        HandlerInfo oldHandler = originalExceptionHandler();
        jit.jumpThunk(oldHandler.nativeCode);
        DisposableCallSiteIndex newExceptionHandlingCallSite = this->callSiteIndexForExceptionHandling();
        jit.addLinkTask(
            [=] (LinkBuffer& linkBuffer) {
                HandlerInfo handlerToRegister = oldHandler;
                handlerToRegister.nativeCode = linkBuffer.locationOf<ExceptionHandlerPtrTag>(makeshiftCatchHandler);
                handlerToRegister.start = newExceptionHandlingCallSite.bits();
                handlerToRegister.end = newExceptionHandlingCallSite.bits() + 1;
                codeBlock->appendExceptionHandler(handlerToRegister);
            });

        // We set these to indicate to the stub to remove itself from the CodeBlock's
        // exception handler table when it is deallocated.
        codeBlockThatOwnsExceptionHandlers = codeBlock;
        ASSERT(JSC::JITCode::isOptimizingJIT(codeBlockThatOwnsExceptionHandlers->jitType()));
        callSiteIndexForExceptionHandling = this->callSiteIndexForExceptionHandling();
    }

    CodeLocationLabel<JSInternalPtrTag> successLabel = m_stubInfo->doneLocation;
    if (codeBlock->useDataIC()) {
        if (useHandlerIC())
            failure.linkThunk(CodeLocationLabel(CodePtr<NoPtrTag> { (generateSlowPathCode(vm(), m_stubInfo->accessType).retaggedCode<NoPtrTag>().dataLocation<uint8_t*>() + prologueSizeInBytesDataIC) }), &jit);
        else {
            failure.link(&jit);
            JIT_COMMENT(jit, "failure far jump");
            jit.farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfSlowPathStartLocation()), JITStubRoutinePtrTag);
        }
    } else {
        m_success.linkThunk(successLabel, &jit);
        failure.linkThunk(m_stubInfo->slowPathStartLocation, &jit);
    }

    RefPtr<PolymorphicAccessJITStubRoutine> stub;
    FixedVector<StructureID> weakStructures(WTFMove(m_weakStructures));
    if (codeBlock->useDataIC() && canBeShared) {
        SharedJITStubSet::Searcher searcher {
            m_stubInfo->m_baseGPR,
            m_stubInfo->m_valueGPR,
            m_stubInfo->m_extraGPR,
            m_stubInfo->m_extra2GPR,
            m_stubInfo->m_stubInfoGPR,
            m_stubInfo->m_arrayProfileGPR,
            m_stubInfo->usedRegisters,
            keys,
            weakStructures,
        };
        stub = vm().m_sharedJITStubs->find(searcher);
        if (stub) {
            dataLogLnIf(InlineCacheCompilerInternal::verbose, "Found existing code stub ", stub->code());
            return finishCodeGeneration(stub.releaseNonNull());
        }
    }

    LinkBuffer linkBuffer(jit, codeBlock, LinkBuffer::Profile::InlineCache, JITCompilationCanFail);
    if (linkBuffer.didFailToAllocate()) {
        dataLogLnIf(InlineCacheCompilerInternal::verbose, "Did fail to allocate.");
        return AccessGenerationResult::GaveUp;
    }


    if (codeBlock->useDataIC())
        ASSERT(m_success.empty());

    dataLogLnIf(InlineCacheCompilerInternal::verbose, FullCodeOrigin(codeBlock, m_stubInfo->codeOrigin), ": Generating polymorphic access stub for ", listDump(cases));

    MacroAssemblerCodeRef<JITStubRoutinePtrTag> code = FINALIZE_CODE_FOR(
        codeBlock, linkBuffer, JITStubRoutinePtrTag,
        "%s", toCString("Access stub for ", *codeBlock, " ", m_stubInfo->codeOrigin, "with start: ", m_stubInfo->startLocation, " with return point ", successLabel, ": ", listDump(cases)).data());

    CodeBlock* owner = codeBlock;
    if (generatedMegamorphicCode && useHandlerIC()) {
        ASSERT(codeBlock->useDataIC());
        ASSERT(!doesCalls);
        ASSERT(cellsToMark.isEmpty());
        owner = nullptr;
    }

    stub = createICJITStubRoutine(code, WTFMove(keys), WTFMove(weakStructures), vm(), owner, doesCalls, cellsToMark, WTFMove(m_callLinkInfos), codeBlockThatOwnsExceptionHandlers, callSiteIndexForExceptionHandling);

    if (generatedMegamorphicCode && useHandlerIC()) {
        ASSERT(codeBlock->useDataIC());
        vm().m_sharedJITStubs->setMegamorphic(m_stubInfo->accessType, *stub);
    }

    if (codeBlock->useDataIC()) {
        if (canBeShared)
            vm().m_sharedJITStubs->add(SharedJITStubSet::Hash::Key(m_stubInfo->m_baseGPR, m_stubInfo->m_valueGPR, m_stubInfo->m_extraGPR, m_stubInfo->m_extra2GPR, m_stubInfo->m_stubInfoGPR, m_stubInfo->m_arrayProfileGPR, m_stubInfo->usedRegisters, stub.get()));
    }

    return finishCodeGeneration(stub.releaseNonNull());
}

PolymorphicAccess::PolymorphicAccess() = default;
PolymorphicAccess::~PolymorphicAccess() = default;

AccessGenerationResult PolymorphicAccess::addCases(
    const GCSafeConcurrentJSLocker& locker, VM& vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo,
    Vector<RefPtr<AccessCase>, 2> originalCasesToAdd)
{
    SuperSamplerScope superSamplerScope(false);

    // This method will add the originalCasesToAdd to the list one at a time while preserving the
    // invariants:
    // - If a newly added case canReplace() any existing case, then the existing case is removed before
    //   the new case is added. Removal doesn't change order of the list. Any number of existing cases
    //   can be removed via the canReplace() rule.
    // - Cases in the list always appear in ascending order of time of addition. Therefore, if you
    //   cascade through the cases in reverse order, you will get the most recent cases first.
    // - If this method fails (returns null, doesn't add the cases), then both the previous case list
    //   and the previous stub are kept intact and the new cases are destroyed. It's OK to attempt to
    //   add more things after failure.

    // First ensure that the originalCasesToAdd doesn't contain duplicates.
    Vector<RefPtr<AccessCase>> casesToAdd;
    for (unsigned i = 0; i < originalCasesToAdd.size(); ++i) {
        RefPtr<AccessCase> myCase = WTFMove(originalCasesToAdd[i]);

        // Add it only if it is not replaced by the subsequent cases in the list.
        bool found = false;
        for (unsigned j = i + 1; j < originalCasesToAdd.size(); ++j) {
            if (originalCasesToAdd[j]->canReplace(*myCase)) {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        casesToAdd.append(WTFMove(myCase));
    }

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "casesToAdd: ", listDump(casesToAdd));

    // If there aren't any cases to add, then fail on the grounds that there's no point to generating a
    // new stub that will be identical to the old one. Returning null should tell the caller to just
    // keep doing what they were doing before.
    if (casesToAdd.isEmpty())
        return AccessGenerationResult::MadeNoChanges;

    if (stubInfo.accessType != AccessType::InstanceOf) {
        bool shouldReset = false;
        AccessGenerationResult resetResult(AccessGenerationResult::ResetStubAndFireWatchpoints);
        auto considerPolyProtoReset = [&] (Structure* a, Structure* b) {
            if (Structure::shouldConvertToPolyProto(a, b)) {
                // For now, we only reset if this is our first time invalidating this watchpoint.
                // The reason we don't immediately fire this watchpoint is that we may be already
                // watching the poly proto watchpoint, which if fired, would destroy us. We let
                // the person handling the result to do a delayed fire.
                ASSERT(a->rareData()->sharedPolyProtoWatchpoint().get() == b->rareData()->sharedPolyProtoWatchpoint().get());
                if (a->rareData()->sharedPolyProtoWatchpoint()->isStillValid()) {
                    shouldReset = true;
                    resetResult.addWatchpointToFire(*a->rareData()->sharedPolyProtoWatchpoint(), StringFireDetail("Detected poly proto optimization opportunity."));
                }
            }
        };

        for (auto& caseToAdd : casesToAdd) {
            for (auto& existingCase : m_list) {
                Structure* a = caseToAdd->structure();
                Structure* b = existingCase->structure();
                considerPolyProtoReset(a, b);
            }
        }
        for (unsigned i = 0; i < casesToAdd.size(); ++i) {
            for (unsigned j = i + 1; j < casesToAdd.size(); ++j) {
                Structure* a = casesToAdd[i]->structure();
                Structure* b = casesToAdd[j]->structure();
                considerPolyProtoReset(a, b);
            }
        }

        if (shouldReset)
            return resetResult;
    }

    // Now add things to the new list. Note that at this point, we will still have old cases that
    // may be replaced by the new ones. That's fine. We will sort that out when we regenerate.
    for (auto& caseToAdd : casesToAdd) {
        commit(locker, vm, m_watchpoints, codeBlock, stubInfo, *caseToAdd);
        m_list.append(WTFMove(caseToAdd));
    }

    dataLogLnIf(InlineCacheCompilerInternal::verbose, "After addCases: m_list: ", listDump(m_list));

    return AccessGenerationResult::Buffered;
}

AccessGenerationResult PolymorphicAccess::addCase(
    const GCSafeConcurrentJSLocker& locker, VM& vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo, Ref<AccessCase> newAccess)
{
    Vector<RefPtr<AccessCase>, 2> newAccesses;
    newAccesses.append(WTFMove(newAccess));
    return addCases(locker, vm, codeBlock, stubInfo, WTFMove(newAccesses));
}

bool PolymorphicAccess::visitWeak(VM& vm) const
{
    for (unsigned i = 0; i < size(); ++i) {
        if (!at(i).visitWeak(vm))
            return false;
    }
    return true;
}

template<typename Visitor>
void PolymorphicAccess::propagateTransitions(Visitor& visitor) const
{
    for (unsigned i = 0; i < size(); ++i)
        at(i).propagateTransitions(visitor);
}

template void PolymorphicAccess::propagateTransitions(AbstractSlotVisitor&) const;
template void PolymorphicAccess::propagateTransitions(SlotVisitor&) const;

template<typename Visitor>
void PolymorphicAccess::visitAggregateImpl(Visitor& visitor)
{
    for (unsigned i = 0; i < size(); ++i)
        at(i).visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(PolymorphicAccess);

void PolymorphicAccess::dump(PrintStream& out) const
{
    out.print(RawPointer(this), ":[");
    CommaPrinter comma;
    for (auto& entry : m_list)
        out.print(comma, *entry);
    out.print("]");
}

void InlineCacheHandler::aboutToDie()
{
    if (m_stubRoutine)
        m_stubRoutine->aboutToDie();
}

bool InlineCacheHandler::visitWeak(VM& vm) const
{
    if (!m_stubRoutine)
        return true;

    for (StructureID weakReference : m_stubRoutine->weakStructures()) {
        Structure* structure = weakReference.decode();
        if (!vm.heap.isMarked(structure))
            return false;
    }

    return true;
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, AccessGenerationResult::Kind kind)
{
    switch (kind) {
    case AccessGenerationResult::MadeNoChanges:
        out.print("MadeNoChanges");
        return;
    case AccessGenerationResult::GaveUp:
        out.print("GaveUp");
        return;
    case AccessGenerationResult::Buffered:
        out.print("Buffered");
        return;
    case AccessGenerationResult::GeneratedNewCode:
        out.print("GeneratedNewCode");
        return;
    case AccessGenerationResult::GeneratedFinalCode:
        out.print("GeneratedFinalCode");
        return;
    case AccessGenerationResult::GeneratedMegamorphicCode:
        out.print("GeneratedMegamorphicCode");
        return;
    case AccessGenerationResult::ResetStubAndFireWatchpoints:
        out.print("ResetStubAndFireWatchpoints");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, AccessCase::AccessType type)
{
    switch (type) {
#define JSC_DEFINE_ACCESS_TYPE_CASE(name) \
    case AccessCase::name: \
        out.print(#name); \
        return;

        JSC_FOR_EACH_ACCESS_TYPE(JSC_DEFINE_ACCESS_TYPE_CASE)

#undef JSC_DEFINE_ACCESS_TYPE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, AccessCase::State state)
{
    switch (state) {
    case AccessCase::Primordial:
        out.print("Primordial");
        return;
    case AccessCase::Committed:
        out.print("Committed");
        return;
    case AccessCase::Generated:
        out.print("Generated");
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(JIT)


