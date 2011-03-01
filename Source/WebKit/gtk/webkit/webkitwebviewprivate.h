/*
 * Copyright (C) 2007, 2008, 2009 Holger Hans Peter Freyther
 * Copyright (C) 2008 Jan Michael C. Alonzo
 * Copyright (C) 2008, 2010 Collabora Ltd.
 * Copyright (C) 2010 Igalia S.L.
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

#ifndef webkitwebviewprivate_h
#define webkitwebviewprivate_h

#include "DataObjectGtk.h"
#include "FullscreenVideoController.h"
#include "GOwnPtr.h"
#include "ResourceHandle.h"
#include <webkit/webkitwebview.h>

namespace WebKit {

WebCore::Page* core(WebKitWebView*);
WebKitWebView* kit(WebCore::Page*);


typedef struct DroppingContext_ {
    WebKitWebView* webView;
    GdkDragContext* gdkContext;
    RefPtr<WebCore::DataObjectGtk> dataObject;
    WebCore::IntPoint lastMotionPosition;
    int pendingDataRequests;
    bool dropHappened;
} DroppingContext;

}

extern "C" {

#define WEBKIT_WEB_VIEW_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_VIEW, WebKitWebViewPrivate))
typedef struct _WebKitWebViewPrivate WebKitWebViewPrivate;
struct _WebKitWebViewPrivate {
    WebCore::Page* corePage;
    GRefPtr<WebKitWebSettings> webSettings;
    GRefPtr<WebKitWebInspector> webInspector;
    GRefPtr<WebKitViewportAttributes> viewportAttributes;
    GRefPtr<WebKitWebWindowFeatures> webWindowFeatures;

    WebKitWebFrame* mainFrame;
    GRefPtr<WebKitWebBackForwardList> backForwardList;

    GtkMenu* currentMenu;
    gint lastPopupXPosition;
    gint lastPopupYPosition;

    HashSet<GtkWidget*> children;
    GRefPtr<GtkIMContext> imContext;

    gboolean transparent;

    GRefPtr<GtkAdjustment> horizontalAdjustment;
    GRefPtr<GtkAdjustment> verticalAdjustment;

#ifndef GTK_API_VERSION_2
    // GtkScrollablePolicy needs to be checked when
    // driving the scrollable adjustment values
    GtkScrollablePolicy horizontalScrollingPolicy;
    GtkScrollablePolicy verticalScrollingPolicy;
#endif

    gboolean zoomFullContent;
    WebKitLoadStatus loadStatus;
    CString encoding;
    CString customEncoding;

    CString iconURI;

    gboolean disposing;
    gboolean usePrimaryForPaste;

#if ENABLE(VIDEO)
    FullscreenVideoController* fullscreenVideoController;
#endif

    // These are hosted here because the DataSource object is
    // created too late in the frame loading process.
    GRefPtr<WebKitWebResource> mainResource;
    CString mainResourceIdentifier;
    GRefPtr<GHashTable> subResources;
    CString tooltipText;
    WebCore::IntRect tooltipArea;

    int currentClickCount;
    WebCore::IntPoint previousClickPoint;
    guint previousClickButton;
    guint32 previousClickTime;
    HashMap<GdkDragContext*, RefPtr<WebCore::DataObjectGtk> > draggingDataObjects;
    HashMap<GdkDragContext*, WebKit::DroppingContext*> droppingContexts;
};

void webkit_web_view_notify_ready(WebKitWebView*);

void webkit_web_view_request_download(WebKitWebView*, WebKitNetworkRequest*, const WebCore::ResourceResponse& = WebCore::ResourceResponse(), WebCore::ResourceHandle* = 0);

void webkit_web_view_add_resource(WebKitWebView*, const char*, WebKitWebResource*);
void webkit_web_view_add_main_resource(WebKitWebView*, const char*, WebKitWebResource*);
void webkit_web_view_remove_resource(WebKitWebView*, const char*);
WebKitWebResource* webkit_web_view_get_resource(WebKitWebView*, char*);
WebKitWebResource* webkit_web_view_get_main_resource(WebKitWebView*);
void webkit_web_view_clear_resources(WebKitWebView*);
GList* webkit_web_view_get_subresources(WebKitWebView*);

void webkit_web_view_set_tooltip_text(WebKitWebView*, const char*);
GtkMenu* webkit_web_view_get_context_menu(WebKitWebView*);

WEBKIT_API gchar* webkit_web_view_get_selected_text(WebKitWebView*);
bool webkit_web_view_use_primary_for_paste(WebKitWebView*);

void webViewEnterFullscreen(WebKitWebView* webView, WebCore::Node*);
void webViewExitFullscreen(WebKitWebView* webView);

}

#endif
