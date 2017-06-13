/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "WebAssemblyModulePrototype.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"

#include "WebAssemblyModulePrototype.lut.h"

namespace JSC {

const ClassInfo WebAssemblyModulePrototype::s_info = { "WebAssembly.Module", &Base::s_info, &prototypeTableWebAssemblyModule, nullptr, CREATE_METHOD_TABLE(WebAssemblyModulePrototype) };

/* Source for WebAssemblyModulePrototype.lut.h
 @begin prototypeTableWebAssemblyModule
 @end
 */

WebAssemblyModulePrototype* WebAssemblyModulePrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyModulePrototype>(vm.heap)) WebAssemblyModulePrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* WebAssemblyModulePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyModulePrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
}

WebAssemblyModulePrototype::WebAssemblyModulePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
