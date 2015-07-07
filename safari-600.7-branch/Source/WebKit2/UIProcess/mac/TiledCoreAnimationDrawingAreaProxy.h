/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TiledCoreAnimationDrawingAreaProxy_h
#define TiledCoreAnimationDrawingAreaProxy_h

#if !PLATFORM(IOS)

#include "DrawingAreaProxy.h"

namespace WebKit {

class TiledCoreAnimationDrawingAreaProxy : public DrawingAreaProxy {
public:
    explicit TiledCoreAnimationDrawingAreaProxy(WebPageProxy*);
    virtual ~TiledCoreAnimationDrawingAreaProxy();

private:
    // DrawingAreaProxy
    virtual void deviceScaleFactorDidChange() override;
    virtual void sizeDidChange() override;
    virtual void waitForPossibleGeometryUpdate(std::chrono::milliseconds timeout = didUpdateBackingStoreStateTimeout()) override;
    virtual void colorSpaceDidChange() override;
    virtual void minimumLayoutSizeDidChange() override;

    virtual void enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) override;
    virtual void exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo&) override;
    virtual void updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) override;

    virtual void adjustTransientZoom(double scale, WebCore::FloatPoint origin) override;
    virtual void commitTransientZoom(double scale, WebCore::FloatPoint origin) override;

    virtual void waitForDidUpdateViewState() override;

    // Message handlers.
    virtual void didUpdateGeometry() override;
    virtual void intrinsicContentSizeDidChange(const WebCore::IntSize& newIntrinsicContentSize) override;

    void sendUpdateGeometry();

    // Whether we're waiting for a DidUpdateGeometry message from the web process.
    bool m_isWaitingForDidUpdateGeometry;

    // The last size we sent to the web process.
    WebCore::IntSize m_lastSentSize;
    WebCore::IntSize m_lastSentLayerPosition;

    // The last minimum layout size we sent to the web process.
    WebCore::IntSize m_lastSentMinimumLayoutSize;
};

DRAWING_AREA_PROXY_TYPE_CASTS(TiledCoreAnimationDrawingAreaProxy, type() == DrawingAreaTypeTiledCoreAnimation);

} // namespace WebKit

#endif // !PLATFORM(IOS)

#endif // TiledCoreAnimationDrawingAreaProxy_h
