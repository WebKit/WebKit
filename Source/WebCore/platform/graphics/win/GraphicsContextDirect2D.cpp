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

GraphicsContextDirect2D::GraphicsContextDirect2D(HDC hdc, bool hasAlpha)
{
    platformInit(hdc, hasAlpha);
}

GraphicsContextDirect2D::GraphicsContextDirect2D(HDC hdc, ID2D1DCRenderTarget** renderTarget, RECT rect, bool hasAlpha)
{
    // Create a DC render target.
    auto targetProperties = Direct2D::renderTargetProperties();

    HRESULT hr = GraphicsContextDirect2D::systemFactory()->CreateDCRenderTarget(&targetProperties, renderTarget);
    RELEASE_ASSERT(SUCCEEDED(hr));

    (*renderTarget)->BindDC(hdc, &rect);

    auto ownedPlatformContext = makeUnique<PlatformContextDirect2D>(*renderTarget);
    m_data = new GraphicsContextPlatformPrivate(WTFMove(ownedPlatformContext), BitmapRenderingContextType::GPUMemory);
    m_data->m_hdc = hdc;
}

GraphicsContextDirect2D::GraphicsContextDirect2D(PlatformContextDirect2D* platformGraphicsContext, BitmapRenderingContextType rendererType)
{
    platformInit(platformGraphicsContext, rendererType);
}

ID2D1Factory* GraphicsContextDirect2D::systemFactory()
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

ID2D1RenderTarget* GraphicsContextDirect2D::defaultRenderTarget()
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

void GraphicsContextDirect2D::platformInit(HDC hdc, bool hasAlpha)
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

void GraphicsContextDirect2D::platformInit(PlatformContextDirect2D* platformContext)
{
    platformInit(platformContext, BitmapRenderingContextType::GPUMemory);
}

void GraphicsContextDirect2D::platformInit(PlatformContextDirect2D* platformContext, BitmapRenderingContextType renderingType)
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

GraphicsContextDirect2D::~GraphicsContextDirect2D()
{
    delete m_data;
}

PlatformContextDirect2D* GraphicsContextDirect2D::platformContext() const
{
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

void GraphicsContextDirect2D::save()
{
    GraphicsContext::save();
    Direct2D::save(*platformContext());
}

void GraphicsContextDirect2D::restore()
{
    if (!stackSize())
        return;

    GraphicsContext::restore();
    Direct2D::restore(*platformContext());
}

void GraphicsContextDirect2D::drawNativeImage(NativeImage& nativeImage, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    auto& state = this->state();
    Direct2D::drawNativeImage(*platformContext(), nativeImage.platformImage().get(), imageSize, destRect, srcRect, options, state.alpha, Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend)
{
    bool createdBitmap = !m_data->m_hdc || isInTransparencyLayer();
    if (!createdBitmap) {
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

void GraphicsContextDirect2D::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
}

void GraphicsContextDirect2D::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
}

void GraphicsContextDirect2D::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    Direct2D::drawDotsForDocumentMarker(m_platformContext, rect, style);
}

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(PlatformContextDirect2D& platformContext, GraphicsContextDirect2D::BitmapRenderingContextType renderingType)
    : m_platformContext(platformContext)
    , m_rendererType(renderingType)
{
    if (!m_platformContext.renderTarget())
        return;

    if (m_rendererType == GraphicsContextDirect2D::BitmapRenderingContextType::GPUMemory)
        beginDraw();
}

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(std::unique_ptr<PlatformContextDirect2D>&& ownedPlatformContext, GraphicsContextDirect2D::BitmapRenderingContextType renderingType)
    : m_ownedPlatformContext(WTFMove(ownedPlatformContext))
    , m_platformContext(*m_ownedPlatformContext)
    , m_rendererType(renderingType)
{
    if (!m_platformContext.renderTarget())
        return;

    if (m_rendererType == GraphicsContextDirect2D::BitmapRenderingContextType::GPUMemory)
        beginDraw();
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
    if (!m_platformContext.renderTarget())
        return;

    if (m_platformContext.beginDrawCount)
        endDraw();
}

ID2D1SolidColorBrush* GraphicsContextDirect2D::brushWithColor(const Color& color)
{
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

D2D1_COLOR_F GraphicsContextDirect2D::colorWithGlobalAlpha(const Color& color) const
{
    auto [r, g, b, a] = color.toSRGBALossy<float>();
    return D2D1::ColorF(r, g, b, a * m_data->currentGlobalAlpha());
}

ID2D1Brush* GraphicsContextDirect2D::solidStrokeBrush() const
{
    return platformContext()->m_solidStrokeBrush.get();
}

ID2D1Brush* GraphicsContextDirect2D::solidFillBrush() const
{
    return platformContext()->m_solidFillBrush.get();
}

ID2D1Brush* GraphicsContextDirect2D::patternStrokeBrush() const
{
    return platformContext()->m_patternStrokeBrush.get();
}

ID2D1Brush* GraphicsContextDirect2D::patternFillBrush() const
{
    return platformContext()->m_patternFillBrush.get();
}

void GraphicsContextDirect2D::beginDraw()
{
    platformContext()->beginDraw();
}

void GraphicsContextDirect2D::endDraw()
{
    platformContext()->endDraw();
}

void GraphicsContextDirect2D::flush()
{
    ASSERT(hasPlatformContext());
    Direct2D::flush(*platformContext());
}

void GraphicsContextDirect2D::drawPattern(const PlatformImagePtr& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    if (!patternTransform.isInvertible())
        return;
    auto tileImage = image;
    Direct2D::drawPattern(*platformContext(), WTFMove(tileImage), IntSize(image.size()), destRect, tileRect, patternTransform, phase, options.compositeOperator(), options.blendMode());
}

// Draws a filled rectangle with a stroked border.
void GraphicsContextDirect2D::drawRect(const FloatRect& rect, float borderThickness)
{
    ASSERT(!rect.isEmpty());
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

ID2D1StrokeStyle* GraphicsContextDirect2D::strokeStyle() const
{
    ASSERT(hasPlatformContext());
    return platformContext()->strokeStyle();
}

// This is only used to draw borders.
void GraphicsContextDirect2D::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    if (strokeStyle() == NoStroke)
        return;
    auto& state = this->state();
    Direct2D::drawLine(*platformContext(), point1, point2, state.strokeStyle, state.strokeColor, state.strokeThickness, state.shouldAntialias);
}

void GraphicsContextDirect2D::drawEllipse(const FloatRect& rect)
{
    auto& state = this->state();
    Direct2D::fillEllipse(*platformContext(), rect, state.fillColor, state.strokeStyle, state.strokeColor, state.strokeThickness);
}

void GraphicsContextDirect2D::applyStrokePattern()
{
    if (!m_state.strokePattern)
        return;

    auto context = platformContext();
    AffineTransform userToBaseCTM; // FIXME: This isn't really needed on Windows

    const float patternAlpha = 1;
    platformContext()->m_patternStrokeBrush = adoptCOM(m_state.strokePattern->createPlatformPattern(*this, patternAlpha, userToBaseCTM));
}

void GraphicsContextDirect2D::applyFillPattern()
{
    if (!m_state.fillPattern)
        return;

    auto context = platformContext();
    AffineTransform userToBaseCTM; // FIXME: This isn't really needed on Windows

    const float patternAlpha = 1;
    platformContext()->m_patternFillBrush = adoptCOM(m_state.fillPattern->createPlatformPattern(*this, patternAlpha, userToBaseCTM));
}

void GraphicsContextDirect2D::drawPath(const Path& path)
{
    if (path.isEmpty())
        return;

    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::drawPath(context, path, Direct2D::StrokeSource(state, *this), Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::fillPath(const Path& path)
{
    if (path.isEmpty())
        return;

    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::fillPath(context, path, Direct2D::FillSource(state, *this), Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::strokePath(const Path& path)
{
    if (path.isEmpty())
        return;

    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::strokePath(context, path, Direct2D::StrokeSource(state, *this), Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::fillRect(const FloatRect& rect)
{
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::fillRect(context, rect, Direct2D::FillSource(state, *this), Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::fillRect(const FloatRect& rect, const Color& color)
{
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::fillRect(context, rect, color, Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::fillRect(const FloatRect& rect, Gradient& gradient)
{
    auto brush = gradient.createBrush(m_platformContext.renderTarget());
    if (!brush)
        return;

    Direct2D::fillRectWithGradient(m_platformContext, rect, brush.get());
}

void GraphicsContextDirect2D::fillRect(const FloatRect& rect, const Color& color, CompositeOperator compositeOperator, BlendMode blendMode)
{
    auto& state = graphicsContext().state();
    CompositeOperator previousOperator = state.compositeOperator;

    Direct2D::State::setCompositeOperation(m_platformContext, compositeOperator, blendMode);
    Direct2D::fillRect(m_platformContext, rect, color, Direct2D::ShadowState(state));
    Direct2D::State::setCompositeOperation(m_platformContext, previousOperator, BlendMode::Normal);
}

void GraphicsContextDirect2D::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    auto& state = this->state();
    Direct2D::fillRoundedRect(*platformContext(), rect, color, Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::fillRectWithRoundedHole(context, rect, roundedHoleRect, Direct2D::FillSource(state, *this), Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::clip(const FloatRect& rect)
{
    Direct2D::clip(*platformContext(), rect);
}

void GraphicsContextDirect2D::clipOut(const FloatRect& rect)
{
    Direct2D::clipOut(*platformContext(), rect);
}

void GraphicsContextDirect2D::clipOut(const Path& path)
{
    Direct2D::clipOut(*platformContext(), path);
}

void GraphicsContextDirect2D::clipPath(const Path& path, WindRule clipRule)
{
    Direct2D::clipPath(*platformContext(), path, clipRule);
}

IntRect GraphicsContextDirect2D::clipBounds() const
{
    return Direct2D::State::getClipBounds(*platformContext());
}

void GraphicsContextPlatformPrivate::beginTransparencyLayer(float opacity)
{
}

void GraphicsContextDirect2D::beginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);
    save();

    m_state.alpha = opacity;

    Direct2D::beginTransparencyLayer(*platformContext(), opacity);
}

void GraphicsContextPlatformPrivate::endTransparencyLayer()
{
}

void GraphicsContextDirect2D::endTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();
    Direct2D::endTransparencyLayer(*platformContext());

    m_state.alpha = m_data->currentGlobalAlpha();

    restore();
}

bool GraphicsContextDirect2D::supportsTransparencyLayers()
{
    return false;
}

void GraphicsContextDirect2D::updateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    if (flags & GraphicsContextState::StrokeThicknessChange)
        Direct2D::State::setStrokeThickness(m_platformContext, state.strokeThickness);

    if (flags & GraphicsContextState::StrokeStyleChange)
        Direct2D::State::setStrokeStyle(m_platformContext, state.strokeStyle);

    if (flags & GraphicsContextState::ShadowChange) {
        if (state.shadowsIgnoreTransforms) {
            // Meaning that this graphics context is associated with a CanvasRenderingContext
            // We flip the height since CG and HTML5 Canvas have opposite Y axis
            auto& mutableState = const_cast<GraphicsContextState&>(graphicsContext().state());
            auto& shadowOffset = state.shadowOffset;
            mutableState.shadowOffset = { shadowOffset.width(), -shadowOffset.height() };
        }
    }

    if (flags & GraphicsContextState::CompositeOperationChange)
        Direct2D::State::setCompositeOperation(m_platformContext, state.compositeOperator, state.blendMode);

    if (flags & GraphicsContextState::ShouldAntialiasChange)
        Direct2D::State::setShouldAntialias(m_platformContext, state.shouldAntialias);

    if (flags.contains(GraphicsContextState::StrokeColorChange)) {
        ASSERT(m_state.strokeColor == color);
        platformContext()->m_solidStrokeBrush = brushWithColor(strokeColor());    
    }
    
    if (flags.contains(GraphicsContextState::FillColorChange)) {
        ASSERT(m_state.fillColor == color);
        platformContext()->m_solidFillBrush = brushWithColor(fillColor());
    }

    if (flags.contains(GraphicsContextState::ShouldSmoothFontsChange)) {
        auto fontSmoothingMode = enable ? D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE : D2D1_TEXT_ANTIALIAS_MODE_ALIASED;
        platformContext()->renderTarget()->SetTextAntialiasMode(fontSmoothingMode);
    }

    if (flags.contains(GraphicsContextState::AlphaChange)) {
        ASSERT(m_state.alpha == alpha);
        m_data->setAlpha(alpha);
    }

    if (flags.contains(GraphicsContextState::ImageInterpolationQualityChange)) {
        D2D1_INTERPOLATION_MODE quality = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;

        switch (mode) {
        case InterpolationQuality::Default:
            quality = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
            break;
        case InterpolationQuality::DoNotInterpolate:
            quality = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
            break;
        case InterpolationQuality::Low:
            quality = D2D1_INTERPOLATION_MODE_LINEAR;
            break;
        case InterpolationQuality::Medium:
            quality = D2D1_INTERPOLATION_MODE_CUBIC;
            break;
        case InterpolationQuality::High:
            quality = D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
            break;
        }
        // FIXME: SetInterpolationQuality(platformContext(), quality);
    }
}

void GraphicsContextDirect2D::setMiterLimit(float limit)
{
    Direct2D::setMiterLimit(*platformContext(), limit);
}

void GraphicsContextDirect2D::clearRect(const FloatRect& rect)
{
    Direct2D::clearRect(*platformContext(), rect);
}

void GraphicsContextDirect2D::strokeRect(const FloatRect& rect, float lineWidth)
{
    auto& state = this->state();
    auto& context = *platformContext();
    Direct2D::strokeRect(context, rect, lineWidth, Direct2D::StrokeSource(state, *this), Direct2D::ShadowState(state));
}

void GraphicsContextDirect2D::setLineCap(LineCap cap)
{
    Direct2D::setLineCap(*platformContext(), cap);
}

void GraphicsContextDirect2D::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (dashOffset < 0) {
        float length = 0;
        for (size_t i = 0; i < dashes.size(); ++i)
            length += static_cast<float>(dashes[i]);
        if (length)
            dashOffset = fmod(dashOffset, length) + length;
    }

    Direct2D::setLineDash(*platformContext(), dashes, dashOffset);
}

void GraphicsContextDirect2D::setLineJoin(LineJoin join)
{
    Direct2D::setLineJoin(*platformContext(), join);
}

void GraphicsContextDirect2D::scale(const FloatSize& size)
{
    Direct2D::scale(*platformContext(), size);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextDirect2D::rotate(float angle)
{
    Direct2D::rotate(*platformContext(), angle);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextDirect2D::translate(float x, float y)
{
    Direct2D::translate(*platformContext(), x, y);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextDirect2D::concatCTM(const AffineTransform& transform)
{
    Direct2D::concatCTM(*platformContext(), transform);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

void GraphicsContextDirect2D::setCTM(const AffineTransform& transform)
{
    Direct2D::State::setCTM(*platformContext(), transform);
    // FIXME: m_data->m_userToDeviceTransformKnownToBeIdentity = false;
}

AffineTransform GraphicsContextDirect2D::getCTM(IncludeDeviceScale includeScale) const
{
    return Direct2D::State::getCTM(*platformContext());
}

FloatRect GraphicsContextDirect2D::roundToDevicePixels(const FloatRect& rect, RoundingMode roundingMode)
{
    return Direct2D::State::roundToDevicePixels(*platformContext(), rect);
}

void GraphicsContextDirect2D::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleUnderlines, StrokeStyle strokeStyle)
{
    if (!widths.size())
        return;

    Direct2D::drawLinesForText(*platformContext(), point, thickness, widths, printing, doubleUnderlines, m_state.strokeColor);
}

void GraphicsContextDirect2D::setURLForRect(const URL& link, const FloatRect& destRect)
{
    notImplemented();
}

void GraphicsContextDirect2D::setIsCALayerContext(bool isLayerContext)
{
    // This function is probabaly not needed.
    notImplemented();
}

bool GraphicsContextDirect2D::isCALayerContext() const
{
    // This function is probabaly not needed.
    notImplemented();
    return false;
}

void GraphicsContextDirect2D::setIsAcceleratedContext(bool isAccelerated)
{
    notImplemented();
}

bool GraphicsContextDirect2D::isAcceleratedContext() const
{
    return Direct2D::State::isAcceleratedContext(*platformContext());
}

void GraphicsContextDirect2D::applyDeviceScaleFactor(float deviceScaleFactor)
{
    // This is a no-op for Direct2D.
}

void GraphicsContextDirect2D::fillEllipse(const FloatRect& ellipse)
{
    if (m_state.fillGradient || m_state.fillPattern) {
        // FIXME: We should be able to fill ellipses with pattern/gradient brushes in D2D.
        fillEllipseAsPath(ellipse);
        return;
    }

    Direct2D::fillEllipse(*platformContext(), ellipse, m_state.fillColor, m_state.strokeStyle, m_state.strokeColor, m_state.strokeThickness);
}

void GraphicsContextDirect2D::strokeEllipse(const FloatRect& ellipse)
{
    if (m_state.strokeGradient || m_state.strokePattern) {
        // FIXME: We should be able to stroke ellipses with pattern/gradient brushes in D2D.
        strokeEllipseAsPath(ellipse);
        return;
    }

    Direct2D::drawEllipse(*platformContext(), ellipse, m_state.strokeStyle, m_state.strokeColor, m_state.strokeThickness);
}

void GraphicsContextDirect2D::drawGlyphs(const Font& font, const GlyphBuffer& glyphBuffer, unsigned from, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode fontSmoothing)
{
    UNUSED_PARAM(fontSmoothing);
    if (!font.platformData().size())
        return;

    auto xOffset = point.x();
    Vector<unsigned short> glyphs(numGlyphs);
    Vector<float> horizontalAdvances(numGlyphs);
    Vector<DWRITE_GLYPH_OFFSET> glyphOffsets(numGlyphs);

    for (unsigned i = 0; i < numGlyphs; ++i) {
        if (i + from >= glyphBuffer.advancesCount())
            break;

        auto advance = glyphBuffer.advances(i + from);
        if (!advance)
            continue;

        glyphs[i] = glyphBuffer.glyphAt(i + from);
        horizontalAdvances[i] = advance->width();
        glyphOffsets[i].advanceOffset = advance->width();
        glyphOffsets[i].ascenderOffset = advance->height();
    }

    double syntheticBoldOffset = font.syntheticBoldOffset();

    auto& state = graphicsContext().state();
    Direct2D::drawGlyphs(m_platformContext, Direct2D::FillSource(state, graphicsContext()), Direct2D::StrokeSource(state, graphicsContext()),
        Direct2D::ShadowState(state), point, font, syntheticBoldOffset, glyphs, horizontalAdvances, glyphOffsets, xOffset,
        state.textDrawingMode, state.strokeThickness, state.shadowOffset, state.shadowColor);
}

GraphicsContext::ClipToDrawingCommandsResult GraphicsContextDirect2D::clipToDrawingCommands(const FloatRect&, const DestinationColorSpace&, Function<void(GraphicsContext&)>&&)
{
    // FIXME: Not implemented.
    return ClipToDrawingCommandsResult::Success;
}

void GraphicsContextDirect2D::clipToImageBuffer(ImageBuffer& buffer, const FloatRect& destRect)
{
    RefPtr<Image> image = buffer.copyImage(DontCopyBackingStore);
    if (!image)
        return;

    auto* context = &graphicsContext();
    if (auto surface = image->nativeImageForCurrentFrame(context))
        notImplemented();
}

#if ENABLE(VIDEO)
void GraphicsContextDirect2D::paintFrameForMedia(MediaPlayer&, const FloatRect&)
{
    // FIXME: Not implemented.
}
#endif


}
