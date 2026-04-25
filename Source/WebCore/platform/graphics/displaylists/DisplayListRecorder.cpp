/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "DisplayListRecorder.h"

#include "DisplayList.h"
#include "DisplayListItems.h"
#include "FEImage.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include <numbers>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Recorder);

Recorder::Recorder(IsDeferred isDeferred, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, const DestinationColorSpace& colorSpace, DrawGlyphsMode drawGlyphsMode)
    : GraphicsContext(isDeferred, state)
    , m_colorSpace(colorSpace)
    , m_initialClip(initialCTM.mapRect(initialClip))
    , m_drawGlyphsMode(drawGlyphsMode)
#if USE(CORE_TEXT)
    , m_initialScale(initialCTM.xScale())
#endif
{
    ASSERT(!state.changes());
    m_stateStack.append({ state, initialCTM, initialCTM.mapRect(initialClip) });
}

Recorder::~Recorder()
{
    ASSERT(m_stateStack.size() == 1); // If this fires, it indicates mismatched save/restore.
}

void Recorder::appendDisplayList(const DisplayList& displayList)
{
    GraphicsContext::drawDisplayList(displayList, Ref { ControlFactory::singleton() });
}

const GraphicsContextState& Recorder::state() const
{
    return currentState().state;
}

void Recorder::didUpdateState(GraphicsContextState& state)
{
    currentState().state.mergeLastChanges(state, currentState().lastDrawingState);
    state.didApplyChanges();
}

void Recorder::didUpdateSingleState(GraphicsContextState& state, GraphicsContextState::ChangeIndex changeIndex)
{
    ASSERT(state.changes() - changeIndex.toChange() == GraphicsContextState::ChangeFlags { });
    currentState().state.mergeSingleChange(state, changeIndex, currentState().lastDrawingState);
    state.didApplyChanges();
}

bool Recorder::decomposeDrawGlyphsIfNeeded(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
#if USE(CORE_TEXT)
    if (m_drawGlyphsMode == DrawGlyphsMode::Deconstruct) {
        if (!m_drawGlyphsRecorder)
            m_drawGlyphsRecorder = makeUnique<DrawGlyphsRecorder>(*this, m_initialScale, DrawGlyphsRecorder::DeriveFontFromContext::No);
        m_drawGlyphsRecorder->drawGlyphs(font, glyphs, advances, localAnchor, smoothingMode);
        return true;
    }
#else
    UNUSED_PARAM(font);
    UNUSED_PARAM(glyphs);
    UNUSED_PARAM(advances);
    UNUSED_PARAM(localAnchor);
    UNUSED_PARAM(smoothingMode);
#endif
    return false;
}

void Recorder::drawConsumingImageBuffer(RefPtr<ImageBuffer> imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    // ImageBuffer draws are recorded as ImageBuffer draws, not as NativeImage draws. So for consistency,
    // record this too. This should be removed once NativeImages are the only image types drawn from.
    if (!imageBuffer)
        return;
    drawImageBuffer(*imageBuffer, destRect, srcRect, options);
}

void Recorder::updateStateForSave(GraphicsContextState::Purpose purpose)
{
    ASSERT(purpose == GraphicsContextState::Purpose::SaveRestore);
    appendStateChangeItemIfNecessary();
    GraphicsContext::save(purpose);
    m_stateStack.append(m_stateStack.last());
}

bool Recorder::updateStateForRestore(GraphicsContextState::Purpose purpose)
{
    if (m_stateStack.size() <= 1)
        return false;

    ASSERT(purpose == GraphicsContextState::Purpose::SaveRestore);
    appendStateChangeItemIfNecessary();
    GraphicsContext::restore(purpose);

    m_stateStack.removeLast();
    return true;
}

bool Recorder::updateStateForTranslate(float x, float y)
{
    if (!x && !y)
        return false;
    currentState().translate(x, y);
    return true;
}

bool Recorder::updateStateForRotate(float angleInRadians)
{
    if (WTF::areEssentiallyEqual(0.f, fmodf(angleInRadians, std::numbers::pi_v<float> * 2.f)))
        return false;
    currentState().rotate(angleInRadians);
    return true;
}

bool Recorder::updateStateForScale(const FloatSize& size)
{
    if (areEssentiallyEqual(size, FloatSize { 1.f, 1.f }))
        return false;
    currentState().scale(size);
    return true;
}

bool Recorder::updateStateForConcatCTM(const AffineTransform& transform)
{
    if (transform.isIdentity())
        return false;

    currentState().concatCTM(transform);
    return true;
}

void Recorder::updateStateForSetCTM(const AffineTransform& transform)
{
    currentState().setCTM(transform);
}

AffineTransform Recorder::getCTM(GraphicsContext::IncludeDeviceScale) const
{
    return currentState().ctm;
}

void Recorder::updateStateForBeginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);
    appendStateChangeItemIfNecessary();
    GraphicsContext::save(GraphicsContextState::Purpose::TransparencyLayer);
    m_stateStack.append(m_stateStack.last().cloneForTransparencyLayer());
}

void Recorder::updateStateForBeginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    GraphicsContext::beginTransparencyLayer(compositeOperator, blendMode);
    appendStateChangeItemIfNecessary();
    GraphicsContext::save(GraphicsContextState::Purpose::TransparencyLayer);
    m_stateStack.append(m_stateStack.last().cloneForTransparencyLayer());
}

bool Recorder::updateStateForEndTransparencyLayer()
{
    if (stateStack().size() <= 1)
        return false;
    GraphicsContext::endTransparencyLayer();
    appendStateChangeItemIfNecessary();
    m_stateStack.removeLast();
    GraphicsContext::restore(GraphicsContextState::Purpose::TransparencyLayer);
    return true;
}

void Recorder::updateStateForResetClip()
{
    currentState().clipBounds = initialClip();
}

FloatRect Recorder::initialClip() const
{
    if (auto inverse = ctm().inverse())
        return inverse->mapRect(m_initialClip);

    return m_initialClip;
}

void Recorder::updateStateForClip(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(rect));
}

void Recorder::updateStateForClipRoundedRect(const FloatRoundedRect& rect)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(rect.rect()));
}

void Recorder::updateStateForClipOut(const FloatRect&)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    // FIXME: Should we update the clip bounds?
}

void Recorder::updateStateForClipOutRoundedRect(const FloatRoundedRect&)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    // FIXME: Should we update the clip bounds?
}

void Recorder::updateStateForClipOut(const Path&)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    // FIXME: Should we update the clip bounds?
}

void Recorder::updateStateForClipPath(const Path& path)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(path.fastBoundingRect()));
}

void Recorder::updateStateForClipToImageBuffer(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(rect));
}

IntRect Recorder::clipBounds() const
{
    if (auto inverse = currentState().ctm.inverse())
        return enclosingIntRect(inverse->mapRect(currentState().clipBounds));

    // If the CTM is not invertible, return the original rect.
    // This matches CGRectApplyInverseAffineTransform behavior.
    return enclosingIntRect(currentState().clipBounds);
}

void Recorder::updateStateForApplyDeviceScaleFactor(float deviceScaleFactor)
{
    // We modify the state directly here instead of calling GraphicsContext::scale()
    // because the recorded item will scale() when replayed.
    currentState().scale({ deviceScaleFactor, deviceScaleFactor });

    // FIXME: this changes the baseCTM, which will invalidate all of our cached extents.
    // Assert that it's only called early on?
}

const AffineTransform& Recorder::ctm() const
{
    return currentState().ctm;
}

void Recorder::ContextState::translate(float x, float y)
{
    ctm.translate(x, y);
}

void Recorder::ContextState::rotate(float angleInRadians)
{
    double angleInDegrees = rad2deg(static_cast<double>(angleInRadians));
    ctm.rotate(angleInDegrees);

    AffineTransform rotation;
    rotation.rotate(angleInDegrees);
}

void Recorder::ContextState::scale(const FloatSize& size)
{
    ctm.scale(size);
}

void Recorder::ContextState::setCTM(const AffineTransform& matrix)
{
    ctm = matrix;
}

void Recorder::ContextState::concatCTM(const AffineTransform& matrix)
{
    ctm *= matrix;
}

} // namespace DisplayList
} // namespace WebCore
