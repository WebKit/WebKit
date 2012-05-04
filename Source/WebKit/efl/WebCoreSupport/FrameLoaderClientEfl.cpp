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
#include "ResourceResponse.h"
#include "Settings.h"
#include "WebKitVersion.h"
#include "ewk_logging.h"
#include "ewk_private.h"
#include <Ecore_Evas.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

using namespace WebCore;

namespace WebCore {

FrameLoaderClientEfl::FrameLoaderClientEfl(Evas_Object* view)
    : m_view(view)
    , m_frame(0)
    , m_userAgent("")
    , m_customUserAgent("")
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
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
    if (!m_pluginView) {
        loader->commitData(data, length);

        // Let the media document handle the rest of loading itself, cancel here.
        Frame* coreFrame = loader->frame();
        if (coreFrame && coreFrame->document()->isMediaDocument())
            loader->cancelMainResourceLoad(pluginWillHandleLoadError(loader->response()));
    }

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
    CString firstParty = coreRequest.firstPartyForCookies().string().utf8();
    CString httpMethod = coreRequest.httpMethod().utf8();
    DBG("Resource url=%s, first_party=%s, http_method=%s", url.data(), firstParty.data(), httpMethod.data());

    // We want to distinguish between a request for a document to be loaded into
    // the main frame, a sub-frame, or the sub-objects in that document.
    bool isMainFrameRequest = false;
    if (loader) {
            const FrameLoader* frameLoader = loader->frameLoader();
            isMainFrameRequest = (loader == frameLoader->provisionalDocumentLoader() && frameLoader->isLoadingMainFrame());
    }

    Ewk_Frame_Resource_Request request = { 0, firstParty.data(), httpMethod.data(), identifier, m_frame, isMainFrameRequest };
    Ewk_Frame_Resource_Request orig = request; /* Initialize const fields. */

    orig.url = request.url = url.data();

    Ewk_Frame_Resource_Response* redirectResponse;
    Ewk_Frame_Resource_Response responseBuffer;
    CString redirectUrl, mimeType;

    if (coreResponse.isNull())
        redirectResponse = 0;
    else {
        redirectUrl = coreResponse.url().string().utf8();
        mimeType = coreResponse.mimeType().utf8();
        responseBuffer.url = redirectUrl.data();
        responseBuffer.status_code = coreResponse.httpStatusCode();
        responseBuffer.identifier = identifier;
        responseBuffer.mime_type = mimeType.data();
        redirectResponse = &responseBuffer;
    }

    Ewk_Frame_Resource_Messages messages = { &request, redirectResponse };
    ewk_frame_request_will_send(m_frame, &messages);
    evas_object_smart_callback_call(m_view, "resource,request,willsend", &messages);

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

void FrameLoaderClientEfl::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& coreRequest)
{
    CString url = coreRequest.url().string().utf8();
    CString firstParty = coreRequest.firstPartyForCookies().string().utf8();
    CString httpMethod = coreRequest.httpMethod().utf8();
    DBG("Resource url=%s, First party=%s, HTTP method=%s", url.data(), firstParty.data(), httpMethod.data());

    bool isMainFrameRequest = false;
    if (loader) {
            const FrameLoader* frameLoader = loader->frameLoader();
            isMainFrameRequest = (loader == frameLoader->provisionalDocumentLoader() && frameLoader->isLoadingMainFrame());
    }

    Ewk_Frame_Resource_Request request = { url.data(), firstParty.data(), httpMethod.data(), identifier, m_frame, isMainFrameRequest };
    ewk_frame_request_assign_identifier(m_frame, &request);
    evas_object_smart_callback_call(m_view, "resource,request,new", &request);
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
    notImplemented();
}

void FrameLoaderClientEfl::frameLoaderDestroyed()
{
    if (m_frame)
        ewk_frame_core_gone(m_frame);
    m_frame = 0;

    delete this;
}

void FrameLoaderClientEfl::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& coreResponse)
{
    // Update our knowledge of request soup flags - some are only set
    // after the request is done.
    loader->request().setSoupMessageFlags(coreResponse.soupMessageFlags());

    m_response = coreResponse;

    CString mimeType = coreResponse.mimeType().utf8();
    Ewk_Frame_Resource_Response response = { 0, coreResponse.httpStatusCode(), identifier, mimeType.data() };
    CString url = coreResponse.url().string().utf8();
    response.url = url.data();

    ewk_frame_response_received(m_frame, &response);
    evas_object_smart_callback_call(m_view, "resource,response,received", &response);
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
    CString url = resourceRequest.url().string().utf8();
    CString firstParty = resourceRequest.firstPartyForCookies().string().utf8();
    CString httpMethod = resourceRequest.httpMethod().utf8();
    Ewk_Frame_Resource_Request request = { url.data(), firstParty.data(), httpMethod.data(), 0, m_frame, false };
    bool ret = ewk_view_navigation_policy_decision(m_view, &request);

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
    ewk_frame_load_finished(m_frame, 0, 0, 0, 0, 0);
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
    ewk_frame_mixed_content_displayed_set(m_frame, true);
}

void FrameLoaderClientEfl::didRunInsecureContent(SecurityOrigin*, const KURL&)
{
    ewk_frame_mixed_content_run_set(m_frame, true);
}

void FrameLoaderClientEfl::didDetectXSS(const KURL& insecureURL, bool didBlockEntirePage)
{
    CString cs = insecureURL.string().utf8();
    Ewk_Frame_Xss_Notification xssInfo = { cs.data(), didBlockEntirePage };

    ewk_frame_xss_detected(m_frame, &xssInfo);
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
    ewk_view_onload_event(m_view, m_frame);
}

void FrameLoaderClientEfl::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    ewk_frame_redirect_provisional_load(m_frame);
}

void FrameLoaderClientEfl::dispatchDidCancelClientRedirect()
{
    ewk_frame_redirect_cancelled(m_frame);
}

void FrameLoaderClientEfl::dispatchWillPerformClientRedirect(const KURL& url, double, double)
{
    ewk_frame_redirect_requested(m_frame, url.string().utf8().data());
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
    ewk_frame_load_committed(m_frame);
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
    if (MIMETypeRegistry::isSupportedImageMIMEType(MIMEType)
        || MIMETypeRegistry::isSupportedNonImageMIMEType(MIMEType)
        || MIMETypeRegistry::isSupportedMediaMIMEType(MIMEType))
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

void FrameLoaderClientEfl::finishedLoading(DocumentLoader*)
{
    if (!m_pluginView)
        return;
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
    ewk_frame_load_resource_finished(m_frame, identifier);
    evas_object_smart_callback_call(m_view, "load,resource,finished", &identifier);
}

void FrameLoaderClientEfl::dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError& err)
{
    Ewk_Frame_Load_Error error;
    CString errorDomain = err.domain().utf8();
    CString errorDescription = err.localizedDescription().utf8();
    CString failingUrl = err.failingURL().utf8();

    DBG("ewkFrame=%p, resource=%ld, error=%s (%d, cancellation=%hhu) \"%s\", url=%s",
        m_frame, identifier, errorDomain.data(), err.errorCode(), err.isCancellation(),
        errorDescription.data(), failingUrl.data());

    error.code = err.errorCode();
    error.is_cancellation = err.isCancellation();
    error.domain = errorDomain.data();
    error.description = errorDescription.data();
    error.failing_url = failingUrl.data();
    error.resource_identifier = identifier;
    error.frame = m_frame;

    ewk_frame_load_resource_failed(m_frame, &error);
    evas_object_smart_callback_call(m_view, "load,resource,failed", &error);
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
    ewk_frame_load_error(m_frame,
                         err.domain().utf8().data(),
                         err.errorCode(), err.isCancellation(),
                         err.localizedDescription().utf8().data(),
                         err.failingURL().utf8().data());

    ewk_frame_load_finished(m_frame,
                            err.domain().utf8().data(),
                            err.errorCode(),
                            err.isCancellation(),
                            err.localizedDescription().utf8().data(),
                            err.failingURL().utf8().data());
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
    WebKitErrorPluginWillHandleLoad = 204
};

// Domains used for ResourceError
const char* const NSURLErrorDomain = "NSURLErrorDomain";
const char* const WebKitErrorDomain = "WebKitErrorDomain";

ResourceError FrameLoaderClientEfl::cancelledError(const ResourceRequest& request)
{
    ResourceError error(NSURLErrorDomain, -999, request.url().string(),
                        "Request cancelled");
    error.setIsCancellation(true);
    return error;
}

ResourceError FrameLoaderClientEfl::blockedError(const ResourceRequest& request)
{
    return ResourceError(WebKitErrorDomain, WebKitErrorCannotUseRestrictedPort, request.url().string(),
                         "Request blocked");
}

ResourceError FrameLoaderClientEfl::cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError(WebKitErrorDomain, WebKitErrorCannotShowURL, request.url().string(),
                         "Cannot show URL");
}

ResourceError FrameLoaderClientEfl::interruptedForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError(WebKitErrorDomain, WebKitErrorFrameLoadInterruptedByPolicyChange,
                         request.url().string(), "Frame load interrupted by policy change");
}

ResourceError FrameLoaderClientEfl::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return ResourceError(NSURLErrorDomain, WebKitErrorCannotShowMIMEType, response.url().string(),
                         "Cannot show mimetype");
}

ResourceError FrameLoaderClientEfl::fileDoesNotExistError(const ResourceResponse& response)
{
    return ResourceError(NSURLErrorDomain, -998 /* ### */, response.url().string(),
                         "File does not exist");
}

ResourceError FrameLoaderClientEfl::pluginWillHandleLoadError(const ResourceResponse& response)
{
    return ResourceError(WebKitErrorDomain, WebKitErrorPluginWillHandleLoad, response.url().string(),
                         "Plugin will handle load");
}

bool FrameLoaderClientEfl::shouldFallBack(const ResourceError& error)
{
    return !(error.isCancellation() || error.errorCode() == WebKitErrorFrameLoadInterruptedByPolicyChange || error.errorCode() == WebKitErrorPluginWillHandleLoad);
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

    if (m_frame == ewk_view_frame_main_get(m_view)) {
        ewk_view_frame_view_creation_notify(m_view);
        ewk_view_frame_main_cleared(m_view);
    }
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
    return FrameNetworkingContextEfl::create(EWKPrivate::coreFrame(m_frame), m_frame);
}

}
