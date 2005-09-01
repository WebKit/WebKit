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

#include <qstringlist.h>

#include <kdom/css/RGBColorImpl.h>

#include "ksvg.h"
#include <ksvg2/css/cssvalues.h>
#include "SVGColorImpl.h"
#include "CDFInterface.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGColorImpl::SVGColorImpl() : KDOM::CSSValueImpl(), m_rgbColor(0)
{
    m_colorType = SVG_COLORTYPE_UNKNOWN;
}

SVGColorImpl::SVGColorImpl(KDOM::DOMStringImpl *rgbColor) : KDOM::CSSValueImpl(), m_rgbColor(0)
{
    m_colorType = SVG_COLORTYPE_RGBCOLOR;
    setRGBColor(rgbColor);
}

SVGColorImpl::SVGColorImpl(unsigned short colorType) : KDOM::CSSValueImpl(), m_rgbColor(0)
{
    m_colorType = colorType;
}

SVGColorImpl::~SVGColorImpl()
{
    if(m_rgbColor)
        m_rgbColor->deref();
}

unsigned short SVGColorImpl::colorType() const
{
    return m_colorType;
}

KDOM::RGBColorImpl *SVGColorImpl::rgbColor() const
{
    return new KDOM::RGBColorImpl(SVGDOMImplementationImpl::self()->cdfInterface(), m_qColor);
}

static const QColor cmap[] =
{
    QColor(240, 248, 255),
    QColor(250, 235, 215),
    QColor(0, 255, 255),
    QColor(127, 255, 212),
    QColor(240, 255, 255),
    QColor(245, 245, 220),
    QColor(255, 228, 196),
    QColor(0, 0, 0),
    QColor(255, 235, 205),
    QColor(0, 0, 255),
    QColor(138, 43, 226),
    QColor(165, 42, 42),
    QColor(222, 184, 135),
    QColor(95, 158, 160),
    QColor(127, 255, 0),
    QColor(210, 105, 30),
    QColor(255, 127, 80),
    QColor(100, 149, 237),
    QColor(255, 248, 220),
    QColor(220, 20, 60),
    QColor(0, 255, 255),
    QColor(0, 0, 139),
    QColor(0, 139, 139),
    QColor(184, 134, 11),
    QColor(169, 169, 169),
    QColor(169, 169, 169),
    QColor(0, 100, 0),
    QColor(189, 183, 107),
    QColor(139, 0, 139),
    QColor(85, 107, 47),
    QColor(255, 140, 0),
    QColor(153, 50, 204),
    QColor(139, 0, 0),
    QColor(233, 150, 122),
    QColor(143, 188, 143),
    QColor(72, 61, 139),
    QColor(47, 79, 79),
    QColor(47, 79, 79),
    QColor(0, 206, 209),
    QColor(148, 0, 211),
    QColor(255, 20, 147),
    QColor(0, 191, 255),
    QColor(105, 105, 105),
    QColor(105, 105, 105),
    QColor(30, 144, 255),
    QColor(178, 34, 34),
    QColor(255, 250, 240),
    QColor(34, 139, 34),
    QColor(255, 0, 255),
    QColor(220, 220, 220),
    QColor(248, 248, 255),
    QColor(255, 215, 0),
    QColor(218, 165, 32),
    QColor(128, 128, 128),
    QColor(128, 128, 128),
    QColor(0, 128, 0),
    QColor(173, 255, 47),
    QColor(240, 255, 240),
    QColor(255, 105, 180),
    QColor(205, 92, 92),
    QColor(75, 0, 130),
    QColor(255, 255, 240),
    QColor(240, 230, 140),
    QColor(230, 230, 250),
    QColor(255, 240, 245),
    QColor(124, 252, 0),
    QColor(255, 250, 205),
    QColor(173, 216, 230),
    QColor(240, 128, 128),
    QColor(224, 255, 255),
    QColor(250, 250, 210),
    QColor(211, 211, 211),
    QColor(211, 211, 211),
    QColor(144, 238, 144),
    QColor(255, 182, 193),
    QColor(255, 160, 122),
    QColor(32, 178, 170),
    QColor(135, 206, 250),
    QColor(119, 136, 153),
    QColor(119, 136, 153),
    QColor(176, 196, 222),
    QColor(255, 255, 224),
    QColor(0, 255, 0),
    QColor(50, 205, 50),
    QColor(250, 240, 230),
    QColor(255, 0, 255),
    QColor(128, 0, 0),
    QColor(102, 205, 170),
    QColor(0, 0, 205),
    QColor(186, 85, 211),
    QColor(147, 112, 219),
    QColor(60, 179, 113),
    QColor(123, 104, 238),
    QColor(0, 250, 154),
    QColor(72, 209, 204),
    QColor(199, 21, 133),
    QColor(25, 25, 112),
    QColor(245, 255, 250),
    QColor(255, 228, 225),
    QColor(255, 228, 181),
    QColor(255, 222, 173),
    QColor(0, 0, 128),
    QColor(253, 245, 230),
    QColor(128, 128, 0),
    QColor(107, 142, 35),
    QColor(255, 165, 0),
    QColor(255, 69, 0),
    QColor(218, 112, 214),
    QColor(238, 232, 170),
    QColor(152, 251, 152),
    QColor(175, 238, 238),
    QColor(219, 112, 147),
    QColor(255, 239, 213),
    QColor(255, 218, 185),
    QColor(205, 133, 63),
    QColor(255, 192, 203),
    QColor(221, 160, 221),
    QColor(176, 224, 230),
    QColor(128, 0, 128),
    QColor(255, 0, 0),
    QColor(188, 143, 143),
    QColor(65, 105, 225),
    QColor(139, 69, 19),
    QColor(250, 128, 114),
    QColor(244, 164, 96),
    QColor(46, 139, 87),
    QColor(255, 245, 238),
    QColor(160, 82, 45),
    QColor(192, 192, 192),
    QColor(135, 206, 235),
    QColor(106, 90, 205),
    QColor(112, 128, 144),
    QColor(112, 128, 144),
    QColor(255, 250, 250),
    QColor(0, 255, 127),
    QColor(70, 130, 180),
    QColor(210, 180, 140),
    QColor(0, 128, 128),
    QColor(216, 191, 216),
    QColor(255, 99, 71),
    QColor(64, 224, 208),
    QColor(238, 130, 238),
    QColor(245, 222, 179),
    QColor(255, 255, 255),
    QColor(245, 245, 245),
    QColor(255, 255, 0),
    QColor(154, 205, 50)
};

void SVGColorImpl::setRGBColor(KDOM::DOMStringImpl *rgbColor)
{
    KDOM_SAFE_SET(m_rgbColor, rgbColor);

    if(!m_rgbColor)
        return;

    QString parse = KDOM::DOMString(m_rgbColor).string().stripWhiteSpace();
    if(parse.startsWith(QString::fromLatin1("rgb(")))
    {
        QStringList colors = QStringList::split(',', parse);
        QString r = colors[0].right((colors[0].length() - 4));
        QString g = colors[1];
        QString b = colors[2].left((colors[2].length() - 1));
    
        if(r.contains(QString::fromLatin1("%")))
        {
            r = r.left(r.length() - 1);
            r = QString::number(int((double(255 * r.toDouble()) / 100.0)));
        }

        if(g.contains(QString::fromLatin1("%")))
        {
            g = g.left(g.length() - 1);
            g = QString::number(int((double(255 * g.toDouble()) / 100.0)));
        }
    
        if(b.contains(QString::fromLatin1("%")))
        {
            b = b.left(b.length() - 1);
            b = QString::number(int((double(255 * b.toDouble()) / 100.0)));
        }

        m_qColor = QColor(r.toInt(), g.toInt(), b.toInt());
    }
    else
    {
        QString name(m_rgbColor->unicode(), m_rgbColor->length());
        name = name.lower();
        int col = KSVG::getValueID(name.ascii(), name.length());
        if(col == 0)
            m_qColor = QColor(name);
        else
            m_qColor = cmap[col - SVGCSS_VAL_ALICEBLUE];
    }
}

void SVGColorImpl::setRGBColorICCColor(KDOM::DOMStringImpl * /* rgbColor */, KDOM::DOMStringImpl * /* iccColor */)
{
    // TODO: implement me!
}

void SVGColorImpl::setColor(unsigned short colorType, KDOM::DOMStringImpl * /* rgbColor */ , KDOM::DOMStringImpl * /* iccColor */)
{
    // TODO: implement me!
    m_colorType = colorType;
}

KDOM::DOMStringImpl *SVGColorImpl::cssText() const
{
    if(m_colorType == SVG_COLORTYPE_RGBCOLOR)
        return m_rgbColor;

    return 0;
}

const QColor &SVGColorImpl::color() const
{
    return m_qColor;
}

// vim:ts=4:noet
