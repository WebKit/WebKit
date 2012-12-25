/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef JITThunks_h
#define JITThunks_h

#include <wtf/Platform.h>

#if ENABLE(JIT)

#include "CallData.h"
#include "Intrinsic.h"
#include "LowLevelInterpreter.h"
#include "MacroAssemblerCodeRef.h"
#include "Register.h"
#include "ResolveOperation.h"
#include "ThunkGenerators.h"
#include <wtf/HashMap.h>

namespace JSC {

class JSGlobalData;
class NativeExecutable;

struct TrampolineStructure {
    MacroAssemblerCodePtr ctiStringLengthTrampoline;
    MacroAssemblerCodePtr ctiVirtualCallLink;
    MacroAssemblerCodePtr ctiVirtualConstructLink;
    MacroAssemblerCodePtr ctiVirtualCall;
    MacroAssemblerCodePtr ctiVirtualConstruct;
    MacroAssemblerCodePtr ctiNativeCall;
    MacroAssemblerCodePtr ctiNativeConstruct;
};

class JITThunksPrivateData;

class JITThunks {
public:
    JITThunks(JSGlobalData*);
    ~JITThunks();

    MacroAssemblerCodePtr ctiStringLengthTrampoline() { return m_trampolineStructure.ctiStringLengthTrampoline; }
    MacroAssemblerCodePtr ctiVirtualCallLink() { return m_trampolineStructure.ctiVirtualCallLink; }
    MacroAssemblerCodePtr ctiVirtualConstructLink() { return m_trampolineStructure.ctiVirtualConstructLink; }
    MacroAssemblerCodePtr ctiVirtualCall() { return m_trampolineStructure.ctiVirtualCall; }
    MacroAssemblerCodePtr ctiVirtualConstruct() { return m_trampolineStructure.ctiVirtualConstruct; }
    MacroAssemblerCodePtr ctiNativeCall()
    {
#if ENABLE(LLINT)
        if (!m_executableMemory)
            return MacroAssemblerCodePtr::createLLIntCodePtr(llint_native_call_trampoline);
#endif
        return m_trampolineStructure.ctiNativeCall;
    }
    MacroAssemblerCodePtr ctiNativeConstruct()
    {
#if ENABLE(LLINT)
        if (!m_executableMemory)
            return MacroAssemblerCodePtr::createLLIntCodePtr(llint_native_construct_trampoline);
#endif
        return m_trampolineStructure.ctiNativeConstruct;
    }

    MacroAssemblerCodeRef ctiStub(JSGlobalData*, ThunkGenerator);

    NativeExecutable* hostFunctionStub(JSGlobalData*, NativeFunction, NativeFunction constructor);
    NativeExecutable* hostFunctionStub(JSGlobalData*, NativeFunction, ThunkGenerator, Intrinsic);

    void clearHostFunctionStubs();

private:
    typedef HashMap<ThunkGenerator, MacroAssemblerCodeRef> CTIStubMap;
    CTIStubMap m_ctiStubMap;
    typedef HashMap<NativeFunction, Weak<NativeExecutable> > HostFunctionStubMap;
    OwnPtr<HostFunctionStubMap> m_hostFunctionStubMap;
    RefPtr<ExecutableMemoryHandle> m_executableMemory;

    TrampolineStructure m_trampolineStructure;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JITThunks_h

