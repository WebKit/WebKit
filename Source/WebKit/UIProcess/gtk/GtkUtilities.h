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

#include <WebCore/DragActions.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <wtf/MonotonicTime.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {
class Color;
class IntPoint;
class SelectionData;
}

namespace WebKit {

WebCore::IntPoint convertWidgetPointToScreenPoint(GtkWidget*, const WebCore::IntPoint&);
bool widgetIsOnscreenToplevelWindow(GtkWidget*);
WebCore::IntPoint widgetRootCoords(GtkWidget*, int, int);
void widgetDevicePosition(GtkWidget*, GdkDevice*, double*, double*, GdkModifierType*);
unsigned widgetKeyvalToKeycode(GtkWidget*, unsigned);

unsigned stateModifierForGdkButton(unsigned button);

OptionSet<WebCore::DragOperation> gdkDragActionToDragOperation(GdkDragAction);
GdkDragAction dragOperationToGdkDragActions(OptionSet<WebCore::DragOperation>);
GdkDragAction dragOperationToSingleGdkDragAction(OptionSet<WebCore::DragOperation>);
GRefPtr<GdkPixbuf> selectionDataImageAsGdkPixbuf(const WebCore::SelectionData&);

void monitorWorkArea(GdkMonitor*, GdkRectangle*);

bool eventModifiersContainCapsLock(GdkEvent*);

GRefPtr<GdkPixbuf> skiaImageToGdkPixbuf(SkImage&);
#if USE(GTK4)
GRefPtr<GdkTexture> skiaImageToGdkTexture(SkImage&);
#else
RefPtr<cairo_surface_t> skiaImageToCairoSurface(SkImage&);
#endif

WebCore::Color gdkRGBAToColor(const GdkRGBA&);
GdkRGBA colorToGdkRGBA(const WebCore::Color&);

} // namespace WebKit
