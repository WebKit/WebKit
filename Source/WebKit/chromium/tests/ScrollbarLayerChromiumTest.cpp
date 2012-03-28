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

#include "Scrollbar.h"
#include "Settings.h"
#include "TreeSynchronizer.h"
#include "cc/CCScrollbarLayerImpl.h"
#include "cc/CCSingleThreadProxy.h"
#include <gtest/gtest.h>

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

    virtual IntPoint convertFromContainingWindow(const IntPoint& windowPoint) { return IntPoint(); }

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

    MockScrollbar() : Scrollbar(0, HorizontalScrollbar, RegularScrollbar) { }
    virtual ~MockScrollbar() { }
};

TEST(ScrollbarLayerChromiumTest, resolveScrollLayerPointer)
{
    DebugScopedSetImplThread impl;

    Settings::setMockScrollbarsEnabled(true);
    {
        RefPtr<MockScrollbar> scrollbar = adoptRef(new MockScrollbar);
        RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
        RefPtr<LayerChromium> child1 = LayerChromium::create();
        RefPtr<LayerChromium> child2 = ScrollbarLayerChromium::create(scrollbar.get(), child1->id());
        layerTreeRoot->addChild(child1);
        layerTreeRoot->addChild(child2);

        OwnPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), nullptr);

        CCLayerImpl* ccChild1 = ccLayerTreeRoot->children()[0].get();
        CCScrollbarLayerImpl* ccChild2 = static_cast<CCScrollbarLayerImpl*>(ccLayerTreeRoot->children()[1].get());

        EXPECT_EQ(ccChild1, ccChild2->scrollLayer());
    }

    { // another traverse order
        RefPtr<MockScrollbar> scrollbar = adoptRef(new MockScrollbar);
        RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
        RefPtr<LayerChromium> child2 = LayerChromium::create();
        RefPtr<LayerChromium> child1 = ScrollbarLayerChromium::create(scrollbar.get(), child2->id());
        layerTreeRoot->addChild(child1);
        layerTreeRoot->addChild(child2);

        OwnPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), nullptr);

        CCScrollbarLayerImpl* ccChild1 = static_cast<CCScrollbarLayerImpl*>(ccLayerTreeRoot->children()[0].get());
        CCLayerImpl* ccChild2 = ccLayerTreeRoot->children()[1].get();

        EXPECT_EQ(ccChild1->scrollLayer(), ccChild2);
    }
}

}
