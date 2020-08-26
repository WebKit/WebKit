/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2020 Apple Inc. All Rights Reserved.
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
#include "IntlCollator.h"

#include "IntlObjectInlines.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include <wtf/HexNumber.h>

namespace JSC {

const ClassInfo IntlCollator::s_info = { "Object", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlCollator) };

namespace IntlCollatorInternal {
constexpr bool verbose = false;
}

IntlCollator* IntlCollator::create(VM& vm, Structure* structure)
{
    IntlCollator* format = new (NotNull, allocateCell<IntlCollator>(vm.heap)) IntlCollator(vm, structure);
    format->finishCreation(vm);
    return format;
}

Structure* IntlCollator::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlCollator::IntlCollator(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlCollator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void IntlCollator::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlCollator* thisObject = jsCast<IntlCollator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_boundCompare);
}

Vector<String> IntlCollator::sortLocaleData(const String& locale, RelevantExtensionKey key)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    Vector<String> keyLocaleData;
    switch (key) {
    case RelevantExtensionKey::Co: {
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.append({ });

        UErrorCode status = U_ZERO_ERROR;
        UEnumeration* enumeration = ucol_getKeywordValuesForLocale("collation", locale.utf8().data(), false, &status);
        if (U_SUCCESS(status)) {
            const char* collation;
            while ((collation = uenum_next(enumeration, nullptr, &status)) && U_SUCCESS(status)) {
                // 10.2.3 "The values "standard" and "search" must not be used as elements in any [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co array."
                if (!strcmp(collation, "standard") || !strcmp(collation, "search"))
                    continue;

                // Map keyword values to BCP 47 equivalents.
                if (!strcmp(collation, "dictionary"))
                    collation = "dict";
                else if (!strcmp(collation, "gb2312han"))
                    collation = "gb2312";
                else if (!strcmp(collation, "phonebook"))
                    collation = "phonebk";
                else if (!strcmp(collation, "traditional"))
                    collation = "trad";

                keyLocaleData.append(collation);
            }
            uenum_close(enumeration);
        }
        break;
    }
    case RelevantExtensionKey::Kf:
        keyLocaleData.reserveInitialCapacity(3);
        keyLocaleData.uncheckedAppend("false"_s);
        keyLocaleData.uncheckedAppend("lower"_s);
        keyLocaleData.uncheckedAppend("upper"_s);
        break;
    case RelevantExtensionKey::Kn:
        keyLocaleData.reserveInitialCapacity(2);
        keyLocaleData.uncheckedAppend("false"_s);
        keyLocaleData.uncheckedAppend("true"_s);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

Vector<String> IntlCollator::searchLocaleData(const String&, RelevantExtensionKey key)
{
    // 9.1 Internal slots of Service Constructors & 10.2.3 Internal slots (ECMA-402 2.0)
    Vector<String> keyLocaleData;
    switch (key) {
    case RelevantExtensionKey::Co:
        // 10.2.3 "The first element of [[sortLocaleData]][locale].co and [[searchLocaleData]][locale].co must be null for all locale values."
        keyLocaleData.reserveInitialCapacity(1);
        keyLocaleData.append({ });
        break;
    case RelevantExtensionKey::Kf:
        keyLocaleData.reserveInitialCapacity(3);
        keyLocaleData.uncheckedAppend("false"_s);
        keyLocaleData.uncheckedAppend("lower"_s);
        keyLocaleData.uncheckedAppend("upper"_s);
        break;
    case RelevantExtensionKey::Kn:
        keyLocaleData.reserveInitialCapacity(2);
        keyLocaleData.uncheckedAppend("false"_s);
        keyLocaleData.uncheckedAppend("true"_s);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return keyLocaleData;
}

// https://tc39.github.io/ecma402/#sec-initializecollator
void IntlCollator::initializeCollator(JSGlobalObject* globalObject, JSValue locales, JSValue optionsValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto requestedLocales = canonicalizeLocaleList(globalObject, locales);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue options = optionsValue;
    if (!optionsValue.isUndefined()) {
        options = optionsValue.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
    }

    m_usage = intlOption<Usage>(globalObject, options, vm.propertyNames->usage, { { "sort"_s, Usage::Sort }, { "search"_s, Usage::Search } }, "usage must be either \"sort\" or \"search\""_s, Usage::Sort);
    RETURN_IF_EXCEPTION(scope, void());

    auto localeData = (m_usage == Usage::Sort) ? sortLocaleData : searchLocaleData;

    ResolveLocaleOptions localeOptions;

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, void());

    TriState numeric = intlBooleanOption(globalObject, options, vm.propertyNames->numeric);
    RETURN_IF_EXCEPTION(scope, void());
    if (numeric != TriState::Indeterminate)
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Kn)] = String(numeric == TriState::True ? "true"_s : "false"_s);

    String caseFirstOption = intlStringOption(globalObject, options, vm.propertyNames->caseFirst, { "upper", "lower", "false" }, "caseFirst must be either \"upper\", \"lower\", or \"false\"", nullptr);
    RETURN_IF_EXCEPTION(scope, void());
    if (!caseFirstOption.isNull())
        localeOptions[static_cast<unsigned>(RelevantExtensionKey::Kf)] = caseFirstOption;

    auto& availableLocales = intlCollatorAvailableLocales();
    auto resolved = resolveLocale(globalObject, availableLocales, requestedLocales, localeMatcher, localeOptions, { RelevantExtensionKey::Co, RelevantExtensionKey::Kf, RelevantExtensionKey::Kn }, localeData);

    m_locale = resolved.locale;
    if (m_locale.isEmpty()) {
        throwTypeError(globalObject, scope, "failed to initialize Collator due to invalid locale"_s);
        return;
    }

    const String& collation = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Co)];
    m_collation = collation.isNull() ? "default"_s : collation;
    m_numeric = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Kn)] == "true"_s;

    const String& caseFirstString = resolved.extensions[static_cast<unsigned>(RelevantExtensionKey::Kf)];
    if (caseFirstString == "lower")
        m_caseFirst = CaseFirst::Lower;
    else if (caseFirstString == "upper")
        m_caseFirst = CaseFirst::Upper;
    else
        m_caseFirst = CaseFirst::False;

    m_sensitivity = intlOption<Sensitivity>(globalObject, options, vm.propertyNames->sensitivity, { { "base"_s, Sensitivity::Base }, { "accent"_s, Sensitivity::Accent }, { "case"_s, Sensitivity::Case }, { "variant"_s, Sensitivity::Variant } }, "sensitivity must be either \"base\", \"accent\", \"case\", or \"variant\""_s, Sensitivity::Variant);
    RETURN_IF_EXCEPTION(scope, void());

    TriState ignorePunctuation = intlBooleanOption(globalObject, options, vm.propertyNames->ignorePunctuation);
    RETURN_IF_EXCEPTION(scope, void());
    m_ignorePunctuation = (ignorePunctuation == TriState::True);

    // UCollator does not offer an option to configure "usage" via ucol_setAttribute. So we need to pass this option via locale.
    CString dataLocaleWithExtensions;
    switch (m_usage) {
    case Usage::Sort:
        dataLocaleWithExtensions = m_locale.utf8();
        break;
    case Usage::Search:
        // searchLocaleData filters out "co" unicode extension. However, we need to pass "co" to ICU when Usage::Search is specified.
        // So we need to pass "co" unicode extension through locale. Since the other relevant extensions are handled via ucol_setAttribute,
        // we can just use dataLocale
        dataLocaleWithExtensions = makeString(resolved.dataLocale, "-u-co-search").utf8();
        break;
    }
    dataLogLnIf(IntlCollatorInternal::verbose, "dataLocaleWithExtensions:(", dataLocaleWithExtensions, ")");

    UErrorCode status = U_ZERO_ERROR;
    m_collator = std::unique_ptr<UCollator, UCollatorDeleter>(ucol_open(dataLocaleWithExtensions.data(), &status));
    if (U_FAILURE(status)) {
        throwTypeError(globalObject, scope, "failed to initialize Collator"_s);
        return;
    }

    UColAttributeValue strength = UCOL_PRIMARY;
    UColAttributeValue caseLevel = UCOL_OFF;
    UColAttributeValue caseFirst = UCOL_OFF;
    switch (m_sensitivity) {
    case Sensitivity::Base:
        break;
    case Sensitivity::Accent:
        strength = UCOL_SECONDARY;
        break;
    case Sensitivity::Case:
        caseLevel = UCOL_ON;
        break;
    case Sensitivity::Variant:
        strength = UCOL_TERTIARY;
        break;
    }
    switch (m_caseFirst) {
    case CaseFirst::False:
        break;
    case CaseFirst::Lower:
        caseFirst = UCOL_LOWER_FIRST;
        break;
    case CaseFirst::Upper:
        caseFirst = UCOL_UPPER_FIRST;
        break;
    }

    // Keep in sync with canDoASCIIUCADUCETComparisonSlow about used attributes.
    ucol_setAttribute(m_collator.get(), UCOL_STRENGTH, strength, &status);
    ucol_setAttribute(m_collator.get(), UCOL_CASE_LEVEL, caseLevel, &status);
    ucol_setAttribute(m_collator.get(), UCOL_CASE_FIRST, caseFirst, &status);
    ucol_setAttribute(m_collator.get(), UCOL_NUMERIC_COLLATION, m_numeric ? UCOL_ON : UCOL_OFF, &status);

    // FIXME: Setting UCOL_ALTERNATE_HANDLING to UCOL_SHIFTED causes punctuation and whitespace to be
    // ignored. There is currently no way to ignore only punctuation.
    ucol_setAttribute(m_collator.get(), UCOL_ALTERNATE_HANDLING, m_ignorePunctuation ? UCOL_SHIFTED : UCOL_DEFAULT, &status);

    // "The method is required to return 0 when comparing Strings that are considered canonically
    // equivalent by the Unicode standard."
    ucol_setAttribute(m_collator.get(), UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    ASSERT(U_SUCCESS(status));
}

// https://tc39.es/ecma402/#sec-collator-comparestrings
JSValue IntlCollator::compareStrings(JSGlobalObject* globalObject, StringView x, StringView y) const
{
    ASSERT(m_collator);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    UErrorCode status = U_ZERO_ERROR;
    UCollationResult result = ([&]() -> UCollationResult {
        if (x.isAllSpecialCharacters<canUseASCIIUCADUCETComparison>() && y.isAllSpecialCharacters<canUseASCIIUCADUCETComparison>()) {
            if (canDoASCIIUCADUCETComparison()) {
                if (x.is8Bit() && y.is8Bit())
                    return compareASCIIWithUCADUCET(x.characters8(), x.length(), y.characters8(), y.length());
                if (x.is8Bit())
                    return compareASCIIWithUCADUCET(x.characters8(), x.length(), y.characters16(), y.length());
                if (y.is8Bit())
                    return compareASCIIWithUCADUCET(x.characters16(), x.length(), y.characters8(), y.length());
                return compareASCIIWithUCADUCET(x.characters16(), x.length(), y.characters16(), y.length());
            }

            if (x.is8Bit() && y.is8Bit())
                return ucol_strcollUTF8(m_collator.get(), bitwise_cast<const char*>(x.characters8()), x.length(), bitwise_cast<const char*>(y.characters8()), y.length(), &status);
        }
        return ucol_strcoll(m_collator.get(), x.upconvertedCharacters(), x.length(), y.upconvertedCharacters(), y.length());
    }());
    if (U_FAILURE(status))
        return throwException(globalObject, scope, createError(globalObject, "Failed to compare strings."_s));
    return jsNumber(result);
}

ASCIILiteral IntlCollator::usageString(Usage usage)
{
    switch (usage) {
    case Usage::Sort:
        return "sort"_s;
    case Usage::Search:
        return "search"_s;
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
}

ASCIILiteral IntlCollator::sensitivityString(Sensitivity sensitivity)
{
    switch (sensitivity) {
    case Sensitivity::Base:
        return "base"_s;
    case Sensitivity::Accent:
        return "accent"_s;
    case Sensitivity::Case:
        return "case"_s;
    case Sensitivity::Variant:
        return "variant"_s;
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
}

ASCIILiteral IntlCollator::caseFirstString(CaseFirst caseFirst)
{
    switch (caseFirst) {
    case CaseFirst::False:
        return "false"_s;
    case CaseFirst::Lower:
        return "lower"_s;
    case CaseFirst::Upper:
        return "upper"_s;
    }
    ASSERT_NOT_REACHED();
    return ASCIILiteral::null();
}

// https://tc39.es/ecma402/#sec-intl.collator.prototype.resolvedoptions
JSObject* IntlCollator::resolvedOptions(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    JSObject* options = constructEmptyObject(globalObject);
    options->putDirect(vm, vm.propertyNames->locale, jsString(vm, m_locale));
    options->putDirect(vm, vm.propertyNames->usage, jsNontrivialString(vm, usageString(m_usage)));
    options->putDirect(vm, vm.propertyNames->sensitivity, jsNontrivialString(vm, sensitivityString(m_sensitivity)));
    options->putDirect(vm, vm.propertyNames->ignorePunctuation, jsBoolean(m_ignorePunctuation));
    options->putDirect(vm, vm.propertyNames->collation, jsString(vm, m_collation));
    options->putDirect(vm, vm.propertyNames->numeric, jsBoolean(m_numeric));
    options->putDirect(vm, vm.propertyNames->caseFirst, jsNontrivialString(vm, caseFirstString(m_caseFirst)));
    return options;
}

void IntlCollator::setBoundCompare(VM& vm, JSBoundFunction* format)
{
    m_boundCompare.set(vm, this, format);
}

static bool canDoASCIIUCADUCETComparisonWithUCollator(UCollator& collator)
{
    // Attributes are default ones unless we set. So, non-configured attributes are default ones.
    static constexpr std::pair<UColAttribute, UColAttributeValue> attributes[] = {
        { UCOL_FRENCH_COLLATION, UCOL_OFF },
        { UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE },
        { UCOL_STRENGTH, UCOL_TERTIARY },
        { UCOL_CASE_LEVEL, UCOL_OFF },
        { UCOL_CASE_FIRST, UCOL_OFF },
        { UCOL_NUMERIC_COLLATION, UCOL_OFF },
        // We do not check UCOL_NORMALIZATION_MODE status since FCD normalization does nothing for ASCII strings.
    };

    for (auto& pair : attributes) {
        UErrorCode status = U_ZERO_ERROR;
        auto result = ucol_getAttribute(&collator, pair.first, &status);
        ASSERT(U_SUCCESS(status));
        if (result != pair.second)
            return false;
    }

    // Check existence of tailoring rules. If they do not exist, collation algorithm is UCA DUCET.
    int32_t length = 0;
    ucol_getRules(&collator, &length);
    return !length;
}

bool IntlCollator::updateCanDoASCIIUCADUCETComparison() const
{
    // ICU uses the CLDR root collation order as a default starting point for ordering. (The CLDR root collation is based on the UCA DUCET.)
    // And customizes this root collation via rules.
    // The root collation is UCA DUCET and it is code-point comparison if the characters are all ASCII.
    // http://www.unicode.org/reports/tr10/
    ASSERT(m_collator);
    auto checkASCIIUCADUCETComparisonCompatibility = [&] {
        if (m_usage != Usage::Sort)
            return false;
        if (m_collation != "default"_s)
            return false;
        if (m_sensitivity != Sensitivity::Variant)
            return false;
        if (m_caseFirst != CaseFirst::False)
            return false;
        if (m_numeric)
            return false;
        if (m_ignorePunctuation)
            return false;
        return canDoASCIIUCADUCETComparisonWithUCollator(*m_collator);
    };
    bool result = checkASCIIUCADUCETComparisonCompatibility();
    m_canDoASCIIUCADUCETComparison = triState(result);
    return result;
}

#if ASSERT_ENABLED
void IntlCollator::checkICULocaleInvariants(const HashSet<String>& locales)
{
    for (auto& locale : locales) {
        auto checkASCIIOrderingWithDUCET = [](const String& locale, UCollator& collator) {
            bool allAreGood = true;
            for (unsigned x = 0; x < 128; ++x) {
                for (unsigned y = 0; y < 128; ++y) {
                    if (canUseASCIIUCADUCETComparison(x) && canUseASCIIUCADUCETComparison(y)) {
                        UErrorCode status = U_ZERO_ERROR;
                        UChar xstring[] = { static_cast<UChar>(x), 0 };
                        UChar ystring[] = { static_cast<UChar>(y), 0 };
                        auto resultICU = ucol_strcoll(&collator, xstring, 1, ystring, 1);
                        ASSERT(U_SUCCESS(status));
                        auto resultJSC = compareASCIIWithUCADUCET(xstring, 1, ystring, 1);
                        if (resultICU != resultJSC) {
                            dataLogLn("BAD ", locale, " ", makeString(hex(x)), "(", StringView(xstring, 1), ") <=> ", makeString(hex(y)), "(", StringView(ystring, 1), ") ICU:(", static_cast<int32_t>(resultICU), "),JSC:(", static_cast<int32_t>(resultJSC), ")");
                            allAreGood = false;
                        }
                    }
                }
            }
            return allAreGood;
        };

        UErrorCode status = U_ZERO_ERROR;
        auto collator = std::unique_ptr<UCollator, ICUDeleter<ucol_close>>(ucol_open(locale.ascii().data(), &status));

        ASSERT(U_SUCCESS(status));
        ucol_setAttribute(collator.get(), UCOL_STRENGTH, UCOL_TERTIARY, &status);
        ASSERT(U_SUCCESS(status));
        ucol_setAttribute(collator.get(), UCOL_CASE_LEVEL, UCOL_OFF, &status);
        ASSERT(U_SUCCESS(status));
        ucol_setAttribute(collator.get(), UCOL_CASE_FIRST, UCOL_OFF, &status);
        ASSERT(U_SUCCESS(status));
        ucol_setAttribute(collator.get(), UCOL_NUMERIC_COLLATION, UCOL_OFF, &status);
        ASSERT(U_SUCCESS(status));
        ucol_setAttribute(collator.get(), UCOL_ALTERNATE_HANDLING, UCOL_DEFAULT, &status);
        ASSERT(U_SUCCESS(status));
        ucol_setAttribute(collator.get(), UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
        ASSERT(U_SUCCESS(status));

        if (!canDoASCIIUCADUCETComparisonWithUCollator(*collator))
            continue;

        // This should not have reorder.
        int32_t length = ucol_getReorderCodes(collator.get(), nullptr, 0, &status);
        ASSERT(U_SUCCESS(status));
        ASSERT(!length);

        // Contractions and Expansions are defined as a rule. If there is no tailoring rule, then they should be UCA DUCET's default.

        auto ensureNotIncludingASCII = [&](USet& set) {
            Vector<UChar, 32> buffer;
            for (int32_t index = 0, count = uset_getItemCount(&set); index < count; ++index) {
                // start and end are inclusive.
                UChar32 start = 0;
                UChar32 end = 0;
                auto status = callBufferProducingFunction(uset_getItem, &set, index, &start, &end, buffer);
                ASSERT(U_SUCCESS(status));
                if (buffer.isEmpty()) {
                    if (isASCII(start)) {
                        dataLogLn("BAD ", locale, " including ASCII tailored characters");
                        CRASH();
                    }
                } else {
                    if (StringView(buffer.data(), buffer.size()).isAllASCII()) {
                        dataLogLn("BAD ", locale, " ", String(buffer.data(), buffer.size()), " including ASCII tailored characters");
                        CRASH();
                    }
                }
            }
        };

        auto contractions = std::unique_ptr<USet, ICUDeleter<uset_close>>(uset_openEmpty());
        auto expansions = std::unique_ptr<USet, ICUDeleter<uset_close>>(uset_openEmpty());
        ucol_getContractionsAndExpansions(collator.get(), contractions.get(), expansions.get(), true, &status);
        ASSERT(U_SUCCESS(status));

        ensureNotIncludingASCII(*contractions);
        ensureNotIncludingASCII(*expansions);

        // This locale should not have tailoring.
        auto tailored = std::unique_ptr<USet, ICUDeleter<uset_close>>(ucol_getTailoredSet(collator.get(), &status));
        ensureNotIncludingASCII(*tailored);

        dataLogLnIf(IntlCollatorInternal::verbose, "LOCALE ", locale);

        ASSERT(checkASCIIOrderingWithDUCET(locale, *collator));
    }
}
#endif

} // namespace JSC
