/*
 * Copyright (c) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HarfBuzzSkia.h"

#include "FontPlatformData.h"

#include "SkFontHost.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkPoint.h"
#include "SkRect.h"
#include "SkUtils.h"

#include <wtf/HashMap.h>
#include <wtf/OwnArrayPtr.h>

extern "C" {
#include "harfbuzz-shaper.h"
}

// This file implements the callbacks which Harfbuzz requires by using Skia
// calls. See the Harfbuzz source for references about what these callbacks do.

namespace WebCore {

static HB_Fixed SkiaScalarToHarfbuzzFixed(SkScalar value)
{
    // HB_Fixed is a 26.6 fixed point format.
    return value * 64;
}

static HB_Bool stringToGlyphs(HB_Font hbFont, const HB_UChar16* characters, hb_uint32 length, HB_Glyph* glyphs, hb_uint32* glyphsSize, HB_Bool isRTL)
{
    FontPlatformData* font = reinterpret_cast<FontPlatformData*>(hbFont->userData);
    SkPaint paint;

    font->setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);

    unsigned codepoints = 0;
    for (hb_uint32 i = 0; i < length; i++) {
        if (!SkUTF16_IsHighSurrogate(characters[i]))
            codepoints++;
        if (codepoints > *glyphsSize)
            return 0;
    }

    int numGlyphs = paint.textToGlyphs(characters, length * sizeof(uint16_t), reinterpret_cast<uint16_t*>(glyphs));

    // HB_Glyph is 32-bit, but Skia outputs only 16-bit numbers. So our
    // |glyphs| array needs to be converted.
    for (int i = numGlyphs - 1; i >= 0; --i) {
        uint16_t value;
        // We use a memcpy to avoid breaking strict aliasing rules.
        memcpy(&value, reinterpret_cast<char*>(glyphs) + sizeof(uint16_t) * i, sizeof(uint16_t));
        glyphs[i] = value;
    }

    *glyphsSize = numGlyphs;
    return 1;
}

static void glyphsToAdvances(HB_Font hbFont, const HB_Glyph* glyphs, hb_uint32 numGlyphs, HB_Fixed* advances, int flags)
{
    FontPlatformData* font = reinterpret_cast<FontPlatformData*>(hbFont->userData);
    SkPaint paint;

    font->setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

    OwnArrayPtr<uint16_t> glyphs16 = adoptArrayPtr(new uint16_t[numGlyphs]);
    if (!glyphs16.get())
        return;
    for (unsigned i = 0; i < numGlyphs; ++i)
        glyphs16[i] = glyphs[i];
    paint.getTextWidths(glyphs16.get(), numGlyphs * sizeof(uint16_t), reinterpret_cast<SkScalar*>(advances));

    // The |advances| values which Skia outputs are SkScalars, which are floats
    // in Chromium. However, Harfbuzz wants them in 26.6 fixed point format.
    // These two formats are both 32-bits long.
    for (unsigned i = 0; i < numGlyphs; ++i) {
        float value;
        // We use a memcpy to avoid breaking strict aliasing rules.
        memcpy(&value, reinterpret_cast<char*>(advances) + sizeof(float) * i, sizeof(float));
        advances[i] = SkiaScalarToHarfbuzzFixed(value);
    }
}

static HB_Bool canRender(HB_Font hbFont, const HB_UChar16* characters, hb_uint32 length)
{
    FontPlatformData* font = reinterpret_cast<FontPlatformData*>(hbFont->userData);
    SkPaint paint;

    font->setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);

    OwnArrayPtr<uint16_t> glyphs16 = adoptArrayPtr(new uint16_t[length]);
    if (!glyphs16.get())
        return 0;
    int numGlyphs = paint.textToGlyphs(characters, length * sizeof(uint16_t), glyphs16.get());

    bool canRender = true;
    for (int i = 0; i < numGlyphs; ++i) {
        if (!glyphs16[i]) {
            canRender = false;
            break;
        }
    }

    return canRender;
}

static HB_Error getOutlinePoint(HB_Font hbFont, HB_Glyph glyph, int flags, hb_uint32 point, HB_Fixed* xPos, HB_Fixed* yPos, hb_uint32* resultingNumPoints)
{
    // We cannot provide the right position of outline points from point index.
    // Just return HB_Err_Ok with resultingNumPoints = 0 so that harfbuzz
    // falls back on using design coordinate value pair.
    // See https://bugs.webkit.org/show_bug.cgi?id=60079 for more details.
    *resultingNumPoints = 0;
    return HB_Err_Ok;
}

static void getGlyphMetrics(HB_Font hbFont, HB_Glyph glyph, HB_GlyphMetrics* metrics)
{
    FontPlatformData* font = reinterpret_cast<FontPlatformData*>(hbFont->userData);
    SkPaint paint;

    font->setupPaint(&paint);
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    uint16_t glyph16 = glyph;
    SkScalar width;
    SkRect bounds;
    paint.getTextWidths(&glyph16, sizeof(glyph16), &width, &bounds);

    metrics->x = SkiaScalarToHarfbuzzFixed(bounds.fLeft);
    metrics->y = SkiaScalarToHarfbuzzFixed(bounds.fTop);
    metrics->width = SkiaScalarToHarfbuzzFixed(bounds.width());
    metrics->height = SkiaScalarToHarfbuzzFixed(bounds.height());

    metrics->xOffset = SkiaScalarToHarfbuzzFixed(width);
    // We can't actually get the |y| correct because Skia doesn't export
    // the vertical advance. However, nor we do ever render vertical text at
    // the moment so it's unimportant.
    metrics->yOffset = 0;
}

static HB_Fixed getFontMetric(HB_Font hbFont, HB_FontMetric metric)
{
    FontPlatformData* font = reinterpret_cast<FontPlatformData*>(hbFont->userData);
    SkPaint paint;

    font->setupPaint(&paint);
    SkPaint::FontMetrics skiaMetrics;
    paint.getFontMetrics(&skiaMetrics);

    switch (metric) {
    case HB_FontAscent:
        return SkiaScalarToHarfbuzzFixed(-skiaMetrics.fAscent);
    // We don't support getting the rest of the metrics and Harfbuzz doesn't seem to need them.
    default:
        return 0;
    }
}

HB_FontClass harfbuzzSkiaClass = {
    stringToGlyphs,
    glyphsToAdvances,
    canRender,
    getOutlinePoint,
    getGlyphMetrics,
    getFontMetric,
};

HB_Font_* allocHarfbuzzFont()
{
    HB_Font_* font = reinterpret_cast<HB_Font_*>(fastMalloc(sizeof(HB_Font_)));
    memset(font, 0, sizeof(HB_Font_));
    font->klass = &harfbuzzSkiaClass;
    font->userData = 0;

    return font;
}

HB_Error harfbuzzSkiaGetTable(void* voidface, const HB_Tag tag, HB_Byte* buffer, HB_UInt* len)
{
    FontPlatformData* font = reinterpret_cast<FontPlatformData*>(voidface);

    const size_t tableSize = SkFontHost::GetTableSize(font->uniqueID(), tag);
    if (!tableSize)
        return HB_Err_Invalid_Argument;
    // If Harfbuzz specified a 0 buffer then it's asking for the size of the table.
    if (!buffer) {
        *len = tableSize;
        return HB_Err_Ok;
    }

    if (*len < tableSize)
        return HB_Err_Invalid_Argument;
    SkFontHost::GetTableData(font->uniqueID(), tag, 0, tableSize, buffer);
    return HB_Err_Ok;
}

typedef pair<HB_FaceRec_*, unsigned> FaceCacheEntry;
typedef HashMap<unsigned int, FaceCacheEntry, WTF::IntHash<unsigned int>, WTF::UnsignedWithZeroKeyHashTraits<unsigned int> > HarfbuzzFaceCache;
static HarfbuzzFaceCache* gHarfbuzzFaceCache = 0;

static HB_FaceRec_* getCachedHarfbuzzFace(FontPlatformData* platformData)
{
    if (!gHarfbuzzFaceCache)
        gHarfbuzzFaceCache = new HarfbuzzFaceCache;
    SkFontID uniqueID = platformData->uniqueID();
    HarfbuzzFaceCache::iterator result = gHarfbuzzFaceCache->find(uniqueID);
    if (result == gHarfbuzzFaceCache->end()) {
        FaceCacheEntry entry(HB_NewFace(platformData, harfbuzzSkiaGetTable), 1);
        gHarfbuzzFaceCache->set(uniqueID, entry);
        return entry.first;
    }
    ++(result.get()->second.second);
    return result.get()->second.first;
}

static void releaseCachedHarfbuzzFace(SkFontID uniqueID)
{
    HarfbuzzFaceCache::iterator result = gHarfbuzzFaceCache->find(uniqueID);
    ASSERT(result != gHarfbuzzFaceCache->end());
    ASSERT(result.get()->second.second > 0);
    --(result.get()->second.second);
    if (!(result.get()->second.second)) {
        HB_FreeFace(result.get()->second.first);
        gHarfbuzzFaceCache->remove(uniqueID);
    }
}

HarfbuzzFace::HarfbuzzFace(FontPlatformData* platformData)
    : m_uniqueID(platformData->uniqueID())
{
    m_harfbuzzFace = getCachedHarfbuzzFace(platformData);
}

HarfbuzzFace::~HarfbuzzFace()
{
    releaseCachedHarfbuzzFace(m_uniqueID);
}

} // namespace WebCore
