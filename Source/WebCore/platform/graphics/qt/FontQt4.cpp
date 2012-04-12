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
#include "FontDescription.h"
#include "FontFallbackList.h"
#include "FontSelector.h"
#include "GlyphBuffer.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "Pattern.h"
#include "ShadowBlur.h"
#include "TextRun.h"

#include <QBrush>
#include <QFontInfo>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTextLayout>
#include <limits.h>
#include <qalgorithms.h>
#include <qdebug.h>


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
    if (style.expansion())
        flags |= Qt::TextJustificationForced;
    layout->setFlags(flags);
    layout->beginLayout();
    QTextLine line = layout->createLine();
    line.setLineWidth(INT_MAX / 256);
    if (style.expansion())
        line.setLineWidth(line.naturalTextWidth() + style.expansion());
    layout->endLayout();
    return line;
}

static QPen fillPenForContext(GraphicsContext* ctx)
{
    if (ctx->fillGradient()) {
        QBrush brush(*ctx->fillGradient()->platformGradient());
        brush.setTransform(ctx->fillGradient()->gradientSpaceTransform());
        return QPen(brush, 0);
    }

    if (ctx->fillPattern()) {
        AffineTransform affine;
        return QPen(QBrush(ctx->fillPattern()->createPlatformPattern(affine)), 0);
    }

    return QPen(QColor(ctx->fillColor()));
}

static QPen strokePenForContext(GraphicsContext* ctx)
{
    if (ctx->strokeGradient()) {
        QBrush brush(*ctx->strokeGradient()->platformGradient());
        brush.setTransform(ctx->strokeGradient()->gradientSpaceTransform());
        return QPen(brush, ctx->strokeThickness());
    }

    if (ctx->strokePattern()) {
        AffineTransform affine;
        QBrush brush(ctx->strokePattern()->createPlatformPattern(affine));
        return QPen(brush, ctx->strokeThickness());
    }

    return QPen(QColor(ctx->strokeColor()), ctx->strokeThickness());
}

static void drawTextCommon(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to, const QFont& font, bool isComplexText)
{
    if (to < 0)
        to = run.length();

    QPainter* p = ctx->platformContext();

    QPen textFillPen;
    if (ctx->textDrawingMode() & TextModeFill)
        textFillPen = fillPenForContext(ctx);

    QPen textStrokePen;
    if (ctx->textDrawingMode() & TextModeStroke)
        textStrokePen = strokePenForContext(ctx);

    String sanitized = Font::normalizeSpaces(run.characters(), run.length());
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

            ShadowBlur* ctxShadow = ctx->shadowBlur();
            if (ctxShadow->type() != ShadowBlur::NoShadow) {
                const QPointF shadowOffset(ctx->state().shadowOffset.width(), ctx->state().shadowOffset.height());
                qreal dx1 = 0, dx2 = 0, dy1 = 0, dy2 = 0;
                if (shadowOffset.x() > 0)
                    dx2 = shadowOffset.x();
                else
                    dx1 = -shadowOffset.x();
                if (shadowOffset.y() > 0)
                    dy2 = shadowOffset.y();
                else
                    dy1 = -shadowOffset.y();
                // expand the clip rect to include the text shadow as well
                const float blurDistance = ctx->state().shadowBlur;
                clip.adjust(dx1, dx2, dy1, dy2);
                clip.adjust(-blurDistance, -blurDistance, blurDistance, blurDistance);
            }
            p->save();
            p->setClipRect(clip.toRect(), Qt::IntersectClip);
            pt.setY(pt.y() - ascent);

            if (ctxShadow->type() != ShadowBlur::NoShadow) {
                ShadowBlur* ctxShadow = ctx->shadowBlur();
                if (ctxShadow->type() != ShadowBlur::BlurShadow
                    && (!ctxShadow->shadowsIgnoreTransforms() || ctx->getCTM().isIdentity())) {
                    p->save();
                    p->setPen(ctx->state().shadowColor);
                    p->translate(QPointF(ctx->state().shadowOffset.width(), ctx->state().shadowOffset.height()));
                    line.draw(p, pt);
                    p->restore();
                } else {
                    GraphicsContext* shadowContext = ctxShadow->beginShadowLayer(ctx, boundingRect);
                    if (shadowContext) {
                        QPainter* shadowPainter = shadowContext->platformContext();
                        // Since it will be blurred anyway, we don't care about render hints.
                        shadowPainter->setFont(p->font());
                        shadowPainter->setPen(ctx->state().shadowColor);
                        line.draw(shadowPainter, pt);
                        ctxShadow->endShadowLayer(ctx);
                    }
                }
            }
            p->setPen(textFillPen);
            line.draw(p, pt);
            p->restore();
            return;
        }
        int skipWidth = QFontMetrics(font).width(string, from, Qt::TextBypassShaping);
        pt.setX(pt.x() + skipWidth);
        string = fromRawDataWithoutRef(sanitized, from, to - from);
    }

    p->setFont(font);

    int flags = run.rtl() ? Qt::TextForceRightToLeft : Qt::TextForceLeftToRight;
    if (!isComplexText && !(ctx->textDrawingMode() & TextModeStroke))
        flags |= Qt::TextBypassShaping;

    QPainterPath textStrokePath;
    if (ctx->textDrawingMode() & TextModeStroke)
        textStrokePath.addText(pt, font, string);

    ShadowBlur* ctxShadow = ctx->shadowBlur();
    if (ctx->hasShadow() && ctxShadow->type() != ShadowBlur::NoShadow) {
        if (ctx->textDrawingMode() & TextModeFill) {
            if (ctxShadow->type() != ShadowBlur::BlurShadow) {
                p->save();
                p->setPen(ctx->state().shadowColor);
                p->translate(QPointF(ctx->state().shadowOffset.width(), ctx->state().shadowOffset.height()));
                p->drawText(pt, string, flags, run.expansion());
                p->restore();
            } else {
                QFontMetrics fm(font);
                QRectF boundingRect(pt.x(), point.y() - fm.ascent(), fm.width(string, -1, flags), fm.height());
                GraphicsContext* shadowContext = ctxShadow->beginShadowLayer(ctx, boundingRect);
                if (shadowContext) {
                    QPainter* shadowPainter = shadowContext->platformContext();
                    // Since it will be blurred anyway, we don't care about render hints.
                    shadowPainter->setFont(p->font());
                    shadowPainter->setPen(ctx->state().shadowColor);
                    shadowPainter->drawText(pt, string, flags, run.expansion());
                    ctxShadow->endShadowLayer(ctx);
                }
            }
        } else if (ctx->textDrawingMode() & TextModeStroke) {
            if (ctxShadow->type() != ShadowBlur::BlurShadow) {
                const QPointF shadowOffset(ctx->state().shadowOffset.width(), ctx->state().shadowOffset.height());
                p->translate(shadowOffset);
                p->strokePath(textStrokePath, QPen(ctx->state().shadowColor));
                p->translate(-shadowOffset);
            } else {
                QFontMetrics fm(font);
                QRectF boundingRect(pt.x(), point.y() - fm.ascent(), fm.width(string, -1, flags), fm.height());
                GraphicsContext* shadowContext = ctxShadow->beginShadowLayer(ctx, boundingRect);
                if (shadowContext) {
                    QPainter* shadowPainter = shadowContext->platformContext();
                    // Since it will be blurred anyway, we don't care about render hints.
                    shadowPainter->setFont(p->font());
                    shadowPainter->strokePath(textStrokePath, QPen(ctx->state().shadowColor));
                    ctxShadow->endShadowLayer(ctx);
                }
            }
        }
    }

    if (ctx->textDrawingMode() & TextModeStroke)
        p->strokePath(textStrokePath, textStrokePen);

    if (ctx->textDrawingMode() & TextModeFill) {
        QPen previousPen = p->pen();
        p->setPen(textFillPen);
        p->drawText(pt, string, flags, run.expansion());
        p->setPen(previousPen);
    }
}

void Font::drawComplexText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    drawTextCommon(ctx, run, point, from, to, font(), /* isComplexText = */true);
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>*, GlyphOverflow*) const
{
    if (!primaryFont()->platformData().size())
        return 0;

    if (!run.length())
        return 0;

    if (run.length() == 1 && treatAsSpace(run[0]))
        return QFontMetrics(font()).width(space) + run.expansion();

    String sanitized = Font::normalizeSpaces(run.characters(), run.length());
    QString string = fromRawDataWithoutRef(sanitized);

    int w = QFontMetrics(font()).width(string);
    // WebKit expects us to ignore word spacing on the first character (as opposed to what Qt does)
    if (treatAsSpace(run[0]))
        w -= m_wordSpacing;

    return w + run.expansion();
}

int Font::offsetForPositionForComplexText(const TextRun& run, float position, bool) const
{
    String sanitized = Font::normalizeSpaces(run.characters(), run.length());
    QString string = fromRawDataWithoutRef(sanitized);

    QTextLayout layout(string, font());
    QTextLine line = setupLayout(&layout, run);
    return line.xToCursor(position);
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const FloatPoint& pt, int h, int from, int to) const
{
    String sanitized = Font::normalizeSpaces(run.characters(), run.length());
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

void Font::drawEmphasisMarksForComplexText(GraphicsContext* /* context */, const TextRun& /* run */, const AtomicString& /* mark */, const FloatPoint& /* point */, int /* from */, int /* to */) const
{
    notImplemented();
}

void Font::drawSimpleText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    drawTextCommon(ctx, run, point, from, to, font(), /* isComplexText = */false);
}

int Font::offsetForPositionForSimpleText(const TextRun& run, float position, bool includePartialGlyphs) const
{
    String sanitized = Font::normalizeSpaces(run.characters(), run.length());
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
}


float Font::floatWidthForSimpleText(const TextRun& run, GlyphBuffer* glyphBuffer, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    if (!primaryFont()->platformData().size())
        return 0;

    if (!run.length())
        return 0;

    String sanitized = Font::normalizeSpaces(run.characters(), run.length());
    QString string = fromRawDataWithoutRef(sanitized);

    int w = QFontMetrics(font()).width(string, -1, Qt::TextBypassShaping);

    // WebKit expects us to ignore word spacing on the first character (as opposed to what Qt does)
    if (treatAsSpace(run[0]))
        w -= m_wordSpacing;

    return w + run.expansion();
}


FloatRect Font::selectionRectForSimpleText(const TextRun& run, const FloatPoint& pt, int h, int from, int to) const
{
    String sanitized = Font::normalizeSpaces(run.characters(), run.length());
    QString wholeText = fromRawDataWithoutRef(sanitized);
    QString selectedText = fromRawDataWithoutRef(sanitized, from, qMin(to - from, wholeText.length() - from));

    int startX = QFontMetrics(font()).width(wholeText, from, Qt::TextBypassShaping);
    int width = QFontMetrics(font()).width(selectedText, -1, Qt::TextBypassShaping);

    return FloatRect(pt.x() + startX, pt.y(), width, h);
}

bool Font::canExpandAroundIdeographsInComplexText()
{
    return false;
}

bool Font::primaryFontHasGlyphForCharacter(UChar32) const
{
    notImplemented();
    return true;
}

int Font::emphasisMarkAscent(const AtomicString&) const
{
    notImplemented();
    return 0;
}

int Font::emphasisMarkDescent(const AtomicString&) const
{
    notImplemented();
    return 0;
}

int Font::emphasisMarkHeight(const AtomicString&) const
{
    notImplemented();
    return 0;
}

void Font::drawEmphasisMarksForSimpleText(GraphicsContext* /* context */, const TextRun& /* run */, const AtomicString& /* mark */, const FloatPoint& /* point */, int /* from */, int /* to */) const
{
    notImplemented();
}

QFont Font::font() const
{
    QFont f = primaryFont()->getQtFont();
    if (m_letterSpacing)
        f.setLetterSpacing(QFont::AbsoluteSpacing, m_letterSpacing);
    if (m_wordSpacing)
        f.setWordSpacing(m_wordSpacing);
    return f;
}

QFont Font::syntheticFont() const
{
    return font();
}

}

