/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CSSSegmentedFontFace_h
#define CSSSegmentedFontFace_h

#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

class CSSFontFace;
class CSSFontSelector;
class FontData;
class FontDescription;
class SegmentedFontData;

struct FontFaceRange {
    FontFaceRange(UChar32 from, UChar32 to, PassRefPtr<CSSFontFace> fontFace)
        : m_from(from)
        , m_to(to)
        , m_fontFace(fontFace)
    {
    }

    UChar32 from() const { return m_from; }
    UChar32 to() const { return m_to; }
    CSSFontFace* fontFace() const { return m_fontFace.get(); }

private:
    UChar32 m_from;
    UChar32 m_to;
    RefPtr<CSSFontFace> m_fontFace;
};

class CSSSegmentedFontFace : public RefCounted<CSSSegmentedFontFace> {
public:
    CSSSegmentedFontFace(CSSFontSelector*);
    ~CSSSegmentedFontFace();

    bool isLoaded() const;
    bool isValid() const;
    CSSFontSelector* fontSelector() const { return m_fontSelector; }

    void fontLoaded(CSSFontFace*);

    unsigned numRanges() const { return m_ranges.size(); }
    void overlayRange(UChar32 from, UChar32 to, PassRefPtr<CSSFontFace>);

    FontData* getFontData(const FontDescription&, bool syntheticBold, bool syntheticItalic);

private:
    void pruneTable();

    CSSFontSelector* m_fontSelector;
    HashMap<unsigned, SegmentedFontData*> m_fontDataTable;
    Vector<FontFaceRange, 1> m_ranges;
};

} // namespace WebCore

#endif // CSSSegmentedFontFace_h
