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
#include "JSHeapInt32.h"

#include "JSCJSValueInlines.h"
#include "JSObjectInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo JSHeapInt32::s_info = { "(Internal) Int32"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSHeapInt32) };

JSHeapInt32::JSHeapInt32(VM& vm, Structure* structure, double value)
    : Base(vm, structure)
    , m_value(value)
{
    RELEASE_ASSERT(useCompressedHeap);
}

Structure* JSHeapInt32::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    if constexpr (!useCompressedHeap)
        return nullptr;
    RELEASE_ASSERT(&vmForHeapDouble() == &vm);
    return Structure::create(vm, globalObject, prototype, TypeInfo(HeapInt32Type, StructureFlags), info());
}

JSHeapInt32* JSHeapInt32::createImpl(int32_t value)
{
    JSHeapInt32* result = nullptr;
    callOnMainThreadAndWait([&result, value] {
        DeferGC deferScope(vmForHeapDouble());
        JSHeapInt32* i = new (NotNull, allocateCell<JSHeapInt32>(vmForHeapDouble())) JSHeapInt32(vmForHeapDouble(), vmForHeapDouble().int32Structure.get(), value);
        i->finishCreation(vmForHeapDouble());
        result = i;
    });
    RELEASE_ASSERT(result);
    return result;
}

JSHeapInt32* JSHeapInt32::createFrom(int32_t value)
{
    // This is a hacky workaround since we can't allocate while constructing.
    // This will go away once boxed numbers are no longer cells.
    static JSHeapInt32* zero = nullptr;
    static JSHeapInt32* one = nullptr;

    if (!zero) {
        zero = createImpl(0);
        one = createImpl(1);
    }

    if (!value)
        return zero;
    if (value == 1)
        return one;

    return createImpl(value);
}

} // namespace JSC
