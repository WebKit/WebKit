/*
 * Copyright (C) 2006-2019 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012, Intel Corporation
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
#include "Direct2DOperations.h"

#if USE(DIRECT2D)

#include "Direct2DUtilities.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "GraphicsContextPlatformPrivateDirect2D.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "ImageDecoderDirect2D.h"
#include "NotImplemented.h"
#include "Path.h"
#include "PlatformContextDirect2D.h"
#include "ShadowBlur.h"
#include <algorithm>
#include <d2d1.h>

namespace WebCore {
namespace Direct2D {

enum PatternAdjustment { NoAdjustment, AdjustPatternForGlobalAlpha };
enum AlphaPreservation { DoNotPreserveAlpha, PreserveAlpha };

// FIXME: Replace once GraphicsContext::dashedLineCornerWidthForStrokeWidth()
// is refactored as a static public function.
static float dashedLineCornerWidthForStrokeWidth(float strokeWidth, StrokeStyle strokeStyle, float strokeThickness)
{
    return strokeStyle == DottedStroke ? strokeThickness : std::min(2.0f * strokeThickness, std::max(strokeThickness, strokeWidth / 3.0f));
}

// FIXME: Replace once GraphicsContext::dashedLinePatternWidthForStrokeWidth()
// is refactored as a static public function.
static float dashedLinePatternWidthForStrokeWidth(float strokeWidth, StrokeStyle strokeStyle, float strokeThickness)
{
    return strokeStyle == DottedStroke ? strokeThickness : std::min(3.0f * strokeThickness, std::max(strokeThickness, strokeWidth / 3.0f));
}

// FIXME: Replace once GraphicsContext::dashedLinePatternOffsetForPatternAndStrokeWidth()
// is refactored as a static public function.
static float dashedLinePatternOffsetForPatternAndStrokeWidth(float patternWidth, float strokeWidth)
{
    // Pattern starts with full fill and ends with the empty fill.
    // 1. Let's start with the empty phase after the corner.
    // 2. Check if we've got odd or even number of patterns and whether they fully cover the line.
    // 3. In case of even number of patterns and/or remainder, move the pattern start position
    // so that the pattern is balanced between the corners.
    float patternOffset = patternWidth;
    int numberOfSegments = std::floor(strokeWidth / patternWidth);
    bool oddNumberOfSegments = numberOfSegments % 2;
    float remainder = strokeWidth - (numberOfSegments * patternWidth);
    if (oddNumberOfSegments && remainder)
        patternOffset -= remainder / 2.0f;
    else if (!oddNumberOfSegments) {
        if (remainder)
            patternOffset += patternOffset - (patternWidth + remainder) / 2.0f;
        else
            patternOffset += patternWidth / 2.0f;
    }

    return patternOffset;
}

// FIXME: Replace once GraphicsContext::centerLineAndCutOffCorners()
// is refactored as a static public function.
static Vector<FloatPoint> centerLineAndCutOffCorners(bool isVerticalLine, float cornerWidth, FloatPoint point1, FloatPoint point2)
{
    // Center line and cut off corners for pattern painting.
    if (isVerticalLine) {
        float centerOffset = (point2.x() - point1.x()) / 2.0f;
        point1.move(centerOffset, cornerWidth);
        point2.move(-centerOffset, -cornerWidth);
    } else {
        float centerOffset = (point2.y() - point1.y()) / 2.0f;
        point1.move(cornerWidth, centerOffset);
        point2.move(-cornerWidth, -centerOffset);
    }

    return { point1, point2 };
}


namespace State {

void setStrokeThickness(PlatformContextDirect2D& platformContext, float strokeThickness)
{
    platformContext.setStrokeThickness(strokeThickness);
}

void setStrokeStyle(PlatformContextDirect2D& platformContext, StrokeStyle strokeStyle)
{
    platformContext.setStrokeStyle(strokeStyle);
}

void setCompositeOperation(PlatformContextDirect2D& platformContext, CompositeOperator compositeOperation, BlendMode blendMode)
{
    D2D1_BLEND_MODE targetBlendMode = D2D1_BLEND_MODE_SCREEN;
    D2D1_COMPOSITE_MODE targetCompositeMode = D2D1_COMPOSITE_MODE_SOURCE_ATOP;

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
        switch (compositeOperation) {
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

    platformContext.setBlendMode(targetBlendMode);
    platformContext.setCompositeMode(targetCompositeMode);
}

void setShouldAntialias(PlatformContextDirect2D& platformContext, bool enable)
{
    auto antialiasMode = enable ? D2D1_ANTIALIAS_MODE_PER_PRIMITIVE : D2D1_ANTIALIAS_MODE_ALIASED;
    platformContext.renderTarget()->SetAntialiasMode(antialiasMode);
}

void setCTM(PlatformContextDirect2D& platformContext, const AffineTransform& transform)
{
    ASSERT(platformContext.renderTarget());
    platformContext.renderTarget()->SetTransform(transform);

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->setCTM(transform);
}

AffineTransform getCTM(PlatformContextDirect2D& platformContext)
{
    ASSERT(platformContext.renderTarget());
    D2D1_MATRIX_3X2_F currentTransform;
    platformContext.renderTarget()->GetTransform(&currentTransform);
    return currentTransform;
}

IntRect getClipBounds(PlatformContextDirect2D& platformContext)
{
    D2D1_SIZE_F clipSize;
    if (auto clipLayer = platformContext.clipLayer())
        clipSize = clipLayer->GetSize();
    else
        clipSize = platformContext.renderTarget()->GetSize();

    FloatRect clipBounds(IntPoint(), clipSize);

    return enclosingIntRect(clipBounds);
}

FloatRect roundToDevicePixels(PlatformContextDirect2D& platformContext, const FloatRect& rect)
{
    notImplemented();

    return rect;
}

bool isAcceleratedContext(PlatformContextDirect2D& platformContext)
{
    auto renderProperties = D2D1::RenderTargetProperties();
    renderProperties.type = D2D1_RENDER_TARGET_TYPE_HARDWARE;
    return platformContext.renderTarget()->IsSupported(&renderProperties);
}

} // namespace State

FillSource::FillSource(const GraphicsContextState& state, PlatformContextDirect2D& platformContext)
    : globalAlpha(state.alpha)
    , color(state.fillColor)
    , fillRule(state.fillRule)
{
    if (state.fillPattern) {
        AffineTransform userToBaseCTM; // FIXME: This isn't really needed on Windows
        brush = state.fillPattern->createPlatformPattern(platformContext, state.alpha, userToBaseCTM);
    } else if (state.fillGradient)
        brush = state.fillGradient->createPlatformGradientIfNecessary(platformContext.renderTarget());
    else
        brush = platformContext.brushWithColor(color);
}

StrokeSource::StrokeSource(const GraphicsContextState& state, PlatformContextDirect2D& platformContext)
    : globalAlpha(state.alpha)
    , thickness(state.strokeThickness)
    , color(state.strokeColor)
{
    if (state.strokePattern) {
        AffineTransform userToBaseCTM; // FIXME: This isn't really needed on Windows
        brush = state.strokePattern->createPlatformPattern(platformContext, state.alpha, userToBaseCTM);
    } else if (state.strokeGradient)
        brush = state.strokeGradient->createPlatformGradientIfNecessary(platformContext.renderTarget());
    else
        brush = platformContext.brushWithColor(color);
}

ShadowState::ShadowState(const GraphicsContextState& state)
    : offset(state.shadowOffset)
    , blur(state.shadowBlur)
    , color(state.shadowColor)
    , ignoreTransforms(state.shadowsIgnoreTransforms)
    , globalAlpha(state.alpha)
    , globalCompositeOperator(state.compositeOperator)
{
}

bool ShadowState::isVisible() const
{
    return color.isVisible() && (offset.width() || offset.height() || blur);
}

bool ShadowState::isRequired(PlatformContextDirect2D& platformContext) const
{
    // We can't avoid ShadowBlur if the shadow has blur.
    if (color.isVisible() && blur)
        return true;

    // We can avoid ShadowBlur and optimize, since we're not drawing on a
    // canvas and box shadows are affected by the transformation matrix.
    if (!ignoreTransforms)
        return false;

    // We can avoid ShadowBlur, since there are no transformations to apply to the canvas.
    if (State::getCTM(platformContext).isIdentity())
        return false;

    // Otherwise, no chance avoiding ShadowBlur.
    return true;
}

void setLineCap(PlatformContextDirect2D& platformContext, LineCap lineCap)
{
    D2D1_CAP_STYLE capStyle = D2D1_CAP_STYLE_FLAT;
    switch (lineCap) {
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

    platformContext.setLineCap(capStyle);
}

void setLineDash(PlatformContextDirect2D& platformContext, const DashArray& dashes, float dashOffset)
{
    platformContext.setDashes(dashes);
    platformContext.setDashOffset(dashOffset);
}

void setLineJoin(PlatformContextDirect2D& platformContext, LineJoin lineJoin)
{
    D2D1_LINE_JOIN joinStyle = D2D1_LINE_JOIN_MITER;
    switch (lineJoin) {
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

    platformContext.setLineJoin(joinStyle);
}

void setMiterLimit(PlatformContextDirect2D& platformContext, float miterLimit)
{
    platformContext.setMiterLimit(miterLimit);
}

void fillRect(PlatformContextDirect2D& platformContext, const FloatRect& rect, const FillSource& fillSource, const ShadowState& shadowState)
{
    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);
    PlatformContextStateSaver stateSaver(platformContext);
    Function<void(ID2D1RenderTarget*)> drawFunction = [&platformContext, rect, &fillSource](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = rect;
        renderTarget->FillRectangle(&d2dRect, fillSource.brush.get());
    };

    if (shadowState.isVisible())
        drawWithShadow(platformContext, rect, shadowState, drawFunction);
    else
        drawWithoutShadow(platformContext, rect, drawFunction);
}

void fillRect(PlatformContextDirect2D& platformContext, const FloatRect& rect, const Color& color, const ShadowState& shadowState)
{
    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);
    PlatformContextStateSaver stateSaver(platformContext);
    Function<void(ID2D1RenderTarget*)> drawFunction = [&platformContext, color, rect](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = rect;
        renderTarget->FillRectangle(&d2dRect, platformContext.brushWithColor(color).get());
    };

    if (shadowState.isVisible())
        drawWithShadow(platformContext, rect, shadowState, drawFunction);
    else
        drawWithoutShadow(platformContext, rect, drawFunction);
}

void fillRoundedRect(PlatformContextDirect2D& platformContext, const FloatRoundedRect& rect, const Color& color, const ShadowState& shadowState)
{
    auto context = platformContext.renderTarget();
    context->SetTags(1, __LINE__);

    const FloatRect& r = rect.rect();
    const FloatRoundedRect::Radii& radii = rect.radii();
    bool equalWidths = (radii.topLeft().width() == radii.topRight().width() && radii.topRight().width() == radii.bottomLeft().width() && radii.bottomLeft().width() == radii.bottomRight().width());
    bool equalHeights = (radii.topLeft().height() == radii.bottomLeft().height() && radii.bottomLeft().height() == radii.topRight().height() && radii.topRight().height() == radii.bottomRight().height());
    bool hasCustomFill = false; // FIXME: Why isn't a FillSource passed to this function?
    if (!hasCustomFill && equalWidths && equalHeights && radii.topLeft().width() * 2 == r.width() && radii.topLeft().height() * 2 == r.height()) {
        PlatformContextStateSaver stateSaver(platformContext);
        Function<void(ID2D1RenderTarget*)> drawFunction = [&platformContext, color, rect, radii, r](ID2D1RenderTarget* renderTarget) {
            auto roundedRect = D2D1::RoundedRect(r, radii.topLeft().width(), radii.topLeft().height());
            renderTarget->FillRoundedRectangle(&roundedRect, platformContext.brushWithColor(color).get());
        };

        if (shadowState.isVisible())
            drawWithShadow(platformContext, r, shadowState, drawFunction);
        else
            drawWithoutShadow(platformContext, r, drawFunction);
    } else {
        PlatformContextStateSaver stateSaver(platformContext);
        Path path;
        path.addRoundedRect(rect);
        fillPath(platformContext, path, color, shadowState);
    }
}

void fillRectWithRoundedHole(PlatformContextDirect2D& platformContext, const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const FillSource& fillSource, const ShadowState& shadowState)
{
    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);

    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    FillSource fillWithHoleSource = fillSource;
    fillWithHoleSource.fillRule = WindRule::EvenOdd;

    fillPath(platformContext, path, fillWithHoleSource, shadowState);
}

void fillRectWithGradient(PlatformContextDirect2D& platformContext, const FloatRect& rect, ID2D1Brush* gradient)
{
    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);
    PlatformContextStateSaver stateSaver(platformContext);
    Function<void(ID2D1RenderTarget*)> drawFunction = [&platformContext, rect, &gradient](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = rect;
        renderTarget->FillRectangle(&d2dRect, gradient);
    };

    drawWithoutShadow(platformContext, rect, drawFunction);
}

void fillPath(PlatformContextDirect2D& platformContext, const Path& path, const FillSource& fillSource, const ShadowState& shadowState)
{
    if (path.activePath()) {
        // Make sure it's closed. This might fail if the path was already closed, so
        // ignore the return value.
        path.activePath()->Close();
    }

    PlatformContextStateSaver stateSaver(platformContext);

    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);

    COMPtr<ID2D1GeometryGroup> pathToFill;
    path.createGeometryWithFillMode(fillSource.fillRule, pathToFill);

    context->SetTags(1, __LINE__);

    Function<void(ID2D1RenderTarget*)> drawFunction = [&platformContext, &pathToFill, &fillSource](ID2D1RenderTarget* renderTarget) {
        renderTarget->FillGeometry(pathToFill.get(), fillSource.brush.get());
    };

    if (shadowState.isVisible()) {
        FloatRect boundingRect = path.fastBoundingRect();
        drawWithShadow(platformContext, boundingRect, shadowState, drawFunction);
    } else {
        FloatRect contextRect(FloatPoint(), context->GetSize());
        drawWithoutShadow(platformContext, contextRect, drawFunction);
    }
}

void fillPath(PlatformContextDirect2D& platformContext, const Path& path, const Color& color, const ShadowState& shadowState)
{
    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);

    COMPtr<ID2D1GeometryGroup> pathToFill;
    path.createGeometryWithFillMode(WindRule::EvenOdd, pathToFill);

    context->SetTags(1, __LINE__);

    Function<void(ID2D1RenderTarget*)> drawFunction = [&platformContext, &pathToFill, color](ID2D1RenderTarget* renderTarget) {
        renderTarget->FillGeometry(pathToFill.get(), platformContext.brushWithColor(color).get());
    };

    if (shadowState.isVisible()) {
        FloatRect boundingRect = path.fastBoundingRect();
        drawWithShadow(platformContext, boundingRect, shadowState, drawFunction);
    } else {
        FloatRect contextRect(FloatPoint(), context->GetSize());
        drawWithoutShadow(platformContext, contextRect, drawFunction);
    }
}

void strokeRect(PlatformContextDirect2D& platformContext, const FloatRect& rect, float lineWidth, const StrokeSource& strokeSource, const ShadowState& shadowState)
{
    Function<void(ID2D1RenderTarget*)> drawFunction = [&platformContext, &strokeSource, rect, lineWidth](ID2D1RenderTarget* renderTarget) {
        renderTarget->SetTags(1, __LINE__);
        const D2D1_RECT_F d2dRect = rect;
        renderTarget->DrawRectangle(&d2dRect, strokeSource.brush.get(), lineWidth, platformContext.strokeStyle());
    };

    if (shadowState.isVisible())
        drawWithShadow(platformContext, rect, shadowState, drawFunction);
    else
        drawWithoutShadow(platformContext, rect, drawFunction);
}

void strokePath(PlatformContextDirect2D& platformContext, const Path& path, const StrokeSource& strokeSource, const ShadowState& shadowState)
{
    auto context = platformContext.renderTarget();
    
    context->SetTags(1, __LINE__);

    PlatformContextStateSaver stateSaver(platformContext);
    auto boundingRect = path.fastBoundingRect();
    Function<void(ID2D1RenderTarget*)> drawFunction = [&platformContext, &path, &strokeSource](ID2D1RenderTarget* renderTarget) {
        renderTarget->DrawGeometry(path.platformPath(), strokeSource.brush.get(), strokeSource.thickness, platformContext.strokeStyle());
    };

    if (shadowState.isVisible())
        drawWithShadow(platformContext, boundingRect, shadowState, drawFunction);
    else
        drawWithoutShadow(platformContext, boundingRect, drawFunction);
}

void drawPath(PlatformContextDirect2D& platformContext, const Path& path, const StrokeSource& strokeSource, const ShadowState&)
{
    auto context = platformContext.renderTarget();

    if (path.activePath())
        path.activePath()->Close();

    context->SetTags(1, __LINE__);

    auto rect = path.fastBoundingRect();
    drawWithoutShadow(platformContext, rect, [&platformContext, &strokeSource, &path](ID2D1RenderTarget* renderTarget) {
        renderTarget->DrawGeometry(path.platformPath(), strokeSource.brush.get(), strokeSource.thickness, platformContext.strokeStyle());
    });

    flush(platformContext);
}

void drawWithShadowHelper(ID2D1RenderTarget* context, ID2D1Bitmap* bitmap, const Color& shadowColor, const FloatSize& shadowOffset, float shadowBlur)
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

    deviceContext->DrawImage(compositor.get(), D2D1_INTERPOLATION_MODE_LINEAR);
}

void drawWithShadow(PlatformContextDirect2D& platformContext, const FloatRect& boundingRect, const ShadowState& shadowState, const WTF::Function<void(ID2D1RenderTarget*)>& drawCommands)
{
    auto context = platformContext.renderTarget();

    // Render the current geometry to a bitmap context
    COMPtr<ID2D1BitmapRenderTarget> bitmapTarget = createBitmapRenderTarget(context);

    bitmapTarget->BeginDraw();
    drawCommands(bitmapTarget.get());
    HRESULT hr = bitmapTarget->EndDraw();
    RELEASE_ASSERT(SUCCEEDED(hr));

    COMPtr<ID2D1Bitmap> bitmap;
    hr = bitmapTarget->GetBitmap(&bitmap);
    RELEASE_ASSERT(SUCCEEDED(hr));

    drawWithShadowHelper(context, bitmap.get(), shadowState.color, shadowState.offset, shadowState.blur);
}

void drawWithoutShadow(PlatformContextDirect2D& platformContext, const FloatRect& /*boundingRect*/, const WTF::Function<void(ID2D1RenderTarget*)>& drawCommands)
{
    drawCommands(platformContext.renderTarget());
}

void clearRect(PlatformContextDirect2D& platformContext, const FloatRect& rect)
{
    drawWithoutShadow(platformContext, rect, [&platformContext, rect](ID2D1RenderTarget* renderTarget) {
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
        renderTarget->FillRectangle(rectToClear, platformContext.brushWithColor(Color(D2D1::ColorF(0, 0, 0, 0))).get());
    });
}


void drawGlyphs(PlatformContextDirect2D& platformContext, const FillSource& fillSource, const StrokeSource& strokeSource, const ShadowState& shadowState, const FloatPoint& point, const Font& font, double syntheticBoldOffset, const Vector<unsigned short>& glyphs, const Vector<float>& horizontalAdvances, const Vector<DWRITE_GLYPH_OFFSET>& glyphOffsets, float xOffset, TextDrawingModeFlags textDrawingMode, float strokeThickness, const FloatSize& shadowOffset, const Color& shadowColor)
{
    auto renderTarget = platformContext.renderTarget();
    const FontPlatformData& platformData = font.platformData();

    platformContext.save();

    D2D1_MATRIX_3X2_F matrix;
    renderTarget->GetTransform(&matrix);

    /*
    // FIXME: Get Synthetic Oblique working
    if (platformData.syntheticOblique()) {
        static float skew = -tanf(syntheticObliqueAngle() * piFloat / 180.0f);
        auto skewMatrix = D2D1::Matrix3x2F::Skew(skew, 0);
        context->SetTransform(matrix * skewMatrix);
    }
    */

    RELEASE_ASSERT(platformData.dwFont());
    RELEASE_ASSERT(platformData.dwFontFace());

    DWRITE_GLYPH_RUN glyphRun;
    glyphRun.fontFace = platformData.dwFontFace();
    glyphRun.fontEmSize = platformData.size();
    glyphRun.glyphCount = glyphs.size();
    glyphRun.glyphIndices = glyphs.data();
    glyphRun.glyphAdvances = horizontalAdvances.data();
    glyphRun.glyphOffsets = nullptr;
    glyphRun.isSideways = platformData.orientation() == FontOrientation::Vertical;
    glyphRun.bidiLevel = 0;

    /*
    // FIXME(): Get Shadows working
    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    graphicsContext.getShadow(shadowOffset, shadowBlur, shadowColor);

    bool hasSimpleShadow = graphicsContext.textDrawingMode() == TextModeFill && shadowColor.isValid() && !shadowBlur && (!graphicsContext.shadowsIgnoreTransforms() || graphicsContext.getCTM().isIdentityOrTranslationOrFlipped());
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        graphicsContext.clearShadow();
        Color fillColor = graphicsContext.fillColor();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        float shadowTextX = point.x() + shadowOffset.width();
        // If shadows are ignoring transforms, then we haven't applied the Y coordinate flip yet, so down is negative.
        float shadowTextY = point.y() + shadowOffset.height() * (graphicsContext.shadowsIgnoreTransforms() ? -1 : 1);

        auto shadowBrush = graphicsContext.brushWithColor(shadowFillColor);
        context->DrawGlyphRun(D2D1::Point2F(shadowTextX, shadowTextY), &glyphRun, shadowBrush);
        if (font.syntheticBoldOffset())
            context->DrawGlyphRun(D2D1::Point2F(shadowTextX + font.syntheticBoldOffset(), shadowTextY), &glyphRun, shadowBrush);
    }
    */

    renderTarget->DrawGlyphRun(D2D1::Point2F(point.x(), point.y()), &glyphRun, fillSource.brush.get());
    // if (font.syntheticBoldOffset())
    //     context->DrawGlyphRun(D2D1::Point2F(point.x() + font.syntheticBoldOffset(), point.y()), &glyphRun, graphicsContext.solidFillBrush());

    // if (hasSimpleShadow)
    //     graphicsContext.setShadow(shadowOffset, shadowBlur, shadowColor);

    platformContext.restore();
}


void drawNativeImage(PlatformContextDirect2D& platformContext, IWICBitmap* image, const FloatSize& originalImageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options, float globalAlpha, const ShadowState& shadowState)
{
    auto nativeImageSize = bitmapSize(image);
    COMPtr<ID2D1Bitmap> deviceBitmap;
    HRESULT hr = platformContext.renderTarget()->CreateBitmapFromWicBitmap(image, &deviceBitmap);
    if (!SUCCEEDED(hr))
        return;

    auto imageSize = bitmapSize(deviceBitmap.get());
    drawNativeImage(platformContext, deviceBitmap.get(), imageSize, destRect, srcRect, options, globalAlpha, shadowState);
}

void drawNativeImage(PlatformContextDirect2D& platformContext, ID2D1Bitmap* image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options, float globalAlpha, const ShadowState& shadowState)
{
    auto bitmapSize = image->GetSize();

    float currHeight = orientation.usesWidthAsHeight() ? bitmapSize.width : bitmapSize.height;
    if (currHeight <= srcRect.y())
        return;

    auto context = platformContext.renderTarget();

    D2D1_MATRIX_3X2_F ctm;
    context->GetTransform(&ctm);

    AffineTransform transform(ctm);

    PlatformContextStateSaver stateSaver(platformContext);

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

    State::setCompositeOperation(platformContext, options.compositeOperator(), options.blendMode());

    // ImageOrientation expects the origin to be at (0, 0).
    transform.translate(adjustedDestRect.x(), adjustedDestRect.y());
    context->SetTransform(transform);
    adjustedDestRect.setLocation(FloatPoint());

    if (options.orientation() != ImageOrientation::None) {
        concatCTM(platformContext, options.orientation().transformFromDefault(adjustedDestRect.size()));
        if (options.orientation().usesWidthAsHeight()) {
            // The destination rect will have it's width and height already reversed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            adjustedDestRect = FloatRect(adjustedDestRect.x(), adjustedDestRect.y(), adjustedDestRect.height(), adjustedDestRect.width());
        }
    }

    context->SetTags(1, __LINE__);

    Function<void(ID2D1RenderTarget*)> drawFunction = [image, adjustedDestRect, globalAlpha, srcRect](ID2D1RenderTarget* renderTarget) {
        renderTarget->DrawBitmap(image, adjustedDestRect, globalAlpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcRect);
    };

    if (shadowState.isVisible())
        drawWithShadow(platformContext, adjustedDestRect, shadowState, drawFunction);
    else
        drawWithoutShadow(platformContext, adjustedDestRect, drawFunction);

    if (!stateSaver.didSave())
        context->SetTransform(ctm);
}

void drawPattern(PlatformContextDirect2D& platformContext, COMPtr<ID2D1Bitmap>&& tileImage, const IntSize& size, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, CompositeOperator compositeOperator, BlendMode blendMode)
{
    auto context = platformContext.renderTarget();
    PlatformContextStateSaver stateSaver(platformContext);

    clip(platformContext, destRect);

    State::setCompositeOperation(platformContext, compositeOperator, blendMode);

    auto bitmapBrushProperties = D2D1::BitmapBrushProperties();
    bitmapBrushProperties.extendModeX = D2D1_EXTEND_MODE_WRAP;
    bitmapBrushProperties.extendModeY = D2D1_EXTEND_MODE_WRAP;

    // Create a brush transformation so we paint using the section of the image we care about.
    AffineTransform transformation = patternTransform;
    transformation.translate(destRect.location());

    auto brushProperties = D2D1::BrushProperties();
    brushProperties.transform = transformation;
    brushProperties.opacity = 1.0f;

    // If we only want a subset of the bitmap, we need to create a cropped bitmap image. According to the documentation,
    // this does not allocate new bitmap memory.
    if (size.width() > destRect.width() || size.height() > destRect.height()) {
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
    ASSERT(SUCCEEDED(hr));
    if (!SUCCEEDED(hr))
        return;

    drawWithoutShadow(platformContext, destRect, [destRect, patternBrush](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = destRect;
        renderTarget->FillRectangle(&d2dRect, patternBrush.get());
    });
}

void drawRect(PlatformContextDirect2D& platformContext, const FloatRect& rect, float borderThickness, const Color& fillColor, StrokeStyle strokeStyle, const Color& strokeColor)
{
    // FIXME: this function does not handle patterns and gradients like drawPath does, it probably should.
    ASSERT(!rect.isEmpty());

    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);

    drawWithoutShadow(platformContext, rect, [&platformContext, rect, fillColor, strokeColor, strokeStyle](ID2D1RenderTarget* renderTarget) {
        const D2D1_RECT_F d2dRect = rect;
        auto fillBrush = platformContext.brushWithColor(fillColor);
        renderTarget->FillRectangle(&d2dRect, fillBrush.get());
        if (strokeStyle != NoStroke) {
            auto strokeBrush = platformContext.brushWithColor(strokeColor);
            renderTarget->DrawRectangle(&d2dRect, strokeBrush.get(), platformContext.strokeThickness(), platformContext.strokeStyle());
        }
    });
}

void drawLine(PlatformContextDirect2D& platformContext, const FloatPoint& point1, const FloatPoint& point2, StrokeStyle strokeStyle, const Color& strokeColor, float strokeThickness, bool shouldAntialias)
{
    bool isVerticalLine = (point1.x() + strokeThickness == point2.x());
    float strokeWidth = isVerticalLine ? point2.y() - point1.y() : point2.x() - point1.x();
    if (!strokeThickness || !strokeWidth)
        return;

    auto context = platformContext.renderTarget();

    float cornerWidth = 0;
    bool drawsDashedLine = strokeStyle == DottedStroke || strokeStyle == DashedStroke;

    COMPtr<ID2D1StrokeStyle> d2dStrokeStyle;
    PlatformContextStateSaver stateSaver(platformContext, drawsDashedLine);
    if (drawsDashedLine) {
        // Figure out end points to ensure we always paint corners.
        cornerWidth = dashedLineCornerWidthForStrokeWidth(strokeWidth, strokeStyle, strokeThickness);
        strokeWidth -= 2 * cornerWidth;
        float patternWidth = dashedLinePatternWidthForStrokeWidth(strokeWidth, strokeStyle, strokeThickness);
        // Check if corner drawing sufficiently covers the line.
        if (strokeWidth <= patternWidth + 1)
            return;

        float patternOffset = dashedLinePatternOffsetForPatternAndStrokeWidth(patternWidth, strokeWidth);
        const float dashes[2] = { patternWidth, patternWidth };
        auto strokeStyleProperties = platformContext.strokeStyleProperties();
        GraphicsContext::systemFactory()->CreateStrokeStyle(&strokeStyleProperties, dashes, ARRAYSIZE(dashes), &d2dStrokeStyle);

        platformContext.setPatternWidth(patternWidth);
        platformContext.setPatternOffset(patternOffset);
        platformContext.setDashes(DashArray(2, patternWidth));

        d2dStrokeStyle = platformContext.strokeStyle();
    }

    auto centeredPoints = centerLineAndCutOffCorners(isVerticalLine, cornerWidth, point1, point2);
    auto p1 = centeredPoints[0];
    auto p2 = centeredPoints[1];

    context->SetTags(1, __LINE__);

    FloatRect boundingRect(p1, p2);

    drawWithoutShadow(platformContext, boundingRect, [&platformContext, p1, p2, d2dStrokeStyle, strokeColor, strokeThickness](ID2D1RenderTarget* renderTarget) {
        renderTarget->DrawLine(p1, p2, platformContext.brushWithColor(strokeColor).get(), strokeThickness, d2dStrokeStyle.get());
    });
}

void drawLinesForText(PlatformContextDirect2D&, const FloatPoint&, float, const DashArray&, bool, bool, const Color&)
{
    notImplemented();
}

void drawDotsForDocumentMarker(PlatformContextDirect2D&, const FloatRect&, DocumentMarkerLineStyle)
{
    notImplemented();
}

void fillEllipse(PlatformContextDirect2D& platformContext, const FloatRect& rect, const Color& fillColor, StrokeStyle strokeStyle, const Color& strokeColor, float strokeThickness)
{
    auto ellipse = D2D1::Ellipse(rect.center(), 0.5 * rect.width(), 0.5 * rect.height());

    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);

    drawWithoutShadow(platformContext, rect, [&platformContext, ellipse, fillColor, strokeStyle, strokeColor, strokeThickness](ID2D1RenderTarget* renderTarget) {
        auto fillBrush = platformContext.brushWithColor(fillColor);
        renderTarget->FillEllipse(&ellipse, fillBrush.get());
        if (strokeStyle != StrokeStyle::NoStroke) {
            auto strokeBrush = platformContext.brushWithColor(strokeColor);
            renderTarget->DrawEllipse(&ellipse, strokeBrush.get(), strokeThickness, platformContext.strokeStyle());
        }
    });
}

void drawEllipse(PlatformContextDirect2D& platformContext, const FloatRect& rect, StrokeStyle strokeStyle, const Color& strokeColor, float strokeThickness)
{
    if (strokeStyle == StrokeStyle::NoStroke)
        return;

    auto ellipse = D2D1::Ellipse(rect.center(), 0.5 * rect.width(), 0.5 * rect.height());

    auto context = platformContext.renderTarget();

    context->SetTags(1, __LINE__);

    drawWithoutShadow(platformContext, rect, [&platformContext, ellipse, strokeColor, strokeThickness](ID2D1RenderTarget* renderTarget) {
        auto strokeBrush = platformContext.brushWithColor(strokeColor);
        renderTarget->DrawEllipse(&ellipse, strokeBrush.get(), strokeThickness, platformContext.strokeStyle());
    });
}

void drawFocusRing(PlatformContextDirect2D&, const Path&, float, const Color&)
{
    notImplemented();
}

void drawFocusRing(PlatformContextDirect2D&, const Vector<FloatRect>&, float, const Color&)
{
    notImplemented();
}

void flush(PlatformContextDirect2D& platformContext)
{
    ASSERT(platformContext.renderTarget());
    D2D1_TAG first, second;
    HRESULT hr = platformContext.renderTarget()->Flush(&first, &second);

    RELEASE_ASSERT(SUCCEEDED(hr));
}

void save(PlatformContextDirect2D& platformContext)
{
    platformContext.save();

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->save();
}

void restore(PlatformContextDirect2D& platformContext)
{
    platformContext.restore();

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->restore();
}

void translate(PlatformContextDirect2D& platformContext, float x, float y)
{
    ASSERT(platformContext.renderTarget());

    D2D1_MATRIX_3X2_F currentTransform;
    platformContext.renderTarget()->GetTransform(&currentTransform);

    auto translation = D2D1::Matrix3x2F::Translation(x, y);
    platformContext.renderTarget()->SetTransform(translation * currentTransform);

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->translate(x, y);
}

void rotate(PlatformContextDirect2D& platformContext, float angleInRadians)
{
    ASSERT(platformContext.renderTarget());

    D2D1_MATRIX_3X2_F currentTransform;
    platformContext.renderTarget()->GetTransform(&currentTransform);

    auto rotation = D2D1::Matrix3x2F::Rotation(rad2deg(angleInRadians));
    platformContext.renderTarget()->SetTransform(rotation * currentTransform);

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->rotate(angleInRadians);
}

void scale(PlatformContextDirect2D& platformContext, const FloatSize& size)
{
    ASSERT(platformContext.renderTarget());

    D2D1_MATRIX_3X2_F currentTransform;
    platformContext.renderTarget()->GetTransform(&currentTransform);

    auto scale = D2D1::Matrix3x2F::Scale(size);
    platformContext.renderTarget()->SetTransform(scale * currentTransform);

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->scale(size);
}

void concatCTM(PlatformContextDirect2D& platformContext, const AffineTransform& affineTransform)
{
    ASSERT(platformContext.renderTarget());

    D2D1_MATRIX_3X2_F currentTransform;
    platformContext.renderTarget()->GetTransform(&currentTransform);

    D2D1_MATRIX_3X2_F transformToConcat = affineTransform;
    platformContext.renderTarget()->SetTransform(transformToConcat * currentTransform);

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->concatCTM(affineTransform);
}

void beginTransparencyLayer(PlatformContextDirect2D& platformContext, float opacity)
{
    PlatformContextDirect2D::TransparencyLayerState transparencyLayer;
    transparencyLayer.opacity = opacity;

    transparencyLayer.renderTarget = createBitmapRenderTarget(platformContext.renderTarget());

    platformContext.m_transparencyLayerStack.append(WTFMove(transparencyLayer));

    platformContext.m_transparencyLayerStack.last().renderTarget->BeginDraw();
    platformContext.m_transparencyLayerStack.last().renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
}

void endTransparencyLayer(PlatformContextDirect2D& platformContext)
{
    auto currentLayer = platformContext.m_transparencyLayerStack.takeLast();
    auto renderTarget = currentLayer.renderTarget;
    if (!renderTarget)
        return;

    HRESULT hr = renderTarget->EndDraw();
    RELEASE_ASSERT(SUCCEEDED(hr));

    COMPtr<ID2D1Bitmap> bitmap;
    hr = renderTarget->GetBitmap(&bitmap);
    RELEASE_ASSERT(SUCCEEDED(hr));

    auto context = platformContext.renderTarget();

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

void clip(PlatformContextDirect2D& platformContext, const FloatRect& rect)
{
    if (platformContext.m_renderStates.isEmpty())
        save(platformContext);

    platformContext.renderTarget()->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    platformContext.m_renderStates.last().m_clips.append(PlatformContextDirect2D::AxisAlignedClip);

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->clip(rect);
}

void clipOut(PlatformContextDirect2D& platformContext, const FloatRect& rect)
{
    Path path;
    path.addRect(rect);

    clipOut(platformContext, path);
}

void clipOut(PlatformContextDirect2D& platformContext, const Path& path)
{
    // To clip Out we need the intersection of the infinite
    // clipping rect and the path we just created.
    D2D1_SIZE_F rendererSize = platformContext.renderTarget()->GetSize();
    FloatRect clipBounds(0, 0, rendererSize.width, rendererSize.height);

    Path boundingRect;
    boundingRect.addRect(clipBounds);
    boundingRect.appendGeometry(path.platformPath());

    COMPtr<ID2D1GeometryGroup> pathToClip;
    boundingRect.createGeometryWithFillMode(WindRule::EvenOdd, pathToClip);
    clipPath(platformContext, pathToClip.get());
}

void clipPath(PlatformContextDirect2D& platformContext, const Path& path, WindRule clipRule)
{
    if (path.isEmpty()) {
        Direct2D::clip(platformContext, FloatRect());
        return;
    }

    COMPtr<ID2D1GeometryGroup> pathToClip;
    path.createGeometryWithFillMode(clipRule, pathToClip);

    clipPath(platformContext, pathToClip.get());

    if (auto* graphicsContextPrivate = platformContext.graphicsContextPrivate())
        graphicsContextPrivate->clip(path);
}

void clipPath(PlatformContextDirect2D& platformContext, ID2D1Geometry* path)
{
    ASSERT(platformContext.hasSavedState());
    if (!platformContext.hasSavedState())
        return;

    COMPtr<ID2D1Layer> clipLayer;
    HRESULT hr = platformContext.renderTarget()->CreateLayer(&clipLayer);
    ASSERT(SUCCEEDED(hr));
    if (!SUCCEEDED(hr))
        return;

    platformContext.renderTarget()->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), path), clipLayer.get());
    platformContext.pushRenderClip(PlatformContextDirect2D::LayerClip);
    platformContext.setActiveLayer(WTFMove(clipLayer));
}

void clipToImageBuffer(PlatformContextDirect2D&, ID2D1RenderTarget*, const FloatRect&)
{
    notImplemented();
}


void copyBits(const uint8_t* srcRows, unsigned rowCount, unsigned colCount, unsigned srcStride, unsigned destStride, uint8_t* destRows)
{
    for (unsigned y = 0; y < rowCount; ++y) {
        // Source data may be power-of-two sized, so we need to only copy the bits that
        // correspond to the rectangle supplied by the caller.
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(srcRows + srcStride * y);
        uint32_t* destRow = reinterpret_cast<uint32_t*>(destRows + destStride * y);
        memcpy(destRow, srcRow, colCount);
    }
}

} // namespace Direct2D

} // namespace WebCore

#endif // USE(DIRECT2D)
