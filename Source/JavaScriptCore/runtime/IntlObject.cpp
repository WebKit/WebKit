/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2015 Sukolsak Sakshuwong (sukolsak@gmail.com)
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#if ENABLE(INTL)

#include "Error.h"
#include "FunctionPrototype.h"
#include "IntlCanonicalizeLanguage.h"
#include "IntlCollatorConstructor.h"
#include "IntlCollatorPrototype.h"
#include "IntlDateTimeFormatConstructor.h"
#include "IntlDateTimeFormatPrototype.h"
#include "IntlNumberFormatConstructor.h"
#include "IntlNumberFormatPrototype.h"
#include "IntlPluralRulesConstructor.h"
#include "IntlPluralRulesPrototype.h"
#include "JSCInlines.h"
#include "JSCJSValueInlines.h"
#include "Lookup.h"
#include "ObjectPrototype.h"
#include "Options.h"
#include <unicode/uloc.h>
#include <unicode/unumsys.h>
#include <wtf/Assertions.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlObject);

static EncodedJSValue JSC_HOST_CALL intlObjectFuncGetCanonicalLocales(ExecState*);

static JSValue createCollatorConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlCollatorConstructor::create(vm, IntlCollatorConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlCollatorPrototype*>(globalObject->collatorStructure()->storedPrototypeObject()));
}

static JSValue createNumberFormatConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlNumberFormatConstructor::create(vm, IntlNumberFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlNumberFormatPrototype*>(globalObject->numberFormatStructure()->storedPrototypeObject()));
}

static JSValue createDateTimeFormatConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlDateTimeFormatConstructor::create(vm, IntlDateTimeFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlDateTimeFormatPrototype*>(globalObject->dateTimeFormatStructure()->storedPrototypeObject()));
}

static JSValue createPluralRulesConstructor(VM& vm, JSObject* object)
{
    IntlObject* intlObject = jsCast<IntlObject*>(object);
    JSGlobalObject* globalObject = intlObject->globalObject(vm);
    return IntlPluralRulesConstructor::create(vm, IntlPluralRulesConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<IntlPluralRulesPrototype*>(globalObject->pluralRulesStructure()->storedPrototypeObject()));
}

}

#include "IntlObject.lut.h"

namespace JSC {

/* Source for IntlObject.lut.h
@begin intlObjectTable
  getCanonicalLocales   intlObjectFuncGetCanonicalLocales            DontEnum|Function 1
  Collator              createCollatorConstructor                    DontEnum|PropertyCallback
  DateTimeFormat        createDateTimeFormatConstructor              DontEnum|PropertyCallback
  NumberFormat          createNumberFormatConstructor                DontEnum|PropertyCallback
  PluralRules           createPluralRulesConstructor                 DontEnum|PropertyCallback
@end
*/

struct MatcherResult {
    String locale;
    String extension;
    size_t extensionIndex { 0 };
};

const ClassInfo IntlObject::s_info = { "Object", &Base::s_info, &intlObjectTable, nullptr, CREATE_METHOD_TABLE(IntlObject) };

IntlObject::IntlObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

IntlObject* IntlObject::create(VM& vm, Structure* structure)
{
    IntlObject* object = new (NotNull, allocateCell<IntlObject>(vm.heap)) IntlObject(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

String convertICULocaleToBCP47LanguageTag(const char* localeID)
{
    UErrorCode status = U_ZERO_ERROR;
    Vector<char, 32> buffer(32);
    auto length = uloc_toLanguageTag(localeID, buffer.data(), buffer.size(), false, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        buffer.grow(length);
        status = U_ZERO_ERROR;
        uloc_toLanguageTag(localeID, buffer.data(), buffer.size(), false, &status);
    }
    if (!U_FAILURE(status))
        return String(buffer.data(), length);
    return String();
}

bool intlBooleanOption(ExecState& state, JSValue options, PropertyName property, bool& usesFallback)
{
    // GetOption (options, property, type="boolean", values, fallback)
    // https://tc39.github.io/ecma402/#sec-getoption

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* opts = options.toObject(&state);
    RETURN_IF_EXCEPTION(scope, false);

    JSValue value = opts->get(&state, property);
    RETURN_IF_EXCEPTION(scope, false);

    if (!value.isUndefined()) {
        bool booleanValue = value.toBoolean(&state);
        usesFallback = false;
        return booleanValue;
    }

    // Because fallback can be undefined, we let the caller handle it instead.
    usesFallback = true;
    return false;
}

String intlStringOption(ExecState& state, JSValue options, PropertyName property, std::initializer_list<const char*> values, const char* notFound, const char* fallback)
{
    // GetOption (options, property, type="string", values, fallback)
    // https://tc39.github.io/ecma402/#sec-getoption

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* opts = options.toObject(&state);
    RETURN_IF_EXCEPTION(scope, String());

    JSValue value = opts->get(&state, property);
    RETURN_IF_EXCEPTION(scope, String());

    if (!value.isUndefined()) {
        String stringValue = value.toWTFString(&state);
        RETURN_IF_EXCEPTION(scope, String());

        if (values.size() && std::find(values.begin(), values.end(), stringValue) == values.end()) {
            throwException(&state, scope, createRangeError(&state, notFound));
            return { };
        }
        return stringValue;
    }

    return fallback;
}

unsigned intlNumberOption(ExecState& state, JSValue options, PropertyName property, unsigned minimum, unsigned maximum, unsigned fallback)
{
    // GetNumberOption (options, property, minimum, maximum, fallback)
    // https://tc39.github.io/ecma402/#sec-getnumberoption

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* opts = options.toObject(&state);
    RETURN_IF_EXCEPTION(scope, 0);

    JSValue value = opts->get(&state, property);
    RETURN_IF_EXCEPTION(scope, 0);

    RELEASE_AND_RETURN(scope, intlDefaultNumberOption(state, value, property, minimum, maximum, fallback));
}

unsigned intlDefaultNumberOption(ExecState& state, JSValue value, PropertyName property, unsigned minimum, unsigned maximum, unsigned fallback)
{
    // DefaultNumberOption (value, minimum, maximum, fallback)
    // https://tc39.github.io/ecma402/#sec-defaultnumberoption

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isUndefined()) {
        double doubleValue = value.toNumber(&state);
        RETURN_IF_EXCEPTION(scope, 0);

        if (!(doubleValue >= minimum && doubleValue <= maximum)) {
            throwException(&state, scope, createRangeError(&state, *property.publicName() + " is out of range"));
            return 0;
        }
        return static_cast<unsigned>(doubleValue);
    }
    return fallback;
}

static String privateUseLangTag(const Vector<String>& parts, size_t startIndex)
{
    size_t numParts = parts.size();
    size_t currentIndex = startIndex;

    // Check for privateuse.
    // privateuse = "x" 1*("-" (1*8alphanum))
    StringBuilder privateuse;
    while (currentIndex < numParts) {
        const String& singleton = parts[currentIndex];
        unsigned singletonLength = singleton.length();
        bool isValid = (singletonLength == 1 && (singleton == "x" || singleton == "X"));
        if (!isValid)
            break;

        if (currentIndex != startIndex)
            privateuse.append('-');

        ++currentIndex;
        unsigned numExtParts = 0;
        privateuse.append('x');
        while (currentIndex < numParts) {
            const String& extPart = parts[currentIndex];
            unsigned extPartLength = extPart.length();

            bool isValid = (extPartLength >= 1 && extPartLength <= 8 && extPart.isAllSpecialCharacters<isASCIIAlphanumeric>());
            if (!isValid)
                break;

            ++currentIndex;
            ++numExtParts;
            privateuse.append('-');
            privateuse.append(extPart.convertToASCIILowercase());
        }

        // Requires at least one production.
        if (!numExtParts)
            return String();
    }

    // Leftovers makes it invalid.
    if (currentIndex < numParts)
        return String();

    return privateuse.toString();
}

static String preferredLanguage(const String& language)
{
    auto preferred = intlPreferredLanguageTag(language);
    if (!preferred.isNull())
        return preferred;
    return language;
}

static String preferredRegion(const String& region)
{
    auto preferred = intlPreferredRegionTag(region);
    if (!preferred.isNull())
        return preferred;
    return region;

}

static String canonicalLangTag(const Vector<String>& parts)
{
    ASSERT(!parts.isEmpty());

    // Follows the grammar at https://www.rfc-editor.org/rfc/bcp/bcp47.txt
    // langtag = language ["-" script] ["-" region] *("-" variant) *("-" extension) ["-" privateuse]

    size_t numParts = parts.size();
    // Check for language.
    // language = 2*3ALPHA ["-" extlang] / 4ALPHA / 5*8ALPHA
    size_t currentIndex = 0;
    const String& language = parts[currentIndex];
    unsigned languageLength = language.length();
    bool canHaveExtlang = languageLength >= 2 && languageLength <= 3;
    bool isValidLanguage = languageLength >= 2 && languageLength <= 8 && language.isAllSpecialCharacters<isASCIIAlpha>();
    if (!isValidLanguage)
        return String();

    ++currentIndex;
    StringBuilder canonical;

    const String langtag = preferredLanguage(language.convertToASCIILowercase());
    canonical.append(langtag);

    // Check for extlang.
    // extlang = 3ALPHA *2("-" 3ALPHA)
    if (canHaveExtlang) {
        for (unsigned times = 0; times < 3 && currentIndex < numParts; ++times) {
            const String& extlang = parts[currentIndex];
            unsigned extlangLength = extlang.length();
            if (extlangLength == 3 && extlang.isAllSpecialCharacters<isASCIIAlpha>()) {
                ++currentIndex;
                auto extlangLower = extlang.convertToASCIILowercase();
                if (!times && intlPreferredExtlangTag(extlangLower) == langtag) {
                    canonical.clear();
                    canonical.append(extlangLower);
                    continue;
                }
                canonical.append('-');
                canonical.append(extlangLower);
            } else
                break;
        }
    }

    // Check for script.
    // script = 4ALPHA
    if (currentIndex < numParts) {
        const String& script = parts[currentIndex];
        unsigned scriptLength = script.length();
        if (scriptLength == 4 && script.isAllSpecialCharacters<isASCIIAlpha>()) {
            ++currentIndex;
            canonical.append('-');
            canonical.append(toASCIIUpper(script[0]));
            canonical.append(script.substring(1, 3).convertToASCIILowercase());
        }
    }

    // Check for region.
    // region = 2ALPHA / 3DIGIT
    if (currentIndex < numParts) {
        const String& region = parts[currentIndex];
        unsigned regionLength = region.length();
        bool isValidRegion = (
            (regionLength == 2 && region.isAllSpecialCharacters<isASCIIAlpha>())
            || (regionLength == 3 && region.isAllSpecialCharacters<isASCIIDigit>())
        );
        if (isValidRegion) {
            ++currentIndex;
            canonical.append('-');
            canonical.append(preferredRegion(region.convertToASCIIUppercase()));
        }
    }

    // Check for variant.
    // variant = 5*8alphanum / (DIGIT 3alphanum)
    HashSet<String> subtags;
    while (currentIndex < numParts) {
        const String& variant = parts[currentIndex];
        unsigned variantLength = variant.length();
        bool isValidVariant = (
            (variantLength >= 5 && variantLength <= 8 && variant.isAllSpecialCharacters<isASCIIAlphanumeric>())
            || (variantLength == 4 && isASCIIDigit(variant[0]) && variant.substring(1, 3).isAllSpecialCharacters<isASCIIAlphanumeric>())
        );
        if (!isValidVariant)
            break;

        // Cannot include duplicate subtags (case insensitive).
        String lowerVariant = variant.convertToASCIILowercase();
        if (!subtags.add(lowerVariant).isNewEntry)
            return String();

        ++currentIndex;

        // Reordering variant subtags is not required in the spec.
        canonical.append('-');
        canonical.append(lowerVariant);
    }

    // Check for extension.
    // extension = singleton 1*("-" (2*8alphanum))
    // singleton = alphanum except x or X
    subtags.clear();
    Vector<String> extensions;
    while (currentIndex < numParts) {
        const String& possibleSingleton = parts[currentIndex];
        unsigned singletonLength = possibleSingleton.length();
        bool isValidSingleton = (singletonLength == 1 && possibleSingleton != "x" && possibleSingleton != "X" && isASCIIAlphanumeric(possibleSingleton[0]));
        if (!isValidSingleton)
            break;

        // Cannot include duplicate singleton (case insensitive).
        String singleton = possibleSingleton.convertToASCIILowercase();
        if (!subtags.add(singleton).isNewEntry)
            return String();

        ++currentIndex;
        int numExtParts = 0;
        StringBuilder extension;
        extension.append(singleton);
        while (currentIndex < numParts) {
            const String& extPart = parts[currentIndex];
            unsigned extPartLength = extPart.length();

            bool isValid = (extPartLength >= 2 && extPartLength <= 8 && extPart.isAllSpecialCharacters<isASCIIAlphanumeric>());
            if (!isValid)
                break;

            ++currentIndex;
            ++numExtParts;
            extension.append('-');
            extension.append(extPart.convertToASCIILowercase());
        }

        // Requires at least one production.
        if (!numExtParts)
            return String();

        extensions.append(extension.toString());
    }

    // Add extensions to canonical sorted by singleton.
    std::sort(
        extensions.begin(),
        extensions.end(),
        [] (const String& a, const String& b) -> bool {
            return a[0] < b[0];
        }
    );
    size_t numExtenstions = extensions.size();
    for (size_t i = 0; i < numExtenstions; ++i) {
        canonical.append('-');
        canonical.append(extensions[i]);
    }

    // Check for privateuse.
    if (currentIndex < numParts) {
        String privateuse = privateUseLangTag(parts, currentIndex);
        if (privateuse.isNull())
            return String();
        canonical.append('-');
        canonical.append(privateuse);
    }

    const String tag = canonical.toString();
    const String preferred = intlRedundantLanguageTag(tag);
    if (!preferred.isNull())
        return preferred;
    return tag;
}

static String canonicalizeLanguageTag(const String& locale)
{
    // IsStructurallyValidLanguageTag (locale)
    // CanonicalizeLanguageTag (locale)
    // These are done one after another in CanonicalizeLocaleList, so they are combined here to reduce duplication.
    // https://www.rfc-editor.org/rfc/bcp/bcp47.txt

    // Language-Tag = langtag / privateuse / grandfathered
    String grandfather = intlGrandfatheredLanguageTag(locale.convertToASCIILowercase());
    if (!grandfather.isNull())
        return grandfather;

    Vector<String> parts = locale.splitAllowingEmptyEntries('-');
    if (!parts.isEmpty()) {
        String langtag = canonicalLangTag(parts);
        if (!langtag.isNull())
            return langtag;

        String privateuse = privateUseLangTag(parts, 0);
        if (!privateuse.isNull())
            return privateuse;
    }

    return String();
}

Vector<String> canonicalizeLocaleList(ExecState& state, JSValue locales)
{
    // CanonicalizeLocaleList (locales)
    // https://tc39.github.io/ecma402/#sec-canonicalizelocalelist

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSGlobalObject* globalObject = state.jsCallee()->globalObject(vm);
    Vector<String> seen;

    if (locales.isUndefined())
        return seen;

    JSObject* localesObject;
    if (locales.isString()) {
        JSArray* localesArray = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous));
        if (!localesArray) {
            throwOutOfMemoryError(&state, scope);
            RETURN_IF_EXCEPTION(scope, Vector<String>());
        }
        localesArray->push(&state, locales);
        RETURN_IF_EXCEPTION(scope, Vector<String>());

        localesObject = localesArray;
    } else {
        localesObject = locales.toObject(&state);
        RETURN_IF_EXCEPTION(scope, Vector<String>());
    }

    // 6. Let len be ToLength(Get(O, "length")).
    JSValue lengthProperty = localesObject->get(&state, vm.propertyNames->length);
    RETURN_IF_EXCEPTION(scope, Vector<String>());

    double length = lengthProperty.toLength(&state);
    RETURN_IF_EXCEPTION(scope, Vector<String>());

    HashSet<String> seenSet;
    for (double k = 0; k < length; ++k) {
        bool kPresent = localesObject->hasProperty(&state, k);
        RETURN_IF_EXCEPTION(scope, Vector<String>());

        if (kPresent) {
            JSValue kValue = localesObject->get(&state, k);
            RETURN_IF_EXCEPTION(scope, Vector<String>());

            if (!kValue.isString() && !kValue.isObject()) {
                throwTypeError(&state, scope, "locale value must be a string or object"_s);
                return Vector<String>();
            }

            JSString* tag = kValue.toString(&state);
            RETURN_IF_EXCEPTION(scope, Vector<String>());

            auto tagValue = tag->value(&state);
            RETURN_IF_EXCEPTION(scope, Vector<String>());

            String canonicalizedTag = canonicalizeLanguageTag(tagValue);
            if (canonicalizedTag.isNull()) {
                throwException(&state, scope, createRangeError(&state, "invalid language tag: " + tagValue));
                return Vector<String>();
            }

            if (seenSet.add(canonicalizedTag).isNewEntry)
                seen.append(canonicalizedTag);
        }
    }

    return seen;
}

String bestAvailableLocale(const HashSet<String>& availableLocales, const String& locale)
{
    // BestAvailableLocale (availableLocales, locale)
    // https://tc39.github.io/ecma402/#sec-bestavailablelocale

    String candidate = locale;
    while (!candidate.isEmpty()) {
        if (availableLocales.contains(candidate))
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

String defaultLocale(ExecState& state)
{
    // DefaultLocale ()
    // https://tc39.github.io/ecma402/#sec-defaultlocale

    // WebCore's global objects will have their own ideas of how to determine the language. It may
    // be determined by WebCore-specific logic like some WK settings. Usually this will return the
    // same thing as userPreferredLanguages()[0].
    VM& vm = state.vm();
    if (auto defaultLanguage = state.jsCallee()->globalObject(vm)->globalObjectMethodTable()->defaultLanguage) {
        String locale = canonicalizeLanguageTag(defaultLanguage());
        if (!locale.isEmpty())
            return locale;
    }

    Vector<String> languages = userPreferredLanguages();
    for (const auto& language : languages) {
        String locale = canonicalizeLanguageTag(language);
        if (!locale.isEmpty())
            return locale;
    }

    // If all else fails, ask ICU. It will probably say something bogus like en_us even if the user
    // has configured some other language, but being wrong is better than crashing.
    String locale = convertICULocaleToBCP47LanguageTag(uloc_getDefault());
    if (!locale.isEmpty())
        return locale;

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
            builder.append('-');
            builder.append(parts[p]);
        }
    }
    return builder.toString();
}

static MatcherResult lookupMatcher(ExecState& state, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales)
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
        result.locale = defaultLocale(state);
    return result;
}

static MatcherResult bestFitMatcher(ExecState& state, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales)
{
    // BestFitMatcher (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-bestfitmatcher

    // FIXME: Implement something better than lookup.
    return lookupMatcher(state, availableLocales, requestedLocales);
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

HashMap<String, String> resolveLocale(ExecState& state, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales, const HashMap<String, String>& options, const char* const relevantExtensionKeys[], size_t relevantExtensionKeyCount, Vector<String> (*localeData)(const String&, size_t))
{
    // ResolveLocale (availableLocales, requestedLocales, options, relevantExtensionKeys, localeData)
    // https://tc39.github.io/ecma402/#sec-resolvelocale

    const String& matcher = options.get("localeMatcher"_s);
    MatcherResult matcherResult = (matcher == "lookup")
        ? lookupMatcher(state, availableLocales, requestedLocales)
        : bestFitMatcher(state, availableLocales, requestedLocales);

    String foundLocale = matcherResult.locale;

    Vector<String> extensionSubtags;
    if (!matcherResult.extension.isNull())
        unicodeExtensionSubTags(matcherResult.extension, extensionSubtags);

    HashMap<String, String> result;
    result.add("dataLocale"_s, foundLocale);

    String supportedExtension = "-u"_s;
    for (size_t keyIndex = 0; keyIndex < relevantExtensionKeyCount; ++keyIndex) {
        const char* key = relevantExtensionKeys[keyIndex];
        Vector<String> keyLocaleData = localeData(foundLocale, keyIndex);
        ASSERT(!keyLocaleData.isEmpty());

        String value = keyLocaleData[0];
        String supportedExtensionAddition;

        if (!extensionSubtags.isEmpty()) {
            size_t keyPos = extensionSubtags.find(key);
            if (keyPos != notFound) {
                if (keyPos + 1 < extensionSubtags.size() && extensionSubtags[keyPos + 1].length() > 2) {
                    const String& requestedValue = extensionSubtags[keyPos + 1];
                    if (keyLocaleData.contains(requestedValue)) {
                        value = requestedValue;
                        supportedExtensionAddition = makeString('-', key, '-', value);
                    }
                } else if (keyLocaleData.contains(static_cast<String>("true"_s))) {
                    value = "true"_s;
                }
            }
        }

        HashMap<String, String>::const_iterator iterator = options.find(key);
        if (iterator != options.end()) {
            const String& optionsValue = iterator->value;
            // Undefined should not get added to the options, it won't displace the extension.
            // Null will remove the extension.
            if ((optionsValue.isNull() || keyLocaleData.contains(optionsValue)) && optionsValue != value) {
                value = optionsValue;
                supportedExtensionAddition = String();
            }
        }
        result.add(key, value);
        supportedExtension.append(supportedExtensionAddition);
    }

    if (supportedExtension.length() > 2) {
        String preExtension = foundLocale.substring(0, matcherResult.extensionIndex);
        String postExtension = foundLocale.substring(matcherResult.extensionIndex);
        foundLocale = preExtension + supportedExtension + postExtension;
    }

    result.add("locale"_s, foundLocale);
    return result;
}

static JSArray* lookupSupportedLocales(ExecState& state, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales)
{
    // LookupSupportedLocales (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-lookupsupportedlocales

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    size_t len = requestedLocales.size();
    JSGlobalObject* globalObject = state.jsCallee()->globalObject(vm);
    JSArray* subset = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!subset) {
        throwOutOfMemoryError(&state, scope);
        return nullptr;
    }

    unsigned index = 0;
    for (size_t k = 0; k < len; ++k) {
        const String& locale = requestedLocales[k];
        String noExtensionsLocale = removeUnicodeLocaleExtension(locale);
        String availableLocale = bestAvailableLocale(availableLocales, noExtensionsLocale);
        if (!availableLocale.isNull()) {
            subset->putDirectIndex(&state, index++, jsString(vm, locale));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    return subset;
}

static JSArray* bestFitSupportedLocales(ExecState& state, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales)
{
    // BestFitSupportedLocales (availableLocales, requestedLocales)
    // https://tc39.github.io/ecma402/#sec-bestfitsupportedlocales

    // FIXME: Implement something better than lookup.
    return lookupSupportedLocales(state, availableLocales, requestedLocales);
}

JSValue supportedLocales(ExecState& state, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales, JSValue options)
{
    // SupportedLocales (availableLocales, requestedLocales, options)
    // https://tc39.github.io/ecma402/#sec-supportedlocales

    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    String matcher;

    if (!options.isUndefined()) {
        matcher = intlStringOption(state, options, vm.propertyNames->localeMatcher, { "lookup", "best fit" }, "localeMatcher must be either \"lookup\" or \"best fit\"", "best fit");
        RETURN_IF_EXCEPTION(scope, JSValue());
    } else
        matcher = "best fit"_s;

    JSArray* supportedLocales = (matcher == "best fit")
        ? bestFitSupportedLocales(state, availableLocales, requestedLocales)
        : lookupSupportedLocales(state, availableLocales, requestedLocales);
    RETURN_IF_EXCEPTION(scope, JSValue());

    PropertyNameArray keys(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    supportedLocales->getOwnPropertyNames(supportedLocales, &state, keys, EnumerationMode());
    RETURN_IF_EXCEPTION(scope, JSValue());

    PropertyDescriptor desc;
    desc.setConfigurable(false);
    desc.setWritable(false);

    size_t len = keys.size();
    for (size_t i = 0; i < len; ++i) {
        supportedLocales->defineOwnProperty(supportedLocales, &state, keys[i], desc, true);
        RETURN_IF_EXCEPTION(scope, JSValue());
    }
    supportedLocales->defineOwnProperty(supportedLocales, &state, vm.propertyNames->length, desc, true);
    RETURN_IF_EXCEPTION(scope, JSValue());

    return supportedLocales;
}

Vector<String> numberingSystemsForLocale(const String& locale)
{
    static NeverDestroyed<Vector<String>> cachedNumberingSystems;
    Vector<String>& availableNumberingSystems = cachedNumberingSystems.get();

    if (UNLIKELY(availableNumberingSystems.isEmpty())) {
        static Lock cachedNumberingSystemsMutex;
        std::lock_guard<Lock> lock(cachedNumberingSystemsMutex);
        if (availableNumberingSystems.isEmpty()) {
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
                    availableNumberingSystems.append(String(result, resultLength));
                unumsys_close(numsys);
            }
            uenum_close(numberingSystemNames);
        }
    }

    UErrorCode status = U_ZERO_ERROR;
    UNumberingSystem* defaultSystem = unumsys_open(locale.utf8().data(), &status);
    ASSERT(U_SUCCESS(status));
    String defaultSystemName(unumsys_getName(defaultSystem));
    unumsys_close(defaultSystem);

    Vector<String> numberingSystems({ defaultSystemName });
    numberingSystems.appendVector(availableNumberingSystems);
    return numberingSystems;
}

EncodedJSValue JSC_HOST_CALL intlObjectFuncGetCanonicalLocales(ExecState* state)
{
    // Intl.getCanonicalLocales(locales)
    // https://tc39.github.io/ecma402/#sec-intl.getcanonicallocales

    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Vector<String> localeList = canonicalizeLocaleList(*state, state->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    auto length = localeList.size();

    JSGlobalObject* globalObject = state->jsCallee()->globalObject(vm);
    JSArray* localeArray = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), length);
    if (!localeArray) {
        throwOutOfMemoryError(state, scope);
        return encodedJSValue();
    }

    for (size_t i = 0; i < length; ++i) {
        localeArray->putDirectIndex(state, i, jsString(vm, localeList[i]));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    return JSValue::encode(localeArray);
}

} // namespace JSC

#endif // ENABLE(INTL)
