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

#include <wtf/CrossThreadCopier.h>
#include <wtf/Language.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FontGenericFamilies);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FontGenericFamilies::Families);

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

static const String& genericFontFamilyForScript(const ScriptFontFamilyMap& fontMap, UScriptCode script)
{
    ScriptFontFamilyMap::const_iterator it = fontMap.find(static_cast<int>(script));
    if (it != fontMap.end())
        return it->value;
    // Content using USCRIPT_HAN doesn't tell us if we should be using Simplified Chinese or Traditional Chinese. In the
    // absence of all other signals, we consult with the user's system preferences.
    if (script == USCRIPT_HAN) {
        it = fontMap.find(static_cast<int>(userPrefersSimplifiedChinese() ? USCRIPT_SIMPLIFIED_HAN : USCRIPT_TRADITIONAL_HAN));
        if (it != fontMap.end())
            return it->value;
    }
    if (script != USCRIPT_COMMON)
        return genericFontFamilyForScript(fontMap, USCRIPT_COMMON);
    return emptyAtom();
}

FontGenericFamilies::FontGenericFamilies()
    : m_families(Families::create())
{
}

FontGenericFamilies FontGenericFamilies::isolatedCopy() const &
{
    FontGenericFamilies copy;
    Ref copyFamilies = copy.m_families.access();
    copyFamilies->standardFontFamilyMap = crossThreadCopy(m_families->standardFontFamilyMap);
    copyFamilies->serifFontFamilyMap = crossThreadCopy(m_families->serifFontFamilyMap);
    copyFamilies->fixedFontFamilyMap = crossThreadCopy(m_families->fixedFontFamilyMap);
    copyFamilies->sansSerifFontFamilyMap = crossThreadCopy(m_families->sansSerifFontFamilyMap);
    copyFamilies->cursiveFontFamilyMap = crossThreadCopy(m_families->cursiveFontFamilyMap);
    copyFamilies->fantasyFontFamilyMap = crossThreadCopy(m_families->fantasyFontFamilyMap);
    copyFamilies->pictographFontFamilyMap = crossThreadCopy(m_families->pictographFontFamilyMap);
    copyFamilies->mathFontFamilyMap = crossThreadCopy(m_families->mathFontFamilyMap);
    return copy;
}

FontGenericFamilies FontGenericFamilies::isolatedCopy() &&
{
    Ref families = m_families.access();
    FontGenericFamilies copy;
    Ref copyFamilies = copy.m_families.access();
    copyFamilies->standardFontFamilyMap = crossThreadCopy(WTF::move(families->standardFontFamilyMap));
    copyFamilies->serifFontFamilyMap = crossThreadCopy(WTF::move(families->serifFontFamilyMap));
    copyFamilies->fixedFontFamilyMap = crossThreadCopy(WTF::move(families->fixedFontFamilyMap));
    copyFamilies->sansSerifFontFamilyMap = crossThreadCopy(WTF::move(families->sansSerifFontFamilyMap));
    copyFamilies->cursiveFontFamilyMap = crossThreadCopy(WTF::move(families->cursiveFontFamilyMap));
    copyFamilies->fantasyFontFamilyMap = crossThreadCopy(WTF::move(families->fantasyFontFamilyMap));
    copyFamilies->pictographFontFamilyMap = crossThreadCopy(WTF::move(families->pictographFontFamilyMap));
    copyFamilies->mathFontFamilyMap = crossThreadCopy(WTF::move(families->mathFontFamilyMap));
    return copy;
}

const String& FontGenericFamilies::standardFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_families->standardFontFamilyMap, script);
}

const String& FontGenericFamilies::fixedFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_families->fixedFontFamilyMap, script);
}

const String& FontGenericFamilies::serifFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_families->serifFontFamilyMap, script);
}

const String& FontGenericFamilies::sansSerifFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_families->sansSerifFontFamilyMap, script);
}

const String& FontGenericFamilies::cursiveFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_families->cursiveFontFamilyMap, script);
}

const String& FontGenericFamilies::fantasyFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_families->fantasyFontFamilyMap, script);
}

const String& FontGenericFamilies::pictographFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_families->pictographFontFamilyMap, script);
}

const String& FontGenericFamilies::mathFontFamily(UScriptCode script) const
{
    return genericFontFamilyForScript(m_families->mathFontFamilyMap, script);
}

bool FontGenericFamilies::setStandardFontFamily(const String& family, UScriptCode script)
{
    return setFontFamily(&Families::standardFontFamilyMap, family, script);
}

bool FontGenericFamilies::setFixedFontFamily(const String& family, UScriptCode script)
{
    return setFontFamily(&Families::fixedFontFamilyMap, family, script);
}

bool FontGenericFamilies::setSerifFontFamily(const String& family, UScriptCode script)
{
    return setFontFamily(&Families::serifFontFamilyMap, family, script);
}

bool FontGenericFamilies::setSansSerifFontFamily(const String& family, UScriptCode script)
{
    return setFontFamily(&Families::sansSerifFontFamilyMap, family, script);
}

bool FontGenericFamilies::setCursiveFontFamily(const String& family, UScriptCode script)
{
    return setFontFamily(&Families::cursiveFontFamilyMap, family, script);
}

bool FontGenericFamilies::setFantasyFontFamily(const String& family, UScriptCode script)
{
    return setFontFamily(&Families::fantasyFontFamilyMap, family, script);
}

bool FontGenericFamilies::setPictographFontFamily(const String& family, UScriptCode script)
{
    return setFontFamily(&Families::pictographFontFamilyMap, family, script);
}

bool FontGenericFamilies::setMathFontFamily(const String& family, UScriptCode script)
{
    return setFontFamily(&Families::mathFontFamilyMap, family, script);
}

bool FontGenericFamilies::setFontFamily(ScriptFontFamilyMap Families::* map, const String& family, UScriptCode script)
{
    auto& currentMap = m_families.get().*map;
    if (family.isEmpty() ? !currentMap.contains(static_cast<int>(script)) : currentMap.get(static_cast<int>(script)) == family)
        return false;
    return setGenericFontFamilyForScript(m_families.access().*map, family, script);
}

bool FontGenericFamilies::Families::operator==(const Families& other) const
{
    return standardFontFamilyMap == other.standardFontFamilyMap
        && serifFontFamilyMap == other.serifFontFamilyMap
        && fixedFontFamilyMap == other.fixedFontFamilyMap
        && sansSerifFontFamilyMap == other.sansSerifFontFamilyMap
        && cursiveFontFamilyMap == other.cursiveFontFamilyMap
        && fantasyFontFamilyMap == other.fantasyFontFamilyMap
        && pictographFontFamilyMap == other.pictographFontFamilyMap
        && mathFontFamilyMap == other.mathFontFamilyMap;
}

const String* FontGenericFamilies::fontFamily(FamilyNamesIndex family, UScriptCode script) const
{
    switch (family) {
    case FamilyNamesIndex::CursiveFamily:
        return &cursiveFontFamily(script);
    case FamilyNamesIndex::FantasyFamily:
        return &fantasyFontFamily(script);
    case FamilyNamesIndex::MathFamily:
        return &mathFontFamily(script);
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
