/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "IntlRelativeTimeFormat.h"

#include "IntlNumberFormat.h"
#include "IntlObject.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

const ClassInfo IntlRelativeTimeFormat::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlRelativeTimeFormat) };

namespace IntlRelativeTimeFormatInternal {
constexpr const char* relevantExtensionKeys[1] = { "nu" };
}

void IntlRelativeTimeFormat::URelativeDateTimeFormatterDeleter::operator()(URelativeDateTimeFormatter* relativeDateTimeFormatter) const
{
    if (relativeDateTimeFormatter)
        ureldatefmt_close(relativeDateTimeFormatter);
}

void IntlRelativeTimeFormat::UNumberFormatDeleter::operator()(UNumberFormat* numberFormat) const
{
    if (numberFormat)
        unum_close(numberFormat);
}

IntlRelativeTimeFormat* IntlRelativeTimeFormat::create(VM& vm, Structure* structure)
{
    auto* format = new (NotNull, allocateCell<IntlRelativeTimeFormat>(vm.heap)) IntlRelativeTimeFormat(vm, structure);
    format->finishCreation(vm);
    return format;
}

Structure* IntlRelativeTimeFormat::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlRelativeTimeFormat::IntlRelativeTimeFormat(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlRelativeTimeFormat::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void IntlRelativeTimeFormat::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<IntlRelativeTimeFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
}

Vector<String> IntlRelativeTimeFormat::localeData(const String& locale, size_t keyIndex)
{
    // The index of the extension key "nu" in relevantExtensionKeys is 0.
    ASSERT_UNUSED(keyIndex, !keyIndex);
    return numberingSystemsForLocale(locale);
}

// https://tc39.es/ecma402/#sec-InitializeRelativeTimeFormat
void IntlRelativeTimeFormat::initializeRelativeTimeFormat(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options;
    if (optionsValue.isUndefined())
        options = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    else {
        options = optionsValue.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
    }

    HashMap<String, String> opt;
    String localeMatcher = intlStringOption(globalObject, options, vm.propertyNames->localeMatcher, { "lookup", "best fit" }, "localeMatcher must be either \"lookup\" or \"best fit\"", "best fit");
    RETURN_IF_EXCEPTION(scope, void());
    opt.add(vm.propertyNames->localeMatcher.string(), localeMatcher);

    String numberingSystem = intlStringOption(globalObject, options, vm.propertyNames->numberingSystem, { }, nullptr, nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!numberingSystem.isNull()) {
        if (!isUnicodeLocaleIdentifierType(numberingSystem)) {
            throwRangeError(globalObject, scope, "numberingSystem is not a well-formed numbering system value"_s);
            return;
        }
        opt.add("nu"_s, numberingSystem);
    }

    const HashSet<String>& availableLocales = intlRelativeTimeFormatAvailableLocales();
    HashMap<String, String> resolved = resolveLocale(globalObject, availableLocales, requestedLocales, opt, IntlRelativeTimeFormatInternal::relevantExtensionKeys, WTF_ARRAY_LENGTH(IntlRelativeTimeFormatInternal::relevantExtensionKeys), localeData);
    m_locale = resolved.get(vm.propertyNames->locale.string());
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize RelativeTimeFormat due to invalid locale"_s);
        return;
    }

    m_numberingSystem = resolved.get("nu"_s);
    CString dataLocaleWithExtensions = makeString(resolved.get("dataLocale"_s), "-u-nu-", m_numberingSystem).utf8();

    String style = intlStringOption(globalObject, options, vm.propertyNames->style, { "long", "short", "narrow" }, "style must be either \"long\", \"short\", or \"narrow\"", "long");
    RETURN_IF_EXCEPTION(scope, void());
    UDateRelativeDateTimeFormatterStyle icuStyle;
    if (style == "long") {
        icuStyle = UDAT_STYLE_LONG;
        m_style = Style::Long;
    } else if (style == "short") {
        icuStyle = UDAT_STYLE_SHORT;
        m_style = Style::Short;
    } else {
        ASSERT(style == "narrow");
        icuStyle = UDAT_STYLE_NARROW;
        m_style = Style::Narrow;
    }

    String numeric = intlStringOption(globalObject, options, vm.propertyNames->numeric, { "always", "auto" }, "numeric must be either \"always\" or \"auto\"", "always");
    RETURN_IF_EXCEPTION(scope, void());
    m_numeric = (numeric == "always");

    UErrorCode status = U_ZERO_ERROR;
    m_numberFormat = std::unique_ptr<UNumberFormat, UNumberFormatDeleter>(unum_open(UNUM_DECIMAL, nullptr, 0, dataLocaleWithExtensions.data(), nullptr, &status));
    if (UNLIKELY(U_FAILURE(status))) {
        throwTypeError(globalObject, scope, "failed to initialize RelativeTimeFormat"_s);
        return;
    }

    m_relativeDateTimeFormatter = std::unique_ptr<URelativeDateTimeFormatter, URelativeDateTimeFormatterDeleter>(ureldatefmt_open(dataLocaleWithExtensions.data(), nullptr, icuStyle, UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status));
    if (UNLIKELY(U_FAILURE(status))) {
        throwTypeError(globalObject, scope, "failed to initialize RelativeTimeFormat"_s);
        return;
    }
}

ASCIILiteral IntlRelativeTimeFormat::styleString(Style style)
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
    return ASCIILiteral::null();
}

// https://tc39.es/ecma402/#sec-intl.relativetimeformat.prototype.resolvedoptions
JSObject* IntlRelativeTimeFormat::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsNontrivialString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->style, jsNontrivialString(vm, styleString(m_style)));
    options->putDirect(vm, vm.propertyNames->numeric, jsNontrivialString(vm, m_numeric ? "always"_s : "auto"_s));
    options->putDirect(vm, vm.propertyNames->numberingSystem, jsNontrivialString(vm, m_numberingSystem));
    return options;
}

static StringView singularUnit(StringView unit)
{
    // Plurals are allowed, but thankfully they're all just a simple -s.
    return unit.endsWith("s") ? unit.left(unit.length() - 1) : unit;
}

// https://tc39.es/ecma402/#sec-singularrelativetimeunit
static Optional<URelativeDateTimeUnit> relativeTimeUnitType(StringView unit)
{
    StringView singular = singularUnit(unit);

    if (singular == "second")
        return UDAT_REL_UNIT_SECOND;
    if (singular == "minute")
        return UDAT_REL_UNIT_MINUTE;
    if (singular == "hour")
        return UDAT_REL_UNIT_HOUR;
    if (singular == "day")
        return UDAT_REL_UNIT_DAY;
    if (singular == "week")
        return UDAT_REL_UNIT_WEEK;
    if (singular == "month")
        return UDAT_REL_UNIT_MONTH;
    if (singular == "quarter")
        return UDAT_REL_UNIT_QUARTER;
    if (singular == "year")
        return UDAT_REL_UNIT_YEAR;

    return WTF::nullopt;
}

String IntlRelativeTimeFormat::formatInternal(JSGlobalObject* globalObject, double value, StringView unit) const
{
    ASSERT(m_relativeDateTimeFormatter);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!std::isfinite(value)) {
        throwRangeError(globalObject, scope, "number argument must be finite"_s);
        return String();
    }

    auto unitType = relativeTimeUnitType(unit);
    if (!unitType) {
        throwRangeError(globalObject, scope, "unit argument is not a recognized unit type"_s);
        return String();
    }

    auto formatRelativeTime = m_numeric ? ureldatefmt_formatNumeric : ureldatefmt_format;

    Vector<UChar, 32> result;
    auto status = callBufferProducingFunction(formatRelativeTime, m_relativeDateTimeFormatter.get(), value, unitType.value(), result);
    if (UNLIKELY(U_FAILURE(status))) {
        throwTypeError(globalObject, scope, "failed to format relative time"_s);
        return String();
    }

    return String(result);
}

// https://tc39.es/ecma402/#sec-FormatRelativeTime
JSValue IntlRelativeTimeFormat::format(JSGlobalObject* globalObject, double value, StringView unit) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String result = formatInternal(globalObject, value, unit);
    RETURN_IF_EXCEPTION(scope, { });

    return jsString(vm, result);
}

// https://tc39.es/ecma402/#sec-FormatRelativeTimeToParts
JSValue IntlRelativeTimeFormat::formatToParts(JSGlobalObject* globalObject, double value, StringView unit) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String formattedRelativeTime = formatInternal(globalObject, value, unit);
    RETURN_IF_EXCEPTION(scope, { });

    UErrorCode status = U_ZERO_ERROR;
    auto iterator = std::unique_ptr<UFieldPositionIterator, UFieldPositionIteratorDeleter>(ufieldpositer_open(&status));
    ASSERT(U_SUCCESS(status));

    double absValue = std::abs(value);

    Vector<UChar, 32> buffer;
    status = callBufferProducingFunction(unum_formatDoubleForFields, m_numberFormat.get(), absValue, buffer, iterator.get());
    if (U_FAILURE(status))
        return throwTypeError(globalObject, scope, "failed to format relative time"_s);

    auto formattedNumber = String(buffer);

    JSArray* parts = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!parts)
        return throwOutOfMemoryError(globalObject, scope);

    JSString* literalString = jsNontrivialString(vm, "literal"_s);

    // We only have one input number, so our relative time will have at most one numeric substring,
    // but we need to list all of the numeric parts separately.
    size_t numberEnd = 0;
    size_t numberStart = formattedRelativeTime.find(formattedNumber);
    if (numberStart != notFound) {
        numberEnd = numberStart + buffer.size();

        // Add initial literal if there is one.
        if (numberStart) {
            JSObject* part = constructEmptyObject(globalObject);
            part->putDirect(vm, vm.propertyNames->type, literalString);
            part->putDirect(vm, vm.propertyNames->value, jsSubstring(vm, formattedRelativeTime, 0, numberStart));
            parts->push(globalObject, part);
            RETURN_IF_EXCEPTION(scope, { });
        }

        IntlNumberFormat::formatToPartsInternal(globalObject, absValue, formattedNumber, iterator.get(), parts, jsString(vm, singularUnit(unit).toString()));
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Add final literal if there is one.
    auto stringLength = formattedRelativeTime.length();
    if (numberEnd != stringLength) {
        JSObject* part = constructEmptyObject(globalObject);
        part->putDirect(vm, vm.propertyNames->type, literalString);
        part->putDirect(vm, vm.propertyNames->value, jsSubstring(vm, formattedRelativeTime, numberEnd, stringLength - numberEnd));
        parts->push(globalObject, part);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return parts;
}

} // namespace JSC
