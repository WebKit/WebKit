/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocaleToScriptMapping.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

UScriptCode localeToScriptCodeForFontSelection(const String& locale)
{
    struct LocaleScript {
        const char* locale;
        UScriptCode script;
    };

    // FIXME: Add all languages from ICU.
    static const LocaleScript localeScriptList[] = {
        { "ja", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "ko", USCRIPT_HANGUL },
        { "ru", USCRIPT_CYRILLIC },
        { "zh", USCRIPT_HAN },
        { "zh_cn", USCRIPT_SIMPLIFIED_HAN },
        { "zh_hans", USCRIPT_SIMPLIFIED_HAN },
        { "zh_hant", USCRIPT_TRADITIONAL_HAN },
        { "zh_hk", USCRIPT_TRADITIONAL_HAN },
        { "zh_tw", USCRIPT_TRADITIONAL_HAN }
    };

    typedef HashMap<String, UScriptCode> LocaleScriptMap;
    DEFINE_STATIC_LOCAL(LocaleScriptMap, localeScriptMap, ());
    if (localeScriptMap.isEmpty()) {
        for (size_t i = 0; i < sizeof(localeScriptList) / sizeof(localeScriptList[0]); ++i)
            localeScriptMap.set(localeScriptList[i].locale, localeScriptList[i].script);
    }

    String canonicalLocale = locale.lower().replace('-', '_');
    while (!canonicalLocale.isEmpty()) {
        HashMap<String, UScriptCode>::iterator it = localeScriptMap.find(canonicalLocale);
        if (it != localeScriptMap.end())
            return it->second;
        size_t pos = canonicalLocale.reverseFind('_');
        if (pos == notFound)
            break;
        canonicalLocale = canonicalLocale.substring(0, pos);
    }
    return USCRIPT_COMMON;
}

} // namespace WebCore
