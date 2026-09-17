/*
 * Copyright (C) 2009-2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/BigIntObject.h>
#include <JavaScriptCore/BooleanObject.h>
#include <JavaScriptCore/CloneBase.h>
#include <JavaScriptCore/DateInstance.h>
#include <JavaScriptCore/Identifier.h>
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSMapInlines.h>
#include <JavaScriptCore/JSMapIterator.h>
#include <JavaScriptCore/JSObjectInlines.h>
#include <JavaScriptCore/JSSetInlines.h>
#include <JavaScriptCore/JSSetIterator.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/NumberObject.h>
#include <JavaScriptCore/PropertyNameArray.h>
#include <JavaScriptCore/RegExpObject.h>
#include <JavaScriptCore/StringObject.h>
#include <JavaScriptCore/TypedArrayController.h>
#include <JavaScriptCore/YarrFlags.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

#if ENABLE(WEBASSEMBLY)
#include <JavaScriptCore/JSWebAssemblyMemory.h>
#include <JavaScriptCore/JSWebAssemblyModule.h>
#include <JavaScriptCore/WasmModule.h>
#endif

namespace JSC {

template<typename Derived>
concept StructuredCloneSerializerHandler = requires(Derived& d, JSObject* obj, SerializationReturnCode& code, ArrayBuffer& buffer) {
    { d.dumpDerivedTerminal(obj, code) } -> std::same_as<bool>;
    // Optional hooks with defaults:
    { d.agentClusterID() } -> std::same_as<String>;
    { d.allowsSharedMemorySerialization() } -> std::same_as<bool>;
    { d.allowsWasmModuleSerialization() } -> std::same_as<bool>;
    { d.toJSArrayBuffer(buffer) } -> std::same_as<JSValue>;
};

namespace StructuredCloneInternal {

template<typename T> inline void writeLittleEndian(Vector<uint8_t>& buffer, T value)
{
    buffer.append(asByteSpan(value));
}

template<> inline void writeLittleEndian<uint8_t>(Vector<uint8_t>& buffer, uint8_t value)
{
    buffer.append(value);
}

template<typename T> inline bool writeLittleEndian(Vector<uint8_t>& buffer, std::span<const T> values)
{
    if (values.size() > std::numeric_limits<uint32_t>::max() / sizeof(T))
        return false;

    buffer.append(asBytes(values));
    return true;
}

template<> inline bool writeLittleEndian<uint8_t>(Vector<uint8_t>& buffer, std::span<const uint8_t> values)
{
    buffer.append(values);
    return true;
}

} // namespace StructuredCloneInternal

template<typename Derived, std::derived_from<CloneSerializationSideChannels> SideChannelsType = CloneSerializationSideChannels>
class CloneSerializerBase : public CloneBase {
public:
    using SideChannels = SideChannelsType;

protected:
    using ObjectPoolMap = HashMap<JSObject*, uint32_t>;
    using StringConstantPool = HashMap<RefPtr<AtomStringImpl>, uint32_t, IdentifierRepHash>;

    CloneSerializerBase(JSGlobalObject* lexicalGlobalObject, Vector<uint8_t>& buffer)
        : CloneBase(lexicalGlobalObject)
        , m_buffer(buffer)
    {
    }

    // Valid once serialize() has run; the serializer must not be used again afterwards.
    SideChannels takeSideChannels() { return WTF::move(m_sideChannels); }

    // BEGIN: hooks for embedders.
    // Derived can override any of these; the defaults below are used when Derived doesn't provide its own.

public:
    String agentClusterID() { return emptyString(); }
    bool allowsSharedMemorySerialization() { return true; }
    bool allowsWasmModuleSerialization() { return true; }
    JSValue toJSArrayBuffer(ArrayBuffer& arrayBuffer)
    {
        VM& vm = m_lexicalGlobalObject->vm();
        return vm.m_typedArrayController->toJS(m_lexicalGlobalObject, m_lexicalGlobalObject, arrayBuffer);
    }

    // END: hooks for embedders.

protected:
    template<typename T> requires std::is_enum_v<T>
    void write(T tag)
    {
        if constexpr (std::is_same_v<T, SerializationTag>)
            SERIALIZE_TRACE("serialize ", tag);
        StructuredCloneInternal::writeLittleEndian<uint8_t>(m_buffer, static_cast<uint8_t>(tag));
    }

    void write(bool b)        { write(static_cast<uint8_t>(b)); }
    void write(uint8_t c)     { StructuredCloneInternal::writeLittleEndian(m_buffer, c); }
    void write(uint16_t i)    { StructuredCloneInternal::writeLittleEndian(m_buffer, i); }
    void write(uint32_t i)    { StructuredCloneInternal::writeLittleEndian(m_buffer, i); }
    void write(int32_t i)     { StructuredCloneInternal::writeLittleEndian(m_buffer, i); }
    void write(uint64_t i)    { StructuredCloneInternal::writeLittleEndian(m_buffer, i); }

    void write(double d)
    {
        StructuredCloneInternal::writeLittleEndian(m_buffer, std::bit_cast<int64_t>(d));
    }

    void write(std::span<const uint8_t> data)
    {
        m_buffer.append(data);
    }

    void writeResizableArrayBuffer(std::span<const uint8_t> data, size_t maxByteLength)
    {
        write(static_cast<uint64_t>(data.size()));
        write(static_cast<uint64_t>(maxByteLength));
        write(data);
    }

    template<class T>
    void writeConstantPoolIndex(const T& constantPool, unsigned i)
    {
        ASSERT(i < constantPool.size());
        if (constantPool.size() <= 0xFF)
            write(static_cast<uint8_t>(i));
        else if (constantPool.size() <= 0xFFFF)
            write(static_cast<uint16_t>(i));
        else
            write(static_cast<uint32_t>(i));
    }

    void writeStringIndex(unsigned i)
    {
        writeConstantPoolIndex(m_constantPool, i);
    }

    void writeObjectIndex(unsigned i)
    {
        writeConstantPoolIndex(m_objectPoolMap, i);
    }

    template<SerializationTag tag1, SerializationTag tag2 = ErrorTag, SerializationTag tag3 = ErrorTag>
    bool writeObjectReferenceIfDupe(JSObject* object)
    {
        static_assert(canBeAddedToObjectPool(tag1)
            && (canBeAddedToObjectPool(tag2) || tag2 == ErrorTag)
            && (canBeAddedToObjectPool(tag3) || tag3 == ErrorTag));

        // Record object for graph reconstruction
        auto found = m_objectPoolMap.find(object);

        // Handle duplicate references
        if (found != m_objectPoolMap.end()) {
            write(ObjectReferenceTag);
            ASSERT(found->value < m_objectPoolMap.size());
            writeObjectIndex(found->value);
            return true; // is dupe.
        }
        return false; // not dupe.
    }

    template<SerializationTag tag1, SerializationTag tag2 = ErrorTag, SerializationTag tag3 = ErrorTag>
    bool addToObjectPool(JSObject* object)
    {
        static_assert(canBeAddedToObjectPool(tag1)
            && (canBeAddedToObjectPool(tag2) || tag2 == ErrorTag)
            && (canBeAddedToObjectPool(tag3) || tag3 == ErrorTag));

        m_objectPoolMap.add(object, m_objectPoolMap.size());
        m_objectPool.appendWithCrashOnOverflow(object);

        if constexpr (tag2 == ErrorTag)
            appendObjectPoolTag(tag1);

        return true; // new object added.
    }

    template<SerializationTag tag1, SerializationTag tag2 = ErrorTag, SerializationTag tag3 = ErrorTag>
    bool addToObjectPoolIfNotDupe(JSObject* object)
    {
        static_assert(canBeAddedToObjectPool(tag1)
            && (canBeAddedToObjectPool(tag2) || tag2 == ErrorTag)
            && (canBeAddedToObjectPool(tag3) || tag3 == ErrorTag));

        if (writeObjectReferenceIfDupe<tag1, tag2, tag3>(object))
            return false; // new object NOT added. It's a dupe.

        addToObjectPool<tag1, tag2, tag3>(object);
        return true; // new object added.
    }

    void write(const AtomString& ident)
    {
        const String& str = ident.string();
        StringConstantPool::AddResult addResult = m_constantPool.add(ident.impl(), m_constantPool.size());
        if (!addResult.isNewEntry) {
            write(StringPoolTag);
            writeStringIndex(addResult.iterator->value);
            return;
        }

        unsigned length = str.length();

        // Guard against overflow
        if (length > (std::numeric_limits<uint32_t>::max() - sizeof(uint32_t)) / sizeof(char16_t)) {
            fail();
            return;
        }

        if (str.is8Bit())
            StructuredCloneInternal::writeLittleEndian<uint32_t>(m_buffer, length | StringDataIs8BitFlag);
        else
            StructuredCloneInternal::writeLittleEndian<uint32_t>(m_buffer, length);

        if (!length)
            return;
        if (str.is8Bit()) {
            if (!StructuredCloneInternal::writeLittleEndian(m_buffer, str.span8()))
                fail();
            return;
        }
        if (!StructuredCloneInternal::writeLittleEndian(m_buffer, str.span16()))
            fail();
    }

    void write(const String& str)
    {
        if (str.isNull())
            write(emptyAtom());
        else
            write(AtomString(str));
    }

    void writeNullableString(const String& str)
    {
        bool isNull = str.isNull();
        write(isNull);
        if (!isNull)
            write(AtomString(str));
    }

    void dumpString(const String& string)
    {
        if (string.isEmpty())
            write(EmptyStringTag);
        else {
            write(StringTag);
            write(string);
        }
    }

    void dumpStringObject(const String& string)
    {
        if (string.isEmpty()) {
            appendObjectPoolTag(EmptyStringObjectTag);
            write(EmptyStringObjectTag);
        } else {
            appendObjectPoolTag(StringObjectTag);
            write(StringObjectTag);
            write(string);
        }
    }

    void dumpBigIntData(JSValue value)
    {
        ASSERT(value.isBigInt());
#if USE(BIGINT32)
        if (value.isBigInt32()) {
            dumpBigInt32Data(value.bigInt32AsInt32());
            return;
        }
#endif
        dumpHeapBigIntData(downcast<JSBigInt>(value));
    }

#if USE(BIGINT32)
    void dumpBigInt32Data(int32_t integer)
    {
        write(integer < 0);
        if (!integer) {
            write(static_cast<uint32_t>(0)); // Length-in-uint64_t
            return;
        }
        write(static_cast<uint32_t>(1)); // Length-in-uint64_t
        int64_t value = static_cast<int64_t>(integer);
        if (value < 0)
            value = -value;
        write(static_cast<uint64_t>(value));
    }
#endif

    void dumpHeapBigIntData(JSBigInt* bigInt)
    {
        write(bigInt->sign());
        if constexpr (sizeof(JSBigInt::Digit) == sizeof(uint64_t)) {
            write(static_cast<uint32_t>(bigInt->length()));
            for (unsigned index = 0; index < bigInt->length(); ++index)
                write(static_cast<uint64_t>(bigInt->digit(index)));
        } else {
            ASSERT(sizeof(JSBigInt::Digit) == sizeof(uint32_t));
            uint32_t numberOfUint64Elements = bigInt->length() / 2;
            if (bigInt->length() & 0x1)
                ++numberOfUint64Elements;
            write(numberOfUint64Elements);
            uint64_t value = 0;
            for (unsigned index = 0; index < bigInt->length(); ++index) {
                if (!(index & 0x1))
                    value = bigInt->digit(index);
                else {
                    value = (static_cast<uint64_t>(bigInt->digit(index)) << 32) | value;
                    write(static_cast<uint64_t>(value));
                    value = 0;
                }
            }
            if (bigInt->length() & 0x1)
                write(static_cast<uint64_t>(value));
        }
    }

    static bool terminalWithException(SerializationReturnCode& code)
    {
        code = SerializationReturnCode::ExistingExceptionError;
        return true;
    }

    bool dumpArrayBufferView(JSObject* obj, SerializationReturnCode& code)
    {
        auto scope = DECLARE_THROW_SCOPE(m_lexicalGlobalObject->vm());
        write(ArrayBufferViewTag);
        switch (obj->type()) {
#define JSC_WRITE_ARRAY_BUFFER_VIEW_SUBTAG(name) \
        case name##ArrayType: \
            write(name##ArrayTag); \
            break;
        FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_WRITE_ARRAY_BUFFER_VIEW_SUBTAG)
#undef JSC_WRITE_ARRAY_BUFFER_VIEW_SUBTAG
        case DataViewType:
            write(DataViewTag);
            break;
        default:
            // We need to return true here because the client only checks for the error condition if
            // the return value is true (same as all the error cases below).
            code = SerializationReturnCode::DataCloneError;
            return true;
        }

        if (uncheckedDowncast<JSArrayBufferView>(obj)->isOutOfBounds()) [[unlikely]] {
            code = SerializationReturnCode::DataCloneError;
            return true;
        }

        RefPtr<ArrayBufferView> arrayBufferView = uncheckedDowncast<JSArrayBufferView>(obj)->possiblySharedImpl();
        if (arrayBufferView->isResizableOrGrowableShared()) {
            uint64_t byteOffset = arrayBufferView->byteOffsetRaw();
            write(byteOffset);
            uint64_t byteLength = arrayBufferView->byteLengthRaw();
            if (arrayBufferView->isAutoLength())
                byteLength = autoLengthMarker;
            write(byteLength);
        } else {
            uint64_t byteOffset = arrayBufferView->byteOffset();
            write(byteOffset);
            uint64_t byteLength = arrayBufferView->byteLength();
            write(byteLength);
        }
        RefPtr<ArrayBuffer> arrayBuffer = arrayBufferView->possiblySharedBuffer();
        if (!arrayBuffer) {
            code = SerializationReturnCode::ValidationError;
            return true;
        }

        JSValue jsArrayBuffer = static_cast<Derived*>(this)->toJSArrayBuffer(*arrayBuffer);
        RETURN_IF_EXCEPTION(scope, terminalWithException(code));
        RELEASE_AND_RETURN(scope, dumpIfTerminal(jsArrayBuffer, code));
    }

    ALWAYS_INLINE bool dumpIfTerminal(JSValue value, SerializationReturnCode& code)
    {
        // Note: This can't be a requirement on the template as, in the common usage,
        // Derived will still be an incomplete type
        static_assert(StructuredCloneSerializerHandler<Derived>,
            "Derived class must satisfy StructuredCloneSerializerHandler");
        auto scope = DECLARE_THROW_SCOPE(m_lexicalGlobalObject->vm());
        if (value.isNull()) {
            write(NullTag);
            return true;
        }
        if (value.isUndefined()) {
            write(UndefinedTag);
            return true;
        }
        if (value.isInt32()) {
            int32_t i = value.asInt32();
            if (!i)
                write(ZeroTag);
            else if (i == 1)
                write(OneTag);
            else {
                write(IntTag);
                write(static_cast<uint32_t>(i));
            }
            return true;
        }
        if (value.isNumber()) {
            write(DoubleTag);
            write(value.asDouble());
            return true;
        }
        if (value.isBoolean()) {
            write(value.isTrue() ? TrueTag : FalseTag);
            return true;
        }
        if (value.isString()) {
            auto string = asString(value)->value(m_lexicalGlobalObject);
            RETURN_IF_EXCEPTION(scope, terminalWithException(code));
            dumpString(string);
            return true;
        }
        if (value.isBigInt()) {
            write(BigIntTag);
            dumpBigIntData(value);
            return true;
        }

        if (!value.isObject()) {
            ASSERT(value.isSymbol());
            code = SerializationReturnCode::DataCloneError;
            return true;
        }

        auto* obj = asObject(value);
        if (auto* dateObject = dynamicDowncast<DateInstance>(obj)) {
            write(DateTag);
            write(dateObject->internalNumber());
            return true;
        }
        if (auto* regExp = dynamicDowncast<RegExpObject>(obj)) {
            write(RegExpTag);
            write(regExp->regExp()->pattern());
            write(String::fromLatin1(JSC::Yarr::flagsString(regExp->regExp()->flags()).data()));
            return true;
        }
        if (auto* booleanObject = dynamicDowncast<BooleanObject>(obj)) {
            if (!addToObjectPoolIfNotDupe<TrueObjectTag, FalseObjectTag>(booleanObject))
                return true;
            auto tag = booleanObject->internalValue().toBoolean(m_lexicalGlobalObject) ? TrueObjectTag : FalseObjectTag;
            RETURN_IF_EXCEPTION(scope, terminalWithException(code));
            write(tag);
            appendObjectPoolTag(tag);
            return true;
        }
        if (auto* stringObject = dynamicDowncast<StringObject>(obj)) {
            if (!addToObjectPoolIfNotDupe<EmptyStringObjectTag, StringObjectTag>(stringObject))
                return true;
            auto str = asString(stringObject->internalValue())->value(m_lexicalGlobalObject);
            RETURN_IF_EXCEPTION(scope, terminalWithException(code));
            dumpStringObject(str);
            return true;
        }
        if (auto* numberObject = dynamicDowncast<NumberObject>(obj)) {
            if (!addToObjectPoolIfNotDupe<NumberObjectTag>(numberObject))
                return true;
            write(NumberObjectTag);
            write(numberObject->internalValue().asNumber());
            return true;
        }
        if (auto* bigIntObject = dynamicDowncast<BigIntObject>(obj)) {
            if (!addToObjectPoolIfNotDupe<BigIntObjectTag>(bigIntObject))
                return true;
            write(BigIntObjectTag);
            JSValue bigIntValue = bigIntObject->internalValue();
            ASSERT(bigIntValue.isBigInt());
            dumpBigIntData(bigIntValue);
            return true;
        }
        if (RefPtr arrayBuffer = toPossiblySharedArrayBuffer(m_lexicalGlobalObject->vm(), obj)) {
            if (arrayBuffer->isDetached()) {
                code = SerializationReturnCode::DataCloneError;
                return true;
            }
            auto index = m_transferredArrayBuffers.find(obj);
            if (index != m_transferredArrayBuffers.end()) {
                write(ArrayBufferTransferTag);
                write(index->value);
                return true;
            }
            if (!addToObjectPoolIfNotDupe<ArrayBufferTag, ResizableArrayBufferTag, SharedArrayBufferTag>(obj))
                return true;

            if (arrayBuffer->isShared()) {
                // https://html.spec.whatwg.org/multipage/structured-data.html#structuredserializeinternal
                if (static_cast<Derived*>(this)->allowsSharedMemorySerialization()) {
                    ArrayBufferContents contents;
                    if (arrayBuffer->shareWith(contents)) {
                        uint32_t sharedBufferIndex = m_sideChannels.sharedBuffers.size();
                        appendObjectPoolTag(SharedArrayBufferTag);
                        write(SharedArrayBufferTag);
                        write(static_cast<Derived*>(this)->agentClusterID());
                        m_sideChannels.sharedBuffers.append(WTF::move(contents));
                        write(sharedBufferIndex);
                        return true;
                    }
                }
                code = SerializationReturnCode::DataCloneError;
                return true;
            }

            if (arrayBuffer->isResizableOrGrowableShared()) {
                appendObjectPoolTag(ResizableArrayBufferTag);
                write(ResizableArrayBufferTag);
                writeResizableArrayBuffer(arrayBuffer->span(), arrayBuffer->maxByteLength().value_or(0));
                return true;
            }

            appendObjectPoolTag(ArrayBufferTag);
            write(ArrayBufferTag);
            uint64_t byteLength = arrayBuffer->byteLength();
            write(byteLength);
            write(arrayBuffer->span());
            return true;
        }
        if (obj->inherits<JSArrayBufferView>()) {
            // Note: we can't just use addToObjectPoolIfNotDupe() here because the deserializer
            // expects to deserialize the children before it deserializes the JSArrayBufferView.
            // We need to make the serializer follow the same serialization order here by doing
            // this dance with writeObjectReferenceIfDupe() and addToObjectPool().
            if (writeObjectReferenceIfDupe<ArrayBufferViewTag>(obj))
                return true;
            bool success = dumpArrayBufferView(obj, code);
            RETURN_IF_EXCEPTION(scope, terminalWithException(code));
            addToObjectPool<ArrayBufferViewTag>(obj);
            return success;
        }
#if ENABLE(WEBASSEMBLY)
        if (JSWebAssemblyModule* module = dynamicDowncast<JSWebAssemblyModule>(obj)) {
            if (!static_cast<Derived*>(this)->allowsWasmModuleSerialization()) {
                code = SerializationReturnCode::DataCloneError;
                return true;
            }

            uint32_t index = m_sideChannels.wasmModules.size();
            m_sideChannels.wasmModules.append(Ref { module->module() });
            write(WasmModuleTag);
            write(static_cast<Derived*>(this)->agentClusterID());
            write(index);
            return true;
        }
        if (JSWebAssemblyMemory* memory = dynamicDowncast<JSWebAssemblyMemory>(obj)) {
            if (!static_cast<Derived*>(this)->allowsSharedMemorySerialization() || memory->memory().sharingMode() != MemorySharingMode::Shared) {
                code = SerializationReturnCode::DataCloneError;
                return true;
            }
            uint32_t index = m_sideChannels.wasmMemoryHandles.size();
            m_sideChannels.wasmMemoryHandles.append(memory->memory().shared());
            write(WasmMemoryTag);
            write(static_cast<Derived*>(this)->agentClusterID());
            write(index);
            // The address type is not recoverable from the shared contents, and a memory declared with a
            // maximum of zero has no contents at all. Embedders that persist serialized values
            // reject shared memory in allowsSharedMemorySerialization(), so this record only ever
            // travels between live agents and needs no version guard.
            write(memory->memory().addressType().is64Bit());
            return true;
        }
#endif
        // The walker descends into JSArray/JSMap/JSSet; never let Derived or the generic error
        // path claim them as terminals.
        if (is<JSArray>(*obj) || is<JSMap>(*obj) || is<JSSet>(*obj))
            return false;

        // Give Derived (the embedder) first refusal. This lets error-like platform objects, such
        // as WebCore's DOMException (which is an ErrorInstance), be serialized as themselves
        // rather than as generic Errors by the ErrorInstance path below.
        bool isDerivedTerminal = static_cast<Derived*>(this)->dumpDerivedTerminal(obj, code);
        RETURN_IF_EXCEPTION(scope, terminalWithException(code));
        if (isDerivedTerminal)
            return true;

        if (auto* errorInstance = dynamicDowncast<ErrorInstance>(obj)) {
            auto errorInformation = extractErrorInformationFromErrorInstance(m_lexicalGlobalObject, *errorInstance);
            RETURN_IF_EXCEPTION(scope, terminalWithException(code));
            if (!errorInformation)
                return false;

            write(ErrorInstanceTag);
            write(static_cast<uint8_t>(errorNameToSerializableErrorType(errorInformation->errorTypeString)));
            writeNullableString(errorInformation->message);
            write(errorInformation->line);
            write(errorInformation->column);
            writeNullableString(errorInformation->sourceURL);
            writeNullableString(errorInformation->stack);
            writeNullableString(errorInformation->cause);
            return true;
        }

        return false;
    }

    void endObject()
    {
        write(TerminatorTag);
    }

    JSValue getProperty(JSObject* object, const Identifier& propertyName)
    {
        auto scope = DECLARE_THROW_SCOPE(m_lexicalGlobalObject->vm());
        PropertySlot slot(object, PropertySlot::InternalMethodType::Get);
        bool found = object->methodTable()->getOwnPropertySlot(object, m_lexicalGlobalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, { });
        if (!found)
            return { };
        RELEASE_AND_RETURN(scope, slot.getValue(m_lexicalGlobalObject, propertyName));
    }

    SerializationReturnCode serialize(JSValue in)
    {
        VM& vm = m_lexicalGlobalObject->vm();
        Vector<uint32_t, 16> indexStack;
        Vector<uint32_t, 16> lengthStack;
        Vector<PropertyNameArrayBuilder, 16> propertyStack;
        Vector<JSObject*, 32> inputObjectStack;
        Vector<JSMapIterator*, 4> mapIteratorStack;
        Vector<JSSetIterator*, 4> setIteratorStack;
        Vector<JSValue, 4> mapIteratorValueStack;
        Vector<WalkerState, 16> stateStack;
        WalkerState state = WalkerState::StateUnknown;
        JSValue inValue = in;
        auto scope = DECLARE_THROW_SCOPE(vm);
        while (1) {
            switch (state) {
            arrayStartState:
            case WalkerState::ArrayStartState: {
                ASSERT(is<JSArray>(inValue));
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;

                JSArray* inArray = asArray(inValue);
                unsigned length = inArray->length();
                if (!addToObjectPoolIfNotDupe<ArrayTag>(inArray))
                    break;
                write(ArrayTag);
                write(length);
                inputObjectStack.append(inArray);
                indexStack.append(0);
                lengthStack.append(length);
            }
            arrayStartVisitIndexedMember:
            [[fallthrough]];
            case WalkerState::ArrayStartVisitIndexedMember: {
                JSObject* array = inputObjectStack.last();
                uint32_t index = indexStack.last();
                if (index == lengthStack.last()) {
                    indexStack.removeLast();
                    lengthStack.removeLast();
                    write(TerminatorTag); // Terminate the indexed property section.

                    propertyStack.append(PropertyNameArrayBuilder(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    array->getOwnNonIndexPropertyNames(m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);
                    if (propertyStack.last().size()) {
                        write(NonIndexPropertiesTag);
                        indexStack.append(0);
                        goto startVisitNamedMember;
                    }
                    propertyStack.removeLast();

                    endObject();
                    inputObjectStack.removeLast();
                    break;
                }
                inValue = array->getDirectIndex(m_lexicalGlobalObject, index);
                RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);
                if (!inValue) {
                    indexStack.last()++;
                    goto arrayStartVisitIndexedMember;
                }

                write(index);
                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                bool isTerminal = dumpIfTerminal(inValue, terminalCode);
                RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);
                if (isTerminal) {
                    if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                        return terminalCode;
                    indexStack.last()++;
                    goto arrayStartVisitIndexedMember;
                }
                stateStack.append(WalkerState::ArrayEndVisitIndexedMember);
                goto stateUnknown;
            }
            case WalkerState::ArrayEndVisitIndexedMember: {
                indexStack.last()++;
                goto arrayStartVisitIndexedMember;
            }
            case WalkerState::ArrayStartVisitNamedMember:
            case WalkerState::ArrayEndVisitNamedMember:
                RELEASE_ASSERT_NOT_REACHED();
            objectStartState:
            case WalkerState::ObjectStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSObject* inObject = asObject(inValue);
                if (!addToObjectPoolIfNotDupe<ObjectTag>(inObject))
                    break;
                write(ObjectTag);
                // At this point, all supported objects other than Object
                // objects have been handled. If we reach this point and
                // the input is not an Object object then we should throw
                // a DataCloneError.
                if (inObject->classInfo() != JSFinalObject::info() && inObject->classInfo() != ObjectPrototype::info())
                    return SerializationReturnCode::DataCloneError;
                inputObjectStack.append(inObject);
                indexStack.append(0);
                propertyStack.append(PropertyNameArrayBuilder(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                inObject->methodTable()->getOwnPropertyNames(inObject, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);
            }
            startVisitNamedMember:
            [[fallthrough]];
            case WalkerState::ObjectStartVisitNamedMember: {
                JSObject* object = inputObjectStack.last();
                uint32_t index = indexStack.last();
                PropertyNameArrayBuilder& properties = propertyStack.last();
                if (index == properties.size()) {
                    endObject();
                    inputObjectStack.removeLast();
                    indexStack.removeLast();
                    propertyStack.removeLast();
                    break;
                }
                inValue = getProperty(object, properties[index]);
                RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);

                if (!inValue) {
                    // Property was removed during serialisation
                    indexStack.last()++;
                    goto startVisitNamedMember;
                }
                write(properties[index].string());

                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                bool isTerminal = dumpIfTerminal(inValue, terminalCode);
                RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);
                if (!isTerminal) {
                    stateStack.append(WalkerState::ObjectEndVisitNamedMember);
                    goto stateUnknown;
                }
                if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                    return terminalCode;
                [[fallthrough]];
            }
            case WalkerState::ObjectEndVisitNamedMember: {
                indexStack.last()++;
                goto startVisitNamedMember;
            }
            mapStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSMap* inMap = downcast<JSMap>(inValue);
                if (!addToObjectPoolIfNotDupe<MapObjectTag>(inMap))
                    break;
                write(MapObjectTag);
                JSMapIterator* iterator = JSMapIterator::create(vm, m_lexicalGlobalObject->mapIteratorStructure(), inMap, IterationKind::Entries);
                m_keepAliveBuffer.appendWithCrashOnOverflow(iterator);
                mapIteratorStack.append(iterator);
                inputObjectStack.append(inMap);
                goto mapDataStartVisitEntry;
            }
            mapDataStartVisitEntry:
            case WalkerState::MapDataStartVisitEntry: {
                JSMapIterator* iterator = mapIteratorStack.last();
                JSValue key, value;
                if (!iterator->nextKeyValue(m_lexicalGlobalObject, key, value)) {
                    mapIteratorStack.removeLast();
                    JSObject* object = inputObjectStack.last();
                    ASSERT(is<JSMap>(*object));
                    propertyStack.append(PropertyNameArrayBuilder(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    object->methodTable()->getOwnPropertyNames(object, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);
                    write(NonMapPropertiesTag);
                    indexStack.append(0);
                    goto startVisitNamedMember;
                }
                inValue = key;
                m_keepAliveBuffer.appendWithCrashOnOverflow(value);
                mapIteratorValueStack.append(value);
                stateStack.append(WalkerState::MapDataEndVisitKey);
                goto stateUnknown;
            }
            case WalkerState::MapDataEndVisitKey: {
                inValue = mapIteratorValueStack.last();
                mapIteratorValueStack.removeLast();
                stateStack.append(WalkerState::MapDataEndVisitValue);
                goto stateUnknown;
            }
            case WalkerState::MapDataEndVisitValue: {
                goto mapDataStartVisitEntry;
            }

            setStartState: {
                ASSERT(inValue.isObject());
                if (inputObjectStack.size() > maximumFilterRecursion)
                    return SerializationReturnCode::StackOverflowError;
                JSSet* inSet = downcast<JSSet>(inValue);
                if (!addToObjectPoolIfNotDupe<SetObjectTag>(inSet))
                    break;
                write(SetObjectTag);
                JSSetIterator* iterator = JSSetIterator::create(vm, m_lexicalGlobalObject->setIteratorStructure(), inSet, IterationKind::Keys);
                m_keepAliveBuffer.appendWithCrashOnOverflow(iterator);
                setIteratorStack.append(iterator);
                inputObjectStack.append(inSet);
                goto setDataStartVisitEntry;
            }
            setDataStartVisitEntry:
            case WalkerState::SetDataStartVisitEntry: {
                JSSetIterator* iterator = setIteratorStack.last();
                JSValue key;
                if (!iterator->next(m_lexicalGlobalObject, key)) {
                    setIteratorStack.removeLast();
                    JSObject* object = inputObjectStack.last();
                    ASSERT(is<JSSet>(*object));
                    propertyStack.append(PropertyNameArrayBuilder(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude));
                    object->methodTable()->getOwnPropertyNames(object, m_lexicalGlobalObject, propertyStack.last(), DontEnumPropertiesMode::Exclude);
                    RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);
                    write(NonSetPropertiesTag);
                    indexStack.append(0);
                    goto startVisitNamedMember;
                }
                inValue = key;
                stateStack.append(WalkerState::SetDataEndVisitKey);
                goto stateUnknown;
            }
            case WalkerState::SetDataEndVisitKey: {
                goto setDataStartVisitEntry;
            }

            stateUnknown:
            case WalkerState::StateUnknown: {
                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                bool isTerminal = dumpIfTerminal(inValue, terminalCode);
                RETURN_IF_EXCEPTION(scope, SerializationReturnCode::ExistingExceptionError);
                if (isTerminal) {
                    if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                        return terminalCode;
                    break;
                }

                if (is<JSArray>(inValue))
                    goto arrayStartState;
                if (is<JSMap>(inValue))
                    goto mapStartState;
                if (is<JSSet>(inValue))
                    goto setStartState;
                goto objectStartState;
            }
            }
            if (stateStack.isEmpty())
                break;

            state = stateStack.last();
            stateStack.removeLast();
        }
        if (m_failed)
            return SerializationReturnCode::UnspecifiedError;

        return SerializationReturnCode::SuccessfullyCompleted;
    }

    Vector<uint8_t>& m_buffer;
    StringConstantPool m_constantPool;
    ObjectPoolMap m_objectPoolMap;
    ObjectPoolMap m_transferredArrayBuffers;
    SideChannels m_sideChannels;
};

} // namespace JSC
