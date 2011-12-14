/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include <gdk/gdk.h>
#include <gtest/gtest.h>

#include "WebInputEvent.h"
#include "WebInputEventFactory.h"

using WebKit::WebMouseEvent;
using WebKit::WebInputEventFactory;

namespace {

TEST(WebInputEventFactoryTest, DoubleClick)
{
    GdkEventButton firstClick;
    firstClick.type = GDK_BUTTON_PRESS;
    firstClick.window = static_cast<GdkWindow*>(GINT_TO_POINTER(1));
    firstClick.x = firstClick.y = firstClick.x_root = firstClick.y_root = 100;
    firstClick.state = 0;
    firstClick.time = 0;
    firstClick.button = 1;

    // Single click works.
    WebMouseEvent firstClickEvent = WebInputEventFactory::mouseEvent(&firstClick);
    EXPECT_EQ(1, firstClickEvent.clickCount);

    // Make sure double click works.
    GdkEventButton secondClick = firstClick;
    secondClick.time = firstClick.time + 100;
    WebMouseEvent secondClickEvent = WebInputEventFactory::mouseEvent(&secondClick);
    EXPECT_EQ(2, secondClickEvent.clickCount);

    // Reset the click count.
    firstClick.time += 10000;
    firstClickEvent = WebInputEventFactory::mouseEvent(&firstClick);
    EXPECT_EQ(1, firstClickEvent.clickCount);

    // Two clicks with a long gap in between aren't counted as a double click.
    secondClick = firstClick;
    secondClick.time = firstClick.time + 1000;
    secondClickEvent = WebInputEventFactory::mouseEvent(&secondClick);
    EXPECT_EQ(1, secondClickEvent.clickCount);

    // Reset the click count.
    firstClick.time += 10000;
    firstClickEvent = WebInputEventFactory::mouseEvent(&firstClick);
    EXPECT_EQ(1, firstClickEvent.clickCount);

    // Two clicks far apart (horizontally) aren't counted as a double click.
    secondClick = firstClick;
    secondClick.time = firstClick.time + 1;
    secondClick.x = firstClick.x + 100;
    secondClickEvent = WebInputEventFactory::mouseEvent(&secondClick);
    EXPECT_EQ(1, secondClickEvent.clickCount);

    // Reset the click count.
    firstClick.time += 10000;
    firstClickEvent = WebInputEventFactory::mouseEvent(&firstClick);
    EXPECT_EQ(1, firstClickEvent.clickCount);

    // Two clicks far apart (vertically) aren't counted as a double click.
    secondClick = firstClick;
    secondClick.time = firstClick.time + 1;
    secondClick.x = firstClick.y + 100;
    secondClickEvent = WebInputEventFactory::mouseEvent(&secondClick);
    EXPECT_EQ(1, secondClickEvent.clickCount);

    // Reset the click count.
    firstClick.time += 10000;
    firstClickEvent = WebInputEventFactory::mouseEvent(&firstClick);
    EXPECT_EQ(1, firstClickEvent.clickCount);

    // Two clicks on different windows aren't a double click.
    secondClick = firstClick;
    secondClick.time = firstClick.time + 1;
    secondClick.window = static_cast<GdkWindow*>(GINT_TO_POINTER(2));
    secondClickEvent = WebInputEventFactory::mouseEvent(&secondClick);
    EXPECT_EQ(1, secondClickEvent.clickCount);
}

TEST(WebInputEventFactoryTest, MouseUpClickCount)
{
    GdkEventButton mouseDown;
    memset(&mouseDown, 0, sizeof(mouseDown));
    mouseDown.type = GDK_BUTTON_PRESS;
    mouseDown.window = static_cast<GdkWindow*>(GINT_TO_POINTER(1));
    mouseDown.x = mouseDown.y = mouseDown.x_root = mouseDown.y_root = 100;
    mouseDown.time = 0;
    mouseDown.button = 1;

    // Properly set the last click time, so that the internal state won't be affected by previous tests.
    WebInputEventFactory::mouseEvent(&mouseDown);

    mouseDown.time += 10000;
    GdkEventButton mouseUp = mouseDown;
    mouseUp.type = GDK_BUTTON_RELEASE;
    WebMouseEvent mouseDownEvent;
    WebMouseEvent mouseUpEvent;

    // Click for three times.
    for (int i = 1; i < 4; ++i) {
        mouseDown.time += 100;
        mouseDownEvent = WebInputEventFactory::mouseEvent(&mouseDown);
        EXPECT_EQ(i, mouseDownEvent.clickCount);

        mouseUp.time = mouseDown.time + 50;
        mouseUpEvent = WebInputEventFactory::mouseEvent(&mouseUp);
        EXPECT_EQ(i, mouseUpEvent.clickCount);
    }

    // Reset the click count.
    mouseDown.time += 10000;
    mouseDownEvent = WebInputEventFactory::mouseEvent(&mouseDown);
    EXPECT_EQ(1, mouseDownEvent.clickCount);

    // Moving the cursor for a significant distance will reset the click count to 0.
    GdkEventMotion mouseMove;
    memset(&mouseMove, 0, sizeof(mouseMove));
    mouseMove.type = GDK_MOTION_NOTIFY;
    mouseMove.window = mouseDown.window;
    mouseMove.time = mouseDown.time;
    mouseMove.x = mouseMove.y = mouseMove.x_root = mouseMove.y_root = mouseDown.x + 100;
    WebInputEventFactory::mouseEvent(&mouseMove);

    mouseUp.time = mouseDown.time + 50;
    mouseUpEvent = WebInputEventFactory::mouseEvent(&mouseUp);
    EXPECT_EQ(0, mouseUpEvent.clickCount);

    // Reset the click count.
    mouseDown.time += 10000;
    mouseDownEvent = WebInputEventFactory::mouseEvent(&mouseDown);
    EXPECT_EQ(1, mouseDownEvent.clickCount);

    // Moving the cursor with a significant delay will reset the click count to 0.
    mouseMove.time = mouseDown.time + 1000;
    mouseMove.x = mouseMove.y = mouseMove.x_root = mouseMove.y_root = mouseDown.x;
    WebInputEventFactory::mouseEvent(&mouseMove);

    mouseUp.time = mouseMove.time + 50;
    mouseUpEvent = WebInputEventFactory::mouseEvent(&mouseUp);
    EXPECT_EQ(0, mouseUpEvent.clickCount);
}

} // anonymous namespace
