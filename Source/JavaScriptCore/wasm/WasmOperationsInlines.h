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

#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyStruct.h"
#include "TypedArrayController.h"
#include "WaiterListManager.h"

namespace JSC {
namespace Wasm {

inline EncodedJSValue refFunc(Instance* instance, uint32_t index)
{
    JSValue value = instance->getFunctionWrapper(index);
    ASSERT(value.isCallable());
    return JSValue::encode(value);
}

inline EncodedJSValue arrayNew(Instance* instance, uint32_t typeIndex, uint32_t size, EncodedJSValue encValue)
{
    JSGlobalObject* globalObject = instance->globalObject();
    VM& vm = instance->vm();

    ASSERT(typeIndex < instance->module().moduleInformation().typeCount());

    const Wasm::TypeDefinition& arraySignature = instance->module().moduleInformation().typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());
    Wasm::FieldType fieldType = arraySignature.as<ArrayType>()->elementType();

    size_t elementSize = fieldType.type.elementSize();

    JSWebAssemblyArray* array = nullptr;

    switch (elementSize) {
    case sizeof(uint8_t): {
        // `encValue` must be an unboxed int32 (since the typechecker guarantees that its type is i32); so it's safe to truncate it in the cases below.
        ASSERT(encValue <= UINT32_MAX);
        FixedVector<uint8_t> values(size);
        for (unsigned i = 0; i < size; i++)
            values[i] = static_cast<uint8_t>(encValue);
        array = JSWebAssemblyArray::create(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(values));
        break;
    }
    case sizeof(uint16_t): {
        ASSERT(encValue <= UINT32_MAX);
        FixedVector<uint16_t> values(size);
        for (unsigned i = 0; i < size; i++)
            values[i] = static_cast<uint16_t>(encValue);
        array = JSWebAssemblyArray::create(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(values));
        break;
    }
    case sizeof(uint32_t): {
        ASSERT(encValue <= UINT32_MAX);
        FixedVector<uint32_t> values(size);
        for (unsigned i = 0; i < size; i++)
            values[i] = static_cast<uint32_t>(encValue);
        array = JSWebAssemblyArray::create(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(values));
        break;
    }
    case sizeof(uint64_t): {
        FixedVector<uint64_t> values(size);
        for (unsigned i = 0; i < size; i++)
            values[i] = static_cast<uint64_t>(encValue);
        array = JSWebAssemblyArray::create(vm, globalObject->webAssemblyArrayStructure(), fieldType, size, WTFMove(values));
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return JSValue::encode(JSValue(array));
}

inline EncodedJSValue arrayGet(Instance* instance, uint32_t typeIndex, EncodedJSValue arrayValue, uint32_t index)
{
#if ASSERT_ENABLED
    ASSERT(typeIndex < instance->module().moduleInformation().typeCount());
    const Wasm::TypeDefinition& arraySignature = instance->module().moduleInformation().typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());
#else
    UNUSED_PARAM(instance);
    UNUSED_PARAM(typeIndex);
#endif

    JSValue arrayRef = JSValue::decode(arrayValue);
    ASSERT(arrayRef.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayRef.getObject());

    return arrayObject->get(index);
}

inline void arraySet(Instance* instance, uint32_t typeIndex, EncodedJSValue arrayValue, uint32_t index, EncodedJSValue value)
{
    ASSERT(typeIndex < instance->module().moduleInformation().typeCount());
    VM& vm = instance->vm();

#if ASSERT_ENABLED
    const Wasm::TypeDefinition& arraySignature = instance->module().moduleInformation().typeSignatures[typeIndex]->expand();
    ASSERT(arraySignature.is<ArrayType>());
#else
    UNUSED_PARAM(typeIndex);
#endif

    JSValue arrayRef = JSValue::decode(arrayValue);
    ASSERT(arrayRef.isObject());
    JSWebAssemblyArray* arrayObject = jsCast<JSWebAssemblyArray*>(arrayRef.getObject());

    arrayObject->set(vm, index, value);
}

inline EncodedJSValue structNew(Instance* instance, uint32_t typeIndex, bool useDefault, uint64_t* arguments)
{
    JSWebAssemblyInstance* jsInstance = instance->owner();
    JSGlobalObject* globalObject = instance->globalObject();
    const TypeDefinition& structTypeDefinition = instance->module().moduleInformation().typeSignatures[typeIndex]->expand();
    const StructType& structType = *structTypeDefinition.as<StructType>();

    JSWebAssemblyStruct* structValue = JSWebAssemblyStruct::tryCreate(globalObject, globalObject->webAssemblyStructStructure(), jsInstance, typeIndex);
    RELEASE_ASSERT(structValue);
    if (static_cast<Wasm::UseDefaultValue>(useDefault) == Wasm::UseDefaultValue::Yes) {
        for (unsigned i = 0; i < structType.fieldCount(); ++i) {
            JSValue value = JSValue(0);
            if (Wasm::isRefType(structType.field(i).type))
                value = jsNull();
            else if (structType.field(i).type.as<Type>().kind == Wasm::TypeKind::I64) {
                // This will convert to the appropriate I64 via ToBigInt() in set().
                value = jsBoolean(false);
            }
            structValue->set(globalObject, i, value);
        }
    } else {
        ASSERT(arguments);
        for (unsigned i = 0; i < structType.fieldCount(); ++i) {
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=246981
            ASSERT(structType.field(i).type.is<Type>());
            structValue->set(globalObject, i, toJSValue(globalObject, structType.field(i).type.as<Type>(), arguments[i]));
        }
    }
    return JSValue::encode(structValue);
}

inline EncodedJSValue structGet(EncodedJSValue encodedStructReference, uint32_t fieldIndex)
{
    auto structReference = JSValue::decode(encodedStructReference);
    ASSERT(structReference.isObject());
    JSObject* structureAsObject = jsCast<JSObject*>(structReference);
    ASSERT(structureAsObject->inherits<JSWebAssemblyStruct>());
    JSWebAssemblyStruct* structPointer = jsCast<JSWebAssemblyStruct*>(structureAsObject);
    return structPointer->get(fieldIndex);
}

inline void structSet(Instance* instance, EncodedJSValue encodedStructReference, uint32_t fieldIndex, EncodedJSValue argument)
{
    JSGlobalObject* globalObject = instance->globalObject();
    auto structReference = JSValue::decode(encodedStructReference);
    ASSERT(structReference.isObject());
    JSObject* structureAsObject = jsCast<JSObject*>(structReference);
    ASSERT(structureAsObject->inherits<JSWebAssemblyStruct>());
    JSWebAssemblyStruct* structPointer = jsCast<JSWebAssemblyStruct*>(structureAsObject);

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=246981
    ASSERT(structPointer->structType()->field(fieldIndex).type.is<Type>());

    const auto fieldType = structPointer->structType()->field(fieldIndex).type.as<Type>();
    return structPointer->set(globalObject, fieldIndex, toJSValue(globalObject, fieldType, argument));
}

inline EncodedJSValue tableGet(Instance* instance, unsigned tableIndex, int32_t signedIndex)
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());
    if (signedIndex < 0)
        return 0;

    uint32_t index = signedIndex;
    if (index >= instance->table(tableIndex)->length())
        return 0;

    return JSValue::encode(instance->table(tableIndex)->get(index));
}

inline bool tableSet(Instance* instance, unsigned tableIndex, uint32_t index, EncodedJSValue encValue)
{
    ASSERT(tableIndex < instance->module().moduleInformation().tableCount());

    if (index >= instance->table(tableIndex)->length())
        return false;

    JSValue value = JSValue::decode(encValue);
    if (instance->table(tableIndex)->type() == Wasm::TableElementType::Externref)
        instance->table(tableIndex)->set(index, value);
    else if (instance->table(tableIndex)->type() == Wasm::TableElementType::Funcref) {
        WebAssemblyFunction* wasmFunction;
        WebAssemblyWrapperFunction* wasmWrapperFunction;

        if (isWebAssemblyHostFunction(value, wasmFunction, wasmWrapperFunction)) {
            ASSERT(!!wasmFunction || !!wasmWrapperFunction);
            if (wasmFunction)
                instance->table(tableIndex)->asFuncrefTable()->setFunction(index, jsCast<JSObject*>(value), wasmFunction->importableFunction(), &wasmFunction->instance()->instance());
            else
                instance->table(tableIndex)->asFuncrefTable()->setFunction(index, jsCast<JSObject*>(value), wasmWrapperFunction->importableFunction(), &wasmWrapperFunction->instance()->instance());
        } else if (value.isNull())
            instance->table(tableIndex)->clear(index);
        else
            ASSERT_NOT_REACHED();
    } else
        ASSERT_NOT_REACHED();

    return true;
}

inline bool tableInit(Instance* instance, unsigned elementIndex, unsigned tableIndex, uint32_t dstOffset, uint32_t srcOffset, uint32_t length)
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

inline bool tableFill(Instance* instance, unsigned tableIndex, uint32_t offset, EncodedJSValue fill, uint32_t count)
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

inline size_t tableGrow(Instance* instance, unsigned tableIndex, EncodedJSValue fill, uint32_t delta)
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

inline bool tableCopy(Instance* instance, unsigned dstTableIndex, unsigned srcTableIndex, int32_t dstOffset, int32_t srcOffset, int32_t length)
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

inline int32_t tableSize(Instance* instance, unsigned tableIndex)
{
    return instance->table(tableIndex)->length();
}

inline int32_t growMemory(Instance* instance, int32_t delta)
{
    if (delta < 0)
        return -1;

    auto grown = instance->memory()->grow(instance->vm(), PageCount(delta));
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

inline bool memoryInit(Instance* instance, unsigned dataSegmentIndex, uint32_t dstAddress, uint32_t srcAddress, uint32_t length)
{
    ASSERT(dataSegmentIndex < instance->module().moduleInformation().dataSegmentsCount());
    return instance->memoryInit(dstAddress, srcAddress, length, dataSegmentIndex);
}

inline bool memoryFill(Instance* instance, uint32_t dstAddress, uint32_t targetValue, uint32_t count)
{
    return instance->memory()->fill(dstAddress, static_cast<uint8_t>(targetValue), count);
}

inline bool memoryCopy(Instance* instance, uint32_t dstAddress, uint32_t srcAddress, uint32_t count)
{
    return instance->memory()->copy(dstAddress, srcAddress, count);
}

inline void dataDrop(Instance* instance, unsigned dataSegmentIndex)
{
    ASSERT(dataSegmentIndex < instance->module().moduleInformation().dataSegmentsCount());
    instance->dataDrop(dataSegmentIndex);
}

inline void elemDrop(Instance* instance, unsigned elementIndex)
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
    return static_cast<int32_t>(WaiterListManager::singleton().waitSync(vm, pointer, expectedValue, timeout));
}

inline int32_t memoryAtomicWait32(Instance* instance, unsigned base, unsigned offset, int32_t value, int64_t timeoutInNanoseconds)
{
    VM& vm = instance->vm();
    uint64_t offsetInMemory = static_cast<uint64_t>(base) + offset;
    if (offsetInMemory & (0x4 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return -1;
    if (!vm.m_typedArrayController->isAtomicsWaitAllowedOnCurrentThread())
        return -1;
    int32_t* pointer = bitwise_cast<int32_t*>(bitwise_cast<uint8_t*>(instance->memory()->basePointer()) + offsetInMemory);
    return waitImpl<int32_t>(vm, pointer, value, timeoutInNanoseconds);
}

inline int32_t memoryAtomicWait64(Instance* instance, unsigned base, unsigned offset, int64_t value, int64_t timeoutInNanoseconds)
{
    VM& vm = instance->vm();
    uint64_t offsetInMemory = static_cast<uint64_t>(base) + offset;
    if (offsetInMemory & (0x8 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return -1;
    if (!vm.m_typedArrayController->isAtomicsWaitAllowedOnCurrentThread())
        return -1;
    int64_t* pointer = bitwise_cast<int64_t*>(bitwise_cast<uint8_t*>(instance->memory()->basePointer()) + offsetInMemory);
    return waitImpl<int64_t>(vm, pointer, value, timeoutInNanoseconds);
}

inline int32_t memoryAtomicNotify(Instance* instance, unsigned base, unsigned offset, int32_t countValue)
{
    uint64_t offsetInMemory = static_cast<uint64_t>(base) + offset;
    if (offsetInMemory & (0x4 - 1))
        return -1;
    if (!instance->memory())
        return -1;
    if (offsetInMemory >= instance->memory()->size())
        return -1;
    if (instance->memory()->sharingMode() != MemorySharingMode::Shared)
        return 0;
    uint8_t* pointer = bitwise_cast<uint8_t*>(instance->memory()->basePointer()) + offsetInMemory;
    unsigned count = UINT_MAX;
    if (countValue >= 0)
        count = static_cast<unsigned>(countValue);

    return static_cast<int32_t>(WaiterListManager::singleton().notifyWaiter(pointer, count));
}

inline void* throwWasmToJSException(CallFrame* callFrame, Wasm::ExceptionType type, Instance* instance)
{
    JSGlobalObject* globalObject = instance->globalObject();

    // Do not retrieve VM& from CallFrame since CallFrame's callee is not a JSCell.
    VM& vm = globalObject->vm();

    {
        auto throwScope = DECLARE_THROW_SCOPE(vm);

        JSObject* error;
        if (type == ExceptionType::StackOverflow)
            error = createStackOverflowError(globalObject);
        else if (isTypeErrorExceptionType(type))
            error = createTypeError(globalObject, Wasm::errorMessageForExceptionType(type));
        else
            error = createJSWebAssemblyRuntimeError(globalObject, vm, type);
        throwException(globalObject, throwScope, error);
    }

    genericUnwind(vm, callFrame);
    ASSERT(!!vm.callFrameForCatch);
    ASSERT(!!vm.targetMachinePCForThrow);
    return vm.targetMachinePCForThrow;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
