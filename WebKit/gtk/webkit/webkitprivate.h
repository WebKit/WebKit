/*
 * Copyright (C) 2007, 2008, 2009 Holger Hans Peter Freyther
 * Copyright (C) 2008 Jan Michael C. Alonzo
 * Copyright (C) 2008 Collabora Ltd.
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

#ifndef webkitprivate_h
#define webkitprivate_h

/*
 * This file knows the shared secret of WebKitWebView, WebKitWebFrame,
 * and WebKitNetworkRequest.
 * They are using WebCore which musn't be exposed to the outer world.
 */

#include <webkit/webkitdefines.h>
#include <webkit/webkitdownload.h>
#include <webkit/webkithittestresult.h>
#include <webkit/webkitnetworkrequest.h>
#include <webkit/webkitviewportattributes.h>
#include <webkit/webkitwebview.h>
#include <webkit/webkitwebdatasource.h>
#include <webkit/webkitwebframe.h>
#include <webkit/webkitwebpolicydecision.h>
#include <webkit/webkitwebnavigationaction.h>
#include <webkit/webkitwebresource.h>
#include <webkit/webkitwebsettings.h>
#include <webkit/webkitwebwindowfeatures.h>
#include <webkit/webkitwebbackforwardlist.h>
#include <webkit/webkitnetworkrequest.h>
#include <webkit/webkitsecurityorigin.h>

#include "ArchiveResource.h"
#include "BackForwardListImpl.h"
#include "DataObjectGtk.h"
#include "DragActions.h"
#include "Frame.h"
#include "GOwnPtr.h"
#include "Geolocation.h"
#include "HistoryItem.h"
#include "InspectorClientGtk.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "FrameLoaderClient.h"
#include "Node.h"
#include "Page.h"
#include "PlatformString.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "WindowFeatures.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <enchant.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>

#include <atk/atk.h>
#include <glib.h>
#include <libsoup/soup.h>

class DownloadClient;

namespace WebKit {

    class DocumentLoader;
    class PasteboardHelperGtk;

    WebKitWebView* getViewFromFrame(WebKitWebFrame*);

    WebCore::Frame* core(WebKitWebFrame*);
    WebKitWebFrame* kit(WebCore::Frame*);

    WebCore::HistoryItem* core(WebKitWebHistoryItem*);
    WebKitWebHistoryItem* kit(PassRefPtr<WebCore::HistoryItem>);

    WebCore::BackForwardListImpl* core(WebKitWebBackForwardList*);

    WebKitWebNavigationReason kit(WebCore::NavigationType type);
    WebCore::NavigationType core(WebKitWebNavigationReason reason);

    WebCore::ResourceRequest core(WebKitNetworkRequest* request);

    WebCore::ResourceResponse core(WebKitNetworkResponse* response);

    WebCore::EditingBehaviorType core(WebKitEditingBehavior type);

    WebKitSecurityOrigin* kit(WebCore::SecurityOrigin*);
    WebCore::SecurityOrigin* core(WebKitSecurityOrigin*);

    WebKitHitTestResult* kit(const WebCore::HitTestResult&);

    PasteboardHelperGtk* pasteboardHelperInstance();
}

extern "C" {
    void webkit_init();

#ifdef HAVE_GSETTINGS
    GSettings* inspectorGSettings();
#endif

#define WEBKIT_PARAM_READABLE ((GParamFlags)(G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB))
#define WEBKIT_PARAM_READWRITE ((GParamFlags)(G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB))

    #define WEBKIT_WEB_FRAME_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_FRAME, WebKitWebFramePrivate))
    typedef struct _WebKitWebFramePrivate WebKitWebFramePrivate;
    struct _WebKitWebFramePrivate {
        WebCore::Frame* coreFrame;
        WebKitWebView* webView;

        gchar* name;
        gchar* title;
        gchar* uri;
        WebKitLoadStatus loadStatus;
        WebKitSecurityOrigin* origin;
    };

#define WEBKIT_SECURITY_ORIGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_SECURITY_ORIGIN, WebKitSecurityOriginPrivate))
    struct _WebKitSecurityOriginPrivate {
        RefPtr<WebCore::SecurityOrigin> coreOrigin;
        gchar* protocol;
        gchar* host;
        GHashTable* webDatabases;

        gboolean disposed;
    };

    struct _WebKitViewportAttributesPrivate {
        WebKitWebView* webView;
        int deviceWidth;
        int deviceHeight;
        int availableWidth;
        int availableHeight;
        int desktopWidth;
        int deviceDPI;

        int width;
        int height;
        float initialScaleFactor;
        float minimumScaleFactor;
        float maximumScaleFactor;
        float devicePixelRatio;
        gboolean userScalable;
        gboolean isValid;
    };

    WTF::String
    webkitUserAgent();

    void
    webkit_web_frame_core_frame_gone(WebKitWebFrame*);

    // WebKitWebHistoryItem private
    WebKitWebHistoryItem*
    webkit_web_history_item_new_with_core_item(PassRefPtr<WebCore::HistoryItem> historyItem);

    WEBKIT_API G_CONST_RETURN gchar*
    webkit_web_history_item_get_target(WebKitWebHistoryItem*);

    WEBKIT_API gboolean
    webkit_web_history_item_is_target_item(WebKitWebHistoryItem*);

    WEBKIT_API GList*
    webkit_web_history_item_get_children(WebKitWebHistoryItem*);
    // end WebKitWebHistoryItem private

    // WebKitWebResource private
    #define WEBKIT_WEB_RESOURCE_GET_PRIVATE(obj)        (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_RESOURCE, WebKitWebResourcePrivate))
    struct _WebKitWebResourcePrivate {
        WebCore::ArchiveResource* resource;

        gchar* uri;
        gchar* mimeType;
        gchar* textEncoding;
        gchar* frameName;

        GString* data;
    };
    WebKitWebResource*
    webkit_web_resource_new_with_core_resource(PassRefPtr<WebCore::ArchiveResource>);

    void
    webkit_web_resource_init_with_core_resource(WebKitWebResource*, PassRefPtr<WebCore::ArchiveResource>);

    // end WebKitWebResource private

    void
    webkit_web_inspector_set_inspector_client(WebKitWebInspector*, WebCore::Page*);

    void
    webkit_web_inspector_set_web_view(WebKitWebInspector *web_inspector, WebKitWebView *web_view);

    void
    webkit_web_inspector_set_inspected_uri(WebKitWebInspector* web_inspector, const gchar* inspected_uri);

    WEBKIT_API void
    webkit_web_inspector_execute_script(WebKitWebInspector* inspector, long callId, const gchar* script);


    WebKitWebWindowFeatures*
    webkit_web_window_features_new_from_core_features (const WebCore::WindowFeatures& features);

    WebKitDownload*
    webkit_download_new_with_handle(WebKitNetworkRequest* request, WebCore::ResourceHandle* handle, const WebCore::ResourceResponse& response);

    void
    webkit_download_set_suggested_filename(WebKitDownload* download, const gchar* suggestedFilename);

    WebKitWebPolicyDecision*
    webkit_web_policy_decision_new (WebKitWebFrame*, WebCore::FramePolicyFunction);

    void
    webkit_web_policy_decision_cancel (WebKitWebPolicyDecision* decision);

    WebKitNetworkRequest*
    webkit_network_request_new_with_core_request(const WebCore::ResourceRequest& resourceRequest);

    WebKitNetworkResponse*
    webkit_network_response_new_with_core_response(const WebCore::ResourceResponse& resourceResponse);

    WebKitGeolocationPolicyDecision*
    webkit_geolocation_policy_decision_new(WebKitWebFrame*, WebCore::Geolocation*);

    // FIXME: move this to webkitnetworkrequest.h once the API is agreed upon.
    WEBKIT_API SoupMessage*
    webkit_network_request_get_message(WebKitNetworkRequest* request);

    // FIXME: move this functionality into a 'WebKitWebDataSource' once implemented
    WEBKIT_API gchar*
    webkit_web_frame_get_response_mime_type(WebKitWebFrame* frame);

    // FIXME: Move these to webkitwebframe.h once their API has been discussed.

    WEBKIT_API void
    webkit_web_settings_add_extra_plugin_directory (WebKitWebView *web_view, const gchar* directory);

    GSList*
    webkit_web_settings_get_enchant_dicts(WebKitWebView* web_view);

    GHashTable*
    webkit_history_items(void);

    WEBKIT_API void
    webkit_application_cache_set_maximum_size(unsigned long long size);

    WEBKIT_API unsigned int
    webkit_worker_thread_count();
    
    // WebKitWebDataSource private
    WebKitWebDataSource*
    webkit_web_data_source_new_with_loader(PassRefPtr<WebKit::DocumentLoader>);

    WEBKIT_API WebKitWebDatabase *
    webkit_security_origin_get_web_database(WebKitSecurityOrigin* securityOrigin, const char* databaseName);

    // WebKitViewportAttributes private
    void webkitViewportAttributesRecompute(WebKitViewportAttributes*);
}

#endif
