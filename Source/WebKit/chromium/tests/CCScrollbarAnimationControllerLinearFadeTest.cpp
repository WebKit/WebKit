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

#include "CCScrollbarAnimationControllerLinearFade.h"

#include "CCScrollbarLayerImpl.h"
#include "CCSingleThreadProxy.h"
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>

using namespace WebCore;

namespace {

class CCScrollbarAnimationControllerLinearFadeTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        m_scrollLayer = CCLayerImpl::create(1);
        m_scrollLayer->addChild(CCLayerImpl::create(2));
        m_contentLayer = m_scrollLayer->children()[0].get();
        m_scrollbarLayer = CCScrollbarLayerImpl::create(3);

        m_scrollLayer->setMaxScrollPosition(IntSize(50, 50));
        m_contentLayer->setBounds(IntSize(50, 50));

        m_scrollbarController = CCScrollbarAnimationControllerLinearFade::create(m_scrollLayer.get(), 2, 3);
        m_scrollbarController->setHorizontalScrollbarLayer(m_scrollbarLayer.get());
    }

    DebugScopedSetImplThread implThread;

    OwnPtr<CCScrollbarAnimationControllerLinearFade> m_scrollbarController;
    OwnPtr<CCLayerImpl> m_scrollLayer;
    CCLayerImpl* m_contentLayer;
    OwnPtr<CCScrollbarLayerImpl> m_scrollbarLayer;

};

TEST_F(CCScrollbarAnimationControllerLinearFadeTest, verifyHiddenInBegin)
{
    m_scrollbarController->animate(0);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 0);
    m_scrollbarController->animate(0);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());
}

TEST_F(CCScrollbarAnimationControllerLinearFadeTest, verifyAwakenByScroll)
{
    m_scrollLayer->setScrollDelta(IntSize(1, 1));
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 0);
    m_scrollbarController->animate(0);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(1);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollLayer->setScrollDelta(IntSize(2, 2));
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 1);
    m_scrollbarController->animate(2);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(3);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(4);
    // Note that we use 3.0f to avoid "argument is truncated from 'double' to
    // 'float'" warnings on Windows.
    EXPECT_FLOAT_EQ(2 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(5);
    EXPECT_FLOAT_EQ(1 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollLayer->setScrollDelta(IntSize(3, 3));
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 5);
    m_scrollbarController->animate(6);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(7);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(8);
    EXPECT_FLOAT_EQ(2 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(9);
    EXPECT_FLOAT_EQ(1 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(10);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());
}

TEST_F(CCScrollbarAnimationControllerLinearFadeTest, verifyForceAwakenByPinch)
{
    m_scrollbarController->didPinchGestureBeginAtTime(0);
    m_scrollbarController->didPinchGestureUpdateAtTime(0);
    m_scrollbarController->animate(0);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(1);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollLayer->setScrollDelta(IntSize(1, 1));
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 1);
    m_scrollbarController->animate(2);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(3);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(4);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(5);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(6);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->didPinchGestureEndAtTime(6);
    m_scrollbarController->animate(7);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(8);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(9);
    EXPECT_FLOAT_EQ(2 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(10);
    EXPECT_FLOAT_EQ(1 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(11);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());

}

}
