/*
 * Copyright (C) 2025 Igalia S.L.
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
 *
 */

#include "config.h"
#include "JSHeapDouble.h"

#include "JSCJSValueInlines.h"
#include "JSObjectInlines.h"
#include "StructureInlines.h"

#include <JavaScriptCore/VMManager.h>
#include <mutex>

namespace JSC {

VM& vmForHeapDouble()
{
    RELEASE_ASSERT(useCompressedHeap);
    static VM* vm = nullptr;
    static std::once_flag once;
    std::call_once(once, [&] {
        VMManager::forEachVM([&] (VM& nextVM) {
            RELEASE_ASSERT(vm == nullptr);
            vm = &nextVM;
            return IterationStatus::Continue;
        });
    });
    RELEASE_ASSERT(VMManager::numberOfVMs() == 1);
    return *vm;
}

const ClassInfo JSHeapDouble::s_info = { "(Internal) Double"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSHeapDouble) };

JSHeapDouble::JSHeapDouble(VM& vm, Structure* structure, double value)
    : Base(vm, structure)
    , m_value(value)
{
    RELEASE_ASSERT(useCompressedHeap);
}

Structure* JSHeapDouble::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    if constexpr (!useCompressedHeap)
        return nullptr;
    RELEASE_ASSERT(&vmForHeapDouble() == &vm);
    return Structure::create(vm, globalObject, prototype, TypeInfo(HeapDoubleType, StructureFlags), info());
}

JSHeapDouble* JSHeapDouble::createFrom(double value)
{
    JSHeapDouble* result = nullptr;
    callOnMainThreadAndWait([&result, value] {
        DeferGC deferScope(vmForHeapDouble());
        JSHeapDouble* d = new (NotNull, allocateCell<JSHeapDouble>(vmForHeapDouble())) JSHeapDouble(vmForHeapDouble(), vmForHeapDouble().doubleStructure.get(), value);
        d->finishCreation(vmForHeapDouble());
        result = d;
    });
    RELEASE_ASSERT(result);
    return result;
}

} // namespace JSC
