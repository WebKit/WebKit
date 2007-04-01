/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
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

#include "FontData.h"
#include "GraphicsContext.h"

#include <QPainter>

namespace WebCore {

Font::operator QFont() const
{
    return primaryFont()->platformData().font();
}

void Font::drawGlyphs(GraphicsContext* graphicsContext, const FontData* font, const GlyphBuffer& glyphBuffer,
                      int from, int numGlyphs, const FloatPoint& point) const
{
    QPainter& p = *graphicsContext->platformContext();

    Color color = graphicsContext->fillColor();
    p.setPen(QColor(color));
    p.setFont(font->platformData().font());

    // TODO: rework this to make justified text work.  Qt doesn't provide a good
    // API to solve this problem yet.
    const QChar* buffer = reinterpret_cast<const QChar*>(glyphBuffer.glyphs(from));
    QString str = QString::fromRawData(buffer, numGlyphs);

    p.drawText(point, str);
}

void Font::drawComplexText(GraphicsContext* ctx, const TextRun& run, const TextStyle&, const FloatPoint& point, int from, int to) const
{
    // FIXME: style, run.from()/length() cut-off
    ctx->platformContext()->drawText(point, 
                                     QString::fromRawData(
                                         reinterpret_cast<const QChar*>(
                                             run.characters() + from), run.length()));
}

float Font::floatWidthForComplexText(const TextRun& run, const TextStyle&) const
{
    // FIXME: style
    QFontMetricsF metrics(primaryFont()->m_font.font());
    return metrics.width(QString::fromRawData(reinterpret_cast<const QChar*>(run.characters()), run.length()));
}

}

// vim: ts=4 sw=4 et
