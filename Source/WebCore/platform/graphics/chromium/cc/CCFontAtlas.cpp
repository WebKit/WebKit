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
#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "TextRun.h"

#define ATLAS_SIZE 128
#define FONT_HEIGHT 14

namespace WebCore {

using namespace std;


CCFontAtlas::CCFontAtlas()
    : m_fontHeight(FONT_HEIGHT)
{
}

static void wrapPositionIfNeeded(IntPoint& position, int textWidth, int textHeight)
{
    if (position.x() + textWidth > ATLAS_SIZE)
        position = IntPoint(0, position.y() + textHeight);
}

void CCFontAtlas::generateAtlasForFont(GraphicsContext* atlasContext, const FontDescription& fontDescription, const Color& fontColor, const IntPoint& startingPosition, IntRect asciiToAtlasTable[128])
{
    ASSERT(CCProxy::isMainThread());
    ASSERT(m_atlas);

    FontCachePurgePreventer fontCachePurgePreventer;

    IntPoint position = startingPosition;
    int textHeight = fontDescription.computedPixelSize();
    // This is a dirty little trick to account for overhang letters like g, p, j.
    int inflation = textHeight / 3;

    Font font(fontDescription, 0, 0);
    font.update(0);

    atlasContext->setStrokeColor(fontColor, ColorSpaceDeviceRGB);
    atlasContext->setFillColor(fontColor, ColorSpaceDeviceRGB);

    // First, draw a generic rect that will be used for special and unknown characters that have nothing else to render.
    {
        int textWidth = textHeight / 2;
        wrapPositionIfNeeded(position, textWidth, textHeight + inflation);
        atlasContext->strokeRect(FloatRect(FloatPoint(position.x() + 1, position.y() - textHeight + 1 + inflation), FloatSize(textWidth - 2, textHeight - 2 - inflation)), 1);

        // Initialize the rect that would be copied when drawing this glyph from the atlas.
        asciiToAtlasTable[0] = IntRect(IntPoint(position.x(), position.y() - textHeight), IntSize(textWidth, textHeight + inflation));

        // Increment to the position where the next glyph will be placed.
        position.setX(position.x() + textWidth);
    }

    // Then, draw the ASCII characters.
    for (LChar i = 1; i < 128; ++i) {
        if (i < 32) {
            // Special characters will simply use the the default glyph.
            asciiToAtlasTable[i] = asciiToAtlasTable[0];
            continue;
        }

        String str;
        str.append(i);
        TextRun text(str);

        int textWidth = round(font.width(text));
        wrapPositionIfNeeded(position, textWidth, textHeight + inflation);
        atlasContext->drawText(font, text, position);

        // Initialize the rect that would be copied when drawing this glyph from the atlas.
        asciiToAtlasTable[i] = IntRect(IntPoint(position.x(), position.y() - textHeight), IntSize(textWidth, textHeight + inflation));

        // Increment to the position where the next glyph will be placed.
        position.setX(position.x() + textWidth);
    }
}

void CCFontAtlas::initialize()
{
    ASSERT(CCProxy::isMainThread());

    // We expect this function to be called only once when the atlas did not exist yet. We should be aware if that's not true.
    ASSERT(!m_atlas);

    m_atlas = ImageBuffer::create(IntSize(ATLAS_SIZE, ATLAS_SIZE));
    GraphicsContext* atlasContext = m_atlas->context();

    // Clear the entire texture atlas to transparent before drawing fonts.
    atlasContext->setFillColor(Color(0, 0, 0, 0), ColorSpaceDeviceRGB);
    atlasContext->fillRect(FloatRect(0, 0, ATLAS_SIZE, ATLAS_SIZE));

    // FIXME: monospace font does not work as expected.
    FontDescription fontDescription;
    fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
    fontDescription.setComputedSize(m_fontHeight);
    generateAtlasForFont(atlasContext, fontDescription, Color(255, 0, 0), IntPoint(0, fontDescription.computedPixelSize()), m_asciiToRectTable);
}

void CCFontAtlas::drawText(GraphicsContext* targetContext, const String& text, const IntPoint& destPosition, const IntSize& clip) const
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(m_atlas);

    Vector<String> lines;
    text.split('\n', lines);

    IntPoint position = destPosition;
    for (size_t i = 0; i < lines.size(); ++i) {
        drawOneLineOfTextInternal(targetContext, lines[i], position);
        position.setY(position.y() + m_fontHeight);
        if (position.y() > clip.height())
            return;
    }
}

void CCFontAtlas::drawOneLineOfTextInternal(GraphicsContext* targetContext, const String& textLine, const IntPoint& destPosition) const
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(m_atlas);

    IntPoint position = destPosition;
    for (unsigned i = 0; i < textLine.length(); ++i) {
        // If the ASCII code is out of bounds, then index 0 is used, which is just a plain rectangle glyph.
        int asciiIndex = (textLine[i] < 128) ? textLine[i] : 0;
        IntRect glyphBounds = m_asciiToRectTable[asciiIndex];
        targetContext->drawImageBuffer(m_atlas.get(), ColorSpaceDeviceRGB, position, glyphBounds);
        position.setX(position.x() + glyphBounds.width());
    }
}

void CCFontAtlas::drawDebugAtlas(GraphicsContext* targetContext, const IntPoint& destPosition) const
{
    ASSERT(CCProxy::isImplThread());
    ASSERT(m_atlas);

    targetContext->drawImageBuffer(m_atlas.get(), ColorSpaceDeviceRGB, destPosition, IntRect(IntPoint::zero(), IntSize(ATLAS_SIZE, ATLAS_SIZE)));
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
