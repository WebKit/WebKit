/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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

static std::variant<TextBreakIteratorICU, TextBreakIteratorPlatform> mapModeToBackingIterator(StringView string, TextBreakIterator::Mode mode, const AtomString& locale)
{
    switch (mode) {
    case TextBreakIterator::Mode::Line:
        return TextBreakIteratorICU(string, TextBreakIteratorICU::Mode::Line, locale.string().utf8().data());
    case TextBreakIterator::Mode::Caret:
        return TextBreakIteratorCF(string, TextBreakIteratorCF::Mode::ComposedCharacter, locale);
    case TextBreakIterator::Mode::Delete:
        return TextBreakIteratorCF(string, TextBreakIteratorCF::Mode::BackwardDeletion, locale);
    case TextBreakIterator::Mode::Character:
        return TextBreakIteratorCF(string, TextBreakIteratorCF::Mode::ComposedCharacter, locale);
    }
}

TextBreakIterator::TextBreakIterator(StringView string, Mode mode, const AtomString& locale)
    : m_backing(mapModeToBackingIterator(string, mode, locale))
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
