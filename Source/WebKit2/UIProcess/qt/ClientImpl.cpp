/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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
#include "ClientImpl.h"

#include "WebPageProxy.h"
#include "WKAPICast.h"
#include <WKArray.h>
#include <WKPage.h>
#include <WKString.h>
#include <WKType.h>

using namespace WebKit;

void qt_wk_didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void*)
{
    if (!WKStringIsEqualToUTF8CString(messageName, "MessageFromNavigatorQtObject"))
        return;

    ASSERT(messageBody);
    ASSERT(WKGetTypeID(messageBody) == WKArrayGetTypeID());

    WKArrayRef body = static_cast<WKArrayRef>(messageBody);
    ASSERT(WKArrayGetSize(body) == 2);
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 0)) == WKPageGetTypeID());
    ASSERT(WKGetTypeID(WKArrayGetItemAtIndex(body, 1)) == WKStringGetTypeID());

    WKPageRef page = static_cast<WKPageRef>(WKArrayGetItemAtIndex(body, 0));
    WKStringRef str = static_cast<WKStringRef>(WKArrayGetItemAtIndex(body, 1));

    toImpl(page)->didReceiveMessageFromNavigatorQtObject(toImpl(str)->string());
}

void setupContextInjectedBundleClient(WKContextRef context)
{
    WKContextInjectedBundleClient injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(WKContextInjectedBundleClient));
    injectedBundleClient.version = kWKContextInjectedBundleClientCurrentVersion;
    injectedBundleClient.didReceiveMessageFromInjectedBundle = qt_wk_didReceiveMessageFromInjectedBundle;
    WKContextSetInjectedBundleClient(context, &injectedBundleClient);
}
