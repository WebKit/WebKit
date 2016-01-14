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

#ifndef JITThunks_h
#define JITThunks_h

#if ENABLE(JIT)

#include "CallData.h"
#include "Intrinsic.h"
#include "LowLevelInterpreter.h"
#include "MacroAssemblerCodeRef.h"
#include "ThunkGenerator.h"
#include "Weak.h"
#include "WeakHandleOwner.h"
#include "WeakInlines.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadingPrimitives.h>

namespace JSC {

class VM;
class NativeExecutable;

class JITThunks final : private WeakHandleOwner {
    WTF_MAKE_FAST_ALLOCATED;
public:
    JITThunks();
    virtual ~JITThunks();

    MacroAssemblerCodePtr ctiNativeCall(VM*);
    MacroAssemblerCodePtr ctiNativeConstruct(VM*);
    MacroAssemblerCodePtr ctiNativeTailCall(VM*);    

    MacroAssemblerCodeRef ctiStub(VM*, ThunkGenerator);

    NativeExecutable* hostFunctionStub(VM*, NativeFunction, NativeFunction constructor, const String& name);
    NativeExecutable* hostFunctionStub(VM*, NativeFunction, ThunkGenerator, Intrinsic, const String& name);

    void clearHostFunctionStubs();

private:
    void finalize(Handle<Unknown>, void* context) override;
    
    typedef HashMap<ThunkGenerator, MacroAssemblerCodeRef> CTIStubMap;
    CTIStubMap m_ctiStubMap;
    typedef HashMap<std::pair<NativeFunction, NativeFunction>, Weak<NativeExecutable>> HostFunctionStubMap;
    std::unique_ptr<HostFunctionStubMap> m_hostFunctionStubMap;
    Lock m_lock;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JITThunks_h

