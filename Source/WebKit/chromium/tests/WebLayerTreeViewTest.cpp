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

#include <public/WebLayerTreeView.h>

#include "CompositorFakeWebGraphicsContext3D.h"
#include "FakeWebCompositorOutputSurface.h"
#include "WebLayerTreeViewTestCommon.h"
#include <gmock/gmock.h>
#include <public/Platform.h>
#include <public/WebCompositor.h>
#include <public/WebLayer.h>
#include <public/WebLayerTreeViewClient.h>
#include <public/WebThread.h>

using namespace WebKit;
using testing::Mock;
using testing::Test;

namespace {

class MockWebLayerTreeViewClientForThreadedTests : public MockWebLayerTreeViewClient {
public:
    virtual void didBeginFrame() OVERRIDE
    {
        WebKit::Platform::current()->currentThread()->exitRunLoop();
        MockWebLayerTreeViewClient::didBeginFrame();
    }
};

class WebLayerTreeViewTestBase : public Test {
protected:
    virtual void initializeCompositor() = 0;
    virtual WebLayerTreeViewClient* client() = 0;

public:
    virtual void SetUp()
    {
        initializeCompositor();
        m_rootLayer = WebLayer::create();
        EXPECT_TRUE(m_view.initialize(client(), m_rootLayer, WebLayerTreeView::Settings()));
        m_view.setSurfaceReady();
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(client());

        m_view.setRootLayer(0);
        m_rootLayer.reset();
        m_view.reset();
        WebKit::WebCompositor::shutdown();
    }

protected:
    WebLayer m_rootLayer;
    WebLayerTreeView m_view;
};

class WebLayerTreeViewSingleThreadTest : public WebLayerTreeViewTestBase {
protected:
    void composite()
    {
        m_view.composite();
    }

    virtual void initializeCompositor() OVERRIDE
    {
        WebKit::WebCompositor::initialize(0);
    }

    virtual WebLayerTreeViewClient* client() OVERRIDE
    {
        return &m_client;
    }

    MockWebLayerTreeViewClient m_client;
};

class CancelableTaskWrapper : public RefCounted<CancelableTaskWrapper> {
    class Task : public WebThread::Task {
    public:
        Task(CancelableTaskWrapper* cancelableTask)
            : m_cancelableTask(cancelableTask)
        {
        }

    private:
        virtual void run() OVERRIDE
        {
            m_cancelableTask->runIfNotCanceled();
        }

        RefPtr<CancelableTaskWrapper> m_cancelableTask;
    };

public:
    CancelableTaskWrapper(PassOwnPtr<WebThread::Task> task)
        : m_task(task)
    {
    }

    void cancel()
    {
        m_task.clear();
    }

    WebThread::Task* createTask()
    {
        ASSERT(m_task);
        return new Task(this);
    }

    void runIfNotCanceled()
    {
        if (!m_task)
            return;
        m_task->run();
        m_task.clear();
    }

private:
    OwnPtr<WebThread::Task> m_task;
};

class WebLayerTreeViewThreadedTest : public WebLayerTreeViewTestBase {
protected:
    class TimeoutTask : public WebThread::Task {
        virtual void run() OVERRIDE
        {
            WebKit::Platform::current()->currentThread()->exitRunLoop();
        }
    };

    void composite()
    {
        m_view.setNeedsRedraw();
        RefPtr<CancelableTaskWrapper> timeoutTask = adoptRef(new CancelableTaskWrapper(adoptPtr(new TimeoutTask())));
        WebKit::Platform::current()->currentThread()->postDelayedTask(timeoutTask->createTask(), 5000);
        WebKit::Platform::current()->currentThread()->enterRunLoop();
        timeoutTask->cancel();
        m_view.finishAllRendering();
    }

    virtual void initializeCompositor() OVERRIDE
    {
        m_webThread = adoptPtr(WebKit::Platform::current()->createThread("WebLayerTreeViewTest"));
        WebCompositor::initialize(m_webThread.get());
    }

    virtual WebLayerTreeViewClient* client() OVERRIDE
    {
        return &m_client;
    }

    MockWebLayerTreeViewClientForThreadedTests m_client;
    OwnPtr<WebThread> m_webThread;
};

TEST_F(WebLayerTreeViewSingleThreadTest, InstrumentationCallbacks)
{
    ::testing::InSequence dummy;

    EXPECT_CALL(m_client, willCommit());
    EXPECT_CALL(m_client, didCommit());
    EXPECT_CALL(m_client, didBeginFrame());

    composite();
}

TEST_F(WebLayerTreeViewThreadedTest, InstrumentationCallbacks)
{
    ::testing::InSequence dummy;

    EXPECT_CALL(m_client, willBeginFrame());
    EXPECT_CALL(m_client, willCommit());
    EXPECT_CALL(m_client, didCommit());
    EXPECT_CALL(m_client, didBeginFrame());

    composite();
}

} // namespace
