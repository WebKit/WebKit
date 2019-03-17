/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebAssemblyToJSCallee.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyModule.h"

namespace JSC {

const ClassInfo WebAssemblyToJSCallee::s_info = { "WebAssemblyToJSCallee", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WebAssemblyToJSCallee) };

WebAssemblyToJSCallee* WebAssemblyToJSCallee::create(VM& vm, Structure* structure, JSWebAssemblyModule* module)
{
    WebAssemblyToJSCallee* callee = new (NotNull, allocateCell<WebAssemblyToJSCallee>(vm.heap)) WebAssemblyToJSCallee(vm, structure);
    callee->finishCreation(vm, module);
    return callee;
}

Structure* WebAssemblyToJSCallee::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(WebAssemblyToJSCalleeType, StructureFlags), info());
}

WebAssemblyToJSCallee::WebAssemblyToJSCallee(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void WebAssemblyToJSCallee::finishCreation(VM& vm, JSWebAssemblyModule* module)
{
    Base::finishCreation(vm);
    m_module.set(vm, this, module);
}

void WebAssemblyToJSCallee::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<WebAssemblyToJSCallee*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_module);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
