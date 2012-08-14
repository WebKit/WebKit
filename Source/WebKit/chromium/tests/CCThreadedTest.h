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

#ifndef CCThreadedTest_h
#define CCThreadedTest_h

#include "CompositorFakeWebGraphicsContext3D.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCScopedThreadProxy.h"
#include <gtest/gtest.h>
#include <public/WebAnimationDelegate.h>

namespace WebCore {
class CCLayerImpl;
class CCLayerTreeHost;
class CCLayerTreeHostClient;
class CCLayerTreeHostImpl;
class GraphicsContext3D;
}

namespace WebKit {
class WebThread;
}

namespace WebKitTests {

// Used by test stubs to notify the test when something interesting happens.
class TestHooks : public WebKit::WebAnimationDelegate {
public:
    virtual void beginCommitOnCCThread(WebCore::CCLayerTreeHostImpl*) { }
    virtual void commitCompleteOnCCThread(WebCore::CCLayerTreeHostImpl*) { }
    virtual bool prepareToDrawOnCCThread(WebCore::CCLayerTreeHostImpl*) { return true; }
    virtual void drawLayersOnCCThread(WebCore::CCLayerTreeHostImpl*) { }
    virtual void animateLayers(WebCore::CCLayerTreeHostImpl*, double monotonicTime) { }
    virtual void willAnimateLayers(WebCore::CCLayerTreeHostImpl*, double monotonicTime) { }
    virtual void applyScrollAndScale(const WebCore::IntSize&, float) { }
    virtual void updateAnimations(double monotonicTime) { }
    virtual void layout() { }
    virtual void didRecreateOutputSurface(bool succeeded) { }
    virtual void didAddAnimation() { }
    virtual void didCommit() { }
    virtual void didCommitAndDrawFrame() { }
    virtual void scheduleComposite() { }

    // Implementation of WebAnimationDelegate
    virtual void notifyAnimationStarted(double time) OVERRIDE { }
    virtual void notifyAnimationFinished(double time) OVERRIDE { }

    virtual PassOwnPtr<WebKit::WebCompositorOutputSurface> createOutputSurface();
};

class TimeoutTask;
class BeginTask;
class EndTestTask;

class MockCCLayerTreeHostClient : public WebCore::CCLayerTreeHostClient {
};

// The CCThreadedTests runs with the main loop running. It instantiates a single MockLayerTreeHost and associated
// MockLayerTreeHostImpl/MockLayerTreeHostClient.
//
// beginTest() is called once the main message loop is running and the layer tree host is initialized.
//
// Key stages of the drawing loop, e.g. drawing or commiting, redirect to CCThreadedTest methods of similar names.
// To track the commit process, override these functions.
//
// The test continues until someone calls endTest. endTest can be called on any thread, but be aware that
// ending the test is an asynchronous process.
class CCThreadedTest : public testing::Test, public TestHooks {
public:
    virtual void afterTest() = 0;
    virtual void beginTest() = 0;

    void endTest();
    void endTestAfterDelay(int delayMilliseconds);

    void postSetNeedsAnimateToMainThread();
    void postAddAnimationToMainThread();
    void postAddInstantAnimationToMainThread();
    void postSetNeedsCommitToMainThread();
    void postAcquireLayerTextures();
    void postSetNeedsRedrawToMainThread();
    void postSetNeedsAnimateAndCommitToMainThread();
    void postSetVisibleToMainThread(bool visible);
    void postDidAddAnimationToMainThread();

    void doBeginTest();
    void timeout();

    void clearTimeout() { m_timeoutTask = 0; }
    void clearEndTestTask() { m_endTestTask = 0; }

    WebCore::CCLayerTreeHost* layerTreeHost() { return m_layerTreeHost.get(); }

protected:
    CCThreadedTest();

    virtual void scheduleComposite();

    static void onEndTest(void* self);

    static void dispatchSetNeedsAnimate(void* self);
    static void dispatchAddInstantAnimation(void* self);
    static void dispatchAddAnimation(void* self);
    static void dispatchSetNeedsAnimateAndCommit(void* self);
    static void dispatchSetNeedsCommit(void* self);
    static void dispatchAcquireLayerTextures(void* self);
    static void dispatchSetNeedsRedraw(void* self);
    static void dispatchSetVisible(void* self);
    static void dispatchSetInvisible(void* self);
    static void dispatchComposite(void* self);
    static void dispatchDidAddAnimation(void* self);

    virtual void runTest(bool threaded);
    WebKit::WebThread* webThread() const { return m_webThread.get(); }

    WebCore::CCLayerTreeSettings m_settings;
    OwnPtr<MockCCLayerTreeHostClient> m_client;
    OwnPtr<WebCore::CCLayerTreeHost> m_layerTreeHost;

private:
    bool m_beginning;
    bool m_endWhenBeginReturns;
    bool m_timedOut;
    bool m_finished;
    bool m_scheduled;
    bool m_started;

    OwnPtr<WebKit::WebThread> m_webThread;
    RefPtr<WebCore::CCScopedThreadProxy> m_mainThreadProxy;
    TimeoutTask* m_timeoutTask;
    BeginTask* m_beginTask;
    EndTestTask* m_endTestTask;
};

class CCThreadedTestThreadOnly : public CCThreadedTest {
public:
    void runTestThreaded()
    {
        CCThreadedTest::runTest(true);
    }
};

// Adapts CCLayerTreeHostImpl for test. Runs real code, then invokes test hooks.
class MockLayerTreeHostImpl : public WebCore::CCLayerTreeHostImpl {
public:
    static PassOwnPtr<MockLayerTreeHostImpl> create(TestHooks*, const WebCore::CCLayerTreeSettings&, WebCore::CCLayerTreeHostImplClient*);

    virtual void beginCommit();
    virtual void commitComplete();
    virtual bool prepareToDraw(FrameData&);
    virtual void drawLayers(const FrameData&);

    // Make these public.
    typedef Vector<WebCore::CCLayerImpl*> CCLayerList;
    using CCLayerTreeHostImpl::calculateRenderSurfaceLayerList;

protected:
    virtual void animateLayers(double monotonicTime, double wallClockTime);
    virtual double lowFrequencyAnimationInterval() const;

private:
    MockLayerTreeHostImpl(TestHooks*, const WebCore::CCLayerTreeSettings&, WebCore::CCLayerTreeHostImplClient*);

    TestHooks* m_testHooks;
};

class CompositorFakeWebGraphicsContext3DWithTextureTracking : public WebKit::CompositorFakeWebGraphicsContext3D {
public:
    static PassOwnPtr<CompositorFakeWebGraphicsContext3DWithTextureTracking> create(Attributes);

    virtual WebKit::WebGLId createTexture();

    virtual void deleteTexture(WebKit::WebGLId texture);

    virtual void bindTexture(WebKit::WGC3Denum target, WebKit::WebGLId texture);

    int numTextures() const;
    int texture(int texture) const;
    void resetTextures();

    int numUsedTextures() const;
    bool usedTexture(int texture) const;
    void resetUsedTextures();

private:
    explicit CompositorFakeWebGraphicsContext3DWithTextureTracking(Attributes attrs);

    Vector<WebKit::WebGLId> m_textures;
    HashSet<WebKit::WebGLId, DefaultHash<WebKit::WebGLId>::Hash, WTF::UnsignedWithZeroKeyHashTraits<WebKit::WebGLId> > m_usedTextures;
};

} // namespace WebKitTests

#define SINGLE_AND_MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME) \
    TEST_F(TEST_FIXTURE_NAME, runSingleThread)            \
    {                                                     \
        runTest(false);                                   \
    }                                                     \
    TEST_F(TEST_FIXTURE_NAME, runMultiThread)             \
    {                                                     \
        runTest(true);                                    \
    }

#endif // CCThreadedTest_h
