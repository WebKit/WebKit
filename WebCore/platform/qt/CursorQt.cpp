/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Charles Samuels <charles@kde.org>
 * Copyright (C) 2008 Holger Hans Peter Freyther
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

#include "Image.h"
#include "IntPoint.h"

#include "NotImplemented.h"

#include <stdio.h>
#include <stdlib.h>

#undef CopyCursor

namespace WebCore {

Cursor::Cursor(PlatformCursor p)
    : m_impl(p)
{
}

Cursor::Cursor(const Cursor& other)
    : m_impl(other.m_impl)
{
}

Cursor::~Cursor()
{
}

Cursor::Cursor(Image* image, const IntPoint& hotspot)
#ifndef QT_NO_CURSOR
    : m_impl(*(image->nativeImageForCurrentFrame()), hotspot.x(), hotspot.y())
#endif
{
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_impl = other.m_impl;
    return *this;
}

namespace {

// FIXME: static deleter
class Cursors {
protected:
    Cursors()
#ifndef QT_NO_CURSOR
        : CrossCursor(Qt::CrossCursor)
        , MoveCursor(Qt::SizeAllCursor)
        , PointerCursor(Qt::ArrowCursor)
        , PointingHandCursor(Qt::PointingHandCursor)
        , IBeamCursor(Qt::IBeamCursor)
        , WaitCursor(Qt::WaitCursor)
        , WhatsThisCursor(Qt::WhatsThisCursor)
        , SizeHorCursor(Qt::SizeHorCursor)
        , SizeVerCursor(Qt::SizeVerCursor)
        , SizeFDiagCursor(Qt::SizeFDiagCursor)
        , SizeBDiagCursor(Qt::SizeBDiagCursor)
        , SplitHCursor(Qt::SplitHCursor)
        , SplitVCursor(Qt::SplitVCursor)
        , NoDropCursor(Qt::ForbiddenCursor)
        , BlankCursor(Qt::BlankCursor)
        , ZoomInCursor(QPixmap(QLatin1String(":/webkit/resources/zoomInCursor.png")))
        , ZoomOutCursor(QPixmap(QLatin1String(":/webkit/resources/zoomOutCursor.png")))
        , VerticalTextCursor(QPixmap(QLatin1String(":/webkit/resources/verticalTextCursor.png")))
        , CellCursor(QPixmap(QLatin1String(":/webkit/resources/cellCursor.png")))
        , ContextMenuCursor(QPixmap(QLatin1String(":/webkit/resources/contextMenuCursor.png")))
        , CopyCursor(QPixmap(QLatin1String(":/webkit/resources/copyCursor.png")))
        , ProgressCursor(QPixmap(QLatin1String(":/webkit/resources/progressCursor.png")))
        , AliasCursor(QPixmap(QLatin1String(":/webkit/resources/aliasCursor.png")))

#endif
    {
    }

    ~Cursors()
    {
    }

public:
    static Cursors* self();
    static Cursors* s_self;

    Cursor CrossCursor;
    Cursor MoveCursor;
    Cursor PointerCursor;
    Cursor PointingHandCursor;
    Cursor IBeamCursor;
    Cursor WaitCursor;
    Cursor WhatsThisCursor;
    Cursor SizeHorCursor;
    Cursor SizeVerCursor;
    Cursor SizeFDiagCursor;
    Cursor SizeBDiagCursor;
    Cursor SplitHCursor;
    Cursor SplitVCursor;
    Cursor NoDropCursor;
    Cursor BlankCursor;
    Cursor ZoomInCursor;
    Cursor ZoomOutCursor;
    Cursor VerticalTextCursor;
    Cursor CellCursor;
    Cursor ContextMenuCursor;
    Cursor CopyCursor;
    Cursor ProgressCursor;
    Cursor AliasCursor;
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
    return Cursors::self()->PointingHandCursor;
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
    return Cursors::self()->WhatsThisCursor;
}

const Cursor& eastResizeCursor()
{
    return Cursors::self()->SizeHorCursor;
}

const Cursor& northResizeCursor()
{
    return Cursors::self()->SizeVerCursor;
}

const Cursor& northEastResizeCursor()
{
    return Cursors::self()->SizeBDiagCursor;
}

const Cursor& northWestResizeCursor()
{
    return Cursors::self()->SizeFDiagCursor;
}

const Cursor& southResizeCursor()
{
    return Cursors::self()->SizeVerCursor;
}

const Cursor& southEastResizeCursor()
{
    return Cursors::self()->SizeFDiagCursor;
}

const Cursor& southWestResizeCursor()
{
    return Cursors::self()->SizeBDiagCursor;
}

const Cursor& westResizeCursor()
{
    return Cursors::self()->SizeHorCursor;
}

const Cursor& northSouthResizeCursor()
{
    return Cursors::self()->SizeVerCursor;
}

const Cursor& eastWestResizeCursor()
{
    return Cursors::self()->SizeHorCursor;
}

const Cursor& northEastSouthWestResizeCursor()
{
    return Cursors::self()->SizeBDiagCursor;
}

const Cursor& northWestSouthEastResizeCursor()
{
    return Cursors::self()->SizeFDiagCursor;
}

const Cursor& columnResizeCursor()
{
    return Cursors::self()->SplitHCursor;
}

const Cursor& rowResizeCursor()
{
    return Cursors::self()->SplitVCursor;
}
    
const Cursor& middlePanningCursor()
{
    return moveCursor();
}

const Cursor& eastPanningCursor()
{
    return eastResizeCursor();
}

const Cursor& northPanningCursor()
{
    return northResizeCursor();
}

const Cursor& northEastPanningCursor()
{
    return northEastResizeCursor();
}

const Cursor& northWestPanningCursor()
{
    return northWestResizeCursor();
}

const Cursor& southPanningCursor()
{
    return southResizeCursor();
}

const Cursor& southEastPanningCursor()
{
    return southEastResizeCursor();
}

const Cursor& southWestPanningCursor()
{
    return southWestResizeCursor();
}

const Cursor& westPanningCursor()
{
    return westResizeCursor();
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
    return Cursors::self()->BlankCursor;
}

const Cursor& notAllowedCursor()
{
   return Cursors::self()->NoDropCursor;
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
    notImplemented();
    return Cursors::self()->PointerCursor;
}

const Cursor& grabbingCursor()
{
    notImplemented();
    return Cursors::self()->PointerCursor;
}

}

// vim: ts=4 sw=4 et
