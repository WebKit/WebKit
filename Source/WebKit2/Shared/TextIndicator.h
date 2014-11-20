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

#include "ShareableBitmap.h"
#include <WebCore/FloatRect.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101000
#define ENABLE_LEGACY_TEXT_INDICATOR_STYLE 1
#else
#define ENABLE_LEGACY_TEXT_INDICATOR_STYLE 0
#endif

namespace WebCore {
class GraphicsContext;
class Range;
}

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class WebFrame;

class TextIndicator : public RefCounted<TextIndicator> {
public:
    enum class PresentationTransition {
        None,
        Bounce,
        BounceAndCrossfade
    };

    struct Data {
        WebCore::FloatRect selectionRectInWindowCoordinates;
        WebCore::FloatRect textBoundingRectInWindowCoordinates;
        Vector<WebCore::FloatRect> textRectsInBoundingRectCoordinates;
        float contentImageScaleFactor;
        RefPtr<ShareableBitmap> contentImageWithHighlight;
        RefPtr<ShareableBitmap> contentImage;

        PresentationTransition presentationTransition;

        void encode(IPC::ArgumentEncoder&) const;
        static bool decode(IPC::ArgumentDecoder&, Data&);
    };

    static PassRefPtr<TextIndicator> create(const TextIndicator::Data&);
    static PassRefPtr<TextIndicator> createWithSelectionInFrame(const WebFrame&, PresentationTransition);
    static PassRefPtr<TextIndicator> createWithRange(const WebCore::Range&, PresentationTransition);

    ~TextIndicator();

    WebCore::FloatRect selectionRectInWindowCoordinates() const { return m_data.selectionRectInWindowCoordinates; }
    WebCore::FloatRect frameRect() const;
    Data data() const { return m_data; }

    void draw(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect);

private:
    TextIndicator(const TextIndicator::Data&);

    void drawContentImage(WebCore::GraphicsContext&, WebCore::FloatRect textRect);

    Data m_data;
};

} // namespace WebKit

#endif // TextIndicator_h
