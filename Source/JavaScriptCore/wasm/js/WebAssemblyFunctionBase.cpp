/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "WebAssemblyFunctionBase.h"

#if ENABLE(WEBASSEMBLY)

#include "HeapCellInlines.h"
#include "HeapInlines.h"
#include "JSWebAssemblyInstance.h"
#include "SlotVisitorInlines.h"

namespace JSC {

const ClassInfo WebAssemblyFunctionBase::s_info = { "WebAssemblyFunctionBase", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyFunctionBase) };

WebAssemblyFunctionBase::WebAssemblyFunctionBase(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, WasmToWasmImportableFunction importableFunction)
    : Base(vm, executable, globalObject, structure)
    , m_importableFunction(importableFunction)
{ }

template<typename Visitor>
void WebAssemblyFunctionBase::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    WebAssemblyFunctionBase* thisObject = jsCast<WebAssemblyFunctionBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_instance);
}

DEFINE_VISIT_CHILDREN(WebAssemblyFunctionBase);

void WebAssemblyFunctionBase::finishCreation(VM& vm, NativeExecutable* executable, unsigned length, const String& name, JSWebAssemblyInstance* instance)
{
    Base::finishCreation(vm, executable, length, name);
    ASSERT(inherits(vm, info()));
    m_instance.set(vm, this, instance);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
