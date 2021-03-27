/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontGenericFamilies.h"

#include <wtf/Language.h>

namespace WebCore {

using namespace WebKitFontFamilyNames;

static bool setGenericFontFamilyForScript(ScriptFontFamilyMap& fontMap, const String& family, UScriptCode script)
{
    if (family.isEmpty())
        return fontMap.remove(static_cast<int>(script));
    auto& familyInMap = fontMap.add(static_cast<int>(script), String { }).iterator->value;
    if (familyInMap == family)
        return false;
    familyInMap = family;
    return true;
}

static inline bool computeUserPrefersSimplified()
{
    const Vector<String>& preferredLanguages = userPreferredLanguages();
    for (auto& language : preferredLanguages) {
        if (equalIgnoringASCIICase(language, "zh-tw"))
            return false;
        if (equalIgnoringASCIICase(language, "zh-cn"))
            return true;
    }
    return true;
}

static bool& cachedUserPrefersSimplified()
{
    static bool cached = true;
    return cached;
}

static void languageChanged(void*)
{
    cachedUserPrefersSimplified() = computeUserPrefersSimplified();
}

static const String& genericFontFamilyForScript(const ScriptFontFamilyMap& fontMap, UScriptCode script)
{
    ScriptFontFamilyMap::const_iterator it = fontMap.find(static_cast<int>(script));
    if (it != fontMap.end())
        return it->value;
    // Content using USCRIPT_HAN doesn't tell us if we should be using Simplified Chinese or Traditional Chinese. In the
    // absence of all other signals, we consult with the user's system preferences.
    if (script == USCRIPT_HAN) {
        it = fontMap.find(static_cast<int>(cachedUserPrefersSimplified() ? USCRIPT_SIMPLIFIED_HAN : USCRIPT_TRADITIONAL_HAN));
        if (it != fontMap.end())
            return it->value;
    }
    if (script != USCRIPT_COMMON)
        return genericFontFamilyForScript(fontMap, USCRIPT_COMMON);
    return emptyAtom();
}

FontGenericFamilies::FontGenericFamilies()
{
    static std::once_flag once;
    std::call_once(once, [&] {
        addLanguageChangeObserver(&once, &languageChanged);
        languageChanged(nullptr);
    });
}

FontGenericFamilies FontGenericFamilies::isolatedCopy() const
{
    FontGenericFamilies copy;
    for (auto &keyValue : m_standardFontFamilyMap)
        copy.m_standardFontFamilyMap.add(keyValue.key, keyValue.value.isolatedCopy());
    for (auto &keyValue : m_serifFontFamilyMap)
        copy.m_serifFontFamilyMap.add(keyValue.key, keyValue.value.isolatedCopy());
    for (auto &keyValue : m_fixedFontFamilyMap)
        copy.m_fixedFontFamilyMap.add(keyValue.key, keyValue.value.isolatedCopy());
    for (auto &keyValue : m_sansSerifFontFamilyMap)
        copy.m_sansSerifFontFamilyMap.add(keyValue.key, keyValue.value.isolatedCopy());
    for (auto &keyValue : m_cursiveFontFamilyMap)
        copy.m_cursiveFontFamilyMap.add(keyValue.key, keyValue.value.isolatedCopy());
    for (auto &keyValue : m_fantasyFontFamilyMap)
        copy.m_fantasyFontFamilyMap.add(keyValue.key, keyValue.value.isolatedCopy());
    for (auto &keyValue : m_pictographFontFamilyMap)
        copy.m_pictographFontFamilyMap.add(keyValue.key, keyValue.value.isolatedCopy());
    return copy;
}

const String& FontGenericFamilies::standardFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_standardFontFamilyMap, script);
}

const String& FontGenericFamilies::fixedFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_fixedFontFamilyMap, script);
}

const String& FontGenericFamilies::serifFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_serifFontFamilyMap, script);
}

const String& FontGenericFamilies::sansSerifFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_sansSerifFontFamilyMap, script);
}

const String& FontGenericFamilies::cursiveFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_cursiveFontFamilyMap, script);
}

const String& FontGenericFamilies::fantasyFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_fantasyFontFamilyMap, script);
}

const String& FontGenericFamilies::pictographFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_pictographFontFamilyMap, script);
}

bool FontGenericFamilies::setStandardFontFamily(const String& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_standardFontFamilyMap, family, script);
}

bool FontGenericFamilies::setFixedFontFamily(const String& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_fixedFontFamilyMap, family, script);
}

bool FontGenericFamilies::setSerifFontFamily(const String& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_serifFontFamilyMap, family, script);
}

bool FontGenericFamilies::setSansSerifFontFamily(const String& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_sansSerifFontFamilyMap, family, script);
}

bool FontGenericFamilies::setCursiveFontFamily(const String& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_cursiveFontFamilyMap, family, script);
}

bool FontGenericFamilies::setFantasyFontFamily(const String& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_fantasyFontFamilyMap, family, script);
}

bool FontGenericFamilies::setPictographFontFamily(const String& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_pictographFontFamilyMap, family, script);
}

const String* FontGenericFamilies::fontFamily(FamilyNamesIndex family, UScriptCode script) const
{
    switch (family) {
    case FamilyNamesIndex::CursiveFamily:
        return &cursiveFontFamily(script);
    case FamilyNamesIndex::FantasyFamily:
        return &fantasyFontFamily(script);
    case FamilyNamesIndex::MonospaceFamily:
        return &fixedFontFamily(script);
    case FamilyNamesIndex::PictographFamily:
        return &pictographFontFamily(script);
    case FamilyNamesIndex::SansSerifFamily:
        return &sansSerifFontFamily(script);
    case FamilyNamesIndex::SerifFamily:
        return &serifFontFamily(script);
    case FamilyNamesIndex::StandardFamily:
        return &standardFontFamily(script);
    case FamilyNamesIndex::SystemUiFamily:
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

}
