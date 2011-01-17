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

} // anonymous namespace
