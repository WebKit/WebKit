/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContext.h"

#include "COMPtr.h"
#include "DisplayListRecorder.h"
#include "FloatRoundedRect.h"
#include "GraphicsContextPlatformPrivateDirect2D.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "URL.h"
#include <d2d1.h>
#include <d2d1effects.h>
#include <dwrite.h>

#pragma warning (disable : 4756)


namespace WebCore {
using namespace std;

GraphicsContext::GraphicsContext(HDC hdc, bool hasAlpha)
{
    platformInit(hdc, hasAlpha);
}

GraphicsContext::GraphicsContext(HDC hdc, ID2D1DCRenderTarget** renderTarget, RECT rect, bool hasAlpha)
{
    m_data->m_hdc = hdc;

    // Create a DC render target.
    auto targetProperties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);

    HRESULT hr = GraphicsContext::systemFactory()->CreateDCRenderTarget(&targetProperties, renderTarget);
    RELEASE_ASSERT(SUCCEEDED(hr));

    (*renderTarget)->BindDC(hdc, &rect);

    m_data = new GraphicsContextPlatformPrivate(*renderTarget);
}

ID2D1Factory* GraphicsContext::systemFactory()
{
    static ID2D1Factory* direct2DFactory = nullptr;
    if (!direct2DFactory) {
#ifndef NDEBUG
        D2D1_FACTORY_OPTIONS options = { };
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, options, &direct2DFactory);
#else
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &direct2DFactory);
#endif
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return direct2DFactory;
}

ID2D1RenderTarget* GraphicsContext::defaultRenderTarget()
{
    static ID2D1RenderTarget* defaultRenderTarget = nullptr;
    if (!defaultRenderTarget) {
        auto renderTargetProperties = D2D1::RenderTargetProperties();
        renderTargetProperties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
        auto hwndRenderTargetProperties = D2D1::HwndRenderTargetProperties(::GetDesktopWindow(), D2D1::SizeU(10, 10));
        HRESULT hr = systemFactory()->CreateHwndRenderTarget(&renderTargetProperties, &hwndRenderTargetProperties, reinterpret_cast<ID2D1HwndRenderTarget**>(&defaultRenderTarget));
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return defaultRenderTarget;
}

void GraphicsContext::platformInit(HDC hdc, bool hasAlpha)
{
    if (!hdc)
        return;

    HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));

    DIBPixelData pixelData(bitmap);

    auto targetProperties = D2D1::RenderTargetProperties();
    targetProperties.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);

    COMPtr<ID2D1DCRenderTarget> renderTarget;
    HRESULT hr = systemFactory()->CreateDCRenderTarget(&targetProperties, &renderTarget);
    if (!SUCCEEDED(hr))
        return;

    RECT clientRect = IntRect(IntPoint(), pixelData.size());
    hr = renderTarget->BindDC(hdc, &clientRect);
    if (!SUCCEEDED(hr))
        return;

    m_data = new GraphicsContextPlatformPrivate(renderTarget.get());
    m_data->m_hdc = hdc;
    // Make sure the context starts in sync with our state.
    setPlatformFillColor(fillColor());
    setPlatformStrokeColor(strokeColor());
    setPlatformStrokeThickness(strokeThickness());
    // FIXME: m_state.imageInterpolationQuality = convertInterpolationQuality(CGContextGetInterpolationQuality(platformContext()));
}

void GraphicsContext::platformInit(ID2D1RenderTarget* renderTarget)
{
    if (!renderTarget)
        return;

    m_data = new GraphicsContextPlatformPrivate(renderTarget);

    // Make sure the context starts in sync with our state.
    setPlatformFillColor(fillColor());
    setPlatformStrokeColor(strokeColor());
    setPlatformStrokeThickness(strokeThickness());
    // FIXME: m_state.imageInterpolationQuality = convertInterpolationQuality(CGContextGetInterpolationQuality(platformContext()));
}

void GraphicsContext::platformDestroy()
{
    delete m_data;
}

ID2D1RenderTarget* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return m_data->renderTarget();
}

ID2D1RenderTarget* GraphicsContextPlatformPrivate::renderTarget()
{
    if (!m_transparencyLayerStack.isEmpty())
        return m_transparencyLayerStack.last().renderTarget.get();

    return m_renderTarget.get();
}

void GraphicsContextPlatformPrivate::setAlpha(float alpha)
{
    ASSERT(m_transparencyLayerStack.isEmpty());
    m_alpha = alpha;
}

float GraphicsContextPlatformPrivate::currentGlobalAlpha() const
{
    if (!m_transparencyLayerStack.isEmpty())
        return m_transparencyLayerStack.last().opacity;

    return m_alpha;
}

void GraphicsContext::savePlatformState()
{
    ASSERT(!paintingDisabled());
    ASSERT(!m_impl);

    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // save of the secondary context (in GraphicsContextPlatformPrivateDirect2D.h).
    m_data->save();
}

void GraphicsContext::restorePlatformState()
{
    ASSERT(!paintingDisabled());
    ASSERT(!m_impl);

    // Note: Do not use this function within this class implementation, since we want to avoid the extra
    // restore of the secondary context (in GraphicsContextPlatformPrivateDirect2D.h).
    m_data->restore();
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::drawNativeImage(const COMPtr<ID2D1Bitmap>& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode, ImageOrientation orientation)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        // FIXME: Implement DisplayListRecorder support for drawNativeImage.
        // m_displayListRecorder->drawNativeImage(image, imageSize, destRect, srcRect, op, blendMode, orientation);
        notImplemented();
        return;
    }

    auto bitmapSize = image->GetSize();

    float currHeight = orientation.usesWidthAsHeight() ? bitmapSize.width : bitmapSize.height;
    if (currHeight <= srcRect.y())
        return;

    auto context = platformContext();

    D2D1_MATRIX_3X2_F ctm;
    context->GetTransform(&ctm);

    AffineTransform transform(ctm);

    D2DContextStateSaver stateSaver(*m_data);

    bool shouldUseSubimage = false;

    // If the source rect is a subportion of the image, then we compute an inflated destination rect that will hold the entire image
    // and then set a clip to the portion that we want to display.
    FloatRect adjustedDestRect = destRect;

    if (srcRect.size() != imageSize) {
        // FIXME: Implement image scaling
        notImplemented();
    }

    // If the image is only partially loaded, then shrink the destination rect that we're drawing into accordingly.
    if (!shouldUseSubimage && currHeight < imageSize.height())
        adjustedDestRect.setHeight(adjustedDestRect.height() * currHeight / imageSize.height());

    setPlatformCompositeOperation(op, blendMode);

    // ImageOrientation expects the origin to be at (0, 0).
    transform.translate(adjustedDestRect.x(), adjustedDestRect.y());
    context->SetTransform(transform);
    adjustedDestRect.setLocation(FloatPoint());

    if (orientation != DefaultImageOrientation) {
        this->concatCTM(orientation.transformFromDefault(adjustedDestRect.size()));
        if (orientation.usesWidthAsHeight()) {
            // The destination rect will have it's width and height already reversed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            adjustedDestRect = FloatRect(adjustedDestRect.x(), adjustedDestRect.y(), adjustedDestRect.height(), adjustedDestRect.width());
        }
    }

    context->SetTags(1, __LINE__);

    drawWithoutShadow(adjustedDestRect, [this, image, adjustedDestRect, srcRect](ID2D1RenderTarget* renderTarget) {
        renderTarget->DrawBitmap(image.get(), adjustedDestRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, static_cast<D2D1_RECT_F>(srcRect));
    });

    flush();

    if (!stateSaver.didSave())
        context->SetTransform(ctm);
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend)
{
    bool createdBitmap = m_impl || !m_data->m_hdc || isInTransparencyLayer();
    if (!createdBitmap) {
        m_data->restore();
        return;
    }

    if (!hdc || dstRect.isEmpty())
        return;

    auto sourceBitmap = adoptGDIObject(static_cast<HBITMAP>(::GetCurrentObject(hdc, OBJ_BITMAP)));

    DIBPixelData pixelData(sourceBitmap.get());
    ASSERT(pixelData.bitsPerPixel() == 32);

    auto bitmapProperties = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));

    COMPtr<ID2D1Bitmap> bitmap;
    HRESULT hr = platformContext()->CreateBitmap(pixelData.size(), pixelData.buffer(), pixelData.bytesPerRow(), &bitmapProperties, &bitmap);
    ASSERT(SUCCEEDED(hr));

    D2DContextStateSaver stateSaver(*m_data);

    // Note: The content in the HDC is inverted compared to Direct2D, so it needs to be flipped.
    auto context = platformContext();

    D2D1_MATRIX_3X2_F currentTransform;
    context->GetTransform(&currentTransform);

    AffineTransform transform(currentTransform);
    transform.translate(dstRect.location());
    transform.scale(1.0, -1.0);
    transform.translate(0, -dstRect.height());

    context->SetTransform(transform);
    context->DrawBitmap(bitmap.get(), D2D1::RectF(0, 0, dstRect.width(), dstRect.height()));

    ::DeleteDC(hdc);
}

void GraphicsContext::drawWindowsBitmap(WindowsBitmap* image, const IntPoint& point)
{
}

void GraphicsContext::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
}

void GraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
}

void GraphicsContext::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
}

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(ID2D1RenderTarget* renderTarget)
    : m_renderTarget(renderTarget)
{
    if (!m_renderTarget)
        return;

    beginDraw();
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
    if (!m_renderTarget)
        return;

    endDraw();
}

COMPtr<ID2D1SolidColorBrush> GraphicsContextPlatformPrivate::brushWithColor(const D2D1_COLOR_F& color)
{
    RGBA32 colorKey = makeRGBA32FromFloats(color.r, color.g, color.b, color.a);

    if (!colorKey) {
        if (!m_zeroBrush)
            m_renderTarget->CreateSolidColorBrush(color, &m_zeroBrush);
        return m_zeroBrush;
    }

    if (colorKey == 0xFFFFFFFF) {
        if (!m_whiteBrush)
            m_renderTarget->CreateSolidColorBrush(color, &m_whiteBrush);
        return m_whiteBrush;
    }

    auto existingBrush = m_solidColoredBrushCache.ensure(colorKey, [this, color] {
        COMPtr<ID2D1SolidColorBrush> colorBrush;
        m_renderTarget->CreateSolidColorBrush(color, &colorBrush);
        return colorBrush;
    });

    return existingBrush.iterator->value;
}

ID2D1SolidColorBrush* GraphicsContext::brushWithColor(const Color& color)
{
    return m_data->brushWithColor(colorWithGlobalAlpha(color)).get();
}

void GraphicsContextPlatformPrivate::clip(const FloatRect& rect)
{
    if (m_renderStates.isEmpty())
        save();

    m_renderTarget->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    m_renderStates.last().m_clips.append(GraphicsContextPlatformPrivate::AxisAlignedClip);
}

void GraphicsContextPlatformPrivate::clip(const Path& path)
{
    clip(path.platformPath());
}

void GraphicsContextPlatformPrivate::clip(ID2D1Geometry* path)
{
    ASSERT(m_renderStates.size());
    if (!m_renderStates.size())
        return;

    COMPtr<ID2D1Layer> clipLayer;
    HRESULT hr = m_renderTarget->CreateLayer(&clipLayer);
    ASSERT(SUCCEEDED(hr));
    if (!SUCCEEDED(hr))
        return;

    m_renderTarget->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), path), clipLayer.get());
    m_renderStates.last().m_clips.append(GraphicsContextPlatformPrivate::LayerClip);
    m_renderStates.last().m_activeLayer = clipLayer;
}

void GraphicsContextPlatformPrivate::concatCTM(const AffineTransform& affineTransform)
{
    ASSERT(m_renderTarget.get());

    D2D1_MATRIX_3X2_F currentTransform;
    m_renderTarget->GetTransform(&currentTransform);

    D2D1_MATRIX_3X2_F transformToConcat = affineTransform;
    m_renderTarget->SetTransform(transformToConcat * currentTransform);
}

void GraphicsContextPlatformPrivate::flush()
{
    ASSERT(m_renderTarget.get());
    D2D1_TAG first, second;
    HRESULT hr = m_renderTarget->Flush(&first, &second);

    RELEASE_ASSERT(SUCCEEDED(hr));
}

void GraphicsContextPlatformPrivate::beginDraw()
{
    ASSERT(m_renderTarget.get());
    m_renderTarget->BeginDraw();
}

void GraphicsContextPlatformPrivate::endDraw()
{
    ASSERT(m_renderTarget.get());
    D2D1_TAG first, second;
    HRESULT hr = m_renderTarget->EndDraw(&first, &second);

    if (!SUCCEEDED(hr))
        WTFLogAlways("Failed in GraphicsContextPlatformPrivate::endDraw: hr=%ld, first=%ld, second=%ld", hr, first, second);
}

void GraphicsContextPlatformPrivate::restore()
{
    ASSERT(m_renderTarget.get());

    auto restoreState = m_renderStates.takeLast();
    m_renderTarget->RestoreDrawingState(restoreState.m_drawingStateBlock.get());

    for (auto clipType = restoreState.m_clips.rbegin(); clipType != restoreState.m_clips.rend(); ++clipType) {
        if (*clipType == GraphicsContextPlatformPrivate::AxisAlignedClip)
            m_renderTarget->PopAxisAlignedClip();
        else
            m_renderTarget->PopLayer();
    }
}

void GraphicsContextPlatformPrivate::save()
{
    ASSERT(m_renderTarget.get());

    RenderState currentState;
    GraphicsContext::systemFactory()->CreateDrawingStateBlock(&currentState.m_drawingStateBlock);

    m_renderTarget->SaveDrawingState(currentState.m_drawingStateBlock.get());

    m_renderStates.append(currentState);
}

void GraphicsContextPlatformPrivate::scale(const FloatSize& size)
{
    ASSERT(m_renderTarget.get());

    D2D1_MATRIX_3X2_F currentTransform;
    m_renderTarget->GetTransform(&currentTransform);

    auto scale = D2D1::Matrix3x2F::Scale(size);
    m_renderTarget->SetTransform(scale * currentTransform);
}

void GraphicsContextPlatformPrivate::setCTM(const AffineTransform& transform)
{
    ASSERT(m_renderTarget.get());
    m_renderTarget->SetTransform(transform);
}

void GraphicsContextPlatformPrivate::translate(float x, float y)
{
    ASSERT(m_renderTarget.get());

    D2D1_MATRIX_3X2_F currentTransform;
    m_renderTarget->GetTransform(&currentTransform);

    auto translation = D2D1::Matrix3x2F::Translation(x, y);
    m_renderTarget->SetTransform(translation * currentTransform);
}

void GraphicsContextPlatformPrivate::rotate(float angle)
{
    ASSERT(m_renderTarget.get());

    D2D1_MATRIX_3X2_F currentTransform;
    m_renderTarget->GetTransform(&currentTransform);

    auto rotation = D2D1::Matrix3x2F::Rotation(rad2deg(angle));
    m_renderTarget->SetTransform(rotation * currentTransform);
}

D2D1_COLOR_F GraphicsContext::colorWithGlobalAlpha(const Color& color) const
{
    float colorAlpha = color.alphaAsFloat();
    float globalAlpha = m_data->currentGlobalAlpha();

    return D2D1::ColorF(color.rgb(), globalAlpha * colorAlpha);
}

ID2D1Brush* GraphicsContext::solidStrokeBrush() const
{
    return m_data->m_solidStrokeBrush.get();
}

ID2D1Brush* GraphicsContext::solidFillBrush() const
{
    return m_data->m_solidFillBrush.get();
}

ID2D1Brush* GraphicsContext::patternStrokeBrush() const
{
    return m_data->m_patternStrokeBrush.get();
}

ID2D1Brush* GraphicsContext::patternFillBrush() const
{
    return m_data->m_patternFillBrush.get();
}

void GraphicsContext::beginDraw()
{
    m_data->beginDraw();
}

void GraphicsContext::endDraw()
{
    m_data->endDraw();
}

void GraphicsContext::flush()
{
    m_data->flush();
}

void GraphicsContext::drawPattern(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, BlendMode blendMode)
{
    if (paintingDisabled() || !patternTransform.isInvertible())
        return;

    if (m_impl) {
        m_impl->drawPattern(image, destRect, tileRect, patternTransform, phase, spacing, op, blendMode);
        return;
    }

    auto context = platformContext();
    D2DContextStateSaver stateSaver(*m_data);

    m_data->clip(destRect);

    setPlatformCompositeOperation(op, blendMode);

    auto bitmapBrushProperties = D2D1::BitmapBrushProperties();
    bitmapBrushProperties.extendModeX = D2D1_EXTEND_MODE_WRAP;
    bitmapBrushProperties.extendModeY = D2D1_EXTEND_MODE_WRAP;

    // Create a brush transformation so we paint using the section of the image we care about.
    AffineTransform transformation = patternTransform;
    transformation.translate(destRect.location());

    auto brushProperties = D2D1::BrushProperties();
    brushProperties.transform = transformation;
    brushProperties.opacity = 1.0f;

    auto tileImage = image.nativeImageForCurrentFrame();

    // If we only want a subset of the bitmap, we need to create a cropped bitmap image. According to the documentation,
    // this does not allocate new bitmap memory.
    if (image.width() > destRect.width() || image.height() > destRect.height()) {
        float dpiX = 0;
        float dpiY = 0;
        tileImage->GetDpi(&dpiX, &dpiY);
        auto bitmapProperties = D2D1::BitmapProperties(tileImage->GetPixelFormat(), dpiX, dpiY);
        COMPtr<ID2D1Bitmap> subImage;
        HRESULT hr = context->CreateBitmap(IntSize(tileRect.size()), bitmapProperties, &subImage);
        if (SUCCEEDED(hr)) {
            D2D1_RECT_U finishRect = IntRect(tileRect);
            hr = subImage->CopyFromBitmap(nullptr, tileImage.get(), &finishRect);
            if (SUCCEEDED(hr))
                tileImage = subImage;
        }
    }

    COMPtr<ID2D1BitmapBrush> patternBrush;
    HRESULT hr = context->CreateBitmapBrush(tileImage.get(), &bitmapBrushProperties, &brushProperties, &patternBrush);

    drawWithoutShadow(destRect, [this, destRect, patternBrush](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = destRect;
        renderTarget->FillRectangle(&d2dRect, patternBrush.get());
    });
}

void GraphicsContext::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    if (paintingDisabled())
        return;

    FloatSize bufferDestinationSize = buffer.sizeForDestinationSize(destRect.size());
    notImplemented();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const FloatRect& rect, float borderThickness)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawRect(rect, borderThickness);
        return;
    }

    // FIXME: this function does not handle patterns and gradients like drawPath does, it probably should.
    ASSERT(!rect.isEmpty());

    auto context = platformContext();

    context->SetTags(1, __LINE__);

    drawWithoutShadow(rect, [this, rect](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = rect;
        renderTarget->FillRectangle(&d2dRect, solidFillBrush());
        renderTarget->DrawRectangle(&d2dRect, solidStrokeBrush(), strokeThickness(), m_data->strokeStyle());
    });
}

void GraphicsContextPlatformPrivate::setLineCap(LineCap cap)
{
    if (m_lineCap == cap)
        return;

    D2D1_CAP_STYLE capStyle = D2D1_CAP_STYLE_FLAT;
    switch (cap) {
    case RoundCap:
        capStyle = D2D1_CAP_STYLE_ROUND;
        break;
    case SquareCap:
        capStyle = D2D1_CAP_STYLE_SQUARE;
        break;
    case ButtCap:
    default:
        capStyle = D2D1_CAP_STYLE_FLAT;
        break;
    }

    m_lineCap = capStyle;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::setLineJoin(LineJoin join)
{
    if (m_lineJoin == join)
        return;

    D2D1_LINE_JOIN joinStyle = D2D1_LINE_JOIN_MITER;
    switch (join) {
    case RoundJoin:
        joinStyle = D2D1_LINE_JOIN_ROUND;
        break;
    case BevelJoin:
        joinStyle = D2D1_LINE_JOIN_BEVEL;
        break;
    case MiterJoin:
    default:
        joinStyle = D2D1_LINE_JOIN_MITER;
        break;
    }

    m_lineJoin = joinStyle;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::setStrokeStyle(StrokeStyle strokeStyle)
{
    if (m_strokeStyle == strokeStyle)
        return;

    m_strokeStyle = strokeStyle;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::setMiterLimit(float miterLimit)
{
    if (WTF::areEssentiallyEqual(miterLimit, m_miterLimit))
        return;

    m_miterLimit = miterLimit;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::setDashOffset(float dashOffset)
{
    if (WTF::areEssentiallyEqual(dashOffset, m_dashOffset))
        return;

    m_dashOffset = dashOffset;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::setPatternWidth(float patternWidth)
{
    if (WTF::areEssentiallyEqual(patternWidth, m_patternWidth))
        return;

    m_patternWidth = patternWidth;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::setPatternOffset(float patternOffset)
{
    if (WTF::areEssentiallyEqual(patternOffset, m_patternOffset))
        return;

    m_patternOffset = patternOffset;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::setStrokeThickness(float thickness)
{
    if (WTF::areEssentiallyEqual(thickness, m_strokeThickness))
        return;

    m_strokeThickness = thickness;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::setDashes(const DashArray& dashes)
{
    if (m_dashes == dashes)
        return;

    m_dashes = dashes;
    m_strokeSyleIsDirty = true;
}

void GraphicsContextPlatformPrivate::recomputeStrokeStyle()
{
    if (!m_strokeSyleIsDirty)
        return;

    m_d2dStrokeStyle = nullptr;

    if ((m_strokeStyle != SolidStroke) && (m_strokeStyle != NoStroke)) {
        float patternOffset = m_patternOffset / m_strokeThickness;

        DashArray dashes = m_dashes;

        // In Direct2D, dashes and dots are defined in terms of the ratio of the dash length to the line thickness.
        for (auto& dash : dashes)
            dash /= m_strokeThickness;

        auto strokeStyleProperties = D2D1::StrokeStyleProperties(m_lineCap, m_lineCap, m_lineCap, m_lineJoin, m_strokeThickness, D2D1_DASH_STYLE_CUSTOM, patternOffset);
        GraphicsContext::systemFactory()->CreateStrokeStyle(&strokeStyleProperties, dashes.data(), dashes.size(), &m_d2dStrokeStyle);
    }

    m_strokeSyleIsDirty = false;
}

ID2D1StrokeStyle* GraphicsContextPlatformPrivate::strokeStyle()
{
    recomputeStrokeStyle();
    return m_d2dStrokeStyle.get();
}

ID2D1StrokeStyle* GraphicsContext::platformStrokeStyle() const
{
    return m_data->strokeStyle();
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (paintingDisabled())
        return;

    if (strokeStyle() == NoStroke)
        return;

    if (m_impl) {
        m_impl->drawLine(point1, point2);
        return;
    }

    float thickness = strokeThickness();
    bool isVerticalLine = (point1.x() + thickness == point2.x());
    float strokeWidth = isVerticalLine ? point2.y() - point1.y() : point2.x() - point1.x();
    if (!thickness || !strokeWidth)
        return;

    auto context = platformContext();

    StrokeStyle strokeStyle = this->strokeStyle();
    float cornerWidth = 0;
    bool drawsDashedLine = strokeStyle == DottedStroke || strokeStyle == DashedStroke;

    COMPtr<ID2D1StrokeStyle> d2dStrokeStyle;
    D2DContextStateSaver stateSaver(*m_data, drawsDashedLine);
    if (drawsDashedLine) {
        // Figure out end points to ensure we always paint corners.
        cornerWidth = dashedLineCornerWidthForStrokeWidth(strokeWidth);
        strokeWidth -= 2 * cornerWidth;
        float patternWidth = dashedLinePatternWidthForStrokeWidth(strokeWidth);
        // Check if corner drawing sufficiently covers the line.
        if (strokeWidth <= patternWidth + 1)
            return;

        float patternOffset = dashedLinePatternOffsetForPatternAndStrokeWidth(patternWidth, strokeWidth);
        const float dashes[2] = { patternWidth, patternWidth };
        auto strokeStyleProperties = D2D1::StrokeStyleProperties();
        GraphicsContext::systemFactory()->CreateStrokeStyle(&strokeStyleProperties, dashes, ARRAYSIZE(dashes), &d2dStrokeStyle);

        m_data->setPatternWidth(patternWidth);
        m_data->setPatternOffset(patternOffset);
        m_data->setDashes(DashArray(2, patternWidth));

        d2dStrokeStyle = m_data->strokeStyle();
    }

    auto centeredPoints = centerLineAndCutOffCorners(isVerticalLine, cornerWidth, point1, point2);
    auto p1 = centeredPoints[0];
    auto p2 = centeredPoints[1];

    context->SetTags(1, __LINE__);

    FloatRect boundingRect(p1, p2);

    drawWithoutShadow(boundingRect, [this, p1, p2, d2dStrokeStyle](ID2D1RenderTarget* renderTarget) {
        renderTarget->DrawLine(p1, p2, solidStrokeBrush(), strokeThickness(), d2dStrokeStyle.get());
    });
}

void GraphicsContext::drawEllipse(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawEllipse(rect);
        return;
    }

    auto ellipse = D2D1::Ellipse(rect.center(), 0.5 * rect.width(), 0.5 * rect.height());

    auto context = platformContext();

    context->SetTags(1, __LINE__);

    drawWithoutShadow(rect, [this, ellipse](ID2D1RenderTarget* renderTarget) {
        renderTarget->FillEllipse(&ellipse, solidFillBrush());

        renderTarget->DrawEllipse(&ellipse, solidStrokeBrush(), strokeThickness(), m_data->strokeStyle());
    });
}

void GraphicsContext::applyStrokePattern()
{
    if (paintingDisabled())
        return;

    auto context = platformContext();
    AffineTransform userToBaseCTM; // FIXME: This isn't really needed on Windows

    const float patternAlpha = 1;
    m_data->m_patternStrokeBrush = adoptCOM(m_state.strokePattern->createPlatformPattern(*this, patternAlpha, userToBaseCTM));
}

void GraphicsContext::applyFillPattern()
{
    if (paintingDisabled())
        return;

    auto context = platformContext();
    AffineTransform userToBaseCTM; // FIXME: This isn't really needed on Windows

    const float patternAlpha = 1;
    m_data->m_patternFillBrush = adoptCOM(m_state.fillPattern->createPlatformPattern(*this, patternAlpha, userToBaseCTM));
}

void GraphicsContext::drawPath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->drawPath(path);
        return;
    }

    auto context = platformContext();
    const GraphicsContextState& state = m_state;

    if (state.fillGradient || state.strokeGradient) {
        // We don't have any optimized way to fill & stroke a path using gradients
        // FIXME: Be smarter about this.
        fillPath(path);
        strokePath(path);
        return;
    }

    if (state.fillPattern)
        applyFillPattern();

    if (state.strokePattern)
        applyStrokePattern();

    if (path.activePath())
        path.activePath()->Close();

    context->SetTags(1, __LINE__);

    auto rect = path.fastBoundingRect();
    drawWithoutShadow(rect, [this, &path](ID2D1RenderTarget* renderTarget) {
        auto brush = m_state.strokePattern ? patternStrokeBrush() : solidStrokeBrush();
        renderTarget->DrawGeometry(path.platformPath(), brush, strokeThickness(), m_data->strokeStyle());
    });

    flush();
}

void GraphicsContext::drawWithoutShadow(const FloatRect& /*boundingRect*/, const WTF::Function<void(ID2D1RenderTarget*)>& drawCommands)
{
    drawCommands(platformContext());
}

static void drawWithShadowHelper(ID2D1RenderTarget* context, ID2D1Bitmap* bitmap, const Color& shadowColor, const FloatSize& shadowOffset, float shadowBlur)
{
    COMPtr<ID2D1DeviceContext> deviceContext;
    HRESULT hr = context->QueryInterface(&deviceContext);
    RELEASE_ASSERT(SUCCEEDED(hr));

    // Create the shadow effect
    COMPtr<ID2D1Effect> shadowEffect;
    hr = deviceContext->CreateEffect(CLSID_D2D1Shadow, &shadowEffect);
    RELEASE_ASSERT(SUCCEEDED(hr));

    shadowEffect->SetInput(0, bitmap);
    shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, static_cast<D2D1_VECTOR_4F>(shadowColor));
    shadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, shadowBlur);

    COMPtr<ID2D1Effect> transformEffect;
    hr = deviceContext->CreateEffect(CLSID_D2D12DAffineTransform, &transformEffect);
    RELEASE_ASSERT(SUCCEEDED(hr));

    transformEffect->SetInputEffect(0, shadowEffect.get());

    auto translation = D2D1::Matrix3x2F::Translation(shadowOffset.width(), shadowOffset.height());
    transformEffect->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX, translation);

    COMPtr<ID2D1Effect> compositor;
    hr = deviceContext->CreateEffect(CLSID_D2D1Composite, &compositor);
    RELEASE_ASSERT(SUCCEEDED(hr));

    compositor->SetInputEffect(0, transformEffect.get());
    compositor->SetInput(1, bitmap);

    // Flip the context
    D2D1_MATRIX_3X2_F ctm;
    deviceContext->GetTransform(&ctm);
    auto translate = D2D1::Matrix3x2F::Translation(0.0f, deviceContext->GetSize().height);
    auto flip = D2D1::Matrix3x2F::Scale(D2D1::SizeF(1.0f, -1.0f));
    deviceContext->SetTransform(ctm * flip * translate);

    deviceContext->DrawImage(compositor.get(), D2D1_INTERPOLATION_MODE_LINEAR);
}

void GraphicsContext::drawWithShadow(const FloatRect& boundingRect, const WTF::Function<void(ID2D1RenderTarget*)>& drawCommands)
{
    auto context = platformContext();

    // Render the current geometry to a bitmap context
    COMPtr<ID2D1BitmapRenderTarget> bitmapTarget;
    HRESULT hr = context->CreateCompatibleRenderTarget(&bitmapTarget);
    RELEASE_ASSERT(SUCCEEDED(hr));

    bitmapTarget->BeginDraw();
    drawCommands(bitmapTarget.get());
    hr = bitmapTarget->EndDraw();
    RELEASE_ASSERT(SUCCEEDED(hr));

    COMPtr<ID2D1Bitmap> bitmap;
    hr = bitmapTarget->GetBitmap(&bitmap);
    RELEASE_ASSERT(SUCCEEDED(hr));

    drawWithShadowHelper(context, bitmap.get(), m_state.shadowColor, m_state.shadowOffset, m_state.shadowBlur);
}

void GraphicsContext::fillPath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->fillPath(path);
        return;
    }

    if (path.activePath()) {
        // Make sure it's closed. This might fail if the path was already closed, so
        // ignore the return value.
        path.activePath()->Close();
    }

    D2DContextStateSaver stateSaver(*m_data);

    auto context = platformContext();

    context->SetTags(1, __LINE__);

    if (m_state.fillGradient) {
        context->SetTags(1, __LINE__);

        FloatRect boundingRect = path.fastBoundingRect();
        WTF::Function<void(ID2D1RenderTarget*)> drawFunction = [this, &path](ID2D1RenderTarget* renderTarget) {
            renderTarget->FillGeometry(path.platformPath(), m_state.fillGradient->createPlatformGradientIfNecessary(renderTarget));
        };

        if (hasShadow())
            drawWithShadow(boundingRect, drawFunction);
        else
            drawWithoutShadow(boundingRect, drawFunction);
        return;
    }

    if (m_state.fillPattern)
        applyFillPattern();

    COMPtr<ID2D1GeometryGroup> pathToFill;
    path.createGeometryWithFillMode(fillRule(), pathToFill);

    context->SetTags(1, __LINE__);

    FloatRect contextRect(FloatPoint(), context->GetSize());
    drawWithoutShadow(contextRect, [this, &pathToFill](ID2D1RenderTarget* renderTarget) {
        auto brush = m_state.fillPattern ? patternFillBrush() : solidFillBrush();
        renderTarget->FillGeometry(pathToFill.get(), brush);
    });

    flush();
}

void GraphicsContext::strokePath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->strokePath(path);
        return;
    }

    auto context = platformContext();
    
    context->SetTags(1, __LINE__);

    if (m_state.strokeGradient) {
        context->SetTags(1, __LINE__);

        D2DContextStateSaver stateSaver(*m_data);
        auto boundingRect = path.fastBoundingRect();
        WTF::Function<void(ID2D1RenderTarget*)> drawFunction = [this, &path](ID2D1RenderTarget* renderTarget) {
            renderTarget->DrawGeometry(path.platformPath(), m_state.strokeGradient->createPlatformGradientIfNecessary(renderTarget));
        };

        if (hasShadow())
            drawWithShadow(boundingRect, drawFunction);
        else
            drawWithoutShadow(boundingRect, drawFunction);

        return;
    }

    if (m_state.strokePattern)
        applyStrokePattern();

    context->SetTags(1, __LINE__);

    FloatRect contextRect(FloatPoint(), context->GetSize());
    drawWithoutShadow(contextRect, [this, &path](ID2D1RenderTarget* renderTarget) {
        auto brush = m_state.strokePattern ? patternStrokeBrush() : solidStrokeBrush();
        renderTarget->DrawGeometry(path.platformPath(), brush, strokeThickness(), m_data->strokeStyle());
    });

    flush();
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRect(rect);
        return;
    }

    auto context = platformContext();

    if (m_state.fillGradient) {
        context->SetTags(1, __LINE__);
        D2DContextStateSaver stateSaver(*m_data);
        WTF::Function<void(ID2D1RenderTarget*)> drawFunction = [this, rect](ID2D1RenderTarget* renderTarget) {
            const D2D1_RECT_F d2dRect = rect;
            renderTarget->FillRectangle(&d2dRect, m_state.fillGradient->createPlatformGradientIfNecessary(renderTarget));
        };

        if (hasShadow())
            drawWithShadow(rect, drawFunction);
        else
            drawWithoutShadow(rect, drawFunction);
        return;
    }

    if (m_state.fillPattern)
        applyFillPattern();

    context->SetTags(1, __LINE__);

    bool drawOwnShadow = !isAcceleratedContext() && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms; // Don't use ShadowBlur for canvas yet.
    if (drawOwnShadow) {
        // FIXME: Get ShadowBlur working on Direct2D
        // ShadowBlur contextShadow(m_state);
        // contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
        notImplemented();
    }

    drawWithoutShadow(rect, [this, rect](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = rect;
        auto brush = m_state.fillPattern ? patternFillBrush() : solidFillBrush();
        renderTarget->FillRectangle(&d2dRect, brush);
    });
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRect(rect, color);
        return;
    }

    auto context = platformContext();

    bool drawOwnShadow = !isAcceleratedContext() && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms; // Don't use ShadowBlur for canvas yet.
    if (drawOwnShadow) {
        // FIXME: Get ShadowBlur working on Direct2D
        // ShadowBlur contextShadow(m_state);
        // contextShadow.drawRectShadow(*this, FloatRoundedRect(rect));
        notImplemented();
    }

    context->SetTags(1, __LINE__);

    drawWithoutShadow(rect, [this, rect, color](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = rect;
        renderTarget->FillRectangle(&d2dRect, brushWithColor(color));
    });
}

void GraphicsContext::platformFillRoundedRect(const FloatRoundedRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    auto context = platformContext();

    bool drawOwnShadow = !isAcceleratedContext() && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms; // Don't use ShadowBlur for canvas yet.
    D2DContextStateSaver stateSaver(*m_data, drawOwnShadow);
    if (drawOwnShadow) {
        // FIXME: Get ShadowBlur working on Direct2D
        // ShadowBlur contextShadow(m_state);
        // contextShadow.drawRectShadow(*this, rect);
        notImplemented();
    }

    context->SetTags(1, __LINE__);

    const FloatRect& r = rect.rect();
    const FloatRoundedRect::Radii& radii = rect.radii();
    bool equalWidths = (radii.topLeft().width() == radii.topRight().width() && radii.topRight().width() == radii.bottomLeft().width() && radii.bottomLeft().width() == radii.bottomRight().width());
    bool equalHeights = (radii.topLeft().height() == radii.bottomLeft().height() && radii.bottomLeft().height() == radii.topRight().height() && radii.topRight().height() == radii.bottomRight().height());
    bool hasCustomFill = m_state.fillGradient || m_state.fillPattern;
    if (!hasCustomFill && equalWidths && equalHeights && radii.topLeft().width() * 2 == r.width() && radii.topLeft().height() * 2 == r.height()) {
        auto roundedRect = D2D1::RoundedRect(r, radii.topLeft().width(), radii.topLeft().height());
        context->FillRoundedRectangle(roundedRect, brushWithColor(color));
    } else {
        D2DContextStateSaver stateSaver(*m_data);
        setFillColor(color);

        Path path;
        path.addRoundedRect(rect);
        fillPath(path);
    }

    if (drawOwnShadow)
        stateSaver.restore();
}

void GraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRectWithRoundedHole(rect, roundedHoleRect, color);
        return;
    }

    auto context = platformContext();

    context->SetTags(1, __LINE__);

    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();

    setFillRule(WindRule::EvenOdd);
    setFillColor(color);

    // fillRectWithRoundedHole() assumes that the edges of rect are clipped out, so we only care about shadows cast around inside the hole.
    bool drawOwnShadow = !isAcceleratedContext() && hasBlurredShadow() && !m_state.shadowsIgnoreTransforms;
    D2DContextStateSaver stateSaver(*m_data, drawOwnShadow);
    if (drawOwnShadow) {
        // FIXME: Get ShadowBlur working on Direct2D
        // ShadowBlur contextShadow(m_state);
        // contextShadow.drawRectShadow(*this, rect);
        notImplemented();
    }

    fillPath(path);

    if (drawOwnShadow)
        stateSaver.restore();

    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clip(rect);
        return;
    }

    m_data->clip(rect);
}

void GraphicsContext::clipOut(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipOut(rect);
        return;
    }

    Path path;
    path.addRect(rect);

    clipOut(path);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipOut(path);
        return;
    }

    // To clip Out we need the intersection of the infinite
    // clipping rect and the path we just created.
    D2D1_SIZE_F rendererSize = platformContext()->GetSize();
    FloatRect clipBounds(0, 0, rendererSize.width, rendererSize.height);

    Path boundingRect;
    boundingRect.addRect(clipBounds);
    boundingRect.appendGeometry(path.platformPath());

    COMPtr<ID2D1GeometryGroup> pathToClip;
    boundingRect.createGeometryWithFillMode(WindRule::EvenOdd, pathToClip);

    m_data->clip(pathToClip.get());
}

void GraphicsContext::clipPath(const Path& path, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipPath(path, clipRule);
        return;
    }

    auto context = platformContext();
    if (path.isEmpty()) {
        m_data->clip(FloatRect());
        return;
    }

    COMPtr<ID2D1GeometryGroup> pathToClip;
    path.createGeometryWithFillMode(clipRule, pathToClip);

    m_data->clip(pathToClip.get());
}

IntRect GraphicsContext::clipBounds() const
{
    if (paintingDisabled())
        return IntRect();

    if (m_impl) {
        WTFLogAlways("Getting the clip bounds not yet supported with display lists");
        return IntRect(-2048, -2048, 4096, 4096); // FIXME: display lists.
    }

    D2D1_SIZE_F clipSize;
    if (auto clipLayer = m_data->clipLayer())
        clipSize = clipLayer->GetSize();
    else
        clipSize = platformContext()->GetSize();

    FloatRect clipBounds(IntPoint(), clipSize);

    return enclosingIntRect(clipBounds);
}

void GraphicsContextPlatformPrivate::beginTransparencyLayer(float opacity)
{
    TransparencyLayerState transparencyLayer;
    transparencyLayer.opacity = opacity;

    HRESULT hr = m_renderTarget->CreateCompatibleRenderTarget(&transparencyLayer.renderTarget);
    RELEASE_ASSERT(SUCCEEDED(hr));
    m_transparencyLayerStack.append(WTFMove(transparencyLayer));

    m_transparencyLayerStack.last().renderTarget->BeginDraw();
    m_transparencyLayerStack.last().renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
}

void GraphicsContext::beginPlatformTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    save();

    m_state.alpha = opacity;

    m_data->beginTransparencyLayer(opacity);
}

void GraphicsContextPlatformPrivate::endTransparencyLayer()
{
    auto currentLayer = m_transparencyLayerStack.takeLast();
    auto renderTarget = currentLayer.renderTarget;
    if (!renderTarget)
        return;

    HRESULT hr = renderTarget->EndDraw();
    RELEASE_ASSERT(SUCCEEDED(hr));

    COMPtr<ID2D1Bitmap> bitmap;
    hr = renderTarget->GetBitmap(&bitmap);
    RELEASE_ASSERT(SUCCEEDED(hr));

    auto context = this->renderTarget();

    if (currentLayer.hasShadow)
        drawWithShadowHelper(context, bitmap.get(), currentLayer.shadowColor, currentLayer.shadowOffset, currentLayer.shadowBlur);
    else {
        COMPtr<ID2D1BitmapBrush> bitmapBrush;
        auto bitmapBrushProperties = D2D1::BitmapBrushProperties();
        auto brushProperties = D2D1::BrushProperties();
        HRESULT hr = context->CreateBitmapBrush(bitmap.get(), bitmapBrushProperties, brushProperties, &bitmapBrush);
        RELEASE_ASSERT(SUCCEEDED(hr));

        auto size = bitmap->GetSize();
        auto rectInDIP = D2D1::RectF(0, 0, size.width, size.height);
        context->FillRectangle(rectInDIP, bitmapBrush.get());
    }
}

void GraphicsContext::endPlatformTransparencyLayer()
{
    if (paintingDisabled())
        return;

    m_data->endTransparencyLayer();

    ASSERT(!m_impl);

    m_state.alpha = m_data->currentGlobalAlpha();

    restore();
}

bool GraphicsContext::supportsTransparencyLayers()
{
    return false;
}

void GraphicsContext::setPlatformShadow(const FloatSize& offset, float blur, const Color& color)
{
    (void)offset;
    (void)blur;
    (void)color;
    notImplemented();
}

void GraphicsContext::clearPlatformShadow()
{
    if (paintingDisabled())
        return;
    notImplemented();
}

void GraphicsContext::setPlatformStrokeStyle(StrokeStyle style)
{
    if (paintingDisabled())
        return;

    m_data->setStrokeStyle(style);
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        // Maybe this should be part of the state.
        m_impl->setMiterLimit(limit);
        return;
    }

    m_data->setMiterLimit(limit);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clearRect(rect);
        return;
    }

    drawWithoutShadow(rect, [this, rect](ID2D1RenderTarget* renderTarget) {
        FloatRect renderTargetRect(FloatPoint(), renderTarget->GetSize());
        FloatRect rectToClear(rect);

        if (rectToClear.contains(renderTargetRect)) {
            renderTarget->SetTags(1, __LINE__);
            renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
            return;
        }

        if (!rectToClear.intersects(renderTargetRect))
            return;

        renderTarget->SetTags(1, __LINE__);
        rectToClear.intersect(renderTargetRect);
        renderTarget->FillRectangle(rectToClear, brushWithColor(Color(D2D1::ColorF(0, 0, 0, 0))));
    });
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->strokeRect(rect, lineWidth);
        return;
    }

    if (m_state.strokeGradient) {
        WTF::Function<void(ID2D1RenderTarget*)> drawFunction = [this, rect, lineWidth](ID2D1RenderTarget* renderTarget) {
            renderTarget->SetTags(1, __LINE__);
            const D2D1_RECT_F d2dRect = rect;
            renderTarget->DrawRectangle(&d2dRect, m_state.strokeGradient->createPlatformGradientIfNecessary(renderTarget), lineWidth, m_data->strokeStyle());
        };

        if (hasShadow())
            drawWithShadow(rect, drawFunction);
        else
            drawWithoutShadow(rect, drawFunction);
        return;
    }

    if (m_state.strokePattern)
        applyStrokePattern();

    drawWithoutShadow(rect, [this, rect, lineWidth](ID2D1RenderTarget* renderTarget) {
        renderTarget->SetTags(1, __LINE__);
        const D2D1_RECT_F d2dRect = rect;
        auto brush = m_state.strokePattern ? patternStrokeBrush() : solidStrokeBrush();
        renderTarget->DrawRectangle(&d2dRect, brush, lineWidth, m_data->strokeStyle());
    });
}

void GraphicsContext::setLineCap(LineCap cap)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineCap(cap);
        return;
    }

    m_data->setLineCap(cap);
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineDash(dashes, dashOffset);
        return;
    }

    if (dashOffset < 0) {
        float length = 0;
        for (size_t i = 0; i < dashes.size(); ++i)
            length += static_cast<float>(dashes[i]);
        if (length)
            dashOffset = fmod(dashOffset, length) + length;
    }

    m_data->setDashes(dashes);
    m_data->setDashOffset(dashOffset);
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineJoin(join);
        return;
    }

    m_data->setLineJoin(join);
}

void GraphicsContext::canvasClip(const Path& path, WindRule fillRule)
{
    clipPath(path, fillRule);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->scale(size);
        return;
    }

    m_data->scale(size);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::rotate(float angle)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->rotate(angle);
        return;
    }

    m_data->rotate(angle);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::translate(float x, float y)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->translate(x, y);
        return;
    }

    m_data->translate(x, y);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->concatCTM(transform);
        return;
    }

    m_data->concatCTM(transform);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::setCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        WTFLogAlways("GraphicsContext::setCTM() is not compatible with recording contexts.");
        return;
    }

    m_data->setCTM(transform);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

AffineTransform GraphicsContext::getCTM(IncludeDeviceScale includeScale) const
{
    if (paintingDisabled())
        return AffineTransform();

    if (m_impl) {
        WTFLogAlways("GraphicsContext::getCTM() is not yet compatible with recording contexts.");
        return AffineTransform();
    }

    D2D1_MATRIX_3X2_F currentTransform;
    platformContext()->GetTransform(&currentTransform);
    return currentTransform;
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode roundingMode)
{
    if (paintingDisabled())
        return rect;

    if (m_impl) {
        WTFLogAlways("GraphicsContext::roundToDevicePixels() is not yet compatible with recording contexts.");
        return rect;
    }

    notImplemented();

    return rect;
}

void GraphicsContext::drawLineForText(const FloatPoint& point, float width, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    DashArray widths;
    widths.append(0);
    widths.append(width);
    drawLinesForText(point, widths, printing, doubleLines, strokeStyle);
}

void GraphicsContext::drawLinesForText(const FloatPoint& point, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    if (paintingDisabled())
        return;

    if (!widths.size())
        return;

    if (m_impl) {
        m_impl->drawLinesForText(point, widths, printing, doubleLines, strokeThickness());
        return;
    }

    notImplemented();
}

void GraphicsContext::setURLForRect(const URL& link, const FloatRect& destRect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        WTFLogAlways("GraphicsContext::setURLForRect() is not yet compatible with recording contexts.");
        return; // FIXME for display lists.
    }

    RetainPtr<CFURLRef> urlRef = link.createCFURL();
    if (!urlRef)
        return;

    notImplemented();
}

void GraphicsContext::setPlatformImageInterpolationQuality(InterpolationQuality mode)
{
    ASSERT(!paintingDisabled());

    D2D1_INTERPOLATION_MODE quality = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;

    switch (mode) {
    case InterpolationDefault:
        quality = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
        break;
    case InterpolationNone:
        quality = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
        break;
    case InterpolationLow:
        quality = D2D1_INTERPOLATION_MODE_LINEAR;
        break;
    case InterpolationMedium:
        quality = D2D1_INTERPOLATION_MODE_CUBIC;
        break;
    case InterpolationHigh:
        quality = D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
        break;
    }
    // FIXME: SetInterpolationQuality(platformContext(), quality);
}

void GraphicsContext::setIsCALayerContext(bool isLayerContext)
{
    if (paintingDisabled())
        return;

    if (m_impl)
        return;

    // This function is probabaly not needed.
    notImplemented();
}

bool GraphicsContext::isCALayerContext() const
{
    if (paintingDisabled())
        return false;

    // FIXME
    if (m_impl)
        return false;

    // This function is probabaly not needed.
    notImplemented();
    return false;
}

void GraphicsContext::setIsAcceleratedContext(bool isAccelerated)
{
    if (paintingDisabled())
        return;

    // FIXME
    if (m_impl)
        return;

    notImplemented();
}

bool GraphicsContext::isAcceleratedContext() const
{
    if (paintingDisabled())
        return false;

    // FIXME
    if (m_impl)
        return false;

    // This function is probabaly not needed.
    notImplemented();
    return false;
}

void GraphicsContext::setPlatformTextDrawingMode(TextDrawingModeFlags mode)
{
    (void)mode;
    notImplemented();
}

void GraphicsContext::setPlatformStrokeColor(const Color& color)
{
    ASSERT(m_state.strokeColor == color);

    m_data->m_solidStrokeBrush = nullptr;

    m_data->m_solidStrokeBrush = brushWithColor(strokeColor());
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    ASSERT(m_state.strokeThickness == thickness);
    m_data->setStrokeThickness(thickness);
}

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    ASSERT(m_state.fillColor == color);

    m_data->m_solidFillBrush = nullptr;

    m_data->m_solidFillBrush = brushWithColor(fillColor());
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    auto antialiasMode = enable ? D2D1_ANTIALIAS_MODE_PER_PRIMITIVE : D2D1_ANTIALIAS_MODE_ALIASED;
    platformContext()->SetAntialiasMode(antialiasMode);
}

void GraphicsContext::setPlatformShouldSmoothFonts(bool enable)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    auto fontSmoothingMode = enable ? D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE : D2D1_TEXT_ANTIALIAS_MODE_ALIASED;
    platformContext()->SetTextAntialiasMode(fontSmoothingMode);
}

void GraphicsContext::setPlatformAlpha(float alpha)
{
    if (paintingDisabled())
        return;

    ASSERT(m_state.alpha == alpha);
    m_data->setAlpha(alpha);
}

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator mode, BlendMode blendMode)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    D2D1_BLEND_MODE targetBlendMode = D2D1_BLEND_MODE_SCREEN;
    D2D1_COMPOSITE_MODE targetCompositeMode = D2D1_COMPOSITE_MODE_SOURCE_ATOP; // ???

    if (blendMode != BlendMode::Normal) {
        switch (blendMode) {
        case BlendMode::Multiply:
            targetBlendMode = D2D1_BLEND_MODE_MULTIPLY;
            break;
        case BlendMode::Screen:
            targetBlendMode = D2D1_BLEND_MODE_SCREEN;
            break;
        case BlendMode::Overlay:
            targetBlendMode = D2D1_BLEND_MODE_OVERLAY;
            break;
        case BlendMode::Darken:
            targetBlendMode = D2D1_BLEND_MODE_DARKEN;
            break;
        case BlendMode::Lighten:
            targetBlendMode = D2D1_BLEND_MODE_LIGHTEN;
            break;
        case BlendMode::ColorDodge:
            targetBlendMode = D2D1_BLEND_MODE_COLOR_DODGE;
            break;
        case BlendMode::ColorBurn:
            targetBlendMode = D2D1_BLEND_MODE_COLOR_BURN;
            break;
        case BlendMode::HardLight:
            targetBlendMode = D2D1_BLEND_MODE_HARD_LIGHT;
            break;
        case BlendMode::SoftLight:
            targetBlendMode = D2D1_BLEND_MODE_SOFT_LIGHT;
            break;
        case BlendMode::Difference:
            targetBlendMode = D2D1_BLEND_MODE_DIFFERENCE;
            break;
        case BlendMode::Exclusion:
            targetBlendMode = D2D1_BLEND_MODE_EXCLUSION;
            break;
        case BlendMode::Hue:
            targetBlendMode = D2D1_BLEND_MODE_HUE;
            break;
        case BlendMode::Saturation:
            targetBlendMode = D2D1_BLEND_MODE_SATURATION;
            break;
        case BlendMode::Color:
            targetBlendMode = D2D1_BLEND_MODE_COLOR;
            break;
        case BlendMode::Luminosity:
            targetBlendMode = D2D1_BLEND_MODE_LUMINOSITY;
            break;
        case BlendMode::PlusDarker:
            targetBlendMode = D2D1_BLEND_MODE_DARKER_COLOR;
            break;
        case BlendMode::PlusLighter:
            targetBlendMode = D2D1_BLEND_MODE_LIGHTER_COLOR;
            break;
        default:
            break;
        }
    } else {
        switch (mode) {
        case CompositeClear:
            // FIXME: targetBlendMode = D2D1_BLEND_MODE_CLEAR;
            break;
        case CompositeCopy:
            // FIXME: targetBlendMode = D2D1_BLEND_MODE_COPY;
            break;
        case CompositeSourceOver:
            // FIXME: kCGBlendModeNormal
            break;
        case CompositeSourceIn:
            targetCompositeMode = D2D1_COMPOSITE_MODE_SOURCE_IN;
            break;
        case CompositeSourceOut:
            targetCompositeMode = D2D1_COMPOSITE_MODE_SOURCE_OUT;
            break;
        case CompositeSourceAtop:
            targetCompositeMode = D2D1_COMPOSITE_MODE_SOURCE_ATOP;
            break;
        case CompositeDestinationOver:
            targetCompositeMode = D2D1_COMPOSITE_MODE_DESTINATION_OVER;
            break;
        case CompositeDestinationIn:
            targetCompositeMode = D2D1_COMPOSITE_MODE_DESTINATION_IN;
            break;
        case CompositeDestinationOut:
            targetCompositeMode = D2D1_COMPOSITE_MODE_DESTINATION_OUT;
            break;
        case CompositeDestinationAtop:
            targetCompositeMode = D2D1_COMPOSITE_MODE_DESTINATION_ATOP;
            break;
        case CompositeXOR:
            targetCompositeMode = D2D1_COMPOSITE_MODE_XOR;
            break;
        case CompositePlusDarker:
            targetBlendMode = D2D1_BLEND_MODE_DARKER_COLOR;
            break;
        case CompositePlusLighter:
            targetBlendMode = D2D1_BLEND_MODE_LIGHTER_COLOR;
            break;
        case CompositeDifference:
            targetBlendMode = D2D1_BLEND_MODE_DIFFERENCE;
            break;
        }
    }

    m_data->m_blendMode = targetBlendMode;
    m_data->m_compositeMode = targetCompositeMode;
}

void GraphicsContext::platformApplyDeviceScaleFactor(float deviceScaleFactor)
{
    // This is a no-op for Direct2D.
}

void GraphicsContext::platformFillEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    if (m_state.fillGradient || m_state.fillPattern) {
        // FIXME: We should be able to fill ellipses with pattern/gradient brushes in D2D.
        fillEllipseAsPath(ellipse);
        return;
    }

    auto d2dEllipse = D2D1::Ellipse(ellipse.center(), 0.5 * ellipse.width(), 0.5 * ellipse.height());

    platformContext()->SetTags(1, __LINE__);

    drawWithoutShadow(ellipse, [this, d2dEllipse](ID2D1RenderTarget* renderTarget) {
        renderTarget->FillEllipse(&d2dEllipse, solidFillBrush());
    });
}

void GraphicsContext::platformStrokeEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    if (m_state.strokeGradient || m_state.strokePattern) {
        // FIXME: We should be able to stroke ellipses with pattern/gradient brushes in D2D.
        strokeEllipseAsPath(ellipse);
        return;
    }

    auto d2dEllipse = D2D1::Ellipse(ellipse.center(), 0.5 * ellipse.width(), 0.5 * ellipse.height());

    platformContext()->SetTags(1, __LINE__);

    drawWithoutShadow(ellipse, [this, d2dEllipse](ID2D1RenderTarget* renderTarget) {
        renderTarget->DrawEllipse(&d2dEllipse, solidStrokeBrush(), strokeThickness(), m_data->strokeStyle());
    });
}

}
