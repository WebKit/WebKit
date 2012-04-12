/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Settings.h"

#include "LocaleToScriptMapping.h"
#include <wtf/text/CString.h>

namespace WebCore {

static NSDictionary *defaultFontFamilyDictionary()
{
    static NSDictionary *fontFamilyDictionary;
    if (fontFamilyDictionary)
        return fontFamilyDictionary;

    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
#ifdef BUILDING_ON_LION
    // Temporary workaround for a Safari Webpage Preview Fetcher crash caused by insufficient sandbox permissions.
    if (!bundle)
        return 0;
#endif
    NSData *fileData = [NSData dataWithContentsOfURL:[bundle URLForResource:@"DefaultFonts" withExtension:@"plist"]];
    if (!fileData)
        FATAL("Could not read font fallback file");
    NSError *error;
    id propertyList = [NSPropertyListSerialization propertyListWithData:fileData options:NSPropertyListImmutable format:0 error:&error];
    if (!propertyList)
        FATAL("Could not parse font fallback property list: %s", [[error description] UTF8String]);
    if (![propertyList isKindOfClass:[NSDictionary class]])
        FATAL("Font fallback file has incorrect format - root object is not a dictionary");

    fontFamilyDictionary = static_cast<NSDictionary *>(propertyList);
    CFRetain(fontFamilyDictionary);
    return fontFamilyDictionary;
}

void Settings::initializeDefaultFontFamilies()
{
    DEFINE_STATIC_LOCAL(AtomicString, standardFamily, ("standard"));
    DEFINE_STATIC_LOCAL(AtomicString, monospaceFamily, ("monospace"));
    DEFINE_STATIC_LOCAL(AtomicString, serifFamily, ("serif"));
    DEFINE_STATIC_LOCAL(AtomicString, sansSerifFamily, ("sans-serif"));
    DEFINE_STATIC_LOCAL(AtomicString, cursiveFamily, ("cursive"));
    DEFINE_STATIC_LOCAL(AtomicString, fantasyFamily, ("fantasy"));
    DEFINE_STATIC_LOCAL(AtomicString, pictographFamily, ("pictograph"));

    NSDictionary *rootDictionary = defaultFontFamilyDictionary();
    for (NSString *scriptName in rootDictionary) {
        NSDictionary *scriptDictionary = static_cast<NSDictionary *>([rootDictionary objectForKey:scriptName]);
        if (![scriptName isKindOfClass:[NSString class]])
            FATAL("Font fallback file has incorrect format - script name is not a string");
        if (![scriptDictionary isKindOfClass:[NSDictionary class]])
            FATAL("Font fallback file has incorrect format - per-script value is not a dictionary");

        UScriptCode script = scriptNameToCode(scriptName);
        if (script == USCRIPT_UNKNOWN)
            FATAL("Font fallback file has incorrect format - unknown language code %s", [scriptName UTF8String]);

        for (NSString *genericFamilyName in scriptDictionary) {
            NSString *familyName = [scriptDictionary objectForKey:genericFamilyName];
            if (![genericFamilyName isKindOfClass:[NSString class]])
                FATAL("Font fallback file has incorrect format - generic family name is not a string");
            if (![familyName isKindOfClass:[NSString class]])
                FATAL("Font fallback file has incorrect format - font family name is not a string");

            AtomicString genericFamily(genericFamilyName);

            if (genericFamily == standardFamily)
                setStandardFontFamily(familyName, script);
            else if (genericFamily == monospaceFamily)
                setFixedFontFamily(familyName, script);
            else if (genericFamily == serifFamily)
                setSerifFontFamily(familyName, script);
            else if (genericFamily == sansSerifFamily)
                setSansSerifFontFamily(familyName, script);
            else if (genericFamily == cursiveFamily)
                setCursiveFontFamily(familyName, script);
            else if (genericFamily == fantasyFamily)
                setFantasyFontFamily(familyName, script);
            else if (genericFamily == pictographFamily)
                setPictographFontFamily(familyName, script);
            else
                FATAL("Font fallback file has incorrect format - unknown font family name %s", [familyName UTF8String]);
        }
    }
}


} // namespace WebCore
