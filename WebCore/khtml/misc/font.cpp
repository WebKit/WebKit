/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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
 * $Id$
 */
#include <font.h>

#include <qfontdatabase.h>

#include <khtmldata.h>
#include <kglobal.h>
#include <kcharsets.h>

using namespace khtml;

Font::Font()
{
    m_defCharset = QFont::Latin1;
}


void Font::setFamilies(const QStringList &f)
{
    m_families = f;
}

void Font::setStyle(Style s)
{
    m_style = s;
}
	
void Font::setVariant(Variant v)
{
    m_variant = v;
}

void Font::setWeight(QFont::Weight w)
{
    m_weight = w;
}

void Font::setSize(int s)
{
    m_size = s;
}

void Font::setDefaultFont()
{
    QFontDatabase db;

    KCharsets *kc = KGlobal::charsets();
    QString style = "Normal";
    if(m_weight >= QFont::Bold && m_style > Normal)
	style = "Bold Italic";
    else if(m_weight >= QFont::Bold)
	style = "Bold";
    else if(m_style > Normal) // ### oblique == italic
	style = "Italic";
    m_font = db.font(m_families[0], style, m_size, kc->xCharsetName(m_defCharset));
}

int Font::width(const QString &str) const
{
    // ###
    return 0;
}

void Font::drawText(QPainter *p, int x, int y, const QString &str, bool complex) const
{
}

bool Font::complexText(const QString &text) const
{
	// TODO
	return true;
}

