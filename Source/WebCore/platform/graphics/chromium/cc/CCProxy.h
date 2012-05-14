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

#ifndef CCProxy_h
#define CCProxy_h

#include "IntRect.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

class CCFontAtlas;
class CCThread;
class GraphicsContext3D;
struct LayerRendererCapabilities;

// Abstract class responsible for proxying commands from the main-thread side of
// the compositor over to the compositor implementation.
class CCProxy {
    WTF_MAKE_NONCOPYABLE(CCProxy);
public:
    static void setMainThread(CCThread*);
    static CCThread* mainThread();

    static bool hasImplThread();
    static void setImplThread(CCThread*);
    static CCThread* implThread();

    // Returns 0 if the current thread is neither the main thread nor the impl thread.
    static CCThread* currentThread();

    virtual ~CCProxy();

    virtual bool compositeAndReadback(void *pixels, const IntRect&) = 0;

    virtual void startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double durationSec) = 0;

    virtual void finishAllRendering() = 0;

    virtual bool isStarted() const = 0;

    // Attempts to initialize a context to use for rendering. Returns false if the context could not be created.
    // The context will not be used and no frames may be produced until initializeLayerRenderer() is called.
    virtual bool initializeContext() = 0;

    // Indicates that the compositing surface associated with our context is ready to use.
    virtual void setSurfaceReady() = 0;

    // Attempts to initialize the layer renderer. Returns false if the context isn't usable for compositing.
    virtual bool initializeLayerRenderer() = 0;

    // Attempts to recreate the context and layer renderer after a context lost. Returns false if the renderer couldn't be
    // reinitialized.
    virtual bool recreateContext() = 0;

    virtual int compositorIdentifier() const = 0;

    virtual const LayerRendererCapabilities& layerRendererCapabilities() const = 0;

    virtual void setNeedsAnimate() = 0;
    virtual void setNeedsCommit() = 0;
    virtual void setNeedsRedraw() = 0;
    virtual void setVisible(bool) = 0;

    virtual bool commitRequested() const = 0;

    virtual void start() = 0; // Must be called before using the proxy.
    virtual void stop() = 0; // Must be called before deleting the proxy.

    // Forces 3D commands on all contexts to wait for all previous SwapBuffers to finish before executing in the GPU
    // process.
    virtual void forceSerializeOnSwapBuffers() = 0;

    // Maximum number of sub-region texture updates supported for each commit.
    virtual size_t maxPartialTextureUpdates() const = 0;

    virtual void acquireLayerTextures() = 0;

    virtual void setFontAtlas(PassOwnPtr<CCFontAtlas>) = 0;

    // Debug hooks
#ifndef NDEBUG
    static bool isMainThread();
    static bool isImplThread();
    static bool isMainThreadBlocked();
    static void setMainThreadBlocked(bool);
#endif

    // Temporary hack while render_widget still does scheduling for CCLayerTreeHostMainThreadI
    virtual GraphicsContext3D* context() = 0;

    // Testing hooks
    virtual void loseContext() = 0;

#ifndef NDEBUG
    static void setCurrentThreadIsImplThread(bool);
#endif

protected:
    CCProxy();
    friend class DebugScopedSetImplThread;
    friend class DebugScopedSetMainThreadBlocked;
};

class DebugScopedSetMainThreadBlocked {
public:
    DebugScopedSetMainThreadBlocked()
    {
#if !ASSERT_DISABLED
        ASSERT(!CCProxy::isMainThreadBlocked());
        CCProxy::setMainThreadBlocked(true);
#endif
    }
    ~DebugScopedSetMainThreadBlocked()
    {
#if !ASSERT_DISABLED
        ASSERT(CCProxy::isMainThreadBlocked());
        CCProxy::setMainThreadBlocked(false);
#endif
    }
};

}

#endif
