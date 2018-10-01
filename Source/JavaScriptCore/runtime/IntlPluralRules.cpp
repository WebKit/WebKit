/*
 * Copyright (C) 2018 Andy VanWagoner (andy@vanwagoner.family)
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
#include "IntlPluralRules.h"

#if ENABLE(INTL)

#include "Error.h"
#include "IntlObject.h"
#include "IntlPluralRulesConstructor.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo IntlPluralRules::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlPluralRules) };

void IntlPluralRules::UPluralRulesDeleter::operator()(UPluralRules* pluralRules) const
{
    if (pluralRules)
        uplrules_close(pluralRules);
}

void IntlPluralRules::UNumberFormatDeleter::operator()(UNumberFormat* numberFormat) const
{
    if (numberFormat)
        unum_close(numberFormat);
}

struct UEnumerationDeleter {
    void operator()(UEnumeration* enumeration) const
    {
        if (enumeration)
            uenum_close(enumeration);
    }
};

IntlPluralRules* IntlPluralRules::create(VM& vm, Structure* structure)
{
    IntlPluralRules* format = new (NotNull, allocateCell<IntlPluralRules>(vm.heap)) IntlPluralRules(vm, structure);
    format->finishCreation(vm);
    return format;
}

Structure* IntlPluralRules::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlPluralRules::IntlPluralRules(VM& vm, Structure* structure)
    : JSDestructibleObject(vm, structure)
{
}

void IntlPluralRules::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void IntlPluralRules::destroy(JSCell* cell)
{
    static_cast<IntlPluralRules*>(cell)->IntlPluralRules::~IntlPluralRules();
}

void IntlPluralRules::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlPluralRules* thisObject = jsCast<IntlPluralRules*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
}

namespace IntlPRInternal {
static Vector<String> localeData(const String&, size_t)
{
    Vector<String> data;
    return data;
}
}

void IntlPluralRules::initializePluralRules(ExecState& exec, JSValue locales, JSValue optionsValue)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.1.1 InitializePluralRules (pluralRules, locales, options)
    // https://tc39.github.io/ecma402/#sec-initializepluralrules
    Vector<String> requestedLocales = canonicalizeLocaleList(exec, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSObject* options;
    if (optionsValue.isUndefined())
        options = constructEmptyObject(&exec, exec.lexicalGlobalObject()->nullPrototypeObjectStructure());
    else {
        options = optionsValue.toObject(&exec);
        RETURN_IF_EXCEPTION(scope, void());
    }

    HashMap<String, String> localeOpt;
    String localeMatcher = intlStringOption(exec, options, vm.propertyNames->localeMatcher, { "lookup", "best fit" }, "localeMatcher must be either \"lookup\" or \"best fit\"", "best fit");
    RETURN_IF_EXCEPTION(scope, void());
    localeOpt.add(vm.propertyNames->localeMatcher.string(), localeMatcher);

    const HashSet<String> availableLocales = exec.jsCallee()->globalObject(vm)->intlNumberFormatAvailableLocales();
    HashMap<String, String> resolved = resolveLocale(exec, availableLocales, requestedLocales, localeOpt, nullptr, 0, IntlPRInternal::localeData);
    m_locale = resolved.get(vm.propertyNames->locale.string());
    if (m_locale.isEmpty()) {
        throwTypeError(&exec, scope, "failed to initialize PluralRules due to invalid locale"_s);
        return;
    }

    String typeString = intlStringOption(exec, options, Identifier::fromString(&vm, "type"), { "cardinal", "ordinal" }, "type must be \"cardinal\" or \"ordinal\"", "cardinal");
    RETURN_IF_EXCEPTION(scope, void());
    m_type = typeString == "ordinal" ? UPLURAL_TYPE_ORDINAL : UPLURAL_TYPE_CARDINAL;

    unsigned minimumIntegerDigits = intlNumberOption(exec, options, Identifier::fromString(&vm, "minimumIntegerDigits"), 1, 21, 1);
    RETURN_IF_EXCEPTION(scope, void());
    m_minimumIntegerDigits = minimumIntegerDigits;

    unsigned minimumFractionDigitsDefault = 0;
    unsigned minimumFractionDigits = intlNumberOption(exec, options, Identifier::fromString(&vm, "minimumFractionDigits"), 0, 20, minimumFractionDigitsDefault);
    RETURN_IF_EXCEPTION(scope, void());
    m_minimumFractionDigits = minimumFractionDigits;

    unsigned maximumFractionDigitsDefault = std::max(minimumFractionDigits, 3u);
    unsigned maximumFractionDigits = intlNumberOption(exec, options, Identifier::fromString(&vm, "maximumFractionDigits"), minimumFractionDigits, 20, maximumFractionDigitsDefault);
    RETURN_IF_EXCEPTION(scope, void());
    m_maximumFractionDigits = maximumFractionDigits;

    JSValue minimumSignificantDigitsValue = options->get(&exec, Identifier::fromString(&vm, "minimumSignificantDigits"));
    RETURN_IF_EXCEPTION(scope, void());

    JSValue maximumSignificantDigitsValue = options->get(&exec, Identifier::fromString(&vm, "maximumSignificantDigits"));
    RETURN_IF_EXCEPTION(scope, void());

    if (!minimumSignificantDigitsValue.isUndefined() || !maximumSignificantDigitsValue.isUndefined()) {
        unsigned minimumSignificantDigits = intlNumberOption(exec, options, Identifier::fromString(&vm, "minimumSignificantDigits"), 1, 21, 1);
        RETURN_IF_EXCEPTION(scope, void());
        unsigned maximumSignificantDigits = intlNumberOption(exec, options, Identifier::fromString(&vm, "maximumSignificantDigits"), minimumSignificantDigits, 21, 21);
        RETURN_IF_EXCEPTION(scope, void());
        m_minimumSignificantDigits = minimumSignificantDigits;
        m_maximumSignificantDigits = maximumSignificantDigits;
    }

    UErrorCode status = U_ZERO_ERROR;
    m_numberFormat = std::unique_ptr<UNumberFormat, UNumberFormatDeleter>(unum_open(UNUM_DECIMAL, nullptr, 0, m_locale.utf8().data(), nullptr, &status));
    if (U_FAILURE(status)) {
        throwTypeError(&exec, scope, "failed to initialize PluralRules"_s);
        return;
    }

    if (m_minimumSignificantDigits) {
        unum_setAttribute(m_numberFormat.get(), UNUM_SIGNIFICANT_DIGITS_USED, true);
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_SIGNIFICANT_DIGITS, m_minimumSignificantDigits.value());
        unum_setAttribute(m_numberFormat.get(), UNUM_MAX_SIGNIFICANT_DIGITS, m_maximumSignificantDigits.value());
    } else {
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_INTEGER_DIGITS, m_minimumIntegerDigits);
        unum_setAttribute(m_numberFormat.get(), UNUM_MIN_FRACTION_DIGITS, m_minimumFractionDigits);
        unum_setAttribute(m_numberFormat.get(), UNUM_MAX_FRACTION_DIGITS, m_maximumFractionDigits);
    }

    status = U_ZERO_ERROR;
    m_pluralRules = std::unique_ptr<UPluralRules, UPluralRulesDeleter>(uplrules_openForType(m_locale.utf8().data(), m_type, &status));
    if (U_FAILURE(status)) {
        throwTypeError(&exec, scope, "failed to initialize PluralRules"_s);
        return;
    }

    m_initializedPluralRules = true;
}

JSObject* IntlPluralRules::resolvedOptions(ExecState& exec)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.4.4 Intl.PluralRules.prototype.resolvedOptions ()
    // https://tc39.github.io/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
    if (!m_initializedPluralRules)
        return throwTypeError(&exec, scope, "Intl.PluralRules.prototype.resolvedOptions called on value that's not an object initialized as a PluralRules"_s);

    JSObject* options = constructEmptyObject(&exec);
    options->putDirect(vm, vm.propertyNames->locale, jsNontrivialString(&exec, m_locale));
    options->putDirect(vm, Identifier::fromString(&vm, "type"), jsNontrivialString(&exec, m_type == UPLURAL_TYPE_ORDINAL ? "ordinal"_s : "cardinal"_s));
    options->putDirect(vm, Identifier::fromString(&vm, "minimumIntegerDigits"), jsNumber(m_minimumIntegerDigits));
    options->putDirect(vm, Identifier::fromString(&vm, "minimumFractionDigits"), jsNumber(m_minimumFractionDigits));
    options->putDirect(vm, Identifier::fromString(&vm, "maximumFractionDigits"), jsNumber(m_maximumFractionDigits));
    if (m_minimumSignificantDigits) {
        options->putDirect(vm, Identifier::fromString(&vm, "minimumSignificantDigits"), jsNumber(m_minimumSignificantDigits.value()));
        options->putDirect(vm, Identifier::fromString(&vm, "maximumSignificantDigits"), jsNumber(m_maximumSignificantDigits.value()));
    }

#if HAVE(ICU_PLURALRULES_KEYWORDS)
    JSGlobalObject* globalObject = exec.jsCallee()->globalObject(vm);
    JSArray* categories = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    if (!categories)
        return throwOutOfMemoryError(&exec, scope);

    UErrorCode status = U_ZERO_ERROR;
    auto keywords = std::unique_ptr<UEnumeration, UEnumerationDeleter>(uplrules_getKeywords(m_pluralRules.get(), &status));
    ASSERT(U_SUCCESS(status));
    int32_t resultLength;

    // Category names are always ASCII, so use char[].
    unsigned index = 0;
    while (const char* result = uenum_next(keywords.get(), &resultLength, &status)) {
        ASSERT(U_SUCCESS(status));
        categories->putDirectIndex(&exec, index++, jsNontrivialString(&exec, String(result, resultLength)));
        RETURN_IF_EXCEPTION(scope, { });
    }
    options->putDirect(vm, Identifier::fromString(&vm, "pluralCategories"), categories);
#endif

    RELEASE_AND_RETURN(scope, options);
}

JSValue IntlPluralRules::select(ExecState& exec, double value)
{
    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.1.4 ResolvePlural (pluralRules, n)
    // https://tc39.github.io/ecma402/#sec-resolveplural
    if (!m_initializedPluralRules)
        return throwTypeError(&exec, scope, "Intl.PluralRules.prototype.select called on value that's not an object initialized as a PluralRules"_s);

    if (!std::isfinite(value))
        return jsNontrivialString(&exec, "other"_s);

#if HAVE(ICU_PLURALRULES_WITH_FORMAT)
    UErrorCode status = U_ZERO_ERROR;
    Vector<UChar, 8> result(8);
    auto length = uplrules_selectWithFormat(m_pluralRules.get(), value, m_numberFormat.get(), result.data(), result.size(), &status);
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to select plural value"_s);
#else
    UErrorCode status = U_ZERO_ERROR;
    Vector<UChar, 32> buffer(32);
    auto length = unum_formatDouble(m_numberFormat.get(), value, buffer.data(), buffer.size(), nullptr, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        buffer.grow(length);
        status = U_ZERO_ERROR;
        unum_formatDouble(m_numberFormat.get(), value, buffer.data(), length, nullptr, &status);
    }
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to select plural value"_s);

    double formatted = unum_parseDouble(m_numberFormat.get(), buffer.data(), length, nullptr, &status);
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to select plural value"_s);

    // Can only be 'zero', 'one', 'two', 'few', 'many' or 'other'
    status = U_ZERO_ERROR;
    Vector<UChar, 8> result(8);
    length = uplrules_select(m_pluralRules.get(), formatted, result.data(), result.size(), &status);
    if (U_FAILURE(status))
        return throwTypeError(&exec, scope, "failed to select plural value"_s);
#endif

    return jsString(&exec, String(result.data(), length));
}

} // namespace JSC

#endif // ENABLE(INTL)
