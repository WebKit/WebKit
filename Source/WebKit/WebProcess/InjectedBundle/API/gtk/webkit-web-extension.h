/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#ifdef __WEBKIT_2_H__
#error "Headers <webkit2/webkit2.h> and <webkit2/webkit-web-extension.h> cannot be included together."
#endif

#ifndef __WEBKIT_WEB_EXTENSION_H__
#define __WEBKIT_WEB_EXTENSION_H__

#define __WEBKIT_WEB_EXTENSION_H_INSIDE__

#include <webkit2/WebKitConsoleMessage.h>
#include <webkit2/WebKitContextMenu.h>
#include <webkit2/WebKitContextMenuActions.h>
#include <webkit2/WebKitContextMenuItem.h>
#include <webkit2/WebKitFrame.h>
#include <webkit2/WebKitScriptWorld.h>
#include <webkit2/WebKitURIRequest.h>
#include <webkit2/WebKitURIResponse.h>
#include <webkit2/WebKitUserMessage.h>
#include <webkit2/WebKitVersion.h>
#include <webkit2/WebKitWebEditor.h>
#include <webkit2/WebKitWebExtension.h>
#include <webkit2/WebKitWebHitTestResult.h>
#include <webkit2/WebKitWebPage.h>
#include <webkit2/WebKitWebProcessEnumTypes.h>

#include <webkit2/WebKitWebExtensionAutocleanups.h>

#undef __WEBKIT_WEB_EXTENSION_H_INSIDE__

#endif
