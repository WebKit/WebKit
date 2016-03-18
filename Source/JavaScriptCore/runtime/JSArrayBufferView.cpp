/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
#include "JSArrayBufferView.h"

#include "JSArrayBuffer.h"
#include "JSCInlines.h"
#include "Reject.h"

namespace JSC {

const ClassInfo JSArrayBufferView::s_info = {
    "ArrayBufferView", &Base::s_info, 0, CREATE_METHOD_TABLE(JSArrayBufferView)
};

JSArrayBufferView::ConstructionContext::ConstructionContext(
    VM& vm, Structure* structure, uint32_t length, uint32_t elementSize,
    InitializationMode mode)
    : m_structure(0)
    , m_length(length)
    , m_butterfly(0)
{
    if (length <= fastSizeLimit) {
        // Attempt GC allocation.
        void* temp = 0;
        size_t size = sizeOf(length, elementSize);
        // CopiedSpace only allows non-zero size allocations.
        if (size && !vm.heap.tryAllocateStorage(0, size, &temp))
            return;

        m_structure = structure;
        m_vector = temp;
        m_mode = FastTypedArray;

#if USE(JSVALUE32_64)
        if (mode == ZeroFill) {
            uint64_t* asWords = static_cast<uint64_t*>(m_vector);
            for (unsigned i = size / sizeof(uint64_t); i--;)
                asWords[i] = 0;
        }
#endif // USE(JSVALUE32_64)
        
        return;
    }

    // Don't allow a typed array to use more than 2GB.
    if (length > static_cast<unsigned>(INT_MAX) / elementSize)
        return;
    
    if (mode == ZeroFill) {
        if (!tryFastCalloc(length, elementSize).getValue(m_vector))
            return;
    } else {
        if (!tryFastMalloc(length * elementSize).getValue(m_vector))
            return;
    }
    
    vm.heap.reportExtraMemoryAllocated(static_cast<size_t>(length) * elementSize);
    
    m_structure = structure;
    m_mode = OversizeTypedArray;
}

JSArrayBufferView::ConstructionContext::ConstructionContext(
    VM& vm, Structure* structure, PassRefPtr<ArrayBuffer> arrayBuffer,
    unsigned byteOffset, unsigned length)
    : m_structure(structure)
    , m_length(length)
    , m_mode(WastefulTypedArray)
{
    m_vector = static_cast<uint8_t*>(arrayBuffer->data()) + byteOffset;
    IndexingHeader indexingHeader;
    indexingHeader.setArrayBuffer(arrayBuffer.get());
    m_butterfly = Butterfly::create(vm, 0, 0, 0, true, indexingHeader, 0);
}

JSArrayBufferView::ConstructionContext::ConstructionContext(
    Structure* structure, PassRefPtr<ArrayBuffer> arrayBuffer,
    unsigned byteOffset, unsigned length, DataViewTag)
    : m_structure(structure)
    , m_length(length)
    , m_mode(DataViewMode)
    , m_butterfly(0)
{
    m_vector = static_cast<uint8_t*>(arrayBuffer->data()) + byteOffset;
}

JSArrayBufferView::JSArrayBufferView(VM& vm, ConstructionContext& context)
    : Base(vm, context.structure(), context.butterfly())
    , m_length(context.length())
    , m_mode(context.mode())
{
    m_vector.setWithoutBarrier(static_cast<char*>(context.vector()));
}

void JSArrayBufferView::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    switch (m_mode) {
    case FastTypedArray:
        return;
    case OversizeTypedArray:
        vm.heap.addFinalizer(this, finalize);
        return;
    case WastefulTypedArray:
        vm.heap.addReference(this, butterfly()->indexingHeader()->arrayBuffer());
        return;
    case DataViewMode:
        ASSERT(!butterfly());
        vm.heap.addReference(this, jsCast<JSDataView*>(this)->buffer());
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void JSArrayBufferView::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(cell);

    if (thisObject->hasArrayBuffer()) {
        ArrayBuffer* buffer = thisObject->buffer();
        RELEASE_ASSERT(buffer);
        visitor.addOpaqueRoot(buffer);
    }
    
    Base::visitChildren(thisObject, visitor);
}

bool JSArrayBufferView::put(
    JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value,
    PutPropertySlot& slot)
{
    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(cell);

    if (UNLIKELY(isThisValueAltered(slot, thisObject)))
        return ordinarySetSlow(exec, thisObject, propertyName, value, slot.thisValue(), slot.isStrictMode());
    
    return Base::put(thisObject, exec, propertyName, value, slot);
}
    
void JSArrayBufferView::finalize(JSCell* cell)
{
    JSArrayBufferView* thisObject = static_cast<JSArrayBufferView*>(cell);
    ASSERT(thisObject->m_mode == OversizeTypedArray || thisObject->m_mode == WastefulTypedArray);
    if (thisObject->m_mode == OversizeTypedArray)
        fastFree(thisObject->m_vector.get());
}

} // namespace JSC

namespace WTF {

using namespace JSC;

void printInternal(PrintStream& out, TypedArrayMode mode)
{
    switch (mode) {
    case FastTypedArray:
        out.print("FastTypedArray");
        return;
    case OversizeTypedArray:
        out.print("OversizeTypedArray");
        return;
    case WastefulTypedArray:
        out.print("WastefulTypedArray");
        return;
    case DataViewMode:
        out.print("DataViewMode");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

