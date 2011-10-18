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

#include "cc/CCLayerTreeHost.h"

#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCMainThreadTask.h"
#include "cc/CCScopedMainThreadProxy.h"
#include "cc/CCThreadTask.h"
#include "GraphicsContext3DPrivate.h"
#include <gtest/gtest.h>
#include "LayerChromium.h"
#include "MockWebGraphicsContext3D.h"
#include "TextureManager.h"
#include "WebCompositor.h"
#include "WebKit.h"
#include "WebKitPlatformSupport.h"
#include "WebThread.h"
#include <webkit/support/webkit_support.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace WebKit;
using namespace WTF;

namespace {

// Used by test stubs to notify the test when something interesting happens.
class TestHooks {
public:
    virtual void beginCommitOnCCThread(CCLayerTreeHostImpl*) { }
    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) { }
    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl*) { }
    virtual void applyScrollDelta(const IntSize&) { }
};

// Adapts CCLayerTreeHostImpl for test. Runs real code, then invokes test hooks.
class MockLayerTreeHostImpl : public CCLayerTreeHostImpl {
public:
    static PassOwnPtr<MockLayerTreeHostImpl> create(TestHooks* testHooks, const CCSettings& settings)
    {
        return adoptPtr(new MockLayerTreeHostImpl(testHooks, settings));
    }

    virtual void beginCommit()
    {
        CCLayerTreeHostImpl::beginCommit();
        m_testHooks->beginCommitOnCCThread(this);
    }

    virtual void commitComplete()
    {
        CCLayerTreeHostImpl::commitComplete();
        m_testHooks->commitCompleteOnCCThread(this);
    }

    virtual void drawLayers()
    {
        CCLayerTreeHostImpl::drawLayers();
        m_testHooks->drawLayersOnCCThread(this);
    }

private:
    MockLayerTreeHostImpl(TestHooks* testHooks, const CCSettings& settings)
        : CCLayerTreeHostImpl(settings)
        , m_testHooks(testHooks)
    {
    }

    TestHooks* m_testHooks;
};

// Adapts CCLayerTreeHost for test. Injects MockLayerTreeHostImpl.
class MockLayerTreeHost : public CCLayerTreeHost {
public:
    static PassRefPtr<MockLayerTreeHost> create(TestHooks* testHooks, CCLayerTreeHostClient* client, PassRefPtr<LayerChromium> rootLayer, const CCSettings& settings)
    {
        return adoptRef(new MockLayerTreeHost(testHooks, client, rootLayer, settings));
    }

    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl()
    {
        return MockLayerTreeHostImpl::create(m_testHooks, settings());
    }

private:
    MockLayerTreeHost(TestHooks* testHooks, CCLayerTreeHostClient* client, PassRefPtr<LayerChromium> rootLayer, const CCSettings& settings)
        : CCLayerTreeHost(client, rootLayer, settings)
        , m_testHooks(testHooks)
    {
        bool success = initialize();
        ASSERT(success);
        UNUSED_PARAM(success);
    }

    TestHooks* m_testHooks;
};

// Test stub for WebGraphicsContext3D. Returns canned values needed for compositor initialization.
class CompositorMockWebGraphicsContext3D : public MockWebGraphicsContext3D {
public:
    static PassOwnPtr<CompositorMockWebGraphicsContext3D> create()
    {
        return adoptPtr(new CompositorMockWebGraphicsContext3D());
    }

    virtual bool makeContextCurrent() { return true; }
    virtual WebGLId createProgram() { return 1; }
    virtual WebGLId createShader(WGC3Denum) { return 1; }
    virtual void getShaderiv(WebGLId, WGC3Denum, WGC3Dint* value) { *value = 1; }
    virtual void getProgramiv(WebGLId, WGC3Denum, WGC3Dint* value) { *value = 1; }

private:
    CompositorMockWebGraphicsContext3D() { }
};

// Implementation of CCLayerTreeHost callback interface.
class MockLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    static PassOwnPtr<MockLayerTreeHostClient> create(TestHooks* testHooks)
    {
        return adoptPtr(new MockLayerTreeHostClient(testHooks));
    }

    virtual void animateAndLayout(double frameBeginTime)
    {
    }

    virtual void applyScrollDelta(const IntSize& scrollDelta)
    {
        m_testHooks->applyScrollDelta(scrollDelta);
    }

    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D()
    {
        OwnPtr<WebGraphicsContext3D> mock = CompositorMockWebGraphicsContext3D::create();
        GraphicsContext3D::Attributes attrs;
        RefPtr<GraphicsContext3D> context = GraphicsContext3DPrivate::createGraphicsContextFromWebContext(mock.release(), attrs, 0, GraphicsContext3D::RenderDirectlyToHostWindow, GraphicsContext3DPrivate::ForUseOnAnotherThread);
        return context;
    }

    virtual void didRecreateGraphicsContext(bool)
    {
    }

    virtual void scheduleComposite() { }

private:
    explicit MockLayerTreeHostClient(TestHooks* testHooks) : m_testHooks(testHooks) { }

    TestHooks* m_testHooks;
};

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
class CCLayerTreeHostTest : public testing::Test, TestHooks {
public:
    virtual void afterTest() = 0;
    virtual void beginTest() = 0;

    void endTest();

    void postSetNeedsCommitThenRedrawToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchSetNeedsCommitThenRedraw, this);
    }

    void postSetNeedsRedrawToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchSetNeedsRedraw, this);
    }

    void timeout()
    {
        m_timedOut = true;
        endTest();
    }

    void clearTimeout()
    {
        m_timeoutTask = 0;
    }


protected:
    CCLayerTreeHostTest()
        : m_beginning(false)
        , m_endWhenBeginReturns(false)
        , m_timedOut(false)
        , m_mainThreadProxy(CCScopedMainThreadProxy::create())
    {
        m_webThread = adoptPtr(webKitPlatformSupport()->createThread("CCLayerTreeHostTest"));
        WebCompositor::setThread(m_webThread.get());
    }

    void doBeginTest();

    static void onBeginTest(void* self)
    {
        static_cast<CCLayerTreeHostTest*>(self)->doBeginTest();
    }

    static void onEndTest(void* self)
    {
        ASSERT(isMainThread());
        webkit_support::QuitMessageLoop();
        webkit_support::RunAllPendingMessages();
        CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
        ASSERT(test);
        test->m_layerTreeHost.clear();
    }

    static void dispatchSetNeedsCommitThenRedraw(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT(test);
      if (test->m_layerTreeHost)
          test->m_layerTreeHost->setNeedsCommitThenRedraw();
    }

    static void dispatchSetNeedsRedraw(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT(test);
      if (test->m_layerTreeHost)
          test->m_layerTreeHost->setNeedsRedraw();
    }

    class TimeoutTask : public webkit_support::TaskAdaptor {
    public:
        explicit TimeoutTask(CCLayerTreeHostTest* test)
            : m_test(test)
        {
        }

        void clearTest()
        {
            m_test = 0;
        }

        virtual ~TimeoutTask()
        {
            if (m_test)
                m_test->clearTimeout();
        }

        virtual void Run()
        {
            if (m_test)
                m_test->timeout();
        }

    private:
        CCLayerTreeHostTest* m_test;
    };

    virtual void runTest(bool threaded)
    {
        m_settings.enableCompositorThread = threaded;
        webkit_support::PostDelayedTask(CCLayerTreeHostTest::onBeginTest, static_cast<void*>(this), 0);
        m_timeoutTask = new TimeoutTask(this);
        webkit_support::PostDelayedTask(m_timeoutTask, 5000); // webkit_support takes ownership of the task
        webkit_support::RunMessageLoop();
        webkit_support::RunAllPendingMessages();

        if (m_timeoutTask)
            m_timeoutTask->clearTest();

        ASSERT(!m_layerTreeHost.get());
        m_client.clear();
        if (m_timedOut) {
            FAIL() << "Test timed out";
            return;
        }
        afterTest();
    }

    CCSettings m_settings;
    OwnPtr<MockLayerTreeHostClient> m_client;
    RefPtr<CCLayerTreeHost> m_layerTreeHost;

private:
    bool m_beginning;
    bool m_endWhenBeginReturns;
    bool m_timedOut;

    OwnPtr<WebThread> m_webThread;
    RefPtr<CCScopedMainThreadProxy> m_mainThreadProxy;
    TimeoutTask* m_timeoutTask;
};

void CCLayerTreeHostTest::doBeginTest()
{
    ASSERT(isMainThread());
    m_client = MockLayerTreeHostClient::create(this);

    RefPtr<LayerChromium> rootLayer = LayerChromium::create(0);
    m_layerTreeHost = MockLayerTreeHost::create(this, m_client.get(), rootLayer, m_settings);
    ASSERT(m_layerTreeHost);

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
        m_mainThreadProxy->postTask(createMainThreadTask(this, &CCLayerTreeHostTest::endTest));
    else {
        // For the case where we endTest during beginTest(), set a flag to indicate that
        // the test should end the second beginTest regains control.
        if (m_beginning)
            m_endWhenBeginReturns = true;
        else
            onEndTest(static_cast<void*>(this));
    }
}

class CCLayerTreeHostTestThreadOnly : public CCLayerTreeHostTest {
public:
    void runTest()
    {
        CCLayerTreeHostTest::runTest(true);
    }
};

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

#define SINGLE_AND_MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME) \
    TEST_F(TEST_FIXTURE_NAME, runSingleThread)            \
    {                                                     \
        runTest(false);                                   \
    }                                                     \
    TEST_F(TEST_FIXTURE_NAME, runMultiThread)             \
    {                                                     \
        runTest(true);                                    \
    }

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestShortlived1)

// Shortlived layerTreeHosts shouldn't die with a commit in flight.
class CCLayerTreeHostTestShortlived2 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived2() { }

    virtual void beginTest()
    {
        postSetNeedsCommitThenRedrawToMainThread();
        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestShortlived2)

// Shortlived layerTreeHosts shouldn't die with a redraw in flight.
class CCLayerTreeHostTestShortlived3 : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestShortlived3() { }

    virtual void beginTest()
    {
        postSetNeedsRedrawToMainThread();
        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestShortlived3)

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
        postSetNeedsCommitThenRedrawToMainThread();
        endTest();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        m_numCompleteCommits++;
        if (m_numCompleteCommits == 2)
            endTest();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl*)
    {
        if (m_numDraws == 1)
          postSetNeedsCommitThenRedrawToMainThread();
        m_numDraws++;
        postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest()
    {
    }

private:
    int m_numCompleteCommits;
    int m_numDraws;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestCommitingWithContinuousRedraw)

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class CCLayerTreeHostTestSetNeedsCommit1 : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestSetNeedsCommit1()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitThenRedrawToMainThread();
        postSetNeedsCommitThenRedrawToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        m_numDraws++;
        if (!impl->sourceFrameNumber())
            endTest();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
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

TEST_F(CCLayerTreeHostTestSetNeedsCommit1, runMultiThread)
{
    runTest();
}

// A setNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class CCLayerTreeHostTestSetNeedsCommit2 : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestSetNeedsCommit2()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitThenRedrawToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        if (!impl->sourceFrameNumber())
            postSetNeedsCommitThenRedrawToMainThread();
        else if (impl->sourceFrameNumber() == 1)
            endTest();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
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

TEST_F(CCLayerTreeHostTestSetNeedsCommit2, runMultiThread)
{
    runTest();
}

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class CCLayerTreeHostTestSetNeedsRedraw : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestSetNeedsRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitThenRedrawToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        EXPECT_EQ(0, impl->sourceFrameNumber());
        if (!m_numDraws)
            postSetNeedsRedrawToMainThread(); // Redraw again to verify that the second redraw doesn't commit.
        else
            endTest();
        m_numDraws++;
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        EXPECT_EQ(0, m_numDraws);
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

TEST_F(CCLayerTreeHostTestSetNeedsRedraw, runMultiThread)
{
    runTest();
}

class CCLayerTreeHostTestScrollSimple : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestScrollSimple()
        : m_initialScroll(IntPoint(10, 20))
        , m_secondScroll(IntPoint(40, 5))
        , m_scrollAmount(2, -1)
        , m_scrolls(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setMaxScrollPosition(IntSize(100, 100));
        m_layerTreeHost->rootLayer()->setScrollPosition(m_initialScroll);
        postSetNeedsCommitThenRedrawToMainThread();
    }

    virtual void beginCommitOnCCThread(CCLayerTreeHostImpl* impl)
    {
        LayerChromium* root = m_layerTreeHost->rootLayer();
        if (!m_layerTreeHost->frameNumber())
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
        else {
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount);

            // Pretend like Javascript updated the scroll position itself.
            root->setScrollPosition(m_secondScroll);
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CCLayerImpl* root = impl->rootLayer();
        EXPECT_EQ(root->scrollDelta(), IntSize());

        root->scrollBy(m_scrollAmount);

        if (impl->frameNumber() == 1) {
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            postSetNeedsCommitThenRedrawToMainThread();
        } else if (impl->frameNumber() == 2) {
            EXPECT_EQ(root->scrollPosition(), m_secondScroll);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollDelta(const IntSize& scrollDelta)
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_scrolls++;
    }

    virtual void afterTest()
    {
        EXPECT_EQ(1, m_scrolls);
    }
private:
    IntPoint m_initialScroll;
    IntPoint m_secondScroll;
    IntSize m_scrollAmount;
    int m_scrolls;
};

TEST_F(CCLayerTreeHostTestScrollSimple, runMultiThread)
{
    runTest();
}

class CCLayerTreeHostTestScrollMultipleRedraw : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestScrollMultipleRedraw()
        : m_initialScroll(IntPoint(40, 10))
        , m_scrollAmount(-3, 17)
        , m_scrolls(0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setMaxScrollPosition(IntSize(100, 100));
        m_layerTreeHost->rootLayer()->setScrollPosition(m_initialScroll);
        postSetNeedsCommitThenRedrawToMainThread();
    }

    virtual void beginCommitOnCCThread(CCLayerTreeHostImpl* impl)
    {
        LayerChromium* root = m_layerTreeHost->rootLayer();
        if (!m_layerTreeHost->frameNumber())
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
        else if (m_layerTreeHost->frameNumber() == 1)
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
        else if (m_layerTreeHost->frameNumber() == 2)
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CCLayerImpl* root = impl->rootLayer();

        if (impl->frameNumber() == 1) {
            EXPECT_EQ(root->scrollDelta(), IntSize());
            root->scrollBy(m_scrollAmount);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);

            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            postSetNeedsRedrawToMainThread();
        } else if (impl->frameNumber() == 2) {
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            root->scrollBy(m_scrollAmount);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount + m_scrollAmount);

            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            postSetNeedsCommitThenRedrawToMainThread();
        } else if (impl->frameNumber() == 3) {
            EXPECT_EQ(root->scrollDelta(), IntSize());
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollDelta(const IntSize& scrollDelta)
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_scrolls++;
    }

    virtual void afterTest()
    {
        EXPECT_EQ(1, m_scrolls);
    }
private:
    IntPoint m_initialScroll;
    IntSize m_scrollAmount;
    int m_scrolls;
};

TEST_F(CCLayerTreeHostTestScrollMultipleRedraw, runMultiThread)
{
    runTest();
}

} // namespace

