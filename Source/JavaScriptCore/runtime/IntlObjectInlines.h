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
#include <unicode/ucol.h>

namespace JSC {

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

        candidate = candidate.substring(0, pos);
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
        ASSERT(!callee->template inherits<JSBoundFunction>(vm));
        JSValue prototype = callee->getDirect(vm, vm.propertyNames->prototype); // Passed constructors always have `prototype` which cannot be deleted.
        ASSERT(prototype);
        bool hasInstance = JSObject::defaultHasInstance(globalObject, thisObject, prototype);
        RETURN_IF_EXCEPTION(scope, JSValue());
        if (hasInstance) {
            PropertyDescriptor descriptor(instance, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
            scope.release();
            thisObject->methodTable(vm)->defineOwnProperty(thisObject, globalObject, vm.propertyNames->builtinNames().intlLegacyConstructedSymbol(), descriptor, true);
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

    JSObject* thisObject = jsDynamicCast<JSObject*>(vm, thisValue);
    if (UNLIKELY(!thisObject))
        return nullptr;

    auto* instance = jsDynamicCast<InstanceType*>(vm, thisObject);
    if (LIKELY(instance))
        return instance;

    ASSERT(!constructor->template inherits<JSBoundFunction>(vm));
    JSValue prototype = constructor->getDirect(vm, vm.propertyNames->prototype); // Passed constructors always have `prototype` which cannot be deleted.
    ASSERT(prototype);
    bool hasInstance = JSObject::defaultHasInstance(globalObject, thisObject, prototype);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (!hasInstance)
        return nullptr;

    JSValue value = thisObject->get(globalObject, vm.propertyNames->builtinNames().intlLegacyConstructedSymbol());
    RETURN_IF_EXCEPTION(scope, nullptr);
    return jsDynamicCast<InstanceType*>(vm, value);
}

template<typename ResultType>
ResultType intlOption(JSGlobalObject* globalObject, JSValue options, PropertyName property, std::initializer_list<std::pair<ASCIILiteral, ResultType>> values, ASCIILiteral notFoundMessage, ResultType fallback)
{
    // GetOption (options, property, type="string", values, fallback)
    // https://tc39.github.io/ecma402/#sec-getoption

    ASSERT(values.size() > 0);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (options.isUndefined())
        return fallback;

    JSObject* opts = options.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue value = opts->get(globalObject, property);
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


ALWAYS_INLINE bool canUseASCIIUCADUCETComparison(UChar character)
{
    return isASCII(character) && ducetWeights[character];
}

template<typename CharacterType1, typename CharacterType2>
inline UCollationResult compareASCIIWithUCADUCET(const CharacterType1* characters1, unsigned length1, const CharacterType2* characters2, unsigned length2)
{
    unsigned commonLength = std::min(length1, length2);
    for (unsigned position = 0; position < commonLength; ++position) {
        auto lhs = characters1[position];
        auto rhs = characters2[position];
        ASSERT(canUseASCIIUCADUCETComparison(lhs));
        ASSERT(canUseASCIIUCADUCETComparison(rhs));
        uint8_t leftWeight = ducetWeights[lhs];
        uint8_t rightWeight = ducetWeights[rhs];
        if (leftWeight == rightWeight)
            continue;
        return leftWeight > rightWeight ? UCOL_GREATER : UCOL_LESS;
    }

    if (length1 == length2)
        return UCOL_EQUAL;
    return length1 > length2 ? UCOL_GREATER : UCOL_LESS;
}

} // namespace JSC
