/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WKPreferencesPrivate.h"

#include "FontSmoothingLevel.h"
#include "WKAPICast.h"
#include "WKPreferences.h"
#include "WebPreferences.h"

using namespace WebKit;

void WKPreferencesSetFontSmoothingLevel(WKPreferencesRef preferencesRef, WKFontSmoothingLevel wkLevel)
{
    FontSmoothingLevel level;
    switch (wkLevel) {
        case kWKFontSmoothingLevelNoSubpixelAntiAliasing:
            level = FontSmoothingLevelNoSubpixelAntiAliasing;
            break;
        case kWKFontSmoothingLevelLight:
            level = FontSmoothingLevelLight;
            break;
        case kWKFontSmoothingLevelMedium:
            level = FontSmoothingLevelMedium;
            break;
        case kWKFontSmoothingLevelStrong:
            level = FontSmoothingLevelStrong;
            break;
#if PLATFORM(WIN)
        case kWKFontSmoothingLevelWindows:
            level = FontSmoothingLevelWindows;
            break;
#endif
        default:
            ASSERT_NOT_REACHED();
            level = FontSmoothingLevelMedium;
            break;
    }
    toImpl(preferencesRef)->setFontSmoothingLevel(level);
}

WKFontSmoothingLevel WKPreferencesGetFontSmoothingLevel(WKPreferencesRef preferencesRef)
{
    FontSmoothingLevel level = toImpl(preferencesRef)->fontSmoothingLevel();
    switch (level) {
        case FontSmoothingLevelNoSubpixelAntiAliasing:
            return kWKFontSmoothingLevelNoSubpixelAntiAliasing;
        case FontSmoothingLevelLight:
            return kWKFontSmoothingLevelLight;
        case FontSmoothingLevelMedium:
            return kWKFontSmoothingLevelMedium;
        case FontSmoothingLevelStrong:
            return kWKFontSmoothingLevelStrong;
#if PLATFORM(WIN)
        case FontSmoothingLevelWindows:
            return kWKFontSmoothingLevelWindows;
#endif
    }

    ASSERT_NOT_REACHED();
    return kWKFontSmoothingLevelMedium;
}

