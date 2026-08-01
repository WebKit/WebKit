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
#include "ClipPathPaintScope.h"

#include "BoxLayoutShape.h"
#include "LegacyRenderSVGResourceClipper.h"
#include "RenderBox.h"
#include "RenderLayerModelObject.h"
#include "RenderObjectDocument.h"
#include "RenderObjectInlines.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGResourceClipperInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePathOperationWrappers.h"

namespace WebCore {

ClipPathPaintScope::ClipPathPaintScope(GraphicsContext& context, RegionContext* regionContext, RenderLayerModelObject& renderer, const LayoutSize& offsetFromRoot, const LayoutSize& subpixelOffset, const LayoutRect& clippedContentBounds, bool isCollectingEventRegion, CoordinateMode mode)
    : m_clipSaver(context, false)
    , m_regionSaver(regionContext)
{
    auto& clipPathStyle = renderer.style().clipPath();
    ASSERT(!WTF::holdsAlternative<CSS::Keyword::None>(clipPathStyle));

    if (WTF::holdsAlternative<Style::BasicShapePath>(clipPathStyle) || (WTF::holdsAlternative<Style::BoxPath>(clipPathStyle) && renderer.isRenderBox()))
        applyBasicShapeOrBoxClip(context, renderer, offsetFromRoot, subpixelOffset, clippedContentBounds, isCollectingEventRegion, mode);
    else if (CheckedPtr svgClipper = renderer.svgClipperResourceFromStyle())
        applySVGResourceClip(context, renderer, *svgClipper, offsetFromRoot, subpixelOffset, clippedContentBounds, mode);
    else if (CheckedPtr legacyClipper = renderer.legacySVGClipperResourceFromStyle())
        applyLegacySVGResourceClip(context, renderer, *legacyClipper, offsetFromRoot, clippedContentBounds);
}

FloatRect ClipPathPaintScope::referenceBoxRectForClipPath(const RenderLayerModelObject& renderer, CSSBoxType boxType, const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBounds)
{
    bool isReferenceBox = renderer.isSVGLayerAwareRenderer() ? true : renderer.isRenderBox();

    // FIXME: Support different reference boxes for inline content.
    // https://bugs.webkit.org/show_bug.cgi?id=129047
    if (!isReferenceBox)
        return rootRelativeBounds;

    auto referenceBoxRect = renderer.referenceBoxRect(boxType);
    referenceBoxRect.move(offsetFromRoot);
    return referenceBoxRect;
}

std::pair<Path, WindRule> ClipPathPaintScope::computeClipPath(const RenderLayerModelObject& renderer, const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBoundsForNonBoxes)
{
    CheckedRef style = renderer.style();

    return WTF::switchOn(style->clipPath(),
        [&](const Style::BasicShapePath& clipPath) -> std::pair<Path, WindRule> {
            auto referenceBoxRect = referenceBoxRectForClipPath(renderer, clipPath.referenceBox(), offsetFromRoot, rootRelativeBoundsForNonBoxes);
            auto snappedReferenceBoxRect = snapRectToDevicePixelsIfNeeded(referenceBoxRect, renderer);
            return { Style::path(clipPath.shape(), snappedReferenceBoxRect, style->usedZoomForLength()), Style::windRule(clipPath.shape()) };
        },
        [&](const Style::BoxPath& clipPath) -> std::pair<Path, WindRule> {
            CheckedPtr box = dynamicDowncast<RenderBox>(renderer);
            if (box) {
                auto shapeRect = computeRoundedRectForBoxShape(clipPath.referenceBox(), *box).pixelSnappedRoundedRectForPainting(renderer.document().deviceScaleFactor());
                shapeRect.move(offsetFromRoot);
                return { shapeRect.path(), WindRule::NonZero };
            }
            return { Path(), WindRule::NonZero };
        },
        [&](const auto&) -> std::pair<Path, WindRule> {
            return { Path(), WindRule::NonZero };
        }
    );
}

void ClipPathPaintScope::applyBasicShapeOrBoxClip(GraphicsContext& context, RenderLayerModelObject& renderer, const LayoutSize& offsetFromRoot, const LayoutSize& subpixelOffset, const LayoutRect& clippedContentBounds, bool isCollectingEventRegion, CoordinateMode mode)
{
    auto offset = offsetFromRoot + subpixelOffset;

    // SVG-layer-aware content paints shifted by its currentSVGLayoutLocation, so fold that into the
    // offset and computeClipPath() places the path at its final position, with no separate translate.
    // Only the non-layer path needs this, the layer path is placed by the layer transform.
    if (mode == CoordinateMode::NonLayerPaint && renderer.isSVGLayerAwareRenderer())
        offset += toLayoutSize(renderer.currentSVGLayoutLocation());

    // Layer-aware SVG content below the SVG root is never device-pixel snapped (rendererNeedsPixelSnapping).
    // Snapping applies only to CSS boxes and the SVG root.
    auto paintingOffsetFromRoot = rendererNeedsPixelSnapping(renderer)
        ? LayoutSize(snapSizeToDevicePixel(offset, LayoutPoint(), renderer.document().deviceScaleFactor()))
        : offset;

    // clippedContentBounds is the reference box for inlines, poorly specified: https://github.com/w3c/csswg-drafts/issues/6383.
    auto [path, windRule] = computeClipPath(renderer, paintingOffsetFromRoot, clippedContentBounds);

    if (isCollectingEventRegion) {
        m_regionSaver.pushClip(path);
        return;
    }

    m_clipSaver.save();
    context.clipPath(path, windRule);
}

void ClipPathPaintScope::applySVGResourceClip(GraphicsContext& context, RenderLayerModelObject& renderer, RenderSVGResourceClipper& svgClipper, const LayoutSize& offsetFromRoot, const LayoutSize& subpixelOffset, const LayoutRect& clippedContentBounds, CoordinateMode mode)
{
    RefPtr graphicsElement = svgClipper.shouldApplyPathClipping();
    if (!graphicsElement) {
        m_needsMaskClipping = true;
        return;
    }

    m_clipSaver.save();
    FloatRect svgReferenceBox;
    // userSpaceOnUse geometry is in absolute user space, so move it to the renderer's content origin
    // with a translate. objectBoundingBox geometry carries its origin in objectBoundingBox.location()
    // (applied inside applyPathClipping) and needs no translate.
    FloatSize userSpaceClipTranslation;
    if (renderer.isSVGLayerAwareRenderer()) {
        ASSERT_UNUSED(subpixelOffset, subpixelOffset.isZero());
        svgReferenceBox = renderer.objectBoundingBox();
        if (svgClipper.clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
            // The layer path's CTM is already at the renderer, so offsetFromRoot minus nominal is
            // enough. The non-layer path's CTM is at the container, so also add currentSVGLayoutLocation
            // to match RenderSVGShape::paint.
            userSpaceClipTranslation = toLayoutPoint(offsetFromRoot) - renderer.nominalSVGLayoutLocation();
            if (mode == CoordinateMode::NonLayerPaint)
                userSpaceClipTranslation += toLayoutSize(renderer.currentSVGLayoutLocation());
        }
    } else {
        auto clipPathObjectBoundingBox = referenceBoxRectForClipPath(renderer, CSSBoxType::BorderBox, offsetFromRoot, clippedContentBounds);
        svgReferenceBox = snapRectToDevicePixels(LayoutRect(clipPathObjectBoundingBox), renderer.document().deviceScaleFactor());
    }

    if (!userSpaceClipTranslation.isZero())
        context.translate(userSpaceClipTranslation);

    svgClipper.applyPathClipping(context, renderer, svgReferenceBox, *graphicsElement);

    if (!userSpaceClipTranslation.isZero())
        context.translate(-userSpaceClipTranslation);
}

void ClipPathPaintScope::applyLegacySVGResourceClip(GraphicsContext& context, RenderLayerModelObject& renderer, LegacyRenderSVGResourceClipper& svgClipper, const LayoutSize& offsetFromRoot, const LayoutRect& clippedContentBounds)
{
    // Use the border box as the reference box, not clearly specified: https://github.com/w3c/csswg-drafts/issues/5786.
    // clippedContentBounds is the reference box for inlines, poorly specified: https://github.com/w3c/csswg-drafts/issues/6383.
    auto referenceBox = referenceBoxRectForClipPath(renderer, CSSBoxType::BorderBox, offsetFromRoot, clippedContentBounds);
    auto snappedReferenceBox = snapRectToDevicePixelsIfNeeded(referenceBox, renderer);
    auto offset = snappedReferenceBox.location();

    auto snappedClippingBounds = snapRectToDevicePixelsIfNeeded(clippedContentBounds, renderer);
    snappedClippingBounds.moveBy(-offset);

    m_clipSaver.save();
    context.translate(offset);
    svgClipper.applyClippingToContext(context, renderer, { { }, referenceBox.size() }, snappedClippingBounds, renderer.style().usedZoom());
    context.translate(-offset);

    // FIXME: Support event regions.
}

} // namespace WebCore
