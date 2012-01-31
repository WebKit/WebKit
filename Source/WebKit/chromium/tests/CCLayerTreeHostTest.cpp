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

#include "CompositorFakeWebGraphicsContext3D.h"
#include "ContentLayerChromium.h"
#include "GraphicsContext3DPrivate.h"
#include "LayerChromium.h"
#include "Region.h"
#include "TextureManager.h"
#include "WebCompositor.h"
#include "WebKit.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCScopedThreadProxy.h"
#include "cc/CCTextureUpdater.h"
#include "cc/CCThreadTask.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebThread.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
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
    virtual void applyScrollAndScale(const IntSize&, float) { }
    virtual void updateAnimations(double frameBeginTime) { }
    virtual void layout() { }
};

// Adapts CCLayerTreeHostImpl for test. Runs real code, then invokes test hooks.
class MockLayerTreeHostImpl : public CCLayerTreeHostImpl {
public:
    static PassOwnPtr<MockLayerTreeHostImpl> create(TestHooks* testHooks, const CCSettings& settings, CCLayerTreeHostImplClient* client)
    {
        return adoptPtr(new MockLayerTreeHostImpl(testHooks, settings, client));
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
    MockLayerTreeHostImpl(TestHooks* testHooks, const CCSettings& settings, CCLayerTreeHostImplClient* client)
        : CCLayerTreeHostImpl(settings, client)
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
        RefPtr<MockLayerTreeHost> layerTreeHost = adoptRef(new MockLayerTreeHost(testHooks, client, settings));
        bool success = layerTreeHost->initialize();
        EXPECT_TRUE(success);
        layerTreeHost->setRootLayer(rootLayer);

        // LayerTreeHostImpl won't draw if it has 1x1 viewport.
        layerTreeHost->setViewportSize(IntSize(1, 1));

        return layerTreeHost.release();
    }

    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient* client)
    {
        return MockLayerTreeHostImpl::create(m_testHooks, settings(), client);
    }

private:
    MockLayerTreeHost(TestHooks* testHooks, CCLayerTreeHostClient* client, const CCSettings& settings)
        : CCLayerTreeHost(client, settings)
        , m_testHooks(testHooks)
    {
    }

    TestHooks* m_testHooks;
};

class CompositorFakeWebGraphicsContext3DWithTextureTracking : public CompositorFakeWebGraphicsContext3D {
public:
    static PassOwnPtr<CompositorFakeWebGraphicsContext3DWithTextureTracking> create(Attributes attrs)
    {
        return adoptPtr(new CompositorFakeWebGraphicsContext3DWithTextureTracking(attrs));
    }

    virtual WebGLId createTexture()
    {
        WebGLId texture = m_textures.size() + 1;
        m_textures.append(texture);
        return texture;
    }

    virtual void deleteTexture(WebGLId texture)
    {
        for (size_t i = 0; i < m_textures.size(); i++) {
            if (m_textures[i] == texture) {
                m_textures.remove(i);
                break;
            }
        }
    }

    virtual void bindTexture(WGC3Denum /* target */, WebGLId texture)
    {
        m_usedTextures.add(texture);
    }

    int numTextures() const { return static_cast<int>(m_textures.size()); }
    int texture(int i) const { return m_textures[i]; }
    void resetTextures() { m_textures.clear(); }

    int numUsedTextures() const { return static_cast<int>(m_usedTextures.size()); }
    bool usedTexture(int texture) const { return m_usedTextures.find(texture) != m_usedTextures.end(); }
    void resetUsedTextures() { m_usedTextures.clear(); }

private:
    explicit CompositorFakeWebGraphicsContext3DWithTextureTracking(Attributes attrs) : CompositorFakeWebGraphicsContext3D(attrs)
    {
    }

    Vector<WebGLId> m_textures;
    HashSet<WebGLId> m_usedTextures;
};

// Implementation of CCLayerTreeHost callback interface.
class MockLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    static PassOwnPtr<MockLayerTreeHostClient> create(TestHooks* testHooks)
    {
        return adoptPtr(new MockLayerTreeHostClient(testHooks));
    }

    virtual void updateAnimations(double frameBeginTime)
    {
        m_testHooks->updateAnimations(frameBeginTime);
    }

    virtual void layout()
    {
        m_testHooks->layout();
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
    {
        m_testHooks->applyScrollAndScale(scrollDelta, scale);
    }

    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D()
    {
        GraphicsContext3D::Attributes attrs;
        WebGraphicsContext3D::Attributes webAttrs;
        webAttrs.alpha = attrs.alpha;

        OwnPtr<WebGraphicsContext3D> webContext = CompositorFakeWebGraphicsContext3DWithTextureTracking::create(webAttrs);
        return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(
            webContext.release(), attrs, 0,
            GraphicsContext3D::RenderDirectlyToHostWindow,
            GraphicsContext3DPrivate::ForUseOnAnotherThread);
    }

    virtual void didCommitAndDrawFrame()
    {
    }

    virtual void didCompleteSwapBuffers()
    {
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

    void postSetNeedsAnimateToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchSetNeedsAnimate, this);
    }

    void postSetNeedsCommitToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchSetNeedsCommit, this);
    }

    void postSetNeedsRedrawToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchSetNeedsRedraw, this);
    }

    void postSetNeedsAnimateAndCommitToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchSetNeedsAnimateAndCommit, this);
    }


    void postSetVisibleToMainThread(bool visible)
    {
        callOnMainThread(visible ? CCLayerTreeHostTest::dispatchSetVisible : CCLayerTreeHostTest::dispatchSetInvisible, this);
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

    CCLayerTreeHost* layerTreeHost() { return m_layerTreeHost.get(); }


protected:
    CCLayerTreeHostTest()
        : m_beginning(false)
        , m_endWhenBeginReturns(false)
        , m_timedOut(false) { }

    void doBeginTest();

    static void onEndTest(void* self)
    {
        ASSERT(isMainThread());
        webKitPlatformSupport()->currentThread()->exitRunLoop();
    }

    static void dispatchSetNeedsAnimate(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT(test);
      if (test->m_layerTreeHost)
          test->m_layerTreeHost->setNeedsAnimate();
    }

    static void dispatchSetNeedsAnimateAndCommit(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT(test);
      if (test->m_layerTreeHost) {
          test->m_layerTreeHost->setNeedsAnimate();
          test->m_layerTreeHost->setNeedsCommit();
      }
    }

    static void dispatchSetNeedsCommit(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT_TRUE(test);
      if (test->m_layerTreeHost)
          test->m_layerTreeHost->setNeedsCommit();
    }

    static void dispatchSetNeedsRedraw(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT_TRUE(test);
      if (test->m_layerTreeHost)
          test->m_layerTreeHost->setNeedsRedraw();
    }

    static void dispatchSetVisible(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT(test);
      if (test->m_layerTreeHost)
          test->m_layerTreeHost->setVisible(true);
    }

    static void dispatchSetInvisible(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT(test);
      if (test->m_layerTreeHost)
          test->m_layerTreeHost->setVisible(false);
    }

    class TimeoutTask : public WebThread::Task {
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

        virtual void run()
        {
            if (m_test)
                m_test->timeout();
        }

    private:
        CCLayerTreeHostTest* m_test;
    };

    class BeginTask : public WebThread::Task {
    public:
        explicit BeginTask(CCLayerTreeHostTest* test)
            : m_test(test)
        {
        }

        virtual ~BeginTask() { }
        virtual void run()
        {
            m_test->doBeginTest();
        }
    private:
        CCLayerTreeHostTest* m_test;
    };

    virtual void runTest(bool threaded)
    {
        if (threaded) {
            m_webThread = adoptPtr(webKitPlatformSupport()->createThread("CCLayerTreeHostTest"));
            WebCompositor::initialize(m_webThread.get());
        } else
            WebCompositor::initialize(0);

        ASSERT(CCProxy::isMainThread());
        m_mainThreadProxy = CCScopedThreadProxy::create(CCProxy::mainThread());

        m_beginTask = new BeginTask(this);
        webKitPlatformSupport()->currentThread()->postDelayedTask(m_beginTask, 0); // postDelayedTask takes ownership of the task
        m_timeoutTask = new TimeoutTask(this);
        webKitPlatformSupport()->currentThread()->postDelayedTask(m_timeoutTask, 5000);
        webKitPlatformSupport()->currentThread()->enterRunLoop();

        if (m_layerTreeHost && m_layerTreeHost->rootLayer())
            m_layerTreeHost->rootLayer()->setLayerTreeHost(0);
        m_layerTreeHost.clear();

        if (m_timeoutTask)
            m_timeoutTask->clearTest();

        ASSERT_FALSE(m_layerTreeHost.get());
        m_client.clear();
        if (m_timedOut) {
            FAIL() << "Test timed out";
            WebCompositor::shutdown();
            return;
        }
        afterTest();
        WebCompositor::shutdown();
    }

    CCSettings m_settings;
    OwnPtr<MockLayerTreeHostClient> m_client;
    RefPtr<CCLayerTreeHost> m_layerTreeHost;

private:
    bool m_beginning;
    bool m_endWhenBeginReturns;
    bool m_timedOut;

    OwnPtr<WebThread> m_webThread;
    RefPtr<CCScopedThreadProxy> m_mainThreadProxy;
    TimeoutTask* m_timeoutTask;
    BeginTask* m_beginTask;
};

void CCLayerTreeHostTest::doBeginTest()
{
    ASSERT(isMainThread());
    m_client = MockLayerTreeHostClient::create(this);

    RefPtr<LayerChromium> rootLayer = LayerChromium::create();
    m_layerTreeHost = MockLayerTreeHost::create(this, m_client.get(), rootLayer, m_settings);
    ASSERT_TRUE(m_layerTreeHost);
    rootLayer->setLayerTreeHost(m_layerTreeHost.get());

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
        m_mainThreadProxy->postTask(createCCThreadTask(this, &CCLayerTreeHostTest::endTest));
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
    void runTestThreaded()
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
        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

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
        postSetNeedsCommitToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

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

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestShortlived3)

// Test interleaving of redraws and commits
class CCLayerTreeHostTestCommitingWithContinuousRedraw : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestCommitingWithContinuousRedraw()
        : m_numCompleteCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
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
          postSetNeedsCommitToMainThread();
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

TEST_F(CCLayerTreeHostTestCommitingWithContinuousRedraw, runMultiThread)
{
    runTestThreaded();
}

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
        postSetNeedsCommitToMainThread();
        postSetNeedsCommitToMainThread();
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

TEST_F(CCLayerTreeHostTestSetNeedsCommit1, DISABLED_runMultiThread)
{
    runTestThreaded();
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
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        if (!impl->sourceFrameNumber())
            postSetNeedsCommitToMainThread();
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
    runTestThreaded();
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
        postSetNeedsCommitToMainThread();
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
    runTestThreaded();
}

// Trigger a frame with setNeedsCommit. Then, inside the resulting animate
// callback, requet another frame using setNeedsAnimate. End the test when
// animate gets called yet-again, indicating that the proxy is correctly
// handling the case where setNeedsAnimate() is called inside the begin frame
// flow.
class CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback()
        : m_numAnimates(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsAnimateToMainThread();
    }

    virtual void updateAnimations(double)
    {
        if (!m_numAnimates) {
            m_layerTreeHost->setNeedsAnimate();
            m_numAnimates++;
            return;
        }
        endTest();
    }

    virtual void afterTest()
    {
    }

private:
    int m_numAnimates;
};

TEST_F(CCLayerTreeHostTestSetNeedsAnimateInsideAnimationCallback, runMultiThread)
{
    runTestThreaded();
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
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(m_initialScroll);
        postSetNeedsCommitToMainThread();
    }

    virtual void layout()
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

        root->setScrollable(true);
        root->setMaxScrollPosition(IntSize(100, 100));
        root->scrollBy(m_scrollAmount);

        if (impl->frameNumber() == 1) {
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            postSetNeedsCommitToMainThread();
        } else if (impl->frameNumber() == 2) {
            EXPECT_EQ(root->scrollPosition(), m_secondScroll);
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
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

TEST_F(CCLayerTreeHostTestScrollSimple, DISABLED_runMultiThread)
{
    runTestThreaded();
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
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(m_initialScroll);
        postSetNeedsCommitToMainThread();
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
        root->setScrollable(true);
        root->setMaxScrollPosition(IntSize(100, 100));

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
            postSetNeedsCommitToMainThread();
        } else if (impl->frameNumber() == 3) {
            EXPECT_EQ(root->scrollDelta(), IntSize());
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
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

TEST_F(CCLayerTreeHostTestScrollMultipleRedraw, DISABLED_runMultiThread)
{
    runTestThreaded();
}

class CCLayerTreeHostTestSetVisible : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestSetVisible()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        postSetVisibleToMainThread(false);
        postSetNeedsRedrawToMainThread(); // This is suppressed while we're invisible.
        postSetVisibleToMainThread(true); // Triggers the redraw.
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        EXPECT_TRUE(impl->visible());
        ++m_numDraws;
        endTest();
    }

    virtual void afterTest()
    {
        EXPECT_EQ(1, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestSetVisible, runMultiThread)
{
    runTest(true);
}

class TestOpacityChangeLayerDelegate : public ContentLayerDelegate {
public:
    TestOpacityChangeLayerDelegate(CCLayerTreeHostTest* test)
        : m_test(test)
    {
    }

    virtual void paintContents(GraphicsContext&, const IntRect&)
    {
        // Set layer opacity to 0.
        m_test->layerTreeHost()->rootLayer()->setOpacity(0);
    }

    virtual bool preserves3D() { return false; }

private:
    CCLayerTreeHostTest* m_test;
};

class ContentLayerChromiumWithUpdateTracking : public ContentLayerChromium {
public:
    static PassRefPtr<ContentLayerChromiumWithUpdateTracking> create(ContentLayerDelegate *delegate) { return adoptRef(new ContentLayerChromiumWithUpdateTracking(delegate)); }

    int paintContentsCount() { return m_paintContentsCount; }
    int idlePaintContentsCount() { return m_idlePaintContentsCount; }
    void resetPaintContentsCount() { m_paintContentsCount = 0; m_idlePaintContentsCount = 0;}

    int updateCount() { return m_updateCount; }
    void resetUpdateCount() { m_updateCount = 0; }

    virtual void paintContentsIfDirty(const Region& occludedScreenSpace)
    {
        ContentLayerChromium::paintContentsIfDirty(occludedScreenSpace);
        m_paintContentsCount++;
    }

    virtual void idlePaintContentsIfDirty()
    {
        ContentLayerChromium::idlePaintContentsIfDirty();
        m_idlePaintContentsCount++;
    }

    virtual void updateCompositorResources(GraphicsContext3D* context, CCTextureUpdater& updater)
    {
        ContentLayerChromium::updateCompositorResources(context, updater);
        m_updateCount++;
    }

private:
    explicit ContentLayerChromiumWithUpdateTracking(ContentLayerDelegate* delegate)
        : ContentLayerChromium(delegate)
        , m_paintContentsCount(0)
        , m_idlePaintContentsCount(0)
        , m_updateCount(0)
    {
        setBounds(IntSize(10, 10));
        setIsDrawable(true);
    }

    int m_paintContentsCount;
    int m_idlePaintContentsCount;
    int m_updateCount;
};

// Layer opacity change during paint should not prevent compositor resources from being updated during commit.
class CCLayerTreeHostTestOpacityChange : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestOpacityChange()
        : m_testOpacityChangeDelegate(this)
        , m_updateCheckLayer(ContentLayerChromiumWithUpdateTracking::create(&m_testOpacityChangeDelegate))
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_updateCheckLayer);
        m_layerTreeHost->setViewportSize(IntSize(10, 10));

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        endTest();
    }

    virtual void afterTest()
    {
        // paintContentsIfDirty() should have been called once.
        EXPECT_EQ(1, m_updateCheckLayer->paintContentsCount());

        // idlePaintContentsIfDirty() should have been called once
        EXPECT_EQ(1, m_updateCheckLayer->idlePaintContentsCount());

        // updateCompositorResources() should have been called the same
        // amout of times as paintContentsIfDirty().
        EXPECT_EQ(m_updateCheckLayer->paintContentsCount(),
                  m_updateCheckLayer->updateCount());

        // clear m_updateCheckLayer so CCLayerTreeHost dies.
        m_updateCheckLayer.clear();
    }

private:
    TestOpacityChangeLayerDelegate m_testOpacityChangeDelegate;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_updateCheckLayer;
};

TEST_F(CCLayerTreeHostTestOpacityChange, runMultiThread)
{
    runTest(true);
}

class CCLayerTreeHostTestSetViewportSize : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestSetViewportSize()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        IntSize viewportSize(10, 10);
        layerTreeHost()->setViewportSize(viewportSize);
        EXPECT_EQ(viewportSize, layerTreeHost()->viewportSize());
        EXPECT_EQ(TextureManager::highLimitBytes(viewportSize), layerTreeHost()->contentsTextureManager()->maxMemoryLimitBytes());
        EXPECT_EQ(TextureManager::reclaimLimitBytes(viewportSize), layerTreeHost()->contentsTextureManager()->preferredMemoryLimitBytes());

        // setViewportSize() should not call TextureManager::setMaxMemoryLimitBytes() or TextureManager::setPreferredMemoryLimitBytes()
        // if the viewport size is not changed.
        IntSize fakeSize(5, 5);
        layerTreeHost()->contentsTextureManager()->setMaxMemoryLimitBytes(TextureManager::highLimitBytes(fakeSize));
        layerTreeHost()->contentsTextureManager()->setPreferredMemoryLimitBytes(TextureManager::reclaimLimitBytes(fakeSize));
        layerTreeHost()->setViewportSize(viewportSize);
        EXPECT_EQ(TextureManager::highLimitBytes(fakeSize), layerTreeHost()->contentsTextureManager()->maxMemoryLimitBytes());
        EXPECT_EQ(TextureManager::reclaimLimitBytes(fakeSize), layerTreeHost()->contentsTextureManager()->preferredMemoryLimitBytes());

        endTest();
    }

    virtual void afterTest()
    {
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestSetViewportSize, runSingleThread)
{
    runTest(false);
}

class MockContentLayerDelegate : public ContentLayerDelegate {
public:
    bool drawsContent() const { return true; }
    MOCK_CONST_METHOD0(preserves3D, bool());
    void paintContents(GraphicsContext&, const IntRect&) { }
    void notifySyncRequired() { }
};

// Verify atomicity of commits and reuse of textures.
class CCLayerTreeHostTestAtomicCommit : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAtomicCommit()
        : m_updateCheckLayer(ContentLayerChromiumWithUpdateTracking::create(&m_delegate))
        , m_numCommits(0)
    {
        // Make sure partial texture updates are turned off.
        m_settings.partialTextureUpdates = false;
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_updateCheckLayer);
        m_layerTreeHost->setViewportSize(IntSize(10, 10));

        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(impl->context()));

        switch (impl->frameNumber()) {
        case 0:
            // Number of textures should be one.
            EXPECT_EQ(1, context->numTextures());
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->numUsedTextures());
            // Verify used texture is correct.
            EXPECT_TRUE(context->usedTexture(context->texture(0)));

            context->resetUsedTextures();
            break;
        case 1:
            // Number of textures should be two as the first texture
            // is used by impl thread and cannot by used for update.
            EXPECT_EQ(2, context->numTextures());
            // Number of textures used for commit should still be one.
            EXPECT_EQ(1, context->numUsedTextures());
            // First texture should not have been used.
            EXPECT_FALSE(context->usedTexture(context->texture(0)));
            // New texture should have been used.
            EXPECT_TRUE(context->usedTexture(context->texture(1)));

            context->resetUsedTextures();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(impl->context()));

        // Number of textures used for draw should always be one.
        EXPECT_EQ(1, context->numUsedTextures());

        if (impl->frameNumber() < 2) {
            context->resetUsedTextures();
            postSetNeedsAnimateAndCommitToMainThread();
            postSetNeedsRedrawToMainThread();
        } else
            endTest();
    }

    virtual void layout()
    {
        m_updateCheckLayer->setNeedsDisplay();
    }

    virtual void afterTest()
    {
    }

private:
    MockContentLayerDelegate m_delegate;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_updateCheckLayer;
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestAtomicCommit, runMultiThread)
{
    runTest(true);
}

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

class TestLayerChromium : public LayerChromium {
public:
    static PassRefPtr<TestLayerChromium> create() { return adoptRef(new TestLayerChromium()); }

    virtual void paintContentsIfDirty(const Region& occludedScreenSpace)
    {
        m_occludedScreenSpace = occludedScreenSpace;
    }

    virtual bool drawsContent() const { return true; }

    const Region& occludedScreenSpace() const { return m_occludedScreenSpace; }
    void clearOccludedScreenSpace() { m_occludedScreenSpace = Region(); }

private:
    TestLayerChromium() : LayerChromium() { }

    Region m_occludedScreenSpace;
};

static void setLayerPropertiesForTesting(TestLayerChromium* layer, LayerChromium* parent, const TransformationMatrix& transform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool opaque)
{
    layer->removeAllChildren();
    if (parent)
        parent->addChild(layer);
    layer->setTransform(transform);
    layer->setAnchorPoint(anchor);
    layer->setPosition(position);
    layer->setBounds(bounds);
    layer->setOpaque(opaque);
    layer->clearOccludedScreenSpace();
}

class CCLayerTreeHostTestLayerOcclusion : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLayerOcclusion() { }

    virtual void beginTest()
    {
        RefPtr<TestLayerChromium> rootLayer = TestLayerChromium::create();
        RefPtr<TestLayerChromium> child = TestLayerChromium::create();
        RefPtr<TestLayerChromium> child2 = TestLayerChromium::create();
        RefPtr<TestLayerChromium> grandChild = TestLayerChromium::create();
        RefPtr<TestLayerChromium> mask = TestLayerChromium::create();

        TransformationMatrix identityMatrix;
        TransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        child->setMasksToBounds(true);

        // See CCLayerTreeHostCommonTest.layerAddsSelfToOccludedRegionWithRotatedSurface for a nice visual of these layers and how they end up
        // positioned on the screen.

        // The child layer is rotated and the grandChild is opaque, but clipped to the child and rootLayer
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers();
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer is opaque, then it adds to the occlusion seen by the rootLayer.
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers();
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Add a second child to the root layer and the regions should merge
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(70, 20), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers();
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 20, 70, 80), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, rootLayer->occludedScreenSpace().rects().size());

        // Move the second child to be sure.
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers();
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 30, 90, 70), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a mask on it, then it shouldn't contribute to occlusion on stuff below it
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        child->setMaskLayer(mask.get());

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers();
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 90, 30), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer with a mask is below child2, then child2 should contribute to occlusion on everything, and child shouldn't contribute to the rootLayer
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        child->setMaskLayer(mask.get());

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers();
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 90, 30), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 40, 90, 60), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 90, 30), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a non-opaque drawOpacity, then it shouldn't contribute to occlusion on stuff below it
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        child->setMaskLayer(0);
        child->setDrawOpacity(0.5);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers();
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 90, 30), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer with non-opaque drawOpacity is below child2, then child2 should contribute to occlusion on everything, and child shouldn't contribute to the rootLayer
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        child->setMaskLayer(0);
        child->setDrawOpacity(0.5);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers();
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 90, 30), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 40, 90, 60), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 90, 30), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLayerOcclusion)

} // namespace
