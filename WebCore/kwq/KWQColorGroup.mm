/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQPalette.h"

QColorGroup::QColorGroup()
{
    brushes[Foreground] = QColor(255,255,255);
    brushes[Shadow] = QColor(255,255,255);
    brushes[Light] = QColor(224,224,224);
    brushes[Midlight] = QColor(192,192,192);
    brushes[Mid] = QColor(128,128,128);
    brushes[Dark] = QColor(64,64,64);
    brushes[Base] = QColor(255,255,255);
    brushes[ButtonText] = QColor(0,0,0);
    brushes[Button] = QColor(192,192,192);
    brushes[Background] = QColor(255,255,255);
    brushes[Text] = QColor(0,0,0);
    brushes[Highlight] = QColor(64,64,64);
    brushes[HighlightedText] = QColor(0,0,0);
}

const QBrush &QColorGroup::brush(ColorRole cr) const
{
    return brushes[cr];
}

const QColor &QColorGroup::color(ColorRole cr) const
{
    return brushes[cr].color();
}

void QColorGroup::setColor(QColorGroup::ColorRole cr, const QColor &color)
{
    brushes[cr].setColor(color);
}

const QColor &QColorGroup::foreground() const
{
    return brushes[Foreground].color();
}

const QColor &QColorGroup::shadow() const
{
    return brushes[Shadow].color();
}

const QColor &QColorGroup::light() const
{
    return brushes[Light].color();
}

const QColor &QColorGroup::midlight() const
{
    return brushes[Midlight].color();
}

const QColor &QColorGroup::dark() const
{
    return brushes[Dark].color();
}

const QColor &QColorGroup::base() const
{
    return brushes[Base].color();
}

const QColor &QColorGroup::buttonText() const
{
    return brushes[ButtonText].color();
}

const QColor &QColorGroup::button() const
{
    return brushes[Button].color();
}

const QColor &QColorGroup::text() const
{
    return brushes[Text].color();
}

const QColor &QColorGroup::background() const
{
    return brushes[Background].color();
}

const QColor &QColorGroup::highlight() const
{
    return brushes[Highlight].color();
}

const QColor &QColorGroup::highlightedText() const
{
    return brushes[HighlightedText].color();
}

bool QColorGroup::operator==(const QColorGroup &other) const
{
    for (int i = 0; i < NColorRoles; i++) {
        if (brushes[i] != other.brushes[i]) {
	    return false;
	}
    }
    return true;
}
