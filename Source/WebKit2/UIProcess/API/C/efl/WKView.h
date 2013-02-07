/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WKView_h
#define WKView_h

#include <WebKit2/WKBase.h>

#if USE(EO)
typedef struct _Eo Evas;
typedef struct _Eo Evas_Object;
#else
typedef struct _Evas Evas;
typedef struct _Evas_Object Evas_Object;
#endif

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT WKViewRef WKViewCreate(Evas* canvas, WKContextRef context, WKPageGroupRef pageGroup);

WK_EXPORT WKViewRef WKViewCreateWithFixedLayout(Evas* canvas, WKContextRef context, WKPageGroupRef pageGroup);

WK_EXPORT void WKViewInitialize(WKViewRef);

WK_EXPORT WKPageRef WKViewGetPage(WKViewRef);

WK_EXPORT void WKViewSetThemePath(WKViewRef, WKStringRef);

WK_EXPORT void WKViewSetDrawsBackground(WKViewRef, bool);
WK_EXPORT bool WKViewGetDrawsBackground(WKViewRef);

WK_EXPORT void WKViewSetDrawsTransparentBackground(WKViewRef, bool);
WK_EXPORT bool WKViewGetDrawsTransparentBackground(WKViewRef);

WK_EXPORT void WKViewSuspendActiveDOMObjectsAndAnimations(WKViewRef);
WK_EXPORT void WKViewResumeActiveDOMObjectsAndAnimations(WKViewRef);

// FIXME: The long term plan is to get rid of this, so keep usage to a bare minimum.
WK_EXPORT Evas_Object* WKViewGetEvasObject(WKViewRef);

WK_EXPORT WKImageRef WKViewCreateSnapshot(WKViewRef);

#ifdef __cplusplus
}
#endif

#endif /* WKView_h */
