/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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
#include "NativeExecutable.h"

#include "Debugger.h"
#include "ExecutableBaseInlines.h"
#include "JSCInlines.h"
#include "VMInlines.h"

namespace JSC {

const ClassInfo NativeExecutable::s_info = { "NativeExecutable"_s, &ExecutableBase::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(NativeExecutable) };

NativeExecutable* NativeExecutable::create(VM& vm, Ref<JSC::JITCode>&& callThunk, TaggedNativeFunction function, Ref<JSC::JITCode>&& constructThunk, TaggedNativeFunction constructor, ImplementationVisibility implementationVisibility, const String& name)
{
    NativeExecutable* executable;
    executable = new (NotNull, allocateCell<NativeExecutable>(vm)) NativeExecutable(vm, function, constructor, implementationVisibility);
    executable->finishCreation(vm, WTFMove(callThunk), WTFMove(constructThunk), name);

    vm.forEachDebugger([&] (Debugger& debugger) {
        debugger.didCreateNativeExecutable(*executable);
    });

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

void NativeExecutable::finishCreation(VM& vm, Ref<JSC::JITCode>&& callThunk, Ref<JSC::JITCode>&& constructThunk, const String& name)
{
    Base::finishCreation(vm);
    m_jitCodeForCall = WTFMove(callThunk);
    m_jitCodeForConstruct = WTFMove(constructThunk);
    m_jitCodeForCallWithArityCheck = m_jitCodeForCall->addressForCall(MustCheckArity);
    m_jitCodeForConstructWithArityCheck = m_jitCodeForConstruct->addressForCall(MustCheckArity);
    m_name = name;

    assertIsTaggedWith<JSEntryPtrTag>(m_jitCodeForCall->addressForCall(ArityCheckNotRequired).taggedPtr());
    assertIsTaggedWith<JSEntryPtrTag>(m_jitCodeForConstruct->addressForCall(ArityCheckNotRequired).taggedPtr());
    assertIsTaggedWith<JSEntryPtrTag>(m_jitCodeForCallWithArityCheck.taggedPtr());
    assertIsTaggedWith<JSEntryPtrTag>(m_jitCodeForConstructWithArityCheck.taggedPtr());
}

NativeExecutable::NativeExecutable(VM& vm, TaggedNativeFunction function, TaggedNativeFunction constructor, ImplementationVisibility implementationVisibility)
    : ExecutableBase(vm, vm.nativeExecutableStructure.get())
    , m_function(function)
    , m_constructor(constructor)
    , m_implementationVisibility(static_cast<unsigned>(implementationVisibility))
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
        return CodeBlockHash(bitwise_cast<uintptr_t>(m_function));

    RELEASE_ASSERT(kind == CodeForConstruct);
    return CodeBlockHash(bitwise_cast<uintptr_t>(m_constructor));
}

JSString* NativeExecutable::toStringSlow(JSGlobalObject *globalObject)
{
    VM& vm = getVM(globalObject);

    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSValue value = jsMakeNontrivialString(globalObject, "function ", name(), "() {\n    [native code]\n}");

    RETURN_IF_EXCEPTION(throwScope, nullptr);

    JSString* asString = ::JSC::asString(value);
    WTF::storeStoreFence();
    m_asString.set(vm, this, asString);
    return asString;
}

template<typename Visitor>
void NativeExecutable::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    NativeExecutable* thisObject = jsCast<NativeExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_asString);
}

DEFINE_VISIT_CHILDREN(NativeExecutable);

} // namespace JSC
