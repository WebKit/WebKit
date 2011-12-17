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

#include "config.h"

#include "TiledLayerChromium.h"

#include "LayerTextureUpdater.h"
#include "TextureManager.h"
#include "cc/CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "cc/CCTextureUpdater.h"
#include "cc/CCTiledLayerImpl.h"
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WTF;

namespace {

class FakeTextureAllocator : public TextureAllocator {
public:
    virtual unsigned createTexture(const IntSize&, GC3Denum) { return 0; }
    virtual void deleteTexture(unsigned, const IntSize&, GC3Denum) { }
};

class FakeLayerTextureUpdater : public LayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(PassOwnPtr<ManagedTexture> texture) : LayerTextureUpdater::Texture(texture) { }
        virtual ~Texture() { }

        virtual void updateRect(GraphicsContext3D*, TextureAllocator*, const IntRect&, const IntRect&) { }
    };

    FakeLayerTextureUpdater() { }
    virtual ~FakeLayerTextureUpdater() { }

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(TextureManager* manager) { return adoptPtr(new Texture(ManagedTexture::create(manager))); }
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum) { return SampledTexelFormatRGBA; }
    virtual void prepareToUpdate(const IntRect&, const IntSize&, int, float) { }
};

class FakeCCTiledLayerImpl : public CCTiledLayerImpl {
public:
    explicit FakeCCTiledLayerImpl(int id)
        : CCTiledLayerImpl(id) { }
    virtual ~FakeCCTiledLayerImpl() { }

    bool hasTileAt(int i, int j)
    {
        return CCTiledLayerImpl::hasTileAt(i, j);
    }
};

class FakeTiledLayerChromium : public TiledLayerChromium {
public:
    explicit FakeTiledLayerChromium(TextureManager* textureManager)
        : TiledLayerChromium(0)
        , m_fakeTextureUpdater(adoptRef(new FakeLayerTextureUpdater))
        , m_textureManager(textureManager)
    {
        setTileSize(IntSize(100, 100));
        setTextureFormat(GraphicsContext3D::RGBA);
        createTiler(CCLayerTilingData::NoBorderTexels);
    }
    virtual ~FakeTiledLayerChromium() { }

    void invalidateRect(const IntRect& rect)
    {
        TiledLayerChromium::invalidateRect(rect);
    }

    void prepareToUpdate(const IntRect& rect)
    {
        TiledLayerChromium::prepareToUpdate(rect);
    }

    void prepareToUpdateIdle(const IntRect& rect)
    {
        TiledLayerChromium::prepareToUpdateIdle(rect);
    }

    bool needsIdlePaint(const IntRect& rect)
    {
        return TiledLayerChromium::needsIdlePaint(rect);
    }

    virtual TextureManager* textureManager() const { return m_textureManager; }

private:
    virtual void createTextureUpdater(const CCLayerTreeHost*) { }

    virtual LayerTextureUpdater* textureUpdater() const
    {
        return m_fakeTextureUpdater.get();
    }

    RefPtr<FakeLayerTextureUpdater> m_fakeTextureUpdater;
    TextureManager* m_textureManager;
};

TEST(TiledLayerChromiumTest, pushDirtyTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    RefPtr<FakeCCTiledLayerImpl> layerImpl = adoptRef(new FakeCCTiledLayerImpl(0));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->prepareToUpdate(IntRect(0, 0, 100, 200));
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    // Invalidates both tiles...
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    // ....but then only update one of them.
    layer->prepareToUpdate(IntRect(0, 0, 100, 100));
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should only have the first tile since the other tile was invalidated but not painted.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
}

TEST(TiledLayerChromiumTest, pushIdlePaintTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    RefPtr<FakeCCTiledLayerImpl> layerImpl = adoptRef(new FakeCCTiledLayerImpl(0));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100. Setup 5x5 tiles with one visible tile in the center.
    IntSize contentBounds(500, 500);
    IntRect contentRect(IntPoint::zero(), contentBounds);
    IntRect visibleRect(200, 200, 100, 100);

    // This invalidates 25 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);
    layer->prepareToUpdate(visibleRect);

    // We should need idle-painting for 3x3 tiles in the center.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(2, 2));

    textureManager->unprotectAllTextures();

    // For the next four updates, we should detect we still need idle painting.
    for (int i = 0; i < 4; i++) {
        layer->prepareToUpdate(visibleRect);
        EXPECT_TRUE(layer->needsIdlePaint(visibleRect));
        layer->prepareToUpdateIdle(visibleRect);
        layer->updateCompositorResources(0, updater);
        layer->pushPropertiesTo(layerImpl.get());
        textureManager->unprotectAllTextures();
    }

    // After four passes of idle painting, we should be finished painting
    EXPECT_FALSE(layer->needsIdlePaint(visibleRect));

    // We should have one tile surrounding the visible tile on all sides, but no other tiles.
    IntRect idlePaintTiles(1, 1, 3, 3);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (idlePaintTiles.contains(i, j))
                EXPECT_TRUE(layerImpl->hasTileAt(i, j));
            else
                EXPECT_FALSE(layerImpl->hasTileAt(i, j));
        }
    }
}


TEST(TiledLayerChromiumTest, idlePaintOutOfMemory)
{
    // The tile size is 100x100. Setup 5x5 tiles with one 1x1 visible tile in the center.
    IntSize contentBounds(300, 300);
    IntRect contentRect(IntPoint::zero(), contentBounds);
    IntRect visibleRect(100, 100, 100, 100);

    // We have enough memory for only the visible rect, so we will run out of memory in first idle paint.
    int memoryLimit = 4 * 100 * 100; // 2 tiles, 4 bytes per pixel.

    OwnPtr<TextureManager> textureManager = TextureManager::create(memoryLimit, memoryLimit / 2, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    RefPtr<FakeCCTiledLayerImpl> layerImpl = adoptRef(new FakeCCTiledLayerImpl(0));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // This invalidates 9 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);
    layer->prepareToUpdate(visibleRect);

    // We should need idle-painting for 3x3 tiles surounding visible tile.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    textureManager->unprotectAllTextures();
    layer->prepareToUpdate(visibleRect);
    layer->prepareToUpdateIdle(visibleRect);

    // We shouldn't signal we need another idle paint after we run out of memory.
    EXPECT_FALSE(layer->needsIdlePaint(visibleRect));

    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());
}

} // namespace

