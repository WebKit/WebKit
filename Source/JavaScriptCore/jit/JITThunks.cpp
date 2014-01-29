/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#include "JITThunks.h"

#if ENABLE(JIT)

#include "Executable.h"
#include "JIT.h"
#include "VM.h"
#include "Operations.h"

namespace JSC {

JITThunks::JITThunks()
    : m_hostFunctionStubMap(adoptPtr(new HostFunctionStubMap))
{
}

JITThunks::~JITThunks()
{
}

MacroAssemblerCodePtr JITThunks::ctiNativeCall(VM* vm)
{
#if ENABLE(LLINT)
    if (!vm->canUseJIT())
        return MacroAssemblerCodePtr::createLLIntCodePtr(llint_native_call_trampoline);
#endif
    return ctiStub(vm, nativeCallGenerator).code();
}

MacroAssemblerCodePtr JITThunks::ctiNativeConstruct(VM* vm)
{
#if ENABLE(LLINT)
    if (!vm->canUseJIT())
        return MacroAssemblerCodePtr::createLLIntCodePtr(llint_native_construct_trampoline);
#endif
    return ctiStub(vm, nativeConstructGenerator).code();
}

MacroAssemblerCodePtr JITThunks::ctiNativeTailCall(VM* vm)
{
    ASSERT(vm->canUseJIT());
    return ctiStub(vm, nativeTailCallGenerator).code();
}

MacroAssemblerCodeRef JITThunks::ctiStub(VM* vm, ThunkGenerator generator)
{
    Locker locker(m_lock);
    CTIStubMap::AddResult entry = m_ctiStubMap.add(generator, MacroAssemblerCodeRef());
    if (entry.isNewEntry) {
        // Compilation thread can only retrieve existing entries.
        ASSERT(!isCompilationThread());
        entry.iterator->value = generator(vm);
    }
    return entry.iterator->value;
}

NativeExecutable* JITThunks::hostFunctionStub(VM* vm, NativeFunction function, NativeFunction constructor)
{
    ASSERT(!isCompilationThread());

    if (NativeExecutable* nativeExecutable = m_hostFunctionStubMap->get(std::make_pair(function, constructor)))
        return nativeExecutable;

    NativeExecutable* nativeExecutable = NativeExecutable::create(
        *vm,
        adoptRef(new NativeJITCode(JIT::compileCTINativeCall(vm, function), JITCode::HostCallThunk)),
        function,
        adoptRef(new NativeJITCode(MacroAssemblerCodeRef::createSelfManagedCodeRef(ctiNativeConstruct(vm)), JITCode::HostCallThunk)),
        constructor, NoIntrinsic);
    weakAdd(*m_hostFunctionStubMap, std::make_pair(function, constructor), Weak<NativeExecutable>(nativeExecutable));
    return nativeExecutable;
}

NativeExecutable* JITThunks::hostFunctionStub(VM* vm, NativeFunction function, ThunkGenerator generator, Intrinsic intrinsic)
{
    ASSERT(!isCompilationThread());    

    if (NativeExecutable* nativeExecutable = m_hostFunctionStubMap->get(std::make_pair(function, &callHostFunctionAsConstructor)))
        return nativeExecutable;

    RefPtr<JITCode> forCall;
    if (generator) {
        if (vm->canUseJIT()) {
            MacroAssemblerCodeRef entry = generator(vm);
            forCall = adoptRef(new DirectJITCode(entry, entry.code(), JITCode::HostCallThunk));
        }
    } else
        forCall = adoptRef(new NativeJITCode(JIT::compileCTINativeCall(vm, function), JITCode::HostCallThunk));
    
    RefPtr<JITCode> forConstruct = adoptRef(new NativeJITCode(MacroAssemblerCodeRef::createSelfManagedCodeRef(ctiNativeConstruct(vm)), JITCode::HostCallThunk));
    
    NativeExecutable* nativeExecutable = NativeExecutable::create(*vm, forCall, function, forConstruct, callHostFunctionAsConstructor, intrinsic);
    weakAdd(*m_hostFunctionStubMap, std::make_pair(function, &callHostFunctionAsConstructor), Weak<NativeExecutable>(nativeExecutable));
    return nativeExecutable;
}

void JITThunks::clearHostFunctionStubs()
{
    m_hostFunctionStubMap.clear();
}

} // namespace JSC

#endif // ENABLE(JIT)
