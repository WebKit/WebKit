/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "cc/CCLayerTreeHostImpl.h"

#include "cc/CCLayerImpl.h"
#include "cc/CCSingleThreadProxy.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class CCLayerTreeHostImplTest : public testing::Test, CCLayerTreeHostImplClient {
public:
    CCLayerTreeHostImplTest()
        : m_didRequestCommit(false)
        , m_didRequestRedraw(false)
    {
        CCSettings settings;
        m_hostImpl = CCLayerTreeHostImpl::create(settings, this);
    }

    virtual void onSwapBuffersCompleteOnImplThread() { }
    virtual void setNeedsRedrawOnImplThread() { m_didRequestRedraw = true; }
    virtual void setNeedsCommitOnImplThread() { m_didRequestCommit = true; }

    static void expectClearedScrollDeltasRecursive(CCLayerImpl* layer)
    {
        ASSERT_EQ(layer->scrollDelta(), IntSize());
        for (size_t i = 0; i < layer->children().size(); ++i)
            expectClearedScrollDeltasRecursive(layer->children()[i].get());
    }

    static void expectContains(const CCScrollUpdateSet& scrollInfo, int id, const IntSize& scrollDelta)
    {
        int timesEncountered = 0;

        for (size_t i = 0; i < scrollInfo.size(); ++i) {
            if (scrollInfo[i].layerId != id)
                continue;
            ASSERT_EQ(scrollInfo[i].scrollDelta, scrollDelta);
            timesEncountered++;
        }

        ASSERT_EQ(timesEncountered, 1);
    }

protected:
    DebugScopedSetImplThread m_alwaysImplThread;
    OwnPtr<CCLayerTreeHostImpl> m_hostImpl;
    bool m_didRequestCommit;
    bool m_didRequestRedraw;
};

TEST_F(CCLayerTreeHostImplTest, scrollDeltaNoLayers)
{
    ASSERT_FALSE(m_hostImpl->rootLayer());

    OwnPtr<CCScrollUpdateSet> scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(scrollInfo->size(), 0u);
}

TEST_F(CCLayerTreeHostImplTest, scrollDeltaTreeButNoChanges)
{
    RefPtr<CCLayerImpl> root = CCLayerImpl::create(0);
    root->addChild(CCLayerImpl::create(1));
    root->addChild(CCLayerImpl::create(2));
    root->children()[1]->addChild(CCLayerImpl::create(3));
    root->children()[1]->addChild(CCLayerImpl::create(4));
    root->children()[1]->children()[0]->addChild(CCLayerImpl::create(5));
    m_hostImpl->setRootLayer(root);

    expectClearedScrollDeltasRecursive(root.get());

    OwnPtr<CCScrollUpdateSet> scrollInfo;

    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(scrollInfo->size(), 0u);
    expectClearedScrollDeltasRecursive(root.get());

    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(scrollInfo->size(), 0u);
    expectClearedScrollDeltasRecursive(root.get());
}

TEST_F(CCLayerTreeHostImplTest, scrollDeltaRepeatedScrolls)
{
    IntPoint scrollPosition(20, 30);
    IntSize scrollDelta(11, -15);
    RefPtr<CCLayerImpl> root = CCLayerImpl::create(10);
    root->setScrollPosition(scrollPosition);
    root->setMaxScrollPosition(IntSize(100, 100));
    root->scrollBy(scrollDelta);
    m_hostImpl->setRootLayer(root);

    OwnPtr<CCScrollUpdateSet> scrollInfo;

    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(root->scrollPosition(), scrollPosition + scrollDelta);
    ASSERT_EQ(scrollInfo->size(), 1u);
    expectContains(*scrollInfo.get(), root->id(), scrollDelta);
    expectClearedScrollDeltasRecursive(root.get());

    IntSize scrollDelta2(-5, 27);
    root->scrollBy(scrollDelta2);
    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(root->scrollPosition(), scrollPosition + scrollDelta + scrollDelta2);
    ASSERT_EQ(scrollInfo->size(), 1u);
    expectContains(*scrollInfo.get(), root->id(), scrollDelta2);
    expectClearedScrollDeltasRecursive(root.get());

    root->scrollBy(IntSize());
    scrollInfo = m_hostImpl->processScrollDeltas();
    ASSERT_EQ(root->scrollPosition(), scrollPosition + scrollDelta + scrollDelta2);
    ASSERT_EQ(scrollInfo->size(), 0u);
    expectClearedScrollDeltasRecursive(root.get());
}

TEST_F(CCLayerTreeHostImplTest, scrollRootCallsCommitAndRedraw)
{
    RefPtr<CCLayerImpl> root = CCLayerImpl::create(0);
    root->setScrollPosition(IntPoint(0, 0));
    root->setMaxScrollPosition(IntSize(100, 100));
    m_hostImpl->setRootLayer(root);
    m_hostImpl->scrollRootLayer(IntSize(0, 10));
    ASSERT(m_didRequestRedraw);
    ASSERT(m_didRequestCommit);
}

} // namespace
