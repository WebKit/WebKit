/*
 * Copyright (C) 2012 Igalia S.L.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "WebContextMenuItemGlib.h"
#include "WebKitContextMenu.h"

#if PLATFORM(GTK)
#include <WebCore/GRefPtrGtk.h>
#include <WebCore/GUniquePtrGtk.h>
#endif

WebKitContextMenu* webkitContextMenuCreate(const Vector<WebKit::WebContextMenuItemData>&);
void webkitContextMenuPopulate(WebKitContextMenu*, Vector<WebKit::WebContextMenuItemGlib>&);
void webkitContextMenuPopulate(WebKitContextMenu*, Vector<WebKit::WebContextMenuItemData>&);
void webkitContextMenuSetParentItem(WebKitContextMenu*, WebKitContextMenuItem*);
WebKitContextMenuItem* webkitContextMenuGetParentItem(WebKitContextMenu*);
#if PLATFORM(GTK)
#if USE(GTK4)
void webkitContextMenuSetEvent(WebKitContextMenu*, GRefPtr<GdkEvent>&&);
#else
void webkitContextMenuSetEvent(WebKitContextMenu*, GUniquePtr<GdkEvent>&&);
#endif
#endif
