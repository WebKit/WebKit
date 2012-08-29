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

#include "CCLayerTreeHost.h"
#include "CCThreadedTest.h"
#include "HeadsUpDisplayLayerChromium.h"
#include "LayerChromium.h"

using namespace WebCore;
using namespace WebKitTests;

namespace {

class CCHeadsUpDisplayTest : public CCThreadedTest {
protected:
    virtual void initializeSettings(CCLayerTreeSettings& settings) OVERRIDE
    {
        // Enable the HUD without requiring text.
        settings.showPropertyChangedRects = true;
    }
};

class DrawsContentLayerChromium : public LayerChromium {
public:
    static PassRefPtr<DrawsContentLayerChromium> create() { return adoptRef(new DrawsContentLayerChromium()); }
    virtual bool drawsContent() const OVERRIDE { return true; }

private:
    DrawsContentLayerChromium() : LayerChromium() { }
};

class CCHudWithRootLayerChange : public CCHeadsUpDisplayTest {
public:
    CCHudWithRootLayerChange()
        : m_rootLayer1(DrawsContentLayerChromium::create())
        , m_rootLayer2(DrawsContentLayerChromium::create())
        , m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_rootLayer1->setBounds(IntSize(30, 30));
        m_rootLayer2->setBounds(IntSize(30, 30));

        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        ++m_numCommits;

        ASSERT_TRUE(m_layerTreeHost->hudLayer());

        switch (m_numCommits) {
        case 1:
            // Change directly to a new root layer.
            m_layerTreeHost->setRootLayer(m_rootLayer1);
            break;
        case 2:
            EXPECT_EQ(m_rootLayer1.get(), m_layerTreeHost->hudLayer()->parent());
            // Unset the root layer.
            m_layerTreeHost->setRootLayer(0);
            break;
        case 3:
            EXPECT_EQ(0, m_layerTreeHost->hudLayer()->parent());
            // Change back to the previous root layer.
            m_layerTreeHost->setRootLayer(m_rootLayer1);
            break;
        case 4:
            EXPECT_EQ(m_rootLayer1.get(), m_layerTreeHost->hudLayer()->parent());
            // Unset the root layer.
            m_layerTreeHost->setRootLayer(0);
            break;
        case 5:
            EXPECT_EQ(0, m_layerTreeHost->hudLayer()->parent());
            // Change to a new root layer from a null root.
            m_layerTreeHost->setRootLayer(m_rootLayer2);
            break;
        case 6:
            EXPECT_EQ(m_rootLayer2.get(), m_layerTreeHost->hudLayer()->parent());
            // Change directly back to the last root layer/
            m_layerTreeHost->setRootLayer(m_rootLayer1);
            break;
        case 7:
            EXPECT_EQ(m_rootLayer1.get(), m_layerTreeHost->hudLayer()->parent());
            endTest();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    RefPtr<DrawsContentLayerChromium> m_rootLayer1;
    RefPtr<DrawsContentLayerChromium> m_rootLayer2;
    int m_numCommits;
};

TEST_F(CCHudWithRootLayerChange, runMultiThread)
{
    runTest(true);
}

} // namespace
