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

#include "ColorHash.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameSnapshotting.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "NodeTraversal.h"
#include "Range.h"
#include "RenderElement.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "TextIterator.h"
#include "TextPaintStyle.h"

#if PLATFORM(IOS_FAMILY)
#include "SelectionRect.h"
#endif

namespace WebCore {

static bool initializeIndicator(TextIndicatorData&, Frame&, const Range&, FloatSize margin, bool indicatesCurrentSelection);

TextIndicator::TextIndicator(const TextIndicatorData& data)
    : m_data(data)
{
}

TextIndicator::~TextIndicator() = default;

Ref<TextIndicator> TextIndicator::create(const TextIndicatorData& data)
{
    return adoptRef(*new TextIndicator(data));
}

RefPtr<TextIndicator> TextIndicator::createWithRange(const Range& range, TextIndicatorOptions options, TextIndicatorPresentationTransition presentationTransition, FloatSize margin)
{
    Frame* frame = range.startContainer().document().frame();

    if (!frame)
        return nullptr;

    Ref<Frame> protector(*frame);

    VisibleSelection oldSelection = frame->selection().selection();
    OptionSet<TemporarySelectionOption> temporarySelectionOptions;
    temporarySelectionOptions.add(TemporarySelectionOption::DoNotSetFocus);
#if PLATFORM(IOS_FAMILY)
    temporarySelectionOptions.add(TemporarySelectionOption::IgnoreSelectionChanges);
    temporarySelectionOptions.add(TemporarySelectionOption::EnableAppearanceUpdates);
#endif
    TemporarySelectionChange selectionChange(*frame, { range }, temporarySelectionOptions);

    TextIndicatorData data;

    data.presentationTransition = presentationTransition;
    data.options = options;

    bool indicatesCurrentSelection = areRangesEqual(&range, oldSelection.toNormalizedRange().get());

    if (!initializeIndicator(data, *frame, range, margin, indicatesCurrentSelection))
        return nullptr;

    return TextIndicator::create(data);
}

RefPtr<TextIndicator> TextIndicator::createWithSelectionInFrame(Frame& frame, TextIndicatorOptions options, TextIndicatorPresentationTransition presentationTransition, FloatSize margin)
{
    RefPtr<Range> range = frame.selection().toNormalizedRange();
    if (!range)
        return nullptr;

    TextIndicatorData data;

    data.presentationTransition = presentationTransition;
    data.options = options;

    if (!initializeIndicator(data, frame, *range, margin, true))
        return nullptr;

    return TextIndicator::create(data);
}

static bool hasNonInlineOrReplacedElements(const Range& range)
{
    Node* stopNode = range.pastLastNode();
    for (Node* node = range.firstNode(); node != stopNode; node = NodeTraversal::next(*node)) {
        if (!node)
            continue;
        RenderObject* renderer = node->renderer();
        if (!renderer)
            continue;
        if ((!renderer->isInline() || renderer->isReplaced()) && range.intersectsNode(*node).releaseReturnValue())
            return true;
    }

    return false;
}

static SnapshotOptions snapshotOptionsForTextIndicatorOptions(TextIndicatorOptions options)
{
    SnapshotOptions snapshotOptions = SnapshotOptionsPaintWithIntegralScaleFactor;

    if (!(options & TextIndicatorOptionPaintAllContent)) {
        if (options & TextIndicatorOptionPaintBackgrounds)
            snapshotOptions |= SnapshotOptionsPaintSelectionAndBackgroundsOnly;
        else {
            snapshotOptions |= SnapshotOptionsPaintSelectionOnly;

            if (!(options & TextIndicatorOptionRespectTextColor))
                snapshotOptions |= SnapshotOptionsForceBlackText;
        }
    } else
        snapshotOptions |= SnapshotOptionsExcludeSelectionHighlighting;

    return snapshotOptions;
}

static RefPtr<Image> takeSnapshot(Frame& frame, IntRect rect, SnapshotOptions options, float& scaleFactor, const Vector<FloatRect>& clipRectsInDocumentCoordinates)
{
    std::unique_ptr<ImageBuffer> buffer = snapshotFrameRectWithClip(frame, rect, clipRectsInDocumentCoordinates, options);
    if (!buffer)
        return nullptr;
    scaleFactor = buffer->resolutionScale();
    return ImageBuffer::sinkIntoImage(WTFMove(buffer), PreserveResolution::Yes);
}

static bool takeSnapshots(TextIndicatorData& data, Frame& frame, IntRect snapshotRect, const Vector<FloatRect>& clipRectsInDocumentCoordinates)
{
    SnapshotOptions snapshotOptions = snapshotOptionsForTextIndicatorOptions(data.options);

    data.contentImage = takeSnapshot(frame, snapshotRect, snapshotOptions, data.contentImageScaleFactor, clipRectsInDocumentCoordinates);
    if (!data.contentImage)
        return false;

    if (data.options & TextIndicatorOptionIncludeSnapshotWithSelectionHighlight) {
        float snapshotScaleFactor;
        data.contentImageWithHighlight = takeSnapshot(frame, snapshotRect, SnapshotOptionsNone, snapshotScaleFactor, clipRectsInDocumentCoordinates);
        ASSERT(!data.contentImageWithHighlight || data.contentImageScaleFactor >= snapshotScaleFactor);
    }

    if (data.options & TextIndicatorOptionIncludeSnapshotOfAllVisibleContentWithoutSelection) {
        float snapshotScaleFactor;
        auto snapshotRect = frame.view()->visibleContentRect();
        data.contentImageWithoutSelection = takeSnapshot(frame, snapshotRect, SnapshotOptionsPaintEverythingExcludingSelection, snapshotScaleFactor, { });
        data.contentImageWithoutSelectionRectInRootViewCoordinates = frame.view()->contentsToRootView(snapshotRect);
    }
    
    return true;
}

#if PLATFORM(IOS_FAMILY)

static void getSelectionRectsForRange(Vector<FloatRect>& resultingRects, const Range& range)
{
    Vector<SelectionRect> selectionRectsForRange;
    Vector<FloatRect> selectionRectsForRangeInBoundingRectCoordinates;
    range.collectSelectionRects(selectionRectsForRange);
    for (auto selectionRect : selectionRectsForRange)
        resultingRects.append(selectionRect.rect());
}

#endif

static bool styleContainsComplexBackground(const RenderStyle& style)
{
    if (style.hasBlendMode())
        return true;

    if (style.hasBackgroundImage())
        return true;

    if (style.hasBackdropFilter())
        return true;

    return false;
}

static HashSet<Color> estimatedTextColorsForRange(const Range& range)
{
    HashSet<Color> colors;
    for (TextIterator iterator(&range); !iterator.atEnd(); iterator.advance()) {
        auto* node = iterator.node();
        if (!is<Text>(node) || !is<RenderText>(node->renderer()))
            continue;

        colors.add(node->renderer()->style().color());
    }
    return colors;
}
    
static FloatRect absoluteBoundingRectForRange(const Range& range)
{
    return range.absoluteBoundingRect({
        Range::BoundingRectBehavior::RespectClipping,
        Range::BoundingRectBehavior::UseVisibleBounds,
        Range::BoundingRectBehavior::IgnoreTinyRects,
    });
}

static Color estimatedBackgroundColorForRange(const Range& range, const Frame& frame)
{
    auto estimatedBackgroundColor = frame.view() ? frame.view()->documentBackgroundColor() : Color::transparent;

    RenderElement* renderer = nullptr;
    auto commonAncestor = range.commonAncestorContainer();
    while (commonAncestor) {
        if (is<RenderElement>(commonAncestor->renderer())) {
            renderer = downcast<RenderElement>(commonAncestor->renderer());
            break;
        }
        commonAncestor = commonAncestor->parentOrShadowHostElement();
    }

    auto boundingRectForRange = enclosingIntRect(absoluteBoundingRectForRange(range));
    Vector<Color> parentRendererBackgroundColors;
    for (; !!renderer; renderer = renderer->parent()) {
        auto absoluteBoundingBox = renderer->absoluteBoundingBoxRect();
        auto& style = renderer->style();
        if (!absoluteBoundingBox.contains(boundingRectForRange) || !style.hasBackground())
            continue;

        if (styleContainsComplexBackground(style))
            return estimatedBackgroundColor;

        auto visitedDependentBackgroundColor = style.visitedDependentColor(CSSPropertyBackgroundColor);
        if (visitedDependentBackgroundColor != Color::transparent)
            parentRendererBackgroundColors.append(visitedDependentBackgroundColor);
    }
    parentRendererBackgroundColors.reverse();
    for (const auto& backgroundColor : parentRendererBackgroundColors)
        estimatedBackgroundColor = estimatedBackgroundColor.blend(backgroundColor);

    return estimatedBackgroundColor;
}

static bool hasAnyIllegibleColors(TextIndicatorData& data, const Color& backgroundColor, HashSet<Color>&& textColors)
{
    if (data.options & TextIndicatorOptionPaintAllContent)
        return false;

    if (!(data.options & TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges))
        return false;

    if (!(data.options & TextIndicatorOptionComputeEstimatedBackgroundColor))
        return false;

    bool hasOnlyLegibleTextColors = true;
    if (data.options & TextIndicatorOptionRespectTextColor) {
        for (auto& textColor : textColors) {
            hasOnlyLegibleTextColors = textColorIsLegibleAgainstBackgroundColor(textColor, backgroundColor);
            if (!hasOnlyLegibleTextColors)
                break;
        }
    } else
        hasOnlyLegibleTextColors = textColorIsLegibleAgainstBackgroundColor(Color::black, backgroundColor);

    return !hasOnlyLegibleTextColors || textColors.isEmpty();
}

static bool containsOnlyWhiteSpaceText(const Range& range)
{
    auto* stop = range.pastLastNode();
    for (auto* node = range.firstNode(); node && node != stop; node = NodeTraversal::next(*node)) {
        if (!is<RenderText>(node->renderer()))
            return false;
    }
    return plainTextReplacingNoBreakSpace(&range).stripWhiteSpace().isEmpty();
}

static bool initializeIndicator(TextIndicatorData& data, Frame& frame, const Range& range, FloatSize margin, bool indicatesCurrentSelection)
{
    if (auto* document = frame.document())
        document->updateLayoutIgnorePendingStylesheets();

    bool treatRangeAsComplexDueToIllegibleTextColors = false;
    if (data.options & TextIndicatorOptionComputeEstimatedBackgroundColor) {
        data.estimatedBackgroundColor = estimatedBackgroundColorForRange(range, frame);
        treatRangeAsComplexDueToIllegibleTextColors = hasAnyIllegibleColors(data, data.estimatedBackgroundColor, estimatedTextColorsForRange(range));
    }

    Vector<FloatRect> textRects;

    // FIXME (138888): Ideally we wouldn't remove the margin in this case, but we need to
    // ensure that the indicator and indicator-with-highlight overlap precisely, and
    // we can't add a margin to the indicator-with-highlight.
    if (indicatesCurrentSelection && !(data.options & TextIndicatorOptionIncludeMarginIfRangeMatchesSelection))
        margin = FloatSize();

    FrameSelection::TextRectangleHeight textRectHeight = (data.options & TextIndicatorOptionTightlyFitContent) ? FrameSelection::TextRectangleHeight::TextHeight : FrameSelection::TextRectangleHeight::SelectionHeight;

    bool useBoundingRectAndPaintAllContentForComplexRanges = data.options & TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges;
    if (useBoundingRectAndPaintAllContentForComplexRanges && containsOnlyWhiteSpaceText(range)) {
        auto commonAncestorContainer = makeRefPtr(range.commonAncestorContainer());
        if (auto* containerRenderer = commonAncestorContainer->renderer()) {
            data.options |= TextIndicatorOptionPaintAllContent;
            textRects.append(containerRenderer->absoluteBoundingBoxRect());
        }
    } else if (useBoundingRectAndPaintAllContentForComplexRanges && (treatRangeAsComplexDueToIllegibleTextColors || hasNonInlineOrReplacedElements(range)))
        data.options |= TextIndicatorOptionPaintAllContent;
#if PLATFORM(IOS_FAMILY)
    else if (data.options & TextIndicatorOptionUseSelectionRectForSizing)
        getSelectionRectsForRange(textRects, range);
#endif
    else {
        Vector<IntRect> absoluteTextRects;
        range.absoluteTextRects(absoluteTextRects, textRectHeight == FrameSelection::TextRectangleHeight::SelectionHeight, nullptr, Range::BoundingRectBehavior::RespectClipping);

        textRects.reserveInitialCapacity(absoluteTextRects.size());
        for (auto& rect : absoluteTextRects)
            textRects.uncheckedAppend(rect);
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
        contentsClipRect = frameView->viewToContents(enclosingIntRect(*viewExposedRect));
    else
        contentsClipRect = frameView->visibleContentRect();
#endif

    if (data.options & TextIndicatorOptionExpandClipBeyondVisibleRect) {
        contentsClipRect.inflateX(contentsClipRect.width() / 2);
        contentsClipRect.inflateY(contentsClipRect.height() / 2);
    }

    FloatRect textBoundingRectInRootViewCoordinates;
    FloatRect textBoundingRectInDocumentCoordinates;
    Vector<FloatRect> clippedTextRectsInDocumentCoordinates;
    Vector<FloatRect> textRectsInRootViewCoordinates;
    for (const FloatRect& textRect : textRects) {
        FloatRect clippedTextRect;
        if (data.options & TextIndicatorOptionDoNotClipToVisibleRect)
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

    Vector<FloatRect> textRectsInBoundingRectCoordinates;
    for (auto rect : textRectsInRootViewCoordinates) {
        rect.moveBy(-textBoundingRectInRootViewCoordinates.location());
        textRectsInBoundingRectCoordinates.append(rect);
    }

    // Store the selection rect in window coordinates, to be used subsequently
    // to determine if the indicator and selection still precisely overlap.
    data.selectionRectInRootViewCoordinates = frame.view()->contentsToRootView(enclosingIntRect(frame.selection().selectionBounds(FrameSelection::ClipToVisibleContent::No)));
    data.textBoundingRectInRootViewCoordinates = textBoundingRectInRootViewCoordinates;
    data.textRectsInBoundingRectCoordinates = textRectsInBoundingRectCoordinates;

    return takeSnapshots(data, frame, enclosingIntRect(textBoundingRectInDocumentCoordinates), clippedTextRectsInDocumentCoordinates);
}

} // namespace WebCore
