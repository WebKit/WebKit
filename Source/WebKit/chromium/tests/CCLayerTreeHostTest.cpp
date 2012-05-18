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

#include "CCAnimationTestCommon.h"
#include "CCOcclusionTrackerTestCommon.h"
#include "CCTiledLayerTestCommon.h"
#include "CompositorFakeWebGraphicsContext3D.h"
#include "ContentLayerChromium.h"
#include "FilterOperations.h"
#include "GraphicsContext3DPrivate.h"
#include "LayerChromium.h"
#include "TextureManager.h"
#include "WebCompositor.h"
#include "WebKit.h"
#include "cc/CCActiveAnimation.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCLayerAnimationDelegate.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCScopedThreadProxy.h"
#include "cc/CCTextureUpdater.h"
#include "cc/CCThreadTask.h"
#include "platform/WebThread.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/Platform.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace WebKit;
using namespace WebKitTests;
using namespace WTF;

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

namespace {

// Used by test stubs to notify the test when something interesting happens.
class TestHooks : public CCLayerAnimationDelegate {
public:
    virtual void beginCommitOnCCThread(CCLayerTreeHostImpl*) { }
    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*) { }
    virtual void prepareToDrawOnCCThread(CCLayerTreeHostImpl*) { }
    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl*) { }
    virtual void animateLayers(CCLayerTreeHostImpl*, double monotonicTime) { }
    virtual void willAnimateLayers(CCLayerTreeHostImpl*, double monotonicTime) { }
    virtual void applyScrollAndScale(const IntSize&, float) { }
    virtual void updateAnimations(double monotonicTime) { }
    virtual void layout() { }
    virtual void didRecreateContext(bool succeded) { }
    virtual void didCommitAndDrawFrame() { }

    // Implementation of CCLayerAnimationDelegate
    virtual void notifyAnimationStarted(double time) { }
    virtual void notifyAnimationFinished(double time) { }
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

    virtual bool prepareToDraw(FrameData& frame)
    {
        bool result = CCLayerTreeHostImpl::prepareToDraw(frame);
        m_testHooks->prepareToDrawOnCCThread(this);
        return result;
    }

    virtual void drawLayers(const FrameData& frame)
    {
        CCLayerTreeHostImpl::drawLayers(frame);
        m_testHooks->drawLayersOnCCThread(this);
    }

    // Make these public.
    typedef Vector<CCLayerImpl*> CCLayerList;
    using CCLayerTreeHostImpl::calculateRenderSurfaceLayerList;

protected:
    virtual void animateLayers(double monotonicTime, double wallClockTime)
    {
        m_testHooks->willAnimateLayers(this, monotonicTime);
        CCLayerTreeHostImpl::animateLayers(monotonicTime, wallClockTime);
        m_testHooks->animateLayers(this, monotonicTime);
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
    static PassOwnPtr<MockLayerTreeHost> create(TestHooks* testHooks, CCLayerTreeHostClient* client, PassRefPtr<LayerChromium> rootLayer, const CCSettings& settings)
    {
        // For these tests, we will enable threaded animations.
        CCSettings settingsCopy = settings;
        settingsCopy.threadedAnimationEnabled = true;

        OwnPtr<MockLayerTreeHost> layerTreeHost(adoptPtr(new MockLayerTreeHost(testHooks, client, settingsCopy)));
        bool success = layerTreeHost->initialize();
        EXPECT_TRUE(success);
        layerTreeHost->setRootLayer(rootLayer);

        // LayerTreeHostImpl won't draw if it has 1x1 viewport.
        layerTreeHost->setViewportSize(IntSize(1, 1));

        layerTreeHost->rootLayer()->setLayerAnimationDelegate(testHooks);

        return layerTreeHost.release();
    }

    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl(CCLayerTreeHostImplClient* client)
    {
        // For these tests, we will enable threaded animations.
        CCSettings copySettings = settings();
        copySettings.threadedAnimationEnabled = true;
        return MockLayerTreeHostImpl::create(m_testHooks, copySettings, client);
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
    HashSet<WebGLId, DefaultHash<WebGLId>::Hash, UnsignedWithZeroKeyHashTraits<WebGLId> > m_usedTextures;
};

// Implementation of CCLayerTreeHost callback interface.
class MockLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    static PassOwnPtr<MockLayerTreeHostClient> create(TestHooks* testHooks)
    {
        return adoptPtr(new MockLayerTreeHostClient(testHooks));
    }

    virtual void willBeginFrame() OVERRIDE
    {
    }

    virtual void updateAnimations(double monotonicTime) OVERRIDE
    {
        m_testHooks->updateAnimations(monotonicTime);
    }

    virtual void layout() OVERRIDE
    {
        m_testHooks->layout();
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale) OVERRIDE
    {
        m_testHooks->applyScrollAndScale(scrollDelta, scale);
    }

    virtual PassRefPtr<GraphicsContext3D> createContext() OVERRIDE
    {
        GraphicsContext3D::Attributes attrs;
        WebGraphicsContext3D::Attributes webAttrs;
        webAttrs.alpha = attrs.alpha;

        OwnPtr<WebGraphicsContext3D> webContext = CompositorFakeWebGraphicsContext3DWithTextureTracking::create(webAttrs);
        return GraphicsContext3DPrivate::createGraphicsContextFromWebContext(webContext.release(), GraphicsContext3D::RenderDirectlyToHostWindow);
    }

    virtual void didCommit() OVERRIDE
    {
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_testHooks->didCommitAndDrawFrame();
    }

    virtual void didCompleteSwapBuffers() OVERRIDE
    {
    }

    virtual void didRecreateContext(bool succeeded) OVERRIDE
    {
        m_testHooks->didRecreateContext(succeeded);
    }

    virtual void scheduleComposite() OVERRIDE
    {
    }

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

    void postAddAnimationToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchAddAnimation, this);
    }

    void postAddInstantAnimationToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchAddInstantAnimation, this);
    }

    void postSetNeedsCommitToMainThread()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchSetNeedsCommit, this);
    }

    void AcquireLayerTextures()
    {
        callOnMainThread(CCLayerTreeHostTest::dispatchAcquireLayerTextures, this);
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
        WebKit::Platform::current()->currentThread()->exitRunLoop();
    }

    static void dispatchSetNeedsAnimate(void* self)
    {
        ASSERT(isMainThread());
        CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
        ASSERT(test);
        if (test->m_layerTreeHost)
            test->m_layerTreeHost->setNeedsAnimate();
    }

    static void dispatchAddInstantAnimation(void* self)
    {
        ASSERT(isMainThread());
        CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
        ASSERT(test);
        if (test->m_layerTreeHost && test->m_layerTreeHost->rootLayer())
            addOpacityTransitionToLayer(*test->m_layerTreeHost->rootLayer(), 0, 0, 0.5, false);
    }

    static void dispatchAddAnimation(void* self)
    {
        ASSERT(isMainThread());
        CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
        ASSERT(test);
        if (test->m_layerTreeHost && test->m_layerTreeHost->rootLayer())
            addOpacityTransitionToLayer(*test->m_layerTreeHost->rootLayer(), 10, 0, 0.5, true);
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

    static void dispatchAcquireLayerTextures(void* self)
    {
      ASSERT(isMainThread());
      CCLayerTreeHostTest* test = static_cast<CCLayerTreeHostTest*>(self);
      ASSERT_TRUE(test);
      if (test->m_layerTreeHost)
          test->m_layerTreeHost->acquireLayerTextures();
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
            m_webThread = adoptPtr(WebKit::Platform::current()->createThread("CCLayerTreeHostTest"));
            WebCompositor::initialize(m_webThread.get());
        } else
            WebCompositor::initialize(0);

        ASSERT(CCProxy::isMainThread());
        m_mainThreadProxy = CCScopedThreadProxy::create(CCProxy::mainThread());

        m_beginTask = new BeginTask(this);
        WebKit::Platform::current()->currentThread()->postDelayedTask(m_beginTask, 0); // postDelayedTask takes ownership of the task
        m_timeoutTask = new TimeoutTask(this);
        WebKit::Platform::current()->currentThread()->postDelayedTask(m_timeoutTask, 5000);
        WebKit::Platform::current()->currentThread()->enterRunLoop();

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
    OwnPtr<CCLayerTreeHost> m_layerTreeHost;

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
    m_layerTreeHost->setSurfaceReady();

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

#if OS(WINDOWS)
// http://webkit.org/b/74623
TEST_F(CCLayerTreeHostTestSetNeedsCommit2, FLAKY_runMultiThread)
#else
TEST_F(CCLayerTreeHostTestSetNeedsCommit2, runMultiThread)
#endif
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

// If the layerTreeHost says it can't draw, then we should not try to draw.
// FIXME: Make this run in single threaded mode too. http://crbug.com/127481
class CCLayerTreeHostTestCanDrawBlocksDrawing : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestCanDrawBlocksDrawing()
        : m_numCommits(0)
    {
    }

    virtual void beginTest()
    {
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        // Only the initial draw should bring us here.
        EXPECT_TRUE(impl->canDraw());
        EXPECT_EQ(0, impl->sourceFrameNumber());
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        if (m_numCommits >= 1) {
            // After the first commit, we should not be able to draw.
            EXPECT_FALSE(impl->canDraw());
        }
    }

    virtual void didCommitAndDrawFrame()
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            // Make the viewport empty so the host says it can't draw.
            m_layerTreeHost->setViewportSize(IntSize(0, 0));

            OwnArrayPtr<char> pixels(adoptArrayPtr(new char[4]));
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(pixels.get()), IntRect(0, 0, 1, 1));
        } else if (m_numCommits == 2) {
            m_layerTreeHost->setNeedsCommit();
            m_layerTreeHost->finishAllRendering();
            endTest();
        }
    }

    virtual void afterTest()
    {
    }

private:
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestCanDrawBlocksDrawing, runMultiThread)
{
    runTestThreaded();
}

// beginLayerWrite should prevent draws from executing until a commit occurs
class CCLayerTreeHostTestWriteLayersRedraw : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestWriteLayersRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest()
    {
        AcquireLayerTextures();
        postSetNeedsRedrawToMainThread(); // should be inhibited without blocking
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        EXPECT_EQ(1, impl->sourceFrameNumber());
        m_numDraws++;
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        m_numCommits++;
        endTest();
    }

    virtual void afterTest()
    {
        EXPECT_EQ(0, m_numDraws);
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(CCLayerTreeHostTestWriteLayersRedraw, runMultiThread)
{
    runTestThreaded();
}

// Verify that when resuming visibility, requesting layer write permission
// will not deadlock the main thread even though there are not yet any
// scheduled redraws. This behavior is critical for reliably surviving tab
// switching. There are no failure conditions to this test, it just passes
// by not timing out.
class CCLayerTreeHostTestWriteLayersAfterVisible : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestWriteLayersAfterVisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        m_numCommits++;
        if (m_numCommits == 2)
            endTest();
        else {
            postSetVisibleToMainThread(false);
            postSetVisibleToMainThread(true);
            AcquireLayerTextures();
            postSetNeedsCommitToMainThread();
        }
    }

    virtual void afterTest()
    {
    }

private:
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestWriteLayersAfterVisible, runMultiThread)
{
    runTestThreaded();
}

// A compositeAndReadback while invisible should force a normal commit without assertion.
class CCLayerTreeHostTestCompositeAndReadbackWhileInvisible : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestCompositeAndReadbackWhileInvisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest()
    {
    }

    virtual void didCommitAndDrawFrame()
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            m_layerTreeHost->setVisible(false);
            m_layerTreeHost->setNeedsCommit();
            m_layerTreeHost->setNeedsCommit();
            OwnArrayPtr<char> pixels(adoptArrayPtr(new char[4]));
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(pixels.get()), IntRect(0, 0, 1, 1));
        } else
            endTest();

    }

    virtual void afterTest()
    {
    }

private:
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestCompositeAndReadbackWhileInvisible, runMultiThread)
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

// Add a layer animation and confirm that CCLayerTreeHostImpl::animateLayers does get
// called and continues to get called.
class CCLayerTreeHostTestAddAnimation : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestAddAnimation()
        : m_numAnimates(0)
        , m_receivedAnimationStartedNotification(false)
        , m_startTime(0)
        , m_firstMonotonicTime(0)
    {
    }

    virtual void beginTest()
    {
        postAddInstantAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        if (!m_numAnimates) {
            // The animation had zero duration so layerTreeHostImpl should no
            // longer need to animate its layers.
            EXPECT_FALSE(layerTreeHostImpl->needsAnimateLayers());
            m_numAnimates++;
            m_firstMonotonicTime = monotonicTime;
            return;
        }
        EXPECT_LT(0, m_startTime);
        EXPECT_LT(0, m_firstMonotonicTime);
        EXPECT_NE(m_startTime, m_firstMonotonicTime);
        EXPECT_TRUE(m_receivedAnimationStartedNotification);
        endTest();
    }

    virtual void notifyAnimationStarted(double wallClockTime)
    {
        m_receivedAnimationStartedNotification = true;
        m_startTime = wallClockTime;
    }

    virtual void afterTest()
    {
    }

private:
    int m_numAnimates;
    bool m_receivedAnimationStartedNotification;
    double m_startTime;
    double m_firstMonotonicTime;
};

TEST_F(CCLayerTreeHostTestAddAnimation, runMultiThread)
{
    runTestThreaded();
}

// Ensures that animations continue to be ticked when we are backgrounded.
class CCLayerTreeHostTestTickAnimationWhileBackgrounded : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestTickAnimationWhileBackgrounded()
        : m_numAnimates(0)
    {
    }

    virtual void beginTest()
    {
        postAddAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        if (m_numAnimates < 2) {
            if (!m_numAnimates) {
                // We have a long animation running. It should continue to tick even if we are not visible.
                postSetVisibleToMainThread(false);
            }
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

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestTickAnimationWhileBackgrounded)

// Ensures that animations continue to be ticked when we are backgrounded.
class CCLayerTreeHostTestAddAnimationWithTimingFunction : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestAddAnimationWithTimingFunction()
    {
    }

    virtual void beginTest()
    {
        postAddAnimationToMainThread();
    }

    virtual void animateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        const CCFloatAnimationCurve* curve = m_layerTreeHost->rootLayer()->layerAnimationController()->getActiveAnimation(0, CCActiveAnimation::Opacity)->curve()->toFloatAnimationCurve();
        float startOpacity = curve->getValue(0);
        float endOpacity = curve->getValue(curve->duration());
        float linearlyInterpolatedOpacity = 0.25 * endOpacity + 0.75 * startOpacity;
        double time = curve->duration() * 0.25;
        // If the linear timing function associated with this animation was not picked up,
        // then the linearly interpolated opacity would be different because of the
        // default ease timing function.
        EXPECT_FLOAT_EQ(linearlyInterpolatedOpacity, curve->getValue(time));
        endTest();
    }

    virtual void afterTest()
    {
    }

private:
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestAddAnimationWithTimingFunction)

// Ensures that when opacity is being animated, this value does not cause the subtree to be skipped.
class CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity()
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setDrawOpacity(1);
        m_layerTreeHost->setViewportSize(IntSize(10, 10));
        m_layerTreeHost->rootLayer()->setOpacity(0);
        postAddAnimationToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl*)
    {
        // If the subtree was skipped when preparing to draw, the layer's draw opacity
        // will not have been updated. It should be set to 0 due to the animation.
        // Without the animation, the layer will be skipped since it has zero opacity.
        EXPECT_EQ(0, m_layerTreeHost->rootLayer()->drawOpacity());
        endTest();
    }

    virtual void afterTest()
    {
    }
};

#if OS(WINDOWS)
// http://webkit.org/b/74623
TEST_F(CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity, FLAKY_runMultiThread)
#else
TEST_F(CCLayerTreeHostTestDoNotSkipLayersWithAnimatedOpacity, runMultiThread)
#endif
{
    runTestThreaded();
}

// Ensures that main thread animations have their start times synchronized with impl thread animations.
class CCLayerTreeHostTestSynchronizeAnimationStartTimes : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestSynchronizeAnimationStartTimes()
        : m_layerTreeHostImpl(0)
    {
    }

    virtual void beginTest()
    {
        postAddAnimationToMainThread();
    }

    // This is guaranteed to be called before CCLayerTreeHostImpl::animateLayers.
    virtual void willAnimateLayers(CCLayerTreeHostImpl* layerTreeHostImpl, double monotonicTime)
    {
        m_layerTreeHostImpl = layerTreeHostImpl;
    }

    virtual void notifyAnimationStarted(double time)
    {
        EXPECT_TRUE(m_layerTreeHostImpl);

        CCLayerAnimationController* controllerImpl = m_layerTreeHostImpl->rootLayer()->layerAnimationController();
        CCLayerAnimationController* controller = m_layerTreeHost->rootLayer()->layerAnimationController();
        CCActiveAnimation* animationImpl = controllerImpl->getActiveAnimation(0, CCActiveAnimation::Opacity);
        CCActiveAnimation* animation = controller->getActiveAnimation(0, CCActiveAnimation::Opacity);

        EXPECT_EQ(animationImpl->startTime(), animation->startTime());

        endTest();
    }

    virtual void afterTest()
    {
    }

private:
    CCLayerTreeHostImpl* m_layerTreeHostImpl;
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestSynchronizeAnimationStartTimes)

// Ensures that main thread animations have their start times synchronized with impl thread animations.
class CCLayerTreeHostTestAnimationFinishedEvents : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestAnimationFinishedEvents()
    {
    }

    virtual void beginTest()
    {
        postAddInstantAnimationToMainThread();
    }

    virtual void notifyAnimationFinished(double time)
    {
        endTest();
    }

    virtual void afterTest()
    {
    }

private:
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestAnimationFinishedEvents)

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
        if (!impl->sourceFrameNumber())
            EXPECT_EQ(root->scrollPosition(), m_initialScroll);
        else if (impl->sourceFrameNumber() == 1)
            EXPECT_EQ(root->scrollPosition(), m_initialScroll + m_scrollAmount + m_scrollAmount);
        else if (impl->sourceFrameNumber() == 2)
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

// This test verifies that properties on the layer tree host are commited to the impl side.
class CCLayerTreeHostTestCommit : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestCommit() { }

    virtual void beginTest()
    {
        m_layerTreeHost->setViewportSize(IntSize(20, 20));
        m_layerTreeHost->setBackgroundColor(Color::gray);
        m_layerTreeHost->setPageScaleFactorAndLimits(5, 5, 5);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        EXPECT_EQ(IntSize(20, 20), impl->viewportSize());
        EXPECT_EQ(Color::gray, impl->backgroundColor());
        EXPECT_EQ(5, impl->pageScale());

        endTest();
    }

    virtual void afterTest() { }
};

TEST_F(CCLayerTreeHostTestCommit, runTest)
{
    runTest(true);
}

// Verifies that startPageScaleAnimation events propagate correctly from CCLayerTreeHost to
// CCLayerTreeHostImpl in the MT compositor.
class CCLayerTreeHostTestStartPageScaleAnimation : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestStartPageScaleAnimation()
        : m_animationRequested(false)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollPosition(IntPoint());
        postSetNeedsRedrawToMainThread();
    }

    static void requestStartPageScaleAnimation(void* self)
    {
        CCLayerTreeHostTestStartPageScaleAnimation* test = static_cast<CCLayerTreeHostTestStartPageScaleAnimation*>(self);
        if (test->layerTreeHost())
            test->layerTreeHost()->startPageScaleAnimation(IntSize(), false, 1.25, 0);
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        impl->rootLayer()->setScrollable(true);
        impl->rootLayer()->setScrollPosition(IntPoint());
        impl->setPageScaleFactorAndLimits(impl->pageScale(), 0.5, 2);

        // We request animation only once.
        if (!m_animationRequested) {
            callOnMainThread(CCLayerTreeHostTestStartPageScaleAnimation::requestStartPageScaleAnimation, this);
            m_animationRequested = true;
        }
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
        m_layerTreeHost->setPageScaleFactorAndLimits(scale, 0.5, 2);
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        impl->processScrollDeltas();
        // We get one commit before the first draw, and the animation doesn't happen until the second draw,
        // so results available on the third commit.
        if (impl->frameNumber() == 2) {
            EXPECT_EQ(1.25, impl->pageScale());
            endTest();
        } else
            postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest()
    {
    }

private:
    bool m_animationRequested;
};

TEST_F(CCLayerTreeHostTestStartPageScaleAnimation, runTest)
{
    runTest(true);
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

    virtual void update(CCTextureUpdater& updater, const CCOcclusionTracker* occlusion) OVERRIDE
    {
        ContentLayerChromium::update(updater, occlusion);
        m_paintContentsCount++;
    }

    virtual void idleUpdate(CCTextureUpdater& updater, const CCOcclusionTracker* occlusion) OVERRIDE
    {
        ContentLayerChromium::idleUpdate(updater, occlusion);
        m_idlePaintContentsCount++;
    }

private:
    explicit ContentLayerChromiumWithUpdateTracking(ContentLayerDelegate* delegate)
        : ContentLayerChromium(delegate)
        , m_paintContentsCount(0)
        , m_idlePaintContentsCount(0)
    {
        setBounds(IntSize(10, 10));
        setIsDrawable(true);
    }

    int m_paintContentsCount;
    int m_idlePaintContentsCount;
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
        // update() should have been called once.
        EXPECT_EQ(1, m_updateCheckLayer->paintContentsCount());

        // idleUpdate() should have been called once
        EXPECT_EQ(1, m_updateCheckLayer->idlePaintContentsCount());

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

        CCTextureUpdater updater;
        layerTreeHost()->updateLayers(updater);

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

class CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers : public CCLayerTreeHostTest {
public:

    CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers()
        : m_rootLayer(ContentLayerChromium::create(&m_delegate))
        , m_childLayer(ContentLayerChromium::create(&m_delegate))
    {
        m_settings.deviceScaleFactor = 1.5;
    }

    virtual void beginTest()
    {
        // The device viewport should be scaled by the device scale factor.
        m_layerTreeHost->setViewportSize(IntSize(40, 40));
        EXPECT_EQ(IntSize(40, 40), m_layerTreeHost->viewportSize());
        EXPECT_EQ(IntSize(60, 60), m_layerTreeHost->deviceViewportSize());

        m_rootLayer->addChild(m_childLayer);

        m_rootLayer->setIsDrawable(true);
        m_rootLayer->setBounds(IntSize(30, 30));
        m_rootLayer->setAnchorPoint(FloatPoint(0, 0));

        m_childLayer->setIsDrawable(true);
        m_childLayer->setPosition(IntPoint(2, 2));
        m_childLayer->setBounds(IntSize(10, 10));
        m_childLayer->setAnchorPoint(FloatPoint(0, 0));

        m_layerTreeHost->setRootLayer(m_rootLayer);
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        // Get access to protected methods.
        MockLayerTreeHostImpl* mockImpl = static_cast<MockLayerTreeHostImpl*>(impl);

        // Should only do one commit.
        EXPECT_EQ(0, impl->sourceFrameNumber());
        // Device scale factor should come over to impl.
        EXPECT_NEAR(impl->settings().deviceScaleFactor, 1.5, 0.00001);

        // Both layers are on impl.
        ASSERT_EQ(1u, impl->rootLayer()->children().size());

        // Device viewport is scaled.
        EXPECT_EQ(IntSize(40, 40), impl->viewportSize());
        EXPECT_EQ(IntSize(60, 60), impl->deviceViewportSize());

        CCLayerImpl* root = impl->rootLayer();
        CCLayerImpl* child = impl->rootLayer()->children()[0].get();

        // Positions remain in layout pixels.
        EXPECT_EQ(IntPoint(0, 0), root->position());
        EXPECT_EQ(IntPoint(2, 2), child->position());

        // Compute all the layer transforms for the frame.
        MockLayerTreeHostImpl::CCLayerList renderSurfaceLayerList;
        mockImpl->calculateRenderSurfaceLayerList(renderSurfaceLayerList);

        // Both layers should be drawing into the root render surface.
        ASSERT_EQ(1u, renderSurfaceLayerList.size());
        ASSERT_EQ(root->renderSurface(), renderSurfaceLayerList[0]->renderSurface());
        ASSERT_EQ(2u, root->renderSurface()->layerList().size());

        // The root render surface is the size of the viewport.
        EXPECT_EQ_RECT(IntRect(0, 0, 60, 60), root->renderSurface()->contentRect());

        TransformationMatrix scaleTransform;
        scaleTransform.scale(impl->settings().deviceScaleFactor);

        // The root layer is scaled by 2x.
        TransformationMatrix rootScreenSpaceTransform = scaleTransform;
        TransformationMatrix rootDrawTransform = scaleTransform;
        rootDrawTransform.translate(root->bounds().width() * 0.5, root->bounds().height() * 0.5);

        EXPECT_EQ(rootDrawTransform, root->drawTransform());
        EXPECT_EQ(rootScreenSpaceTransform, root->screenSpaceTransform());

        // The child is at position 2,2, so translate by 2,2 before applying the scale by 2x.
        TransformationMatrix childScreenSpaceTransform = scaleTransform;
        childScreenSpaceTransform.translate(2, 2);
        TransformationMatrix childDrawTransform = scaleTransform;
        childDrawTransform.translate(2, 2);
        childDrawTransform.translate(child->bounds().width() * 0.5, child->bounds().height() * 0.5);

        EXPECT_EQ(childDrawTransform, child->drawTransform());
        EXPECT_EQ(childScreenSpaceTransform, child->screenSpaceTransform());

        endTest();
    }

    virtual void afterTest()
    {
        m_rootLayer.clear();
        m_childLayer.clear();
    }

private:
    MockContentLayerDelegate m_delegate;
    RefPtr<ContentLayerChromium> m_rootLayer;
    RefPtr<ContentLayerChromium> m_childLayer;
};

TEST_F(CCLayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers, runMultiThread)
{
    runTest(true);
}

// Verify atomicity of commits and reuse of textures.
class CCLayerTreeHostTestAtomicCommit : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAtomicCommit()
        : m_layer(ContentLayerChromiumWithUpdateTracking::create(&m_delegate))
    {
        // Make sure partial texture updates are turned off.
        m_settings.maxPartialTextureUpdates = 0;
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(IntSize(10, 10));

        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(impl->context()));

        switch (impl->sourceFrameNumber()) {
        case 0:
            // Number of textures should be one.
            ASSERT_EQ(1, context->numTextures());
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->numUsedTextures());
            // Verify that used texture is correct.
            EXPECT_TRUE(context->usedTexture(context->texture(0)));

            context->resetUsedTextures();
            break;
        case 1:
            // Number of textures should be two as the first texture
            // is used by impl thread and cannot by used for update.
            ASSERT_EQ(2, context->numTextures());
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

        if (impl->sourceFrameNumber() < 1) {
            context->resetUsedTextures();
            postSetNeedsAnimateAndCommitToMainThread();
            postSetNeedsRedrawToMainThread();
        } else
            endTest();
    }

    virtual void layout()
    {
        m_layer->setNeedsDisplay();
    }

    virtual void afterTest()
    {
    }

private:
    MockContentLayerDelegate m_delegate;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_layer;
};

TEST_F(CCLayerTreeHostTestAtomicCommit, runMultiThread)
{
    runTest(true);
}

static void setLayerPropertiesForTesting(LayerChromium* layer, LayerChromium* parent, const TransformationMatrix& transform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool opaque)
{
    layer->removeAllChildren();
    if (parent)
        parent->addChild(layer);
    layer->setTransform(transform);
    layer->setAnchorPoint(anchor);
    layer->setPosition(position);
    layer->setBounds(bounds);
    layer->setOpaque(opaque);
}

class CCLayerTreeHostTestAtomicCommitWithPartialUpdate : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestAtomicCommitWithPartialUpdate()
        : m_parent(ContentLayerChromiumWithUpdateTracking::create(&m_delegate))
        , m_child(ContentLayerChromiumWithUpdateTracking::create(&m_delegate))
        , m_numCommits(0)
    {
        // Allow one partial texture update.
        m_settings.maxPartialTextureUpdates = 1;
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_parent);
        m_layerTreeHost->setViewportSize(IntSize(10, 20));

        TransformationMatrix identityMatrix;
        setLayerPropertiesForTesting(m_parent.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(10, 20), true);
        setLayerPropertiesForTesting(m_child.get(), m_parent.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(0, 10), IntSize(10, 10), false);

        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    virtual void commitCompleteOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CompositorFakeWebGraphicsContext3DWithTextureTracking* context = static_cast<CompositorFakeWebGraphicsContext3DWithTextureTracking*>(GraphicsContext3DPrivate::extractWebGraphicsContext3D(impl->context()));

        switch (impl->sourceFrameNumber()) {
        case 0:
            // Number of textures should be two.
            ASSERT_EQ(2, context->numTextures());
            // Number of textures used for commit should be two.
            EXPECT_EQ(2, context->numUsedTextures());
            // Verify that used textures are correct.
            EXPECT_TRUE(context->usedTexture(context->texture(0)));
            EXPECT_TRUE(context->usedTexture(context->texture(1)));

            context->resetUsedTextures();
            break;
        case 1:
            // Number of textures should be four as the first two
            // textures are used by the impl thread.
            ASSERT_EQ(4, context->numTextures());
            // Number of textures used for commit should still be two.
            EXPECT_EQ(2, context->numUsedTextures());
            // First two textures should not have been used.
            EXPECT_FALSE(context->usedTexture(context->texture(0)));
            EXPECT_FALSE(context->usedTexture(context->texture(1)));
            // New textures should have been used.
            EXPECT_TRUE(context->usedTexture(context->texture(2)));
            EXPECT_TRUE(context->usedTexture(context->texture(3)));

            context->resetUsedTextures();
            break;
        case 2:
            // Number of textures should be three as we allow one
            // partial update and the first two textures are used by
            // the impl thread.
            ASSERT_EQ(3, context->numTextures());
            // Number of textures used for commit should still be two.
            EXPECT_EQ(2, context->numUsedTextures());
            // First texture should have been used.
            EXPECT_TRUE(context->usedTexture(context->texture(0)));
            // Second texture should not have been used.
            EXPECT_FALSE(context->usedTexture(context->texture(1)));
            // Third texture should have been used.
            EXPECT_TRUE(context->usedTexture(context->texture(2)));

            context->resetUsedTextures();
            break;
        case 3:
            // Number of textures should be two.
            EXPECT_EQ(2, context->numTextures());
            // No textures should be used for commit.
            EXPECT_EQ(0, context->numUsedTextures());

            context->resetUsedTextures();
            break;
        case 4:
            // Number of textures should be two.
            EXPECT_EQ(2, context->numTextures());
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->numUsedTextures());

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

        // Number of textures used for drawing should two except for frame 4
        // where the viewport only contains one layer.
        if (impl->sourceFrameNumber() == 3)
            EXPECT_EQ(1, context->numUsedTextures());
        else
            EXPECT_EQ(2, context->numUsedTextures());

        if (impl->sourceFrameNumber() < 4) {
            context->resetUsedTextures();
            postSetNeedsAnimateAndCommitToMainThread();
            postSetNeedsRedrawToMainThread();
        } else
            endTest();
    }

    virtual void layout()
    {
        switch (m_numCommits++) {
        case 0:
        case 1:
            m_parent->setNeedsDisplay();
            m_child->setNeedsDisplay();
            break;
        case 2:
            // Damage part of layers.
            m_parent->setNeedsDisplayRect(FloatRect(0, 0, 5, 5));
            m_child->setNeedsDisplayRect(FloatRect(0, 0, 5, 5));
            break;
        case 3:
            m_child->setNeedsDisplay();
            m_layerTreeHost->setViewportSize(IntSize(10, 10));
            break;
        case 4:
            m_layerTreeHost->setViewportSize(IntSize(10, 20));
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    virtual void afterTest()
    {
    }

private:
    MockContentLayerDelegate m_delegate;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_parent;
    RefPtr<ContentLayerChromiumWithUpdateTracking> m_child;
    int m_numCommits;
};

TEST_F(CCLayerTreeHostTestAtomicCommitWithPartialUpdate, runMultiThread)
{
    runTest(true);
}

class TestLayerChromium : public LayerChromium {
public:
    static PassRefPtr<TestLayerChromium> create() { return adoptRef(new TestLayerChromium()); }

    virtual void update(CCTextureUpdater&, const CCOcclusionTracker* occlusion) OVERRIDE
    {
        // Gain access to internals of the CCOcclusionTracker.
        const TestCCOcclusionTracker* testOcclusion = static_cast<const TestCCOcclusionTracker*>(occlusion);
        m_occludedScreenSpace = testOcclusion ? testOcclusion->occlusionInScreenSpace() : Region();
    }

    virtual bool drawsContent() const OVERRIDE { return true; }

    const Region& occludedScreenSpace() const { return m_occludedScreenSpace; }
    void clearOccludedScreenSpace() { m_occludedScreenSpace = Region(); }

private:
    TestLayerChromium() : LayerChromium() { }

    Region m_occludedScreenSpace;
};

static void setTestLayerPropertiesForTesting(TestLayerChromium* layer, LayerChromium* parent, const TransformationMatrix& transform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool opaque)
{
    setLayerPropertiesForTesting(layer, parent, transform, anchor, position, bounds, opaque);
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
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        CCTextureUpdater updater;
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer is opaque, then it adds to the occlusion seen by the rootLayer.
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 170, 170), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Add a second child to the root layer and the regions should merge
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(70, 20), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 170, 170), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 20, 170, 180), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, rootLayer->occludedScreenSpace().rects().size());

        // Move the second child to be sure.
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 30, 170, 170), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 30, 190, 170), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a mask on it, then it shouldn't contribute to occlusion on stuff below it
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        child->setMaskLayer(mask.get());

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer with a mask is below child2, then child2 should contribute to occlusion on everything, and child shouldn't contribute to the rootLayer
        setLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        child->setMaskLayer(mask.get());

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a non-opaque drawOpacity, then it shouldn't contribute to occlusion on stuff below it
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

        child->setMaskLayer(0);
        child->setOpacity(0.5);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer with non-opaque drawOpacity is below child2, then child2 should contribute to occlusion on everything, and child shouldn't contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        child->setMaskLayer(0);
        child->setOpacity(0.5);

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
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

class CCLayerTreeHostTestLayerOcclusionWithFilters : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestLayerOcclusionWithFilters() { }

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

        // If the child layer has a filter that changes alpha values, and is below child2, then child2 should contribute to occlusion on everything,
        // and child shouldn't contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        {
            FilterOperations filters;
            filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::OPACITY));
            child->setFilters(filters);
        }

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        CCTextureUpdater updater;
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 40, 190, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(2u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // If the child layer has a filter that moves pixels/changes alpha, and is below child2, then child should not inherit occlusion from outside its subtree,
        // and should not contribute to the rootLayer
        setTestLayerPropertiesForTesting(rootLayer.get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
        setTestLayerPropertiesForTesting(child.get(), rootLayer.get(), childTransform, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(grandChild.get(), child.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);
        setTestLayerPropertiesForTesting(child2.get(), rootLayer.get(), identityMatrix, FloatPoint(0, 0), FloatPoint(10, 70), IntSize(500, 500), true);

        {
            FilterOperations filters;
            filters.operations().append(BlurFilterOperation::create(Length(10, WebCore::Percent), FilterOperation::BLUR));
            child->setFilters(filters);
        }

        m_layerTreeHost->setRootLayer(rootLayer);
        m_layerTreeHost->setViewportSize(rootLayer->bounds());
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        EXPECT_EQ_RECT(IntRect(), child2->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, child2->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(), grandChild->occludedScreenSpace().bounds());
        EXPECT_EQ(0u, grandChild->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(30, 40, 170, 160), child->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, child->occludedScreenSpace().rects().size());
        EXPECT_EQ_RECT(IntRect(10, 70, 190, 130), rootLayer->occludedScreenSpace().bounds());
        EXPECT_EQ(1u, rootLayer->occludedScreenSpace().rects().size());

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        CCLayerTreeHost::setNeedsFilterContext(false);
        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestLayerOcclusionWithFilters)

class CCLayerTreeHostTestManySurfaces : public CCLayerTreeHostTest {
public:
    CCLayerTreeHostTestManySurfaces() { }

    virtual void beginTest()
    {
        // We create enough RenderSurfaces that it will trigger Vector reallocation while computing occlusion.
        Region occluded;
        const TransformationMatrix identityMatrix;
        Vector<RefPtr<TestLayerChromium> > layers;
        Vector<RefPtr<TestLayerChromium> > children;
        int numSurfaces = 20;
        RefPtr<TestLayerChromium> replica = TestLayerChromium::create();

        for (int i = 0; i < numSurfaces; ++i) {
            layers.append(TestLayerChromium::create());
            if (!i) {
                setTestLayerPropertiesForTesting(layers.last().get(), 0, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(200, 200), true);
                layers.last()->createRenderSurface();
            } else {
                setTestLayerPropertiesForTesting(layers.last().get(), layers[layers.size()-2].get(), identityMatrix, FloatPoint(0, 0), FloatPoint(1, 1), IntSize(200-i, 200-i), true);
                layers.last()->setMasksToBounds(true);
                layers.last()->setReplicaLayer(replica.get()); // Make it have a RenderSurface
            }
        }

        for (int i = 1; i < numSurfaces; ++i) {
            children.append(TestLayerChromium::create());
            setTestLayerPropertiesForTesting(children.last().get(), layers[i].get(), identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), false);
        }

        m_layerTreeHost->setRootLayer(layers[0].get());
        m_layerTreeHost->setViewportSize(layers[0]->bounds());
        CCTextureUpdater updater;
        m_layerTreeHost->updateLayers(updater);
        m_layerTreeHost->commitComplete();

        for (int i = 0; i < numSurfaces-1; ++i) {
            IntRect expectedOcclusion(i+1, i+1, 200-i-1, 200-i-1);

            EXPECT_EQ_RECT(expectedOcclusion, layers[i]->occludedScreenSpace().bounds());
            EXPECT_EQ(1u, layers[i]->occludedScreenSpace().rects().size());
        }

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();

        endTest();
    }

    virtual void afterTest()
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(CCLayerTreeHostTestManySurfaces)

// A loseContext(1) should lead to a didRecreateContext(true)
class CCLayerTreeHostTestSetSingleLostContext : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestSetSingleLostContext()
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame()
    {
        m_layerTreeHost->loseContext(1);
    }

    virtual void didRecreateContext(bool succeeded)
    {
        EXPECT_TRUE(succeeded);
        endTest();
    }

    virtual void afterTest()
    {
    }
};

TEST_F(CCLayerTreeHostTestSetSingleLostContext, runMultiThread)
{
    runTestThreaded();
}

// A loseContext(10) should lead to a didRecreateContext(false), and
// a finishAllRendering() should not hang.
class CCLayerTreeHostTestSetRepeatedLostContext : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestSetRepeatedLostContext()
    {
    }

    virtual void beginTest()
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame()
    {
        m_layerTreeHost->loseContext(10);
    }

    virtual void didRecreateContext(bool succeeded)
    {
        EXPECT_FALSE(succeeded);
        m_layerTreeHost->finishAllRendering();
        endTest();
    }

    virtual void afterTest()
    {
    }
};

TEST_F(CCLayerTreeHostTestSetRepeatedLostContext, runMultiThread)
{
    runTestThreaded();
}

class CCLayerTreeHostTestFractionalScroll : public CCLayerTreeHostTestThreadOnly {
public:
    CCLayerTreeHostTestFractionalScroll()
        : m_scrollAmount(1.75, 0)
    {
    }

    virtual void beginTest()
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnCCThread(CCLayerTreeHostImpl* impl)
    {
        CCLayerImpl* root = impl->rootLayer();
        root->setMaxScrollPosition(IntSize(100, 100));

        // Check that a fractional scroll delta is correctly accumulated over multiple commits.
        if (impl->frameNumber() == 1) {
            EXPECT_EQ(root->scrollPosition(), IntPoint(0, 0));
            EXPECT_EQ(root->scrollDelta(), FloatSize(0, 0));
            postSetNeedsCommitToMainThread();
        } else if (impl->frameNumber() == 2) {
            EXPECT_EQ(root->scrollPosition(), flooredIntPoint(m_scrollAmount));
            EXPECT_EQ(root->scrollDelta(), FloatSize(fmod(m_scrollAmount.width(), 1), 0));
            postSetNeedsCommitToMainThread();
        } else if (impl->frameNumber() == 3) {
            EXPECT_EQ(root->scrollPosition(), flooredIntPoint(m_scrollAmount + m_scrollAmount));
            EXPECT_EQ(root->scrollDelta(), FloatSize(fmod(2 * m_scrollAmount.width(), 1), 0));
            endTest();
        }
        root->scrollBy(m_scrollAmount);
    }

    virtual void applyScrollAndScale(const IntSize& scrollDelta, float scale)
    {
        IntPoint position = m_layerTreeHost->rootLayer()->scrollPosition();
        m_layerTreeHost->rootLayer()->setScrollPosition(position + scrollDelta);
    }

    virtual void afterTest()
    {
    }
private:
    FloatSize m_scrollAmount;
};

TEST_F(CCLayerTreeHostTestFractionalScroll, runMultiThread)
{
    runTestThreaded();
}

} // namespace
