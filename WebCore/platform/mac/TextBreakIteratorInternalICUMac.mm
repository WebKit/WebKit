/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "TextBreakIteratorInternalICU.h"

namespace WebCore {

static const int maxLocaleStringLength = 32;

// This code was swiped from the CarbonCore UnicodeUtilities. One change from that is to use the empty
// string instead of the "old locale model" as the ultimate fallback. This change is per the UnicodeUtilities
// engineer.
static void getTextBreakLocale(char localeStringBuffer[maxLocaleStringLength])
{
    // Empty string means "root locale", which is what we use if we can't use a pref.

    // We get the parts string from AppleTextBreakLocale pref.
    // If that fails then look for the first language in the AppleLanguages pref.
    CFStringRef prefLocaleStr = (CFStringRef)CFPreferencesCopyValue(CFSTR("AppleTextBreakLocale"),
        kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if (!prefLocaleStr) {
        CFArrayRef appleLangArr = (CFArrayRef)CFPreferencesCopyValue(CFSTR("AppleLanguages"),
            kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (appleLangArr)  {
            // Take the topmost language. Retain so that we can blindly release later.                                                                                                   
            prefLocaleStr = (CFStringRef)CFArrayGetValueAtIndex(appleLangArr, 0);
            if (prefLocaleStr)
                CFRetain(prefLocaleStr); 
            CFRelease(appleLangArr);
        }
    }
    if (prefLocaleStr) {
        // Canonicalize pref string in case it is not in the canonical format.
        CFStringRef canonLocaleCFStr = CFLocaleCreateCanonicalLanguageIdentifierFromString(kCFAllocatorDefault, prefLocaleStr);
        if (canonLocaleCFStr) {
            CFStringGetCString(canonLocaleCFStr, localeStringBuffer, maxLocaleStringLength, kCFStringEncodingASCII);
            CFRelease(canonLocaleCFStr);
        }
        CFRelease(prefLocaleStr);
    }
}

const char* currentTextBreakLocaleID()
{
    static char localeStringBuffer[maxLocaleStringLength];
    static bool gotTextBreakLocale = false;
    if (!gotTextBreakLocale) {
        getTextBreakLocale(localeStringBuffer);
        gotTextBreakLocale = true;
    }
    return localeStringBuffer;
}

}
