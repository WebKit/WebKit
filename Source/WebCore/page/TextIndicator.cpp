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

#include "ColorBlending.h"
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

static bool initializeIndicator(TextIndicatorData&, Frame&, const SimpleRange&, FloatSize margin, bool indicatesCurrentSelection);

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
    auto frame = makeRefPtr(range.startContainer().document().frame());
    if (!frame)
        return nullptr;

    auto document = makeRefPtr(frame->document());
    if (!document)
        return nullptr;

    bool indicatesCurrentSelection = range == document->selection().selection().toNormalizedRange();

    OptionSet<TemporarySelectionOption> temporarySelectionOptions;
    temporarySelectionOptions.add(TemporarySelectionOption::DoNotSetFocus);
#if PLATFORM(IOS_FAMILY)
    temporarySelectionOptions.add(TemporarySelectionOption::IgnoreSelectionChanges);
    temporarySelectionOptions.add(TemporarySelectionOption::EnableAppearanceUpdates);
#endif
    TemporarySelectionChange selectionChange(*document, { range }, temporarySelectionOptions);

    TextIndicatorData data;

    data.presentationTransition = presentationTransition;
    data.options = options;

    if (!initializeIndicator(data, *frame, range, margin, indicatesCurrentSelection))
        return nullptr;

    return TextIndicator::create(data);
}

RefPtr<TextIndicator> TextIndicator::createWithSelectionInFrame(Frame& frame, OptionSet<TextIndicatorOption> options, TextIndicatorPresentationTransition presentationTransition, FloatSize margin)
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
        if (renderer && (!renderer->isInline() || renderer->isReplaced()))
            return true;
    }
    return false;
}

static SnapshotOptions snapshotOptionsForTextIndicatorOptions(OptionSet<TextIndicatorOption> options)
{
    SnapshotOptions snapshotOptions = SnapshotOptionsPaintWithIntegralScaleFactor;

    if (!options.contains(TextIndicatorOption::PaintAllContent)) {
        if (options.contains(TextIndicatorOption::PaintBackgrounds))
            snapshotOptions |= SnapshotOptionsPaintSelectionAndBackgroundsOnly;
        else {
            snapshotOptions |= SnapshotOptionsPaintSelectionOnly;

            if (!options.contains(TextIndicatorOption::RespectTextColor))
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

    if (data.options.contains(TextIndicatorOption::IncludeSnapshotWithSelectionHighlight)) {
        float snapshotScaleFactor;
        data.contentImageWithHighlight = takeSnapshot(frame, snapshotRect, SnapshotOptionsNone, snapshotScaleFactor, clipRectsInDocumentCoordinates);
        ASSERT(!data.contentImageWithHighlight || data.contentImageScaleFactor >= snapshotScaleFactor);
    }

    if (data.options.contains(TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection)) {
        float snapshotScaleFactor;
        auto snapshotRect = frame.view()->visibleContentRect();
        data.contentImageWithoutSelection = takeSnapshot(frame, snapshotRect, SnapshotOptionsPaintEverythingExcludingSelection, snapshotScaleFactor, { });
        data.contentImageWithoutSelectionRectInRootViewCoordinates = frame.view()->contentsToRootView(snapshotRect);
    }
    
    return true;
}

static bool styleContainsComplexBackground(const RenderStyle& style)
{
    return style.hasBlendMode() || style.hasBackgroundImage() || style.hasBackdropFilter();
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

static Color estimatedBackgroundColorForRange(const SimpleRange& range, const Frame& frame)
{
    auto estimatedBackgroundColor = frame.view() ? frame.view()->documentBackgroundColor() : Color::transparentBlack;

    RenderElement* renderer = nullptr;
    auto commonAncestor = commonInclusiveAncestor(range);
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
        if (visitedDependentBackgroundColor != Color::transparentBlack)
            parentRendererBackgroundColors.append(visitedDependentBackgroundColor);
    }
    parentRendererBackgroundColors.reverse();
    for (const auto& backgroundColor : parentRendererBackgroundColors)
        estimatedBackgroundColor = blendSourceOver(estimatedBackgroundColor, backgroundColor);

    return estimatedBackgroundColor;
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
    return plainTextReplacingNoBreakSpace(range).stripWhiteSpace().isEmpty();
}

static bool initializeIndicator(TextIndicatorData& data, Frame& frame, const SimpleRange& range, FloatSize margin, bool indicatesCurrentSelection)
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
        if (auto* containerRenderer = commonInclusiveAncestor(range)->renderer()) {
            data.options.add(TextIndicatorOption::PaintAllContent);
            textRects.append(containerRenderer->absoluteBoundingBoxRect());
        }
    } else if (useBoundingRectAndPaintAllContentForComplexRanges && (treatRangeAsComplexDueToIllegibleTextColors || hasNonInlineOrReplacedElements(range)))
        data.options.add(TextIndicatorOption::PaintAllContent);
#if PLATFORM(IOS_FAMILY)
    else if (data.options.contains(TextIndicatorOption::UseSelectionRectForSizing)) {
        textRects = RenderObject::collectSelectionRects(range).map([&](auto& rect) -> FloatRect {
            return rect.rect();
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
