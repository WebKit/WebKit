/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "font.h"

#include "khtml_settings.h"

#include <algorithm>

namespace khtml {

IntRect Font::selectionRectForText(int x, int y, int h, int tabWidth, int xpos, 
                      QChar *str, int slen, int pos, int len, int toAdd,
                      bool rtl, bool visuallyOrdered, int from, int to) const
{
    return fm.selectionRectForText(x, y, h, tabWidth, xpos, str + pos, std::min(slen - pos, len), from, to, toAdd, rtl, visuallyOrdered, m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps());

}

void Font::drawHighlightForText( QPainter *p, int x, int y, int h, int tabWidth, int xpos, 
                     QChar *str, int slen, int pos, int len,
                     int toAdd, QPainter::TextDirection d, bool visuallyOrdered, int from, int to, Color bg) const
{
    p->drawHighlightForText(x, y, h, tabWidth, xpos, str + pos, std::min(slen - pos, len), from, to, toAdd, bg, d, visuallyOrdered,
                m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps());
}
                     
void Font::drawText( QPainter *p, int x, int y, int tabWidth, int xpos, QChar *str, int slen, int pos, int len,
                     int toAdd, QPainter::TextDirection d, bool visuallyOrdered, int from, int to, Color bg ) const
{
    p->drawText(x, y, tabWidth, xpos, str + pos, std::min(slen - pos, len), from, to, toAdd, bg, d, visuallyOrdered,
                m_letterSpacing, m_wordSpacing, m_fontDescription.smallCaps());
}

void Font::update() const
{
    f.setFirstFamily(m_fontDescription.family());
    f.setItalic(m_fontDescription.italic());
    f.setWeight(m_fontDescription.weight());
    f.setPixelSize(m_fontDescription.computedPixelSize());
    f.setPrinterFont(m_fontDescription.usePrinterFont());

    fm.setFont(f);
}

}
