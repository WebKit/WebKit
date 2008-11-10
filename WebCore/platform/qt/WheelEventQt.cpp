/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "PlatformWheelEvent.h"

#include "PlatformMouseEvent.h"

#include <QWheelEvent>

namespace WebCore {


PlatformWheelEvent::PlatformWheelEvent(QWheelEvent* e)
#ifdef QT_NO_WHEELEVENT
{
    Q_UNUSED(e);
}
#else
    : m_position(e->pos())
    , m_globalPosition(e->globalPos())
#ifdef QT_MAC_USE_COCOA
    , m_granularity(ScrollByPixelWheelEvent)
#else
    , m_granularity(ScrollByLineWheelEvent)
#endif
    , m_isAccepted(false)
    , m_shiftKey(e->modifiers() & Qt::ShiftModifier)
    , m_ctrlKey(e->modifiers() & Qt::ControlModifier)
    , m_altKey(e->modifiers() & Qt::AltModifier)
    , m_metaKey(e->modifiers() & Qt::MetaModifier)
{
    if (e->orientation() == Qt::Horizontal) {
        m_deltaX = (e->delta() / 120);
        m_deltaY = 0;
    } else {
        m_deltaX = 0;
        m_deltaY = (e->delta() / 120);
    }

    // FIXME: retrieve the user setting for the number of lines to scroll on each wheel event
    m_deltaX *= horizontalLineMultiplier();
    m_deltaY *= verticalLineMultiplier();
}
#endif // QT_NO_WHEELEVENT

} // namespace WebCore
