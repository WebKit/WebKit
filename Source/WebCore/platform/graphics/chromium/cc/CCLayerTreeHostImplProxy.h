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

#ifndef CCLayerTreeHostImplProxy_h
#define CCLayerTreeHostImplProxy_h

#include "cc/CCCompletionEvent.h"
#include "cc/CCLayerTreeHostImpl.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class CCLayerTreeHost;
class CCLayerTreeHostCommitter;

class CCLayerTreeHostImplProxy : public CCLayerTreeHostImplClient {
    WTF_MAKE_NONCOPYABLE(CCLayerTreeHostImplProxy);
public:
    virtual ~CCLayerTreeHostImplProxy();

    bool isStarted() const;

    void setNeedsCommitAndRedraw();
    void setNeedsRedraw();

    void start(); // Must be called before using the proxy.
    void stop(); // Must be called before deleting the proxy.

    // CCLayerTreeHostImplCient -- called on CCThread
    virtual void postDrawLayersTaskOnCCThread();
    virtual void requestFrameAndCommitOnCCThread(double frameBeginTime);

#ifndef NDEBUG
    static bool isMainThread();
    static bool isImplThread();
#if !USE(THREADED_COMPOSITING)
    // Fake threaded compositing so we can catch incorrect usage.
    static void setImplThread(bool);
#endif
#endif

protected:
    explicit CCLayerTreeHostImplProxy(CCLayerTreeHost*);
    virtual PassOwnPtr<CCLayerTreeHostImpl> createLayerTreeHostImpl() = 0;
    CCLayerTreeHost* host() const { return m_layerTreeHost; }

private:
    // Called on CCMainThread
    void requestFrameAndCommit(double frameBeginTime);

    // Called on CCThread
    void commitOnCCThread(CCLayerTreeHostCommitter*, CCCompletionEvent*);
    void drawLayersOnCCThread();
    void initImplOnCCThread(CCCompletionEvent*);
    void setNeedsCommitAndRedrawOnCCThread();
    void setNeedsRedrawOnCCThread();
    void layerTreeHostClosedOnCCThread(CCCompletionEvent*);

    // Used on main-thread only.
    bool m_commitPending;

    // Accessed on main thread only.
    CCLayerTreeHost* m_layerTreeHost;

    // Used on the CCThread, but checked on main thread during initialization/shutdown.
    OwnPtr<CCLayerTreeHostImpl> m_layerTreeHostImpl;
};

}

#endif
