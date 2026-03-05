/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include "FontCascadeDescription.h"
#include "Logging.h"
#include "SystemFontDatabaseCoreText.h"

namespace WebCore {

static inline Vector<RetainPtr<CTFontDescriptorRef>> systemFontCascadeList(const FontDescription& description, const AtomString& cssFamily, SystemFontKind systemFontKind, AllowUserInstalledFonts allowUserInstalledFonts)
{
    return SystemFontDatabaseCoreText::forCurrentThread().cascadeList(description, cssFamily, systemFontKind, allowUserInstalledFonts);
}

unsigned FontCascadeDescription::effectiveFamilyCount() const
{
    // FIXME: Move all the other system font keywords from fontDescriptorWithFamilySpecialCase() to here.
    unsigned result = 0;
    for (unsigned i = 0; i < familyCount(); ++i) {
        const auto& cssFamily = familyAt(i);
        if (auto use = SystemFontDatabaseCoreText::forCurrentThread().matchSystemFontUse(cssFamily))
            result += systemFontCascadeList(*this, cssFamily, *use, shouldAllowUserInstalledFonts()).size();
        else
            ++result;
    }
    return result;
}

FontFamilySpecification FontCascadeDescription::effectiveFamilyAt(unsigned index) const
{
    // The special cases in this function need to match the behavior in FontCacheCoreText.cpp. This code
    // is used for regular (element style) lookups, and the code in FontDescriptionCocoa.cpp is used when
    // src:local(special-cased-name) is specified inside an @font-face block.
    // FIXME: Currently, an @font-face block corresponds to a single item in the font-family: fallback list, which
    // means that "src:local(system-ui)" can't follow the Core Text cascade list (the way it does for regular lookups).
    // These two behaviors should be unified, which would hopefully allow us to delete this duplicate code.
    for (unsigned i = 0; i < familyCount(); ++i) {
        const auto& cssFamily = familyAt(i);
        if (auto use = SystemFontDatabaseCoreText::forCurrentThread().matchSystemFontUse(cssFamily)) {
            auto cascadeList = systemFontCascadeList(*this, cssFamily, *use, shouldAllowUserInstalledFonts());
            if (index < cascadeList.size())
                return FontFamilySpecification(cascadeList[index].get());
            index -= cascadeList.size();
        }
        else if (!index)
            return cssFamily;
        else
            --index;
    }
    ASSERT_NOT_REACHED();
    return nullAtom();
}

AtomString FontDescription::platformResolveGenericFamily(UScriptCode script, const AtomString& locale, const AtomString& familyName)
{
    ASSERT((locale.isNull() && script == USCRIPT_COMMON) || !locale.isNull());
    if (script == USCRIPT_COMMON)
        return nullAtom();

    String result;
    // FIXME: Use the system font database to handle standardFamily
    if (familyName == serifFamily)
        result = SystemFontDatabaseCoreText::forCurrentThread().serifFamily(locale.string());
    else if (familyName == sansSerifFamily)
        result = SystemFontDatabaseCoreText::forCurrentThread().sansSerifFamily(locale.string());
    else if (familyName == cursiveFamily)
        result = SystemFontDatabaseCoreText::forCurrentThread().cursiveFamily(locale.string());
    else if (familyName == fantasyFamily)
        result = SystemFontDatabaseCoreText::forCurrentThread().fantasyFamily(locale.string());
    else if (familyName == monospaceFamily)
        result = SystemFontDatabaseCoreText::forCurrentThread().monospaceFamily(locale.string());
    else
        return nullAtom();

    // Per CTFont.h: "Any font name beginning with a '.' is reserved for the system" and should be
    // created using CTFontCreateUIFontForLanguage() or similar APIs, not through standard font lookup.
    // CoreText sometimes returns these system-internal font names (e.g., ".Times Fallback") when
    // resolving CSS generic families for certain locales (rdar://139338599). Since WebKit's font
    // lookup cannot properly handle these reserved names, we reject them and fall back to
    // settings-based resolution instead.
    auto isValidFontName = [](const String& fontName) {
        if (fontName.isEmpty())
            return false;
        if (fontName.startsWith('.')) {
            LOG(Fonts, "CoreText returned reserved font name '%s'; using settings-based font resolution instead", fontName.utf8().data());
            return false;
        }
        return true;
    };

    if (!isValidFontName(result))
        return nullAtom();

    return AtomString { result };
}

}

