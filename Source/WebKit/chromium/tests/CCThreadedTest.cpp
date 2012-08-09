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

#include "CCThreadedTest.h"

#include "CCAnimationTestCommon.h"
#include "CCOcclusionTrackerTestCommon.h"
#include "CCTiledLayerTestCommon.h"
#include "ContentLayerChromium.h"
#include "FakeWebCompositorOutputSurface.h"
#include "FakeWebGraphicsContext3D.h"
#include "LayerChromium.h"
#include "cc/CCActiveAnimation.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCScopedThreadProxy.h"
#include "cc/CCSingleThreadProxy.h"
#include "cc/CCTextureUpdateQueue.h"
#include "cc/CCThreadTask.h"
#include "cc/CCTimingFunction.h"
#include <gmock/gmock.h>
#include <public/Platform.h>
#include <public/WebCompositor.h>
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <public/WebThread.h>
#include <wtf/Locker.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>

using namespace WebCore;
using namespace WebKit;
using namespace WTF;

namespace WebKitTests {

PassOwnPtr<CompositorFakeWebGraphicsContext3DWithTextureTracking> CompositorFakeWebGraphicsContext3DWithTextureTracking::create(Attributes attrs)
{
    return adoptPtr(new CompositorFakeWebGraphicsContext3DWithTextureTracking(attrs));
}

WebGLId CompositorFakeWebGraphicsContext3DWithTextureTracking::createTexture()
{
    WebGLId texture = m_textures.size() + 1;
    m_textures.append(texture);
    return texture;
}

void CompositorFakeWebGraphicsContext3DWithTextureTracking::deleteTexture(WebGLId texture)
{
    for (size_t i = 0; i < m_textures.size(); i++) {
        if (m_textures[i] == texture) {
            m_textures.remove(i);
            break;
        }
    }
}

void CompositorFakeWebGraphicsContext3DWithTextureTracking::bindTexture(WGC3Denum /* target */, WebGLId texture)
{
    m_usedTextures.add(texture);
}

int CompositorFakeWebGraphicsContext3DWithTextureTracking::numTextures() const { return static_cast<int>(m_textures.size()); }
int CompositorFakeWebGraphicsContext3DWithTextureTracking::texture(int i) const { return m_textures[i]; }
void CompositorFakeWebGraphicsContext3DWithTextureTracking::resetTextures() { m_textures.clear(); }

int CompositorFakeWebGraphicsContext3DWithTextureTracking::numUsedTextures() const { return static_cast<int>(m_usedTextures.size()); }
bool CompositorFakeWebGraphicsContext3DWithTextureTracking::usedTexture(int texture) const { return m_usedTextures.find(texture) != m_usedTextures.end(); }
void CompositorFakeWebGraphicsContext3DWithTextureTracking::resetUsedTextures() { m_usedTextures.clear(); }

CompositorFakeWebGraphicsContext3DWithTextureTracking::CompositorFakeWebGraphicsContext3DWithTextureTracking(Attributes attrs) : CompositorFakeWebGraphicsContext3D(attrs)
{
}

PassOwnPtr<WebCompositorOutputSurface> TestHooks::createOutputSurface()
{
    return FakeWebCompositorOutputSurface::create(CompositorFakeWebGraphicsContext3DWithTextureTracking::create(WebGraphicsContext3D::Attributes()));
}

PassOwnPtr<MockLayerTreeHostImpl> MockLayerTreeHostImpl::create(TestHooks* testHooks, const CCLayerTreeSettings& settings, CCLayerTreeHostImplClient* client)
{
    return adoptPtr(new MockLayerTreeHostImpl(testHooks, settings, client));
}

void MockLayerTreeHostImpl::beginCommit()
{
    CCLayerTreeHostImpl::beginCommit();
    m_testHooks->beginCommitOnCCThread(this);
}

void MockLayerTreeHostImpl::commitComplete()
{
    CCLayerTreeHostImpl::commitComplete();
    m_testHooks->commitCompleteOnCCThread(this);
}

bool MockLayerTreeHostImpl::prepareToDraw(FrameData& frame)
{
    bool result = CCLayerTreeHostImpl::prepareToDraw(frame);
    if (!m_testHooks->prepareToDrawOnCCThread(this))
        result = false;
    return result;
}

void MockLayerTreeHostImpl::drawLayers(const FrameData& frame)
{
    CCLayerTreeHostImpl::drawLayers(frame);
    m_testHooks->drawLayersOnCCThread(this);
}

void MockLayerTreeHostImpl::animateLayers(double monotonicTime, double wallClockTime)
{
    m_testHooks->willAnimateLayers(this, monotonicTime);
    CCLayerTreeHostImpl::animateLayers(monotonicTime, wallClockTime);
    m_testHooks->animateLayers(this, monotonicTime);
}

double MockLayerTreeHostImpl::lowFrequencyAnimationInterval() const
{
    return 1.0 / 60;
}

MockLayerTreeHostImpl::MockLayerTreeHostImpl(TestHooks* testHooks, const CCLayerTreeSettings& settings, CCLayerTreeHostImplClient* client)
    : CCLayerTreeHostImpl(settings, client)
    , m_testHooks(testHooks)
{
}

// Adapts CCLayerTreeHost for test. Injects MockLayerTreeHostImpl.
class MockLayerTreeHost : public WebCore::CCLayerTreeHost {
public:
    static PassOwnPtr<MockLayerTreeHost> create(TestHooks* testHooks, WebCore::CCLayerTreeHostClient* client, PassRefPtr<WebCore::LayerChromium> rootLayer, const WebCore::CCLayerTreeSettings& settings)
    {
        OwnPtr<MockLayerTreeHost> layerTreeHost(adoptPtr(new MockLayerTreeHost(testHooks, client, settings)));
        bool success = layerTreeHost->initialize();
        EXPECT_TRUE(success);
        layerTreeHost->setRootLayer(rootLayer);

        // LayerTreeHostImpl won't draw if it has 1x1 viewport.
        layerTreeHost->setViewportSize(IntSize(1, 1), IntSize(1, 1));

        layerTreeHost->rootLayer()->setLayerAnimationDelegate(testHooks);

        return layerTreeHost.release();
    }

    virtual PassOwnPtr<WebCore::CCLayerTreeHostImpl> createLayerTreeHostImpl(WebCore::CCLayerTreeHostImplClient* client)
    {
        return MockLayerTreeHostImpl::create(m_testHooks, settings(), client);
    }

    virtual void didAddAnimation() OVERRIDE
    {
        CCLayerTreeHost::didAddAnimation();
        m_testHooks->didAddAnimation();
    }

private:
    MockLayerTreeHost(TestHooks* testHooks, WebCore::CCLayerTreeHostClient* client, const WebCore::CCLayerTreeSettings& settings)
        : CCLayerTreeHost(client, settings)
        , m_testHooks(testHooks)
    {
    }

    TestHooks* m_testHooks;
};

// Implementation of CCLayerTreeHost callback interface.
class MockLayerTreeHostClient : public MockCCLayerTreeHostClient {
public:
    static PassOwnPtr<MockLayerTreeHostClient> create(TestHooks* testHooks)
    {
        return adoptPtr(new MockLayerTreeHostClient(testHooks));
    }

    virtual void willBeginFrame() OVERRIDE
    {
    }

    virtual void didBeginFrame() OVERRIDE
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

    virtual PassOwnPtr<WebCompositorOutputSurface> createOutputSurface() OVERRIDE
    {
        return m_testHooks->createOutputSurface();
    }

    virtual void willCommit() OVERRIDE
    {
    }

    virtual void didCommit() OVERRIDE
    {
        m_testHooks->didCommit();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_testHooks->didCommitAndDrawFrame();
    }

    virtual void didCompleteSwapBuffers() OVERRIDE
    {
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        m_testHooks->didRecreateOutputSurface(succeeded);
    }

    virtual void scheduleComposite() OVERRIDE
    {
        m_testHooks->scheduleComposite();
    }

private:
    explicit MockLayerTreeHostClient(TestHooks* testHooks) : m_testHooks(testHooks) { }

    TestHooks* m_testHooks;
};

class TimeoutTask : public WebThread::Task {
public:
    explicit TimeoutTask(CCThreadedTest* test)
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
    CCThreadedTest* m_test;
};

class BeginTask : public WebThread::Task {
public:
    explicit BeginTask(CCThreadedTest* test)
        : m_test(test)
    {
    }

    virtual ~BeginTask() { }
    virtual void run()
    {
        m_test->doBeginTest();
    }
private:
    CCThreadedTest* m_test;
};

class EndTestTask : public WebThread::Task {
public:
    explicit EndTestTask(CCThreadedTest* test)
        : m_test(test)
    {
    }

    virtual ~EndTestTask()
    {
        if (m_test)
            m_test->clearEndTestTask();
    }

    void clearTest()
    {
        m_test = 0;
    }

    virtual void run()
    {
        if (m_test)
            m_test->endTest();
    }

private:
    CCThreadedTest* m_test;
};

CCThreadedTest::CCThreadedTest()
    : m_beginning(false)
    , m_endWhenBeginReturns(false)
    , m_timedOut(false)
    , m_finished(false)
    , m_scheduled(false)
    , m_started(false)
    , m_endTestTask(0)
{ }

void CCThreadedTest::endTest()
{
    m_finished = true;

    // If we are called from the CCThread, re-call endTest on the main thread.
    if (!isMainThread())
        m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::endTest));
    else {
        // For the case where we endTest during beginTest(), set a flag to indicate that
        // the test should end the second beginTest regains control.
        if (m_beginning)
            m_endWhenBeginReturns = true;
        else
            onEndTest(static_cast<void*>(this));
    }
}

void CCThreadedTest::endTestAfterDelay(int delayMilliseconds)
{
    // If we are called from the CCThread, re-call endTest on the main thread.
    if (!isMainThread())
        m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadedTest::endTestAfterDelay, delayMilliseconds));
    else {
        m_endTestTask = new EndTestTask(this);
        WebKit::Platform::current()->currentThread()->postDelayedTask(m_endTestTask, delayMilliseconds);
    }
}

void CCThreadedTest::postSetNeedsAnimateToMainThread()
{
    callOnMainThread(CCThreadedTest::dispatchSetNeedsAnimate, this);
}

void CCThreadedTest::postAddAnimationToMainThread()
{
    callOnMainThread(CCThreadedTest::dispatchAddAnimation, this);
}

void CCThreadedTest::postAddInstantAnimationToMainThread()
{
    callOnMainThread(CCThreadedTest::dispatchAddInstantAnimation, this);
}

void CCThreadedTest::postSetNeedsCommitToMainThread()
{
    callOnMainThread(CCThreadedTest::dispatchSetNeedsCommit, this);
}

void CCThreadedTest::postAcquireLayerTextures()
{
    callOnMainThread(CCThreadedTest::dispatchAcquireLayerTextures, this);
}

void CCThreadedTest::postSetNeedsRedrawToMainThread()
{
    callOnMainThread(CCThreadedTest::dispatchSetNeedsRedraw, this);
}

void CCThreadedTest::postSetNeedsAnimateAndCommitToMainThread()
{
    callOnMainThread(CCThreadedTest::dispatchSetNeedsAnimateAndCommit, this);
}

void CCThreadedTest::postSetVisibleToMainThread(bool visible)
{
    callOnMainThread(visible ? CCThreadedTest::dispatchSetVisible : CCThreadedTest::dispatchSetInvisible, this);
}

void CCThreadedTest::postDidAddAnimationToMainThread()
{
    callOnMainThread(CCThreadedTest::dispatchDidAddAnimation, this);
}

void CCThreadedTest::doBeginTest()
{
    ASSERT(isMainThread());
    m_client = MockLayerTreeHostClient::create(this);

    RefPtr<LayerChromium> rootLayer = LayerChromium::create();
    m_layerTreeHost = MockLayerTreeHost::create(this, m_client.get(), rootLayer, m_settings);
    ASSERT_TRUE(m_layerTreeHost);
    rootLayer->setLayerTreeHost(m_layerTreeHost.get());
    m_layerTreeHost->setSurfaceReady();

    m_started = true;
    m_beginning = true;
    beginTest();
    m_beginning = false;
    if (m_endWhenBeginReturns)
        onEndTest(static_cast<void*>(this));
}

void CCThreadedTest::timeout()
{
    m_timedOut = true;
    endTest();
}

void CCThreadedTest::scheduleComposite()
{
    if (!m_started || m_scheduled || m_finished)
        return;
    m_scheduled = true;
    callOnMainThread(&CCThreadedTest::dispatchComposite, this);
}

void CCThreadedTest::onEndTest(void* self)
{
    ASSERT(isMainThread());
    WebKit::Platform::current()->currentThread()->exitRunLoop();
}

void CCThreadedTest::dispatchSetNeedsAnimate(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost)
        test->m_layerTreeHost->setNeedsAnimate();
}

void CCThreadedTest::dispatchAddInstantAnimation(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost && test->m_layerTreeHost->rootLayer())
        addOpacityTransitionToLayer(*test->m_layerTreeHost->rootLayer(), 0, 0, 0.5, false);
}

void CCThreadedTest::dispatchAddAnimation(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost && test->m_layerTreeHost->rootLayer())
        addOpacityTransitionToLayer(*test->m_layerTreeHost->rootLayer(), 10, 0, 0.5, true);
}

void CCThreadedTest::dispatchSetNeedsAnimateAndCommit(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost) {
        test->m_layerTreeHost->setNeedsAnimate();
        test->m_layerTreeHost->setNeedsCommit();
    }
}

void CCThreadedTest::dispatchSetNeedsCommit(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT_TRUE(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost)
        test->m_layerTreeHost->setNeedsCommit();
}

void CCThreadedTest::dispatchAcquireLayerTextures(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT_TRUE(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost)
        test->m_layerTreeHost->acquireLayerTextures();
}

void CCThreadedTest::dispatchSetNeedsRedraw(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT_TRUE(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost)
        test->m_layerTreeHost->setNeedsRedraw();
}

void CCThreadedTest::dispatchSetVisible(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost)
        test->m_layerTreeHost->setVisible(true);
}

void CCThreadedTest::dispatchSetInvisible(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost)
        test->m_layerTreeHost->setVisible(false);
}

void CCThreadedTest::dispatchComposite(void* self)
{
    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT(isMainThread());
    ASSERT(test);
    test->m_scheduled = false;
    if (test->m_layerTreeHost && !test->m_finished)
        test->m_layerTreeHost->composite();
}

void CCThreadedTest::dispatchDidAddAnimation(void* self)
{
    ASSERT(isMainThread());

    CCThreadedTest* test = static_cast<CCThreadedTest*>(self);
    ASSERT(test);
    if (test->m_finished)
        return;

    if (test->m_layerTreeHost)
        test->m_layerTreeHost->didAddAnimation();
}

void CCThreadedTest::runTest(bool threaded)
{
    // For these tests, we will enable threaded animations.
    WebCompositor::setAcceleratedAnimationEnabled(true);

    if (threaded) {
        m_webThread = adoptPtr(WebKit::Platform::current()->createThread("CCThreadedTest"));
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

    if (m_endTestTask)
        m_endTestTask->clearTest();

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

} // namespace WebKitTests
