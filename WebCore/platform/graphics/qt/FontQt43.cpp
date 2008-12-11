/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
*/

#include "config.h"
#include "Font.h"
#include "FontDescription.h"
#include "FontFallbackList.h"
#include "FontSelector.h"

#include "GraphicsContext.h"
#include <QTextLayout>
#include <QPainter>
#include <QFontMetrics>
#include <QFontInfo>
#include <qalgorithms.h>
#include <qdebug.h>

#include <limits.h>

#if QT_VERSION < 0x040400

namespace WebCore {

struct TextRunComponent {
    TextRunComponent() : font(0) {}
    TextRunComponent(const UChar *start, int length, bool rtl, const QFont *font, int offset, bool sc = false);
    TextRunComponent(int spaces, bool rtl, const QFont *font, int offset);

    inline bool isSpace() const { return spaces != 0; }

    QString string;
    const QFont *font;
    int width;
    int offset;
    int spaces;
};

TextRunComponent::TextRunComponent(const UChar *start, int length, bool rtl, const QFont *f, int o, bool sc)
    : string(reinterpret_cast<const QChar*>(start), length)
    , font(f)
    , offset(o)
    , spaces(0)
{
    if (sc)
        string = string.toUpper();
    string.prepend(rtl ? QChar(0x202e) : QChar(0x202d));
    width = QFontMetrics(*font).width(string);
}

TextRunComponent::TextRunComponent(int s, bool rtl, const QFont *f, int o)
    : string(s, QLatin1Char(' '))
    , font(f)
    , offset(o)
    , spaces(s)
{
    string.prepend(rtl ? QChar(0x202e) : QChar(0x202d));
    width = spaces * QFontMetrics(*font).width(QLatin1Char(' '));
}


static int generateComponents(Vector<TextRunComponent, 1024>* components, const Font &font, const TextRun &run)
{
//     qDebug() << "generateComponents" << QString((const QChar *)run.characters(), run.length());
    int letterSpacing = font.letterSpacing();
    int wordSpacing = font.wordSpacing();
    bool smallCaps = font.fontDescription().smallCaps();
    int padding = run.padding();
    int numSpaces = 0;
    if (padding) {
        for (int i = 0; i < run.length(); i++)
            if (Font::treatAsSpace(run[i]))
                ++numSpaces;
    }

    int offset = 0;
    const QFont *f = &font.font();
    if (letterSpacing || smallCaps) {
        // need to draw every letter on it's own
        int start = 0;
        if (Font::treatAsSpace(run[0])) {
            int add = 0;
            if (numSpaces) {
                add = padding/numSpaces;
                padding -= add;
                --numSpaces;
            }
            components->append(TextRunComponent(1, run.rtl(), &font.font(), offset));
            offset += add + letterSpacing + components->last().width;
            start = 1;
//         qDebug() << "space at 0" << offset;
        } else if (smallCaps) {
            f = (QChar::category(run[0]) == QChar::Letter_Lowercase ? &font.scFont() : &font.font());
        }
        for (int i = 1; i < run.length(); ++i) {
            uint ch = run[i];
            if (QChar(ch).isHighSurrogate() && QChar(run[i-1]).isLowSurrogate())
                ch = QChar::surrogateToUcs4(ch, run[i-1]);
            if (QChar(ch).isLowSurrogate() || QChar::category(ch) == QChar::Mark_NonSpacing)
                continue;
            if (Font::treatAsSpace(run[i])) {
                int add = 0;
//                 qDebug() << "    treatAsSpace:" << i << start;
                if (i - start > 0) {
                    components->append(TextRunComponent(run.characters() + start, i - start,
                                                        run.rtl(),
                                                        f, offset, f == &font.scFont()));
                    offset += components->last().width + letterSpacing;
//                     qDebug() << "   appending(1) " << components->last().string << components->last().width;
                }
                if (numSpaces) {
                    add = padding/numSpaces;
                    padding -= add;
                    --numSpaces;
                }
                components->append(TextRunComponent(1, run.rtl(), &font.font(), offset));
                offset += wordSpacing + add + components->last().width + letterSpacing;
                start = i + 1;
                continue;
            } else if (!letterSpacing) {
//                 qDebug() << i << char(run[i]) << (QChar::category(ch) == QChar::Letter_Lowercase) <<
//                     QFontInfo(*f).pointSizeF();
                if (QChar::category(ch) == QChar::Letter_Lowercase) {
                    if (f == &font.scFont())
                        continue;
                } else {
                    if (f == &font.font())
                        continue;
                }
            }
            if (i - start > 0) {
                components->append(TextRunComponent(run.characters() + start, i - start,
                                                    run.rtl(),
                                                    f, offset, f == &font.scFont()));
                offset += components->last().width + letterSpacing;
//                 qDebug() << "   appending(2) " << components->last().string << components->last().width;
            }
            if (smallCaps)
                f = (QChar::category(ch) == QChar::Letter_Lowercase ? &font.scFont() : &font.font());
            start = i;
        }
        if (run.length() - start > 0) {
            components->append(TextRunComponent(run.characters() + start, run.length() - start,
                                                run.rtl(),
                                                f, offset, f == &font.scFont()));
            offset += components->last().width;
//             qDebug() << "   appending(3) " << components->last().string << components->last().width;
        }
        offset += letterSpacing;
    } else {
        int start = 0;
        for (int i = 0; i < run.length(); ++i) {
            if (Font::treatAsSpace(run[i])) {
                if (i - start > 0) {
                    components->append(TextRunComponent(run.characters() + start, i - start,
                                                        run.rtl(),
                                                        f, offset));
                    offset += components->last().width;
                }
                int add = 0;
                if (numSpaces) {
                    add = padding/numSpaces;
                    padding -= add;
                    --numSpaces;
                }
                components->append(TextRunComponent(1, run.rtl(), &font.font(), offset));
                offset += add + components->last().width;
                if (i)
                    offset += wordSpacing;
                start = i + 1;
            }
        }
        if (run.length() - start > 0) {
            components->append(TextRunComponent(run.characters() + start, run.length() - start,
                                                run.rtl(),
                                                f, offset));
            offset += components->last().width;
        }
    }
    return offset;
}

void Font::drawComplexText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    if (to < 0)
        to = run.length();

    QPainter *p = ctx->platformContext();
    Color color = ctx->fillColor();
    p->setPen(QColor(color));

    Vector<TextRunComponent, 1024> components;
    int w = generateComponents(&components, *this, run);

    if (from > 0 || to < run.length()) {
        FloatRect clip = selectionRectForComplexText(run,
                                              IntPoint(qRound(point.x()), qRound(point.y())),
                                              QFontMetrics(font()).height(), from, to);
        QRectF rect(clip.x(), clip.y() - ascent(), clip.width(), clip.height());
        p->save();
        p->setClipRect(rect.toRect());
    }

    if (run.rtl()) {
        for (int i = 0; i < components.size(); ++i) {
            if (!components.at(i).isSpace()) {
                p->setFont(*components.at(i).font);
                QPointF pt(point.x() + w - components.at(i).offset - components.at(i).width, point.y());
                p->drawText(pt, components.at(i).string);
            }
        }
    } else {
        for (int i = 0; i < components.size(); ++i) {
            if (!components.at(i).isSpace()) {
                p->setFont(*components.at(i).font);
                QPointF pt(point.x() + components.at(i).offset, point.y());
                p->drawText(pt, components.at(i).string);
            }
        }
    }
    if (from > 0 || to < run.length())
        p->restore();
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    Vector<TextRunComponent, 1024> components;
    int w = generateComponents(&components, *this, run);

    return w;
}

int Font::offsetForPositionForComplexText(const TextRun& run, int position, bool includePartialGlyphs) const
{
    Vector<TextRunComponent, 1024> components;
    int w = generateComponents(&components, *this, run);

    int offset = 0;
    if (run.rtl()) {
        for (int i = 0; i < components.size(); ++i) {
            int xe = w - components.at(i).offset;
            int xs = xe - components.at(i).width;
            if (position >= xs) {
                QTextLayout layout(components.at(i).string, *components.at(i).font);
                layout.beginLayout();
                QTextLine l = layout.createLine();
                if (!l.isValid())
                    return offset;

                l.setLineWidth(INT_MAX/256);
                layout.endLayout();

                if (position - xs >= l.width())
                    return offset;
                int cursor = l.xToCursor(position - xs);
                if (cursor > 1)
                    --cursor;
                return offset + cursor;
            } else {
                offset += components.at(i).string.length() - 1;
            }
        }
    } else {
        for (int i = 0; i < components.size(); ++i) {
            int xs = components.at(i).offset;
            int xe = xs + components.at(i).width;
            if (position <= xe) {
                QTextLayout layout(components.at(i).string, *components.at(i).font);
                layout.beginLayout();
                QTextLine l = layout.createLine();
                if (!l.isValid())
                    return offset;

                l.setLineWidth(INT_MAX/256);
                layout.endLayout();

                if (position - xs >= l.width())
                    return offset + components.at(i).string.length() - 1;
                int cursor = l.xToCursor(position - xs);
                if (cursor > 1)
                    --cursor;
                return offset + cursor;
            } else {
                offset += components.at(i).string.length() - 1;
            }
        }
    }
    return run.length();
}

static float cursorToX(const Vector<TextRunComponent, 1024>& components, int width, bool rtl, int cursor)
{
    int start = 0;
    for (int i = 0; i < components.size(); ++i) {
        if (start + components.at(i).string.length() - 1 < cursor) {
            start += components.at(i).string.length() - 1;
            continue;
        }
        int xs = components.at(i).offset;
        if (rtl)
            xs = width - xs - components.at(i).width;
        QTextLayout layout(components.at(i).string, *components.at(i).font);
        layout.beginLayout();
        QTextLine l = layout.createLine();
        if (!l.isValid())
            return 0;

        l.setLineWidth(INT_MAX/256);
        layout.endLayout();

        return xs + l.cursorToX(cursor - start + 1);
    }
    return width;
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const IntPoint& pt,
                                     int h, int from, int to) const
{
    Vector<TextRunComponent, 1024> components;
    int w = generateComponents(&components, *this, run);

    if (from == 0 && to == run.length())
        return FloatRect(pt.x(), pt.y(), w, h);

    float x1 = cursorToX(components, w, run.rtl(), from);
    float x2 = cursorToX(components, w, run.rtl(), to);
    if (x2 < x1)
        qSwap(x1, x2);

    return FloatRect(pt.x() + x1, pt.y(), x2 - x1, h);
}

int Font::lineGap() const
{
    return QFontMetrics(m_font).leading();
}

}

#endif
