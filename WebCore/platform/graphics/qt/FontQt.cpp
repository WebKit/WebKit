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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "config.h"
#include "Font.h"
#include "FontDescription.h"
#include "FontSelector.h"

#include "GraphicsContext.h"
#include <QTextLayout>
#include <QPainter>
#include <QFontMetrics>
#include <QFontInfo>
#include <qalgorithms.h>
#include <qdebug.h>

#include <limits.h>
namespace WebCore {

#if QT_VERSION >= 0x040400

Font::Font()
    : m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_font()
    , m_scFont()
{
    QFontMetrics metrics(m_font);
    m_spaceWidth = metrics.width(QLatin1Char(' '));
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
    bool smallCaps = description.smallCaps();
    m_font.setCapitalization(smallCaps ? QFont::SmallCaps : QFont::MixedCase);

    QFontMetrics metrics = QFontMetrics(m_font);
    m_spaceWidth = metrics.width(QLatin1Char(' '));

    if (wordSpacing)
        m_font.setWordSpacing(wordSpacing);
    if (letterSpacing)
        m_font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
}

void Font::setWordSpacing(short s)
{
    m_font.setWordSpacing(s);
    m_wordSpacing = s;
}
void Font::setLetterSpacing(short s)
{
    m_font.setLetterSpacing(QFont::AbsoluteSpacing, s);
    m_letterSpacing = s;
}


static QString qstring(const TextRun& run)
{
    QString string((QChar *)run.characters(), run.length());
    QChar *uc = string.data();
    for (int i = 0; i < string.length(); ++i) {
        if (Font::treatAsSpace(uc[i].unicode()))
            uc[i] = 0x20;
        else if (Font::treatAsZeroWidthSpace(uc[i].unicode()))
            uc[i] = 0x200c;
    }
    return string;
}


static QTextLine setupLayout(QTextLayout* layout, const TextRun& style)
{
    int flags = style.rtl() ? Qt::TextForceRightToLeft : Qt::TextForceLeftToRight;
    if (style.padding())
        flags |= Qt::TextJustificationForced;
    layout->setFlags(flags);
    layout->beginLayout();
    QTextLine line = layout->createLine();
    line.setLineWidth(INT_MAX/256);
    if (style.padding())
        line.setLineWidth(line.naturalTextWidth() + style.padding());
    layout->endLayout();
    return line;
}

void Font::drawText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    if (to < 0)
        to = run.length();
    QPainter *p = ctx->platformContext();
    Color color = ctx->fillColor();
    p->setPen(QColor(color));

    QString string = qstring(run);

    if (from > 0 || to < run.length()) {
        QTextLayout layout(string, m_font);
        QTextLine line = setupLayout(&layout, run);
        float x1 = line.cursorToX(from);
        float x2 = line.cursorToX(to);
        if (x2 < x1)
            qSwap(x1, x2);

        QFontMetrics fm(m_font);
        int ascent = fm.ascent();
        QRectF clip(point.x() + x1, point.y() - ascent, x2 - x1, fm.height());

        p->save();
        p->setClipRect(clip.toRect());
        QPointF pt(point.x(), point.y() - ascent);
        line.draw(p, pt);
        p->restore();
        return;
    }

    p->setFont(m_font);

    QPointF pt(point.x(), point.y());
    int flags = run.rtl() ? Qt::TextForceRightToLeft : Qt::TextForceLeftToRight;
    p->drawText(pt, string, flags, run.padding());
}

int Font::width(const TextRun& run) const
{
    if (!run.length())
        return 0;
    QString string = qstring(run);
    int w = QFontMetrics(m_font).width(string);
    // WebKit expects us to ignore word spacing on the first character (as opposed to what Qt does)
    if (treatAsSpace(run[0]))
        w -= m_wordSpacing;

    return w + run.padding();
}

float Font::floatWidth(const TextRun& run) const
{
    return width(run);
}

int Font::offsetForPosition(const TextRun& run, int position, bool /*includePartialGlyphs*/) const
{
    QString string = qstring(run);
    QTextLayout layout(string, m_font);
    QTextLine line = setupLayout(&layout, run);
    return line.xToCursor(position);
}

FloatRect Font::selectionRectForText(const TextRun& run, const IntPoint& pt, int h, int from, int to) const
{
    QString string = qstring(run);
    QTextLayout layout(string, m_font);
    QTextLine line = setupLayout(&layout, run);

    float x1 = line.cursorToX(from);
    float x2 = line.cursorToX(to);
    if (x2 < x1)
        qSwap(x1, x2);

    return FloatRect(pt.x() + x1, pt.y(), x2 - x1, h);
}

#else


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


Font::Font()
    : m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_font()
    , m_scFont()
{
    QFontMetrics metrics(m_font);
    m_spaceWidth = metrics.width(QLatin1Char(' '));
    qreal pointsize = m_font.pointSizeF();
    if (pointsize > 0)
        m_scFont.setPointSizeF(pointsize*0.7);
    else
        m_scFont.setPixelSize(qRound(m_font.pixelSize()*.7));
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
    QFontMetrics metrics = QFontMetrics(m_font);
    m_spaceWidth = metrics.width(QLatin1Char(' '));
    m_scFont = m_font;
    m_scFont.setPixelSize(qRound(description.computedSize()*.7));
}

void Font::setWordSpacing(short s)
{
    m_wordSpacing = s;
}
void Font::setLetterSpacing(short s)
{
    m_letterSpacing = s;
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

void Font::drawText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    if (to < 0)
        to = run.length();
    QPainter *p = ctx->platformContext();
    Color color = ctx->fillColor();
    p->setPen(QColor(color));

    Vector<TextRunComponent, 1024> components;
    int w = generateComponents(&components, *this, run);

    if (from > 0 || to < run.length()) {
        FloatRect clip = selectionRectForText(run,
                                              IntPoint(qRound(point.x()), qRound(point.y())),
                                              QFontMetrics(m_font).height(), from, to);
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

int Font::width(const TextRun& run) const
{
    Vector<TextRunComponent, 1024> components;
    int w = generateComponents(&components, *this, run);

//     qDebug() << "     width=" << w;
    return w;
}

float Font::floatWidth(const TextRun& run) const
{
    return width(run);
}

int Font::offsetForPosition(const TextRun& run, int position, bool includePartialGlyphs) const
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

FloatRect Font::selectionRectForText(const TextRun& run, const IntPoint& pt,
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
#endif


Font::~Font()
{
}

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_letterSpacing(other.m_letterSpacing)
    , m_wordSpacing(other.m_wordSpacing)
    , m_font(other.m_font)
    , m_scFont(other.m_scFont)
    , m_spaceWidth(other.m_spaceWidth)
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_font = other.m_font;
    m_scFont = other.m_scFont;
    m_spaceWidth = other.m_spaceWidth;
    return *this;
}

bool Font::operator==(const Font& other) const
{
    return m_fontDescription == other.m_fontDescription
        && m_letterSpacing == other.m_letterSpacing
        && m_wordSpacing == other.m_wordSpacing
        && m_font == other.m_font
        && m_scFont == other.m_scFont
        && m_spaceWidth == other.m_spaceWidth;
}

void Font::update(PassRefPtr<FontSelector>) const
{
    // don't think we need this
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

unsigned Font::unitsPerEm() const
{
    return 1; // FIXME!
}

int Font::spaceWidth() const
{
    return m_spaceWidth;
}

}
