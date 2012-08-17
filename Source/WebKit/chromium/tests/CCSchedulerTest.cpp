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

#include "CCScheduler.h"

#include "CCSchedulerTestCommon.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>

using namespace WTF;
using namespace WebCore;
using namespace WebKitTests;

namespace {

class FakeCCSchedulerClient : public CCSchedulerClient {
public:
    FakeCCSchedulerClient() { reset(); }
    void reset()
    {
        m_actions.clear();
        m_hasMoreResourceUpdates = false;
        m_canDraw = true;
        m_drawWillHappen = true;
        m_swapWillHappenIfDrawHappens = true;
        m_numDraws = 0;
    }

    void setHasMoreResourceUpdates(bool b) { m_hasMoreResourceUpdates = b; }
    void setCanDraw(bool b) { m_canDraw = b; }

    int numDraws() const { return m_numDraws; }
    int numActions() const { return static_cast<int>(m_actions.size()); }
    const char* action(int i) const { return m_actions[i]; }

    bool hasAction(const char* action) const
    {
        for (size_t i = 0; i < m_actions.size(); i++)
            if (!strcmp(m_actions[i], action))
                return true;
        return false;
    }

    virtual bool canDraw() OVERRIDE { return m_canDraw; }
    virtual bool hasMoreResourceUpdates() const OVERRIDE { return m_hasMoreResourceUpdates; }
    virtual void scheduledActionBeginFrame() OVERRIDE { m_actions.push_back("scheduledActionBeginFrame"); }
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE
    {
        m_actions.push_back("scheduledActionDrawAndSwapIfPossible");
        m_numDraws++;
        return CCScheduledActionDrawAndSwapResult(m_drawWillHappen, m_drawWillHappen && m_swapWillHappenIfDrawHappens);
    }

    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE
    {
        m_actions.push_back("scheduledActionDrawAndSwapForced");
        return CCScheduledActionDrawAndSwapResult(true, m_swapWillHappenIfDrawHappens);
    }

    virtual void scheduledActionUpdateMoreResources() OVERRIDE { m_actions.push_back("scheduledActionUpdateMoreResources"); }
    virtual void scheduledActionCommit() OVERRIDE { m_actions.push_back("scheduledActionCommit"); }
    virtual void scheduledActionBeginContextRecreation() OVERRIDE { m_actions.push_back("scheduledActionBeginContextRecreation"); }
    virtual void scheduledActionAcquireLayerTexturesForMainThread() OVERRIDE { m_actions.push_back("scheduledActionAcquireLayerTexturesForMainThread"); }

    void setDrawWillHappen(bool drawWillHappen) { m_drawWillHappen = drawWillHappen; }
    void setSwapWillHappenIfDrawHappens(bool swapWillHappenIfDrawHappens) { m_swapWillHappenIfDrawHappens = swapWillHappenIfDrawHappens; }

protected:
    bool m_hasMoreResourceUpdates;
    bool m_canDraw;
    bool m_drawWillHappen;
    bool m_swapWillHappenIfDrawHappens;
    int m_numDraws;
    std::vector<const char*> m_actions;
};

TEST(CCSchedulerTest, RequestCommit)
{
    FakeCCSchedulerClient client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);

    // SetNeedsCommit should begin the frame.
    scheduler->setNeedsCommit();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    EXPECT_FALSE(timeSource->active());
    client.reset();

    // Since, hasMoreResourceUpdates is set to false,
    // beginFrameComplete should updateMoreResources, then
    // commit
    scheduler->beginFrameComplete();
    EXPECT_EQ(2, client.numActions());
    EXPECT_STREQ("scheduledActionUpdateMoreResources", client.action(0));
    EXPECT_STREQ("scheduledActionCommit", client.action(1));
    EXPECT_TRUE(timeSource->active());
    client.reset();

    // Tick should draw.
    timeSource->tick();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionDrawAndSwapIfPossible", client.action(0));
    EXPECT_FALSE(timeSource->active());
    client.reset();

    // Timer should be off.
    EXPECT_FALSE(timeSource->active());
}

TEST(CCSchedulerTest, RequestCommitAfterBeginFrame)
{
    FakeCCSchedulerClient client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);

    // SetNedsCommit should begin the frame.
    scheduler->setNeedsCommit();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    client.reset();

    // Now setNeedsCommit again. Calling here means we need a second frame.
    scheduler->setNeedsCommit();

    // Since, hasMoreResourceUpdates is set to false, and another commit is
    // needed, beginFrameComplete should updateMoreResources, then commit, then
    // begin another frame.
    scheduler->beginFrameComplete();
    EXPECT_EQ(2, client.numActions());
    EXPECT_STREQ("scheduledActionUpdateMoreResources", client.action(0));
    EXPECT_STREQ("scheduledActionCommit", client.action(1));
    client.reset();

    // Tick should draw but then begin another frame.
    timeSource->tick();
    EXPECT_FALSE(timeSource->active());
    EXPECT_EQ(2, client.numActions());
    EXPECT_STREQ("scheduledActionDrawAndSwapIfPossible", client.action(0));
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(1));
    client.reset();
}

TEST(CCSchedulerTest, TextureAcquisitionCollision)
{
    FakeCCSchedulerClient client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);

    scheduler->setNeedsCommit();
    scheduler->setMainThreadNeedsLayerTextures();
    EXPECT_EQ(2, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    EXPECT_STREQ("scheduledActionAcquireLayerTexturesForMainThread", client.action(1));
    client.reset();

    // Compositor not scheduled to draw because textures are locked by main thread
    EXPECT_FALSE(timeSource->active());

    // Trigger the commit
    scheduler->beginFrameComplete();
    EXPECT_TRUE(timeSource->active());
    client.reset();

    // Between commit and draw, texture acquisition for main thread delayed,
    // and main thread blocks.
    scheduler->setMainThreadNeedsLayerTextures();
    EXPECT_EQ(0, client.numActions());
    client.reset();

    // Once compositor draw complete, the delayed texture acquisition fires.
    timeSource->tick();
    EXPECT_EQ(3, client.numActions());
    EXPECT_STREQ("scheduledActionDrawAndSwapIfPossible", client.action(0));
    EXPECT_STREQ("scheduledActionAcquireLayerTexturesForMainThread", client.action(1));
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(2));
    client.reset();
}

TEST(CCSchedulerTest, VisibilitySwitchWithTextureAcquisition)
{
    FakeCCSchedulerClient client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);

    scheduler->setNeedsCommit();
    scheduler->beginFrameComplete();
    scheduler->setMainThreadNeedsLayerTextures();
    client.reset();
    // Verify that pending texture acquisition fires when visibility
    // is lost in order to avoid a deadlock.
    scheduler->setVisible(false);
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionAcquireLayerTexturesForMainThread", client.action(0));
    client.reset();

    // Regaining visibility with textures acquired by main thread while
    // compositor is waiting for first draw should result in a request
    // for a new frame in order to escape a deadlock.
    scheduler->setVisible(true);
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    client.reset();
}

class SchedulerClientThatSetNeedsDrawInsideDraw : public FakeCCSchedulerClient {
public:
    SchedulerClientThatSetNeedsDrawInsideDraw()
        : m_scheduler(0) { }

    void setScheduler(CCScheduler* scheduler) { m_scheduler = scheduler; }

    virtual void scheduledActionBeginFrame() OVERRIDE { }
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE
    {
        // Only setNeedsRedraw the first time this is called
        if (!m_numDraws)
            m_scheduler->setNeedsRedraw();
        return FakeCCSchedulerClient::scheduledActionDrawAndSwapIfPossible();
    }

    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE
    {
        ASSERT_NOT_REACHED();
        return CCScheduledActionDrawAndSwapResult(true, true);
    }

    virtual void scheduledActionUpdateMoreResources() OVERRIDE { }
    virtual void scheduledActionCommit() OVERRIDE { }
    virtual void scheduledActionBeginContextRecreation() OVERRIDE { }

protected:
    CCScheduler* m_scheduler;
};

// Tests for two different situations:
// 1. the scheduler dropping setNeedsRedraw requests that happen inside
//    a scheduledActionDrawAndSwap
// 2. the scheduler drawing twice inside a single tick
TEST(CCSchedulerTest, RequestRedrawInsideDraw)
{
    SchedulerClientThatSetNeedsDrawInsideDraw client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());
    EXPECT_EQ(0, client.numDraws());

    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_FALSE(scheduler->redrawPending());
    EXPECT_FALSE(timeSource->active());
}

// Test that requesting redraw inside a failed draw doesn't lose the request.
TEST(CCSchedulerTest, RequestRedrawInsideFailedDraw)
{
    SchedulerClientThatSetNeedsDrawInsideDraw client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    client.setDrawWillHappen(false);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());
    EXPECT_EQ(0, client.numDraws());

    // Fail the draw.
    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());

    // We have a commit pending and the draw failed, and we didn't lose the redraw request.
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Fail the draw again.
    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Draw successfully.
    client.setDrawWillHappen(true);
    timeSource->tick();
    EXPECT_EQ(3, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_FALSE(scheduler->redrawPending());
    EXPECT_FALSE(timeSource->active());
}

class SchedulerClientThatSetNeedsCommitInsideDraw : public FakeCCSchedulerClient {
public:
    SchedulerClientThatSetNeedsCommitInsideDraw()
        : m_scheduler(0) { }

    void setScheduler(CCScheduler* scheduler) { m_scheduler = scheduler; }

    virtual void scheduledActionBeginFrame() OVERRIDE { }
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE
    {
        // Only setNeedsCommit the first time this is called
        if (!m_numDraws)
            m_scheduler->setNeedsCommit();
        return FakeCCSchedulerClient::scheduledActionDrawAndSwapIfPossible();
    }

    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE
    {
        ASSERT_NOT_REACHED();
        return CCScheduledActionDrawAndSwapResult(true, true);
    }

    virtual void scheduledActionUpdateMoreResources() OVERRIDE { }
    virtual void scheduledActionCommit() OVERRIDE { }
    virtual void scheduledActionBeginContextRecreation() OVERRIDE { }

protected:
    CCScheduler* m_scheduler;
};

// Tests for the scheduler infinite-looping on setNeedsCommit requests that
// happen inside a scheduledActionDrawAndSwap
TEST(CCSchedulerTest, RequestCommitInsideDraw)
{
    SchedulerClientThatSetNeedsCommitInsideDraw client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_EQ(0, client.numDraws());
    EXPECT_TRUE(timeSource->active());

    timeSource->tick();
    EXPECT_FALSE(timeSource->active());
    EXPECT_EQ(1, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    scheduler->beginFrameComplete();

    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_FALSE(timeSource->active());
    EXPECT_FALSE(scheduler->redrawPending());
}

// Tests that when a draw fails then the pending commit should not be dropped.
TEST(CCSchedulerTest, RequestCommitInsideFailedDraw)
{
    SchedulerClientThatSetNeedsDrawInsideDraw client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);
    client.setDrawWillHappen(false);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());
    EXPECT_EQ(0, client.numDraws());

    // Fail the draw.
    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());

    // We have a commit pending and the draw failed, and we didn't lose the commit request.
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Fail the draw again.
    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Draw successfully.
    client.setDrawWillHappen(true);
    timeSource->tick();
    EXPECT_EQ(3, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    EXPECT_FALSE(scheduler->redrawPending());
    EXPECT_FALSE(timeSource->active());
}

TEST(CCSchedulerTest, NoBeginFrameWhenDrawFails)
{
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    SchedulerClientThatSetNeedsCommitInsideDraw client;
    OwnPtr<FakeCCFrameRateController> controller = adoptPtr(new FakeCCFrameRateController(timeSource));
    FakeCCFrameRateController* controllerPtr = controller.get();
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, controller.release());
    client.setScheduler(scheduler.get());
    scheduler->setCanBeginFrame(true);
    scheduler->setVisible(true);

    EXPECT_EQ(0, controllerPtr->numFramesPending());

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());
    EXPECT_EQ(0, client.numDraws());

    // Draw successfully, this starts a new frame.
    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());
    EXPECT_EQ(1, controllerPtr->numFramesPending());
    scheduler->didSwapBuffersComplete();
    EXPECT_EQ(0, controllerPtr->numFramesPending());

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_TRUE(timeSource->active());

    // Fail to draw, this should not start a frame.
    client.setDrawWillHappen(false);
    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_EQ(0, controllerPtr->numFramesPending());
}

TEST(CCSchedulerTest, NoBeginFrameWhenSwapFailsDuringForcedCommit)
{
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    FakeCCSchedulerClient client;
    OwnPtr<FakeCCFrameRateController> controller = adoptPtr(new FakeCCFrameRateController(timeSource));
    FakeCCFrameRateController* controllerPtr = controller.get();
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, controller.release());

    EXPECT_EQ(0, controllerPtr->numFramesPending());

    // Tell the client that it will fail to swap.
    client.setDrawWillHappen(true);
    client.setSwapWillHappenIfDrawHappens(false);

    // Get the compositor to do a scheduledActionDrawAndSwapForced.
    scheduler->setNeedsRedraw();
    scheduler->setNeedsForcedRedraw();
    EXPECT_TRUE(client.hasAction("scheduledActionDrawAndSwapForced"));

    // We should not have told the frame rate controller that we began a frame.
    EXPECT_EQ(0, controllerPtr->numFramesPending());
}

}
