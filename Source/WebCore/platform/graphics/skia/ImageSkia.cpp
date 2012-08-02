/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#include "AffineTransform.h"
#include "BitmapImage.h"
#include "BitmapImageSingleFrameSkia.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageObserver.h"
#include "Logging.h"
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "PlatformString.h"
#include "SkPixelRef.h"
#include "SkRect.h"
#include "SkShader.h"
#include "SkiaUtils.h"
#include "Texture.h"

#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

namespace WebCore {

// Used by computeResamplingMode to tell how bitmaps should be resampled.
enum ResamplingMode {
    // Nearest neighbor resampling. Used when we detect that the page is
    // trying to make a pattern by stretching a small bitmap very large.
    RESAMPLE_NONE,

    // Default skia resampling. Used for large growing of images where high
    // quality resampling doesn't get us very much except a slowdown.
    RESAMPLE_LINEAR,

    // High quality resampling.
    RESAMPLE_AWESOME,
};

static ResamplingMode computeResamplingMode(const SkMatrix& matrix, const NativeImageSkia& bitmap, int srcWidth, int srcHeight, float destWidth, float destHeight)
{
    int destIWidth = static_cast<int>(destWidth);
    int destIHeight = static_cast<int>(destHeight);

    // The percent change below which we will not resample. This usually means
    // an off-by-one error on the web page, and just doing nearest neighbor
    // sampling is usually good enough.
    const float kFractionalChangeThreshold = 0.025f;

    // Images smaller than this in either direction are considered "small" and
    // are not resampled ever (see below).
    const int kSmallImageSizeThreshold = 8;

    // The amount an image can be stretched in a single direction before we
    // say that it is being stretched so much that it must be a line or
    // background that doesn't need resampling.
    const float kLargeStretch = 3.0f;

    // Figure out if we should resample this image. We try to prune out some
    // common cases where resampling won't give us anything, since it is much
    // slower than drawing stretched.
    if (srcWidth == destIWidth && srcHeight == destIHeight) {
        // We don't need to resample if the source and destination are the same.
        return RESAMPLE_NONE;
    }

    if (srcWidth <= kSmallImageSizeThreshold
        || srcHeight <= kSmallImageSizeThreshold
        || destWidth <= kSmallImageSizeThreshold
        || destHeight <= kSmallImageSizeThreshold) {
        // Never resample small images. These are often used for borders and
        // rules (think 1x1 images used to make lines).
        return RESAMPLE_NONE;
    }

    if (srcHeight * kLargeStretch <= destHeight || srcWidth * kLargeStretch <= destWidth) {
        // Large image detected.

        // Don't resample if it is being stretched a lot in only one direction.
        // This is trying to catch cases where somebody has created a border
        // (which might be large) and then is stretching it to fill some part
        // of the page.
        if (srcWidth == destWidth || srcHeight == destHeight)
            return RESAMPLE_NONE;

        // The image is growing a lot and in more than one direction. Resampling
        // is slow and doesn't give us very much when growing a lot.
        return RESAMPLE_LINEAR;
    }

    if ((fabs(destWidth - srcWidth) / srcWidth < kFractionalChangeThreshold)
        && (fabs(destHeight - srcHeight) / srcHeight < kFractionalChangeThreshold)) {
        // It is disappointingly common on the web for image sizes to be off by
        // one or two pixels. We don't bother resampling if the size difference
        // is a small fraction of the original size.
        return RESAMPLE_NONE;
    }

    // When the image is not yet done loading, use linear. We don't cache the
    // partially resampled images, and as they come in incrementally, it causes
    // us to have to resample the whole thing every time.
    if (!bitmap.isDataComplete())
        return RESAMPLE_LINEAR;

    // Everything else gets resampled.
    // High quality interpolation only enabled for scaling and translation.
    if (!(matrix.getType() & (SkMatrix::kAffine_Mask | SkMatrix::kPerspective_Mask)))
        return RESAMPLE_AWESOME;
    
    return RESAMPLE_LINEAR;
}

static ResamplingMode limitResamplingMode(PlatformContextSkia* platformContext, ResamplingMode resampling)
{
    switch (platformContext->interpolationQuality()) {
    case InterpolationNone:
        return RESAMPLE_NONE;
    case InterpolationMedium:
        // For now we treat InterpolationMedium and InterpolationLow the same.
    case InterpolationLow:
        if (resampling == RESAMPLE_AWESOME)
            return RESAMPLE_LINEAR;
        break;
    case InterpolationHigh:
    case InterpolationDefault:
        break;
    }

    return resampling;
}

// Draws the given bitmap to the given canvas. The subset of the source bitmap
// identified by src_rect is drawn to the given destination rect. The bitmap
// will be resampled to resample_width * resample_height (this is the size of
// the whole image, not the subset). See shouldResampleBitmap for more.
//
// This does a lot of computation to resample only the portion of the bitmap
// that will only be drawn. This is critical for performance since when we are
// scrolling, for example, we are only drawing a small strip of the image.
// Resampling the whole image every time is very slow, so this speeds up things
// dramatically.
//
// Note: this code is only used when the canvas transformation is limited to
// scaling or translation.
static void drawResampledBitmap(SkCanvas& canvas, SkPaint& paint, const NativeImageSkia& bitmap, const SkIRect& srcIRect, const SkRect& destRect)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT0("skia", "drawResampledBitmap");
#endif
    // Apply forward transform to destRect to estimate required size of
    // re-sampled bitmap, and use only in calls required to resize, or that
    // check for the required size.
    SkRect destRectTransformed;
    canvas.getTotalMatrix().mapRect(&destRectTransformed, destRect);
    SkIRect destRectTransformedRounded;
    destRectTransformed.round(&destRectTransformedRounded);

    // Compute the visible portion of our rect.
    SkRect destRectVisibleSubset;
    ClipRectToCanvas(canvas, destRect, &destRectVisibleSubset);
    // ClipRectToCanvas often overshoots, resulting in a larger region than our
    // original destRect. Intersecting gets us back inside.
    if (!destRectVisibleSubset.intersect(destRect))
        return; // Nothing visible in destRect.

    // Compute the transformed (screen space) portion of the visible portion for
    // use below.
    SkRect destRectVisibleSubsetTransformed;
    canvas.getTotalMatrix().mapRect(&destRectVisibleSubsetTransformed, destRectVisibleSubset);
    SkRect destBitmapSubsetTransformed = destRectVisibleSubsetTransformed;
    destBitmapSubsetTransformed.offset(-destRectTransformed.fLeft,
                                       -destRectTransformed.fTop);
    SkIRect destBitmapSubsetTransformedRounded;
    destBitmapSubsetTransformed.round(&destBitmapSubsetTransformedRounded);

    // Transforms above plus rounding may cause destBitmapSubsetTransformedRounded
    // to go outside the image, so need to clip to avoid problems.
    if (!destBitmapSubsetTransformedRounded.intersect(
            0, 0, destRectTransformedRounded.width(), destRectTransformedRounded.height()))
        return; // Image is not visible.

    SkBitmap resampled = bitmap.resizedBitmap(srcIRect,
                                              destRectTransformedRounded.width(),
                                              destRectTransformedRounded.height(),
                                              destBitmapSubsetTransformedRounded);
    canvas.drawBitmapRect(resampled, 0, destRectVisibleSubset, &paint);
}

static bool hasNon90rotation(PlatformContextSkia* context)
{
    return !context->canvas()->getTotalMatrix().rectStaysRect();
}

static void paintSkBitmap(PlatformContextSkia* platformContext, const NativeImageSkia& bitmap, const SkIRect& srcRect, const SkRect& destRect, const SkXfermode::Mode& compOp)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT0("skia", "paintSkBitmap");
#endif
    SkPaint paint;
    paint.setXfermodeMode(compOp);
    paint.setAlpha(platformContext->getNormalizedAlpha());
    paint.setLooper(platformContext->getDrawLooper());
    // only antialias if we're rotated or skewed
    paint.setAntiAlias(hasNon90rotation(platformContext));

    SkCanvas* canvas = platformContext->canvas();

    ResamplingMode resampling;
    if (platformContext->isAccelerated())
        resampling = RESAMPLE_LINEAR;
    else if (platformContext->printing())
        resampling = RESAMPLE_NONE;
    else {
        // Take into account scale applied to the canvas when computing sampling mode (e.g. CSS scale or page scale).
        SkRect destRectTarget = destRect;
        if (!(canvas->getTotalMatrix().getType() & (SkMatrix::kAffine_Mask | SkMatrix::kPerspective_Mask)))
            canvas->getTotalMatrix().mapRect(&destRectTarget, destRect);

        resampling = computeResamplingMode(canvas->getTotalMatrix(), bitmap, srcRect.width(), srcRect.height(),
                                           SkScalarToFloat(destRectTarget.width()), SkScalarToFloat(destRectTarget.height()));
    }

    if (resampling == RESAMPLE_NONE) {
      // FIXME: This is to not break tests (it results in the filter bitmap flag
      // being set to true). We need to decide if we respect RESAMPLE_NONE
      // being returned from computeResamplingMode.
        resampling = RESAMPLE_LINEAR;
    }
    resampling = limitResamplingMode(platformContext, resampling);
    paint.setFilterBitmap(resampling == RESAMPLE_LINEAR);
    if (resampling == RESAMPLE_AWESOME)
        drawResampledBitmap(*canvas, paint, bitmap, srcRect, destRect);
    else {
        // No resampling necessary, we can just draw the bitmap. We want to
        // filter it if we decided to do linear interpolation above, or if there
        // is something interesting going on with the matrix (like a rotation).
        // Note: for serialization, we will want to subset the bitmap first so
        // we don't send extra pixels.
        canvas->drawBitmapRect(bitmap.bitmap(), &srcRect, destRect, &paint);
    }
    platformContext->didDrawRect(destRect, paint, &bitmap.bitmap());
}

// A helper method for translating negative width and height values.
FloatRect normalizeRect(const FloatRect& rect)
{
    FloatRect norm = rect;
    if (norm.width() < 0) {
        norm.setX(norm.x() + norm.width());
        norm.setWidth(-norm.width());
    }
    if (norm.height() < 0) {
        norm.setY(norm.y() + norm.height());
        norm.setHeight(-norm.height());
    }
    return norm;
}

bool FrameData::clear(bool clearMetadata)
{
    if (clearMetadata)
        m_haveMetadata = false;

    if (m_frame) {
        // ImageSource::createFrameAtIndex() allocated |m_frame| and passed
        // ownership to BitmapImage; we must delete it here.
        delete m_frame;
        m_frame = 0;
        return true;
    }
    return false;
}

void Image::drawPattern(GraphicsContext* context,
                        const FloatRect& floatSrcRect,
                        const AffineTransform& patternTransform,
                        const FloatPoint& phase,
                        ColorSpace styleColorSpace,
                        CompositeOperator compositeOp,
                        const FloatRect& destRect)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT0("skia", "Image::drawPattern");
#endif
    FloatRect normSrcRect = normalizeRect(floatSrcRect);
    if (destRect.isEmpty() || normSrcRect.isEmpty())
        return; // nothing to draw

    NativeImageSkia* bitmap = nativeImageForCurrentFrame();
    if (!bitmap)
        return;

    SkIRect srcRect = enclosingIntRect(normSrcRect);
    SkMatrix ctm = context->platformContext()->canvas()->getTotalMatrix();
    SkMatrix totalMatrix;
    totalMatrix.setConcat(ctm, patternTransform);

    // Figure out what size the bitmap will be in the destination. The
    // destination rect is the bounds of the pattern, we need to use the
    // matrix to see how big it will be.
    SkRect destRectTarget;
    totalMatrix.mapRect(&destRectTarget, normSrcRect);

    float destBitmapWidth = SkScalarToFloat(destRectTarget.width());
    float destBitmapHeight = SkScalarToFloat(destRectTarget.height());

    // Compute the resampling mode.
    ResamplingMode resampling;
    if (context->platformContext()->isAccelerated() || context->platformContext()->printing())
        resampling = RESAMPLE_LINEAR;
    else
        resampling = computeResamplingMode(totalMatrix, *bitmap, srcRect.width(), srcRect.height(), destBitmapWidth, destBitmapHeight);
    resampling = limitResamplingMode(context->platformContext(), resampling);

    // Load the transform WebKit requested.
    SkMatrix matrix(patternTransform);

    SkShader* shader;
    if (resampling == RESAMPLE_AWESOME) {
        // Do nice resampling.
        int width = static_cast<int>(destBitmapWidth);
        int height = static_cast<int>(destBitmapHeight);
        SkBitmap resampled = bitmap->resizedBitmap(srcRect, width, height);
        shader = SkShader::CreateBitmapShader(resampled, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);

        // Since we just resized the bitmap, we need to remove the scale 
        // applied to the pixels in the bitmap shader. This means we need
        // CTM * patternTransform to have identity scale. Since we
        // can't modify CTM (or the rectangle will be drawn in the wrong
        // place), we must set patternTransform's scale to the inverse of
        // CTM scale.
        matrix.setScaleX(ctm.getScaleX() ? 1 / ctm.getScaleX() : 1);
        matrix.setScaleY(ctm.getScaleY() ? 1 / ctm.getScaleY() : 1);
    } else {
        // No need to do nice resampling.
        SkBitmap srcSubset;
        bitmap->bitmap().extractSubset(&srcSubset, srcRect);
        shader = SkShader::CreateBitmapShader(srcSubset, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode);
    }

    // We also need to translate it such that the origin of the pattern is the
    // origin of the destination rect, which is what WebKit expects. Skia uses
    // the coordinate system origin as the base for the patter. If WebKit wants
    // a shifted image, it will shift it from there using the patternTransform.
    float adjustedX = phase.x() + normSrcRect.x() *
                      narrowPrecisionToFloat(patternTransform.a());
    float adjustedY = phase.y() + normSrcRect.y() *
                      narrowPrecisionToFloat(patternTransform.d());
    matrix.postTranslate(SkFloatToScalar(adjustedX),
                         SkFloatToScalar(adjustedY));
    shader->setLocalMatrix(matrix);

    SkPaint paint;
    paint.setShader(shader)->unref();
    paint.setXfermodeMode(WebCoreCompositeToSkiaComposite(compositeOp));
    paint.setFilterBitmap(resampling == RESAMPLE_LINEAR);

    context->platformContext()->paintSkPaint(destRect, paint);
}

// ================================================
// BitmapImage Class
// ================================================

// FIXME: These should go to BitmapImageSkia.cpp

void BitmapImage::invalidatePlatformData()
{
}

void BitmapImage::checkForSolidColor()
{
    m_isSolidColor = false;
    m_checkedForSolidColor = true;

    if (frameCount() > 1)
        return;

    WebCore::NativeImageSkia* frame = frameAtIndex(0);

    if (frame && size().width() == 1 && size().height() == 1) {
        SkAutoLockPixels lock(frame->bitmap());
        if (!frame->bitmap().getPixels())
            return;

        m_isSolidColor = true;
        m_solidColor = Color(frame->bitmap().getColor(0, 0));
    }
}

void BitmapImage::draw(GraphicsContext* ctxt, const FloatRect& dstRect,
                       const FloatRect& srcRect, ColorSpace colorSpace, CompositeOperator compositeOp)
{
    if (!m_source.initialized())
        return;

    // Spin the animation to the correct frame before we try to draw it, so we
    // don't draw an old frame and then immediately need to draw a newer one,
    // causing flicker and wasting CPU.
    startAnimation();

    NativeImageSkia* bm = nativeImageForCurrentFrame();
    if (!bm)
        return; // It's too early and we don't have an image yet.

    FloatRect normDstRect = normalizeRect(dstRect);
    FloatRect normSrcRect = normalizeRect(srcRect);

    if (normSrcRect.isEmpty() || normDstRect.isEmpty())
        return; // Nothing to draw.

    paintSkBitmap(ctxt->platformContext(),
                  *bm,
                  enclosingIntRect(normSrcRect),
                  normDstRect,
                  WebCoreCompositeToSkiaComposite(compositeOp));

    if (ImageObserver* observer = imageObserver())
        observer->didDraw(this);
}

// FIXME: These should go into BitmapImageSingleFrameSkia.cpp

void BitmapImageSingleFrameSkia::draw(GraphicsContext* ctxt,
                                      const FloatRect& dstRect,
                                      const FloatRect& srcRect,
                                      ColorSpace styleColorSpace,
                                      CompositeOperator compositeOp)
{
    FloatRect normDstRect = normalizeRect(dstRect);
    FloatRect normSrcRect = normalizeRect(srcRect);

    if (normSrcRect.isEmpty() || normDstRect.isEmpty())
        return; // Nothing to draw.

    paintSkBitmap(ctxt->platformContext(),
                  m_nativeImage,
                  enclosingIntRect(normSrcRect),
                  normDstRect,
                  WebCoreCompositeToSkiaComposite(compositeOp));

    if (ImageObserver* observer = imageObserver())
        observer->didDraw(this);
}

BitmapImageSingleFrameSkia::BitmapImageSingleFrameSkia(const SkBitmap& bitmap, float resolutionScale)
    : m_nativeImage(bitmap, resolutionScale)
{
}

PassRefPtr<BitmapImageSingleFrameSkia> BitmapImageSingleFrameSkia::create(const SkBitmap& bitmap, bool copyPixels, float resolutionScale)
{
    if (copyPixels) {
        SkBitmap temp;
        if (!bitmap.deepCopyTo(&temp, bitmap.config()))
            bitmap.copyTo(&temp, bitmap.config());
        return adoptRef(new BitmapImageSingleFrameSkia(temp, resolutionScale));
    }
    return adoptRef(new BitmapImageSingleFrameSkia(bitmap, resolutionScale));
}

} // namespace WebCore
