/*
    Copyright (C) 2008 Holger Hans Peter Freyther
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

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

*/

#include "config.h"
#include "FontPlatformData.h"

#include "PlatformString.h"

namespace WebCore {

FontPlatformData::FontPlatformData(const FontDescription& description, int wordSpacing, int letterSpacing)
    : m_size(0.0f)
    , m_bold(false)
    , m_oblique(false)
{
    QString familyName;
    const FontFamily* family = &description.family();
    while (family) {
        familyName += family->family();
        family = family->next();
        if (family)
            familyName += QLatin1Char(',');
    }

    m_font.setFamily(familyName);
    m_font.setPixelSize(qRound(description.computedSize()));
    m_font.setItalic(description.italic());

    m_font.setWeight(toQFontWeight(description.weight()));
    m_bold = m_font.bold();

    bool smallCaps = description.smallCaps();
    m_font.setCapitalization(smallCaps ? QFont::SmallCaps : QFont::MixedCase);
    m_font.setWordSpacing(wordSpacing);
    m_font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    m_size = m_font.pointSize();
}

FontPlatformData::FontPlatformData(const QFont& font, bool bold)
    : m_size(font.pointSize())
    , m_bold(bold)
    , m_oblique(false)
    , m_font(font)
{
}

#if ENABLE(SVG_FONTS)
FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
    : m_size(size)
    , m_bold(bold)
    , m_oblique(oblique)
{
}
#endif

FontPlatformData::FontPlatformData()
    : m_size(0.0f)
    , m_bold(false)
    , m_oblique(false)
{
}

#ifndef NDEBUG
String FontPlatformData::description() const
{
    return String();
}
#endif

}
