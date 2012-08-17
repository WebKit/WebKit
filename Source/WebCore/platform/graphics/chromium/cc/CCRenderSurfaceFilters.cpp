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

#include "FloatSize.h"
#include "SkBlurImageFilter.h"
#include "SkCanvas.h"
#include "SkColorMatrixFilter.h"
#include "SkGpuDevice.h"
#include "SkGrTexturePixelRef.h"
#include "SkMagnifierImageFilter.h"
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/MathExtras.h>

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
    // Note, these values are computed to ensure matrixNeedsClamping is false
    // for amount in [0..1]
    matrix[0] = 0.213f + 0.787f * amount;
    matrix[1] = 0.715f - 0.715f * amount;
    matrix[2] = 1.f - (matrix[0] + matrix[1]);
    matrix[3] = matrix[4] = 0;
    matrix[5] = 0.213f - 0.213f * amount;
    matrix[6] = 0.715f + 0.285f * amount;
    matrix[7] = 1.f - (matrix[5] + matrix[6]);
    matrix[8] = matrix[9] = 0;
    matrix[10] = 0.213f - 0.213f * amount;
    matrix[11] = 0.715f - 0.715f * amount;
    matrix[12] = 1.f - (matrix[10] + matrix[11]);
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
    // Note, these values are computed to ensure matrixNeedsClamping is false
    // for amount in [0..1]
    matrix[0] = 0.2126f + 0.7874f * amount;
    matrix[1] = 0.7152f - 0.7152f * amount;
    matrix[2] = 1.f - (matrix[0] + matrix[1]);
    matrix[3] = matrix[4] = 0;

    matrix[5] = 0.2126f - 0.2126f * amount;
    matrix[6] = 0.7152f + 0.2848f * amount;
    matrix[7] = 1.f - (matrix[5] + matrix[6]);
    matrix[8] = matrix[9] = 0;

    matrix[10] = 0.2126f - 0.2126f * amount;
    matrix[11] = 0.7152f - 0.7152f * amount;
    matrix[12] = 1.f - (matrix[10] + matrix[11]);
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

// The 5x4 matrix is really a "compressed" version of a 5x5 matrix that'd have
// (0 0 0 0 1) as a last row, and that would be applied to a 5-vector extended
// from the 4-vector color with a 1.
void multColorMatrix(SkScalar a[20], SkScalar b[20], SkScalar out[20])
{
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 5; ++i) {
            out[i+j*5] = i == 4 ? a[4+j*5] : 0;
            for (int k = 0; k < 4; ++k)
                out[i+j*5] += a[k+j*5] * b[i+k*5];
        }
    }
}

// To detect if we need to apply clamping after applying a matrix, we check if
// any output component might go outside of [0, 255] for any combination of
// input components in [0..255].
// Each output component is an affine transformation of the input component, so
// the minimum and maximum values are for any combination of minimum or maximum
// values of input components (i.e. 0 or 255).
// E.g. if R' = x*R + y*G + z*B + w*A + t
// Then the maximum value will be for R=255 if x>0 or R=0 if x<0, and the
// minimum value will be for R=0 if x>0 or R=255 if x<0.
// Same goes for all components.
bool componentNeedsClamping(SkScalar row[5])
{
    SkScalar maxValue = row[4] / 255;
    SkScalar minValue = row[4] / 255;
    for (int i = 0; i < 4; ++i) {
        if (row[i] > 0)
            maxValue += row[i];
        else
            minValue += row[i];
    }
    return (maxValue > 1) || (minValue < 0);
}

bool matrixNeedsClamping(SkScalar matrix[20])
{
    return componentNeedsClamping(matrix)
        || componentNeedsClamping(matrix+5)
        || componentNeedsClamping(matrix+10)
        || componentNeedsClamping(matrix+15);
}

bool getColorMatrix(const WebKit::WebFilterOperation& op, SkScalar matrix[20])
{
    switch (op.type()) {
    case WebKit::WebFilterOperation::FilterTypeBrightness: {
        getBrightnessMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeContrast: {
        getContrastMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeGrayscale: {
        getGrayscaleMatrix(1 - op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeSepia: {
        getSepiaMatrix(1 - op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeSaturate: {
        getSaturateMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeHueRotate: {
        getHueRotateMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeInvert: {
        getInvertMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeOpacity: {
        getOpacityMatrix(op.amount(), matrix);
        return true;
    }
    case WebKit::WebFilterOperation::FilterTypeColorMatrix: {
        memcpy(matrix, op.matrix(), sizeof(SkScalar[20]));
        return true;
    }
    default:
        return false;
    }
}

class FilterBufferState {
public:
    FilterBufferState(GrContext* grContext, const WebCore::FloatSize& size, unsigned textureId)
        : m_grContext(grContext)
        , m_currentTexture(0)
    {
        // Wrap the source texture in a Ganesh platform texture.
        GrPlatformTextureDesc platformTextureDescription;
        platformTextureDescription.fWidth = size.width();
        platformTextureDescription.fHeight = size.height();
        platformTextureDescription.fConfig = kSkia8888_PM_GrPixelConfig;
        platformTextureDescription.fTextureHandle = textureId;
        SkAutoTUnref<GrTexture> texture(grContext->createPlatformTexture(platformTextureDescription));
        // Place the platform texture inside an SkBitmap.
        m_source.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
        m_source.setPixelRef(new SkGrTexturePixelRef(texture.get()))->unref();
    }

    ~FilterBufferState() { }

    bool init(int filterCount)
    {
        int scratchCount = std::min(2, filterCount);
        GrTextureDesc desc;
        desc.fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
        desc.fSampleCnt = 0;
        desc.fWidth = m_source.width();
        desc.fHeight = m_source.height();
        desc.fConfig = kSkia8888_PM_GrPixelConfig;
        for (int i = 0; i < scratchCount; ++i) {
            GrAutoScratchTexture scratchTexture(m_grContext, desc, GrContext::kExact_ScratchTexMatch);
            m_scratchTextures[i].reset(scratchTexture.detach());
            if (!m_scratchTextures[i].get())
                return false;
        }
        return true;
    }

    SkCanvas* canvas()
    {
        if (!m_canvas.get())
            createCanvas();
        return m_canvas.get();
    }

    const SkBitmap& source() { return m_source; }

    void swap()
    {
        m_canvas.reset(0);
        m_device.reset(0);

        m_source.setPixelRef(new SkGrTexturePixelRef(m_scratchTextures[m_currentTexture].get()))->unref();
        m_currentTexture = 1 - m_currentTexture;
    }

private:
    void createCanvas()
    {
        ASSERT(m_scratchTextures[m_currentTexture].get());
        m_device.reset(new SkGpuDevice(m_grContext, m_scratchTextures[m_currentTexture].get()));
        m_canvas.reset(new SkCanvas(m_device.get()));
        m_canvas->clear(0x0);
    }

    GrContext* m_grContext;
    SkBitmap m_source;
    SkAutoTUnref<GrTexture> m_scratchTextures[2];
    int m_currentTexture;
    SkAutoTUnref<SkGpuDevice> m_device;
    SkAutoTUnref<SkCanvas> m_canvas;
};

}

namespace WebCore {

WebKit::WebFilterOperations CCRenderSurfaceFilters::optimize(const WebKit::WebFilterOperations& filters)
{
    WebKit::WebFilterOperations newList;

    SkScalar accumulatedColorMatrix[20];
    bool haveAccumulatedColorMatrix = false;
    for (unsigned i = 0; i < filters.size(); ++i) {
        const WebKit::WebFilterOperation& op = filters.at(i);

        // If the filter is a color matrix, we may be able to combine it with
        // following filter(s) that also are color matrices.
        SkScalar matrix[20];
        if (getColorMatrix(op, matrix)) {
            if (haveAccumulatedColorMatrix) {
                SkScalar newMatrix[20];
                multColorMatrix(matrix, accumulatedColorMatrix, newMatrix);
                memcpy(accumulatedColorMatrix, newMatrix, sizeof(accumulatedColorMatrix));
            } else {
                memcpy(accumulatedColorMatrix, matrix, sizeof(accumulatedColorMatrix));
                haveAccumulatedColorMatrix = true;
            }

            // We can only combine matrices if clamping of color components
            // would have no effect.
            if (!matrixNeedsClamping(accumulatedColorMatrix))
                continue;
        }

        if (haveAccumulatedColorMatrix)
            newList.append(WebKit::WebFilterOperation::createColorMatrixFilter(accumulatedColorMatrix));
        haveAccumulatedColorMatrix = false;

        switch (op.type()) {
        case WebKit::WebFilterOperation::FilterTypeBlur:
        case WebKit::WebFilterOperation::FilterTypeDropShadow:
        case WebKit::WebFilterOperation::FilterTypeZoom:
            newList.append(op);
            break;
        case WebKit::WebFilterOperation::FilterTypeBrightness:
        case WebKit::WebFilterOperation::FilterTypeContrast:
        case WebKit::WebFilterOperation::FilterTypeGrayscale:
        case WebKit::WebFilterOperation::FilterTypeSepia:
        case WebKit::WebFilterOperation::FilterTypeSaturate:
        case WebKit::WebFilterOperation::FilterTypeHueRotate:
        case WebKit::WebFilterOperation::FilterTypeInvert:
        case WebKit::WebFilterOperation::FilterTypeOpacity:
        case WebKit::WebFilterOperation::FilterTypeColorMatrix:
            break;
        }
    }
    if (haveAccumulatedColorMatrix)
        newList.append(WebKit::WebFilterOperation::createColorMatrixFilter(accumulatedColorMatrix));
    return newList;
}

SkBitmap CCRenderSurfaceFilters::apply(const WebKit::WebFilterOperations& filters, unsigned textureId, const FloatSize& size, WebKit::WebGraphicsContext3D* context3D, GrContext* grContext)
{
    if (!context3D || !grContext)
        return SkBitmap();

    WebKit::WebFilterOperations optimizedFilters = optimize(filters);
    FilterBufferState state(grContext, size, textureId);
    if (!state.init(optimizedFilters.size()))
        return SkBitmap();

    for (unsigned i = 0; i < optimizedFilters.size(); ++i) {
        const WebKit::WebFilterOperation& op = optimizedFilters.at(i);
        SkCanvas* canvas = state.canvas();
        switch (op.type()) {
        case WebKit::WebFilterOperation::FilterTypeColorMatrix: {
            SkPaint paint;
            paint.setColorFilter(new SkColorMatrixFilter(op.matrix()))->unref();
            canvas->drawBitmap(state.source(), 0, 0, &paint);
            break;
        }
        case WebKit::WebFilterOperation::FilterTypeBlur: {
            float stdDeviation = op.amount();
            SkAutoTUnref<SkImageFilter> filter(new SkBlurImageFilter(stdDeviation, stdDeviation));
            SkPaint paint;
            paint.setImageFilter(filter.get());
            canvas->drawSprite(state.source(), 0, 0, &paint);
            break;
        }
        case WebKit::WebFilterOperation::FilterTypeDropShadow: {
            SkAutoTUnref<SkImageFilter> blurFilter(new SkBlurImageFilter(op.amount(), op.amount()));
            SkAutoTUnref<SkColorFilter> colorFilter(SkColorFilter::CreateModeFilter(op.dropShadowColor(), SkXfermode::kSrcIn_Mode));
            SkPaint paint;
            paint.setImageFilter(blurFilter.get());
            paint.setColorFilter(colorFilter.get());
            paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
            canvas->saveLayer(0, &paint);
            canvas->drawBitmap(state.source(), op.dropShadowOffset().x, -op.dropShadowOffset().y);
            canvas->restore();
            canvas->drawBitmap(state.source(), 0, 0);
            break;
        }
        case WebKit::WebFilterOperation::FilterTypeZoom: {
            SkPaint paint;
            SkAutoTUnref<SkImageFilter> zoomFilter(
                new SkMagnifierImageFilter(
                    SkRect::MakeXYWH(op.zoomRect().x,
                                     op.zoomRect().y,
                                     op.zoomRect().width,
                                     op.zoomRect().height),
                    op.amount()));
            paint.setImageFilter(zoomFilter.get());
            canvas->saveLayer(0, &paint);
            canvas->drawBitmap(state.source(), 0, 0);
            break;
        }
        case WebKit::WebFilterOperation::FilterTypeBrightness:
        case WebKit::WebFilterOperation::FilterTypeContrast:
        case WebKit::WebFilterOperation::FilterTypeGrayscale:
        case WebKit::WebFilterOperation::FilterTypeSepia:
        case WebKit::WebFilterOperation::FilterTypeSaturate:
        case WebKit::WebFilterOperation::FilterTypeHueRotate:
        case WebKit::WebFilterOperation::FilterTypeInvert:
        case WebKit::WebFilterOperation::FilterTypeOpacity:
            ASSERT_NOT_REACHED();
            break;
        }
        state.swap();
    }
    context3D->flush();
    return state.source();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
