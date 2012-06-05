/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef HarfBuzzShaper_h
#define HarfBuzzShaper_h

#include "FloatPoint.h"
#include "GlyphBuffer.h"
#include "HarfBuzzShaperBase.h"
#include "TextRun.h"
#include "hb.h"
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Font;
class SimpleFontData;

class HarfBuzzShaper : public HarfBuzzShaperBase {
public:
    HarfBuzzShaper(const Font*, const TextRun&);
    virtual ~HarfBuzzShaper();

    bool shape(GlyphBuffer* = 0);
    float totalWidth() { return m_totalWidth; }
    int offsetForPosition(float targetX);
    FloatRect selectionRect(const FloatPoint&, int height, int from, int to);

private:
    class HarfBuzzRun {
    public:
        static PassOwnPtr<HarfBuzzRun> create(unsigned numCharacters, TextDirection direction, hb_buffer_t* buffer)
        {
            return adoptPtr(new HarfBuzzRun(numCharacters, direction, buffer));
        }

        void setGlyphAndPositions(unsigned index, uint16_t glyphId, float x, float y, float);
        void setWidth(float width) { m_width = width; }

        int characterIndexForXPosition(int targetX);
        int xPositionForOffset(unsigned offset);

        unsigned numCharacters() const { return m_numCharacters; }
        unsigned numGlyphs() const { return m_numGlyphs; }
        float width() { return m_width; }

    private:
        HarfBuzzRun(unsigned numCharacters, TextDirection, hb_buffer_t*);
        bool rtl() { return m_direction == RTL; }

        size_t m_numCharacters;
        unsigned m_numGlyphs;
        TextDirection m_direction;
        Vector<uint16_t, 256> m_glyphs;
        Vector<float, 256> m_advances;
        Vector<FloatPoint, 256> m_offsets;
        Vector<uint16_t, 256> m_logClusters;
        Vector<uint16_t, 256> m_glyphToCharacterIndex;
        float m_width;
    };

    void setFontFeatures();

    bool setupHarfBuzzRun();
    bool shapeHarfBuzzRun();
    void setGlyphPositionsForHarfBuzzRun(GlyphBuffer*);

    GlyphBufferAdvance createGlyphBufferAdvance(float, float);

    Vector<hb_feature_t, 4> m_features;
    unsigned m_startIndexOfCurrentRun;
    unsigned m_numCharactersOfCurrentRun;
    const SimpleFontData* m_currentFontData;
    hb_buffer_t* m_harfbuzzBuffer;
    Vector<OwnPtr<HarfBuzzRun>, 16> m_harfbuzzRuns;

    float m_totalWidth;
};

} // namespace WebCore

#endif // HarfBuzzShaper_h
