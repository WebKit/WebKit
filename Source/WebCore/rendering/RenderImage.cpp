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
#include "FontCache.h"
#include "FontCascade.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "GeometryUtilities.h"
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
#include "RenderFlowThread.h"
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
    IntRect absoluteBounds = localToAbsoluteQuad(FloatRect(imageRect), UseTransforms, &isFixed).enclosingBoundingBox();
    IntRect lineExtentBounds = localToAbsoluteQuad(FloatRect(lineExtentRect)).enclosingBoundingBox();
    if (!containingBlock->isHorizontalWritingMode())
        lineExtentBounds = lineExtentBounds.transposedRect();

    // FIXME: We should consider either making SelectionRect a struct or better organize its optional fields into
    // an auxiliary struct to simplify its initialization.
    rects.append(SelectionRect(absoluteBounds, containingBlock->style().direction(), lineExtentBounds.x(), lineExtentBounds.maxX(), lineExtentBounds.maxY(), 0, false /* line break */, isFirstOnLine, isLastOnLine, false /* contains start */, false /* contains end */, containingBlock->style().isHorizontalWritingMode(), isFixed, false /* ruby text */, view().pageNumberForBlockProgressionOffset(absoluteBounds.x())));
}
#endif

using namespace HTMLNames;

RenderImage::RenderImage(Element& element, Ref<RenderStyle>&& style, StyleImage* styleImage, const float imageDevicePixelRatio)
    : RenderReplaced(element, WTFMove(style), IntSize())
    , m_imageResource(styleImage ? std::make_unique<RenderImageResourceStyleImage>(*styleImage) : std::make_unique<RenderImageResource>())
    , m_needsToSetSizeForAltText(false)
    , m_didIncrementVisuallyNonEmptyPixelCount(false)
    , m_isGeneratedContent(false)
    , m_hasShadowControls(false)
    , m_imageDevicePixelRatio(imageDevicePixelRatio)
{
    updateAltText();
    imageResource().initialize(this);

    if (is<HTMLImageElement>(element))
        m_hasShadowControls = downcast<HTMLImageElement>(element).hasShadowControls();
}

RenderImage::RenderImage(Document& document, Ref<RenderStyle>&& style, StyleImage* styleImage)
    : RenderReplaced(document, WTFMove(style), IntSize())
    , m_imageResource(styleImage ? std::make_unique<RenderImageResourceStyleImage>(*styleImage) : std::make_unique<RenderImageResource>())
    , m_needsToSetSizeForAltText(false)
    , m_didIncrementVisuallyNonEmptyPixelCount(false)
    , m_isGeneratedContent(false)
    , m_hasShadowControls(false)
    , m_imageDevicePixelRatio(1.0f)
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

    FloatSize imageSize;
    if (newImage->willPaintBrokenImage()) {
        std::pair<Image*, float> brokenImageAndImageScaleFactor = newImage->brokenImage(document().deviceScaleFactor());
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
ImageSizeChangeType RenderImage::setImageSizeForAltText(CachedImage* newImage /* = 0 */)
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
        const FontCascade& font = style().fontCascade();
        IntSize paddedTextSize(paddingWidth + std::min(ceilf(font.width(RenderBlock::constructTextRun(this, font, m_altText, style()))), maxAltTextWidth), paddingHeight + std::min(font.fontMetrics().height(), maxAltTextHeight));
        imageSize = imageSize.expandedTo(paddedTextSize);
    }

    if (imageSize == intrinsicSize())
        return ImageSizeChangeNone;

    setIntrinsicSize(imageSize);
    return ImageSizeChangeForAltText;
}

void RenderImage::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    if (m_needsToSetSizeForAltText) {
        if (!m_altText.isEmpty() && setImageSizeForAltText(imageResource().cachedImage()))
            repaintOrMarkForLayout(ImageSizeChangeForAltText);
        m_needsToSetSizeForAltText = false;
    }
#if ENABLE(CSS_IMAGE_ORIENTATION)
    if (diff == StyleDifferenceLayout && oldStyle->imageOrientation() != style().imageOrientation())
        return repaintOrMarkForLayout(ImageSizeChangeNone);
#endif

#if ENABLE(CSS_IMAGE_RESOLUTION)
    if (diff == StyleDifferenceLayout
        && (oldStyle->imageResolution() != style().imageResolution()
            || oldStyle->imageResolutionSnap() != style().imageResolutionSnap()
            || oldStyle->imageResolutionSource() != style().imageResolutionSource()))
        repaintOrMarkForLayout(ImageSizeChangeNone);
#endif
}

void RenderImage::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    // FIXME (86669): Instead of the RenderImage determining whether its document is in the page
    // cache, the RenderImage should remove itself as a client when its document is put into the
    // page cache.
    if (documentBeingDestroyed() || document().inPageCache())
        return;

    if (hasBoxDecorations() || hasMask() || hasShapeOutside())
        RenderReplaced::imageChanged(newImage, rect);

    if (newImage != imageResource().imagePtr() || !newImage)
        return;
    
    if (!m_didIncrementVisuallyNonEmptyPixelCount) {
        // At a zoom level of 1 the image is guaranteed to have an integer size.
        view().frameView().incrementVisuallyNonEmptyPixelCount(flooredIntSize(imageResource().imageSize(1.0f)));
        m_didIncrementVisuallyNonEmptyPixelCount = true;
    }

    ImageSizeChangeType imageSizeChange = ImageSizeChangeNone;

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
        imageSizeChange = setImageSizeForAltText(imageResource().cachedImage());
    }

    repaintOrMarkForLayout(imageSizeChange, rect);
}

void RenderImage::updateIntrinsicSizeIfNeeded(const LayoutSize& newSize)
{
    if (imageResource().errorOccurred() || !m_imageResource->hasImage())
        return;
    setIntrinsicSize(newSize);
}

void RenderImage::updateInnerContentRect()
{
    // Propagate container size to image resource.
    IntSize containerSize(replacedContentRect(intrinsicSize()).size());
    if (!containerSize.isEmpty())
        imageResource().setContainerSizeForRenderer(containerSize);
}

void RenderImage::repaintOrMarkForLayout(ImageSizeChangeType imageSizeChange, const IntRect* rect)
{
#if ENABLE(CSS_IMAGE_RESOLUTION)
    double scale = style().imageResolution();
    if (style().imageResolutionSnap() == ImageResolutionSnapPixels)
        scale = roundForImpreciseConversion<int>(scale);
    if (scale <= 0)
        scale = 1;
    LayoutSize newIntrinsicSize = imageResource().intrinsicSize(style().effectiveZoom() / scale);
#else
    LayoutSize newIntrinsicSize = imageResource().intrinsicSize(style().effectiveZoom());
#endif
    LayoutSize oldIntrinsicSize = intrinsicSize();

    updateIntrinsicSizeIfNeeded(newIntrinsicSize);

    // In the case of generated image content using :before/:after/content, we might not be
    // in the render tree yet. In that case, we just need to update our intrinsic size.
    // layout() will be called after we are inserted in the tree which will take care of
    // what we are doing here.
    if (!containingBlock())
        return;

    bool imageSourceHasChangedSize = oldIntrinsicSize != newIntrinsicSize || imageSizeChange != ImageSizeChangeNone;

    if (imageSourceHasChangedSize) {
        setPreferredLogicalWidthsDirty(true);

        // If the actual area occupied by the image has changed and it is not constrained by style then a layout is required.
        bool imageSizeIsConstrained = style().logicalWidth().isSpecified() && style().logicalHeight().isSpecified();

        // FIXME: We only need to recompute the containing block's preferred size
        // if the containing block's size depends on the image's size (i.e., the container uses shrink-to-fit sizing).
        // There's no easy way to detect that shrink-to-fit is needed, always force a layout.
        bool containingBlockNeedsToRecomputePreferredSize =
            style().logicalWidth().isPercentOrCalculated()
            || style().logicalMaxWidth().isPercentOrCalculated()
            || style().logicalMinWidth().isPercentOrCalculated();

        bool layoutSizeDependsOnIntrinsicSize = style().aspectRatioType() == AspectRatioFromIntrinsic;

        if (!imageSizeIsConstrained || containingBlockNeedsToRecomputePreferredSize || layoutSizeDependsOnIntrinsicSize) {
            // FIXME: It's not clear that triggering a layout guarantees a repaint in all cases.
            // But many callers do depend on this code causing a layout.
            setNeedsLayout();
            return;
        }
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

    LayoutRect repaintRect = contentBoxRect();
    if (rect) {
        // The image changed rect is in source image coordinates (pre-zooming),
        // so map from the bounds of the image to the contentsBox.
        repaintRect.intersect(enclosingIntRect(mapRect(*rect, FloatRect(FloatPoint(), imageResource().imageSize(1.0f)), repaintRect)));
    }
        
    repaintRectangle(repaintRect);

    // Tell any potential compositing layers that the image needs updating.
    contentChanged(ImageChanged);
}

void RenderImage::notifyFinished(CachedResource* newImage)
{
    if (documentBeingDestroyed())
        return;

    invalidateBackgroundObscurationStatus();

    if (newImage == imageResource().cachedImage()) {
        // tell any potential compositing layers
        // that the image is done and they can reference it directly.
        contentChanged(ImageChanged);
    }
}

void RenderImage::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutUnit cWidth = contentWidth();
    LayoutUnit cHeight = contentHeight();
    LayoutUnit leftBorder = borderLeft();
    LayoutUnit topBorder = borderTop();
    LayoutUnit leftPad = paddingLeft();
    LayoutUnit topPad = paddingTop();

    GraphicsContext& context = paintInfo.context();
    float deviceScaleFactor = document().deviceScaleFactor();

    Page* page = frame().page();

    if (!imageResource().hasImage() || imageResource().errorOccurred()) {
        if (paintInfo.phase == PaintPhaseSelection)
            return;

        if (page && paintInfo.phase == PaintPhaseForeground)
            page->addRelevantUnpaintedObject(this, visualOverflowRect());

        if (cWidth > 2 && cHeight > 2) {
            LayoutUnit borderWidth = LayoutUnit(1 / deviceScaleFactor);

            // Draw an outline rect where the image should be.
            context.setStrokeStyle(SolidStroke);
            context.setStrokeColor(Color::lightGray);
            context.setFillColor(Color::transparent);
            context.drawRect(snapRectToDevicePixels(LayoutRect(paintOffset.x() + leftBorder + leftPad, paintOffset.y() + topBorder + topPad, cWidth, cHeight), deviceScaleFactor), borderWidth);

            bool errorPictureDrawn = false;
            LayoutSize imageOffset;
            // When calculating the usable dimensions, exclude the pixels of
            // the ouline rect so the error image/alt text doesn't draw on it.
            LayoutUnit usableWidth = cWidth - 2 * borderWidth;
            LayoutUnit usableHeight = cHeight - 2 * borderWidth;

            RefPtr<Image> image = imageResource().image();

            if (imageResource().errorOccurred() && !image->isNull() && usableWidth >= image->width() && usableHeight >= image->height()) {
                // Call brokenImage() explicitly to ensure we get the broken image icon at the appropriate resolution.
                std::pair<Image*, float> brokenImageAndImageScaleFactor = imageResource().cachedImage()->brokenImage(document().deviceScaleFactor());
                image = brokenImageAndImageScaleFactor.first;
                FloatSize imageSize = image->size();
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
                context.drawImage(*image, snapRectToDevicePixels(LayoutRect(paintOffset + imageOffset, imageSize), deviceScaleFactor), orientationDescription);
                errorPictureDrawn = true;
            }

            if (!m_altText.isEmpty()) {
                String text = document().displayStringModifiedByEncoding(m_altText);
                context.setFillColor(style().visitedDependentColor(CSSPropertyColor));
                const FontCascade& font = style().fontCascade();
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
                        context.drawText(font, textRun, altTextOffset);
                } else if (usableWidth >= textWidth && usableHeight >= fontMetrics.height())
                    context.drawText(font, textRun, altTextOffset);
            }
        }
    } else if (imageResource().hasImage() && cWidth > 0 && cHeight > 0) {
        RefPtr<Image> img = imageResource().image(cWidth, cHeight);
        if (!img || img->isNull()) {
            if (page && paintInfo.phase == PaintPhaseForeground)
                page->addRelevantUnpaintedObject(this, visualOverflowRect());
            return;
        }

        LayoutRect contentBoxRect = this->contentBoxRect();
        contentBoxRect.moveBy(paintOffset);
        LayoutRect replacedContentRect = this->replacedContentRect(intrinsicSize());
        replacedContentRect.moveBy(paintOffset);
        bool clip = !contentBoxRect.contains(replacedContentRect);
        GraphicsContextStateSaver stateSaver(context, clip);
        if (clip)
            context.clip(contentBoxRect);

        paintIntoRect(context, snapRectToDevicePixels(replacedContentRect, deviceScaleFactor));
        
        if (cachedImage() && page && paintInfo.phase == PaintPhaseForeground) {
            // For now, count images as unpainted if they are still progressively loading. We may want 
            // to refine this in the future to account for the portion of the image that has painted.
            LayoutRect visibleRect = intersection(replacedContentRect, contentBoxRect);
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
    
    if (paintInfo.context().paintingDisabled() && !paintInfo.context().updatingControlTints())
        return;

    Element* focusedElement = document().focusedElement();
    if (!is<HTMLAreaElement>(focusedElement))
        return;

    HTMLAreaElement& areaElement = downcast<HTMLAreaElement>(*focusedElement);
    if (areaElement.imageElement() != element())
        return;

    // Even if the theme handles focus ring drawing for entire elements, it won't do it for
    // an area within an image, so we don't call RenderTheme::supportsFocusRing here.

    Path path = areaElement.computePath(this);
    if (path.isEmpty())
        return;

    // FIXME: Do we need additional code to clip the path to the image's bounding box?

    RenderStyle* areaElementStyle = areaElement.computedStyle();
    float outlineWidth = areaElementStyle->outlineWidth();
    if (!outlineWidth)
        return;

    paintInfo.context().drawFocusRing(path, outlineWidth,
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

void RenderImage::paintIntoRect(GraphicsContext& context, const FloatRect& rect)
{
    if (!imageResource().hasImage() || imageResource().errorOccurred() || rect.width() <= 0 || rect.height() <= 0)
        return;

    RefPtr<Image> img = imageResource().image(rect.width(), rect.height());
    if (!img || img->isNull())
        return;

    HTMLImageElement* imageElement = is<HTMLImageElement>(element()) ? downcast<HTMLImageElement>(element()) : nullptr;
    CompositeOperator compositeOperator = imageElement ? imageElement->compositeOperator() : CompositeSourceOver;

    // FIXME: Document when image != img.get().
    Image* image = imageResource().image().get();
    InterpolationQuality interpolation = image ? chooseInterpolationQuality(context, *image, image, LayoutSize(rect.size())) : InterpolationDefault;

    ImageOrientationDescription orientationDescription(shouldRespectImageOrientation(), style().imageOrientation());
    context.drawImage(*img, rect, ImagePaintingOptions(compositeOperator, BlendModeNormal, orientationDescription, interpolation));
}

bool RenderImage::boxShadowShouldBeAppliedToBackground(const LayoutPoint& paintOffset, BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox*) const
{
    if (!RenderBoxModelObject::boxShadowShouldBeAppliedToBackground(paintOffset, bleedAvoidance))
        return false;

    return !const_cast<RenderImage*>(this)->backgroundIsKnownToBeObscured(paintOffset);
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

bool RenderImage::computeBackgroundIsKnownToBeObscured(const LayoutPoint& paintOffset)
{
    if (!hasBackground())
        return false;
    
    LayoutRect paintedExtent;
    if (!getBackgroundPaintedExtent(paintOffset, paintedExtent))
        return false;
    return foregroundIsKnownToBeOpaqueInRect(paintedExtent, 0);
}

LayoutUnit RenderImage::minimumReplacedHeight() const
{
    return imageResource().errorOccurred() ? intrinsicSize().height() : LayoutUnit();
}

HTMLMapElement* RenderImage::imageMap() const
{
    HTMLImageElement* image = is<HTMLImageElement>(element()) ? downcast<HTMLImageElement>(element()) : nullptr;
    return image ? image->treeScope().getImageMap(image->fastGetAttribute(usemapAttr)) : nullptr;
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

    if (is<HTMLInputElement>(*element()))
        m_altText = downcast<HTMLInputElement>(*element()).altText();
    else if (is<HTMLImageElement>(*element()))
        m_altText = downcast<HTMLImageElement>(*element()).altText();
}

bool RenderImage::canHaveChildren() const
{
#if !ENABLE(SERVICE_CONTROLS)
    return false;
#else
    return m_hasShadowControls;
#endif
}

void RenderImage::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    LayoutSize oldSize = contentBoxRect().size();
    RenderReplaced::layout();

    updateInnerContentRect();

    if (m_hasShadowControls)
        layoutShadowControls(oldSize);
}

void RenderImage::layoutShadowControls(const LayoutSize& oldSize)
{
    auto* controlsRenderer = downcast<RenderBox>(firstChild());
    if (!controlsRenderer)
        return;
    
    bool controlsNeedLayout = controlsRenderer->needsLayout();
    // If the region chain has changed we also need to relayout the controls to update the region box info.
    // FIXME: We can do better once we compute region box info for RenderReplaced, not only for RenderBlock.
    const RenderFlowThread* flowThread = flowThreadContainingBlock();
    if (flowThread && !controlsNeedLayout) {
        if (flowThread->pageLogicalSizeChanged())
            controlsNeedLayout = true;
    }

    LayoutSize newSize = contentBoxRect().size();
    if (newSize == oldSize && !controlsNeedLayout)
        return;

    // When calling layout() on a child node, a parent must either push a LayoutStateMaintainter, or 
    // instantiate LayoutStateDisabler. Since using a LayoutStateMaintainer is slightly more efficient,
    // and this method might be called many times per second during video playback, use a LayoutStateMaintainer:
    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

    if (shadowControlsNeedCustomLayoutMetrics()) {
        controlsRenderer->setLocation(LayoutPoint(borderLeft(), borderTop()) + LayoutSize(paddingLeft(), paddingTop()));
        controlsRenderer->style().setHeight(Length(newSize.height(), Fixed));
        controlsRenderer->style().setWidth(Length(newSize.width(), Fixed));
    }

    controlsRenderer->setNeedsLayout(MarkOnlyThis);
    controlsRenderer->layout();
    clearChildNeedsLayout();

    statePusher.pop();
}

void RenderImage::computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const
{
    RenderReplaced::computeIntrinsicRatioInformation(intrinsicSize, intrinsicRatio);

    // Our intrinsicSize is empty if we're rendering generated images with relative width/height. Figure out the right intrinsic size to use.
    if (intrinsicSize.isEmpty() && (imageResource().imageHasRelativeWidth() || imageResource().imageHasRelativeHeight())) {
        RenderObject* containingBlock = isOutOfFlowPositioned() ? container() : this->containingBlock();
        if (is<RenderBox>(*containingBlock)) {
            auto& box = downcast<RenderBox>(*containingBlock);
            intrinsicSize.setWidth(box.availableLogicalWidth());
            intrinsicSize.setHeight(box.availableLogicalHeight(IncludeMarginBorderPadding));
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
    CachedImage* cachedImage = imageResource().cachedImage();
    if (cachedImage && is<SVGImage>(cachedImage->image()))
        return downcast<SVGImage>(*cachedImage->image()).embeddedContentBox();

    return nullptr;
}

} // namespace WebCore
