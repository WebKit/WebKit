/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "Font.h"
#include "FontFallbackList.h"
#include "FontCache.h"

namespace WebCore
{

FontFallbackList::FontFallbackList()
:m_familyIndex(0), m_pitch(UnknownPitch)
{
}

FontFallbackList::~FontFallbackList()
{
}

void FontFallbackList::invalidate()
{
    m_fontList.clear();
    m_familyIndex = 0;    
    m_pitch = UnknownPitch;
}

void FontFallbackList::determinePitch(const Font* font) const
{
    m_pitch = primaryFont(font)->pitch();
}

const FontData* FontFallbackList::fontDataAt(const Font* font, unsigned realizedFontIndex) const
{
    if (realizedFontIndex < m_fontList.size())
        return m_fontList[realizedFontIndex]; // This fallback font is already in our list.

    // Make sure we're not passing in some crazy value here.
    assert(realizedFontIndex == m_fontList.size());

    if (m_familyIndex == cAllFamiliesScanned)
        return 0;

    // Ask the font cache for the font data.
    // We are obtaining this font for the first time.  We keep track of the families we've looked at before
    // in |m_familyIndex|, so that we never scan the same spot in the list twice.  getFontData will adjust our
    // |m_familyIndex| as it scans for the right font to make.
    const FontData* result = FontCache::getFontData(*font, m_familyIndex);
    if (result)
        m_fontList.append(result);
    return result;
}

const FontData* FontFallbackList::fontDataForCharacters(const Font* font, const UChar* characters, int length) const
{
    // This method is only called when the primary font does not contain the characters we need.
    // Begin our search at position 1.
    unsigned realizedFontIndex = 1;
    const FontData* fontData = fontDataAt(font, realizedFontIndex);
    while (fontData && !fontData->containsCharacters(characters, length))
        fontData = fontDataAt(font, ++realizedFontIndex);
    
    if (!fontData)
        fontData = FontCache::getFontDataForCharacters(*font, characters, length);

    return fontData;
}

}
