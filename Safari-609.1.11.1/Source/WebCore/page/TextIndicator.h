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

#pragma once

#include "FloatRect.h"
#include "Image.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Frame;
class GraphicsContext;
class Range;

// FIXME: Move PresentationTransition to TextIndicatorWindow, because it's about presentation.
enum class TextIndicatorPresentationTransition : uint8_t {
    None,

    // These animations drive themselves.
    Bounce,
    BounceAndCrossfade,

    // This animation needs to be driven manually via TextIndicatorWindow::setAnimationProgress.
    FadeIn,
};

// Make sure to keep these in sync with the ones in Internals.idl.
enum TextIndicatorOption : uint16_t {
    TextIndicatorOptionDefault = 0,

    // Use the styled text color instead of forcing black text (the default)
    TextIndicatorOptionRespectTextColor = 1 << 0,

    // Paint backgrounds, even if they're not part of the Range
    TextIndicatorOptionPaintBackgrounds = 1 << 1,

    // Don't restrict painting to the given Range
    TextIndicatorOptionPaintAllContent = 1 << 2,

    // Take two snapshots:
    //    - one including the selection highlight and ignoring other painting-related options
    //    - one respecting the other painting-related options
    TextIndicatorOptionIncludeSnapshotWithSelectionHighlight = 1 << 3,

    // Tightly fit the content instead of expanding to cover the bounds of the selection highlight
    TextIndicatorOptionTightlyFitContent = 1 << 4,

    // If there are any non-inline or replaced elements in the Range, indicate the bounding rect
    // of the range instead of the individual subrects, and don't restrict painting to the given Range
    TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges = 1 << 5,

    // By default, TextIndicator removes any margin if the given Range matches the
    // selection Range. If this option is set, maintain the margin in any case.
    TextIndicatorOptionIncludeMarginIfRangeMatchesSelection = 1 << 6,

    // By default, TextIndicator clips the indicated rects to the visible content rect.
    // If this option is set, expand the clip rect outward so that slightly offscreen content will be included.
    TextIndicatorOptionExpandClipBeyondVisibleRect = 1 << 7,

    // By default, TextIndicator clips the indicated rects to the visible content rect.
    // If this option is set, do not clip to the visible rect.
    TextIndicatorOptionDoNotClipToVisibleRect = 1 << 8,

    // Include an additional snapshot of everything in view, with the exception of nodes within the currently selected range.
    TextIndicatorOptionIncludeSnapshotOfAllVisibleContentWithoutSelection = 1 << 9,

    // By default, TextIndicator uses text rects to size the snapshot. Enabling this flag causes it to use the bounds of the
    // selection rects that would enclose the given Range instead.
    // Currently, this is only supported on iOS.
    TextIndicatorOptionUseSelectionRectForSizing = 1 << 10,

    // Compute a background color to use when rendering a platter around the content image, falling back to a default if the
    // content's background is too complex to be captured by a single color.
    TextIndicatorOptionComputeEstimatedBackgroundColor = 1 << 11,
};
typedef uint16_t TextIndicatorOptions;

struct TextIndicatorData {
    FloatRect selectionRectInRootViewCoordinates;
    FloatRect textBoundingRectInRootViewCoordinates;
    FloatRect contentImageWithoutSelectionRectInRootViewCoordinates;
    Vector<FloatRect> textRectsInBoundingRectCoordinates;
    float contentImageScaleFactor;
    RefPtr<Image> contentImageWithHighlight;
    RefPtr<Image> contentImageWithoutSelection;
    RefPtr<Image> contentImage;
    Color estimatedBackgroundColor;
    TextIndicatorPresentationTransition presentationTransition;
    TextIndicatorOptions options;
};

class TextIndicator : public RefCounted<TextIndicator> {
public:
    // FIXME: These are fairly Mac-specific, and they don't really belong here.
    // But they're needed at TextIndicator creation time, so they can't go in TextIndicatorWindow.
    // Maybe they can live in some Theme code somewhere?
    constexpr static float defaultHorizontalMargin { 2 };
    constexpr static float defaultVerticalMargin { 1 };

    WEBCORE_EXPORT static Ref<TextIndicator> create(const TextIndicatorData&);
    WEBCORE_EXPORT static RefPtr<TextIndicator> createWithSelectionInFrame(Frame&, TextIndicatorOptions, TextIndicatorPresentationTransition, FloatSize margin = FloatSize(defaultHorizontalMargin, defaultVerticalMargin));
    WEBCORE_EXPORT static RefPtr<TextIndicator> createWithRange(const Range&, TextIndicatorOptions, TextIndicatorPresentationTransition, FloatSize margin = FloatSize(defaultHorizontalMargin, defaultVerticalMargin));

    WEBCORE_EXPORT ~TextIndicator();

    FloatRect selectionRectInRootViewCoordinates() const { return m_data.selectionRectInRootViewCoordinates; }
    FloatRect textBoundingRectInRootViewCoordinates() const { return m_data.textBoundingRectInRootViewCoordinates; }
    const Vector<FloatRect>& textRectsInBoundingRectCoordinates() const { return m_data.textRectsInBoundingRectCoordinates; }
    float contentImageScaleFactor() const { return m_data.contentImageScaleFactor; }
    Image* contentImageWithHighlight() const { return m_data.contentImageWithHighlight.get(); }
    Image* contentImage() const { return m_data.contentImage.get(); }

    TextIndicatorPresentationTransition presentationTransition() const { return m_data.presentationTransition; }
    void setPresentationTransition(TextIndicatorPresentationTransition transition) { m_data.presentationTransition = transition; }

    TextIndicatorData data() const { return m_data; }

private:
    TextIndicator(const TextIndicatorData&);

    TextIndicatorData m_data;
};

} // namespace WebKit
