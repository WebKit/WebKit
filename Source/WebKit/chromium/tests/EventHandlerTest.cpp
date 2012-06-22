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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "EventHandler.h"

#include "Frame.h"
#include "HitTestResult.h"
#include "PlatformWheelEvent.h"
#include "ScrollTypes.h"
#include "Scrollbar.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std;
using namespace WebCore;

class MockScrollbar : public Scrollbar {
public:
    MockScrollbar(ScrollbarOrientation orientation) : Scrollbar(0, orientation, RegularScrollbar) { }
    virtual ~MockScrollbar() { }
};

class MockHitTestResult : public HitTestResult {
public:
    MockHitTestResult(Scrollbar* scrollbar) : HitTestResult()
    {
        setScrollbar(scrollbar);
    }

};

class MockPlatformWheelEvent : public PlatformWheelEvent {
public:
    MockPlatformWheelEvent(bool hasPreciseScrollingDeltas) : PlatformWheelEvent()
    {
        m_hasPreciseScrollingDeltas = hasPreciseScrollingDeltas;
    }
};

class EventHandlerTest : public EventHandler {
public:
    EventHandlerTest() : EventHandler(0) { };
    bool externalShouldTurnVerticalTicksIntoHorizontal(const HitTestResult& hitTestResult, const PlatformWheelEvent& wheelEvent)
    {
         return EventHandler::shouldTurnVerticalTicksIntoHorizontal(hitTestResult, wheelEvent);
    }
};

TEST(EventHandler, shouldTurnVerticalTicksIntoHorizontal)
{
    EventHandlerTest handler;

    RefPtr<MockScrollbar> verticalScrollbar = adoptRef(new MockScrollbar(VerticalScrollbar));
    RefPtr<MockScrollbar> horizontalScrollbar = adoptRef(new MockScrollbar(HorizontalScrollbar));

    // Verify that the MockScrollbar instances have the correct orientations.
    EXPECT_EQ(VerticalScrollbar, verticalScrollbar->orientation());
    EXPECT_EQ(HorizontalScrollbar, horizontalScrollbar->orientation());

    MockHitTestResult noScrollbarHitTestResult(0);
    MockHitTestResult horizontalScrollbarHitTestResult(horizontalScrollbar.get());
    MockHitTestResult verticalScrollbarHitTestResult(verticalScrollbar.get());

    // Verify that the MockHitTestResult instances have the right scrollbars.
    EXPECT_EQ(horizontalScrollbar, horizontalScrollbarHitTestResult.scrollbar());
    EXPECT_EQ(verticalScrollbar, verticalScrollbarHitTestResult.scrollbar());
    EXPECT_EQ(0, noScrollbarHitTestResult.scrollbar());
    EXPECT_EQ(HorizontalScrollbar, horizontalScrollbarHitTestResult.scrollbar()->orientation());

    MockPlatformWheelEvent preciseWheelEvent(true);
    MockPlatformWheelEvent basicWheelEvent(false);

    // Verify that the wheel event instances have the right hasPreciseScrollingDeltas setting.
    EXPECT_TRUE(preciseWheelEvent.hasPreciseScrollingDeltas());
    EXPECT_FALSE(basicWheelEvent.hasPreciseScrollingDeltas());

#if OS(UNIX) && !OS(DARWIN)
    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(noScrollbarHitTestResult, basicWheelEvent));
    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(noScrollbarHitTestResult, preciseWheelEvent));

    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(verticalScrollbarHitTestResult, basicWheelEvent));
    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(verticalScrollbarHitTestResult, preciseWheelEvent));

    EXPECT_TRUE(handler.externalShouldTurnVerticalTicksIntoHorizontal(horizontalScrollbarHitTestResult, basicWheelEvent));
    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(horizontalScrollbarHitTestResult, preciseWheelEvent));
#else
    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(noScrollbarHitTestResult, basicWheelEvent));
    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(noScrollbarHitTestResult, preciseWheelEvent));

    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(verticalScrollbarHitTestResult, basicWheelEvent));
    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(verticalScrollbarHitTestResult, preciseWheelEvent));

    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(horizontalScrollbarHitTestResult, basicWheelEvent));
    EXPECT_FALSE(handler.externalShouldTurnVerticalTicksIntoHorizontal(horizontalScrollbarHitTestResult, preciseWheelEvent));
#endif
}

