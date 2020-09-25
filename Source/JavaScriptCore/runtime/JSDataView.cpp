/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "JSDataView.h"

#include "JSCInlines.h"
#include "TypeError.h"

namespace JSC {

const ClassInfo JSDataView::s_info = {
    "DataView", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDataView)};

JSDataView::JSDataView(VM& vm, ConstructionContext& context, ArrayBuffer* buffer)
    : Base(vm, context)
    , m_buffer(buffer)
{
}

JSDataView* JSDataView::create(
    JSGlobalObject* globalObject, Structure* structure, RefPtr<ArrayBuffer>&& buffer,
    unsigned byteOffset, unsigned byteLength)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(buffer);
    if (buffer->isNeutered()) {
        throwTypeError(globalObject, scope, "Buffer is already detached"_s);
        return nullptr;
    }
    if (!ArrayBufferView::verifySubRangeLength(*buffer, byteOffset, byteLength, sizeof(uint8_t))) {
        throwRangeError(globalObject, scope, "Length out of range of buffer"_s);
        return nullptr;
    }
    if (!ArrayBufferView::verifyByteOffsetAlignment(byteOffset, sizeof(uint8_t))) {
        throwRangeError(globalObject, scope, "Byte offset is not aligned"_s);
        return nullptr;
    }

    ConstructionContext context(
        structure, buffer.copyRef(), byteOffset, byteLength, ConstructionContext::DataView);
    ASSERT(context);
    JSDataView* result =
        new (NotNull, allocateCell<JSDataView>(vm.heap)) JSDataView(vm, context, buffer.get());
    result->finishCreation(vm);
    return result;
}

JSDataView* JSDataView::createUninitialized(JSGlobalObject*, Structure*, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return nullptr;
}

JSDataView* JSDataView::create(JSGlobalObject*, Structure*, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return nullptr;
}

bool JSDataView::set(JSGlobalObject*, unsigned, JSObject*, unsigned, unsigned)
{
    UNREACHABLE_FOR_PLATFORM();
    return false;
}

bool JSDataView::setIndex(JSGlobalObject*, unsigned, JSValue)
{
    UNREACHABLE_FOR_PLATFORM();
    return false;
}

RefPtr<DataView> JSDataView::possiblySharedTypedImpl()
{
    return DataView::create(possiblySharedBuffer(), byteOffset(), length());
}

RefPtr<DataView> JSDataView::unsharedTypedImpl()
{
    return DataView::create(unsharedBuffer(), byteOffset(), length());
}

Structure* JSDataView::createStructure(
    VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(
        vm, globalObject, prototype, TypeInfo(DataViewType, StructureFlags), info(),
        NonArray);
}

} // namespace JSC

