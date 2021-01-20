/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include <WebKit/WKEvent.h>
#include <WebKit/WKGeometry.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKEventNoType = -1,

    // WebMouseEvent
    kWKEventMouseDown,
    kWKEventMouseUp,
    kWKEventMouseMove,
    kWKEventMouseForceChanged,
    kWKEventMouseForceDown,
    kWKEventMouseForceUp,

    // WebWheelEvent
    kWKEventWheel,

    // WebKeyboardEvent
    kWKEventKeyDown,
    kWKEventKeyUp
};
typedef int32_t WKEventType;

struct WKKeyboardEvent {
    WKEventType type;
    const char* text;
    int32_t length;
    const char* keyIdentifier;
    uint32_t modifiers;
    int32_t virtualKeyCode;
    int32_t caretOffset;
};
typedef struct WKKeyboardEvent WKKeyboardEvent;

enum {
    kWKInputTypeNormal,
    kWKInputTypeSetComposition,
    kWKInputTypeConfirmComposition,
    kWKInputTypeCancelComposition,
};
typedef uint8_t WKInputType;

struct WKMouseEvent {
    WKEventType type;
    WKEventMouseButton button;
    WKPoint position;
    int32_t clickCount;
    uint32_t modifiers;
};
typedef struct WKMouseEvent WKMouseEvent;

struct WKWheelEvent {
    WKEventType type;
    WKPoint position;
    WKSize delta;
    WKSize wheelTicks;
    uint32_t modifiers;
};
typedef struct WKWheelEvent WKWheelEvent;

WK_EXPORT WKKeyboardEvent WKKeyboardEventMake(WKEventType type, WKInputType inputType, const char* text, uint32_t length, const char* keyIdentifier, int32_t virtualKeyCode, int32_t caretOffset, uint32_t attributes, uint32_t modifiers);

WK_EXPORT WKMouseEvent WKMouseEventMake(WKEventType type, WKEventMouseButton button, WKPoint position, int32_t clickCount, uint32_t modifiers);

WK_EXPORT WKWheelEvent WKWheelEventMake(WKEventType type, WKPoint position, WKSize delta, WKSize wheelTicks, uint32_t modifiers);

#ifdef __cplusplus
}
#endif
