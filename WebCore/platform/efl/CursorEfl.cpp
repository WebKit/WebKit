/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Charles Samuels <charles@kde.org>
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2008 Kenneth Rohde Christiansen
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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

#include <Edje.h>
#include <Evas.h>
#include <stdio.h>
#include <wtf/Assertions.h>

namespace WebCore {

Cursor::Cursor(PlatformCursor p)
{
    m_impl = eina_stringshare_add(p);
}

Cursor::Cursor(const Cursor& other)
{
    m_impl = eina_stringshare_ref(other.m_impl);
}

Cursor::~Cursor()
{
    if (m_impl) {
        eina_stringshare_del(m_impl);
        m_impl = 0;
    }
}

Cursor::Cursor(Image* image, const IntPoint& hotspot)
    : m_impl(0)
{
    notImplemented();
}

Cursor& Cursor::operator=(const Cursor& other)
{
    eina_stringshare_ref(other.m_impl);
    eina_stringshare_del(m_impl);
    m_impl = other.m_impl;
    return *this;
}

namespace {

class Cursors {
protected:
    Cursors()
        : PointerCursor("cursor/pointer")
        , MoveCursor("cursor/move")
        , CrossCursor("cursor/cross")
        , HandCursor("cursor/hand")
        , IBeamCursor("cursor/i_beam")
        , WaitCursor("cursor/wait")
        , HelpCursor("cursor/help")
        , EastResizeCursor("cursor/east_resize")
        , NorthResizeCursor("cursor/north_resize")
        , NorthEastResizeCursor("cursor/north_east_resize")
        , NorthWestResizeCursor("cursor/north_west_resize")
        , SouthResizeCursor("cursor/south_resize")
        , SouthEastResizeCursor("cursor/south_east_resize")
        , SouthWestResizeCursor("cursor/south_west_resize")
        , WestResizeCursor("cursor/west_resize")
        , NorthSouthResizeCursor("cursor/north_south_resize")
        , EastWestResizeCursor("cursor/east_west_resize")
        , NorthEastSouthWestResizeCursor("cursor/north_east_south_west_resize")
        , NorthWestSouthEastResizeCursor("cursor/north_west_south_east_resize")
        , ColumnResizeCursor("cursor/column_resize")
        , RowResizeCursor("cursor/row_resize")
        , MiddlePanningCursor("cursor/middle_panning")
        , EastPanningCursor("cursor/east_panning")
        , NorthPanningCursor("cursor/north_panning")
        , NorthEastPanningCursor("cursor/north_east_panning")
        , NorthWestPanningCursor("cursor/north_west_panning")
        , SouthPanningCursor("cursor/south_panning")
        , SouthEastPanningCursor("cursor/south_east_panning")
        , SouthWestPanningCursor("cursor/south_west_panning")
        , WestPanningCursor("cursor/west_panning")
        , VerticalTextCursor("cursor/vertical_text")
        , CellCursor("cursor/cell")
        , ContextMenuCursor("cursor/context_menu")
        , NoDropCursor("cursor/no_drop")
        , CopyCursor("cursor/copy")
        , ProgressCursor("cursor/progress")
        , AliasCursor("cursor/alias")
        , NoneCursor("cursor/none")
        , NotAllowedCursor("cursor/not_allowed")
        , ZoomInCursor("cursor/zoom_in")
        , ZoomOutCursor("cursor/zoom_out")
        , GrabCursor("cursor/grab")
        , GrabbingCursor("cursor/grabbing")
    {
    }

    ~Cursors()
    {
    }

public:
    static Cursors* self();
    static Cursors* s_self;

    Cursor PointerCursor;
    Cursor MoveCursor;
    Cursor CrossCursor;
    Cursor HandCursor;
    Cursor IBeamCursor;
    Cursor WaitCursor;
    Cursor HelpCursor;
    Cursor EastResizeCursor;
    Cursor NorthResizeCursor;
    Cursor NorthEastResizeCursor;
    Cursor NorthWestResizeCursor;
    Cursor SouthResizeCursor;
    Cursor SouthEastResizeCursor;
    Cursor SouthWestResizeCursor;
    Cursor WestResizeCursor;
    Cursor NorthSouthResizeCursor;
    Cursor EastWestResizeCursor;
    Cursor NorthEastSouthWestResizeCursor;
    Cursor NorthWestSouthEastResizeCursor;
    Cursor ColumnResizeCursor;
    Cursor RowResizeCursor;
    Cursor MiddlePanningCursor;
    Cursor EastPanningCursor;
    Cursor NorthPanningCursor;
    Cursor NorthEastPanningCursor;
    Cursor NorthWestPanningCursor;
    Cursor SouthPanningCursor;
    Cursor SouthEastPanningCursor;
    Cursor SouthWestPanningCursor;
    Cursor WestPanningCursor;
    Cursor VerticalTextCursor;
    Cursor CellCursor;
    Cursor ContextMenuCursor;
    Cursor NoDropCursor;
    Cursor CopyCursor;
    Cursor ProgressCursor;
    Cursor AliasCursor;
    Cursor NoneCursor;
    Cursor NotAllowedCursor;
    Cursor ZoomInCursor;
    Cursor ZoomOutCursor;
    Cursor GrabCursor;
    Cursor GrabbingCursor;
};

Cursors* Cursors::s_self = 0;

Cursors* Cursors::self()
{
    if (!s_self)
        s_self = new Cursors();

    return s_self;
}

}

const Cursor& pointerCursor()
{
    return Cursors::self()->PointerCursor;
}

const Cursor& moveCursor()
{
    return Cursors::self()->MoveCursor;
}

const Cursor& crossCursor()
{
    return Cursors::self()->CrossCursor;
}

const Cursor& handCursor()
{
    return Cursors::self()->HandCursor;
}

const Cursor& iBeamCursor()
{
    return Cursors::self()->IBeamCursor;
}

const Cursor& waitCursor()
{
    return Cursors::self()->WaitCursor;
}

const Cursor& helpCursor()
{
    return Cursors::self()->HelpCursor;
}

const Cursor& eastResizeCursor()
{
    return Cursors::self()->EastResizeCursor;
}

const Cursor& northResizeCursor()
{
    return Cursors::self()->NorthResizeCursor;
}

const Cursor& northEastResizeCursor()
{
    return Cursors::self()->NorthEastResizeCursor;
}

const Cursor& northWestResizeCursor()
{
    return Cursors::self()->NorthWestResizeCursor;
}

const Cursor& southResizeCursor()
{
    return Cursors::self()->SouthResizeCursor;
}

const Cursor& southEastResizeCursor()
{
    return Cursors::self()->SouthEastResizeCursor;
}

const Cursor& southWestResizeCursor()
{
    return Cursors::self()->SouthWestResizeCursor;
}

const Cursor& westResizeCursor()
{
    return Cursors::self()->WestResizeCursor;
}

const Cursor& northSouthResizeCursor()
{
    return Cursors::self()->NorthSouthResizeCursor;
}

const Cursor& eastWestResizeCursor()
{
    return Cursors::self()->EastWestResizeCursor;
}

const Cursor& northEastSouthWestResizeCursor()
{
    return Cursors::self()->NorthEastSouthWestResizeCursor;
}

const Cursor& northWestSouthEastResizeCursor()
{
    return Cursors::self()->NorthWestSouthEastResizeCursor;
}

const Cursor& columnResizeCursor()
{
    return Cursors::self()->ColumnResizeCursor;
}

const Cursor& rowResizeCursor()
{
    return Cursors::self()->RowResizeCursor;
}

const Cursor& middlePanningCursor()
{
    return Cursors::self()->MiddlePanningCursor;
}

const Cursor& eastPanningCursor()
{
    return Cursors::self()->EastPanningCursor;
}

const Cursor& northPanningCursor()
{
    return Cursors::self()->NorthPanningCursor;
}

const Cursor& northEastPanningCursor()
{
    return Cursors::self()->NorthEastPanningCursor;
}

const Cursor& northWestPanningCursor()
{
    return Cursors::self()->NorthWestPanningCursor;
}

const Cursor& southPanningCursor()
{
    return Cursors::self()->SouthPanningCursor;
}

const Cursor& southEastPanningCursor()
{
    return Cursors::self()->SouthEastPanningCursor;
}

const Cursor& southWestPanningCursor()
{
    return Cursors::self()->SouthWestPanningCursor;
}

const Cursor& westPanningCursor()
{
    return Cursors::self()->WestPanningCursor;
}

const Cursor& verticalTextCursor()
{
    return Cursors::self()->VerticalTextCursor;
}

const Cursor& cellCursor()
{
    return Cursors::self()->CellCursor;
}

const Cursor& contextMenuCursor()
{
    return Cursors::self()->ContextMenuCursor;
}

const Cursor& noDropCursor()
{
    return Cursors::self()->NoDropCursor;
}

const Cursor& copyCursor()
{
    return Cursors::self()->CopyCursor;
}

const Cursor& progressCursor()
{
    return Cursors::self()->ProgressCursor;
}

const Cursor& aliasCursor()
{
    return Cursors::self()->AliasCursor;
}

const Cursor& noneCursor()
{
    return Cursors::self()->NoneCursor;
}

const Cursor& notAllowedCursor()
{
    return Cursors::self()->NotAllowedCursor;
}

const Cursor& zoomInCursor()
{
    return Cursors::self()->ZoomInCursor;
}

const Cursor& zoomOutCursor()
{
    return Cursors::self()->ZoomOutCursor;
}

const Cursor& grabCursor()
{
    return Cursors::self()->GrabCursor;
}

const Cursor& grabbingCursor()
{
    return Cursors::self()->GrabbingCursor;
}

}
