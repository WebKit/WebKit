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

#include "EwkViewImpl.h"
#include "WKAPICast.h"
#include "ewk_view_private.h"
#include <WebKit2/WKImageCairo.h>

using namespace WebKit;

WKViewRef WKViewCreate(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    return toAPI(ewk_view_base_add(canvas, contextRef, pageGroupRef, EwkViewImpl::LegacyBehavior));
}

WKViewRef WKViewCreateWithFixedLayout(Evas* canvas, WKContextRef contextRef, WKPageGroupRef pageGroupRef)
{
    return toAPI(ewk_view_base_add(canvas, contextRef, pageGroupRef, EwkViewImpl::DefaultBehavior));
}

WKPageRef WKViewGetPage(WKViewRef viewRef)
{
    EwkViewImpl* viewImpl = EwkViewImpl::fromEvasObject(toImpl(viewRef));

    return viewImpl->wkPage();
}

WKImageRef WKViewCreateSnapshot(WKViewRef viewRef)
{
    EwkViewImpl* viewImpl = EwkViewImpl::fromEvasObject(toImpl(viewRef));

    return WKImageCreateFromCairoSurface(viewImpl->takeSnapshot().get(), 0 /* options */);
}
