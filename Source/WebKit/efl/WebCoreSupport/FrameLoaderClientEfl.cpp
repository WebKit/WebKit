/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2008 Kenneth Rohde Christiansen
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLoaderClientEfl.h"

#include "APICast.h"
#include "DocumentLoader.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameNetworkingContextEfl.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PluginDatabase.h"
#include "ProgressTracker.h"
#include "RenderPart.h"
#include "ResourceRequest.h"
#include "Settings.h"
#include "WebKitVersion.h"
#include "ewk_logging.h"
#include "ewk_private.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>
#include <Ecore_Evas.h>

using namespace WebCore;

namespace WebCore {

FrameLoaderClientEfl::FrameLoaderClientEfl(Evas_Object* view)
    : m_view(view)
    , m_frame(0)
    , m_userAgent("")
    , m_customUserAgent("")
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
    , m_hasRepresentation(false)
{
}

static String composeUserAgent()
{
    return String(ewk_settings_default_user_agent_get());
}

void FrameLoaderClientEfl::setCustomUserAgent(const String& agent)
{
    m_customUserAgent = agent;
}

const String& FrameLoaderClientEfl::customUserAgent() const
{
    return m_customUserAgent;
}

String FrameLoaderClientEfl::userAgent(const KURL&)
{
    if (!m_customUserAgent.isEmpty())
        return m_customUserAgent;

    if (m_userAgent.isEmpty())
        m_userAgent = composeUserAgent();
    return m_userAgent;
}

void FrameLoaderClientEfl::callPolicyFunction(FramePolicyFunction function, PolicyAction action)
{
    Frame* f = EWKPrivate::coreFrame(m_frame);
    ASSERT(f);
    (f->loader()->policyChecker()->*function)(action);
}

WTF::PassRefPtr<DocumentLoader> FrameLoaderClientEfl::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<DocumentLoader> loader = DocumentLoader::create(request, substituteData);
    if (substituteData.isValid())
        loader->setDeferMainResourceDataLoad(false);
    return loader.release();
}

void FrameLoaderClientEfl::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState>)
{
    // FIXME: This is surely too simple
    ASSERT(function);
    callPolicyFunction(function, PolicyUse);
}

void FrameLoaderClientEfl::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    if (!m_pluginView)
        loader->commitData(data, length);

    // We re-check here as the plugin can have been created
    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            m_hasSentResponseToPlugin = true;
        }
        m_pluginView->didReceiveData(data, length);
    }
}

void FrameLoaderClientEfl::dispatchDidReplaceStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidPushStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidPopStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& coreRequest, const ResourceResponse& coreResponse)
{
    CString url = coreRequest.url().string().utf8();
    DBG("Resource url=%s", url.data());

    Ewk_Frame_Resource_Request request = { 0, identifier };
    Ewk_Frame_Resource_Request orig = request; /* Initialize const fields. */

    orig.url = request.url = url.data();

    ewk_frame_request_will_send(m_frame, &request);

    if (request.url != orig.url) {
        coreRequest.setURL(KURL(KURL(), request.url));

        // Calling client might have changed our url pointer.
        // Free the new allocated string.
        free(const_cast<char*>(request.url));
    }
}

bool FrameLoaderClientEfl::shouldUseCredentialStorage(DocumentLoader*, unsigned long)
{
    notImplemented();
    return false;
}

void FrameLoaderClientEfl::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest& coreRequest)
{
    CString url = coreRequest.url().string().utf8();
    DBG("Resource url=%s", url.data());

    Ewk_Frame_Resource_Request request = { 0, identifier };
    ewk_frame_request_assign_identifier(m_frame, &request);
}

void FrameLoaderClientEfl::postProgressStartedNotification()
{
    ewk_frame_load_started(m_frame);
    postProgressEstimateChangedNotification();
}

void FrameLoaderClientEfl::postProgressEstimateChangedNotification()
{
    ewk_frame_load_progress_changed(m_frame);
}

void FrameLoaderClientEfl::postProgressFinishedNotification()
{
    if (m_loadError.isNull())
        ewk_frame_load_finished(m_frame, 0, 0, 0, 0, 0);
    else {
        ewk_frame_load_finished(m_frame,
                                m_loadError.domain().utf8().data(),
                                m_loadError.errorCode(),
                                m_loadError.isCancellation(),
                                m_loadError.localizedDescription().utf8().data(),
                                m_loadError.failingURL().utf8().data());
    }
}

void FrameLoaderClientEfl::frameLoaderDestroyed()
{
    if (m_frame)
        ewk_frame_core_gone(m_frame);
    m_frame = 0;

    delete this;
}

void FrameLoaderClientEfl::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long, const ResourceResponse& response)
{
#if USE(SOUP)
    // Update our knowledge of request soup flags - some are only set
    // after the request is done.
    loader->request().setSoupMessageFlags(response.soupMessageFlags());
#else
    UNUSED_PARAM(loader);
#endif

    m_response = response;
}

void FrameLoaderClientEfl::dispatchDecidePolicyForResponse(FramePolicyFunction function, const ResourceResponse& response, const ResourceRequest& resourceRequest)
{
    // we need to call directly here (currently callPolicyFunction does that!)
    ASSERT(function);

    if (resourceRequest.isNull()) {
        callPolicyFunction(function, PolicyIgnore);
        return;
    }

    if (canShowMIMEType(response.mimeType()))
        callPolicyFunction(function, PolicyUse);
    else
        callPolicyFunction(function, PolicyDownload);
}

void FrameLoaderClientEfl::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& resourceRequest, PassRefPtr<FormState>, const String&)
{
    ASSERT(function);
    ASSERT(m_frame);

    if (resourceRequest.isNull()) {
        callPolicyFunction(function, PolicyIgnore);
        return;
    }

    // if not acceptNavigationRequest - look at Qt -> PolicyIgnore;
    // FIXME: do proper check and only reset forms when on PolicyIgnore
    Frame* f = EWKPrivate::coreFrame(m_frame);
    f->loader()->resetMultipleFormSubmissionProtection();
    callPolicyFunction(function, PolicyUse);
}

void FrameLoaderClientEfl::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& resourceRequest, PassRefPtr<FormState>)
{
    ASSERT(function);
    ASSERT(m_frame);

    if (resourceRequest.isNull()) {
        callPolicyFunction(function, PolicyIgnore);
        return;
    }

    // if not acceptNavigationRequest - look at Qt -> PolicyIgnore;
    // FIXME: do proper check and only reset forms when on PolicyIgnore
    char* url = strdup(resourceRequest.url().string().utf8().data());
    Ewk_Frame_Resource_Request request = { url, 0 };
    bool ret = ewk_view_navigation_policy_decision(m_view, &request);
    free(url);

    PolicyAction policy;
    if (!ret)
        policy = PolicyIgnore;
    else {
        if (action.type() == NavigationTypeFormSubmitted || action.type() == NavigationTypeFormResubmitted) {
            Frame* f = EWKPrivate::coreFrame(m_frame);
            f->loader()->resetMultipleFormSubmissionProtection();
        }
        policy = PolicyUse;
    }
    callPolicyFunction(function, policy);
}

PassRefPtr<Widget> FrameLoaderClientEfl::createPlugin(const IntSize& pluginSize, HTMLPlugInElement* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    ASSERT(m_frame);
    ASSERT(m_view);

    return ewk_view_plugin_create(m_view, m_frame, pluginSize,
                                  element, url, paramNames, paramValues,
                                  mimeType, loadManually);
}

PassRefPtr<Frame> FrameLoaderClientEfl::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    ASSERT(m_frame);
    ASSERT(m_view);

    return ewk_view_frame_create(m_view, m_frame, name, ownerElement, url, referrer);
}

void FrameLoaderClientEfl::didTransferChildFrameToNewDocument(Page*)
{
    ASSERT(m_frame);

    Frame* currentFrame = EWKPrivate::coreFrame(m_frame);
    Evas_Object* currentView = ewk_frame_view_get(m_frame);
    Frame* parentFrame = currentFrame->tree()->parent();

    FrameLoaderClientEfl* client = static_cast<FrameLoaderClientEfl*>(parentFrame->loader()->client());
    Evas_Object* clientFrame = client ? client->webFrame() : 0;
    Evas_Object* clientView = ewk_frame_view_get(clientFrame);

    if (currentView != clientView) {
        ewk_frame_view_set(m_frame, clientView);
        m_view = clientView;
    }

    ASSERT(ewk_view_core_page_get(ewk_frame_view_get(m_frame)) == currentFrame->page());
}

void FrameLoaderClientEfl::transferLoadingResourceFromPage(ResourceLoader*, const ResourceRequest&, Page*)
{
}

void FrameLoaderClientEfl::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    m_hasSentResponseToPlugin = false;
}

PassRefPtr<Widget> FrameLoaderClientEfl::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL,
                                                                const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoaderClientEfl::objectContentType(const KURL& url, const String& mimeType, bool shouldPreferPlugInsForImages)
{
    // FIXME: once plugin support is enabled, this method needs to correctly handle the 'shouldPreferPlugInsForImages' flag. See
    // WebCore::FrameLoader::defaultObjectContentType() for an example.
    UNUSED_PARAM(shouldPreferPlugInsForImages);

    if (url.isEmpty() && mimeType.isEmpty())
        return ObjectContentNone;

    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure
    String type = mimeType;
    if (type.isEmpty())
        type = MIMETypeRegistry::getMIMETypeForExtension(url.path().substring(url.path().reverseFind('.') + 1));

    if (type.isEmpty())
        return ObjectContentFrame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return ObjectContentImage;

#if 0 // PluginDatabase is disabled until we have Plugin system done.
    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return ObjectContentNetscapePlugin;
#endif

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return ObjectContentFrame;

    if (url.protocol() == "about")
        return ObjectContentFrame;

    return ObjectContentNone;
}

String FrameLoaderClientEfl::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClientEfl::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    Frame* coreFrame = EWKPrivate::coreFrame(m_frame);
    ASSERT(coreFrame);

    Settings* settings = coreFrame->settings();
    if (!settings || !settings->isScriptEnabled())
        return;

    Ewk_Window_Object_Cleared_Event event;
    event.context = toGlobalRef(coreFrame->script()->globalObject(mainThreadNormalWorld())->globalExec());
    event.windowObject = toRef(coreFrame->script()->globalObject(mainThreadNormalWorld()));
    event.frame = m_frame;

    evas_object_smart_callback_call(m_view, "window,object,cleared", &event);

#if ENABLE(NETSCAPE_PLUGIN_API)
    ewk_view_js_window_object_clear(m_view, m_frame);
#endif
}

void FrameLoaderClientEfl::documentElementAvailable()
{
    return;
}

void FrameLoaderClientEfl::didPerformFirstNavigation() const
{
    ewk_frame_did_perform_first_navigation(m_frame);
}

void FrameLoaderClientEfl::registerForIconNotification(bool)
{
    notImplemented();
}

void FrameLoaderClientEfl::setMainFrameDocumentReady(bool)
{
    // this is only interesting once we provide an external API for the DOM
}

bool FrameLoaderClientEfl::hasWebView() const
{
    if (!m_view)
        return false;

    return true;
}

bool FrameLoaderClientEfl::hasFrameView() const
{
    notImplemented();
    return true;
}

void FrameLoaderClientEfl::dispatchDidFinishLoad()
{
    m_loadError = ResourceError(); /* clears previous error */
}

void FrameLoaderClientEfl::frameLoadCompleted()
{
    // Note: Can be called multiple times.
}

void FrameLoaderClientEfl::saveViewStateToItem(HistoryItem* item)
{
    ewk_frame_view_state_save(m_frame, item);
}

void FrameLoaderClientEfl::restoreViewState()
{
    ASSERT(m_frame);
    ASSERT(m_view);

    ewk_view_restore_state(m_view, m_frame);
}

void FrameLoaderClientEfl::updateGlobalHistoryRedirectLinks()
{
}

bool FrameLoaderClientEfl::shouldGoToHistoryItem(HistoryItem* item) const
{
    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return item;
}

bool FrameLoaderClientEfl::shouldStopLoadingForHistoryItem(HistoryItem* item) const
{
    return true;
}

void FrameLoaderClientEfl::didDisplayInsecureContent()
{
    notImplemented();
}

void FrameLoaderClientEfl::didRunInsecureContent(SecurityOrigin*, const KURL&)
{
    notImplemented();
}

void FrameLoaderClientEfl::didDetectXSS(const KURL&, bool)
{
    notImplemented();
}

void FrameLoaderClientEfl::makeRepresentation(DocumentLoader*)
{
    m_hasRepresentation = true;
}

void FrameLoaderClientEfl::forceLayout()
{
    ewk_frame_force_layout(m_frame);
}

void FrameLoaderClientEfl::forceLayoutForNonHTML()
{
}

void FrameLoaderClientEfl::setCopiesOnScroll()
{
    // apparently mac specific (Qt comment)
}

void FrameLoaderClientEfl::detachedFromParent2()
{
}

void FrameLoaderClientEfl::detachedFromParent3()
{
}

void FrameLoaderClientEfl::loadedFromCachedPage()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidHandleOnloadEvents()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidCancelClientRedirect()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchWillPerformClientRedirect(const KURL&, double, double)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidChangeLocationWithinPage()
{
    ewk_frame_uri_changed(m_frame);

    if (ewk_view_frame_main_get(m_view) != m_frame)
        return;
    ewk_view_uri_changed(m_view);
}

void FrameLoaderClientEfl::dispatchWillClose()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidReceiveIcon()
{
    /* report received favicon only for main frame. */
    if (ewk_view_frame_main_get(m_view) != m_frame)
        return;

    ewk_view_frame_main_icon_received(m_view);
}

void FrameLoaderClientEfl::dispatchDidStartProvisionalLoad()
{
    ewk_frame_load_provisional(m_frame);
    if (ewk_view_frame_main_get(m_view) == m_frame)
        ewk_view_load_provisional(m_view);
}

void FrameLoaderClientEfl::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    // FIXME: use direction of title.
    CString cs = title.string().utf8();
    ewk_frame_title_set(m_frame, cs.data());

    if (ewk_view_frame_main_get(m_view) != m_frame)
        return;
    ewk_view_title_set(m_view, cs.data());
}

void FrameLoaderClientEfl::dispatchDidChangeIcons(WebCore::IconType)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidCommitLoad()
{
    ewk_frame_uri_changed(m_frame);
    if (ewk_view_frame_main_get(m_view) != m_frame)
        return;
    ewk_view_title_set(m_view, 0);
    ewk_view_uri_changed(m_view);
}

void FrameLoaderClientEfl::dispatchDidFinishDocumentLoad()
{
    ewk_frame_load_document_finished(m_frame);
}

void FrameLoaderClientEfl::dispatchDidFirstLayout()
{
    ewk_frame_load_firstlayout_finished(m_frame);
}

void FrameLoaderClientEfl::dispatchDidFirstVisuallyNonEmptyLayout()
{
    ewk_frame_load_firstlayout_nonempty_finished(m_frame);
}

void FrameLoaderClientEfl::dispatchShow()
{
    ewk_view_load_show(m_view);
}

void FrameLoaderClientEfl::cancelPolicyCheck()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientEfl::revertToProvisionalState(DocumentLoader*)
{
    m_hasRepresentation = true;
}

void FrameLoaderClientEfl::willChangeTitle(DocumentLoader*)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}

void FrameLoaderClientEfl::didChangeTitle(DocumentLoader*)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}

bool FrameLoaderClientEfl::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool FrameLoaderClientEfl::canShowMIMETypeAsHTML(const String& MIMEType) const
{
    notImplemented();
    return false;
}

bool FrameLoaderClientEfl::canShowMIMEType(const String& MIMEType) const
{
    if (MIMETypeRegistry::isSupportedImageMIMEType(MIMEType))
        return true;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(MIMEType))
        return true;

#if 0 // PluginDatabase is disabled until we have Plugin system done.
    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(MIMEType))
        return true;
#endif

    return false;
}

bool FrameLoaderClientEfl::representationExistsForURLScheme(const String&) const
{
    return false;
}

String FrameLoaderClientEfl::generatedMIMETypeForURLScheme(const String&) const
{
    notImplemented();
    return String();
}

void FrameLoaderClientEfl::finishedLoading(DocumentLoader* documentLoader)
{
    if (!m_pluginView) {
        if (m_hasRepresentation)
            documentLoader->writer()->setEncoding("", false);
        return;
    }
    m_pluginView->didFinishLoading();
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}


void FrameLoaderClientEfl::provisionalLoadStarted()
{
    notImplemented();
}

void FrameLoaderClientEfl::didFinishLoad()
{
    notImplemented();
}

void FrameLoaderClientEfl::prepareForDataSourceReplacement()
{
    notImplemented();
}

void FrameLoaderClientEfl::setTitle(const StringWithDirection& title, const KURL& url)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}

void FrameLoaderClientEfl::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int dataLength)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& err)
{
    notImplemented();
}

bool FrameLoaderClientEfl::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length)
{
    notImplemented();
    return false;
}

void FrameLoaderClientEfl::dispatchDidLoadResourceByXMLHttpRequest(unsigned long, const String&)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidFailProvisionalLoad(const ResourceError& err)
{
    dispatchDidFailLoad(err);
}

void FrameLoaderClientEfl::dispatchDidFailLoad(const ResourceError& err)
{
    if (!shouldFallBack(err))
        return;

    m_loadError = err;
    ewk_frame_load_error(m_frame,
                         m_loadError.domain().utf8().data(),
                         m_loadError.errorCode(), m_loadError.isCancellation(),
                         m_loadError.localizedDescription().utf8().data(),
                         m_loadError.failingURL().utf8().data());
}

void FrameLoaderClientEfl::download(ResourceHandle*, const ResourceRequest& request, const ResourceResponse&)
{
    if (!m_view)
        return;

    CString url = request.url().string().utf8();
    Ewk_Download download;

    download.url = url.data();
    ewk_view_download_request(m_view, &download);
}

// copied from WebKit/Misc/WebKitErrors[Private].h
enum {
    WebKitErrorCannotShowMIMEType = 100,
    WebKitErrorCannotShowURL = 101,
    WebKitErrorFrameLoadInterruptedByPolicyChange = 102,
    WebKitErrorCannotUseRestrictedPort = 103,
    WebKitErrorCannotFindPlugIn = 200,
    WebKitErrorCannotLoadPlugIn = 201,
    WebKitErrorJavaUnavailable = 202,
};

ResourceError FrameLoaderClientEfl::cancelledError(const ResourceRequest& request)
{
    ResourceError error("Error", -999, request.url().string(),
                        "Request cancelled");
    error.setIsCancellation(true);
    return error;
}

ResourceError FrameLoaderClientEfl::blockedError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorCannotUseRestrictedPort, request.url().string(),
                         "Request blocked");
}

ResourceError FrameLoaderClientEfl::cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorCannotShowURL, request.url().string(),
                         "Cannot show URL");
}

ResourceError FrameLoaderClientEfl::interruptedForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorFrameLoadInterruptedByPolicyChange,
                         request.url().string(), "Frame load interrupted by policy change");
}

ResourceError FrameLoaderClientEfl::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return ResourceError("Error", WebKitErrorCannotShowMIMEType, response.url().string(),
                         "Cannot show mimetype");
}

ResourceError FrameLoaderClientEfl::fileDoesNotExistError(const ResourceResponse& response)
{
    return ResourceError("Error", -998 /* ### */, response.url().string(),
                         "File does not exist");
}

ResourceError FrameLoaderClientEfl::pluginWillHandleLoadError(const ResourceResponse&)
{
    notImplemented();
    return ResourceError("Error", 0, "", "");
}

bool FrameLoaderClientEfl::shouldFallBack(const ResourceError& error)
{
    return !(error.isCancellation() || (error.errorCode() == WebKitErrorFrameLoadInterruptedByPolicyChange));
}

bool FrameLoaderClientEfl::canCachePage() const
{
    return false;
}

Frame* FrameLoaderClientEfl::dispatchCreatePage(const NavigationAction&)
{
    if (!m_view)
        return 0;

    Evas_Object* newView = ewk_view_window_create(m_view, EINA_FALSE, 0);
    Evas_Object* mainFrame;
    if (!newView)
        mainFrame = m_frame;
    else
        mainFrame = ewk_view_frame_main_get(newView);

    return EWKPrivate::coreFrame(mainFrame);
}

void FrameLoaderClientEfl::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientEfl::setMainDocumentError(DocumentLoader* loader, const ResourceError& error)
{
    if (!m_pluginView)
        return;
    m_pluginView->didFail(error);
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

void FrameLoaderClientEfl::startDownload(const ResourceRequest& request, const String& /* suggestedName */)
{
    if (!m_view)
        return;

    CString url = request.url().string().utf8();
    Ewk_Download download;

    download.url = url.data();
    ewk_view_download_request(m_view, &download);
}

void FrameLoaderClientEfl::updateGlobalHistory()
{
    notImplemented();
}

void FrameLoaderClientEfl::savePlatformDataToCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientEfl::transitionToCommittedFromCachedFrame(CachedFrame*)
{
}

void FrameLoaderClientEfl::transitionToCommittedForNewPage()
{
    ASSERT(m_frame);
    ASSERT(m_view);

    ewk_frame_view_create_for_view(m_frame, m_view);

    if (m_frame == ewk_view_frame_main_get(m_view))
        ewk_view_frame_main_cleared(m_view);
}

void FrameLoaderClientEfl::didSaveToPageCache()
{
}

void FrameLoaderClientEfl::didRestoreFromPageCache()
{
}

void FrameLoaderClientEfl::dispatchDidBecomeFrameset(bool)
{
}

PassRefPtr<FrameNetworkingContext> FrameLoaderClientEfl::createNetworkingContext()
{
    return FrameNetworkingContextEfl::create(EWKPrivate::coreFrame(m_frame));
}

}
