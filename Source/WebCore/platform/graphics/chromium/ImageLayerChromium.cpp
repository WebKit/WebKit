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

namespace WebCore {

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
        m_decodedImage.updateFromImage(m_contents->nativeImageForCurrentFrame());
        updateLayerSize(m_decodedImage.size());
        IntRect paintRect(IntPoint(0, 0), m_decodedImage.size());
        if (!m_dirtyRect.isEmpty()) {
            m_tiler->invalidateRect(paintRect);
            m_dirtyRect = IntRect();
        }
    }
}

void ImageLayerChromium::updateCompositorResources()
{
    IntRect paintRect(IntPoint(0, 0), m_decodedImage.size());
    m_tiler->updateFromPixels(paintRect, paintRect, m_decodedImage.pixels());
}

IntRect ImageLayerChromium::layerBounds() const
{
    return IntRect(IntPoint(0, 0), m_decodedImage.size());
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
