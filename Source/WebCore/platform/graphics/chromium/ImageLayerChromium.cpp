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

#include "Image.h"
#include "LayerTextureSubImage.h"
#include "LayerTextureUpdater.h"
#include "ManagedTexture.h"
#include "PlatformColor.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHost.h"

namespace WebCore {

class ImageLayerTextureUpdater : public LayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(ImageLayerTextureUpdater* textureUpdater, PassOwnPtr<ManagedTexture> texture)
            : LayerTextureUpdater::Texture(texture)
            , m_textureUpdater(textureUpdater)
        {
        }

        virtual void updateRect(GraphicsContext3D* context, TextureAllocator* allocator, const IntRect& sourceRect, const IntRect& destRect)
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

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(TextureManager* manager)
    {
        return adoptPtr(new Texture(this, ManagedTexture::create(manager)));
    }

    virtual Orientation orientation() { return LayerTextureUpdater::BottomUpOrientation; }

    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat)
    {
        return PlatformColor::sameComponentOrder(textureFormat) ?
                LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
    }

    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int /* borderTexels */, float /* contentsScale */)
    {
        m_texSubImage.setSubImageSize(tileSize);
    }

    virtual void updateTextureRect(GraphicsContext3D* context, TextureAllocator* allocator, ManagedTexture* texture, const IntRect& sourceRect, const IntRect& destRect)
    {
        texture->bindTexture(context, allocator);

        // Source rect should never go outside the image pixels, even if this
        // is requested because the texture extends outside the image.
        IntRect clippedSourceRect = sourceRect;
        clippedSourceRect.intersect(imageRect());

        IntRect clippedDestRect = destRect;
        clippedDestRect.move(clippedSourceRect.location() - sourceRect.location());
        clippedDestRect.setSize(clippedSourceRect.size());

        m_texSubImage.upload(m_image.pixels(), imageRect(), clippedSourceRect, clippedDestRect, texture->format(), context);
    }

    void updateFromImage(NativeImagePtr nativeImage)
    {
        m_image.updateFromImage(nativeImage);
    }

    IntSize imageSize() const
    {
        return m_image.size();
    }

private:
    explicit ImageLayerTextureUpdater(bool useMapTexSubImage)
        : m_texSubImage(useMapTexSubImage)
    {
    }

    IntRect imageRect() const
    {
        return IntRect(IntPoint::zero(), m_image.size());
    }

    PlatformImage m_image;
    LayerTextureSubImage m_texSubImage;
};

PassRefPtr<ImageLayerChromium> ImageLayerChromium::create(CCLayerDelegate* delegate)
{
    return adoptRef(new ImageLayerChromium(delegate));
}

ImageLayerChromium::ImageLayerChromium(CCLayerDelegate* delegate)
    : TiledLayerChromium(delegate)
    , m_imageForCurrentFrame(0)
{
}

ImageLayerChromium::~ImageLayerChromium()
{
}

void ImageLayerChromium::cleanupResources()
{
    m_textureUpdater.clear();
    TiledLayerChromium::cleanupResources();
}

void ImageLayerChromium::setContents(Image* contents)
{
    // setContents() currently gets called whenever there is any
    // style change that affects the layer even if that change doesn't
    // affect the actual contents of the image (e.g. a CSS animation).
    // With this check in place we avoid unecessary texture uploads.
    if ((m_contents == contents) && (m_contents->nativeImageForCurrentFrame() == m_imageForCurrentFrame))
        return;

    m_contents = contents;
    m_imageForCurrentFrame = m_contents->nativeImageForCurrentFrame();
    setNeedsDisplay();
}

void ImageLayerChromium::paintContentsIfDirty()
{
    if (m_needsDisplay) {
        m_textureUpdater->updateFromImage(m_contents->nativeImageForCurrentFrame());
        updateTileSizeAndTilingOption();
        IntRect paintRect(IntPoint(), contentBounds());
        if (m_needsDisplay) {
            invalidateRect(paintRect);
            m_needsDisplay = false;
        }
    }

    if (visibleLayerRect().isEmpty())
        return;

    prepareToUpdate(visibleLayerRect());
}

LayerTextureUpdater* ImageLayerChromium::textureUpdater() const
{
    return m_textureUpdater.get();
}

IntSize ImageLayerChromium::contentBounds() const
{
    if (!m_contents)
        return IntSize();
    return m_contents->size();
}

bool ImageLayerChromium::drawsContent() const
{
    return m_contents && TiledLayerChromium::drawsContent();
}

void ImageLayerChromium::createTextureUpdater(const CCLayerTreeHost* host)
{
    m_textureUpdater = ImageLayerTextureUpdater::create(host->layerRendererCapabilities().usingMapSub);
}

bool ImageLayerChromium::needsContentsScale() const
{
    // Contents scale is not need for image layer because this can be done in compositor more efficiently.
    return false;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
