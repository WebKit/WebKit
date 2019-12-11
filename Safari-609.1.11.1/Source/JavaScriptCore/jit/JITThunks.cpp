/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "JIT.h"
#include "JSCInlines.h"
#include "LLIntData.h"
#include "ThunkGenerators.h"
#include "VM.h"

namespace JSC {

JITThunks::JITThunks()
    : m_hostFunctionStubMap(makeUnique<HostFunctionStubMap>())
{
}

JITThunks::~JITThunks()
{
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiNativeCall(VM& vm)
{
    ASSERT(VM::canUseJIT());
    return ctiStub(vm, nativeCallGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiNativeConstruct(VM& vm)
{
    ASSERT(VM::canUseJIT());
    return ctiStub(vm, nativeConstructGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiNativeTailCall(VM& vm)
{
    ASSERT(VM::canUseJIT());
    return ctiStub(vm, nativeTailCallGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiNativeTailCallWithoutSavedTags(VM& vm)
{
    ASSERT(VM::canUseJIT());
    return ctiStub(vm, nativeTailCallWithoutSavedTagsGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiInternalFunctionCall(VM& vm)
{
    ASSERT(VM::canUseJIT());
    return ctiStub(vm, internalFunctionCallGenerator).code();
}

MacroAssemblerCodePtr<JITThunkPtrTag> JITThunks::ctiInternalFunctionConstruct(VM& vm)
{
    ASSERT(VM::canUseJIT());
    return ctiStub(vm, internalFunctionConstructGenerator).code();
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::ctiStub(VM& vm, ThunkGenerator generator)
{
    LockHolder locker(m_lock);
    CTIStubMap::AddResult entry = m_ctiStubMap.add(generator, MacroAssemblerCodeRef<JITThunkPtrTag>());
    if (entry.isNewEntry) {
        // Compilation thread can only retrieve existing entries.
        ASSERT(!isCompilationThread());
        entry.iterator->value = generator(vm);
    }
    return entry.iterator->value;
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::existingCTIStub(ThunkGenerator generator)
{
    LockHolder locker(m_lock);
    CTIStubMap::iterator entry = m_ctiStubMap.find(generator);
    if (entry == m_ctiStubMap.end())
        return MacroAssemblerCodeRef<JITThunkPtrTag>();
    return entry->value;
}

void JITThunks::finalize(Handle<Unknown> handle, void*)
{
    auto* nativeExecutable = static_cast<NativeExecutable*>(handle.get().asCell());
    weakRemove(*m_hostFunctionStubMap, std::make_tuple(nativeExecutable->function(), nativeExecutable->constructor(), nativeExecutable->name()), nativeExecutable);
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, TaggedNativeFunction constructor, const String& name)
{
    return hostFunctionStub(vm, function, constructor, nullptr, NoIntrinsic, nullptr, name);
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, TaggedNativeFunction constructor, ThunkGenerator generator, Intrinsic intrinsic, const DOMJIT::Signature* signature, const String& name)
{
    ASSERT(!isCompilationThread());    
    ASSERT(VM::canUseJIT());

    if (NativeExecutable* nativeExecutable = m_hostFunctionStubMap->get(std::make_tuple(function, constructor, name)))
        return nativeExecutable;

    RefPtr<JITCode> forCall;
    if (generator) {
        MacroAssemblerCodeRef<JSEntryPtrTag> entry = generator(vm).retagged<JSEntryPtrTag>();
        forCall = adoptRef(new DirectJITCode(entry, entry.code(), JITType::HostCallThunk, intrinsic));
    } else if (signature)
        forCall = adoptRef(new NativeDOMJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeCall(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, intrinsic, signature));
    else
        forCall = adoptRef(new NativeJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeCall(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, intrinsic));
    
    Ref<JITCode> forConstruct = adoptRef(*new NativeJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeConstruct(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, NoIntrinsic));
    
    NativeExecutable* nativeExecutable = NativeExecutable::create(vm, forCall.releaseNonNull(), function, WTFMove(forConstruct), constructor, name);
    weakAdd(*m_hostFunctionStubMap, std::make_tuple(function, constructor, name), Weak<NativeExecutable>(nativeExecutable, this));
    return nativeExecutable;
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, ThunkGenerator generator, Intrinsic intrinsic, const String& name)
{
    return hostFunctionStub(vm, function, callHostFunctionAsConstructor, generator, intrinsic, nullptr, name);
}

void JITThunks::clearHostFunctionStubs()
{
    m_hostFunctionStubMap = nullptr;
}

} // namespace JSC

#endif // ENABLE(JIT)
