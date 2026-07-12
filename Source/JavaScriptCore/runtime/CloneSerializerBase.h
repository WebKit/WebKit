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
#include <JavaScriptCore/Identifier.h>
#include <JavaScriptCore/JSArray.h>
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
#include <JavaScriptCore/YarrFlags.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace JSC {

template<typename Derived>
concept StructuredCloneSerializerHandler = requires(Derived& d, JSObject* obj, SerializationReturnCode& code) {
    { d.dumpDerivedTerminal(obj, code) } -> std::same_as<bool>;
};

namespace StructuredCloneInternal {

#if JSC_ASSUME_LITTLE_ENDIAN
template<typename T> inline void writeLittleEndian(Vector<uint8_t>& buffer, T value)
{
    buffer.append(asByteSpan(value));
}
#else
template<typename T> inline void writeLittleEndian(Vector<uint8_t>& buffer, T value)
{
    for (unsigned i = 0; i < sizeof(T); i++) {
        buffer.append(value & 0xFF);
        value >>= 8;
    }
}
#endif

template<> inline void writeLittleEndian<uint8_t>(Vector<uint8_t>& buffer, uint8_t value)
{
    buffer.append(value);
}

template<typename T> inline bool writeLittleEndian(Vector<uint8_t>& buffer, std::span<const T> values)
{
    if (values.size() > std::numeric_limits<uint32_t>::max() / sizeof(T))
        return false;

#if JSC_ASSUME_LITTLE_ENDIAN
    buffer.append(asBytes(values));
#else
    for (unsigned i = 0; i < values.size(); i++) {
        T value = values[i];
        for (unsigned j = 0; j < sizeof(T); j++) {
            buffer.append(static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
    }
#endif
    return true;
}

template<> inline bool writeLittleEndian<uint8_t>(Vector<uint8_t>& buffer, std::span<const uint8_t> values)
{
    buffer.append(values);
    return true;
}

} // namespace StructuredCloneInternal

template<typename Derived>
class CloneSerializerBase : public CloneBase {
protected:
    using ObjectPoolMap = HashMap<JSObject*, uint32_t>;
    using StringConstantPool = HashMap<RefPtr<AtomStringImpl>, uint32_t, IdentifierRepHash>;

    CloneSerializerBase(JSGlobalObject* lexicalGlobalObject, Vector<uint8_t>& buffer)
        : CloneBase(lexicalGlobalObject)
        , m_buffer(buffer)
    {
    }

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

    ALWAYS_INLINE bool dumpIfTerminal(JSValue value, SerializationReturnCode& code)
    {
        // Note: This can't be a requirement on the template as, in the common usage,
        // Derived will still be an incomplete type
        static_assert(StructuredCloneSerializerHandler<Derived>,
            "Derived class must satisfy StructuredCloneSerializerHandler");
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
            dumpString(asString(value)->value(m_lexicalGlobalObject));
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
            write(tag);
            appendObjectPoolTag(tag);
            return true;
        }
        if (auto* stringObject = dynamicDowncast<StringObject>(obj)) {
            if (!addToObjectPoolIfNotDupe<EmptyStringObjectTag, StringObjectTag>(stringObject))
                return true;
            auto str = asString(stringObject->internalValue())->value(m_lexicalGlobalObject);
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
        if (auto* errorInstance = dynamicDowncast<ErrorInstance>(obj)) {
            auto errorInformation = extractErrorInformationFromErrorInstance(m_lexicalGlobalObject, *errorInstance);
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

        // The walker descends into JSArray/JSMap/JSSet; never let Derived claim
        // them as terminals. Plain objects (JSFinalObject / ObjectPrototype) fall
        // through to dumpDerivedTerminal so DOM types may opt in, with the walker
        // catching anything Derived didn't claim.
        if (is<JSArray>(*obj) || is<JSMap>(*obj) || is<JSSet>(*obj))
            return false;

        return static_cast<Derived*>(this)->dumpDerivedTerminal(obj, code);
    }

    void endObject()
    {
        write(TerminatorTag);
    }

    JSValue getProperty(JSObject* object, const Identifier& propertyName)
    {
        PropertySlot slot(object, PropertySlot::InternalMethodType::Get);
        if (object->methodTable()->getOwnPropertySlot(object, m_lexicalGlobalObject, propertyName, slot))
            return slot.getValue(m_lexicalGlobalObject, propertyName);
        return JSValue();
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
                    if (scope.exception()) [[unlikely]]
                        return SerializationReturnCode::ExistingExceptionError;
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
                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;
                if (!inValue) {
                    indexStack.last()++;
                    goto arrayStartVisitIndexedMember;
                }

                write(index);
                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                if (dumpIfTerminal(inValue, terminalCode)) {
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
                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;
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
                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;

                if (!inValue) {
                    // Property was removed during serialisation
                    indexStack.last()++;
                    goto startVisitNamedMember;
                }
                write(properties[index].string());

                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;

                auto terminalCode = SerializationReturnCode::SuccessfullyCompleted;
                if (!dumpIfTerminal(inValue, terminalCode)) {
                    stateStack.append(WalkerState::ObjectEndVisitNamedMember);
                    goto stateUnknown;
                }
                if (terminalCode != SerializationReturnCode::SuccessfullyCompleted)
                    return terminalCode;
                [[fallthrough]];
            }
            case WalkerState::ObjectEndVisitNamedMember: {
                if (scope.exception()) [[unlikely]]
                    return SerializationReturnCode::ExistingExceptionError;

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
                    if (scope.exception()) [[unlikely]]
                        return SerializationReturnCode::ExistingExceptionError;
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
                    if (scope.exception()) [[unlikely]]
                        return SerializationReturnCode::ExistingExceptionError;
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
                if (dumpIfTerminal(inValue, terminalCode)) {
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
};

} // namespace JSC
