/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "SimpleFontData.h"

#include <QFontMetrics>

namespace WebCore {

void SimpleFontData::determinePitch()
{
    m_treatAsFixedPitch = m_platformData.font().fixedPitch();
}

bool SimpleFontData::containsCharacters(const UChar*, int) const
{
    return true;
}

void SimpleFontData::platformInit()
{
    QFontMetrics fm(m_platformData.font());

    m_ascent = fm.ascent();
    m_descent = fm.descent();
    m_lineSpacing = fm.lineSpacing();
    m_xHeight = fm.xHeight();
    m_spaceWidth = fm.width(QLatin1Char(' '));
    m_lineGap = fm.leading();
}

void SimpleFontData::platformGlyphInit()
{
    m_spaceGlyph = 0;
    m_adjustedSpaceWidth = m_spaceWidth;
    determinePitch();
    m_missingGlyphData.fontData = this;
    m_missingGlyphData.glyph = 0;
}

void SimpleFontData::platformCharWidthInit()
{
    QFontMetrics fm(m_platformData.font());
    m_avgCharWidth = fm.averageCharWidth();
    m_maxCharWidth = fm.maxWidth();
}

void SimpleFontData::platformDestroy()
{
}

}
