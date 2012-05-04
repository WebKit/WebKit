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

#include "cc/CCThreadProxy.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "SharedGraphicsContext3D.h"
#include "TraceEvent.h"
#include "cc/CCDelayBasedTimeSource.h"
#include "cc/CCFontAtlas.h"
#include "cc/CCFrameRateController.h"
#include "cc/CCInputHandler.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCScheduler.h"
#include "cc/CCScopedThreadProxy.h"
#include "cc/CCTextureUpdater.h"
#include "cc/CCThreadTask.h"
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

using namespace WTF;

namespace {

// Number of textures to update with each call to
// scheduledActionUpdateMoreResources().
static const size_t textureUpdatesPerFrame = 48;

// Measured in seconds.
static const double contextRecreationTickRate = 0.03;

} // anonymous namespace

namespace WebCore {

PassOwnPtr<CCProxy> CCThreadProxy::create(CCLayerTreeHost* layerTreeHost)
{
    return adoptPtr(new CCThreadProxy(layerTreeHost));
}

CCThreadProxy::CCThreadProxy(CCLayerTreeHost* layerTreeHost)
    : m_animateRequested(false)
    , m_commitRequested(false)
    , m_contextLost(false)
    , m_layerTreeHost(layerTreeHost)
    , m_compositorIdentifier(-1)
    , m_layerRendererInitialized(false)
    , m_started(false)
    , m_texturesAcquired(true)
    , m_mainThreadProxy(CCScopedThreadProxy::create(CCProxy::mainThread()))
    , m_beginFrameCompletionEventOnImplThread(0)
    , m_readbackRequestOnImplThread(0)
    , m_finishAllRenderingCompletionEventOnImplThread(0)
    , m_commitCompletionEventOnImplThread(0)
    , m_textureAcquisitionCompletionEventOnImplThread(0)
    , m_nextFrameIsNewlyCommittedFrameOnImplThread(false)
{
    TRACE_EVENT("CCThreadProxy::CCThreadProxy", this, 0);
    ASSERT(isMainThread());
}

CCThreadProxy::~CCThreadProxy()
{
    TRACE_EVENT("CCThreadProxy::~CCThreadProxy", this, 0);
    ASSERT(isMainThread());
    ASSERT(!m_started);
}

bool CCThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    TRACE_EVENT("CCThreadPRoxy::compositeAndReadback", this, 0);
    ASSERT(isMainThread());
    ASSERT(m_layerTreeHost);

    if (!m_layerRendererInitialized) {
        TRACE_EVENT("compositeAndReadback_EarlyOut_LR_Uninitialized", this, 0);
        return false;
    }


    // Perform a synchronous commit.
    CCCompletionEvent beginFrameCompletion;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::forceBeginFrameOnImplThread, AllowCrossThreadAccess(&beginFrameCompletion)));
    beginFrameCompletion.wait();
    beginFrame();

    // Perform a synchronous readback.
    ReadbackRequest request;
    request.rect = rect;
    request.pixels = pixels;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::requestReadbackOnImplThread, AllowCrossThreadAccess(&request)));
    request.completion.wait();
    return request.success;
}

void CCThreadProxy::requestReadbackOnImplThread(ReadbackRequest* request)
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(!m_readbackRequestOnImplThread);
    if (!m_layerTreeHostImpl) {
        request->success = false;
        request->completion.signal();
        return;
    }

    m_readbackRequestOnImplThread = request;
    m_schedulerOnImplThread->setNeedsRedraw();
    m_schedulerOnImplThread->setNeedsForcedRedraw();
}

void CCThreadProxy::startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration)
{
    ASSERT(CCProxy::isMainThread());
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::requestStartPageScaleAnimationOnImplThread, targetPosition, useAnchor, scale, duration));
}

void CCThreadProxy::requestStartPageScaleAnimationOnImplThread(IntSize targetPosition, bool useAnchor, float scale, double duration)
{
    ASSERT(CCProxy::isImplThread());
    if (m_layerTreeHostImpl)
        m_layerTreeHostImpl->startPageScaleAnimation(targetPosition, useAnchor, scale, monotonicallyIncreasingTime(), duration);
}

GraphicsContext3D* CCThreadProxy::context()
{
    return 0;
}

void CCThreadProxy::finishAllRendering()
{
    ASSERT(CCProxy::isMainThread());

    // Make sure all GL drawing is finished on the impl thread.
    CCCompletionEvent completion;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::finishAllRenderingOnImplThread, AllowCrossThreadAccess(&completion)));
    completion.wait();
}

bool CCThreadProxy::isStarted() const
{
    ASSERT(CCProxy::isMainThread());
    return m_started;
}

bool CCThreadProxy::initializeContext()
{
    TRACE_EVENT("CCThreadProxy::initializeContext", this, 0);
    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createContext();
    if (!context)
        return false;
    ASSERT(context->hasOneRef());

    // Leak the context pointer so we can transfer ownership of it to the other side...
    GraphicsContext3D* contextPtr = context.release().leakRef();
    ASSERT(contextPtr->hasOneRef());

    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::initializeContextOnImplThread,
                                                       AllowCrossThreadAccess(contextPtr)));
    return true;
}

void CCThreadProxy::setSurfaceReady()
{
    TRACE_EVENT0("cc", "CCThreadProxy::setSurfaceReady");
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::setSurfaceReadyOnImplThread));
}

void CCThreadProxy::setSurfaceReadyOnImplThread()
{
    TRACE_EVENT0("cc", "CCThreadProxy::setSurfaceReadyOnImplThread");
    m_schedulerOnImplThread->setCanBeginFrame(true);
}

bool CCThreadProxy::initializeLayerRenderer()
{
    TRACE_EVENT("CCThreadProxy::initializeLayerRenderer", this, 0);
    // Make a blocking call to initializeLayerRendererOnImplThread. The results of that call
    // are pushed into the initializeSucceeded and capabilities local variables.
    CCCompletionEvent completion;
    bool initializeSucceeded = false;
    LayerRendererCapabilities capabilities;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::initializeLayerRendererOnImplThread,
                                                       AllowCrossThreadAccess(&completion),
                                                       AllowCrossThreadAccess(&initializeSucceeded),
                                                       AllowCrossThreadAccess(&capabilities)));
    completion.wait();

    if (initializeSucceeded) {
        m_layerRendererInitialized = true;
        m_layerRendererCapabilitiesMainThreadCopy = capabilities;
    }
    return initializeSucceeded;
}

bool CCThreadProxy::recreateContext()
{
    TRACE_EVENT0("cc", "CCThreadProxy::recreateContext");
    ASSERT(isMainThread());

    // Try to create the context.
    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createContext();
    if (!context)
        return false;
    if (CCLayerTreeHost::needsFilterContext())
        if (!SharedGraphicsContext3D::createForImplThread())
            return false;

    ASSERT(context->hasOneRef());

    // Leak the context pointer so we can transfer ownership of it to the other side...
    GraphicsContext3D* contextPtr = context.release().leakRef();
    ASSERT(contextPtr->hasOneRef());

    // Make a blocking call to recreateContextOnImplThread. The results of that
    // call are pushed into the recreateSucceeded and capabilities local
    // variables.
    CCCompletionEvent completion;
    bool recreateSucceeded = false;
    LayerRendererCapabilities capabilities;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::recreateContextOnImplThread,
                                                       AllowCrossThreadAccess(&completion),
                                                       AllowCrossThreadAccess(contextPtr),
                                                       AllowCrossThreadAccess(&recreateSucceeded),
                                                       AllowCrossThreadAccess(&capabilities)));
    completion.wait();

    if (recreateSucceeded)
        m_layerRendererCapabilitiesMainThreadCopy = capabilities;
    return recreateSucceeded;
}

int CCThreadProxy::compositorIdentifier() const
{
    ASSERT(isMainThread());
    return m_compositorIdentifier;
}

const LayerRendererCapabilities& CCThreadProxy::layerRendererCapabilities() const
{
    ASSERT(m_layerRendererInitialized);
    return m_layerRendererCapabilitiesMainThreadCopy;
}

void CCThreadProxy::loseContext()
{
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::didLoseContextOnImplThread));
}

void CCThreadProxy::setNeedsAnimate()
{
    ASSERT(isMainThread());
    if (m_animateRequested)
        return;

    TRACE_EVENT("CCThreadProxy::setNeedsAnimate", this, 0);
    m_animateRequested = true;
    setNeedsCommit();
}

void CCThreadProxy::setNeedsCommit()
{
    ASSERT(isMainThread());
    if (m_commitRequested)
        return;

    TRACE_EVENT("CCThreadProxy::setNeedsCommit", this, 0);
    m_commitRequested = true;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::setNeedsCommitOnImplThread));
}

void CCThreadProxy::didLoseContextOnImplThread()
{
    ASSERT(isImplThread());
    TRACE_EVENT0("cc", "CCThreadProxy::didLoseContextOnImplThread");
    m_schedulerOnImplThread->didLoseContext();
}

void CCThreadProxy::onSwapBuffersCompleteOnImplThread()
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::onSwapBuffersCompleteOnImplThread", this, 0);
    m_schedulerOnImplThread->didSwapBuffersComplete();
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadProxy::didCompleteSwapBuffers));
}

void CCThreadProxy::setNeedsCommitOnImplThread()
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::setNeedsCommitOnImplThread", this, 0);
    m_schedulerOnImplThread->setNeedsCommit();
}

void CCThreadProxy::postAnimationEventsToMainThreadOnImplThread(PassOwnPtr<CCAnimationEventsVector> events, double wallClockTime)
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::postAnimationEventsToMainThreadOnImplThread", this, 0);
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadProxy::setAnimationEvents, events, wallClockTime));
}

void CCThreadProxy::postSetContentsMemoryAllocationLimitBytesToMainThreadOnImplThread(size_t bytes)
{
    ASSERT(isImplThread());
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadProxy::setContentsMemoryAllocationLimitBytes, bytes));
}

void CCThreadProxy::setNeedsRedraw()
{
    ASSERT(isMainThread());
    TRACE_EVENT("CCThreadProxy::setNeedsRedraw", this, 0);
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::setFullRootLayerDamageOnImplThread));
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::setNeedsRedrawOnImplThread));
}

bool CCThreadProxy::commitRequested() const
{
    ASSERT(isMainThread());
    return m_commitRequested;
}

void CCThreadProxy::setVisible(bool visible)
{
    ASSERT(isMainThread());
    CCCompletionEvent completion;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::setVisibleOnImplThread, AllowCrossThreadAccess(&completion), visible));
    completion.wait();
}

void CCThreadProxy::setVisibleOnImplThread(CCCompletionEvent* completion, bool visible)
{
    ASSERT(isImplThread());
    m_schedulerOnImplThread->setVisible(visible);
    m_layerTreeHostImpl->setVisible(visible);
    if (!visible) {
        m_layerTreeHost->didBecomeInvisibleOnImplThread(m_layerTreeHostImpl.get());
        // as partial or all the textures may be evicted in didBecomeInvisibleOnImplThread,
        // schedule a commit which will be executed when it goes to visible again.
        m_schedulerOnImplThread->setNeedsCommit();
    } else
        m_schedulerOnImplThread->setNeedsRedraw();
    completion->signal();
}

void CCThreadProxy::setNeedsRedrawOnImplThread()
{
    ASSERT(isImplThread());
    TRACE_EVENT("CCThreadProxy::setNeedsRedrawOnImplThread", this, 0);
    m_schedulerOnImplThread->setNeedsRedraw();
}

void CCThreadProxy::start()
{
    ASSERT(isMainThread());
    ASSERT(CCProxy::implThread());
    // Create LayerTreeHostImpl.
    CCCompletionEvent completion;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::initializeImplOnImplThread, AllowCrossThreadAccess(&completion)));
    completion.wait();

    m_started = true;
}

void CCThreadProxy::stop()
{
    TRACE_EVENT("CCThreadProxy::stop", this, 0);
    ASSERT(isMainThread());
    ASSERT(m_started);

    // Synchronously deletes the impl.
    CCCompletionEvent completion;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::layerTreeHostClosedOnImplThread, AllowCrossThreadAccess(&completion)));
    completion.wait();

    m_mainThreadProxy->shutdown(); // Stop running tasks posted to us.

    ASSERT(!m_layerTreeHostImpl); // verify that the impl deleted.
    m_layerTreeHost = 0;
    m_started = false;
}

void CCThreadProxy::forceSerializeOnSwapBuffers()
{
    CCCompletionEvent completion;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::forceSerializeOnSwapBuffersOnImplThread, AllowCrossThreadAccess(&completion)));
    completion.wait();
}

void CCThreadProxy::forceSerializeOnSwapBuffersOnImplThread(CCCompletionEvent* completion)
{
    if (m_layerRendererInitialized)
        m_layerTreeHostImpl->layerRenderer()->doNoOp();
    completion->signal();
}


void CCThreadProxy::finishAllRenderingOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::finishAllRenderingOnImplThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(!m_finishAllRenderingCompletionEventOnImplThread);
    m_finishAllRenderingCompletionEventOnImplThread = completion;

    m_schedulerOnImplThread->setNeedsForcedRedraw();
}

void CCThreadProxy::forceBeginFrameOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT0("cc", "CCThreadProxy::forceBeginFrameOnImplThread");
    ASSERT(!m_beginFrameCompletionEventOnImplThread);

    if (m_schedulerOnImplThread->commitPending()) {
        completion->signal();
        return;
    }

    m_beginFrameCompletionEventOnImplThread = completion;
    m_schedulerOnImplThread->setNeedsCommit();
    m_schedulerOnImplThread->setNeedsForcedCommit();
}

void CCThreadProxy::scheduledActionBeginFrame()
{
    TRACE_EVENT0("cc", "CCThreadProxy::scheduledActionBeginFrame");
    ASSERT(!m_pendingBeginFrameRequest);
    m_pendingBeginFrameRequest = adoptPtr(new BeginFrameAndCommitState());
    m_pendingBeginFrameRequest->monotonicFrameBeginTime = monotonicallyIncreasingTime();
    m_pendingBeginFrameRequest->scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
    m_currentTextureUpdaterOnImplThread = adoptPtr(new CCTextureUpdater);
    m_pendingBeginFrameRequest->updater = m_currentTextureUpdaterOnImplThread.get();

    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadProxy::beginFrame));

    if (m_beginFrameCompletionEventOnImplThread) {
        m_beginFrameCompletionEventOnImplThread->signal();
        m_beginFrameCompletionEventOnImplThread = 0;
    }
}

void CCThreadProxy::beginFrame()
{
    TRACE_EVENT0("cc", "CCThreadProxy::beginFrame");
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;

    if (!m_pendingBeginFrameRequest) {
        TRACE_EVENT0("cc", "EarlyOut_StaleBeginFrameMessage");
        return;
    }

    if (CCLayerTreeHost::needsFilterContext() && !SharedGraphicsContext3D::haveForImplThread())
        SharedGraphicsContext3D::createForImplThread();

    OwnPtr<BeginFrameAndCommitState> request(m_pendingBeginFrameRequest.release());

    // Do not notify the impl thread of commit requests that occur during
    // the apply/animate/layout part of the beginFrameAndCommit process since
    // those commit requests will get painted immediately. Once we have done
    // the paint, m_commitRequested will be set to false to allow new commit
    // requests to be scheduled.
    m_commitRequested = true;

    // On the other hand, the animationRequested flag needs to be cleared
    // here so that any animation requests generated by the apply or animate
    // callbacks will trigger another frame.
    m_animateRequested = false;

    // FIXME: technically, scroll deltas need to be applied for dropped commits as well.
    // Re-do the commit flow so that we don't send the scrollInfo on the BFAC message.
    m_layerTreeHost->applyScrollAndScale(*request->scrollInfo);

    m_layerTreeHost->willBeginFrame();

    // FIXME: recreate the context if it was requested by the impl thread.
    m_layerTreeHost->updateAnimations(request->monotonicFrameBeginTime);
    m_layerTreeHost->layout();

    // Clear the commit flag after updating animations and layout here --- objects that only
    // layout when painted will trigger another setNeedsCommit inside
    // updateLayers.
    m_commitRequested = false;

    if (!m_layerTreeHost->updateLayers(*request->updater))
        return;

    // Once single buffered layers are committed, they cannot be modified until
    // they are drawn by the impl thread.
    m_texturesAcquired = false;

    // Before applying scrolls and calling animate, we set m_animateRequested to false.
    // If it is true now, it means setNeedAnimate was called again. Call setNeedsCommit
    // now so that we get begin frame when this one is done.
    if (m_animateRequested)
        setNeedsCommit();

    // Notify the impl thread that the beginFrame has completed. This will
    // begin the commit process, which is blocking from the main thread's
    // point of view, but asynchronously performed on the impl thread,
    // coordinated by the CCScheduler.
    {
        TRACE_EVENT("commit", this, 0);
        CCCompletionEvent completion;
        CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::beginFrameCompleteOnImplThread, AllowCrossThreadAccess(&completion)));
        completion.wait();
    }

    m_layerTreeHost->commitComplete();
}

void CCThreadProxy::beginFrameCompleteOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::beginFrameCompleteOnImplThread", this, 0);
    ASSERT(!m_commitCompletionEventOnImplThread);
    ASSERT(isImplThread());
    ASSERT(m_schedulerOnImplThread);
    ASSERT(m_schedulerOnImplThread->commitPending());

    if (!m_layerTreeHostImpl) {
        completion->signal();
        return;
    }

    m_commitCompletionEventOnImplThread = completion;

    m_schedulerOnImplThread->beginFrameComplete();
}

bool CCThreadProxy::hasMoreResourceUpdates() const
{
    if (!m_currentTextureUpdaterOnImplThread)
        return false;
    return m_currentTextureUpdaterOnImplThread->hasMoreUpdates();
}

bool CCThreadProxy::canDraw()
{
    ASSERT(isImplThread());
    if (!m_layerTreeHostImpl)
        return false;
    return m_layerTreeHostImpl->canDraw();
}

void CCThreadProxy::scheduledActionUpdateMoreResources()
{
    TRACE_EVENT("CCThreadProxy::scheduledActionUpdateMoreResources", this, 0);
    ASSERT(m_currentTextureUpdaterOnImplThread);
    m_currentTextureUpdaterOnImplThread->update(m_layerTreeHostImpl->context(), m_layerTreeHostImpl->contentsTextureAllocator(), m_layerTreeHostImpl->layerRenderer()->textureCopier(), m_layerTreeHostImpl->layerRenderer()->textureUploader(), textureUpdatesPerFrame);
}

void CCThreadProxy::scheduledActionCommit()
{
    TRACE_EVENT("CCThreadProxy::scheduledActionCommit", this, 0);
    ASSERT(isImplThread());
    ASSERT(m_currentTextureUpdaterOnImplThread);
    ASSERT(!m_currentTextureUpdaterOnImplThread->hasMoreUpdates());
    ASSERT(m_commitCompletionEventOnImplThread);

    m_currentTextureUpdaterOnImplThread.clear();

    m_layerTreeHostImpl->beginCommit();

    m_layerTreeHost->beginCommitOnImplThread(m_layerTreeHostImpl.get());
    m_layerTreeHost->finishCommitOnImplThread(m_layerTreeHostImpl.get());

    m_layerTreeHostImpl->commitComplete();

    m_nextFrameIsNewlyCommittedFrameOnImplThread = true;

    m_commitCompletionEventOnImplThread->signal();
    m_commitCompletionEventOnImplThread = 0;
}

void CCThreadProxy::scheduledActionBeginContextRecreation()
{
    ASSERT(isImplThread());
    m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadProxy::beginContextRecreation));
}

CCScheduledActionDrawAndSwapResult CCThreadProxy::scheduledActionDrawAndSwapInternal(bool forcedDraw)
{
    TRACE_EVENT("CCThreadProxy::scheduledActionDrawAndSwap", this, 0);
    CCScheduledActionDrawAndSwapResult result;
    result.didDraw = false;
    result.didSwap = false;
    ASSERT(isImplThread());
    ASSERT(m_layerTreeHostImpl);
    if (!m_layerTreeHostImpl)
        return result;

    ASSERT(m_layerTreeHostImpl->layerRenderer());
    if (!m_layerTreeHostImpl->layerRenderer())
        return result;

    // FIXME: compute the frame display time more intelligently
    double monotonicTime = monotonicallyIncreasingTime();
    double wallClockTime = currentTime();

    m_inputHandlerOnImplThread->animate(monotonicTime);
    m_layerTreeHostImpl->animate(monotonicTime, wallClockTime);
    CCLayerTreeHostImpl::FrameData frame;
    bool drawFrame = m_layerTreeHostImpl->prepareToDraw(frame) || forcedDraw;
    if (drawFrame) {
        m_layerTreeHostImpl->drawLayers(frame);
        result.didDraw = true;
    }
    m_layerTreeHostImpl->didDrawAllLayers(frame);

    // Check for a pending compositeAndReadback.
    if (m_readbackRequestOnImplThread) {
        ASSERT(drawFrame); // This should be a forcedDraw
        m_layerTreeHostImpl->readback(m_readbackRequestOnImplThread->pixels, m_readbackRequestOnImplThread->rect);
        m_readbackRequestOnImplThread->success = !m_layerTreeHostImpl->isContextLost();
        m_readbackRequestOnImplThread->completion.signal();
        m_readbackRequestOnImplThread = 0;
    }

    if (drawFrame)
        result.didSwap = m_layerTreeHostImpl->swapBuffers();

    // Process any finish request
    if (m_finishAllRenderingCompletionEventOnImplThread) {
        ASSERT(drawFrame); // This should be a forcedDraw
        m_layerTreeHostImpl->finishAllRendering();
        m_finishAllRenderingCompletionEventOnImplThread->signal();
        m_finishAllRenderingCompletionEventOnImplThread = 0;
    }

    // Tell the main thread that the the newly-commited frame was drawn.
    if (m_nextFrameIsNewlyCommittedFrameOnImplThread) {
        m_nextFrameIsNewlyCommittedFrameOnImplThread = false;
        m_mainThreadProxy->postTask(createCCThreadTask(this, &CCThreadProxy::didCommitAndDrawFrame));
    }

    ASSERT(drawFrame || (!drawFrame && !forcedDraw));
    return result;
}

void CCThreadProxy::acquireLayerTextures()
{
    // Called when the main thread needs to modify a layer texture that is used
    // directly by the compositor.
    // This method will block until the next compositor draw if there is a
    // previously committed frame that is still undrawn. This is necessary to
    // ensure that the main thread does not monopolize access to the textures.
    ASSERT(isMainThread());

    if (m_texturesAcquired)
        return;

    TRACE_EVENT("CCThreadProxy::acquireLayerTextures", this, 0);
    CCCompletionEvent completion;
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::acquireLayerTexturesForMainThreadOnImplThread, AllowCrossThreadAccess(&completion)));
    completion.wait(); // Block until it is safe to write to layer textures from the main thread.

    m_texturesAcquired = true;
}

void CCThreadProxy::acquireLayerTexturesForMainThreadOnImplThread(CCCompletionEvent* completion)
{
    ASSERT(isImplThread());
    ASSERT(!m_textureAcquisitionCompletionEventOnImplThread);

    m_textureAcquisitionCompletionEventOnImplThread = completion;
    m_schedulerOnImplThread->setMainThreadNeedsLayerTextures();
}

void CCThreadProxy::scheduledActionAcquireLayerTexturesForMainThread()
{
    ASSERT(m_textureAcquisitionCompletionEventOnImplThread);
    m_textureAcquisitionCompletionEventOnImplThread->signal();
    m_textureAcquisitionCompletionEventOnImplThread = 0;
}

CCScheduledActionDrawAndSwapResult CCThreadProxy::scheduledActionDrawAndSwapIfPossible()
{
    return scheduledActionDrawAndSwapInternal(false);
}

CCScheduledActionDrawAndSwapResult CCThreadProxy::scheduledActionDrawAndSwapForced()
{
    return scheduledActionDrawAndSwapInternal(true);
}

void CCThreadProxy::didCommitAndDrawFrame()
{
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->didCommitAndDrawFrame();
}

void CCThreadProxy::didCompleteSwapBuffers()
{
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->didCompleteSwapBuffers();
}

void CCThreadProxy::setAnimationEvents(PassOwnPtr<CCAnimationEventsVector> events, double wallClockTime)
{
    TRACE_EVENT0("cc", "CCThreadProxy::setAnimationEvents");
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->setAnimationEvents(events, wallClockTime);
}

void CCThreadProxy::setContentsMemoryAllocationLimitBytes(size_t bytes)
{
    ASSERT(isMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->setContentsMemoryAllocationLimitBytes(bytes);
}

class CCThreadProxyContextRecreationTimer : public CCTimer, CCTimerClient {
public:
    static PassOwnPtr<CCThreadProxyContextRecreationTimer> create(CCThreadProxy* proxy) { return adoptPtr(new CCThreadProxyContextRecreationTimer(proxy)); }

    virtual void onTimerFired() OVERRIDE
    {
        m_proxy->tryToRecreateContext();
    }

private:
    explicit CCThreadProxyContextRecreationTimer(CCThreadProxy* proxy)
        : CCTimer(CCProxy::mainThread(), this)
        , m_proxy(proxy)
    {
    }

    CCThreadProxy* m_proxy;
};

void CCThreadProxy::beginContextRecreation()
{
    TRACE_EVENT0("cc", "CCThreadProxy::beginContextRecreation");
    ASSERT(isMainThread());
    ASSERT(!m_contextRecreationTimer);
    m_contextRecreationTimer = CCThreadProxyContextRecreationTimer::create(this);
    m_layerTreeHost->didLoseContext();
    m_contextRecreationTimer->startOneShot(contextRecreationTickRate);
}

void CCThreadProxy::tryToRecreateContext()
{
    ASSERT(isMainThread());
    ASSERT(m_layerTreeHost);
    CCLayerTreeHost::RecreateResult result = m_layerTreeHost->recreateContext();
    if (result == CCLayerTreeHost::RecreateFailedButTryAgain)
        m_contextRecreationTimer->startOneShot(contextRecreationTickRate);
    else if (result == CCLayerTreeHost::RecreateSucceeded)
        m_contextRecreationTimer.clear();
}

void CCThreadProxy::initializeImplOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::initializeImplOnImplThread", this, 0);
    ASSERT(isImplThread());
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
    const double displayRefreshInterval = 1.0 / 60.0;
    OwnPtr<CCFrameRateController> frameRateController = adoptPtr(new CCFrameRateController(CCDelayBasedTimeSource::create(displayRefreshInterval, CCProxy::implThread())));
    m_schedulerOnImplThread = CCScheduler::create(this, frameRateController.release());
    m_schedulerOnImplThread->setVisible(m_layerTreeHostImpl->visible());

    m_inputHandlerOnImplThread = CCInputHandler::create(m_layerTreeHostImpl.get());
    m_compositorIdentifier = m_inputHandlerOnImplThread->identifier();

    completion->signal();
}

void CCThreadProxy::initializeContextOnImplThread(GraphicsContext3D* context)
{
    TRACE_EVENT("CCThreadProxy::initializeContextOnImplThread", this, 0);
    ASSERT(isImplThread());
    m_contextBeforeInitializationOnImplThread = adoptRef(context);
}

void CCThreadProxy::initializeLayerRendererOnImplThread(CCCompletionEvent* completion, bool* initializeSucceeded, LayerRendererCapabilities* capabilities)
{
    TRACE_EVENT("CCThreadProxy::initializeLayerRendererOnImplThread", this, 0);
    ASSERT(isImplThread());
    ASSERT(m_contextBeforeInitializationOnImplThread);
    *initializeSucceeded = m_layerTreeHostImpl->initializeLayerRenderer(m_contextBeforeInitializationOnImplThread.release());
    if (*initializeSucceeded) {
        *capabilities = m_layerTreeHostImpl->layerRendererCapabilities();
        if (capabilities->usingSwapCompleteCallback)
            m_schedulerOnImplThread->setMaxFramesPending(2);
    }

    completion->signal();
}

void CCThreadProxy::layerTreeHostClosedOnImplThread(CCCompletionEvent* completion)
{
    TRACE_EVENT("CCThreadProxy::layerTreeHostClosedOnImplThread", this, 0);
    ASSERT(isImplThread());
    m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->contentsTextureAllocator());
    m_inputHandlerOnImplThread.clear();
    m_layerTreeHostImpl.clear();
    m_schedulerOnImplThread.clear();
    completion->signal();
}

void CCThreadProxy::setFullRootLayerDamageOnImplThread()
{
    ASSERT(isImplThread());
    m_layerTreeHostImpl->setFullRootLayerDamage();
}

size_t CCThreadProxy::maxPartialTextureUpdates() const
{
    return textureUpdatesPerFrame;
}

void CCThreadProxy::setFontAtlas(PassOwnPtr<CCFontAtlas> fontAtlas)
{
    ASSERT(isMainThread());
    CCProxy::implThread()->postTask(createCCThreadTask(this, &CCThreadProxy::setFontAtlasOnImplThread, fontAtlas));

}

void CCThreadProxy::setFontAtlasOnImplThread(PassOwnPtr<CCFontAtlas> fontAtlas)
{
    ASSERT(isImplThread());
    m_layerTreeHostImpl->setFontAtlas(fontAtlas);
}

void CCThreadProxy::recreateContextOnImplThread(CCCompletionEvent* completion, GraphicsContext3D* contextPtr, bool* recreateSucceeded, LayerRendererCapabilities* capabilities)
{
    TRACE_EVENT0("cc", "CCThreadProxy::recreateContextOnImplThread");
    ASSERT(isImplThread());
    m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->contentsTextureAllocator());
    *recreateSucceeded = m_layerTreeHostImpl->initializeLayerRenderer(adoptRef(contextPtr));
    if (*recreateSucceeded) {
        *capabilities = m_layerTreeHostImpl->layerRendererCapabilities();
        m_schedulerOnImplThread->didRecreateContext();
    }
    completion->signal();
}

} // namespace WebCore
