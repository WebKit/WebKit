/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "DocumentLoader.h"
#include "EWebKit.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "Language.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "PluginDatabase.h"
#include "ProgressTracker.h"
#include "RenderPart.h"
#include "ResourceRequest.h"
#include "ewk_private.h"
#include <wtf/text/CString.h>

#if PLATFORM(UNIX)
#include <sys/utsname.h>
#endif

#include <Ecore_Evas.h>

using namespace WebCore;

namespace WebCore {

FrameLoaderClientEfl::FrameLoaderClientEfl(Evas_Object *view)
    : m_view(view)
    , m_frame(0)
    , m_firstData(false)
    , m_userAgent("")
    , m_customUserAgent("")
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
{
}

static String agentPlatform()
{
    notImplemented();
    return "Unknown";
}

static String agentOS()
{
#if PLATFORM(DARWIN)
#if PLATFORM(X86)
    return "Intel Mac OS X";
#else
    return "PPC Mac OS X";
#endif
#elif PLATFORM(UNIX)
    struct utsname name;
    if (uname(&name) != -1)
        return String::format("%s %s", name.sysname, name.machine);

    return "Unknown";
#elif PLATFORM(WIN_OS)
    // FIXME: Compute the Windows version
    return "Windows";
#else
    notImplemented();
    return "Unknown";
#endif
}

static String composeUserAgent()
{
    // This is a liberal interpretation of http://www.mozilla.org/build/revised-user-agent-strings.html
    // See also http://developer.apple.com/internet/safari/faq.html#anchor2

    String ua;

    // Product
    ua += "Mozilla/5.0";

    // Comment
    ua += " (";
    ua += agentPlatform(); // Platform
    ua += "; U; "; // Security
    ua += agentOS(); // OS-or-CPU
    ua += "; ";
    ua += defaultLanguage(); // Localization information
    ua += ") ";

    // WebKit Product
    // FIXME: The WebKit version is hardcoded
    static const String webKitVersion = "525.1+";
    ua += "AppleWebKit/" + webKitVersion;
    ua += " (KHTML, like Gecko, ";
    // We mention Safari since many broken sites check for it (OmniWeb does this too)
    // We re-use the WebKit version, though it doesn't seem to matter much in practice
    ua += "Safari/" + webKitVersion;
    ua += ") ";

    // Vendor Product
    // ua += g_get_prgname();

    return ua;
}

void FrameLoaderClientEfl::setCustomUserAgent(const String &agent)
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
    Frame* f = ewk_frame_core_get(m_frame);
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
        if (!m_frame)
            return;

        FrameLoader* fl = loader->frameLoader();
        if (m_firstData) {
            fl->writer()->setEncoding(m_response.textEncodingName(), false);
            m_firstData = false;
        }
        fl->addData(data, length);
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

void FrameLoaderClientEfl::dispatchDidRemoveBackForwardItem(WebCore::HistoryItem*) const
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

void FrameLoaderClientEfl::dispatchDidChangeBackForwardIndex() const
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidAddBackForwardItem(WebCore::HistoryItem*) const
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld*)
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

void FrameLoaderClientEfl::dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

bool FrameLoaderClientEfl::shouldUseCredentialStorage(DocumentLoader*, unsigned long)
{
    notImplemented();
    return false;
}

void FrameLoaderClientEfl::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();
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

void FrameLoaderClientEfl::dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse& response)
{
    m_response = response;
    m_firstData = true;
}

void FrameLoaderClientEfl::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const String& MIMEType, const ResourceRequest&)
{
    // we need to call directly here (currently callPolicyFunction does that!)
    ASSERT(function);
    if (canShowMIMEType(MIMEType))
        callPolicyFunction(function, PolicyUse);
    else
        callPolicyFunction(function, PolicyDownload);
}

void FrameLoaderClientEfl::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest&, PassRefPtr<FormState>, const String&)
{
    ASSERT(function);
    ASSERT(m_frame);
    // if not acceptNavigationRequest - look at Qt -> PolicyIgnore;
    // FIXME: do proper check and only reset forms when on PolicyIgnore
    Frame* f = ewk_frame_core_get(m_frame);
    f->loader()->resetMultipleFormSubmissionProtection();
    callPolicyFunction(function, PolicyUse);
}

void FrameLoaderClientEfl::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& resourceRequest, PassRefPtr<FormState>)
{
    ASSERT(function);
    ASSERT(m_frame);
    // if not acceptNavigationRequest - look at Qt -> PolicyIgnore;
    // FIXME: do proper check and only reset forms when on PolicyIgnore
    Frame* f = ewk_frame_core_get(m_frame);
    f->loader()->resetMultipleFormSubmissionProtection();
    callPolicyFunction(function, PolicyUse);
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

void FrameLoaderClientEfl::didTransferChildFrameToNewDocument()
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

ObjectContentType FrameLoaderClientEfl::objectContentType(const KURL& url, const String& mimeType)
{
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

void FrameLoaderClientEfl::windowObjectCleared()
{
    notImplemented();
}

void FrameLoaderClientEfl::documentElementAvailable()
{
    return;
}

void FrameLoaderClientEfl::didPerformFirstNavigation() const
{
    notImplemented();
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
    // notImplemented();
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

void FrameLoaderClientEfl::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClientEfl::restoreViewState()
{
    notImplemented();
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

void FrameLoaderClientEfl::didDisplayInsecureContent()
{
    notImplemented();
}

void FrameLoaderClientEfl::didRunInsecureContent(SecurityOrigin*)
{
    notImplemented();
}

void FrameLoaderClientEfl::makeRepresentation(DocumentLoader*)
{
    notImplemented();
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
    notImplemented();
}

void FrameLoaderClientEfl::dispatchWillClose()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidReceiveIcon()
{
}

void FrameLoaderClientEfl::dispatchDidStartProvisionalLoad()
{
}

void FrameLoaderClientEfl::dispatchDidReceiveTitle(const String& title)
{
    CString cs = title.utf8();
    ewk_frame_title_set(m_frame, cs.data());

    if (ewk_view_frame_main_get(m_view) != m_frame)
        return;
    ewk_view_title_set(m_view, cs.data());
}

void FrameLoaderClientEfl::dispatchDidChangeIcons()
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
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidFirstLayout()
{
    // emit m_frame->initialLayoutCompleted();
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidFirstVisuallyNonEmptyLayout()
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchShow()
{
    notImplemented();
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
    notImplemented();
}

void FrameLoaderClientEfl::willChangeTitle(DocumentLoader*)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}

void FrameLoaderClientEfl::didChangeTitle(DocumentLoader *l)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}

bool FrameLoaderClientEfl::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
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

void FrameLoaderClientEfl::finishedLoading(DocumentLoader* loader)
{
    if (!m_pluginView) {
        if (m_firstData) {
            FrameLoader* fl = loader->frameLoader();
            fl->writer()->setEncoding(m_response.textEncodingName(), false);
            m_firstData = false;
        }
    } else {
        m_pluginView->didFinishLoading();
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
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

void FrameLoaderClientEfl::setTitle(const String& title, const KURL& url)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}

void FrameLoaderClientEfl::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError&)
{
    if (m_firstData) {
        FrameLoader* fl = loader->frameLoader();
        fl->writer()->setEncoding(m_response.textEncodingName(), false);
        m_firstData = false;
    }

}

bool FrameLoaderClientEfl::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length)
{
    notImplemented();
    return false;
}

void FrameLoaderClientEfl::dispatchDidLoadResourceByXMLHttpRequest(unsigned long, const ScriptString&)
{
    notImplemented();
}

void FrameLoaderClientEfl::dispatchDidFailProvisionalLoad(const ResourceError& err)
{
    dispatchDidFailLoad(err);
}

void FrameLoaderClientEfl::dispatchDidFailLoad(const ResourceError& err)
{
    m_loadError = err;
    ewk_frame_load_error(m_frame,
                         m_loadError.domain().utf8().data(),
                         m_loadError.errorCode(), m_loadError.isCancellation(),
                         m_loadError.localizedDescription().utf8().data(),
                         m_loadError.failingURL().utf8().data());
}

void FrameLoaderClientEfl::download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
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
    ResourceError error("Error", -999, request.url().prettyURL(),
                        "Request cancelled");
    error.setIsCancellation(true);
    return error;
}

ResourceError FrameLoaderClientEfl::blockedError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorCannotUseRestrictedPort, request.url().prettyURL(),
                         "Request blocked");
}

ResourceError FrameLoaderClientEfl::cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorCannotShowURL, request.url().string(),
                         "Cannot show URL");
}

ResourceError FrameLoaderClientEfl::interruptForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError("Error", WebKitErrorFrameLoadInterruptedByPolicyChange,
                         request.url().string(), "Frame load interruped by policy change");
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

bool FrameLoaderClientEfl::shouldFallBack(const ResourceError&)
{
    notImplemented();
    return false;
}

bool FrameLoaderClientEfl::canCachePage() const
{
    return false;
}

Frame* FrameLoaderClientEfl::dispatchCreatePage()
{
    notImplemented();
    return 0;
}

void FrameLoaderClientEfl::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientEfl::setMainDocumentError(DocumentLoader* loader, const ResourceError& error)
{
    if (!m_pluginView) {
        if (m_firstData) {
            loader->frameLoader()->writer()->setEncoding(m_response.textEncodingName(), false);
            m_firstData = false;
        }
    } else {
        m_pluginView->didFail(error);
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}

void FrameLoaderClientEfl::startDownload(const ResourceRequest&)
{
    notImplemented();
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
}

}
