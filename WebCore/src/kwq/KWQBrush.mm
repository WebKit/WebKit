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

QBrush::QBrush()
{
    qcolor = Qt::black;
    qbrushstyle = NoBrush;
}


QBrush::QBrush(const QColor &c)
{
    qcolor = c;
    qbrushstyle = SolidPattern;
}


QBrush::QBrush(const QBrush &copyFrom)
{
    qcolor = copyFrom.qcolor;
    qbrushstyle = copyFrom.qbrushstyle;
}


QBrush &QBrush::operator=(const QBrush &assignFrom)
{
    qcolor = assignFrom.qcolor;
    qbrushstyle = assignFrom.qbrushstyle;
    return *this;
}


QBrush::~QBrush()
{
}


bool QBrush::operator==(const QBrush &compareTo) const
{
    return qcolor == compareTo.qcolor;
}


bool QBrush::operator!=(const QBrush &compareTo) const
{
    return !(operator==( compareTo ));
}

