/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "WebInputEventConversion.h"

#include "KeyboardEvent.h"
#include "TouchEvent.h"
#include <gtest/gtest.h>

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebKeyboardEventBuilder;
using WebKit::WebMouseEventBuilder;

namespace {

PassRefPtr<WebCore::KeyboardEvent> createKeyboardEventWithLocation(WebCore::KeyboardEvent::KeyLocationCode location)
{
    return WebCore::KeyboardEvent::create("keydown", true, true, 0, "", location, false, false, false, false, false);
}

int getModifiersForKeyLocationCode(WebCore::KeyboardEvent::KeyLocationCode location)
{
    RefPtr<WebCore::KeyboardEvent> event = createKeyboardEventWithLocation(location);
    WebKit::WebKeyboardEventBuilder convertedEvent(*event);
    return convertedEvent.modifiers;
}

TEST(WebInputEventConversionTest, WebKeyboardEventBuilder)
{
    // Test key location conversion.
    int modifiers = getModifiersForKeyLocationCode(WebCore::KeyboardEvent::DOM_KEY_LOCATION_STANDARD);
    EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad || modifiers & WebInputEvent::IsLeft || modifiers & WebInputEvent::IsRight);

    modifiers = getModifiersForKeyLocationCode(WebCore::KeyboardEvent::DOM_KEY_LOCATION_LEFT);
    EXPECT_TRUE(modifiers & WebInputEvent::IsLeft);
    EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad || modifiers & WebInputEvent::IsRight);

    modifiers = getModifiersForKeyLocationCode(WebCore::KeyboardEvent::DOM_KEY_LOCATION_RIGHT);
    EXPECT_TRUE(modifiers & WebInputEvent::IsRight);
    EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad || modifiers & WebInputEvent::IsLeft);

    modifiers = getModifiersForKeyLocationCode(WebCore::KeyboardEvent::DOM_KEY_LOCATION_NUMPAD);
    EXPECT_TRUE(modifiers & WebInputEvent::IsKeyPad);
    EXPECT_FALSE(modifiers & WebInputEvent::IsLeft || modifiers & WebInputEvent::IsRight);
}

TEST(WebInputEventConversionTest, WebTouchEventBuilder)
{
    RefPtr<WebCore::TouchEvent> event = WebCore::TouchEvent::create();
    WebMouseEventBuilder mouse(0, 0, *event);
    EXPECT_EQ(WebInputEvent::Undefined, mouse.type);
}

} // anonymous namespace
