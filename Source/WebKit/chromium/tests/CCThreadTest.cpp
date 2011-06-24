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

#include "cc/CCThread.h"

#include "cc/CCCompletionEvent.h"
#include "cc/CCMainThreadTask.h"
#include "cc/CCThreadTask.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebCore;

namespace {

class PingPongUsingCondition {
public:
    void ping(CCCompletionEvent* completion)
    {
        hitThreadID = currentThread();
        completion->signal();
    }

    ThreadIdentifier hitThreadID;
};


TEST(CCThreadTest, pingPongUsingCondition)
{
    OwnPtr<CCThread> thread = CCThread::create();
    PingPongUsingCondition target;
    CCCompletionEvent completion;
    thread->postTask(createCCThreadTask(&target, &PingPongUsingCondition::ping, 
                                        AllowCrossThreadAccess(&completion)));
    completion.wait();

    EXPECT_EQ(thread->threadID(), target.hitThreadID);
}

class PingPongTestUsingTasks {
public:
    void ping()
    {
        CCMainThread::postTask(createMainThreadTask(this, &PingPongTestUsingTasks::pong));
        hit = true;
    }

    void pong()
    {
        EXPECT_TRUE(isMainThread());
        webkit_support::QuitMessageLoop();
    }

    bool hit;
};

TEST(CCThreadTest, DISABLED_startPostAndWaitOnCondition)
{
    OwnPtr<CCThread> thread = CCThread::create();

    PingPongTestUsingTasks target;
    thread->postTask(createCCThreadTask(&target, &PingPongTestUsingTasks::ping));
    webkit_support::RunMessageLoop();

    EXPECT_TRUE(target.hit);
}

} // namespace
