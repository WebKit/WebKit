/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#include "BatchedTransitionOptimizer.h"
#include "CodeBlock.h"
#include "Debugger.h"
#include "JIT.h"
#include "JSCInlines.h"
#include "LLIntEntrypoint.h"
#include "Parser.h"
#include "TypeProfiler.h"
#include "VMInlines.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo NativeExecutable::s_info = { "NativeExecutable", &ExecutableBase::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(NativeExecutable) };

NativeExecutable* NativeExecutable::create(VM& vm, Ref<JITCode>&& callThunk, TaggedNativeFunction function, Ref<JITCode>&& constructThunk, TaggedNativeFunction constructor, const String& name)
{
    NativeExecutable* executable;
    executable = new (NotNull, allocateCell<NativeExecutable>(vm.heap)) NativeExecutable(vm, function, constructor);
    executable->finishCreation(vm, WTFMove(callThunk), WTFMove(constructThunk), name);
    return executable;
}

void NativeExecutable::destroy(JSCell* cell)
{
    static_cast<NativeExecutable*>(cell)->NativeExecutable::~NativeExecutable();
}

Structure* NativeExecutable::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
{
    return Structure::create(vm, globalObject, proto, TypeInfo(NativeExecutableType, StructureFlags), info());
}

void NativeExecutable::finishCreation(VM& vm, Ref<JITCode>&& callThunk, Ref<JITCode>&& constructThunk, const String& name)
{
    Base::finishCreation(vm);
    m_jitCodeForCall = WTFMove(callThunk);
    m_jitCodeForConstruct = WTFMove(constructThunk);
    m_jitCodeForCallWithArityCheck = m_jitCodeForCall->addressForCall(MustCheckArity);
    m_jitCodeForConstructWithArityCheck = m_jitCodeForConstruct->addressForCall(MustCheckArity);
    m_name = name;

    assertIsTaggedWith(m_jitCodeForCall->addressForCall(ArityCheckNotRequired).executableAddress(), JSEntryPtrTag);
    assertIsTaggedWith(m_jitCodeForConstruct->addressForCall(ArityCheckNotRequired).executableAddress(), JSEntryPtrTag);
    assertIsTaggedWith(m_jitCodeForCallWithArityCheck.executableAddress(), JSEntryPtrTag);
    assertIsTaggedWith(m_jitCodeForConstructWithArityCheck.executableAddress(), JSEntryPtrTag);
}

NativeExecutable::NativeExecutable(VM& vm, TaggedNativeFunction function, TaggedNativeFunction constructor)
    : ExecutableBase(vm, vm.nativeExecutableStructure.get())
    , m_function(function)
    , m_constructor(constructor)
{
}

const DOMJIT::Signature* NativeExecutable::signatureFor(CodeSpecializationKind kind) const
{
    ASSERT(hasJITCodeFor(kind));
    return generatedJITCodeFor(kind)->signature();
}

Intrinsic NativeExecutable::intrinsic() const
{
    return generatedJITCodeFor(CodeForCall)->intrinsic();
}

CodeBlockHash NativeExecutable::hashFor(CodeSpecializationKind kind) const
{
    if (kind == CodeForCall)
        return CodeBlockHash(m_function.bits());

    RELEASE_ASSERT(kind == CodeForConstruct);
    return CodeBlockHash(m_constructor.bits());
}

} // namespace JSC
