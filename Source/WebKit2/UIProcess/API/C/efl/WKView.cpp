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

#include "config.h"
#include "WKView.h"

#include "EwkView.h"
#include "WKAPICast.h"
#include "ewk_view_private.h"
#include <WebKit2/WKImageCairo.h>

using namespace WebKit;

WKViewRef WKViewCreate(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    EwkView* ewkView = ewk_view_base_add(canvas, contextRef, pageGroupRef, EwkView::LegacyBehavior);
    if (!ewkView)
        return 0;

    return static_cast<WKViewRef>(WKRetain(ewkView->wkView()));
}

WKViewRef WKViewCreateWithFixedLayout(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    EwkView* ewkView = ewk_view_base_add(canvas, contextRef, pageGroupRef, EwkView::DefaultBehavior);
    if (!ewkView)
        return 0;

    return static_cast<WKViewRef>(WKRetain(ewkView->wkView()));
}

void WKViewInitialize(WKViewRef viewRef)
{
    toImpl(viewRef)->initialize();
}

WKPageRef WKViewGetPage(WKViewRef viewRef)
{
    return toImpl(viewRef)->pageRef();
}

void WKViewSetThemePath(WKViewRef viewRef, WKStringRef theme)
{
    toImpl(viewRef)->setThemePath(theme);
}

void WKViewSuspendActiveDOMObjectsAndAnimations(WKViewRef viewRef)
{
    toImpl(viewRef)->suspendActiveDOMObjectsAndAnimations();
}

void WKViewResumeActiveDOMObjectsAndAnimations(WKViewRef viewRef)
{
    toImpl(viewRef)->resumeActiveDOMObjectsAndAnimations();
}

Evas_Object* WKViewGetEvasObject(WKViewRef viewRef)
{
    return toImpl(viewRef)->evasObject();
}

WKImageRef WKViewCreateSnapshot(WKViewRef viewRef)
{
    EwkView* ewkView = EwkView::fromEvasObject(toImpl(viewRef)->evasObject());
    return WKImageCreateFromCairoSurface(ewkView->takeSnapshot().get(), 0 /* options */);
}
