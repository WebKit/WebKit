/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/WebKitFontFamilyNames.h>
#include <unicode/uscript.h>
#include <wtf/DataRef.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

// UScriptCode uses -1 and 0 for UScriptInvalidCode and UScriptCommon.
// We need to use -2 and -3 for empty value and deleted value.
struct UScriptCodeHashTraits : HashTraits<int> {
    static const bool emptyValueIsZero = false;
    static int emptyValue() { return -2; }
    static void constructDeletedValue(int& slot) { slot = -3; }
    static bool isDeletedValue(int value) { return value == -3; }
};

using ScriptFontFamilyMap = HashMap<int, String, DefaultHash<int>, UScriptCodeHashTraits>;

class FontGenericFamilies {
    WTF_MAKE_TZONE_ALLOCATED(FontGenericFamilies);
public:
    FontGenericFamilies();

    FontGenericFamilies isolatedCopy() const &;
    FontGenericFamilies isolatedCopy() &&;

    const String& standardFontFamily(UScriptCode = USCRIPT_COMMON) const;
    const String& fixedFontFamily(UScriptCode = USCRIPT_COMMON) const;
    const String& serifFontFamily(UScriptCode = USCRIPT_COMMON) const;
    const String& sansSerifFontFamily(UScriptCode = USCRIPT_COMMON) const;
    const String& cursiveFontFamily(UScriptCode = USCRIPT_COMMON) const;
    const String& fantasyFontFamily(UScriptCode = USCRIPT_COMMON) const;
    const String& pictographFontFamily(UScriptCode = USCRIPT_COMMON) const;
    const String& mathFontFamily(UScriptCode = USCRIPT_COMMON) const;

    const String* fontFamily(WebKitFontFamilyNames::FamilyNamesIndex, UScriptCode = USCRIPT_COMMON) const;

    bool setStandardFontFamily(const String&, UScriptCode);
    bool setFixedFontFamily(const String&, UScriptCode);
    bool setSerifFontFamily(const String&, UScriptCode);
    bool setSansSerifFontFamily(const String&, UScriptCode);
    bool setCursiveFontFamily(const String&, UScriptCode);
    bool setFantasyFontFamily(const String&, UScriptCode);
    bool setPictographFontFamily(const String&, UScriptCode);
    bool setMathFontFamily(const String&, UScriptCode);

    bool operator==(const FontGenericFamilies&) const = default;

private:
    struct Families : RefCounted<Families> {
        WTF_MAKE_TZONE_ALLOCATED(Families);
    public:
        static Ref<Families> create() { return adoptRef(*new Families); }
        Ref<Families> copy() const { return adoptRef(*new Families(*this)); }

        bool operator==(const Families&) const;

        ScriptFontFamilyMap standardFontFamilyMap;
        ScriptFontFamilyMap serifFontFamilyMap;
        ScriptFontFamilyMap fixedFontFamilyMap;
        ScriptFontFamilyMap sansSerifFontFamilyMap;
        ScriptFontFamilyMap cursiveFontFamilyMap;
        ScriptFontFamilyMap fantasyFontFamilyMap;
        ScriptFontFamilyMap pictographFontFamilyMap;
        ScriptFontFamilyMap mathFontFamilyMap;

    private:
        Families() = default;
        Families(const Families& other)
            : standardFontFamilyMap(other.standardFontFamilyMap)
            , serifFontFamilyMap(other.serifFontFamilyMap)
            , fixedFontFamilyMap(other.fixedFontFamilyMap)
            , sansSerifFontFamilyMap(other.sansSerifFontFamilyMap)
            , cursiveFontFamilyMap(other.cursiveFontFamilyMap)
            , fantasyFontFamilyMap(other.fantasyFontFamilyMap)
            , pictographFontFamilyMap(other.pictographFontFamilyMap)
            , mathFontFamilyMap(other.mathFontFamilyMap)
        {
        }
    };

    bool setFontFamily(ScriptFontFamilyMap Families::*, const String&, UScriptCode);

    DataRef<Families> m_families;
};

}
