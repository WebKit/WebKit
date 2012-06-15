/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Settings.h"

#include "LocaleToScriptMapping.h"

#include <BlackBerryPlatformFontInfo.h>

namespace WebCore {

static const char* languages[] = {
    "ar",
    "bn",
    "gu",
    "he",
    "hi",
    "ja",
    "kn",
    "ko",
    "ml",
    "pa",
    "ta",
    "te",
    "th",
    "zh-CN",
    "zh-TW",
};

void Settings::initializeDefaultFontFamilies()
{
    setCursiveFontFamily(BlackBerry::Platform::fontFamily("-webkit-cursive", "").c_str());
    setFantasyFontFamily(BlackBerry::Platform::fontFamily("-webkit-fantasy", "").c_str());
    setFixedFontFamily(BlackBerry::Platform::fontFamily("-webkit-monospace", "").c_str());
    setSansSerifFontFamily(BlackBerry::Platform::fontFamily("-webkit-sans-serif", "").c_str());
    setSerifFontFamily(BlackBerry::Platform::fontFamily("-webkit-serif", "").c_str());
    setStandardFontFamily(BlackBerry::Platform::fontFamily("-webkit-standard", "").c_str());

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(languages); ++i) {
        UScriptCode script = localeToScriptCodeForFontSelection(languages[i]);
        setFixedFontFamily(BlackBerry::Platform::fontFamily("monospace", languages[i]).c_str(), script);
        setSansSerifFontFamily(BlackBerry::Platform::fontFamily("sans-serif", languages[i]).c_str(), script);
        setSerifFontFamily(BlackBerry::Platform::fontFamily("serif", languages[i]).c_str(), script);
        setStandardFontFamily(BlackBerry::Platform::fontFamily("", languages[i]).c_str(), script);
    }
}

} // namespace WebCore
