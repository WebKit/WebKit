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
#include <kwqdebug.h>

#include <qpalette.h>
#include <qcolor.h>

#define QCOLOR_GROUP_SIZE (QColorGroup::HighlightedText + 1)

class QColorGroupPrivate
{
friend class QColorGroup;
public:
    QColorGroupPrivate() {
        brushes = new QBrush[QCOLOR_GROUP_SIZE];    
        //brushes[QColorGroup::Foreground] = QColor(0,0,0);    
        //brushes[QColorGroup::Shadow] = QColor(0,0,0);    
        brushes[QColorGroup::Foreground] = QColor(255,255,255);    
        brushes[QColorGroup::Shadow] = QColor(255,255,255);    
        brushes[QColorGroup::Light] = QColor(224,224,224);    
        brushes[QColorGroup::Midlight] = QColor(192,192,192);    
        brushes[QColorGroup::Mid] = QColor(128,128,128);    
        brushes[QColorGroup::Dark] = QColor(64,64,64);    
        brushes[QColorGroup::Base] = QColor(255,255,255);    
        brushes[QColorGroup::ButtonText] = QColor(0,0,0);    
        brushes[QColorGroup::Button] = QColor(192,192,192);    
        brushes[QColorGroup::Background] = QColor(255,255,255);    
        brushes[QColorGroup::Text] = QColor(0,0,0);    
        brushes[QColorGroup::Highlight] = QColor(64,64,64);    
        brushes[QColorGroup::HighlightedText] = QColor(0,0,0);    
    }

    QColorGroupPrivate(const QColorGroupPrivate *other) {
        brushes = new QBrush[QCOLOR_GROUP_SIZE];    
        for (int i = 0; i < QCOLOR_GROUP_SIZE; i++) {
            brushes[i] = other->brushes[i];    
        }
    }

    ~QColorGroupPrivate() {
        delete [] brushes;
    }

private:
    QBrush *brushes;  
};

QColorGroup::QColorGroup()
{
    d = new QColorGroupPrivate();
}


QColorGroup::QColorGroup(const QColorGroup &other)
{
    d = new QColorGroupPrivate(other.d);
}


QColorGroup::~QColorGroup()
{
    delete d;
}


const QBrush &QColorGroup::brush(QColorGroup::ColorRole cr) const
{
    return d->brushes[cr];
}

const QColor &QColorGroup::color(QColorGroup::ColorRole cr) const
{
    return d->brushes[cr].color();
}


void QColorGroup::setColor(QColorGroup::ColorRole cr, const QColor &color)
{
    d->brushes[cr].setColor(color);
}


const QColor &QColorGroup::foreground() const
{
    return d->brushes[Foreground].color();
}


const QColor &QColorGroup::shadow() const
{
    return d->brushes[Shadow].color();
}


const QColor &QColorGroup::light() const
{
    return d->brushes[Light].color();
}


const QColor &QColorGroup::midlight() const
{
    return d->brushes[Midlight].color();
}


const QColor &QColorGroup::dark() const
{
    return d->brushes[Dark].color();
}


const QColor &QColorGroup::base() const
{
    return d->brushes[Base].color();
}


const QColor &QColorGroup::buttonText() const
{
    return d->brushes[ButtonText].color();
}


const QColor &QColorGroup::button() const
{
    return d->brushes[Button].color();
}


const QColor &QColorGroup::text() const
{
    return d->brushes[Text].color();
}


const QColor &QColorGroup::background() const
{
    return d->brushes[Background].color();
}

const QColor &QColorGroup::highlight() const
{
    return d->brushes[Highlight].color();
}


const QColor &QColorGroup::highlightedText() const
{
    return d->brushes[HighlightedText].color();
}


QColorGroup &QColorGroup::operator=(const QColorGroup &other)
{
    for (int i = 0; i < QCOLOR_GROUP_SIZE; i++) {
        d->brushes[i] = other.d->brushes[i];    
    }
    return *this;
}


bool QColorGroup::operator==(const QColorGroup &other)
{
    for (int i = 0; i < QCOLOR_GROUP_SIZE; i++) {
        if (d->brushes[i] != other.d->brushes[i]) {
	    return false;
	}
    }
    return true;
}
