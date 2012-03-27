/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "cc/CCRenderSurfaceFilters.h"

#include "GraphicsContext3D.h"
#include "LengthFunctions.h"
#include "SkBlurImageFilter.h"
#include "SkCanvas.h"
#include "SkColorMatrixFilter.h"
#include "SkGpuDevice.h"
#include "SkGrTexturePixelRef.h"

namespace {

void getBrightnessMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    //    Old implementation, a la the draft spec, a straight-up scale,
    //    representing <feFunc[R|G|B] type="linear" slope="[amount]">
    //    (See http://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#brightnessEquivalent)
    // matrix[0] = matrix[6] = matrix[12] = amount;
    // matrix[18] = 1;
    //    New implementation, a translation in color space, representing
    //    <feFunc[R|G|B] type="linear" intercept="[amount]"/>
    //    (See https://www.w3.org/Bugs/Public/show_bug.cgi?id=15647)
    matrix[0] = matrix[6] = matrix[12] = matrix[18] = 1;
    matrix[4] = matrix[9] = matrix[14] = amount * 255;
}

void getContrastMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = amount;
    matrix[4] = matrix[9] = matrix[14] = (-0.5f * amount + 0.5f) * 255;
    matrix[18] = 1;
}

void getSaturateMatrix(float amount, SkScalar matrix[20])
{
    matrix[0] = 0.213f + 0.787f * amount;
    matrix[1] = 0.715f - 0.715f * amount;
    matrix[2] = 0.072f - 0.072f * amount;
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - 0.213f * amount;
    matrix[6] = 0.715f + 0.285f * amount;
    matrix[7] = 0.072f - 0.072f * amount;
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - 0.213f * amount;
    matrix[11] = 0.715f - 0.715f * amount;
    matrix[12] = 0.072f + 0.928f * amount;
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

void getHueRotateMatrix(float hue, SkScalar matrix[20])
{
    float cosHue = cosf(hue * piFloat / 180); 
    float sinHue = sinf(hue * piFloat / 180); 
    matrix[0] = 0.213f + cosHue * 0.787f - sinHue * 0.213f;
    matrix[1] = 0.715f - cosHue * 0.715f - sinHue * 0.715f;
    matrix[2] = 0.072f - cosHue * 0.072f + sinHue * 0.928f;
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - cosHue * 0.213f + sinHue * 0.143f;
    matrix[6] = 0.715f + cosHue * 0.285f + sinHue * 0.140f;
    matrix[7] = 0.072f - cosHue * 0.072f - sinHue * 0.283f;
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - cosHue * 0.213f - sinHue * 0.787f;
    matrix[11] = 0.715f - cosHue * 0.715f + sinHue * 0.715f;
    matrix[12] = 0.072f + cosHue * 0.928f + sinHue * 0.072f;
    matrix[13] = matrix[14] = 0;
    matrix[15] = matrix[16] = matrix[17] = 0;
    matrix[18] = 1;
    matrix[19] = 0;
}

void getInvertMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = 1 - 2 * amount;
    matrix[4] = matrix[9] = matrix[14] = amount * 255;
    matrix[18] = 1;
}

void getOpacityMatrix(float amount, SkScalar matrix[20])
{
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = 1;
    matrix[18] = amount;
}

void getGrayscaleMatrix(float amount, SkScalar matrix[20])
{
    matrix[0] = 0.2126f + 0.7874f * amount;
    matrix[1] = 0.7152f - 0.7152f * amount;
    matrix[2] = 0.0722f - 0.0722f * amount;
    matrix[3] = matrix[4] = 0;

    matrix[5] = 0.2126f - 0.2126f * amount;
    matrix[6] = 0.7152f + 0.2848f * amount;
    matrix[7] = 0.0722f - 0.0722f * amount;
    matrix[8] = matrix[9] = 0;

    matrix[10] = 0.2126f - 0.2126f * amount;
    matrix[11] = 0.7152f - 0.7152f * amount;
    matrix[12] = 0.0722f + 0.9278f * amount;
    matrix[13] = matrix[14] = 0;

    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

void getSepiaMatrix(float amount, SkScalar matrix[20])
{
    matrix[0] = 0.393f + 0.607f * amount;
    matrix[1] = 0.769f - 0.769f * amount;
    matrix[2] = 0.189f - 0.189f * amount;
    matrix[3] = matrix[4] = 0;

    matrix[5] = 0.349f - 0.349f * amount;
    matrix[6] = 0.686f + 0.314f * amount;
    matrix[7] = 0.168f - 0.168f * amount;
    matrix[8] = matrix[9] = 0;

    matrix[10] = 0.272f - 0.272f * amount;
    matrix[11] = 0.534f - 0.534f * amount;
    matrix[12] = 0.131f + 0.869f * amount;
    matrix[13] = matrix[14] = 0;

    matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
    matrix[18] = 1;
}

bool applyColorMatrix(SkCanvas* canvas, SkBitmap source, SkScalar matrix[20])
{
    SkPaint paint;
    paint.setColorFilter(new SkColorMatrixFilter(matrix))->unref();
    canvas->drawBitmap(source, 0, 0, &paint);
    return true;
}

}

namespace WebCore {

SkBitmap CCRenderSurfaceFilters::apply(const FilterOperations& filters, unsigned textureId, const FloatSize& size, GraphicsContext3D* context3D)
{
    SkBitmap source;
    if (!context3D)
        return source;

    GrContext* gr = context3D->grContext();
    if (!gr)
        return source;
    // Wrap the RenderSurface's texture in a Ganesh platform texture.
    GrPlatformTextureDesc platformTexDesc;
    platformTexDesc.fWidth = size.width();
    platformTexDesc.fHeight = size.height();
    platformTexDesc.fConfig = kSkia8888_PM_GrPixelConfig;
    platformTexDesc.fTextureHandle = textureId;
    SkAutoTUnref<GrTexture> texture(gr->createPlatformTexture(platformTexDesc));
    // Place the platform texture inside an SkBitmap.
    source.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
    source.setPixelRef(new SkGrTexturePixelRef(texture.get()))->unref();
    
    GrContext::TextureCacheEntry dest;

    for (unsigned i = 0; i < filters.size(); ++i) {
        // Save the previous texture cache destination (if any), and keep it
        // locked during draw to prevent it be re-used as destination.
        GrContext::TextureCacheEntry sourceEntry = dest;
        const FilterOperation* filterOperation = filters.at(i);
        // Allocate a destination texture.
        GrTextureDesc desc;
        desc.fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
        desc.fSampleCnt = 0;
        desc.fWidth = size.width();
        desc.fHeight = size.height();
        desc.fConfig = kSkia8888_PM_GrPixelConfig;
        // FIXME: could we use approximate match, and fix texcoords on draw
        dest = gr->lockScratchTexture(desc, GrContext::kExact_ScratchTexMatch);
        if (!dest.texture())
            return SkBitmap();
        SkGpuDevice device(gr, dest.texture());
        SkCanvas canvas(&device);
        canvas.clear(0x0);
        switch (filterOperation->getOperationType()) {
        case FilterOperation::BRIGHTNESS: {
            const BasicColorMatrixFilterOperation* op = static_cast<const BasicColorMatrixFilterOperation*>(filterOperation);
            SkScalar matrix[20];
            getBrightnessMatrix(op->amount(), matrix);
            applyColorMatrix(&canvas, source, matrix);
            break;
        }
        case FilterOperation::CONTRAST: {
            const BasicColorMatrixFilterOperation* op = static_cast<const BasicColorMatrixFilterOperation*>(filterOperation);
            SkScalar matrix[20];
            getContrastMatrix(op->amount(), matrix);
            applyColorMatrix(&canvas, source, matrix);
            break;
        }
        case FilterOperation::GRAYSCALE: {
            const BasicColorMatrixFilterOperation* op = static_cast<const BasicColorMatrixFilterOperation*>(filterOperation);
            SkScalar matrix[20];
            getGrayscaleMatrix(1 - op->amount(), matrix);
            applyColorMatrix(&canvas, source, matrix);
            break;
        }
        case FilterOperation::SEPIA: {
            const BasicColorMatrixFilterOperation* op = static_cast<const BasicColorMatrixFilterOperation*>(filterOperation);
            SkScalar matrix[20];
            getSepiaMatrix(1 - op->amount(), matrix);
            applyColorMatrix(&canvas, source, matrix);
            break;
        }
        case FilterOperation::SATURATE: {
            const BasicColorMatrixFilterOperation* op = static_cast<const BasicColorMatrixFilterOperation*>(filterOperation);
            SkScalar matrix[20];
            getSaturateMatrix(op->amount(), matrix);
            applyColorMatrix(&canvas, source, matrix);
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            const BasicColorMatrixFilterOperation* op = static_cast<const BasicColorMatrixFilterOperation*>(filterOperation);
            SkScalar matrix[20];
            getHueRotateMatrix(op->amount(), matrix);
            applyColorMatrix(&canvas, source, matrix);
            break;
        }
        case FilterOperation::INVERT: {
            const BasicColorMatrixFilterOperation* op = static_cast<const BasicColorMatrixFilterOperation*>(filterOperation);
            SkScalar matrix[20];
            getInvertMatrix(op->amount(), matrix);
            applyColorMatrix(&canvas, source, matrix);
            break;
        }
        case FilterOperation::OPACITY: {
            const BasicComponentTransferFilterOperation* op = static_cast<const BasicComponentTransferFilterOperation*>(filterOperation);
            SkScalar matrix[20];
            getOpacityMatrix(op->amount(), matrix);
            applyColorMatrix(&canvas, source, matrix);
            break;
        }
        case FilterOperation::BLUR: {
            const BlurFilterOperation* op = static_cast<const BlurFilterOperation*>(filterOperation);
            float stdX = floatValueForLength(op->stdDeviation(), 0);
            float stdY = floatValueForLength(op->stdDeviation(), 1);
            SkAutoTUnref<SkImageFilter> filter(new SkBlurImageFilter(stdX, stdY));
            SkPaint paint;
            paint.setImageFilter(filter.get());
            canvas.drawSprite(source, 0, 0, &paint);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            const DropShadowFilterOperation* op = static_cast<const DropShadowFilterOperation*>(filterOperation);
            SkAutoTUnref<SkImageFilter> blurFilter(new SkBlurImageFilter(op->stdDeviation(), op->stdDeviation()));
            SkAutoTUnref<SkColorFilter> colorFilter(SkColorFilter::CreateModeFilter(op->color().rgb(), SkXfermode::kSrcIn_Mode));
            SkPaint paint;
            paint.setImageFilter(blurFilter.get());
            paint.setColorFilter(colorFilter.get());
            paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
            canvas.saveLayer(0, &paint);
            canvas.drawBitmap(source, op->x(), -op->y());
            canvas.restore();
            canvas.drawBitmap(source, 0, 0);
            break;
        }
        case FilterOperation::PASSTHROUGH:
        default:
            canvas.drawBitmap(source, 0, 0);
            break;
        }
        // Dest texture from this filter becomes source bitmap for next filter.
        source.setPixelRef(new SkGrTexturePixelRef(dest.texture()))->unref();
        // Unlock the previous texture cache entry.
        if (sourceEntry.texture())
            gr->unlockTexture(sourceEntry);
    }
    if (dest.texture())
        gr->unlockTexture(dest);
    context3D->flush();
    return source;
}

}
#endif // USE(ACCELERATED_COMPOSITING)
