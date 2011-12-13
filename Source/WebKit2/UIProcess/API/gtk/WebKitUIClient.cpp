/*
 * Copyright (C) 2011 Igalia S.L.
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

#include "config.h"
#include "WebKitUIClient.h"

#include "WebKitWebViewPrivate.h"
#include "WebPageProxy.h"
#include <WebKit2/WKBase.h>

using namespace WebKit;

G_DEFINE_TYPE(WebKitUIClient, webkit_ui_client, G_TYPE_OBJECT)

static WKPageRef createNewPage(WKPageRef page, WKURLRequestRef, WKDictionaryRef, WKEventModifiers, WKEventMouseButton, const void*)
{
    return webkitWebViewCreateNewPage(WEBKIT_WEB_VIEW(toImpl(page)->viewWidget()));
}

static void showPage(WKPageRef page, const void*)
{
    webkitWebViewReadyToShowPage(WEBKIT_WEB_VIEW(toImpl(page)->viewWidget()));
}

static void closePage(WKPageRef page, const void*)
{
    webkitWebViewClosePage(WEBKIT_WEB_VIEW(toImpl(page)->viewWidget()));
}

static void runJavaScriptAlert(WKPageRef page, WKStringRef message, WKFrameRef, const void*)
{
    webkitWebViewRunJavaScriptAlert(WEBKIT_WEB_VIEW(toImpl(page)->viewWidget()), toImpl(message)->string().utf8());
}

static bool runJavaScriptConfirm(WKPageRef page, WKStringRef message, WKFrameRef, const void*)
{
    return webkitWebViewRunJavaScriptConfirm(WEBKIT_WEB_VIEW(toImpl(page)->viewWidget()), toImpl(message)->string().utf8());
}

static WKStringRef runJavaScriptPrompt(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef, const void*)
{
    return webkitWebViewRunJavaScriptPrompt(WEBKIT_WEB_VIEW(toImpl(page)->viewWidget()), toImpl(message)->string().utf8(),
                                            toImpl(defaultValue)->string().utf8());
}

void webkitUIClientAttachUIClientToPage(WebKitUIClient* uiClient, WKPageRef wkPage)
{
    WKPageUIClient wkUIClient = {
        kWKPageUIClientCurrentVersion,
        uiClient, // clientInfo
        0, // createNewPage_deprecatedForUseWithV0
        showPage,
        closePage,
        0, // takeFocus
        0, // focus
        0, // unfocus
        runJavaScriptAlert,
        runJavaScriptConfirm,
        runJavaScriptPrompt,
        0, // setStatusText
        0, // mouseDidMoveOverElement_deprecatedForUseWithV0
        0, // missingPluginButtonClicked
        0, // didNotHandleKeyEvent
        0, // didNotHandleWheelEvent
        0, // toolbarsAreVisible
        0, // setToolbarsAreVisible
        0, // menuBarIsVisible
        0, // setMenuBarIsVisible
        0, // statusBarIsVisible
        0, // setStatusBarIsVisible
        0, // isResizable
        0, // setIsResizable
        0, // getWindowFrame
        0, // setWindowFrame
        0, // runBeforeUnloadConfirmPanel
        0, // didDraw
        0, // pageDidScroll
        0, // exceededDatabaseQuota
        0, // runOpenPanel
        0, // decidePolicyForGeolocationPermissionRequest
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        0, // printFrame
        0, // runModal
        0, // didCompleteRubberBandForMainFrame
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        createNewPage,
        0, // mouseDidMoveOverElement
        0, // decidePolicyForNotificationPermissionRequest
    };
    WKPageSetPageUIClient(wkPage, &wkUIClient);
}

static void webkit_ui_client_init(WebKitUIClient* client)
{
}

static void webkit_ui_client_class_init(WebKitUIClientClass* clientClass)
{
}
