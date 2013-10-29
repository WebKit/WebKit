/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformCALayerRemoteTiledBacking_h
#define PlatformCALayerRemoteTiledBacking_h

#if USE(ACCELERATED_COMPOSITING)

#include "PlatformCALayerRemote.h"

namespace WebKit {

class PlatformCALayerRemoteTiledBacking FINAL : public PlatformCALayerRemote {
    friend class PlatformCALayerRemote;
public:
    virtual ~PlatformCALayerRemoteTiledBacking();

private:
    PlatformCALayerRemoteTiledBacking(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext*);

    virtual WebCore::TiledBacking* tiledBacking() OVERRIDE { return m_tileController.get(); }

    virtual void setNeedsDisplay(const WebCore::FloatRect* dirtyRect = 0) OVERRIDE;

    virtual const WebCore::PlatformCALayerList* customSublayers() const OVERRIDE;

    virtual void setBounds(const WebCore::FloatRect&) OVERRIDE;
    virtual bool isOpaque() const OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;
    virtual bool acceleratesDrawing() const OVERRIDE;
    virtual void setAcceleratesDrawing(bool) OVERRIDE;
    virtual void setContentsScale(float) OVERRIDE;
    virtual void setBorderWidth(float) OVERRIDE;
    virtual void setBorderColor(const WebCore::Color&) OVERRIDE;

    OwnPtr<WebCore::TileController> m_tileController;
    OwnPtr<WebCore::PlatformCALayerList> m_customSublayers;
};

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)

#endif // PlatformCALayerRemoteTiledBacking_h
