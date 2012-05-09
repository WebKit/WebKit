/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "RenderSurfaceChromium.h"

#include "FilterOperations.h"
#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "LayerRendererChromium.h"
#include <wtf/text/CString.h>

namespace WebCore {

RenderSurfaceChromium::RenderSurfaceChromium(LayerChromium* owningLayer)
    : m_owningLayer(owningLayer)
    , m_maskLayer(0)
    , m_skipsDraw(false)
    , m_drawOpacity(1)
    , m_drawOpacityIsAnimating(false)
    , m_targetSurfaceTransformsAreAnimating(false)
    , m_screenSpaceTransformsAreAnimating(false)
    , m_nearestAncestorThatMovesPixels(0)
{
}

RenderSurfaceChromium::~RenderSurfaceChromium()
{
}

FloatRect RenderSurfaceChromium::drawableContentRect() const
{
    FloatRect localContentRect(-0.5 * m_contentRect.width(), -0.5 * m_contentRect.height(),
                               m_contentRect.width(), m_contentRect.height());
    FloatRect drawableContentRect = m_drawTransform.mapRect(localContentRect);
    if (m_owningLayer->replicaLayer())
        drawableContentRect.unite(m_replicaDrawTransform.mapRect(localContentRect));

    return drawableContentRect;
}

bool RenderSurfaceChromium::hasReplica() const
{
    return m_owningLayer->replicaLayer();
}

bool RenderSurfaceChromium::hasMask() const
{
    return m_maskLayer;
}

bool RenderSurfaceChromium::replicaHasMask() const
{
    return hasReplica() && (m_maskLayer || m_owningLayer->replicaLayer()->maskLayer());
}

}
#endif // USE(ACCELERATED_COMPOSITING)
