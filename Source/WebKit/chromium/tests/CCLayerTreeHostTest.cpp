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

#if USE(THREADED_COMPOSITING)

#include "cc/CCLayerTreeHost.h"

#include "GraphicsContext3D.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCMainThreadTask.h"
#include "cc/CCThreadTask.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>


using namespace WebCore;
using namespace WTF;

namespace {

class MockLayerTreeHost;
class MockLayerTreeHostClient;
class MockLayerTreeHostImpl;

// The CCLayerTreeHostTest runs with the main loop running. It instantiates a single MockLayerTreeHost and associated
// MockLayerTreeHostImpl/MockLayerTreeHostClient.
//
// beginTest() is called once the main message loop is running and the layer tree host is initialized.
//
// Key stages of the drawing loop, e.g. drawing or commiting, redirect to CCLayerTreeHostTest methods of similar names.
// To track the commit process, override these functions.
//
// The test continues until someone calls endTest. endTest can be called on any thread, but be aware that
// ending the test is an asynchronous process.
class CCLayerTreeHostTest : public testing::Test {
public:
    CCLayerTreeHostTest()
        : m_beginning(false)
        , m_endWhenBeginReturns(false)
        , m_running(false)
        , m_timedOut(false) { }

    virtual void afterTest() = 0;

    virtual void beginTest() = 0;
    virtual void animateAndLayout(MockLayerTreeHostClient* layerTreeHost, double frameBeginTime) { }
    virtual void beginCommitOnCCThread(MockLayerTreeHostImpl* layerTreeHostImpl) { }
    virtual void beginCommitOnMainThread(MockLayerTreeHost* layerTreeHost) { }
    virtual void commitOnCCThread(MockLayerTreeHost* layerTreeHost, MockLayerTreeHostImpl* layerTreeHostImpl) { }
    virtual void commitCompleteOnCCThread(MockLayerTreeHostImpl* layerTreeHostImpl) { }
    virtual void commitCompleteOnMainThread(MockLayerTreeHost* layerTreeHost) { }
    virtual void drawLayersAndPresentOnCCThread(MockLayerTreeHostImpl* layerTreeHostImpl) { }
    virtual void updateLayers(MockLayerTreeHost* layerTreeHost) { }

    void endTest();

protected:
    void doBeginTest();

    static void onBeginTest(void* self)
    {
        static_cast<CCLayerTreeHostTest*>(self)->doBeginTest();
    }

    void doEndTest()
    {
    }

    static void onEndTest(void* self)
    {
        ASSERT(isMainThread());
        CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
        test->m_layerTreeHost.clear();
        test->m_client.clear();
        webkit_support::QuitMessageLoop();
    }

    void runTest()
    {
        webkit_support::PostDelayedTask(CCLayerTreeHostTest::onBeginTest, static_cast<void*>(this), 0);
        webkit_support::PostDelayedTask(CCLayerTreeHostTest::testTimeout, static_cast<void*>(this), 5000);
        webkit_support::RunMessageLoop();
        m_running = false;
        bool timedOut = m_timedOut; // Save whether we're timed out in case RunAllPendingMessages has the timeout.
        webkit_support::RunAllPendingMessages();
        if (timedOut) {
            printf("Test timed out");
            FAIL() << "Test timed out";
            return;
        }
        afterTest();
    }

    static void testTimeout(void* self)
    {
        CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
        if (!test->m_running)
            return;
        test->m_timedOut = true;
        test->endTest();
    }

    Mutex m_tracesLock;
    Vector<std::string> m_traces;

    OwnPtr<MockLayerTreeHostClient> m_client;
    RefPtr<MockLayerTreeHost> m_layerTreeHost;

private:
    bool m_beginning;
    bool m_endWhenBeginReturns;
    bool m_running;
    bool m_timedOut;
};

class MockLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    MockLayerTreeHostClient(CCLayerTreeHostTest* test) : m_test(test) { }

    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D()
    {
        return adoptRef<GraphicsContext3D>(0);
    }

    virtual void animateAndLayout(double frameBeginTime)
    {
        m_test->animateAndLayout(this, frameBeginTime);
    }

    virtual void updateLayers()
    {
    }

private:
    CCLayerTreeHostTest* m_test;
};

class MockLayerTreeHostCommitter : public CCLayerTreeHostCommitter {
public:
    static PassOwnPtr<MockLayerTreeHostCommitter> create(CCLayerTreeHostTest* test)
    {
        return adoptPtr(new MockLayerTreeHostCommitter(test));
    }

    virtual void commit(CCLayerTreeHost* host, CCLayerTreeHostImpl* hostImpl)
    {
        CCLayerTreeHostCommitter::commit(host, hostImpl);
        m_test->commitOnCCThread(reinterpret_cast<MockLayerTreeHost*>(host), reinterpret_cast<MockLayerTreeHostImpl*>(hostImpl));
    }

private:
    MockLayerTreeHostCommitter(CCLayerTreeHostTest* test) : m_test(test) { }
    CCLayerTreeHostTest* m_test;
};

class MockLayerTreeHostImpl : public CCLayerTreeHostImpl {
public:
    static PassOwnPtr<MockLayerTreeHostImpl> create(CCLayerTreeHostImplClient* client, CCLayerTreeHostTest* test)
    {
        return adoptPtr(new MockLayerTreeHostImpl(client, test));
    }

    virtual void beginCommit()
    {
        CCLayerTreeHostImpl::beginCommit();
        m_test->beginCommitOnCCThread(this);
    }

    virtual void commitComplete()
    {
        CCLayerTreeHostImpl::commitComplete();
        m_test->commitCompleteOnCCThread(this);
    }

    virtual void drawLayersAndPresent()
    {
        m_test->drawLayersAndPresentOnCCThread(this);
    }

private:
    MockLayerTreeHostImpl(CCLayerTreeHostImplClient* client, CCLayerTreeHostTest* test)
            : CCLayerTreeHostImpl(client)
            , m_test(test)
    {
    }

    CCLayerTreeHostTest* m_test;
};

class MockLayerTreeHostImplProxy : public CCLayerTreeHostImplProxy {
public:
    static PassOwnPtr<MockLayerTreeHostImplProxy> create(CCLayerTreeHost* host, CCLayerTreeHostTest* test)
    {
        return adoptPtr(new MockLayerTreeHostImplProxy(host, test));
    }

    PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl()
    {
        return MockLayerTreeHostImpl::create(this, m_test);
    }

private:
    MockLayerTreeHostImplProxy(CCLayerTreeHost* host, CCLayerTreeHostTest* test)
        : CCLayerTreeHostImplProxy(host)
        , m_test(test) { }

    CCLayerTreeHostTest* m_test;
};

class MockLayerTreeHost : public CCLayerTreeHost {
public:
    MockLayerTreeHost(CCLayerTreeHostClient* client, CCLayerTreeHostTest* test)
        : CCLayerTreeHost(client)
        , m_test(test) { }

    virtual PassOwnPtr<CCLayerTreeHostImplProxy> createLayerTreeHostImplProxy()
    {
        OwnPtr<CCLayerTreeHostImplProxy> proxy = MockLayerTreeHostImplProxy::create(this, m_test);
        proxy->start();
        return proxy.release();
    }

    virtual void updateLayers()
    {
        m_test->updateLayers(this);
    }

    virtual PassOwnPtr<CCLayerTreeHostCommitter> createLayerTreeHostCommitter()
    {
        return MockLayerTreeHostCommitter::create(m_test);
    }

    virtual void beginCommit()
    {
        CCLayerTreeHost::beginCommit();
        m_test->beginCommitOnMainThread(this);
    }

    virtual void commitComplete()
    {
        m_test->commitCompleteOnMainThread(this);
        CCLayerTreeHost::commitComplete();
    }

private:
    CCLayerTreeHostTest* m_test;
};

void CCLayerTreeHostTest::doBeginTest()
{
    ASSERT(!m_running);
    m_running = true;
    m_client = adoptPtr(new MockLayerTreeHostClient(this));
    m_layerTreeHost = adoptRef(new MockLayerTreeHost(m_client.get(), this));
    m_layerTreeHost->init();
    m_beginning = true;
    beginTest();
    m_beginning = false;
    if (m_endWhenBeginReturns)
        onEndTest(static_cast<void*>(this));
}

void CCLayerTreeHostTest::endTest()
{
    // If we are called from the CCThread, re-call endTest on the main thread.
    if (!isMainThread())
        CCMainThread::postTask(createMainThreadTask(this, &CCLayerTreeHostTest::endTest));
    else {
        // For the case where we endTest during beginTest(), set a flag to indicate that
        // the test should end the second beginTest regains control.
        if (m_beginning)
            m_endWhenBeginReturns = true;
        else
            onEndTest(static_cast<void*>(this));
    }
}

// Shortlived layerTreeHosts shouldn't die.
class CCLayerTreeHostTestShortlived1 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived1() { }

    virtual void beginTest()
    {
        endTest();
    }

    virtual void afterTest()
    {
    }
};
TEST_F(CCLayerTreeHostTestShortlived1, run)
{
    runTest();
}

// Shortlived layerTreeHosts shouldn't die with a commit in flight.
class CCLayerTreeHostTestShortlived2 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived2() { }

    virtual void beginTest()
    {
        m_layerTreeHost->setNeedsCommitAndRedraw();
        endTest();
    }

    virtual void afterTest()
    {
    }
};
TEST_F(CCLayerTreeHostTestShortlived2, run)
{
    runTest();
}

// Shortlived layerTreeHosts shouldn't die with a redraw in flight.
class CCLayerTreeHostTestShortlived3 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived3() { }

    virtual void beginTest()
    {
        m_layerTreeHost->setNeedsRedraw();
        endTest();
    }

    virtual void afterTest()
    {
    }
};
TEST_F(CCLayerTreeHostTestShortlived3, run)
{
    runTest();
}

// Constantly redrawing layerTreeHosts shouldn't die when they commit
class CCLayerTreeHostTestCommitingWithContinuousRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestCommitingWithContinuousRedraw()
        : m_numCompleteCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setNeedsCommitAndRedraw();
        endTest();
    }

    virtual void commitCompleteOnCCThread(MockLayerTreeHostImpl* layerTreeHostImpl)
    {
        m_numCompleteCommits++;
        if (m_numCompleteCommits == 2)
            endTest();
    }

    virtual void drawLayersAndPresentOnCCThread(MockLayerTreeHostImpl* layerTreeHostImpl)
    {
        if (m_numDraws == 1)
            layerTreeHostImpl->setNeedsCommitAndRedraw();
        m_numDraws++;
        layerTreeHostImpl->setNeedsRedraw();
    }

    virtual void afterTest()
    {
    }

private:
    int m_numCompleteCommits;
    int m_numDraws;
};
TEST_F(CCLayerTreeHostTestCommitingWithContinuousRedraw, run)
{
    runTest();
}

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class CCLayerTreeHostTestSetNeedsCommit1 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsCommit1()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setNeedsCommitAndRedraw();
        m_layerTreeHost->setNeedsCommitAndRedraw();
    }

    virtual void drawLayersAndPresentOnCCThread(MockLayerTreeHostImpl* layerTreeHostImpl)
    {
        m_numDraws++;
        if (!layerTreeHostImpl->sourceFrameNumber())
            endTest();
    }

    virtual void commitOnCCThread(MockLayerTreeHost* layerTreeHost, MockLayerTreeHostImpl* impl)
    {
        m_numCommits++;
    }

    virtual void afterTest()
    {
        EXPECT_GE(1, m_numCommits);
        EXPECT_GE(1, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};
TEST_F(CCLayerTreeHostTestSetNeedsCommit1, run)
{
    runTest();
}

// A setNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class CCLayerTreeHostTestSetNeedsCommit2 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsCommit2()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setNeedsCommitAndRedraw();
    }

    virtual void drawLayersAndPresentOnCCThread(MockLayerTreeHostImpl* layerTreeHostImpl)
    {
        if (!layerTreeHostImpl->sourceFrameNumber())
            layerTreeHostImpl->setNeedsCommitAndRedraw();
        else if (layerTreeHostImpl->sourceFrameNumber() == 1)
            endTest();
    }

    virtual void commitOnCCThread(MockLayerTreeHost* layerTreeHost, MockLayerTreeHostImpl* impl)
    {
        m_numCommits++;
    }

    virtual void afterTest()
    {
        EXPECT_EQ(2, m_numCommits);
        EXPECT_GE(2, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};
TEST_F(CCLayerTreeHostTestSetNeedsCommit2, run)
{
    runTest();
}

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class CCLayerTreeHostTestSetNeedsRedraw : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestSetNeedsRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
    }

    virtual void drawLayersAndPresentOnCCThread(MockLayerTreeHostImpl* impl)
    {
        EXPECT_EQ(0, impl->sourceFrameNumber());
        if (!m_numDraws)
            impl->setNeedsRedraw(); // redraw again to verify that the second redraw doesnt commit.
        else
            endTest();
        m_numDraws++;
    }

    virtual void commitOnCCThread(MockLayerTreeHost* layerTreeHost, MockLayerTreeHostImpl* impl)
    {
        m_numCommits++;
    }

    virtual void afterTest()
    {
        EXPECT_GE(2, m_numDraws);
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};
TEST_F(CCLayerTreeHostTestSetNeedsRedraw, run)
{
    runTest();
}

} // namespace

#endif // USE(THREADED_COMPOSITING)
