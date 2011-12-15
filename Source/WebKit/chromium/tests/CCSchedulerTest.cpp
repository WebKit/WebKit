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

#include "cc/CCScheduler.h"

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
    }

    void setHasMoreResourceUpdates(bool b) { m_hasMoreResourceUpdates = b; }
    void setCanDraw(bool b) { m_canDraw = b; }

    int numActions() const { return static_cast<int>(m_actions.size()); }
    const char* action(int i) const { return m_actions[i]; }

    virtual bool canDraw() { return m_canDraw; }
    virtual bool hasMoreResourceUpdates() const { return m_hasMoreResourceUpdates; }
    virtual void scheduledActionBeginFrame() { m_actions.push_back("scheduledActionBeginFrame"); }
    virtual void scheduledActionDrawAndSwap() { m_actions.push_back("scheduledActionDrawAndSwap"); }
    virtual void scheduledActionUpdateMoreResources() { m_actions.push_back("scheduledActionUpdateMoreResources"); }
    virtual void scheduledActionCommit() { m_actions.push_back("scheduledActionCommit"); }

protected:
    bool m_hasMoreResourceUpdates;
    bool m_canDraw;
    std::vector<const char*> m_actions;
};

TEST(CCSchedulerTest, RequestCommit)
{
    FakeCCSchedulerClient client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
    scheduler->setVisible(true);

    // SetNeedsCommit should begin the frame.
    scheduler->setNeedsCommit();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(0));
    client.reset();

    // Since, hasMoreResourceUpdates is set to false,
    // beginFrameComplete should updateMoreResources, then
    // commit
    scheduler->beginFrameComplete();
    EXPECT_EQ(2, client.numActions());
    EXPECT_STREQ("scheduledActionUpdateMoreResources", client.action(0));
    EXPECT_STREQ("scheduledActionCommit", client.action(1));
    client.reset();

    // Tick should draw.
    timeSource->tick();
    EXPECT_EQ(1, client.numActions());
    EXPECT_STREQ("scheduledActionDrawAndSwap", client.action(0));
    client.reset();

    // Tick should do nothing.
    timeSource->tick();
    EXPECT_EQ(0, client.numActions());
}

TEST(CCSchedulerTest, RequestCommitAfterBeginFrame)
{
    FakeCCSchedulerClient client;
    RefPtr<FakeCCTimeSource> timeSource = adoptRef(new FakeCCTimeSource());
    OwnPtr<CCScheduler> scheduler = CCScheduler::create(&client, adoptPtr(new CCFrameRateController(timeSource)));
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
    EXPECT_EQ(2, client.numActions());
    EXPECT_STREQ("scheduledActionDrawAndSwap", client.action(0));
    EXPECT_STREQ("scheduledActionBeginFrame", client.action(1));
    client.reset();
}

class SchedulerClientThatSetNeedsDrawInsideDraw : public CCSchedulerClient {
public:
    SchedulerClientThatSetNeedsDrawInsideDraw()
        : m_numDraws(0)
        , m_scheduler(0) { }

    void setScheduler(CCScheduler* scheduler) { m_scheduler = scheduler; }

    int numDraws() const { return m_numDraws; }

    virtual bool hasMoreResourceUpdates() const { return false; }
    virtual bool canDraw() { return true; }
    virtual void scheduledActionBeginFrame() { }
    virtual void scheduledActionDrawAndSwap()
    {
        // Only setNeedsRedraw the first time this is called
        if (!m_numDraws)
            m_scheduler->setNeedsRedraw();
        m_numDraws++;
    }

    virtual void scheduledActionUpdateMoreResources() { }
    virtual void scheduledActionCommit() { }

protected:
    int m_numDraws;
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
    scheduler->setVisible(true);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_EQ(0, client.numDraws());

    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());
    EXPECT_TRUE(scheduler->redrawPending());

    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_FALSE(scheduler->redrawPending());
}

class SchedulerClientThatSetNeedsCommitInsideDraw : public CCSchedulerClient {
public:
    SchedulerClientThatSetNeedsCommitInsideDraw()
        : m_numDraws(0)
        , m_scheduler(0) { }

    void setScheduler(CCScheduler* scheduler) { m_scheduler = scheduler; }

    int numDraws() const { return m_numDraws; }

    virtual bool hasMoreResourceUpdates() const { return false; }
    virtual bool canDraw() { return true; }
    virtual void scheduledActionBeginFrame() { }
    virtual void scheduledActionDrawAndSwap()
    {
        // Only setNeedsCommit the first time this is called
        if (!m_numDraws)
            m_scheduler->setNeedsCommit();
        m_numDraws++;
    }

    virtual void scheduledActionUpdateMoreResources() { }
    virtual void scheduledActionCommit() { }

protected:
    int m_numDraws;
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
    scheduler->setVisible(true);

    scheduler->setNeedsRedraw();
    EXPECT_TRUE(scheduler->redrawPending());
    EXPECT_EQ(0, client.numDraws());

    timeSource->tick();
    EXPECT_EQ(1, client.numDraws());
    EXPECT_TRUE(scheduler->commitPending());
    scheduler->beginFrameComplete();

    timeSource->tick();
    EXPECT_EQ(2, client.numDraws());
    EXPECT_FALSE(scheduler->redrawPending());
}

}
