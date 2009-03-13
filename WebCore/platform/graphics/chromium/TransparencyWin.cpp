/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include <windows.h>

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PlatformContextSkia.h"
#include "SimpleFontData.h"
#include "TransformationMatrix.h"
#include "TransparencyWin.h"

#include "SkColorPriv.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

namespace {

// The maximum size in pixels of the buffer we'll keep around for drawing text
// into. Buffers larger than this will be destroyed when we're done with them.
const int maxCachedBufferPixelSize = 65536;

inline skia::PlatformCanvas* canvasForContext(const GraphicsContext& context)
{
    return context.platformContext()->canvas();
}

inline const SkBitmap& bitmapForContext(const GraphicsContext& context)
{
    return canvasForContext(context)->getTopPlatformDevice().accessBitmap(false);
}

void compositeToCopy(const GraphicsContext& sourceLayers,
                     GraphicsContext& destContext,
                     const TransformationMatrix& matrix)
{
    // Make a list of all devices. The iterator goes top-down, and we want
    // bottom-up. Note that each layer can also have an offset in canvas
    // coordinates, which is the (x, y) position.
    struct DeviceInfo {
        DeviceInfo(SkDevice* d, int lx, int ly)
            : device(d)
            , x(lx)
            , y(ly) {}
        SkDevice* device;
        int x;
        int y;
    };
    Vector<DeviceInfo> devices;
    SkCanvas* sourceCanvas = canvasForContext(sourceLayers);
    SkCanvas::LayerIter iter(sourceCanvas, false);
    while (!iter.done()) {
        devices.append(DeviceInfo(iter.device(), iter.x(), iter.y()));
        iter.next();
    }

    // Create a temporary canvas for the compositing into the destination.
    SkBitmap* destBmp = const_cast<SkBitmap*>(&bitmapForContext(destContext));
    SkCanvas destCanvas(*destBmp);
    destCanvas.setMatrix(matrix);

    for (int i = devices.size() - 1; i >= 0; i--) {
        const SkBitmap& srcBmp = devices[i].device->accessBitmap(false);

        SkRect destRect;
        destRect.fLeft = devices[i].x;
        destRect.fTop = devices[i].y;
        destRect.fRight = destRect.fLeft + srcBmp.width();
        destRect.fBottom = destRect.fTop + srcBmp.height();

        destCanvas.drawBitmapRect(srcBmp, 0, destRect);
    }
}

}  // namespace

// If either of these pointers is non-null, both must be valid and point to
// bitmaps of the same size.
class TransparencyWin::OwnedBuffers {
public:
    OwnedBuffers(const IntSize& size, bool needReferenceBuffer)
    {
        m_destBitmap.adopt(ImageBuffer::create(size, false));

        if (needReferenceBuffer) {
            m_referenceBitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
            m_referenceBitmap.allocPixels();
            m_referenceBitmap.eraseARGB(0, 0, 0, 0);
        }
    }

    ImageBuffer* destBitmap() { return m_destBitmap.get(); }

    // This bitmap will be empty if you don't specify needReferenceBuffer to the
    // constructor.
    SkBitmap* referenceBitmap() { return &m_referenceBitmap; }

    // Returns whether the current layer will fix a buffer of the given size.
    bool canHandleSize(const IntSize& size) const
    {
        return m_destBitmap->size().width() >= size.width() && m_destBitmap->size().height() >= size.height();
    }

private:
    // The destination bitmap we're drawing into.
    OwnPtr<ImageBuffer> m_destBitmap;

    // This could be an ImageBuffer but this is an optimization. Since this is
    // only ever used as a reference, we don't need to make a full
    // PlatformCanvas using Skia on Windows. Just allocating a regular SkBitmap
    // is much faster since it's just a Malloc rather than a GDI call.
    SkBitmap m_referenceBitmap;
};

TransparencyWin::OwnedBuffers* TransparencyWin::m_cachedBuffers = 0;

TransparencyWin::TransparencyWin()
    : m_destContext(0)
    , m_orgTransform()
    , m_layerMode(NoLayer)
    , m_transformMode(KeepTransform)
    , m_drawContext(0)
    , m_savedOnDrawContext(false)
    , m_layerBuffer(0)
    , m_referenceBitmap(0)
{
}

TransparencyWin::~TransparencyWin()
{
    // This should be false, since calling composite() is mandatory.
    ASSERT(!m_savedOnDrawContext);
}

void TransparencyWin::composite()
{
    // Matches the save() in initializeNewTextContext (or the constructor for
    // SCALE) to put the context back into the same state we found it.
    if (m_savedOnDrawContext) {
        m_drawContext->restore();
        m_savedOnDrawContext = false;
    }

    switch (m_layerMode) {
    case NoLayer:
        break;
    case OpaqueCompositeLayer:
    case WhiteLayer:
        compositeOpaqueComposite();
        break;
    case TextComposite:
        compositeTextComposite();
        break;
    }
}

void TransparencyWin::init(GraphicsContext* dest,
                           LayerMode layerMode,
                           TransformMode transformMode,
                           const IntRect& region)
{
    m_destContext = dest;
    m_orgTransform = dest->getCTM();
    m_layerMode = layerMode;
    m_transformMode = transformMode;
    m_sourceRect = region;

    computeLayerSize();
    setupLayer();
    setupTransform(region);
}

void TransparencyWin::computeLayerSize()
{
    if (m_transformMode == Untransform) {
        // The meaning of the "transformed" source rect is a little ambigous
        // here. The rest of the code doesn't care about it in the Untransform
        // case since we're using our own happy coordinate system. So we set it
        // to be the source rect since that matches how the code below actually
        // uses the variable: to determine how to translate things to account
        // for the offset of the layer.
        m_transformedSourceRect = m_sourceRect;
        m_layerSize = IntSize(m_sourceRect.width(), m_sourceRect.height());
    } else {
        m_transformedSourceRect = m_orgTransform.mapRect(m_sourceRect);
        m_layerSize = IntSize(m_transformedSourceRect.width(), m_transformedSourceRect.height());
    }
}

void TransparencyWin::setupLayer()
{
    switch (m_layerMode) {
    case NoLayer:
        setupLayerForNoLayer();
        break;
    case OpaqueCompositeLayer:
        setupLayerForOpaqueCompositeLayer();
        break;
    case TextComposite:
        setupLayerForTextComposite();
        break;
    case WhiteLayer:
        setupLayerForWhiteLayer();
        break;
    }
}

void TransparencyWin::setupLayerForNoLayer()
{
    m_drawContext = m_destContext;  // Draw to the source context.
}

void TransparencyWin::setupLayerForOpaqueCompositeLayer()
{
    initializeNewContext();

    TransformationMatrix mapping;
    mapping.translate(-m_transformedSourceRect.x(), -m_transformedSourceRect.y());
    if (m_transformMode == Untransform){ 
        // Compute the inverse mapping from the canvas space to the
        // coordinate space of our bitmap.
        mapping = m_orgTransform.inverse() * mapping;
    }
    compositeToCopy(*m_destContext, *m_drawContext, mapping);

    // Save the reference layer so we can tell what changed.
    SkCanvas referenceCanvas(*m_referenceBitmap);
    referenceCanvas.drawBitmap(bitmapForContext(*m_drawContext), 0, 0);
    // Layer rect represents the part of the original layer.
}

void TransparencyWin::setupLayerForTextComposite()
{
    ASSERT(m_transformMode == KeepTransform);
    // Fall through to filling with white.
    setupLayerForWhiteLayer();
}

void TransparencyWin::setupLayerForWhiteLayer()
{
    initializeNewContext();
    m_drawContext->fillRect(IntRect(IntPoint(0, 0), m_layerSize), Color::white);
    // Layer rect represents the part of the original layer.
}

void TransparencyWin::setupTransform(const IntRect& region)
{
    switch (m_transformMode) {
    case KeepTransform:
        setupTransformForKeepTransform(region);
        break;
    case Untransform:
        setupTransformForUntransform();
        break;
    case ScaleTransform:
        setupTransformForScaleTransform();
        break;
    }
}

void TransparencyWin::setupTransformForKeepTransform(const IntRect& region)
{
    if (m_layerMode != NoLayer) {
        // Need to save things since we're modifying the transform.
        m_drawContext->save();
        m_savedOnDrawContext = true;

        // Account for the fact that the layer may be offset from the
        // original. This only happens when we create a layer that has the
        // same coordinate space as the parent.
        TransformationMatrix xform;
        xform.translate(-m_transformedSourceRect.x(), -m_transformedSourceRect.y());

        // We're making a layer, so apply the old transform to the new one
        // so it's maintained. We know the new layer has the identity
        // transform now, we we can just multiply it.
        xform = m_orgTransform * xform;
        m_drawContext->concatCTM(xform);
    }
    m_drawRect = m_sourceRect;
}

void TransparencyWin::setupTransformForUntransform()
{
    ASSERT(m_layerMode != NoLayer);
    // We now have a new layer with the identity transform, which is the
    // Untransformed space we'll use for drawing.
    m_drawRect = IntRect(IntPoint(0, 0), m_layerSize);
}

void TransparencyWin::setupTransformForScaleTransform()
{
    if (m_layerMode == NoLayer) {
        // Need to save things since we're modifying the layer.
        m_drawContext->save();
        m_savedOnDrawContext = true;

        // Undo the transform on the current layer when we're re-using the
        // current one.
        m_drawContext->concatCTM(m_drawContext->getCTM().inverse());

        // We're drawing to the original layer with just a different size.
        m_drawRect = m_transformedSourceRect;
    } else {
        // Just go ahead and use the layer's coordinate space to draw into.
        // It will have the scaled size, and an identity transform loaded.
        m_drawRect = IntRect(IntPoint(0, 0), m_layerSize);
    }
}

void TransparencyWin::setTextCompositeColor(Color color)
{
    m_textCompositeColor = color;
}

void TransparencyWin::initializeNewContext()
{
    int pixelSize = m_layerSize.width() * m_layerSize.height();
    if (pixelSize > maxCachedBufferPixelSize) {
        // Create a 1-off buffer for drawing into. We only need the reference
        // buffer if we're making an OpaqueCompositeLayer.
        bool needReferenceBitmap = m_layerMode == OpaqueCompositeLayer;
        m_ownedBuffers.set(new OwnedBuffers(m_layerSize, needReferenceBitmap));

        m_layerBuffer = m_ownedBuffers->destBitmap();
        m_drawContext = m_layerBuffer->context();
        if (needReferenceBitmap)
            m_referenceBitmap = m_ownedBuffers->referenceBitmap();
        return;
    }

    if (m_cachedBuffers && m_cachedBuffers->canHandleSize(m_layerSize)) {
        // We can re-use the existing buffer. We don't need to clear it since
        // all layer modes will clear it in their initialization.
        m_layerBuffer = m_cachedBuffers->destBitmap();
        m_drawContext = m_cachedBuffers->destBitmap()->context();
        bitmapForContext(*m_drawContext).eraseARGB(0, 0, 0, 0);
        m_referenceBitmap = m_cachedBuffers->referenceBitmap();
        m_referenceBitmap->eraseARGB(0, 0, 0, 0);
        return;
    }

    // Create a new cached buffer.
    if (m_cachedBuffers)
      delete m_cachedBuffers;
    m_cachedBuffers = new OwnedBuffers(m_layerSize, true);

    m_layerBuffer = m_cachedBuffers->destBitmap();
    m_drawContext = m_cachedBuffers->destBitmap()->context();
    m_referenceBitmap = m_cachedBuffers->referenceBitmap();
}

void TransparencyWin::compositeOpaqueComposite()
{
    SkCanvas* destCanvas = canvasForContext(*m_destContext);
    destCanvas->save();

    SkBitmap* bitmap = const_cast<SkBitmap*>(
        &bitmapForContext(*m_layerBuffer->context()));
    
    // This function will be called for WhiteLayer as well, which we don't want
    // to change.
    if (m_layerMode == OpaqueCompositeLayer) {
        // Fix up our bitmap, making it contain only the pixels which changed
        // and transparent everywhere else.
        SkAutoLockPixels sourceLock(*m_referenceBitmap);
        SkAutoLockPixels lock(*bitmap);
        for (int y = 0; y < bitmap->height(); y++) {
            uint32_t* source = m_referenceBitmap->getAddr32(0, y);
            uint32_t* dest = bitmap->getAddr32(0, y);
            for (int x = 0; x < bitmap->width(); x++) {
                // Clear out any pixels that were untouched.
                if (dest[x] == source[x])
                    dest[x] = 0;
                else
                    dest[x] |= (0xFF << SK_A32_SHIFT);
            }
        }
    } else
        makeLayerOpaque();

    SkRect destRect;
    if (m_transformMode != Untransform) {
        // We want to use Untransformed space.
        //
        // Note that we DON'T call m_layerBuffer->image() here. This actually
        // makes a copy of the image, which is unnecessary and slow. Instead, we
        // just draw the image from inside the destination context.
        SkMatrix identity;
        identity.reset();
        destCanvas->setMatrix(identity);

        destRect.set(m_transformedSourceRect.x(), m_transformedSourceRect.y(), m_transformedSourceRect.right(), m_transformedSourceRect.bottom());
    } else
        destRect.set(m_sourceRect.x(), m_sourceRect.y(), m_sourceRect.right(), m_sourceRect.bottom());

    SkPaint paint;
    paint.setFilterBitmap(true);
    paint.setAntiAlias(true);

    // Note that we need to specify the source layer subset, since the bitmap
    // may have been cached and it could be larger than what we're using.
    SkIRect sourceRect = { 0, 0, m_layerSize.width(), m_layerSize.height() };
    destCanvas->drawBitmapRect(*bitmap, &sourceRect, destRect, &paint);
    destCanvas->restore();
}

void TransparencyWin::compositeTextComposite()
{
    const SkBitmap& bitmap = m_layerBuffer->context()->platformContext()->canvas()->getTopPlatformDevice().accessBitmap(true);
    SkColor textColor = m_textCompositeColor.rgb();
    for (int y = 0; y < m_layerSize.height(); y++) {
        uint32_t* row = bitmap.getAddr32(0, y);
        for (int x = 0; x < m_layerSize.width(); x++) {
            // The alpha is the average of the R, G, and B channels.
            int alpha = (SkGetPackedR32(row[x]) + SkGetPackedG32(row[x]) + SkGetPackedB32(row[x])) / 3;

            // Apply that alpha to the text color and write the result.
            row[x] = SkAlphaMulQ(textColor, SkAlpha255To256(255 - alpha));
        }
    }

    // Now the layer has text with the proper color and opacity.
    SkCanvas* destCanvas = canvasForContext(*m_destContext);

    // We want to use Untransformed space (see above)
    SkMatrix identity;
    identity.reset();
    destCanvas->setMatrix(identity);
    SkRect destRect = { m_transformedSourceRect.x(), m_transformedSourceRect.y(), m_transformedSourceRect.right(), m_transformedSourceRect.bottom() };

    // Note that we need to specify the source layer subset, since the bitmap
    // may have been cached and it could be larger than what we're using.
    SkIRect sourceRect = { 0, 0, m_layerSize.width(), m_layerSize.height() };
    destCanvas->drawBitmapRect(bitmap, &sourceRect, destRect, 0);
    destCanvas->restore();
}

void TransparencyWin::makeLayerOpaque()
{
    SkBitmap& bitmap = const_cast<SkBitmap&>(m_drawContext->platformContext()->
        canvas()->getTopPlatformDevice().accessBitmap(true));
    for (int y = 0; y < m_layerSize.height(); y++) {
        uint32_t* row = bitmap.getAddr32(0, y);
        for (int x = 0; x < m_layerSize.width(); x++)
            row[x] |= 0xFF000000;
    }
}

} // namespace WebCore

