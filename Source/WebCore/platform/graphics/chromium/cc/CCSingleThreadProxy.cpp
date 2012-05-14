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

#include "cc/CCSingleThreadProxy.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "TraceEvent.h"
#include "cc/CCFontAtlas.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCTextureUpdater.h"
#include <wtf/CurrentTime.h>

using namespace WTF;

namespace WebCore {

PassOwnPtr<CCProxy> CCSingleThreadProxy::create(CCLayerTreeHost* layerTreeHost)
{
    return adoptPtr(new CCSingleThreadProxy(layerTreeHost));
}

CCSingleThreadProxy::CCSingleThreadProxy(CCLayerTreeHost* layerTreeHost)
    : m_layerTreeHost(layerTreeHost)
    , m_contextLost(false)
    , m_compositorIdentifier(-1)
    , m_layerRendererInitialized(false)
    , m_nextFrameIsNewlyCommittedFrame(false)
{
    TRACE_EVENT("CCSingleThreadProxy::CCSingleThreadProxy", this, 0);
    ASSERT(CCProxy::isMainThread());
}

void CCSingleThreadProxy::start()
{
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
}

CCSingleThreadProxy::~CCSingleThreadProxy()
{
    TRACE_EVENT("CCSingleThreadProxy::~CCSingleThreadProxy", this, 0);
    ASSERT(CCProxy::isMainThread());
    ASSERT(!m_layerTreeHostImpl && !m_layerTreeHost); // make sure stop() got called.
}

bool CCSingleThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    TRACE_EVENT("CCSingleThreadProxy::compositeAndReadback", this, 0);
    ASSERT(CCProxy::isMainThread());

    if (!commitAndComposite())
        return false;

    m_layerTreeHostImpl->readback(pixels, rect);

    if (m_layerTreeHostImpl->isContextLost())
        return false;

    m_layerTreeHostImpl->swapBuffers();
    didSwapFrame();

    return true;
}

void CCSingleThreadProxy::startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration)
{
    m_layerTreeHostImpl->startPageScaleAnimation(targetPosition, useAnchor, scale, monotonicallyIncreasingTime(), duration);
}

GraphicsContext3D* CCSingleThreadProxy::context()
{
    ASSERT(CCProxy::isMainThread());
    if (m_contextBeforeInitialization)
        return m_contextBeforeInitialization.get();
    DebugScopedSetImplThread impl;
    return m_layerTreeHostImpl->context();
}

void CCSingleThreadProxy::finishAllRendering()
{
    ASSERT(CCProxy::isMainThread());
    {
        DebugScopedSetImplThread impl;
        m_layerTreeHostImpl->finishAllRendering();
    }
}

bool CCSingleThreadProxy::isStarted() const
{
    ASSERT(CCProxy::isMainThread());
    return m_layerTreeHostImpl;
}

bool CCSingleThreadProxy::initializeContext()
{
    ASSERT(CCProxy::isMainThread());
    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createContext();
    if (!context)
        return false;
    ASSERT(context->hasOneRef());
    m_contextBeforeInitialization = context;
    return true;
}

void CCSingleThreadProxy::setSurfaceReady()
{
    // Scheduling is controlled by the embedder in the single thread case, so nothing to do.
}

bool CCSingleThreadProxy::initializeLayerRenderer()
{
    ASSERT(CCProxy::isMainThread());
    ASSERT(m_contextBeforeInitialization);
    {
        DebugScopedSetImplThread impl;
        bool ok = m_layerTreeHostImpl->initializeLayerRenderer(m_contextBeforeInitialization.release(), UnthrottledUploader);
        if (ok) {
            m_layerRendererInitialized = true;
            m_layerRendererCapabilitiesForMainThread = m_layerTreeHostImpl->layerRendererCapabilities();
        }
        return ok;
    }
}

bool CCSingleThreadProxy::recreateContext()
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::recreateContext");
    ASSERT(CCProxy::isMainThread());
    ASSERT(m_contextLost);

    RefPtr<GraphicsContext3D> context = m_layerTreeHost->createContext();
    if (!context)
        return false;

    ASSERT(context->hasOneRef());
    bool initialized;
    {
        DebugScopedSetImplThread impl;
        m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->contentsTextureAllocator());
        initialized = m_layerTreeHostImpl->initializeLayerRenderer(context, UnthrottledUploader);
        if (initialized) {
            m_layerRendererCapabilitiesForMainThread = m_layerTreeHostImpl->layerRendererCapabilities();
        }
    }

    if (initialized)
        m_contextLost = false;

    return initialized;
}

const LayerRendererCapabilities& CCSingleThreadProxy::layerRendererCapabilities() const
{
    ASSERT(m_layerRendererInitialized);
    // Note: this gets called during the commit by the "impl" thread
    return m_layerRendererCapabilitiesForMainThread;
}

void CCSingleThreadProxy::loseContext()
{
    ASSERT(CCProxy::isMainThread());
    m_layerTreeHost->didLoseContext();
    m_contextLost = true;
}

void CCSingleThreadProxy::setNeedsAnimate()
{
    // CCThread-only feature
    ASSERT_NOT_REACHED();
}

void CCSingleThreadProxy::doCommit(CCTextureUpdater& updater)
{
    ASSERT(CCProxy::isMainThread());
    // Commit immediately
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;

        m_layerTreeHostImpl->beginCommit();

        m_layerTreeHost->beginCommitOnImplThread(m_layerTreeHostImpl.get());

        // CCTextureUpdater is non-blocking and will return without updating
        // any textures if the uploader is busy. This shouldn't be a problem
        // here as the throttled uploader isn't used in single thread mode.
        // For correctness, loop until no more updates are pending.
        while (updater.hasMoreUpdates())
            updater.update(m_layerTreeHostImpl->context(), m_layerTreeHostImpl->contentsTextureAllocator(), m_layerTreeHostImpl->layerRenderer()->textureCopier(), m_layerTreeHostImpl->layerRenderer()->textureUploader(), maxPartialTextureUpdates());

        m_layerTreeHostImpl->setVisible(m_layerTreeHost->visible());
        m_layerTreeHost->finishCommitOnImplThread(m_layerTreeHostImpl.get());

        m_layerTreeHostImpl->commitComplete();

#if !ASSERT_DISABLED
        // In the single-threaded case, the scroll deltas should never be
        // touched on the impl layer tree.
        OwnPtr<CCScrollAndScaleSet> scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
        ASSERT(!scrollInfo->scrolls.size());
#endif
    }
    m_layerTreeHost->commitComplete();
    m_nextFrameIsNewlyCommittedFrame = true;
}

void CCSingleThreadProxy::setNeedsCommit()
{
    ASSERT(CCProxy::isMainThread());
    m_layerTreeHost->setNeedsCommit();
}

void CCSingleThreadProxy::setNeedsRedraw()
{
    // FIXME: Once we move render_widget scheduling into this class, we can
    // treat redraw requests more efficiently than commitAndRedraw requests.
    m_layerTreeHostImpl->setFullRootLayerDamage();
    setNeedsCommit();
}

bool CCSingleThreadProxy::commitRequested() const
{
    return false;
}

void CCSingleThreadProxy::setVisible(bool visible)
{
    m_layerTreeHostImpl->setVisible(visible);

    if (!visible) {
        DebugScopedSetImplThread impl;
        m_layerTreeHost->didBecomeInvisibleOnImplThread(m_layerTreeHostImpl.get());
        return;
    }

    setNeedsCommit();
}

void CCSingleThreadProxy::stop()
{
    TRACE_EVENT("CCSingleThreadProxy::stop", this, 0);
    ASSERT(CCProxy::isMainThread());
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;

        m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->contentsTextureAllocator());
        m_layerTreeHostImpl.clear();
    }
    m_layerTreeHost = 0;
}

void CCSingleThreadProxy::setFontAtlas(PassOwnPtr<CCFontAtlas> fontAtlas)
{
    ASSERT(isMainThread());
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl->setFontAtlas(fontAtlas);
}

void CCSingleThreadProxy::postAnimationEventsToMainThreadOnImplThread(PassOwnPtr<CCAnimationEventsVector> events, double wallClockTime)
{
    ASSERT(CCProxy::isImplThread());
    DebugScopedSetMainThread main;
    m_layerTreeHost->setAnimationEvents(events, wallClockTime);
}

void CCSingleThreadProxy::postSetContentsMemoryAllocationLimitBytesToMainThreadOnImplThread(size_t bytes)
{
    // FIXME: This is called via a graphics context callback, on main thread in single threaded mode, because its hard to fake the impl thread. This should need to DebugScopedSetMainThread, but is actually already on the main thread.
    ASSERT(CCProxy::isMainThread());
    ASSERT(m_layerTreeHost);
    m_layerTreeHost->setContentsMemoryAllocationLimitBytes(bytes);
}

// Called by the legacy scheduling path (e.g. where render_widget does the scheduling)
void CCSingleThreadProxy::compositeImmediately()
{
    if (commitAndComposite()) {
        m_layerTreeHostImpl->swapBuffers();
        didSwapFrame();
    }
}

void CCSingleThreadProxy::forceSerializeOnSwapBuffers()
{
    {
        DebugScopedSetImplThread impl;
        if (m_layerRendererInitialized)
            m_layerTreeHostImpl->layerRenderer()->doNoOp();
    }
}

bool CCSingleThreadProxy::commitAndComposite()
{
    ASSERT(CCProxy::isMainThread());

    CCTextureUpdater updater;

    if (!m_layerTreeHost->updateLayers(updater))
        return false;

    doCommit(updater);
    return doComposite();
}

bool CCSingleThreadProxy::doComposite()
{
    ASSERT(!m_contextLost);
    {
      DebugScopedSetImplThread impl;
      double monotonicTime = monotonicallyIncreasingTime();
      double wallClockTime = currentTime();
      m_layerTreeHostImpl->animate(monotonicTime, wallClockTime);

      // We guard prepareToDraw() with canDraw() because it always returns a valid frame, so can only
      // be used when such a frame is possible. Since drawLayers() depends on the result of
      // prepareToDraw(), it is guarded on canDraw() as well.
      if (!m_layerTreeHostImpl->canDraw())
          return false;

      CCLayerTreeHostImpl::FrameData frame;
      m_layerTreeHostImpl->prepareToDraw(frame);
      m_layerTreeHostImpl->drawLayers(frame);
      m_layerTreeHostImpl->didDrawAllLayers(frame);
    }

    if (m_layerTreeHostImpl->isContextLost()) {
        m_contextLost = true;
        m_layerTreeHost->didLoseContext();
        return false;
    }

    return true;
}

void CCSingleThreadProxy::didSwapFrame()
{
    if (m_nextFrameIsNewlyCommittedFrame) {
        m_nextFrameIsNewlyCommittedFrame = false;
        m_layerTreeHost->didCommitAndDrawFrame();
    }
}

}
