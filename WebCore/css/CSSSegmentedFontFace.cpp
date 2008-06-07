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

#include "config.h"
#include "CSSSegmentedFontFace.h"

#include "CSSFontFace.h"
#include "CSSFontSelector.h"
#include "FontDescription.h"
#include "SegmentedFontData.h"
#include "SimpleFontData.h"

namespace WebCore {

CSSSegmentedFontFace::CSSSegmentedFontFace(CSSFontSelector* fontSelector)
    : m_fontSelector(fontSelector)
{
}

CSSSegmentedFontFace::~CSSSegmentedFontFace()
{
    pruneTable();
}

void CSSSegmentedFontFace::pruneTable()
{
    // Make sure the glyph page tree prunes out all uses of this custom font.
    if (m_fontDataTable.isEmpty())
        return;
    HashMap<unsigned, SegmentedFontData*>::iterator end = m_fontDataTable.end();
    for (HashMap<unsigned, SegmentedFontData*>::iterator it = m_fontDataTable.begin(); it != end; ++it)
        GlyphPageTreeNode::pruneTreeCustomFontData(it->second);
    deleteAllValues(m_fontDataTable);
    m_fontDataTable.clear();
}

bool CSSSegmentedFontFace::isLoaded() const
{
    unsigned size = m_ranges.size();
    for (unsigned i = 0; i < size; i++) {
        if (!m_ranges[i].fontFace()->isLoaded())
            return false;
    }
    return true;
}

bool CSSSegmentedFontFace::isValid() const
{
    unsigned size = m_ranges.size();
    for (unsigned i = 0; i < size; i++) {
        if (!m_ranges[i].fontFace()->isValid())
            return false;
    }
    return true;
}

void CSSSegmentedFontFace::fontLoaded(CSSFontFace*)
{
    pruneTable();
    m_fontSelector->fontLoaded(this);
}

void CSSSegmentedFontFace::overlayRange(UChar32 from, UChar32 to, PassRefPtr<CSSFontFace> fontFace)
{
    pruneTable();
    fontFace->setSegmentedFontFace(this);

    unsigned size = m_ranges.size();
    if (size) {
        Vector<FontFaceRange, 8> oldRanges = m_ranges;
        m_ranges.clear();
        for (unsigned i = 0; i < size; i++) {
            const FontFaceRange& range = oldRanges[i];
            if (range.from() > to || range.to() < from)
                m_ranges.append(range);
            else if (range.from() < from) {
                m_ranges.append(FontFaceRange(range.from(), from - 1, range.fontFace()));
                if (range.to() > to)
                    m_ranges.append(FontFaceRange(to + 1, range.to(), range.fontFace()));
            } else if (range.to() > to)
                m_ranges.append(FontFaceRange(to + 1, range.to(), range.fontFace()));
        }
    }
    m_ranges.append(FontFaceRange(from, to, fontFace));
}

FontData* CSSSegmentedFontFace::getFontData(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic)
{
    if (!isValid())
        return 0;

    unsigned hashKey = fontDescription.computedPixelSize() << 2 | (syntheticBold ? 0 : 2) | (syntheticItalic ? 0 : 1);

    SegmentedFontData* fontData = m_fontDataTable.get(hashKey);
    if (fontData)
        return fontData;

    fontData = new SegmentedFontData();

    unsigned size = m_ranges.size();
    for (unsigned i = 0; i < size; i++) {
        if (const FontData* rangeFontData = m_ranges[i].fontFace()->getFontData(fontDescription, syntheticBold, syntheticItalic)) {
            ASSERT(!rangeFontData->isSegmented());
            fontData->appendRange(FontDataRange(m_ranges[i].from(), m_ranges[i].to(), static_cast<const SimpleFontData*>(rangeFontData)));
        }
    }
    if (fontData->numRanges())
        m_fontDataTable.set(hashKey, fontData);
    else {
        delete fontData;
        fontData = 0;
    }

    return fontData;
}

}
