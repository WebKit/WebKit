/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#pragma once

#include "JSCJSValueInlines.h"
#include "JSObject.h"

struct UFieldPositionIterator;

namespace JSC {

enum class LocaleMatcher : uint8_t {
    Lookup,
    BestFit,
};

#define JSC_INTL_RELEVANT_EXTENSION_KEYS(macro) \
    macro(ca, Ca) /* calendar */ \
    macro(co, Co) /* collation */ \
    macro(hc, Hc) /* hour-cycle */ \
    macro(kf, Kf) /* case-first */ \
    macro(kn, Kn) /* numeric */ \
    macro(nu, Nu) /* numbering-system */ \

enum class RelevantExtensionKey : uint8_t {
#define JSC_DECLARE_INTL_RELEVANT_EXTENSION_KEYS(lowerName, capitalizedName) capitalizedName,
JSC_INTL_RELEVANT_EXTENSION_KEYS(JSC_DECLARE_INTL_RELEVANT_EXTENSION_KEYS)
#undef JSC_DECLARE_INTL_RELEVANT_EXTENSION_KEYS
};
#define JSC_COUNT_INTL_RELEVANT_EXTENSION_KEYS(lowerName, capitalizedName) + 1
static constexpr uint8_t numberOfRelevantExtensionKeys = 0 JSC_INTL_RELEVANT_EXTENSION_KEYS(JSC_COUNT_INTL_RELEVANT_EXTENSION_KEYS);
#undef JSC_COUNT_INTL_RELEVANT_EXTENSION_KEYS

class IntlObject final : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(IntlObject, Base);
        return &vm.plainObjectSpace;
    }

    static IntlObject* create(VM&, JSGlobalObject*, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    DECLARE_INFO;

private:
    IntlObject(VM&, Structure*);
    void finishCreation(VM&, JSGlobalObject*);
};

String defaultLocale(JSGlobalObject*);
const HashSet<String>& intlAvailableLocales();
const HashSet<String>& intlCollatorAvailableLocales();
inline const HashSet<String>& intlDateTimeFormatAvailableLocales() { return intlAvailableLocales(); }
inline const HashSet<String>& intlDisplayNamesAvailableLocales() { return intlAvailableLocales(); }
inline const HashSet<String>& intlNumberFormatAvailableLocales() { return intlAvailableLocales(); }
inline const HashSet<String>& intlPluralRulesAvailableLocales() { return intlAvailableLocales(); }
inline const HashSet<String>& intlRelativeTimeFormatAvailableLocales() { return intlAvailableLocales(); }

TriState intlBooleanOption(JSGlobalObject*, JSValue options, PropertyName);
String intlStringOption(JSGlobalObject*, JSValue options, PropertyName, std::initializer_list<const char*> values, const char* notFound, const char* fallback);
unsigned intlNumberOption(JSGlobalObject*, JSValue options, PropertyName, unsigned minimum, unsigned maximum, unsigned fallback);
unsigned intlDefaultNumberOption(JSGlobalObject*, JSValue, PropertyName, unsigned minimum, unsigned maximum, unsigned fallback);
Vector<char, 32> localeIDBufferForLanguageTag(const CString&);
String languageTagForLocaleID(const char*, bool isImmortal = false);
Vector<String> canonicalizeLocaleList(JSGlobalObject*, JSValue locales);

using ResolveLocaleOptions = std::array<Optional<String>, numberOfRelevantExtensionKeys>;
using RelevantExtensions = std::array<String, numberOfRelevantExtensionKeys>;
struct ResolvedLocale {
    String locale;
    String dataLocale;
    RelevantExtensions extensions;
};

ResolvedLocale resolveLocale(JSGlobalObject*, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales, LocaleMatcher, const ResolveLocaleOptions&, std::initializer_list<RelevantExtensionKey> relevantExtensionKeys, Vector<String> (*localeData)(const String&, RelevantExtensionKey));
JSValue supportedLocales(JSGlobalObject*, const HashSet<String>& availableLocales, const Vector<String>& requestedLocales, JSValue options);
String removeUnicodeLocaleExtension(const String& locale);
String bestAvailableLocale(const HashSet<String>& availableLocales, const String& requestedLocale);
template<typename Predicate> String bestAvailableLocale(const String& requestedLocale, Predicate);
Vector<String> numberingSystemsForLocale(const String& locale);

bool isUnicodeLocaleIdentifierType(StringView);

bool isUnicodeLanguageSubtag(StringView);
bool isUnicodeScriptSubtag(StringView);
bool isUnicodeRegionSubtag(StringView);
bool isUnicodeVariantSubtag(StringView);
bool isUnicodeLanguageId(StringView);

bool isWellFormedCurrencyCode(StringView);

struct UFieldPositionIteratorDeleter {
    void operator()(UFieldPositionIterator*) const;
};

} // namespace JSC
