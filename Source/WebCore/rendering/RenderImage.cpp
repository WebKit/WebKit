/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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
#include "DocumentView.h"
#include "FocusController.h"
#include "FontCache.h"
#include "FontCascade.h"
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
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "LineSelection.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderObjectInlines.h"
#include "RenderStyleInlines.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "TextPainter.h"
#include <wtf/StackStats.h>
#include <wtf/TypeCasts.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include "LogicalSelectionOffsetCachesInlines.h"
#include "SelectionGeometry.h"
#endif

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#include "MultiRepresentationHEICMetrics.h"
#endif

#if USE(CG)
#include "PDFDocumentImage.h"
#include "Settings.h"
#endif

#if ENABLE(SPATIAL_IMAGE_CONTROLS)
#include "SpatialImageControls.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderImage);

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
        int selectionTop = !containingBlock->writingMode().isBlockFlipped() ? selectionLogicalRect.y() - logicalTop() : logicalBottom() - selectionLogicalRect.maxY();
        int selectionHeight = selectionLogicalRect.height();
        imageRect = IntRect { 0,  selectionTop, logicalWidth(), selectionHeight };
        isFirstOnLine = !run->nextLineLeftwardOnLine();
        isLastOnLine = !run->nextLineRightwardOnLine();
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
    geometries.append(SelectionGeometry(absoluteQuad, SelectionRenderingBehavior::CoalesceBoundingRects, containingBlock->writingMode().bidiDirection(), lineExtentBounds.x(), lineExtentBounds.maxX(), lineExtentBounds.maxY(), 0, false /* line break */, isFirstOnLine, isLastOnLine, false /* contains start */, false /* contains end */, containingBlock->writingMode().isHorizontal(), isFixed, view().pageNumberForBlockProgressionOffset(absoluteQuad.enclosingBoundingBox().x())));
}
#endif

using namespace HTMLNames;

RenderImage::RenderImage(Type type, Element& element, RenderStyle&& style, OptionSet<ReplacedFlag> flags, StyleImage* styleImage, const float imageDevicePixelRatio)
    : RenderReplaced(type, element, WTFMove(style), IntSize(), flags | ReplacedFlag::IsImage)
    , m_imageResource(styleImage ? makeUnique<RenderImageResourceStyleImage>(*styleImage) : makeUnique<RenderImageResource>())
    , m_hasImageOverlay([&] {
        auto* htmlElement = dynamicDowncast<HTMLElement>(element);
        return htmlElement && ImageOverlay::hasOverlay(*htmlElement);
    }())
    , m_imageDevicePixelRatio(imageDevicePixelRatio)
{
    updateAltText();
#if ENABLE(SERVICE_CONTROLS)
    if (RefPtr image = dynamicDowncast<HTMLImageElement>(element))
        m_hasShadowControls = image->isImageMenuEnabled();
#endif
#if ENABLE(SPATIAL_IMAGE_CONTROLS)
    if (RefPtr image = dynamicDowncast<HTMLElement>(element))
        m_hasShadowControls = SpatialImageControls::hasSpatialImageControls(*image);
#endif
}

RenderImage::RenderImage(Type type, Element& element, RenderStyle&& style, StyleImage* styleImage, const float imageDevicePixelRatio)
    : RenderImage(type, element, WTFMove(style), ReplacedFlag::IsImage, styleImage, imageDevicePixelRatio)
{
}

RenderImage::RenderImage(Type type, Document& document, RenderStyle&& style, StyleImage* styleImage)
    : RenderReplaced(type, document, WTFMove(style), IntSize(), ReplacedFlag::IsImage)
    , m_imageResource(styleImage ? makeUnique<RenderImageResourceStyleImage>(*styleImage) : makeUnique<RenderImageResource>())
{
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderImage::~RenderImage() = default;

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
        auto brokenImageAndImageScaleFactor = newImage->brokenImage(document().deviceScaleFactor());
        imageSize = brokenImageAndImageScaleFactor.first->size();
        imageSize.scale(1 / brokenImageAndImageScaleFactor.second);
    } else
        imageSize = newImage->imageForRenderer(this)->size();

    // imageSize() returns 0 for the error image. We need the true size of the
    // error image, so we have to get it by grabbing image() directly.
    return IntSize(paddingWidth + imageSize.width() * style().usedZoom(), paddingHeight + imageSize.height() * style().usedZoom());
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
        IntSize paddedTextSize(paddingWidth + std::min(ceilf(font.width(RenderBlock::constructTextRun(m_altText, style()))), maxAltTextWidth), paddingHeight + std::min(font.metricsOfPrimaryFont().intHeight(), maxAltTextHeight));
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

    if (oldStyle && diff == StyleDifference::Layout) {
        if (oldStyle->imageOrientation() != style().imageOrientation())
            return repaintOrMarkForLayout(ImageSizeChangeNone);

#if ENABLE(MULTI_REPRESENTATION_HEIC)
        if (isMultiRepresentationHEIC() && oldStyle->fontCascade() != style().fontCascade())
            return repaintOrMarkForLayout(ImageSizeChangeNone);
#endif
    }
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
    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->deferRecomputeIsIgnoredIfNeeded(element());

    if (auto* image = cachedImage(); image && image->currentFrameIsComplete(this)) {
        if (auto styleable = Styleable::fromRenderer(*this))
            protectedDocument()->didLoadImage(styleable->protectedElement().get(), image);
    }
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
            imageSourceURL = imageElement->currentURL();
        imageResource().setContainerContext(containerSize, imageSourceURL);
    }
}

void RenderImage::repaintOrMarkForLayout(ImageSizeChangeType imageSizeChange, const IntRect* rect)
{
    LayoutSize newIntrinsicSize = imageResource().intrinsicSize(style().usedZoom());
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

    if (parent()) {
        auto repaintRect = contentBoxRect();
        if (rect) {
            // The image changed rect is in source image coordinates (pre-zooming),
            // so map from the bounds of the image to the contentsBox.
            RefPtr<Image> srcImg = imageResource().image(flooredIntSize(contentBoxSize()));
            FloatSize sourceSize = srcImg->size() / style().usedZoom();
            repaintRect.intersect(enclosingIntRect(mapRect(*rect, FloatRect(FloatPoint(), sourceSize), repaintRect)));
        }
        repaintRectangle(repaintRect);
    }

    // Tell any potential compositing layers that the image needs updating.
    contentChanged(ContentChangeType::Image);
}

void RenderImage::notifyFinished(CachedResource& newImage, const NetworkLoadMetrics& metrics, LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    if (renderTreeBeingDestroyed())
        return;

    invalidateBackgroundObscurationStatus();

    if (&newImage == cachedImage()) {
        // tell any potential compositing layers
        // that the image is done and they can reference it directly.
        contentChanged(ContentChangeType::Image);
    }

    if (RefPtr image = dynamicDowncast<HTMLImageElement>(element()))
        page().didFinishLoadingImageForElement(*image);

    RenderReplaced::notifyFinished(newImage, metrics, loadWillContinueInAnotherProcess);
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

// Per CSSWG resolution, we should respect 0px inline sizes.
// See: https://github.com/w3c/csswg-drafts/issues/11236#issuecomment-2718502765
bool RenderImage::shouldRespectZeroIntrinsicWidth() const
{
    auto* cachedImage = this->cachedImage();
    if (!cachedImage)
        return false;
    if (auto* svgImage = dynamicDowncast<SVGImage>(cachedImage->image())) {
        if (auto rootElement = svgImage->rootElement())
            return rootElement->hasIntrinsicWidth();
    }
    return false;
}

#if ENABLE(MULTI_REPRESENTATION_HEIC)
bool RenderImage::isMultiRepresentationHEIC() const
{
    RefPtr imageElement = dynamicDowncast<HTMLImageElement>(element());
    if (!imageElement)
        return false;

    return imageElement->isMultiRepresentationHEIC();
}
#endif

void RenderImage::paintIncompleteImageOutline(PaintInfo& paintInfo, LayoutPoint paintOffset, LayoutUnit borderWidth) const
{
    auto contentSize = this->contentBoxSize();
    if (contentSize.width() <= 2 || contentSize.height() <= 2)
        return;

    auto leftBorder = borderLeft();
    auto topBorder = borderTop();
    auto leftPadding = paddingLeft();
    auto topPadding = paddingTop();

    // Draw an outline rect where the image should be.
    GraphicsContext& context = paintInfo.context();
    context.setStrokeStyle(StrokeStyle::SolidStroke);
    context.setStrokeColor(Color::lightGray);
    context.setFillColor(Color::transparentBlack);
    context.drawRect(snapRectToDevicePixels(LayoutRect({ paintOffset.x() + leftBorder + leftPadding, paintOffset.y() + topBorder + topPadding }, contentSize), document().deviceScaleFactor()), borderWidth);
}

static bool isDeferredImage(Element* element)
{
    RefPtr image = dynamicDowncast<HTMLImageElement>(element);
    return image && image->isDeferred();
}

void RenderImage::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!isSkippedContentRoot(*this));

    GraphicsContext& context = paintInfo.context();
    if (context.invalidatingImagesWithAsyncDecodes()) {
        if (cachedImage() && cachedImage()->isClientWaitingForAsyncDecoding(cachedImageClient()))
            cachedImage()->removeAllClientsWaitingForAsyncDecoding();
        return;
    }

    auto contentSize = this->contentBoxSize();
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
            page().addRelevantUnpaintedObject(*this, visualOverflowRect());

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

            RefPtr image = imageResource().image();

            if (shouldDisplayBrokenImageIcon() && !image->isNull() && usableSize.width() >= image->width() && usableSize.height() >= image->height()) {
                // Call brokenImage() explicitly to ensure we get the broken image icon at the appropriate resolution.
                auto brokenImageAndImageScaleFactor = cachedImage()->brokenImage(deviceScaleFactor);
                image = brokenImageAndImageScaleFactor.first.get();
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
                auto& style = this->style();
                auto& fontCascade = style.fontCascade();
                auto& fontMetrics = fontCascade.metricsOfPrimaryFont();
                auto isHorizontal = writingMode().isHorizontal();
                auto encodedDisplayString = document().displayStringModifiedByEncoding(m_altText);
                auto textRun = RenderBlock::constructTextRun(encodedDisplayString, style, ExpansionBehavior::defaultBehavior(), RespectDirection | RespectDirectionOverride);
                auto textWidth = LayoutUnit { fontCascade.width(textRun) };

                auto hasRoomForAltText = [&] {
                    // Only draw the alt text if it fits within the content box (content width) and above the error image (content height).
                    // Error picture is always visually below the text regardless of writing direction.
                    auto availableLogicalWidth = isHorizontal ? usableSize.width() : (errorPictureDrawn ? imageOffset.height() : usableSize.height());
                    if (availableLogicalWidth < textWidth)
                        return false;
                    auto availableLogicalHeight = isHorizontal ? (errorPictureDrawn ? imageOffset.height() : usableSize.height()) : usableSize.width();
                    return availableLogicalHeight >= fontMetrics.intHeight();
                };
                if (hasRoomForAltText()) {
                    context.setFillColor(style.visitedDependentColorWithColorFilter(CSSPropertyColor));
                    if (isHorizontal) {
                        auto altTextLocation = [&]() -> LayoutPoint {
                            auto contentHorizontalOffset = LayoutUnit { leftBorder + leftPadding + (paddingWidth / 2) - missingImageBorderWidth };
                            auto contentVerticalOffset = LayoutUnit { topBorder + topPadding + fontMetrics.intAscent() + (paddingHeight / 2) - missingImageBorderWidth };
                            if (!style.writingMode().isInlineLeftToRight())
                                contentHorizontalOffset += contentSize.width() - textWidth;
                            return paintOffset + LayoutPoint { contentHorizontalOffset, contentVerticalOffset };
                        };
                        context.drawBidiText(fontCascade, textRun, altTextLocation());
                    } else {
                        // FIXME: TextBoxPainter has this logic already, maybe we should transition to some painter class.
                        auto contentLogicalHeight = fontMetrics.intHeight();
                        auto adjustedPaintOffset = LayoutPoint { paintOffset.x(), paintOffset.y() - contentLogicalHeight };

                        auto visualLeft = size().width() / 2 - contentLogicalHeight / 2;
                        auto visualRight = visualLeft + contentLogicalHeight;
                        if (writingMode().isBlockFlipped())
                            visualLeft = size().width() - visualRight;
                        visualLeft += adjustedPaintOffset.x();

                        auto rotationRect = LayoutRect { visualLeft, adjustedPaintOffset.y(), textWidth, contentLogicalHeight };
                        context.concatCTM(rotation(rotationRect, RotationDirection::Clockwise));
                        auto textOrigin = LayoutPoint { visualLeft, adjustedPaintOffset.y() + fontCascade.metricsOfPrimaryFont().intAscent() };
                        context.drawBidiText(fontCascade, textRun, textOrigin);
                        context.concatCTM(rotation(rotationRect, RotationDirection::Counterclockwise));
                    }
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
            page().addRelevantUnpaintedObject(*this, visualOverflowRect());
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
    
    if (cachedImage() && paintInfo.phase == PaintPhase::Foreground && !context.paintingDisabled()) {
        // For now, count images as unpainted if they are still progressively loading. We may want 
        // to refine this in the future to account for the portion of the image that has painted.
        LayoutRect visibleRect = intersection(replacedContentRect, contentBoxRect);
        if (cachedImage()->isLoading() || result == ImageDrawResult::DidRequestDecoding)
            page().addRelevantUnpaintedObject(*this, visibleRect);
        else
            page().addRelevantRepaintedObject(*this, visibleRect);

        if (cachedImage()->currentFrameIsComplete(this)) {
            if (auto styleable = Styleable::fromRenderer(*this)) {
                auto localVisibleRect = visibleRect;
                localVisibleRect.moveBy(-paintOffset);
                protectedDocument()->didPaintImage(styleable->element, cachedImage(), localVisibleRect);
            }
        }
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

    RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(document().focusedElement());
    if (!areaElement || areaElement->imageElement() != element())
        return;

    auto* areaElementStyle = areaElement->computedStyle();
    if (!areaElementStyle)
        return;

    auto outlineWidth = Style::evaluate<float>(areaElementStyle->outlineWidth(), areaElementStyle->usedZoomForLength());
    if (!outlineWidth)
        return;

    // Even if the theme handles focus ring drawing for entire elements, it won't do it for
    // an area within an image, so we don't call RenderTheme::supportsFocusRing here.
    auto path = areaElement->computePathForFocusRing(size());
    if (path.isEmpty())
        return;

    AffineTransform zoomTransform;
    zoomTransform.scale(style().usedZoom());
    path.transform(zoomTransform);

    auto adjustedOffset = paintOffset;
    adjustedOffset.moveBy(location());
    path.translate(toFloatSize(adjustedOffset));

#if PLATFORM(MAC)
    auto styleOptions = styleColorOptions();
    styleOptions.add(StyleColorOptions::UseSystemAppearance);
    paintInfo.context().drawFocusRing(path, outlineWidth, RenderTheme::singleton().focusRingColor(styleOptions));
#else
    paintInfo.context().drawFocusRing(path, outlineWidth, areaElementStyle->visitedDependentColorWithColorFilter(CSSPropertyOutlineColor));
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

    // FIXME: Document when image != img.get().
    auto* image = imageResource().image().unsafeGet();

    ImagePaintingOptions options = {
        CompositeOperator::SourceOver,
        decodingModeForImageDraw(*image, paintInfo),
        imageOrientation(),
        image ? chooseInterpolationQuality(paintInfo.context(), *image, image, LayoutSize(rect.size())) : InterpolationQuality::Default,
        settings().imageSubsamplingEnabled() ? AllowImageSubsampling::Yes : AllowImageSubsampling::No,
        settings().showDebugBorders() ? ShowDebugBackground::Yes : ShowDebugBackground::No,
#if USE(SKIA)
        StrictImageClamping::No,
#endif
        paintInfo.paintBehavior.contains(PaintBehavior::DrawsHDRContent) ? DrawsHDRContent::Yes : DrawsHDRContent::No,
        style().dynamicRangeLimit().toPlatformDynamicRangeLimit()
    };

    auto drawResult = ImageDrawResult::DidNothing;
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    if (isMultiRepresentationHEIC())
        drawResult = paintInfo.context().drawMultiRepresentationHEIC(*img, style().fontCascade().primaryFont(), rect, options);
#endif

    if (drawResult == ImageDrawResult::DidNothing)
        drawResult = paintInfo.context().drawImage(*img, rect, options);

    if (drawResult == ImageDrawResult::DidRequestDecoding)
        imageResource().cachedImage()->addClientWaitingForAsyncDecoding(cachedImageClient());

#if USE(SYSTEM_PREVIEW)
    auto* imageElement = dynamicDowncast<HTMLImageElement>(element());
    if (imageElement && imageElement->isSystemPreviewImage() && drawResult == ImageDrawResult::DidDraw && imageElement->document().settings().systemPreviewEnabled())
        theme().paintSystemPreviewBadge(*img, paintInfo, rect);
#endif

    if (element() && !paintInfo.context().paintingDisabled())
        element()->setHasEverPaintedImages(true);

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
    auto backgroundClip = style().backgroundLayers().usedFirst().clip();
    // Background paints under borders.
    if (backgroundClip == FillBox::BorderBox && style().hasBorder() && !borderObscuresBackground())
        return false;
    // Background shows in padding area.
    if ((backgroundClip == FillBox::BorderBox || backgroundClip == FillBox::PaddingBox) && !Style::isKnownZero(style().paddingBox()))
        return false;
    // Object-fit may leave parts of the content box empty.
    if (auto objectFit = style().objectFit(); objectFit != ObjectFit::Fill && objectFit != ObjectFit::Cover)
        return false;

    if (style().objectPosition() != RenderStyle::initialObjectPosition())
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

RefPtr<HTMLMapElement> RenderImage::imageMap() const
{
    auto* imageElement = dynamicDowncast<HTMLImageElement>(element());
    return imageElement ? imageElement->associatedMapElement() : nullptr;
}

bool RenderImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    HitTestResult tempResult(result.hitTestLocation());
    bool inside = RenderReplaced::nodeAtPoint(request, tempResult, locationInContainer, accumulatedOffset, hitTestAction);

    if (tempResult.innerNode() && element()) {
        if (RefPtr map = imageMap()) {
            LayoutRect contentBox = contentBoxRect();
            float scaleFactor = 1 / style().usedZoom();
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

    if (auto* input = dynamicDowncast<HTMLInputElement>(*element()))
        m_altText = input->altText();
    else if (auto* image = dynamicDowncast<HTMLImageElement>(*element()))
        m_altText = image->altText();

    if (m_altText.isNull()) {
        // We check isNull() and not isEmpty() because we don't want to override empty-string
        // alt text provided by either of the above branches.
        m_altText = style().altFromContent();
    }
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

FloatSize RenderImage::computeIntrinsicSize() const
{
    ASSERT(!shouldApplySizeContainment());
    auto intrinsicSize = RenderReplaced::computeIntrinsicSize();

    // Our intrinsicSize is empty if we're rendering generated images with relative width/height. Figure out the right intrinsic size to use.
    if (intrinsicSize.isEmpty() && (imageResource().imageHasRelativeWidth() || imageResource().imageHasRelativeHeight())) {
        RenderObject* containingBlock = isOutOfFlowPositioned() ? container() : this->containingBlock();
        if (auto* box = dynamicDowncast<RenderBox>(*containingBlock)) {
            intrinsicSize.setWidth(box->contentBoxLogicalWidth());
            intrinsicSize.setHeight(box->availableLogicalHeight(AvailableLogicalHeightType::IncludeMarginBorderPadding));
        }
    }

    return intrinsicSize;
}

FloatSize RenderImage::preferredAspectRatio() const
{
    ASSERT(!shouldApplySizeContainment());

    // Don't compute an intrinsic ratio to preserve historical WebKit behavior if we're painting alt text and/or a broken image.
    if (shouldDisplayBrokenImageIcon()) {
        if (style().aspectRatio().isAutoAndRatio() && !isShowingAltText())
            return FloatSize::narrowPrecision(style().aspectRatioLogicalWidth().value, style().aspectRatioLogicalHeight().value);
        return { 1.0, 1.0 };
    }

    return RenderReplaced::preferredAspectRatio();
}

bool RenderImage::shouldInvalidatePreferredWidths() const
{
    if (RenderReplaced::shouldInvalidatePreferredWidths())
        return true;
    return embeddedContentBox();
}

RenderBox* RenderImage::embeddedContentBox() const
{
    if (auto* cachedImage = this->cachedImage()) {
        if (auto* image = dynamicDowncast<SVGImage>(cachedImage->image()))
            return image->embeddedContentBox();
    }
    return nullptr;
}

CheckedRef<RenderImageResource> RenderImage::checkedImageResource() const
{
    return *m_imageResource;
}

} // namespace WebCore
