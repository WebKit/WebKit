/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
#include "ewk_context_private.h"
#include <WebKit2/WKImageCairo.h>

using namespace WebKit;

static inline WKViewRef createWKView(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef, EwkView::ViewBehavior behavior)
{
    RefPtr<EwkContext> context = contextRef ? EwkContext::create(contextRef) : EwkContext::defaultContext();
    Evas_Object* evasObject = EwkView::createEvasObject(canvas, context, pageGroupRef, behavior);
    if (!evasObject)
        return 0;

    return static_cast<WKViewRef>(WKRetain(toEwkView(evasObject)->wkView()));
}

WKViewRef WKViewCreate(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    return createWKView(canvas, contextRef, pageGroupRef, EwkView::LegacyBehavior);
}

WKViewRef WKViewCreateWithFixedLayout(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    return createWKView(canvas, contextRef, pageGroupRef, EwkView::DefaultBehavior);
}

void WKViewInitialize(WKViewRef viewRef)
{
    toImpl(viewRef)->initialize();
}

void WKViewSetViewClient(WKViewRef viewRef, const WKViewClient* client)
{
    toImpl(viewRef)->initializeClient(client);
}

void WKViewSetUserViewportTranslation(WKViewRef viewRef, double tx, double ty)
{
    toImpl(viewRef)->setUserViewportTranslation(tx, ty);
}

WKPoint WKViewUserViewportToContents(WKViewRef viewRef, WKPoint point)
{
    const WebCore::IntPoint& result = toImpl(viewRef)->userViewportToContents(toIntPoint(point));
    return WKPointMake(result.x(), result.y());
}

void WKViewPaintToCurrentGLContext(WKViewRef viewRef)
{
    toImpl(viewRef)->paintToCurrentGLContext();
}

void WKViewPaintToCairoSurface(WKViewRef viewRef, cairo_surface_t* surface)
{
    toImpl(viewRef)->paintToCairoSurface(surface);
}

WKPageRef WKViewGetPage(WKViewRef viewRef)
{
    return toImpl(viewRef)->pageRef();
}

void WKViewSetDrawsBackground(WKViewRef viewRef, bool flag)
{
    toImpl(viewRef)->setDrawsBackground(flag);
}

bool WKViewGetDrawsBackground(WKViewRef viewRef)
{
    return toImpl(viewRef)->drawsBackground();
}

void WKViewSetDrawsTransparentBackground(WKViewRef viewRef, bool flag)
{
    toImpl(viewRef)->setDrawsTransparentBackground(flag);
}

bool WKViewGetDrawsTransparentBackground(WKViewRef viewRef)
{
    return toImpl(viewRef)->drawsTransparentBackground();
}

void WKViewSetThemePath(WKViewRef viewRef, WKStringRef theme)
{
    toImpl(viewRef)->setThemePath(toImpl(theme)->string());
}

void WKViewSuspendActiveDOMObjectsAndAnimations(WKViewRef viewRef)
{
    toImpl(viewRef)->suspendActiveDOMObjectsAndAnimations();
}

void WKViewResumeActiveDOMObjectsAndAnimations(WKViewRef viewRef)
{
    toImpl(viewRef)->resumeActiveDOMObjectsAndAnimations();
}

void WKViewSetShowsAsSource(WKViewRef viewRef, bool flag)
{
    toImpl(viewRef)->setShowsAsSource(flag);
}

bool WKViewGetShowsAsSource(WKViewRef viewRef)
{
    return toImpl(viewRef)->showsAsSource();
}

void WKViewExitFullScreen(WKViewRef viewRef)
{
#if ENABLE(FULLSCREEN_API)
    toImpl(viewRef)->exitFullScreen();
#else
    UNUSED_PARAM(viewRef);
#endif
}

Evas_Object* WKViewGetEvasObject(WKViewRef viewRef)
{
    return toImpl(viewRef)->evasObject();
}

WKImageRef WKViewCreateSnapshot(WKViewRef viewRef)
{
    EwkView* ewkView = toEwkView(toImpl(viewRef)->evasObject());
    return WKImageCreateFromCairoSurface(ewkView->takeSnapshot().get(), 0 /* options */);
}
