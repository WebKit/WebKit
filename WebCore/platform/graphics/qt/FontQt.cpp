/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008, 2010 Holger Hans Peter Freyther
    Copyright (C) 2009 Dirk Schulze <krit@webkit.org>

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

#include "AffineTransform.h"
#include "ContextShadow.h"
#include "FontDescription.h"
#include "FontFallbackList.h"
#include "FontSelector.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "Pattern.h"

#include <QBrush>
#include <QFontInfo>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTextLayout>
#include <qalgorithms.h>
#include <qdebug.h>

#include <limits.h>

namespace WebCore {

static const QString fromRawDataWithoutRef(const String& string, int start = 0, int len = -1)
{
    if (len < 0)
        len = string.length() - start;
    Q_ASSERT(start + len <= string.length());

    // We don't detach. This assumes the WebCore string data will stay valid for the
    // lifetime of the QString we pass back, since we don't ref the WebCore string.
    return QString::fromRawData(reinterpret_cast<const QChar*>(string.characters() + start), len);
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

static void drawTextCommon(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to, const QFont& font, bool isComplexText)
{
    if (to < 0)
        to = run.length();

    QPainter *p = ctx->platformContext();

    QPen textFillPen;
    if (ctx->textDrawingMode() & cTextFill) {
        if (ctx->fillGradient()) {
            QBrush brush(*ctx->fillGradient()->platformGradient());
            brush.setTransform(ctx->fillGradient()->gradientSpaceTransform());
            textFillPen = QPen(brush, 0);
        } else if (ctx->fillPattern()) {
            AffineTransform affine;
            textFillPen = QPen(QBrush(ctx->fillPattern()->createPlatformPattern(affine)), 0);
        } else
            textFillPen = QPen(QColor(ctx->fillColor()));
    }

    QPen textStrokePen;
    if (ctx->textDrawingMode() & cTextStroke) {
        if (ctx->strokeGradient()) {
            QBrush brush(*ctx->strokeGradient()->platformGradient());
            brush.setTransform(ctx->strokeGradient()->gradientSpaceTransform());
            textStrokePen = QPen(brush, ctx->strokeThickness());
        } else if (ctx->strokePattern()) {
            AffineTransform affine;
            QBrush brush(ctx->strokePattern()->createPlatformPattern(affine));
            textStrokePen = QPen(brush, ctx->strokeThickness());
        } else
            textStrokePen = QPen(QColor(ctx->strokeColor()), ctx->strokeThickness());
    }

    String sanitized = Font::normalizeSpaces(String(run.characters(), run.length()));
    QString string = fromRawDataWithoutRef(sanitized);
    QPointF pt(point.x(), point.y());

    if (from > 0 || to < run.length()) {
        if (isComplexText) {
            QTextLayout layout(string, font);
            QTextLine line = setupLayout(&layout, run);
            float x1 = line.cursorToX(from);
            float x2 = line.cursorToX(to);
            if (x2 < x1)
                qSwap(x1, x2);

            QFontMetrics fm(font);
            int ascent = fm.ascent();
            QRectF boundingRect(point.x() + x1, point.y() - ascent, x2 - x1, fm.height());
            QRectF clip = boundingRect;

            ContextShadow* ctxShadow = ctx->contextShadow();

            if (ctxShadow->m_type != ContextShadow::NoShadow) {
                qreal dx1 = 0, dx2 = 0, dy1 = 0, dy2 = 0;
                if (ctxShadow->offset().x() > 0)
                    dx2 = ctxShadow->offset().x();
                else
                    dx1 = -ctxShadow->offset().x();
                if (ctxShadow->offset().y() > 0)
                    dy2 = ctxShadow->offset().y();
                else
                    dy1 = -ctxShadow->offset().y();
                // expand the clip rect to include the text shadow as well
                clip.adjust(dx1, dx2, dy1, dy2);
                clip.adjust(-ctxShadow->m_blurRadius, -ctxShadow->m_blurRadius, ctxShadow->m_blurRadius, ctxShadow->m_blurRadius);
            }
            p->save();
            p->setClipRect(clip.toRect(), Qt::IntersectClip);
            pt.setY(pt.y() - ascent);

            if (ctxShadow->m_type != ContextShadow::NoShadow) {
                ContextShadow* ctxShadow = ctx->contextShadow();
                if (ctxShadow->m_type != ContextShadow::BlurShadow) {
                    p->save();
                    p->setPen(ctxShadow->m_color);
                    p->translate(ctxShadow->offset());
                    line.draw(p, pt);
                    p->restore();
                } else {
                    QPainter* shadowPainter = ctxShadow->beginShadowLayer(p, boundingRect);
                    if (shadowPainter) {
                        // Since it will be blurred anyway, we don't care about render hints.
                        shadowPainter->setFont(p->font());
                        shadowPainter->setPen(ctxShadow->m_color);
                        line.draw(shadowPainter, pt);
                        ctxShadow->endShadowLayer(p);
                    }
                }
            }
            p->setPen(textFillPen);
            line.draw(p, pt);
            p->restore();
            return;
        }
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
        int skipWidth = QFontMetrics(font).width(string, from, Qt::TextBypassShaping);
        pt.setX(pt.x() + skipWidth);
        string = fromRawDataWithoutRef(sanitized, from, to - from);
#endif
    }

    p->setFont(font);

    int flags = run.rtl() ? Qt::TextForceRightToLeft : Qt::TextForceLeftToRight;
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    // See QWebPagePrivate::QWebPagePrivate() where the default path is set to Complex for Qt 4.6 and earlier.
    if (!isComplexText && !(ctx->textDrawingMode() & cTextStroke))
        flags |= Qt::TextBypassShaping;
#endif
    if (ctx->contextShadow()->m_type != ContextShadow::NoShadow) {
        ContextShadow* ctxShadow = ctx->contextShadow();
        if (ctxShadow->m_type != ContextShadow::BlurShadow) {
            p->save();
            p->setPen(ctxShadow->m_color);
            p->translate(ctxShadow->offset());
            p->drawText(pt, string, flags, run.padding());
            p->restore();
        } else {
            QFontMetrics fm(font);
            QRectF boundingRect(point.x(), point.y() - fm.ascent(), fm.width(string), fm.height());
            QPainter* shadowPainter = ctxShadow->beginShadowLayer(p, boundingRect);
            if (shadowPainter) {
                // Since it will be blurred anyway, we don't care about render hints.
                shadowPainter->setFont(p->font());
                shadowPainter->setPen(ctxShadow->m_color);
                shadowPainter->drawText(pt, string, flags, run.padding());
                ctxShadow->endShadowLayer(p);
            }
        }
    }
    if (ctx->textDrawingMode() & cTextStroke) {
        QPainterPath path;
        path.addText(pt, font, string);
        p->setPen(textStrokePen);
        p->strokePath(path, p->pen());
    }
    if (ctx->textDrawingMode() & cTextFill) {
        p->setPen(textFillPen);
        p->drawText(pt, string, flags, run.padding());
    }
}

void Font::drawSimpleText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to) const
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    drawTextCommon(ctx, run, point, from, to, font(), /* isComplexText = */false);
#else
    Q_ASSERT(false);
#endif
}

void Font::drawComplexText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    drawTextCommon(ctx, run, point, from, to, font(), /* isComplexText = */true);
}

float Font::floatWidthForSimpleText(const TextRun& run, GlyphBuffer* glyphBuffer, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    if (!run.length())
        return 0;

    String sanitized = Font::normalizeSpaces(String(run.characters(), run.length()));
    QString string = fromRawDataWithoutRef(sanitized);

    int w = QFontMetrics(font()).width(string, -1, Qt::TextBypassShaping);

    // WebKit expects us to ignore word spacing on the first character (as opposed to what Qt does)
    if (treatAsSpace(run[0]))
        w -= m_wordSpacing;

    return w + run.padding();
#else
    Q_ASSERT(false);
    return 0.0f;
#endif
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>*, GlyphOverflow*) const
{
    if (!run.length())
        return 0;

    if (run.length() == 1 && treatAsSpace(run[0]))
        return QFontMetrics(font()).width(space) + run.padding();

    String sanitized = Font::normalizeSpaces(String(run.characters(), run.length()));
    QString string = fromRawDataWithoutRef(sanitized);

    int w = QFontMetrics(font()).width(string);
    // WebKit expects us to ignore word spacing on the first character (as opposed to what Qt does)
    if (treatAsSpace(run[0]))
        w -= m_wordSpacing;

    return w + run.padding();
}

int Font::offsetForPositionForSimpleText(const TextRun& run, float position, bool includePartialGlyphs) const
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    String sanitized = Font::normalizeSpaces(String(run.characters(), run.length()));
    QString string = fromRawDataWithoutRef(sanitized);

    QFontMetrics fm(font());
    float delta = position;
    int curPos = 0;
    do {
        float charWidth = fm.width(string[curPos]);
        delta -= charWidth;
        if (includePartialGlyphs) {
            if (delta + charWidth / 2 <= 0)
                break;
        } else {
            if (delta + charWidth <= 0)
                break;
        }
    } while (++curPos < string.size());

    return curPos;
#else
    Q_ASSERT(false);
    return 0;
#endif
}

int Font::offsetForPositionForComplexText(const TextRun& run, float position, bool) const
{
    String sanitized = Font::normalizeSpaces(String(run.characters(), run.length()));
    QString string = fromRawDataWithoutRef(sanitized);

    QTextLayout layout(string, font());
    QTextLine line = setupLayout(&layout, run);
    return line.xToCursor(position);
}

FloatRect Font::selectionRectForSimpleText(const TextRun& run, const FloatPoint& pt, int h, int from, int to) const
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    String sanitized = Font::normalizeSpaces(String(run.characters(), run.length()));
    QString wholeText = fromRawDataWithoutRef(sanitized);
    QString selectedText = fromRawDataWithoutRef(sanitized, from, to - from);

    int startX = QFontMetrics(font()).width(wholeText, from, Qt::TextBypassShaping);
    int width = QFontMetrics(font()).width(selectedText, -1, Qt::TextBypassShaping);

    return FloatRect(pt.x() + startX, pt.y(), width, h);
#else
    Q_ASSERT(false);
    return FloatRect();
#endif
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const FloatPoint& pt, int h, int from, int to) const
{
    String sanitized = Font::normalizeSpaces(String(run.characters(), run.length()));
    QString string = fromRawDataWithoutRef(sanitized);

    QTextLayout layout(string, font());
    QTextLine line = setupLayout(&layout, run);

    float x1 = line.cursorToX(from);
    float x2 = line.cursorToX(to);
    if (x2 < x1)
        qSwap(x1, x2);

    return FloatRect(pt.x() + x1, pt.y(), x2 - x1, h);
}

bool Font::canReturnFallbackFontsForComplexText()
{
    return false;
}

QFont Font::font() const
{
    QFont f = primaryFont()->getQtFont();
    if (m_letterSpacing != 0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, m_letterSpacing);
    if (m_wordSpacing != 0)
        f.setWordSpacing(m_wordSpacing);
    return f;
}

}

