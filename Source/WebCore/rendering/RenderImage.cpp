/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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
#include "DocumentInlines.h"
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
#include "ImageOverlay.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "LineSelection.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderChildIterator.h"
#include "RenderFragmentedFlow.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderLayoutState.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "Settings.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

#if PLATFORM(IOS_FAMILY)
#include "LogicalSelectionOffsetCaches.h"
#include "SelectionGeometry.h"
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
void RenderImage::collectSelectionGeometries(Vector<SelectionGeometry>& geometries, unsigned, unsigned)
{
    RenderBlock* containingBlock = this->containingBlock();

    IntRect imageRect;
    // FIXME: It doesn't make sense to package line bounds into SelectionGeometry. We should find
    // the right and left extent of the selection once for the entire selected Range, perhaps
    // using the Range's common ancestor.
    IntRect lineExtentRect;
    bool isFirstOnLine = false;
    bool isLastOnLine = false;

    auto run = InlineIterator::boxFor(*this);
    if (!run) {
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
        auto selectionLogicalRect = LineSelection::logicalRect(*run->lineBox());
        int selectionTop = !containingBlock->style().isFlippedBlocksWritingMode() ? selectionLogicalRect.y() - logicalTop() : logicalBottom() - selectionLogicalRect.maxY();
        int selectionHeight = selectionLogicalRect.height();
        imageRect = IntRect { 0,  selectionTop, logicalWidth(), selectionHeight };
        isFirstOnLine = !run->previousOnLine();
        isLastOnLine = !run->nextOnLine();
        LogicalSelectionOffsetCaches cache(*containingBlock);
        LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, LayoutUnit(run->logicalTop()), cache);
        LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, LayoutUnit(run->logicalTop()), cache);
        lineExtentRect = IntRect(leftOffset - logicalLeft(), imageRect.y(), rightOffset - leftOffset, imageRect.height());
        if (!run->isHorizontal()) {
            imageRect = imageRect.transposedRect();
            lineExtentRect = lineExtentRect.transposedRect();
        }
    }

    bool isFixed = false;
    auto absoluteQuad = localToAbsoluteQuad(FloatRect(imageRect), UseTransforms, &isFixed);
    auto lineExtentBounds = localToAbsoluteQuad(FloatRect(lineExtentRect)).enclosingBoundingBox();
    if (!containingBlock->isHorizontalWritingMode())
        lineExtentBounds = lineExtentBounds.transposedRect();

    // FIXME: We should consider either making SelectionGeometry a struct or better organize its optional fields into
    // an auxiliary struct to simplify its initialization.
    geometries.append(SelectionGeometry(absoluteQuad, SelectionRenderingBehavior::CoalesceBoundingRects, containingBlock->style().direction(), lineExtentBounds.x(), lineExtentBounds.maxX(), lineExtentBounds.maxY(), 0, false /* line break */, isFirstOnLine, isLastOnLine, false /* contains start */, false /* contains end */, containingBlock->style().isHorizontalWritingMode(), isFixed, false /* ruby text */, view().pageNumberForBlockProgressionOffset(absoluteQuad.enclosingBoundingBox().x())));
}
#endif

using namespace HTMLNames;

RenderImage::RenderImage(Element& element, RenderStyle&& style, StyleImage* styleImage, const float imageDevicePixelRatio)
    : RenderReplaced(element, WTFMove(style), IntSize())
    , m_imageResource(styleImage ? makeUnique<RenderImageResourceStyleImage>(*styleImage) : makeUnique<RenderImageResource>())
    , m_hasImageOverlay(is<HTMLElement>(element) && ImageOverlay::hasOverlay(downcast<HTMLElement>(element)))
    , m_imageDevicePixelRatio(imageDevicePixelRatio)
{
    updateAltText();
#if ENABLE(SERVICE_CONTROLS)
    if (is<HTMLImageElement>(element))
        m_hasShadowControls = downcast<HTMLImageElement>(element).isImageMenuEnabled();
#endif
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
        IntSize paddedTextSize(paddingWidth + std::min(ceilf(font.width(RenderBlock::constructTextRun(m_altText, style()))), maxAltTextWidth), paddingHeight + std::min(font.metricsOfPrimaryFont().height(), maxAltTextHeight));
        imageSize = imageSize.expandedTo(paddedTextSize);
    }

    if (imageSize == intrinsicSize())
        return ImageSizeChangeNone;

    setIntrinsicSize(imageSize);
    return ImageSizeChangeForAltText;
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
    if (diff == StyleDifference::Layout && oldStyle && oldStyle->imageOrientation() != style().imageOrientation())
        return repaintOrMarkForLayout(ImageSizeChangeNone);

#if ENABLE(CSS_IMAGE_RESOLUTION)
    if (diff == StyleDifference::Layout && oldStyle
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

LayoutUnit RenderImage::computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth) const
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
        if (auto* imageElement = dynamicDowncast<HTMLImageElement>(element()))
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

bool RenderImage::hasAnimatedImage() const
{
    if (auto* image = cachedImage() ? cachedImage()->image() : nullptr)
        return image->isAnimated();
    return false;
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

    if (isDeferredImage(element()))
        return;

    if (context.detectingContentfulPaint()) {
        if (!context.contentfulPaintDetected() && cachedImage() && cachedImage()->canRender(this, deviceScaleFactor) && !contentSize.isEmpty())
            context.setContentfulPaintDetected();

        return;
    }

    if (!imageResource().cachedImage() || shouldDisplayBrokenImageIcon()) {
        if (paintInfo.phase == PaintPhase::Selection)
            return;

        if (paintInfo.phase == PaintPhase::Foreground)
            page().addRelevantUnpaintedObject(this, visualOverflowRect());

        paintIncompleteImageOutline(paintInfo, paintOffset, missingImageBorderWidth);

        if (contentSize.width() > 2 && contentSize.height() > 2) {
            LayoutUnit leftBorder = borderLeft();
            LayoutUnit topBorder = borderTop();
            LayoutUnit leftPadding = paddingLeft();
            LayoutUnit topPadding = paddingTop();

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
                imageOffset = LayoutSize(leftBorder + leftPadding + centerX + missingImageBorderWidth, topBorder + topPadding + centerY + missingImageBorderWidth);

                context.drawImage(*image, snapRectToDevicePixels(LayoutRect(paintOffset + imageOffset, imageSize), deviceScaleFactor), { imageOrientation() });
                errorPictureDrawn = true;
            }

            if (!m_altText.isEmpty()) {
                auto& font = style().fontCascade();
                auto& fontMetrics = font.metricsOfPrimaryFont();
                auto encodedDisplayString = document().displayStringModifiedByEncoding(m_altText);
                auto textRun = RenderBlock::constructTextRun(encodedDisplayString, style(), ExpansionBehavior::defaultBehavior(), RespectDirection | RespectDirectionOverride);
                auto textWidth = LayoutUnit { font.width(textRun) };

                auto hasRoomForAltText = [&] {
                    // Only draw the alt text if it'll fit within the content box,
                    // and only if it fits above the error image.
                    if (usableSize.width() < textWidth)
                        return false;
                    return errorPictureDrawn ? fontMetrics.height() <= imageOffset.height() : usableSize.height() >= fontMetrics.height();
                };
                if (hasRoomForAltText()) {
                    auto altTextLocation = [&]() -> LayoutPoint {
                        auto contentHorizontalOffset = LayoutUnit { leftBorder + leftPadding + (paddingWidth / 2) - missingImageBorderWidth };
                        auto contentVerticalOffset = LayoutUnit { topBorder + topPadding + fontMetrics.ascent() + (paddingHeight / 2) - missingImageBorderWidth };
                        if (!style().isLeftToRightDirection())
                            contentHorizontalOffset += contentSize.width() - textWidth;
                        return paintOffset + LayoutPoint { contentHorizontalOffset, contentVerticalOffset };
                    };
                    context.setFillColor(style().visitedDependentColorWithColorFilter(CSSPropertyColor));
                    context.drawBidiText(font, textRun, altTextLocation());
                }
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

    auto* imageElement = dynamicDowncast<HTMLImageElement>(element());

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
    if (imageElement && imageElement->isSystemPreviewImage() && drawResult == ImageDrawResult::DidDraw && imageElement->document().settings().systemPreviewEnabled())
        theme().paintSystemPreviewBadge(*img, paintInfo, rect);
#endif

    return drawResult;
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
    return hasShadowContent();
}

void RenderImage::layout()
{
    // Recomputing overflow is required only when child content is present.
    if (needsSimplifiedNormalFlowLayoutOnly() && !hasShadowContent()) {
        clearNeedsLayout();
        return;
    }

    StackStats::LayoutCheckPoint layoutCheckPoint;

    LayoutSize oldSize = contentBoxRect().size();
    RenderReplaced::layout();

    updateInnerContentRect();

    if (hasShadowContent())
        layoutShadowContent(oldSize);
}

void RenderImage::layoutShadowContent(const LayoutSize& oldSize)
{
    for (auto& renderBox : childrenOfType<RenderBox>(*this)) {
        bool childNeedsLayout = renderBox.needsLayout();
        // If the region chain has changed we also need to relayout the children to update the region box info.
        // FIXME: We can do better once we compute region box info for RenderReplaced, not only for RenderBlock.
        auto* fragmentedFlow = enclosingFragmentedFlow();
        if (fragmentedFlow && !childNeedsLayout) {
            if (fragmentedFlow->pageLogicalSizeChanged())
                childNeedsLayout = true;
        }

        auto newSize = contentBoxRect().size();
        if (newSize == oldSize && !childNeedsLayout)
            continue;

        // When calling layout() on a child node, a parent must either push a LayoutStateMaintainer, or
        // instantiate LayoutStateDisabler. Since using a LayoutStateMaintainer is slightly more efficient,
        // and this method might be called many times per second during video playback, use a LayoutStateMaintainer:
        LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());
        renderBox.setLocation(LayoutPoint(borderLeft(), borderTop()) + LayoutSize(paddingLeft(), paddingTop()));
        renderBox.mutableStyle().setHeight(Length(newSize.height(), LengthType::Fixed));
        renderBox.mutableStyle().setWidth(Length(newSize.width(), LengthType::Fixed));
        renderBox.setNeedsLayout(MarkOnlyThis);
        renderBox.layout();
    }

    clearChildNeedsLayout();
}

void RenderImage::computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const
{
    ASSERT(!shouldApplySizeContainment());
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
        if (settings().aspectRatioOfImgFromWidthAndHeightEnabled()
            && style().aspectRatioType() == AspectRatioType::AutoAndRatio && !isShowingAltText())
            intrinsicRatio = style().logicalAspectRatio();
        else
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

bool RenderImage::allowsAnimation() const
{
    if (auto* imageElement = dynamicDowncast<HTMLImageElement>(element()))
        return imageElement->allowsAnimation();
    return RenderReplaced::allowsAnimation();
}

} // namespace WebCore
