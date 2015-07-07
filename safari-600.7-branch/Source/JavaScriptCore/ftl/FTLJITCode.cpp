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
#include "FTLJITCode.h"

#if ENABLE(FTL_JIT)

namespace JSC { namespace FTL {

JITCode::JITCode()
    : JSC::JITCode(FTLJIT)
{
}

JITCode::~JITCode()
{
}

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

void JITCode::initializeArityCheckEntrypoint(CodeRef entrypoint)
{
    m_arityCheckEntrypoint = entrypoint;
}

void JITCode::initializeAddressForCall(CodePtr address)
{
    m_addressForCall = address;
}

JITCode::CodePtr JITCode::addressForCall(VM&, ExecutableBase*, ArityCheckMode arityCheck, RegisterPreservationMode)
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

JITCode::CodePtr JITCode::exitThunks()
{
    return m_exitThunks.code();
}

JITCode* JITCode::ftl()
{
    return this;
}

DFG::CommonData* JITCode::dfgCommon()
{
    return &common;
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

