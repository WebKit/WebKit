/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef HAVE_ALLOCA_H
#  include <alloca.h>
#endif

#include <kdebug.h>
#include <kglobal.h>

#include <qfontdatabase.h>
#include <qpaintdevicemetrics.h>

#include "Font.h"
#include "KDOMSettings.h"

using namespace KDOM;

Font::Font() : m_fontDef(), m_font(), m_scFont(0), m_fontMetrics(m_font), m_wordSpacing(0), m_letterSpacing(0)
{
}

Font::Font(const Font &o) : m_fontDef(o.m_fontDef), m_font(o.m_font), m_scFont(o.m_scFont),
							m_fontMetrics(o.m_fontMetrics), m_wordSpacing(o.m_wordSpacing),
							m_letterSpacing(o.m_letterSpacing)
{
	if(o.m_scFont)
		m_scFont = new QFont(*o.m_scFont);
}

Font::Font(const FontDef &fd) : m_fontDef(fd), m_font(), m_scFont(0),
								m_fontMetrics(m_font), m_wordSpacing(0), m_letterSpacing(0)
{
}

Font::~Font()
{
	delete m_scFont;
}

bool Font::operator==(const Font &other) const
{
	return (m_fontDef == other.m_fontDef &&
			m_wordSpacing == other.m_wordSpacing &&
			m_letterSpacing == other.m_letterSpacing);
}

void Font::update(QPaintDeviceMetrics *devMetrics, const KDOMSettings *settings) const
{
#ifndef APPLE_COMPILE_HACK
	m_font.setFamily(m_fontDef.family.isEmpty() ? settings->stdFontName() : m_fontDef.family);

	m_font.setFamily(m_fontDef.family);
	m_font.setItalic(m_fontDef.italic);
	m_font.setWeight(m_fontDef.weight);

	int size = m_fontDef.size;
	const int lDpiY = kMax(devMetrics->logicalDpiY(), 96);
        
	QFontDatabase db;

	// ok, now some magic to get a nice unscaled font
	// all other font properties should be set before this one!!!!
	if(!db.isSmoothlyScalable(m_font.family(), db.styleString(m_font)))
	{
		const QValueList<int> pointSizes = db.smoothSizes(m_font.family(), db.styleString(m_font));
		
		// lets see if we find a nice looking font, which is not too far away from the requested one.
		// kdDebug(6080) << "khtml::setFontSize family = " << m_font.family() << " size requested=" << size << endl;

		float diff = 1; // ### 100% deviation
		float bestSize = 0;

		QValueList<int>::ConstIterator it = pointSizes.begin();
		const QValueList<int>::ConstIterator itEnd = pointSizes.end();

		for(; it != itEnd; ++it)
		{
			float newDiff = ((*it) * (lDpiY / 72.) - float(size)) /  float(size);
			// kdDebug( 6080 ) << "smooth font size: " << *it << " diff=" << newDiff << endl;
			
			if(newDiff < 0)
				newDiff = -newDiff;
			
			if(newDiff < diff)
			{
				diff = newDiff;
				bestSize = *it;
			}
		}
		
		// kdDebug( 6080 ) << "best smooth font size: " << bestSize << " diff=" << diff << endl;
		if(bestSize != 0 && diff < 0.2) // 20% deviation, otherwise we use a scaled font...
			size = (int)((bestSize * lDpiY) / 72);
	}

	// make sure we don't bust up X11
	size = kMax(0, kMin(255, size));

	qDebug("setting font to %s, italic=%d, weight=%d, size=%d", m_fontDef.family.latin1(), m_fontDef.italic, m_fontDef.weight, size );

	m_font.setPixelSize(size);

	m_fontMetrics = QFontMetrics(m_font);
	m_fontDef.hasNbsp = m_fontMetrics.inFont(0xa0);

	// small caps
	delete m_scFont;
	m_scFont = 0;

	if(m_fontDef.smallCaps)
	{
		m_scFont = new QFont(m_font);
		m_scFont->setPixelSize(m_font.pixelSize() * 7 / 10);
	}
#endif
}

#ifndef APPLE_COMPILE_HACK

/** closes the current word and returns its width in pixels
 * @param fm metrics of font to be used
 * @param str string
 * @param pos zero-indexed position within @p str upon which all other
 *	indices are based
 * @param wordStart relative index pointing to the position where the word started
 * @param wordEnd relative index pointing one position after the word ended
 * @return the width in pixels. May be 0 if @p wordStart and @p wordEnd were
 *	equal.
 */
inline int closeWordAndGetWidth(const QFontMetrics &fm, const QChar *str, int pos, int wordStart, int wordEnd)
{
	if(wordEnd <= wordStart)
		return 0;

	QConstString s(str + pos + wordStart, wordEnd - wordStart);
	return fm.width(s.string());
}

/** closes the current word and draws it
 * @param p painter
 * @param d text direction
 * @param x current x position, will be inc-/decremented correctly according
 *	to text direction
 * @param y baseline of text
 * @param widths list of widths; width of word is expected at position
 *		wordStart
 * @param str string
 * @param pos zero-indexed position within @p str upon which all other
 *	indices are based
 * @param wordStart relative index pointing to the position where the word started,
 *	will be set to wordEnd after function
 * @param wordEnd relative index pointing one position after the word ended
 */
inline void closeAndDrawWord(QPainter *p, QPainter::TextDirection d, int &x, int y,
							 const short widths[], const QChar *str, int pos, int &wordStart, int wordEnd)
{
	if(wordEnd <= wordStart)
		return;

	int width = widths[wordStart];
	if(d == QPainter::RTL)
		x -= width;

	QConstString s(str + pos + wordStart, wordEnd - wordStart);
	p->drawText(x, y, s.string(), -1, d);

	if(d != QPainter::RTL)
		x += width;

	wordStart = wordEnd;
}

void Font::drawText(QPainter *p, int x, int y, QChar *str, int slen, int pos, int len,
					int toAdd, QPainter::TextDirection d, int from, int to, QColor bg, int uy, int h, int deco) const
{
	if(!str)
		return;
	
	QConstString cstr = QConstString(str, slen);
	QString qstr = cstr.string();

	// ### fixme for RTL
	if(!m_scFont && !m_letterSpacing && !m_wordSpacing && !toAdd && from == -1)
	{
		// simply draw it
		// Due to some unfounded cause QPainter::drawText traverses the
		// *whole* string when painting, not only the specified
		// [pos, pos + len) segment. This makes painting *extremely* slow for
		// long render texts (in the order of several megabytes).
		// Hence, only hand over the piece of text of the actual inline text box
		QConstString cstr = QConstString(str + pos, len);
		p->drawText( x, y, cstr.string(), 0, len, d );
	}
	else
	{
		if(from < 0)
			from = 0;
		if(to < 0)
			to = len;

		int numSpaces = 0;
		if(toAdd)
		{
			for(int i = 0; i < len; ++i)
			{
				if(str[i + pos].category() == QChar::Separator_Space)
					++numSpaces;
			}

			const int totWidth = width(str, slen, pos, len);
			if(d == QPainter::RTL)
				x += totWidth + toAdd;
			
			QString upper = qstr;
			QFontMetrics sc_fm = m_fontMetrics;
			if(m_scFont)
			{
				// draw in small caps
				upper = qstr.upper();
				sc_fm = QFontMetrics(*m_scFont);
			}

			// ### sc could be optimized by only painting uppercase letters extra,
			// and treat the rest WordWise, but I think it's not worth it.
			// Somebody else may volunteer, and implement this ;-) (LS)

			// The mode determines whether the text is displayed character by
			// character, word by word, or as a whole
			enum { CharacterWise, WordWise, Whole }
			mode = Whole;
			if(!m_letterSpacing && !m_scFont && (m_wordSpacing || toAdd > 0))
				mode = WordWise;
			else if(m_letterSpacing || m_scFont)
				mode = CharacterWise;

			if(mode == Whole) // most likely variant is treated extra
			{
				if(to < 0)
					to = len;
				
				const QConstString cstr(str + pos, len);
				const QConstString segStr(str + pos + from, to - from);
				const int preSegmentWidth = m_fontMetrics.width(cstr.string(), from);
				const int segmentWidth = m_fontMetrics.width(segStr.string());
				const int eff_x = d == QPainter::RTL ? x - preSegmentWidth - segmentWidth : x + preSegmentWidth;

				// draw whole string segment, with optional background
				if(bg.isValid())
					p->fillRect(eff_x, uy, segmentWidth, h, bg);
				
				p->drawText(eff_x, y, segStr.string(), -1, d);
				
				if(deco)
					drawDecoration(p, eff_x, uy, y - uy, segmentWidth - 1, h, deco);
					
				return;
			}

			// We are using two passes. In the first pass, the widths are collected,
			// and stored. In the second, the actual characters are drawn.

			// For each letter in the text box, save the width of the character.
			// When word-wise, only the first letter contains the width, but of the
			// whole word.
			short *const widthList = (short *) alloca(to * sizeof(short));

			// First pass: gather widths
			int preSegmentWidth = 0;
			int segmentWidth = 0;
			int lastWordBegin = 0;
			bool onSegment = from == 0;
			for(int i = 0; i < to; ++i)
			{
				if (i == from)
				{
					// Also close words on visibility boundary
					if(mode == WordWise)
					{
						const int width = closeWordAndGetWidth(m_fontMetrics, str, pos, lastWordBegin, i);

						if(lastWordBegin < i)
						{
							widthList[lastWordBegin] = (short) width;
							lastWordBegin = i;
							preSegmentWidth += width;
						}
					}

					onSegment = true;
				}

				const QChar ch = str[pos + i];
				bool lowercase = (ch.category() == QChar::Letter_Lowercase);
				bool is_space = (ch.category() == QChar::Separator_Space);
				
				int chw = 0;
				if(m_letterSpacing)
					chw += m_letterSpacing;
					
				if((m_wordSpacing || toAdd) && is_space)
				{
					if(mode == WordWise)
					{
						const int width = closeWordAndGetWidth(m_fontMetrics, str, pos, lastWordBegin, i);
						if(lastWordBegin < i)
						{
							widthList[lastWordBegin] = (short)width;
							lastWordBegin = i;
							(onSegment ? segmentWidth : preSegmentWidth) += width;
						}
						
						++lastWordBegin; // ignore this space
					}
					
					chw += m_wordSpacing;
					if(numSpaces)
					{
						const int a = toAdd / numSpaces;
						chw += a;
						toAdd -= a;
						--numSpaces;
					}
				}
				
				if(is_space || mode == CharacterWise)
				{
					chw += lowercase ? sc_fm.charWidth(upper, pos + i) : m_fontMetrics.charWidth(qstr, pos + i);
					widthList[i] = (short) chw;

					(onSegment ? segmentWidth : preSegmentWidth) += chw;
				}

			}

			// close last word
			Q_ASSERT(onSegment);
			if(mode == WordWise)
			{
				const int width = closeWordAndGetWidth(m_fontMetrics, str, pos, lastWordBegin, to);
				segmentWidth += width;
				widthList[lastWordBegin] = (short) width;
			}

			if(d == QPainter::RTL)
				x -= preSegmentWidth;
			else
				x += preSegmentWidth;

			const int startx = d == QPainter::RTL ? x-segmentWidth : x;

			// optionally draw background
			if(bg.isValid())
				p->fillRect(startx, uy, segmentWidth, h, bg);

			// second pass: do the actual drawing
			lastWordBegin = from;
			for(int i = from; i < to; ++i)
			{
				const QChar ch = str[pos+i];
				
				bool lowercase = (ch.category() == QChar::Letter_Lowercase);
				bool is_space = (ch.category() == QChar::Separator_Space);
				if(is_space)
				{
					if(mode == WordWise)
					{	
						closeAndDrawWord(p, d, x, y, widthList, str, pos, lastWordBegin, i);
						++lastWordBegin; // jump over space
					}
				}
				
				if(is_space || mode == CharacterWise)
				{
					const int chw = widthList[i];
					if(d == QPainter::RTL)
						x -= chw;

					if(m_scFont)
						p->setFont(lowercase ? *m_scFont : m_font);

					p->drawText(x, y, (lowercase ? upper : qstr), pos + i, 1, d);

					if(d != QPainter::RTL)
						x += chw;
				}
			}

			// don't forget to draw last word
			if(mode == WordWise)
				closeAndDrawWord(p, d, x, y, widthList, str, pos, lastWordBegin, to);

			if(deco)
				drawDecoration(p, startx, uy, y - uy, segmentWidth - 1, h, deco);

			if(m_scFont)
				p->setFont(m_font);
		}
	}
}

int Font::width(QChar *chs, int, int pos, int len) const
{
	const QConstString cstr(chs + pos, len);
	int w = 0;

	const QString qstr = cstr.string();
	if(m_scFont)
	{
		const QString upper = qstr.upper();
		const QChar *uc = qstr.unicode();
		const QFontMetrics sc_fm(*m_scFont);
		for(int i = 0; i < len; ++i)
		{
			if((uc + i)->category() == QChar::Letter_Lowercase)
				w += sc_fm.charWidth(upper, i);
			else
				w += m_fontMetrics.charWidth(qstr, i);
		}
	}
	else // ### might be a little inaccurate
		w = m_fontMetrics.width(qstr);
	
	if(m_letterSpacing)
		w += len * m_letterSpacing;

	if(m_wordSpacing)
	{
		// add amount
		for(int i = 0; i < len; ++i)
		{
			if(chs[i + pos].category() == QChar::Separator_Space)
				w += m_wordSpacing;
		}
	}

	return w;
}

int Font::width(QChar *chs, int slen, int pos) const
{
	int w;
	if(m_scFont && chs[pos].category() == QChar::Letter_Lowercase)
	{
		QString str(chs, slen);
		str[pos] = chs[pos].upper();
		w = QFontMetrics(*m_scFont).charWidth(str, pos);
	}
	else
	{
		const QConstString cstr(chs, slen);
		w = m_fontMetrics.charWidth(cstr.string(), pos);
	}

	if(m_letterSpacing)
		w += m_letterSpacing;

	if(m_wordSpacing && (chs + pos)->category() == QChar::Separator_Space)
		w += m_wordSpacing;
		
	return w;
}

void Font::drawDecoration(QPainter *pt, int _tx, int _ty, int baseline, int width, int height, int deco) const
{
	Q_UNUSED(height);
	
	// thick lines on small fonts look ugly
	const int thickness = m_fontMetrics.height() > 20 ? m_fontMetrics.lineWidth() : 1;
	const QBrush brush = pt->pen().color();
	
	if(deco & UNDERLINE)
	{
		int underlineOffset = (m_fontMetrics.height() + baseline) / 2;
		if(underlineOffset <= baseline)
			underlineOffset = baseline + 1;

		pt->fillRect(_tx, _ty + underlineOffset, width + 1, thickness, brush);
	}

	if(deco & OVERLINE)
		pt->fillRect(_tx, _ty, width + 1, thickness, brush);
	
	if (deco & LINE_THROUGH)
		pt->fillRect(_tx, _ty + 2 * baseline / 3, width + 1, thickness, brush);
}

#endif

// vim:ts=4:noet
