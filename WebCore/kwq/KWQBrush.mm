/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <qbrush.h>

void QBrush::init(const QColor &color, BrushStyle style)
{
    data = new QBrushData();
    data->color = color;
    data->brushStyle = style;
}

QBrush::QBrush()
{
    init(Qt::black, SolidPattern);
}


QBrush::QBrush(const QColor &c)
{
    init(c, SolidPattern);
}

QBrush::QBrush(const QColor &c, BrushStyle style)
{
    init(c, style);
}

QBrush::QBrush(const QBrush &copyFrom)
{
    data = copyFrom.data;
    data->ref();
}

QBrush &QBrush::operator=(const QBrush &assignFrom)
{
    assignFrom.data->ref();
    if (data->deref()) {
        delete data;
    }
    data = assignFrom.data;
    return *this;
}

QBrush::~QBrush()
{
    if (data->deref()) {
        delete data;
    }
}

QBrush QBrush::copy() const
{
    return QBrush(data->color, data->brushStyle);
}

void QBrush::detach()
{
    if (data->count != 1) {
        *this = copy();
    }
}

const QColor &QBrush::color() const
{
    return data->color;
}

void QBrush::setColor(const QColor &c)
{
    detach();
    data->color = c;
}

Qt::BrushStyle QBrush::style() const
{
    return data->brushStyle;
}

void QBrush::setStyle(Qt::BrushStyle bs)
{
    detach();
    data->brushStyle = bs;
}

bool QBrush::operator==(const QBrush &compareTo) const
{
    return (compareTo.data == data) || 
        (compareTo.data->brushStyle == data->brushStyle && 
         compareTo.data->color == data->color);
}


bool QBrush::operator!=(const QBrush &compareTo) const
{
    return !(operator==( compareTo ));
}

