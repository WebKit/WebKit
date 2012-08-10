/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCFontAtlas_h
#define CCFontAtlas_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"
#include "SkBitmap.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

class SkCanvas;

namespace WebCore {

class Color;
class FontDescription;
class GraphicsContext;
class IntPoint;
class IntSize;

// This class provides basic ability to draw text onto the heads-up display.
class CCFontAtlas {
    WTF_MAKE_NONCOPYABLE(CCFontAtlas);
public:
    static PassOwnPtr<CCFontAtlas> create(SkBitmap bitmap, IntRect asciiToRectTable[128], int fontHeight)
    {
        return adoptPtr(new CCFontAtlas(bitmap, asciiToRectTable, fontHeight));
    }
    ~CCFontAtlas();

    // Draws multiple lines of text where each line of text is separated by '\n'.
    // - Correct glyphs will be drawn for ASCII codes in the range 32-127; any characters
    //   outside that range will be displayed as a default rectangle glyph.
    // - IntSize clip is used to avoid wasting time drawing things that are outside the
    //   target canvas bounds.
    // - Should only be called only on the impl thread.
    void drawText(SkCanvas*, const SkPaint&, const String& text, const IntPoint& destPosition, const IntSize& clip) const;

    // Draws the entire atlas at the specified position, just for debugging purposes.
    void drawDebugAtlas(SkCanvas*, const IntPoint& destPosition) const;

private:
    CCFontAtlas(SkBitmap, IntRect asciiToRectTable[128], int fontHeight);

    void drawOneLineOfTextInternal(SkCanvas*, const SkPaint&, const String&, const IntPoint& destPosition) const;

    // The actual texture atlas containing all the pre-rendered glyphs.
    SkBitmap m_atlas;

    // The look-up tables mapping ascii characters to their IntRect locations on the atlas.
    IntRect m_asciiToRectTable[128];

    int m_fontHeight;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
