/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include <wtf/text/TextBreakIteratorInternalICU.h>

#include <array>
#include <wtf/RetainPtr.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/TextBreakIterator.h>

namespace WTF {

// Buffer sized to hold ASCII locale ID strings up to 32 characters long.
using LocaleIDBuffer = std::array<char, 33>;

TextBreakIterator::Backing TextBreakIterator::mapModeToBackingIterator(StringView string, std::span<const char16_t> priorContext, Mode mode, ContentAnalysis contentAnalysis, const AtomString& locale)
{
    return switchOn(mode, [string, priorContext, contentAnalysis, &locale](TextBreakIterator::LineMode lineMode) -> TextBreakIterator::Backing {
        if (contentAnalysis == ContentAnalysis::Linguistic && lineMode.behavior == LineMode::Behavior::Default)
            return TextBreakIteratorCF(string, priorContext, TextBreakIteratorCF::Mode::LineBreak, locale);
        return TextBreakIteratorICU(string, priorContext, TextBreakIteratorICU::LineMode { lineMode.behavior }, locale);
    }, [string, priorContext, &locale](TextBreakIterator::CaretMode) -> TextBreakIterator::Backing {
        return TextBreakIteratorCF(string, priorContext, TextBreakIteratorCF::Mode::ComposedCharacter, locale);
    }, [string, priorContext, &locale](TextBreakIterator::DeleteMode) -> TextBreakIterator::Backing {
        return TextBreakIteratorCF(string, priorContext, TextBreakIteratorCF::Mode::BackwardDeletion, locale);
    }, [string, priorContext, &locale](TextBreakIterator::CharacterMode) -> TextBreakIterator::Backing {
        return TextBreakIteratorCF(string, priorContext, TextBreakIteratorCF::Mode::ComposedCharacter, locale);
    });
}

TextBreakIterator::TextBreakIterator(StringView string, std::span<const char16_t> priorContext, Mode mode, ContentAnalysis contentAnalysis, const AtomString& locale)
    : m_backing(mapModeToBackingIterator(string, priorContext, mode, contentAnalysis, locale))
    , m_mode(mode)
    , m_locale(locale)
{
}

static RetainPtr<CFStringRef> textBreakLocalePreference()
{
    return dynamic_cf_cast<CFStringRef>(adoptCF(CFPreferencesCopyValue(CFSTR("AppleTextBreakLocale"),
        kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost)));
}

static RetainPtr<CFStringRef> topLanguagePreference()
{
    auto languagesArray = adoptCF(CFLocaleCopyPreferredLanguages());
    if (!languagesArray || !CFArrayGetCount(languagesArray.get()))
        return nullptr;
    return static_cast<CFStringRef>(CFArrayGetValueAtIndex(languagesArray.get(), 0));
}

static LocaleIDBuffer localeIDInBuffer(CFStringRef string)
{
    // Empty string means "root locale", and is what we use if we can't get a preference.
    LocaleIDBuffer buffer;
    if (!string || !CFStringGetCString(string, buffer.data(), buffer.size(), kCFStringEncodingASCII))
        buffer.front() = '\0';
    return buffer;
}

const char* currentSearchLocaleID()
{
    static const auto buffer = localeIDInBuffer(topLanguagePreference().get());
    return buffer.data();
}

static RetainPtr<CFStringRef> textBreakLocale()
{
    // If there is no text break locale, use the top language preference.
    auto locale = textBreakLocalePreference();
    if (!locale)
        return topLanguagePreference();
    if (auto canonicalLocale = adoptCF(CFLocaleCreateCanonicalLanguageIdentifierFromString(kCFAllocatorDefault, locale.get())))
        return canonicalLocale;
    return locale;
}

const char* currentTextBreakLocaleID()
{
    static const auto buffer = localeIDInBuffer(textBreakLocale().get());
    return buffer.data();
}

}
