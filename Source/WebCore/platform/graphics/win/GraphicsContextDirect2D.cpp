/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "Direct2DOperations.h"
#include "Direct2DUtilities.h"
#include "DisplayListRecorder.h"
#include "FloatRoundedRect.h"
#include "GraphicsContextPlatformPrivateDirect2D.h"
#include "ImageBuffer.h"
#include "ImageDecoderDirect2D.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PlatformContextDirect2D.h"
#include <d2d1.h>
#include <d2d1effects.h>
#include <dwrite_3.h>
#include <wtf/URL.h>

#pragma warning (disable : 4756)


namespace WebCore {

GraphicsContext::GraphicsContext(HDC hdc, bool hasAlpha)
{
    platformInit(hdc, hasAlpha);
}

GraphicsContext::GraphicsContext(HDC hdc, ID2D1DCRenderTarget** renderTarget, RECT rect, bool hasAlpha)
{
    // Create a DC render target.
    auto targetProperties = Direct2D::renderTargetProperties();

    HRESULT hr = GraphicsContext::systemFactory()->CreateDCRenderTarget(&targetProperties, renderTarget);
    RELEASE_ASSERT(SUCCEEDED(hr));

    (*renderTarget)->BindDC(hdc, &rect);

    auto ownedPlatformContext = makeUnique<PlatformContextDirect2D>(*renderTarget);
    m_data = new GraphicsContextPlatformPrivate(WTFMove(ownedPlatformContext), BitmapRenderingContextType::GPUMemory);
    m_data->m_hdc = hdc;
}

GraphicsContext::GraphicsContext(PlatformContextDirect2D* platformGraphicsContext, BitmapRenderingContextType rendererType)
{
    platformInit(platformGraphicsContext, rendererType);
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
        defaultRenderTarget->AddRef();
    }

    return defaultRenderTarget;
}

void GraphicsContext::platformInit(HDC hdc, bool hasAlpha)
{
    if (!hdc)
        return;

    HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));

    DIBPixelData pixelData(bitmap);

    auto targetProperties = Direct2D::renderTargetProperties();

    COMPtr<ID2D1DCRenderTarget> renderTarget;
    HRESULT hr = systemFactory()->CreateDCRenderTarget(&targetProperties, &renderTarget);
    if (!SUCCEEDED(hr))
        return;

    RECT clientRect = IntRect(IntPoint(), pixelData.size());
    hr = renderTarget->BindDC(hdc, &clientRect);
    if (!SUCCEEDED(hr))
        return;

    auto ownedPlatformContext = makeUnique<PlatformContextDirect2D>(renderTarget.get());
    m_data = new GraphicsContextPlatformPrivate(WTFMove(ownedPlatformContext), BitmapRenderingContextType::GPUMemory);
    m_data->m_hdc = hdc;

    // Make sure the context starts in sync with our state.
    setPlatformFillColor(fillColor());
    setPlatformStrokeColor(strokeColor());
    setPlatformStrokeThickness(strokeThickness());
    // FIXME: m_state.imageInterpolationQuality = convertInterpolationQuality(CGContextGetInterpolationQuality(platformContext()));
}

void GraphicsContext::platformInit(PlatformContextDirect2D* platformContext)
{
    platformInit(platformContext, BitmapRenderingContextType::GPUMemory);
}

void GraphicsContext::platformInit(PlatformContextDirect2D* platformContext, BitmapRenderingContextType renderingType)
{
    if (!platformContext)
        return;

    m_data = new GraphicsContextPlatformPrivate(*platformContext, renderingType);

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

PlatformContextDirect2D* GraphicsContext::platformContext() const
{
    if (m_impl)
        return m_impl->platformContext();
    ASSERT(!paintingDisabled());
    return &m_data->platformContext();
}

void GraphicsContextPlatformPrivate::syncContext(PlatformContextDirect2D&)
{
    notImplemented();
}

ID2D1RenderTarget* GraphicsContextPlatformPrivate::renderTarget()
{
    if (!m_platformContext.m_transparencyLayerStack.isEmpty())
        return m_platformContext.m_transparencyLayerStack.last().renderTarget.get();

    return m_platformContext.renderTarget();
}

void GraphicsContextPlatformPrivate::setAlpha(float alpha)
{
    ASSERT(m_platformContext.m_transparencyLayerStack.isEmpty());
    m_alpha = alpha;
}

float GraphicsContextPlatformPrivate::currentGlobalAlpha() const
{
    if (!m_platformContext.m_transparencyLayerStack.isEmpty())
        return m_platformContext.m_transparencyLayerStack.last().opacity;

    return m_alpha;
}

void GraphicsContext::savePlatformState()
{
    ASSERT(hasPlatformContext());
    Direct2D::save(*platformContext());
}

void GraphicsContext::restorePlatformState()
{
    ASSERT(hasPlatformContext());
    Direct2D::restore(*platformContext());
}

void GraphicsContext::drawNativeImage(const COMPtr<ID2D1Bitmap>& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator compositeOperator, BlendMode blendMode, ImageOrientation orientation)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawNativeImage(image, imageSize, destRect, srcRect, compositeOperator, blendMode, orientation);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Direct2D::drawNativeImage(*platformContext(), image.get(), imageSize, destRect, srcRect, compositeOperator, blendMode, orientation, state.imageInterpolationQuality, state.alpha, Direct2D::ShadowState(state));
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend)
{
    bool createdBitmap = m_impl || !m_data->m_hdc || isInTransparencyLayer();
    if (!createdBitmap) {
        ASSERT(hasPlatformContext());
        Direct2D::restore(*platformContext());
        return;
    }

    if (!hdc || dstRect.isEmpty())
        return;

    auto sourceBitmap = adoptGDIObject(static_cast<HBITMAP>(::GetCurrentObject(hdc, OBJ_BITMAP)));

    DIBPixelData pixelData(sourceBitmap.get());
    ASSERT(pixelData.bitsPerPixel() == 32);

    auto bitmapProperties = Direct2D::bitmapProperties();

    ASSERT(hasPlatformContext());
    auto& platformContext = *this->platformContext();

    COMPtr<ID2D1Bitmap> bitmap;
    HRESULT hr = platformContext.renderTarget()->CreateBitmap(pixelData.size(), pixelData.buffer(), pixelData.bytesPerRow(), &bitmapProperties, &bitmap);
    ASSERT(SUCCEEDED(hr));

    PlatformContextStateSaver stateSaver(platformContext);

    // Note: The content in the HDC is inverted compared to Direct2D, so it needs to be flipped.
    auto context = platformContext.renderTarget();

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

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(PlatformContextDirect2D& platformContext, GraphicsContext::BitmapRenderingContextType renderingType)
    : m_platformContext(platformContext)
    , m_rendererType(renderingType)
{
    if (!m_platformContext.renderTarget())
        return;

    if (m_rendererType == GraphicsContext::BitmapRenderingContextType::GPUMemory)
        beginDraw();
}

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(std::unique_ptr<PlatformContextDirect2D>&& ownedPlatformContext, GraphicsContext::BitmapRenderingContextType renderingType)
    : m_ownedPlatformContext(WTFMove(ownedPlatformContext))
    , m_platformContext(*ownedPlatformContext)
    , m_rendererType(renderingType)
{
    if (!m_platformContext.renderTarget())
        return;

    if (m_rendererType == GraphicsContext::BitmapRenderingContextType::GPUMemory)
        beginDraw();
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
    if (!m_platformContext.renderTarget())
        return;

    if (m_platformContext.beginDrawCount)
        endDraw();
}

ID2D1SolidColorBrush* GraphicsContext::brushWithColor(const Color& color)
{
    ASSERT(hasPlatformContext());
    return platformContext()->brushWithColor(colorWithGlobalAlpha(color)).get();
}

void GraphicsContextPlatformPrivate::clip(const FloatRect& rect)
{
}

void GraphicsContextPlatformPrivate::clip(const Path& path)
{
}

void GraphicsContextPlatformPrivate::clip(ID2D1Geometry* path)
{
}

void GraphicsContextPlatformPrivate::concatCTM(const AffineTransform& affineTransform)
{
}

void GraphicsContextPlatformPrivate::flush()
{
}

void GraphicsContextPlatformPrivate::beginDraw()
{
    m_platformContext.beginDraw();
}

void GraphicsContextPlatformPrivate::endDraw()
{
    m_platformContext.endDraw();
}

void GraphicsContextPlatformPrivate::restore()
{
}

void GraphicsContextPlatformPrivate::save()
{
}

void GraphicsContextPlatformPrivate::scale(const FloatSize& size)
{
}

void GraphicsContextPlatformPrivate::setCTM(const AffineTransform& transform)
{
    ASSERT(m_platformContext.renderTarget());
    m_platformContext.renderTarget()->SetTransform(transform);
}

void GraphicsContextPlatformPrivate::translate(float x, float y)
{
}

void GraphicsContextPlatformPrivate::rotate(float angle)
{
}

D2D1_COLOR_F GraphicsContext::colorWithGlobalAlpha(const Color& color) const
{
    float colorAlpha = color.alphaAsFloat();
    float globalAlpha = m_data->currentGlobalAlpha();

    return D2D1::ColorF(color.rgb(), globalAlpha * colorAlpha);
}

ID2D1Brush* GraphicsContext::solidStrokeBrush() const
{
    return platformContext()->m_solidStrokeBrush.get();
}

ID2D1Brush* GraphicsContext::solidFillBrush() const
{
    return platformContext()->m_solidFillBrush.get();
}

ID2D1Brush* GraphicsContext::patternStrokeBrush() const
{
    return platformContext()->m_patternStrokeBrush.get();
}

ID2D1Brush* GraphicsContext::patternFillBrush() const
{
    return platformContext()->m_patternFillBrush.get();
}

void GraphicsContext::beginDraw()
{
    ASSERT(hasPlatformContext());
    platformContext()->beginDraw();
}

void GraphicsContext::endDraw()
{
    ASSERT(hasPlatformContext());
    platformContext()->endDraw();
}

void GraphicsContext::flush()
{
    ASSERT(hasPlatformContext());
    Direct2D::flush(*platformContext());
}

void GraphicsContext::drawPattern(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator compositeOperator, BlendMode blendMode)
{
    if (paintingDisabled() || !patternTransform.isInvertible())
        return;

    if (m_impl) {
        m_impl->drawPattern(image, destRect, tileRect, patternTransform, phase, spacing, compositeOperator, blendMode);
        return;
    }

    ASSERT(hasPlatformContext());
    if (auto tileImage = image.nativeImageForCurrentFrame(this))
        Direct2D::drawPattern(*platformContext(), WTFMove(tileImage), IntSize(image.size()), destRect, tileRect, patternTransform, phase, compositeOperator, blendMode);
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

    ASSERT(!rect.isEmpty());
    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Direct2D::drawRect(*platformContext(), rect, borderThickness, state.fillColor, state.strokeStyle, state.strokeColor);
}

void GraphicsContextPlatformPrivate::setLineCap(LineCap)
{
}

void GraphicsContextPlatformPrivate::setLineJoin(LineJoin)
{
}

void GraphicsContextPlatformPrivate::setStrokeStyle(StrokeStyle)
{
}

void GraphicsContextPlatformPrivate::setMiterLimit(float canvasMiterLimit)
{
    m_platformContext.setDashOffset(canvasMiterLimit);
}

void GraphicsContextPlatformPrivate::setDashOffset(float dashOffset)
{
    m_platformContext.setDashOffset(dashOffset);
}

void GraphicsContextPlatformPrivate::setPatternWidth(float patternWidth)
{
    m_platformContext.setPatternWidth(patternWidth);
}

void GraphicsContextPlatformPrivate::setPatternOffset(float patternOffset)
{
    m_platformContext.setPatternOffset(patternOffset);
}

void GraphicsContextPlatformPrivate::setStrokeThickness(float thickness)
{
    m_platformContext.setStrokeThickness(thickness);
}

void GraphicsContextPlatformPrivate::setDashes(const DashArray& dashes)
{
    m_platformContext.setDashes(dashes);
}

D2D1_STROKE_STYLE_PROPERTIES GraphicsContextPlatformPrivate::strokeStyleProperties() const
{
    return D2D1::StrokeStyleProperties(m_platformContext.m_lineCap, m_platformContext.m_lineCap, m_platformContext.m_lineCap, m_platformContext.m_lineJoin, m_platformContext.m_miterLimit, D2D1_DASH_STYLE_SOLID, 0.0f);
}

ID2D1StrokeStyle* GraphicsContext::platformStrokeStyle() const
{
    ASSERT(hasPlatformContext());
    return platformContext()->strokeStyle();
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

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Direct2D::drawLine(*platformContext(), point1, point2, state.strokeStyle, state.strokeColor, state.strokeThickness, state.shouldAntialias);
}

void GraphicsContext::drawEllipse(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->drawEllipse(rect);
        return;
    }


    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Direct2D::fillEllipse(*platformContext(), rect, state.fillColor, state.strokeStyle, state.strokeColor, state.strokeThickness);
}

void GraphicsContext::applyStrokePattern()
{
    if (paintingDisabled())
        return;

    auto context = platformContext();
    AffineTransform userToBaseCTM; // FIXME: This isn't really needed on Windows

    const float patternAlpha = 1;
    platformContext()->m_patternStrokeBrush = adoptCOM(m_state.strokePattern->createPlatformPattern(*this, patternAlpha, userToBaseCTM));
}

void GraphicsContext::applyFillPattern()
{
    if (paintingDisabled())
        return;

    auto context = platformContext();
    AffineTransform userToBaseCTM; // FIXME: This isn't really needed on Windows

    const float patternAlpha = 1;
    platformContext()->m_patternFillBrush = adoptCOM(m_state.fillPattern->createPlatformPattern(*this, patternAlpha, userToBaseCTM));
}

void GraphicsContext::drawPath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->drawPath(path);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::drawPath(context, path, Direct2D::StrokeSource(state, context), Direct2D::ShadowState(state));
}

void GraphicsContext::fillPath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->fillPath(path);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::fillPath(context, path, Direct2D::FillSource(state, context), Direct2D::ShadowState(state));
}

void GraphicsContext::strokePath(const Path& path)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    if (m_impl) {
        m_impl->strokePath(path);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::strokePath(context, path, Direct2D::StrokeSource(state, context), Direct2D::ShadowState(state));
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRect(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::fillRect(context, rect, Direct2D::FillSource(state, context), Direct2D::ShadowState(state));
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRect(rect, color);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::fillRect(context, rect, color, Direct2D::ShadowState(state));
}

void GraphicsContext::platformFillRoundedRect(const FloatRoundedRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    Direct2D::fillRoundedRect(*platformContext(), rect, color, Direct2D::ShadowState(state));
}

void GraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->fillRectWithRoundedHole(rect, roundedHoleRect, color);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::fillRectWithRoundedHole(context, rect, roundedHoleRect, Direct2D::FillSource(state, context), Direct2D::ShadowState(state));
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clip(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::clip(*platformContext(), rect);
}

void GraphicsContext::clipOut(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipOut(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::clipOut(*platformContext(), rect);
}

void GraphicsContext::clipOut(const Path& path)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipOut(path);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::clipOut(*platformContext(), path);
}

void GraphicsContext::clipPath(const Path& path, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clipPath(path, clipRule);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::clipPath(*platformContext(), path, clipRule);
}

IntRect GraphicsContext::clipBounds() const
{
    if (paintingDisabled())
        return IntRect();

    if (m_impl)
        return m_impl->clipBounds();

    ASSERT(hasPlatformContext());
    return Direct2D::State::getClipBounds(*platformContext());
}

void GraphicsContextPlatformPrivate::beginTransparencyLayer(float opacity)
{
}

void GraphicsContext::beginPlatformTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    save();

    m_state.alpha = opacity;

    ASSERT(hasPlatformContext());
    Direct2D::beginTransparencyLayer(*platformContext(), opacity);
}

void GraphicsContextPlatformPrivate::endTransparencyLayer()
{
}

void GraphicsContext::endPlatformTransparencyLayer()
{
    if (paintingDisabled())
        return;

    ASSERT(hasPlatformContext());
    Direct2D::endTransparencyLayer(*platformContext());

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

    ASSERT(hasPlatformContext());
    Direct2D::State::setStrokeStyle(*platformContext(), style);
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

    ASSERT(hasPlatformContext());
    Direct2D::setMiterLimit(*platformContext(), limit);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->clearRect(rect);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::clearRect(*platformContext(), rect);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->strokeRect(rect, lineWidth);
        return;
    }

    ASSERT(hasPlatformContext());
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::strokeRect(context, rect, lineWidth, Direct2D::StrokeSource(state, context), Direct2D::ShadowState(state));
}

void GraphicsContext::setLineCap(LineCap cap)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineCap(cap);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::setLineCap(*platformContext(), cap);
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

    ASSERT(hasPlatformContext());
    Direct2D::setLineDash(*platformContext(), dashes, dashOffset);
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setLineJoin(join);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::setLineJoin(*platformContext(), join);
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

    ASSERT(hasPlatformContext());
    Direct2D::scale(*platformContext(), size);
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

    ASSERT(hasPlatformContext());
    Direct2D::rotate(*platformContext(), angle);
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

    ASSERT(hasPlatformContext());
    Direct2D::translate(*platformContext(), x, y);
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

    ASSERT(hasPlatformContext());
    Direct2D::concatCTM(*platformContext(), transform);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContext::setCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        m_impl->setCTM(transform);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::State::setCTM(*platformContext(), transform);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

AffineTransform GraphicsContext::getCTM(IncludeDeviceScale includeScale) const
{
    if (paintingDisabled())
        return AffineTransform();

    if (m_impl)
        return m_impl->getCTM(includeScale);

    ASSERT(hasPlatformContext());
    return Direct2D::State::getCTM(*platformContext());
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode roundingMode)
{
    if (paintingDisabled())
        return rect;

    if (m_impl)
        return m_impl->roundToDevicePixels(rect, roundingMode);

    return Direct2D::State::roundToDevicePixels(*platformContext(), rect);
}

void GraphicsContext::drawLineForText(const FloatRect& rect, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    DashArray widths;
    widths.append(0);
    widths.append(rect.width());
    drawLinesForText(rect.location(), rect.height(), widths, printing, doubleLines, strokeStyle);
}

void GraphicsContext::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleUnderlines, StrokeStyle strokeStyle)
{
    if (paintingDisabled())
        return;

    if (!widths.size())
        return;

    if (m_impl) {
        m_impl->drawLinesForText(point, thickness, widths, printing, doubleUnderlines);
        return;
    }

    ASSERT(hasPlatformContext());
    Direct2D::drawLinesForText(*platformContext(), point, thickness, widths, printing, doubleUnderlines, m_state.strokeColor);
}

void GraphicsContext::setURLForRect(const URL& link, const FloatRect& destRect)
{
    if (paintingDisabled())
        return;

    if (m_impl) {
        WTFLogAlways("GraphicsContext::setURLForRect() is not yet compatible with recording contexts.");
        return; // FIXME for display lists.
    }

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
    if (!hasPlatformContext())
        return false;

    return Direct2D::State::isAcceleratedContext(*platformContext());
}

void GraphicsContext::setPlatformTextDrawingMode(TextDrawingModeFlags mode)
{
    (void)mode;
    notImplemented();
}

void GraphicsContext::setPlatformStrokeColor(const Color& color)
{
    ASSERT(m_state.strokeColor == color);
    ASSERT(hasPlatformContext());
    platformContext()->m_solidStrokeBrush = brushWithColor(strokeColor());
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    ASSERT(m_state.strokeThickness == thickness);
    m_data->setStrokeThickness(thickness);
}

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    ASSERT(m_state.fillColor == color);
    ASSERT(hasPlatformContext());
    platformContext()->m_solidFillBrush = brushWithColor(fillColor());
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);
    ASSERT(hasPlatformContext());
    Direct2D::State::setShouldAntialias(*platformContext(), enable);
}

void GraphicsContext::setPlatformShouldSmoothFonts(bool enable)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    auto fontSmoothingMode = enable ? D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE : D2D1_TEXT_ANTIALIAS_MODE_ALIASED;
    platformContext()->renderTarget()->SetTextAntialiasMode(fontSmoothingMode);
}

void GraphicsContext::setPlatformAlpha(float alpha)
{
    if (paintingDisabled())
        return;

    ASSERT(m_state.alpha == alpha);
    m_data->setAlpha(alpha);
}

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator compositeOperator, BlendMode blendMode)
{
    if (paintingDisabled())
        return;

    ASSERT(!m_impl);

    ASSERT(hasPlatformContext());
    Direct2D::State::setCompositeOperation(*platformContext(), compositeOperator, blendMode);
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

    ASSERT(hasPlatformContext());
    Direct2D::fillEllipse(*platformContext(), ellipse, m_state.fillColor, m_state.strokeStyle, m_state.strokeColor, m_state.strokeThickness);
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

    ASSERT(hasPlatformContext());
    Direct2D::drawEllipse(*platformContext(), ellipse, m_state.strokeStyle, m_state.strokeColor, m_state.strokeThickness);
}

}
