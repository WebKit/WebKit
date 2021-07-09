/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
#include "CairoOperations.h"

#if USE(CAIRO)

#include "CairoUniquePtr.h"
#include "CairoUtilities.h"
#include "DrawErrorUnderline.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "GraphicsContextCairo.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "Path.h"
#include "ShadowBlur.h"
#include <algorithm>
#include <cairo.h>

namespace WebCore {
namespace Cairo {

enum PatternAdjustment { NoAdjustment, AdjustPatternForGlobalAlpha };
enum AlphaPreservation { DoNotPreserveAlpha, PreserveAlpha };

static void reduceSourceByAlpha(cairo_t* cr, float alpha)
{
    if (alpha >= 1)
        return;
    cairo_push_group(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint_with_alpha(cr, alpha);
    cairo_pop_group_to_source(cr);
}

static void prepareCairoContextSource(cairo_t* cr, cairo_pattern_t* pattern, cairo_pattern_t* gradient, const Color& color, float globalAlpha)
{
    if (pattern) {
        // Pattern source
        cairo_set_source(cr, pattern);
        reduceSourceByAlpha(cr, globalAlpha);
    } else if (gradient) {
        // Gradient source
        cairo_set_source(cr, gradient);
    } else {
        // Solid color source
        if (globalAlpha < 1)
            setSourceRGBAFromColor(cr, color.colorWithAlphaMultipliedBy(globalAlpha));
        else
            setSourceRGBAFromColor(cr, color);
    }
}

static void clipForPatternFilling(cairo_t* cr, const FloatSize& patternSize, const AffineTransform& patternTransform, bool repeatX, bool repeatY)
{
    // Hold current cairo path in a variable for restoring it after configuring the pattern clip rectangle.
    CairoUniquePtr<cairo_path_t> currentPath(cairo_copy_path(cr));
    cairo_new_path(cr);

    // Initialize clipping extent from current cairo clip extents, then shrink if needed according to pattern.
    // Inspired by GraphicsContextQt::drawRepeatPattern.
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    FloatRect clipRect(x1, y1, x2 - x1, y2 - y1);

    FloatRect patternRect = patternTransform.mapRect(FloatRect(FloatPoint(), patternSize));

    if (!repeatX) {
        clipRect.setX(patternRect.x());
        clipRect.setWidth(patternRect.width());
    }
    if (!repeatY) {
        clipRect.setY(patternRect.y());
        clipRect.setHeight(patternRect.height());
    }
    if (!repeatX || !repeatY) {
        cairo_rectangle(cr, clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height());
        cairo_clip(cr);
    }

    // Restoring cairo path.
    cairo_append_path(cr, currentPath.get());
}

static void prepareForFilling(cairo_t* cr, const Cairo::FillSource& fillSource, PatternAdjustment patternAdjustment)
{
    cairo_set_fill_rule(cr, fillSource.fillRule == WindRule::EvenOdd ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);

    bool adjustForAlpha = patternAdjustment == AdjustPatternForGlobalAlpha;

    auto* gradient = fillSource.gradient.base.get();
    if (adjustForAlpha && fillSource.gradient.alphaAdjusted)
        gradient = fillSource.gradient.alphaAdjusted.get();

    prepareCairoContextSource(cr, fillSource.pattern.object.get(), gradient,
        fillSource.color, adjustForAlpha ? fillSource.globalAlpha : 1);

    if (fillSource.pattern.object) {
        clipForPatternFilling(cr, fillSource.pattern.size, fillSource.pattern.transform,
            fillSource.pattern.repeatX, fillSource.pattern.repeatY);
    }
}

static void prepareForStroking(cairo_t* cr, const Cairo::StrokeSource& strokeSource, AlphaPreservation alphaPreservation)
{
    bool preserveAlpha = alphaPreservation == PreserveAlpha;

    auto* gradient = strokeSource.gradient.base.get();
    if (preserveAlpha && strokeSource.gradient.alphaAdjusted)
        gradient = strokeSource.gradient.alphaAdjusted.get();

    prepareCairoContextSource(cr, strokeSource.pattern.get(), gradient,
        strokeSource.color, preserveAlpha ? strokeSource.globalAlpha : 1);
}

static void drawPatternToCairoContext(cairo_t* cr, cairo_pattern_t* pattern, const FloatRect& destRect, float alpha)
{
    cairo_translate(cr, destRect.x(), destRect.y());
    cairo_set_source(cr, pattern);
    cairo_rectangle(cr, 0, 0, destRect.width(), destRect.height());
    cairo_clip(cr);
    cairo_paint_with_alpha(cr, std::max<float>(0, std::min<float>(1.0, alpha)));
}

static inline void fillRectWithColor(cairo_t* cr, const FloatRect& rect, const Color& color)
{
    if (!color.isVisible() && cairo_get_operator(cr) == CAIRO_OPERATOR_OVER)
        return;

    setSourceRGBAFromColor(cr, color);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill(cr);
}

enum PathDrawingStyle {
    Fill = 1,
    Stroke = 2,
    FillAndStroke = Fill + Stroke
};

static void drawShadowLayerBuffer(GraphicsContextCairo& platformContext, ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize, const ShadowState& shadowState)
{
    if (auto nativeImage = layerImage.copyNativeImage(DontCopyBackingStore)) {
        drawPlatformImage(platformContext, nativeImage->platformImage().get(), FloatRect(roundedIntPoint(layerOrigin), layerSize), FloatRect(FloatPoint(), layerSize), { shadowState.globalCompositeOperator }, shadowState.globalAlpha, ShadowState());
    }
}

// FIXME: This is mostly same as drawShadowLayerBuffer, so we should merge two.
static void drawShadowImage(GraphicsContextCairo& platformContext, ImageBuffer& layerImage, const FloatRect& destRect, const FloatRect& srcRect, const ShadowState& shadowState)
{
    if (auto nativeImage = layerImage.copyNativeImage(DontCopyBackingStore)) {
        drawPlatformImage(platformContext, nativeImage->platformImage().get(), destRect, srcRect, { shadowState.globalCompositeOperator }, shadowState.globalAlpha, ShadowState());
    }
}

static void fillShadowBuffer(GraphicsContextCairo& platformContext, ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize, const ShadowState& shadowState)
{
    platformContext.save();

    if (auto nativeImage = layerImage.copyNativeImage(DontCopyBackingStore))
        clipToImageBuffer(platformContext, nativeImage->platformImage().get(), FloatRect(layerOrigin, expandedIntSize(layerSize)));

    FillSource fillSource;
    fillSource.globalAlpha = shadowState.globalAlpha;
    fillSource.color = shadowState.color;
    fillRect(platformContext, FloatRect(layerOrigin, expandedIntSize(layerSize)), fillSource, ShadowState());

    platformContext.restore();
}

static inline void drawPathShadow(GraphicsContextCairo& platformContext, const FillSource& fillSource, const StrokeSource& strokeSource, const ShadowState& shadowState, PathDrawingStyle drawingStyle)
{
    ShadowBlur shadow({ shadowState.blur, shadowState.blur }, shadowState.offset, shadowState.color, shadowState.ignoreTransforms);
    if (shadow.type() == ShadowBlur::NoShadow)
        return;

    // Calculate the extents of the rendered solid paths.
    cairo_t* cairoContext = platformContext.cr();
    CairoUniquePtr<cairo_path_t> path(cairo_copy_path(cairoContext));

    FloatRect solidFigureExtents;
    double x0 = 0;
    double x1 = 0;
    double y0 = 0;
    double y1 = 0;
    if (drawingStyle & Stroke) {
        cairo_stroke_extents(cairoContext, &x0, &y0, &x1, &y1);
        solidFigureExtents = FloatRect(x0, y0, x1 - x0, y1 - y0);
    }
    if (drawingStyle & Fill) {
        cairo_fill_extents(cairoContext, &x0, &y0, &x1, &y1);
        FloatRect fillExtents(x0, y0, x1 - x0, y1 - y0);
        solidFigureExtents.unite(fillExtents);
    }

    shadow.drawShadowLayer(State::getCTM(platformContext), State::getClipBounds(platformContext), solidFigureExtents,
        [cairoContext, drawingStyle, &path, &fillSource, &strokeSource](GraphicsContext& shadowContext)
        {
            cairo_t* cairoShadowContext = shadowContext.platformContext()->cr();

            // It's important to copy the context properties to the new shadow
            // context to preserve things such as the fill rule and stroke width.
            copyContextProperties(cairoContext, cairoShadowContext);

            if (drawingStyle & Fill) {
                cairo_save(cairoShadowContext);
                cairo_append_path(cairoShadowContext, path.get());
                prepareForFilling(cairoShadowContext, fillSource, NoAdjustment);
                cairo_fill(cairoShadowContext);
                cairo_restore(cairoShadowContext);
            }

            if (drawingStyle & Stroke) {
                cairo_append_path(cairoShadowContext, path.get());
                prepareForStroking(cairoShadowContext, strokeSource, DoNotPreserveAlpha);
                cairo_stroke(cairoShadowContext);
            }
        },
        [&platformContext, &shadowState, &cairoContext, &path](ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize)
        {
            // The original path may still be hanging around on the context and endShadowLayer
            // will take care of properly creating a path to draw the result shadow. We remove the path
            // temporarily and then restore it.
            // See: https://bugs.webkit.org/show_bug.cgi?id=108897
            cairo_new_path(cairoContext);

            drawShadowLayerBuffer(platformContext, layerImage, layerOrigin, layerSize, shadowState);

            cairo_append_path(cairoContext, path.get());
        });
}


static inline void fillCurrentCairoPath(GraphicsContextCairo& platformContext, const FillSource& fillSource)
{
    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    prepareForFilling(cr, fillSource, AdjustPatternForGlobalAlpha);
    cairo_fill(cr);

    cairo_restore(cr);
}

static void drawGlyphsToContext(cairo_t* context, cairo_scaled_font_t* scaledFont, double syntheticBoldOffset, const Vector<cairo_glyph_t>& glyphs, FontSmoothingMode fontSmoothingMode)
{
    cairo_matrix_t originalTransform;
    if (syntheticBoldOffset)
        cairo_get_matrix(context, &originalTransform);

    cairo_set_scaled_font(context, scaledFont);

    // The scaled font defaults to FontSmoothingMode::AutoSmoothing. Only override antialiasing settings if its not auto.
    if (fontSmoothingMode != FontSmoothingMode::AutoSmoothing) {
        CairoUniquePtr<cairo_font_options_t> fontOptionsSmoothing(cairo_font_options_create());
        cairo_scaled_font_get_font_options(scaledFont, fontOptionsSmoothing.get());

        switch (fontSmoothingMode) {
        case FontSmoothingMode::Antialiased:
            // Don't use CAIRO_ANTIALIAS_GRAY in Windows. It is mapped to ANTIALIASED_QUALITY which looks jaggy and faint.
#if !OS(WINDOWS)
            cairo_font_options_set_antialias(fontOptionsSmoothing.get(), CAIRO_ANTIALIAS_GRAY);
            break;
#endif
        case FontSmoothingMode::SubpixelAntialiased:
            cairo_font_options_set_antialias(fontOptionsSmoothing.get(), CAIRO_ANTIALIAS_SUBPIXEL);
            break;
        case FontSmoothingMode::NoSmoothing:
            cairo_font_options_set_antialias(fontOptionsSmoothing.get(), CAIRO_ANTIALIAS_NONE);
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        cairo_set_font_options(context, fontOptionsSmoothing.get());
    }

    cairo_show_glyphs(context, glyphs.data(), glyphs.size());

    if (syntheticBoldOffset) {
        cairo_translate(context, syntheticBoldOffset, 0);
        cairo_show_glyphs(context, glyphs.data(), glyphs.size());

        cairo_set_matrix(context, &originalTransform);
    }
}

static void drawGlyphsShadow(GraphicsContextCairo& platformContext, const ShadowState& shadowState, TextDrawingModeFlags textDrawingMode, const FloatSize& shadowOffset, const Color& shadowColor, const FloatPoint& point, cairo_scaled_font_t* scaledFont, double syntheticBoldOffset, const Vector<cairo_glyph_t>& glyphs, FontSmoothingMode fontSmoothingMode)
{
    ShadowBlur shadow({ shadowState.blur, shadowState.blur }, shadowState.offset, shadowState.color, shadowState.ignoreTransforms);
    if (!textDrawingMode.contains(TextDrawingMode::Fill) || shadow.type() == ShadowBlur::NoShadow)
        return;

    if (!shadowState.isRequired(platformContext)) {
        // Optimize non-blurry shadows, by just drawing text without the ShadowBlur.
        cairo_t* context = platformContext.cr();
        cairo_save(context);

        cairo_translate(context, shadowOffset.width(), shadowOffset.height());
        setSourceRGBAFromColor(context, shadowColor);
        drawGlyphsToContext(context, scaledFont, syntheticBoldOffset, glyphs, fontSmoothingMode);

        cairo_restore(context);
        return;
    }

    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(scaledFont, glyphs.data(), glyphs.size(), &extents);
    FloatRect fontExtentsRect(point.x() + extents.x_bearing, point.y() + extents.y_bearing, extents.width, extents.height);

    shadow.drawShadowLayer(State::getCTM(platformContext), State::getClipBounds(platformContext), fontExtentsRect,
        [scaledFont, syntheticBoldOffset, &glyphs, fontSmoothingMode](GraphicsContext& shadowContext)
        {
            drawGlyphsToContext(shadowContext.platformContext()->cr(), scaledFont, syntheticBoldOffset, glyphs, fontSmoothingMode);
        },
        [&platformContext, &shadowState](ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize)
        {
            drawShadowLayerBuffer(platformContext, layerImage, layerOrigin, layerSize, shadowState);
        });
}

static bool cairoSurfaceHasAlpha(cairo_surface_t* surface)
{
    return cairo_surface_get_content(surface) != CAIRO_CONTENT_COLOR;
}

// FIXME: Fix GraphicsContext::computeLineBoundsAndAntialiasingModeForText()
// to be a static public function that operates on CTM and strokeThickness
// arguments instead of using an underlying GraphicsContext object.
FloatRect computeLineBoundsAndAntialiasingModeForText(GraphicsContextCairo& platformContext, const FloatPoint& point, float width, bool printing, Color& color, float strokeThickness)
{
    FloatPoint origin = point;
    float thickness = std::max(strokeThickness, 0.5f);
    if (printing)
        return FloatRect(origin, FloatSize(width, thickness));

    AffineTransform transform = Cairo::State::getCTM(platformContext);
    // Just compute scale in x dimension, assuming x and y scales are equal.
    float scale = transform.b() ? std::hypot(transform.a(), transform.b()) : transform.a();
    if (scale < 1.0) {
        // This code always draws a line that is at least one-pixel line high,
        // which tends to visually overwhelm text at small scales. To counter this
        // effect, an alpha is applied to the underline color when text is at small scales.
        static const float minimumUnderlineAlpha = 0.4f;
        float shade = scale > minimumUnderlineAlpha ? scale : minimumUnderlineAlpha;
        color = color.colorWithAlphaMultipliedBy(shade);
    }

    FloatPoint devicePoint = transform.mapPoint(point);
    // Visual overflow might occur here due to integral roundf/ceilf. visualOverflowForDecorations adjusts the overflow value for underline decoration.
    FloatPoint deviceOrigin = FloatPoint(roundf(devicePoint.x()), ceilf(devicePoint.y()));
    if (auto inverse = transform.inverse())
        origin = inverse.value().mapPoint(deviceOrigin);
    return FloatRect(origin, FloatSize(width, thickness));
};

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

void setStrokeThickness(GraphicsContextCairo& platformContext, float strokeThickness)
{
    cairo_set_line_width(platformContext.cr(), strokeThickness);
}

void setStrokeStyle(GraphicsContextCairo& platformContext, StrokeStyle strokeStyle)
{
    static const double dashPattern[] = { 5.0, 5.0 };
    static const double dotPattern[] = { 1.0, 1.0 };

    cairo_t* cr = platformContext.cr();
    switch (strokeStyle) {
    case NoStroke:
        // FIXME: is it the right way to emulate NoStroke?
        cairo_set_line_width(cr, 0);
        break;
    case SolidStroke:
    case DoubleStroke:
    case WavyStroke:
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=94110 - Needs platform support.
        cairo_set_dash(cr, 0, 0, 0);
        break;
    case DottedStroke:
        cairo_set_dash(cr, dotPattern, 2, 0);
        break;
    case DashedStroke:
        cairo_set_dash(cr, dashPattern, 2, 0);
        break;
    }
}

void setCompositeOperation(GraphicsContextCairo& platformContext, CompositeOperator compositeOperation, BlendMode blendMode)
{
    cairo_set_operator(platformContext.cr(), toCairoOperator(compositeOperation, blendMode));
}

void setShouldAntialias(GraphicsContextCairo& platformContext, bool enable)
{
    // When true, use the default Cairo backend antialias mode (usually this
    // enables standard 'grayscale' antialiasing); false to explicitly disable
    // antialiasing.
    cairo_set_antialias(platformContext.cr(), enable ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
}

void setCTM(GraphicsContextCairo& platformContext, const AffineTransform& transform)
{
    const cairo_matrix_t matrix = toCairoMatrix(transform);
    cairo_set_matrix(platformContext.cr(), &matrix);
}

AffineTransform getCTM(GraphicsContextCairo& platformContext)
{
    cairo_matrix_t m;
    cairo_get_matrix(platformContext.cr(), &m);
    return AffineTransform(m.xx, m.yx, m.xy, m.yy, m.x0, m.y0);
}

IntRect getClipBounds(GraphicsContextCairo& platformContext)
{
    double x1, x2, y1, y2;
    cairo_clip_extents(platformContext.cr(), &x1, &y1, &x2, &y2);
    return enclosingIntRect(FloatRect(x1, y1, x2 - x1, y2 - y1));
}

FloatRect roundToDevicePixels(GraphicsContextCairo& platformContext, const FloatRect& rect)
{
    FloatRect result;
    double x = rect.x();
    double y = rect.y();

    cairo_t* cr = platformContext.cr();
    cairo_user_to_device(cr, &x, &y);
    x = round(x);
    y = round(y);
    cairo_device_to_user(cr, &x, &y);
    result.setX(narrowPrecisionToFloat(x));
    result.setY(narrowPrecisionToFloat(y));

    // We must ensure width and height are at least 1 (or -1) when
    // we're given float values in the range between 0 and 1 (or -1 and 0).
    double width = rect.width();
    double height = rect.height();
    cairo_user_to_device_distance(cr, &width, &height);
    if (width > -1 && width < 0)
        width = -1;
    else if (width > 0 && width < 1)
        width = 1;
    else
        width = round(width);
    if (height > -1 && height < 0)
        height = -1;
    else if (height > 0 && height < 1)
        height = 1;
    else
        height = round(height);
    cairo_device_to_user_distance(cr, &width, &height);
    result.setWidth(narrowPrecisionToFloat(width));
    result.setHeight(narrowPrecisionToFloat(height));

    return result;
}

bool isAcceleratedContext(GraphicsContextCairo& platformContext)
{
    return cairo_surface_get_type(cairo_get_target(platformContext.cr())) == CAIRO_SURFACE_TYPE_GL;
}

} // namespace State

FillSource::FillSource(const GraphicsContextState& state)
    : globalAlpha(state.alpha)
    , fillRule(state.fillRule)
{
    if (state.fillPattern) {
        pattern.object = adoptRef(state.fillPattern->createPlatformPattern(AffineTransform()));

        auto& patternImage = state.fillPattern->tileImage();
        pattern.size = patternImage.size();
        pattern.transform = state.fillPattern->patternSpaceTransform();
        pattern.repeatX = state.fillPattern->repeatX();
        pattern.repeatY = state.fillPattern->repeatY();
    } else if (state.fillGradient) {
        gradient.base = state.fillGradient->createPattern(1, state.fillGradientSpaceTransform);
        if (state.alpha != 1)
            gradient.alphaAdjusted = state.fillGradient->createPattern(state.alpha, state.fillGradientSpaceTransform);
    } else
        color = state.fillColor;
}

StrokeSource::StrokeSource(const GraphicsContextState& state)
    : globalAlpha(state.alpha)
{
    if (state.strokePattern)
        pattern = adoptRef(state.strokePattern->createPlatformPattern(AffineTransform()));
    else if (state.strokeGradient) {
        gradient.base = state.strokeGradient->createPattern(1, state.strokeGradientSpaceTransform);
        if (state.alpha != 1)
            gradient.alphaAdjusted = state.strokeGradient->createPattern(state.alpha, state.strokeGradientSpaceTransform);
    } else
        color = state.strokeColor;
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

bool ShadowState::isRequired(GraphicsContextCairo& platformContext) const
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

void setLineCap(GraphicsContextCairo& platformContext, LineCap lineCap)
{
    cairo_line_cap_t cairoCap { };
    switch (lineCap) {
    case ButtCap:
        cairoCap = CAIRO_LINE_CAP_BUTT;
        break;
    case RoundCap:
        cairoCap = CAIRO_LINE_CAP_ROUND;
        break;
    case SquareCap:
        cairoCap = CAIRO_LINE_CAP_SQUARE;
        break;
    }
    cairo_set_line_cap(platformContext.cr(), cairoCap);
}

void setLineDash(GraphicsContextCairo& platformContext, const DashArray& dashes, float dashOffset)
{
    if (std::all_of(dashes.begin(), dashes.end(), [](auto& dash) { return !dash; }))
        cairo_set_dash(platformContext.cr(), 0, 0, 0);
    else
        cairo_set_dash(platformContext.cr(), dashes.data(), dashes.size(), dashOffset);
}

void setLineJoin(GraphicsContextCairo& platformContext, LineJoin lineJoin)
{
    cairo_line_join_t cairoJoin { };
    switch (lineJoin) {
    case MiterJoin:
        cairoJoin = CAIRO_LINE_JOIN_MITER;
        break;
    case RoundJoin:
        cairoJoin = CAIRO_LINE_JOIN_ROUND;
        break;
    case BevelJoin:
        cairoJoin = CAIRO_LINE_JOIN_BEVEL;
        break;
    }
    cairo_set_line_join(platformContext.cr(), cairoJoin);
}

void setMiterLimit(GraphicsContextCairo& platformContext, float miterLimit)
{
    cairo_set_miter_limit(platformContext.cr(), miterLimit);
}

void fillRect(GraphicsContextCairo& platformContext, const FloatRect& rect, const FillSource& fillSource, const ShadowState& shadowState)
{
    cairo_t* cr = platformContext.cr();

    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    drawPathShadow(platformContext, fillSource, { }, shadowState, Fill);
    fillCurrentCairoPath(platformContext, fillSource);
}

void fillRect(GraphicsContextCairo& platformContext, const FloatRect& rect, const Color& color, const ShadowState& shadowState)
{
    if (shadowState.isVisible()) {
        ShadowBlur shadow({ shadowState.blur, shadowState.blur }, shadowState.offset, shadowState.color, shadowState.ignoreTransforms);
        shadow.drawRectShadow(State::getCTM(platformContext), State::getClipBounds(platformContext), FloatRoundedRect(rect),
            [&platformContext, &shadowState](ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize)
            {
                fillShadowBuffer(platformContext, layerImage, layerOrigin, layerSize, shadowState);
            },
            [&platformContext, &shadowState](ImageBuffer& layerImage, const FloatRect& destRect, const FloatRect& srcRect)
            {
                drawShadowImage(platformContext, layerImage, destRect, srcRect, shadowState);
            },
            [&platformContext](const FloatRect& rect, const Color& color)
            {
                fillRectWithColor(platformContext.cr(), rect, color);
            });
    }

    fillRectWithColor(platformContext.cr(), rect, color);
}

void fillRect(GraphicsContextCairo& platformContext, const FloatRect& rect, cairo_pattern_t* platformPattern)
{
    cairo_t* cr = platformContext.cr();

    cairo_set_source(cr, platformPattern);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill(cr);
}

void fillRoundedRect(GraphicsContextCairo& platformContext, const FloatRoundedRect& rect, const Color& color, const ShadowState& shadowState)
{
    if (shadowState.isVisible()) {
        ShadowBlur shadow({ shadowState.blur, shadowState.blur }, shadowState.offset, shadowState.color, shadowState.ignoreTransforms);
        shadow.drawRectShadow(State::getCTM(platformContext), State::getClipBounds(platformContext), rect,
            [&platformContext, &shadowState](ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize)
            {
                fillShadowBuffer(platformContext, layerImage, layerOrigin, layerSize, shadowState);
            },
            [&platformContext, &shadowState](ImageBuffer& layerImage, const FloatRect& destRect, const FloatRect& srcRect)
            {
                drawShadowImage(platformContext, layerImage, destRect, srcRect, shadowState);
            },
            [&platformContext](const FloatRect& rect, const Color& color)
            {
                fillRectWithColor(platformContext.cr(), rect, color);
            });
    }

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    Path path;
    path.addRoundedRect(rect);
    appendWebCorePathToCairoContext(cr, path);
    setSourceRGBAFromColor(cr, color);
    cairo_fill(cr);

    cairo_restore(cr);
}

void fillRectWithRoundedHole(GraphicsContextCairo& platformContext, const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const FillSource& fillSource, const ShadowState& shadowState)
{
    // FIXME: this should leverage the specified color.

    if (shadowState.isVisible()) {
        ShadowBlur shadow({ shadowState.blur, shadowState.blur }, shadowState.offset, shadowState.color, shadowState.ignoreTransforms);
        shadow.drawInsetShadow(State::getCTM(platformContext), State::getClipBounds(platformContext), rect, roundedHoleRect,
            [&platformContext, &shadowState](ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize)
            {
                fillShadowBuffer(platformContext, layerImage, layerOrigin, layerSize, shadowState);
            },
            [&platformContext, &shadowState](ImageBuffer& layerImage, const FloatRect& destRect, const FloatRect& srcRect)
            {
                drawShadowImage(platformContext, layerImage, destRect, srcRect, shadowState);
            },
            [&platformContext](const FloatRect& rect, const FloatRect& holeRect, const Color& color)
            {
                // FIXME: We should use fillRectWithRoundedHole.
                cairo_t* cr = platformContext.cr();
                cairo_save(cr);
                setSourceRGBAFromColor(cr, color);
                cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
                cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
                cairo_rectangle(cr, holeRect.x(), holeRect.y(), holeRect.width(), holeRect.height());
                cairo_fill(cr);
                cairo_restore(cr);
            });
    }

    Path path;
    path.addRect(rect);
    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    cairo_t* cr = platformContext.cr();

    cairo_save(cr);
    setPathOnCairoContext(platformContext.cr(), path.cairoPath());
    fillCurrentCairoPath(platformContext, fillSource);
    cairo_restore(cr);
}

void fillPath(GraphicsContextCairo& platformContext, const Path& path, const FillSource& fillSource, const ShadowState& shadowState)
{
    cairo_t* cr = platformContext.cr();

    setPathOnCairoContext(cr, path.cairoPath());
    drawPathShadow(platformContext, fillSource, { }, shadowState, Fill);
    fillCurrentCairoPath(platformContext, fillSource);
}

void strokeRect(GraphicsContextCairo& platformContext, const FloatRect& rect, float lineWidth, const StrokeSource& strokeSource, const ShadowState& shadowState)
{
    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_line_width(cr, lineWidth);
    drawPathShadow(platformContext, { }, strokeSource, shadowState, Stroke);
    prepareForStroking(cr, strokeSource, PreserveAlpha);
    cairo_stroke(cr);

    cairo_restore(cr);
}

void strokePath(GraphicsContextCairo& platformContext, const Path& path, const StrokeSource& strokeSource, const ShadowState& shadowState)
{
    cairo_t* cr = platformContext.cr();

    setPathOnCairoContext(cr, path.cairoPath());
    drawPathShadow(platformContext, { }, strokeSource, shadowState, Stroke);
    prepareForStroking(cr, strokeSource, PreserveAlpha);
    cairo_stroke(cr);
}

void clearRect(GraphicsContextCairo& platformContext, const FloatRect& rect)
{
    cairo_t* cr = platformContext.cr();

    cairo_save(cr);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_fill(cr);
    cairo_restore(cr);
}

void drawGlyphs(GraphicsContextCairo& platformContext, const FillSource& fillSource, const StrokeSource& strokeSource, const ShadowState& shadowState, const FloatPoint& point, cairo_scaled_font_t* scaledFont, double syntheticBoldOffset, const Vector<cairo_glyph_t>& glyphs, float xOffset, TextDrawingModeFlags textDrawingMode, float strokeThickness, const FloatSize& shadowOffset, const Color& shadowColor, FontSmoothingMode fontSmoothingMode)
{
    drawGlyphsShadow(platformContext, shadowState, textDrawingMode, shadowOffset, shadowColor, point, scaledFont, syntheticBoldOffset, glyphs, fontSmoothingMode);

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    if (textDrawingMode.contains(TextDrawingMode::Fill)) {
        prepareForFilling(cr, fillSource, AdjustPatternForGlobalAlpha);
        drawGlyphsToContext(cr, scaledFont, syntheticBoldOffset, glyphs, fontSmoothingMode);
    }

    // Prevent running into a long computation within cairo. If the stroke width is
    // twice the size of the width of the text we will not ask cairo to stroke
    // the text as even one single stroke would cover the full wdth of the text.
    //  See https://bugs.webkit.org/show_bug.cgi?id=33759.
    if (textDrawingMode.contains(TextDrawingMode::Stroke) && strokeThickness < 2 * xOffset) {
        prepareForStroking(cr, strokeSource, PreserveAlpha);
        cairo_set_line_width(cr, strokeThickness);

        // This may disturb the CTM, but we are going to call cairo_restore soon after.
        cairo_set_scaled_font(cr, scaledFont);
        cairo_glyph_path(cr, glyphs.data(), glyphs.size());
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

void drawPlatformImage(GraphicsContextCairo& platformContext, cairo_surface_t* surface, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options, float globalAlpha, const ShadowState& shadowState)
{
    platformContext.save();

    // Set the compositing operation.
    if (options.compositeOperator() == CompositeOperator::SourceOver && options.blendMode() == BlendMode::Normal && !cairoSurfaceHasAlpha(surface))
        Cairo::State::setCompositeOperation(platformContext, CompositeOperator::Copy, BlendMode::Normal);
    else
        Cairo::State::setCompositeOperation(platformContext, options.compositeOperator(), options.blendMode());

    FloatRect dst = destRect;
    if (options.orientation() != ImageOrientation::None) {
        // ImageOrientation expects the origin to be at (0, 0).
        Cairo::translate(platformContext, dst.x(), dst.y());
        dst.setLocation(FloatPoint());
        Cairo::concatCTM(platformContext, options.orientation().transformFromDefault(dst.size()));
        if (options.orientation().usesWidthAsHeight()) {
            // The destination rectangle will have its width and height already reversed for the orientation of
            // the image, as it was needed for page layout, so we need to reverse it back here.
            dst = FloatRect(dst.x(), dst.y(), dst.height(), dst.width());
        }
    }

    auto orientationSizing = options.orientation().usesWidthAsHeight() ? OrientationSizing::WidthAsHeight : OrientationSizing::Normal;
    drawSurface(platformContext, surface, dst, srcRect, options.interpolationQuality(), globalAlpha, shadowState, orientationSizing);
    platformContext.restore();
}

void drawPattern(GraphicsContextCairo& platformContext, cairo_surface_t* surface, const IntSize& size, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const ImagePaintingOptions& options)
{
    // FIXME: Investigate why the size has to be passed in as an IntRect.
    drawPatternToCairoContext(platformContext.cr(), surface, size, tileRect, patternTransform, phase, toCairoOperator(options.compositeOperator(), options.blendMode()), options.interpolationQuality(), destRect);
}

void drawSurface(GraphicsContextCairo& platformContext, cairo_surface_t* surface, const FloatRect& destRect, const FloatRect& originalSrcRect, InterpolationQuality imageInterpolationQuality, float globalAlpha, const ShadowState& shadowState, OrientationSizing orientationSizing)
{
    // Avoid invalid cairo matrix with small values.
    if (std::fabs(destRect.width()) < 0.5f || std::fabs(destRect.height()) < 0.5f)
        return;

    FloatRect srcRect = originalSrcRect;

    // We need to account for negative source dimensions by flipping the rectangle.
    if (originalSrcRect.width() < 0) {
        srcRect.setX(originalSrcRect.x() + originalSrcRect.width());
        srcRect.setWidth(std::fabs(originalSrcRect.width()));
    }
    if (originalSrcRect.height() < 0) {
        srcRect.setY(originalSrcRect.y() + originalSrcRect.height());
        srcRect.setHeight(std::fabs(originalSrcRect.height()));
    }

    RefPtr<cairo_surface_t> patternSurface = surface;
    float leftPadding = 0;
    float topPadding = 0;
    auto surfaceSize = cairoSurfaceSize(surface);
    bool didUseWidthAsHeight = orientationSizing == OrientationSizing::WidthAsHeight;
    bool differentSize = srcRect.size() != (didUseWidthAsHeight ? surfaceSize.transposedSize() : surfaceSize);
    if (srcRect.x() || srcRect.y() || differentSize) {
        // Cairo subsurfaces don't support floating point boundaries well, so we expand the rectangle.
        IntRect expandedSrcRect(enclosingIntRect(srcRect));
        expandedSrcRect.intersect({ { }, cairoSurfaceSize(surface) });

        // We use a subsurface here so that we don't end up sampling outside the originalSrcRect rectangle.
        // See https://bugs.webkit.org/show_bug.cgi?id=58309
        patternSurface = adoptRef(cairo_surface_create_for_rectangle(surface, expandedSrcRect.x(),
            expandedSrcRect.y(), expandedSrcRect.width(), expandedSrcRect.height()));

        leftPadding = static_cast<float>(expandedSrcRect.x()) - floorf(srcRect.x());
        topPadding = static_cast<float>(expandedSrcRect.y()) - floorf(srcRect.y());
    }

    RefPtr<cairo_pattern_t> pattern = adoptRef(cairo_pattern_create_for_surface(patternSurface.get()));

    switch (imageInterpolationQuality) {
    case InterpolationQuality::DoNotInterpolate:
    case InterpolationQuality::Low:
        cairo_pattern_set_filter(pattern.get(), CAIRO_FILTER_FAST);
        break;
    case InterpolationQuality::Medium:
    case InterpolationQuality::Default:
        cairo_pattern_set_filter(pattern.get(), CAIRO_FILTER_GOOD);
        break;
    case InterpolationQuality::High:
        cairo_pattern_set_filter(pattern.get(), CAIRO_FILTER_BEST);
        break;
    }
    cairo_pattern_set_extend(pattern.get(), CAIRO_EXTEND_PAD);

    // The pattern transformation properly scales the pattern for when the source rectangle is a
    // different size than the destination rectangle. We also account for any offset we introduced
    // by expanding floating point source rectangle sizes. It's important to take the absolute value
    // of the scale since the original width and height might be negative.
    float scaleX = 1;
    float scaleY = 1;
    if (didUseWidthAsHeight) {
        scaleX = std::fabs(srcRect.width() / destRect.height());
        scaleY = std::fabs(srcRect.height() / destRect.width());
    } else {
        scaleX = std::fabs(srcRect.width() / destRect.width());
        scaleY = std::fabs(srcRect.height() / destRect.height());
    }
    cairo_matrix_t matrix = { scaleX, 0, 0, scaleY, leftPadding, topPadding };
    cairo_pattern_set_matrix(pattern.get(), &matrix);

    ShadowBlur shadow({ shadowState.blur, shadowState.blur }, shadowState.offset, shadowState.color, shadowState.ignoreTransforms);
    if (shadow.type() != ShadowBlur::NoShadow) {
        shadow.drawShadowLayer(State::getCTM(platformContext), State::getClipBounds(platformContext), destRect,
            [&pattern, &destRect](GraphicsContext& shadowContext)
            {
                drawPatternToCairoContext(shadowContext.platformContext()->cr(), pattern.get(), destRect, 1);
            },
            [&platformContext, &shadowState](ImageBuffer& layerImage, const FloatPoint& layerOrigin, const FloatSize& layerSize)
            {
                drawShadowLayerBuffer(platformContext, layerImage, layerOrigin, layerSize, shadowState);
            });
    }

    auto* cr = platformContext.cr();
    cairo_save(cr);
    drawPatternToCairoContext(cr, pattern.get(), destRect, globalAlpha);
    cairo_restore(cr);
}

void drawRect(GraphicsContextCairo& platformContext, const FloatRect& rect, float borderThickness, const Color& fillColor, StrokeStyle strokeStyle, const Color& strokeColor)
{
    // FIXME: how should borderThickness be used?
    UNUSED_PARAM(borderThickness);

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    fillRectWithColor(cr, rect, fillColor);

    if (strokeStyle != NoStroke) {
        setSourceRGBAFromColor(cr, strokeColor);
        FloatRect r(rect);
        r.inflate(-.5f);
        cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
        cairo_set_line_width(cr, 1.0); // borderThickness?
        cairo_stroke(cr);
    }

    cairo_restore(cr);
}

void drawLine(GraphicsContextCairo& platformContext, const FloatPoint& point1, const FloatPoint& point2, StrokeStyle strokeStyle, const Color& strokeColor, float strokeThickness, bool shouldAntialias)
{
    bool isVerticalLine = (point1.x() + strokeThickness == point2.x());
    float strokeWidth = isVerticalLine ? point2.y() - point1.y() : point2.x() - point1.x();
    if (!strokeThickness || !strokeWidth)
        return;

    cairo_t* cairoContext = platformContext.cr();
    float cornerWidth = 0;
    bool drawsDashedLine = strokeStyle == DottedStroke || strokeStyle == DashedStroke;

    if (drawsDashedLine) {
        cairo_save(cairoContext);
        // Figure out end points to ensure we always paint corners.
        cornerWidth = dashedLineCornerWidthForStrokeWidth(strokeWidth, strokeStyle, strokeThickness);
        if (isVerticalLine) {
            fillRectWithColor(cairoContext, FloatRect(point1.x(), point1.y(), strokeThickness, cornerWidth), strokeColor);
            fillRectWithColor(cairoContext, FloatRect(point1.x(), point2.y() - cornerWidth, strokeThickness, cornerWidth), strokeColor);
        } else {
            fillRectWithColor(cairoContext, FloatRect(point1.x(), point1.y(), cornerWidth, strokeThickness), strokeColor);
            fillRectWithColor(cairoContext, FloatRect(point2.x() - cornerWidth, point1.y(), cornerWidth, strokeThickness), strokeColor);
        }
        strokeWidth -= 2 * cornerWidth;
        float patternWidth = dashedLinePatternWidthForStrokeWidth(strokeWidth, strokeStyle, strokeThickness);
        // Check if corner drawing sufficiently covers the line.
        if (strokeWidth <= patternWidth + 1) {
            cairo_restore(cairoContext);
            return;
        }

        float patternOffset = dashedLinePatternOffsetForPatternAndStrokeWidth(patternWidth, strokeWidth);
        const double dashedLine[2] = { static_cast<double>(patternWidth), static_cast<double>(patternWidth) };
        cairo_set_dash(cairoContext, dashedLine, 2, patternOffset);
    } else {
        setSourceRGBAFromColor(cairoContext, strokeColor);
        if (strokeThickness < 1)
            cairo_set_line_width(cairoContext, 1);
    }

    auto centeredPoints = centerLineAndCutOffCorners(isVerticalLine, cornerWidth, point1, point2);
    auto p1 = centeredPoints[0];
    auto p2 = centeredPoints[1];

    if (shouldAntialias)
        cairo_set_antialias(cairoContext, CAIRO_ANTIALIAS_NONE);

    cairo_new_path(cairoContext);
    cairo_move_to(cairoContext, p1.x(), p1.y());
    cairo_line_to(cairoContext, p2.x(), p2.y());
    cairo_stroke(cairoContext);
    if (drawsDashedLine)
        cairo_restore(cairoContext);

    if (shouldAntialias)
        cairo_set_antialias(cairoContext, CAIRO_ANTIALIAS_DEFAULT);
}

void drawLinesForText(GraphicsContextCairo& platformContext, const FloatPoint& point, float strokeThickness, const DashArray& widths, bool printing, bool doubleUnderlines, const Color& color)
{
    Color modifiedColor = color;
    FloatRect bounds = computeLineBoundsAndAntialiasingModeForText(platformContext, point, widths.last(), printing, modifiedColor, strokeThickness);

    Vector<FloatRect, 4> dashBounds;
    ASSERT(!(widths.size() % 2));
    dashBounds.reserveInitialCapacity(dashBounds.size() / 2);
    for (size_t i = 0; i < widths.size(); i += 2)
        dashBounds.append(FloatRect(FloatPoint(bounds.x() + widths[i], bounds.y()), FloatSize(widths[i+1] - widths[i], bounds.height())));

    if (doubleUnderlines) {
        // The space between double underlines is equal to the height of the underline
        for (size_t i = 0; i < widths.size(); i += 2)
            dashBounds.append(FloatRect(FloatPoint(bounds.x() + widths[i], bounds.y() + 2 * bounds.height()), FloatSize(widths[i+1] - widths[i], bounds.height())));
    }

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    for (auto& dash : dashBounds)
        fillRectWithColor(cr, dash, modifiedColor);

    cairo_restore(cr);
}

void drawDotsForDocumentMarker(GraphicsContextCairo& platformContext, const FloatRect& rect, DocumentMarkerLineStyle style)
{
    if (style.mode != DocumentMarkerLineStyle::Mode::Spelling
        && style.mode != DocumentMarkerLineStyle::Mode::Grammar)
        return;

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    if (style.mode == DocumentMarkerLineStyle::Mode::Spelling)
        cairo_set_source_rgb(cr, 1, 0, 0);
    else if (style.mode == DocumentMarkerLineStyle::Mode::Grammar)
        cairo_set_source_rgb(cr, 0, 1, 0);

    drawErrorUnderline(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_restore(cr);
}

void drawEllipse(GraphicsContextCairo& platformContext, const FloatRect& rect, const Color& fillColor, StrokeStyle strokeStyle, const Color& strokeColor, float strokeThickness)
{
    cairo_t* cr = platformContext.cr();

    cairo_save(cr);
    float yRadius = .5 * rect.height();
    float xRadius = .5 * rect.width();
    cairo_translate(cr, rect.x() + xRadius, rect.y() + yRadius);
    cairo_scale(cr, xRadius, yRadius);
    cairo_arc(cr, 0., 0., 1., 0., 2 * piFloat);
    cairo_restore(cr);

    if (fillColor.isVisible()) {
        setSourceRGBAFromColor(cr, fillColor);
        cairo_fill_preserve(cr);
    }

    if (strokeStyle != NoStroke) {
        setSourceRGBAFromColor(cr, strokeColor);
        cairo_set_line_width(cr, strokeThickness);
        cairo_stroke(cr);
    } else
        cairo_new_path(cr);
}

void drawFocusRing(GraphicsContextCairo& platformContext, const Path& path, float width, const Color& color)
{
    // FIXME: We should draw paths that describe a rectangle with rounded corners
    // so as to be consistent with how we draw rectangular focus rings.

    // Force the alpha to 50%. This matches what the Mac does with outline rings.
    Color ringColor = color.colorWithAlpha(.5);

    cairo_t* cr = platformContext.cr();
    cairo_save(cr);

    cairo_push_group(cr);
    appendWebCorePathToCairoContext(cr, path);
    setSourceRGBAFromColor(cr, ringColor);
    cairo_set_line_width(cr, width);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_stroke_preserve(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    cairo_fill(cr);

    cairo_pop_group_to_source(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint(cr);

    cairo_restore(cr);
}

void drawFocusRing(GraphicsContextCairo& platformContext, const Vector<FloatRect>& rects, float width, const Color& color)
{
    Path path;
    unsigned rectCount = rects.size();
    int radius = (width - 1) / 2;
    Path subPath;
    for (unsigned i = 0; i < rectCount; ++i) {
        if (i > 0)
            subPath.clear();
        subPath.addRoundedRect(rects[i], FloatSize(radius, radius));
        path.addPath(subPath, AffineTransform());
    }

    drawFocusRing(platformContext, path, width, color);
}

void translate(GraphicsContextCairo& platformContext, float x, float y)
{
    cairo_translate(platformContext.cr(), x, y);
}

void rotate(GraphicsContextCairo& platformContext, float angleInRadians)
{
    cairo_rotate(platformContext.cr(), angleInRadians);
}

void scale(GraphicsContextCairo& platformContext, const FloatSize& size)
{
    cairo_scale(platformContext.cr(), size.width(), size.height());
}

void concatCTM(GraphicsContextCairo& platformContext, const AffineTransform& transform)
{
    const cairo_matrix_t matrix = toCairoMatrix(transform);
    cairo_transform(platformContext.cr(), &matrix);
}

void beginTransparencyLayer(GraphicsContextCairo& platformContext, float opacity)
{
    cairo_push_group(platformContext.cr());
    platformContext.layers().append(opacity);
}

void endTransparencyLayer(GraphicsContextCairo& platformContext)
{
    cairo_t* cr = platformContext.cr();
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, platformContext.layers().takeLast());
}

static void doClipWithAntialias(cairo_t* cr, cairo_antialias_t antialias)
{
    auto savedAntialiasRule = cairo_get_antialias(cr);
    cairo_set_antialias(cr, antialias);
    cairo_clip(cr);
    cairo_set_antialias(cr, savedAntialiasRule);
}

void clip(GraphicsContextCairo& platformContext, const FloatRect& rect)
{
    cairo_t* cr = platformContext.cr();
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
    // The rectangular clip function is traditionally not expected to
    // antialias. If we don't force antialiased clipping here,
    // edge fringe artifacts may occur at the layer edges
    // when a transformation is applied to the GraphicsContext
    // while drawing the transformed layer.
    doClipWithAntialias(cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_fill_rule(cr, savedFillRule);
}

void clipOut(GraphicsContextCairo& platformContext, const FloatRect& rect)
{
    cairo_t* cr = platformContext.cr();
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    doClipWithAntialias(cr, CAIRO_ANTIALIAS_NONE);
    cairo_set_fill_rule(cr, savedFillRule);
}

void clipOut(GraphicsContextCairo& platformContext, const Path& path)
{
    cairo_t* cr = platformContext.cr();
    double x1, y1, x2, y2;
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    appendWebCorePathToCairoContext(cr, path);

    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    // Enforce default antialias when clipping paths, since they can contain curves.
    doClipWithAntialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_fill_rule(cr, savedFillRule);
}

void clipPath(GraphicsContextCairo& platformContext, const Path& path, WindRule clipRule)
{
    cairo_t* cr = platformContext.cr();

    if (!path.isNull())
        setPathOnCairoContext(cr, path.cairoPath());

    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, clipRule == WindRule::EvenOdd ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    // Enforce default antialias when clipping paths, since they can contain curves.
    doClipWithAntialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_fill_rule(cr, savedFillRule);
}

void clipToImageBuffer(GraphicsContextCairo& platformContext, cairo_surface_t* image, const FloatRect& destRect)
{
    platformContext.pushImageMask(image, destRect);
}

} // namespace Cairo
} // namespace WebCore

#endif // USE(CAIRO)
