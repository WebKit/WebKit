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
#include "B3AbstractHeapRepository.h"

#if ENABLE(B3_JIT)

#include "B3CCallValue.h"
#include "B3FenceValue.h"
#include "B3MemoryValue.h"
#include "B3PatchpointValue.h"
#include "B3ValueInlines.h"
#include "ClonedArguments.h"
#include "ConcatKeyAtomStringCache.h"
#include "DateInstance.h"
#include "DirectArguments.h"
#include "GetterSetter.h"
#include "HasOwnPropertyCache.h"
#include "JSBoundFunction.h"
#include "JSGlobalObject.h"
#include "JSGlobalProxy.h"
#include "JSMap.h"
#include "JSPropertyNameEnumerator.h"
#include "JSSet.h"
#include "JSWeakMap.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyStruct.h"
#include "JSWrapperObject.h"
#include "KeyAtomStringCache.h"
#include "NumericStrings.h"
#include "RegExpObject.h"
#include "ScopedArguments.h"
#include "ShadowChicken.h"
#include "StructureChain.h"
#include "StructureRareDataInlines.h"
#include "Symbol.h"
#include "WasmGlobal.h"
#include "WasmTable.h"
#include "WasmTypeDefinition.h"
#include "WebAssemblyModuleRecord.h"

namespace JSC::B3 {

AbstractHeapRepository::AbstractHeapRepository()
    : root(nullptr, "jscRoot")

#define ABSTRACT_HEAP_INITIALIZATION(name) , name(&root, #name)
    FOR_EACH_ABSTRACT_HEAP(ABSTRACT_HEAP_INITIALIZATION)
#undef ABSTRACT_HEAP_INITIALIZATION

    , WebAssemblyMemory(TypedArrayProperties)

#define ABSTRACT_FIELD_INITIALIZATION(name, offset, mutability) , name(&root, #name, offset, mutability)
    FOR_EACH_ABSTRACT_FIELD(ABSTRACT_FIELD_INITIALIZATION)
#undef ABSTRACT_FIELD_INITIALIZATION

    , JSCell_freeListNext(JSCell_header)
    , ArrayStorage_publicLength(Butterfly_publicLength)
    , ArrayStorage_vectorLength(Butterfly_vectorLength)

#define INDEXED_ABSTRACT_HEAP_INITIALIZATION(name, offset, size) , name(&root, #name, offset, size)
    FOR_EACH_INDEXED_ABSTRACT_HEAP(INDEXED_ABSTRACT_HEAP_INITIALIZATION)
#undef INDEXED_ABSTRACT_HEAP_INITIALIZATION

#define NUMBERED_ABSTRACT_HEAP_INITIALIZATION(name) , name(&root, #name)
    FOR_EACH_NUMBERED_ABSTRACT_HEAP(NUMBERED_ABSTRACT_HEAP_INITIALIZATION)
#undef NUMBERED_ABSTRACT_HEAP_INITIALIZATION

    , JSString_value(JSRopeString_fiber0)
    , JSWrapperObject_internalValue(const_cast<AbstractHeap&>(JSInternalFieldObjectImpl_internalFields[static_cast<unsigned>(JSWrapperObject::Field::WrappedValue)]))

    , absolute(&root, "absolute")
{
    JSCell_header.changeParent(&JSCellHeaderAndNamedProperties);
    properties.atAnyNumber().changeParent(&JSCellHeaderAndNamedProperties);

    // Make sure that our explicit assumptions about the TypeInfoBlob match reality.
    RELEASE_ASSERT(!(JSCell_indexingTypeAndMisc.offset() & (sizeof(int32_t) - 1)));
    RELEASE_ASSERT(JSCell_indexingTypeAndMisc.offset() + 1 == JSCell_typeInfoType.offset());
    RELEASE_ASSERT(JSCell_indexingTypeAndMisc.offset() + 2 == JSCell_typeInfoFlags.offset());
    RELEASE_ASSERT(JSCell_indexingTypeAndMisc.offset() + 3 == JSCell_cellState.offset());

    JSCell_structureID.changeParent(&JSCell_header);
    JSCell_usefulBytes.changeParent(&JSCell_header);
    JSCell_indexingTypeAndMisc.changeParent(&JSCell_usefulBytes);
    JSCell_typeInfoType.changeParent(&JSCell_usefulBytes);
    JSCell_typeInfoFlags.changeParent(&JSCell_usefulBytes);
    JSCell_cellState.changeParent(&JSCell_usefulBytes);
    JSRopeString_flags.changeParent(&JSRopeString_fiber0);
    JSRopeString_length.changeParent(&JSRopeString_fiber1);

    RELEASE_ASSERT(!JSCell_freeListNext.offset());
}

AbstractHeapRepository::~AbstractHeapRepository() = default;

void AbstractHeapRepository::decorateMemory(const AbstractHeap* heap, Value* value)
{
    m_heapForMemory.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decorateCCallRead(const AbstractHeap* heap, Value* value)
{
    m_heapForCCallRead.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decorateCCallWrite(const AbstractHeap* heap, Value* value)
{
    m_heapForCCallWrite.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decoratePatchpointRead(const AbstractHeap* heap, Value* value)
{
    m_heapForPatchpointRead.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decoratePatchpointWrite(const AbstractHeap* heap, Value* value)
{
    m_heapForPatchpointWrite.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decorateFenceRead(const AbstractHeap* heap, Value* value)
{
    m_heapForFenceRead.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decorateFenceWrite(const AbstractHeap* heap, Value* value)
{
    m_heapForFenceWrite.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decorateFencedAccess(const AbstractHeap* heap, Value* value)
{
    m_heapForFencedAccess.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decorateWasmStructGet(const AbstractHeap* heap, Value* value)
{
    m_heapForWasmStructGet.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::decorateWasmStructSet(const AbstractHeap* heap, Value* value)
{
    m_heapForWasmStructSet.append(HeapForValue(heap, value));
}

void AbstractHeapRepository::computeRangesAndDecorateInstructions()
{
    root.compute();

    if (Options::verboseCompilation() || Options::verboseFTLCompilation()) [[unlikely]] {
        WTF::dataFile().atomically([&](auto&) {
            dataLogLn("Abstract Heap Repository:");
            root.deepDump(WTF::dataFile());
        });
    }

    auto rangeFor = [&] (const AbstractHeap* heap) -> HeapRange {
        if (heap)
            return heap->range();
        return HeapRange();
    };

    for (HeapForValue entry : m_heapForMemory) {
        auto* memoryValue = entry.value->as<MemoryValue>();
        memoryValue->setRange(rangeFor(entry.heap));
        if (memoryValue->isLoad() && entry.heap->mutability() == B3::Mutability::Immutable)
            memoryValue->setReadsMutability(B3::Mutability::Immutable);
    }
    for (HeapForValue entry : m_heapForCCallRead)
        entry.value->as<CCallValue>()->effects.reads = rangeFor(entry.heap);
    for (HeapForValue entry : m_heapForCCallWrite)
        entry.value->as<CCallValue>()->effects.writes = rangeFor(entry.heap);
    for (HeapForValue entry : m_heapForPatchpointRead)
        entry.value->as<PatchpointValue>()->effects.reads = rangeFor(entry.heap);
    for (HeapForValue entry : m_heapForPatchpointWrite)
        entry.value->as<PatchpointValue>()->effects.writes = rangeFor(entry.heap);
    for (HeapForValue entry : m_heapForFenceRead)
        entry.value->as<FenceValue>()->read = rangeFor(entry.heap);
    for (HeapForValue entry : m_heapForFenceWrite)
        entry.value->as<FenceValue>()->write = rangeFor(entry.heap);
    for (HeapForValue entry : m_heapForFencedAccess)
        entry.value->as<MemoryValue>()->setFenceRange(rangeFor(entry.heap));
    for (HeapForValue entry : m_heapForWasmStructGet) {
        auto* wasmStructGet = entry.value->as<WasmStructGetValue>();
        wasmStructGet->setRange(rangeFor(entry.heap));
    }
    for (HeapForValue entry : m_heapForWasmStructSet) {
        auto* wasmStructSet = entry.value->as<WasmStructSetValue>();
        wasmStructSet->setRange(rangeFor(entry.heap));
    }
}

} // namespace JSC::B3

#endif // ENABLE(B3_JIT)
