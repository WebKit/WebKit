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

#include "WebPreferencesStore.h"

#include "FontSmoothingLevel.h"

namespace WebKit {

static bool hasXSSAuditorEnabledTestRunnerOverride;
static bool xssAuditorEnabledTestRunnerOverride;

WebPreferencesStore::WebPreferencesStore()
    : javaScriptEnabled(true)
    , loadsImagesAutomatically(true)
    , pluginsEnabled(true)
    , offlineWebApplicationCacheEnabled(false)
    , localStorageEnabled(true)
    , xssAuditorEnabled(true)
    , frameFlatteningEnabled(false)
    , fontSmoothingLevel(FontSmoothingLevelMedium)
    , minimumFontSize(1)
    , minimumLogicalFontSize(9)
    , defaultFontSize(16)
    , defaultFixedFontSize(13)
    , standardFontFamily("Times")
    , cursiveFontFamily("Apple Chancery")
    , fantasyFontFamily("Papyrus")
    , fixedFontFamily("Courier")
    , sansSerifFontFamily("Helvetica")
    , serifFontFamily("Times")
{
}

void WebPreferencesStore::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(javaScriptEnabled);
    encoder->encode(loadsImagesAutomatically);
    encoder->encode(pluginsEnabled);
    encoder->encode(offlineWebApplicationCacheEnabled);
    encoder->encode(localStorageEnabled);
    encoder->encode(xssAuditorEnabled);
    encoder->encode(frameFlatteningEnabled);
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

bool WebPreferencesStore::decode(CoreIPC::ArgumentDecoder* decoder, WebPreferencesStore& s)
{
    if (!decoder->decode(s.javaScriptEnabled))
        return false;
    if (!decoder->decode(s.loadsImagesAutomatically))
        return false;
    if (!decoder->decode(s.pluginsEnabled))
        return false;
    if (!decoder->decode(s.offlineWebApplicationCacheEnabled))
        return false;
    if (!decoder->decode(s.localStorageEnabled))
        return false;
    if (!decoder->decode(s.xssAuditorEnabled))
        return false;
    if (!decoder->decode(s.frameFlatteningEnabled))
        return false;
    if (!decoder->decode(s.fontSmoothingLevel))
        return false;
    if (!decoder->decode(s.minimumFontSize))
        return false;
    if (!decoder->decode(s.minimumLogicalFontSize))
        return false;
    if (!decoder->decode(s.defaultFontSize))
        return false;
    if (!decoder->decode(s.defaultFixedFontSize))
        return false;
    if (!decoder->decode(s.standardFontFamily))
        return false;
    if (!decoder->decode(s.cursiveFontFamily))
        return false;
    if (!decoder->decode(s.fantasyFontFamily))
        return false;
    if (!decoder->decode(s.fixedFontFamily))
        return false;
    if (!decoder->decode(s.sansSerifFontFamily))
        return false;
    if (!decoder->decode(s.serifFontFamily))
        return false;

    if (hasXSSAuditorEnabledTestRunnerOverride)
        s.xssAuditorEnabled = xssAuditorEnabledTestRunnerOverride;

    return true;
}

void WebPreferencesStore::overrideXSSAuditorEnabledForTestRunner(bool enabled)
{
    hasXSSAuditorEnabledTestRunnerOverride = true;
    xssAuditorEnabledTestRunnerOverride = enabled;
}

void WebPreferencesStore::removeTestRunnerOverrides()
{
    hasXSSAuditorEnabledTestRunnerOverride = false;
}

} // namespace WebKit
