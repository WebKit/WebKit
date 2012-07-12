/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ImageLayerChromium.h"

#include "LayerTextureSubImage.h"
#include "LayerTextureUpdater.h"
#include "PlatformColor.h"
#include "cc/CCLayerTreeHost.h"

namespace WebCore {

class ImageLayerTextureUpdater : public LayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(ImageLayerTextureUpdater* textureUpdater, PassOwnPtr<CCPrioritizedTexture> texture)
            : LayerTextureUpdater::Texture(texture)
            , m_textureUpdater(textureUpdater)
        {
        }

        virtual void updateRect(CCGraphicsContext* context, TextureAllocator* allocator, const IntRect& sourceRect, const IntRect& destRect) OVERRIDE
        {
            textureUpdater()->updateTextureRect(context, allocator, texture(), sourceRect, destRect);
        }

    private:
        ImageLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        ImageLayerTextureUpdater* m_textureUpdater;
    };

    static PassRefPtr<ImageLayerTextureUpdater> create(bool useMapTexSubImage)
    {
        return adoptRef(new ImageLayerTextureUpdater(useMapTexSubImage));
    }

    virtual ~ImageLayerTextureUpdater() { }

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(CCPrioritizedTextureManager* manager)
    {
        return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
    }

    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat) OVERRIDE
    {
        return PlatformColor::sameComponentOrder(textureFormat) ?
                LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
    }

    void updateTextureRect(CCGraphicsContext* context, TextureAllocator* allocator, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntRect& destRect)
    {
        texture->bindTexture(context, allocator);

        // Source rect should never go outside the image pixels, even if this
        // is requested because the texture extends outside the image.
        IntRect clippedSourceRect = sourceRect;
        IntRect imageRect = IntRect(0, 0, m_bitmap.width(), m_bitmap.height());
        clippedSourceRect.intersect(imageRect);

        IntRect clippedDestRect = destRect;
        clippedDestRect.move(clippedSourceRect.location() - sourceRect.location());
        clippedDestRect.setSize(clippedSourceRect.size());

        SkAutoLockPixels lock(m_bitmap);
        m_texSubImage.upload(static_cast<const uint8_t*>(m_bitmap.getPixels()), imageRect, clippedSourceRect, clippedDestRect, texture->format(), context);
    }

    void setBitmap(const SkBitmap& bitmap)
    {
        m_bitmap = bitmap;
    }

private:
    explicit ImageLayerTextureUpdater(bool useMapTexSubImage)
        : m_texSubImage(useMapTexSubImage)
    {
    }

    SkBitmap m_bitmap;
    LayerTextureSubImage m_texSubImage;
};

PassRefPtr<ImageLayerChromium> ImageLayerChromium::create()
{
    return adoptRef(new ImageLayerChromium());
}

ImageLayerChromium::ImageLayerChromium()
    : TiledLayerChromium()
{
}

ImageLayerChromium::~ImageLayerChromium()
{
}

void ImageLayerChromium::setBitmap(const SkBitmap& bitmap)
{
    // setBitmap() currently gets called whenever there is any
    // style change that affects the layer even if that change doesn't
    // affect the actual contents of the image (e.g. a CSS animation).
    // With this check in place we avoid unecessary texture uploads.
    if (bitmap.pixelRef() && bitmap.pixelRef() == m_bitmap.pixelRef())
        return;

    m_bitmap = bitmap;
    setNeedsDisplay();
}

void ImageLayerChromium::setTexturePriorities(const CCPriorityCalculator& priorityCalc)
{
    // Update the tile data before creating all the layer's tiles.
    updateTileSizeAndTilingOption();

    TiledLayerChromium::setTexturePriorities(priorityCalc);
}

void ImageLayerChromium::update(CCTextureUpdater& updater, const CCOcclusionTracker* occlusion)
{
    createTextureUpdaterIfNeeded();
    if (m_needsDisplay) {
        m_textureUpdater->setBitmap(m_bitmap);
        updateTileSizeAndTilingOption();
        invalidateContentRect(IntRect(IntPoint(), contentBounds()));
        m_needsDisplay = false;
    }

    updateContentRect(updater, visibleContentRect(), occlusion);
}

void ImageLayerChromium::createTextureUpdaterIfNeeded()
{
    if (m_textureUpdater)
        return;

    m_textureUpdater = ImageLayerTextureUpdater::create(layerTreeHost()->layerRendererCapabilities().usingMapSub);
    GC3Denum textureFormat = layerTreeHost()->layerRendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
    setSampledTexelFormat(textureUpdater()->sampledTexelFormat(textureFormat));
}

LayerTextureUpdater* ImageLayerChromium::textureUpdater() const
{
    return m_textureUpdater.get();
}

IntSize ImageLayerChromium::contentBounds() const
{
    return IntSize(m_bitmap.width(), m_bitmap.height());
}

bool ImageLayerChromium::drawsContent() const
{
    return !m_bitmap.isNull() && TiledLayerChromium::drawsContent();
}

bool ImageLayerChromium::needsContentsScale() const
{
    // Contents scale is not need for image layer because this can be done in compositor more efficiently.
    return false;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
