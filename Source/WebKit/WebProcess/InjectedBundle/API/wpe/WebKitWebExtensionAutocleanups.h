/*
 * Copyright (C) 2015 Igalia S.L.
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

#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit-web-extension.h> can be included directly."
#endif

#ifndef WebKitWebExtensionAutocleanups_h
#define WebKitWebExtensionAutocleanups_h

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
#ifndef __GI_SCANNER__

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitFrame, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitScriptWorld, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebEditor, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebExtension, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebHitTestResult, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebPage, g_object_unref)

#endif /* __GI_SCANNER__ */
#endif /* G_DEFINE_AUTOPTR_CLEANUP_FUNC */

#endif /* WebKitWebExtensionAutocleanups_h */
