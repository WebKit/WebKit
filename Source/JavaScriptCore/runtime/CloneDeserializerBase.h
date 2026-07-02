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

#include <JavaScriptCore/BigIntObject.h>
#include <JavaScriptCore/BooleanObject.h>
#include <JavaScriptCore/CloneBase.h>
#include <JavaScriptCore/DateInstance.h>
#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/NumberObject.h>
#include <JavaScriptCore/RegExpObject.h>
#include <JavaScriptCore/StringObject.h>
#include <JavaScriptCore/YarrFlags.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace JSC {

template<typename Derived>
concept StructuredCloneDeserializerHandler = requires(Derived& d, SerializationTag t) {
    { d.readDerivedTerminal(t) } -> std::same_as<JSValue>;
    { d.isTagExposed(t) } -> std::same_as<bool>;
};

namespace StructuredCloneInternal {

#if JSC_ASSUME_LITTLE_ENDIAN
template<typename T> inline bool readLittleEndian(std::span<const uint8_t>& span, T& value)
{
    if (span.size() < sizeof(value))
        return false;
    value = consumeAndReinterpretCastTo<const T>(span);
    return true;
}
#else
template<typename T> inline bool readLittleEndian(std::span<const uint8_t>& span, T& value)
{
    if (span.size() < sizeof(value))
        return false;

    if constexpr (sizeof(T) == 1)
        value = consume(span);
    else {
        value = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
            value += static_cast<T>(span[i]) << (i * 8);
        skip(span, sizeof(T));
    }
    return true;
}
#endif

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
    CloneDeserializerBase(JSGlobalObject* lexicalGlobalObject, JSGlobalObject* globalObject, std::span<const uint8_t> data)
        : CloneBase(lexicalGlobalObject)
        , m_globalObject(globalObject)
        , m_data(data)
    {
    }

    template<typename T> bool readLittleEndian(T& value)
    {
        if (m_failed || !StructuredCloneInternal::readLittleEndian(m_data, value)) {
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
        if (m_data.empty()) {
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

#if JSC_ASSUME_LITTLE_ENDIAN
        auto stringSpan = consumeSpan(span, size);
        if (shouldAtomize == ShouldAtomize::Yes)
            str = AtomString(spanReinterpretCast<const char16_t>(stringSpan));
        else
            str = String(spanReinterpretCast<const char16_t>(stringSpan));
#else
        std::span<char16_t> characters;
        str = String::createUninitialized(length, characters);
        for (unsigned i = 0; i < length; ++i) {
            uint16_t c;
            StructuredCloneInternal::readLittleEndian(span, c);
            characters[i] = c;
        }
        if (shouldAtomize == ShouldAtomize::Yes)
            str = AtomString { str };
#endif
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
            if (!index || *index >= m_constantPool.size()) {
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
        if (!readString(m_data, str, length, is8Bit, shouldAtomize)) {
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
            if (!bigInt) {
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
            if (actualBigIntLength.hasOverflowed()) {
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
        if (!bigInt) {
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
            if (!readSerializableErrorType(serializedErrorType)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String message;
            if (!readNullableString(message)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t line;
            if (!read(line)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            uint32_t column;
            if (!read(column)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String sourceURL;
            if (!readNullableString(sourceURL)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String stackString;
            if (!readNullableString(stackString)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            String causeString;
            if (!readNullableString(causeString)) {
                SERIALIZE_TRACE("FAIL deserialize");
                fail();
                return JSValue();
            }
            return ErrorInstance::create(m_lexicalGlobalObject, WTF::move(message), toErrorType(serializedErrorType), { line, column }, WTF::move(sourceURL), WTF::move(stackString), WTF::move(causeString));
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
        if (!isSafeToRecurse()) {
            fail();
            return JSValue();
        }
        auto originalData = m_data;
        SerializationTag tag = readTag();
        if (!static_cast<Derived*>(this)->isTagExposed(tag)) {
            fail();
            return JSValue();
        }
        JSValue result = readTerminalImpl(tag);
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

    JSGlobalObject* const m_globalObject;
    std::span<const uint8_t> m_data;
    Vector<CachedString> m_constantPool;
    unsigned m_majorVersion { 0xFFFFFFFFu };
    unsigned m_minorVersion { 0xFFFFFFFFu };
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
