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

#include <WebKit/WKBase.h>
#include <WebKit/WKGeometry.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKCursorTypePointer = 0,
    kWKCursorTypeHand = 2,
    kWKCursorTypeNone = 37,
};
typedef uint32_t WKCursorType;

typedef void(*WKViewSetViewNeedsDisplayCallback)(WKViewRef view, WKRect rect, const void* clientInfo);
typedef void(*WKViewEnterFullScreen)(WKViewRef view, const void* clientInfo);
typedef void(*WKViewExitFullScreen)(WKViewRef view, const void* clientInfo);
typedef void(*WKViewCloseFullScreen)(WKViewRef view, const void* clientInfo);
typedef void(*WKViewBeganEnterFullScreen)(WKViewRef view, const WKRect initialFrame, const WKRect finalFrame, const void* clientInfo);
typedef void(*WKViewBeganExitFullScreen)(WKViewRef view, const WKRect initialFrame, const WKRect finalFrame, const void* clientInfo);
typedef void(*WKViewSetCursorCallback)(WKViewRef view, WKCursorType cursorType, const void* clientInfo);

typedef struct WKViewClientBase {
    int version;
    const void* clientInfo;
} WKViewClientBase;

typedef struct WKViewClientV0 {
    WKViewClientBase base;

    // version 0
    WKViewSetViewNeedsDisplayCallback setViewNeedsDisplay;
    WKViewEnterFullScreen enterFullScreen;
    WKViewExitFullScreen exitFullScreen;
    WKViewCloseFullScreen closeFullScreen;
    WKViewBeganEnterFullScreen beganEnterFullScreen;
    WKViewBeganExitFullScreen beganExitFullScreen;
    WKViewSetCursorCallback setCursor;
} WKViewClientV0;

#ifdef __cplusplus
}
#endif
