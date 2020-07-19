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

#include "AXObjectCache.h"
#include "BitmapImage.h"
#include "CachedImage.h"
#include "FocusController.h"
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
#include "RenderFragmentedFlow.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderLayoutState.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGImage.h"
#include "Settings.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

#if PLATFORM(IOS_FAMILY)
#include "LogicalSelectionOffsetCaches.h"
#include "SelectionRect.h"
#endif

#if USE(CG)
#include "PDFDocumentImage.h"
#include "Settings.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderImage);

#if PLATFORM(IOS_FAMILY)
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
        LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, LayoutUnit(inlineBox->logicalTop()), cache);
        LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, LayoutUnit(inlineBox->logicalTop()), cache);
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

RenderImage::RenderImage(Element& element, RenderStyle&& style, StyleImage* styleImage, const float imageDevicePixelRatio)
    : RenderReplaced(element, WTFMove(style), IntSize())
    , m_imageResource(styleImage ? makeUnique<RenderImageResourceStyleImage>(*styleImage) : makeUnique<RenderImageResource>())
    , m_imageDevicePixelRatio(imageDevicePixelRatio)
{
    updateAltText();
    if (is<HTMLImageElement>(element))
        m_hasShadowControls = downcast<HTMLImageElement>(element).hasShadowControls();
}

RenderImage::RenderImage(Document& document, RenderStyle&& style, StyleImage* styleImage)
    : RenderReplaced(document, WTFMove(style), IntSize())
    , m_imageResource(styleImage ? makeUnique<RenderImageResourceStyleImage>(*styleImage) : makeUnique<RenderImageResource>())
{
}

RenderImage::~RenderImage()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderImage::willBeDestroyed()
{
    imageResource().shutdown();
    RenderReplaced::willBeDestroyed();
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
        IntSize paddedTextSize(paddingWidth + std::min(ceilf(font.width(RenderBlock::constructTextRun(m_altText, style()))), maxAltTextWidth), paddingHeight + std::min(font.fontMetrics().height(), maxAltTextHeight));
        imageSize = imageSize.expandedTo(paddedTextSize);
    }

    if (imageSize == intrinsicSize())
        return ImageSizeChangeNone;

    setIntrinsicSize(imageSize);
    return ImageSizeChangeForAltText;
}

bool RenderImage::isEditableImage() const
{
    if (!element() || !is<HTMLImageElement>(element()))
        return false;
    return downcast<HTMLImageElement>(element())->hasEditableImageAttribute();
}
    
bool RenderImage::requiresLayer() const
{
    if (RenderReplaced::requiresLayer())
        return true;
    
    if (isEditableImage())
        return true;
    
    return false;
}

void RenderImage::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    if (!hasInitializedStyle())
        imageResource().initialize(*this);
    RenderReplaced::styleWillChange(diff, newStyle);
}

void RenderImage::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(diff, oldStyle);
    if (m_needsToSetSizeForAltText) {
        if (!m_altText.isEmpty() && setImageSizeForAltText(cachedImage()))
            repaintOrMarkForLayout(ImageSizeChangeForAltText);
        m_needsToSetSizeForAltText = false;
    }
    if (diff == StyleDifference::Layout && oldStyle->imageOrientation() != style().imageOrientation())
        return repaintOrMarkForLayout(ImageSizeChangeNone);

#if ENABLE(CSS_IMAGE_RESOLUTION)
    if (diff == StyleDifference::Layout
        && (oldStyle->imageResolution() != style().imageResolution()
            || oldStyle->imageResolutionSnap() != style().imageResolutionSnap()
            || oldStyle->imageResolutionSource() != style().imageResolutionSource()))
        repaintOrMarkForLayout(ImageSizeChangeNone);
#endif
}

bool RenderImage::shouldCollapseToEmpty() const
{
    auto imageRepresentsNothing = [&] {
        if (!element()->hasAttribute(HTMLNames::altAttr))
            return false;
        return imageResource().errorOccurred() && m_altText.isEmpty();
    };
    if (!element()) {
        // Images with no associated elements do not fall under the category of unwanted content.
        return false;
    }
    if (!isInline())
        return false;
    if (!imageRepresentsNothing())
        return false;
    return document().inNoQuirksMode() || (style().logicalWidth().isAuto() && style().logicalHeight().isAuto());
}

LayoutUnit RenderImage::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    if (shouldCollapseToEmpty())
        return { };
    return RenderReplaced::computeReplacedLogicalWidth(shouldComputePreferred);
}

LayoutUnit RenderImage::computeReplacedLogicalHeight(Optional<LayoutUnit> estimatedUsedWidth) const
{
    if (shouldCollapseToEmpty())
        return { };
    return RenderReplaced::computeReplacedLogicalHeight(estimatedUsedWidth);
}

void RenderImage::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    if (renderTreeBeingDestroyed())
        return;

    if (hasVisibleBoxDecorations() || hasMask() || hasShapeOutside())
        RenderReplaced::imageChanged(newImage, rect);

    if (shouldCollapseToEmpty()) {
        // Image might need resizing when we are at the final state.
        setNeedsLayout();
    }

    if (newImage != imageResource().imagePtr() || !newImage)
        return;

    // At a zoom level of 1 the image is guaranteed to have an integer size.
    incrementVisuallyNonEmptyPixelCountIfNeeded(flooredIntSize(imageResource().imageSize(1.0f)));

    ImageSizeChangeType imageSizeChange = ImageSizeChangeNone;

    // Set image dimensions, taking into account the size of the alt text.
    if (imageResource().errorOccurred()) {
        if (!m_altText.isEmpty() && document().hasPendingStyleRecalc()) {
            ASSERT(element());
            if (element()) {
                m_needsToSetSizeForAltText = true;
                element()->invalidateStyle();
            }
            return;
        }
        imageSizeChange = setImageSizeForAltText(cachedImage());
    }
    repaintOrMarkForLayout(imageSizeChange, rect);
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferRecomputeIsIgnoredIfNeeded(element());

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextEnabled())
        view().frameView().layoutContext().invalidateLayoutTreeContent();
#endif
}

void RenderImage::updateIntrinsicSizeIfNeeded(const LayoutSize& newSize)
{
    if (imageResource().errorOccurred() || !m_imageResource->cachedImage())
        return;
    setIntrinsicSize(newSize);
}

void RenderImage::updateInnerContentRect()
{
    // Propagate container size to image resource.
    IntSize containerSize(replacedContentRect().size());
    if (!containerSize.isEmpty()) {
        URL imageSourceURL;
        if (HTMLImageElement* imageElement = is<HTMLImageElement>(element()) ? downcast<HTMLImageElement>(element()) : nullptr)
            imageSourceURL = document().completeURL(imageElement->imageSourceURL());
        imageResource().setContainerContext(containerSize, imageSourceURL);
    }
}

void RenderImage::repaintOrMarkForLayout(ImageSizeChangeType imageSizeChange, const IntRect* rect)
{
#if ENABLE(CSS_IMAGE_RESOLUTION)
    double scale = style().imageResolution();
    if (style().imageResolutionSnap() == ImageResolutionSnap::Pixels)
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

    if (imageSourceHasChangedSize && setNeedsLayoutIfNeededAfterIntrinsicSizeChange())
        return;

    if (everHadLayout() && !selfNeedsLayout()) {
        // The inner content rectangle is calculated during layout, but may need an update now
        // (unless the box has already been scheduled for layout). In order to calculate it, we
        // may need values from the containing block, though, so make sure that we're not too
        // early. It may be that layout hasn't even taken place once yet.

        // FIXME: we should not have to trigger another call to setContainerContextForRenderer()
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

void RenderImage::notifyFinished(CachedResource& newImage, const NetworkLoadMetrics& metrics)
{
    if (renderTreeBeingDestroyed())
        return;

    invalidateBackgroundObscurationStatus();

    if (&newImage == cachedImage()) {
        // tell any potential compositing layers
        // that the image is done and they can reference it directly.
        contentChanged(ImageChanged);
    }

    if (is<HTMLImageElement>(element()))
        page().didFinishLoadingImageForElement(downcast<HTMLImageElement>(*element()));

    RenderReplaced::notifyFinished(newImage, metrics);
}

void RenderImage::setImageDevicePixelRatio(float factor)
{
    if (m_imageDevicePixelRatio == factor)
        return;

    m_imageDevicePixelRatio = factor;
    intrinsicSizeChanged();
}

bool RenderImage::isShowingMissingOrImageError() const
{
    return !imageResource().cachedImage() || imageResource().errorOccurred();
}

bool RenderImage::isShowingAltText() const
{
    return isShowingMissingOrImageError() && !m_altText.isEmpty();
}

bool RenderImage::shouldDisplayBrokenImageIcon() const
{
    return imageResource().errorOccurred();
}

bool RenderImage::hasNonBitmapImage() const
{
    if (!imageResource().cachedImage())
        return false;

    Image* image = cachedImage()->imageForRenderer(this);
    return image && !is<BitmapImage>(image);
}

void RenderImage::paintIncompleteImageOutline(PaintInfo& paintInfo, LayoutPoint paintOffset, LayoutUnit borderWidth) const
{
    auto contentSize = this->contentSize();
    if (contentSize.width() <= 2 || contentSize.height() <= 2)
        return;

    auto leftBorder = borderLeft();
    auto topBorder = borderTop();
    auto leftPadding = paddingLeft();
    auto topPadding = paddingTop();

    // Draw an outline rect where the image should be.
    GraphicsContext& context = paintInfo.context();
    context.setStrokeStyle(SolidStroke);
    context.setStrokeColor(Color::lightGray);
    context.setFillColor(Color::transparentBlack);
    context.drawRect(snapRectToDevicePixels(LayoutRect({ paintOffset.x() + leftBorder + leftPadding, paintOffset.y() + topBorder + topPadding }, contentSize), document().deviceScaleFactor()), borderWidth);
}

static bool isDeferredImage(Element* element)
{
    return is<HTMLImageElement>(element) && downcast<HTMLImageElement>(element)->isDeferred();
}

void RenderImage::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    GraphicsContext& context = paintInfo.context();
    if (context.invalidatingImagesWithAsyncDecodes()) {
        if (cachedImage() && cachedImage()->isClientWaitingForAsyncDecoding(*this))
            cachedImage()->removeAllClientsWaitingForAsyncDecoding();
        return;
    }

    auto contentSize = this->contentSize();
    float deviceScaleFactor = document().deviceScaleFactor();
    LayoutUnit missingImageBorderWidth(1 / deviceScaleFactor);

    if (context.detectingContentfulPaint()) {
        if (!context.contenfulPaintDetected() && !isDeferredImage(element()) && cachedImage() && cachedImage()->canRender(this, deviceScaleFactor) && !contentSize.isEmpty())
            context.setContentfulPaintDetected();

        return;
    }

    if (!imageResource().cachedImage() || isDeferredImage(element()) || shouldDisplayBrokenImageIcon()) {
        if (paintInfo.phase == PaintPhase::Selection)
            return;

        if (paintInfo.phase == PaintPhase::Foreground)
            page().addRelevantUnpaintedObject(this, visualOverflowRect());

        paintIncompleteImageOutline(paintInfo, paintOffset, missingImageBorderWidth);

        if (contentSize.width() > 2 && contentSize.height() > 2) {
            LayoutUnit leftBorder = borderLeft();
            LayoutUnit topBorder = borderTop();
            LayoutUnit leftPad = paddingLeft();
            LayoutUnit topPad = paddingTop();

            bool errorPictureDrawn = false;
            LayoutSize imageOffset;
            // When calculating the usable dimensions, exclude the pixels of
            // the outline rect so the error image/alt text doesn't draw on it.
            LayoutSize usableSize = contentSize - LayoutSize(2 * missingImageBorderWidth, 2 * missingImageBorderWidth);

            RefPtr<Image> image = imageResource().image();

            if (shouldDisplayBrokenImageIcon() && !image->isNull() && usableSize.width() >= image->width() && usableSize.height() >= image->height()) {
                // Call brokenImage() explicitly to ensure we get the broken image icon at the appropriate resolution.
                std::pair<Image*, float> brokenImageAndImageScaleFactor = cachedImage()->brokenImage(deviceScaleFactor);
                image = brokenImageAndImageScaleFactor.first;
                FloatSize imageSize = image->size();
                imageSize.scale(1 / brokenImageAndImageScaleFactor.second);

                // Center the error image, accounting for border and padding.
                LayoutUnit centerX { (usableSize.width() - imageSize.width()) / 2 };
                if (centerX < 0)
                    centerX = 0;
                LayoutUnit centerY { (usableSize.height() - imageSize.height()) / 2 };
                if (centerY < 0)
                    centerY = 0;
                imageOffset = LayoutSize(leftBorder + leftPad + centerX + missingImageBorderWidth, topBorder + topPad + centerY + missingImageBorderWidth);

                context.drawImage(*image, snapRectToDevicePixels(LayoutRect(paintOffset + imageOffset, imageSize), deviceScaleFactor), { imageOrientation() });
                errorPictureDrawn = true;
            }

            if (!m_altText.isEmpty()) {
                String text = document().displayStringModifiedByEncoding(m_altText);
                context.setFillColor(style().visitedDependentColorWithColorFilter(CSSPropertyColor));
                const FontCascade& font = style().fontCascade();
                const FontMetrics& fontMetrics = font.fontMetrics();
                LayoutUnit ascent = fontMetrics.ascent();
                LayoutPoint altTextOffset = paintOffset;
                altTextOffset.move(leftBorder + leftPad + (paddingWidth / 2) - missingImageBorderWidth, topBorder + topPad + ascent + (paddingHeight / 2) - missingImageBorderWidth);

                // Only draw the alt text if it'll fit within the content box,
                // and only if it fits above the error image.
                TextRun textRun = RenderBlock::constructTextRun(text, style());
                LayoutUnit textWidth { font.width(textRun) };
                if (errorPictureDrawn) {
                    if (usableSize.width() >= textWidth && fontMetrics.height() <= imageOffset.height())
                        context.drawText(font, textRun, altTextOffset);
                } else if (usableSize.width() >= textWidth && usableSize.height() >= fontMetrics.height())
                    context.drawText(font, textRun, altTextOffset);
            }
        }
        return;
    }
    
    if (contentSize.isEmpty())
        return;

    bool showBorderForIncompleteImage = settings().incompleteImageBorderEnabled();

    RefPtr<Image> img = imageResource().image(flooredIntSize(contentSize));
    if (!img || img->isNull()) {
        if (showBorderForIncompleteImage)
            paintIncompleteImageOutline(paintInfo, paintOffset, missingImageBorderWidth);

        if (paintInfo.phase == PaintPhase::Foreground)
            page().addRelevantUnpaintedObject(this, visualOverflowRect());
        return;
    }

    LayoutRect contentBoxRect = this->contentBoxRect();
    contentBoxRect.moveBy(paintOffset);
    LayoutRect replacedContentRect = this->replacedContentRect();
    replacedContentRect.moveBy(paintOffset);
    bool clip = !contentBoxRect.contains(replacedContentRect);
    GraphicsContextStateSaver stateSaver(context, clip);
    if (clip)
        context.clip(contentBoxRect);

    ImageDrawResult result = paintIntoRect(paintInfo, snapRectToDevicePixels(replacedContentRect, deviceScaleFactor));

    if (showBorderForIncompleteImage && (result != ImageDrawResult::DidDraw || (cachedImage() && cachedImage()->isLoading())))
        paintIncompleteImageOutline(paintInfo, paintOffset, missingImageBorderWidth);
    
    if (cachedImage() && paintInfo.phase == PaintPhase::Foreground) {
        // For now, count images as unpainted if they are still progressively loading. We may want 
        // to refine this in the future to account for the portion of the image that has painted.
        LayoutRect visibleRect = intersection(replacedContentRect, contentBoxRect);
        if (cachedImage()->isLoading() || result == ImageDrawResult::DidRequestDecoding)
            page().addRelevantUnpaintedObject(this, visibleRect);
        else
            page().addRelevantRepaintedObject(this, visibleRect);
    }
}

void RenderImage::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    RenderReplaced::paint(paintInfo, paintOffset);
    
    if (paintInfo.phase == PaintPhase::Outline)
        paintAreaElementFocusRing(paintInfo, paintOffset);
}
    
void RenderImage::paintAreaElementFocusRing(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
#if !PLATFORM(IOS_FAMILY) || ENABLE(FULL_KEYBOARD_ACCESS)
    if (document().printing() || !frame().selection().isFocusedAndActive())
        return;
    
    if (paintInfo.context().paintingDisabled() && !paintInfo.context().performingPaintInvalidation())
        return;

    Element* focusedElement = document().focusedElement();
    if (!is<HTMLAreaElement>(focusedElement))
        return;

    HTMLAreaElement& areaElement = downcast<HTMLAreaElement>(*focusedElement);
    if (areaElement.imageElement() != element())
        return;

    auto* areaElementStyle = areaElement.computedStyle();
    if (!areaElementStyle)
        return;

    float outlineWidth = areaElementStyle->outlineWidth();
    if (!outlineWidth)
        return;

    // Even if the theme handles focus ring drawing for entire elements, it won't do it for
    // an area within an image, so we don't call RenderTheme::supportsFocusRing here.
    auto path = areaElement.computePathForFocusRing(size());
    if (path.isEmpty())
        return;

    AffineTransform zoomTransform;
    zoomTransform.scale(style().effectiveZoom());
    path.transform(zoomTransform);

    auto adjustedOffset = paintOffset;
    adjustedOffset.moveBy(location());
    path.translate(toFloatSize(adjustedOffset));

#if PLATFORM(MAC)
    bool needsRepaint;
    paintInfo.context().drawFocusRing(path, page().focusController().timeSinceFocusWasSet().seconds(), needsRepaint, RenderTheme::singleton().focusRingColor(styleColorOptions()));
    if (needsRepaint)
        page().focusController().setFocusedElementNeedsRepaint();
#else
    paintInfo.context().drawFocusRing(path, outlineWidth, areaElementStyle->outlineOffset(), areaElementStyle->visitedDependentColorWithColorFilter(CSSPropertyOutlineColor));
#endif // PLATFORM(MAC)
#else
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(paintOffset);
#endif // ENABLE(FULL_KEYBOARD_ACCESS)
}

void RenderImage::areaElementFocusChanged(HTMLAreaElement* element)
{
    ASSERT_UNUSED(element, element->imageElement() == this->element());

    // It would be more efficient to only repaint the focus ring rectangle
    // for the passed-in area element. That would require adding functions
    // to the area element class.
    repaint();
}

ImageDrawResult RenderImage::paintIntoRect(PaintInfo& paintInfo, const FloatRect& rect)
{
    if (!imageResource().cachedImage() || imageResource().errorOccurred() || rect.width() <= 0 || rect.height() <= 0)
        return ImageDrawResult::DidNothing;

    RefPtr<Image> img = imageResource().image(flooredIntSize(rect.size()));
    if (!img || img->isNull())
        return ImageDrawResult::DidNothing;

    HTMLImageElement* imageElement = is<HTMLImageElement>(element()) ? downcast<HTMLImageElement>(element()) : nullptr;

    // FIXME: Document when image != img.get().
    Image* image = imageResource().image().get();

#if USE(CG)
    if (is<PDFDocumentImage>(image))
        downcast<PDFDocumentImage>(*image).setPdfImageCachingPolicy(settings().pdfImageCachingPolicy());
#endif

    if (is<BitmapImage>(image))
        downcast<BitmapImage>(*image).updateFromSettings(settings());

    ImagePaintingOptions options = {
        imageElement ? imageElement->compositeOperator() : CompositeOperator::SourceOver,
        decodingModeForImageDraw(*image, paintInfo),
        imageOrientation(),
        image ? chooseInterpolationQuality(paintInfo.context(), *image, image, LayoutSize(rect.size())) : InterpolationQuality::Default
    };

    auto drawResult = paintInfo.context().drawImage(*img, rect, options);
    if (drawResult == ImageDrawResult::DidRequestDecoding)
        imageResource().cachedImage()->addClientWaitingForAsyncDecoding(*this);

#if USE(SYSTEM_PREVIEW)
    if (imageElement && imageElement->isSystemPreviewImage() && drawResult == ImageDrawResult::DidDraw && RuntimeEnabledFeatures::sharedFeatures().systemPreviewEnabled())
        theme().paintSystemPreviewBadge(*img, paintInfo, rect);
#endif

    return drawResult;
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
    if (!imageResource().cachedImage() || imageResource().errorOccurred())
        return false;
    if (cachedImage() && !cachedImage()->isLoaded())
        return false;
    if (!contentBoxRect().contains(localRect))
        return false;
    FillBox backgroundClip = style().backgroundClip();
    // Background paints under borders.
    if (backgroundClip == FillBox::Border && style().hasBorder() && !borderObscuresBackground())
        return false;
    // Background shows in padding area.
    if ((backgroundClip == FillBox::Border || backgroundClip == FillBox::Padding) && style().hasPadding())
        return false;
    // Object-fit may leave parts of the content box empty.
    ObjectFit objectFit = style().objectFit();
    if (objectFit != ObjectFit::Fill && objectFit != ObjectFit::Cover)
        return false;

    LengthPoint objectPosition = style().objectPosition();
    if (objectPosition != RenderStyle::initialObjectPosition())
        return false;

    // Check for image with alpha.
    return cachedImage() && cachedImage()->currentFrameKnownToBeOpaque(this);
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
    return imageResource().errorOccurred() ? intrinsicSize().height() : 0_lu;
}

HTMLMapElement* RenderImage::imageMap() const
{
    auto* imageElement = element();
    if (!imageElement || !is<HTMLImageElement>(imageElement))
        return nullptr;
    return downcast<HTMLImageElement>(imageElement)->associatedMapElement();
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
            mapLocation.scale(scaleFactor);

            if (map->mapMouseEvent(mapLocation, contentBox.size(), tempResult))
                tempResult.setInnerNonSharedNode(element());
        }
    }

    if (!inside && request.resultIsElementList())
        result.append(tempResult, request);
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
    // Recomputing overflow is required only when child content is present. 
    if (needsSimplifiedNormalFlowLayoutOnly() && !m_hasShadowControls) {
        clearNeedsLayout();
        return;
    }

    StackStats::LayoutCheckPoint layoutCheckPoint;

    LayoutSize oldSize = contentBoxRect().size();
    RenderReplaced::layout();

    updateInnerContentRect();

    if (m_hasShadowControls)
        layoutShadowControls(oldSize);
}

void RenderImage::layoutShadowControls(const LayoutSize& oldSize)
{
    // We expect a single containing box under the UA shadow root.
    ASSERT(firstChild() == lastChild());

    auto* controlsRenderer = downcast<RenderBox>(firstChild());
    if (!controlsRenderer)
        return;
    
    bool controlsNeedLayout = controlsRenderer->needsLayout();
    // If the region chain has changed we also need to relayout the controls to update the region box info.
    // FIXME: We can do better once we compute region box info for RenderReplaced, not only for RenderBlock.
    const RenderFragmentedFlow* fragmentedFlow = enclosingFragmentedFlow();
    if (fragmentedFlow && !controlsNeedLayout) {
        if (fragmentedFlow->pageLogicalSizeChanged())
            controlsNeedLayout = true;
    }

    LayoutSize newSize = contentBoxRect().size();
    if (newSize == oldSize && !controlsNeedLayout)
        return;

    // When calling layout() on a child node, a parent must either push a LayoutStateMaintainter, or 
    // instantiate LayoutStateDisabler. Since using a LayoutStateMaintainer is slightly more efficient,
    // and this method might be called many times per second during video playback, use a LayoutStateMaintainer:
    LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

    if (shadowControlsNeedCustomLayoutMetrics()) {
        controlsRenderer->setLocation(LayoutPoint(borderLeft(), borderTop()) + LayoutSize(paddingLeft(), paddingTop()));
        controlsRenderer->mutableStyle().setHeight(Length(newSize.height(), Fixed));
        controlsRenderer->mutableStyle().setWidth(Length(newSize.width(), Fixed));
    }

    controlsRenderer->setNeedsLayout(MarkOnlyThis);
    controlsRenderer->layout();
    clearChildNeedsLayout();
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
    if (shouldDisplayBrokenImageIcon()) {
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
    CachedImage* cachedImage = this->cachedImage();
    if (cachedImage && is<SVGImage>(cachedImage->image()))
        return downcast<SVGImage>(*cachedImage->image()).embeddedContentBox();

    return nullptr;
}

} // namespace WebCore
