/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameSnapshotting.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "Page.h"

using namespace WebCore;

// These should match the values in TextIndicatorWindow.
// FIXME: Ideally these would only be in one place.
#if ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
const float horizontalBorder = 3;
const float verticalBorder = 1;
const float dropShadowBlurRadius = 1.5;
#else
const float horizontalBorder = 2;
const float verticalBorder = 1;
const float dropShadowBlurRadius = 12;
#endif

namespace WebCore {

static FloatRect outsetIndicatorRectIncludingShadow(const FloatRect rect)
{
    FloatRect outsetRect = rect;
    outsetRect.inflateX(dropShadowBlurRadius + horizontalBorder);
    outsetRect.inflateY(dropShadowBlurRadius + verticalBorder);
    return outsetRect;
}

static bool textIndicatorsForTextRectsOverlap(const Vector<FloatRect>& textRects)
{
    size_t count = textRects.size();
    if (count <= 1)
        return false;

    Vector<FloatRect> indicatorRects;
    indicatorRects.reserveInitialCapacity(count);

    for (size_t i = 0; i < count; ++i) {
        FloatRect indicatorRect = outsetIndicatorRectIncludingShadow(textRects[i]);

        for (size_t j = indicatorRects.size(); j; ) {
            --j;
            if (indicatorRect.intersects(indicatorRects[j]))
                return true;
        }

        indicatorRects.uncheckedAppend(indicatorRect);
    }

    return false;
}

PassRefPtr<TextIndicator> TextIndicator::create(const TextIndicatorData& data)
{
    return adoptRef(new TextIndicator(data));
}

PassRefPtr<TextIndicator> TextIndicator::createWithRange(const Range& range, TextIndicatorPresentationTransition presentationTransition)
{
    Frame* frame = range.startContainer()->document().frame();

    if (!frame)
        return nullptr;

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(&range);

    RefPtr<TextIndicator> indicator = TextIndicator::createWithSelectionInFrame(*frame, presentationTransition);

    frame->selection().setSelection(oldSelection);
    
    return indicator.release();
}

// FIXME (138889): Ideally the FrameSnapshotting functions would be more flexible
// and we wouldn't have to implement this here.
static PassRefPtr<Image> snapshotSelectionWithHighlight(Frame& frame)
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

PassRefPtr<TextIndicator> TextIndicator::createWithSelectionInFrame(Frame& frame, TextIndicatorPresentationTransition presentationTransition)
{
    IntRect selectionRect = enclosingIntRect(frame.selection().selectionBounds());
    std::unique_ptr<ImageBuffer> indicatorBuffer = snapshotSelection(frame, SnapshotOptionsForceBlackText);
    if (!indicatorBuffer)
        return nullptr;
    RefPtr<Image> indicatorBitmap = indicatorBuffer->copyImage(CopyBackingStore, Unscaled);
    if (!indicatorBitmap)
        return nullptr;

    RefPtr<Image> indicatorBitmapWithHighlight;
    if (presentationTransition == TextIndicatorPresentationTransition::BounceAndCrossfade || presentationTransition == TextIndicatorPresentationTransition::Crossfade)
        indicatorBitmapWithHighlight = snapshotSelectionWithHighlight(frame);

    // Store the selection rect in window coordinates, to be used subsequently
    // to determine if the indicator and selection still precisely overlap.
    IntRect selectionRectInWindowCoordinates = frame.view()->contentsToWindow(selectionRect);

    Vector<FloatRect> textRects;
    frame.selection().getClippedVisibleTextRectangles(textRects);

    // The bounding rect of all the text rects can be different than the selection
    // rect when the selection spans multiple lines; the indicator doesn't actually
    // care where the selection highlight goes, just where the text actually is.
    FloatRect textBoundingRectInWindowCoordinates;
    Vector<FloatRect> textRectsInWindowCoordinates;
    for (const FloatRect& textRect : textRects) {
        FloatRect textRectInWindowCoordinates = frame.view()->contentsToWindow(enclosingIntRect(textRect));
        textRectsInWindowCoordinates.append(textRectInWindowCoordinates);
        textBoundingRectInWindowCoordinates.unite(textRectInWindowCoordinates);
    }

    Vector<FloatRect> textRectsInBoundingRectCoordinates;
    for (auto rect : textRectsInWindowCoordinates) {
        rect.moveBy(-textBoundingRectInWindowCoordinates.location());
        textRectsInBoundingRectCoordinates.append(rect);
    }

    TextIndicatorData data;
    data.selectionRectInWindowCoordinates = selectionRectInWindowCoordinates;
    data.textBoundingRectInWindowCoordinates = textBoundingRectInWindowCoordinates;
    data.textRectsInBoundingRectCoordinates = textRectsInBoundingRectCoordinates;
    data.contentImageScaleFactor = frame.page()->deviceScaleFactor();
    data.contentImage = indicatorBitmap;
    data.contentImageWithHighlight = indicatorBitmapWithHighlight;
    data.presentationTransition = presentationTransition;

    return TextIndicator::create(data);
}

TextIndicator::TextIndicator(const TextIndicatorData& data)
    : m_data(data)
{
    ASSERT(m_data.contentImageScaleFactor != 1 || m_data.contentImage->size() == enclosingIntRect(m_data.selectionRectInWindowCoordinates).size());

    if (textIndicatorsForTextRectsOverlap(m_data.textRectsInBoundingRectCoordinates)) {
        m_data.textRectsInBoundingRectCoordinates[0] = unionRect(m_data.textRectsInBoundingRectCoordinates);
        m_data.textRectsInBoundingRectCoordinates.shrink(1);
    }
}

TextIndicator::~TextIndicator()
{
}

} // namespace WebCore
