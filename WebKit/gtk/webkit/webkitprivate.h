/*
 * Copyright (C) 2007, 2008, 2009 Holger Hans Peter Freyther
 * Copyright (C) 2008 Jan Michael C. Alonzo
 * Copyright (C) 2008 Collabora Ltd.
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

#ifndef WEBKIT_PRIVATE_H
#define WEBKIT_PRIVATE_H

/*
 * This file knows the shared secret of WebKitWebView, WebKitWebFrame,
 * and WebKitNetworkRequest.
 * They are using WebCore which musn't be exposed to the outer world.
 */

#include <webkit/webkitdefines.h>
#include <webkit/webkitdownload.h>
#include <webkit/webkitnetworkrequest.h>
#include <webkit/webkitwebview.h>
#include <webkit/webkitwebframe.h>
#include <webkit/webkitwebpolicydecision.h>
#include <webkit/webkitwebnavigationaction.h>
#include <webkit/webkitwebsettings.h>
#include <webkit/webkitwebwindowfeatures.h>
#include <webkit/webkitwebbackforwardlist.h>
#include <webkit/webkitnetworkrequest.h>

#include "BackForwardList.h"
#include <enchant.h>
#include "HistoryItem.h"
#include "Settings.h"
#include "Page.h"
#include "Frame.h"
#include "InspectorClientGtk.h"
#include "FrameLoaderClient.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "WindowFeatures.h"

#include <glib.h>
#include <libsoup/soup.h>

class DownloadClient;

namespace WebKit {
    WebKitWebView* getViewFromFrame(WebKitWebFrame*);

    WebCore::Frame* core(WebKitWebFrame*);
    WebKitWebFrame* kit(WebCore::Frame*);

    WebCore::Page* core(WebKitWebView*);
    WebKitWebView* kit(WebCore::Page*);

    WebCore::HistoryItem* core(WebKitWebHistoryItem*);
    WebKitWebHistoryItem* kit(PassRefPtr<WebCore::HistoryItem>);

    WebCore::BackForwardList* core(WebKitWebBackForwardList*);

    WebKitWebNavigationReason kit(WebCore::NavigationType type);
    WebCore::NavigationType core(WebKitWebNavigationReason reason);

    WebCore::ResourceRequest core(WebKitNetworkRequest* request);
}

typedef struct {
    EnchantBroker* config;
    EnchantDict* speller;
} SpellLanguage;

extern "C" {
    void webkit_init();

#define WEBKIT_PARAM_READABLE ((GParamFlags)(G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB))
#define WEBKIT_PARAM_READWRITE ((GParamFlags)(G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB))

    #define WEBKIT_WEB_VIEW_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_VIEW, WebKitWebViewPrivate))
    typedef struct _WebKitWebViewPrivate WebKitWebViewPrivate;
    struct _WebKitWebViewPrivate {
        WebCore::Page* corePage;
        WebKitWebSettings* webSettings;
        WebKitWebInspector* webInspector;
        WebKitWebWindowFeatures* webWindowFeatures;

        WebKitWebFrame* mainFrame;
        WebKitWebBackForwardList* backForwardList;

        gint lastPopupXPosition;
        gint lastPopupYPosition;

        HashSet<GtkWidget*> children;
        bool editable;
        GtkIMContext* imContext;

        GtkTargetList* copy_target_list;
        GtkTargetList* paste_target_list;

        gboolean transparent;

        GtkAdjustment* horizontalAdjustment;
        GtkAdjustment* verticalAdjustment;

        gboolean zoomFullContent;
        WebKitLoadStatus loadStatus;
        char* encoding;
        char* customEncoding;

        gboolean disposing;
        gboolean usePrimaryForPaste;
    };

    #define WEBKIT_WEB_FRAME_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_FRAME, WebKitWebFramePrivate))
    typedef struct _WebKitWebFramePrivate WebKitWebFramePrivate;
    struct _WebKitWebFramePrivate {
        WebCore::Frame* coreFrame;
        WebKitWebView* webView;

        gchar* name;
        gchar* title;
        gchar* uri;
        WebKitLoadStatus loadStatus;
    };

    PassRefPtr<WebCore::Frame>
    webkit_web_frame_init_with_web_view(WebKitWebView*, WebCore::HTMLFrameOwnerElement*);

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

    void
    webkit_web_inspector_set_inspector_client(WebKitWebInspector*, WebCore::Page*);

    void
    webkit_web_inspector_set_web_view(WebKitWebInspector *web_inspector, WebKitWebView *web_view);

    void
    webkit_web_inspector_set_inspected_uri(WebKitWebInspector* web_inspector, const gchar* inspected_uri);

    WebKitWebWindowFeatures*
    webkit_web_window_features_new_from_core_features (const WebCore::WindowFeatures& features);

    void
    webkit_web_view_notify_ready (WebKitWebView* web_view);

    void
    webkit_web_view_request_download(WebKitWebView* web_view, WebKitNetworkRequest* request, const WebCore::ResourceResponse& response = WebCore::ResourceResponse());

    void
    webkit_download_set_suggested_filename(WebKitDownload* download, const gchar* suggestedFilename);

    WebKitWebPolicyDecision*
    webkit_web_policy_decision_new (WebKitWebFrame*, WebCore::FramePolicyFunction);

    void
    webkit_web_policy_decision_cancel (WebKitWebPolicyDecision* decision);

    WebKitNetworkRequest*
    webkit_network_request_new_with_core_request(const WebCore::ResourceRequest& resourceRequest);

    // FIXME: move this to webkitnetworkrequest.h once the API is agreed upon.
    WEBKIT_API SoupMessage*
    webkit_network_request_get_message(WebKitNetworkRequest* request);

    // FIXME: move this functionality into a 'WebKitWebDataSource' once implemented
    WEBKIT_API gchar*
    webkit_web_frame_get_response_mime_type(WebKitWebFrame* frame);

    // FIXME: Move these to webkitwebframe.h once their API has been discussed.

    WEBKIT_API GSList*
    webkit_web_frame_get_children (WebKitWebFrame* frame);

    WEBKIT_API gchar*
    webkit_web_frame_get_inner_text (WebKitWebFrame* frame);

    WEBKIT_API gchar*
    webkit_web_frame_dump_render_tree (WebKitWebFrame* frame);

    WEBKIT_API bool
    webkit_web_frame_pause_animation(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);

    WEBKIT_API bool
    webkit_web_frame_pause_transition(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);

    WEBKIT_API unsigned int
    webkit_web_frame_number_of_active_animations(WebKitWebFrame* frame);

    WEBKIT_API void
    webkit_web_frame_clear_main_frame_name(WebKitWebFrame* frame);

    WEBKIT_API gchar*
    webkit_web_view_get_selected_text (WebKitWebView* web_view);

    WEBKIT_API void
    webkit_web_settings_add_extra_plugin_directory (WebKitWebView *web_view, const gchar* directory);

    GSList*
    webkit_web_settings_get_spell_languages(WebKitWebView* web_view);

    bool
    webkit_web_view_use_primary_for_paste(WebKitWebView* web_view);

    GHashTable*
    webkit_history_items(void);
}

#endif
