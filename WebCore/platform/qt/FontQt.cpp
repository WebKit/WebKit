/*
    Copyright (C) 2007 Trolltech ASA

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "config.h"
#include "Font.h"
#include "FontDescription.h"
#include "TextStyle.h"

#include "GraphicsContext.h"
#include <QTextLayout>
#include <QPainter>
#include <QFontMetrics>
#include <QFontInfo>
#include <qalgorithms.h>
#include <qdebug.h>

#include <limits.h>
namespace WebCore {

Font::Font()
    : m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_font()
    , m_metrics(QFont())
{
    m_spaceWidth = m_metrics.width(QLatin1Char(' '));
}

Font::Font(const FontDescription& description, short letterSpacing, short wordSpacing)
    : m_fontDescription(description)
    , m_letterSpacing(letterSpacing)
    , m_wordSpacing(wordSpacing)
    , m_metrics(QFont())
{
    const FontFamily* family = &description.family();
    QString familyName;
    while (family) {
        familyName += family->family();
        family = family->next();
        if (family)
            familyName += QLatin1Char(',');
    }

    m_font.setFamily(familyName);
    m_font.setPixelSize(qRound(description.computedSize()));
    m_font.setItalic(description.italic());
    if (description.bold()) {
        // Qt's Bold is 75, Webkit is 63.
        m_font.setWeight(QFont::Bold);
    } else {
        m_font.setWeight(description.weight());
    }
    m_metrics = QFontMetrics(m_font);
    m_spaceWidth = m_metrics.width(QLatin1Char(' '));
}

Font::~Font()
{
}
    
Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_letterSpacing(other.m_letterSpacing)
    , m_wordSpacing(other.m_wordSpacing)
    , m_font(other.m_font)
    , m_metrics(other.m_metrics)
    , m_spaceWidth(other.m_spaceWidth)
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_font = other.m_font;
    m_metrics = other.m_metrics;
    m_spaceWidth = other.m_spaceWidth;
    return *this;
}

void Font::update() const
{
    // don't think we need this
}

void Font::drawText(GraphicsContext* ctx, const TextRun& run, const TextStyle& style, const FloatPoint& point, int from, int to) const
{
    // FIXME: treat RTL correctly.
    if (to < 0)
        to = run.length();
    QPainter *p = ctx->platformContext();
    Color color = ctx->fillColor();
    p->setPen(QColor(color));
    p->setFont(m_font);

    int padding = style.padding();
    int numSpaces = 0;
    if (padding || m_wordSpacing) {
        for (int i = 0; i < run.length(); i++)
            if (treatAsSpace(run[i]))
                ++numSpaces;      
    }
//     qDebug() << ">>>>>>> drawText" << padding << numSpaces;
    
    if (m_letterSpacing) {
        // need to draw every letter on it's own
        int start = 0;
        qreal x = point.x();
        qreal y = point.y();
        if (treatAsSpace(run[0])) {
            int add = 0;
            if (numSpaces) {
                add = padding/numSpaces;
                padding -= add;
                --numSpaces;
            }
            x += m_wordSpacing + add;
        }
        for (int i = 1; i < from; ++i) {
            uint ch = run[i];
            if (QChar(ch).isHighSurrogate() && QChar(run[i-1]).isLowSurrogate())
                ch = QChar::surrogateToUcs4(ch, run[i-1]);
            if (QChar(ch).isLowSurrogate() || QChar::category(ch) == QChar::Mark_NonSpacing)
                continue;
            start = i;
            int add = 0;
            if (treatAsSpace(run[i])) {
                if (numSpaces) {
                    add = padding/numSpaces;
                    padding -= add;
                    --numSpaces;
                }
                x += m_wordSpacing + add;
            } else if (i >= from && i < to) {
                QString str(reinterpret_cast<const QChar*>(run.characters() + start), i - start);
                p->drawText(QPointF(x, y), str);
                x += m_metrics.width(str);
                start = i + 1;
            }
        }
        QString str(reinterpret_cast<const QChar*>(run.characters() + start), run.length() - start);
        p->drawText(QPointF(x, y), str);
    } else { //if (padding || m_wordSpacing) {
        int start = 0;
        qreal x = point.x();
        qreal y = point.y();
        for (int i = 0; i < run.length(); ++i) {
            if (treatAsSpace(run[i])) {
                QString str(reinterpret_cast<const QChar*>(run.characters() + start), i - start);
//                 qDebug() << "drawing " << str << "at " << x;
                if (i >= from && i < to) 
                    p->drawText(QPointF(x, y), str);
                x += m_metrics.width(str);

                int add = 0;
                if (numSpaces) {
                    add = padding/numSpaces;
                    padding -= add;
                    --numSpaces;
                }
                x += m_wordSpacing + add + spaceWidth();
                start = i + 1;
            }
        }
        QString str(reinterpret_cast<const QChar*>(run.characters() + start), run.length() - start);
//         qDebug() << "last drawing " << str << "at " << x;
        p->drawText(QPointF(x, y), str);
//     } else {
//         p->drawText(point, QString(reinterpret_cast<const QChar*>(run.characters() + from), to - from));
    }
}

int Font::width(const TextRun& run, const TextStyle& style) const
{
    int w = 0;
    if (style.padding() || m_wordSpacing) {
        int numSpaces = 0;
        for (int i = 0; i < run.length(); i++)
            if (treatAsSpace(run[i]))
                ++numSpaces;      

        if (numSpaces == 0)
            w += style.padding();

        if (m_wordSpacing)
            w += m_wordSpacing*numSpaces;
    }
    if (m_letterSpacing) {
        for (int i = 1; i < run.length(); ++i) {
            uint ch = run[i];
            if (QChar(ch).isLowSurrogate() || QChar::category(ch) == QChar::Mark_NonSpacing)
                continue;
            if (QChar(ch).isHighSurrogate() && QChar(run[i-1]).isLowSurrogate()) {
                ch = QChar::surrogateToUcs4(ch, run[i-1]);
                w += m_metrics.width(QString((QChar *)run.characters() + i - 1, 2));
            } else if (treatAsSpace(ch)) {
                w += spaceWidth();
            } else {
                w += m_metrics.width(QChar(run[i]));
            }
            w += m_letterSpacing;
        }
    } else {
        int start = 0;
        for (int i = 0; i < run.length(); ++i) {
            if (treatAsSpace(run[i])) {
                QString str(reinterpret_cast<const QChar*>(run.characters() + start), i - start);
                w += m_metrics.width(str) + spaceWidth();
                start = i + 1;
            }
        }
        QString str(reinterpret_cast<const QChar*>(run.characters() + start), run.length() - start);
        w += m_metrics.width(str);
    }
//     qDebug() << ">>>> width" << QString::fromRawData(reinterpret_cast<const QChar*>(run.characters()), run.length()) << w << style.padding() << m_wordSpacing << m_letterSpacing << hex << run[0];
    return w;
}

int Font::width(const TextRun& run) const
{
    return width(run, TextStyle());
}

float Font::floatWidth(const TextRun& run, const TextStyle& style) const
{
    return width(run, style);
}

float Font::floatWidth(const TextRun& run) const
{
    return width(run);
}

int Font::offsetForPosition(const TextRun& run, const TextStyle&, int position, bool includePartialGlyphs) const
{
    QTextLayout layout(QString::fromRawData(reinterpret_cast<const QChar*>(run.characters()), run.length()),
                m_font);
    layout.beginLayout();
    QTextLine l = layout.createLine();
    if (!l.isValid())
        return 0;

    l.setLineWidth(INT_MAX/256);
    layout.endLayout();

    return (int)l.cursorToX(position);
    
}

FloatRect Font::selectionRectForText(const TextRun& run, const TextStyle& style, const IntPoint&, int h,
                                     int from, int to) const
{
    QTextLayout layout(QString::fromRawData(reinterpret_cast<const QChar*>(run.characters()), run.length()),
                m_font);
    layout.beginLayout();
    QTextLine l = layout.createLine();
    if (!l.isValid())
        return FloatRect(0, 0, 0, m_metrics.height());

    l.setLineWidth(INT_MAX/256);
    layout.endLayout();

    qreal x1 = l.cursorToX(from);
    qreal x2 = l.cursorToX(to);
    if (x2 < x1)
        qSwap(x1, x2);

    return FloatRect(x1, 0, x2 - x1, l.height());
}

bool Font::isFixedPitch() const
{
    return QFontInfo(m_font).fixedPitch();
}

// Metrics that we query the FontFallbackList for.
int Font::ascent() const
{
    return m_metrics.ascent();
}

int Font::descent() const
{
    return m_metrics.descent();
}

int Font::lineSpacing() const
{
    return m_metrics.lineSpacing();
}

float Font::xHeight() const
{
    return m_metrics.xHeight();
}

int Font::spaceWidth() const
{
    return m_spaceWidth;
}

Font::operator QFont() const
{
    return m_font;
}

}
