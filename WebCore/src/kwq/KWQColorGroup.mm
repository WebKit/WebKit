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

#include <qpalette.h>
#include <qcolor.h>

#include <kwqdebug.h>

QColorGroup::QColorGroup()
{
    _logNotYetImplemented();
}


QColorGroup::QColorGroup(const QColorGroup &)
{
    _logNotYetImplemented();
}


QColorGroup::~QColorGroup()
{
    _logNotYetImplemented();
}


const QBrush &QColorGroup::brush(QColorGroup::ColorRole cr) const
{
    _logNotYetImplemented();
    // FIXME Gross leak
    return *(new QBrush (Qt::white));
}

const QColor &QColorGroup::color(QColorGroup::ColorRole cr) const
{
    _logNotYetImplemented();
    return Qt::white;
}


void setColor(QColorGroup::ColorRole cr, const QColor &)
{
    _logNotYetImplemented();
}


const QColor &QColorGroup::foreground() const
{
    _logNotYetImplemented();
    return Qt::white;
}


const QColor &QColorGroup::shadow() const
{
    _logNotYetImplemented();
    return Qt::white;
}


const QColor &QColorGroup::light() const
{
    _logNotYetImplemented();
    return Qt::white;
}


const QColor &QColorGroup::midlight() const
{
    _logNotYetImplemented();
    return Qt::white;
}


const QColor &QColorGroup::dark() const
{
    _logNotYetImplemented();
    return Qt::white;
}


const QColor &QColorGroup::base() const
{
    _logNotYetImplemented();
    return Qt::white;
}


const QColor &QColorGroup::buttonText() const
{
    _logNotYetImplemented();
    return Qt::white;
}


const QColor &QColorGroup::button() const
{
    _logNotYetImplemented();
    return Qt::white;
}


const QColor &QColorGroup::text() const
{
    _logNotYetImplemented();
    return Qt::black;
}


const QColor &QColorGroup::background() const
{
    _logNotYetImplemented();
    return Qt::white;
}


