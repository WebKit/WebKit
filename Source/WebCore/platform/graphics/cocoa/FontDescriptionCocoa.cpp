/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "FontDescription.h"

#include "SystemFontDatabaseCoreText.h"

#if USE_PLATFORM_SYSTEM_FALLBACK_LIST

namespace WebCore {

static inline bool isSystemFontString(const AtomicString& string)
{
    return equalLettersIgnoringASCIICase(string, "-webkit-system-font")
        || equalLettersIgnoringASCIICase(string, "-apple-system")
        || equalLettersIgnoringASCIICase(string, "-apple-system-font")
        || equalLettersIgnoringASCIICase(string, "system-ui");
}

#if PLATFORM(IOS_FAMILY)

template<typename T, typename U, std::size_t size, std::size_t... indices> std::array<T, size> convertArray(U (&array)[size], std::index_sequence<indices...>)
{
    return { { array[indices]... } };
}

template<typename T, typename U, std::size_t size> inline std::array<T, size> convertArray(U (&array)[size])
{
    return convertArray<T>(array, std::make_index_sequence<size> { });
}

static inline bool isUIFontTextStyle(const AtomicString& string)
{
    static const CFStringRef styles[] = {
        kCTUIFontTextStyleHeadline,
        kCTUIFontTextStyleBody,
        kCTUIFontTextStyleTitle1,
        kCTUIFontTextStyleTitle2,
        kCTUIFontTextStyleTitle3,
        kCTUIFontTextStyleSubhead,
        kCTUIFontTextStyleFootnote,
        kCTUIFontTextStyleCaption1,
        kCTUIFontTextStyleCaption2,
        kCTUIFontTextStyleShortHeadline,
        kCTUIFontTextStyleShortBody,
        kCTUIFontTextStyleShortSubhead,
        kCTUIFontTextStyleShortFootnote,
        kCTUIFontTextStyleShortCaption1,
        kCTUIFontTextStyleTallBody,
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
        kCTUIFontTextStyleTitle0,
        kCTUIFontTextStyleTitle4,
#endif
    };
    
    static auto strings { makeNeverDestroyed(convertArray<AtomicString>(styles)) };
    return std::find(strings.get().begin(), strings.get().end(), string) != strings.get().end();
}
#endif

static inline Vector<RetainPtr<CTFontDescriptorRef>> systemFontCascadeList(const FontCascadeDescription& description, const AtomicString& cssFamily, SystemFontDatabaseCoreText::ClientUse clientUse, AllowUserInstalledFonts allowUserInstalledFonts)
{
    return SystemFontDatabaseCoreText::singleton().cascadeList(description, cssFamily, clientUse, allowUserInstalledFonts);
}

unsigned FontCascadeDescription::effectiveFamilyCount() const
{
    // FIXME: Move all the other system font keywords from platformFontWithFamilySpecialCase() to here.
    unsigned result = 0;
    for (unsigned i = 0; i < familyCount(); ++i) {
        const auto& cssFamily = familyAt(i);
        if (isSystemFontString(cssFamily))
            result += systemFontCascadeList(*this, cssFamily, SystemFontDatabaseCoreText::ClientUse::ForSystemUI, shouldAllowUserInstalledFonts()).size();
#if PLATFORM(IOS_FAMILY)
        else if (isUIFontTextStyle(cssFamily))
            result += systemFontCascadeList(*this, cssFamily, SystemFontDatabaseCoreText::ClientUse::ForTextStyle, shouldAllowUserInstalledFonts()).size();
#endif
        else
            ++result;
    }
    return result;
}

FontFamilySpecification FontCascadeDescription::effectiveFamilyAt(unsigned index) const
{
    // The special cases in this function need to match the behavior in FontCacheIOS.mm and FontCacheMac.mm. On systems
    // where USE_PLATFORM_SYSTEM_FALLBACK_LIST is set to true, this code is used for regular (element style) lookups,
    // and the code in FontDescriptionCocoa.cpp is used when src:local(special-cased-name) is specified inside an
    // @font-face block.
    // FIXME: Currently, an @font-face block corresponds to a single item in the font-family: fallback list, which
    // means that "src:local(system-ui)" can't follow the Core Text cascade list (the way it does for regular lookups).
    // These two behaviors should be unified, which would hopefully allow us to delete this duplicate code.
    for (unsigned i = 0; i < familyCount(); ++i) {
        const auto& cssFamily = familyAt(i);
        if (isSystemFontString(cssFamily)) {
            auto cascadeList = systemFontCascadeList(*this, cssFamily, SystemFontDatabaseCoreText::ClientUse::ForSystemUI, shouldAllowUserInstalledFonts());
            if (index < cascadeList.size())
                return FontFamilySpecification(cascadeList[index].get());
            index -= cascadeList.size();
        }
#if PLATFORM(IOS_FAMILY)
        else if (isUIFontTextStyle(cssFamily)) {
            auto cascadeList = systemFontCascadeList(*this, cssFamily, SystemFontDatabaseCoreText::ClientUse::ForTextStyle, shouldAllowUserInstalledFonts());
            if (index < cascadeList.size())
                return FontFamilySpecification(cascadeList[index].get());
            index -= cascadeList.size();
        }
#endif
        else if (!index)
            return cssFamily;
        else
            --index;
    }
    ASSERT_NOT_REACHED();
    return nullAtom();
}

}

#endif

