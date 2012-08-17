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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "CCFontAtlas.h"

#include "CCProxy.h"
#include "SkCanvas.h"

namespace WebCore {

using namespace std;

CCFontAtlas::CCFontAtlas(SkBitmap bitmap, IntRect asciiToRectTable[128], int fontHeight)
    : m_atlas(bitmap)
    , m_fontHeight(fontHeight)
{
    for (size_t i = 0; i < 128; ++i)
        m_asciiToRectTable[i] = asciiToRectTable[i];
}

CCFontAtlas::~CCFontAtlas()
{
}

void CCFontAtlas::drawText(SkCanvas* canvas, const SkPaint& paint, const String& text, const IntPoint& destPosition, const IntSize& clip) const
{
    ASSERT(CCProxy::isImplThread());

    Vector<String> lines;
    text.split('\n', lines);

    IntPoint position = destPosition;
    for (size_t i = 0; i < lines.size(); ++i) {
        drawOneLineOfTextInternal(canvas, paint, lines[i], position);
        position.setY(position.y() + m_fontHeight);
        if (position.y() > clip.height())
            return;
    }
}

void CCFontAtlas::drawOneLineOfTextInternal(SkCanvas* canvas, const SkPaint& paint, const String& textLine, const IntPoint& destPosition) const
{
    ASSERT(CCProxy::isImplThread());

    IntPoint position = destPosition;
    for (unsigned i = 0; i < textLine.length(); ++i) {
        // If the ASCII code is out of bounds, then index 0 is used, which is just a plain rectangle glyph.
        int asciiIndex = (textLine[i] < 128) ? textLine[i] : 0;
        IntRect glyphBounds = m_asciiToRectTable[asciiIndex];
        SkIRect source = SkIRect::MakeXYWH(glyphBounds.x(), glyphBounds.y(), glyphBounds.width(), glyphBounds.height());
        canvas->drawBitmapRect(m_atlas, &source, SkRect::MakeXYWH(position.x(), position.y(), glyphBounds.width(), glyphBounds.height()), &paint);
        position.setX(position.x() + glyphBounds.width());
    }
}

void CCFontAtlas::drawDebugAtlas(SkCanvas* canvas, const IntPoint& destPosition) const
{
    ASSERT(CCProxy::isImplThread());

    SkIRect source = SkIRect::MakeWH(m_atlas.width(), m_atlas.height());
    canvas->drawBitmapRect(m_atlas, &source, SkRect::MakeXYWH(destPosition.x(), destPosition.y(), m_atlas.width(), m_atlas.height()));
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
