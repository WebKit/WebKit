/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#include "render_br.h"

using namespace khtml;


RenderBR::RenderBR(DOM::NodeImpl* node)
    : RenderText(node, new DOM::DOMStringImpl(QChar('\n')))
{
}

RenderBR::~RenderBR()
{
}

void RenderBR::cursorPos(int /*offset*/, int &_x, int &_y, int &height)
{
    if (previousSibling() && !previousSibling()->isBR() && !previousSibling()->isFloating()) {
        int offset = 0;
        if (previousSibling()->isText())
            offset = static_cast<RenderText*>(previousSibling())->length();

        previousSibling()->cursorPos(offset,_x,_y,height);
        return;
    }

    int absx, absy;
    absolutePosition(absx,absy);
    if (absx == -1) {
        // we don't know out absoluate position, and there is not point returning
        // just a relative one
        _x = _y = -1;
    }
    else {
        _x += absx;
        _y += absy;
    }
    height = RenderText::verticalPositionHint( false );

}

