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
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameSnapshotting.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "Page.h"
#include "Range.h"

using namespace WebCore;

namespace WebCore {

Ref<TextIndicator> TextIndicator::create(const TextIndicatorData& data)
{
    return adoptRef(*new TextIndicator(data));
}

RefPtr<TextIndicator> TextIndicator::createWithRange(const Range& range, TextIndicatorPresentationTransition presentationTransition, unsigned margin)
{
    Frame* frame = range.startContainer()->document().frame();

    if (!frame)
        return nullptr;

#if PLATFORM(IOS)
    frame->editor().setIgnoreCompositionSelectionChange(true);
    frame->selection().setUpdateAppearanceEnabled(true);
#endif

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(range);

    RefPtr<TextIndicator> indicator = TextIndicator::createWithSelectionInFrame(*frame, presentationTransition, margin);

    frame->selection().setSelection(oldSelection);

    if (indicator)
        indicator->setWantsMargin(!areRangesEqual(&range, oldSelection.toNormalizedRange().get()));

#if PLATFORM(IOS)
    frame->editor().setIgnoreCompositionSelectionChange(false, Editor::RevealSelection::No);
    frame->selection().setUpdateAppearanceEnabled(false);
#endif

    return indicator.release();
}

// FIXME (138889): Ideally the FrameSnapshotting functions would be more flexible
// and we wouldn't have to implement this here.
static RefPtr<Image> snapshotSelectionWithHighlight(Frame& frame)
{
    auto& selection = frame.selection();

    if (!selection.isRange())
        return nullptr;

    FloatRect selectionBounds = selection.selectionBounds();

    // It is possible for the selection bounds to be empty; see https://bugs.webkit.org/show_bug.cgi?id=56645.
    if (selectionBounds.isEmpty())
        return nullptr;

    std::unique_ptr<ImageBuffer> snapshot = snapshotFrameRect(frame, enclosingIntRect(selectionBounds), 0);

    if (!snapshot)
        return nullptr;

    return snapshot->copyImage(CopyBackingStore, Unscaled);
}

RefPtr<TextIndicator> TextIndicator::createWithSelectionInFrame(Frame& frame, TextIndicatorPresentationTransition presentationTransition, unsigned margin)
{
    Vector<FloatRect> textRects;

    // On iOS, we don't need to expand the TextIndicator to cover the whole selection height.
    // FIXME: Ideally, on Mac, there are times when we don't need to (if we don't have a selection),
    // and using TextHeight would provide a more sensible appearance.
#if PLATFORM(IOS)
    FrameSelection::TextRectangleHeight textRectHeight = FrameSelection::TextRectangleHeight::TextHeight;
#else
    FrameSelection::TextRectangleHeight textRectHeight = FrameSelection::TextRectangleHeight::SelectionHeight;
#endif
    frame.selection().getClippedVisibleTextRectangles(textRects, textRectHeight);

    // The bounding rect of all the text rects can be different than the selection
    // rect when the selection spans multiple lines; the indicator doesn't actually
    // care where the selection highlight goes, just where the text actually is.
    FloatRect textBoundingRectInRootViewCoordinates;
    FloatRect textBoundingRectInDocumentCoordinates;
    Vector<FloatRect> textRectsInRootViewCoordinates;
    for (const FloatRect& textRect : textRects) {
        FloatRect textRectInDocumentCoordinatesIncludingMargin = textRect;
        textRectInDocumentCoordinatesIncludingMargin.inflate(margin);

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

    // FIXME: We should have TextIndicator options instead of this being platform-specific.
#if PLATFORM(IOS)
    SnapshotOptions snapshotOptions = SnapshotOptionsPaintSelectionAndBackgroundsOnly;
#else
    SnapshotOptions snapshotOptions = SnapshotOptionsForceBlackText | SnapshotOptionsPaintSelectionOnly;
#endif

    std::unique_ptr<ImageBuffer> indicatorBuffer = snapshotFrameRect(frame, enclosingIntRect(textBoundingRectInDocumentCoordinates), snapshotOptions);
    if (!indicatorBuffer)
        return nullptr;
    RefPtr<Image> indicatorBitmap = indicatorBuffer->copyImage(CopyBackingStore, Unscaled);
    if (!indicatorBitmap)
        return nullptr;

    RefPtr<Image> indicatorBitmapWithHighlight;
    if (presentationTransition == TextIndicatorPresentationTransition::BounceAndCrossfade)
        indicatorBitmapWithHighlight = snapshotSelectionWithHighlight(frame);

    TextIndicatorData data;

    // Store the selection rect in window coordinates, to be used subsequently
    // to determine if the indicator and selection still precisely overlap.
    data.selectionRectInRootViewCoordinates = frame.view()->contentsToRootView(enclosingIntRect(frame.selection().selectionBounds()));
    data.textBoundingRectInRootViewCoordinates = textBoundingRectInRootViewCoordinates;
    data.textRectsInBoundingRectCoordinates = textRectsInBoundingRectCoordinates;
    data.contentImageScaleFactor = indicatorBuffer->resolutionScale();
    data.contentImage = indicatorBitmap;
    data.contentImageWithHighlight = indicatorBitmapWithHighlight;
    data.presentationTransition = presentationTransition;
    data.wantsMargin = true;

    return TextIndicator::create(data);
}

TextIndicator::TextIndicator(const TextIndicatorData& data)
    : m_data(data)
{
}

TextIndicator::~TextIndicator()
{
}
    
bool TextIndicator::wantsBounce() const
{
    switch (m_data.presentationTransition) {
    case TextIndicatorPresentationTransition::BounceAndCrossfade:
    case TextIndicatorPresentationTransition::Bounce:
        return true;
        
    case TextIndicatorPresentationTransition::FadeIn:
    case TextIndicatorPresentationTransition::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool TextIndicator::wantsContentCrossfade() const
{
    if (!m_data.contentImageWithHighlight)
        return false;
    
    switch (m_data.presentationTransition) {
    case TextIndicatorPresentationTransition::BounceAndCrossfade:
        return true;
        
    case TextIndicatorPresentationTransition::Bounce:
    case TextIndicatorPresentationTransition::FadeIn:
    case TextIndicatorPresentationTransition::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool TextIndicator::wantsFadeIn() const
{
    switch (m_data.presentationTransition) {
    case TextIndicatorPresentationTransition::FadeIn:
        return true;
        
    case TextIndicatorPresentationTransition::Bounce:
    case TextIndicatorPresentationTransition::BounceAndCrossfade:
    case TextIndicatorPresentationTransition::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

bool TextIndicator::wantsManualAnimation() const
{
    switch (m_data.presentationTransition) {
    case TextIndicatorPresentationTransition::FadeIn:
        return true;

    case TextIndicatorPresentationTransition::Bounce:
    case TextIndicatorPresentationTransition::BounceAndCrossfade:
    case TextIndicatorPresentationTransition::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
