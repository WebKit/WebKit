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

static inline bool isEmtpyValue(const float size, const bool bold, const bool oblique)
{
     // this is the empty value by definition of the trait FontDataCacheKeyTraits
    return !bold && !oblique && size == 0.f;
}

FontPlatformData::FontPlatformData(float size, bool bold, bool oblique)
{
    if (isEmtpyValue(size, bold, oblique))
        m_data = 0;
    else
        m_data = new FontPlatformDataPrivate(size, bold, oblique);
}

FontPlatformData::FontPlatformData(const FontPlatformData &other) : m_data(other.m_data)
{
    if (m_data && m_data != reinterpret_cast<FontPlatformDataPrivate*>(-1))
        ++m_data->refCount;
}

FontPlatformData::FontPlatformData(const FontDescription& description, const AtomicString& familyName, int wordSpacing, int letterSpacing)
    : m_data(new FontPlatformDataPrivate())
{
    QFont& font = m_data->font;
    font.setFamily(familyName);
    font.setPixelSize(qRound(description.computedSize()));
    font.setItalic(description.italic());
    font.setWeight(toQFontWeight(description.weight()));
    font.setWordSpacing(wordSpacing);
    font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    const bool smallCaps = description.smallCaps();
    font.setCapitalization(smallCaps ? QFont::SmallCaps : QFont::MixedCase);

    m_data->bold = font.bold();
    m_data->size = font.pointSizeF();
}

FontPlatformData::~FontPlatformData()
{
    if (!m_data || m_data == reinterpret_cast<FontPlatformDataPrivate*>(-1))
        return;
    --m_data->refCount;
    if (!m_data->refCount)
        delete m_data;
}

FontPlatformData& FontPlatformData::operator=(const FontPlatformData& other)
{
    if (m_data == other.m_data)
        return *this;
    if (m_data && m_data != reinterpret_cast<FontPlatformDataPrivate*>(-1)) {
        --m_data->refCount;
        if (!m_data->refCount)
            delete m_data;
    }
    m_data = other.m_data;
    if (m_data && m_data != reinterpret_cast<FontPlatformDataPrivate*>(-1))
        ++m_data->refCount;
    return *this;
}

bool FontPlatformData::operator==(const FontPlatformData& other) const
{
    if (m_data == other.m_data)
        return true;

    if (!m_data || !other.m_data
        || m_data == reinterpret_cast<FontPlatformDataPrivate*>(-1) || other.m_data == reinterpret_cast<FontPlatformDataPrivate*>(-1))
        return  false;

    const bool equals = (m_data->size == other.m_data->size
                         && m_data->bold == other.m_data->bold
                         && m_data->oblique == other.m_data->oblique
                         && m_data->font == other.m_data->font);
    return equals;
}

unsigned FontPlatformData::hash() const
{
    if (!m_data)
        return 0;
    if (m_data == reinterpret_cast<FontPlatformDataPrivate*>(-1))
        return 1;
    return qHash(m_data->font.toString())
           ^ qHash(*reinterpret_cast<quint32*>(&m_data->size))
           ^ qHash(m_data->bold)
           ^ qHash(m_data->oblique);
}

#ifndef NDEBUG
String FontPlatformData::description() const
{
    return String();
}
#endif

}
