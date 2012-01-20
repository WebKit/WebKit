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

#include "PluginLayerChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCPluginLayerImpl.h"

namespace WebCore {

PassRefPtr<PluginLayerChromium> PluginLayerChromium::create()
{
    return adoptRef(new PluginLayerChromium);
}

PluginLayerChromium::PluginLayerChromium()
    : LayerChromium()
    , m_textureId(0)
    , m_flipped(true)
    , m_uvRect(0, 0, 1, 1)
    , m_ioSurfaceWidth(0)
    , m_ioSurfaceHeight(0)
    , m_ioSurfaceId(0)
{
}

void PluginLayerChromium::updateCompositorResources(GraphicsContext3D* rendererContext, CCTextureUpdater&)
{
    if (!m_needsDisplay)
        return;

    // PluginLayers are updated externally (outside of the compositor).
    // |m_dirtyRect| covers the region that has changed since the last composite.
    m_updateRect = m_dirtyRect;
    m_dirtyRect = FloatRect();
}

PassRefPtr<CCLayerImpl> PluginLayerChromium::createCCLayerImpl()
{
    return CCPluginLayerImpl::create(m_layerId);
}

void PluginLayerChromium::setTextureId(unsigned id)
{
    m_textureId = id;
    setNeedsCommit();
}

void PluginLayerChromium::setFlipped(bool flipped)
{
    m_flipped = flipped;
    setNeedsCommit();
}

void PluginLayerChromium::setUVRect(const FloatRect& rect)
{
    m_uvRect = rect;
    setNeedsCommit();
}

void PluginLayerChromium::setIOSurfaceProperties(int width, int height, uint32_t ioSurfaceId)
{
    m_ioSurfaceWidth = width;
    m_ioSurfaceHeight = height;
    m_ioSurfaceId = ioSurfaceId;
    setNeedsCommit();
}

uint32_t PluginLayerChromium::getIOSurfaceId() const
{
    return m_ioSurfaceId;
}

void PluginLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCPluginLayerImpl* pluginLayer = static_cast<CCPluginLayerImpl*>(layer);
    pluginLayer->setTextureId(m_textureId);
    pluginLayer->setFlipped(m_flipped);
    pluginLayer->setUVRect(m_uvRect);
    pluginLayer->setIOSurfaceProperties(m_ioSurfaceWidth, m_ioSurfaceHeight, m_ioSurfaceId);
}

void PluginLayerChromium::invalidateRect(const FloatRect& dirtyRect)
{
    setNeedsDisplayRect(dirtyRect);
    m_dirtyRect.unite(dirtyRect);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
