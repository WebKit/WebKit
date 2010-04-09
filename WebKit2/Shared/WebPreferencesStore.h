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
#include "WebCoreTypeArgumentMarshalling.h"
#include <WebCore/PlatformString.h>

namespace WebKit {

struct WebPreferencesStore {
    WebPreferencesStore();
    WebPreferencesStore(const WebPreferencesStore&);
    WebPreferencesStore& operator=(const WebPreferencesStore&);
    void swap(WebPreferencesStore&);

    void encode(CoreIPC::ArgumentEncoder& encoder) const
    {
        encoder.encode(javaScriptEnabled);
        encoder.encode(loadsImagesAutomatically);
        encoder.encode(minimumFontSize);
        encoder.encode(minimumLogicalFontSize);
        encoder.encode(defaultFontSize);
        encoder.encode(defaultFixedFontSize);
        encoder.encode(standardFontFamily);
        encoder.encode(cursiveFontFamily);
        encoder.encode(fantasyFontFamily);
        encoder.encode(fixedFontFamily);
        encoder.encode(sansSerifFontFamily);
        encoder.encode(serifFontFamily);
    }

    static bool decode(CoreIPC::ArgumentDecoder& decoder, WebPreferencesStore& s)
    {
        if (!decoder.decode(s.javaScriptEnabled))
            return false;
        if (!decoder.decode(s.loadsImagesAutomatically))
            return false;
        if (!decoder.decode(s.minimumFontSize))
            return false;
        if (!decoder.decode(s.minimumLogicalFontSize))
            return false;
        if (!decoder.decode(s.defaultFontSize))
            return false;
        if (!decoder.decode(s.defaultFixedFontSize))
            return false;
        if (!decoder.decode(s.standardFontFamily))
            return false;
        if (!decoder.decode(s.cursiveFontFamily))
            return false;
        if (!decoder.decode(s.fantasyFontFamily))
            return false;
        if (!decoder.decode(s.fixedFontFamily))
            return false;
        if (!decoder.decode(s.sansSerifFontFamily))
            return false;
        if (!decoder.decode(s.serifFontFamily))
            return false;
        return true;
    }

    bool javaScriptEnabled;
    bool loadsImagesAutomatically;
    uint32_t minimumFontSize;
    uint32_t minimumLogicalFontSize;
    uint32_t defaultFontSize;
    uint32_t defaultFixedFontSize;
    WebCore::String standardFontFamily;
    WebCore::String cursiveFontFamily;
    WebCore::String fantasyFontFamily;
    WebCore::String fixedFontFamily;
    WebCore::String sansSerifFontFamily;
    WebCore::String serifFontFamily;
};

} // namespace WebKit

#endif // WebPreferencesStore_h
