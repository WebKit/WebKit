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

class QPenPrivate
{
friend class QPen;
public:

    QPenPrivate(const QColor &c, uint w, QPen::PenStyle ps) : 
        qcolor(c), width(w), penStyle(ps)
    {
    }

    ~QPenPrivate() {}

private:    
    QColor qcolor;
    uint width;
    QPen::PenStyle penStyle;
};


QPen::QPen()
{
    d = new QPenPrivate(Qt::black, 3, SolidLine);
}


QPen::QPen(const QColor &c, uint w, PenStyle ps)
{
    d = new QPenPrivate(c, w, ps);
}


QPen::QPen(const QPen &copyFrom)
{
    d->qcolor = copyFrom.d->qcolor;
}


QPen::~QPen()
{
    delete d;
}


const QColor &QPen::color() const
{
    return d->qcolor;
}

uint QPen::width() const
{
    return d->width;
}

QPen::PenStyle QPen::style() const
{
    return d->penStyle;
}

void QPen::setColor(const QColor &c)
{
    d->qcolor = c;
}

void QPen::setWidth(uint w)
{
    d->width = w;
}

void QPen::setStyle(PenStyle ps)
{
    d->penStyle = ps;
}

QPen &QPen::operator=(const QPen &assignFrom)
{
    d->qcolor = assignFrom.d->qcolor;
    d->width = assignFrom.d->width;
    d->penStyle = assignFrom.d->penStyle;
    return *this;
}

bool QPen::operator==(const QPen &compareTo) const
{
    return (d->width == compareTo.d->width) &&
        (d->penStyle == compareTo.d->penStyle) &&
        (d->qcolor == compareTo.d->qcolor);
}


bool QPen::operator!=(const QPen &compareTo) const
{
    return !(operator==( compareTo ));
}


