/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WKCACFViewLayerTreeHost_h
#define WKCACFViewLayerTreeHost_h

#include "CACFLayerTreeHost.h"

typedef struct _WKCACFView* WKCACFViewRef;

namespace WebCore {

class WKCACFViewLayerTreeHost : public CACFLayerTreeHost {
public:
    static RefPtr<WKCACFViewLayerTreeHost> create();

    virtual bool createRenderer();

private:
    WKCACFViewLayerTreeHost();

    void updateViewIfNeeded();
    static void contextDidChangeCallback(WKCACFViewRef, void* info);

    virtual void initializeContext(void* userData, PlatformCALayer*);
    virtual void resize();
    virtual void setScaleFactor(float);
    virtual void destroyRenderer();
    virtual void flushContext();
    virtual void contextDidChange();
    void paint(HDC = nullptr) override;
    void render(const Vector<CGRect>& dirtyRects = Vector<CGRect>(), HDC dc = nullptr) override;
    virtual CFTimeInterval lastCommitTime() const;
    virtual void setShouldInvertColors(bool);
#if USE(AVFOUNDATION)
    GraphicsDeviceAdapter* graphicsDeviceAdapter() const override;
#endif

    RetainPtr<WKCACFViewRef> m_view;
    bool m_viewNeedsUpdate;
};

} // namespace WebCore

#endif // WKCACFViewLayerTreeHost_h
