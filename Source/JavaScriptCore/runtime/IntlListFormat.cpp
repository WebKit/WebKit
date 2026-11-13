/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#include "config.h"
#include "IntlListFormat.h"

#include "IntlObjectInlines.h"
#include "IntlPartObject.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo IntlListFormat::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlListFormat) };

IntlListFormat* IntlListFormat::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlListFormat>(vm)) IntlListFormat(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlListFormat::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlListFormat::IntlListFormat(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat
void IntlListFormat::initializeListFormat(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options = intlGetOptionsObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, void());

    ResolveLocaleOptions localeOptions;

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, void());

    auto localeData = [](const String&, RelevantExtensionKey) -> Vector<String> {
        return { };
    };

    const auto& availableLocales = intlListFormatAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { }, localeData);

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize ListFormat due to invalid locale"_s);
        return;
    }

    m_type = intlOption<Type>(globalObject, options, vm.propertyNames->type, { { "conjunction"_s, Type::Conjunction }, { "disjunction"_s, Type::Disjunction }, { "unit"_s, Type::Unit } }, "type must be either \"conjunction\", \"disjunction\", or \"unit\""_s, Type::Conjunction);
    RETURN_IF_EXCEPTION(scope, void());

    m_style = intlOption<Style>(globalObject, options, vm.propertyNames->style, { { "long"_s, Style::Long }, { "short"_s, Style::Short }, { "narrow"_s, Style::Narrow } }, "style must be either \"long\", \"short\", or \"narrow\""_s, Style::Long);
    RETURN_IF_EXCEPTION(scope, void());

    m_listFormat = ListFormatter { m_locale.ascii(), m_type, m_style };
    if (!m_listFormat) {
        throwTypeError(globalObject, scope, "failed to initialize ListFormat"_s);
        return;
    }
}

static Vector<String, 4> stringListFromIterable(JSGlobalObject* globalObject, JSValue iterable)
{
    Vector<String, 4> result;

    if (iterable.isUndefined())
        return result;

    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        if (!value.isString()) {
            throwTypeError(globalObject, scope, "Iterable passed to ListFormat includes non String"_s);
            return;
        }
        String item = value.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        result.append(item);
    });
    return result;
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat.prototype.format
JSValue IntlListFormat::format(JSGlobalObject* globalObject, JSValue list) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const auto stringList = stringListFromIterable(globalObject, list);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = m_listFormat.format(stringList.span());
    if (!result)
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

    return jsString(vm, WTF::move(*result));
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat.prototype.formatToParts
JSValue IntlListFormat::formatToParts(JSGlobalObject* globalObject, JSValue list) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    const auto stringList = stringListFromIterable(globalObject, list);
    RETURN_IF_EXCEPTION(scope, { });

    auto formattedList = m_listFormat.formatToParts(stringList.span());
    if (!formattedList)
        return throwTypeError(globalObject, scope, "failed to format list of strings"_s);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    StringView resultStringView = formattedList->string;

    auto literalString = jsNontrivialString(vm, "literal"_s);
    auto elementString = jsNontrivialString(vm, "element"_s);

    auto createPart = [&] (JSString* type, JSString* value) {
        return createIntlPartObject(globalObject, type, value);
    };

    unsigned resultLength = resultStringView.length();
    unsigned previousEndIndex = 0;
    // FIXME: Empty strings should get empty element parts, but ICU reports no field for them.
    for (auto range : formattedList->elementRanges) {
        auto beginIndex = range.begin();
        auto endIndex = range.end();
        if (previousEndIndex < beginIndex) {
            auto value = jsString(vm, resultStringView.substring(previousEndIndex, beginIndex - previousEndIndex));
            JSObject* part = createPart(literalString, value);
            parts->putDirectIndex(globalObject, parts->length(), part);
            RETURN_IF_EXCEPTION(scope, { });
        }
        previousEndIndex = endIndex;

        auto value = jsString(vm, resultStringView.substring(beginIndex, endIndex - beginIndex));
        JSObject* part = createPart(elementString, value);
        parts->putDirectIndex(globalObject, parts->length(), part);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (previousEndIndex < resultLength) {
        auto value = jsString(vm, resultStringView.substring(previousEndIndex, resultLength - previousEndIndex));
        JSObject* part = createPart(literalString, value);
        parts->putDirectIndex(globalObject, parts->length(), part);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return parts;
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat.prototype.resolvedOptions
JSObject* IntlListFormat::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->type, jsNontrivialString(vm, typeString(m_type)));
    options->putDirect(vm, vm.propertyNames->style, jsNontrivialString(vm, styleString(m_style)));
    return options;
}

ASCIILiteral IntlListFormat::styleString(Style style)
{
    switch (style) {
    case Style::Long:
        return "long"_s;
    case Style::Short:
        return "short"_s;
    case Style::Narrow:
        return "narrow"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

ASCIILiteral IntlListFormat::typeString(Type type)
{
    switch (type) {
    case Type::Conjunction:
        return "conjunction"_s;
    case Type::Disjunction:
        return "disjunction"_s;
    case Type::Unit:
        return "unit"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
}

} // namespace JSC
