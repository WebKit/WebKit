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

static bool setGenericFontFamilyForScript(ScriptFontFamilyMap& fontMap, const AtomString& family, UScriptCode script)
{
    if (family.isEmpty())
        return fontMap.remove(static_cast<int>(script));
    auto& familyInMap = fontMap.add(static_cast<int>(script), AtomString { }).iterator->value;
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

static const AtomString& genericFontFamilyForScript(const ScriptFontFamilyMap& fontMap, UScriptCode script)
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
    addLanguageChangeObserver(this, &languageChanged);
    languageChanged(nullptr);
}

const AtomString& FontGenericFamilies::standardFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_standardFontFamilyMap, script);
}

const AtomString& FontGenericFamilies::fixedFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_fixedFontFamilyMap, script);
}

const AtomString& FontGenericFamilies::serifFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_serifFontFamilyMap, script);
}

const AtomString& FontGenericFamilies::sansSerifFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_sansSerifFontFamilyMap, script);
}

const AtomString& FontGenericFamilies::cursiveFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_cursiveFontFamilyMap, script);
}

const AtomString& FontGenericFamilies::fantasyFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_fantasyFontFamilyMap, script);
}

const AtomString& FontGenericFamilies::pictographFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_pictographFontFamilyMap, script);
}

bool FontGenericFamilies::setStandardFontFamily(const AtomString& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_standardFontFamilyMap, family, script);
}

bool FontGenericFamilies::setFixedFontFamily(const AtomString& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_fixedFontFamilyMap, family, script);
}

bool FontGenericFamilies::setSerifFontFamily(const AtomString& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_serifFontFamilyMap, family, script);
}

bool FontGenericFamilies::setSansSerifFontFamily(const AtomString& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_sansSerifFontFamilyMap, family, script);
}

bool FontGenericFamilies::setCursiveFontFamily(const AtomString& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_cursiveFontFamilyMap, family, script);
}

bool FontGenericFamilies::setFantasyFontFamily(const AtomString& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_fantasyFontFamilyMap, family, script);
}

bool FontGenericFamilies::setPictographFontFamily(const AtomString& family, UScriptCode script)
{
    return setGenericFontFamilyForScript(m_pictographFontFamilyMap, family, script);
}

}
