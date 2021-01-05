/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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

#include "config.h"
#include "Cursor.h"

#include "NotImplemented.h"

namespace WebCore {

void Cursor::ensurePlatformCursor() const
{
    if (m_platformCursor)
        return;

    BCursorID which;
    switch(m_type)
    {
        case Cursor::Pointer:
            which = B_CURSOR_ID_SYSTEM_DEFAULT;
            break;
        case Cursor::Hand:
            which = B_CURSOR_ID_FOLLOW_LINK;
            break;
        case Cursor::Cross:
        case Cursor::Cell:
            which = B_CURSOR_ID_CROSS_HAIR;
            break;
        case Cursor::IBeam:
            which = B_CURSOR_ID_I_BEAM;
            break;
        case Cursor::Help:
            which = B_CURSOR_ID_HELP;
            break;
        case Cursor::EastResize:
            which = B_CURSOR_ID_RESIZE_EAST;
            break;
        case Cursor::NorthResize:
            which = B_CURSOR_ID_RESIZE_NORTH;
            break;
        case Cursor::NorthEastResize:
            which = B_CURSOR_ID_RESIZE_NORTH_EAST;
            break;
        case Cursor::NorthWestResize:
            which = B_CURSOR_ID_RESIZE_NORTH_WEST;
            break;
        case Cursor::SouthResize:
            which = B_CURSOR_ID_RESIZE_SOUTH;
            break;
        case Cursor::SouthEastResize:
            which = B_CURSOR_ID_RESIZE_SOUTH_EAST;
            break;
        case Cursor::SouthWestResize:
            which = B_CURSOR_ID_RESIZE_SOUTH_WEST;
            break;
        case Cursor::WestResize:
            which = B_CURSOR_ID_RESIZE_WEST;
            break;
        case Cursor::NorthSouthResize:
        case Cursor::RowResize:
            which = B_CURSOR_ID_RESIZE_NORTH_SOUTH;
            break;
        case Cursor::EastWestResize:
        case Cursor::ColumnResize:
            which = B_CURSOR_ID_RESIZE_EAST_WEST;
            break;
        case Cursor::NorthEastSouthWestResize:
            which = B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST;
            break;
        case Cursor::NorthWestSouthEastResize:
            which = B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST;
            break;
        case Cursor::MiddlePanning:
        case Cursor::EastPanning:
        case Cursor::NorthPanning:
        case Cursor::NorthEastPanning:
        case Cursor::NorthWestPanning:
        case Cursor::SouthPanning:
        case Cursor::SouthEastPanning:
        case Cursor::SouthWestPanning:
        case Cursor::WestPanning:
        case Cursor::Grabbing:
            which = B_CURSOR_ID_GRABBING;
            break;
        case Cursor::Move:
            which = B_CURSOR_ID_MOVE;
            break;
        case Cursor::VerticalText:
            which = B_CURSOR_ID_I_BEAM_HORIZONTAL;
            break;
        case Cursor::ContextMenu:
            which = B_CURSOR_ID_CONTEXT_MENU;
            break;
        case Cursor::Alias:
            which = B_CURSOR_ID_CREATE_LINK;
            break;
        case Cursor::Wait:
        case Cursor::Progress:
            which = B_CURSOR_ID_PROGRESS;
            break;
        case Cursor::NoDrop:
        case Cursor::NotAllowed:
            which = B_CURSOR_ID_NOT_ALLOWED;
            break;
        case Cursor::Copy:
            which = B_CURSOR_ID_COPY;
            break;
        case Cursor::None:
            which = B_CURSOR_ID_NO_CURSOR;
            break;
        case Cursor::ZoomIn:
            which = B_CURSOR_ID_ZOOM_IN;
            break;
        case Cursor::ZoomOut:
            which = B_CURSOR_ID_ZOOM_OUT;
            break;
        case Cursor::Grab:
            which = B_CURSOR_ID_GRAB;
            break;
        case Cursor::Custom:
            which = B_CURSOR_ID_SYSTEM_DEFAULT;
            notImplemented();
            // TODO create from bitmap.
            break;
    }

    m_platformCursor = new BCursor(which);
}

} // namespace WebCore

