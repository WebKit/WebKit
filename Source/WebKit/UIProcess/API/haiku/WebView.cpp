/*
   Copyright (C) 2014 Haiku, inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "WebView.h"

#include "NotImplemented.h"
#include "WebContext.h"
#include "WebPageGroup.h"

using namespace WebKit;

BWebView::BWebView(WKContextRef context, WKPageGroupRef pageGroup)
    : BView("web view", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
    fWebView = adoptWK(WKViewCreate(context, pageGroup));
}

WKViewRef BWebView::GetWKView()
{
    return fWebView.get();
}

WKPageRef BWebView::pageRef()
{
    return WKViewGetPage(GetWKView());
}

