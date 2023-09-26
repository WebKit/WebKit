/*
 * Copyright (C) 2010, 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextIndicator.h"

#include "BitmapImage.h"
#include "ColorBlending.h"
#include "ColorHash.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "ElementAncestorIteratorInlines.h"
#include "FrameSelection.h"
#include "FrameSnapshotting.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NodeTraversal.h"
#include "Range.h"
#include "RenderElement.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "TextIterator.h"
#include "TextPaintStyle.h"

#if PLATFORM(IOS_FAMILY)
#include "SelectionGeometry.h"
#endif

namespace WebCore {

static bool initializeIndicator(TextIndicatorData&, LocalFrame&, const SimpleRange&, FloatSize margin, bool indicatesCurrentSelection);

TextIndicator::TextIndicator(const TextIndicatorData& data)
    : m_data(data)
{
}

TextIndicator::~TextIndicator() = default;

Ref<TextIndicator> TextIndicator::create(const TextIndicatorData& data)
{
    return adoptRef(*new TextIndicator(data));
}

RefPtr<TextIndicator> TextIndicator::createWithRange(const SimpleRange& range, OptionSet<TextIndicatorOption> options, TextIndicatorPresentationTransition presentationTransition, FloatSize margin)
{
    auto rangeToUse = range;
    if (options.contains(TextIndicatorOption::UseUserSelectAllCommonAncestor)) {
        if (auto* commonAncestor = commonInclusiveAncestor<ComposedTree>(range)) {
            Ref indicatorNode = *commonAncestor;

            for (auto& ancestorElement : ancestorsOfType<Element>(*commonAncestor)) {
                if (auto* renderer = ancestorElement.renderer(); renderer && renderer->style().effectiveUserSelect() == UserSelect::All)
                    indicatorNode = ancestorElement;
            }

            rangeToUse = makeRangeSelectingNodeContents(indicatorNode.get());
        }
    }

    RefPtr frame = rangeToUse.startContainer().document().frame();
    if (!frame)
        return nullptr;

    RefPtr document = frame->document();
    if (!document)
        return nullptr;

    bool indicatesCurrentSelection = rangeToUse == document->selection().selection().toNormalizedRange();

    OptionSet<TemporarySelectionOption> temporarySelectionOptions;
    temporarySelectionOptions.add(TemporarySelectionOption::DoNotSetFocus);
    temporarySelectionOptions.add(TemporarySelectionOption::IgnoreSelectionChanges);
#if PLATFORM(IOS_FAMILY)
    temporarySelectionOptions.add(TemporarySelectionOption::EnableAppearanceUpdates);
#endif
    TemporarySelectionChange selectionChange(*document, { rangeToUse }, temporarySelectionOptions);

    TextIndicatorData data;

    data.presentationTransition = presentationTransition;
    data.options = options;

    if (!initializeIndicator(data, *frame, rangeToUse, margin, indicatesCurrentSelection))
        return nullptr;

    return TextIndicator::create(data);
}

RefPtr<TextIndicator> TextIndicator::createWithSelectionInFrame(LocalFrame& frame, OptionSet<TextIndicatorOption> options, TextIndicatorPresentationTransition presentationTransition, FloatSize margin)
{
    auto range = frame.selection().selection().toNormalizedRange();
    if (!range)
        return nullptr;

    TextIndicatorData data;

    data.presentationTransition = presentationTransition;
    data.options = options;

    if (!initializeIndicator(data, frame, *range, margin, true))
        return nullptr;

    return TextIndicator::create(data);
}

static bool hasNonInlineOrReplacedElements(const SimpleRange& range)
{
    for (auto& node : intersectingNodes(range)) {
        auto renderer = node.renderer();
        if (renderer && (!renderer->isInline() || renderer->isReplacedOrInlineBlock()))
            return true;
    }
    return false;
}

static SnapshotOptions snapshotOptionsForTextIndicatorOptions(OptionSet<TextIndicatorOption> options)
{
    SnapshotOptions snapshotOptions { { SnapshotFlags::PaintWithIntegralScaleFactor }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };

    if (!options.contains(TextIndicatorOption::PaintAllContent)) {
        if (options.contains(TextIndicatorOption::PaintBackgrounds))
            snapshotOptions.flags.add(SnapshotFlags::PaintSelectionAndBackgroundsOnly);
        else {
            snapshotOptions.flags.add(SnapshotFlags::PaintSelectionOnly);

            if (!options.contains(TextIndicatorOption::RespectTextColor))
                snapshotOptions.flags.add(SnapshotFlags::ForceBlackText);
        }
    } else
        snapshotOptions.flags.add(SnapshotFlags::ExcludeSelectionHighlighting);

    return snapshotOptions;
}

static RefPtr<Image> takeSnapshot(LocalFrame& frame, IntRect rect, SnapshotOptions&& options, float& scaleFactor, const Vector<FloatRect>& clipRectsInDocumentCoordinates)
{
    auto buffer = snapshotFrameRectWithClip(frame, rect, clipRectsInDocumentCoordinates, WTFMove(options));
    if (!buffer)
        return nullptr;
    scaleFactor = buffer->resolutionScale();
    return BitmapImage::create(ImageBuffer::sinkIntoNativeImage(WTFMove(buffer)));
}

static bool takeSnapshots(TextIndicatorData& data, LocalFrame& frame, IntRect snapshotRect, const Vector<FloatRect>& clipRectsInDocumentCoordinates)
{
    data.contentImage = takeSnapshot(frame, snapshotRect, snapshotOptionsForTextIndicatorOptions(data.options), data.contentImageScaleFactor, clipRectsInDocumentCoordinates);
    if (!data.contentImage)
        return false;

    if (data.options.contains(TextIndicatorOption::IncludeSnapshotWithSelectionHighlight)) {
        float snapshotScaleFactor;
        data.contentImageWithHighlight = takeSnapshot(frame, snapshotRect, { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, snapshotScaleFactor, clipRectsInDocumentCoordinates);
        ASSERT(!data.contentImageWithHighlight || data.contentImageScaleFactor >= snapshotScaleFactor);
    }

    if (data.options.contains(TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection)) {
        float snapshotScaleFactor;
        auto snapshotRect = frame.view()->visibleContentRect();
        data.contentImageWithoutSelection = takeSnapshot(frame, snapshotRect, { { SnapshotFlags::PaintEverythingExcludingSelection }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, snapshotScaleFactor, { });
        data.contentImageWithoutSelectionRectInRootViewCoordinates = frame.view()->contentsToRootView(snapshotRect);
    }
    
    return true;
}

static HashSet<Color> estimatedTextColorsForRange(const SimpleRange& range)
{
    HashSet<Color> colors;
    for (TextIterator iterator(range); !iterator.atEnd(); iterator.advance()) {
        auto node = iterator.node();
        if (!node)
            continue;
        auto renderer = node->renderer();
        if (is<RenderText>(renderer))
            colors.add(renderer->style().color());
    }
    return colors;
}

static FloatRect absoluteBoundingRectForRange(const SimpleRange& range)
{
    return unionRectIgnoringZeroRects(RenderObject::absoluteBorderAndTextRects(range, {
        RenderObject::BoundingRectBehavior::RespectClipping,
        RenderObject::BoundingRectBehavior::UseVisibleBounds,
        RenderObject::BoundingRectBehavior::IgnoreTinyRects,
    }));
}

static bool hasAnyIllegibleColors(TextIndicatorData& data, const Color& backgroundColor, HashSet<Color>&& textColors)
{
    if (data.options.contains(TextIndicatorOption::PaintAllContent))
        return false;

    if (!data.options.contains(TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges))
        return false;

    if (!data.options.contains(TextIndicatorOption::ComputeEstimatedBackgroundColor))
        return false;

    bool hasOnlyLegibleTextColors = true;
    if (data.options.contains(TextIndicatorOption::RespectTextColor)) {
        for (auto& textColor : textColors) {
            hasOnlyLegibleTextColors = textColorIsLegibleAgainstBackgroundColor(textColor, backgroundColor);
            if (!hasOnlyLegibleTextColors)
                break;
        }
    } else
        hasOnlyLegibleTextColors = textColorIsLegibleAgainstBackgroundColor(Color::black, backgroundColor);

    return !hasOnlyLegibleTextColors || textColors.isEmpty();
}

static bool containsOnlyWhiteSpaceText(const SimpleRange& range)
{
    for (auto& node : intersectingNodes(range)) {
        if (!is<RenderText>(node.renderer()))
            return false;
    }
    return plainTextReplacingNoBreakSpace(range).find(deprecatedIsNotSpaceOrNewline) == notFound;
}

static bool initializeIndicator(TextIndicatorData& data, LocalFrame& frame, const SimpleRange& range, FloatSize margin, bool indicatesCurrentSelection)
{
    if (auto* document = frame.document())
        document->updateLayoutIgnorePendingStylesheets();

    bool treatRangeAsComplexDueToIllegibleTextColors = false;
    if (data.options.contains(TextIndicatorOption::ComputeEstimatedBackgroundColor)) {
        data.estimatedBackgroundColor = estimatedBackgroundColorForRange(range, frame);
        treatRangeAsComplexDueToIllegibleTextColors = hasAnyIllegibleColors(data, data.estimatedBackgroundColor, estimatedTextColorsForRange(range));
    }

    // FIXME (138888): Ideally we wouldn't remove the margin in this case, but we need to
    // ensure that the indicator and indicator-with-highlight overlap precisely, and
    // we can't add a margin to the indicator-with-highlight.
    if (indicatesCurrentSelection && !data.options.contains(TextIndicatorOption::IncludeMarginIfRangeMatchesSelection))
        margin = FloatSize();

    Vector<FloatRect> textRects;

    bool useBoundingRectAndPaintAllContentForComplexRanges = data.options.contains(TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges);
    if (useBoundingRectAndPaintAllContentForComplexRanges && containsOnlyWhiteSpaceText(range)) {
        if (auto* containerRenderer = commonInclusiveAncestor<ComposedTree>(range)->renderer()) {
            data.options.add(TextIndicatorOption::PaintAllContent);
            textRects.append(containerRenderer->absoluteBoundingBoxRect());
        }
    } else if (useBoundingRectAndPaintAllContentForComplexRanges && (treatRangeAsComplexDueToIllegibleTextColors || hasNonInlineOrReplacedElements(range)))
        data.options.add(TextIndicatorOption::PaintAllContent);
#if PLATFORM(IOS_FAMILY)
    else if (data.options.contains(TextIndicatorOption::UseSelectionRectForSizing)) {
        textRects = RenderObject::collectSelectionGeometries(range).map([&](auto& geometry) -> FloatRect {
            return geometry.rect();
        });
    }
#endif
    else {
        OptionSet<RenderObject::BoundingRectBehavior> behavior { RenderObject::BoundingRectBehavior::RespectClipping };
        if (!data.options.contains(TextIndicatorOption::TightlyFitContent))
            behavior.add(RenderObject::BoundingRectBehavior::UseSelectionHeight);
        textRects = RenderObject::absoluteTextRects(range, behavior).map([&](auto& rect) -> FloatRect {
            return rect;
        });
    }

    if (textRects.isEmpty())
        textRects.append(absoluteBoundingRectForRange(range));

    auto frameView = frame.view();

    // Use the exposedContentRect/viewExposedRect instead of visibleContentRect to avoid creating a huge indicator for a large view inside a scroll view.
    IntRect contentsClipRect;
#if PLATFORM(IOS_FAMILY)
    contentsClipRect = enclosingIntRect(frameView->exposedContentRect());
#else
    if (auto viewExposedRect = frameView->viewExposedRect())
        contentsClipRect = enclosingIntRect(*viewExposedRect);
    else
        contentsClipRect = frameView->visibleContentRect();
#endif

    if (data.options.contains(TextIndicatorOption::ExpandClipBeyondVisibleRect)) {
        contentsClipRect.inflateX(contentsClipRect.width() / 2);
        contentsClipRect.inflateY(contentsClipRect.height() / 2);
    }

    FloatRect textBoundingRectInRootViewCoordinates;
    FloatRect textBoundingRectInDocumentCoordinates;
    Vector<FloatRect> clippedTextRectsInDocumentCoordinates;
    Vector<FloatRect> textRectsInRootViewCoordinates;
    for (const FloatRect& textRect : textRects) {
        FloatRect clippedTextRect;
        if (data.options.contains(TextIndicatorOption::DoNotClipToVisibleRect))
            clippedTextRect = textRect;
        else
            clippedTextRect = intersection(textRect, contentsClipRect);
        if (clippedTextRect.isEmpty())
            continue;

        clippedTextRectsInDocumentCoordinates.append(clippedTextRect);

        FloatRect textRectInDocumentCoordinatesIncludingMargin = clippedTextRect;
        textRectInDocumentCoordinatesIncludingMargin.inflateX(margin.width());
        textRectInDocumentCoordinatesIncludingMargin.inflateY(margin.height());
        textBoundingRectInDocumentCoordinates.unite(textRectInDocumentCoordinatesIncludingMargin);

        FloatRect textRectInRootViewCoordinates = frame.view()->contentsToRootView(enclosingIntRect(textRectInDocumentCoordinatesIncludingMargin));
        textRectsInRootViewCoordinates.append(textRectInRootViewCoordinates);
        textBoundingRectInRootViewCoordinates.unite(textRectInRootViewCoordinates);
    }

    auto textRectsInBoundingRectCoordinates = textRectsInRootViewCoordinates.map([&](auto rect) {
        rect.moveBy(-textBoundingRectInRootViewCoordinates.location());
        return rect;
    });

    // Store the selection rect in window coordinates, to be used subsequently
    // to determine if the indicator and selection still precisely overlap.
    data.selectionRectInRootViewCoordinates = frame.view()->contentsToRootView(enclosingIntRect(frame.selection().selectionBounds(FrameSelection::ClipToVisibleContent::No)));
    data.textBoundingRectInRootViewCoordinates = textBoundingRectInRootViewCoordinates;
    data.textRectsInBoundingRectCoordinates = WTFMove(textRectsInBoundingRectCoordinates);

    return takeSnapshots(data, frame, enclosingIntRect(textBoundingRectInDocumentCoordinates), clippedTextRectsInDocumentCoordinates);
}

} // namespace WebCore
