/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
#include "Font.h"

#include "FontDataSet.h"
#include "GraphicsContext.h"
#include "khtml_settings.h"
#include <algorithm>

namespace WebCore {

Font::Font() :m_dataSet(0), m_letterSpacing(0), m_wordSpacing(0) {}
Font::Font(const FontDescription& fd, short letterSpacing, short wordSpacing) 
: m_fontDescription(fd),
  m_dataSet(0),
  m_letterSpacing(letterSpacing),
  m_wordSpacing(wordSpacing)
{}

Font::Font(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_dataSet = other.m_dataSet;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
}

Font& Font::operator=(const Font& other)
{
    if (&other != this) {
        m_fontDescription = other.m_fontDescription;
        m_dataSet = other.m_dataSet;
        m_letterSpacing = other.m_letterSpacing;
        m_wordSpacing = other.m_wordSpacing;
    }
    return *this;
}

Font::~Font()
{
}

void Font::update() const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up 
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway.  Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble).  Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_dataSet)
        m_dataSet = new FontDataSet();
    m_dataSet->invalidate();
}

int Font::width(const QChar* chs, int slen, int pos, int len, int tabWidth, int xpos) const
{
    // FIXME: Want to define an lroundf for win32.
#if __APPLE__
    return lroundf(floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos));
#else
    return floatWidth(chs + pos, slen - pos, 0, len, tabWidth, xpos) + 0.5f;
#endif
}

}
