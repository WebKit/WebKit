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

#ifndef TextIndicator_h
#define TextIndicator_h

#include "FloatRect.h"
#include "Image.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101000
#define ENABLE_LEGACY_TEXT_INDICATOR_STYLE 1
#else
#define ENABLE_LEGACY_TEXT_INDICATOR_STYLE 0
#endif

namespace WebCore {

class Frame;
class GraphicsContext;
class Range;

enum class TextIndicatorPresentationTransition {
    None,

    // These animations drive themselves.
    Bounce,
    BounceAndCrossfade,

    // This animation needs to be driven manually via TextIndicatorWindow::setAnimationProgress.
    FadeIn,
};

enum class TextIndicatorLifetime {
    // The TextIndicator should indicate the text until dismissed.
    Permanent,

    // The TextIndicator should briefly indicate the text and then automatically dismiss.
    Temporary
};

enum class TextIndicatorDismissalAnimation {
    None,
    FadeOut
};

struct TextIndicatorData {
    FloatRect selectionRectInRootViewCoordinates;
    FloatRect textBoundingRectInRootViewCoordinates;
    Vector<FloatRect> textRectsInBoundingRectCoordinates;
    float contentImageScaleFactor;
    RefPtr<Image> contentImageWithHighlight;
    RefPtr<Image> contentImage;
    TextIndicatorPresentationTransition presentationTransition;
    bool wantsMargin;
};

class TextIndicator : public RefCounted<TextIndicator> {
public:
    WEBCORE_EXPORT static Ref<TextIndicator> create(const TextIndicatorData&);
    WEBCORE_EXPORT static RefPtr<TextIndicator> createWithSelectionInFrame(Frame&, TextIndicatorPresentationTransition, unsigned margin = 0);
    WEBCORE_EXPORT static RefPtr<TextIndicator> createWithRange(const Range&, TextIndicatorPresentationTransition, unsigned margin = 0);

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
    
    bool wantsBounce() const;
    bool wantsContentCrossfade() const;
    bool wantsFadeIn() const;
    bool wantsManualAnimation() const;

    void setWantsMargin(bool wantsMargin) { m_data.wantsMargin = wantsMargin; }
    bool wantsMargin() const { return m_data.wantsMargin; }

private:
    TextIndicator(const TextIndicatorData&);

    TextIndicatorData m_data;
};

} // namespace WebKit

#endif // TextIndicator_h
