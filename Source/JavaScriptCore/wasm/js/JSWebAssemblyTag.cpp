/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyTag.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCellInlines.h"
#include "JSObjectInlines.h"
#include "StructureInlines.h"
#include "WasmTag.h"

namespace JSC {

const ClassInfo JSWebAssemblyTag::s_info = { "WebAssembly.Tag", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyTag) };

JSWebAssemblyTag* JSWebAssemblyTag::create(VM& vm, JSGlobalObject* globalObject, Structure* structure, const Wasm::Tag& tag)
{
    UNUSED_PARAM(globalObject);
    auto* jsTag = new (NotNull, allocateCell<JSWebAssemblyTag>(vm)) JSWebAssemblyTag(vm, structure, tag);
    jsTag->finishCreation(vm);
    return jsTag;
}

Structure* JSWebAssemblyTag::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSWebAssemblyTag::JSWebAssemblyTag(VM& vm, Structure* structure, const Wasm::Tag& tag)
    : Base(vm, structure)
    , m_tag(Ref { tag })
{
}

void JSWebAssemblyTag::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyTag*>(cell)->JSWebAssemblyTag::~JSWebAssemblyTag();
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
