/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CompositorHUDFontAtlas.h"

#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "GraphicsContext.h"
#include "PlatformContextSkia.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include "TextRun.h"
#include "skia/ext/platform_canvas.h"

using WebKit::WebRect;

namespace WebCore {

#define ATLAS_SIZE 128

static void wrapPositionIfNeeded(IntPoint& position, int textWidth, int textHeight)
{
    if (position.x() + textWidth > ATLAS_SIZE)
        position = IntPoint(0, position.y() + textHeight);
}

// Paints the font into the atlas, from left-to-right, top-to-bottom, starting at
// startingPosition. At the same time, it updates the ascii-to-WebRect mapping for
// each character. By doing things this way, it is possible to support variable-width
// fonts and multiple fonts on the same atlas.
SkBitmap CompositorHUDFontAtlas::generateFontAtlas(WebRect asciiToRectTable[128], int& fontHeight)
{
    fontHeight = 14;

    OwnPtr<SkCanvas> canvas = adoptPtr(skia::CreateBitmapCanvas(ATLAS_SIZE, ATLAS_SIZE, false /* opaque */));

    PlatformContextSkia platformContext(canvas.get());
    platformContext.setDrawingToImageBuffer(true);
    GraphicsContext atlasContext(&platformContext);

    // Clear the entire texture atlas to transparent before drawing fonts.
    atlasContext.setFillColor(Color(0, 0, 0, 0), ColorSpaceDeviceRGB);
    atlasContext.clearRect(FloatRect(0, 0, ATLAS_SIZE, ATLAS_SIZE));

    // FIXME: monospace font does not work as expected.
    FontDescription fontDescription;
    fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
    fontDescription.setComputedSize(fontHeight);

    FontCachePurgePreventer fontCachePurgePreventer;

    int textHeight = fontDescription.computedPixelSize();
    IntPoint position(0, textHeight);
    // This is a dirty little trick to account for overhang letters like g, p, j.
    int inflation = textHeight / 3;

    Font font(fontDescription, 0, 0);
    font.update(0);

    Color fontColor(255, 0, 0);
    atlasContext.setStrokeColor(fontColor, ColorSpaceDeviceRGB);
    atlasContext.setFillColor(fontColor, ColorSpaceDeviceRGB);

    // First, draw a generic rect that will be used for special and unknown characters that have nothing else to render.
    {
        int textWidth = textHeight / 2;
        wrapPositionIfNeeded(position, textWidth, textHeight + inflation);
        atlasContext.strokeRect(FloatRect(FloatPoint(position.x() + 1, position.y() - textHeight + 1 + inflation), FloatSize(textWidth - 2, textHeight - 2 - inflation)), 1);

        // Initialize the rect that would be copied when drawing this glyph from the atlas.
        asciiToRectTable[0] = WebRect(position.x(), position.y() - textHeight, textWidth, textHeight + inflation);

        // Increment to the position where the next glyph will be placed.
        position.setX(position.x() + textWidth);
    }

    // Then, draw the ASCII characters.
    for (LChar i = 1; i < 128; ++i) {
        if (i < 32) {
            // Special characters will simply use the the default glyph.
            asciiToRectTable[i] = asciiToRectTable[0];
            continue;
        }

        String str;
        str.append(i);
        TextRun text(str);

        int textWidth = round(font.width(text));
        wrapPositionIfNeeded(position, textWidth, textHeight + inflation);
        atlasContext.drawText(font, text, position);

        // Initialize the rect that would be copied when drawing this glyph from the atlas.
        asciiToRectTable[i] = WebRect(position.x(), position.y() - textHeight, textWidth, textHeight + inflation);

        // Increment to the position where the next glyph will be placed.
        position.setX(position.x() + textWidth);
    }

    SkBitmap copy;
    const SkBitmap& source = canvas->getDevice()->accessBitmap(false);
    source.copyTo(&copy, source.config());
    return copy;
}

} // namespace WebCore
