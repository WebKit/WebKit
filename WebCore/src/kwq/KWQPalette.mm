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


class QPalettePrivate
{
friend class QPalette;
public:
    QPalettePrivate() : active(), inactive(), disabled() 
    {
    }

    QPalettePrivate(const QPalettePrivate *other) :
        active(other->active),
        inactive(other->inactive),
        disabled(other->disabled)
    {
    }

private:
    QColorGroup active;  
    QColorGroup inactive;  
    QColorGroup disabled;  
};

#include <kwqdebug.h>

QPalette::QPalette()
{
    d = new QPalettePrivate(); 
}


QPalette::QPalette(const QPalette &other)
{
    d = new QPalettePrivate(other.d); 
}


QPalette::~QPalette()
{
    delete d;
}


void QPalette::setColor(ColorGroup cg, QColorGroup::ColorRole role, const QColor &color)
{
    switch (cg) {
        case Active:
            d->active.setColor(role, color);
            break;
        case Inactive:
            d->inactive.setColor(role, color);
            break;
        case Disabled:
            d->disabled.setColor(role, color);
            break;
    }
}


const QColorGroup &QPalette::active() const
{
    return d->active;
}


const QColorGroup &QPalette::inactive() const
{
    return d->inactive;
}


const QColorGroup &QPalette::disabled() const
{
    return d->disabled;
}


const QColorGroup &QPalette::normal() const
{
    return d->active;
}


QPalette &QPalette::operator=(const QPalette &other)
{
    d->active = other.d->active;
    d->inactive = other.d->inactive;
    d->disabled = other.d->disabled;
    return *this;
}
