/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2006 Zack Rusin <zack@kde.org>
    Copyright (C) 2011 Research In Motion Limited.

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

#include <qglobal.h>

QT_BEGIN_NAMESPACE
class QInputEvent;
class QGraphicsSceneMouseEvent;
class QWheelEvent;
class QGraphicsSceneWheelEvent;
class QTouchEvent;
QT_END_NAMESPACE

namespace WebCore {

class PlatformMouseEvent;
class PlatformWheelEvent;


PlatformMouseEvent convertMouseEvent(QInputEvent*, int clickCount);
PlatformMouseEvent convertMouseEvent(QGraphicsSceneMouseEvent*, int clickCount);
PlatformWheelEvent convertWheelEvent(QWheelEvent*);
PlatformWheelEvent convertWheelEvent(QGraphicsSceneWheelEvent*);

#if ENABLE(TOUCH_EVENTS)
class PlatformTouchEvent;
PlatformTouchEvent convertTouchEvent(QTouchEvent*);
#endif
}
