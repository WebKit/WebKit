/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
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

#import "config.h"
#import "Font.h"
#import "WebCoreSystemInterface.h"

namespace WebCore
{

typedef float WebGlyphWidth;

struct WidthMap {
    WidthMap() :next(0), widths(0) {}

    ATSGlyphRef startRange;
    ATSGlyphRef endRange;
    WidthMap *next;
    WebGlyphWidth *widths;
};

static WidthMap *extendWidthMap(const FontData *, ATSGlyphRef);

static void freeWidthMap(WidthMap *);

// Map utility functions

float FontData::widthForGlyph(Glyph glyph) const
{
    WidthMap *map;
    for (map = m_glyphToWidthMap; 1; map = map->next) {
        if (!map)
            map = extendWidthMap(this, glyph);
        if (glyph >= map->startRange && glyph <= map->endRange)
            break;
    }
    float width = map->widths[glyph - map->startRange];
    if (width >= 0)
        return width;
        
    width = platformWidthForGlyph(glyph);
    map->widths[glyph - map->startRange] = width;
    return width;
}

FontData::FontData(const FontPlatformData& f)
:m_font(f), m_glyphToWidthMap(0), m_treatAsFixedPitch(false),
 m_smallCapsFontData(0)
 {    
    m_font = f;
    platformInit();
    
    // Nasty hack to determine if we should round or ceil space widths.
    // If the font is monospace or fake monospace we ceil to ensure that 
    // every character and the space are the same width.  Otherwise we round.
    m_spaceGlyph = m_characterToGlyphMap.glyphDataForCharacter(' ', this).glyph;
    float width = widthForGlyph(m_spaceGlyph);
    m_spaceWidth = width;
    determinePitch();
    m_adjustedSpaceWidth = m_treatAsFixedPitch ? ceilf(width) : roundf(width);
}

FontData::~FontData()
{
    platformDestroy();

    freeWidthMap(m_glyphToWidthMap);

    // We only get deleted when the cache gets cleared.  Since the smallCapsRenderer is also in that cache,
    // it will be deleted then, so we don't need to do anything here.
}

static WidthMap *extendWidthMap(const FontData *renderer, ATSGlyphRef glyph)
{
    WidthMap *map = new WidthMap;
    unsigned end;
    ATSGlyphRef start;
    unsigned blockSize;
    unsigned i, count;
    
    NSFont *f = renderer->m_font.font;
    blockSize = cGlyphPageSize;
    
    if (blockSize == 0)
        start = 0;
    else
        start = (glyph / blockSize) * blockSize;

    end = ((unsigned)start) + blockSize; 

    map->startRange = start;
    map->endRange = end;
    count = end - start + 1;

    map->widths = new WebGlyphWidth[count];
    for (i = 0; i < count; i++)
        map->widths[i] = NAN;

    if (renderer->m_glyphToWidthMap == 0)
        renderer->m_glyphToWidthMap = map;
    else {
        WidthMap *lastMap = renderer->m_glyphToWidthMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    return map;
}

static void freeWidthMap(WidthMap *map)
{
    while (map) {
        WidthMap *next = map->next;
        delete []map->widths;
        delete map;
        map = next;
    }
}

}
