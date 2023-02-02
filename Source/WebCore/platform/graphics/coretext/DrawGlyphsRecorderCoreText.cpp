/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "DrawGlyphsRecorder.h"

#include "BitmapImage.h"
#include "Color.h"
#include "FloatPoint.h"
#include "Font.h"
#include "FontCascade.h"
#include "FontPlatformData.h"
#include "GlyphBuffer.h"
#include "GraphicsContextCG.h"
#include "ImageBuffer.h"

#include <CoreText/CoreText.h>
#include <wtf/Vector.h>

#if PLATFORM(WIN)
#include <pal/spi/win/CoreTextSPIWin.h>
#endif

namespace WebCore {

static CGContextDelegateRef beginLayer(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate, CGRect rect, CFDictionaryRef, CGContextDelegateRef)
{
    DrawGlyphsRecorder& recorder = *static_cast<DrawGlyphsRecorder*>(CGContextDelegateGetInfo(delegate));
    recorder.recordBeginLayer(rstate, gstate, rect);
    return delegate;
}

static CGContextDelegateRef endLayer(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate)
{
    DrawGlyphsRecorder& recorder = *static_cast<DrawGlyphsRecorder*>(CGContextDelegateGetInfo(delegate));
    recorder.recordEndLayer(rstate, gstate);
    return delegate;
}

static CGError drawGlyphs(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate, const CGAffineTransform* tm, const CGGlyph glyphs[], const CGPoint positions[], size_t count)
{
    if (CGGStateGetAlpha(gstate) > 0) {
        DrawGlyphsRecorder& recorder = *static_cast<DrawGlyphsRecorder*>(CGContextDelegateGetInfo(delegate));
        recorder.recordDrawGlyphs(rstate, gstate, tm, glyphs, positions, count);
    }
    return kCGErrorSuccess;
}

static CGError drawImage(CGContextDelegateRef delegate, CGRenderingStateRef rstate, CGGStateRef gstate, CGRect rect, CGImageRef image)
{
    DrawGlyphsRecorder& recorder = *static_cast<DrawGlyphsRecorder*>(CGContextDelegateGetInfo(delegate));
    recorder.recordDrawImage(rstate, gstate, rect, image);
    return kCGErrorSuccess;
}

UniqueRef<GraphicsContext> DrawGlyphsRecorder::createInternalContext()
{
    auto contextDelegate = adoptCF(CGContextDelegateCreate(this));
    CGContextDelegateSetCallback(contextDelegate.get(), deBeginLayer, reinterpret_cast<CGContextDelegateCallback>(&beginLayer));
    CGContextDelegateSetCallback(contextDelegate.get(), deEndLayer, reinterpret_cast<CGContextDelegateCallback>(&endLayer));
    CGContextDelegateSetCallback(contextDelegate.get(), deDrawGlyphs, reinterpret_cast<CGContextDelegateCallback>(&WebCore::drawGlyphs));
    CGContextDelegateSetCallback(contextDelegate.get(), deDrawImage, reinterpret_cast<CGContextDelegateCallback>(&drawImage));
#if HAVE(CORE_TEXT_FIX_FOR_RADAR_93925620)
    auto contextType = kCGContextTypeUnknown;
#else
    auto contextType = kCGContextTypeWindow;
#endif
    auto context = adoptCF(CGContextCreateWithDelegate(contextDelegate.get(), contextType, nullptr, nullptr));
    return makeUniqueRef<GraphicsContextCG>(context.get());
}

DrawGlyphsRecorder::DrawGlyphsRecorder(GraphicsContext& owner, float scaleFactor, DeriveFontFromContext deriveFontFromContext)
    : m_owner(owner)
    , m_internalContext(createInternalContext())
    , m_deriveFontFromContext(deriveFontFromContext)
{
    m_internalContext->applyDeviceScaleFactor(scaleFactor);
}

void DrawGlyphsRecorder::populateInternalState(const GraphicsContextState& contextState)
{
    m_originalState.fillBrush = contextState.fillBrush();
    m_originalState.strokeBrush = contextState.strokeBrush();

    m_originalState.ctm = m_owner.getCTM();

    m_originalState.dropShadow = contextState.dropShadow();
    m_originalState.ignoreTransforms = contextState.shadowsIgnoreTransforms();
}

void DrawGlyphsRecorder::populateInternalContext(const GraphicsContextState& contextState)
{
    m_internalContext->setCTM(m_originalState.ctm);

    m_internalContext->setFillBrush(m_originalState.fillBrush);
    m_internalContext->applyFillPattern();

    m_internalContext->setStrokeBrush(m_originalState.strokeBrush);
    m_internalContext->applyStrokePattern();

    m_internalContext->setShadowsIgnoreTransforms(m_originalState.ignoreTransforms);
    m_internalContext->setDropShadow(m_originalState.dropShadow);

    m_internalContext->setTextDrawingMode(contextState.textDrawingMode());
}

void DrawGlyphsRecorder::recordInitialColors()
{
    CGContextRef cgContext = m_internalContext->platformContext();
    m_initialFillColor = CGContextGetFillColorAsColor(cgContext);
    m_initialStrokeColor = CGContextGetStrokeColorAsColor(cgContext);
}

void DrawGlyphsRecorder::prepareInternalContext(const Font& font, FontSmoothingMode smoothingMode)
{
    ASSERT(CGAffineTransformIsIdentity(CTFontGetMatrix(font.platformData().ctFont())));

    m_originalFont = &font;
    m_smoothingMode = smoothingMode;

    m_originalTextMatrix = computeOverallTextMatrix(font);
    if (font.platformData().orientation() == FontOrientation::Vertical)
        m_originalTextMatrix = computeVerticalTextMatrix(font, m_originalTextMatrix);

    auto& contextState = m_owner.state();
    populateInternalState(contextState);
    populateInternalContext(contextState);
    recordInitialColors();
}

void DrawGlyphsRecorder::concludeInternalContext()
{
    updateCTM(m_originalState.ctm);
    updateFillBrush(m_originalState.fillBrush);
    updateStrokeBrush(m_originalState.strokeBrush);
    updateShadow(m_originalState.dropShadow, m_originalState.ignoreTransforms ? ShadowsIgnoreTransforms::Yes : ShadowsIgnoreTransforms::No);
}

void DrawGlyphsRecorder::updateFillColor(CGColorRef fillColor)
{
    if (CGColorGetPattern(fillColor)) {
        ASSERT(m_originalState.fillBrush.pattern());
        return;
    }
    if (fillColor == m_initialFillColor)
        m_owner.setFillBrush(m_originalState.fillBrush);
    else
        m_owner.setFillBrush(Color::createAndPreserveColorSpace(fillColor));
}

void DrawGlyphsRecorder::updateFillBrush(const SourceBrush& newBrush)
{
    m_owner.setFillBrush(newBrush);
}

void DrawGlyphsRecorder::updateStrokeColor(CGColorRef strokeColor)
{
    if (CGColorGetPattern(strokeColor)) {
        ASSERT(m_originalState.strokeBrush.pattern());
        return;
    }
    if (strokeColor == m_initialStrokeColor)
        m_owner.setStrokeBrush(m_originalState.strokeBrush);
    else
        m_owner.setStrokeBrush(Color::createAndPreserveColorSpace(strokeColor));
}

void DrawGlyphsRecorder::updateStrokeBrush(const SourceBrush& newBrush)
{
    m_owner.setStrokeBrush(newBrush);
}

void DrawGlyphsRecorder::updateCTM(const AffineTransform& ctm)
{
    if (m_owner.getCTM() == ctm)
        return;
    // Instead of recording a SetCTM command, we compute the transform needed
    // to change the current CTM to `ctm`. This allows the recorded comamnds
    // to be re-used by elements drawing the same text in different locations.
    if (auto inverseOfCurrentCTM = m_owner.getCTM().inverse())
        m_owner.concatCTM(*inverseOfCurrentCTM * ctm);
}

void DrawGlyphsRecorder::updateShadow(const DropShadow& dropShadow, ShadowsIgnoreTransforms shadowsIgnoreTransforms)
{
    m_owner.setDropShadow(dropShadow);
    m_owner.setShadowsIgnoreTransforms(shadowsIgnoreTransforms == ShadowsIgnoreTransforms::Yes);
}

void DrawGlyphsRecorder::updateShadow(CGStyleRef style)
{
    if (CGStyleGetType(style) != kCGStyleShadow) {
        // FIXME: Support more kinds of CGStyles.
        updateShadow({ }, ShadowsIgnoreTransforms::Unspecified);
        return;
    }

    const auto& shadowStyle = *static_cast<const CGShadowStyle*>(CGStyleGetData(style));
    auto rad = deg2rad(shadowStyle.azimuth - 180);
    auto shadowOffset = FloatSize(std::cos(rad), std::sin(rad)) * shadowStyle.height;
    auto shadowColor = CGStyleGetColor(style);
    updateShadow({ shadowOffset, static_cast<float>(shadowStyle.radius), Color::createAndPreserveColorSpace(shadowColor) }, ShadowsIgnoreTransforms::Yes);
}

void DrawGlyphsRecorder::recordBeginLayer(CGRenderingStateRef, CGGStateRef gstate, CGRect)
{
    updateCTM(*CGGStateGetCTM(gstate));
    auto alpha = CGGStateGetAlpha(gstate);
    m_owner.beginTransparencyLayer(alpha);
}

void DrawGlyphsRecorder::recordEndLayer(CGRenderingStateRef, CGGStateRef gstate)
{
    updateCTM(*CGGStateGetCTM(gstate));
    m_owner.endTransparencyLayer();
}

struct AdvancesAndInitialPosition {
    Vector<CGSize> advances;
    CGPoint initialPosition;
};

static AdvancesAndInitialPosition computeHorizontalAdvancesFromPositions(const CGPoint positions[], size_t count, const CGAffineTransform& textMatrix)
{
    // This function needs to be the inverse of fillVectorWithHorizontalGlyphPositions().

    ASSERT(count); // Because we say "positions[0]" below.

    AdvancesAndInitialPosition result;
    result.advances.reserveInitialCapacity(count);
    result.initialPosition = CGPointApplyAffineTransform(positions[0], textMatrix);
    for (size_t i = 0; i < count - 1; ++i) {
        auto nextPosition = positions[i + 1];
        auto currentPosition = positions[i];
        auto advance = CGSizeMake(nextPosition.x - currentPosition.x, nextPosition.y - currentPosition.y);
        result.advances.uncheckedAppend(CGSizeApplyAffineTransform(advance, textMatrix));
    }
    result.advances.uncheckedConstructAndAppend(CGSizeZero);
    return result;
}

static AdvancesAndInitialPosition computeVerticalAdvancesFromPositions(const CGSize translations[], const CGPoint positions[], unsigned count, float ascentDelta, AffineTransform textMatrix)
{
    // This function needs to be the inverse of fillVectorWithVerticalGlyphPositions().

    ASSERT(count); // Because we say "positions[0]" below.

    auto constantSyntheticTextMatrixOmittingOblique = computeBaseVerticalTextMatrix(computeBaseOverallTextMatrix(std::nullopt)); // See fillVectorWithVerticalGlyphPositions(), which describes what this is.

    auto transformPoint = [&](CGPoint position, CGSize translation) -> CGPoint {
        auto positionInUserCoordinates = CGPointApplyAffineTransform(position, textMatrix);
        auto translationInUserCoordinates = CGSizeApplyAffineTransform(translation, constantSyntheticTextMatrixOmittingOblique);
        return CGPointMake(positionInUserCoordinates.x - translationInUserCoordinates.width, positionInUserCoordinates.y - translationInUserCoordinates.height);
    };

    AdvancesAndInitialPosition result;
    result.advances.reserveInitialCapacity(count);
    result.initialPosition = transformPoint(positions[0], translations[0]);
    CGPoint previousPosition = result.initialPosition;
    result.initialPosition.y -= ascentDelta;

    for (size_t i = 1; i < count; ++i) {
        auto currentPosition = transformPoint(positions[i], translations[i]);
        result.advances.uncheckedConstructAndAppend(CGSizeMake(currentPosition.x - previousPosition.x, currentPosition.y - previousPosition.y));
        previousPosition = currentPosition;
    }
    result.advances.uncheckedAppend(CGSizeZero);
    return result;
}

void DrawGlyphsRecorder::recordDrawGlyphs(CGRenderingStateRef, CGGStateRef gstate, const CGAffineTransform*, const CGGlyph glyphs[], const CGPoint positions[], size_t count)
{
    ASSERT_IMPLIES(m_deriveFontFromContext == DeriveFontFromContext::No, m_originalFont);

    if (!count)
        return;

    CGFontRef usedFont = CGGStateGetFont(gstate);
    if (m_deriveFontFromContext == DeriveFontFromContext::No && usedFont != adoptCF(CTFontCopyGraphicsFont(m_originalFont->platformData().ctFont(), nullptr)).get())
        return;

    updateCTM(*CGGStateGetCTM(gstate));

    // We want the replayer's CTM and text matrix to match the current CTM and text matrix.
    // The current text matrix is a concatenation of whatever WebKit sets it to and whatever
    // Core Text appends to it. So, we have
    // CTM * m_originalTextMatrix * Core Text's text matrix.
    // However, CGContextGetTextMatrix() just tells us what the whole text matrix is, so
    // m_originalTextMatrix * Core Text's text matrix = currentTextMatrix.
    // The only way we can emulate Core Text's text matrix is by modifying the CTM here.
    // So, if we do that, the GPU process will have
    // CTM * X * m_originalTextMatrix
    // If you set these two equal to each other, and solve for X, you get
    // CTM * currentTextMatrix = CTM * X * m_originalTextMatrix
    // currentTextMatrix * inverse(m_originalTextMatrix) = X
    AffineTransform currentTextMatrix = CGContextGetTextMatrix(m_internalContext->platformContext());
    AffineTransform ctmFixup;
    if (auto invertedOriginalTextMatrix = m_originalTextMatrix.inverse())
        ctmFixup = currentTextMatrix * invertedOriginalTextMatrix.value();
    AffineTransform inverseCTMFixup;
    if (auto inverse = ctmFixup.inverse())
        inverseCTMFixup = inverse.value();
    else
        ctmFixup = AffineTransform();
    m_owner.concatCTM(ctmFixup);

    updateFillColor(CGGStateGetFillColor(gstate));
    updateStrokeColor(CGGStateGetStrokeColor(gstate));
    updateShadow(CGGStateGetStyle(gstate));

    auto fontSize = CGGStateGetFontSize(gstate);
    Ref font = m_deriveFontFromContext == DeriveFontFromContext::No ? *m_originalFont : Font::create(FontPlatformData(adoptCF(CTFontCreateWithGraphicsFont(usedFont, fontSize, nullptr, nullptr)), fontSize));

    // The above does the work of ensuring the right CTM (which is the combination of CG's CTM and
    // CG's text matrix) is set for the replayer, but in order to provide the right values to
    // `FontCascade::drawGlyphs` we need to recalculate the original advances from the resulting
    // positions by inverting the operations applied to the original advances.
    auto textMatrix = m_originalTextMatrix;

    AdvancesAndInitialPosition advances;
    if (font->platformData().orientation() == FontOrientation::Vertical) {
        Vector<CGSize, 256> translations(count);
        CTFontGetVerticalTranslationsForGlyphs(font->platformData().ctFont(), glyphs, translations.data(), count);
        auto ascentDelta = font->fontMetrics().floatAscent(IdeographicBaseline) - font->fontMetrics().floatAscent();
        advances = computeVerticalAdvancesFromPositions(translations.data(), positions, count, ascentDelta, textMatrix);
    } else
        advances = computeHorizontalAdvancesFromPositions(positions, count, textMatrix);

    m_owner.drawGlyphsAndCacheResources(font, glyphs, advances.advances.data(), count, advances.initialPosition, m_smoothingMode);

    m_owner.concatCTM(inverseCTMFixup);
}

void DrawGlyphsRecorder::recordDrawImage(CGRenderingStateRef, CGGStateRef gstate, CGRect rect, CGImageRef cgImage)
{
    updateCTM(*CGGStateGetCTM(gstate));
    updateShadow(CGGStateGetStyle(gstate));

    // Core Graphics assumes a "y up" coordinate system, but in WebKit, we use a "y-down" coordinate system.
    // This means that WebKit's drawing routines (GraphicsContext::drawImage()) intentionally draw images upside-down from Core Graphics's point of view.
    // (There's a y-flip inside the implementation of GraphicsContext::drawImage().)
    // The rect has the right bounds, but we need to transform from CG's coordinate system to WebKit's by performing our own y-flip so images are drawn the right-side-up.
    // We do this at the boundary between the two APIs, which is right here.
    m_owner.translate(0, rect.size.height + 2 * rect.origin.y);
    m_owner.scale(FloatSize(1, -1));

    auto image = NativeImage::create(cgImage);
    m_owner.drawNativeImage(*image, image->size(), FloatRect(rect), FloatRect { { }, image->size() }, ImagePaintingOptions { ImageOrientation::Orientation::OriginTopLeft });

    // Undo the above y-flip to restore the context.
    m_owner.scale(FloatSize(1, -1));
    m_owner.translate(0, -(rect.size.height + 2 * rect.origin.y));
}

void DrawGlyphsRecorder::drawOTSVGRun(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    FloatPoint penPosition = startPoint;

    for (unsigned i = 0; i < numGlyphs; ++i) {
        auto bounds = font.boundsForGlyph(glyphs[i]);

        // Create a local ImageBuffer because decoding the SVG fonts has to happen in WebProcess.
        if (auto imageBuffer = m_owner.createAlignedImageBuffer(bounds, DestinationColorSpace::SRGB(), RenderingMethod::Local)) {
            FontCascade::drawGlyphs(imageBuffer->context(), font, glyphs + i, advances + i, 1, FloatPoint(), smoothingMode);

            FloatRect destinationRect = enclosingIntRect(bounds);
            destinationRect.moveBy(penPosition);
            m_owner.drawImageBuffer(*imageBuffer, destinationRect);
        }

        penPosition.move(size(advances[i]));
    }
}

void DrawGlyphsRecorder::drawNonOTSVGRun(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    prepareInternalContext(font, smoothingMode);
    FontCascade::drawGlyphs(m_internalContext, font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
    concludeInternalContext();
}

void DrawGlyphsRecorder::drawBySplittingIntoOTSVGAndNonOTSVGRuns(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    auto otsvgGlyphs = font.findOTSVGGlyphs(glyphs, numGlyphs);
    if (!otsvgGlyphs) {
        drawNonOTSVGRun(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
        return;
    }

    ASSERT(otsvgGlyphs->size() >= numGlyphs);

    // We can't just partition the glyphs into OT-SVG glyphs and non-OT-SVG glyphs because glyphs are allowed to draw outside of their layout boxes.
    // This means that glyphs can overlap, which means we have to get the z-order correct. We can't have an earlier run be drawn on top of a later run.
    FloatPoint runOrigin = startPoint;
    FloatPoint penPosition = startPoint;
    size_t glyphCountInRun = 0;
    bool isOTSVGRun = false;
    unsigned i;
    auto draw = [&] () {
        if (!glyphCountInRun)
            return;
        if (isOTSVGRun)
            drawOTSVGRun(font, glyphs + i - glyphCountInRun, advances + i - glyphCountInRun, glyphCountInRun, runOrigin, smoothingMode);
        else
            drawNonOTSVGRun(font, glyphs + i - glyphCountInRun, advances + i - glyphCountInRun, glyphCountInRun, runOrigin, smoothingMode);
    };
    for (i = 0; i < numGlyphs; ++i) {
        bool isOTSVGGlyph = otsvgGlyphs->quickGet(i);
        if (isOTSVGGlyph != isOTSVGRun) {
            draw();
            isOTSVGRun = isOTSVGGlyph;
            glyphCountInRun = 0;
            runOrigin = penPosition;
        }
        ++glyphCountInRun;
        penPosition.move(size(advances[i]));
    }
    draw();
}

void DrawGlyphsRecorder::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    drawBySplittingIntoOTSVGAndNonOTSVGRuns(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
}

void DrawGlyphsRecorder::drawNativeText(CTFontRef font, CGFloat fontSize, CTLineRef line, CGRect lineRect)
{
    GraphicsContextStateSaver ownerSaver(m_owner);
    GraphicsContextStateSaver internalContextSaver(m_internalContext.get());

    m_owner.translate(lineRect.origin.x, lineRect.origin.y + lineRect.size.height);
    m_owner.scale(FloatSize(1, -1));

    prepareInternalContext(Font::create(FontPlatformData(font, fontSize)), FontSmoothingMode::SubpixelAntialiased);
    CGContextSetTextPosition(m_internalContext->platformContext(), 0, 0);
    CTLineDraw(line, m_internalContext->platformContext());
    concludeInternalContext();
}

} // namespace WebCore
