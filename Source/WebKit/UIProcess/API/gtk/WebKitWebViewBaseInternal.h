/*
 * Copyright (C) 2020 Igalia S.L.
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
#include <WebCore/PlatformMouseEvent.h>
#include <wtf/text/WTFString.h>

typedef struct _WebKitWebViewBase WebKitWebViewBase;

struct KeyEvent {
    unsigned type { 0 };
    unsigned keyval { 0 };
    unsigned modifiers { 0 };
};

enum class MouseEventType { Press, Release, Motion };
WK_EXPORT void webkitWebViewBaseSynthesizeMouseEvent(WebKitWebViewBase*, MouseEventType type, unsigned button, unsigned short buttons, int x, int y, unsigned modifiers, int clickCount, const String& pointerType = "mouse"_s, WebCore::PlatformMouseEvent::IsTouch isTouchEvent = WebCore::PlatformMouseEvent::IsTouch::No);

enum class KeyEventType { Press, Release, Insert };
enum class ShouldTranslateKeyboardState : bool { No, Yes };
WK_EXPORT void webkitWebViewBaseSynthesizeKeyEvent(WebKitWebViewBase*, KeyEventType, unsigned keyVal, unsigned modifiers, ShouldTranslateKeyboardState);

enum class WheelEventPhase { NoPhase, Began, Changed, Ended, Cancelled, MayBegin };
WK_EXPORT void webkitWebViewBaseSynthesizeWheelEvent(WebKitWebViewBase*, double deltaX, double deltaY, int x, int y, WheelEventPhase, WheelEventPhase momentumPhase, bool);

