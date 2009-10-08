/*
 *  Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 *  Copyright (C) 2007, 2008, 2009 Holger Hans Peter Freyther
 *  Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>
 *  Copyright (C) 2008, 2009 Collabora Ltd.  All rights reserved.
 *  Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "FrameLoaderClientGtk.h"

#include "ArchiveResource.h"
#include "Color.h"
#include "DocumentLoader.h"
#include "DocumentLoaderGtk.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "FrameTree.h"
#include "GOwnPtr.h"
#include "GtkPluginWidget.h"
#include "HTMLAppletElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSDOMWindow.h"
#include "Language.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "PluginDatabase.h"
#include "RenderPart.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "CString.h"
#include "ProgressTracker.h"
#include "JSDOMBinding.h"
#include "ScriptController.h"
#include "webkiterror.h"
#include "webkitnetworkrequest.h"
#include "webkitnetworkresponse.h"
#include "webkitprivate.h"
#include "webkitwebframe.h"
#include "webkitwebnavigationaction.h"
#include "webkitwebpolicydecision.h"
#include "webkitwebview.h"

#include <JavaScriptCore/APICast.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>

using namespace WebCore;

namespace WebKit {

FrameLoaderClient::FrameLoaderClient(WebKitWebFrame* frame)
    : m_frame(frame)
    , m_policyDecision(0)
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
{
    ASSERT(m_frame);
}

FrameLoaderClient::~FrameLoaderClient()
{
    if (m_policyDecision)
        g_object_unref(m_policyDecision);
}

String FrameLoaderClient::userAgent(const KURL&)
{
    WebKitWebSettings* settings = webkit_web_view_get_settings(getViewFromFrame(m_frame));
    return String::fromUTF8(webkit_web_settings_get_user_agent(settings));
}

static void notifyStatus(WebKitWebFrame* frame, WebKitLoadStatus loadStatus)
{
    frame->priv->loadStatus = loadStatus;
    g_object_notify(G_OBJECT(frame), "load-status");

    WebKitWebView* webView = getViewFromFrame(frame);
    if (frame == webkit_web_view_get_main_frame(webView)) {
        webView->priv->loadStatus = loadStatus;
        g_object_notify(G_OBJECT(webView), "load-status");
    }
}

static void loadDone(WebKitWebFrame* frame, bool didSucceed)
{
    // FIXME: load-done is deprecated. Please remove when signal's been removed.
    g_signal_emit_by_name(frame, "load-done", didSucceed);
    notifyStatus(frame, WEBKIT_LOAD_FINISHED);
}

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClient::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<WebKit::DocumentLoader> loader = WebKit::DocumentLoader::create(request, substituteData);

    WebKitWebDataSource* webDataSource = webkit_web_data_source_new_with_loader(loader.get());
    loader->setDataSource(webDataSource);
    g_object_unref(webDataSource);

    return loader.release();
}

void FrameLoaderClient::dispatchWillSubmitForm(FramePolicyFunction policyFunction, PassRefPtr<FormState>)
{
    // FIXME: This is surely too simple
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
    (core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyUse);
}


void FrameLoaderClient::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    if (!m_pluginView) {
        ASSERT(loader->frame());
        // Setting the encoding on the frame loader is our way to get work done that is normally done
        // when the first bit of data is received, even for the case of a document with no data (like about:blank).
        String encoding = loader->overrideEncoding();
        bool userChosen = !encoding.isNull();
        if (!userChosen)
            encoding = loader->response().textEncodingName();

        FrameLoader* frameLoader = loader->frameLoader();
        frameLoader->setEncoding(encoding, userChosen);
        if (data)
            frameLoader->addData(data, length);
    }

    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            m_hasSentResponseToPlugin = true;
        }

        // FIXME: We may want to investigate refactoring our plugin loading
        // code to be similar to mac's.
        // Also, see http://trac.webkit.org/changeset/24118.
        if (!m_pluginView)
            return;

        m_pluginView->didReceiveData(data, length);
    }
}

bool
FrameLoaderClient::shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long  identifier)
{
    notImplemented();
    return false;
}

void FrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidCancelAuthenticationChallenge(WebCore::DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

// We convert this to string because it's easier to use strings as
// keys in a GHashTable.
static char* toString(unsigned long identifier)
{
    return g_strdup_printf("%ld", identifier);
}

void FrameLoaderClient::dispatchWillSendRequest(WebCore::DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    GOwnPtr<WebKitNetworkResponse> networkResponse(0);

    // We are adding one more resource to the load, or maybe we are
    // just redirecting a load.
    if (redirectResponse.isNull())
        static_cast<WebKit::DocumentLoader*>(loader)->increaseLoadCount(identifier);
    else
        networkResponse.set(webkit_network_response_new_with_core_response(redirectResponse));

    WebKitWebView* webView = getViewFromFrame(m_frame);
    GOwnPtr<gchar> identifierString(toString(identifier));
    WebKitWebResource* webResource = webkit_web_view_get_resource(webView, identifierString.get());
    GOwnPtr<WebKitNetworkRequest> networkRequest(webkit_network_request_new_with_core_request(request));

    if (!redirectResponse.isNull()) {
        // This is a redirect, so we need to update the WebResource's knowledge
        // of the URI.
        g_free(webResource->priv->uri);
        webResource->priv->uri = g_strdup(request.url().string().utf8().data());
    }
    
    g_signal_emit_by_name(webView, "resource-request-starting", m_frame, webResource, networkRequest.get(), networkResponse.get());

    // Feed any changes back into the ResourceRequest object.
    SoupMessage* message = webkit_network_request_get_message(networkRequest.get());
    if (!message) {
        request.setURL(KURL(KURL(), String::fromUTF8(webkit_network_request_get_uri(networkRequest.get()))));
        return;
    }

    request.updateFromSoupMessage(message);
}

void FrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, WebCore::DocumentLoader*, const ResourceRequest& request)
{
    webkit_web_view_add_resource(getViewFromFrame(m_frame), toString(identifier),
                                 WEBKIT_WEB_RESOURCE(g_object_new(WEBKIT_TYPE_WEB_RESOURCE, "uri", request.url().string().utf8().data(), 0)));
}

void FrameLoaderClient::postProgressStartedNotification()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    g_signal_emit_by_name(webView, "load-started", m_frame);

    g_object_notify(G_OBJECT(webView), "progress");
}

void FrameLoaderClient::postProgressEstimateChangedNotification()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    Page* corePage = core(webView);

    g_signal_emit_by_name(webView, "load-progress-changed", lround(corePage->progress()->estimatedProgress()*100));

    g_object_notify(G_OBJECT(webView), "progress");
}

void FrameLoaderClient::postProgressFinishedNotification()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    WebKitWebViewPrivate* privateData = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);

    // We can get a stopLoad() from dispose when the object is being
    // destroyed, don't emit the signal in that case.
    if (!privateData->disposing)
        g_signal_emit_by_name(webView, "load-finished", m_frame);
}

void FrameLoaderClient::frameLoaderDestroyed()
{
    webkit_web_frame_core_frame_gone(m_frame);
    g_object_unref(m_frame);
    m_frame = 0;
    delete this;
}

void FrameLoaderClient::dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long, const ResourceResponse& response)
{
    m_response = response;
}

void FrameLoaderClient::dispatchDecidePolicyForMIMEType(FramePolicyFunction policyFunction, const String& mimeType, const ResourceRequest& resourceRequest)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;

    if (resourceRequest.isNull()) {
        (core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyIgnore);
        return;
    }

    WebKitWebView* page = getViewFromFrame(m_frame);
    WebKitNetworkRequest* request = webkit_network_request_new_with_core_request(resourceRequest);

    WebKitWebPolicyDecision* policyDecision = webkit_web_policy_decision_new(m_frame, policyFunction);
    if (m_policyDecision)
        g_object_unref(m_policyDecision);
    m_policyDecision = policyDecision;

    gboolean isHandled = false;
    g_signal_emit_by_name(page, "mime-type-policy-decision-requested", m_frame, request, mimeType.utf8().data(), policyDecision, &isHandled);

    g_object_unref(request);

    if (isHandled)
        return;

    if (canShowMIMEType(mimeType))
        webkit_web_policy_decision_use (policyDecision);
    else
        webkit_web_policy_decision_ignore (policyDecision);
}

static WebKitWebNavigationAction* getNavigationAction(const NavigationAction& action, const char* targetFrame)
{
    gint button = -1;

    const Event* event = action.event();
    if (event && event->isMouseEvent()) {
        const MouseEvent* mouseEvent = static_cast<const MouseEvent*>(event);
        // DOM button values are 0, 1 and 2 for left, middle and right buttons.
        // GTK+ uses 1, 2 and 3, so let's add 1 to remain consistent.
        button = mouseEvent->button() + 1;
    }

    gint modifierFlags = 0;
    UIEventWithKeyState* keyStateEvent = findEventWithKeyState(const_cast<Event*>(event));
    if (keyStateEvent) {
        if (keyStateEvent->shiftKey())
            modifierFlags |= GDK_SHIFT_MASK;
        if (keyStateEvent->ctrlKey())
            modifierFlags |= GDK_CONTROL_MASK;
        if (keyStateEvent->altKey())
            modifierFlags |= GDK_MOD1_MASK;
        if (keyStateEvent->metaKey())
            modifierFlags |= GDK_MOD2_MASK;
    }

    return WEBKIT_WEB_NAVIGATION_ACTION(g_object_new(WEBKIT_TYPE_WEB_NAVIGATION_ACTION,
                                                     "reason", kit(action.type()),
                                                     "original-uri", action.url().string().utf8().data(),
                                                     "button", button,
                                                     "modifier-state", modifierFlags,
                                                     "target-frame", targetFrame,
                                                     NULL));
}

void FrameLoaderClient::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction policyFunction, const NavigationAction& action, const ResourceRequest& resourceRequest, PassRefPtr<FormState>, const String& frameName)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;

    if (resourceRequest.isNull()) {
        (core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyIgnore);
        return;
    }

    WebKitWebPolicyDecision* policyDecision = webkit_web_policy_decision_new(m_frame, policyFunction);

    if (m_policyDecision)
        g_object_unref(m_policyDecision);
    m_policyDecision = policyDecision;

    WebKitWebView* webView = getViewFromFrame(m_frame);
    WebKitNetworkRequest* request = webkit_network_request_new(resourceRequest.url().string().utf8().data());
    WebKitWebNavigationAction* navigationAction = getNavigationAction(action, frameName.utf8().data());
    gboolean isHandled = false;

    g_signal_emit_by_name(webView, "new-window-policy-decision-requested", m_frame, request, navigationAction, policyDecision, &isHandled);

    g_object_unref(navigationAction);
    g_object_unref(request);

    // FIXME: I think Qt version marshals this to another thread so when we
    // have multi-threaded download, we might need to do the same
    if (!isHandled)
        (core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyUse);
}

void FrameLoaderClient::dispatchDecidePolicyForNavigationAction(FramePolicyFunction policyFunction, const NavigationAction& action, const ResourceRequest& resourceRequest, PassRefPtr<FormState>)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;

    if (resourceRequest.isNull()) {
        (core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyIgnore);
        return;
    }

    WebKitWebView* webView = getViewFromFrame(m_frame);
    WebKitNetworkRequest* request = webkit_network_request_new_with_core_request(resourceRequest);
    WebKitNavigationResponse response;
    /*
     * We still support the deprecated navigation-requested signal, if the
     * application doesn't ignore the navigation then the new signal is
     * emitted.
     * navigation-policy-decision-requested must be emitted after
     * navigation-requested as the policy decision can be async.
     */
    g_signal_emit_by_name(webView, "navigation-requested", m_frame, request, &response);

    if (response == WEBKIT_NAVIGATION_RESPONSE_IGNORE) {
        (core(m_frame)->loader()->policyChecker()->*policyFunction)(PolicyIgnore);
        g_object_unref(request);
        return;
    }

    WebKitWebPolicyDecision* policyDecision = webkit_web_policy_decision_new(m_frame, policyFunction);
    if (m_policyDecision)
        g_object_unref(m_policyDecision);
    m_policyDecision = policyDecision;

    WebKitWebNavigationAction* navigationAction = getNavigationAction(action, NULL);
    gboolean isHandled = false;
    g_signal_emit_by_name(webView, "navigation-policy-decision-requested", m_frame, request, navigationAction, policyDecision, &isHandled);

    g_object_unref(navigationAction);
    g_object_unref(request);

    // FIXME Implement default behavior when we can query the backend what protocols it supports
    if (!isHandled)
        webkit_web_policy_decision_use(m_policyDecision);
}

PassRefPtr<Widget> FrameLoaderClient::createPlugin(const IntSize& pluginSize, HTMLPlugInElement* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    /* Check if we want to embed a GtkWidget, fallback to plugins later */
    CString urlString = url.string().utf8();
    CString mimeTypeString = mimeType.utf8();

    ASSERT(paramNames.size() == paramValues.size());
    GOwnPtr<GHashTable> hash(g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free));
    for (unsigned i = 0; i < paramNames.size(); ++i) {
        g_hash_table_insert(hash.get(),
                            g_strdup(paramNames[i].utf8().data()),
                            g_strdup(paramValues[i].utf8().data()));
    }

    GtkWidget* gtkWidget = 0;
    g_signal_emit_by_name(getViewFromFrame(m_frame), "create-plugin-widget",
                          mimeTypeString.data(), urlString.data(), hash.get(), &gtkWidget);
    if (gtkWidget)
        return adoptRef(new GtkPluginWidget(gtkWidget));

    RefPtr<PluginView> pluginView = PluginView::create(core(m_frame), pluginSize, element, url, paramNames, paramValues, mimeType, loadManually);

    if (pluginView->status() == PluginStatusLoadedSuccessfully)
        return pluginView;

    return 0;
}

PassRefPtr<Frame> FrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                                 const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    Frame* coreFrame = core(m_frame);

    ASSERT(core(getViewFromFrame(m_frame)) == coreFrame->page());

    RefPtr<Frame> childFrame = webkit_web_frame_init_with_web_view(getViewFromFrame(m_frame), ownerElement);

    coreFrame->tree()->appendChild(childFrame);

    childFrame->tree()->setName(name);
    childFrame->init();

    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!childFrame->page())
        return 0;

    childFrame->loader()->loadURLIntoChildFrame(url, referrer, childFrame.get());

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}

void FrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    m_hasSentResponseToPlugin = false;
}

PassRefPtr<Widget> FrameLoaderClient::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL,
                                                  const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoaderClient::objectContentType(const KURL& url, const String& mimeType)
{
    String type = mimeType;
    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure
    if (type.isEmpty())
        type = MIMETypeRegistry::getMIMETypeForExtension(url.path().substring(url.path().reverseFind('.') + 1));

    if (type.isEmpty())
        return WebCore::ObjectContentFrame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return WebCore::ObjectContentImage;

    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return WebCore::ObjectContentNetscapePlugin;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return WebCore::ObjectContentFrame;

    return WebCore::ObjectContentNone;
}

String FrameLoaderClient::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClient::windowObjectCleared()
{
    // Is this obsolete now?
    g_signal_emit_by_name(m_frame, "cleared");

    Frame* coreFrame = core(m_frame);
    ASSERT(coreFrame);

    Settings* settings = coreFrame->settings();
    if (!settings || !settings->isJavaScriptEnabled())
        return;

    // TODO: Consider using g_signal_has_handler_pending() to avoid the overhead
    // when there are no handlers.
    JSGlobalContextRef context = toGlobalRef(coreFrame->script()->globalObject()->globalExec());
    JSObjectRef windowObject = toRef(coreFrame->script()->globalObject());
    ASSERT(windowObject);

    WebKitWebView* webView = getViewFromFrame(m_frame);
    g_signal_emit_by_name(webView, "window-object-cleared", m_frame, context, windowObject);

    // TODO: Re-attach debug clients if present.
    // The Win port has an example of how we might do this.
}

void FrameLoaderClient::documentElementAvailable()
{
}

void FrameLoaderClient::didPerformFirstNavigation() const
{
}

void FrameLoaderClient::registerForIconNotification(bool)
{
    notImplemented();
}

void FrameLoaderClient::setMainFrameDocumentReady(bool)
{
    // this is only interesting once we provide an external API for the DOM
}

bool FrameLoaderClient::hasWebView() const
{
    notImplemented();
    return true;
}

void FrameLoaderClient::dispatchDidFinishLoad()
{
    loadDone(m_frame, true);
}

void FrameLoaderClient::frameLoadCompleted()
{
    notImplemented();
}

void FrameLoaderClient::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClient::restoreViewState()
{
    notImplemented();
}

bool FrameLoaderClient::shouldGoToHistoryItem(HistoryItem* item) const
{
    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return item != 0;
}

void FrameLoaderClient::didDisplayInsecureContent()
{
    notImplemented();
}

void FrameLoaderClient::didRunInsecureContent(SecurityOrigin*)
{
    notImplemented();
}

void FrameLoaderClient::makeRepresentation(WebCore::DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClient::forceLayout()
{
    FrameView* view = core(m_frame)->view();
    if (view)
        view->forceLayout(true);
}

void FrameLoaderClient::forceLayoutForNonHTML()
{
    notImplemented();
}

void FrameLoaderClient::setCopiesOnScroll()
{
    notImplemented();
}

void FrameLoaderClient::detachedFromParent2()
{
    notImplemented();
}

void FrameLoaderClient::detachedFromParent3()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidCancelClientRedirect()
{
    notImplemented();
}

void FrameLoaderClient::dispatchWillPerformClientRedirect(const KURL&, double, double)
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebKitWebFramePrivate* priv = m_frame->priv;
    g_free(priv->uri);
    priv->uri = g_strdup(core(m_frame)->loader()->url().prettyURL().utf8().data());
    g_object_notify(G_OBJECT(m_frame), "uri");
    WebKitWebView* webView = getViewFromFrame(m_frame);
    if (m_frame == webkit_web_view_get_main_frame(webView))
        g_object_notify(G_OBJECT(webView), "uri");
}

void FrameLoaderClient::dispatchWillClose()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidReceiveIcon()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);

    g_signal_emit_by_name(webView, "icon-loaded", m_frame);
}

void FrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    notifyStatus(m_frame, WEBKIT_LOAD_PROVISIONAL);
}

void FrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebKitWebFramePrivate* priv = m_frame->priv;
    g_free(priv->title);
    priv->title = g_strdup(title.utf8().data());

    g_signal_emit_by_name(m_frame, "title-changed", priv->title);
    g_object_notify(G_OBJECT(m_frame), "title");

    WebKitWebView* webView = getViewFromFrame(m_frame);
    if (m_frame == webkit_web_view_get_main_frame(webView)) {
        g_signal_emit_by_name(webView, "title-changed", m_frame, title.utf8().data());
        g_object_notify(G_OBJECT(webView), "title");
    }
}

void FrameLoaderClient::dispatchDidCommitLoad()
{
    /* Update the URI once first data has been received.
     * This means the URI is valid and successfully identify the page that's going to be loaded.
     */
    g_object_freeze_notify(G_OBJECT(m_frame));

    WebKitWebFramePrivate* priv = m_frame->priv;
    g_free(priv->uri);
    priv->uri = g_strdup(core(m_frame)->loader()->url().prettyURL().utf8().data());
    g_free(priv->title);
    priv->title = NULL;
    g_object_notify(G_OBJECT(m_frame), "uri");
    g_object_notify(G_OBJECT(m_frame), "title");

    g_signal_emit_by_name(m_frame, "load-committed");
    notifyStatus(m_frame, WEBKIT_LOAD_COMMITTED);

    WebKitWebView* webView = getViewFromFrame(m_frame);
    if (m_frame == webkit_web_view_get_main_frame(webView)) {
        g_object_freeze_notify(G_OBJECT(webView));
        g_object_notify(G_OBJECT(webView), "uri");
        g_object_notify(G_OBJECT(webView), "title");
        g_object_thaw_notify(G_OBJECT(webView));
        g_signal_emit_by_name(webView, "load-committed", m_frame);
    }

    g_object_thaw_notify(G_OBJECT(m_frame));
}

void FrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidFirstLayout()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidFirstVisuallyNonEmptyLayout()
{
    notifyStatus(m_frame, WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT);
}

void FrameLoaderClient::dispatchShow()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    webkit_web_view_notify_ready(webView);
}

void FrameLoaderClient::cancelPolicyCheck()
{
    //FIXME Add support for more than one policy decision at once
    if (m_policyDecision)
        webkit_web_policy_decision_cancel(m_policyDecision);
}

void FrameLoaderClient::dispatchDidLoadMainResource(WebCore::DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClient::revertToProvisionalState(WebCore::DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClient::willChangeTitle(WebCore::DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClient::didChangeTitle(WebCore::DocumentLoader *l)
{
    setTitle(l->title(), l->url());
}

bool FrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool FrameLoaderClient::canShowMIMEType(const String& type) const
{
    return MIMETypeRegistry::isSupportedImageMIMEType(type) || MIMETypeRegistry::isSupportedNonImageMIMEType(type) ||
        PluginDatabase::installedPlugins()->isMIMETypeRegistered(type);
}

bool FrameLoaderClient::representationExistsForURLScheme(const String&) const
{
    notImplemented();
    return false;
}

String FrameLoaderClient::generatedMIMETypeForURLScheme(const String&) const
{
    notImplemented();
    return String();
}

void FrameLoaderClient::finishedLoading(WebCore::DocumentLoader* documentLoader)
{
    if (!m_pluginView)
        committedLoad(documentLoader, 0, 0);
    else {
        m_pluginView->didFinishLoading();
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}


void FrameLoaderClient::provisionalLoadStarted()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);

    if (m_frame == webkit_web_view_get_main_frame(webView))
        webkit_web_view_clear_resources(webView);
}

void FrameLoaderClient::didFinishLoad() {
    notImplemented();
}

void FrameLoaderClient::prepareForDataSourceReplacement() { notImplemented(); }

void FrameLoaderClient::setTitle(const String& title, const KURL& url)
{
    WebKitWebFramePrivate* frameData = WEBKIT_WEB_FRAME_GET_PRIVATE(m_frame);
    g_free(frameData->title);
    frameData->title = g_strdup(title.utf8().data());
}

void FrameLoaderClient::dispatchDidReceiveContentLength(WebCore::DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidFinishLoading(WebCore::DocumentLoader* loader, unsigned long identifier)
{
    static_cast<WebKit::DocumentLoader*>(loader)->decreaseLoadCount(identifier);

    WebKitWebView* webView = getViewFromFrame(m_frame);
    GOwnPtr<gchar> identifierString(toString(identifier));
    WebKitWebResource* webResource = webkit_web_view_get_resource(webView, identifierString.get());

    const char* uri = webkit_web_resource_get_uri(webResource);
    RefPtr<ArchiveResource> coreResource(loader->subresource(KURL(KURL(), uri)));

    // If coreResource is NULL here, the resource failed to load,
    // unless it's the main resource.
    if (!coreResource && webResource != webkit_web_view_get_main_resource(webView))
        return;

    if (!coreResource)
        coreResource = loader->mainResource().releaseRef();

    webkit_web_resource_init_with_core_resource(webResource, coreResource.get());

    // FIXME: This function should notify the application that the resource
    // finished loading, maybe using a load-status property in the
    // WebKitWebResource object, similar to what we do for WebKitWebFrame'
    // signal.
    notImplemented();
}

void FrameLoaderClient::dispatchDidFailLoading(WebCore::DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    static_cast<WebKit::DocumentLoader*>(loader)->decreaseLoadCount(identifier);

    // FIXME: This function should notify the application that the resource failed
    // loading, maybe a 'load-error' signal in the WebKitWebResource object.
    notImplemented();
}

void FrameLoaderClient::dispatchDidLoadResourceByXMLHttpRequest(unsigned long, const ScriptString&)
{
    notImplemented();
}

bool FrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(WebCore::DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length)
{
    notImplemented();
    return false;
}

void FrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    dispatchDidFailLoad(error);
}

void FrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    notifyStatus(m_frame, WEBKIT_LOAD_FAILED);

    WebKitWebView* webView = getViewFromFrame(m_frame);
    GError* webError = g_error_new_literal(g_quark_from_string(error.domain().utf8().data()),
                                           error.errorCode(),
                                           error.localizedDescription().utf8().data());
    gboolean isHandled = false;
    g_signal_emit_by_name(webView, "load-error", m_frame, error.failingURL().utf8().data(), webError, &isHandled);

    if (isHandled) {
        g_error_free(webError);
        return;
    }

    if (!shouldFallBack(error)) {
        g_error_free(webError);
        return;
    }

    String content;
    gchar* fileContent = 0;
    gchar* errorURI = g_filename_to_uri(DATA_DIR"/webkit-1.0/resources/error.html", NULL, NULL);
    GFile* errorFile = g_file_new_for_uri(errorURI);
    g_free(errorURI);

    if (!errorFile)
        content = String::format("<html><body>%s</body></html>", webError->message);
    else {
        gboolean loaded = g_file_load_contents(errorFile, 0, &fileContent, 0, 0, 0);
        if (!loaded)
            content = String::format("<html><body>%s</body></html>", webError->message);
        else
            content = String::format(fileContent, error.failingURL().utf8().data(), webError->message);
    }

    webkit_web_frame_load_alternate_string(m_frame, content.utf8().data(), 0, error.failingURL().utf8().data());

    g_free(fileContent);

    if (errorFile)
        g_object_unref(errorFile);

    g_error_free(webError);
}

void FrameLoaderClient::download(ResourceHandle* handle, const ResourceRequest& request, const ResourceRequest&, const ResourceResponse& response)
{
    WebKitNetworkRequest* networkRequest = webkit_network_request_new_with_core_request(request);
    WebKitWebView* view = getViewFromFrame(m_frame);

    webkit_web_view_request_download(view, networkRequest, response, handle);
    g_object_unref(networkRequest);
}

ResourceError FrameLoaderClient::cancelledError(const ResourceRequest& request)
{
    return ResourceError(g_quark_to_string(WEBKIT_NETWORK_ERROR), WEBKIT_NETWORK_ERROR_CANCELLED,
                         request.url().string(), _("Load request cancelled"));
}

ResourceError FrameLoaderClient::blockedError(const ResourceRequest& request)
{
    return ResourceError(g_quark_to_string(WEBKIT_POLICY_ERROR), WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT,
                         request.url().string(), _("Not allowed to use restricted network port"));
}

ResourceError FrameLoaderClient::cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError(g_quark_to_string(WEBKIT_POLICY_ERROR), WEBKIT_POLICY_ERROR_CANNOT_SHOW_URL,
                         request.url().string(), _("URL cannot be shown"));
}

ResourceError FrameLoaderClient::interruptForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError(g_quark_to_string(WEBKIT_POLICY_ERROR), WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE,
                         request.url().string(), _("Frame load was interrupted"));
}

ResourceError FrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return ResourceError(g_quark_to_string(WEBKIT_POLICY_ERROR), WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE,
                         response.url().string(), _("Content with the specified MIME type cannot be shown"));
}

ResourceError FrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response)
{
    return ResourceError(g_quark_to_string(WEBKIT_NETWORK_ERROR), WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST,
                         response.url().string(), _("File does not exist"));
}

ResourceError FrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse& response)
{
    return ResourceError(g_quark_to_string(WEBKIT_PLUGIN_ERROR), WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD,
                         response.url().string(), _("Plugin will handle load"));
}

bool FrameLoaderClient::shouldFallBack(const ResourceError& error)
{
    // FIXME: Mac checks for WebKitErrorPlugInWillHandleLoad here to avoid
    // loading plugin content twice. Do we need it?
    return !(error.isCancellation() || error.errorCode() == WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE);
}

bool FrameLoaderClient::canCachePage() const
{
    return true;
}

Frame* FrameLoaderClient::dispatchCreatePage()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    WebKitWebView* newWebView = 0;

    g_signal_emit_by_name(webView, "create-web-view", m_frame, &newWebView);

    if (!newWebView)
        return 0;

    WebKitWebViewPrivate* privateData = WEBKIT_WEB_VIEW_GET_PRIVATE(newWebView);
    return core(privateData->mainFrame);
}

void FrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClient::setMainDocumentError(WebCore::DocumentLoader*, const ResourceError& error)
{
    if (m_pluginView) {
        m_pluginView->didFail(error);
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}

void FrameLoaderClient::startDownload(const ResourceRequest& request)
{
    WebKitNetworkRequest* networkRequest = webkit_network_request_new_with_core_request(request);
    WebKitWebView* view = getViewFromFrame(m_frame);

    webkit_web_view_request_download(view, networkRequest);
    g_object_unref(networkRequest);
}

void FrameLoaderClient::updateGlobalHistory()
{
    notImplemented();
}

void FrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    notImplemented();
}

void FrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*)
{
}

void FrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
}

void FrameLoaderClient::transitionToCommittedForNewPage()
{
    WebKitWebView* containingWindow = getViewFromFrame(m_frame);
    IntSize size = IntSize(GTK_WIDGET(containingWindow)->allocation.width,
                           GTK_WIDGET(containingWindow)->allocation.height);
    bool transparent = webkit_web_view_get_transparent(containingWindow);
    Color backgroundColor = transparent ? WebCore::Color::transparent : WebCore::Color::white;
    Frame* frame = core(m_frame);
    ASSERT(frame);

    frame->createView(size, backgroundColor, transparent, IntSize(), false);

    // We need to do further manipulation on the FrameView if it was the mainFrame
    if (frame != frame->page()->mainFrame())
        return;

    WebKitWebViewPrivate* priv = WEBKIT_WEB_VIEW_GET_PRIVATE(containingWindow);
    frame->view()->setGtkAdjustments(priv->horizontalAdjustment, priv->verticalAdjustment);

    if (priv->currentMenu) {
        GtkMenu* menu = priv->currentMenu;
        priv->currentMenu = NULL;

        gtk_menu_popdown(menu);
        g_object_unref(menu);
    }
}

}
