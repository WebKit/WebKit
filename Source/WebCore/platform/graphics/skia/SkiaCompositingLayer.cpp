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
#include "SkiaCompositingLayer3DRenderingContext.h"
#include "SkiaCompositingLayerFilters.h"
#include "SkiaCompositingLayerOverlapRegions.h"
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

void SkiaCompositingLayer::setSize(const FloatSize& size)
{
#if ENABLE(DAMAGE_TRACKING)
    if (frameDamagePropagationEnabled() && m_size != size) {
        m_size = size;
        damageWholeLayer();
        addPreviousRectToSharedFrameDamage();
        return;
    }
#endif
    m_size = size;
}

void SkiaCompositingLayer::setOpacity(float opacity)
{
#if ENABLE(DAMAGE_TRACKING)
    if (frameDamagePropagationEnabled() && m_opacity != opacity)
        damageWholeLayer();
#endif
    m_opacity = opacity;
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

#if ENABLE(DAMAGE_TRACKING)
    if (parent->frameDamagePropagationEnabled())
        recursiveAddPreviousRectToSharedFrameDamage(Ref { *this });
#endif
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
    m_backingStore->update(m_size, scale, WTF::move(update));
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

void SkiaCompositingLayer::setContentsSolidColor(const Color& color)
{
#if ENABLE(DAMAGE_TRACKING)
    if (frameDamagePropagationEnabled() && m_contentsSolidColor != color)
        damageWholeLayer();
#endif
    m_contentsSolidColor = color;
}

void SkiaCompositingLayer::setMask(RefPtr<SkiaCompositingLayer>&& mask)
{
    m_mask = WTF::move(mask);
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

void SkiaCompositingLayer::addPreviousRectToSharedFrameDamage()
{
    if (m_previousLayerRectInFrameCoordinates.isEmpty())
        return;

    // In many cases, damaging the whole layer in the "new" state is not enough.
    // When e.g. changing size, transform, etc. the layer (or its parts) effectively disappears from one place
    // and re-appears in another. Therefore the damaging of a layer in the "old" state is required as well.
    ASSERT(m_sharedFrameDamage);
    m_sharedFrameDamage->add(std::exchange(m_previousLayerRectInFrameCoordinates, { }));
}

void SkiaCompositingLayer::recursiveAddPreviousRectToSharedFrameDamage(Ref<SkiaCompositingLayer> layer)
{
    layer->addPreviousRectToSharedFrameDamage();

    for (auto& child : layer->m_children)
        recursiveAddPreviousRectToSharedFrameDamage(child);
}
#endif

void SkiaCompositingLayer::setDebugIndicators(Color&& debugBorderColor, std::optional<float> debugBorderWidth, std::optional<unsigned> repaintCount)
{
    if (debugBorderColor.isValid())
        m_debugBorder = { WTF::move(debugBorderColor), debugBorderWidth.value_or(1) };
    else
        m_debugBorder = std::nullopt;

    m_repaintCount = repaintCount;
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
#if ENABLE(DAMAGE_TRACKING)
    if (frameDamagePropagationEnabled() && opacity() != state.opacity.value_or(m_opacity)) {
        damageWholeLayer();
        // FIXME: add collectFrameDamageDespiteBeingInvisible?
    }
#endif
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

    if (!m_size.isEmpty() || !m_masksToBounds) {
#if ENABLE(DAMAGE_TRACKING)
        TransformationMatrix previousTransform = m_transforms.combined;
#endif

        FloatPoint origin(m_anchorPoint.x(), m_anchorPoint.y());
        origin.scale(m_size.width(), m_size.height());
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
        if (frameDamagePropagationEnabled() && previousTransform != m_transforms.combined) {
            damageWholeLayer();
            addPreviousRectToSharedFrameDamage();
        }
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

bool SkiaCompositingLayer::paint(SkCanvas& canvas, std::optional<Damage>& damage)
{
    bool hasRunningAnimations = computeTransformsAndAnimations({ }, { }, MonotonicTime::now());
    PaintContext context(damage);

    context.mode = PaintMode::Paint;
    recursivePaint(canvas, context);
    context.imageSetBatch.flushIfNeeded(canvas);

    recursiveCleanUpAfterPaint();

#if ENABLE(DAMAGE_TRACKING)
    if (damage && frameDamagePropagationEnabled()) {
        damage->add(*m_sharedFrameDamage);
        *m_sharedFrameDamage = Damage(m_size);
    }
#endif

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
    if (m_size.isEmpty() || !m_visible || !m_contentsVisible || !hasVisualContent())
        return;

    if (context.mode == PaintMode::Paint) {
        paintContents(canvas, context);
#if ENABLE(DAMAGE_TRACKING)
        collectFrameDamage(canvas, context);
#endif
    }
}

void SkiaCompositingLayer::paintContents(SkCanvas& canvas, PaintContext& context)
{
    TransformationMatrix transform(context.accumulatedReplicaTransform);
    transform.multiply(m_transforms.combined);

    auto ctm = SkM44(transform).asM33();
    bool enableAntialias = !ctm.preservesAxisAlignment() && !ctm.preservesRightAngles();

    context.imageSetBatch.updatePaintProperties(canvas, context.colorFilter, context.blendMode);

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
        context.imageSetBatch.addImageSet(canvas, *m_backingStore, ctm, context.opacity, enableAntialias);

    if (m_contentsSolidColor.isValid() && m_contentsSolidColor.isVisible()) {
        ScopedFlush autoFlush(canvas, context.imageSetBatch, ScopedFlush::Mode::FlushBefore);
        canvas.concat(SkM44(transform));
        SkPaint paint = setupPaint();
        paint.setColor(SkColor(m_contentsSolidColor.colorWithAlphaMultipliedBy(context.opacity)));
        canvas.drawRect(m_contentsRect, paint);
    } else if (m_contentsBuffer || m_imageBackingStore) {
        bool shouldPaintNow = [&] {
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
            canvas.concat(SkM44(transform));

            if (m_contentsClippingRect.hasNonZeroRadii() || !m_contentsClippingRect.rect().contains(m_contentsRect))
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
                canvas.drawRect(SkRect(m_contentsRect), paint);
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
                canvas.drawRect(m_contentsRect, paint);
            }
        }

        if (image) {
            if (shouldPaintNow) {
                SkPaint paint = setupPaint();
                canvas.drawImageRect(image, SkRect::MakeSize(SkSize::Make(image->dimensions())), SkRect(m_contentsRect),
                    SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone), &paint, SkCanvas::kFast_SrcRectConstraint);
            } else {
                auto clippingRect = m_contentsClippingRect.rect();
                if (clippingRect.contains(m_contentsRect))
                    clippingRect = { };
                context.imageSetBatch.addImage(canvas, image, m_contentsRect, clippingRect, ctm, context.opacity, enableAntialias);
            }
        }
    }
}

#if ENABLE(DAMAGE_TRACKING)
void SkiaCompositingLayer::collectFrameDamage(SkCanvas& canvas, PaintContext& context)
{
    if (!frameDamagePropagationEnabled() || !context.frameDamage)
        return;

    TransformationMatrix transform(context.accumulatedReplicaTransform);
    transform.multiply(m_transforms.combined);
    auto frameDamage = transform.mapRect(effectiveLayerRect());
    auto clipBounds = FloatRect(this->clipBounds(canvas, context));
    if (!clipBounds.isEmpty())
        frameDamage.intersect(clipBounds);

    m_previousLayerRectInFrameCoordinates.unite(frameDamage);

    if (m_layerDamage) {
        for (const auto& rect : *m_layerDamage) {
            auto damageRect = transform.mapRect(FloatRect(rect));
            if (!clipBounds.isEmpty())
                damageRect.intersect(clipBounds);
            context.frameDamage->add(damageRect);
        }
    }
}

#endif

void SkiaCompositingLayer::paintDebugIndicators(SkCanvas& canvas, PaintContext& context)
{
    if (m_size.isEmpty() || !m_visible || !m_contentsVisible || !hasVisualContent())
        return;

    TransformationMatrix transform(context.accumulatedReplicaTransform);
    transform.multiply(m_transforms.combined);

    if (m_debugBorder) {
        SkAutoCanvasRestore autoRestore(&canvas, true);
        canvas.concat(SkM44(transform));

        SkPaint borderPaint;
        borderPaint.setStyle(SkPaint::kStroke_Style);
        borderPaint.setColor(SkColor(m_debugBorder->color));
        borderPaint.setStrokeWidth(m_debugBorder->width);
        borderPaint.setAntiAlias(true);

        if (m_backingStore)
            m_backingStore->drawDebugBorders(canvas, borderPaint);
        if (m_contentsBuffer || m_imageBackingStore || (m_contentsSolidColor.isValid() && m_contentsSolidColor.isVisible()))
            canvas.drawRect(SkRect(m_contentsRect), borderPaint);
    }

    if (!m_repaintCount)
        return;

    // Capture the full canvas-to-device position while the layer transform is still active.
    SkPoint deviceOrigin { 0, 0 };
    auto mapped = canvas.getLocalToDevice().map(0, 0, 0, 1);
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

    SkAutoCanvasRestore autoRestore(&canvas, true);

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
            childBounds = m_children[0]->effectiveLayerRect();
        if (m_children[0]->m_contentsBuffer || m_children[0]->m_imageBackingStore || (m_children[0]->m_contentsSolidColor.isValid() && m_children[0]->m_contentsSolidColor.isVisible()))
            childBounds.unite(m_children[0]->m_contentsRect);
        return matrix.mapRect(SkRect(rect.rect())).contains(childMatrix.mapRect(SkRect(childBounds)));
    };

    const bool contentsRectClipsDescendants = !m_preserves3D && m_contentsRectClipsDescendants && (m_contentsClippingRect.hasNonZeroRadii() || !m_contentsClippingRect.rect().contains(m_contentsRect));
    const bool masksToBounds = !m_preserves3D && m_masksToBounds;
    TransformationMatrix clipTransform;
    FloatRoundedRect clippingRect;
    if (masksToBounds || contentsRectClipsDescendants) {
        clipTransform = context.accumulatedReplicaTransform;
        clipTransform.multiply(m_transforms.combined);
        if (!contentsRectClipsDescendants)
            clipTransform.translate(m_boundsOrigin.x(), m_boundsOrigin.y());

        FloatRoundedRect rect = contentsRectClipsDescendants ? m_contentsClippingRect : FloatRoundedRect(effectiveLayerRect());
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
    if (m_size.isEmpty() && (m_masksToBounds || m_children.isEmpty()))
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
    auto rect = effectiveLayerRect();
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

    if (context.mode != PaintMode::Paint) {
        paintFunction(canvas, context);
        return;
    }

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
    paintFunction(*surfaceCanvas, context);
    context.imageSetBatch.flushIfNeeded(*surfaceCanvas);
    grContext->flushAndSubmit(surface.get(), GrSyncCpu::kNo);

    canvas.drawImageRect(surface->makeImageSnapshot(), SkRect::MakeWH(surfaceRect.width(), surfaceRect.height()), SkRect::Make(surfaceRect), SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone), paint, SkCanvas::kFast_SrcRectConstraint);
}

void SkiaCompositingLayer::paintBackdrop(SkCanvas& canvas, PaintContext& context)
{
    context.imageSetBatch.flushIfNeeded(canvas);

    SkAutoCanvasRestore autoRestore(&canvas, true);
    TransformationMatrix clipTransform(context.accumulatedReplicaTransform);
    clipTransform.multiply(m_transforms.combined);
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
    // An empty clip path fully clips the layer out, so it isn't painted at all.
    // Skip it in every mode: there's nothing to paint, to collect damage for, or
    // to annotate with debug indicators.
    if (m_mask && m_mask->m_clipPath && m_mask->m_clipPath->isEmpty())
        return;

    // Otherwise the mask only affects the painted result, so apply it (clip path
    // or mask image) only in PaintMode::Paint. Damage collection and debug
    // indicators walk the tree unmasked.
    bool shouldClipPath = false;
    sk_sp<SkImage> maskImage;
    if (context.mode == PaintMode::Paint && m_mask) {
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
    auto clipBounds = FloatRect(this->clipBounds(canvas, context));
    const bool collectsDamage = context.mode == PaintMode::Paint;
    const bool needToProcessFrameDamage = collectsDamage && frameDamagePropagationEnabled() && context.frameDamage;
#endif

    if (context.mode != PaintMode::Paint) {
#if ENABLE(DAMAGE_TRACKING)
        if (needToProcessFrameDamage) {
            for (const auto& rect : overlapRects) {
                FloatRect damageRect(rect);
                if (!clipBounds.isEmpty())
                    damageRect.intersect(clipBounds);
                m_accumulatedOverlapRegionFrameDamage.unite(damageRect);
            }
            if (!m_accumulatedOverlapRegionFrameDamage.isEmpty())
                context.frameDamage->add(m_accumulatedOverlapRegionFrameDamage);
        }
#endif
        paintSelfAndChildren(canvas, context);
        return;
    }

    SkPaint paint;
    paint.setImageFilter(filter->filter);

    for (const auto& rect : overlapRects) {
#if ENABLE(DAMAGE_TRACKING)
        if (needToProcessFrameDamage) {
            FloatRect damageRect(rect);
            if (!clipBounds.isEmpty())
                damageRect.intersect(clipBounds);

            m_accumulatedOverlapRegionFrameDamage.unite(damageRect);
        }
#endif

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

#if ENABLE(DAMAGE_TRACKING)
    if (needToProcessFrameDamage && !m_accumulatedOverlapRegionFrameDamage.isEmpty())
        context.frameDamage->add(m_accumulatedOverlapRegionFrameDamage);
#endif
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

    // Drawn after the layer's own content, filters and masks have been composited
    // into the parent, but still in tree order, so layers painted on top (e.g. a
    // modal) occlude the indicators of the layers they cover.
    if (context.mode == PaintMode::Paint && hasDebugIndicators())
        paintDebugIndicators(canvas, context);
}

void SkiaCompositingLayer::computeOverlapRegions(ComputeOverlapRegionData& data, const TransformationMatrix& accumulatedReplicaTransform, IncludesReplica includesReplica)
{
    if (!m_visible || !m_contentsVisible)
        return;

    auto filter = this->filter();

    FloatRect localBoundingRect;
    if (m_backingStore || m_masksToBounds || m_mask || filter || m_backdrop.filter)
        localBoundingRect = effectiveLayerRect();
    else if (m_contentsBuffer || m_imageBackingStore || (m_contentsSolidColor.isValid() && m_contentsSolidColor.isVisible()))
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

bool SkiaCompositingLayer::hasVisualContent() const
{
    return m_backingStore || m_imageBackingStore || m_contentsBuffer
        || (m_contentsSolidColor.isValid() && m_contentsSolidColor.isVisible());
}

void SkiaCompositingLayer::collect3DRenderingContextLayers(Vector<Ref<SkiaCompositingLayer>>& layers)
{
    if (m_preserves3D || isLeafOf3DRenderingContext()) {
        // Add layers to 3d rendering context only if they get actually painted.
        bool hasVisualContentOrFilters = hasVisualContent() || filter() || m_backdrop.filter;
        if (isVisible() && (hasVisualContentOrFilters || (isLeafOf3DRenderingContext() && !m_children.isEmpty())))
            layers.append(Ref { *this });

        // Stop recursion on 3d rendering context leaf
        if (isLeafOf3DRenderingContext())
            return;
    }

    for (auto& child : m_children)
        child->collect3DRenderingContextLayers(layers);
}

void SkiaCompositingLayer::paintWith3DRenderingContext(SkCanvas& canvas, PaintContext& context)
{
    Vector<Ref<SkiaCompositingLayer>> layers;
    collect3DRenderingContextLayers(layers);

    SkiaCompositingLayer3DRenderingContext::paint(layers, [&](SkiaCompositingLayer& layer, std::optional<SkPath> clipPath) {
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
    for (Ref child : m_children)
        child->recursiveCleanUpAfterPaint();
#endif
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
