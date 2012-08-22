/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"

#include "CCTiledLayerTestCommon.h"

using namespace WebCore;

namespace WebKitTests {

FakeLayerTextureUpdater::Texture::Texture(FakeLayerTextureUpdater* layer, PassOwnPtr<CCPrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture)
    , m_layer(layer)
{
}

FakeLayerTextureUpdater::Texture::~Texture()
{
}

void FakeLayerTextureUpdater::Texture::updateRect(CCResourceProvider* resourceProvider, const IntRect&, const IntSize&)
{
    texture()->acquireBackingTexture(resourceProvider);
    m_layer->updateRect();
}

void FakeLayerTextureUpdater::Texture::prepareRect(const IntRect&, WebCore::CCRenderingStats&)
{
    m_layer->prepareRect();
}

FakeLayerTextureUpdater::FakeLayerTextureUpdater()
    : m_prepareCount(0)
    , m_updateCount(0)
    , m_prepareRectCount(0)
{
}

FakeLayerTextureUpdater::~FakeLayerTextureUpdater()
{
}

void FakeLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize&, float, float, IntRect& resultingOpaqueRect, CCRenderingStats&)
{
    m_prepareCount++;
    m_lastUpdateRect = contentRect;
    if (!m_rectToInvalidate.isEmpty()) {
        m_layer->invalidateContentRect(m_rectToInvalidate);
        m_rectToInvalidate = IntRect();
        m_layer = 0;
    }
    resultingOpaqueRect = m_opaquePaintRect;
}

void FakeLayerTextureUpdater::setRectToInvalidate(const IntRect& rect, FakeTiledLayerChromium* layer)
{
    m_rectToInvalidate = rect;
    m_layer = layer;
}

PassOwnPtr<LayerTextureUpdater::Texture> FakeLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
}

FakeCCTiledLayerImpl::FakeCCTiledLayerImpl(int id)
    : CCTiledLayerImpl(id)
{
}

FakeCCTiledLayerImpl::~FakeCCTiledLayerImpl()
{
}

FakeTiledLayerChromium::FakeTiledLayerChromium(CCPrioritizedTextureManager* textureManager)
    : TiledLayerChromium()
    , m_fakeTextureUpdater(adoptRef(new FakeLayerTextureUpdater))
    , m_textureManager(textureManager)
{
    setTileSize(tileSize());
    setTextureFormat(GraphicsContext3D::RGBA);
    setBorderTexelOption(CCLayerTilingData::NoBorderTexels);
    setIsDrawable(true); // So that we don't get false positives if any of these tests expect to return false from drawsContent() for other reasons.
}

FakeTiledLayerChromium::~FakeTiledLayerChromium()
{
}

void FakeTiledLayerChromium::setNeedsDisplayRect(const FloatRect& rect)
{
    m_lastNeedsDisplayRect = rect;
    TiledLayerChromium::setNeedsDisplayRect(rect);
}

void FakeTiledLayerChromium::setTexturePriorities(const CCPriorityCalculator& calculator)
{
    // Ensure there is always a target render surface available. If none has been
    // set (the layer is an orphan for the test), then just set a surface on itself.
    bool missingTargetRenderSurface = !renderTarget();

    if (missingTargetRenderSurface)
        createRenderSurface();

    TiledLayerChromium::setTexturePriorities(calculator);

    if (missingTargetRenderSurface) {
        clearRenderSurface();
        setRenderTarget(0);
    }
}

FakeTiledLayerWithScaledBounds::FakeTiledLayerWithScaledBounds(CCPrioritizedTextureManager* textureManager)
    : FakeTiledLayerChromium(textureManager)
{
}

} // namespace
