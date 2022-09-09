/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "BackgroundPainter.h"

#include "BitmapImage.h"
#include "BorderPainter.h"
#include "CachedImage.h"
#include "ColorBlending.h"
#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "InlineIteratorInlineBox.h"
#include "NinePieceImage.h"
#include "PaintInfo.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderView.h"
#include "TextBoxPainter.h"

namespace WebCore {

BackgroundImageGeometry::BackgroundImageGeometry(const LayoutRect& destinationRect, const LayoutSize& tileSizeWithoutPixelSnapping, const LayoutSize& tileSize, const LayoutSize& phase, const LayoutSize& spaceSize, bool fixedAttachment)
    : destinationRect(destinationRect)
    , destinationOrigin(destinationRect.location())
    , tileSizeWithoutPixelSnapping(tileSizeWithoutPixelSnapping)
    , tileSize(tileSize)
    , phase(phase)
    , spaceSize(spaceSize)
    , hasNonLocalGeometry(fixedAttachment)
{
}

BackgroundPainter::BackgroundPainter(RenderBoxModelObject& renderer, const PaintInfo& paintInfo)
    : m_renderer(renderer)
    , m_paintInfo(paintInfo)
{
}

void BackgroundPainter::paintFillLayers(const Color& color, const FillLayer& fillLayer, const LayoutRect& rect, BackgroundBleedAvoidance bleedAvoidance, CompositeOperator op, RenderElement* backgroundObject)
{
    Vector<const FillLayer*, 8> layers;
    bool shouldDrawBackgroundInSeparateBuffer = false;

    for (auto* layer = &fillLayer; layer; layer = layer->next()) {
        layers.append(layer);

        if (layer->blendMode() != BlendMode::Normal)
            shouldDrawBackgroundInSeparateBuffer = true;

        // Stop traversal when an opaque layer is encountered.
        // FIXME: It would be possible for the following occlusion culling test to be more aggressive
        // on layers with no repeat by testing whether the image covers the layout rect.
        // Testing that here would imply duplicating a lot of calculations that are currently done in
        // BackgroundPainter::paintFillLayer. A more efficient solution might be to move
        // the layer recursion into paintFillLayer, or to compute the layer geometry here
        // and pass it down.

        // The clipOccludesNextLayers condition must be evaluated first to avoid short-circuiting.
        if (layer->clipOccludesNextLayers(layer == &fillLayer) && layer->hasOpaqueImage(m_renderer) && layer->image()->canRender(&m_renderer, m_renderer.style().effectiveZoom()) && layer->hasRepeatXY() && layer->blendMode() == BlendMode::Normal)
            break;
    }

    auto& context = m_paintInfo.context();
    auto baseBgColorUsage = BaseBackgroundColorUse;

    if (shouldDrawBackgroundInSeparateBuffer) {
        paintFillLayer(color, *layers.last(), rect, bleedAvoidance, { }, { }, op, backgroundObject, BaseBackgroundColorOnly);
        baseBgColorUsage = BaseBackgroundColorSkip;
        context.beginTransparencyLayer(1);
    }

    for (auto& layer : makeReversedRange(layers))
        paintFillLayer(color, *layer, rect, bleedAvoidance, { }, { }, op, backgroundObject, baseBgColorUsage);

    if (shouldDrawBackgroundInSeparateBuffer)
        context.endTransparencyLayer();
}

static void applyBoxShadowForBackground(GraphicsContext& context, const RenderStyle& style)
{
    const ShadowData* boxShadow = style.boxShadow();
    while (boxShadow->style() != ShadowStyle::Normal)
        boxShadow = boxShadow->next();

    FloatSize shadowOffset(boxShadow->x().value(), boxShadow->y().value());
    context.setShadow(shadowOffset, boxShadow->radius().value(), style.colorByApplyingColorFilter(boxShadow->color()), boxShadow->isWebkitBoxShadow() ? ShadowRadiusMode::Legacy : ShadowRadiusMode::Default);
}

void BackgroundPainter::paintFillLayer(const Color& color, const FillLayer& bgLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, const InlineIterator::InlineBoxIterator& box, const LayoutRect& backgroundImageStrip, CompositeOperator op, RenderElement* backgroundObject, BaseBackgroundColorUsage baseBgColorUsage)
{
    GraphicsContext& context = m_paintInfo.context();

    if ((context.paintingDisabled() && !context.detectingContentfulPaint()) || rect.isEmpty())
        return;

    auto [includeLeftEdge, includeRightEdge] = box ? box->hasClosedLeftAndRightEdge() : std::pair(true, true);

    auto& style = m_renderer.style();

    bool hasRoundedBorder = style.hasBorderRadius() && (includeLeftEdge || includeRightEdge);
    bool clippedWithLocalScrolling = m_renderer.hasNonVisibleOverflow() && bgLayer.attachment() == FillAttachment::LocalBackground;
    bool isBorderFill = bgLayer.clip() == FillBox::Border;
    bool isRoot = m_renderer.isDocumentElementRenderer();

    Color bgColor = color;
    StyleImage* bgImage = bgLayer.image();
    bool shouldPaintBackgroundImage = bgImage && bgImage->canRender(&m_renderer, style.effectiveZoom());

    if (context.detectingContentfulPaint()) {
        if (!context.contentfulPaintDetected() && shouldPaintBackgroundImage && bgImage->cachedImage()) {
            if (style.backgroundSizeType() != FillSizeType::Size || !style.backgroundSizeLength().isEmpty())
                context.setContentfulPaintDetected();
            return;
        }
    }

    if (context.invalidatingImagesWithAsyncDecodes()) {
        if (shouldPaintBackgroundImage && bgImage->cachedImage()->isClientWaitingForAsyncDecoding(m_renderer))
            bgImage->cachedImage()->removeAllClientsWaitingForAsyncDecoding();
        return;
    }

    bool forceBackgroundToWhite = false;
    if (document().printing()) {
        if (style.printColorAdjust() == PrintColorAdjust::Economy)
            forceBackgroundToWhite = true;
        if (document().settings().shouldPrintBackgrounds())
            forceBackgroundToWhite = false;
    }

    // When printing backgrounds is disabled or using economy mode,
    // change existing background colors and images to a solid white background.
    // If there's no bg color or image, leave it untouched to avoid affecting transparency.
    // We don't try to avoid loading the background images, because this style flag is only set
    // when printing, and at that point we've already loaded the background images anyway. (To avoid
    // loading the background images we'd have to do this check when applying styles rather than
    // while rendering.)
    if (forceBackgroundToWhite) {
        // Note that we can't reuse this variable below because the bgColor might be changed
        bool shouldPaintBackgroundColor = !bgLayer.next() && bgColor.isVisible();
        if (shouldPaintBackgroundImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            shouldPaintBackgroundImage = false;
        }
    }

    bool baseBgColorOnly = (baseBgColorUsage == BaseBackgroundColorOnly);
    if (baseBgColorOnly && (!isRoot || bgLayer.next() || bgColor.isOpaque()))
        return;

    bool colorVisible = bgColor.isVisible();
    float deviceScaleFactor = document().deviceScaleFactor();
    FloatRect pixelSnappedRect = snapRectToDevicePixels(rect, deviceScaleFactor);

    // Fast path for drawing simple color backgrounds.
    if (!isRoot && !clippedWithLocalScrolling && !shouldPaintBackgroundImage && isBorderFill && !bgLayer.next()) {
        if (!colorVisible)
            return;

        bool boxShadowShouldBeAppliedToBackground = m_renderer.boxShadowShouldBeAppliedToBackground(rect.location(), bleedAvoidance, box);
        GraphicsContextStateSaver shadowStateSaver(context, boxShadowShouldBeAppliedToBackground);
        if (boxShadowShouldBeAppliedToBackground)
            applyBoxShadowForBackground(context, style);

        if (hasRoundedBorder && bleedAvoidance != BackgroundBleedUseTransparencyLayer) {
            FloatRoundedRect pixelSnappedBorder = backgroundRoundedRectAdjustedForBleedAvoidance(rect, bleedAvoidance, box,
                includeLeftEdge, includeRightEdge).pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            if (pixelSnappedBorder.isRenderable()) {
                CompositeOperator previousOperator = context.compositeOperation();
                bool saveRestoreCompositeOp = op != previousOperator;
                if (saveRestoreCompositeOp)
                    context.setCompositeOperation(op);

                context.fillRoundedRect(pixelSnappedBorder, bgColor);

                if (saveRestoreCompositeOp)
                    context.setCompositeOperation(previousOperator);
            } else {
                context.save();
                clipRoundedInnerRect(context, pixelSnappedRect, pixelSnappedBorder);
                context.fillRect(pixelSnappedBorder.rect(), bgColor, op);
                context.restore();
            }
        } else
            context.fillRect(pixelSnappedRect, bgColor, op);

        return;
    }

    // FillBox::Border radius clipping is taken care of by BackgroundBleedUseTransparencyLayer
    bool clipToBorderRadius = hasRoundedBorder && !(isBorderFill && bleedAvoidance == BackgroundBleedUseTransparencyLayer);
    GraphicsContextStateSaver clipToBorderStateSaver(context, clipToBorderRadius);
    if (clipToBorderRadius) {
        RoundedRect border = isBorderFill ? backgroundRoundedRectAdjustedForBleedAvoidance(rect, bleedAvoidance, box, includeLeftEdge, includeRightEdge) : backgroundRoundedRect(rect, box, includeLeftEdge, includeRightEdge);

        // Clip to the padding or content boxes as necessary.
        if (bgLayer.clip() == FillBox::Content) {
            border = style.getRoundedInnerBorderFor(border.rect(),
                m_renderer.paddingTop() + m_renderer.borderTop(), m_renderer.paddingBottom() + m_renderer.borderBottom(),
                m_renderer.paddingLeft() + m_renderer.borderLeft(), m_renderer.paddingRight() + m_renderer.borderRight(),
                includeLeftEdge, includeRightEdge);
        } else if (bgLayer.clip() == FillBox::Padding)
            border = style.getRoundedInnerBorderFor(border.rect(), includeLeftEdge, includeRightEdge);

        clipRoundedInnerRect(context, pixelSnappedRect, border.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
    }

    LayoutUnit bLeft = includeLeftEdge ? m_renderer.borderLeft() : 0_lu;
    LayoutUnit bRight = includeRightEdge ? m_renderer.borderRight() : 0_lu;
    LayoutUnit pLeft = includeLeftEdge ? m_renderer.paddingLeft() : 0_lu;
    LayoutUnit pRight = includeRightEdge ? m_renderer.paddingRight() : 0_lu;

    GraphicsContextStateSaver clipWithScrollingStateSaver(context, clippedWithLocalScrolling);
    LayoutRect scrolledPaintRect = rect;
    if (clippedWithLocalScrolling) {
        // Clip to the overflow area.
        auto& renderBox = downcast<RenderBox>(m_renderer);
        context.clip(renderBox.overflowClipRect(rect.location()));

        // Adjust the paint rect to reflect a scrolled content box with borders at the ends.
        scrolledPaintRect.moveBy(-renderBox.scrollPosition());
        scrolledPaintRect.setWidth(bLeft + renderBox.layer()->scrollWidth() + bRight);
        scrolledPaintRect.setHeight(renderBox.borderTop() + renderBox.layer()->scrollHeight() + renderBox.borderBottom());
    }

    GraphicsContextStateSaver backgroundClipStateSaver(context, false);
    RefPtr<ImageBuffer> maskImage;
    FloatRect maskRect;

    if (bgLayer.clip() == FillBox::Padding || bgLayer.clip() == FillBox::Content) {
        // Clip to the padding or content boxes as necessary.
        if (!clipToBorderRadius) {
            bool includePadding = bgLayer.clip() == FillBox::Content;
            LayoutRect clipRect = LayoutRect(scrolledPaintRect.x() + bLeft + (includePadding ? pLeft : 0_lu),
                scrolledPaintRect.y() + m_renderer.borderTop() + (includePadding ? m_renderer.paddingTop() : 0_lu),
                scrolledPaintRect.width() - bLeft - bRight - (includePadding ? pLeft + pRight : 0_lu),
                scrolledPaintRect.height() - m_renderer.borderTop() - m_renderer.borderBottom() - (includePadding ? m_renderer.paddingTop() + m_renderer.paddingBottom() : 0_lu));
            backgroundClipStateSaver.save();
            context.clip(clipRect);
        }
    } else if (bgLayer.clip() == FillBox::Text) {
        // We have to draw our text into a mask that can then be used to clip background drawing.
        // First figure out how big the mask has to be. It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        maskRect = snapRectToDevicePixels(rect, deviceScaleFactor);
        maskRect.intersect(snapRectToDevicePixels(m_paintInfo.rect, deviceScaleFactor));

        // Now create the mask.
        maskImage = context.createAlignedImageBuffer(maskRect.size());
        if (!maskImage)
            return;

        m_renderer.paintMaskForTextFillBox(maskImage.get(), maskRect, box, scrolledPaintRect);

        // The mask has been created. Now we just need to clip to it.
        backgroundClipStateSaver.save();
        context.clip(maskRect);
        context.beginTransparencyLayer(1);
    }

    auto isOpaqueRoot = false;
    if (isRoot) {
        isOpaqueRoot = bgLayer.next() || bgColor.isOpaque() || view().shouldPaintBaseBackground();
        view().frameView().setContentIsOpaque(isOpaqueRoot);
    }

    // Paint the color first underneath all images, culled if background image occludes it.
    // FIXME: In the bgLayer.hasFiniteBounds() case, we could improve the culling test
    // by verifying whether the background image covers the entire layout rect.
    if (!bgLayer.next()) {
        LayoutRect backgroundRect(scrolledPaintRect);
        bool boxShadowShouldBeAppliedToBackground = m_renderer.boxShadowShouldBeAppliedToBackground(rect.location(), bleedAvoidance, box);
        if (boxShadowShouldBeAppliedToBackground || !shouldPaintBackgroundImage || !bgLayer.hasOpaqueImage(m_renderer) || !bgLayer.hasRepeatXY() || bgLayer.isEmpty()) {
            if (!boxShadowShouldBeAppliedToBackground)
                backgroundRect.intersect(m_paintInfo.rect);

            // If we have an alpha and we are painting the root element, blend with the base background color.
            Color baseColor;
            bool shouldClearBackground = false;
            if ((baseBgColorUsage != BaseBackgroundColorSkip) && isOpaqueRoot) {
                baseColor = view().frameView().baseBackgroundColor();
                if (!baseColor.isVisible())
                    shouldClearBackground = true;
            }

            GraphicsContextStateSaver shadowStateSaver(context, boxShadowShouldBeAppliedToBackground);
            if (boxShadowShouldBeAppliedToBackground)
                applyBoxShadowForBackground(context, style);

            FloatRect backgroundRectForPainting = snapRectToDevicePixels(backgroundRect, deviceScaleFactor);
            if (baseColor.isVisible()) {
                if (!baseBgColorOnly && bgColor.isVisible())
                    baseColor = blendSourceOver(baseColor, bgColor);
                context.fillRect(backgroundRectForPainting, baseColor, CompositeOperator::Copy);
            } else if (!baseBgColorOnly && bgColor.isVisible()) {
                auto operation = context.compositeOperation();
                if (shouldClearBackground) {
                    if (op == CompositeOperator::DestinationOut) // We're punching out the background.
                        operation = op;
                    else
                        operation = CompositeOperator::Copy;
                }
                context.fillRect(backgroundRectForPainting, bgColor, operation);
            } else if (shouldClearBackground)
                context.clearRect(backgroundRectForPainting);
        }
    }

    // no progressive loading of the background image
    if (!baseBgColorOnly && shouldPaintBackgroundImage) {
        // Multiline inline boxes paint like the image was one long strip spanning lines. The backgroundImageStrip is this fictional rectangle.
        auto imageRect = backgroundImageStrip.isEmpty() ? scrolledPaintRect : backgroundImageStrip;
        auto paintOffset = backgroundImageStrip.isEmpty() ? rect.location() : backgroundImageStrip.location();
        auto geometry = calculateBackgroundImageGeometry(m_renderer, m_paintInfo.paintContainer, bgLayer, paintOffset, imageRect);

        auto& clientForBackgroundImage = backgroundObject ? *backgroundObject : m_renderer;
        bgImage->setContainerContextForRenderer(clientForBackgroundImage, geometry.tileSizeWithoutPixelSnapping, m_renderer.style().effectiveZoom());

        geometry.clip(LayoutRect(pixelSnappedRect));
        RefPtr<Image> image;
        if (!geometry.destinationRect.isEmpty() && (image = bgImage->image(backgroundObject ? backgroundObject : &m_renderer, geometry.tileSize))) {
            context.setDrawLuminanceMask(bgLayer.maskMode() == MaskMode::Luminance);

            if (is<BitmapImage>(image))
                downcast<BitmapImage>(*image).updateFromSettings(document().settings());

            ImagePaintingOptions options = {
                op == CompositeOperator::SourceOver ? bgLayer.compositeForPainting() : op,
                bgLayer.blendMode(),
                m_renderer.decodingModeForImageDraw(*image, m_paintInfo),
                ImageOrientation::FromImage,
                m_renderer.chooseInterpolationQuality(context, *image, &bgLayer, geometry.tileSize)
            };

            auto drawResult = context.drawTiledImage(*image, geometry.destinationRect, toLayoutPoint(geometry.relativePhase()), geometry.tileSize, geometry.spaceSize, options);
            if (drawResult == ImageDrawResult::DidRequestDecoding) {
                ASSERT(bgImage->hasCachedImage());
                bgImage->cachedImage()->addClientWaitingForAsyncDecoding(m_renderer);
            }
        }
    }

    if (maskImage && bgLayer.clip() == FillBox::Text) {
        context.drawConsumingImageBuffer(WTFMove(maskImage), maskRect, CompositeOperator::DestinationIn);
        context.endTransparencyLayer();
    }
}

void BackgroundPainter::clipRoundedInnerRect(GraphicsContext& context, const FloatRect& rect, const FloatRoundedRect& clipRect)
{
    if (clipRect.isRenderable()) {
        context.clipRoundedRect(clipRect);
        return;
    }

    // We create a rounded rect for each of the corners and clip it, while making sure we clip opposing corners together.
    if (!clipRect.radii().topLeft().isEmpty() || !clipRect.radii().bottomRight().isEmpty()) {
        FloatRect topCorner(clipRect.rect().x(), clipRect.rect().y(), rect.maxX() - clipRect.rect().x(), rect.maxY() - clipRect.rect().y());
        FloatRoundedRect::Radii topCornerRadii;
        topCornerRadii.setTopLeft(clipRect.radii().topLeft());
        context.clipRoundedRect(FloatRoundedRect(topCorner, topCornerRadii));

        FloatRect bottomCorner(rect.x(), rect.y(), clipRect.rect().maxX() - rect.x(), clipRect.rect().maxY() - rect.y());
        FloatRoundedRect::Radii bottomCornerRadii;
        bottomCornerRadii.setBottomRight(clipRect.radii().bottomRight());
        context.clipRoundedRect(FloatRoundedRect(bottomCorner, bottomCornerRadii));
    }

    if (!clipRect.radii().topRight().isEmpty() || !clipRect.radii().bottomLeft().isEmpty()) {
        FloatRect topCorner(rect.x(), clipRect.rect().y(), clipRect.rect().maxX() - rect.x(), rect.maxY() - clipRect.rect().y());
        FloatRoundedRect::Radii topCornerRadii;
        topCornerRadii.setTopRight(clipRect.radii().topRight());
        context.clipRoundedRect(FloatRoundedRect(topCorner, topCornerRadii));

        FloatRect bottomCorner(clipRect.rect().x(), rect.y(), rect.maxX() - clipRect.rect().x(), clipRect.rect().maxY() - rect.y());
        FloatRoundedRect::Radii bottomCornerRadii;
        bottomCornerRadii.setBottomLeft(clipRect.radii().bottomLeft());
        context.clipRoundedRect(FloatRoundedRect(bottomCorner, bottomCornerRadii));
    }
}

RoundedRect BackgroundPainter::backgroundRoundedRectAdjustedForBleedAvoidance(const LayoutRect& borderRect, BackgroundBleedAvoidance bleedAvoidance, const InlineIterator::InlineBoxIterator& box, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    if (bleedAvoidance == BackgroundBleedShrinkBackground) {
        // We shrink the rectangle by one device pixel on each side because the bleed is one pixel maximum.
        return backgroundRoundedRect(shrinkRectByOneDevicePixel(m_paintInfo.context(), borderRect, document().deviceScaleFactor()), box,
            includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    if (bleedAvoidance == BackgroundBleedBackgroundOverBorder)
        return m_renderer.style().getRoundedInnerBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    return backgroundRoundedRect(borderRect, box, includeLogicalLeftEdge, includeLogicalRightEdge);
}

RoundedRect BackgroundPainter::backgroundRoundedRect(const LayoutRect& borderRect, const InlineIterator::InlineBoxIterator& box, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    RoundedRect border = m_renderer.style().getRoundedBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);
    if (box && (box->nextInlineBox() || box->previousInlineBox())) {
        RoundedRect segmentBorder = m_renderer.style().getRoundedBorderFor(LayoutRect(0_lu, 0_lu, borderRect.width(), borderRect.height()), includeLogicalLeftEdge, includeLogicalRightEdge);
        border.setRadii(segmentBorder.radii());
    }
    return border;
}

static inline std::optional<LayoutUnit> getSpace(LayoutUnit areaSize, LayoutUnit tileSize)
{
    if (int numberOfTiles = areaSize / tileSize; numberOfTiles > 1)
        return (areaSize - numberOfTiles * tileSize) / (numberOfTiles - 1);
    return std::nullopt;
}

static LayoutUnit resolveEdgeRelativeLength(const Length& length, Edge edge, LayoutUnit availableSpace, const LayoutSize& areaSize, const LayoutSize& tileSize)
{
    LayoutUnit result = minimumValueForLength(length, availableSpace);

    if (edge == Edge::Right)
        return areaSize.width() - tileSize.width() - result;

    if (edge == Edge::Bottom)
        return areaSize.height() - tileSize.height() - result;

    return result;
}

static void pixelSnapBackgroundImageGeometryForPainting(LayoutRect& destinationRect, LayoutSize& tileSize, LayoutSize& phase, LayoutSize& space, float scaleFactor)
{
    tileSize = LayoutSize(snapRectToDevicePixels(LayoutRect(destinationRect.location(), tileSize), scaleFactor).size());
    phase = LayoutSize(snapRectToDevicePixels(LayoutRect(destinationRect.location(), phase), scaleFactor).size());
    space = LayoutSize(snapRectToDevicePixels(LayoutRect(LayoutPoint(), space), scaleFactor).size());
    destinationRect = LayoutRect(snapRectToDevicePixels(destinationRect, scaleFactor));
}

BackgroundImageGeometry BackgroundPainter::calculateBackgroundImageGeometry(const RenderBoxModelObject& renderer, const RenderLayerModelObject* paintContainer, const FillLayer& fillLayer, const LayoutPoint& paintOffset, const LayoutRect& borderBoxRect)
{
    auto& view = renderer.view();

    LayoutUnit left;
    LayoutUnit top;
    LayoutSize positioningAreaSize;
    // Determine the background positioning area and set destination rect to the background painting area.
    // Destination rect will be adjusted later if the background is non-repeating.
    // FIXME: transforms spec says that fixed backgrounds behave like scroll inside transforms. https://bugs.webkit.org/show_bug.cgi?id=15679
    LayoutRect destinationRect(borderBoxRect);
    bool fixedAttachment = fillLayer.attachment() == FillAttachment::FixedBackground;
    float deviceScaleFactor = renderer.document().deviceScaleFactor();
    if (!fixedAttachment) {
        LayoutUnit right;
        LayoutUnit bottom;
        // Scroll and Local.
        if (fillLayer.origin() != FillBox::Border) {
            left = renderer.borderLeft();
            right = renderer.borderRight();
            top = renderer.borderTop();
            bottom = renderer.borderBottom();
            if (fillLayer.origin() == FillBox::Content) {
                left += renderer.paddingLeft();
                right += renderer.paddingRight();
                top += renderer.paddingTop();
                bottom += renderer.paddingBottom();
            }
        }

        // The background of the box generated by the root element covers the entire canvas including
        // its margins. Since those were added in already, we have to factor them out when computing
        // the background positioning area.
        if (renderer.isDocumentElementRenderer()) {
            positioningAreaSize = downcast<RenderBox>(renderer).size() - LayoutSize(left + right, top + bottom);
            positioningAreaSize = LayoutSize(snapSizeToDevicePixel(positioningAreaSize, LayoutPoint(), deviceScaleFactor));
            if (view.frameView().hasExtendedBackgroundRectForPainting()) {
                LayoutRect extendedBackgroundRect = view.frameView().extendedBackgroundRectForPainting();
                left += (renderer.marginLeft() - extendedBackgroundRect.x());
                top += (renderer.marginTop() - extendedBackgroundRect.y());
            }
        } else {
            positioningAreaSize = borderBoxRect.size() - LayoutSize(left + right, top + bottom);
            positioningAreaSize = LayoutSize(snapRectToDevicePixels(LayoutRect(paintOffset, positioningAreaSize), deviceScaleFactor).size());
        }
    } else {
        LayoutRect viewportRect;
        float topContentInset = 0;
        if (renderer.settings().fixedBackgroundsPaintRelativeToDocument())
            viewportRect = view.unscaledDocumentRect();
        else {
            FrameView& frameView = view.frameView();
            bool useFixedLayout = frameView.useFixedLayout() && !frameView.fixedLayoutSize().isEmpty();

            if (useFixedLayout) {
                // Use the fixedLayoutSize() when useFixedLayout() because the rendering will scale
                // down the frameView to to fit in the current viewport.
                viewportRect.setSize(frameView.fixedLayoutSize());
            } else
                viewportRect.setSize(frameView.sizeForVisibleContent());

            if (renderer.fixedBackgroundPaintsInLocalCoordinates()) {
                if (!useFixedLayout) {
                    // Shifting location up by topContentInset is needed for layout tests which expect
                    // layout to be shifted down when calling window.internals.setTopContentInset().
                    topContentInset = frameView.topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset);
                    viewportRect.setLocation(LayoutPoint(0, -topContentInset));
                }
            } else if (useFixedLayout || frameView.frameScaleFactor() != 1) {
                // scrollPositionForFixedPosition() is adjusted for page scale and it does not include
                // topContentInset so do not add it to the calculation below.
                viewportRect.setLocation(frameView.scrollPositionForFixedPosition());
            } else {
                // documentScrollPositionRelativeToViewOrigin() includes -topContentInset in its height
                // so we need to account for that in calculating the phase size
                topContentInset = frameView.topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset);
                viewportRect.setLocation(frameView.documentScrollPositionRelativeToViewOrigin());
            }

            top += topContentInset;
        }

        if (paintContainer)
            viewportRect.moveBy(LayoutPoint(-paintContainer->localToAbsolute(FloatPoint())));

        destinationRect = viewportRect;
        positioningAreaSize = destinationRect.size();
        positioningAreaSize.setHeight(positioningAreaSize.height() - topContentInset);
        positioningAreaSize = LayoutSize(snapRectToDevicePixels(LayoutRect(destinationRect.location(), positioningAreaSize), deviceScaleFactor).size());
    }

    LayoutSize tileSize = calculateFillTileSize(renderer, fillLayer, positioningAreaSize);

    FillRepeat backgroundRepeatX = fillLayer.repeat().x;
    FillRepeat backgroundRepeatY = fillLayer.repeat().y;
    LayoutUnit availableWidth = positioningAreaSize.width() - tileSize.width();
    LayoutUnit availableHeight = positioningAreaSize.height() - tileSize.height();

    LayoutSize spaceSize;
    LayoutSize phase;
    LayoutSize noRepeat;
    LayoutUnit computedXPosition = resolveEdgeRelativeLength(fillLayer.xPosition(), fillLayer.backgroundXOrigin(), availableWidth, positioningAreaSize, tileSize);
    if (backgroundRepeatX == FillRepeat::Round && positioningAreaSize.width() > 0 && tileSize.width() > 0) {
        int numTiles = std::max(1, roundToInt(positioningAreaSize.width() / tileSize.width()));
        if (fillLayer.size().size.height.isAuto() && backgroundRepeatY != FillRepeat::Round)
            tileSize.setHeight(tileSize.height() * positioningAreaSize.width() / (numTiles * tileSize.width()));

        tileSize.setWidth(positioningAreaSize.width() / numTiles);
        phase.setWidth(tileSize.width() ? tileSize.width() - fmodf((computedXPosition + left), tileSize.width()) : 0);
    }

    LayoutUnit computedYPosition = resolveEdgeRelativeLength(fillLayer.yPosition(), fillLayer.backgroundYOrigin(), availableHeight, positioningAreaSize, tileSize);
    if (backgroundRepeatY == FillRepeat::Round && positioningAreaSize.height() > 0 && tileSize.height() > 0) {
        int numTiles = std::max(1, roundToInt(positioningAreaSize.height() / tileSize.height()));
        if (fillLayer.size().size.width.isAuto() && backgroundRepeatX != FillRepeat::Round)
            tileSize.setWidth(tileSize.width() * positioningAreaSize.height() / (numTiles * tileSize.height()));

        tileSize.setHeight(positioningAreaSize.height() / numTiles);
        phase.setHeight(tileSize.height() ? tileSize.height() - fmodf((computedYPosition + top), tileSize.height()) : 0);
    }

    if (backgroundRepeatX == FillRepeat::Repeat) {
        phase.setWidth(tileSize.width() ? tileSize.width() - fmodf(computedXPosition + left, tileSize.width()) : 0);
        spaceSize.setWidth(0);
    } else if (backgroundRepeatX == FillRepeat::Space && tileSize.width() > 0) {
        if (auto space = getSpace(positioningAreaSize.width(), tileSize.width())) {
            LayoutUnit actualWidth = tileSize.width() + *space;
            computedXPosition = minimumValueForLength(Length(), availableWidth);
            spaceSize.setWidth(*space);
            spaceSize.setHeight(0);
            phase.setWidth(actualWidth ? actualWidth - fmodf((computedXPosition + left), actualWidth) : 0);
        } else
            backgroundRepeatX = FillRepeat::NoRepeat;
    }

    if (backgroundRepeatX == FillRepeat::NoRepeat) {
        LayoutUnit xOffset = left + computedXPosition;
        if (xOffset > 0)
            destinationRect.move(xOffset, 0_lu);
        xOffset = std::min<LayoutUnit>(xOffset, 0);
        phase.setWidth(-xOffset);
        destinationRect.setWidth(tileSize.width() + xOffset);
        spaceSize.setWidth(0);
    }

    if (backgroundRepeatY == FillRepeat::Repeat) {
        phase.setHeight(tileSize.height() ? tileSize.height() - fmodf(computedYPosition + top, tileSize.height()) : 0);
        spaceSize.setHeight(0);
    } else if (backgroundRepeatY == FillRepeat::Space && tileSize.height() > 0) {
        if (auto space = getSpace(positioningAreaSize.height(), tileSize.height())) {
            LayoutUnit actualHeight = tileSize.height() + *space;
            computedYPosition = minimumValueForLength(Length(), availableHeight);
            spaceSize.setHeight(*space);
            phase.setHeight(actualHeight ? actualHeight - fmodf((computedYPosition + top), actualHeight) : 0);
        } else
            backgroundRepeatY = FillRepeat::NoRepeat;
    }
    if (backgroundRepeatY == FillRepeat::NoRepeat) {
        LayoutUnit yOffset = top + computedYPosition;
        if (yOffset > 0)
            destinationRect.move(0_lu, yOffset);
        yOffset = std::min<LayoutUnit>(yOffset, 0);
        phase.setHeight(-yOffset);
        destinationRect.setHeight(tileSize.height() + yOffset);
        spaceSize.setHeight(0);
    }

    if (fixedAttachment) {
        LayoutPoint attachmentPoint = borderBoxRect.location();
        phase.expand(std::max<LayoutUnit>(attachmentPoint.x() - destinationRect.x(), 0), std::max<LayoutUnit>(attachmentPoint.y() - destinationRect.y(), 0));
    }

    destinationRect.intersect(borderBoxRect);

    auto tileSizeWithoutPixelSnapping = tileSize;
    pixelSnapBackgroundImageGeometryForPainting(destinationRect, tileSize, phase, spaceSize, deviceScaleFactor);

    return BackgroundImageGeometry(destinationRect, tileSizeWithoutPixelSnapping, tileSize, phase, spaceSize, fixedAttachment);
}

LayoutSize BackgroundPainter::calculateFillTileSize(const RenderBoxModelObject& renderer, const FillLayer& fillLayer, const LayoutSize& positioningAreaSize)
{
    StyleImage* image = fillLayer.image();
    FillSizeType type = fillLayer.size().type;
    auto devicePixelSize = LayoutUnit { 1.0 / renderer.document().deviceScaleFactor() };

    LayoutSize imageIntrinsicSize;
    if (image) {
        imageIntrinsicSize = renderer.calculateImageIntrinsicDimensions(image, positioningAreaSize, RenderBoxModelObject::ScaleByEffectiveZoom);
        imageIntrinsicSize.scale(1 / image->imageScaleFactor(), 1 / image->imageScaleFactor());
    } else
        imageIntrinsicSize = positioningAreaSize;

    switch (type) {
    case FillSizeType::Size: {
        LayoutSize tileSize = positioningAreaSize;

        Length layerWidth = fillLayer.size().size.width;
        Length layerHeight = fillLayer.size().size.height;

        if (layerWidth.isFixed())
            tileSize.setWidth(layerWidth.value());
        else if (layerWidth.isPercentOrCalculated()) {
            auto resolvedWidth = valueForLength(layerWidth, positioningAreaSize.width());
            // Non-zero resolved value should always produce some content.
            tileSize.setWidth(!resolvedWidth ? resolvedWidth : std::max(devicePixelSize, resolvedWidth));
        }

        if (layerHeight.isFixed())
            tileSize.setHeight(layerHeight.value());
        else if (layerHeight.isPercentOrCalculated()) {
            auto resolvedHeight = valueForLength(layerHeight, positioningAreaSize.height());
            // Non-zero resolved value should always produce some content.
            tileSize.setHeight(!resolvedHeight ? resolvedHeight : std::max(devicePixelSize, resolvedHeight));
        }

        // If one of the values is auto we have to use the appropriate
        // scale to maintain our aspect ratio.
        if (layerWidth.isAuto() && !layerHeight.isAuto()) {
            if (imageIntrinsicSize.height())
                tileSize.setWidth(imageIntrinsicSize.width() * tileSize.height() / imageIntrinsicSize.height());
        } else if (!layerWidth.isAuto() && layerHeight.isAuto()) {
            if (imageIntrinsicSize.width())
                tileSize.setHeight(imageIntrinsicSize.height() * tileSize.width() / imageIntrinsicSize.width());
        } else if (layerWidth.isAuto() && layerHeight.isAuto()) {
            // If both width and height are auto, use the image's intrinsic size.
            tileSize = imageIntrinsicSize;
        }

        tileSize.clampNegativeToZero();
        return tileSize;
    }
    case FillSizeType::None: {
        // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
        if (!imageIntrinsicSize.isEmpty())
            return imageIntrinsicSize;

        // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for ‘contain’.
        type = FillSizeType::Contain;
    }
    FALLTHROUGH;
    case FillSizeType::Contain:
    case FillSizeType::Cover: {
        // Scale computation needs higher precision than what LayoutUnit can offer.
        FloatSize localImageIntrinsicSize = imageIntrinsicSize;
        FloatSize localPositioningAreaSize = positioningAreaSize;

        float horizontalScaleFactor = localImageIntrinsicSize.width() ? (localPositioningAreaSize.width() / localImageIntrinsicSize.width()) : 1;
        float verticalScaleFactor = localImageIntrinsicSize.height() ? (localPositioningAreaSize.height() / localImageIntrinsicSize.height()) : 1;
        float scaleFactor = type == FillSizeType::Contain ? std::min(horizontalScaleFactor, verticalScaleFactor) : std::max(horizontalScaleFactor, verticalScaleFactor);

        if (localImageIntrinsicSize.isEmpty())
            return { };

        return LayoutSize(localImageIntrinsicSize.scaled(scaleFactor).expandedTo({ devicePixelSize, devicePixelSize }));
    }
    }

    ASSERT_NOT_REACHED();
    return { };
}

const Document& BackgroundPainter::document() const
{
    return m_renderer.document();
}

const RenderView& BackgroundPainter::view() const
{
    return m_renderer.view();
}

}
