/*
    Copyright (C) 2008, 2009, 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include <QFontMetricsF>

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
    if (!m_platformData.size()) {
         m_fontMetrics.reset();
         m_avgCharWidth = 0;
         m_maxCharWidth = 0;
         return;
    }

    QFontMetricsF fm(m_platformData.font());

    // Qt subtracts 1 from the descent to account for the baseline,
    // we add it back here to get correct metrics for WebKit.
    float descent = fm.descent() + 1;
    float ascent = fm.ascent();

    float lineSpacing = fm.lineSpacing();

    // The line spacing should always be >= (ascent + descent), but this
    // may be false in some cases due to misbehaving platform libraries.
    // Workaround from SimpleFontPango.cpp and SimpleFontFreeType.cpp
    if (lineSpacing < ascent + descent)
        lineSpacing = ascent + descent;

    // QFontMetricsF::leading() may return negative values on platforms
    // such as FreeType. Calculate the line gap manually instead.
    float lineGap = lineSpacing - ascent - descent;

    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setLineSpacing(lineSpacing);
    m_fontMetrics.setXHeight(fm.xHeight());
    m_fontMetrics.setLineGap(lineGap);
    m_spaceWidth = fm.width(QLatin1Char(' '));
}

void SimpleFontData::platformGlyphInit()
{
    if (!m_platformData.size())
        return;
    m_spaceGlyph = 0;
    determinePitch();
    m_missingGlyphData.fontData = this;
    m_missingGlyphData.glyph = 0;
}

void SimpleFontData::platformCharWidthInit()
{
    if (!m_platformData.size())
        return;
    QFontMetrics fm(m_platformData.font());
    m_avgCharWidth = fm.averageCharWidth();
    m_maxCharWidth = fm.maxWidth();
}

void SimpleFontData::platformDestroy()
{
}

}
