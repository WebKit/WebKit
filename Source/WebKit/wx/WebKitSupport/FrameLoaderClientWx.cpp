/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "FrameLoaderClientWx.h"

#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/APICast.h>

#include "DocumentLoader.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoaderTypes.h"
#include "FrameView.h"
#include "FrameTree.h"
#include "PluginView.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PluginView.h"
#include "ProgressTracker.h"
#include "RenderPart.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "ScriptController.h"
#if OS(WINDOWS)
#include "SystemInfo.h"
#endif

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#include <stdio.h>
#if OS(UNIX)
#include <sys/utsname.h>
#endif

#include "FrameNetworkingContextWx.h"
#include "WebFrame.h"
#include "WebFramePrivate.h"
#include "WebKitVersion.h"
#include "WebView.h"
#include "WebViewPrivate.h"

using namespace WebKit;

namespace WebCore {

inline int wxNavTypeFromWebNavType(NavigationType type){
    if (type == NavigationTypeLinkClicked)
        return WEBVIEW_NAV_LINK_CLICKED;
    
    if (type == NavigationTypeFormSubmitted)
        return WEBVIEW_NAV_FORM_SUBMITTED;
        
    if (type == NavigationTypeBackForward)
        return WEBVIEW_NAV_BACK_NEXT;
        
    if (type == NavigationTypeReload)
        return WEBVIEW_NAV_RELOAD;
        
    if (type == NavigationTypeFormResubmitted)
        return WEBVIEW_NAV_FORM_RESUBMITTED;
        
    return WEBVIEW_NAV_OTHER;
}

FrameLoaderClientWx::FrameLoaderClientWx()
    : m_frame(0)
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
    , m_webFrame(0)
{
}


FrameLoaderClientWx::~FrameLoaderClientWx()
{
}

void FrameLoaderClientWx::setFrame(WebKit::WebFrame *frame)
{
    m_webFrame = frame;
    m_frame = m_webFrame->m_impl->frame;
}

void FrameLoaderClientWx::setWebView(WebKit::WebView *webview)
{
    m_webView = webview;
}

bool FrameLoaderClientWx::hasWebView() const
{
    return m_webView != NULL;
}

bool FrameLoaderClientWx::hasBackForwardList() const
{
    notImplemented();
    return true;
}


void FrameLoaderClientWx::resetBackForwardList()
{
    notImplemented();
}


bool FrameLoaderClientWx::provisionalItemIsTarget() const
{
    notImplemented();
    return false;
}

void FrameLoaderClientWx::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientWx::forceLayout()
{
    notImplemented();
}


void FrameLoaderClientWx::forceLayoutForNonHTML()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForCommit()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForBackForwardNavigation()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForReload()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForStandardLoad()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryForInternalLoad()
{
    notImplemented();
}


void FrameLoaderClientWx::updateHistoryAfterClientRedirect()
{
    notImplemented();
}


void FrameLoaderClientWx::setCopiesOnScroll()
{
    // apparently mac specific
    notImplemented();
}


LoadErrorResetToken* FrameLoaderClientWx::tokenForLoadErrorReset()
{
    notImplemented();
    return 0;
}


void FrameLoaderClientWx::resetAfterLoadError(LoadErrorResetToken*)
{
    notImplemented();
}


void FrameLoaderClientWx::doNotResetAfterLoadError(LoadErrorResetToken*)
{
    notImplemented();
}


void FrameLoaderClientWx::willCloseDocument()
{
    notImplemented();
}


void FrameLoaderClientWx::detachedFromParent2()
{
    notImplemented();
}


void FrameLoaderClientWx::detachedFromParent3()
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidHandleOnloadEvents()
{
    if (m_webView) {
        WebKit::WebViewLoadEvent wkEvent(m_webView);
        wkEvent.SetState(WEBVIEW_LOAD_ONLOAD_HANDLED);
        wkEvent.SetURL(m_frame->loader()->documentLoader()->request().url().string());
        wkEvent.SetFrame(m_webFrame);
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}


void FrameLoaderClientWx::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchDidCancelClientRedirect()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchWillPerformClientRedirect(const KURL&,
                                                            double interval,
                                                            double fireDate)
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchDidChangeLocationWithinPage()
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidPushStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidReplaceStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidPopStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchWillClose()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchDidStartProvisionalLoad()
{
    if (m_webView) {
        WebKit::WebViewLoadEvent wkEvent(m_webView);
        wkEvent.SetState(WEBVIEW_LOAD_NEGOTIATING);
        wkEvent.SetURL(m_frame->loader()->provisionalDocumentLoader()->request().url().string());
        wkEvent.SetFrame(m_webFrame);
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}


void FrameLoaderClientWx::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    if (m_webView) {
        // FIXME: use direction of title.
        if (m_webFrame == m_webView->GetMainFrame())
            m_webView->SetPageTitle(title.string());
        
        WebViewReceivedTitleEvent wkEvent(m_webView);
        wkEvent.SetFrame(m_webFrame);
        wkEvent.SetTitle(title.string());
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}


void FrameLoaderClientWx::dispatchDidCommitLoad()
{
    if (m_webView) {
        WebKit::WebViewLoadEvent wkEvent(m_webView);
        wkEvent.SetState(WEBVIEW_LOAD_TRANSFERRING);
        wkEvent.SetURL(m_frame->loader()->documentLoader()->request().url().string());
        wkEvent.SetFrame(m_webFrame);
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}

void FrameLoaderClientWx::dispatchDidFinishDocumentLoad()
{
    if (m_webView) {
        WebKit::WebViewLoadEvent wkEvent(m_webView);
        wkEvent.SetState(WEBVIEW_LOAD_DOC_COMPLETED);
        wkEvent.SetURL(m_frame->document()->url().string());
        wkEvent.SetFrame(m_webFrame);
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}

void FrameLoaderClientWx::dispatchDidChangeIcons(WebCore::IconType)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidFinishLoad()
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidLayout(LayoutMilestones)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchShow()
{
    notImplemented();
}


void FrameLoaderClientWx::cancelPolicyCheck()
{
    notImplemented();
}


void FrameLoaderClientWx::dispatchWillSubmitForm(FramePolicyFunction function,
                                                 PassRefPtr<FormState>)
{
    // FIXME: Send an event to allow for alerts and cancellation
    if (!m_webFrame)
        return;
    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}


void FrameLoaderClientWx::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientWx::postProgressStartedNotification()
{
    notImplemented();
}

void FrameLoaderClientWx::postProgressEstimateChangedNotification()
{
    notImplemented();
}

void FrameLoaderClientWx::postProgressFinishedNotification()
{
    if (m_webView) {
        WebKit::WebViewLoadEvent wkEvent(m_webView);
        wkEvent.SetState(WEBVIEW_LOAD_DL_COMPLETED);
        wkEvent.SetURL(m_frame->document()->url().string());
        wkEvent.SetFrame(m_webFrame);
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}

void FrameLoaderClientWx::progressStarted()
{
    notImplemented();
}


void FrameLoaderClientWx::progressCompleted()
{
    notImplemented();
}


void FrameLoaderClientWx::setMainFrameDocumentReady(bool b)
{
    notImplemented();
    // this is only interesting once we provide an external API for the DOM
}


void FrameLoaderClientWx::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}


void FrameLoaderClientWx::didChangeTitle(DocumentLoader *l)
{
    setTitle(l->title(), l->url());
}


void FrameLoaderClientWx::finishedLoading(DocumentLoader*)
{
    if (m_pluginView) {
        m_pluginView->didFinishLoading();
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}

bool FrameLoaderClientWx::canShowMIMETypeAsHTML(const String& MIMEType) const
{
    notImplemented();
    return true;
}

    
bool FrameLoaderClientWx::canShowMIMEType(const String& MIMEType) const
{
    notImplemented();
    return true;
}


bool FrameLoaderClientWx::representationExistsForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return false;
}


String FrameLoaderClientWx::generatedMIMETypeForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return String();
}


void FrameLoaderClientWx::frameLoadCompleted()
{
    notImplemented();
}

void FrameLoaderClientWx::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClientWx::restoreViewState()
{
    notImplemented();
}
        
void FrameLoaderClientWx::restoreScrollPositionAndViewState()
{
    notImplemented();
}


void FrameLoaderClientWx::provisionalLoadStarted()
{
    notImplemented();
}


bool FrameLoaderClientWx::shouldTreatURLAsSameAsCurrent(const KURL&) const
{
    notImplemented();
    return false;
}


void FrameLoaderClientWx::addHistoryItemForFragmentScroll()
{
    notImplemented();
}


void FrameLoaderClientWx::didFinishLoad()
{
    notImplemented();
}


void FrameLoaderClientWx::prepareForDataSourceReplacement()
{
    notImplemented();
}


void FrameLoaderClientWx::setTitle(const StringWithDirection& title, const KURL&)
{
    notImplemented();
}

static String agentOS()
{
#if OS(DARWIN)
#if CPU(X86)
    return "Intel Mac OS X";
#else
    return "PPC Mac OS X";
#endif
#elif OS(UNIX)
    struct utsname name;
    if (uname(&name) != -1)
        return makeString(name.sysname, ' ', name.machine);
    
    return "Unknown";
#elif OS(WINDOWS)
    return windowsVersionForUAString();
#else
    notImplemented();
    return "Unknown";
#endif
}

static String composeUserAgent()
{
    String webKitVersion = String::format("%d.%d", WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION);
    return makeString("Mozilla/5.0 (", agentOS(), ") AppleWebKit/", webKitVersion, " (KHTML, like Gecko) version/5.1.1 Safari/", webKitVersion);
}

    
String FrameLoaderClientWx::userAgent(const KURL&)
{
    // FIXME: Use the new APIs introduced by the GTK port to fill in these values.
    return composeUserAgent();
}

void FrameLoaderClientWx::dispatchDidReceiveIcon()
{
    notImplemented();
}

void FrameLoaderClientWx::frameLoaderDestroyed()
{
    if (m_webFrame)
        delete m_webFrame;
    m_webFrame = 0;
    m_frame = 0;
    delete this;
}

bool FrameLoaderClientWx::canHandleRequest(const WebCore::ResourceRequest&) const
{
    notImplemented();
    return true;
}

void FrameLoaderClientWx::partClearedInBegin()
{
    notImplemented();
}

void FrameLoaderClientWx::updateGlobalHistory()
{
    notImplemented();
}

void FrameLoaderClientWx::updateGlobalHistoryRedirectLinks()
{
    notImplemented();
}

bool FrameLoaderClientWx::shouldGoToHistoryItem(WebCore::HistoryItem*) const
{
    notImplemented();
    return true;
}

bool FrameLoaderClientWx::shouldStopLoadingForHistoryItem(WebCore::HistoryItem*) const
{
    return true;
}

void FrameLoaderClientWx::didDisplayInsecureContent()
{
    notImplemented();
}

void FrameLoaderClientWx::didRunInsecureContent(WebCore::SecurityOrigin*, const KURL&)
{
    notImplemented();
}

void FrameLoaderClientWx::didDetectXSS(const KURL&, bool)
{
    notImplemented();
}

void FrameLoaderClientWx::saveScrollPositionAndViewStateToItem(WebCore::HistoryItem*)
{
    notImplemented();
}

bool FrameLoaderClientWx::canCachePage() const
{
    return false;
}

void FrameLoaderClientWx::setMainDocumentError(WebCore::DocumentLoader* loader, const WebCore::ResourceError&)
{
    notImplemented();
}

// FIXME: This function should be moved into WebCore.
void FrameLoaderClientWx::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    if (!m_webFrame)
        return;
    if (!m_pluginView)
        loader->commitData(data, length);

    // We re-check here as the plugin can have been created
    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            // didReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
            // setting up this stream can cause the main document load to be cancelled, setting m_pluginView
            // to null
            if (!m_pluginView)
                return;
            m_hasSentResponseToPlugin = true;
        }
        m_pluginView->didReceiveData(data, length);
    }
}

WebCore::ResourceError FrameLoaderClientWx::cancelledError(const WebCore::ResourceRequest& request)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotShowURL, request.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientWx::blockedError(const ResourceRequest& request)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotShowURL, request.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientWx::cannotShowURLError(const WebCore::ResourceRequest& request)
{
    return ResourceError(String(), WebKitErrorCannotShowURL, request.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientWx::interruptedForPolicyChangeError(const WebCore::ResourceRequest& request)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorFrameLoadInterruptedByPolicyChange, request.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientWx::cannotShowMIMETypeError(const WebCore::ResourceResponse& response)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotShowMIMEType, response.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientWx::fileDoesNotExistError(const WebCore::ResourceResponse& response)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotShowURL, response.url().string(), String());
}

bool FrameLoaderClientWx::shouldFallBack(const WebCore::ResourceError& error)
{
    notImplemented();
    return false;
}

WTF::PassRefPtr<DocumentLoader> FrameLoaderClientWx::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

void FrameLoaderClientWx::convertMainResourceLoadToDownload(WebCore::MainResourceLoader*, const ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientWx::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();   
}

void FrameLoaderClientWx::dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest& request, const ResourceResponse& response)
{
    notImplemented();
}

bool FrameLoaderClientWx::shouldUseCredentialStorage(DocumentLoader*, unsigned long)
{
    notImplemented();
    return false;
}

void FrameLoaderClientWx::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long id, const ResourceResponse& response)
{
    notImplemented();
    m_response = response;
}

void FrameLoaderClientWx::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long id, int length)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidFinishLoading(DocumentLoader*, unsigned long)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidFailLoading(DocumentLoader* loader, unsigned long, const ResourceError&)
{
    if (m_webView) {
        WebKit::WebViewLoadEvent wkEvent(m_webView);
        wkEvent.SetState(WEBVIEW_LOAD_FAILED);
        wkEvent.SetURL(m_frame->loader()->documentLoader()->request().url().string());
        wkEvent.SetFrame(m_webFrame);
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}

bool FrameLoaderClientWx::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int)
{
    notImplemented();
    return false;
}

void FrameLoaderClientWx::dispatchDidFailProvisionalLoad(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientWx::dispatchDidFailLoad(const ResourceError&)
{
    notImplemented();
}

Frame* FrameLoaderClientWx::dispatchCreatePage(const NavigationAction&)
{
    notImplemented();
    return false;
}

void FrameLoaderClientWx::dispatchDecidePolicyForResponse(FramePolicyFunction function, const ResourceResponse& response, const ResourceRequest& request)
{
    if (!m_webFrame)
        return;
    
    notImplemented();
    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

void FrameLoaderClientWx::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction&, const ResourceRequest& request, PassRefPtr<FormState>, const String& targetName)
{
    if (!m_webFrame)
        return;

    if (m_webView) {
        WebKit::WebViewNewWindowEvent wkEvent(m_webView);
        wkEvent.SetURL(request.url().string());
        wkEvent.SetTargetName(targetName);
        wkEvent.SetFrame(m_webFrame);
        if (m_webView->GetEventHandler()->ProcessEvent(wkEvent)) {
            // if the app handles and doesn't skip the event, 
            // from WebKit's perspective treat it as blocked / ignored
            (m_frame->loader()->policyChecker()->*function)(PolicyIgnore);
            return;
        }
    }
    
    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

void FrameLoaderClientWx::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& action, const ResourceRequest& request, PassRefPtr<FormState>)
{
    if (!m_webFrame)
        return;
        
    if (m_webView) {
        WebKit::WebViewBeforeLoadEvent wkEvent(m_webView);
        wkEvent.SetNavigationType(wxNavTypeFromWebNavType(action.type()));
        wkEvent.SetURL(request.url().string());
        wkEvent.SetFrame(m_webFrame);
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
        if (wkEvent.IsCancelled())
            (m_frame->loader()->policyChecker()->*function)(PolicyIgnore);
        else
            (m_frame->loader()->policyChecker()->*function)(PolicyUse);
        
    }
}

void FrameLoaderClientWx::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientWx::startDownload(const ResourceRequest&, const String& /* suggestedName */)
{
    notImplemented();
}

PassRefPtr<Frame> FrameLoaderClientWx::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                   const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    WebViewFrameData* data = new WebViewFrameData();
    data->name = name;
    data->ownerElement = ownerElement;
    data->url = url;
    data->referrer = referrer;
    data->allowsScrolling = allowsScrolling;
    data->marginWidth = marginWidth;
    data->marginHeight = marginHeight;

    WebKit::WebFrame* newFrame = new WebKit::WebFrame(m_webView, m_webFrame, data);

    RefPtr<Frame> childFrame = adoptRef(newFrame->m_impl->frame);

    // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    if (!childFrame->page())
        return 0;

    m_frame->loader()->loadURLIntoChildFrame(url, referrer, childFrame.get());
    
    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;
    
    return childFrame.release();
}

ObjectContentType FrameLoaderClientWx::objectContentType(const KURL& url, const String& mimeType, bool shouldPreferPlugInsForImages)
{
    notImplemented();
    return ObjectContentType();
}

PassRefPtr<Widget> FrameLoaderClientWx::createPlugin(const IntSize& size, HTMLPlugInElement* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
#if __WXMSW__ || __WXMAC__
    RefPtr<PluginView> pv = PluginView::create(m_frame, size, element, url, paramNames, paramValues, mimeType, loadManually);
    if (pv->status() == PluginStatusLoadedSuccessfully)
        return pv;
#endif
    return 0;
}

void FrameLoaderClientWx::redirectDataToPlugin(Widget* pluginWidget)
{
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    if (pluginWidget)
        m_hasSentResponseToPlugin = false;
}

ResourceError FrameLoaderClientWx::pluginWillHandleLoadError(const ResourceResponse& response)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotLoadPlugIn, response.url().string(), String());
}

PassRefPtr<Widget> FrameLoaderClientWx::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL,
                                                    const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

String FrameLoaderClientWx::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClientWx::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    if (m_webView) {
        WebKit::WebViewWindowObjectClearedEvent wkEvent(m_webView);
        Frame* coreFrame = m_webView->GetMainFrame()->GetFrame();
        JSGlobalContextRef context = toGlobalRef(coreFrame->script()->globalObject(mainThreadNormalWorld())->globalExec());
        JSObjectRef windowObject = toRef(coreFrame->script()->globalObject(mainThreadNormalWorld()));
        wkEvent.SetJSContext(context);
        wkEvent.SetWindowObject(windowObject);
        m_webView->GetEventHandler()->ProcessEvent(wkEvent);
    }
}

void FrameLoaderClientWx::documentElementAvailable()
{
}

void FrameLoaderClientWx::didPerformFirstNavigation() const
{
    notImplemented();
}

void FrameLoaderClientWx::registerForIconNotification(bool listen)
{
    notImplemented();
}

void FrameLoaderClientWx::savePlatformDataToCachedFrame(CachedFrame*)
{ 
    notImplemented();
}

void FrameLoaderClientWx::transitionToCommittedFromCachedFrame(CachedFrame*)
{ 
    notImplemented();
}

void FrameLoaderClientWx::transitionToCommittedForNewPage()
{ 
    ASSERT(m_webFrame);
    ASSERT(m_frame);
    ASSERT(m_webView);
    
    IntSize size = IntRect(m_webView->GetRect()).size();
    // FIXME: This value should be gotten from m_webView->IsTransparent();
    // but transitionToCommittedForNewPage() can be called while m_webView is
    // still being initialized.
    bool transparent = false;
    Color backgroundColor = transparent ? WebCore::Color::transparent : WebCore::Color::white;
    
    if (m_frame)
        m_frame->createView(size, backgroundColor, transparent);
}

void FrameLoaderClientWx::didSaveToPageCache()
{
}

void FrameLoaderClientWx::didRestoreFromPageCache()
{
}

void FrameLoaderClientWx::dispatchDidBecomeFrameset(bool)
{
}

bool FrameLoaderClientWx::shouldUsePluginDocument(const String &mimeType) const
{
    // NOTE: Plugin Documents are used for viewing PDFs, etc. inline, and should
    // not be used for pages with plugins in them.
    return false;
}

PassRefPtr<FrameNetworkingContext> FrameLoaderClientWx::createNetworkingContext()
{
    return FrameNetworkingContextWx::create(m_frame);
}

}
