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

#ifndef GtkUtilities_h 
#define GtkUtilities_h 

#include <wtf/MonotonicTime.h>
#include <wtf/WallTime.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class IntPoint;

IntPoint convertWidgetPointToScreenPoint(GtkWidget*, const IntPoint&);
bool widgetIsOnscreenToplevelWindow(GtkWidget*);

template<typename GdkEventType>
WallTime wallTimeForEvent(const GdkEventType* event)
{
    // FIXME: 0 is GDK_CURRENT_TIME. We should stop including this header from
    // HyphenationLibHyphen.cpp so that we can include gtk/gtk.h here and use
    // GDK_CURRENT_TIME.
    if (event->time == 0)
        return WallTime::now();
    return MonotonicTime::fromRawSeconds(event->time / 1000.).approximateWallTime();
}

template<>
WallTime wallTimeForEvent(const GdkEvent*);

String defaultGtkSystemFont();

#if ENABLE(DEVELOPER_MODE)
CString webkitBuildDirectory();
#endif

} // namespace WebCore

#endif // GtkUtilities_h
