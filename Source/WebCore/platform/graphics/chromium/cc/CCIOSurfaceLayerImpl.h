/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCIOSurfaceLayerImpl_h
#define CCIOSurfaceLayerImpl_h

#include "IntSize.h"
#include "cc/CCLayerImpl.h"

namespace WebCore {

class CCIOSurfaceLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCIOSurfaceLayerImpl> create(int id)
    {
        return adoptPtr(new CCIOSurfaceLayerImpl(id));
    }
    virtual ~CCIOSurfaceLayerImpl();

    void setIOSurfaceProperties(unsigned ioSurfaceId, const IntSize&);

    virtual void appendQuads(CCQuadCuller&, const CCSharedQuadState*, bool& hadMissingTiles) OVERRIDE;

    virtual void willDraw(LayerRendererChromium*) OVERRIDE;
    virtual void didLoseContext() OVERRIDE;

    virtual void dumpLayerProperties(TextStream&, int indent) const OVERRIDE;

private:
    explicit CCIOSurfaceLayerImpl(int);

    virtual const char* layerTypeAsString() const OVERRIDE { return "IOSurfaceLayer"; }

    unsigned m_ioSurfaceId;
    IntSize m_ioSurfaceSize;
    bool m_ioSurfaceChanged;
    unsigned m_ioSurfaceTextureId;
};

}

#endif // CCIOSurfaceLayerImpl_h
