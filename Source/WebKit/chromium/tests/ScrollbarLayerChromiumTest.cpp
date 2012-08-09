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

#include "ScrollbarLayerChromium.h"

#include "FakeWebScrollbarThemeGeometry.h"
#include "Scrollbar.h"
#include "Settings.h"
#include "TreeSynchronizer.h"
#include "cc/CCScrollbarAnimationController.h"
#include "cc/CCScrollbarLayerImpl.h"
#include "cc/CCSingleThreadProxy.h"
#include <gtest/gtest.h>
#include <public/WebScrollbar.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

using namespace WebCore;

namespace {

class MockScrollbar : public Scrollbar {
public:
    virtual int x() const { return 0; }
    virtual int y() const { return 0; }
    virtual int width() const { return 0; }
    virtual int height() const { return 0; }
    virtual IntSize size() const { return IntSize(); }
    virtual IntPoint location() const { return IntPoint(); }

    virtual ScrollView* parent() const { return 0; }
    virtual ScrollView* root() const { return 0; }

    virtual void setFrameRect(const IntRect&) { }
    virtual IntRect frameRect() const { return IntRect(); }

    virtual void invalidate() { }
    virtual void invalidateRect(const IntRect&) { }

    virtual ScrollbarOverlayStyle scrollbarOverlayStyle() const { return ScrollbarOverlayStyleDefault; }
    virtual void getTickmarks(Vector<IntRect>&) const { }
    virtual bool isScrollableAreaActive() const { return false; }
    virtual bool isScrollViewScrollbar() const { return false; }

    virtual IntPoint convertFromContainingWindow(const IntPoint& windowPoint) { return windowPoint; }

    virtual bool isCustomScrollbar() const { return false; }
    virtual ScrollbarOrientation orientation() const { return HorizontalScrollbar; }

    virtual int value() const { return 0; }
    virtual float currentPos() const { return 0; }
    virtual int visibleSize() const { return 1; }
    virtual int totalSize() const { return 1; }
    virtual int maximum() const { return 0; }
    virtual ScrollbarControlSize controlSize() const { return RegularScrollbar; }

    virtual int lineStep() const { return 0; }
    virtual int pageStep() const { return 0; }

    virtual ScrollbarPart pressedPart() const { return NoPart; }
    virtual ScrollbarPart hoveredPart() const { return NoPart; }

    virtual void styleChanged() { }

    virtual bool enabled() const { return false; }
    virtual void setEnabled(bool) { }

    virtual bool isOverlayScrollbar() const { return false; }

    MockScrollbar() : Scrollbar(0, HorizontalScrollbar, RegularScrollbar) { }
    virtual ~MockScrollbar() { }
};

TEST(ScrollbarLayerChromiumTest, resolveScrollLayerPointer)
{
    DebugScopedSetImplThread impl;

    RefPtr<MockScrollbar> mockScrollbar = adoptRef(new MockScrollbar);
    WebKit::WebScrollbarThemePainter painter(0, mockScrollbar.get());

    Settings::setMockScrollbarsEnabled(true);
    {
        OwnPtr<WebKit::WebScrollbar> scrollbar = WebKit::WebScrollbar::create(mockScrollbar.get());
        RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
        RefPtr<LayerChromium> child1 = LayerChromium::create();
        RefPtr<LayerChromium> child2 = ScrollbarLayerChromium::create(scrollbar.release(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), child1->id());
        layerTreeRoot->addChild(child1);
        layerTreeRoot->addChild(child2);

        OwnPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), nullptr, 0);

        CCLayerImpl* ccChild1 = ccLayerTreeRoot->children()[0].get();
        CCScrollbarLayerImpl* ccChild2 = static_cast<CCScrollbarLayerImpl*>(ccLayerTreeRoot->children()[1].get());

        EXPECT_TRUE(ccChild1->scrollbarAnimationController());
        EXPECT_EQ(ccChild1->horizontalScrollbarLayer(), ccChild2);
    }

    { // another traverse order
        OwnPtr<WebKit::WebScrollbar> scrollbar = WebKit::WebScrollbar::create(mockScrollbar.get());
        RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
        RefPtr<LayerChromium> child2 = LayerChromium::create();
        RefPtr<LayerChromium> child1 = ScrollbarLayerChromium::create(scrollbar.release(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), child2->id());
        layerTreeRoot->addChild(child1);
        layerTreeRoot->addChild(child2);

        OwnPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), nullptr, 0);

        CCScrollbarLayerImpl* ccChild1 = static_cast<CCScrollbarLayerImpl*>(ccLayerTreeRoot->children()[0].get());
        CCLayerImpl* ccChild2 = ccLayerTreeRoot->children()[1].get();

        EXPECT_TRUE(ccChild2->scrollbarAnimationController());
        EXPECT_EQ(ccChild2->horizontalScrollbarLayer(), ccChild1);
    }
}

TEST(ScrollbarLayerChromiumTest, scrollOffsetSynchronization)
{
    DebugScopedSetImplThread impl;

    RefPtr<MockScrollbar> mockScrollbar = adoptRef(new MockScrollbar);
    WebKit::WebScrollbarThemePainter painter(0, mockScrollbar.get());

    Settings::setMockScrollbarsEnabled(true);

    OwnPtr<WebKit::WebScrollbar> scrollbar = WebKit::WebScrollbar::create(mockScrollbar.get());
    RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
    RefPtr<LayerChromium> contentLayer = LayerChromium::create();
    RefPtr<LayerChromium> scrollbarLayer = ScrollbarLayerChromium::create(scrollbar.release(), painter, WebKit::FakeWebScrollbarThemeGeometry::create(), layerTreeRoot->id());
    layerTreeRoot->addChild(contentLayer);
    layerTreeRoot->addChild(scrollbarLayer);

    layerTreeRoot->setScrollPosition(IntPoint(10, 20));
    layerTreeRoot->setMaxScrollPosition(IntSize(30, 50));
    contentLayer->setBounds(IntSize(100, 200));

    OwnPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), nullptr, 0);

    CCScrollbarLayerImpl* ccScrollbarLayer = static_cast<CCScrollbarLayerImpl*>(ccLayerTreeRoot->children()[1].get());

    EXPECT_EQ(10, ccScrollbarLayer->currentPos());
    EXPECT_EQ(100, ccScrollbarLayer->totalSize());
    EXPECT_EQ(30, ccScrollbarLayer->maximum());

    layerTreeRoot->setScrollPosition(IntPoint(100, 200));
    layerTreeRoot->setMaxScrollPosition(IntSize(300, 500));
    contentLayer->setBounds(IntSize(1000, 2000));

    CCScrollbarAnimationController* scrollbarController = ccLayerTreeRoot->scrollbarAnimationController();
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.release(), 0);
    EXPECT_EQ(scrollbarController, ccLayerTreeRoot->scrollbarAnimationController());

    EXPECT_EQ(100, ccScrollbarLayer->currentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->totalSize());
    EXPECT_EQ(300, ccScrollbarLayer->maximum());

    ccLayerTreeRoot->scrollBy(FloatSize(12, 34));

    EXPECT_EQ(112, ccScrollbarLayer->currentPos());
    EXPECT_EQ(1000, ccScrollbarLayer->totalSize());
    EXPECT_EQ(300, ccScrollbarLayer->maximum());
}

}
