/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#pragma once

#include <WebKit/WKViewClient.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wpe_view_backend;

WK_EXPORT WKViewRef WKViewCreate(WKPageConfigurationRef configuration);
WK_EXPORT WKViewRef WKViewCreateWPE(struct wpe_view_backend*, WKPageConfigurationRef);

WK_EXPORT void WKViewSetViewClient(WKViewRef, const WKViewClientBase*);

WK_EXPORT WKPageRef WKViewGetPage(WKViewRef);

WK_EXPORT void WKViewSetSize(WKViewRef, WKSize viewSize);

WK_EXPORT void WKViewSetFocus(WKViewRef, bool);
WK_EXPORT void WKViewSetActive(WKViewRef, bool);
WK_EXPORT void WKViewSetVisible(WKViewRef, bool);

WK_EXPORT void WKViewWillEnterFullScreen(WKViewRef);
WK_EXPORT void WKViewDidEnterFullScreen(WKViewRef);
WK_EXPORT void WKViewWillExitFullScreen(WKViewRef);
WK_EXPORT void WKViewDidExitFullScreen(WKViewRef);
WK_EXPORT void WKViewRequestExitFullScreen(WKViewRef);
WK_EXPORT bool WKViewIsFullScreen(WKViewRef);

#ifdef __cplusplus
}
#endif
