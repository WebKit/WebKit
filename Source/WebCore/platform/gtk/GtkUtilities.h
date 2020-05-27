/*
 * Copyright (C) 2011, 2012 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "DragActions.h"
#include <gtk/gtk.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WallTime.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class IntPoint;

IntPoint convertWidgetPointToScreenPoint(GtkWidget*, const IntPoint&);
bool widgetIsOnscreenToplevelWindow(GtkWidget*);
IntPoint widgetRootCoords(GtkWidget*, int, int);
unsigned widgetKeyvalToKeycode(GtkWidget*, unsigned);

template<typename GdkEventType>
WallTime wallTimeForEvent(const GdkEventType* event)
{
    const auto eventTime = gdk_event_get_time(reinterpret_cast<GdkEvent*>(const_cast<GdkEventType*>(event)));
    if (eventTime == GDK_CURRENT_TIME)
        return WallTime::now();
    return MonotonicTime::fromRawSeconds(eventTime / 1000.).approximateWallTime();
}

template<>
WallTime wallTimeForEvent(const GdkEvent*);

String defaultGtkSystemFont();

WEBCORE_EXPORT unsigned stateModifierForGdkButton(unsigned button);

WEBCORE_EXPORT DragOperation gdkDragActionToDragOperation(GdkDragAction);
WEBCORE_EXPORT GdkDragAction dragOperationToGdkDragActions(DragOperation);
WEBCORE_EXPORT GdkDragAction dragOperationToSingleGdkDragAction(DragOperation);

} // namespace WebCore
