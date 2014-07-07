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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformCALayerRemoteCustom_h
#define PlatformCALayerRemoteCustom_h

#include "PlatformCALayerRemote.h"

namespace WebKit {

class LayerHostingContext;

// PlatformCALayerRemoteCustom is used for CALayers that live in the web process and are hosted into the UI process via remote context.
class PlatformCALayerRemoteCustom final : public PlatformCALayerRemote {
    friend class PlatformCALayerRemote;
public:
    static PassRefPtr<PlatformCALayerRemote> create(PlatformLayer *, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);

    virtual ~PlatformCALayerRemoteCustom();

    virtual PlatformLayer* platformLayer() const override { return m_platformLayer.get(); }

    virtual uint32_t hostingContextID() override;

    virtual void setNeedsDisplay(const WebCore::FloatRect* dirtyRect = 0) override;

private:
    PlatformCALayerRemoteCustom(WebCore::PlatformCALayer::LayerType, PlatformLayer *, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext&);

    virtual PassRefPtr<WebCore::PlatformCALayer> clone(WebCore::PlatformCALayerClient* owner) const override;

    virtual bool isPlatformCALayerRemoteCustom() const { return true; }

    virtual CFTypeRef contents() const override;
    virtual void setContents(CFTypeRef) override;

    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    RetainPtr<PlatformLayer> m_platformLayer;
    bool m_providesContents;
};

PLATFORM_CALAYER_TYPE_CASTS(PlatformCALayerRemoteCustom, isPlatformCALayerRemote())

} // namespace WebKit

#endif // PlatformCALayerRemoteCustom_h
