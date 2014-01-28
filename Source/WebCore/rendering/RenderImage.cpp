/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
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
#include "RenderImage.h"

#include "BitmapImage.h"
#include "CachedImage.h"
#include "Font.h"
#include "FontCache.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "InlineElementBox.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderView.h"
#include "SVGImage.h"
#include <wtf/StackStats.h>

#if PLATFORM(IOS)
#include "LogicalSelectionOffsetCaches.h"
#include "SelectionRect.h"
#endif

namespace WebCore {

#if PLATFORM(IOS)
// FIXME: This doesn't behave correctly for floating or positioned images, but WebCore doesn't handle those well
// during selection creation yet anyway.
// FIXME: We can't tell whether or not we contain the start or end of the selected Range using only the offsets
// of the start and end, we need to know the whole Position.
void RenderImage::collectSelectionRects(Vector<SelectionRect>& rects, unsigned, unsigned)
{
    RenderBlock* containingBlock = this->containingBlock();

    IntRect imageRect;
    // FIXME: It doesn't make sense to package line bounds into SelectionRects. We should find
    // the right and left extent of the selection once for the entire selected Range, perhaps
    // using the Range's common ancestor.
    IntRect lineExtentRect;
    bool isFirstOnLine = false;
    bool isLastOnLine = false;

    InlineBox* inlineBox = inlineBoxWrapper();
    if (!inlineBox) {
        // This is a block image.
        imageRect = IntRect(0, 0, width(), height());
        isFirstOnLine = true;
        isLastOnLine = true;
        lineExtentRect = imageRect;
        if (containingBlock->isHorizontalWritingMode()) {
            lineExtentRect.setX(containingBlock->x());
            lineExtentRect.setWidth(containingBlock->width());
        } else {
            lineExtentRect.setY(containingBlock->y());
            lineExtentRect.setHeight(containingBlock->height());
        }
    } else {
        LayoutUnit selectionTop = !containingBlock->style().isFlippedBlocksWritingMode() ? inlineBox->root().selectionTop() - logicalTop() : logicalBottom() - inlineBox->root().selectionBottom();
        imageRect = IntRect(0,  selectionTop, logicalWidth(), inlineBox->root().selectionHeight());
        isFirstOnLine = !inlineBox->previousOnLineExists();
        isLastOnLine = !inlineBox->nextOnLineExists();
        LogicalSelectionOffsetCaches cache(*containingBlock);
        LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, inlineBox->logicalTop(), cache);
        LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, inlineBox->logicalTop(), cache);
        lineExtentRect = IntRect(leftOffset - logicalLeft(), imageRect.y(), rightOffset - leftOffset, imageRect.height());
        if (!inlineBox->isHorizontal()) {
            imageRect = imageRect.transposedRect();
            lineExtentRect = lineExtentRect.transposedRect();
        }
    }

    bool isFixed = false;
    IntRect absoluteBounds = localToAbsoluteQuad(FloatRect(imageRect), false, &isFixed).enclosingBoundingBox();
    IntRect lineExtentBounds = localToAbsoluteQuad(FloatRect(lineExtentRect)).enclosingBoundingBox();
    if (!containingBlock->isHorizontalWritingMode())
        lineExtentBounds = lineExtentBounds.transposedRect();

    // FIXME: We should consider either making SelectionRect a struct or better organize its optional fields into
    // an auxiliary struct to simplify its initialization.
    rects.append(SelectionRect(absoluteBounds, containingBlock->style().direction(), lineExtentBounds.x(), lineExtentBounds.maxX(), lineExtentBounds.maxY(), 0, false /* line break */, isFirstOnLine, isLastOnLine, false /* contains start */, false /* contains end */, containingBlock->style().isHorizontalWritingMode(), isFixed, false /* ruby text */, columnNumberForOffset(absoluteBounds.x())));
}
#endif

using namespace HTMLNames;

RenderImage::RenderImage(Element& element, PassRef<RenderStyle> style, StyleImage* styleImage)
    : RenderReplaced(element, std::move(style), IntSize())
    , m_imageResource(styleImage ? std::make_unique<RenderImageResourceStyleImage>(*styleImage) : std::make_unique<RenderImageResource>())
    , m_needsToSetSizeForAltText(false)
    , m_didIncrementVisuallyNonEmptyPixelCount(false)
    , m_isGeneratedContent(false)
{
    updateAltText();
    imageResource().initialize(this);
}

RenderImage::RenderImage(Document& document, PassRef<RenderStyle> style, StyleImage* styleImage)
    : RenderReplaced(document, std::move(style), IntSize())
    , m_imageResource(styleImage ? std::make_unique<RenderImageResourceStyleImage>(*styleImage) : std::make_unique<RenderImageResource>())
    , m_needsToSetSizeForAltText(false)
    , m_didIncrementVisuallyNonEmptyPixelCount(false)
    , m_isGeneratedContent(false)
{
    imageResource().initialize(this);
}

RenderImage::~RenderImage()
{
    imageResource().shutdown();
}

// If we'll be displaying either alt text or an image, add some padding.
static const unsigned short paddingWidth = 4;
static const unsigned short paddingHeight = 4;

// Alt text is restricted to this maximum size, in pixels.  These are
// signed integers because they are compared with other signed values.
static const float maxAltTextWidth = 1024;
static const int maxAltTextHeight = 256;

IntSize RenderImage::imageSizeForError(CachedImage* newImage) const
{
    ASSERT_ARG(newImage, newImage);
    ASSERT_ARG(newImage, newImage->imageForRenderer(this));

    IntSize imageSize;
    if (newImage->willPaintBrokenImage()) {
        float deviceScaleFactor = WebCore::deviceScaleFactor(&frame());
        std::pair<Image*, float> brokenImageAndImageScaleFactor = newImage->brokenImage(deviceScaleFactor);
        imageSize = brokenImageAndImageScaleFactor.first->size();
        imageSize.scale(1 / brokenImageAndImageScaleFactor.second);
    } else
        imageSize = newImage->imageForRenderer(this)->size();

    // imageSize() returns 0 for the error image. We need the true size of the
    // error image, so we have to get it by grabbing image() directly.
    return IntSize(paddingWidth + imageSize.width() * style().effectiveZoom(), paddingHeight + imageSize.height() * style().effectiveZoom());
}

// Sets the image height and width to fit the alt text.  Returns true if the
// image size changed.
bool RenderImage::setImageSizeForAltText(CachedImage* newImage /* = 0 */)
{
    IntSize imageSize;
    if (newImage && newImage->imageForRenderer(this))
        imageSize = imageSizeForError(newImage);
    else if (!m_altText.isEmpty() || newImage) {
        // If we'll be displaying either text or an image, add a little padding.
        imageSize = IntSize(paddingWidth, paddingHeight);
    }

    // we have an alt and the user meant it (its not a text we invented)
    if (!m_altText.isEmpty()) {
        FontCachePurgePreventer fontCachePurgePreventer;

        const Font& font = style().font();
        IntSize paddedTextSize(paddingWidth + std::min(ceilf(font.width(RenderBlock::constructTextRun(this, font, m_altText, style()))), maxAltTextWidth), paddingHeight + std::min(font.fontMetrics().height(), maxAltTextHeight));
        imageSize = imageSize.expandedTo(paddedTextSize);
    }

    if (imageSize == intrinsicSize())
        return false;

    setIntrinsicSize(imageSize);
    return true;
}

void RenderImage::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    if (m_needsToSetSizeForAltText) {
        if (!m_altText.isEmpty() && setImageSizeForAltText(imageResource().cachedImage()))
            imageDimensionsChanged(true /* imageSizeChanged */);
        m_needsToSetSizeForAltText = false;
    }
#if ENABLE(CSS_IMAGE_ORIENTATION)
    if (diff == StyleDifferenceLayout && oldStyle->imageOrientation() != style().imageOrientation())
        return imageDimensionsChanged(true /* imageSizeChanged */);
#endif

#if ENABLE(CSS_IMAGE_RESOLUTION)
    if (diff == StyleDifferenceLayout
        && (oldStyle->imageResolution() != style().imageResolution()
            || oldStyle->imageResolutionSnap() != style().imageResolutionSnap()
            || oldStyle->imageResolutionSource() != style().imageResolutionSource()))
        imageDimensionsChanged(true /* imageSizeChanged */);
#endif
}

void RenderImage::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    // FIXME (86669): Instead of the RenderImage determining whether its document is in the page
    // cache, the RenderImage should remove itself as a client when its document is put into the
    // page cache.
    if (documentBeingDestroyed() || document().inPageCache())
        return;

    if (hasBoxDecorations() || hasMask())
        RenderReplaced::imageChanged(newImage, rect);

    if (newImage != imageResource().imagePtr() || !newImage)
        return;
    
    if (!m_didIncrementVisuallyNonEmptyPixelCount) {
        // At a zoom level of 1 the image is guaranteed to have an integer size.
        view().frameView().incrementVisuallyNonEmptyPixelCount(flooredIntSize(imageResource().imageSize(1.0f)));
        m_didIncrementVisuallyNonEmptyPixelCount = true;
    }

    bool imageSizeChanged = false;

    // Set image dimensions, taking into account the size of the alt text.
    if (imageResource().errorOccurred()) {
        if (!m_altText.isEmpty() && document().hasPendingStyleRecalc()) {
            ASSERT(element());
            if (element()) {
                m_needsToSetSizeForAltText = true;
                element()->setNeedsStyleRecalc(SyntheticStyleChange);
            }
            return;
        }
        imageSizeChanged = setImageSizeForAltText(imageResource().cachedImage());
    }

    imageDimensionsChanged(imageSizeChanged, rect);
}

bool RenderImage::updateIntrinsicSizeIfNeeded(const LayoutSize& newSize, bool imageSizeChanged)
{
    if (newSize == intrinsicSize() && !imageSizeChanged)
        return false;
    if (imageResource().errorOccurred())
        return imageSizeChanged;
    setIntrinsicSize(newSize);
    return true;
}

void RenderImage::updateInnerContentRect()
{
    // Propagate container size to image resource.
    LayoutRect paintRect = replacedContentRect(intrinsicSize());
    IntSize containerSize(paintRect.width(), paintRect.height());
    if (!containerSize.isEmpty())
        imageResource().setContainerSizeForRenderer(containerSize);
}

void RenderImage::imageDimensionsChanged(bool imageSizeChanged, const IntRect* rect)
{
#if ENABLE(CSS_IMAGE_RESOLUTION)
    double scale = style().imageResolution();
    if (style().imageResolutionSnap() == ImageResolutionSnapPixels)
        scale = roundForImpreciseConversion<int>(scale);
    if (scale <= 0)
        scale = 1;
    bool intrinsicSizeChanged = updateIntrinsicSizeIfNeeded(imageResource().intrinsicSize(style().effectiveZoom() / scale), imageSizeChanged);
#else
    bool intrinsicSizeChanged = updateIntrinsicSizeIfNeeded(imageResource().intrinsicSize(style().effectiveZoom()), imageSizeChanged);
#endif

    // In the case of generated image content using :before/:after/content, we might not be
    // in the render tree yet. In that case, we just need to update our intrinsic size.
    // layout() will be called after we are inserted in the tree which will take care of
    // what we are doing here.
    if (!containingBlock())
        return;

    bool shouldRepaint = true;
    if (intrinsicSizeChanged) {
        if (!preferredLogicalWidthsDirty())
            setPreferredLogicalWidthsDirty(true);

        bool hasOverrideSize = hasOverrideHeight() || hasOverrideWidth();
        if (!hasOverrideSize && !imageSizeChanged) {
            LogicalExtentComputedValues computedValues;
            computeLogicalWidthInRegion(computedValues);
            LayoutUnit newWidth = computedValues.m_extent;
            computeLogicalHeight(height(), 0, computedValues);
            LayoutUnit newHeight = computedValues.m_extent;

            imageSizeChanged = width() != newWidth || height() != newHeight;
        }

        // FIXME: We only need to recompute the containing block's preferred size
        // if the containing block's size depends on the image's size (i.e., the container uses shrink-to-fit sizing).
        // There's no easy way to detect that shrink-to-fit is needed, always force a layout.
        bool containingBlockNeedsToRecomputePreferredSize =
            style().logicalWidth().isPercent()
            || style().logicalMaxWidth().isPercent()
            || style().logicalMinWidth().isPercent();

        if (imageSizeChanged || hasOverrideSize || containingBlockNeedsToRecomputePreferredSize) {
            shouldRepaint = false;
            if (!selfNeedsLayout())
                setNeedsLayout();
        }

        if (everHadLayout() && !selfNeedsLayout()) {
            // The inner content rectangle is calculated during layout, but may need an update now
            // (unless the box has already been scheduled for layout). In order to calculate it, we
            // may need values from the containing block, though, so make sure that we're not too
            // early. It may be that layout hasn't even taken place once yet.

            // FIXME: we should not have to trigger another call to setContainerSizeForRenderer()
            // from here, since it's already being done during layout.
            updateInnerContentRect();
        }
    }

    if (shouldRepaint) {
        LayoutRect repaintRect;
        if (rect) {
            // The image changed rect is in source image coordinates (pre-zooming),
            // so map from the bounds of the image to the contentsBox.
            repaintRect = enclosingIntRect(mapRect(*rect, FloatRect(FloatPoint(), imageResource().imageSize(1.0f)), contentBoxRect()));
            // Guard against too-large changed rects.
            repaintRect.intersect(contentBoxRect());
        } else
            repaintRect = contentBoxRect();
        
        repaintRectangle(repaintRect);

#if USE(ACCELERATED_COMPOSITING)
        // Tell any potential compositing layers that the image needs updating.
        contentChanged(ImageChanged);
#endif
    }
}

void RenderImage::notifyFinished(CachedResource* newImage)
{
    if (documentBeingDestroyed())
        return;

    invalidateBackgroundObscurationStatus();

#if USE(ACCELERATED_COMPOSITING)
    if (newImage == imageResource().cachedImage()) {
        // tell any potential compositing layers
        // that the image is done and they can reference it directly.
        contentChanged(ImageChanged);
    }
#else
    UNUSED_PARAM(newImage);
#endif
}

void RenderImage::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutUnit cWidth = contentWidth();
    LayoutUnit cHeight = contentHeight();
    LayoutUnit leftBorder = borderLeft();
    LayoutUnit topBorder = borderTop();
    LayoutUnit leftPad = paddingLeft();
    LayoutUnit topPad = paddingTop();

    GraphicsContext* context = paintInfo.context;

    Page* page = frame().page();

    if (!imageResource().hasImage() || imageResource().errorOccurred()) {
        if (paintInfo.phase == PaintPhaseSelection)
            return;

        if (page && paintInfo.phase == PaintPhaseForeground)
            page->addRelevantUnpaintedObject(this, visualOverflowRect());

        if (cWidth > 2 && cHeight > 2) {
            const int borderWidth = 1;

            // Draw an outline rect where the image should be.
            context->setStrokeStyle(SolidStroke);
            context->setStrokeColor(Color::lightGray, style().colorSpace());
            context->setFillColor(Color::transparent, style().colorSpace());
            context->drawRect(pixelSnappedIntRect(LayoutRect(paintOffset.x() + leftBorder + leftPad, paintOffset.y() + topBorder + topPad, cWidth, cHeight)));

            bool errorPictureDrawn = false;
            LayoutSize imageOffset;
            // When calculating the usable dimensions, exclude the pixels of
            // the ouline rect so the error image/alt text doesn't draw on it.
            LayoutUnit usableWidth = cWidth - 2 * borderWidth;
            LayoutUnit usableHeight = cHeight - 2 * borderWidth;

            RefPtr<Image> image = imageResource().image();

            if (imageResource().errorOccurred() && !image->isNull() && usableWidth >= image->width() && usableHeight >= image->height()) {
                float deviceScaleFactor = WebCore::deviceScaleFactor(&frame());
                // Call brokenImage() explicitly to ensure we get the broken image icon at the appropriate resolution.
                std::pair<Image*, float> brokenImageAndImageScaleFactor = imageResource().cachedImage()->brokenImage(deviceScaleFactor);
                image = brokenImageAndImageScaleFactor.first;
                IntSize imageSize = image->size();
                imageSize.scale(1 / brokenImageAndImageScaleFactor.second);
                // Center the error image, accounting for border and padding.
                LayoutUnit centerX = (usableWidth - imageSize.width()) / 2;
                if (centerX < 0)
                    centerX = 0;
                LayoutUnit centerY = (usableHeight - imageSize.height()) / 2;
                if (centerY < 0)
                    centerY = 0;
                imageOffset = LayoutSize(leftBorder + leftPad + centerX + borderWidth, topBorder + topPad + centerY + borderWidth);

                ImageOrientationDescription orientationDescription(shouldRespectImageOrientation());
#if ENABLE(CSS_IMAGE_ORIENTATION)
                orientationDescription.setImageOrientationEnum(style().imageOrientation());
#endif
                context->drawImage(image.get(), style().colorSpace(), pixelSnappedIntRect(LayoutRect(paintOffset + imageOffset, imageSize)), CompositeSourceOver, orientationDescription);
                errorPictureDrawn = true;
            }

            if (!m_altText.isEmpty()) {
                String text = document().displayStringModifiedByEncoding(m_altText);
                context->setFillColor(style().visitedDependentColor(CSSPropertyColor), style().colorSpace());
                const Font& font = style().font();
                const FontMetrics& fontMetrics = font.fontMetrics();
                LayoutUnit ascent = fontMetrics.ascent();
                LayoutPoint altTextOffset = paintOffset;
                altTextOffset.move(leftBorder + leftPad + (paddingWidth / 2) - borderWidth, topBorder + topPad + ascent + (paddingHeight / 2) - borderWidth);

                // Only draw the alt text if it'll fit within the content box,
                // and only if it fits above the error image.
                TextRun textRun = RenderBlock::constructTextRun(this, font, text, style());
                LayoutUnit textWidth = font.width(textRun);
                if (errorPictureDrawn) {
                    if (usableWidth >= textWidth && fontMetrics.height() <= imageOffset.height())
                        context->drawText(font, textRun, altTextOffset);
                } else if (usableWidth >= textWidth && usableHeight >= fontMetrics.height())
                    context->drawText(font, textRun, altTextOffset);
            }
        }
    } else if (imageResource().hasImage() && cWidth > 0 && cHeight > 0) {
        RefPtr<Image> img = imageResource().image(cWidth, cHeight);
        if (!img || img->isNull()) {
            if (page && paintInfo.phase == PaintPhaseForeground)
                page->addRelevantUnpaintedObject(this, visualOverflowRect());
            return;
        }

#if PLATFORM(MAC)
        if (style().highlight() != nullAtom && !paintInfo.context->paintingDisabled())
            paintCustomHighlight(toPoint(paintOffset - location()), style().highlight(), true);
#endif

        LayoutRect contentRect = contentBoxRect();
        contentRect.moveBy(paintOffset);
        LayoutRect paintRect = replacedContentRect(intrinsicSize());
        paintRect.moveBy(paintOffset);
        bool clip = !contentRect.contains(paintRect);
        GraphicsContextStateSaver stateSaver(*context, clip);
        if (clip)
            context->clip(contentRect);

        paintIntoRect(context, paintRect);
        
        if (cachedImage() && page && paintInfo.phase == PaintPhaseForeground) {
            // For now, count images as unpainted if they are still progressively loading. We may want 
            // to refine this in the future to account for the portion of the image that has painted.
            LayoutRect visibleRect = intersection(paintRect, contentRect);
            if (cachedImage()->isLoading())
                page->addRelevantUnpaintedObject(this, visibleRect);
            else
                page->addRelevantRepaintedObject(this, visibleRect);
        }
    }
}

void RenderImage::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    RenderReplaced::paint(paintInfo, paintOffset);
    
    if (paintInfo.phase == PaintPhaseOutline)
        paintAreaElementFocusRing(paintInfo);
}
    
void RenderImage::paintAreaElementFocusRing(PaintInfo& paintInfo)
{
#if PLATFORM(IOS)
    UNUSED_PARAM(paintInfo);
#else
    if (document().printing() || !frame().selection().isFocusedAndActive())
        return;
    
    if (paintInfo.context->paintingDisabled() && !paintInfo.context->updatingControlTints())
        return;

    Element* focusedElement = document().focusedElement();
    if (!focusedElement || !isHTMLAreaElement(focusedElement))
        return;

    HTMLAreaElement* areaElement = toHTMLAreaElement(focusedElement);
    if (areaElement->imageElement() != element())
        return;

    // Even if the theme handles focus ring drawing for entire elements, it won't do it for
    // an area within an image, so we don't call RenderTheme::supportsFocusRing here.

    Path path = areaElement->computePath(this);
    if (path.isEmpty())
        return;

    // FIXME: Do we need additional code to clip the path to the image's bounding box?

    RenderStyle* areaElementStyle = areaElement->computedStyle();
    unsigned short outlineWidth = areaElementStyle->outlineWidth();
    if (!outlineWidth)
        return;

    paintInfo.context->drawFocusRing(path, outlineWidth,
        areaElementStyle->outlineOffset(),
        areaElementStyle->visitedDependentColor(CSSPropertyOutlineColor));
#endif
}

void RenderImage::areaElementFocusChanged(HTMLAreaElement* element)
{
    ASSERT_UNUSED(element, element->imageElement() == this->element());

    // It would be more efficient to only repaint the focus ring rectangle
    // for the passed-in area element. That would require adding functions
    // to the area element class.
    repaint();
}

void RenderImage::paintIntoRect(GraphicsContext* context, const LayoutRect& rect)
{
    IntRect alignedRect = pixelSnappedIntRect(rect);
    if (!imageResource().hasImage() || imageResource().errorOccurred() || alignedRect.width() <= 0 || alignedRect.height() <= 0)
        return;

    RefPtr<Image> img = imageResource().image(alignedRect.width(), alignedRect.height());
    if (!img || img->isNull())
        return;

    HTMLImageElement* imageElt = (element() && isHTMLImageElement(element())) ? toHTMLImageElement(element()) : 0;
    CompositeOperator compositeOperator = imageElt ? imageElt->compositeOperator() : CompositeSourceOver;
    Image* image = imageResource().image().get();
    bool useLowQualityScaling = shouldPaintAtLowQuality(context, image, image, alignedRect.size());
    ImageOrientationDescription orientationDescription(shouldRespectImageOrientation());
#if ENABLE(CSS_IMAGE_ORIENTATION)
    orientationDescription.setImageOrientationEnum(style().imageOrientation());
#endif
    context->drawImage(imageResource().image(alignedRect.width(), alignedRect.height()).get(), style().colorSpace(), alignedRect, compositeOperator, orientationDescription, useLowQualityScaling);
}

bool RenderImage::boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox*) const
{
    if (!RenderBoxModelObject::boxShadowShouldBeAppliedToBackground(bleedAvoidance))
        return false;

    return !const_cast<RenderImage*>(this)->backgroundIsKnownToBeObscured();
}

bool RenderImage::foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const
{
    UNUSED_PARAM(maxDepthToTest);
    if (!imageResource().hasImage() || imageResource().errorOccurred())
        return false;
    if (imageResource().cachedImage() && !imageResource().cachedImage()->isLoaded())
        return false;
    if (!contentBoxRect().contains(localRect))
        return false;
    EFillBox backgroundClip = style().backgroundClip();
    // Background paints under borders.
    if (backgroundClip == BorderFillBox && style().hasBorder() && !borderObscuresBackground())
        return false;
    // Background shows in padding area.
    if ((backgroundClip == BorderFillBox || backgroundClip == PaddingFillBox) && style().hasPadding())
        return false;
    // Object-fit may leave parts of the content box empty.
    ObjectFit objectFit = style().objectFit();
    if (objectFit != ObjectFitFill && objectFit != ObjectFitCover)
        return false;
    // Check for image with alpha.
    return imageResource().cachedImage() && imageResource().cachedImage()->currentFrameKnownToBeOpaque(this);
}

bool RenderImage::computeBackgroundIsKnownToBeObscured()
{
    if (!hasBackground())
        return false;
    
    LayoutRect paintedExtent;
    if (!getBackgroundPaintedExtent(paintedExtent))
        return false;
    return foregroundIsKnownToBeOpaqueInRect(paintedExtent, 0);
}

LayoutUnit RenderImage::minimumReplacedHeight() const
{
    return imageResource().errorOccurred() ? intrinsicSize().height() : LayoutUnit();
}

HTMLMapElement* RenderImage::imageMap() const
{
    HTMLImageElement* i = element() && isHTMLImageElement(element()) ? toHTMLImageElement(element()) : 0;
    return i ? i->treeScope().getImageMap(i->fastGetAttribute(usemapAttr)) : 0;
}

bool RenderImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    HitTestResult tempResult(result.hitTestLocation());
    bool inside = RenderReplaced::nodeAtPoint(request, tempResult, locationInContainer, accumulatedOffset, hitTestAction);

    if (tempResult.innerNode() && element()) {
        if (HTMLMapElement* map = imageMap()) {
            LayoutRect contentBox = contentBoxRect();
            float scaleFactor = 1 / style().effectiveZoom();
            LayoutPoint mapLocation = locationInContainer.point() - toLayoutSize(accumulatedOffset) - locationOffset() - toLayoutSize(contentBox.location());
            mapLocation.scale(scaleFactor, scaleFactor);

            if (map->mapMouseEvent(mapLocation, contentBox.size(), tempResult))
                tempResult.setInnerNonSharedNode(element());
        }
    }

    if (!inside && result.isRectBasedTest())
        result.append(tempResult);
    if (inside)
        result = tempResult;
    return inside;
}

void RenderImage::updateAltText()
{
    if (!element())
        return;

    if (isHTMLInputElement(element()))
        m_altText = toHTMLInputElement(element())->altText();
    else if (isHTMLImageElement(element()))
        m_altText = toHTMLImageElement(element())->altText();
}

void RenderImage::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    RenderReplaced::layout();
    updateInnerContentRect();
}

void RenderImage::computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio, bool& isPercentageIntrinsicSize) const
{
    RenderReplaced::computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio, isPercentageIntrinsicSize);

    // Our intrinsicSize is empty if we're rendering generated images with relative width/height. Figure out the right intrinsic size to use.
    if (intrinsicSize.isEmpty() && (imageResource().imageHasRelativeWidth() || imageResource().imageHasRelativeHeight())) {
        RenderObject* containingBlock = isOutOfFlowPositioned() ? container() : this->containingBlock();
        if (containingBlock->isBox()) {
            RenderBox* box = toRenderBox(containingBlock);
            intrinsicSize.setWidth(box->availableLogicalWidth());
            intrinsicSize.setHeight(box->availableLogicalHeight(IncludeMarginBorderPadding));
        }
    }
    // Don't compute an intrinsic ratio to preserve historical WebKit behavior if we're painting alt text and/or a broken image.
    if (imageResource().errorOccurred()) {
        intrinsicRatio = 1;
        return;
    }
}

bool RenderImage::needsPreferredWidthsRecalculation() const
{
    if (RenderReplaced::needsPreferredWidthsRecalculation())
        return true;
    return embeddedContentBox();
}

RenderBox* RenderImage::embeddedContentBox() const
{
#if ENABLE(SVG)
    CachedImage* cachedImage = imageResource().cachedImage();
    if (cachedImage && cachedImage->image() && cachedImage->image()->isSVGImage())
        return static_cast<SVGImage*>(cachedImage->image())->embeddedContentBox();
#endif

    return 0;
}

} // namespace WebCore
