/*
 * Copyright (C) 2012 Koji Ishii <kojiishi@gmail.com>
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

#ifndef OpenTypeVerticalData_h
#define OpenTypeVerticalData_h

#if ENABLE(OPENTYPE_VERTICAL)

#include "Glyph.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Font;
class FontPlatformData;
class GlyphPage;

class OpenTypeVerticalData : public RefCounted<OpenTypeVerticalData> {
public:
    static RefPtr<OpenTypeVerticalData> create(const FontPlatformData&);

    bool hasVerticalMetrics() const { return !m_advanceHeights.isEmpty(); }
    float advanceHeight(const Font*, Glyph) const;
    void getVerticalTranslationsForGlyphs(const Font*, const Glyph*, size_t, float* outXYArray) const;
    void substituteWithVerticalGlyphs(const Font*, GlyphPage*) const;

private:
    explicit OpenTypeVerticalData(const FontPlatformData&, Vector<uint16_t>&& advanceWidths);

    void loadMetrics(const FontPlatformData&);
    void loadVerticalGlyphSubstitutions(const FontPlatformData&);
    bool hasVORG() const { return !m_vertOriginY.isEmpty(); }

    HashMap<Glyph, Glyph> m_verticalGlyphMap;
    Vector<uint16_t> m_advanceWidths;
    Vector<uint16_t> m_advanceHeights;
    Vector<int16_t> m_topSideBearings;
    int16_t m_defaultVertOriginY { 0 };
    HashMap<Glyph, int16_t> m_vertOriginY;
};

} // namespace WebCore

#endif // ENABLE(OPENTYPE_VERTICAL)

#endif // OpenTypeVerticalData_h
