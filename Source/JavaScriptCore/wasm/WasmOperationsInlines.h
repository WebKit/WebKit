/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "WasmOperations.h"

#if ENABLE(WEBASSEMBLY)

#include "JITExceptions.h"
#include "JSWebAssemblyArrayInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyStruct.h"
#include "TypedArrayController.h"
#include "WaiterListManager.h"
#include "WasmIPIntGenerator.h"
#include "WasmModuleInformation.h"
#include "WasmTypeDefinition.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {
namespace Wasm {

inline EncodedJSValue refFunc(JSWebAssemblyInstance* instance, uint32_t index)
{
    JSValue value = instance->getFunctionWrapper(index);
    ASSERT(value.isCallable());
    return JSValue::encode(value);
}

template <typename T>
JSWebAssemblyArray* tryFillArray(JSWebAssemblyInstance* instance, WebAssemblyGCStructure* structure, uint32_t size, T value)
{
    VM& vm = instance->vm();

    auto* array = JSWebAssemblyArray::tryCreate(vm, structure, size);
    if (array) [[likely]]
        array->fill(vm, 0, static_cast<T>(value), size);
    return array;
}

inline JSValue arrayNew(JSWebAssemblyInstance* instance, WebAssemblyGCStructure* structure, uint32_t size, EncodedJSValue encValue)
{
    const Wasm::TypeDefinition& arraySignature = structure->typeDefinition();
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::FieldType fieldType = arraySignature.as<ArrayType>()->elementType();

    size_t elementSize = fieldType.type.elementSize();

    JSWebAssemblyArray* array = nullptr;
    switch (elementSize) {
    case sizeof(uint8_t): {
        array = tryFillArray<uint8_t>(instance, structure, size, static_cast<uint8_t>(encValue));
        break;
    }
    case sizeof(uint16_t): {
        array = tryFillArray<uint16_t>(instance, structure, size, static_cast<uint16_t>(encValue));
        break;
    }
    case sizeof(uint32_t): {
        array = tryFillArray<uint32_t>(instance, structure, size, static_cast<uint32_t>(encValue));
        break;
    }
    case sizeof(uint64_t): {
        array = tryFillArray<uint64_t>(instance, structure, size, encValue);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    if (!array) [[unlikely]]
        return jsNull();
    return array;
}

inline JSValue arrayNew(JSWebAssemblyInstance* instance, WebAssemblyGCStructure* structure, uint32_t size, v128_t value)
{
    VM& vm = instance->vm();

    const Wasm::TypeDefinition& arraySignature = structure->typeDefinition();
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::FieldType fieldType = arraySignature.as<ArrayType>()->elementType();
    ASSERT_UNUSED(fieldType, fieldType.type.unpacked().isV128());

    auto* array = JSWebAssemblyArray::tryCreate(vm, structure, size);
    if (!array) [[unlikely]]
        return jsNull();

    array->fill(vm, 0, value, size);
    return array;
}

template <typename T>
JSWebAssemblyArray* tryCopyElementsInReverse(JSWebAssemblyInstance* instance, WebAssemblyGCStructure* structure, uint32_t size, IPInt::IPIntStackEntry* arguments)
{
    VM& vm = instance->vm();

    auto* array = JSWebAssemblyArray::tryCreate(vm, structure, size);
    if (!array) [[unlikely]]
        return array;

    if (!size)
        return array;

    ASSERT(arguments);
    auto span = array->span<T>();
    for (int srcIndex = size - 1; srcIndex >= 0; srcIndex--) {
        unsigned dstIndex = size - srcIndex - 1;
        if constexpr (std::is_same_v<T, v128_t>)
            span[dstIndex] = arguments[srcIndex].v128;
        else
            span[dstIndex] = static_cast<T>(arguments[srcIndex].i64);
    }
    if (array->elementsAreRefTypes())
        vm.writeBarrier(array);
    return array;
}

// Expects arguments in reverse order
inline JSValue arrayNewFixed(JSWebAssemblyInstance* instance, WebAssemblyGCStructure* structure, uint32_t size, IPInt::IPIntStackEntry* arguments)
{
    // Get the array element type and determine the element size
    const Wasm::TypeDefinition& arraySignature = structure->typeDefinition();
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::FieldType fieldType = arraySignature.as<ArrayType>()->elementType();
    size_t elementSize = fieldType.type.elementSize();

    // Copy the elements into the result array in reverse order
    JSWebAssemblyArray* array = nullptr;
    switch (elementSize) {
    case sizeof(uint8_t): {
        array = tryCopyElementsInReverse<uint8_t>(instance, structure, size, arguments);
        break;
    }
    case sizeof(uint16_t): {
        array = tryCopyElementsInReverse<uint16_t>(instance, structure, size, arguments);
        break;
    }
    case sizeof(uint32_t): {
        array = tryCopyElementsInReverse<uint32_t>(instance, structure, size, arguments);
        break;
    }
    case sizeof(uint64_t): {
        array = tryCopyElementsInReverse<uint64_t>(instance, structure, size, arguments);
        break;
    }
    case sizeof(v128_t): {
        array = tryCopyElementsInReverse<v128_t>(instance, structure, size, arguments);
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    if (!array) [[unlikely]]
        return jsNull();
    return array;
}

template<typename T>
EncodedJSValue createArrayFromDataSegment(JSWebAssemblyInstance* instance, WebAssemblyGCStructure* structure, size_t arraySize, unsigned dataSegmentIndex, unsigned offset)
{
    JSGlobalObject* globalObject = instance->globalObject();
    VM& vm = globalObject->vm();
    auto* array = JSWebAssemblyArray::tryCreate(vm, structure, arraySize);
    if (!array) [[unlikely]]
        return JSValue::encode(jsNull());

    ASSERT(!array->elementsAreRefTypes());
    auto span = array->span<T>();
    // Copy the data from the segment into the temp `values` vector
    if (!instance->copyDataSegment(array, dataSegmentIndex, offset, span.size_bytes(), reinterpret_cast<uint8_t*>(span.data()))) {
        // If copyDataSegment() returns false, the segment access is out of bounds.
        // In that case, the caller is responsible for throwing an exception.
        return JSValue::encode(jsNull());
    }
    return JSValue::encode(array);
}

inline EncodedJSValue arrayNewData(JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t dataSegmentIndex, uint32_t arraySize, uint32_t offset)
{
    // Check that the type index is within bounds
    ASSERT(typeIndex < instance->module().moduleInformation().typeCount());
    WebAssemblyGCStructure* structure = instance->gcObjectStructure(typeIndex);
    const Wasm::TypeDefinition& arraySignature = structure->typeDefinition();
    ASSERT(arraySignature.is<ArrayType>());

    // Get the array element type
    Wasm::FieldType fieldType = arraySignature.as<ArrayType>()->elementType();
    // Determine the array length in bytes from the element type and desired array size
    size_t elementSize = fieldType.type.elementSize();

    // Check for overflow when determining array length in bytes
    if (productOverflows<uint32_t>(elementSize, arraySize)) [[unlikely]]
        return JSValue::encode(jsNull());

    uint32_t arrayLengthInBytes = arraySize * elementSize;

    // Check for offset + arrayLengthInBytes overflow
    if (sumOverflows<uint32_t>(offset, arrayLengthInBytes)) [[unlikely]]
        return JSValue::encode(jsNull());

    // Finally, allocate the array from the `values` vector
    if (fieldType.type.is<PackedType>()) {
        switch (fieldType.type.as<PackedType>()) {
        case PackedType::I8: {
            return createArrayFromDataSegment<uint8_t>(instance, structure, arraySize, dataSegmentIndex, offset);
        }
        case PackedType::I16: {
            return createArrayFromDataSegment<uint16_t>(instance, structure, arraySize, dataSegmentIndex, offset);
        }
        default:
            break;
        }
    } else {
        switch (fieldType.type.as<Type>().kind) {
        case Wasm::TypeKind::I32:
        case Wasm::TypeKind::F32: {
            return createArrayFromDataSegment<uint32_t>(instance, structure, arraySize, dataSegmentIndex, offset);
        }
        case Wasm::TypeKind::I64:
        case Wasm::TypeKind::F64: {
            return createArrayFromDataSegment<uint64_t>(instance, structure, arraySize, dataSegmentIndex, offset);
        }
        case Wasm::TypeKind::V128: {
            return createArrayFromDataSegment<v128_t>(instance, structure, arraySize, dataSegmentIndex, offset);
        }
        default:
            break;
        }
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline EncodedJSValue arrayNewElem(JSWebAssemblyInstance* instance, uint32_t typeIndex, uint32_t elemSegmentIndex, uint32_t arraySize, uint32_t offset)
{
    // Check that the type index is within bounds
    ASSERT(typeIndex < instance->module().moduleInformation().typeCount());

    WebAssemblyGCStructure* structure = instance->gcObjectStructure(typeIndex);
    ASSERT(structure->typeDefinition().is<ArrayType>());

    // Ensure that adding the offset to the desired array length doesn't overflow int32 or
    // overflow the length of the element segment
    auto element = instance->elementAt(elemSegmentIndex);
    size_t segmentLength = element ? element->length() : 0U;
    auto calculatedArrayEnd = CheckedUint32 { offset } + arraySize;
    if (calculatedArrayEnd.hasOverflowed() || calculatedArrayEnd > segmentLength) [[unlikely]]
        return JSValue::encode(jsNull());

    VM& vm = instance->vm();
    StorageType arrayType = structure->typeDefinition().as<ArrayType>()->elementType().type;
    ASSERT_UNUSED(arrayType, !arraySize || isSubtype(StorageType(element->elementType), arrayType));
    auto* array = JSWebAssemblyArray::tryCreate(vm, structure, arraySize);
    if (!array) [[unlikely]]
        return JSValue::encode(jsNull());
#if ASSERT_ENABLED
    array->setIsUnpopulated(true);
    WTF::storeLoadFence();
#endif
    instance->copyElementSegment(array, instance->module().moduleInformation().elements[elemSegmentIndex], offset, arraySize, array->span<uint64_t>().data());
#if ASSERT_ENABLED
    WTF::storeStoreFence();
    array->setIsUnpopulated(false);
#endif
    ASSERT(!arraySize || Wasm::isRefType(element->elementType));
    vm.writeBarrier(array);
    return JSValue::encode(array);
}

inline void arrayGet(JSWebAssemblyInstance* instance, uint32_t typeIndex, EncodedJSValue arrayValue, uint32_t index, IPInt::IPIntStackEntry* result)
{
    ASSERT_UNUSED(instance, typeIndex < instance->module().moduleInformation().typeCount());
    const Wasm::TypeDefinition& arraySignature = instance->module().moduleInformation().typeSignatures[typeIndex]->expand();
    ASSERT_UNUSED(arraySignature, arraySignature.is<ArrayType>());

    JSValue arrayRef = JSValue::decode(arrayValue);
    ASSERT(arrayRef.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayRef.getObject());

    if (arrayObject->elementType().type.unpacked().isV128())
        result->v128 = arrayObject->getVector(index);
    else
        result->i64 = arrayObject->get(index);
}

inline void arraySet(JSWebAssemblyInstance* instance, uint32_t typeIndex, EncodedJSValue arrayValue, uint32_t index, IPInt::IPIntStackEntry* value)
{
    ASSERT_UNUSED(instance, typeIndex < instance->module().moduleInformation().typeCount());
    const Wasm::TypeDefinition& arraySignature = instance->module().moduleInformation().typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());

    JSValue arrayRef = JSValue::decode(arrayValue);
    ASSERT(arrayRef.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayRef.getObject());

    Wasm::FieldType elementType = arraySignature.as<ArrayType>()->elementType();
    if (elementType.type.unpacked().isV128())
        arrayObject->set(instance->vm(), index, value->v128);
    else
        arrayObject->set(instance->vm(), index, value->i64);
}

inline bool doArrayFill(VM& vm, EncodedJSValue arrayValue, uint32_t offset, Variant<uint64_t, v128_t> value, uint32_t size)
{
    JSValue arrayRef = JSValue::decode(arrayValue);
    ASSERT(arrayRef.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayRef.getObject());

    CheckedUint32 lastElementIndexChecked = offset;
    lastElementIndexChecked += size;

    if (lastElementIndexChecked.hasOverflowed())
        return false;

    if (lastElementIndexChecked > arrayObject->size())
        return false;

    if (std::holds_alternative<uint64_t>(value))
        arrayObject->fill(vm, offset, std::get<uint64_t>(value), size);
    else {
        ASSERT(std::holds_alternative<v128_t>(value));
        arrayObject->fill(vm, offset, std::get<v128_t>(value), size);
    }
    return true;
}

inline bool arrayFill(VM& vm, EncodedJSValue arrayValue, uint32_t offset, uint64_t value, uint32_t size)
{
    return doArrayFill(vm, arrayValue, offset, value, size);
}

inline bool arrayFill(VM& vm, EncodedJSValue arrayValue, uint32_t offset, v128_t value, uint32_t size)
{
    return doArrayFill(vm, arrayValue, offset, value, size);
}

// FIXME:
// To be consistent it would make sense to include dst and src type indices, but they
// are not necessary for operation and hits a limitation of BBQ JIT calls.
inline bool arrayCopy(JSWebAssemblyInstance* instance, EncodedJSValue dst, uint32_t dstOffset, EncodedJSValue src, uint32_t srcOffset, uint32_t size)
{
    JSValue dstRef = JSValue::decode(dst);
    JSValue srcRef = JSValue::decode(src);
    ASSERT(dstRef.isObject());
    ASSERT(srcRef.isObject());
    JSWebAssemblyArray* dstObject = jsCast<JSWebAssemblyArray*>(dstRef.getObject());
    JSWebAssemblyArray* srcObject = jsCast<JSWebAssemblyArray*>(srcRef.getObject());

    CheckedUint32 lastDstElementIndexChecked = dstOffset;
    lastDstElementIndexChecked += size;

    if (lastDstElementIndexChecked.hasOverflowed())
        return false;

    if (lastDstElementIndexChecked > dstObject->size())
        return false;

    CheckedUint32 lastSrcElementIndexChecked = srcOffset;
    lastSrcElementIndexChecked += size;

    if (lastSrcElementIndexChecked.hasOverflowed())
        return false;

    if (lastSrcElementIndexChecked > srcObject->size())
        return false;

    srcObject->copy(instance->vm(), *dstObject, dstOffset, srcOffset, size);
    return true;
}

inline bool arrayInitElem(JSWebAssemblyInstance* instance, EncodedJSValue dst, uint32_t dstOffset, uint32_t srcElementIndex, uint32_t srcOffset, uint32_t size)
{
    JSValue dstRef = JSValue::decode(dst);
    ASSERT(dstRef.isObject());
    JSWebAssemblyArray* dstObject = jsCast<JSWebAssemblyArray*>(dstRef.getObject());

    CheckedUint32 lastDstElementIndexChecked = dstOffset;
    lastDstElementIndexChecked += size;

    if (lastDstElementIndexChecked.hasOverflowed())
        return false;

    if (lastDstElementIndexChecked > dstObject->size())
        return false;

    CheckedUint32 lastSrcElementIndexChecked = srcOffset;
    lastSrcElementIndexChecked += size;

    if (lastSrcElementIndexChecked.hasOverflowed())
        return false;

    const uint32_t lengthOfElementSegment = instance->elementAt(srcElementIndex) ? instance->elementAt(srcElementIndex)->length() : 0U;
    if (lastSrcElementIndexChecked > lengthOfElementSegment)
        return false;

    auto* elementSegment = instance->elementAt(srcElementIndex);
    if (elementSegment) {
        instance->copyElementSegment(dstObject, *elementSegment, srcOffset, size, dstObject->refTypeSpan().data() + dstOffset);
        instance->vm().writeBarrier(dstObject);
    }
    else
        ASSERT(!lastSrcElementIndexChecked);

    return true;
}

inline bool arrayInitData(JSWebAssemblyInstance* instance, EncodedJSValue dst, uint32_t dstOffset, uint32_t srcDataIndex, uint32_t srcOffset, uint32_t size)
{
    JSValue dstRef = JSValue::decode(dst);
    ASSERT(dstRef.isObject());
    JSWebAssemblyArray* dstObject = jsCast<JSWebAssemblyArray*>(dstRef.getObject());

    CheckedUint32 lastDstElementIndexChecked = dstOffset;
    lastDstElementIndexChecked += size;

    if (lastDstElementIndexChecked.hasOverflowed())
        return false;

    if (lastDstElementIndexChecked > dstObject->size())
        return false;

    size_t elementSize = dstObject->elementType().type.elementSize();

    CheckedUint32 lastSrcByteChecked = size;
    lastSrcByteChecked *= elementSize;
    lastSrcByteChecked += srcOffset;

    if (lastSrcByteChecked.hasOverflowed())
        return false;

    // Otherwise we need a writeBarrier.
    ASSERT(!dstObject->elementsAreRefTypes());
    return dstObject->visitSpan([&](auto span) ALWAYS_INLINE_LAMBDA -> bool {
        return instance->copyDataSegment(dstObject, srcDataIndex, srcOffset, size * elementSize, reinterpret_cast<uint8_t*>(span.subspan(dstOffset).data()));
    });
}

// structNew() expects the `arguments` array (when used) to be in reverse order
inline JSValue structNew(JSWebAssemblyInstance* instance, WebAssemblyGCStructure* structure, bool useDefault, IPInt::IPIntStackEntry* arguments)
{
    JSGlobalObject* globalObject = instance->globalObject();
    VM& vm = globalObject->vm();

    ASSERT(structure->typeDefinition().is<StructType>());
    const StructType& structType = *structure->typeDefinition().as<StructType>();
    JSWebAssemblyStruct* structValue = JSWebAssemblyStruct::create(vm, structure);
    if (static_cast<Wasm::UseDefaultValue>(useDefault) == Wasm::UseDefaultValue::Yes) {
        for (unsigned i = 0; i < structType.fieldCount(); ++i) {
            if (structType.field(i).type.unpacked().isV128()) {
                structValue->set(i, vectorAllZeros());
                continue;
            }
            EncodedJSValue value = 0;
            if (Wasm::isRefType(structType.field(i).type))
                value = JSValue::encode(jsNull());
            structValue->set(i, value);
        }
    } else {
        ASSERT(arguments);
        for (unsigned dstIndex = 0; dstIndex < structType.fieldCount(); ++dstIndex) {
            // Arguments are in reverse order!
            unsigned srcIndex = structType.fieldCount() - dstIndex - 1;
            if (structType.field(dstIndex).type.unpacked().isV128())
                structValue->set(dstIndex, arguments[srcIndex].v128);
            else
                structValue->set(dstIndex, arguments[srcIndex].i64);
        }
    }
    return structValue;
}

inline void structGet(EncodedJSValue encodedStructReference, uint32_t fieldIndex, IPInt::IPIntStackEntry* result)
{
    auto structReference = JSValue::decode(encodedStructReference);
    ASSERT(structReference.isObject());
    JSObject* structureAsObject = jsCast<JSObject*>(structReference);
    ASSERT(structureAsObject->inherits<JSWebAssemblyStruct>());
    JSWebAssemblyStruct* structPointer = jsCast<JSWebAssemblyStruct*>(structureAsObject);

    Wasm::FieldType field = structPointer->fieldType(fieldIndex);
    if (field.type.unpacked().isV128())
        result->v128 = structPointer->getVector(fieldIndex);
    else
        result->i64 = structPointer->get(fieldIndex);
}

inline void structSet(EncodedJSValue encodedStructReference, uint32_t fieldIndex, IPInt::IPIntStackEntry* argument)
{
    auto structReference = JSValue::decode(encodedStructReference);
    ASSERT(structReference.isObject());
    JSObject* structureAsObject = jsCast<JSObject*>(structReference);
    ASSERT(structureAsObject->inherits<JSWebAssemblyStruct>());
    JSWebAssemblyStruct* structPointer = jsCast<JSWebAssemblyStruct*>(structureAsObject);

    Wasm::FieldType field = structPointer->fieldType(fieldIndex);
    if (field.type.unpacked().isV128())
        structPointer->set(fieldIndex, argument->v128);
    else
        structPointer->set(fieldIndex, argument->i64);
}

inline bool refCast(EncodedJSValue encodedReference, bool allowNull, TypeIndex typeIndex, const RTT* rtt)
{
    return TypeInformation::isReferenceValueAssignable(JSValue::decode(encodedReference), allowNull, typeIndex, rtt);
}

inline EncodedJSValue externInternalize(EncodedJSValue reference)
{
    return JSValue::encode(Wasm::internalizeExternref(JSValue::decode(reference)));
}

inline EncodedJSValue tableGet(JSWebAssemblyInstance* instance, unsigned tableIndex, int32_t signedIndex)
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());
    if (signedIndex < 0)
        return 0;

    uint32_t index = signedIndex;
    if (index >= instance->table(tableIndex)->length())
        return 0;

    return JSValue::encode(instance->table(tableIndex)->get(index));
}

inline bool tableSet(JSWebAssemblyInstance* instance, unsigned tableIndex, uint32_t index, EncodedJSValue encValue)
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());

    if (index >= instance->table(tableIndex)->length())
        return false;

    JSValue value = JSValue::decode(encValue);
    if (value.isNull())
        instance->table(tableIndex)->clear(index);
    else
        instance->table(tableIndex)->set(index, value);

    return true;
}

inline bool tableInit(JSWebAssemblyInstance* instance, unsigned elementIndex, unsigned tableIndex, uint32_t dstOffset, uint32_t srcOffset, uint32_t length)
{
    ASSERT(elementIndex < instance->module().moduleInformation().elementCount());
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());

    if (WTF::sumOverflows<uint32_t>(srcOffset, length))
        return false;

    if (WTF::sumOverflows<uint32_t>(dstOffset, length))
        return false;

    if (dstOffset + length > instance->table(tableIndex)->length())
        return false;

    const uint32_t lengthOfElementSegment = instance->elementAt(elementIndex) ? instance->elementAt(elementIndex)->length() : 0U;
    if (srcOffset + length > lengthOfElementSegment)
        return false;

    if (!lengthOfElementSegment)
        return true;

    instance->tableInit(dstOffset, srcOffset, length, elementIndex, tableIndex);
    return true;
}

inline bool tableFill(JSWebAssemblyInstance* instance, unsigned tableIndex, uint32_t offset, EncodedJSValue fill, uint32_t count)
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());

    if (WTF::sumOverflows<uint32_t>(offset, count))
        return false;

    if (offset + count > instance->table(tableIndex)->length())
        return false;

    for (uint32_t index = 0; index < count; ++index)
        tableSet(instance, tableIndex, offset + index, fill);

    return true;
}

inline size_t tableGrow(JSWebAssemblyInstance* instance, unsigned tableIndex, EncodedJSValue fill, uint32_t delta)
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());
    auto oldSize = instance->table(tableIndex)->length();
    auto newSize = instance->table(tableIndex)->grow(delta, jsNull());
    if (!newSize)
        return -1;

    for (unsigned i = oldSize; i < instance->table(tableIndex)->length(); ++i)
        tableSet(instance, tableIndex, i, fill);

    return oldSize;
}

inline bool tableCopy(JSWebAssemblyInstance* instance, unsigned dstTableIndex, unsigned srcTableIndex, int32_t dstOffset, int32_t srcOffset, int32_t length)
{
    ASSERT(dstTableIndex < instance->module().moduleInformation().tableCount());
    ASSERT(srcTableIndex < instance->module().moduleInformation().tableCount());
    const Table* dstTable = instance->table(dstTableIndex);
    const Table* srcTable = instance->table(srcTableIndex);
    ASSERT(dstTable->type() == srcTable->type());

    if ((srcOffset < 0) || (dstOffset < 0) || (length < 0))
        return false;

    CheckedUint32 lastDstElementIndexChecked = static_cast<uint32_t>(dstOffset);
    lastDstElementIndexChecked += static_cast<uint32_t>(length);

    if (lastDstElementIndexChecked.hasOverflowed())
        return false;

    if (lastDstElementIndexChecked > dstTable->length())
        return false;

    CheckedUint32 lastSrcElementIndexChecked = static_cast<uint32_t>(srcOffset);
    lastSrcElementIndexChecked += static_cast<uint32_t>(length);

    if (lastSrcElementIndexChecked.hasOverflowed())
        return false;

    if (lastSrcElementIndexChecked > srcTable->length())
        return false;

    instance->tableCopy(dstOffset, srcOffset, length, dstTableIndex, srcTableIndex);
    return true;
}

inline int32_t tableSize(JSWebAssemblyInstance* instance, unsigned tableIndex)
{
    return instance->table(tableIndex)->length();
}

inline int32_t growMemory(JSWebAssemblyInstance* instance, int32_t delta)
{
    if (delta < 0)
        return -1;

    auto grown = instance->memory()->memory().grow(instance->vm(), PageCount(delta));
    if (!grown) {
        switch (grown.error()) {
        case GrowFailReason::InvalidDelta:
        case GrowFailReason::InvalidGrowSize:
        case GrowFailReason::WouldExceedMaximum:
        case GrowFailReason::OutOfMemory:
        case GrowFailReason::GrowSharedUnavailable:
            return -1;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    return grown.value().pageCount();
}

inline bool memoryInit(JSWebAssemblyInstance* instance, unsigned dataSegmentIndex, uint64_t dstAddress, uint32_t srcAddress, uint32_t length)
{
    ASSERT(dataSegmentIndex < instance->module().moduleInformation().dataSegmentsCount());
    return instance->memoryInit(dstAddress, srcAddress, length, dataSegmentIndex);
}

inline bool memoryFill(JSWebAssemblyInstance* instance, uint32_t dstAddress, uint32_t targetValue, uint32_t count)
{
    auto* base = std::bit_cast<uint8_t*>(instance->cachedMemory());
    uint64_t size = instance->cachedMemorySize();

    uint64_t lastDstAddress = static_cast<uint64_t>(dstAddress) + count;
    if (lastDstAddress > size)
        return false;

    memset(base + dstAddress, targetValue, count);
    return true;
}

inline bool memoryCopy(JSWebAssemblyInstance* instance, uint32_t dstAddress, uint32_t srcAddress, uint32_t count)
{
    auto* base = std::bit_cast<uint8_t*>(instance->cachedMemory());
    uint64_t size = instance->cachedMemorySize();

    uint64_t lastDstAddress = static_cast<uint64_t>(dstAddress) + count;
    uint64_t lastSrcAddress = static_cast<uint64_t>(srcAddress) + count;

    if (lastDstAddress > size || lastSrcAddress > size)
        return false;

    if (!count)
        return true;

    // Source and destination areas might overlap, so using memmove.
    memmove(base + dstAddress, base + srcAddress, count);
    return true;
}

inline void dataDrop(JSWebAssemblyInstance* instance, unsigned dataSegmentIndex)
{
    ASSERT(dataSegmentIndex < instance->module().moduleInformation().dataSegmentsCount());
    instance->dataDrop(dataSegmentIndex);
}

inline void elemDrop(JSWebAssemblyInstance* instance, unsigned elementIndex)
{
    ASSERT(elementIndex < instance->module().moduleInformation().elementCount());
    instance->elemDrop(elementIndex);
}

template<typename ValueType>
static inline int32_t waitImpl(VM& vm, ValueType* pointer, ValueType expectedValue, int64_t timeoutInNanoseconds)
{
    Seconds timeout = Seconds::infinity();
    if (timeoutInNanoseconds >= 0)
        timeout = Seconds::fromNanoseconds(timeoutInNanoseconds);
    auto result = WaiterListManager::singleton().waitSync(vm, pointer, expectedValue, timeout);
    switch (result) {
    case WaiterListManager::WaitSyncResult::OK:
    case WaiterListManager::WaitSyncResult::NotEqual:
    case WaiterListManager::WaitSyncResult::TimedOut:
        return static_cast<int32_t>(result);
    case WaiterListManager::WaitSyncResult::Terminated:
        vm.throwTerminationException();
        return -1;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return -1;
}

inline int32_t memoryAtomicWait32(JSWebAssemblyInstance* instance, uint64_t offsetInMemory, int32_t value, int64_t timeoutInNanoseconds)
{
    VM& vm = instance->vm();
    if (offsetInMemory & (0x4 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->memory().size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return -1;
    if (!vm.m_typedArrayController->isAtomicsWaitAllowedOnCurrentThread())
        return -1;
    int32_t* pointer = std::bit_cast<int32_t*>(std::bit_cast<uint8_t*>(instance->memory()->basePointer()) + offsetInMemory);
    return waitImpl<int32_t>(vm, pointer, value, timeoutInNanoseconds);
}

inline int32_t memoryAtomicWait32(JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int32_t value, int64_t timeoutInNanoseconds)
{
    return memoryAtomicWait32(instance, static_cast<uint64_t>(base) + offset, value, timeoutInNanoseconds);
}

inline int32_t memoryAtomicWait64(JSWebAssemblyInstance* instance, uint64_t offsetInMemory, int64_t value, int64_t timeoutInNanoseconds)
{
    VM& vm = instance->vm();
    if (offsetInMemory & (0x8 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->memory().size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return -1;
    if (!vm.m_typedArrayController->isAtomicsWaitAllowedOnCurrentThread())
        return -1;
    int64_t* pointer = std::bit_cast<int64_t*>(std::bit_cast<uint8_t*>(instance->memory()->basePointer()) + offsetInMemory);
    return waitImpl<int64_t>(vm, pointer, value, timeoutInNanoseconds);
}

inline int32_t memoryAtomicWait64(JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int64_t value, int64_t timeoutInNanoseconds)
{
    return memoryAtomicWait64(instance, static_cast<uint64_t>(base) + offset, value, timeoutInNanoseconds);
}

inline int32_t memoryAtomicNotify(JSWebAssemblyInstance* instance, unsigned base, unsigned offset, int32_t countValue)
{
    uint64_t offsetInMemory = static_cast<uint64_t>(base) + offset;
    if (offsetInMemory & (0x4 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->memory().size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return 0;
    uint8_t* pointer = std::bit_cast<uint8_t*>(instance->memory()->basePointer()) + offsetInMemory;
    unsigned count = UINT_MAX;
    if (countValue >= 0)
        count = static_cast<unsigned>(countValue);

    return static_cast<int32_t>(WaiterListManager::singleton().notifyWaiter(pointer, count));
}

inline void* throwWasmToJSException(CallFrame* callFrame, Wasm::ExceptionType type, JSWebAssemblyInstance* instance)
{
    JSGlobalObject* globalObject = instance->globalObject();

    // Do not retrieve VM& from CallFrame since CallFrame's callee is not a JSCell.
    VM& vm = globalObject->vm();

    do {
        auto throwScope = DECLARE_THROW_SCOPE(vm);

        JSObject* error;
        if (type == ExceptionType::Termination) {
            // Nothing to do because the exception should have already been thrown.
            RELEASE_ASSERT(vm.hasPendingTerminationException());
            break;
        }
        if (type == ExceptionType::StackOverflow)
            error = createStackOverflowError(globalObject);
        else if (isTypeErrorExceptionType(type))
            error = createTypeError(globalObject, Wasm::errorMessageForExceptionType(type));
        else
            error = createJSWebAssemblyRuntimeError(globalObject, vm, type);
        throwException(globalObject, throwScope, error);
    } while (false);

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
