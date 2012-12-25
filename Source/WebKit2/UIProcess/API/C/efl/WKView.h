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
#else
typedef struct _Evas Evas;
#endif

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT WKViewRef WKViewCreate(Evas* canvas, WKContextRef context, WKPageGroupRef pageGroup);

WK_EXPORT WKViewRef WKViewCreateWithFixedLayout(Evas* canvas, WKContextRef context, WKPageGroupRef pageGroup);

WK_EXPORT WKPageRef WKViewGetPage(WKViewRef view);

WK_EXPORT WKImageRef WKViewCreateSnapshot(WKViewRef viewRef);

#ifdef __cplusplus
}
#endif

#endif /* WKView_h */
