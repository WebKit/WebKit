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

#include "cc/CCLayerImpl.h"
#include "Image.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"
#include "LayerTextureSubImage.h"
#include "LayerTextureUpdater.h"
#include "PlatformColor.h"

namespace WebCore {

class ImageLayerTextureUpdater : public LayerTextureUpdater {
    WTF_MAKE_NONCOPYABLE(ImageLayerTextureUpdater);
public:
    static PassOwnPtr<ImageLayerTextureUpdater> create(GraphicsContext3D* context, bool useMapTexSubImage)
    {
        return adoptPtr(new ImageLayerTextureUpdater(context, useMapTexSubImage));
    }

    virtual ~ImageLayerTextureUpdater() { }

    virtual Orientation orientation() { return LayerTextureUpdater::BottomUpOrientation; }

    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat)
    {
        return PlatformColor::sameComponentOrder(textureFormat) ?
                LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
    }

    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, int borderTexels)
    {
        m_texSubImage.setSubImageSize(tileSize);
    }

    virtual void updateTextureRect(LayerTexture* texture, const IntRect& sourceRect, const IntRect& destRect)
    {
        texture->bindTexture();

        // Source rect should never go outside the image pixels, even if this
        // is requested because the texture extends outside the image.
        IntRect clippedSourceRect = sourceRect;
        clippedSourceRect.intersect(imageRect());

        IntRect clippedDestRect = destRect;
        clippedDestRect.move(clippedSourceRect.location() - sourceRect.location());
        clippedDestRect.setSize(clippedSourceRect.size());

        m_texSubImage.upload(m_image.pixels(), imageRect(), clippedSourceRect, clippedDestRect, texture->format(), context());
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
    ImageLayerTextureUpdater(GraphicsContext3D* context, bool useMapTexSubImage)
        : LayerTextureUpdater(context)
        , m_texSubImage(useMapTexSubImage)
    {
    }

    IntRect imageRect() const
    {
        return IntRect(IntPoint::zero(), m_image.size());
    }

    PlatformImage m_image;
    LayerTextureSubImage m_texSubImage;
};

PassRefPtr<ImageLayerChromium> ImageLayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new ImageLayerChromium(owner));
}

ImageLayerChromium::ImageLayerChromium(GraphicsLayerChromium* owner)
    : ContentLayerChromium(owner)
    , m_imageForCurrentFrame(0)
    , m_contents(0)
{
}

ImageLayerChromium::~ImageLayerChromium()
{
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
    m_dirtyRect = IntRect(IntPoint(0, 0), bounds());
    setNeedsDisplay();
}

void ImageLayerChromium::paintContentsIfDirty(const IntRect&)
{
    ASSERT(layerRenderer());

    if (!m_dirtyRect.isEmpty()) {
        // FIXME: This downcast is bad. The fix is to make ImageLayerChromium not derive from ContentLayerChromium.
        ImageLayerTextureUpdater* imageTextureUpdater = static_cast<ImageLayerTextureUpdater*>(m_textureUpdater.get());
        imageTextureUpdater->updateFromImage(m_contents->nativeImageForCurrentFrame());
        updateLayerSize(imageTextureUpdater->imageSize());
        IntRect paintRect(IntPoint(0, 0), imageTextureUpdater->imageSize());
        if (!m_dirtyRect.isEmpty()) {
            m_tiler->invalidateRect(paintRect);
            m_dirtyRect = IntRect();
        }
        m_tiler->prepareToUpdate(paintRect, m_textureUpdater.get());
    }
}

void ImageLayerChromium::updateCompositorResources()
{
    m_tiler->updateRect(m_textureUpdater.get());
}

void ImageLayerChromium::setLayerRenderer(LayerRendererChromium* newLayerRenderer)
{
    if (newLayerRenderer != layerRenderer())
        m_textureUpdater.clear();
    ContentLayerChromium::setLayerRenderer(newLayerRenderer);
}

void ImageLayerChromium::createTextureUpdaterIfNeeded()
{
    if (!m_textureUpdater)
        m_textureUpdater = ImageLayerTextureUpdater::create(layerRendererContext(), layerRenderer()->contextSupportsMapSub());
}

IntRect ImageLayerChromium::layerBounds() const
{
    if (!m_textureUpdater)
        return IntRect();
    ImageLayerTextureUpdater* imageTextureUpdater = static_cast<ImageLayerTextureUpdater*>(m_textureUpdater.get());
    return IntRect(IntPoint(), imageTextureUpdater->imageSize());
}

TransformationMatrix ImageLayerChromium::tilingTransform()
{
    // Tiler draws from the upper left corner. The draw transform
    // specifies the middle of the layer.
    TransformationMatrix transform = ccLayerImpl()->drawTransform();
    const IntRect sourceRect = layerBounds();
    const IntSize destSize = bounds();

    transform.translate(-destSize.width() / 2.0, -destSize.height() / 2.0);

    // Tiler also draws at the original content size, so rescale the original
    // image dimensions to the bounds that it is meant to be drawn at.
    float scaleX = destSize.width() / static_cast<float>(sourceRect.size().width());
    float scaleY = destSize.height() / static_cast<float>(sourceRect.size().height());
    transform.scale3d(scaleX, scaleY, 1.0f);

    return transform;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
