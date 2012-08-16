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

#ifndef CCTiledLayerTestCommon_h
#define CCTiledLayerTestCommon_h

#include "IntRect.h"
#include "IntSize.h"
#include "LayerTextureUpdater.h"
#include "Region.h"
#include "TextureCopier.h"
#include "TextureUploader.h"
#include "TiledLayerChromium.h"
#include "cc/CCGraphicsContext.h"
#include "cc/CCPrioritizedTexture.h"
#include "cc/CCResourceProvider.h"
#include "cc/CCTextureUpdateQueue.h"
#include "cc/CCTiledLayerImpl.h"

namespace WebKitTests {

class FakeTiledLayerChromium;

class FakeLayerTextureUpdater : public WebCore::LayerTextureUpdater {
public:
    class Texture : public WebCore::LayerTextureUpdater::Texture {
    public:
        Texture(FakeLayerTextureUpdater*, PassOwnPtr<WebCore::CCPrioritizedTexture>);
        virtual ~Texture();

        virtual void updateRect(WebCore::CCResourceProvider* , const WebCore::IntRect&, const WebCore::IntSize&) OVERRIDE;
        virtual void prepareRect(const WebCore::IntRect&, WebCore::CCRenderingStats&) OVERRIDE;

    private:
        FakeLayerTextureUpdater* m_layer;
    };

    FakeLayerTextureUpdater();
    virtual ~FakeLayerTextureUpdater();

    virtual PassOwnPtr<WebCore::LayerTextureUpdater::Texture> createTexture(WebCore::CCPrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum) OVERRIDE { return SampledTexelFormatRGBA; }

    virtual void prepareToUpdate(const WebCore::IntRect& contentRect, const WebCore::IntSize&, float, float, WebCore::IntRect& resultingOpaqueRect, WebCore::CCRenderingStats&) OVERRIDE;
    // Sets the rect to invalidate during the next call to prepareToUpdate(). After the next
    // call to prepareToUpdate() the rect is reset.
    void setRectToInvalidate(const WebCore::IntRect&, FakeTiledLayerChromium*);
    // Last rect passed to prepareToUpdate().
    const WebCore::IntRect& lastUpdateRect()  const { return m_lastUpdateRect; }

    // Number of times prepareToUpdate has been invoked.
    int prepareCount() const { return m_prepareCount; }
    void clearPrepareCount() { m_prepareCount = 0; }

    // Number of times updateRect has been invoked.
    int updateCount() const { return m_updateCount; }
    void clearUpdateCount() { m_updateCount = 0; }
    void updateRect() { m_updateCount++; }

    // Number of times prepareRect() has been invoked on a texture.
    int prepareRectCount() const { return m_prepareRectCount; }
    void clearPrepareRectCount() { m_prepareRectCount = 0; }
    void prepareRect() { m_prepareRectCount++; }

    void setOpaquePaintRect(const WebCore::IntRect& opaquePaintRect) { m_opaquePaintRect = opaquePaintRect; }

private:
    int m_prepareCount;
    int m_updateCount;
    int m_prepareRectCount;
    WebCore::IntRect m_rectToInvalidate;
    WebCore::IntRect m_lastUpdateRect;
    WebCore::IntRect m_opaquePaintRect;
    RefPtr<FakeTiledLayerChromium> m_layer;
};

class FakeCCTiledLayerImpl : public WebCore::CCTiledLayerImpl {
public:
    explicit FakeCCTiledLayerImpl(int id);
    virtual ~FakeCCTiledLayerImpl();

    using WebCore::CCTiledLayerImpl::hasTileAt;
    using WebCore::CCTiledLayerImpl::hasTextureIdForTileAt;
};

class FakeTiledLayerChromium : public WebCore::TiledLayerChromium {
public:
    explicit FakeTiledLayerChromium(WebCore::CCPrioritizedTextureManager*);
    virtual ~FakeTiledLayerChromium();

    static WebCore::IntSize tileSize() { return WebCore::IntSize(100, 100); }

    using WebCore::TiledLayerChromium::invalidateContentRect;
    using WebCore::TiledLayerChromium::updateContentRect;
    using WebCore::TiledLayerChromium::needsIdlePaint;
    using WebCore::TiledLayerChromium::skipsDraw;
    using WebCore::TiledLayerChromium::numPaintedTiles;
    using WebCore::TiledLayerChromium::idlePaintRect;

    virtual void setNeedsDisplayRect(const WebCore::FloatRect&) OVERRIDE;
    const WebCore::FloatRect& lastNeedsDisplayRect() const { return m_lastNeedsDisplayRect; }

    // Updates the visibleContentRect().
    virtual void update(WebCore::CCTextureUpdateQueue&, const WebCore::CCOcclusionTracker*, WebCore::CCRenderingStats&) OVERRIDE;

    virtual void setTexturePriorities(const WebCore::CCPriorityCalculator&) OVERRIDE;

    virtual WebCore::CCPrioritizedTextureManager* textureManager() const OVERRIDE { return m_textureManager; }
    FakeLayerTextureUpdater* fakeLayerTextureUpdater() { return m_fakeTextureUpdater.get(); }
    WebCore::FloatRect updateRect() { return m_updateRect; }

protected:
    virtual WebCore::LayerTextureUpdater* textureUpdater() const OVERRIDE { return m_fakeTextureUpdater.get(); }
    virtual void createTextureUpdaterIfNeeded() OVERRIDE { }

private:
    RefPtr<FakeLayerTextureUpdater> m_fakeTextureUpdater;
    WebCore::CCPrioritizedTextureManager* m_textureManager;
    WebCore::FloatRect m_lastNeedsDisplayRect;
};

class FakeTiledLayerWithScaledBounds : public FakeTiledLayerChromium {
public:
    explicit FakeTiledLayerWithScaledBounds(WebCore::CCPrioritizedTextureManager*);

    void setContentBounds(const WebCore::IntSize& contentBounds) { m_forcedContentBounds = contentBounds; }
    virtual WebCore::IntSize contentBounds() const OVERRIDE { return m_forcedContentBounds; }

protected:
    WebCore::IntSize m_forcedContentBounds;
};

class FakeTextureCopier : public WebCore::TextureCopier {
public:
    virtual void copyTexture(Parameters) { }
    virtual void flush() { }
};

class FakeTextureUploader : public WebCore::TextureUploader {
public:
    virtual bool isBusy() { return false; }
    virtual void beginUploads() { }
    virtual void endUploads() { }
    virtual void uploadTexture(WebCore::CCResourceProvider* resourceProvider, Parameters upload) { upload.texture->updateRect(resourceProvider, upload.sourceRect, upload.destOffset); }
};

}
#endif // CCTiledLayerTestCommon_h
