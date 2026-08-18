/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "SkiaCompositingLayer.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BitmapTexture.h"
#include "CoordinatedAnimatedBackingStoreClient.h"
#include "CoordinatedImageBackingStore.h"
#include "CoordinatedPlatformLayerBuffer.h"
#include "CoordinatedPlatformLayerBufferHolePunch.h"
#include "CoordinatedTileBuffer.h"
#include "FilterOperations.h"
#include "FontCache.h"
#include "PlatformDisplay.h"
#include "Region.h"
#include "SkiaBackingStore.h"
#include "SkiaCompositingLayerFilters.h"
#include "SkiaCompositingLayerOverlapRegions.h"
#include "SkiaDamageRegion.h"
#include "SkiaUtilities.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkFont.h>
#include <skia/core/SkPathBuilder.h>
#include <skia/core/SkRRect.h>
#include <skia/effects/SkImageFilters.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/utils/SkNoDrawCanvas.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaCompositingLayer);

static constexpr float s_opacityVisibilityThreshold = 0.01;

Ref<SkiaCompositingLayer> SkiaCompositingLayer::create()
{
    return adoptRef(*new SkiaCompositingLayer());
}

SkiaCompositingLayer::~SkiaCompositingLayer() = default;

void SkiaCompositingLayer::invalidate()
{
    m_backingStore = nullptr;
    m_animatedBackingStoreClient = nullptr;
    m_maskImage = nullptr;
    m_imageBackingStore = nullptr;
    m_contentsBuffer = nullptr;

    m_mask = nullptr;
    m_replica = nullptr;
    m_replicatedLayer = nullptr;

    for (auto& child : m_children)
        child->m_parent = nullptr;
    removeFromParent();
}

// The setters below damage once the new state is in place, since damageWholeLayer() reads the layer's
// size to decide what to damage, and what it has to damage is what the layer is about to paint.
void SkiaCompositingLayer::setSize(const FloatSize& size)
{
    if (m_rect.size() == size)
        return;

    m_rect.setSize(size);
    damageWholeLayer();
}

void SkiaCompositingLayer::setOpacity(float opacity)
{
    if (m_opacity == opacity)
        return;

    m_opacity = opacity;
    damageWholeLayer();
}

void SkiaCompositingLayer::setBlendMode(BlendMode blendMode)
{
    if (blendMode == BlendMode::Normal) {
        m_blendMode = std::nullopt;
        return;
    }

    m_blendMode = SkiaUtilities::toSkiaBlendMode(blendMode);
}

void SkiaCompositingLayer::setChildren(Vector<Ref<SkiaCompositingLayer>>&& newChildren)
{
    if (m_children == newChildren)
        return;

    while (!m_children.isEmpty()) {
        auto child = m_children.takeLast();
        child->m_parent = nullptr;
    }

    m_children = WTF::move(newChildren);
    for (auto& child : m_children) {
        child->removeFromParent();
        child->m_parent = this;
    }
}

void SkiaCompositingLayer::removeFromParent()
{
    RefPtr parent = std::exchange(m_parent, nullptr);
    if (!parent)
        return;

    parent->m_children.removeFirstMatching([this](auto& layer) {
        return layer.ptr() == this;
    });
}

void SkiaCompositingLayer::setUseBackingStore(bool useBackingStore, CoordinatedAnimatedBackingStoreClient* animatedBackingStoreClient)
{
    if (!useBackingStore) {
        m_backingStore = nullptr;
        m_animatedBackingStoreClient = nullptr;
        m_maskImage = nullptr;
        return;
    }

    if (!m_backingStore)
        m_backingStore = makeUnique<SkiaBackingStore>();
    m_animatedBackingStoreClient = animatedBackingStoreClient;
}

void SkiaCompositingLayer::updateBackingStore(CoordinatedBackingStoreProxy::Update&& update, float scale)
{
    if (m_maskImage && !update.isEmpty())
        m_maskImage = nullptr;

    ASSERT(m_backingStore);
    m_backingStore->update(m_rect.size(), scale, WTF::move(update));
}

bool SkiaCompositingLayer::hasPendingBackingStoreTileUpdates() const
{
    return m_backingStore ? m_backingStore->hasPendingTileUpdates() : false;
}

void SkiaCompositingLayer::processPendingTileUpdates()
{
    if (m_backingStore)
        m_backingStore->processPendingTileUpdates();
}

void SkiaCompositingLayer::setImageBackingStore(CoordinatedImageBackingStore* imageBackingStore)
{
    m_imageBackingStore = imageBackingStore;
}

void SkiaCompositingLayer::setContentsBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& contentsBuffer)
{
    m_contentsBuffer = WTF::move(contentsBuffer);
}

std::unique_ptr<CoordinatedPlatformLayerBuffer> SkiaCompositingLayer::takeContentsBuffer()
{
    return WTF::move(m_contentsBuffer);
}

void SkiaCompositingLayer::setContentsSolidColor(const Color& color)
{
    if (m_contentsSolidColor == color)
        return;

    m_contentsSolidColor = color;
    damageWholeLayer();
}

void SkiaCompositingLayer::setMask(RefPtr<SkiaCompositingLayer>&& mask)
{
    if (m_mask == mask)
        return;

    m_mask = WTF::move(mask);

    // Damaging the layer is not enough, since the mask applies to the whole subtree, and a mask that is
    // gone has no damage of its own for collectMaskDamage() to find. Both are handled there.
#if ENABLE(DAMAGE_TRACKING)
    m_maskChanged = true;
#endif
}

void SkiaCompositingLayer::setReplica(RefPtr<SkiaCompositingLayer>&& replica)
{
    m_replica = WTF::move(replica);
    if (m_replica)
        m_replica->m_replicatedLayer = this;
}

void SkiaCompositingLayer::setFilters(const FilterOperations& filterOperations)
{
    if (filterOperations.isEmpty())
        m_filter = std::nullopt;
    else
        m_filter = { SkiaCompositingLayerFilters::create(filterOperations), filterOperations.outsets() };
}

void SkiaCompositingLayer::setBackdropFilters(const FilterOperations& filterOperations)
{
    m_backdrop.filter = SkiaCompositingLayerFilters::create(filterOperations, SkTileMode::kClamp);
}

void SkiaCompositingLayer::setBackdropFiltersRect(const FloatRoundedRect& clipRect)
{
    m_backdrop.clipRect = clipRect;
}

FloatRect SkiaCompositingLayer::paintedLayerRect() const
{
    auto rect = m_rect;
    if (paintsContentsRect())
        rect.unite(m_contentsRect);
    if (m_backdrop.filter)
        rect.unite(m_backdrop.clipRect.rect());

    // A debug border is stroked on the edges of what the layer paints, respect that.
    if (m_debugBorder)
        rect.inflate(m_debugBorder->width / 2);

    // A filter such as a blur spreads what the layer paints past its bounds, unless something clips it
    // back in, which mirrors what computeOverlapRegions() does with the same outsets.
    auto filter = this->filter();
    if (filter && !filter->outsets.isZero() && !m_masksToBounds && !m_mask && !m_backdrop.filter) {
        rect.move(-filter->outsets.left(), -filter->outsets.top());
        rect.expand(filter->outsets.left() + filter->outsets.right(), filter->outsets.top() + filter->outsets.bottom());
    }

    return rect;
}

Ref<SkiaCompositingLayer> SkiaCompositingLayer::backdropRoot()
{
    if (m_isBackdropRoot)
        return *this;

    if (m_parent)
        return m_parent->backdropRoot();

    if (m_replicatedLayer)
        return m_replicatedLayer->backdropRoot();

    return *this;
}

#if ENABLE(DAMAGE_TRACKING)
void SkiaCompositingLayer::addDamage(Damage&& damage)
{
    // The damage is added not to override the damage that could be inferred from other set* operations.
    if (m_layerDamage)
        m_layerDamage->add(damage);
    else
        m_layerDamage = WTF::move(damage);
}

void SkiaCompositingLayer::LayerRectTracker::recordVisit(uint64_t id, const FloatRect& rectInFrame)
{
    auto& entry = m_entries.add(id, Entry { }).iterator->value;

    // A replicated layer is walked once per replica, so unite the visits rather than let the last one win,
    // or the place the replica moved away from would never be repainted.
    if (!wasVisitedThisFrame(entry)) {
        entry.lastVisitedFrameID = m_currentFrameID;
        entry.currentRect = rectInFrame;
    } else
        entry.currentRect.unite(rectInFrame);
}

void SkiaCompositingLayer::LayerRectTracker::damageStaleLayerRectsAndAdvanceEntries(Damage& frameDamage)
{
    // Runs once the walk has visited every layer, so each entry's currentRect is complete.
    m_entries.removeIf([&](auto& keyAndEntry) {
        auto& entry = keyAndEntry.value;

        // Never visited, so the layer is hidden, out of the tree or already destroyed. Repaint where it
        // was and forget it, and if it ever comes back it gets a fresh entry under the same id.
        if (!wasVisitedThisFrame(entry)) {
            frameDamage.add(entry.previousRect);
            return true;
        }

        // The layer moved, so repaint both where it was and where it now is.
        if (entry.previousRect != entry.currentRect) {
            frameDamage.add(std::exchange(entry.previousRect, entry.currentRect));
            frameDamage.add(entry.currentRect);
        }

        return false;
    });
}

void SkiaCompositingLayer::trackLayerRect(PaintContext& context, const FloatRect& layerRectInFrame)
{
    auto& layerRectTracker = context.collectState->layerRectTracker;

    // Identifiers are never reused, so an entry can outlive its layer without a later layer inheriting it.
    if (!m_layerRectID)
        m_layerRectID = layerRectTracker.generateLayerRectID();

    layerRectTracker.recordVisit(m_layerRectID, layerRectInFrame);
}

void SkiaCompositingLayer::resolveBackdropDamage(const Vector<FloatRect>& backdropRectsInFrame, Damage& frameDamage)
{
    // A backdrop is filtered from what lies under it, so anything changing in it changes the whole
    // backdrop. Walked in paint order, so a damaged backdrop also changes the backdrops above it.
    for (const auto& backdropRect : backdropRectsInFrame) {
        const auto backdropRectToTest = enclosingIntRect(backdropRect);

        // Tested against the damage rects rather than their bounds, or a change anywhere on screen would
        // repaint every backdrop on the page.
        bool damagedUnderneath = false;
        for (const auto& rect : frameDamage) {
            if (rect.intersects(backdropRectToTest)) {
                damagedUnderneath = true;
                break;
            }
        }

        if (damagedUnderneath)
            frameDamage.add(backdropRect);
    }
}
#endif

void SkiaCompositingLayer::setDebugIndicators(Color&& debugBorderColor, std::optional<float> debugBorderWidth, std::optional<unsigned> repaintCount)
{
    std::optional<DebugBorder> debugBorder;
    if (debugBorderColor.isValid())
        debugBorder = { WTF::move(debugBorderColor), debugBorderWidth.value_or(1) };

    if (m_debugBorder == debugBorder && m_repaintCount == repaintCount)
        return;

    m_debugBorder = WTF::move(debugBorder);
    m_repaintCount = repaintCount;
    damageWholeLayer();
}

const TransformationMatrix& SkiaCompositingLayer::localTransform() const
{
    if (!m_animationsState || !m_animationsState->isRunning)
        return m_transform;

    return m_animationsState->transform ? m_animationsState->transform.value() : m_transform;
}

const TransformationMatrix& SkiaCompositingLayer::futureLocalTransform() const
{
    if (!m_animationsState || !m_animationsState->isRunning)
        return m_transform;

    return m_animationsState->futureTransform ? m_animationsState->futureTransform.value() : localTransform();
}

float SkiaCompositingLayer::opacity() const
{
    if (!m_animationsState || !m_animationsState->isRunning)
        return m_opacity;

    return m_animationsState->opacity.value_or(m_opacity);
}

const std::optional<SkiaCompositingLayer::Filter> SkiaCompositingLayer::filter() const
{
    if (!m_animationsState || !m_animationsState->isRunning)
        return m_filter;

    return m_animationsState->filter ? m_animationsState->filter : m_filter;
}

std::optional<SkiaCompositingLayer::AnimationsState> SkiaCompositingLayer::syncAnimations(MonotonicTime time)
{
    if (m_animations.isEmpty())
        return std::nullopt;

    TextureMapperAnimation::ApplicationResult applicationResults;
    m_animations.apply(applicationResults, time);

    AnimationsState state;
    state.transform = applicationResults.transform;
    if (state.transform) {
        // Calculate localTransform 50ms in the future.
        TextureMapperAnimation::ApplicationResult futureResults;
        m_animations.apply(futureResults, time + 50_ms, TextureMapperAnimation::KeepInternalState::Yes);
        state.futureTransform = futureResults.transform;
    }
    state.opacity = applicationResults.opacity;
    if (opacity() != state.opacity.value_or(m_opacity)) {
        damageWholeLayer();
        // FIXME: add collectFrameDamageDespiteBeingInvisible?
    }
    if (applicationResults.filters)
        state.filter = { SkiaCompositingLayerFilters::create(*applicationResults.filters), applicationResults.filters->outsets() };
    state.isRunning = applicationResults.hasRunningAnimations;
    return state;
}

bool SkiaCompositingLayer::computeTransformsAndAnimations(const TransformationMatrix& parentTransform, const TransformationMatrix& futureParentTransform, MonotonicTime time)
{
    m_animationsState = syncAnimations(time);
    bool hasRunningAnimations = m_animationsState ? m_animationsState->isRunning : false;

    TransformationMatrix combinedForChildren;
    TransformationMatrix futureCombinedForChildren;

    if (!m_rect.isEmpty() || !m_masksToBounds) {
#if ENABLE(DAMAGE_TRACKING)
        TransformationMatrix previousTransform = m_transforms.combined;
#endif

        FloatPoint origin(m_anchorPoint.x(), m_anchorPoint.y());
        origin.scale(m_rect.size().width(), m_rect.size().height());
        m_transforms.combined = parentTransform;
        m_transforms.combined
            .translate3d(origin.x() + (m_position.x() - m_boundsOrigin.x()), origin.y() + (m_position.y() - m_boundsOrigin.y()), m_anchorPoint.z())
            .multiply(localTransform());

        combinedForChildren = m_transforms.combined;
        m_transforms.combined.translate3d(-origin.x(), -origin.y(), -m_anchorPoint.z());

        if (isReplica())
            m_transforms.combined.translate(-m_position.x(), -m_position.y());

        if (!m_preserves3D)
            combinedForChildren.flatten();
        combinedForChildren.multiply(m_childrenTransform);
        combinedForChildren.translate3d(-origin.x(), -origin.y(), -m_anchorPoint.z());

        m_transforms.futureCombined = futureParentTransform;
        m_transforms.futureCombined
            .translate3d(origin.x() + (m_position.x() - m_boundsOrigin.x()), origin.y() + (m_position.y() - m_boundsOrigin.y()), m_anchorPoint.z())
            .multiply(futureLocalTransform());

        futureCombinedForChildren = m_transforms.futureCombined;
        m_transforms.futureCombined.translate3d(-origin.x(), -origin.y(), -m_anchorPoint.z());

        if (isReplica())
            m_transforms.futureCombined.translate(-m_position.x(), -m_position.y());

        if (!m_preserves3D)
            futureCombinedForChildren.flatten();
        futureCombinedForChildren.multiply(m_childrenTransform);
        futureCombinedForChildren.translate3d(-origin.x(), -origin.y(), -m_anchorPoint.z());

#if ENABLE(DAMAGE_TRACKING)
        if (previousTransform != m_transforms.combined)
            damageWholeLayer();
#endif

        m_visible = m_backfaceVisibility || !m_transforms.combined.isBackFaceVisible();

        if (m_animatedBackingStoreClient)
            m_animatedBackingStoreClient->requestBackingStoreUpdateIfNeeded(m_transforms.futureCombined);
    }

    if (m_mask) {
        auto& maskParent = m_replicatedLayer ? *m_replicatedLayer : *this;
        hasRunningAnimations |= m_mask->computeTransformsAndAnimations(maskParent.m_transforms.combined, maskParent.m_transforms.futureCombined, time);
    }
    if (m_replica)
        hasRunningAnimations |= m_replica->computeTransformsAndAnimations(m_transforms.combined, m_transforms.futureCombined, time);

    m_shouldBlend = !!m_blendMode;
    for (auto& child : m_children) {
        hasRunningAnimations |= child->computeTransformsAndAnimations(combinedForChildren, futureCombinedForChildren, time);
        m_shouldBlend |= !!child->m_blendMode;
    }

    // If the layer is invisible because of opacity and there's no opacity animation, the content won't
    // be visible ever, so triggering repaints doesn't make sense.
    if (!m_opacity && !(m_animationsState && m_animationsState->opacity))
        return false;

    return hasRunningAnimations;
}

bool SkiaCompositingLayer::paint(SkCanvas& canvas, std::optional<Damage>& frameDamage, const std::optional<Damage>& priorTargetDamage, std::optional<SkColor> clearColor)
{
    // Both walks below assume the animations have been applied and the transforms computed.
    bool hasRunningAnimations = computeTransformsAndAnimations({ }, { }, MonotonicTime::now());

#if ENABLE(DAMAGE_TRACKING)
    // Collect the damage in a walk of its own first, so it is complete before the walk that draws. Each
    // layer's damage stays until recursiveCleanUpAfterPaint() clears it at the end of the frame.
    if (frameDamage) {
        // Only the layer paint() is called on needs the table, so only the root ever allocates one.
        if (!m_layerRectTracker)
            m_layerRectTracker = makeUnique<LayerRectTracker>();
        m_layerRectTracker->advanceToNextFrame();

        PaintContext collectContext;
        collectContext.collectState.emplace(*frameDamage, *m_layerRectTracker);

        // Sized like the canvas that will be drawn into, so both walks clip against the same device
        // bounds. The layer's own size is in layer coordinates, which the root transform scales to the
        // device by the device pixel ratio.
        const auto canvasSize = canvas.getBaseLayerSize();
        SkNoDrawCanvas noDrawCanvas(canvasSize.width(), canvasSize.height());
        recursivePaint(noDrawCanvas, collectContext);

        m_layerRectTracker->damageStaleLayerRectsAndAdvanceEntries(*frameDamage);
        resolveBackdropDamage(collectContext.collectState->backdropRectsInFrame, *frameDamage);
    }
#else
    UNUSED_PARAM(frameDamage);
#endif

    // The region this target must redraw is the damage still on its record from before combined with
    // this frame's. No prior damage means the target cannot be trusted, so repaint the whole of it.
    std::optional<SkiaDamageRegion> damageRegion;
#if ENABLE(DAMAGE_TRACKING)
    if (priorTargetDamage) {
        // The damage is in device space, so the surface size the region is tested against must be too.
        // m_rect is in layer coordinates, which the root transform scales to the device by the device
        // pixel ratio - use the canvas device size instead, as the collect walk above already does.
        const auto deviceSize = canvas.getBaseLayerSize();
        const IntSize surfaceSize(deviceSize.width(), deviceSize.height());
        if (frameDamage) {
            auto repaintRegion = *priorTargetDamage;
            repaintRegion.add(*frameDamage);
            damageRegion = SkiaDamageRegion::create(repaintRegion, surfaceSize);
        } else
            damageRegion = SkiaDamageRegion::create(*priorTargetDamage, surfaceSize);
    }
#else
    UNUSED_PARAM(priorTargetDamage);
#endif

    // An empty region means the target already holds the frame, so draw nothing and skip the clear.
    const bool skipDraw = damageRegion && damageRegion->isEmpty();
    if (!skipDraw) {
        // Clear what will be redrawn - only the damage rects when compositing from the damage, which keeps
        // undamaged content and composites translucent content once, or the whole target otherwise.
        if (clearColor) {
            if (damageRegion) {
                SkPaint clearPaint;
                clearPaint.setColor(*clearColor);
                clearPaint.setBlendMode(SkBlendMode::kSrc);
                damageRegion->fillCanvasInDeviceSpace(canvas, clearPaint);
            } else
                canvas.clear(*clearColor);
        }

        PaintContext context;
        context.compositingDamageRegion = WTF::move(damageRegion);

        recursivePaint(canvas, context);
        context.imageSetBatch.flushIfNeeded(canvas);
    }

    recursiveCleanUpAfterPaint();

    return hasRunningAnimations;
}

void SkiaCompositingLayer::clipRect(SkCanvas& canvas, const FloatRoundedRect& rect, const TransformationMatrix& transform)
{
    if (transform.isIdentity()) {
        if (rect.hasNonZeroRadii())
            canvas.clipRRect(SkRRect(rect), true);
        else
            canvas.clipRect(SkRect(rect.rect()));
        return;
    }

    auto matrix = SkM44(transform).asM33();
    if (rect.hasNonZeroRadii())
        canvas.clipPath(SkPath::RRect(SkRRect(rect)).makeTransform(matrix), true);
    else if (matrix.rectStaysRect())
        canvas.clipRect(matrix.mapRect(SkRect(rect.rect())));
    else
        canvas.clipPath(SkPath::Rect(SkRect(rect.rect())).makeTransform(matrix));
}

void SkiaCompositingLayer::paintSelf(SkCanvas& canvas, PaintContext& context)
{
#if ENABLE(DAMAGE_TRACKING)
    // Collected before the content check, because a backdrop filter paints without content of its own.
    if (!context.shouldDraw()) {
        collectFrameDamage(canvas, context);
        return;
    }
#endif

    if (!hasVisiblePaintableContent())
        return;

    paintContents(canvas, context);

    // Drawn in the content stream, so content above hides the border and it shares
    // the group's opacity, filter and mask - similar to the Viz design.
    if (m_debugBorder)
        paintDebugBorder(canvas, context);
}

TransformationMatrix SkiaCompositingLayer::combinedTransform(const PaintContext& context) const
{
    TransformationMatrix transform(context.accumulatedReplicaTransform);
    transform.multiply(m_transforms.combined);
    return transform;
}

void SkiaCompositingLayer::paintContents(SkCanvas& canvas, PaintContext& context)
{
    // Important: the walk does not clip the canvas to the damage, so every content draw below limits
    // itself, with drawRectRestricted() or drawImageRectRestricted() when it draws now, or by passing the
    // region to the batch when it draws later. A draw that does not paints over undamaged pixels and
    // composites translucent content twice, so any content type added here has to limit itself too. A set
    // region is never empty, because paint() turns an empty one into a no-op.
    ASSERT(!context.compositingDamageRegion || !context.compositingDamageRegion->isEmpty());

    const SkM44 transform(combinedTransform(context));

    const auto ctm = transform.asM33();
    bool enableAntialias = !ctm.preservesAxisAlignment() && !ctm.preservesRightAngles();

    auto shouldBlend = [&]() -> bool {
        if (m_contentsOpaque)
            return false;

        if (m_backingStore)
            return true;

        if (m_contentsBuffer)
            return m_contentsBuffer->flags().contains(TextureMapperFlags::ShouldBlend);

        if (m_imageBackingStore) {
            if (const auto* buffer = m_imageBackingStore->buffer())
                return buffer->flags().contains(TextureMapperFlags::ShouldBlend);
        }

        return true;
    };

    // When the layer contents are fully opaque and composited at full opacity with the default
    // (source-over) blend mode, drawing the batched tiles/images with source-over needlessly keeps
    // GL_BLEND enabled for every draw. Compositing with source (kSrc) instead lets Skia's blend
    // formula collapse to (kOne, kZero) so GL_BLEND is disabled -- mirroring TextureMapper's per-draw
    // ShouldBlend decision and saving substantial bandwidth on tiled GPUs (e.g. Vivante/etnaviv).
    // FIXME: a color filter forces source-over conservatively -- it may turn opaque contents
    // translucent, and kSrc would then write those pixels without blending. We could inspect the
    // filter and still use kSrc when it provably keeps the contents opaque.
    bool forcedSrcBlendMode = false;
    auto batchBlendMode = context.blendMode;
    if (!batchBlendMode && !context.colorFilter && !shouldBlend() && context.opacity == 1) {
        batchBlendMode = SkBlendMode::kSrc;
        forcedSrcBlendMode = true;
    }
    context.imageSetBatch.updatePaintProperties(canvas, context.colorFilter, batchBlendMode);

    auto setupPaint = [&] -> SkPaint {
        SkPaint paint;
        paint.setStyle(SkPaint::kFill_Style);
        paint.setAntiAlias(enableAntialias);
        paint.setAlphaf(context.opacity);
        if (context.blendMode)
            paint.setBlendMode(*context.blendMode);
        if (context.colorFilter)
            paint.setColorFilter(context.colorFilter);
        return paint;
    };

    if (m_backingStore)
        context.imageSetBatch.addImageSet(canvas, *m_backingStore, transform, context.opacity, enableAntialias, context.damageRegionOrNull(), setupPaint());
    else if (m_backgroundColor.isValid() && m_backgroundColor.isVisible()) {
        ScopedFlush autoFlush(canvas, context.imageSetBatch, ScopedFlush::Mode::FlushBefore);
        canvas.concat(transform);
        SkPaint paint = setupPaint();
        paint.setColor(SkColor(m_backgroundColor.colorWithAlphaMultipliedBy(context.opacity)));
        drawRectRestricted(canvas, context.damageRegionOrNull(), SkRect(m_rect), paint);
    }

    if (m_contentsSolidColor.isValid() && m_contentsSolidColor.isVisible()) {
        ScopedFlush autoFlush(canvas, context.imageSetBatch, ScopedFlush::Mode::FlushBefore);
        canvas.concat(transform);
        SkPaint paint = setupPaint();
        paint.setColor(SkColor(m_contentsSolidColor.colorWithAlphaMultipliedBy(context.opacity)));
        drawRectRestricted(canvas, context.damageRegionOrNull(), SkRect(m_contentsRect), paint);
    } else if (m_contentsBuffer || m_imageBackingStore) {
        bool shouldPaintNow = [&] {
            if (m_contentsClipPath)
                return true;

            if (m_contentsClippingRect.hasNonZeroRadii())
                return true;

            if (!m_contentsBuffer && !m_contentsTiling.size.isEmpty())
                return true;

#if ENABLE(VIDEO)
            if (is<CoordinatedPlatformLayerBufferHolePunch>(m_contentsBuffer))
                return true;
#endif

            // FIXME: clip is not correctly applied with batched painting.
            if (!m_contentsClippingRect.rect().contains(m_contentsRect))
                return true;

            return false;
        }();

        ScopedFlush autoFlush(canvas, context.imageSetBatch, shouldPaintNow ? ScopedFlush::Mode::FlushBefore : ScopedFlush::Mode::DoNothing);
        if (shouldPaintNow) {
            canvas.concat(transform);

            // A corner shape the clipping rect cannot express arrives as a path instead; the rect has had
            // its radii dropped in that case, so the path is the whole clip.
            if (m_contentsClipPath)
                canvas.clipPath(*m_contentsClipPath, true);
            else if (m_contentsClippingRect.hasNonZeroRadii() || !m_contentsClippingRect.rect().contains(m_contentsRect))
                clipRect(canvas, m_contentsClippingRect);
        }

        sk_sp<SkImage> image;

        if (m_contentsBuffer) {
#if ENABLE(VIDEO)
            if (is<CoordinatedPlatformLayerBufferHolePunch>(*m_contentsBuffer)) {
#if USE(GSTREAMER)
                TransformationMatrix matrix = canvas.getLocalToDevice();
                downcast<CoordinatedPlatformLayerBufferHolePunch>(*m_contentsBuffer).setHolePunchVideoRectangle(enclosingIntRect(matrix.mapRect(m_contentsRect)));
#endif
                SkPaint paint = setupPaint();
                paint.setBlendMode(SkBlendMode::kClear);
                drawRectRestricted(canvas, context.damageRegionOrNull(), SkRect(m_contentsRect), paint);
            } else
#endif // ENABLE(VIDEO)
                image = m_contentsBuffer->skiaImage();
        } else if (auto* buffer = m_imageBackingStore->buffer()) {
            image = buffer->skiaImage();
            if (!m_contentsTiling.size.isEmpty()) {
                sk_sp<SkImage> tileImage = std::exchange(image, nullptr);
                SkMatrix matrix;
                matrix.setScale(m_contentsTiling.size.width() / tileImage->width(), m_contentsTiling.size.height() / tileImage->height());
                matrix.postTranslate(m_contentsRect.x() - m_contentsTiling.phase.width(), m_contentsRect.y() - m_contentsTiling.phase.height());
                SkPaint paint = setupPaint();
                paint.setShader(tileImage->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat, SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone), matrix));
                drawRectRestricted(canvas, context.damageRegionOrNull(), SkRect(m_contentsRect), paint);
            }
        }

        if (image) {
            if (shouldPaintNow) {
                SkPaint paint = setupPaint();
                drawImageRectRestricted(canvas, context.damageRegionOrNull(), image.get(), SkRect::MakeSize(SkSize::Make(image->dimensions())), SkRect(m_contentsRect),
                    SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone), &paint);
            } else {
                // The contents image composites over the backing store, so it must use SrcOver always.
                if (forcedSrcBlendMode && m_backingStore)
                    context.imageSetBatch.updatePaintProperties(canvas, context.colorFilter, context.blendMode);

                context.imageSetBatch.addImage(canvas, image, m_contentsRect, transform, context.opacity, enableAntialias, context.damageRegionOrNull(), setupPaint());
            }
        }
    }
}

#if ENABLE(DAMAGE_TRACKING)
void SkiaCompositingLayer::collectFrameDamage(SkCanvas& canvas, PaintContext& context)
{
    if (!damagePropagationEnabled() || !contributesToFrame())
        return;

    const auto transform = combinedTransform(context);
    auto layerRectInFrame = transform.mapRect(paintedLayerRect());
    auto clipBounds = FloatRect(this->clipBounds(canvas, context));
    layerRectInFrame.intersect(clipBounds);

    trackLayerRect(context, layerRectInFrame);

    if (!m_layerDamage)
        return;

    // A Damage is sized to the layer, so it cannot express a contents rect that overhangs it. Whole-layer
    // damage, which is what a layer-state change produces, therefore damages what the layer paints.
    if (m_layerDamage->mode() == Damage::Mode::Full) {
        context.collectState->frameDamage.add(layerRectInFrame);
        return;
    }

    for (const auto& rect : *m_layerDamage) {
        auto damageRect = transform.mapRect(FloatRect(rect));
        damageRect.intersect(clipBounds);
        context.collectState->frameDamage.add(damageRect);
    }
}

void SkiaCompositingLayer::collectBackdropDamage(SkCanvas& canvas, PaintContext& context)
{
    if (!damagePropagationEnabled())
        return;

    // The filter samples the whole backdrop, so anything changing underneath changes every pixel of it. Only
    // the rect is collected here, because what changed underneath is only known once the walk has finished.
    // The backdrop root's subtree is walked by the walk itself, so it is not walked again.
    auto backdropRectInFrame = combinedTransform(context).mapRect(m_backdrop.clipRect.rect());
    backdropRectInFrame.intersect(FloatRect(this->clipBounds(canvas, context)));

    context.collectState->backdropRectsInFrame.append(backdropRectInFrame);
}

void SkiaCompositingLayer::collectMaskDamage(SkCanvas& canvas, PaintContext& context)
{
    if (!damagePropagationEnabled())
        return;

    // A mask that was taken away has no damage left to find, so the removal is remembered by setMask().
    if (!m_maskChanged && (!m_mask || !m_mask->hasLayerDamage()))
        return;

    // The walk never reaches a mask, so its damage is collected here, and a mask that only moves is
    // damaged by computing the transforms, which does reach it. A changed mask changes what the masked
    // layer composites to, so that is damaged rather than the mask itself, and the whole subtree with it,
    // since the mask applies to descendants that need not paint within the layer's bounds. The overlap
    // rects are already in frame coordinates and clipped, but take the layer's bounds, which the contents
    // rect may overhang.
    auto maskedRectInFrame = combinedTransform(context).mapRect(paintedLayerRect());
    for (const auto& rect : computeConsolidatedOverlapRegionRects(canvas, context, ComputeOverlapRegionMode::Union))
        maskedRectInFrame.unite(FloatRect(rect));
    maskedRectInFrame.intersect(FloatRect(this->clipBounds(canvas, context)));

    context.collectState->frameDamage.add(maskedRectInFrame);
}

#endif

void SkiaCompositingLayer::paintDebugBorder(SkCanvas& canvas, PaintContext& context)
{
    ASSERT(m_debugBorder);

    ScopedFlush autoFlush(canvas, context.imageSetBatch, ScopedFlush::Mode::FlushBefore);
    canvas.concat(SkM44(combinedTransform(context)));

    SkPaint borderPaint;
    borderPaint.setStyle(SkPaint::kStroke_Style);
    borderPaint.setColor(SkColor(m_debugBorder->color));
    borderPaint.setStrokeWidth(m_debugBorder->width);
    borderPaint.setAntiAlias(true);

    if (m_backingStore)
        m_backingStore->drawDebugBorders(canvas, borderPaint);
    if (paintsContentsRect())
        canvas.drawRect(SkRect(m_contentsRect), borderPaint);
}

void SkiaCompositingLayer::paintRepaintCounter(SkCanvas& canvas, PaintContext& context)
{
    ASSERT(m_repaintCount);

    if (!hasVisiblePaintableContent())
        return;

    // The counter is drawn in device space, so compose the layer transform here rather than concatenating
    // it onto the canvas only to reset the matrix again below.
    SkPoint deviceOrigin { 0, 0 };
    const auto mapped = (canvas.getLocalToDevice() * SkM44(combinedTransform(context))).map(0, 0, 0, 1);
    if (std::abs(mapped.w) > std::numeric_limits<float>::epsilon())
        deviceOrigin = { mapped.x / mapped.w, mapped.y / mapped.w };
    else
        deviceOrigin = { mapped.x, mapped.y };

    constexpr float pointSize = 14;
    constexpr float padding = 3;

    static SkFont font = [] {
        auto typeface = FontCache::forCurrentThread().fontManager().matchFamilyStyle("monospace", SkFontStyle::Bold());
        SkFont f(typeface, pointSize);
        f.setEdging(SkFont::Edging::kAntiAlias);
        f.setSubpixel(true);
        return f;
    }();

    if (m_repaintCountOverlay.count != m_repaintCount) {
        m_repaintCountOverlay.count = m_repaintCount;
        m_repaintCountOverlay.string = String::number(*m_repaintCount).ascii();
        SkRect textBounds;
        font.measureText(m_repaintCountOverlay.string.data(), m_repaintCountOverlay.string.length(), SkTextEncoding::kUTF8, &textBounds);
        m_repaintCountOverlay.backgroundWidth = textBounds.width() + padding * 2;
        m_repaintCountOverlay.backgroundHeight = textBounds.height() + padding * 2;
        m_repaintCountOverlay.baselineOffset = -textBounds.fTop + padding;
    }

    ScopedFlush autoFlush(canvas, context.imageSetBatch, ScopedFlush::Mode::FlushBefore);
    canvas.resetMatrix();

    SkPaint backgroundPaint;
    backgroundPaint.setColor(m_debugBorder ? SkColor(m_debugBorder->color) : SK_ColorBLACK);
    backgroundPaint.setStyle(SkPaint::kFill_Style);
    canvas.drawRect(SkRect::MakeXYWH(deviceOrigin.x(), deviceOrigin.y(), m_repaintCountOverlay.backgroundWidth, m_repaintCountOverlay.backgroundHeight), backgroundPaint);

    SkPaint textPaint;
    textPaint.setColor(SK_ColorWHITE);
    textPaint.setAntiAlias(true);
    canvas.drawString(m_repaintCountOverlay.string.data(), deviceOrigin.x() + padding, deviceOrigin.y() + m_repaintCountOverlay.baselineOffset, font, textPaint);
}

void SkiaCompositingLayer::paintSelfAndChildren(SkCanvas& canvas, PaintContext& context)
{
    if (m_backdrop.filter && context.paintingBackdropForLayer == this) {
        context.skipAfterBackdrop = true;
        return;
    }

    paintSelf(canvas, context);

    if (m_children.isEmpty())
        return;

    auto canSkipClip = [&](const FloatRoundedRect& rect, const TransformationMatrix& transform) {
        if (rect.hasNonZeroRadii())
            return false;

        // We can only skip clipping for layers having one child that is a leaf.
        if (m_children.size() != 1 || !m_children[0]->m_children.isEmpty())
            return false;

        // We don't need to clip if the child is not visible.
        if (!m_children[0]->isVisible())
            return true;

        // If the child has a replica, the local bounds don't include the replicated content.
        if (m_children[0]->m_replica)
            return false;

        // Do not skip the clip if the child has a backdrop filter.
        if (m_children[0]->m_backdrop.filter)
            return false;

        auto matrix = canvas.getLocalToDeviceAs3x3() * SkM44(transform).asM33();
        if (!matrix.rectStaysRect())
            return false;

        // We don't need to clip if the clipped area is bigger or equal than the child bounds.
        auto childMatrix = canvas.getLocalToDeviceAs3x3() * SkM44(m_children[0]->m_transforms.combined).asM33();
        FloatRect childBounds;
        if (m_children[0]->m_backingStore)
            childBounds = m_children[0]->m_rect;
        if (m_children[0]->m_contentsBuffer || m_children[0]->m_imageBackingStore || (m_children[0]->m_contentsSolidColor.isValid() && m_children[0]->m_contentsSolidColor.isVisible()))
            childBounds.unite(m_children[0]->m_contentsRect);
        return matrix.mapRect(SkRect(rect.rect())).contains(childMatrix.mapRect(SkRect(childBounds)));
    };

    const bool contentsRectClipsDescendants = !m_preserves3D && m_contentsRectClipsDescendants && (m_contentsClipPath || m_contentsClippingRect.hasNonZeroRadii() || !m_contentsClippingRect.rect().contains(m_contentsRect));
    const bool masksToBounds = !m_preserves3D && m_masksToBounds;
    TransformationMatrix clipTransform;
    FloatRoundedRect clippingRect;
    if (masksToBounds || contentsRectClipsDescendants) {
        clipTransform = context.accumulatedReplicaTransform;
        clipTransform.multiply(m_transforms.combined);
        if (!contentsRectClipsDescendants)
            clipTransform.translate(m_boundsOrigin.x(), m_boundsOrigin.y());

        FloatRoundedRect rect = contentsRectClipsDescendants ? m_contentsClippingRect : FloatRoundedRect(m_rect);
        if (!canSkipClip(rect, clipTransform))
            clippingRect = rect;
    }

    ScopedFlush autoFlush(canvas, context.imageSetBatch, clippingRect.isEmpty() ? ScopedFlush::Mode::DoNothing : ScopedFlush::Mode::FlushBeforeAndAfter);
    if (!clippingRect.isEmpty())
        clipRect(canvas, clippingRect, clipTransform);

    for (auto& child : m_children)
        child->recursivePaint(canvas, context);
}

bool SkiaCompositingLayer::isVisible() const
{
    if (m_rect.isEmpty() && (m_masksToBounds || m_children.isEmpty()))
        return false;
    if (!m_visible && m_children.isEmpty())
        return false;
    if (!m_contentsVisible && m_children.isEmpty())
        return false;
    if (!hasVisualContent() && !m_backdrop.filter && m_children.isEmpty())
        return false;
    if (opacity() < s_opacityVisibilityThreshold)
        return false;
    return true;
}

TransformationMatrix SkiaCompositingLayer::replicaTransform() const
{
    return TransformationMatrix(m_replica->m_transforms.combined)
        .multiply(m_transforms.combined.inverse().value_or(TransformationMatrix()));
}

IntRect SkiaCompositingLayer::clipBounds(const SkCanvas& canvas, const PaintContext& context) const
{
    IntRect clip = canvas.getDeviceClipBounds();
    clip.move(context.offset);
    return clip;
}

sk_sp<SkImage> SkiaCompositingLayer::maskImage()
{
    if (m_maskImage)
        return m_maskImage;

    if (!m_backingStore)
        return nullptr;

    // Paint the mask at the same scale the tiles were painted.
    auto scale = m_backingStore->scale();
    auto rect = m_rect;
    rect.scale(scale);

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    auto imageInfo = SkImageInfo::Make(rect.width(), rect.height(), kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, 0, kTopLeft_GrSurfaceOrigin, nullptr);
    if (!surface)
        return nullptr;

    auto* surfaceCanvas = surface->getCanvas();
    if (!surfaceCanvas)
        return nullptr;

    surfaceCanvas->clear(SK_ColorTRANSPARENT);
    SkPaint paint;
    surfaceCanvas->scale(scale, scale);
    m_backingStore->paintToCanvas(*surfaceCanvas, paint);
    grContext->flushAndSubmit(surface.get(), GrSyncCpu::kNo);
    m_maskImage = surface->makeImageSnapshot();
    return m_maskImage;
}

void SkiaCompositingLayer::paintWithIntermediateSurface(SkCanvas& canvas, PaintContext& context, const IntRect& contentsRect, SkPaint* paint, PaintFunction&& paintFunction)
{
    auto bounds = clipBounds(canvas, context);
    if (bounds.isEmpty())
        return;

    auto surfaceRect = intersection(bounds, contentsRect);
    if (surfaceRect.isEmpty())
        return;

#if ENABLE(DAMAGE_TRACKING)
    if (!context.shouldDraw()) {
        paintFunction(canvas, context);
        return;
    }
#endif

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    auto imageInfo = SkImageInfo::Make(surfaceRect.width(), surfaceRect.height(), kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, 0, kTopLeft_GrSurfaceOrigin, nullptr);
    if (!surface)
        return;

    auto* surfaceCanvas = surface->getCanvas();
    if (!surfaceCanvas)
        return;

    context.imageSetBatch.flushIfNeeded(canvas);

    surfaceCanvas->clear(SK_ColorTRANSPARENT);
    surfaceCanvas->translate(-surfaceRect.x(), -surfaceRect.y());
    SetForScope scopedOffset(context.offset, toIntSize(surfaceRect.location()));

    {
        // The filter may sample outside the damage, so paint the whole subtree and limit only the composite.
        SetForScope scopedNoDamageRestriction(context.compositingDamageRegion, std::nullopt);
        paintFunction(*surfaceCanvas, context);
        context.imageSetBatch.flushIfNeeded(*surfaceCanvas);
    }
    grContext->flushAndSubmit(surface.get(), GrSyncCpu::kNo);

    auto snapshot = surface->makeImageSnapshot();
    const auto sampling = SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone);

    drawImageRectRestricted(canvas, context.damageRegionOrNull(), snapshot.get(), SkRect::MakeWH(surfaceRect.width(), surfaceRect.height()),
        SkRect::Make(surfaceRect), sampling, paint);
}

void SkiaCompositingLayer::paintBackdrop(SkCanvas& canvas, PaintContext& context)
{
#if ENABLE(DAMAGE_TRACKING)
    if (!context.shouldDraw()) {
        collectBackdropDamage(canvas, context);
        return;
    }
#endif

    context.imageSetBatch.flushIfNeeded(canvas);

    SkAutoCanvasRestore autoRestore(&canvas, true);
    const auto clipTransform = combinedTransform(context);
    clipRect(canvas, m_backdrop.clipRect, clipTransform);

    // Paint the backdrop root's subtree into a fresh surface (spec step 1),
    // apply the backdrop filter (step 2), and composite via SrcOver so the
    // filtered result blends onto the canvas without destroying ancestor
    // backgrounds that aren't part of the backdrop root's subtree.
    //
    // Use paintSelfAndChildren (not recursivePaint) on the backdrop root to
    // exclude the root's own effects (replica, filter, mask) per the CSS spec.
    SkPaint paint;
    paint.setImageFilter(m_backdrop.filter);
    paint.setAlphaf(context.opacity);
    if (context.blendMode)
        paint.setBlendMode(*context.blendMode);
    paintWithIntermediateSurface(canvas, context, enclosingIntRect(clipTransform.mapRect(m_backdrop.clipRect.rect())), &paint, [&](SkCanvas& canvas, PaintContext& context) {
        SetForScope scopedPaintBackdropForLayer(context.paintingBackdropForLayer, this);
        SetForScope scopedOpacity(context.opacity, 1.f);
        SetForScope scopedBlendMode(context.blendMode, std::nullopt);
        SetForScope scopedReplicaTransform(context.accumulatedReplicaTransform, TransformationMatrix());
        SetForScope scopedSkipAfterBackdrop(context.skipAfterBackdrop, false);
        backdropRoot()->paintSelfAndChildren(canvas, context);
    });
}

void SkiaCompositingLayer::paintWithMaskAndBackdrop(SkCanvas& canvas, PaintContext& context)
{
    // An empty clip path clips the whole layer away, so skip it in every mode.
    if (m_mask && m_mask->m_clipPath && m_mask->m_clipPath->isEmpty())
        return;

#if ENABLE(DAMAGE_TRACKING)
    // The mask only affects the drawn result, so apply it in the draw pass only. Damage collection
    // and debug indicators walk the tree unmasked.
    if (!context.shouldDraw()) {
        collectMaskDamage(canvas, context);

        if (m_backdrop.filter && !context.paintingBackdropForLayer)
            paintBackdrop(canvas, context);

        paintWithBlendMode(canvas, context);
        return;
    }
#endif

    bool shouldClipPath = false;
    sk_sp<SkImage> maskImage;
    if (m_mask) {
        shouldClipPath = m_mask->m_clipPath.has_value();
        if (!shouldClipPath)
            maskImage = m_mask->maskImage();
    }

    ScopedFlush autoFlush(canvas, context.imageSetBatch, shouldClipPath || maskImage ? ScopedFlush::Mode::FlushBeforeAndAfter : ScopedFlush::Mode::DoNothing);
    if (shouldClipPath || maskImage) {
        TransformationMatrix transform(context.accumulatedReplicaTransform);
        transform.multiply(m_mask->m_transforms.combined);
        if (maskImage)
            transform = transform.scale(1 / m_mask->m_backingStore->scale());
        auto matrix = SkM44(transform).asM33();

        if (shouldClipPath)
            canvas.clipPath(m_mask->m_clipPath->makeTransform(matrix), true);
        else if (auto maskShader = maskImage->makeShader({ SkFilterMode::kLinear, SkMipmapMode::kNone }, &matrix))
            canvas.clipShader(maskShader);
    }

    if (m_backdrop.filter && !context.paintingBackdropForLayer)
        paintBackdrop(canvas, context);

    paintWithBlendMode(canvas, context);
}

void SkiaCompositingLayer::paintWithFilterAndMask(SkCanvas& canvas, PaintContext& context)
{
    auto filter = this->filter();
    if (!filter) {
        paintSelfAndChildren(canvas, context);
        return;
    }

    // If we have a filter that can be simplified as a color filter
    // we don't need to create an intermediate surface.
    SkColorFilter* colorFilterPtr = nullptr;
    if (filter->filter->asAColorFilter(&colorFilterPtr)) {
        sk_sp<SkColorFilter> colorFilter(colorFilterPtr);
        SetForScope scopedColorFilter(context.colorFilter, colorFilter);
        paintSelfAndChildren(canvas, context);
        return;
    }

    // Restrict intermediate surface size to the consolidated overlap region rects,
    // matching TextureMapperLayer::paintSelfChildrenFilterAndMask behavior.
    auto mode = m_mask ? ComputeOverlapRegionMode::Mask : ComputeOverlapRegionMode::Union;
    auto overlapRects = computeConsolidatedOverlapRegionRects(canvas, context, mode);

#if ENABLE(DAMAGE_TRACKING)
    if (!context.shouldDraw()) {
        // The filter samples outside the layer, so the whole overlap region is damaged, not just what
        // the subtree drew into it. Accumulated per frame, not across frames, or a layer that moves would
        // keep damaging every region it has ever overlapped.
        // FIXME: Unlike the plain damage path, the previous overlap region is not added here, so a moved
        // or resized filtered layer leaves its old overlap undamaged. Once this damage feeds composition,
        // that could leave artifacts for blurred overlapping elements.
        if (damagePropagationEnabled()) {
            const auto clipBounds = FloatRect(this->clipBounds(canvas, context));

            FloatRect overlapRegionDamage;
            for (const auto& rect : overlapRects) {
                FloatRect damageRect(rect);
                damageRect.intersect(clipBounds);
                overlapRegionDamage.unite(damageRect);
            }

            if (!overlapRegionDamage.isEmpty())
                context.collectState->frameDamage.add(overlapRegionDamage);
        }

        paintSelfAndChildren(canvas, context);
        return;
    }
#endif

    SkPaint paint;
    paint.setImageFilter(filter->filter);

    for (const auto& rect : overlapRects) {
        if (m_mask) {
            // Mask and filter: the filter should be applied first and then the mask on the result.
            paintWithIntermediateSurface(canvas, context, rect, nullptr, [&](SkCanvas& canvas, PaintContext& context) {
                paintWithIntermediateSurface(canvas, context, rect, &paint, [&](SkCanvas& canvas, PaintContext& context) {
                    paintSelfAndChildren(canvas, context);
                });
            });
        } else {
            paintWithIntermediateSurface(canvas, context, rect, &paint, [&](SkCanvas& canvas, PaintContext& context) {
                paintSelfAndChildren(canvas, context);
            });
        }
    }
}

Vector<IntRect, 1> SkiaCompositingLayer::computeConsolidatedOverlapRegionRects(const SkCanvas& canvas, const PaintContext& context, ComputeOverlapRegionMode mode)
{
    ComputeOverlapRegionData data {
        .mode = mode,
        .clipBounds = clipBounds(canvas, context),
        .overlapRegion = { },
        .nonOverlapRegion = { }
    };
    computeOverlapRegions(data, context.accumulatedReplicaTransform, IncludesReplica::No);

    auto rects = data.overlapRegion.rects();
    if (rects.size() > cOverlapRegionConsolidationThreshold) {
        rects.clear();
        rects.append(data.overlapRegion.bounds());
    }

    return rects;
}

void SkiaCompositingLayer::paintWithReplica(SkCanvas& canvas, PaintContext& context)
{
    if (m_replica) {
        auto newAccumulatedReplicaTransform = TransformationMatrix(context.accumulatedReplicaTransform).multiply(replicaTransform());
        SetForScope scopedReplicaTransform(context.accumulatedReplicaTransform, newAccumulatedReplicaTransform);
        paintWithMaskAndBackdrop(canvas, context);
    }

    paintWithMaskAndBackdrop(canvas, context);
}

void SkiaCompositingLayer::recursivePaint(SkCanvas& canvas, PaintContext& context)
{
    if (context.skipAfterBackdrop)
        return;
    if (!isVisible())
        return;

    SetForScope scopedOpacity(context.opacity, context.opacity * opacity());

    if (m_preserves3D)
        paintWith3DRenderingContext(canvas, context);
    else
        paintWithOpacity(canvas, context);

#if ENABLE(DAMAGE_TRACKING)
    if (!context.shouldDraw())
        return;
#endif

    if (m_repaintCount)
        paintRepaintCounter(canvas, context);
}

void SkiaCompositingLayer::computeOverlapRegions(ComputeOverlapRegionData& data, const TransformationMatrix& accumulatedReplicaTransform, IncludesReplica includesReplica)
{
    if (!m_visible || !m_contentsVisible)
        return;

    auto filter = this->filter();

    FloatRect localBoundingRect;
    if (m_backingStore || m_masksToBounds || m_mask || filter || m_backdrop.filter)
        localBoundingRect = m_rect;
    else if (paintsContentsRect())
        localBoundingRect = m_contentsRect;

    if (filter && !filter->outsets.isZero() && !m_masksToBounds && !m_mask && !m_backdrop.filter) {
        localBoundingRect.move(-filter->outsets.left(), -filter->outsets.top());
        localBoundingRect.expand(filter->outsets.left() + filter->outsets.right(), filter->outsets.top() + filter->outsets.bottom());
    }

    TransformationMatrix transform(accumulatedReplicaTransform);
    transform.multiply(m_transforms.combined);

    auto viewportBoundingRect = data.transformedBoundingBox(transform, localBoundingRect);

    switch (data.mode) {
    case ComputeOverlapRegionMode::Intersection:
        data.resolveOverlaps(viewportBoundingRect);
        break;
    case ComputeOverlapRegionMode::Union:
    case ComputeOverlapRegionMode::Mask:
        data.overlapRegion.unite(viewportBoundingRect);
        break;
    }

    if (m_replica && includesReplica == IncludesReplica::Yes) {
        TransformationMatrix newReplicaTransform(accumulatedReplicaTransform);
        newReplicaTransform.multiply(replicaTransform());
        computeOverlapRegions(data, newReplicaTransform, IncludesReplica::No);
    }

    if (!m_masksToBounds && data.mode != ComputeOverlapRegionMode::Mask) {
        for (auto& child : m_children)
            child->computeOverlapRegions(data, accumulatedReplicaTransform);
    }
}

void SkiaCompositingLayer::paintWithOpacity(SkCanvas& canvas, PaintContext& context)
{
    if (opacity() == 1) {
        paintWithReplica(canvas, context);
        return;
    }

#if ENABLE(DAMAGE_TRACKING)
    // The overlap regions below only exist to composite the group correctly, and the collecting walk
    // composites nothing. Splitting there would narrow the clip, shrink the damage each sub-walk collects
    // and walk the subtree once per region rect. One unsplit walk collects a superset of the same rects.
    if (!context.shouldDraw()) {
        paintWithReplica(canvas, context);
        return;
    }
#endif

    ComputeOverlapRegionData data {
        .mode = ComputeOverlapRegionMode::Intersection,
        .clipBounds = clipBounds(canvas, context),
        .overlapRegion = { },
        .nonOverlapRegion = { }
    };
    computeOverlapRegions(data, context.accumulatedReplicaTransform);

    if (data.overlapRegion.isEmpty()) {
        paintWithReplica(canvas, context);
        return;
    }

    // Having both overlap and non-overlap regions carries some overhead.
    // Avoid it if the overlap area is big anyway.
    if (data.overlapRegion.totalArea() > data.nonOverlapRegion.totalArea()) {
        data.overlapRegion.unite(data.nonOverlapRegion);
        data.nonOverlapRegion = Region();
    }

    for (const auto& rect : data.nonOverlapRegion.rects()) {
        ScopedFlush autoFlush(canvas, context.imageSetBatch, ScopedFlush::Mode::FlushBeforeAndAfter);
        canvas.clipIRect(SkIRect::MakeLTRB(rect.x(), rect.y(), rect.maxX(), rect.maxY()));
        paintWithReplica(canvas, context);
    }

    auto overlapRects = data.overlapRegion.rects();
    if (data.nonOverlapRegion.isEmpty() && overlapRects.size() > cOverlapRegionConsolidationThreshold) {
        overlapRects.clear();
        overlapRects.append(data.overlapRegion.bounds());
    }

    SkPaint layerPaint;
    layerPaint.setAlphaf(context.opacity);
    for (const auto& rect : overlapRects) {
        SkAutoCanvasRestore autoRestore(&canvas, true);
        paintWithIntermediateSurface(canvas, context, rect, &layerPaint, [&](SkCanvas& canvas, PaintContext& context) {
            SetForScope scopedOpacity(context.opacity, 1);
            paintWithReplica(canvas, context);
        });
    }
}

void SkiaCompositingLayer::paintWithBlendMode(SkCanvas& canvas, PaintContext& context)
{
    if (!m_shouldBlend) {
        paintWithFilterAndMask(canvas, context);
        return;
    }

#if ENABLE(DAMAGE_TRACKING)
    // See paintWithOpacity(): the collecting walk composites nothing, so it needs neither the blend mode
    // nor the overlap regions.
    if (!context.shouldDraw()) {
        paintWithFilterAndMask(canvas, context);
        return;
    }
#endif

    auto blendMode = m_blendMode;
    if (!blendMode && m_shouldBlend)
        blendMode = SkBlendMode::kSrcOver;
    SetForScope scopedBlendMode(context.blendMode, context.blendMode ? context.blendMode : blendMode);

    ComputeOverlapRegionData data {
        .mode = ComputeOverlapRegionMode::Intersection,
        .clipBounds = clipBounds(canvas, context),
        .overlapRegion = { },
        .nonOverlapRegion = { }
    };
    computeOverlapRegions(data, context.accumulatedReplicaTransform);

    if (data.overlapRegion.isEmpty()) {
        paintWithFilterAndMask(canvas, context);
        return;
    }

    // Having both overlap and non-overlap regions carries some overhead.
    // Avoid it if the overlap area is big anyway.
    if (data.overlapRegion.totalArea() > data.nonOverlapRegion.totalArea()) {
        data.overlapRegion.unite(data.nonOverlapRegion);
        data.nonOverlapRegion = Region();
    }

    for (const auto& rect : data.nonOverlapRegion.rects()) {
        ScopedFlush autoFlush(canvas, context.imageSetBatch, ScopedFlush::Mode::FlushBeforeAndAfter);
        canvas.clipIRect(SkIRect::MakeLTRB(rect.x(), rect.y(), rect.maxX(), rect.maxY()));
        paintWithFilterAndMask(canvas, context);
    }

    auto overlapRects = data.overlapRegion.rects();
    if (data.nonOverlapRegion.isEmpty() && overlapRects.size() > cOverlapRegionConsolidationThreshold) {
        overlapRects.clear();
        overlapRects.append(data.overlapRegion.bounds());
    }

    SkPaint layerPaint;
    if (context.blendMode)
        layerPaint.setBlendMode(*context.blendMode);
    for (const auto& rect : overlapRects) {
        SkAutoCanvasRestore autoRestore(&canvas, true);
        paintWithIntermediateSurface(canvas, context, rect, &layerPaint, [&](SkCanvas& canvas, PaintContext& context) {
            SetForScope scopedBlendMode(context.blendMode, std::nullopt);
            paintWithFilterAndMask(canvas, context);
        });
    }
}

FloatRect SkiaCompositingLayer::transformedFlattenedBounds() const
{
    auto bounds = m_transforms.combined.mapRect(m_rect);

    if (!m_masksToBounds && !m_mask) {
        for (const auto& child : m_children)
            bounds.unite(child->transformedFlattenedBounds());
    }

    return bounds;
}

FloatPolygon3D SkiaCompositingLayer::geometryFor3DRenderingContext() const
{
    FloatRect bounds = m_rect;
    if (bounds.isEmpty() && isLeafOf3DRenderingContext() && !m_children.isEmpty() && !m_masksToBounds && !m_mask) {
        if (auto inverse = m_transforms.combined.inverse())
            bounds = inverse->mapRect(transformedFlattenedBounds());
    }

    return FloatPolygon3D(bounds, m_transforms.combined);
}

void SkiaCompositingLayer::collect3DRenderingContextLayers(Vector<SkiaCompositingLayer3DRenderingContext::Layer>& layers)
{
    if (m_preserves3D || isLeafOf3DRenderingContext()) {
        // Add layers to 3d rendering context only if they get actually painted.
        bool hasVisualContentOrFilters = hasVisualContent() || filter() || m_backdrop.filter;
        if (isVisible() && (hasVisualContentOrFilters || (isLeafOf3DRenderingContext() && !m_children.isEmpty())))
            layers.append(SkiaCompositingLayer3DRenderingContext::Layer(Ref { *this }, geometryFor3DRenderingContext()));

        // Stop recursion on 3d rendering context leaf
        if (isLeafOf3DRenderingContext())
            return;
    }

    for (const auto& child : m_children)
        child->collect3DRenderingContextLayers(layers);
}

void SkiaCompositingLayer::paintWith3DRenderingContext(SkCanvas& canvas, PaintContext& context)
{
    Vector<SkiaCompositingLayer3DRenderingContext::Layer> layers;
    collect3DRenderingContextLayers(layers);

    SkiaCompositingLayer3DRenderingContext::paint(WTF::move(layers), [&](SkiaCompositingLayer& layer, std::optional<SkPath> clipPath) {
        ScopedFlush autoFlush(canvas, context.imageSetBatch, clipPath ? ScopedFlush::Mode::FlushBeforeAndAfter : ScopedFlush::Mode::DoNothing);
        if (clipPath)
            canvas.clipPath(*clipPath);

        if (layer.m_preserves3D)
            layer.paintSelf(canvas, context);
        else
            layer.recursivePaint(canvas, context);
    });
}

void SkiaCompositingLayer::recursiveCleanUpAfterPaint()
{
#if ENABLE(DAMAGE_TRACKING)
    m_layerDamage = std::nullopt;
    m_maskChanged = false;

    // A mask and a replica are not children, and the walk does not reach them, so their damage would
    // otherwise pile up forever.
    if (m_mask)
        m_mask->recursiveCleanUpAfterPaint();
    if (m_replica)
        m_replica->recursiveCleanUpAfterPaint();

    for (auto& child : m_children)
        child->recursiveCleanUpAfterPaint();
#endif
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
