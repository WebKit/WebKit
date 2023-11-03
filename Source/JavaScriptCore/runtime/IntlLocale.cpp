/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "IntlLocale.h"

#include "IntlDateTimeFormat.h"
#include "IntlObjectInlines.h"
#include "JSCInlines.h"
#include <unicode/ucal.h>
#include <unicode/ucol.h>
#include <unicode/udat.h>
#include <unicode/udatpg.h>
#include <unicode/uloc.h>
#include <unicode/unumsys.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

const ClassInfo IntlLocale::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlLocale) };

namespace IntlLocaleInternal {
static constexpr bool verbose = false;
}

IntlLocale* IntlLocale::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlLocale>(vm)) IntlLocale(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlLocale::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlLocale::IntlLocale(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

template<typename Visitor>
void IntlLocale::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<IntlLocale*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(IntlLocale);

class LocaleIDBuilder final {
public:
    bool initialize(const String&);
    CString toCanonical();

    void overrideLanguageScriptRegion(StringView language, StringView script, StringView region);
    bool setKeywordValue(ASCIILiteral key, StringView value);

private:
    Vector<char, 32> m_buffer;
};

bool LocaleIDBuilder::initialize(const String& tag)
{
    if (!isStructurallyValidLanguageTag(tag))
        return false;
    ASSERT(tag.containsOnlyASCII());
    m_buffer = localeIDBufferForLanguageTagWithNullTerminator(tag.ascii());
    return m_buffer.size();
}

CString LocaleIDBuilder::toCanonical()
{
    ASSERT(m_buffer.size());

    auto buffer = canonicalizeLocaleIDWithoutNullTerminator(m_buffer.data());
    if (!buffer)
        return CString();

    auto result = canonicalizeUnicodeExtensionsAfterICULocaleCanonicalization(WTFMove(buffer.value()));
    return CString(result.data(), result.size());
}

// Because ICU's C API doesn't have set[Language|Script|Region] functions...
void LocaleIDBuilder::overrideLanguageScriptRegion(StringView language, StringView script, StringView region)
{
    unsigned length = strlen(m_buffer.data());

    StringView localeIDView { m_buffer.data(), length };

    auto endOfLanguageScriptRegionVariant = localeIDView.find(ULOC_KEYWORD_SEPARATOR);
    if (endOfLanguageScriptRegionVariant == notFound)
        endOfLanguageScriptRegionVariant = length;

    Vector<StringView> subtags;
    for (auto subtag : localeIDView.left(endOfLanguageScriptRegionVariant).splitAllowingEmptyEntries('_'))
        subtags.append(subtag);

    if (!language.isNull())
        subtags[0] = language;

    bool hasScript = subtags.size() > 1 && subtags[1].length() == 4;
    if (!script.isNull()) {
        if (hasScript)
            subtags[1] = script;
        else {
            subtags.insert(1, script);
            hasScript = true;
        }
    }

    if (!region.isNull()) {
        size_t index = hasScript ? 2 : 1;
        bool hasRegion = subtags.size() > index && subtags[index].length() < 4;
        if (hasRegion)
            subtags[index] = region;
        else
            subtags.insert(index, region);
    }

    Vector<char, 32> buffer;
    bool hasAppended = false;
    for (auto subtag : subtags) {
        if (hasAppended)
            buffer.append('_');
        else
            hasAppended = true;

        ASSERT(subtag.containsOnlyASCII());
        if (subtag.is8Bit())
            buffer.append(subtag.characters8(), subtag.length());
        else
            buffer.append(subtag.characters16(), subtag.length());
    }

    if (endOfLanguageScriptRegionVariant != length) {
        auto rest = localeIDView.right(length - endOfLanguageScriptRegionVariant);

        ASSERT(rest.containsOnlyASCII());
        if (rest.is8Bit())
            buffer.append(rest.characters8(), rest.length());
        else
            buffer.append(rest.characters16(), rest.length());
    }

    buffer.append('\0');
    m_buffer.swap(buffer);
}

bool LocaleIDBuilder::setKeywordValue(ASCIILiteral key, StringView value)
{
    ASSERT(m_buffer.size());

    ASSERT(value.containsOnlyASCII());
    Vector<char, 32> rawValue(value.length() + 1);
    value.getCharacters(reinterpret_cast<LChar*>(rawValue.data()));
    rawValue[value.length()] = '\0';

    UErrorCode status = U_ZERO_ERROR;
    auto length = uloc_setKeywordValue(key.characters(), rawValue.data(), m_buffer.data(), m_buffer.size(), &status);
    // uloc_setKeywordValue does not set U_STRING_NOT_TERMINATED_WARNING.
    if (needsToGrowToProduceBuffer(status)) {
        m_buffer.grow(length + 1);
        status = U_ZERO_ERROR;
        uloc_setKeywordValue(key.characters(), rawValue.data(), m_buffer.data(), length + 1, &status);
    }
    return U_SUCCESS(status);
}

String IntlLocale::keywordValue(ASCIILiteral key, bool isBoolean) const
{
    UErrorCode status = U_ZERO_ERROR;
    Vector<char, 32> buffer(32);
    auto bufferLength = uloc_getKeywordValue(m_localeID.data(), key.characters(), buffer.data(), buffer.size(), &status);
    if (needsToGrowToProduceCString(status)) {
        buffer.grow(bufferLength + 1);
        status = U_ZERO_ERROR;
        uloc_getKeywordValue(m_localeID.data(), key.characters(), buffer.data(), buffer.size(), &status);
    }
    ASSERT(U_SUCCESS(status));
    if (isBoolean)
        return String::fromLatin1(buffer.data());
    const char* value = uloc_toUnicodeLocaleType(key.characters(), buffer.data());
    if (!value)
        return nullString();
    auto result = String::fromLatin1(value);
    if (result == "true"_s)
        return emptyString();
    return result;
}

// https://tc39.es/ecma402/#sec-Intl.Locale
void IntlLocale::initializeLocale(JSGlobalObject* globalObject, JSValue tagValue, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String tag = tagValue.inherits<IntlLocale>() ? jsCast<IntlLocale*>(tagValue)->toString() : tagValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    initializeLocale(globalObject, tag, optionsValue);
}

// https://tc39.es/ecma402/#sec-Intl.Locale
void IntlLocale::initializeLocale(JSGlobalObject* globalObject, const String& tag, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* options = intlCoerceOptionsToObject(globalObject, optionsValue);
    RETURN_IF_EXCEPTION(scope, void());

    LocaleIDBuilder localeID;
    if (!localeID.initialize(tag)) {
        throwRangeError(globalObject, scope, "invalid language tag"_s);
        return;
    }

    String language = intlStringOption(globalObject, options, vm.propertyNames->language, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!language.isNull() && !isUnicodeLanguageSubtag(language)) {
        throwRangeError(globalObject, scope, "language is not a well-formed language value"_s);
        return;
    }

    String script = intlStringOption(globalObject, options, vm.propertyNames->script, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!script.isNull() && !isUnicodeScriptSubtag(script)) {
        throwRangeError(globalObject, scope, "script is not a well-formed script value"_s);
        return;
    }

    String region = intlStringOption(globalObject, options, vm.propertyNames->region, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!region.isNull() && !isUnicodeRegionSubtag(region)) {
        throwRangeError(globalObject, scope, "region is not a well-formed region value"_s);
        return;
    }

    if (!language.isNull() || !script.isNull() || !region.isNull())
        localeID.overrideLanguageScriptRegion(language, script, region);

    String calendar = intlStringOption(globalObject, options, vm.propertyNames->calendar, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!calendar.isNull()) {
        if (!isUnicodeLocaleIdentifierType(calendar) || !localeID.setKeywordValue("calendar"_s, calendar)) {
            throwRangeError(globalObject, scope, "calendar is not a well-formed calendar value"_s);
            return;
        }
    }

    String collation = intlStringOption(globalObject, options, vm.propertyNames->collation, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!collation.isNull()) {
        if (!isUnicodeLocaleIdentifierType(collation) || !localeID.setKeywordValue("collation"_s, collation)) {
            throwRangeError(globalObject, scope, "collation is not a well-formed collation value"_s);
            return;
        }
    }

    String hourCycle = intlStringOption(globalObject, options, vm.propertyNames->hourCycle, { "h11"_s, "h12"_s, "h23"_s, "h24"_s }, "hourCycle must be \"h11\", \"h12\", \"h23\", or \"h24\""_s, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!hourCycle.isNull()) {
        bool success = localeID.setKeywordValue("hours"_s, hourCycle);
        ASSERT_UNUSED(success, success);
    }

    String caseFirst = intlStringOption(globalObject, options, vm.propertyNames->caseFirst, { "upper"_s, "lower"_s, "false"_s }, "caseFirst must be either \"upper\", \"lower\", or \"false\""_s, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!caseFirst.isNull()) {
        bool success = localeID.setKeywordValue("colcasefirst"_s, caseFirst);
        ASSERT_UNUSED(success, success);
    }

    TriState numeric = intlBooleanOption(globalObject, options, vm.propertyNames->numeric);
    RETURN_IF_EXCEPTION(scope, void());
    if (numeric != TriState::Indeterminate) {
        bool success = localeID.setKeywordValue("colnumeric"_s, numeric == TriState::True ? "yes"_s : "no"_s);
        ASSERT_UNUSED(success, success);
    }

    String numberingSystem = intlStringOption(globalObject, options, vm.propertyNames->numberingSystem, { }, { }, { });
    RETURN_IF_EXCEPTION(scope, void());
    if (!numberingSystem.isNull()) {
        if (!isUnicodeLocaleIdentifierType(numberingSystem) || !localeID.setKeywordValue("numbers"_s, numberingSystem)) {
            throwRangeError(globalObject, scope, "numberingSystem is not a well-formed numbering system value"_s);
            return;
        }
    }

    m_localeID = localeID.toCanonical();
    if (m_localeID.isNull()) {
        throwTypeError(globalObject, scope, "failed to initialize Locale"_s);
        return;
    }
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.maximize
const String& IntlLocale::maximal()
{
    if (m_maximal.isNull()) {
        // ICU has a serious bug that it fails to perform uloc_addLikelySubtags when the input localeID is longer than ULOC_FULLNAME_CAPACITY,
        // and that can be achieved if we add many unicode extensions. While ICU needs to be fixed, we work-around this bug for now: We pass
        // non-keyword part of ICU locale ID and later, concatenate keyword part to the output.
        // Note that ICU locale ID consists of Language, Script, Country (unicode language tag's region.
        // FIXME: ICU tracking bug https://unicode-org.atlassian.net/browse/ICU-21639.
        UErrorCode status = U_ZERO_ERROR;
        Vector<char, 32> buffer(32);
        auto bufferLength = uloc_addLikelySubtags(m_localeID.data(), buffer.data(), buffer.size(), &status);
        if (needsToGrowToProduceCString(status)) {
            buffer.grow(bufferLength + 1);
            status = U_ZERO_ERROR;
            uloc_addLikelySubtags(m_localeID.data(), buffer.data(), buffer.size(), &status);
        }

        if (U_SUCCESS(status))
            m_maximal = languageTagForLocaleID(buffer.data());
        else {
            status = U_ZERO_ERROR;
            Vector<char, 32> baseNameID;
            auto bufferLength = uloc_getBaseName(m_localeID.data(), baseNameID.data(), baseNameID.size(), &status);
            if (needsToGrowToProduceCString(status)) {
                baseNameID.grow(bufferLength + 1);
                status = U_ZERO_ERROR;
                uloc_getBaseName(m_localeID.data(), baseNameID.data(), baseNameID.size(), &status);
            }
            ASSERT(U_SUCCESS(status));

            Vector<char, 32> maximal;
            status = callBufferProducingFunction(uloc_addLikelySubtags, baseNameID.data(), maximal);
            // We fail if,
            // 1. uloc_addLikelySubtags still fails.
            // 2. New maximal locale ID includes newly-added keywords.
            if (!U_SUCCESS(status) || maximal.find(ULOC_KEYWORD_SEPARATOR) != notFound) {
                m_maximal = toString();
                return m_maximal;
            }

            auto endOfLanguageScriptRegionVariant = WTF::find(m_localeID.data(), m_localeID.length(), ULOC_KEYWORD_SEPARATOR);
            if (endOfLanguageScriptRegionVariant != notFound)
                maximal.appendRange(m_localeID.data() + endOfLanguageScriptRegionVariant, m_localeID.data() + m_localeID.length());
            maximal.append('\0');
            m_maximal = languageTagForLocaleID(maximal.data());
        }
    }
    return m_maximal;
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.minimize
const String& IntlLocale::minimal()
{
    if (m_minimal.isNull()) {
        // ICU has a serious bug that it fails to perform uloc_minimizeSubtags when the input localeID is longer than ULOC_FULLNAME_CAPACITY,
        // and that can be achieved if we add many unicode extensions. While ICU needs to be fixed, we work-around this bug for now: We pass
        // non-keyword part of ICU locale ID and later, concatenate keyword part to the output.
        // Note that ICU locale ID consists of Language, Script, Country (unicode language tag's region.
        // FIXME: ICU tracking bug https://unicode-org.atlassian.net/browse/ICU-21639.
        UErrorCode status = U_ZERO_ERROR;
        Vector<char, 32> buffer(32);
        auto bufferLength = uloc_minimizeSubtags(m_localeID.data(), buffer.data(), buffer.size(), &status);
        if (needsToGrowToProduceCString(status)) {
            buffer.grow(bufferLength + 1);
            status = U_ZERO_ERROR;
            uloc_minimizeSubtags(m_localeID.data(), buffer.data(), buffer.size(), &status);
        }

        if (U_SUCCESS(status))
            m_minimal = languageTagForLocaleID(buffer.data());
        else {
            status = U_ZERO_ERROR;
            Vector<char, 32> baseNameID;
            auto bufferLength = uloc_getBaseName(m_localeID.data(), baseNameID.data(), baseNameID.size(), &status);
            if (needsToGrowToProduceCString(status)) {
                baseNameID.grow(bufferLength + 1);
                status = U_ZERO_ERROR;
                uloc_getBaseName(m_localeID.data(), baseNameID.data(), baseNameID.size(), &status);
            }
            ASSERT(U_SUCCESS(status));

            Vector<char, 32> minimal;
            auto status = callBufferProducingFunction(uloc_minimizeSubtags, baseNameID.data(), minimal);
            // We fail if,
            // 1. uloc_minimizeSubtags still fails.
            // 2. New minimal locale ID includes newly-added keywords.
            if (!U_SUCCESS(status) || minimal.find(ULOC_KEYWORD_SEPARATOR) != notFound) {
                m_minimal = toString();
                return m_minimal;
            }

            auto endOfLanguageScriptRegionVariant = WTF::find(m_localeID.data(), m_localeID.length(), ULOC_KEYWORD_SEPARATOR);
            if (endOfLanguageScriptRegionVariant != notFound)
                minimal.appendRange(m_localeID.data() + endOfLanguageScriptRegionVariant, m_localeID.data() + m_localeID.length());
            minimal.append('\0');
            m_minimal = languageTagForLocaleID(minimal.data());
        }
    }
    return m_minimal;
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.toString
const String& IntlLocale::toString()
{
    if (m_fullString.isNull())
        m_fullString = languageTagForLocaleID(m_localeID.data());
    return m_fullString;
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.baseName
const String& IntlLocale::baseName()
{
    if (m_baseName.isNull()) {
        UErrorCode status = U_ZERO_ERROR;
        Vector<char, 32> buffer(32);
        auto bufferLength = uloc_getBaseName(m_localeID.data(), buffer.data(), buffer.size(), &status);
        if (needsToGrowToProduceCString(status)) {
            buffer.grow(bufferLength + 1);
            status = U_ZERO_ERROR;
            uloc_getBaseName(m_localeID.data(), buffer.data(), buffer.size(), &status);
        }
        ASSERT(U_SUCCESS(status));

        m_baseName = languageTagForLocaleID(buffer.data());
    }
    return m_baseName;
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.language
const String& IntlLocale::language()
{
    if (m_language.isNull()) {
        Vector<char, 8> buffer;
        auto status = callBufferProducingFunction(uloc_getLanguage, m_localeID.data(), buffer);
        ASSERT_UNUSED(status, U_SUCCESS(status));
        m_language = String(buffer.data(), buffer.size());
    }
    return m_language;
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.script
const String& IntlLocale::script()
{
    if (m_script.isNull()) {
        Vector<char, 4> buffer;
        auto status = callBufferProducingFunction(uloc_getScript, m_localeID.data(), buffer);
        ASSERT_UNUSED(status, U_SUCCESS(status));
        m_script = String(buffer.data(), buffer.size());
    }
    return m_script;
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.region
const String& IntlLocale::region()
{
    if (m_region.isNull()) {
        Vector<char, 3> buffer;
        auto status = callBufferProducingFunction(uloc_getCountry, m_localeID.data(), buffer);
        ASSERT_UNUSED(status, U_SUCCESS(status));
        m_region = String(buffer.data(), buffer.size());
    }
    return m_region;
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.calendar
const String& IntlLocale::calendar()
{
    if (!m_calendar)
        m_calendar = keywordValue("calendar"_s);
    return m_calendar.value();
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.caseFirst
const String& IntlLocale::caseFirst()
{
    if (!m_caseFirst)
        m_caseFirst = keywordValue("colcasefirst"_s);
    return m_caseFirst.value();
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.collation
const String& IntlLocale::collation()
{
    if (!m_collation)
        m_collation = keywordValue("collation"_s);
    return m_collation.value();
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.hourCycle
const String& IntlLocale::hourCycle()
{
    if (!m_hourCycle)
        m_hourCycle = keywordValue("hours"_s);
    return m_hourCycle.value();
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.numberingSystem
const String& IntlLocale::numberingSystem()
{
    if (!m_numberingSystem)
        m_numberingSystem = keywordValue("numbers"_s);
    return m_numberingSystem.value();
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.numeric
TriState IntlLocale::numeric()
{
    constexpr bool isBoolean = true;
    if (m_numeric == TriState::Indeterminate)
        m_numeric = triState(keywordValue("colnumeric"_s, isBoolean) == "yes"_s);
    return m_numeric;
}

JSArray* IntlLocale::calendars(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String, 1> elements;

    String preferred = calendar();
    if (!preferred.isEmpty()) {
        elements.append(WTFMove(preferred));
        RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
    }

    UErrorCode status = U_ZERO_ERROR;
    constexpr bool commonlyUsed = true;
    auto calendars = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucal_getKeywordValuesForLocale("calendar", m_localeID.data(), commonlyUsed, &status));
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }

    const char* pointer;
    int32_t length = 0;
    while ((pointer = uenum_next(calendars.get(), &length, &status)) && U_SUCCESS(status)) {
        String calendar(pointer, length);
        if (auto mapped = mapICUCalendarKeywordToBCP47(calendar))
            elements.append(WTFMove(mapped.value()));
        else
            elements.append(WTFMove(calendar));
    }
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
}

JSArray* IntlLocale::collations(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String, 1> elements;

    String preferred = collation();
    if (!preferred.isEmpty()) {
        elements.append(WTFMove(preferred));
        RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
    }

    UErrorCode status = U_ZERO_ERROR;
    constexpr bool commonlyUsed = true;
    auto enumeration = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucol_getKeywordValuesForLocale("collation", m_localeID.data(), commonlyUsed, &status));
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }

    const char* pointer;
    int32_t length = 0;
    while ((pointer = uenum_next(enumeration.get(), &length, &status)) && U_SUCCESS(status)) {
        String collation(pointer, length);
        // 1.1.3 step 4, The values "standard" and "search" must be excluded from list.
        if (collation == "standard"_s || collation == "search"_s)
            continue;
        if (auto mapped = mapICUCollationKeywordToBCP47(collation))
            elements.append(WTFMove(mapped.value()));
        else
            elements.append(WTFMove(collation));
    }
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }

    RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
}

JSArray* IntlLocale::hourCycles(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String, 1> elements;

    String preferred = hourCycle();
    if (!preferred.isEmpty()) {
        elements.append(WTFMove(preferred));
        RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
    }

    UErrorCode status = U_ZERO_ERROR;
    auto generator = std::unique_ptr<UDateTimePatternGenerator, ICUDeleter<udatpg_close>>(udatpg_open(m_localeID.data(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }

    // Use "j" skeleton and parse pattern to retrieve the configured hour-cycle information.
    constexpr const UChar skeleton[] = { 'j', 0 };
    Vector<UChar, 32> pattern;
    status = callBufferProducingFunction(udatpg_getBestPatternWithOptions, generator.get(), skeleton, 1, UDATPG_MATCH_HOUR_FIELD_LENGTH, pattern);
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }

    dataLogLnIf(IntlLocaleInternal::verbose, "pattern:(", StringView(pattern.data(), pattern.size()), ")");

    switch (IntlDateTimeFormat::hourCycleFromPattern(pattern)) {
    case IntlDateTimeFormat::HourCycle::None:
        break;
    case IntlDateTimeFormat::HourCycle::H11: {
        elements.append("h11"_s);
        RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
    }
    case IntlDateTimeFormat::HourCycle::H12: {
        elements.append("h12"_s);
        RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
    }
    case IntlDateTimeFormat::HourCycle::H23: {
        elements.append("h23"_s);
        RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
    }
    case IntlDateTimeFormat::HourCycle::H24: {
        elements.append("h24"_s);
        RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
    }
    }

    RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
}

JSArray* IntlLocale::numberingSystems(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String, 1> elements;
    String preferred = numberingSystem();
    if (!preferred.isEmpty()) {
        elements.append(WTFMove(preferred));
        RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
    }

    UErrorCode status = U_ZERO_ERROR;
    auto numberingSystem = std::unique_ptr<UNumberingSystem, ICUDeleter<unumsys_close>>(unumsys_open(m_localeID.data(), &status));
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }
    elements.append(String::fromLatin1(unumsys_getName(numberingSystem.get())));

    RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
}

JSValue IntlLocale::timeZones(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String, 1> elements;

    // 11.6-3 Let region be the substring of locale corresponding to the unicode_region_subtag production of the unicode_language_id.
    String region = this->region();
    if (region.isEmpty())
        return jsUndefined();

    UErrorCode status = U_ZERO_ERROR;
    auto enumeration = std::unique_ptr<UEnumeration, ICUDeleter<uenum_close>>(ucal_openTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL, region.utf8().data(), nullptr, &status));
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return { };
    }

    int32_t length;
    const char* collation;
    while ((collation = uenum_next(enumeration.get(), &length, &status)) && U_SUCCESS(status))
        elements.constructAndAppend(collation, length);
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, createArrayFromStringVector(globalObject, WTFMove(elements)));
}

JSObject* IntlLocale::textInfo(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    ULayoutType layout = uloc_getCharacterOrientation(m_localeID.data(), &status);
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }

    JSString* layoutString = nullptr;
    switch (layout) {
    default:
    case ULOC_LAYOUT_LTR:
        layoutString = jsNontrivialString(vm, "ltr"_s);
        break;
    case ULOC_LAYOUT_RTL:
        layoutString = jsNontrivialString(vm, "rtl"_s);
        break;
    case ULOC_LAYOUT_TTB:
        layoutString = jsNontrivialString(vm, "ttb"_s);
        break;
    case ULOC_LAYOUT_BTT:
        layoutString = jsNontrivialString(vm, "btt"_s);
        break;
    }

    JSObject* result = constructEmptyObject(globalObject);
    result->putDirect(vm, Identifier::fromString(vm, "direction"_s), layoutString);
    return result;
}

JSObject* IntlLocale::weekInfo(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    auto calendar = std::unique_ptr<UCalendar, ICUDeleter<ucal_close>>(ucal_open(nullptr, 0, m_localeID.data(), UCAL_DEFAULT, &status));
    if (!U_SUCCESS(status)) {
        throwTypeError(globalObject, scope, "invalid locale"_s);
        return nullptr;
    }

    int32_t firstDayOfWeek = ucal_getAttribute(calendar.get(), UCAL_FIRST_DAY_OF_WEEK);
    int32_t minimalDays = ucal_getAttribute(calendar.get(), UCAL_MINIMAL_DAYS_IN_FIRST_WEEK);

    auto canonicalizeDayOfWeekType = [](UCalendarWeekdayType type) {
        switch (type) {
        // UCAL_WEEKEND_ONSET is a day that starts as a weekday and transitions to the weekend. It means this is WeekDay.
        case UCAL_WEEKEND_ONSET:
        case UCAL_WEEKDAY:
            return UCAL_WEEKDAY;
        // UCAL_WEEKEND_CEASE is a day that starts as the weekend and transitions to a weekday. It means this is WeekEnd.
        case UCAL_WEEKEND_CEASE:
        case UCAL_WEEKEND:
            return UCAL_WEEKEND;
        default:
            return UCAL_WEEKEND;
        }
    };

    auto convertMondayBasedDayToUCalendarDaysOfWeek = [](int32_t day) -> UCalendarDaysOfWeek {
        // Convert from
        //     Monday => 1
        //     Sunday => 7
        // to
        //     Sunday => 1
        //     Saturday => 7
        static_assert(UCAL_SUNDAY == 1);
        static_assert(UCAL_SATURDAY == 7);
        if (day == 7)
            return UCAL_SUNDAY;
        return static_cast<UCalendarDaysOfWeek>(day + 1);
    };

    auto convertUCalendarDaysOfWeekToMondayBasedDay = [](int32_t day) -> int32_t {
        // Convert from
        //     Sunday => 1
        //     Saturday => 7
        // to
        //     Monday => 1
        //     Sunday => 7
        if (day == UCAL_SUNDAY)
            return 7;
        return day - 1;
    };

    Vector<int32_t, 7> weekend;
    for (int32_t day = 1; day <= 7; ++day) {
        UCalendarWeekdayType type = canonicalizeDayOfWeekType(ucal_getDayOfWeekType(calendar.get(), convertMondayBasedDayToUCalendarDaysOfWeek(day), &status));
        if (!U_SUCCESS(status)) {
            throwTypeError(globalObject, scope, "invalid locale"_s);
            return nullptr;
        }
        switch (type) {
        case UCAL_WEEKDAY:
            break;
        case UCAL_WEEKEND:
            weekend.append(day);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    auto* weekendArray = createArrayFromIntVector(globalObject, WTFMove(weekend));
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* result = constructEmptyObject(globalObject);
    result->putDirect(vm, Identifier::fromString(vm, "firstDay"_s), jsNumber(convertUCalendarDaysOfWeekToMondayBasedDay(firstDayOfWeek)));
    result->putDirect(vm, Identifier::fromString(vm, "weekend"_s), weekendArray);
    result->putDirect(vm, Identifier::fromString(vm, "minimalDays"_s), jsNumber(minimalDays));
    return result;
}

} // namespace JSC
