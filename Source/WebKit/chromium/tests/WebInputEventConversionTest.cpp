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

#include "Frame.h"
#include "FrameTestHelpers.h"
#include "FrameView.h"
#include "GestureEvent.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "Touch.h"
#include "TouchEvent.h"
#include "TouchList.h"
#include "URLTestHelpers.h"
#include "WebFrame.h"
#include "WebSettings.h"
#include "WebViewImpl.h"
#include <gtest/gtest.h>

using namespace WebKit;
using namespace WebCore;

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
    int modifiers = getModifiersForKeyLocationCode(WebCore::KeyboardEvent::DOMKeyLocationStandard);
    EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad || modifiers & WebInputEvent::IsLeft || modifiers & WebInputEvent::IsRight);

    modifiers = getModifiersForKeyLocationCode(WebCore::KeyboardEvent::DOMKeyLocationLeft);
    EXPECT_TRUE(modifiers & WebInputEvent::IsLeft);
    EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad || modifiers & WebInputEvent::IsRight);

    modifiers = getModifiersForKeyLocationCode(WebCore::KeyboardEvent::DOMKeyLocationRight);
    EXPECT_TRUE(modifiers & WebInputEvent::IsRight);
    EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad || modifiers & WebInputEvent::IsLeft);

    modifiers = getModifiersForKeyLocationCode(WebCore::KeyboardEvent::DOMKeyLocationNumpad);
    EXPECT_TRUE(modifiers & WebInputEvent::IsKeyPad);
    EXPECT_FALSE(modifiers & WebInputEvent::IsLeft || modifiers & WebInputEvent::IsRight);
}

TEST(WebInputEventConversionTest, WebTouchEventBuilder)
{
    RefPtr<WebCore::TouchEvent> event = WebCore::TouchEvent::create();
    WebMouseEventBuilder mouse(0, 0, *event);
    EXPECT_EQ(WebInputEvent::Undefined, mouse.type);
}

TEST(WebInputEventConversionTest, InputEventsScaling)
{
    const std::string baseURL("http://www.test.com/");
    const std::string fileName("fixed_layout.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("fixed_layout.html"));
    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(baseURL + fileName, true));
    webViewImpl->enableFixedLayoutMode(true);
    webViewImpl->settings()->setViewportEnabled(true);
    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->layout();

    webViewImpl->setPageScaleFactor(2, WebPoint());

    FrameView* view = webViewImpl->page()->mainFrame()->view();
    RefPtr<Document> document = webViewImpl->page()->mainFrame()->document();
    DOMWindow* domWindow = webViewImpl->page()->mainFrame()->document()->defaultView();
    RenderObject* docRenderer = webViewImpl->page()->mainFrame()->document()->renderer();

    {
        WebMouseEvent webMouseEvent;
        webMouseEvent.type = WebInputEvent::MouseMove;
        webMouseEvent.x = 10;
        webMouseEvent.y = 10;
        webMouseEvent.windowX = 10;
        webMouseEvent.windowY = 10;
        webMouseEvent.globalX = 10;
        webMouseEvent.globalY = 10;
        webMouseEvent.movementX = 10;
        webMouseEvent.movementY = 10;

        PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
        EXPECT_EQ(5, platformMouseBuilder.position().x());
        EXPECT_EQ(5, platformMouseBuilder.position().y());
        EXPECT_EQ(10, platformMouseBuilder.globalPosition().x());
        EXPECT_EQ(10, platformMouseBuilder.globalPosition().y());
        EXPECT_EQ(5, platformMouseBuilder.movementDelta().x());
        EXPECT_EQ(5, platformMouseBuilder.movementDelta().y());
    }

    {
        WebGestureEvent webGestureEvent;
        webGestureEvent.type = WebInputEvent::GestureScrollUpdate;
        webGestureEvent.x = 10;
        webGestureEvent.y = 10;
        webGestureEvent.globalX = 10;
        webGestureEvent.globalY = 10;
        webGestureEvent.data.scrollUpdate.deltaX = 10;
        webGestureEvent.data.scrollUpdate.deltaY = 10;

        PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
        EXPECT_EQ(5, platformGestureBuilder.position().x());
        EXPECT_EQ(5, platformGestureBuilder.position().y());
        EXPECT_EQ(10, platformGestureBuilder.globalPosition().x());
        EXPECT_EQ(10, platformGestureBuilder.globalPosition().y());
        EXPECT_EQ(5, platformGestureBuilder.deltaX());
        EXPECT_EQ(5, platformGestureBuilder.deltaY());
    }

    {
        WebGestureEvent webGestureEvent;
        webGestureEvent.type = WebInputEvent::GestureTap;
        webGestureEvent.data.tap.width = 10;
        webGestureEvent.data.tap.height = 10;

        PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
        EXPECT_EQ(5, platformGestureBuilder.area().width());
        EXPECT_EQ(5, platformGestureBuilder.area().height());
    }

    {
        WebGestureEvent webGestureEvent;
        webGestureEvent.type = WebInputEvent::GestureTapDown;
        webGestureEvent.data.tapDown.width = 10;
        webGestureEvent.data.tapDown.height = 10;

        PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
        EXPECT_EQ(5, platformGestureBuilder.area().width());
        EXPECT_EQ(5, platformGestureBuilder.area().height());
    }

    {
        WebGestureEvent webGestureEvent;
        webGestureEvent.type = WebInputEvent::GestureLongPress;
        webGestureEvent.data.longPress.width = 10;
        webGestureEvent.data.longPress.height = 10;

        PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
        EXPECT_EQ(5, platformGestureBuilder.area().width());
        EXPECT_EQ(5, platformGestureBuilder.area().height());
    }

    {
        WebGestureEvent webGestureEvent;
        webGestureEvent.type = WebInputEvent::GestureTwoFingerTap;
        webGestureEvent.data.twoFingerTap.firstFingerWidth = 10;
        webGestureEvent.data.twoFingerTap.firstFingerHeight = 10;

        PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
        EXPECT_EQ(5, platformGestureBuilder.area().width());
        EXPECT_EQ(5, platformGestureBuilder.area().height());
    }

    {
        WebTouchEvent webTouchEvent;
        webTouchEvent.type = WebInputEvent::TouchMove;
        webTouchEvent.touchesLength = 1;
        webTouchEvent.touches[0].state = WebTouchPoint::StateMoved;
        webTouchEvent.touches[0].screenPosition.x = 10;
        webTouchEvent.touches[0].screenPosition.y = 10;
        webTouchEvent.touches[0].position.x = 10;
        webTouchEvent.touches[0].position.y = 10;
        webTouchEvent.touches[0].radiusX = 10;
        webTouchEvent.touches[0].radiusY = 10;

        PlatformTouchEventBuilder platformTouchBuilder(view, webTouchEvent);
        EXPECT_EQ(10, platformTouchBuilder.touchPoints()[0].screenPos().x());
        EXPECT_EQ(10, platformTouchBuilder.touchPoints()[0].screenPos().y());
        EXPECT_EQ(5, platformTouchBuilder.touchPoints()[0].pos().x());
        EXPECT_EQ(5, platformTouchBuilder.touchPoints()[0].pos().y());
        EXPECT_EQ(5, platformTouchBuilder.touchPoints()[0].radiusX());
        EXPECT_EQ(5, platformTouchBuilder.touchPoints()[0].radiusY());
    }

    // Reverse builders should *not* go back to physical pixels, as they are used for plugins
    // which expect CSS pixel coordinates.
    {
        PlatformMouseEvent platformMouseEvent(IntPoint(10, 10), IntPoint(10, 10), LeftButton, PlatformEvent::MouseMoved, 1, false, false, false, false, 0);
        RefPtr<MouseEvent> mouseEvent = MouseEvent::create(WebCore::eventNames().mousemoveEvent, domWindow, platformMouseEvent, 0, document);
        WebMouseEventBuilder webMouseBuilder(view, docRenderer, *mouseEvent);

        EXPECT_EQ(10, webMouseBuilder.x);
        EXPECT_EQ(10, webMouseBuilder.y);
        EXPECT_EQ(10, webMouseBuilder.globalX);
        EXPECT_EQ(10, webMouseBuilder.globalY);
        EXPECT_EQ(10, webMouseBuilder.windowX);
        EXPECT_EQ(10, webMouseBuilder.windowY);
    }

    {
        PlatformGestureEvent platformGestureEvent(PlatformEvent::GestureScrollUpdate, IntPoint(10, 10), IntPoint(10, 10), 0, IntSize(10, 10), FloatPoint(10, 10), false, false, false, false);
        RefPtr<GestureEvent> gestureEvent = GestureEvent::create(domWindow, platformGestureEvent);
        WebGestureEventBuilder webGestureBuilder(view, docRenderer, *gestureEvent);

        EXPECT_EQ(10, webGestureBuilder.x);
        EXPECT_EQ(10, webGestureBuilder.y);
        EXPECT_EQ(10, webGestureBuilder.globalX);
        EXPECT_EQ(10, webGestureBuilder.globalY);
        EXPECT_EQ(10, webGestureBuilder.data.scrollUpdate.deltaX);
        EXPECT_EQ(10, webGestureBuilder.data.scrollUpdate.deltaY);
    }

    {
        RefPtr<Touch> touch = Touch::create(webViewImpl->page()->mainFrame(), document.get(), 0, 10, 10, 10, 10, 10, 10, 0, 0);
        RefPtr<TouchList> touchList = TouchList::create();
        touchList->append(touch);
        RefPtr<TouchEvent> touchEvent = TouchEvent::create(touchList.get(), touchList.get(), touchList.get(), WebCore::eventNames().touchmoveEvent, domWindow, 10, 10, 10, 10, false, false, false, false);

        WebTouchEventBuilder webTouchBuilder(view, docRenderer, *touchEvent);
        ASSERT_EQ(1u, webTouchBuilder.touchesLength);
        EXPECT_EQ(10, webTouchBuilder.touches[0].screenPosition.x);
        EXPECT_EQ(10, webTouchBuilder.touches[0].screenPosition.y);
        EXPECT_EQ(10, webTouchBuilder.touches[0].position.x);
        EXPECT_EQ(10, webTouchBuilder.touches[0].position.y);
        EXPECT_EQ(10, webTouchBuilder.touches[0].radiusX);
        EXPECT_EQ(10, webTouchBuilder.touches[0].radiusY);
    }

    webViewImpl->close();
}

} // anonymous namespace
