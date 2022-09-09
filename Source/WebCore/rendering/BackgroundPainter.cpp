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

BackgroundPainter::BackgroundPainter(RenderBoxModelObject& renderer, const PaintInfo& paintInfo)
    : m_renderer(renderer)
    , m_paintInfo(paintInfo)
{
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
        RoundedRect border = isBorderFill ? backgroundRoundedRectAdjustedForBleedAvoidance(rect, bleedAvoidance, box, includeLeftEdge, includeRightEdge) : getBackgroundRoundedRect(rect, box, includeLeftEdge, includeRightEdge);

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
        auto geometry = m_renderer.calculateBackgroundImageGeometry(m_paintInfo.paintContainer, bgLayer, paintOffset, imageRect, backgroundObject);
        geometry.clip(LayoutRect(pixelSnappedRect));
        RefPtr<Image> image;
        if (!geometry.destRect().isEmpty() && (image = bgImage->image(backgroundObject ? backgroundObject : &m_renderer, geometry.tileSize()))) {
            context.setDrawLuminanceMask(bgLayer.maskMode() == MaskMode::Luminance);

            if (is<BitmapImage>(image))
                downcast<BitmapImage>(*image).updateFromSettings(document().settings());

            ImagePaintingOptions options = {
                op == CompositeOperator::SourceOver ? bgLayer.compositeForPainting() : op,
                bgLayer.blendMode(),
                m_renderer.decodingModeForImageDraw(*image, m_paintInfo),
                ImageOrientation::FromImage,
                m_renderer.chooseInterpolationQuality(context, *image, &bgLayer, geometry.tileSize())
            };

            auto drawResult = context.drawTiledImage(*image, geometry.destRect(), toLayoutPoint(geometry.relativePhase()), geometry.tileSize(), geometry.spaceSize(), options);
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
        return getBackgroundRoundedRect(shrinkRectByOneDevicePixel(m_paintInfo.context(), borderRect, document().deviceScaleFactor()), box,
            includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    if (bleedAvoidance == BackgroundBleedBackgroundOverBorder)
        return m_renderer.style().getRoundedInnerBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    return getBackgroundRoundedRect(borderRect, box, includeLogicalLeftEdge, includeLogicalRightEdge);
}

RoundedRect BackgroundPainter::getBackgroundRoundedRect(const LayoutRect& borderRect, const InlineIterator::InlineBoxIterator& box, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    RoundedRect border = m_renderer.style().getRoundedBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);
    if (box && (box->nextInlineBox() || box->previousInlineBox())) {
        RoundedRect segmentBorder = m_renderer.style().getRoundedBorderFor(LayoutRect(0_lu, 0_lu, borderRect.width(), borderRect.height()), includeLogicalLeftEdge, includeLogicalRightEdge);
        border.setRadii(segmentBorder.radii());
    }
    return border;
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
