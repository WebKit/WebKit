/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "PopupContainer.h"

#include <gtest/gtest.h>

using namespace WebCore;

class MockPopupContent : public PopupContent {
public:
    virtual void setMaxHeight(int max) OVERRIDE { maxHeight = max; }
    virtual int popupContentHeight() const OVERRIDE { return height; }
    virtual ~MockPopupContent() { }

    virtual void layout() OVERRIDE
    {
        layoutCount++;
        width = std::min(maxWidth, width);
        height = std::min(maxHeight, height);
        height -= height % 16;
    }

    virtual void setMaxWidthAndLayout(int max) OVERRIDE
    {
        maxWidth = max;
        layout();
    }

    MockPopupContent(const IntSize& widgetSize)
        : width(widgetSize.width() - borderSize * 2)
        , height(widgetSize.height() - borderSize * 2)
        , maxWidth(width)
        , maxHeight(height)
        , layoutCount(0)
    {
    }

    int width;
    int height;
    int maxWidth;
    int maxHeight;
    unsigned layoutCount;

    static const int borderSize = 1; // Should match to kBorderSize in PopupContainer.cpp.
};

const int screenMaxX = 1024;
const int screenMaxY = 768;
const int targetControlWidth = 130;

static IntRect calculatePosition(const IntRect& initialRect, PopupContent* content)
{
    const bool isRTL = true;
    const int targetControlHeight = 20;
    const FloatRect screenRect(0, 0, screenMaxX, screenMaxY);
    const FloatRect windowRect(0, 0, 512, 512);
    int rtlOffset = targetControlWidth - initialRect.width();
    bool needToResizeView = false;
    return PopupContainer::layoutAndCalculateWidgetRectInternal(initialRect, targetControlHeight, windowRect, screenRect, !isRTL, rtlOffset, content, needToResizeView);
}

TEST(PopupContainerTest, PopupPosition)
{
    // Suppose that initialRect.location is the bottom-left corner of the target
    // control such as <select>.

    {
        // If initialRect is in the screen, nothing should happen.
        IntRect initialRect(100, 100, 256, 258);
        MockPopupContent content(initialRect.size());
        IntRect resultRect = calculatePosition(initialRect, &content);
        EXPECT_EQ(initialRect, resultRect);
        EXPECT_EQ(0u, content.layoutCount);
    }

    {
        // If the left edge of the control is projecting from the screen, making
        // the widget aligned to the right edge of the control.
        IntRect initialRect(-10, 100, 100, 258);
        MockPopupContent content(initialRect.size());
        IntRect resultRect = calculatePosition(initialRect, &content);
        EXPECT_EQ(IntRect(20, 100, 100, 258), resultRect);
    }

    {
        // Made the widget aligned to the right edge. But it's still projecting
        // from the screen.
        IntRect initialRect(-10, 100, targetControlWidth, 258);
        MockPopupContent content(initialRect.size());
        IntRect resultRect = calculatePosition(initialRect, &content);
        EXPECT_EQ(IntRect(0, 100, 120, 258), resultRect);
        EXPECT_EQ(118, content.width);
        EXPECT_TRUE(content.layoutCount);
    }

    {
        // If the right edge of the control is projecting from the screen,
        // shrink the width of the widget.
        IntRect initialRect(screenMaxX - 100, 100, targetControlWidth, 258);
        MockPopupContent content(initialRect.size());
        IntRect resultRect = calculatePosition(initialRect, &content);
        EXPECT_EQ(IntRect(screenMaxX - 100, 100, 100, 258), resultRect);
        EXPECT_EQ(98, content.width);
        EXPECT_TRUE(content.layoutCount);
    }

    {
        // If there is no enough room below, move the widget upwards.
        IntRect initialRect(100, 700, targetControlWidth, 258);
        MockPopupContent content(initialRect.size());
        IntRect resultRect = calculatePosition(initialRect, &content);
        EXPECT_EQ(IntRect(100, 422, targetControlWidth, 258), resultRect);
        EXPECT_EQ(0u, content.layoutCount);
    }

    {
        // There is no enough room below and above, and the below space is larger.
        IntRect initialRect(100, 300, targetControlWidth, 514);
        MockPopupContent content(initialRect.size());
        IntRect resultRect = calculatePosition(initialRect, &content);
        EXPECT_EQ(IntRect(100, 300, targetControlWidth, 466), resultRect);
        EXPECT_TRUE(content.layoutCount);
        EXPECT_EQ(464, content.height);
    }

    {
        // There is no enough room below and above, and the above space is larger.
        IntRect initialRect(100, 400, targetControlWidth, 514);
        MockPopupContent content(initialRect.size());
        IntRect resultRect = calculatePosition(initialRect, &content);
        EXPECT_EQ(IntRect(100, 10, targetControlWidth, 370), resultRect);
        EXPECT_TRUE(content.layoutCount);
        EXPECT_EQ(368, content.height);
    }
}
