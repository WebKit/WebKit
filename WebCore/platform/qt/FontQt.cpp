/*
 * Copyright (C) 2007 Trolltech ASA
 * 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
{
}

Font::Font(const FontDescription& description, short letterSpacing, short wordSpacing)
    : m_fontDescription(description)
    , m_letterSpacing(letterSpacing)
    , m_wordSpacing(wordSpacing)
{
    const FontFamily* family = &description.family();
    QString familyName;
    while (family) {
        familyName += family->family();
        family = family->next();
        if (family)
            familyName += QLatin1Char(';');
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
}

Font::~Font()
{
}
    
Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_letterSpacing(other.m_letterSpacing)
    , m_wordSpacing(other.m_wordSpacing)
    , m_font(other.m_font)
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_font = other.m_font;
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
        QFontMetrics fm(m_font);
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
                x += fm.width(str);
                start = i + 1;
            }
        }
        QString str(reinterpret_cast<const QChar*>(run.characters() + start), run.length() - start);
        p->drawText(QPointF(x, y), str);
    } else { //if (padding || m_wordSpacing) {
        int start = 0;
        qreal x = point.x();
        qreal y = point.y();
        QFontMetrics fm(m_font);
        for (int i = 0; i < run.length(); ++i) {
            if (treatAsSpace(run[i])) {
                QString str(reinterpret_cast<const QChar*>(run.characters() + start), i - start);
//                 qDebug() << "drawing " << str << "at " << x;
                if (i >= from && i < to) 
                    p->drawText(QPointF(x, y), str);
                x += fm.width(str);

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
//         qDebug() << "drawing " << str << "at " << x;
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
    QFontMetrics metrics(m_font);
    if (m_letterSpacing) {
        for (int i = 1; i < run.length(); ++i) {
            uint ch = run[i];
            if (QChar(ch).isHighSurrogate() && QChar(run[i-1]).isLowSurrogate()) {
                ch = QChar::surrogateToUcs4(ch, run[i-1]);
                w += metrics.width(QString((QChar *)run.characters() + i - 1, 2));
            } else {
                w += metrics.width(QChar(run[i]));
            }                
            if (QChar(ch).isLowSurrogate() || QChar::category(ch) == QChar::Mark_NonSpacing)
                continue;
        }
    } else {
        w += metrics.width(QString::fromRawData(reinterpret_cast<const QChar*>(run.characters()), run.length()));
    }
    return w;
}

int Font::width(const TextRun& run) const
{
    QFontMetrics metrics(m_font);
    return metrics.width(QString::fromRawData(reinterpret_cast<const QChar*>(run.characters()), run.length()));
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
        return FloatRect(0, 0, 0, QFontMetrics(m_font).height());

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
    return QFontMetrics(m_font).ascent();
}

int Font::descent() const
{
    return QFontMetrics(m_font).descent();
}

int Font::lineSpacing() const
{
    return QFontMetrics(m_font).lineSpacing();
}

float Font::xHeight() const
{
    return QFontMetrics(m_font).xHeight();
}

int Font::spaceWidth() const
{
    return QFontMetrics(m_font).width(QLatin1Char(' '));
}

Font::operator QFont() const
{
    return m_font;
}

}
