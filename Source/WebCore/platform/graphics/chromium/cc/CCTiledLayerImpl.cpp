/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCTiledLayerImpl.h"

#include "LayerRendererChromium.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

class ManagedTexture;

CCTiledLayerImpl::CCTiledLayerImpl(int id)
    : CCLayerImpl(id)
    , m_tiler(0)
{
}

CCTiledLayerImpl::~CCTiledLayerImpl()
{
}

void CCTiledLayerImpl::draw()
{
    const IntRect& layerRect = visibleLayerRect();
    if (!layerRect.isEmpty()) {
        GraphicsContext3D* context = layerRenderer()->context();
        // Mask out writes to the alpha channel for the root layer, subpixel antialiasing
        // via Skia results in invalid zero alpha values on text glyphs. The root layer
        // is always opaque so the alpha channel isn't meaningful anyway.
        if (isRootLayer())
            context->colorMask(true, true, true, false);
        m_tiler->draw(layerRect, m_tilingTransform, drawOpacity());
        if (isRootLayer())
            context->colorMask(true, true, true, true);
    }
}

void CCTiledLayerImpl::bindContentsTexture()
{
    // This function is only valid for single texture layers, e.g. masks.
    ASSERT(m_tiler);

    ManagedTexture* texture = m_tiler->getSingleTexture();
    ASSERT(texture);

    texture->bindTexture(layerRenderer()->context());
}

void CCTiledLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

}

#endif // USE(ACCELERATED_COMPOSITING)
