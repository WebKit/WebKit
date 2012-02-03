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

class CCThread;
class CCLayerTreeHost;
class CCLayerTreeHostImpl;
class CCLayerTreeHostImplClient;
class GraphicsContext3D;
struct LayerRendererCapabilities;
class TextureManager;

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

    virtual ~CCProxy();

    virtual bool compositeAndReadback(void *pixels, const IntRect&) = 0;

    virtual void finishAllRendering() = 0;

    virtual bool isStarted() const = 0;

    virtual bool initializeLayerRenderer() = 0;

    virtual int compositorIdentifier() const = 0;

    virtual const LayerRendererCapabilities& layerRendererCapabilities() const = 0;

    virtual void setNeedsAnimate() = 0;
    virtual void setNeedsCommit() = 0;
    virtual void setNeedsRedraw() = 0;
    virtual void setVisible(bool) = 0;

    virtual void start() = 0; // Must be called before using the proxy.
    virtual void stop() = 0; // Must be called before deleting the proxy.

    // Whether sub-regions of textures can be updated or if complete texture
    // updates are required.
    virtual bool partialTextureUpdateCapability() const = 0;

    // Debug hooks
#ifndef NDEBUG
    static bool isMainThread();
    static bool isImplThread();
#endif

    // Temporary hack while render_widget still does scheduling for CCLayerTreeHostMainThreadI
    virtual GraphicsContext3D* context() = 0;

    // Testing hooks
    virtual void loseCompositorContext(int numTimes) = 0;

#ifndef NDEBUG
    static void setCurrentThreadIsImplThread(bool);
#endif

protected:
    CCProxy();
    friend class DebugScopedSetImplThread;
};

}

#endif
