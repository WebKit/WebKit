/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "GenericTypedArrayViewInlines.h"
#include "JSCInlines.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSTypedArrays.h"
#include "TypedArrayController.h"
#include "TypedArrays.h"
#include <wtf/Gigacage.h>

namespace JSC {

const ClassInfo JSArrayBufferView::s_info = {
    "ArrayBufferView", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSArrayBufferView)
};

JSArrayBufferView::ConstructionContext::ConstructionContext(
    Structure* structure, uint32_t length, void* vector)
    : m_structure(structure)
    , m_vector(vector, length)
    , m_length(length)
    , m_mode(FastTypedArray)
    , m_butterfly(nullptr)
{
    ASSERT(!Gigacage::isEnabled() || (Gigacage::contains(vector) && Gigacage::contains(static_cast<const uint8_t*>(vector) + length - 1)));
    ASSERT(vector == removeArrayPtrTag(vector));
    RELEASE_ASSERT(length <= fastSizeLimit);
}

JSArrayBufferView::ConstructionContext::ConstructionContext(
    VM& vm, Structure* structure, uint32_t length, uint32_t elementSize,
    InitializationMode mode)
    : m_structure(nullptr)
    , m_length(length)
    , m_butterfly(nullptr)
{
    if (length <= fastSizeLimit) {
        // Attempt GC allocation.
        void* temp;
        size_t size = sizeOf(length, elementSize);
        temp = vm.primitiveGigacageAuxiliarySpace.allocateNonVirtual(vm, size, nullptr, AllocationFailureMode::ReturnNull);
        if (!temp)
            return;

        m_structure = structure;
        m_vector = VectorType(temp, length);
        m_mode = FastTypedArray;

        if (mode == ZeroFill) {
            uint64_t* asWords = static_cast<uint64_t*>(vector());
            for (unsigned i = size / sizeof(uint64_t); i--;)
                asWords[i] = 0;
        }
        
        return;
    }

    // Don't allow a typed array to use more than 2GB.
    if (length > static_cast<unsigned>(INT_MAX) / elementSize)
        return;
    
    size_t size = static_cast<size_t>(length) * static_cast<size_t>(elementSize);
    m_vector = VectorType(Gigacage::tryMalloc(Gigacage::Primitive, size), length);
    if (!m_vector)
        return;
    if (mode == ZeroFill)
        memset(vector(), 0, size);
    
    vm.heap.reportExtraMemoryAllocated(static_cast<size_t>(length) * elementSize);
    
    m_structure = structure;
    m_mode = OversizeTypedArray;
}

JSArrayBufferView::ConstructionContext::ConstructionContext(
    VM& vm, Structure* structure, RefPtr<ArrayBuffer>&& arrayBuffer,
    unsigned byteOffset, unsigned length)
    : m_structure(structure)
    , m_length(length)
    , m_mode(WastefulTypedArray)
{
    ASSERT(arrayBuffer->data() == removeArrayPtrTag(arrayBuffer->data()));
    m_vector = VectorType(static_cast<uint8_t*>(arrayBuffer->data()) + byteOffset, length);
    IndexingHeader indexingHeader;
    indexingHeader.setArrayBuffer(arrayBuffer.get());
    m_butterfly = Butterfly::create(vm, nullptr, 0, 0, true, indexingHeader, 0);
}

JSArrayBufferView::ConstructionContext::ConstructionContext(
    Structure* structure, RefPtr<ArrayBuffer>&& arrayBuffer,
    unsigned byteOffset, unsigned length, DataViewTag)
    : m_structure(structure)
    , m_length(length)
    , m_mode(DataViewMode)
    , m_butterfly(nullptr)
{
    ASSERT(arrayBuffer->data() == removeArrayPtrTag(arrayBuffer->data()));
    m_vector = VectorType(static_cast<uint8_t*>(arrayBuffer->data()) + byteOffset, length);
}

JSArrayBufferView::JSArrayBufferView(VM& vm, ConstructionContext& context)
    : Base(vm, context.structure(), nullptr)
    , m_length(context.length())
    , m_mode(context.mode())
{
    setButterfly(vm, context.butterfly());
    ASSERT(context.vector() == removeArrayPtrTag(context.vector()));
    m_vector.setWithoutBarrier(context.vector(), m_length);
}

void JSArrayBufferView::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(jsDynamicCast<JSArrayBufferView*>(vm, this));
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
        vm.heap.addReference(this, jsCast<JSDataView*>(this)->possiblySharedBuffer());
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename Visitor>
void JSArrayBufferView::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSArrayBufferView* thisObject = jsCast<JSArrayBufferView*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);

    if (thisObject->hasArrayBuffer()) {
        WTF::loadLoadFence();
        ArrayBuffer* buffer = thisObject->possiblySharedBuffer();
        RELEASE_ASSERT(buffer);
        visitor.addOpaqueRoot(buffer);
    }
}

DEFINE_VISIT_CHILDREN(JSArrayBufferView);

ArrayBuffer* JSArrayBufferView::unsharedBuffer()
{
    ArrayBuffer* result = possiblySharedBuffer();
    RELEASE_ASSERT(!result || !result->isShared());
    return result;
}
    
void JSArrayBufferView::finalize(JSCell* cell)
{
    JSArrayBufferView* thisObject = static_cast<JSArrayBufferView*>(cell);

    // This JSArrayBufferView could be an OversizeTypedArray that was converted
    // to a WastefulTypedArray via slowDownAndWasteMemory(). Hence, it is possible
    // to get to this finalizer and found the mode to be WastefulTypedArray.
    ASSERT(thisObject->m_mode == OversizeTypedArray || thisObject->m_mode == WastefulTypedArray);
    if (thisObject->m_mode == OversizeTypedArray)
        Gigacage::free(Gigacage::Primitive, thisObject->vector());
}

JSArrayBuffer* JSArrayBufferView::unsharedJSBuffer(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (ArrayBuffer* buffer = unsharedBuffer())
        return vm.m_typedArrayController->toJS(globalObject, this->globalObject(vm), buffer);
    scope.throwException(globalObject, createOutOfMemoryError(globalObject));
    return nullptr;
}

JSArrayBuffer* JSArrayBufferView::possiblySharedJSBuffer(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (ArrayBuffer* buffer = possiblySharedBuffer())
        return vm.m_typedArrayController->toJS(globalObject, this->globalObject(vm), buffer);
    scope.throwException(globalObject, createOutOfMemoryError(globalObject));
    return nullptr;
}

void JSArrayBufferView::detach()
{
    Locker locker { cellLock() };
    RELEASE_ASSERT(hasArrayBuffer());
    RELEASE_ASSERT(!isShared());
    m_length = 0;
    m_vector.clear();
}

static const constexpr size_t ElementSizeData[] = {
#define FACTORY(type) sizeof(typename type ## Adaptor::Type),
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(FACTORY)
#undef FACTORY
    1, // DataViewType
};

#define FACTORY(type) static_assert(std::is_final<JS ## type ## Array>::value, "");
FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(FACTORY)
#undef FACTORY

static inline size_t elementSize(JSType type)
{
    ASSERT(type >= Int8ArrayType && type <= DataViewType);
    static_assert(BigUint64ArrayType + 1 == DataViewType);
    return ElementSizeData[type - Int8ArrayType];
}

unsigned JSArrayBufferView::byteLength() const
{
    return length() * elementSize(type());
}

ArrayBuffer* JSArrayBufferView::slowDownAndWasteMemory()
{
    ASSERT(m_mode == FastTypedArray || m_mode == OversizeTypedArray);

    // We play this game because we want this to be callable even from places that
    // don't have access to CallFrame* or the VM, and we only allocate so little
    // memory here that it's not necessary to trigger a GC - just accounting what
    // we have done is good enough. The sort of bizarre exception to the "allocating
    // little memory" is when we transfer a backing buffer into the C heap; this
    // will temporarily get counted towards heap footprint (incorrectly, in the case
    // of adopting an oversize typed array) but we don't GC here anyway. That's
    // almost certainly fine. The worst case is if you created a ton of fast typed
    // arrays, and did nothing but caused all of them to slow down and waste memory.
    // In that case, your memory footprint will double before the GC realizes what's
    // up. But if you do *anything* to trigger a GC watermark check, it will know
    // that you *had* done those allocations and it will GC appropriately.
    Heap* heap = Heap::heap(this);
    VM& vm = heap->vm();
    DeferGCForAWhile deferGC(*heap);

    RELEASE_ASSERT(!hasIndexingHeader(vm));
    Structure* structure = this->structure(vm);

    RefPtr<ArrayBuffer> buffer;
    unsigned byteLength = this->byteLength();

    switch (m_mode) {
    case FastTypedArray: {
        buffer = ArrayBuffer::tryCreate(vector(), byteLength);
        if (!buffer)
            return nullptr;
        break;
    }

    case OversizeTypedArray: {
        // FIXME: consider doing something like "subtracting" from extra memory
        // cost, since right now this case will cause the GC to think that we reallocated
        // the whole buffer.
        buffer = ArrayBuffer::createAdopted(vector(), byteLength);
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    RELEASE_ASSERT(buffer);
    // Don't create bufferfly until we know we have an ArrayBuffer.
    setButterfly(vm, Butterfly::createOrGrowArrayRight(
        butterfly(), vm, this, structure,
        structure->outOfLineCapacity(), false, 0, 0));

    {
        Locker locker { cellLock() };
        butterfly()->indexingHeader()->setArrayBuffer(buffer.get());
        m_vector.setWithoutBarrier(buffer->data(), m_length);
        WTF::storeStoreFence();
        m_mode = WastefulTypedArray;
    }
    heap->addReference(this, buffer.get());

    return buffer.get();
}

// Allocates the full-on native buffer and moves data into the C heap if
// necessary. Note that this never allocates in the GC heap.
RefPtr<ArrayBufferView> JSArrayBufferView::possiblySharedImpl()
{
    ArrayBuffer* buffer = possiblySharedBuffer();
    if (!buffer)
        return nullptr;
    unsigned byteOffset = this->byteOffset();
    unsigned length = this->length();
    switch (type()) {
#define FACTORY(type) \
    case type ## ArrayType: \
        return type ## Array::tryCreate(buffer, byteOffset, length);
    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(FACTORY)
#undef FACTORY
    case DataViewType:
        return DataView::create(buffer, byteOffset, length);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }
}

JSArrayBufferView* validateTypedArray(JSGlobalObject* globalObject, JSValue typedArrayValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!typedArrayValue.isCell()) {
        throwTypeError(globalObject, scope, "Argument needs to be a typed array."_s);
        return nullptr;
    }

    JSCell* typedArrayCell = typedArrayValue.asCell();
    if (!isTypedView(typedArrayCell->classInfo(vm)->typedArrayStorageType)) {
        throwTypeError(globalObject, scope, "Argument needs to be a typed array."_s);
        return nullptr;
    }

    JSArrayBufferView* typedArray = jsCast<JSArrayBufferView*>(typedArrayCell);
    if (typedArray->isDetached()) {
        throwTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        return nullptr;
    }
    return typedArray;
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

