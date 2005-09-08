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

#ifndef KDOM_FONT_H
#define KDOM_FONT_H

#include <qcolor.h>
#include <qfont.h>
#include <qpainter.h>
#include <qfontmetrics.h>

class Q3PaintDeviceMetrics;

namespace KDOM
{

class FontDef
{
public:
    FontDef() : size(0), italic(false), smallCaps(false), weight(50), hasNbsp(true) { }

    bool operator==(const FontDef &other) const
    {
        return (family == other.family && size == other.size &&
                italic == other.italic && smallCaps == other.smallCaps &&
                weight == other.weight);
    }

    QString family;

    short int size;
    bool italic : 1;
    bool smallCaps : 1;
    unsigned int weight : 8;

    bool hasNbsp : 1;
};

class KDOMSettings;
class Font
{
public:
    Font();
    Font(const Font &o);
    Font(const FontDef &fd);
    ~Font();

    bool operator==(const Font &other) const;
    
    void update(Q3PaintDeviceMetrics *devMetrics, const KDOMSettings *settings) const;

    /**
     * Draws a piece from the given piece of text.
     * @param p painter
     * @param x x-coordinate to begin drawing, always denotes leftmost position
     * @param y y-coordinate of baseline of text
     * @param str string to draw a piece from
     * @param slen total length of string
     * @param pos zero-based offset of beginning of piece
     * @param len length of piece
     * @param width additional pixels to be distributed equally among all spaces
     * @param d text direction
     * @param from begin with this position relative to @p pos, -1 to start at @p pos
     * @param to stop before this position relative to @p pos, -1 to use full length of piece
     * @param bg if valid, fill the background of the drawn piece with this color
     * @param uy y-coordinate of top position, used for background and text decoration painting
     * @param h total height of line, only used for background and text    decoration painting
     * @param deco combined text decoration (see Decoration)
     */
    void drawText(QPainter *p, int x, int y, QChar *str, int slen, int pos, int len, int width,
                  Qt::LayoutDirection d, int from = -1, int to = -1, QColor bg = QColor(),
                  int uy = -1, int h = -1, int deco = 0) const;

    /** returns the width of the given string chunk in pixels.
     *
     * The method also considers various styles like text-align and font-variant
     * @param str pointer to string
     * @param slen total length of string
     * @param pos zero-based position in string where to start measuring
     * @param len count of characters up to which the width should be determined
     */
    int width(QChar *str, int slen, int pos, int len) const;

    /** return the width of the given char in pixels.
     *
     * The method also considers various styles like text-align and font-variant
     * @param str pointer to string
     * @param slen total length of string
     * @param pos zero-based position of char in string
     */
    int width(QChar *str, int slen, int pos) const;

    /** Text decoration constants.
     *
     * The enumeration constant values match those of ETextDecoration, but only
     * a subset is supported. Keep in sync with ETextDecoration!
     */
    enum Decoration { NONE = 0x0, UNDERLINE = 0x1, OVERLINE = 0x2, LINE_THROUGH = 0x4 };

    /** draws text decoration
     * @param p painter
     * @param x x-coordinate
     * @param y top y-coordinate of line box
     * @param baseline baseline
     * @param width length of decoration in pixels
     * @param height height of line box
     * @param deco decoration to be drawn (see Decoration).
     *        (The enumeration constants may be combined.)
     */
    void drawDecoration(QPainter *p, int x, int y, int baseline, int width, int height, int deco) const;

    // Accessors
    const FontDef &fontDef() const { return m_fontDef; }

    int wordSpacing() const { return m_wordSpacing; }
    int letterSpacing() const { return m_letterSpacing; }

private:
    friend class RenderStyle;
    
    mutable FontDef m_fontDef;
    mutable QFont m_font, *m_scFont;
    mutable QFontMetrics m_fontMetrics;

    short m_wordSpacing;
    short m_letterSpacing;
};

}

#endif

// vim:ts=4:noet
