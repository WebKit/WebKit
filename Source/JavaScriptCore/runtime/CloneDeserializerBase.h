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
#include <JavaScriptCore/DataView.h>
#include <JavaScriptCore/DateInstance.h>
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSDataView.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSMapInlines.h>
#include <JavaScriptCore/JSObjectInlines.h>
#include <JavaScriptCore/JSSetInlines.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/JSTypedArrays.h>
#include <JavaScriptCore/MarkedVector.h>
#include <JavaScriptCore/NumberObject.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <JavaScriptCore/RegExpObject.h>
#include <JavaScriptCore/StringObject.h>
#include <JavaScriptCore/TopExceptionScope.h>
#include <JavaScriptCore/TypedArrayController.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/TypedArrays.h>
#include <JavaScriptCore/YarrFlags.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBASSEMBLY)
#include <JavaScriptCore/JSWebAssemblyMemory.h>
#include <JavaScriptCore/JSWebAssemblyModule.h>
#include <JavaScriptCore/WasmModule.h>
#endif

namespace JSC {

template<typename Derived>
concept StructuredCloneDeserializerHandler = requires(Derived& d, SerializationTag t, Ref<ArrayBuffer>&& buffer) {
    { d.readDerivedTerminal(t) } -> std::same_as<JSValue>;
    { d.isTagExposed(t) } -> std::same_as<bool>;
    // Optional hooks with defaults:
    { d.agentClusterID() } -> std::same_as<String>;
    { d.toJSArrayBuffer(WTF::move(buffer)) } -> std::same_as<JSValue>;
};

namespace StructuredCloneInternal {

template<typename T> inline bool readLittleEndian(std::span<const uint8_t>& span, T& value)
{
    if (span.size() < sizeof(value))
        return false;
    value = consumeAndReinterpretCastTo<const T>(span);
    return true;
}

} // namespace StructuredCloneInternal

class CachedString {
public:
    CachedString(String&& string)
        : m_string(WTF::move(string))
    {
    }

    JSValue jsString(CloneBase&);
    const String& string() const { return m_string; }
    String takeString() { return WTF::move(m_string); }

private:
    String m_string;
    JSValue m_jsString;
};

class CachedStringRef {
public:
    CachedStringRef() = default;

    CachedStringRef(Vector<CachedString>* base, size_t index)
        : m_base(base)
        , m_index(index)
    {
    }

    CachedString* operator->() { ASSERT(m_base); return &m_base->at(m_index); }

private:
    Vector<CachedString>* m_base { nullptr };
    size_t m_index { 0 };
};

enum class ShouldAtomize : bool { No, Yes };

template<typename Derived>
class CloneDeserializerBase : public CloneBase {
protected:
    CloneDeserializerBase(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, std::span<const uint8_t> data, CloneDeserializationSideChannels sideChannels)
        : CloneBase(lexicalGlobalObject)
        , m_globalObject(globalObject)
        , m_data(data)
        , m_sideChannels(sideChannels)
        , m_arrayBuffers(sideChannels.arrayBufferContents ? sideChannels.arrayBufferContents->size() : 0)
    {
    }

    // BEGIN: hooks for embedders.
    // Derived can override any of these; the defaults below are used when Derived doesn't provide its own.

public:
    String agentClusterID() { return emptyString(); }
    JSValue toJSArrayBuffer(Ref<ArrayBuffer>&& arrayBuffer)
    {
        VM& vm = m_lexicalGlobalObject->vm();
        return vm.m_typedArrayController->toJS(m_lexicalGlobalObject, m_globalObject, arrayBuffer.get());
    }

    // END: hooks for embedders.

protected:
    template<typename T> bool readLittleEndian(T& value)
    {
        if (m_failed || !StructuredCloneInternal::readLittleEndian(m_data, value)) [[unlikely]] {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return false;
        }
        return true;
    }

    bool read(uint8_t& i)  { return readLittleEndian(i); }
    bool read(uint16_t& i) { return readLittleEndian(i); }
    bool read(uint32_t& i) { return readLittleEndian(i); }
    bool read(uint64_t& i) { return readLittleEndian(i); }
    bool read(int32_t& i)  { return readLittleEndian(*reinterpret_cast<uint32_t*>(&i)); }

    bool read(double& d)
    {
        uint64_t bits;
        if (!readLittleEndian(bits))
            return false;
        d = purifyNaN(std::bit_cast<double>(bits));
        return true;
    }

    bool read(bool& b)
    {
        if (m_majorVersion >= 14) {
            uint8_t integer;
            if (!read(integer) || integer > 1)
                return false;
            b = !!integer;
            return true;
        }
        int32_t integer;
        if (!read(integer) || integer > 1)
            return false;
        b = !!integer;
        return true;
    }

    SerializationTag readTag()
    {
        if (m_data.empty()) [[unlikely]] {
            SERIALIZE_TRACE("FAIL deserialize");
            return ErrorTag;
        }
        auto tag = static_cast<SerializationTag>(consume(m_data));
        SERIALIZE_TRACE("deserialize ", tag);
        return tag;
    }

    bool readAndStoreVersion()
    {
        unsigned version;
        if (!read(version))
            return false;
        m_majorVersion = majorVersionFor(version);
        m_minorVersion = minorVersionFor(version);
        return true;
    }

    bool isValid() const
    {
        if (m_majorVersion > CurrentMajorVersion)
            return false;
        if (m_majorVersion == 12)
            return m_minorVersion <= 1;
        return !m_minorVersion;
    }

    bool shouldRetryWithVersionUpgrade() const
    {
        if (m_majorVersion == 14 && !m_minorVersion)
            return true;
        if (m_majorVersion == 12 && !m_minorVersion)
            return true;
        return false;
    }

    void upgradeVersion()
    {
        ASSERT(shouldRetryWithVersionUpgrade());
        if (m_majorVersion == 14 && !m_minorVersion) {
            m_majorVersion = 15;
            return;
        }
        if (m_majorVersion == 12 && !m_minorVersion)
            m_minorVersion = 1;
    }

    template<typename T>
    std::optional<uint32_t> readConstantPoolIndex(const T& constantPool)
    {
        if (constantPool.size() <= 0xFF) {
            uint8_t i8;
            if (!read(i8))
                return std::nullopt;
            return i8;
        }
        if (constantPool.size() <= 0xFFFF) {
            uint16_t i16;
            if (!read(i16))
                return std::nullopt;
            return i16;
        }
        uint32_t i;
        if (!read(i))
            return std::nullopt;
        return i;
    }

    std::optional<uint32_t> readStringIndex()
    {
        return readConstantPoolIndex(m_constantPool);
    }

    static bool readString(std::span<const uint8_t>& span, String& str, unsigned length, bool is8Bit, ShouldAtomize shouldAtomize)
    {
        if (length >= std::numeric_limits<int32_t>::max() / sizeof(char16_t))
            return false;

        if (is8Bit) {
            if (span.size() < length)
                return false;
            if (shouldAtomize == ShouldAtomize::Yes)
                str = AtomString(byteCast<Latin1Character>(consumeSpan(span, length)));
            else
                str = String(byteCast<Latin1Character>(consumeSpan(span, length)));
            return true;
        }

        size_t size = length * sizeof(char16_t);
        if (span.size() < size)
            return false;

        auto stringSpan = consumeSpan(span, size);
        if (shouldAtomize == ShouldAtomize::Yes)
            str = AtomString(spanReinterpretCast<const char16_t>(stringSpan));
        else
            str = String(spanReinterpretCast<const char16_t>(stringSpan));
        return true;
    }

    bool readStringData(CachedStringRef& cachedString, ShouldAtomize shouldAtomize = ShouldAtomize::No)
    {
        bool scratch;
        return readStringData(cachedString, scratch, shouldAtomize);
    }

    bool readStringData(CachedStringRef& cachedString, bool& wasTerminator, ShouldAtomize shouldAtomize = ShouldAtomize::No)
    {
        if (m_failed)
            return false;
        uint32_t length = 0;
        if (!read(length))
            return false;
        if (length == TerminatorTag) {
            wasTerminator = true;
            return false;
        }
        if (length == StringPoolTag) {
            auto index = readStringIndex();
            if (!index || *index >= m_constantPool.size()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return false;
            }
            cachedString = CachedStringRef(&m_constantPool, *index);
            return true;
        }
        bool is8Bit = length & StringDataIs8BitFlag;
        length &= ~StringDataIs8BitFlag;
        String str;
        if (!readString(m_data, str, length, is8Bit, shouldAtomize)) [[unlikely]] {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return false;
        }
        m_constantPool.append(WTF::move(str));
        cachedString = CachedStringRef(&m_constantPool, m_constantPool.size() - 1);
        return true;
    }

    bool readNullableString(String& nullableString, ShouldAtomize shouldAtomize = ShouldAtomize::No)
    {
        bool isNull;
        if (!read(isNull))
            return false;
        if (isNull)
            return true;
        CachedStringRef stringData;
        if (!readStringData(stringData, shouldAtomize))
            return false;
        nullableString = stringData->string();
        return true;
    }

    JSValue readBigInt()
    {
        // Sign is always written as a single byte.
        // FIXME: Why not read this back as a bool since it's written as one?
        uint8_t signByte;
        if (!read(signByte))
            return JSValue();
        bool sign = !!signByte;
        uint32_t numberOfUint64Elements = 0;
        if (!read(numberOfUint64Elements))
            return JSValue();

        if (!numberOfUint64Elements) {
#if USE(BIGINT32)
            return jsBigInt32(0);
#else
            JSBigInt* bigInt = JSBigInt::tryCreateZero(m_lexicalGlobalObject->vm());
            if (!bigInt) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return bigInt;
#endif
        }

#if USE(BIGINT32)
        static_assert(sizeof(JSBigInt::Digit) == sizeof(uint64_t));
        if (numberOfUint64Elements == 1) {
            uint64_t digit64 = 0;
            if (!read(digit64))
                return JSValue();
            if (sign) {
                if (digit64 <= static_cast<uint64_t>(-static_cast<int64_t>(INT32_MIN)))
                    return jsBigInt32(static_cast<int32_t>(-static_cast<int64_t>(digit64)));
            } else {
                if (digit64 <= INT32_MAX)
                    return jsBigInt32(static_cast<int32_t>(digit64));
            }
            ASSERT(digit64);
            JSBigInt* bigInt = JSBigInt::tryCreateFrom(nullptr, m_lexicalGlobalObject->vm(), sign, std::span { &digit64, 1 });
            if (!bigInt) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return tryConvertToBigInt32(bigInt);
        }
#endif
        Vector<JSBigInt::Digit, 16> digits;
        if constexpr (sizeof(JSBigInt::Digit) == sizeof(uint64_t)) {
            digits.reserveInitialCapacity(numberOfUint64Elements);
            for (uint32_t index = 0; index < numberOfUint64Elements; ++index) {
                uint64_t digit64 = 0;
                if (!read(digit64))
                    return JSValue();
                digits.append(digit64);
            }
        } else {
            ASSERT(sizeof(JSBigInt::Digit) == sizeof(uint32_t));
            auto actualBigIntLength = WTF::checkedProduct<uint32_t>(numberOfUint64Elements, 2);
            if (actualBigIntLength.hasOverflowed()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            digits.reserveInitialCapacity(actualBigIntLength.value());
            for (uint32_t index = 0; index < numberOfUint64Elements; ++index) {
                uint64_t digit64 = 0;
                if (!read(digit64))
                    return JSValue();
                digits.append(static_cast<uint32_t>(digit64));
                digits.append(static_cast<uint32_t>(digit64 >> 32));
            }
        }

        auto* bigInt = JSBigInt::tryCreateFrom(nullptr, m_lexicalGlobalObject->vm(), sign, digits.span());
        if (!bigInt) [[unlikely]] {
            SERIALIZE_TRACE("FAIL deserialize");
            fail();
            return JSValue();
        }
        return tryConvertToBigInt32(bigInt);
    }

    template<SerializationTag tag>
    void addToObjectPool(JSValue object)
    {
        static_assert(canBeAddedToObjectPool(tag));
        m_objectPool.appendWithCrashOnOverflow(object);
        appendObjectPoolTag(tag);
    }

    bool NODELETE readArrayBufferViewSubtag(ArrayBufferViewSubtag& tag)
    {
        if (m_data.empty())
            return false;
        tag = static_cast<ArrayBufferViewSubtag>(consume(m_data));
        return true;
    }

    template<typename LengthType>
    bool readArrayBufferImpl(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        LengthType length;
        if (!read(length))
            return false;
        if (m_data.size() < length)
            return false;
        arrayBuffer = ArrayBuffer::tryCreate(m_data.first(length));
        if (!arrayBuffer)
            return false;
        skip(m_data, length);
        return true;
    }

    bool readArrayBuffer(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        if (m_majorVersion < 10)
            return readArrayBufferImpl<uint32_t>(arrayBuffer);
        return readArrayBufferImpl<uint64_t>(arrayBuffer);
    }

    bool readResizableNonSharedArrayBuffer(RefPtr<ArrayBuffer>& arrayBuffer)
    {
        uint64_t byteLength;
        if (!read(byteLength))
            return false;
        uint64_t maxByteLength;
        if (!read(maxByteLength))
            return false;
        if (m_data.size() < byteLength)
            return false;
        arrayBuffer = ArrayBuffer::tryCreate(byteLength, 1, maxByteLength);
        if (!arrayBuffer)
            return false;
        ASSERT(arrayBuffer->isResizableNonShared());
        memcpySpan(arrayBuffer->mutableSpan(), consumeSpan(m_data, byteLength));
        return true;
    }

    template<typename LengthType>
    bool readArrayBufferViewImpl(VM& vm, JSValue& arrayBufferView)
    {
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (!isSafeToRecurse())
            return false;
        ArrayBufferViewSubtag arrayBufferViewSubtag;
        if (!readArrayBufferViewSubtag(arrayBufferViewSubtag))
            return false;
        LengthType byteOffset;
        if (!read(byteOffset))
            return false;
        LengthType byteLength;
        if (!read(byteLength))
            return false;
        JSValue arrayBufferValue = readTerminal();
        RETURN_IF_EXCEPTION(scope, false);
        if (!arrayBufferValue || !arrayBufferValue.inherits<JSArrayBuffer>())
            return false;
        JSObject* arrayBufferObj = asObject(arrayBufferValue);

        unsigned elementSize = typedArrayElementSize(arrayBufferViewSubtag);
        if (!elementSize)
            return false;

        RefPtr<ArrayBuffer> arrayBuffer = toPossiblySharedArrayBuffer(vm, arrayBufferObj);
        if (!arrayBuffer) {
            arrayBufferView = jsNull();
            return true;
        }

        std::optional<size_t> length;
        if (byteLength != autoLengthMarker) {
            LengthType computedLength = byteLength / elementSize;
            if (computedLength * elementSize != byteLength)
                return false;
            length = computedLength;
        } else {
            if (!arrayBuffer->isResizableOrGrowableShared())
                return false;
        }

        if (!ArrayBufferView::verifySubRangeLength(arrayBuffer->byteLength(), byteOffset, length.value_or(0), 1))
            return false;

        auto makeArrayBufferView = [&](auto&& view) -> bool {
            if (!view)
                return false;
            arrayBufferView = view->wrap(m_lexicalGlobalObject, m_globalObject);
            RETURN_IF_EXCEPTION(scope, false);
            return true;
        };

        switch (arrayBufferViewSubtag) {
#define JSC_READ_ARRAY_BUFFER_VIEW_SUBTAG(name) \
        case name##ArrayTag: \
            return makeArrayBufferView(name##Array::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(JSC_READ_ARRAY_BUFFER_VIEW_SUBTAG)
#undef JSC_READ_ARRAY_BUFFER_VIEW_SUBTAG
        case DataViewTag:
            return makeArrayBufferView(DataView::wrappedAs(arrayBuffer.releaseNonNull(), byteOffset, length));
        default:
            return false;
        }
    }

    bool readArrayBufferView(VM& vm, JSValue& arrayBufferView)
    {
        if (!isSafeToRecurse())
            return false;
        if (m_majorVersion < 10)
            return readArrayBufferViewImpl<uint32_t>(vm, arrayBufferView);
        return readArrayBufferViewImpl<uint64_t>(vm, arrayBufferView);
    }

    ALWAYS_INLINE JSValue readTerminalImpl(SerializationTag tag)
    {
        switch (tag) {
        case UndefinedTag:
            return jsUndefined();
        case NullTag:
            return jsNull();
        case IntTag: {
            int32_t i;
            if (!read(i))
                return JSValue();
            return jsNumber(i);
        }
        case ZeroTag:
            return jsNumber(0);
        case OneTag:
            return jsNumber(1);
        case FalseTag:
            return jsBoolean(false);
        case TrueTag:
            return jsBoolean(true);
        case DoubleTag: {
            double d;
            if (!read(d))
                return JSValue();
            return jsNumber(d);
        }
        case StringTag: {
            CachedStringRef cachedString;
            if (!readStringData(cachedString))
                return JSValue();
            return cachedString->jsString(*this);
        }
        case EmptyStringTag:
            return jsEmptyString(m_lexicalGlobalObject->vm());
        case BigIntTag:
            return readBigInt();
        case DateTag: {
            double d;
            if (!read(d))
                return JSValue();
            return DateInstance::create(m_lexicalGlobalObject->vm(), m_globalObject->dateStructure(), d);
        }
        case RegExpTag: {
            CachedStringRef pattern;
            if (!readStringData(pattern))
                return JSValue();
            CachedStringRef flags;
            if (!readStringData(flags))
                return JSValue();
            auto reFlags = Yarr::parseFlags(flags->string());
            if (!reFlags.has_value())
                return JSValue();
            VM& vm = m_lexicalGlobalObject->vm();
            RegExp* regExp = RegExp::create(vm, pattern->string(), reFlags.value());
            return RegExpObject::create(vm, m_globalObject->regExpStructure(), regExp);
        }
        case NumberObjectTag: {
            double d;
            if (!read(d))
                return JSValue();
            NumberObject* obj = constructNumber(m_globalObject, jsNumber(d));
            addToObjectPool<NumberObjectTag>(obj);
            return obj;
        }
        case StringObjectTag: {
            CachedStringRef cachedString;
            if (!readStringData(cachedString))
                return JSValue();
            StringObject* obj = constructString(m_lexicalGlobalObject->vm(), m_globalObject, cachedString->jsString(*this));
            addToObjectPool<StringObjectTag>(obj);
            return obj;
        }
        case EmptyStringObjectTag: {
            VM& vm = m_lexicalGlobalObject->vm();
            StringObject* obj = constructString(vm, m_globalObject, jsEmptyString(vm));
            addToObjectPool<EmptyStringObjectTag>(obj);
            return obj;
        }
        case FalseObjectTag: {
            BooleanObject* obj = BooleanObject::create(m_lexicalGlobalObject->vm(), m_globalObject->booleanObjectStructure());
            obj->setInternalValue(m_lexicalGlobalObject->vm(), jsBoolean(false));
            addToObjectPool<FalseObjectTag>(obj);
            return obj;
        }
        case TrueObjectTag: {
            BooleanObject* obj = BooleanObject::create(m_lexicalGlobalObject->vm(), m_globalObject->booleanObjectStructure());
            obj->setInternalValue(m_lexicalGlobalObject->vm(), jsBoolean(true));
            addToObjectPool<TrueObjectTag>(obj);
            return obj;
        }
        case BigIntObjectTag: {
            JSValue bigInt = readBigInt();
            if (!bigInt)
                return JSValue();
            ASSERT(bigInt.isBigInt());
            BigIntObject* obj = BigIntObject::create(m_lexicalGlobalObject->vm(), m_globalObject, bigInt);
            addToObjectPool<BigIntObjectTag>(obj);
            return obj;
        }
        case ErrorInstanceTag: {
            SerializableErrorType serializedErrorType;
            if (!readSerializableErrorType(serializedErrorType)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String message;
            if (!readNullableString(message)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t line;
            if (!read(line)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t column;
            if (!read(column)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String sourceURL;
            if (!readNullableString(sourceURL)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String stackString;
            if (!readNullableString(stackString)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String causeString;
            if (!readNullableString(causeString)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return ErrorInstance::create(m_lexicalGlobalObject, WTF::move(message), toErrorType(serializedErrorType), { line, column }, WTF::move(sourceURL), WTF::move(stackString), WTF::move(causeString));
        }
#if ENABLE(WEBASSEMBLY)
        case WasmModuleTag: {
            // https://webassembly.github.io/spec/web-api/index.html#serialization
            CachedStringRef agentClusterID;
            bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
            if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != static_cast<Derived*>(this)->agentClusterID()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_sideChannels.wasmModules || index >= m_sideChannels.wasmModules->size()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return JSWebAssemblyModule::create(m_lexicalGlobalObject->vm(), m_globalObject->webAssemblyModuleStructure(), m_sideChannels.wasmModules->at(index).copyRef());
        }
        case WasmMemoryTag: {
            CachedStringRef agentClusterID;
            bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
            if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != static_cast<Derived*>(this)->agentClusterID()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || !m_sideChannels.wasmMemoryHandles || index >= m_sideChannels.wasmMemoryHandles->size()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }

            bool isMemory64;
            if (!read(isMemory64)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            Wasm::AddressType addressType { isMemory64 };

            auto& vm = m_lexicalGlobalObject->vm();
            JSWebAssemblyMemory* result = JSWebAssemblyMemory::create(vm, m_globalObject->webAssemblyMemoryStructure());
            RefPtr<Wasm::Memory> memory;
            auto handler = [&vm, result] (Wasm::Memory::GrowSuccess, PageCount oldPageCount, PageCount newPageCount) { result->growSuccessCallback(vm, oldPageCount, newPageCount); };
            if (RefPtr<SharedArrayBufferContents> contents = m_sideChannels.wasmMemoryHandles->at(index)) {
                if (!contents->memoryHandle()) [[unlikely]] {
                    SERIALIZE_TRACE("FAIL deserialize");
                    fail();
                    return JSValue();
                }
                memory = Wasm::Memory::create(contents.releaseNonNull(), addressType, WTF::move(handler));
            } else {
                // zero size & max-size.
                memory = Wasm::Memory::createZeroSized(MemorySharingMode::Shared, addressType, WTF::move(handler));
            }

            result->adopt(memory.releaseNonNull());
            return result;
        }
#endif
        case ArrayBufferTag: {
            RefPtr<ArrayBuffer> arrayBuffer;
            if (!readArrayBuffer(arrayBuffer)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            Structure* structure = m_globalObject->arrayBufferStructure(arrayBuffer->sharingMode());
            // A crazy RuntimeFlags mismatch could mean that we are not equipped to handle shared
            // array buffers while the sender is. In that case, we would see a null structure here.
            if (!structure) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            JSValue result = static_cast<Derived*>(this)->toJSArrayBuffer(arrayBuffer.releaseNonNull());
            addToObjectPool<ArrayBufferTag>(result);
            return result;
        }
        case ResizableArrayBufferTag: {
            RefPtr<ArrayBuffer> arrayBuffer;
            if (!readResizableNonSharedArrayBuffer(arrayBuffer)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            Structure* structure = m_globalObject->arrayBufferStructure(arrayBuffer->sharingMode());
            // A crazy RuntimeFlags mismatch could mean that we are not equipped to handle shared
            // array buffers while the sender is. In that case, we would see a null structure here.
            if (!structure) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            JSValue result = static_cast<Derived*>(this)->toJSArrayBuffer(arrayBuffer.releaseNonNull());
            addToObjectPool<ResizableArrayBufferTag>(result);
            return result;
        }
        case ArrayBufferTransferTag: {
            uint32_t index;
            bool indexSuccessfullyRead = read(index);
            if (!indexSuccessfullyRead || index >= m_arrayBuffers.size()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize ArrayBufferTransferTag: indexSuccessfullyRead ", indexSuccessfullyRead, " index ", index, " m_arrayBuffers.size() ", m_arrayBuffers.size());
                fail();
                return JSValue();
            }

            if (!m_arrayBuffers[index])
                m_arrayBuffers[index] = ArrayBuffer::create(WTF::move(m_sideChannels.arrayBufferContents->at(index)));
            return static_cast<Derived*>(this)->toJSArrayBuffer(Ref { *m_arrayBuffers[index] });
        }
        case SharedArrayBufferTag: {
            // https://html.spec.whatwg.org/multipage/structured-data.html#structureddeserialize
            CachedStringRef agentClusterID;
            bool agentClusterIDSuccessfullyRead = readStringData(agentClusterID);
            uint32_t index = UINT_MAX;
            bool indexSuccessfullyRead = read(index);
            if (!agentClusterIDSuccessfullyRead || agentClusterID->string() != static_cast<Derived*>(this)->agentClusterID()
                || !indexSuccessfullyRead || !m_sideChannels.sharedBuffers || index >= m_sideChannels.sharedBuffers->size()) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }

            RELEASE_ASSERT(m_sideChannels.sharedBuffers->at(index));
            ArrayBufferContents arrayBufferContents;
            m_sideChannels.sharedBuffers->at(index).shareWith(arrayBufferContents);
            auto buffer = ArrayBuffer::create(WTF::move(arrayBufferContents));
            JSValue result = static_cast<Derived*>(this)->toJSArrayBuffer(WTF::move(buffer));
            addToObjectPool<SharedArrayBufferTag>(result);
            return result;
        }
        case ArrayBufferViewTag: {
            JSValue arrayBufferView;
            if (!readArrayBufferView(m_lexicalGlobalObject->vm(), arrayBufferView)) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            addToObjectPool<ArrayBufferViewTag>(arrayBufferView);
            return arrayBufferView;
        }
        case ObjectReferenceTag: {
            // The counterpart of CloneSerializer's writeObjectReferenceIfDupe(): a backreference to
            // an object already in the pool, which is how a graph with duplicate or cyclic
            // references is encoded.
            auto index = readConstantPoolIndex(m_objectPool);
            if (!index) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return m_objectPool.at(*index);
        }
        default:
            return static_cast<Derived*>(this)->readDerivedTerminal(tag);
        }
    }

    ALWAYS_INLINE JSValue readTerminal()
    {
        // Note: This can't be a requirement on the template as, in the common usage,
        // Derived will still be an incomplete type
        static_assert(StructuredCloneDeserializerHandler<Derived>,
            "Derived class must satisfy StructuredCloneDeserializerHandler");
        auto scope = DECLARE_THROW_SCOPE(m_lexicalGlobalObject->vm());
        if (!isSafeToRecurse()) [[unlikely]] {
            fail();
            return JSValue();
        }
        auto originalData = m_data;
        SerializationTag tag = readTag();
        if (!static_cast<Derived*>(this)->isTagExposed(tag)) [[unlikely]] {
            fail();
            return JSValue();
        }
        JSValue result = readTerminalImpl(tag);
        RETURN_IF_EXCEPTION(scope, { });
        if (!result) {
            SERIALIZE_TRACE("push back ", tag);
            m_data = originalData;
        }
        return result;
    }

    bool readSerializableErrorType(SerializableErrorType& errorType)
    {
        std::underlying_type_t<SerializableErrorType> errorTypeInt;
        if (!read(errorTypeInt) || errorTypeInt > std::to_underlying(SerializableErrorType::Last))
            return false;
        errorType = static_cast<SerializableErrorType>(errorTypeInt);
        return true;
    }

    void putProperty(JSObject* object, unsigned index, JSValue value)
    {
        object->putDirectIndex(m_lexicalGlobalObject, index, value);
    }

    void putProperty(JSObject* object, const Identifier& property, JSValue value)
    {
        object->putDirectMayBeIndex(m_lexicalGlobalObject, property, value);
    }

    template<SerializationTag Tag>
    bool consumeCollectionDataTerminationIfPossible()
    {
        auto originalData = m_data;
        if (readTag() == Tag)
            return true;
        m_data = originalData;
        return false;
    }

    enum class VisitNamedMemberResult : uint8_t { Error, Break, Start, Unknown };

    template<WalkerState endState>
    ALWAYS_INLINE VisitNamedMemberResult startVisitNamedMember(MarkedVector<JSObject*, 32>& outputObjectStack, Vector<Identifier, 16>& propertyNameStack, Vector<WalkerState, 16>& stateStack, JSValue& outValue)
    {
        static_assert(endState == WalkerState::ArrayEndVisitNamedMember || endState == WalkerState::ObjectEndVisitNamedMember);
        VM& vm = m_lexicalGlobalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        CachedStringRef cachedString;
        bool wasTerminator = false;
        if (!readStringData(cachedString, wasTerminator, ShouldAtomize::Yes)) {
            if (!wasTerminator) [[unlikely]] {
                SERIALIZE_TRACE("FAIL deserialize");
                return VisitNamedMemberResult::Error;
            }

            JSObject* outObject = outputObjectStack.last();
            outValue = outObject;
            outputObjectStack.removeLast();
            return VisitNamedMemberResult::Break;
        }

        Identifier identifier = Identifier::fromString(vm, cachedString->string());
        if constexpr (endState == WalkerState::ArrayEndVisitNamedMember)
            RELEASE_ASSERT(identifier != vm.propertyNames->length);

        JSValue terminal = readTerminal();
        RETURN_IF_EXCEPTION(scope, VisitNamedMemberResult::Error);
        if (terminal) {
            putProperty(outputObjectStack.last(), identifier, terminal);
            RETURN_IF_EXCEPTION(scope, VisitNamedMemberResult::Error);
            return VisitNamedMemberResult::Start;
        }

        stateStack.append(endState);
        propertyNameStack.append(identifier);
        return VisitNamedMemberResult::Unknown;
    }

    ALWAYS_INLINE void objectEndVisitNamedMember(MarkedVector<JSObject*, 32>& outputObjectStack, Vector<Identifier, 16>& propertyNameStack, JSValue& outValue)
    {
        VM& vm = m_lexicalGlobalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        putProperty(outputObjectStack.last(), propertyNameStack.last(), outValue);
        RETURN_IF_EXCEPTION(scope, void());
        propertyNameStack.removeLast();
    }

    DeserializationResult deserializationFailure()
    {
        SERIALIZE_TRACE("FAIL deserialize");
        fail();
        return { JSValue(), SerializationReturnCode::ValidationError };
    }

    DeserializationResult deserialize()
    {
        VM& vm = m_lexicalGlobalObject->vm();
        auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

        Vector<uint32_t, 16> indexStack;
        Vector<Identifier, 16> propertyNameStack;
        MarkedVector<JSObject*, 32> outputObjectStack;
        MarkedVector<JSValue, 4> mapKeyStack;
        MarkedVector<JSMap*, 4> mapStack;
        MarkedVector<JSSet*, 4> setStack;
        Vector<WalkerState, 16> stateStack;
        WalkerState state = WalkerState::StateUnknown;
        JSValue outValue;

        while (true) {
            switch (state) {
            arrayStartState:
            case WalkerState::ArrayStartState: {
                uint32_t length;
                if (!read(length)) [[unlikely]]
                    return deserializationFailure();
                JSArray* outArray = constructEmptyArray(m_globalObject, static_cast<JSC::ArrayAllocationProfile*>(nullptr), length);
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                addToObjectPool<ArrayTag>(outArray);
                outputObjectStack.append(outArray);
            }
            arrayStartVisitIndexedMember:
            [[fallthrough]];
            case WalkerState::ArrayStartVisitIndexedMember: {
                uint32_t index;
                if (!read(index)) [[unlikely]]
                    return deserializationFailure();

                if (m_majorVersion >= 15 || (m_majorVersion == 12 && m_minorVersion == 1)) {
                    if (index == TerminatorTag) {
                        // We reached the end of the indexed properties section.
                        if (!read(index)) [[unlikely]]
                            return deserializationFailure();
                        // At this point, we're either done with the array or is starting the
                        // non-indexed property section.
                        if (index == TerminatorTag) {
                            JSObject* outArray = outputObjectStack.last();
                            outValue = outArray;
                            outputObjectStack.removeLast();
                            break;
                        }
                        if (index == NonIndexPropertiesTag)
                            goto arrayStartVisitNamedMember;
                    }
                } else if (index == TerminatorTag) {
                    JSObject* outArray = outputObjectStack.last();
                    outValue = outArray;
                    outputObjectStack.removeLast();
                    break;
                } else if (index == NonIndexPropertiesTag)
                    goto arrayStartVisitNamedMember;

                JSValue terminal = readTerminal();
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                if (terminal) {
                    putProperty(outputObjectStack.last(), index, terminal);
                    RETURN_IF_EXCEPTION(scope, deserializationFailure());
                    goto arrayStartVisitIndexedMember;
                }
                if (m_failed) [[unlikely]]
                    return deserializationFailure();
                indexStack.append(index);
                stateStack.append(WalkerState::ArrayEndVisitIndexedMember);
                goto stateUnknown;
            }
            case WalkerState::ArrayEndVisitIndexedMember: {
                JSObject* outArray = outputObjectStack.last();
                putProperty(outArray, indexStack.last(), outValue);
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                indexStack.removeLast();
                goto arrayStartVisitIndexedMember;
            }
            arrayStartVisitNamedMember:
            case WalkerState::ArrayStartVisitNamedMember: {
                auto result = startVisitNamedMember<WalkerState::ArrayEndVisitNamedMember>(outputObjectStack, propertyNameStack, stateStack, outValue);
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                switch (result) {
                case VisitNamedMemberResult::Error: [[unlikely]]
                    return deserializationFailure();
                case VisitNamedMemberResult::Break:
                    break;
                case VisitNamedMemberResult::Start:
                    goto arrayStartVisitNamedMember;
                case VisitNamedMemberResult::Unknown:
                    goto stateUnknown;
                }
                break;
            }
            case WalkerState::ArrayEndVisitNamedMember: {
                objectEndVisitNamedMember(outputObjectStack, propertyNameStack, outValue);
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                goto arrayStartVisitNamedMember;
            }
            objectStartState:
            case WalkerState::ObjectStartState: {
                if (outputObjectStack.size() > maximumFilterRecursion)
                    return { JSValue(), SerializationReturnCode::StackOverflowError };
                JSObject* outObject = JSC::constructEmptyObject(m_lexicalGlobalObject, m_globalObject->objectPrototype());
                addToObjectPool<ObjectTag>(outObject);
                outputObjectStack.append(outObject);
            }
            startVisitNamedMember:
            [[fallthrough]];
            case WalkerState::ObjectStartVisitNamedMember: {
                auto result = startVisitNamedMember<WalkerState::ObjectEndVisitNamedMember>(outputObjectStack, propertyNameStack, stateStack, outValue);
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                switch (result) {
                case VisitNamedMemberResult::Error: [[unlikely]]
                    return deserializationFailure();
                case VisitNamedMemberResult::Break:
                    break;
                case VisitNamedMemberResult::Start:
                    goto startVisitNamedMember;
                case VisitNamedMemberResult::Unknown:
                    goto stateUnknown;
                }
                break;
            }
            case WalkerState::ObjectEndVisitNamedMember: {
                objectEndVisitNamedMember(outputObjectStack, propertyNameStack, outValue);
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                goto startVisitNamedMember;
            }
            mapStartState: {
                if (outputObjectStack.size() > maximumFilterRecursion) [[unlikely]] {
                    SERIALIZE_TRACE("FAIL deserialize");
                    return { JSValue(), SerializationReturnCode::StackOverflowError };
                }
                JSMap* map = JSMap::create(m_lexicalGlobalObject->vm(), m_globalObject->mapStructure());
                addToObjectPool<MapObjectTag>(map);
                outputObjectStack.append(map);
                mapStack.append(map);
                goto mapDataStartVisitEntry;
            }
            mapDataStartVisitEntry:
            case WalkerState::MapDataStartVisitEntry: {
                if (consumeCollectionDataTerminationIfPossible<NonMapPropertiesTag>()) {
                    mapStack.removeLast();
                    goto startVisitNamedMember;
                }
                stateStack.append(WalkerState::MapDataEndVisitKey);
                goto stateUnknown;
            }
            case WalkerState::MapDataEndVisitKey: {
                mapKeyStack.append(outValue);
                stateStack.append(WalkerState::MapDataEndVisitValue);
                goto stateUnknown;
            }
            case WalkerState::MapDataEndVisitValue: {
                mapStack.last()->set(m_lexicalGlobalObject, mapKeyStack.last(), outValue);
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                mapKeyStack.removeLast();
                goto mapDataStartVisitEntry;
            }

            setStartState: {
                if (outputObjectStack.size() > maximumFilterRecursion) [[unlikely]] {
                    SERIALIZE_TRACE("FAIL deserialize");
                    return { JSValue(), SerializationReturnCode::StackOverflowError };
                }
                JSSet* set = JSSet::create(m_lexicalGlobalObject->vm(), m_globalObject->setStructure());
                addToObjectPool<SetObjectTag>(set);
                outputObjectStack.append(set);
                setStack.append(set);
                goto setDataStartVisitEntry;
            }
            setDataStartVisitEntry:
            case WalkerState::SetDataStartVisitEntry: {
                if (consumeCollectionDataTerminationIfPossible<NonSetPropertiesTag>()) {
                    setStack.removeLast();
                    goto startVisitNamedMember;
                }
                stateStack.append(WalkerState::SetDataEndVisitKey);
                goto stateUnknown;
            }
            case WalkerState::SetDataEndVisitKey: {
                JSSet* set = setStack.last();
                set->add(m_lexicalGlobalObject, outValue);
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                goto setDataStartVisitEntry;
            }

            stateUnknown:
            case WalkerState::StateUnknown:
                JSValue terminal = readTerminal();
                RETURN_IF_EXCEPTION(scope, deserializationFailure());
                if (terminal) {
                    outValue = terminal;
                    break;
                }
                SerializationTag tag = readTag();
                if (tag == ArrayTag)
                    goto arrayStartState;
                if (tag == ObjectTag)
                    goto objectStartState;
                if (tag == MapObjectTag)
                    goto mapStartState;
                if (tag == SetObjectTag)
                    goto setStartState;
                return deserializationFailure();
            }
            if (stateStack.isEmpty())
                break;

            state = stateStack.last();
            stateStack.removeLast();
        }
        ASSERT(outValue);
        ASSERT(!m_failed);
        return { outValue, SerializationReturnCode::SuccessfullyCompleted };
    }

    JSGlobalObject* const m_globalObject;
    std::span<const uint8_t> m_data;
    Vector<CachedString> m_constantPool;
    unsigned m_majorVersion { 0xFFFFFFFFu };
    unsigned m_minorVersion { 0xFFFFFFFFu };
    CloneDeserializationSideChannels m_sideChannels;
    Vector<RefPtr<ArrayBuffer>> m_arrayBuffers;
};

inline JSValue CachedString::jsString(CloneBase& deserializer)
{
    if (!m_jsString) {
        auto& vm = deserializer.m_lexicalGlobalObject->vm();
        m_jsString = JSC::jsString(vm, m_string);
        deserializer.m_keepAliveBuffer.appendWithCrashOnOverflow(m_jsString);
    }
    return m_jsString;
}

} // namespace JSC
