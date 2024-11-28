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
#include "DisplayListDrawingContext.h"
#include "DisplayListItems.h"
#include "FEImage.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

#if USE(SYSTEM_PREVIEW)
#include "ARKitBadgeSystemImage.h"
#endif

namespace WebCore {
namespace DisplayList {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Recorder);

Recorder::Recorder(IsDeferred isDeferred, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, const DestinationColorSpace& colorSpace, DrawGlyphsMode drawGlyphsMode)
    : GraphicsContext(isDeferred, state)
    , m_initialScale(initialCTM.xScale())
    , m_colorSpace(colorSpace)
    , m_drawGlyphsMode(drawGlyphsMode)
    , m_initialClip(initialClip)
{
    ASSERT(!state.changes());
    m_stateStack.append({ state, initialCTM, initialCTM.mapRect(initialClip) });
}

Recorder::~Recorder()
{
    ASSERT(m_stateStack.size() == 1); // If this fires, it indicates mismatched save/restore.
}

void Recorder::commitRecording()
{
    // Fixup the possible pending state.
    appendStateChangeItemIfNecessary();
}

void Recorder::appendStateChangeItem(const GraphicsContextState& state)
{
    ASSERT(state.changes());
    
    if (state.containsOnlyInlineChanges()) {
        if (state.changes().contains(GraphicsContextState::Change::FillBrush))
            recordSetInlineFillColor(*fillColor().tryGetAsPackedInline());

        if (state.changes().containsAny({ GraphicsContextState::Change::StrokeBrush, GraphicsContextState::Change::StrokeThickness }))
            recordSetInlineStroke(buildSetInlineStroke(state));
        return;
    }

    if (state.changes().contains(GraphicsContextState::Change::FillBrush)) {
        if (auto pattern = fillPattern())
            recordResourceUse(pattern->tileImage());
        else if (auto gradient = fillGradient()) {
            if (gradient->hasValidRenderingResourceIdentifier())
                recordResourceUse(*gradient);
        }
    }

    if (state.changes().contains(GraphicsContextState::Change::StrokeBrush)) {
        if (auto pattern = strokePattern())
            recordResourceUse(pattern->tileImage());
        else if (auto gradient = strokeGradient()) {
            if (gradient->hasValidRenderingResourceIdentifier())
                recordResourceUse(*gradient);
        }
    }

    recordSetState(state);
}

void Recorder::appendStateChangeItemIfNecessary()
{
    // FIXME: This is currently invoked in an ad-hoc manner when recording drawing items. We should consider either
    // splitting GraphicsContext state changes into individual display list items, or refactoring the code such that
    // this method is automatically invoked when recording a drawing item.
    auto& state = currentState().state;
    if (!state.changes())
        return;

    LOG_WITH_STREAM(DisplayLists, stream << "pre-drawing, saving state " << state);
    appendStateChangeItem(state);
    state.didApplyChanges();
    currentState().lastDrawingState = state;
}

SetInlineStroke Recorder::buildSetInlineStroke(const GraphicsContextState& state)
{
    ASSERT(state.containsOnlyInlineChanges());
    ASSERT(state.changes().containsAny({ GraphicsContextState::Change::StrokeBrush, GraphicsContextState::Change::StrokeThickness }));

    if (!state.changes().contains(GraphicsContextState::Change::StrokeBrush))
        return SetInlineStroke(strokeThickness());

    ASSERT(strokeColor().tryGetAsPackedInline());
    if (!state.changes().contains(GraphicsContextState::Change::StrokeThickness))
        return SetInlineStroke(*strokeColor().tryGetAsPackedInline());

    return SetInlineStroke(*strokeColor().tryGetAsPackedInline(), strokeThickness());
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


void Recorder::drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
    appendStateChangeItemIfNecessary();

    for (auto& effect : filter.effectsOfType(FilterEffect::Type::FEImage)) {
        auto& feImage = downcast<FEImage>(effect.get());
        if (!recordResourceUse(feImage.sourceImage())) {
            GraphicsContext::drawFilteredImageBuffer(sourceImage, sourceImageRect, filter, results);
            return;
        }
    }

    if (!sourceImage) {
        recordDrawFilteredImageBuffer(nullptr, sourceImageRect, filter);
        return;
    }

    if (!recordResourceUse(*sourceImage)) {
        GraphicsContext::drawFilteredImageBuffer(sourceImage, sourceImageRect, filter, results);
        return;
    }

    recordDrawFilteredImageBuffer(sourceImage, sourceImageRect, filter);
}

bool Recorder::shouldDeconstructDrawGlyphs() const
{
    switch (m_drawGlyphsMode) {
    case DrawGlyphsMode::Normal:
        return false;
    case DrawGlyphsMode::DeconstructUsingDrawGlyphsCommands:
    case DrawGlyphsMode::DeconstructUsingDrawDecomposedGlyphsCommands:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void Recorder::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    if (shouldDeconstructDrawGlyphs()) {
        if (!m_drawGlyphsRecorder)
            m_drawGlyphsRecorder = makeUnique<DrawGlyphsRecorder>(*this, m_initialScale);
        m_drawGlyphsRecorder->drawGlyphs(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
        return;
    }

    drawGlyphsAndCacheResources(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
}

void Recorder::drawDecomposedGlyphs(const Font& font, const DecomposedGlyphs& decomposedGlyphs)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));
    recordResourceUse(const_cast<DecomposedGlyphs&>(decomposedGlyphs));
    recordDrawDecomposedGlyphs(font, decomposedGlyphs);
}

void Recorder::drawGlyphsAndCacheResources(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));

    if (m_drawGlyphsMode == DrawGlyphsMode::DeconstructUsingDrawDecomposedGlyphsCommands) {
        auto decomposedGlyphs = DecomposedGlyphs::create(glyphs, advances, numGlyphs, localAnchor, smoothingMode);
        recordResourceUse(decomposedGlyphs.get());
        recordDrawDecomposedGlyphs(font, decomposedGlyphs.get());
        return;
    }

    recordDrawGlyphs(font, glyphs, advances, numGlyphs, localAnchor, smoothingMode);
}

void Recorder::drawDisplayListItems(const Vector<Item>& items, const ResourceHeap& resourceHeap, ControlFactory&, const FloatPoint& destination)
{
    appendStateChangeItemIfNecessary();

    for (auto& resource : resourceHeap.resources().values()) {
        WTF::switchOn(resource,
            [](std::monostate) {
                RELEASE_ASSERT_NOT_REACHED();
            }, [&](const Ref<ImageBuffer>& imageBuffer) {
                recordResourceUse(imageBuffer);
            }, [&](const Ref<RenderingResource>& renderingResource) {
                if (auto* image = dynamicDowncast<NativeImage>(renderingResource.ptr()))
                    recordResourceUse(*image);
                else if (auto* filter = dynamicDowncast<Filter>(renderingResource.ptr()))
                    recordResourceUse(*filter);
                else if (auto* decomposedGlyphs = dynamicDowncast<DecomposedGlyphs>(renderingResource.ptr()))
                    recordResourceUse(*decomposedGlyphs);
                else if (auto* gradient = dynamicDowncast<Gradient>(renderingResource.ptr()))
                    recordResourceUse(*gradient);
                else
                    RELEASE_ASSERT_NOT_REACHED();
            }, [&](const Ref<Font>& font) {
                recordResourceUse(font);
            }, [&](const Ref<FontCustomPlatformData>&) {
                RELEASE_ASSERT_NOT_REACHED();
            }
        );
    }

    recordDrawDisplayListItems(items, destination);
}

void Recorder::drawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();

    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawImageBuffer(imageBuffer, destRect, srcRect, options);
        return;
    }

    recordDrawImageBuffer(imageBuffer, destRect, srcRect, options);
}
void Recorder::drawConsumingImageBuffer(RefPtr<ImageBuffer> imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    // ImageBuffer draws are recorded as ImageBuffer draws, not as NativeImage draws. So for consistency,
    // record this too. This should be removed once NativeImages are the only image types drawn from.
    if (!imageBuffer)
        return;
    drawImageBuffer(*imageBuffer, destRect, srcRect, options);
}

void Recorder::drawNativeImageInternal(NativeImage& image, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(image);
    recordDrawNativeImage(image.renderingResourceIdentifier(), destRect, srcRect, options);
}

void Recorder::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    appendStateChangeItemIfNecessary();
#if USE(SYSTEM_PREVIEW)
    if (auto* badgeSystemImage = dynamicDowncast<ARKitBadgeSystemImage>(systemImage)) {
        if (auto image = badgeSystemImage->image()) {
            auto nativeImage = image->nativeImage();
            if (!nativeImage)
                return;
            recordResourceUse(*nativeImage);
        }
    }
#endif
    recordDrawSystemImage(systemImage, destinationRect);
}

void Recorder::drawPattern(NativeImage& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(image);
    recordDrawPattern(image.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options);
}

void Recorder::drawPattern(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();

    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawPattern(imageBuffer, destRect, tileRect, patternTransform, phase, spacing, options);
        return;
    }

    recordDrawPattern(imageBuffer.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options);
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
    ASSERT(purpose == GraphicsContextState::Purpose::SaveRestore);
    appendStateChangeItemIfNecessary();
    GraphicsContext::restore(purpose);

    if (!m_stateStack.size())
        return false;

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
    if (WTF::areEssentiallyEqual(0.f, fmodf(angleInRadians, piFloat * 2.f)))
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

void Recorder::updateStateForEndTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();
    appendStateChangeItemIfNecessary();
    m_stateStack.removeLast();
    GraphicsContext::restore(GraphicsContextState::Purpose::TransparencyLayer);
}

void Recorder::fillPath(const Path& path)
{
    appendStateChangeItemIfNecessary();

    if (auto segment = path.singleSegment()) {
#if ENABLE(INLINE_PATH_DATA)
        if (auto line = path.singleDataLine())
            recordFillLine(*line);
        else if (auto arc = path.singleArc())
            recordFillArc(*arc);
        else if (auto closedArc = path.singleClosedArc())
            recordFillClosedArc(*closedArc);
        else if (auto curve = path.singleQuadCurve())
            recordFillQuadCurve(*curve);
        else if (auto curve = path.singleBezierCurve())
            recordFillBezierCurve(*curve);
        else
#endif
            recordFillPathSegment(*segment);
        return;
    }

    recordFillPath(path);
}


void Recorder::strokePath(const Path& path)
{
#if ENABLE(INLINE_PATH_DATA)
    auto& state = currentState().state;
    if (state.containsOnlyInlineStrokeChanges()) {
        if (auto line = path.singleDataLine()) {
            recordStrokeLineWithColorAndThickness(*line, buildSetInlineStroke(state));
            state.didApplyChanges();
            currentState().lastDrawingState = state;
            return;
        }
    }
#endif

    appendStateChangeItemIfNecessary();

    if (auto segment = path.singleSegment()) {
#if ENABLE(INLINE_PATH_DATA)
        if (auto line = path.singleDataLine())
            recordStrokeLine(*line);
        else if (auto arc = path.singleArc())
            recordStrokeArc(*arc);
        else if (auto closedArc = path.singleClosedArc())
            recordStrokeClosedArc(*closedArc);
        else if (auto curve = path.singleQuadCurve())
            recordStrokeQuadCurve(*curve);
        else if (auto curve = path.singleBezierCurve())
            recordStrokeBezierCurve(*curve);
        else
#endif
            recordStrokePathSegment(*segment);
        return;
    }

    recordStrokePath(path);
}

void Recorder::updateStateForResetClip()
{
    currentState().clipBounds = m_initialClip;
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

IntRect Recorder::clipBounds() const
{
    if (auto inverse = currentState().ctm.inverse())
        return enclosingIntRect(inverse->mapRect(currentState().clipBounds));

    // If the CTM is not invertible, return the original rect.
    // This matches CGRectApplyInverseAffineTransform behavior.
    return enclosingIntRect(currentState().clipBounds);
}

void Recorder::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect)
{
    appendStateChangeItemIfNecessary(); // Conservative: we do not know if the clip application might use state such as antialiasing.
    currentState().clipBounds.intersect(currentState().ctm.mapRect(destRect));
    recordResourceUse(imageBuffer);
    recordClipToImageBuffer(imageBuffer, destRect);
}

void Recorder::updateStateForApplyDeviceScaleFactor(float deviceScaleFactor)
{
    // We modify the state directly here instead of calling GraphicsContext::scale()
    // because the recorded item will scale() when replayed.
    currentState().scale({ deviceScaleFactor, deviceScaleFactor });

    // FIXME: this changes the baseCTM, which will invalidate all of our cached extents.
    // Assert that it's only called early on?
}

const Recorder::ContextState& Recorder::currentState() const
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

Recorder::ContextState& Recorder::currentState()
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
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
