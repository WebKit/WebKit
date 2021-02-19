/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "DisplayListDrawGlyphsRecorder.h"

#include "BitmapImage.h"
#include "Color.h"
#include "DisplayListItems.h"
#include "DisplayListRecorder.h"
#include "FloatPoint.h"
#include "Font.h"
#include "FontCascade.h"
#include "GlyphBuffer.h"

#include <CoreText/CoreText.h>
#include <wtf/Vector.h>

#if PLATFORM(WIN)
#include <pal/spi/win/CoreTextSPIWin.h>
#endif

namespace WebCore {

namespace DisplayList {

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

GraphicsContext DrawGlyphsRecorder::createInternalContext()
{
    auto contextDelegate = adoptCF(CGContextDelegateCreate(this));
    CGContextDelegateSetCallback(contextDelegate.get(), deBeginLayer, reinterpret_cast<CGContextDelegateCallback>(&beginLayer));
    CGContextDelegateSetCallback(contextDelegate.get(), deEndLayer, reinterpret_cast<CGContextDelegateCallback>(&endLayer));
    CGContextDelegateSetCallback(contextDelegate.get(), deDrawGlyphs, reinterpret_cast<CGContextDelegateCallback>(&WebCore::DisplayList::drawGlyphs));
    CGContextDelegateSetCallback(contextDelegate.get(), deDrawImage, reinterpret_cast<CGContextDelegateCallback>(&drawImage));
    auto context = adoptCF(CGContextCreateWithDelegate(contextDelegate.get(), kCGContextTypeUnknown, nullptr, nullptr));
    return GraphicsContext(context.get());
}

DrawGlyphsRecorder::DrawGlyphsRecorder(Recorder& owner, DrawGlyphsDeconstruction drawGlyphsDeconstruction)
    : m_owner(owner)
    , m_drawGlyphsDeconstruction(drawGlyphsDeconstruction)
    , m_internalContext(createInternalContext())
{
}

void DrawGlyphsRecorder::populateInternalState(const GraphicsContextState& contextState)
{
    m_originalState.fillStyle.color = contextState.fillColor;
    m_originalState.fillStyle.gradient = contextState.fillGradient;
    m_originalState.fillStyle.gradientSpaceTransform = contextState.fillGradientSpaceTransform;
    m_originalState.fillStyle.pattern = contextState.fillPattern;

    m_originalState.strokeStyle.color = contextState.strokeColor;
    m_originalState.strokeStyle.gradient = contextState.strokeGradient;
    m_originalState.strokeStyle.gradientSpaceTransform = contextState.strokeGradientSpaceTransform;
    m_originalState.strokeStyle.pattern = contextState.strokePattern;

    m_originalState.ctm = m_owner.currentState().ctm; // FIXME: Deal with base CTM.

    m_originalState.shadow.offset = contextState.shadowOffset;
    m_originalState.shadow.blur = contextState.shadowBlur;
    m_originalState.shadow.color = contextState.shadowColor;
    m_originalState.shadow.ignoreTransforms = contextState.shadowsIgnoreTransforms;

    m_currentState = m_originalState;
}

void DrawGlyphsRecorder::populateInternalContext(const GraphicsContextState& contextState)
{
    if (m_originalState.fillStyle.color.isValid())
        m_internalContext.setFillColor(m_originalState.fillStyle.color);
    else if (m_originalState.fillStyle.gradient)
        m_internalContext.setFillGradient(*m_originalState.fillStyle.gradient, m_originalState.fillStyle.gradientSpaceTransform);
    else {
        ASSERT(m_originalState.fillStyle.pattern);
        if (m_originalState.fillStyle.pattern)
            m_internalContext.setFillPattern(*m_originalState.fillStyle.pattern);
    }

    if (m_originalState.strokeStyle.color.isValid())
        m_internalContext.setStrokeColor(m_originalState.strokeStyle.color);
    else if (m_originalState.strokeStyle.gradient)
        m_internalContext.setStrokeGradient(*m_originalState.strokeStyle.gradient, m_originalState.strokeStyle.gradientSpaceTransform);
    else {
        ASSERT(m_originalState.strokeStyle.pattern);
        if (m_originalState.strokeStyle.pattern)
            m_internalContext.setStrokePattern(*m_originalState.strokeStyle.pattern);
    }

    m_internalContext.setCTM(m_originalState.ctm);

    m_internalContext.setShadowsIgnoreTransforms(m_originalState.shadow.ignoreTransforms);
    if (contextState.shadowsUseLegacyRadius)
        m_internalContext.setLegacyShadow(m_originalState.shadow.offset, m_originalState.shadow.blur, m_originalState.shadow.color);
    else
        m_internalContext.setShadow(m_originalState.shadow.offset, m_originalState.shadow.blur, m_originalState.shadow.color);

    m_internalContext.setTextDrawingMode(contextState.textDrawingMode);
}

void DrawGlyphsRecorder::prepareInternalContext(const Font& font, FontSmoothingMode smoothingMode)
{
    ASSERT(CGAffineTransformIsIdentity(CTFontGetMatrix(font.platformData().ctFont())));

    m_originalFont = &font;
    m_smoothingMode = smoothingMode;

    m_originalTextMatrix = computeOverallTextMatrix(font);
    if (font.platformData().orientation() == FontOrientation::Vertical)
        m_originalTextMatrix = computeVerticalTextMatrix(font, m_originalTextMatrix);

    auto& contextState = m_owner.currentState().stateChange.m_state;
    populateInternalState(contextState);
    populateInternalContext(contextState);
}

void DrawGlyphsRecorder::concludeInternalContext()
{
    updateCTM(m_originalState.ctm);
    updateFillColor(m_originalState.fillStyle.color, m_originalState.fillStyle.gradient.get(), m_originalState.fillStyle.pattern.get());
    updateStrokeColor(m_originalState.strokeStyle.color, m_originalState.strokeStyle.gradient.get(), m_originalState.strokeStyle.pattern.get());
    updateShadow(m_originalState.shadow.offset, m_originalState.shadow.blur, m_originalState.shadow.color, m_originalState.shadow.ignoreTransforms ? ShadowsIgnoreTransforms::Yes : ShadowsIgnoreTransforms::No);
}

void DrawGlyphsRecorder::updateFillColor(const Color& newColor, Gradient* newGradient, Pattern* newPattern)
{
    // This check looks wrong but it actually isn't, for our limited use.
    // CT will only ever set this to a solid color, which this is the correct check for.
    // In concludeInternalContext() we set it back to what it was originally, which this check works correctly for too.
    if (newColor == m_currentState.fillStyle.color)
        return;

    GraphicsContextState newState;
    newState.fillColor = newColor;
    if (newGradient)
        newState.fillGradient = newGradient;
    if (newPattern)
        newState.fillPattern = newPattern;
    m_owner.updateState(newState, { GraphicsContextState::FillColorChange });
    m_currentState.fillStyle.color = newColor;
}

void DrawGlyphsRecorder::updateStrokeColor(const Color& newColor, Gradient* newGradient, Pattern* newPattern)
{
    // This check looks wrong but it actually isn't, for our limited use.
    // CT will only ever set this to a solid color, which this is the correct check for.
    // In concludeInternalContext() we set it back to what it was originally, which this check works correctly for too.
    if (newColor == m_currentState.strokeStyle.color)
        return;

    GraphicsContextState newState;
    newState.strokeColor = newColor;
    if (newGradient)
        newState.strokeGradient = newGradient;
    if (newPattern)
        newState.strokePattern = newPattern;
    m_owner.updateState(newState, { GraphicsContextState::StrokeColorChange });
    m_currentState.strokeStyle.color = newColor;
}

void DrawGlyphsRecorder::updateCTM(const AffineTransform& ctm)
{
    if (ctm == m_currentState.ctm)
        return;

    m_owner.setCTM(ctm);
    m_currentState.ctm = ctm;
}

static bool shadowIsCleared(const FloatSize& shadowOffset, float shadowBlur)
{
    return shadowOffset == FloatSize() && !shadowBlur;
}

void DrawGlyphsRecorder::updateShadow(const FloatSize& shadowOffset, float shadowBlur, const Color& shadowColor, ShadowsIgnoreTransforms shadowsIgnoreTransforms)
{
    // We don't need to consider shadowsIgnoreTransforms if nobody has any shadows.
    if (shadowIsCleared(shadowOffset, shadowBlur) && shadowIsCleared(m_currentState.shadow.offset, m_currentState.shadow.blur))
        return;

    GraphicsContextState newState;
    GraphicsContextState::StateChangeFlags stateChangeFlags;

    if (shadowOffset != m_currentState.shadow.offset || shadowBlur != m_currentState.shadow.blur || shadowColor != m_currentState.shadow.color) {
        newState.shadowOffset = shadowOffset;
        newState.shadowBlur = shadowBlur;
        newState.shadowColor = shadowColor;
        stateChangeFlags.add(GraphicsContextState::ShadowChange);
    }
    if (shadowsIgnoreTransforms != ShadowsIgnoreTransforms::Unspecified && (shadowsIgnoreTransforms == ShadowsIgnoreTransforms::Yes) != m_currentState.shadow.ignoreTransforms) {
        newState.shadowsIgnoreTransforms = (shadowsIgnoreTransforms == ShadowsIgnoreTransforms::Yes);
        stateChangeFlags.add(GraphicsContextState::ShadowsIgnoreTransformsChange);
    }
    if (stateChangeFlags.isEmpty())
        return;
    m_owner.updateState(newState, stateChangeFlags);

    m_currentState.shadow.offset = shadowOffset;
    m_currentState.shadow.blur = shadowBlur;
    m_currentState.shadow.color = shadowColor;
    m_currentState.shadow.ignoreTransforms = (shadowsIgnoreTransforms == ShadowsIgnoreTransforms::Yes);
}

void DrawGlyphsRecorder::updateShadow(CGStyleRef style)
{
    if (CGStyleGetType(style) != kCGStyleShadow) {
        // FIXME: Support more kinds of CGStyles.
        updateShadow({0, 0}, 0, Color(), ShadowsIgnoreTransforms::Unspecified);
        return;
    }

    const auto& shadowStyle = *static_cast<const CGShadowStyle*>(CGStyleGetData(style));
    auto rad = deg2rad(shadowStyle.azimuth - 180);
    auto shadowOffset = FloatSize(std::cos(rad), std::sin(rad)) * shadowStyle.height;
    updateShadow(shadowOffset, shadowStyle.radius, CGStyleGetColor(style), ShadowsIgnoreTransforms::Yes);
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

static Vector<CGSize> computeAdvancesFromPositions(const CGPoint positions[], size_t count, const CGAffineTransform& textMatrix)
{
    Vector<CGSize> result;
    for (size_t i = 0; i < count - 1; ++i) {
        auto nextPosition = positions[i + 1];
        auto currentPosition = positions[i];
        auto advance = CGSizeMake(nextPosition.x - currentPosition.x, nextPosition.y - currentPosition.y);
        result.append(CGSizeApplyAffineTransform(advance, textMatrix));
    }
    result.constructAndAppend(CGSizeMake(0, 0));
    return result;
}

void DrawGlyphsRecorder::recordDrawGlyphs(CGRenderingStateRef, CGGStateRef gstate, const CGAffineTransform*, const CGGlyph glyphs[], const CGPoint positions[], size_t count)
{
    if (!count)
        return;

    CGFontRef usedFont = CGGStateGetFont(gstate);
    if (usedFont != adoptCF(CTFontCopyGraphicsFont(m_originalFont->platformData().ctFont(), nullptr)).get())
        return;

#if ASSERT_ENABLED
    auto textPosition = CGContextGetTextPosition(m_internalContext.platformContext());
    ASSERT(!textPosition.x);
    ASSERT(!textPosition.y);
#endif

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
    AffineTransform currentTextMatrix = CGContextGetTextMatrix(m_internalContext.platformContext());
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

    m_owner.appendDrawGraphsItemWithCachedFont(*m_originalFont, glyphs, computeAdvancesFromPositions(positions, count, currentTextMatrix).data(), count, currentTextMatrix.mapPoint(positions[0]), m_smoothingMode);

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
    m_owner.drawNativeImage(*image, image->size(), FloatRect(rect), FloatRect {{ }, image->size()}, ImagePaintingOptions { ImageOrientation::OriginTopLeft });

    // Undo the above y-flip to restore the context.
    m_owner.scale(FloatSize(1, -1));
    m_owner.translate(0, -(rect.size.height + 2 * rect.origin.y));
}

struct GlyphsAndAdvancesStorage {
    Vector<GlyphBufferGlyph> glyphs;
    Vector<GlyphBufferAdvance> advances;
};

struct GlyphsAndAdvances {
    const GlyphBufferGlyph* glyphs;
    const GlyphBufferAdvance* advances;
    unsigned numGlyphs;
    GlyphBufferAdvance initialAdvance;
    Optional<GlyphsAndAdvancesStorage> storage;
};

static GlyphsAndAdvances filterOutOTSVGGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs)
{
    auto otsvgGlyphs = font.findOTSVGGlyphs(glyphs, numGlyphs);
    if (!otsvgGlyphs)
        return { glyphs, advances, numGlyphs, makeGlyphBufferAdvance(), { }};

    ASSERT(otsvgGlyphs->size() >= numGlyphs);

    GlyphsAndAdvances result;
    result.initialAdvance = makeGlyphBufferAdvance();
    result.storage = GlyphsAndAdvancesStorage();

    result.storage->glyphs.reserveInitialCapacity(numGlyphs);
    result.storage->advances.reserveInitialCapacity(numGlyphs);

    for (unsigned i = 0; i < numGlyphs; ++i) {
        ASSERT(result.storage->glyphs.size() == result.storage->advances.size());
        if (otsvgGlyphs->quickGet(i)) {
            if (result.storage->advances.isEmpty())
                result.initialAdvance = makeGlyphBufferAdvance(size(result.initialAdvance) + size(advances[i]));
            else
                result.storage->advances.last() = makeGlyphBufferAdvance(size(result.storage->advances.last()) + size(advances[i]));
        } else {
            result.storage->glyphs.uncheckedAppend(glyphs[i]);
            result.storage->advances.uncheckedAppend(advances[i]);
        }
        ASSERT(result.storage->glyphs.size() == result.storage->advances.size());
    }

    result.glyphs = result.storage->glyphs.data();
    result.advances = result.storage->advances.data();
    result.numGlyphs = result.storage->glyphs.size();

    return result;
}

void DrawGlyphsRecorder::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    if (m_drawGlyphsDeconstruction == DrawGlyphsDeconstruction::DontDeconstruct) {
        m_owner.appendDrawGraphsItemWithCachedFont(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
        return;
    }

    ASSERT(m_drawGlyphsDeconstruction == DrawGlyphsDeconstruction::Deconstruct);

    // FIXME: <rdar://problem/70166552> Record OTSVG glyphs.
    GlyphsAndAdvances glyphsAndAdvancesWithoutOTSVGGlyphs = filterOutOTSVGGlyphs(font, glyphs, advances, numGlyphs);
    ASSERT(glyphsAndAdvancesWithoutOTSVGGlyphs.glyphs == glyphs || glyphsAndAdvancesWithoutOTSVGGlyphs.glyphs == glyphsAndAdvancesWithoutOTSVGGlyphs.storage->glyphs.data());
    ASSERT(glyphsAndAdvancesWithoutOTSVGGlyphs.advances == advances || glyphsAndAdvancesWithoutOTSVGGlyphs.advances == glyphsAndAdvancesWithoutOTSVGGlyphs.storage->advances.data());

    prepareInternalContext(font, smoothingMode);
    FontCascade::drawGlyphs(m_internalContext, font, glyphsAndAdvancesWithoutOTSVGGlyphs.glyphs, glyphsAndAdvancesWithoutOTSVGGlyphs.advances, glyphsAndAdvancesWithoutOTSVGGlyphs.numGlyphs, startPoint + size(glyphsAndAdvancesWithoutOTSVGGlyphs.initialAdvance), smoothingMode);
    concludeInternalContext();
}

} // namespace DisplayList

} // namespace WebCore
