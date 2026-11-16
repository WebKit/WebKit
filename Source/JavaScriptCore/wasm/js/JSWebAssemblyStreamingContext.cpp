/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyStreamingContext.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSPromise.h"

namespace JSC {

const ClassInfo JSWebAssemblyStreamingContext::s_info = { "WebAssemblyStreamingContext"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyStreamingContext) };

JSWebAssemblyStreamingContext::JSWebAssemblyStreamingContext(VM& vm, Structure* structure, JSPromise* promise, JSObject* importObject, std::optional<WebAssemblyCompileOptions>&& compileOptions)
    : Base(vm, structure)
    , m_promise(promise, WriteBarrierEarlyInit)
    , m_importObject(importObject, WriteBarrierEarlyInit)
    , m_compileOptions(WTF::move(compileOptions))
{
}

JSWebAssemblyStreamingContext::~JSWebAssemblyStreamingContext() = default;

JSWebAssemblyStreamingContext* JSWebAssemblyStreamingContext::create(VM& vm, JSPromise* promise, JSObject* importObject, std::optional<WebAssemblyCompileOptions>&& compileOptions)
{
    auto* structure = vm.webAssemblyStreamingContextStructure.get();
    JSWebAssemblyStreamingContext* result = new (NotNull, allocateCell<JSWebAssemblyStreamingContext>(vm)) JSWebAssemblyStreamingContext(vm, structure, promise, importObject, WTF::move(compileOptions));
    result->finishCreation(vm);
    return result;
}

void JSWebAssemblyStreamingContext::destroy(JSCell* cell)
{
    SUPPRESS_MEMORY_UNSAFE_CAST static_cast<JSWebAssemblyStreamingContext*>(cell)->JSWebAssemblyStreamingContext::~JSWebAssemblyStreamingContext();
}

template<typename Visitor>
void JSWebAssemblyStreamingContext::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<JSWebAssemblyStreamingContext>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_promise);
    visitor.append(thisObject->m_importObject);
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyStreamingContext);

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
