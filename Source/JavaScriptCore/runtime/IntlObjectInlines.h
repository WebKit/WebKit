/*
 * Copyright (C) 2016 Yusuke Suzuki <yusuke.suzuki@sslab.ics.keio.ac.jp>
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BuiltinNames.h"
#include "IntlObject.h"
#include "JSBoundFunction.h"
#include "JSObject.h"
#include "ObjectConstructor.h"
#include <unicode/ucol.h>

namespace JSC {

template<typename StringType>
static constexpr uint32_t computeTwoCharacters16Code(const StringType& string)
{
    return static_cast<uint16_t>(string.characterAt(0)) | (static_cast<uint32_t>(static_cast<uint16_t>(string.characterAt(1))) << 16);
}

template<typename Predicate> String bestAvailableLocale(const String& locale, Predicate predicate)
{
    // BestAvailableLocale (availableLocales, locale)
    // https://tc39.github.io/ecma402/#sec-bestavailablelocale

    String candidate = locale;
    while (!candidate.isEmpty()) {
        if (predicate(candidate))
            return candidate;

        size_t pos = candidate.reverseFind('-');
        if (pos == notFound)
            return String();

        if (pos >= 2 && candidate[pos - 2] == '-')
            pos -= 2;

        candidate = candidate.left(pos);
    }

    return String();
}

template<typename Constructor, typename Factory>
JSValue constructIntlInstanceWithWorkaroundForLegacyIntlConstructor(JSGlobalObject* globalObject, JSValue thisValue, Constructor* callee, Factory factory)
{
    // FIXME: Workaround to provide compatibility with ECMA-402 1.0 call/apply patterns.
    // https://bugs.webkit.org/show_bug.cgi?id=153679
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* instance = factory(vm);
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (thisValue.isObject()) {
        JSObject* thisObject = asObject(thisValue);
        ASSERT(!callee->template inherits<JSBoundFunction>());
        JSValue prototype = callee->getDirect(vm, vm.propertyNames->prototype); // Passed constructors always have `prototype` which cannot be deleted.
        ASSERT(prototype);
        bool hasInstance = JSObject::defaultHasInstance(globalObject, thisObject, prototype);
        RETURN_IF_EXCEPTION(scope, JSValue());
        if (hasInstance) {
            PropertyDescriptor descriptor(instance, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
            scope.release();
            thisObject->methodTable()->defineOwnProperty(thisObject, globalObject, vm.propertyNames->builtinNames().intlLegacyConstructedSymbol(), descriptor, true);
            return thisObject;
        }
    }
    return instance;
}

template<typename InstanceType>
InstanceType* unwrapForLegacyIntlConstructor(JSGlobalObject* globalObject, JSValue thisValue, JSObject* constructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = jsDynamicCast<JSObject*>(thisValue);
    if (UNLIKELY(!thisObject))
        return nullptr;

    auto* instance = jsDynamicCast<InstanceType*>(thisObject);
    if (LIKELY(instance))
        return instance;

    ASSERT(!constructor->template inherits<JSBoundFunction>());
    JSValue prototype = constructor->getDirect(vm, vm.propertyNames->prototype); // Passed constructors always have `prototype` which cannot be deleted.
    ASSERT(prototype);
    bool hasInstance = JSObject::defaultHasInstance(globalObject, thisObject, prototype);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!hasInstance)
        return nullptr;

    JSValue value = thisObject->get(globalObject, vm.propertyNames->builtinNames().intlLegacyConstructedSymbol());
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsDynamicCast<InstanceType*>(value);
}

template<typename ResultType>
ResultType intlOption(JSGlobalObject* globalObject, JSObject* options, PropertyName property, std::initializer_list<std::pair<ASCIILiteral, ResultType>> values, ASCIILiteral notFoundMessage, ResultType fallback)
{
    // GetOption (options, property, type="string", values, fallback)
    // https://tc39.github.io/ecma402/#sec-getoption

    ASSERT(values.size() > 0);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!options)
        return fallback;

    JSValue value = options->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, { });

    if (!value.isUndefined()) {
        String stringValue = value.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        for (const auto& entry : values) {
            if (entry.first == stringValue)
                return entry.second;
        }
        throwException(globalObject, scope, createRangeError(globalObject, notFoundMessage));
        return { };
    }

    return fallback;
}

template<typename ResultType>
ResultType intlStringOrBooleanOption(JSGlobalObject* globalObject, JSObject* options, PropertyName property, ResultType trueValue, ResultType falsyValue, std::initializer_list<std::pair<ASCIILiteral, ResultType>> values, ASCIILiteral notFoundMessage, ResultType fallback)
{
    // https://tc39.es/proposal-intl-numberformat-v3/out/negotiation/diff.html#sec-getstringorbooleanoption

    ASSERT(values.size() > 0);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!options)
        return fallback;

    JSValue value = options->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, { });

    if (value.isUndefined())
        return fallback;

    if (value.isBoolean() && value.asBoolean())
        return trueValue;

    bool valueBoolean = value.toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (!valueBoolean)
        return falsyValue;

    String stringValue = value.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // FIXME: We need to know whether this fallback is actually correct.
    // https://github.com/tc39/proposal-intl-numberformat-v3/issues/111
    if (stringValue == "true"_s || stringValue == "false"_s)
        return fallback;

    for (const auto& entry : values) {
        if (entry.first == stringValue)
            return entry.second;
    }

    throwRangeError(globalObject, scope, notFoundMessage);
    return { };
}

ALWAYS_INLINE bool canUseASCIIUCADUCETComparison(UChar character)
{
    return isASCII(character) && ducetLevel1Weights[character];
}

ALWAYS_INLINE bool canUseASCIIUCADUCETComparison(LChar character)
{
    return ducetLevel1Weights[character];
}

ALWAYS_INLINE bool followedByNonLatinCharacter(const UChar* characters, unsigned length, unsigned index)
{
    unsigned nextIndex = index + 1;
    if (length > nextIndex)
        return !isLatin1(characters[nextIndex]);
    return false;
}

ALWAYS_INLINE bool followedByNonLatinCharacter(const LChar*, unsigned, unsigned)
{
    return false;
}

template<typename CharacterType1, typename CharacterType2>
UCollationResult compareASCIIWithUCADUCETLevel3(const CharacterType1* characters1, const CharacterType2* characters2, unsigned length)
{
    for (unsigned position = 0; position < length; ++position) {
        auto lhs = characters1[position];
        auto rhs = characters2[position];
        uint8_t leftWeight = ducetLevel3Weights[lhs];
        uint8_t rightWeight = ducetLevel3Weights[rhs];
        if (leftWeight == rightWeight)
            continue;
        return leftWeight > rightWeight ? UCOL_GREATER : UCOL_LESS;
    }
    return UCOL_EQUAL;
}

template<typename CharacterType1, typename CharacterType2>
inline std::optional<UCollationResult> compareASCIIWithUCADUCET(const CharacterType1* characters1, unsigned length1, const CharacterType2* characters2, unsigned length2)
{
    if (length1 == length2) {
        if (equal(characters1, characters2, length1))
            return UCOL_EQUAL;
    }

    unsigned commonLength = std::min(length1, length2);
    for (unsigned position = 0; position < commonLength; ++position) {
        auto lhs = characters1[position];
        auto rhs = characters2[position];

        if (!canUseASCIIUCADUCETComparison(lhs) || !canUseASCIIUCADUCETComparison(rhs))
            return std::nullopt;

        uint8_t leftWeight = ducetLevel1Weights[lhs];
        uint8_t rightWeight = ducetLevel1Weights[rhs];
        if (leftWeight == rightWeight)
            continue;

        // If the following character is a non-latin, then it is possible that current and next characters can be combined into different character.
        if (followedByNonLatinCharacter(characters1, length1, position) || followedByNonLatinCharacter(characters2, length2, position))
            return std::nullopt;

        return leftWeight > rightWeight ? UCOL_GREATER : UCOL_LESS;
    }

    if (length1 == length2)
        return compareASCIIWithUCADUCETLevel3(characters1, characters2, length1);

    // If the next character is valid, then we do not need to look into the rest of characters.
    if (length1 > length2) {
        auto lhs = characters1[length2];
        if (!canUseASCIIUCADUCETComparison(lhs))
            return std::nullopt;
        return UCOL_GREATER;
    }

    auto rhs = characters2[length1];
    if (!canUseASCIIUCADUCETComparison(rhs))
        return std::nullopt;
    return UCOL_LESS;
}

// https://tc39.es/ecma402/#sec-getoptionsobject
inline JSObject* intlGetOptionsObject(JSGlobalObject* globalObject, JSValue options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (options.isUndefined())
        return nullptr;
    if (LIKELY(options.isObject()))
        return asObject(options);
    throwTypeError(globalObject, scope, "options argument is not an object or undefined"_s);
    return nullptr;
}

// https://tc39.es/ecma402/#sec-coerceoptionstoobject
inline JSObject* intlCoerceOptionsToObject(JSGlobalObject* globalObject, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (optionsValue.isUndefined())
        return nullptr;
    JSObject* options = optionsValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    return options;
}

template<typename Container>
JSArray* createArrayFromStringVector(JSGlobalObject* globalObject, const Container& elements)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), elements.size());
    if (!result) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    for (unsigned index = 0; index < elements.size(); ++index) {
        result->putDirectIndex(globalObject, index, jsString(vm, elements[index]));
        RETURN_IF_EXCEPTION(scope, { });
    }
    return result;
}

template<typename Container>
JSArray* createArrayFromIntVector(JSGlobalObject* globalObject, const Container& elements)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), elements.size());
    if (!result) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }
    for (unsigned index = 0; index < elements.size(); ++index) {
        result->putDirectIndex(globalObject, index, jsNumber(elements[index]));
        RETURN_IF_EXCEPTION(scope, { });
    }
    return result;
}

class ListFormatInput {
    WTF_MAKE_NONCOPYABLE(ListFormatInput);
public:
    ListFormatInput(Vector<String, 4>&& strings)
        : m_strings(WTFMove(strings))
    {
        m_stringPointers.reserveInitialCapacity(m_strings.size());
        m_stringLengths.reserveInitialCapacity(m_strings.size());
        for (auto& string : m_strings) {
            string.convertTo16Bit();
            m_stringPointers.append(string.characters16());
            m_stringLengths.append(string.length());
        }
    }

    int32_t size() const { return m_stringPointers.size(); }
    const UChar* const* stringPointers() const { return m_stringPointers.data(); }
    const int32_t* stringLengths() const { return m_stringLengths.data(); }

private:
    Vector<String, 4> m_strings;
    Vector<const UChar*, 4> m_stringPointers;
    Vector<int32_t, 4> m_stringLengths;
};

} // namespace JSC
