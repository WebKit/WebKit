/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This implements the WebThemeEngine API in such a way that we match the Mac
// port rendering more than usual Chromium path, thus allowing us to share
// more pixel baselines.

#ifndef WebThemeEngineDRTMac_h
#define WebThemeEngineDRTMac_h

#include "third_party/WebKit/Source/Platform/chromium/public/mac/WebThemeEngine.h"

class WebThemeEngineDRTMac : public WebKit::WebThemeEngine {
public:
    virtual void paintScrollbarThumb(
        WebKit::WebCanvas*,
        WebKit::WebThemeEngine::State,
        WebKit::WebThemeEngine::Size,
        const WebKit::WebRect&,
        const WebKit::WebThemeEngine::ScrollbarInfo&);

private:
    virtual void paintHIThemeScrollbarThumb(
        WebKit::WebCanvas*,
        WebKit::WebThemeEngine::State,
        WebKit::WebThemeEngine::Size,
        const WebKit::WebRect&,
        const WebKit::WebThemeEngine::ScrollbarInfo&);
    virtual void paintNSScrollerScrollbarThumb(
        WebKit::WebCanvas*,
        WebKit::WebThemeEngine::State,
        WebKit::WebThemeEngine::Size,
        const WebKit::WebRect&,
        const WebKit::WebThemeEngine::ScrollbarInfo&);
};

#endif // WebThemeEngineDRTMac_h
