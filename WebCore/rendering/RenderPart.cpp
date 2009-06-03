/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderPart.h"

#include "Frame.h"
#include "FrameView.h"

namespace WebCore {

RenderPart::RenderPart(Element* node)
    : RenderWidget(node)
{
    // init RenderObject attributes
    setInline(false);
}

RenderPart::~RenderPart()
{
    clearWidget();
}

void RenderPart::setWidget(Widget* widget)
{
    if (widget == this->widget())
        return;

    if (widget && widget->isFrameView())
        static_cast<FrameView*>(widget)->ref();
    RenderWidget::setWidget(widget);

    // make sure the scrollbars are set correctly for restore
    // ### find better fix
    viewCleared();
}

void RenderPart::viewCleared()
{
}

void RenderPart::deleteWidget(Widget* widget)
{
    // Since deref ends up calling setWidget back on us, need to make sure
    // that widget is already 0 so it won't do any work.
    ASSERT(!this->widget());

    if (widget && widget->isFrameView())
        static_cast<FrameView*>(widget)->deref();
    else
        delete widget;
}

}
