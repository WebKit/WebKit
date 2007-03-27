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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "PlatformWheelEvent.h"

#include "PlatformMouseEvent.h"

#include <QWheelEvent>

namespace WebCore {

PlatformWheelEvent::PlatformWheelEvent(QWheelEvent* e)
    : m_position(e->pos())
    , m_globalPosition(e->globalPos())
    , m_deltaX(e->delta())
    , m_deltaY(0)
    , m_isAccepted(false)
    , m_shiftKey(e->modifiers() & Qt::ShiftModifier)
    , m_ctrlKey(e->modifiers() & Qt::ControlModifier)
    , m_altKey(e->modifiers() & Qt::AltModifier)
    , m_metaKey(e->modifiers() & Qt::MetaModifier)
    , m_isContinuous(false)
{
}

} // namespace WebCore
