/*
 * Copyright (C) 2013, 2015-2016 Apple Inc. All rights reserved.
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
#include "FTLJITCode.h"

#if ENABLE(FTL_JIT)

#include "FTLState.h"

namespace JSC { namespace FTL {

using namespace B3;

JITCode::JITCode()
    : JSC::JITCode(FTLJIT)
{
}

JITCode::~JITCode()
{
    if (FTL::shouldDumpDisassembly()) {
        dataLog("Destroying FTL JIT code at ");
        CommaPrinter comma;
#if FTL_USES_B3
        dataLog(comma, m_b3Code);
        dataLog(comma, m_arityCheckEntrypoint);
#else
        for (auto& handle : m_handles)
            dataLog(comma, pointerDump(handle.get()));
        dataLog(comma, pointerDump(m_arityCheckEntrypoint.executableMemory()));
        dataLog(comma, pointerDump(m_exitThunks.executableMemory()));
        dataLog("\n");
#endif
    }
}

#if FTL_USES_B3
void JITCode::initializeB3Code(CodeRef b3Code)
{
    m_b3Code = b3Code;
}

void JITCode::initializeB3Byproducts(std::unique_ptr<OpaqueByproducts> byproducts)
{
    m_b3Byproducts = WTFMove(byproducts);
}
#else // FTL_USES_B3
void JITCode::initializeExitThunks(CodeRef exitThunks)
{
    m_exitThunks = exitThunks;
}

void JITCode::addHandle(PassRefPtr<ExecutableMemoryHandle> handle)
{
    m_handles.append(handle);
}

void JITCode::addDataSection(PassRefPtr<DataSection> dataSection)
{
    m_dataSections.append(dataSection);
}
#endif // FTL_USES_B3

void JITCode::initializeAddressForCall(CodePtr address)
{
    m_addressForCall = address;
}

void JITCode::initializeArityCheckEntrypoint(CodeRef entrypoint)
{
    m_arityCheckEntrypoint = entrypoint;
}

JITCode::CodePtr JITCode::addressForCall(ArityCheckMode arityCheck)
{
    switch (arityCheck) {
    case ArityCheckNotRequired:
        return m_addressForCall;
    case MustCheckArity:
        return m_arityCheckEntrypoint.code();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return CodePtr();
}

void* JITCode::executableAddressAtOffset(size_t offset)
{
    return reinterpret_cast<char*>(m_addressForCall.executableAddress()) + offset;
}

void* JITCode::dataAddressAtOffset(size_t)
{
    // We can't patch FTL code, yet. Even if we did, it's not clear that we would do so
    // through this API.
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

unsigned JITCode::offsetOf(void*)
{
    // We currently don't have visibility into the FTL code.
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

size_t JITCode::size()
{
    // We don't know the size of FTL code, yet. Make a wild guess. This is mostly used for
    // GC load estimates.
    return 1000;
}

bool JITCode::contains(void*)
{
    // We have no idea what addresses the FTL code contains, yet.
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

#if !FTL_USES_B3
JITCode::CodePtr JITCode::exitThunks()
{
    return m_exitThunks.code();
}
#endif

JITCode* JITCode::ftl()
{
    return this;
}

DFG::CommonData* JITCode::dfgCommon()
{
    return &common;
}

void JITCode::validateReferences(const TrackedReferences& trackedReferences)
{
    common.validateReferences(trackedReferences);
    
    for (OSRExit& exit : osrExit)
        exit.m_descriptor->validateReferences(trackedReferences);
}

RegisterSet JITCode::liveRegistersToPreserveAtExceptionHandlingCallSite(CodeBlock*, CallSiteIndex callSiteIndex)
{
#if FTL_USES_B3
    for (OSRExit& exit : osrExit) {
        if (exit.m_exceptionHandlerCallSiteIndex.bits() == callSiteIndex.bits()) {
            RELEASE_ASSERT(exit.isExceptionHandler());
            RELEASE_ASSERT(exit.isGenericUnwindHandler());
            return ValueRep::usedRegisters(exit.m_valueReps);
        }
    }
#else // FTL_USES_B3
    for (OSRExit& exit : osrExit) {
        if (exit.m_exceptionHandlerCallSiteIndex.bits() == callSiteIndex.bits()) {
            RELEASE_ASSERT(exit.isExceptionHandler());
            return stackmaps.records[exit.m_stackmapRecordIndex].usedRegisterSet();
        }
    }
#endif // FTL_USES_B3
    return RegisterSet();
}

Optional<CodeOrigin> JITCode::findPC(CodeBlock* codeBlock, void* pc)
{
    for (OSRExit& exit : osrExit) {
        if (ExecutableMemoryHandle* handle = exit.m_code.executableMemory()) {
            if (handle->start() <= pc && pc < handle->end())
                return Optional<CodeOrigin>(exit.m_codeOriginForExitProfile);
        }
    }

    for (std::unique_ptr<LazySlowPath>& lazySlowPath : lazySlowPaths) {
        if (ExecutableMemoryHandle* handle = lazySlowPath->stub().executableMemory()) {
            if (handle->start() <= pc && pc < handle->end())
                return Optional<CodeOrigin>(codeBlock->codeOrigin(lazySlowPath->callSiteIndex()));
        }
    }

    return Nullopt;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

