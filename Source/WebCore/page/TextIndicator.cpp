/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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
#include "Page.h"
#include "Range.h"
#include "RenderObject.h"

using namespace WebCore;

namespace WebCore {

static bool initializeIndicator(TextIndicatorData&, Frame&, const Range&, FloatSize margin, bool indicatesCurrentSelection);

TextIndicator::TextIndicator(const TextIndicatorData& data)
    : m_data(data)
{
}

TextIndicator::~TextIndicator()
{
}

Ref<TextIndicator> TextIndicator::create(const TextIndicatorData& data)
{
    return adoptRef(*new TextIndicator(data));
}

RefPtr<TextIndicator> TextIndicator::createWithRange(const Range& range, TextIndicatorOptions options, TextIndicatorPresentationTransition presentationTransition, FloatSize margin)
{
    Frame* frame = range.startContainer().document().frame();

    if (!frame)
        return nullptr;

#if PLATFORM(IOS)
    frame->editor().setIgnoreCompositionSelectionChange(true);
    frame->selection().setUpdateAppearanceEnabled(true);
#endif

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(range);

    TextIndicatorData data;

    data.presentationTransition = presentationTransition;
    data.options = options;

    bool indicatesCurrentSelection = areRangesEqual(&range, oldSelection.toNormalizedRange().get());

    if (!initializeIndicator(data, *frame, range, margin, indicatesCurrentSelection))
        return nullptr;

    RefPtr<TextIndicator> indicator = TextIndicator::create(data);

    frame->selection().setSelection(oldSelection);

#if PLATFORM(IOS)
    frame->editor().setIgnoreCompositionSelectionChange(false, Editor::RevealSelection::No);
    frame->selection().setUpdateAppearanceEnabled(false);
#endif

    return indicator;
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
        if ((!renderer->isInline() || renderer->isReplaced()) && range.intersectsNode(node, ASSERT_NO_EXCEPTION))
            return true;
    }

    return false;
}

static SnapshotOptions snapshotOptionsForTextIndicatorOptions(TextIndicatorOptions options)
{
    SnapshotOptions snapshotOptions = SnapshotOptionsNone;
    if (!(options & TextIndicatorOptionRespectTextColor))
        snapshotOptions |= SnapshotOptionsForceBlackText;

    if (!(options & TextIndicatorOptionPaintAllContent)) {
        if (options & TextIndicatorOptionPaintBackgrounds)
            snapshotOptions |= SnapshotOptionsPaintSelectionAndBackgroundsOnly;
        else
            snapshotOptions |= SnapshotOptionsPaintSelectionOnly;
    } else
        snapshotOptions |= SnapshotOptionsExcludeSelectionHighlighting;

    return snapshotOptions;
}

static RefPtr<Image> takeSnapshot(Frame& frame, IntRect rect, SnapshotOptions options, float& scaleFactor)
{
    std::unique_ptr<ImageBuffer> buffer = snapshotFrameRect(frame, rect, options);
    if (!buffer)
        return nullptr;
    scaleFactor = buffer->resolutionScale();
    return ImageBuffer::sinkIntoImage(WTF::move(buffer), Unscaled);
}

static bool takeSnapshots(TextIndicatorData& data, Frame& frame, IntRect snapshotRect)
{
    SnapshotOptions snapshotOptions = snapshotOptionsForTextIndicatorOptions(data.options);

    data.contentImage = takeSnapshot(frame, snapshotRect, snapshotOptions, data.contentImageScaleFactor);
    if (!data.contentImage)
        return false;

    if (data.options & TextIndicatorOptionIncludeSnapshotWithSelectionHighlight) {
        float snapshotScaleFactor;
        data.contentImageWithHighlight = takeSnapshot(frame, snapshotRect, SnapshotOptionsNone, snapshotScaleFactor);
        ASSERT(!data.contentImageWithHighlight || data.contentImageScaleFactor == snapshotScaleFactor);
    }
    
    return true;
}

static bool initializeIndicator(TextIndicatorData& data, Frame& frame, const Range& range, FloatSize margin, bool indicatesCurrentSelection)
{
    Vector<FloatRect> textRects;

    // FIXME (138888): Ideally we wouldn't remove the margin in this case, but we need to
    // ensure that the indicator and indicator-with-highlight overlap precisely, and
    // we can't add a margin to the indicator-with-highlight.
    if (indicatesCurrentSelection && !(data.options & TextIndicatorOptionIncludeMarginIfRangeMatchesSelection))
        margin = FloatSize();

    FrameSelection::TextRectangleHeight textRectHeight = (data.options & TextIndicatorOptionTightlyFitContent) ? FrameSelection::TextRectangleHeight::TextHeight : FrameSelection::TextRectangleHeight::SelectionHeight;

    if ((data.options & TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges) && hasNonInlineOrReplacedElements(range))
        data.options |= TextIndicatorOptionPaintAllContent;
    else {
        if (data.options & TextIndicatorOptionDoNotClipToVisibleRect)
            frame.selection().getTextRectangles(textRects, textRectHeight);
        else
            frame.selection().getClippedVisibleTextRectangles(textRects, textRectHeight);
    }

    if (textRects.isEmpty()) {
        RenderView* renderView = frame.contentRenderer();
        if (!renderView)
            return false;
        FloatRect boundingRect = range.absoluteBoundingRect();
        if (data.options & TextIndicatorOptionDoNotClipToVisibleRect)
            textRects.append(boundingRect);
        else {
            // Clip to the visible rect, just like getClippedVisibleTextRectangles does.
            // FIXME: We really want to clip to the unobscured rect in both cases, I think.
            // (this seems to work on Mac, but maybe not iOS?)
            FloatRect visibleContentRect = frame.view()->visibleContentRect(ScrollableArea::LegacyIOSDocumentVisibleRect);
            textRects.append(intersection(visibleContentRect, boundingRect));
        }
    }

    FloatRect textBoundingRectInRootViewCoordinates;
    FloatRect textBoundingRectInDocumentCoordinates;
    Vector<FloatRect> textRectsInRootViewCoordinates;
    for (const FloatRect& textRect : textRects) {
        FloatRect textRectInDocumentCoordinatesIncludingMargin = textRect;
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
    data.selectionRectInRootViewCoordinates = frame.view()->contentsToRootView(enclosingIntRect(frame.selection().selectionBounds()));
    data.textBoundingRectInRootViewCoordinates = textBoundingRectInRootViewCoordinates;
    data.textRectsInBoundingRectCoordinates = textRectsInBoundingRectCoordinates;

    return takeSnapshots(data, frame, enclosingIntRect(textBoundingRectInDocumentCoordinates));
}

} // namespace WebCore
