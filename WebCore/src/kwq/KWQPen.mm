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

#include <qpen.h>

void QPen::init(const QColor &color, uint width, uint linestyle)
{
    data = new QPenData();
    data->style = (PenStyle)(linestyle & MPenStyle);
    data->width = width;
    data->color = color;
    data->linest = linestyle;
}

QPen::QPen()
{
    init(Qt::black, 1, SolidLine);
}


QPen::QPen(const QColor &color, uint width, PenStyle style)
{
    init(color, width, style);
}


QPen::QPen(const QPen &copyFrom)
{
    data = copyFrom.data;
    data->ref();
}


QPen::~QPen()
{
    if (data->deref()) {
        delete data;
    }
}

QPen QPen::copy() const
{
    QPen p(data->color, data->width, data->style);
    return p;
}

void QPen::detach()
{
    if (data->count != 1) {
        *this = copy();
    }
}

const QColor &QPen::color() const
{
    return data->color;
}

uint QPen::width() const
{
    return data->width;
}

QPen::PenStyle QPen::style() const
{
    return data->style;
}

void QPen::setColor(const QColor &color)
{
    detach();
    data->color = color;
}

void QPen::setWidth(uint width)
{
    if (data->width == width) {
        return;
    }
    detach();
    data->width = width;
}

void QPen::setStyle(PenStyle style)
{
    if (data->style == style) {
        return;
    }
    detach();
    data->style = style;
    data->linest = (data->linest & ~MPenStyle) | style;
}

QPen &QPen::operator=(const QPen &assignFrom)
{
    assignFrom.data->ref();
    if (data->deref()) {
        delete data;
    }
    data = assignFrom.data;
    return *this;
}

bool QPen::operator==(const QPen &compareTo) const
{
    return (data->width == compareTo.data->width) &&
        (data->style == compareTo.data->style) &&
        (data->color == compareTo.data->color);
}


bool QPen::operator!=(const QPen &compareTo) const
{
    return !(operator==( compareTo ));
}


