/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "IntlObject.h"

#include "Error.h"
#include "FunctionPrototype.h"
#include "IntlCollator.h"
#include "IntlCollatorConstructor.h"
#include "IntlCollatorPrototype.h"
#include "IntlDateTimeFormatConstructor.h"
#include "IntlDateTimeFormatPrototype.h"
#include "IntlDisplayNames.h"
#include "IntlDisplayNamesConstructor.h"
#include "IntlDisplayNamesPrototype.h"
#include "IntlLocale.h"
#include "IntlLocaleConstructor.h"
#include "IntlLocalePrototype.h"
#include "IntlNumberFormatConstructor.h"
#include "IntlNumberFormatPrototype.h"
#include "IntlObjectInlines.h"
#include "IntlPluralRulesConstructor.h"
#include "IntlPluralRulesPrototype.h"
#include "IntlRelativeTimeFormatConstructor.h"
#include "IntlRelativeTimeFormatPrototype.h"
#include "IntlSegmenter.h"
#include "IntlSegmenterConstructor.h"
#include "IntlSegmenterPrototype.h"
#include "JSCInlines.h"
#include "Options.h"
#include <unicode/ubrk.h>
#include <unicode/ucol.h>
#include <unicode/ufieldpositer.h>
#include <unicode/uloc.h>
#include <unicode/unumsys.h>
#include <wtf/Assertions.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringImpl.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlObject);

static EncodedJSValue JSC_HOST_CALL intlObjectFuncGetCanonicalLocales(JSGlobalObject*, CallFrame*);

static JSValue createCollatorConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlCollatorConstructor::create(vm, IntlCollatorConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlCollatorPrototype*>(globalObject->collatorStructure()->storedPrototypeObject()));
}

static JSValue createDateTimeFormatConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlDateTimeFormatConstructor::create(vm, IntlDateTimeFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlDateTimeFormatPrototype*>(globalObject->dateTimeFormatStructure()->storedPrototypeObject()));
}

static JSValue createDisplayNamesConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlDisplayNamesConstructor::create(vm, IntlDisplayNamesConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlDisplayNamesPrototype*>(globalObject->displayNamesStructure()->storedPrototypeObject()));
}

static JSValue createLocaleConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlLocaleConstructor::create(vm, IntlLocaleConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlLocalePrototype*>(globalObject->localeStructure()->storedPrototypeObject()));
}

static JSValue createNumberFormatConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlNumberFormatConstructor::create(vm, IntlNumberFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlNumberFormatPrototype*>(globalObject->numberFormatStructure()->storedPrototypeObject()));
}

static JSValue createPluralRulesConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlPluralRulesConstructor::create(vm, IntlPluralRulesConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlPluralRulesPrototype*>(globalObject->pluralRulesStructure()->storedPrototypeObject()));
}

static JSValue createRelativeTimeFormatConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlRelativeTimeFormatConstructor::create(vm, IntlRelativeTimeFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlRelativeTimeFormatPrototype*>(globalObject->relativeTimeFormatStructure()->storedPrototypeObject()));
}

static JSValue createSegmenterConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlSegmenterConstructor::create(vm, IntlSegmenterConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlSegmenterPrototype*>(globalObject->segmenterStructure()->storedPrototypeObject()));
}

}

#include "IntlObject.lut.h"

namespace JSC {

/* Source for IntlObject.lut.h
@begin intlObjectTable
  getCanonicalLocales   intlObjectFuncGetCanonicalLocales            DontEnum|Function 1
  Collator              createCollatorConstructor                    DontEnum|PropertyCallback
  DateTimeFormat        createDateTimeFormatConstructor              DontEnum|PropertyCallback
  Locale                createLocaleConstructor                      DontEnum|PropertyCallback
  NumberFormat          createNumberFormatConstructor                DontEnum|PropertyCallback
  PluralRules           createPluralRulesConstructor                 DontEnum|PropertyCallback
  RelativeTimeFormat    createRelativeTimeFormatConstructor          DontEnum|PropertyCallback
  Segmenter             createSegmenterConstructor                   DontEnum|PropertyCallback
@end
*/

struct MatcherResult {
    String locale;
    String extension;
    size_t extensionIndex { 0 };
};

const ClassInfo IntlObject::s_info = { "Intl", &Base::s_info, &intlObjectTable, nullptr, CREATE_METHOD_TABLE(IntlObject) };

void UFieldPositionIteratorDeleter::operator()(UFieldPositionIterator* iterator) const
{
    if (iterator)
        ufieldpositer_close(iterator);
}

IntlObject::IntlObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

IntlObject* IntlObject::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    IntlObject* object = new (NotNull, allocateCell<IntlObject>(vm.heap)) IntlObject(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

void IntlObject::finishCreation(VM& vm, JSGlobalObject*)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
#if HAVE(ICU_U_LOCALE_DISPLAY_NAMES)
    putDirectWithoutTransition(vm, vm.propertyNames->DisplayNames, createDisplayNamesConstructor(vm, this), static_cast<unsigned>(PropertyAttribute::DontEnum));
#else
    UNUSED_PARAM(createDisplayNamesConstructor);
#endif
}

Structure* IntlObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

Vector<char, 32> localeIDBufferForLanguageTag(const CString& tag)
{
    if (!tag.length())
        return { };

    UErrorCode status = U_ZERO_ERROR;
    Vector<char, 32> buffer(32);
    int32_t parsedLength;
    auto bufferLength = uloc_forLanguageTag(tag.data(), buffer.data(), buffer.size(), &parsedLength, &status);
    if (needsToGrowToProduceCString(status)) {
        // Before ICU 64, there's a chance uloc_forLanguageTag will "buffer overflow" while requesting a *smaller* size.
        buffer.resize(bufferLength + 1);
        status = U_ZERO_ERROR;
        uloc_forLanguageTag(tag.data(), buffer.data(), bufferLength + 1, &parsedLength, &status);
    }
    if (U_FAILURE(status) || parsedLength != static_cast<int32_t>(tag.length()))
        return { };

    ASSERT(buffer.contains('\0'));
    return buffer;
}

String languageTagForLocaleID(const char* localeID, bool isImmortal)
{
    Vector<char, 32> buffer;
    auto status = callBufferProducingFunction(uloc_toLanguageTag, localeID, buffer, false);
    if (U_FAILURE(status))
        return String();

    // This is used to store into static variables that may be shared across JSC execution threads.
    // This must be immortal to make concurrent ref/deref safe.
    if (isImmortal)
        return StringImpl::createStaticStringImpl(buffer.data(), buffer.size());

    return String(buffer.data(), buffer.size());
}

// Ensure we have xx-ZZ whenever we have xx-Yyyy-ZZ.
static void addScriptlessLocaleIfNeeded(HashSet<String>& availableLocales, StringView locale)
{
    if (locale.length() < 10)
        return;

    Vector<StringView, 3> subtags;
    for (auto subtag : locale.split('-')) {
        if (subtags.size() == 3)
            return;
        subtags.append(subtag);
    }

    if (subtags.size() != 3 || subtags[1].length() != 4 || subtags[2].length() > 3)
        return;

    Vector<char, 12> buffer;
    ASSERT(subtags[0].is8Bit() && subtags[0].isAllASCII());
    buffer.append(reinterpret_cast<const char*>(subtags[0].characters8()), subtags[0].length());
    buffer.append('-');
    ASSERT(subtags[2].is8Bit() && subtags[2].isAllASCII());
    buffer.append(reinterpret_cast<const char*>(subtags[2].characters8()), subtags[2].length());

    availableLocales.add(StringImpl::createStaticStringImpl(buffer.data(), buffer.size()));
}

const HashSet<String>& intlAvailableLocales()
{
    static LazyNeverDestroyed<HashSet<String>> availableLocales;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        availableLocales.construct();
        ASSERT(availableLocales->isEmpty());
        constexpr bool isImmortal = true;
        int32_t count = uloc_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = languageTagForLocaleID(uloc_getAvailable(i), isImmortal);
            if (locale.isEmpty())
                continue;
            availableLocales->add(locale);
            addScriptlessLocaleIfNeeded(availableLocales.get(), locale);
        }
    });
    return availableLocales;
}

// This table is total ordering indexes for ASCII characters in UCA DUCET.
// It is generated from CLDR common/uca/allkeys_DUCET.txt.
//
// Rough overview of UCA is the followings.
// https://unicode.org/reports/tr10/#Main_Algorithm
//
//     1. Normalize each input string.
//
//     2. Produce an array of collation elements for each string.
//
//         There are 3 (or 4) levels. And each character has 4 weights. We concatenate them into one sequence called collation elements.
//         For example, "c" has `[.0706.0020.0002]`. And "ca◌́b" becomes `[.0706.0020.0002], [.06D9.0020.0002], [.0000.0021.0002], [.06EE.0020.0002]`
//         We need to consider variable weighting (https://unicode.org/reports/tr10/#Variable_Weighting), but if it is Non-ignorable, we can just use
//         the collation elements defined in the table.
//
//     3. Produce a sort key for each string from the arrays of collation elements.
//
//         Generate sort key from collation elements. From lower levels to higher levels, we collect weights. But 0000 weight is skipped.
//         Between levels, we insert 0000 weight if the boundary.
//
//             string: "ca◌́b"
//             collation elements: `[.0706.0020.0002], [.06D9.0020.0002], [.0000.0021.0002], [.06EE.0020.0002]`
//             sort key: `0706 06D9 06EE 0000 0020 0020 0021 0020 0000 0002 0002 0002 0002`
//                                        ^                        ^
//                                        level boundary           level boundary
//
//     4. Compare the two sort keys with a binary comparison operation.
//
// Key observations are the followings.
//
//     1. If an input is an ASCII string, UCA step-1 normalization does nothing.
//     2. If an input is an ASCII string, non-starters (https://unicode.org/reports/tr10/#UTS10-D33) does not exist. So no special handling in UCA step-2 is required.
//     3. If an input is an ASCII string, no multiple character collation elements exist. So no special handling in UCA step-2 is required. For example, "L·" is not ASCII.
//     4. UCA step-3 handles 0000 weighted characters specially. And ASCII contains these characters. But 0000 elements are used only for rare control characters.
//        We can ignore this special handling if ASCII strings do not include control characters.
//     5. Except 0000 cases, all characters' level-1 weights are different. And level-2 weights are always 0020, which is lower than any level-1 weights.
//        This means that binary comparison in UCA step-4 do not need to check level 2~ weights.
//
//  Based on the above observation, our fast path handles ASCII strings excluding control characters. The following weight is recomputed weights from level-1 weights.
const uint8_t ducetWeights[128] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 3, 4, 5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    6, 12, 16, 28, 38, 29, 27, 15,
    17, 18, 24, 32, 9, 8, 14, 25,
    39, 40, 41, 42, 43, 44, 45, 46,
    47, 48, 11, 10, 33, 34, 35, 13,
    23, 50, 52, 54, 56, 58, 60, 62,
    64, 66, 68, 70, 72, 74, 76, 78,
    80, 82, 84, 86, 88, 90, 92, 94,
    96, 98, 100, 19, 26, 20, 31, 7,
    30, 49, 51, 53, 55, 57, 59, 61,
    63, 65, 67, 69, 71, 73, 75, 77,
    79, 81, 83, 85, 87, 89, 91, 93,
    95, 97, 99, 21, 36, 22, 37, 0,
};

const HashSet<String>& intlCollatorAvailableLocales()
{
    static LazyNeverDestroyed<HashSet<String>> availableLocales;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        availableLocales.construct();
        ASSERT(availableLocales->isEmpty());
        constexpr bool isImmortal = true;
        int32_t count = ucol_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = languageTagForLocaleID(ucol_getAvailable(i), isImmortal);
            if (locale.isEmpty())
                continue;
            availableLocales->add(locale);
            addScriptlessLocaleIfNeeded(availableLocales.get(), locale);
        }
        IntlCollator::checkICULocaleInvariants(availableLocales.get());
    });
    return availableLocales;
}

const HashSet<String>& intlSegmenterAvailableLocales()
{
    static NeverDestroyed<HashSet<String>> cachedAvailableLocales;
    HashSet<String>& availableLocales = cachedAvailableLocales.get();

    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        ASSERT(availableLocales.isEmpty());
        constexpr bool isImmortal = true;
        int32_t count = ubrk_countAvailable();
        for (int32_t i = 0; i < count; ++i) {
            String locale = languageTagForLocaleID(ubrk_getAvailable(i), isImmortal);
            if (locale.isEmpty())
                continue;
            availableLocales.add(locale);
            addScriptlessLocaleIfNeeded(availableLocales, locale);
        }
    });
    return availableLocales;
}

// https://tc39.es/ecma402/#sec-getoption
TriState intlBooleanOption(JSGlobalObject* globalObject, JSValue options, PropertyName property)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (options.isUndefined())
        return TriState::Indeterminate;

    JSObject* opts = options.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, TriState::Indeterminate);

    JSValue value = opts->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, TriState::Indeterminate);

    if (value.isUndefined())
        return TriState::Indeterminate;

    return triState(value.toBoolean(globalObject));
}

String intlStringOption(JSGlobalObject* globalObject, JSValue options, PropertyName property, std::initializer_list<const char*> values, const char* notFound, const char* fallback)
{
    // GetOption (options, property, type="string", values, fallback)
    // https://tc39.github.io/ecma402/#sec-getoption

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (options.isUndefined())
        return fallback;

    JSObject* opts = options.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, String());

    JSValue value = opts->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, String());

    if (!value.isUndefined()) {
        String stringValue = value.toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, String());

        if (values.size() && std::find(values.begin(), values.end(), stringValue) == values.end()) {
            throwException(globalObject, scope, createRangeError(globalObject, notFound));
            return { };
        }
        return stringValue;
    }

    return fallback;
}

unsigned intlNumberOption(JSGlobalObject* globalObject, JSValue options, PropertyName property, unsigned minimum, unsigned maximum, unsigned fallback)
{
    // GetNumberOption (options, property, minimum, maximum, fallback)
    // https://tc39.github.io/ecma402/#sec-getnumberoption

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (options.isUndefined())
        return fallback;

    JSObject* opts = options.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, 0);

    JSValue value = opts->get(globalObject, property);
    RETURN_IF_EXCEPTION(scope, 0);

    RELEASE_AND_RETURN(scope, intlDefaultNumberOption(globalObject, value, property, minimum, maximum, fallback));
}

unsigned intlDefaultNumberOption(JSGlobalObject* globalObject, JSValue value, PropertyName property, unsigned minimum, unsigned maximum, unsigned fallback)
{
    // DefaultNumberOption (value, minimum, maximum, fallback)
    // https://tc39.github.io/ecma402/#sec-defaultnumberoption

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isUndefined()) {
        double doubleValue = value.toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, 0);

        if (!(doubleValue >= minimum && doubleValue <= maximum)) {
            throwException(globalObject, scope, createRangeError(globalObject, *property.publicName() + " is out of range"));
            return 0;
        }
        return static_cast<unsigned>(doubleValue);
    }
    return fallback;
}

// http://www.unicode.org/reports/tr35/#Unicode_locale_identifier
bool isUnicodeLocaleIdentifierType(StringView string)
{
    ASSERT(!string.isNull());

    for (auto part : string.splitAllowingEmptyEntries('-')) {
        auto length = part.length();
        if (length < 3 || length > 8)
            return false;

        for (auto character : part.codeUnits()) {
            if (!isASCIIAlphanumeric(character))
                return false;
        }
    }

    return true;
}

// https://tc39.es/ecma402/#sec-canonicalizeunicodelocaleid
static String canonicalizeLanguageTag(const CString& tag)
{
    auto buffer = localeIDBufferForLanguageTag(tag);
    if (buffer.isEmpty())
        return String();

    return languageTagForLocaleID(buffer.data());
}

Vector<String> canonicalizeLocaleList(JSGlobalObject* globalObject, JSValue locales)
{
    // CanonicalizeLocaleList (locales)
    // https://tc39.github.io/ecma402/#sec-canonicalizelocalelist

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> seen;

    if (locales.isUndefined())
        return seen;

    JSObject* localesObject;
    if (locales.isString() || locales.inherits<IntlLocale>(vm)) {
        JSArray* localesArray = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous));
        if (!localesArray) {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
        localesArray->push(globalObject, locales);
        RETURN_IF_EXCEPTION(scope, Vector<String>());

        localesObject = localesArray;
    } else {
        localesObject = locales.toObject(globalObject);
        RETURN_IF_EXCEPTION(scope, Vector<String>());
    }

    // 6. Let len be ToLength(Get(O, "length")).
    JSValue lengthProperty = localesObject->get(globalObject, vm.propertyNames->length);
    RETURN_IF_EXCEPTION(scope, Vector<String>());

    uint64_t length = static_cast<uint64_t>(lengthProperty.toLength(globalObject));
    RETURN_IF_EXCEPTION(scope, Vector<String>());

    HashSet<String> seenSet;
    for (uint64_t k = 0; k < length; ++k) {
        bool kPresent = localesObject->hasProperty(globalObject, k);
        RETURN_IF_EXCEPTION(scope, Vector<String>());

        if (kPresent) {
            JSValue kValue = localesObject->get(globalObject, k);
            RETURN_IF_EXCEPTION(scope, Vector<String>());

            if (!kValue.isString() && !kValue.isObject()) {
                throwTypeError(globalObject, scope, "locale value must be a string or object"_s);
                return { };
            }

            String tag;
            if (kValue.inherits<IntlLocale>(vm))
                tag = jsCast<IntlLocale*>(kValue)->toString();
            else {
                JSString* string = kValue.toString(globalObject);
                RETURN_IF_EXCEPTION(scope, Vector<String>());

                tag = string->value(globalObject);
                RETURN_IF_EXCEPTION(scope, Vector<String>());
            }

            if (isStructurallyValidLanguageTag(tag)) {
                ASSERT(tag.isAllASCII());
                String canonicalizedTag = canonicalizeLanguageTag(tag.ascii());
                if (!canonicalizedTag.isNull()) {
                    if (seenSet.add(canonicalizedTag).isNewEntry)
                        seen.append(canonicalizedTag);
                    continue;
                }
            }

            String errorMessage = tryMakeString("invalid language tag: ", tag);
            if (UNLIKELY(!errorMessage)) {
                throwException(globalObject, scope, createOutOfMemoryError(globalObject));
                return { };
            }
            throwException(globalObject, scope, createRangeError(globalObject, errorMessage));
            return { };
        }
    }

    return seen;
}

String bestAvailableLocale(const HashSet<String>& availableLocales, const String& locale)
{
    return bestAvailableLocale(locale, [&](const String& candidate) {
        return availableLocales.contains(candidate);
    });
}

String defaultLocale(JSGlobalObject* globalObject)
{
    // DefaultLocale ()
    // https://tc39.github.io/ecma402/#sec-defaultlocale

    // WebCore's global objects will have their own ideas of how to determine the language. It may
    // be determined by WebCore-specific logic like some WK settings. Usually this will return the
    // same thing as userPreferredLanguages()[0].
    if (auto defaultLanguage = globalObject->globalObjectMethodTable()->defaultLanguage) {
        String locale = canonicalizeLanguageTag(defaultLanguage().utf8());
        if (!locale.isEmpty())
            return locale;
    }

    Vector<String> languages = userPreferredLanguages();
    for (const auto& language : languages) {
        String locale = canonicalizeLanguageTag(language.utf8());
        if (!locale.isEmpty())
            return locale;
    }

    // If all else fails, ask ICU. It will probably say something bogus like en_us even if the user
    // has configured some other language, but being wrong is better than crashing.
    static LazyNeverDestroyed<String> icuDefaultLocalString;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        constexpr bool isImmortal = true;
        icuDefaultLocalString.construct(languageTagForLocaleID(uloc_getDefault(), isImmortal));
    });
    if (!icuDefaultLocalString->isEmpty())
        return icuDefaultLocalString.get();

    return "en"_s;
}

String removeUnicodeLocaleExtension(const String& locale)
{
    Vector<String> parts = locale.split('-');
    StringBuilder builder;
    size_t partsSize = parts.size();
    bool atPrivate = false;
    if (partsSize > 0)
        builder.append(parts[0]);
    for (size_t p = 1; p < partsSize; ++p) {
        if (parts[p] == "x")
            atPrivate = true;
        if (!atPrivate && parts[p] == "u" && p + 1 < partsSize) {
            // Skip the u- and anything that follows until another singleton.
            // While the next part is part of the unicode extension, skip it.
            while (p + 1 < partsSize && parts[p + 1].length() > 1)
                ++p;
        } else {
            builder.append('-', parts[p]);
        }
    }
    return builder.toString();
}

static MatcherResult lookupMatcher(JSGlobalObject* globalObject, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales)
{
    // LookupMatcher (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-lookupmatcher

    String locale;
    String noExtensionsLocale;
    String availableLocale;
    for (size_t i = 0; i < requestedLocales.size() && availableLocale.isNull(); ++i) {
        locale = requestedLocales[i];
        noExtensionsLocale = removeUnicodeLocaleExtension(locale);
        availableLocale = bestAvailableLocale(availableLocales, noExtensionsLocale);
    }

    MatcherResult result;
    if (!availableLocale.isEmpty()) {
        result.locale = availableLocale;
        if (locale != noExtensionsLocale) {
            size_t extensionIndex = locale.find("-u-");
            RELEASE_ASSERT(extensionIndex != notFound);

            size_t extensionLength = locale.length() - extensionIndex;
            size_t end = extensionIndex + 3;
            while (end < locale.length()) {
                end = locale.find('-', end);
                if (end == notFound)
                    break;
                if (end + 2 < locale.length() && locale[end + 2] == '-') {
                    extensionLength = end - extensionIndex;
                    break;
                }
                end++;
            }
            result.extension = locale.substring(extensionIndex, extensionLength);
            result.extensionIndex = extensionIndex;
        }
    } else
        result.locale = defaultLocale(globalObject);
    return result;
}

static MatcherResult bestFitMatcher(JSGlobalObject* globalObject, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales)
{
    // BestFitMatcher (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-bestfitmatcher

    // FIXME: Implement something better than lookup.
    return lookupMatcher(globalObject, availableLocales, requestedLocales);
}

static void unicodeExtensionSubTags(const String& extension, Vector<String>& subtags)
{
    // UnicodeExtensionSubtags (extension)
    // https://tc39.github.io/ecma402/#sec-unicodeextensionsubtags

    auto extensionLength = extension.length();
    if (extensionLength < 3)
        return;

    size_t subtagStart = 3; // Skip initial -u-.
    size_t valueStart = 3;
    bool isLeading = true;
    for (size_t index = subtagStart; index < extensionLength; ++index) {
        if (extension[index] == '-') {
            if (index - subtagStart == 2) {
                // Tag is a key, first append prior key's value if there is one.
                if (subtagStart - valueStart > 1)
                    subtags.append(extension.substring(valueStart, subtagStart - valueStart - 1));
                subtags.append(extension.substring(subtagStart, index - subtagStart));
                valueStart = index + 1;
                isLeading = false;
            } else if (isLeading) {
                // Leading subtags before first key.
                subtags.append(extension.substring(subtagStart, index - subtagStart));
                valueStart = index + 1;
            }
            subtagStart = index + 1;
        }
    }
    if (extensionLength - subtagStart == 2) {
        // Trailing an extension key, first append prior key's value if there is one.
        if (subtagStart - valueStart > 1)
            subtags.append(extension.substring(valueStart, subtagStart - valueStart - 1));
        valueStart = subtagStart;
    }
    // Append final key's value.
    subtags.append(extension.substring(valueStart, extensionLength - valueStart));
}

constexpr ASCIILiteral relevantExtensionKeyString(RelevantExtensionKey key)
{
    switch (key) {
#define JSC_RETURN_INTL_RELEVANT_EXTENSION_KEYS(lowerName, capitalizedName) \
    case RelevantExtensionKey::capitalizedName: \
        return #lowerName ""_s;
    JSC_INTL_RELEVANT_EXTENSION_KEYS(JSC_RETURN_INTL_RELEVANT_EXTENSION_KEYS)
#undef JSC_RETURN_INTL_RELEVANT_EXTENSION_KEYS
    }
    return ASCIILiteral::null();
}

ResolvedLocale resolveLocale(JSGlobalObject* globalObject, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales, LocaleMatcher localeMatcher, const ResolveLocaleOptions& options, std::initializer_list<RelevantExtensionKey> relevantExtensionKeys, Vector<String> (*localeData)(const String&, RelevantExtensionKey))
{
    // ResolveLocale (availableLocales, requestedLocales, options, relevantExtensionKeys, localeData)
    // https://tc39.github.io/ecma402/#sec-resolvelocale

    MatcherResult matcherResult = localeMatcher == LocaleMatcher::Lookup
        ? lookupMatcher(globalObject, availableLocales, requestedLocales)
        : bestFitMatcher(globalObject, availableLocales, requestedLocales);

    String foundLocale = matcherResult.locale;

    Vector<String> extensionSubtags;
    if (!matcherResult.extension.isNull())
        unicodeExtensionSubTags(matcherResult.extension, extensionSubtags);

    ResolvedLocale resolved;
    resolved.dataLocale = foundLocale;

    String supportedExtension = "-u"_s;
    for (RelevantExtensionKey key : relevantExtensionKeys) {
        ASCIILiteral keyString = relevantExtensionKeyString(key);
        Vector<String> keyLocaleData = localeData(foundLocale, key);
        ASSERT(!keyLocaleData.isEmpty());

        String value = keyLocaleData[0];
        String supportedExtensionAddition;

        if (!extensionSubtags.isEmpty()) {
            size_t keyPos = extensionSubtags.find(keyString);
            if (keyPos != notFound) {
                if (keyPos + 1 < extensionSubtags.size() && extensionSubtags[keyPos + 1].length() > 2) {
                    const String& requestedValue = extensionSubtags[keyPos + 1];
                    if (keyLocaleData.contains(requestedValue)) {
                        value = requestedValue;
                        supportedExtensionAddition = makeString('-', keyString, '-', value);
                    }
                } else if (keyLocaleData.contains("true"_s)) {
                    value = "true"_s;
                    supportedExtensionAddition = makeString('-', keyString);
                }
            }
        }

        if (auto optionsValue = options[static_cast<unsigned>(key)]) {
            // Undefined should not get added to the options, it won't displace the extension.
            // Null will remove the extension.
            if ((optionsValue->isNull() || keyLocaleData.contains(*optionsValue)) && *optionsValue != value) {
                value = optionsValue.value();
                supportedExtensionAddition = String();
            }
        }
        resolved.extensions[static_cast<unsigned>(key)] = value;
        supportedExtension.append(supportedExtensionAddition);
    }

    if (supportedExtension.length() > 2) {
        StringView foundLocaleView(foundLocale);
        foundLocale = makeString(foundLocaleView.substring(0, matcherResult.extensionIndex), supportedExtension, foundLocaleView.substring(matcherResult.extensionIndex));
    }

    resolved.locale = WTFMove(foundLocale);
    return resolved;
}

static JSArray* lookupSupportedLocales(JSGlobalObject* globalObject, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales)
{
    // LookupSupportedLocales (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-lookupsupportedlocales

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t len = requestedLocales.size();
    JSArray* subset = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!subset) {
        throwOutOfMemoryError(globalObject, scope);
        return nullptr;
    }

    unsigned index = 0;
    for (size_t k = 0; k < len; ++k) {
        const String& locale = requestedLocales[k];
        String noExtensionsLocale = removeUnicodeLocaleExtension(locale);
        String availableLocale = bestAvailableLocale(availableLocales, noExtensionsLocale);
        if (!availableLocale.isNull()) {
            subset->putDirectIndex(globalObject, index++, jsString(vm, locale));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    return subset;
}

static JSArray* bestFitSupportedLocales(JSGlobalObject* globalObject, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales)
{
    // BestFitSupportedLocales (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-bestfitsupportedlocales

    // FIXME: Implement something better than lookup.
    return lookupSupportedLocales(globalObject, availableLocales, requestedLocales);
}

JSValue supportedLocales(JSGlobalObject* globalObject, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales, JSValue options)
{
    // SupportedLocales (availableLocales, requestedLocales, options)
    // https://tc39.github.io/ecma402/#sec-supportedlocales

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    String matcher;

    LocaleMatcher localeMatcher = intlOption<LocaleMatcher>(globalObject, options, vm.propertyNames->localeMatcher, { { "lookup"_s, LocaleMatcher::Lookup }, { "best fit"_s, LocaleMatcher::BestFit } }, "localeMatcher must be either \"lookup\" or \"best fit\""_s, LocaleMatcher::BestFit);
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (localeMatcher == LocaleMatcher::BestFit)
        RELEASE_AND_RETURN(scope, bestFitSupportedLocales(globalObject, availableLocales, requestedLocales));
    RELEASE_AND_RETURN(scope, lookupSupportedLocales(globalObject, availableLocales, requestedLocales));
}

Vector<String> numberingSystemsForLocale(const String& locale)
{
    static LazyNeverDestroyed<Vector<String>> availableNumberingSystems;
    static std::once_flag initializeOnce;
    std::call_once(initializeOnce, [&] {
        availableNumberingSystems.construct();
        ASSERT(availableNumberingSystems->isEmpty());
        UErrorCode status = U_ZERO_ERROR;
        UEnumeration* numberingSystemNames = unumsys_openAvailableNames(&status);
        ASSERT(U_SUCCESS(status));

        int32_t resultLength;
        // Numbering system names are always ASCII, so use char[].
        while (const char* result = uenum_next(numberingSystemNames, &resultLength, &status)) {
            ASSERT(U_SUCCESS(status));
            auto numsys = unumsys_openByName(result, &status);
            ASSERT(U_SUCCESS(status));
            // Only support algorithmic if it is the default fot the locale, handled below.
            if (!unumsys_isAlgorithmic(numsys))
                availableNumberingSystems->append(String(StringImpl::createStaticStringImpl(result, resultLength)));
            unumsys_close(numsys);
        }
        uenum_close(numberingSystemNames);
    });

    UErrorCode status = U_ZERO_ERROR;
    UNumberingSystem* defaultSystem = unumsys_open(locale.utf8().data(), &status);
    ASSERT(U_SUCCESS(status));
    String defaultSystemName(unumsys_getName(defaultSystem));
    unumsys_close(defaultSystem);

    Vector<String> numberingSystems({ defaultSystemName });
    numberingSystems.appendVector(availableNumberingSystems.get());
    return numberingSystems;
}

// unicode_language_subtag = alpha{2,3} | alpha{5,8} ;
bool isUnicodeLanguageSubtag(StringView string)
{
    auto length = string.length();
    return length >= 2 && length <= 8 && length != 4 && string.isAllSpecialCharacters<isASCIIAlpha>();
}

// unicode_script_subtag = alpha{4} ;
bool isUnicodeScriptSubtag(StringView string)
{
    return string.length() == 4 && string.isAllSpecialCharacters<isASCIIAlpha>();
}

// unicode_region_subtag = alpha{2} | digit{3} ;
bool isUnicodeRegionSubtag(StringView string)
{
    auto length = string.length();
    return (length == 2 && string.isAllSpecialCharacters<isASCIIAlpha>())
        || (length == 3 && string.isAllSpecialCharacters<isASCIIDigit>());
}

// unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
bool isUnicodeVariantSubtag(StringView string)
{
    auto length = string.length();
    if (length >= 5 && length <= 8)
        return string.isAllSpecialCharacters<isASCIIAlphanumeric>();
    return length == 4 && isASCIIDigit(string[0]) && string.substring(1).isAllSpecialCharacters<isASCIIAlphanumeric>();
}

using VariantCode = uint64_t;
static VariantCode parseVariantCode(StringView string)
{
    ASSERT(isUnicodeVariantSubtag(string));
    ASSERT(string.isAllASCII());
    ASSERT(string.length() <= 8);
    ASSERT(string.length() >= 1);
    struct Code {
        LChar characters[8] { };
    };
    static_assert(std::is_unsigned_v<LChar>);
    static_assert(sizeof(VariantCode) == sizeof(Code));
    Code code { };
    for (unsigned index = 0; index < string.length(); ++index)
        code.characters[index] = string[index];
    VariantCode result = bitwise_cast<VariantCode>(code);
    ASSERT(result); // Not possible since some characters exist.
    ASSERT(result != static_cast<VariantCode>(-1)); // Not possible since all characters are ASCII (not Latin-1).
    return result;
}

static unsigned convertToUnicodeSingletonIndex(UChar singleton)
{
    ASSERT(isASCIIAlphanumeric(singleton));
    singleton = toASCIILower(singleton);
    // 0 - 9 => numeric
    // 10 - 35 => alpha
    if (isASCIIDigit(singleton))
        return singleton - '0';
    return (singleton - 'a') + 10;
}
static constexpr unsigned numberOfUnicodeSingletons = 10 + 26; // Digits + Alphabets.

static bool isUnicodeExtensionAttribute(StringView string)
{
    auto length = string.length();
    return length >= 3 && length <= 8 && string.isAllSpecialCharacters<isASCIIAlphanumeric>();
}

static bool isUnicodeExtensionKey(StringView string)
{
    return string.length() == 2 && isASCIIAlphanumeric(string[0]) && isASCIIAlpha(string[1]);
}

static bool isUnicodeExtensionTypeComponent(StringView string)
{
    auto length = string.length();
    return length >= 3 && length <= 8 && string.isAllSpecialCharacters<isASCIIAlphanumeric>();
}

static bool isUnicodePUExtensionValue(StringView string)
{
    auto length = string.length();
    return length >= 1 && length <= 8 && string.isAllSpecialCharacters<isASCIIAlphanumeric>();
}

static bool isUnicodeOtherExtensionValue(StringView string)
{
    auto length = string.length();
    return length >= 2 && length <= 8 && string.isAllSpecialCharacters<isASCIIAlphanumeric>();
}

static bool isUnicodeTKey(StringView string)
{
    return string.length() == 2 && isASCIIAlpha(string[0]) && isASCIIDigit(string[1]);
}

static bool isUnicodeTValueComponent(StringView string)
{
    auto length = string.length();
    return length >= 3 && length <= 8 && string.isAllSpecialCharacters<isASCIIAlphanumeric>();
}

// The IsStructurallyValidLanguageTag abstract operation verifies that the locale argument (which must be a String value)
//
//     represents a well-formed "Unicode BCP 47 locale identifier" as specified in Unicode Technical Standard 35 section 3.2,
//     does not include duplicate variant subtags, and
//     does not include duplicate singleton subtags.
//
//  The abstract operation returns true if locale can be generated from the EBNF grammar in section 3.2 of the Unicode Technical Standard 35,
//  starting with unicode_locale_id, and does not contain duplicate variant or singleton subtags (other than as a private use subtag).
//  It returns false otherwise. Terminal value characters in the grammar are interpreted as the Unicode equivalents of the ASCII octet values given.
//
// https://unicode.org/reports/tr35/#Unicode_locale_identifier
class LanguageTagParser {
public:
    LanguageTagParser(StringView tag)
        : m_range(tag.splitAllowingEmptyEntries('-'))
        , m_cursor(m_range.begin())
    {
        ASSERT(m_cursor != m_range.end());
        m_current = *m_cursor;
    }

    bool parseUnicodeLocaleId();
    bool parseUnicodeLanguageId();

    bool isEOS()
    {
        return m_cursor == m_range.end();
    }

    bool next()
    {
        if (isEOS())
            return false;

        ++m_cursor;
        if (isEOS()) {
            m_current = StringView();
            return false;
        }
        m_current = *m_cursor;
        return true;
    }

private:
    bool parseExtensionsAndPUExtensions();

    bool parseUnicodeExtensionAfterPrefix();
    bool parseTransformedExtensionAfterPrefix();
    bool parseOtherExtensionAfterPrefix();
    bool parsePUExtensionAfterPrefix();

    StringView::SplitResult m_range;
    StringView::SplitResult::Iterator m_cursor;
    StringView m_current;
};

bool LanguageTagParser::parseUnicodeLocaleId()
{
    // unicode_locale_id    = unicode_language_id
    //                        extensions*
    //                        pu_extensions? ;
    ASSERT(!isEOS());
    if (!parseUnicodeLanguageId())
        return false;
    if (isEOS())
        return true;
    if (!parseExtensionsAndPUExtensions())
        return false;
    return true;
}

bool LanguageTagParser::parseUnicodeLanguageId()
{
    // unicode_language_id  = unicode_language_subtag (sep unicode_script_subtag)? (sep unicode_region_subtag)? (sep unicode_variant_subtag)* ;
    ASSERT(!isEOS());
    if (!isUnicodeLanguageSubtag(m_current))
        return false;
    if (!next())
        return true;

    if (isUnicodeScriptSubtag(m_current)) {
        if (!next())
            return true;
    }

    if (isUnicodeRegionSubtag(m_current)) {
        if (!next())
            return true;
    }

    HashSet<VariantCode> variantCodes;
    while (true) {
        if (!isUnicodeVariantSubtag(m_current))
            return true;
        // https://tc39.es/ecma402/#sec-isstructurallyvalidlanguagetag
        // does not include duplicate variant subtags
        if (!variantCodes.add(parseVariantCode(m_current)).isNewEntry)
            return false;
        if (!next())
            return true;
    }
}

bool LanguageTagParser::parseUnicodeExtensionAfterPrefix()
{
    // ((sep keyword)+ | (sep attribute)+ (sep keyword)*) ;
    //
    // keyword = key (sep type)? ;
    // key = alphanum alpha ;
    // type = alphanum{3,8} (sep alphanum{3,8})* ;
    // attribute = alphanum{3,8} ;
    ASSERT(!isEOS());
    bool isAttributeOrKeyword = false;
    if (isUnicodeExtensionAttribute(m_current)) {
        isAttributeOrKeyword = true;
        while (true) {
            if (!isUnicodeExtensionAttribute(m_current))
                break;
            if (!next())
                return true;
        }
    }

    if (isUnicodeExtensionKey(m_current)) {
        isAttributeOrKeyword = true;
        while (true) {
            if (!isUnicodeExtensionKey(m_current))
                break;
            if (!next())
                return true;
            while (true) {
                if (!isUnicodeExtensionTypeComponent(m_current))
                    break;
                if (!next())
                    return true;
            }
        }
    }

    if (!isAttributeOrKeyword)
        return false;
    return true;
}

bool LanguageTagParser::parseTransformedExtensionAfterPrefix()
{
    // ((sep tlang (sep tfield)*) | (sep tfield)+) ;
    //
    // tlang = unicode_language_subtag (sep unicode_script_subtag)? (sep unicode_region_subtag)? (sep unicode_variant_subtag)* ;
    // tfield = tkey tvalue;
    // tkey = alpha digit ;
    // tvalue = (sep alphanum{3,8})+ ;
    ASSERT(!isEOS());
    bool found = false;
    if (isUnicodeLanguageSubtag(m_current)) {
        found = true;
        if (!parseUnicodeLanguageId())
            return false;
        if (isEOS())
            return true;
    }

    if (isUnicodeTKey(m_current)) {
        found = true;
        while (true) {
            if (!isUnicodeTKey(m_current))
                break;
            if (!next())
                return false;
            if (!isUnicodeTValueComponent(m_current))
                return false;
            if (!next())
                return true;
            while (true) {
                if (!isUnicodeTValueComponent(m_current))
                    break;
                if (!next())
                    return true;
            }
        }
    }

    return found;
}

bool LanguageTagParser::parseOtherExtensionAfterPrefix()
{
    // (sep alphanum{2,8})+ ;
    ASSERT(!isEOS());
    if (!isUnicodeOtherExtensionValue(m_current))
        return false;
    if (!next())
        return true;

    while (true) {
        if (!isUnicodeOtherExtensionValue(m_current))
            return true;
        if (!next())
            return true;
    }
}

bool LanguageTagParser::parsePUExtensionAfterPrefix()
{
    // (sep alphanum{1,8})+ ;
    ASSERT(!isEOS());
    if (!isUnicodePUExtensionValue(m_current))
        return false;
    if (!next())
        return true;

    while (true) {
        if (!isUnicodePUExtensionValue(m_current))
            return true;
        if (!next())
            return true;
    }
}

bool LanguageTagParser::parseExtensionsAndPUExtensions()
{
    // unicode_locale_id    = unicode_language_id
    //                        extensions*
    //                        pu_extensions? ;
    //
    // extensions = unicode_locale_extensions
    //            | transformed_extensions
    //            | other_extensions ;
    //
    // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
    ASSERT(!isEOS());
    Bitmap<numberOfUnicodeSingletons> singletonsSet { };
    while (true) {
        if (m_current.length() != 1)
            return true;
        UChar prefixCode = m_current[0];
        if (!isASCIIAlphanumeric(prefixCode))
            return true;

        // https://tc39.es/ecma402/#sec-isstructurallyvalidlanguagetag
        // does not include duplicate singleton subtags.
        //
        // https://unicode.org/reports/tr35/#Unicode_locale_identifier
        // As is often the case, the complete syntactic constraints are not easily captured by ABNF,
        // so there is a further condition: There cannot be more than one extension with the same singleton (-a-, …, -t-, -u-, …).
        // Note that the private use extension (-x-) must come after all other extensions.
        if (singletonsSet.get(convertToUnicodeSingletonIndex(prefixCode)))
            return false;
        singletonsSet.set(convertToUnicodeSingletonIndex(prefixCode), true);

        switch (prefixCode) {
        case 'u':
        case 'U': {
            // unicode_locale_extensions = sep [uU] ((sep keyword)+ | (sep attribute)+ (sep keyword)*) ;
            if (!next())
                return false;
            if (!parseUnicodeExtensionAfterPrefix())
                return false;
            if (isEOS())
                return true;
            break; // Next extension.
        }
        case 't':
        case 'T': {
            // transformed_extensions = sep [tT] ((sep tlang (sep tfield)*) | (sep tfield)+) ;
            if (!next())
                return false;
            if (!parseTransformedExtensionAfterPrefix())
                return false;
            if (isEOS())
                return true;
            break; // Next extension.
        }
        case 'x':
        case 'X': {
            // pu_extensions = sep [xX] (sep alphanum{1,8})+ ;
            if (!next())
                return false;
            if (!parsePUExtensionAfterPrefix())
                return false;
            return true; // If pu_extensions appear, no extensions can follow after that. This must be the end of unicode_locale_id.
        }
        default: {
            // other_extensions = sep [alphanum-[tTuUxX]] (sep alphanum{2,8})+ ;
            if (!next())
                return false;
            if (!parseOtherExtensionAfterPrefix())
                return false;
            if (isEOS())
                return true;
            break; // Next extension.
        }
        }
    }
}

// https://tc39.es/ecma402/#sec-isstructurallyvalidlanguagetag
bool isStructurallyValidLanguageTag(StringView string)
{
    LanguageTagParser parser(string);
    if (!parser.parseUnicodeLocaleId())
        return false;
    if (!parser.isEOS())
        return false;
    return true;
}

// unicode_language_id, but intersection of BCP47 and UTS35.
// unicode_language_id =
//     | unicode_language_subtag (sep unicode_script_subtag)? (sep unicode_region_subtag)? (sep unicode_variant_subtag)* ;
// https://github.com/tc39/proposal-intl-displaynames/issues/79
bool isUnicodeLanguageId(StringView string)
{
    LanguageTagParser parser(string);
    if (!parser.parseUnicodeLanguageId())
        return false;
    if (!parser.isEOS())
        return false;
    return true;
}

bool isWellFormedCurrencyCode(StringView currency)
{
    return currency.length() == 3 && currency.isAllSpecialCharacters<isASCIIAlpha>();
}

EncodedJSValue JSC_HOST_CALL intlObjectFuncGetCanonicalLocales(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // Intl.getCanonicalLocales(locales)
    // https://tc39.github.io/ecma402/#sec-intl.getcanonicallocales

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> localeList = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto length = localeList.size();

    JSArray* localeArray = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), length);
    if (!localeArray) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    for (size_t i = 0; i < length; ++i) {
        localeArray->putDirectIndex(globalObject, i, jsString(vm, localeList[i]));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(localeArray);
}

} // namespace JSC
