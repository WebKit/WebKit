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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>

using namespace WTF;
using namespace WebCore;

namespace {

class MockCCSchedulerClient : public CCSchedulerClient {
public:
    MOCK_METHOD0(scheduleBeginFrameAndCommit, void());
    MOCK_METHOD0(scheduleDrawAndSwap, void());
};

class CCSchedulerTest : public testing::Test {
public:
    CCSchedulerTest()
        : m_client(adoptPtr(new MockCCSchedulerClient))
        , m_scheduler(CCScheduler::create(m_client.get()))
    {
        EXPECT_CALL(*m_client, scheduleBeginFrameAndCommit()).Times(0);
        EXPECT_CALL(*m_client, scheduleDrawAndSwap()).Times(0);
    }

protected:
    OwnPtr<MockCCSchedulerClient> m_client;
    OwnPtr<CCScheduler> m_scheduler;
};

TEST_F(CCSchedulerTest, RequestCommit)
{
    EXPECT_CALL(*m_client, scheduleBeginFrameAndCommit()).Times(1);
    m_scheduler->requestCommit();
}

TEST_F(CCSchedulerTest, RequestCommitTwiceBeforeCommit)
{
    EXPECT_CALL(*m_client, scheduleBeginFrameAndCommit()).Times(1);
    m_scheduler->requestCommit();
    m_scheduler->requestCommit();

    EXPECT_CALL(*m_client, scheduleBeginFrameAndCommit()).Times(1);
    m_scheduler->didCommit();
    m_scheduler->requestCommit();
}

TEST_F(CCSchedulerTest, RequestRedraw)
{
    EXPECT_CALL(*m_client, scheduleDrawAndSwap()).Times(1);
    m_scheduler->requestRedraw();
}

}
