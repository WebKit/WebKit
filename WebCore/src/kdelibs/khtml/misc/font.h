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
#ifndef _html_font_h_
#define _html_font_h_

#include <qfont.h>
#include <qstringlist.h>

namespace khtml {

    /**
     * This class implements the CSS2 font selection specifications.
     *
     * QFont can only handle one character set at a time, but for HTML
     * we might need to switch charsets of the font (or even font families)
     * for HTML pages mixing several languages.
     *
     * It will also implement several font effects like small caps
     * and letter spacing.
     */
    class Font
    {
    public:
	Font();

	// attributes needed for the font description:

	// families. 5 generic family names as defined in the CSS specs
	// are allowed too
	const QStringList &families() const { return m_families; }
	void setFamilies(const QStringList &);
	
	enum Style { Normal, Italic, Oblique };
	Style style() const { return m_style; }
	void setStyle(Style);
	
	enum Variant { VariantNormal, SmallCaps };
	Variant variant() const { return m_variant; }
	void setVariant(Variant);

	// has only 5 members in the enum instead of the 9 specified in
	// HTML and CSS, but that should be enough anyway.
	QFont::Weight weight() const { return m_weight; }
	void setWeight(QFont::Weight);
	
	int size() const { return m_size; }
	void setSize(int);

	// these are not used at the moment since QFont doesn't support them
	enum Stretch { UltraCondensed, ExtraCondensed, Condensed,
		       SemiCondensed, StretchNormal, SemiExpanded, Expanded,
		       ExtraExpanded, UltraExpanded };
	Stretch stretch() const { return StretchNormal; }
	void setStretch(Stretch) {};

	// replacement for QFontMetrics::width
	int width(const QString &str) const;
	
	// replacement for QPainter::drawText()
	// complex = true means we have to use a different QFont than the standard one
	// (or several fonts) to print the string 
	void drawText(QPainter *p, int x, int y, const QString &str, bool complex = false) const;

	// do we need complex text processing?
	bool complexText(const QString &text) const;
	
    private:
	void setDefaultFont();
	
	// the font that will be used for most of the characters in the
	// document. It's charset will be either the one specified
	// by the meta tag or by the language.
	QFont m_font;

	QStringList m_families;
	Style m_style;
	Variant m_variant;
	QFont::Weight m_weight;
	int m_size;
	QFont::CharSet m_defCharset;
    };

};

#endif
