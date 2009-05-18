/*
    Copyright (C) 2008 Holger Hans Peter Freyther

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    Replacement of the stock FontFallbackList as Qt is going to find us a
    replacement font, will do caching and the other stuff we implement in
    WebKit.
*/

#include "config.h"
#include "FontFallbackList.h"

#include "Font.h"
#include "SegmentedFontData.h"

#include <QDebug>

namespace WebCore {

FontFallbackList::FontFallbackList()
    : m_familyIndex(0)
    , m_pitch(UnknownPitch)
    , m_loadingCustomFonts(false)
    , m_fontSelector(0)
    , m_generation(0)
{
}

void FontFallbackList::invalidate(WTF::PassRefPtr<WebCore::FontSelector> fontSelector)
{
    m_familyIndex = 0;
    m_pitch = UnknownPitch;
    m_loadingCustomFonts = false;
    m_fontSelector = fontSelector;
    m_generation = 0;
}

void FontFallbackList::releaseFontData()
{
    if (m_fontList.size())
        delete m_fontList[0].first;
    m_fontList.clear();
}

void FontFallbackList::determinePitch(const WebCore::Font* font) const
{
    const FontData* fontData = primaryFont(font);
    if (!fontData->isSegmented())
        m_pitch = static_cast<const SimpleFontData*>(fontData)->pitch();
    else {
        const SegmentedFontData* segmentedFontData = static_cast<const SegmentedFontData*>(fontData);
        unsigned numRanges = segmentedFontData->numRanges();
        if (numRanges == 1)
            m_pitch = segmentedFontData->rangeAt(0).fontData()->pitch();
        else
            m_pitch = VariablePitch;
    }
}

const FontData* FontFallbackList::fontDataAt(const WebCore::Font* _font, unsigned index) const
{
    if (index != 0)
        return 0;

    // Use the FontSelector to get a WebCore font and then fallback to Qt
    const FontDescription& description = _font->fontDescription();
    const FontFamily* family = &description.family();
    while (family) {
        if (m_fontSelector) {
            FontData* data = m_fontSelector->getFontData(description, family->family());
            if (data) {
                if (data->isLoading())
                    m_loadingCustomFonts = true;
                return data;
            }
        }
        family = family->next();
    }

    if (m_fontList.size())
        return m_fontList[0].first;

    const FontData* result = new SimpleFontData(FontPlatformData(description), _font->wordSpacing(), _font->letterSpacing());
    m_fontList.append(pair<const FontData*, bool>(result, result->isCustomFont()));
    return result;
}

const FontData* FontFallbackList::fontDataForCharacters(const WebCore::Font* font, const UChar*, int) const
{
    return primaryFont(font);
}

void FontFallbackList::setPlatformFont(const WebCore::FontPlatformData& platformData)
{
    m_familyIndex = cAllFamiliesScanned;
}

}
