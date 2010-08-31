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

#ifndef WebPreferencesStore_h
#define WebPreferencesStore_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "WebCoreArgumentCoders.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

struct WebPreferencesStore {
    WebPreferencesStore();

    void encode(CoreIPC::ArgumentEncoder* encoder) const
    {
        encoder->encode(javaScriptEnabled);
        encoder->encode(loadsImagesAutomatically);
        encoder->encode(pluginsEnabled);
        encoder->encode(offlineWebApplicationCacheEnabled);
        encoder->encode(localStorageEnabled);
        encoder->encode(xssAuditorEnabled);
        encoder->encode(fontSmoothingLevel);
        encoder->encode(minimumFontSize);
        encoder->encode(minimumLogicalFontSize);
        encoder->encode(defaultFontSize);
        encoder->encode(defaultFixedFontSize);
        encoder->encode(standardFontFamily);
        encoder->encode(cursiveFontFamily);
        encoder->encode(fantasyFontFamily);
        encoder->encode(fixedFontFamily);
        encoder->encode(sansSerifFontFamily);
        encoder->encode(serifFontFamily);
    }

    static bool decode(CoreIPC::ArgumentDecoder*, WebPreferencesStore&);

    static void overrideXSSAuditorEnabledForTestRunner(bool);
    static void removeTestRunnerOverrides();

    bool javaScriptEnabled;
    bool loadsImagesAutomatically;
    bool pluginsEnabled;
    bool offlineWebApplicationCacheEnabled;
    bool localStorageEnabled;
    bool xssAuditorEnabled;

    uint32_t fontSmoothingLevel;

    uint32_t minimumFontSize;
    uint32_t minimumLogicalFontSize;
    uint32_t defaultFontSize;
    uint32_t defaultFixedFontSize;
    WTF::String standardFontFamily;
    WTF::String cursiveFontFamily;
    WTF::String fantasyFontFamily;
    WTF::String fixedFontFamily;
    WTF::String sansSerifFontFamily;
    WTF::String serifFontFamily;
};

} // namespace WebKit

#endif // WebPreferencesStore_h
