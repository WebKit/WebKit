/*
    Copyright (C) 2007 Trolltech ASA

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "PlatformScrollBar.h"
#include "ScrollBar.h"

#include <QScrollBar>
#include <QDebug>

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {    

Scrollbar::Scrollbar(ScrollbarClient*, ScrollbarOrientation, ScrollbarControlSize)
{
}
    
void Scrollbar::setSteps(int, int)
{
    notImplemented();
}

bool Scrollbar::scroll(ScrollDirection, ScrollGranularity, float)
{
    notImplemented();
    return 0;
}

bool Scrollbar::setValue(int)
{
    notImplemented();
    return 0;
}

void Scrollbar::setProportion(int, int)
{
    notImplemented();
}

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation,
                                     ScrollbarControlSize controlSize)
    : Scrollbar(client, orientation, controlSize)
{
    QScrollBar *bar = 0;
    if (orientation == HorizontalScrollbar)
        bar = new QScrollBar(Qt::Horizontal);
    else
        bar = new QScrollBar(Qt::Vertical);
    
    setQWidget(bar);
}

PlatformScrollbar::~PlatformScrollbar()
{
}

int PlatformScrollbar::width() const
{
    return Widget::width();
}

int PlatformScrollbar::height() const
{
    return Widget::height();
}

void PlatformScrollbar::setEnabled(bool e)
{
    Widget::setEnabled(e);
}

void PlatformScrollbar::paint(GraphicsContext* ctxt, const IntRect& damageRect)
{
    //Widget::paint(ctxt, damageRect);
}

void PlatformScrollbar::updateThumbPosition()
{
    notImplemented();
}

void PlatformScrollbar::updateThumbProportion()
{
    notImplemented();
}

void PlatformScrollbar::setRect(const IntRect&)
{
    notImplemented();
}


}
