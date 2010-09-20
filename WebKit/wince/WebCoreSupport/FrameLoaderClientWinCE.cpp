/*
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLoaderClientWinCE.h"

#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "FrameNetworkingContextWinCE.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "PluginDatabase.h"
#include "RenderPart.h"
#include "WebView.h"

using namespace WebCore;

namespace WebKit {

FrameLoaderClientWinCE::FrameLoaderClientWinCE(WebView* view)
    : m_webView(view)
    , m_pluginView(0)
{
    ASSERT(m_webView);
}

FrameLoaderClientWinCE::~FrameLoaderClientWinCE()
{
}

String FrameLoaderClientWinCE::userAgent(const KURL&)
{
    return "WebKitWinCE";
}

PassRefPtr<DocumentLoader> FrameLoaderClientWinCE::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

void FrameLoaderClientWinCE::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            m_hasSentResponseToPlugin = true;
        }
        m_pluginView->didReceiveData(data, length);
    } else
        loader->commitData(data, length);
}

bool FrameLoaderClientWinCE::shouldUseCredentialStorage(DocumentLoader*, unsigned long)
{
    notImplemented();
    return false;
}

void FrameLoaderClientWinCE::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchWillSendRequest(DocumentLoader*, unsigned long, WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::assignIdentifierToInitialRequest(unsigned long, DocumentLoader*, const WebCore::ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::postProgressStartedNotification()
{
    notImplemented();
}

void FrameLoaderClientWinCE::postProgressEstimateChangedNotification()
{
    notImplemented();
}

void FrameLoaderClientWinCE::postProgressFinishedNotification()
{
    notImplemented();
}

void FrameLoaderClientWinCE::frameLoaderDestroyed()
{
    m_webView = 0;
    m_frame = 0;
    delete this;
}

void FrameLoaderClientWinCE::dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse& response)
{
    m_response = response;
}

void FrameLoaderClientWinCE::dispatchDecidePolicyForMIMEType(FramePolicyFunction policyFunction, const String& mimeType, const WebCore::ResourceRequest&)
{
    if (canShowMIMEType(mimeType))
        (m_frame->loader()->policyChecker()->*policyFunction)(PolicyUse);
    else
        (m_frame->loader()->policyChecker()->*policyFunction)(PolicyDownload);
}

void FrameLoaderClientWinCE::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction policyFunction, const NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<FormState>, const String&)
{
    (m_frame->loader()->policyChecker()->*policyFunction)(PolicyUse);
}

void FrameLoaderClientWinCE::dispatchDecidePolicyForNavigationAction(FramePolicyFunction policyFunction, const NavigationAction&, const WebCore::ResourceRequest&, PassRefPtr<FormState>)
{
    (m_frame->loader()->policyChecker()->*policyFunction)(PolicyUse);
}

void FrameLoaderClientWinCE::dispatchWillSubmitForm(FramePolicyFunction policyFunction, PassRefPtr<FormState>)
{
    (m_frame->loader()->policyChecker()->*policyFunction)(PolicyUse);
}

PassRefPtr<Widget> FrameLoaderClientWinCE::createPlugin(const IntSize&, HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    return 0;
}

PassRefPtr<Frame> FrameLoaderClientWinCE::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                                 const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    return m_webView->createFrame(url, name, ownerElement, referrer, allowsScrolling, marginWidth, marginHeight);
}

void FrameLoaderClientWinCE::didTransferChildFrameToNewDocument()
{
}

void FrameLoaderClientWinCE::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    m_hasSentResponseToPlugin = false;
}

PassRefPtr<Widget> FrameLoaderClientWinCE::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL&, const Vector<String>&, const Vector<String>&)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoaderClientWinCE::objectContentType(const KURL& url, const String& mimeType)
{
    return FrameLoader::defaultObjectContentType(url, mimeType);
}

String FrameLoaderClientWinCE::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClientWinCE::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::documentElementAvailable()
{
    notImplemented();
}

void FrameLoaderClientWinCE::didPerformFirstNavigation() const
{
    notImplemented();
}

void FrameLoaderClientWinCE::registerForIconNotification(bool)
{
    notImplemented();
}

void FrameLoaderClientWinCE::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

bool FrameLoaderClientWinCE::hasWebView() const
{
    return true;
}

void FrameLoaderClientWinCE::dispatchDidFinishLoad()
{
    notImplemented();
}

void FrameLoaderClientWinCE::frameLoadCompleted()
{
    notImplemented();
}

void FrameLoaderClientWinCE::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::restoreViewState()
{
    notImplemented();
}

bool FrameLoaderClientWinCE::shouldGoToHistoryItem(HistoryItem* item) const
{
    return item;
}

void FrameLoaderClientWinCE::dispatchDidAddBackForwardItem(HistoryItem*) const
{
}

void FrameLoaderClientWinCE::dispatchDidRemoveBackForwardItem(HistoryItem*) const
{
}

void FrameLoaderClientWinCE::dispatchDidChangeBackForwardIndex() const
{
}

void FrameLoaderClientWinCE::didDisplayInsecureContent()
{
    notImplemented();
}

void FrameLoaderClientWinCE::didRunInsecureContent(SecurityOrigin*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::forceLayout()
{
    notImplemented();
}

void FrameLoaderClientWinCE::forceLayoutForNonHTML()
{
    notImplemented();
}

void FrameLoaderClientWinCE::setCopiesOnScroll()
{
    notImplemented();
}

void FrameLoaderClientWinCE::detachedFromParent2()
{
    notImplemented();
}

void FrameLoaderClientWinCE::detachedFromParent3()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidHandleOnloadEvents()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidCancelClientRedirect()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchWillPerformClientRedirect(const KURL&, double, double)
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidChangeLocationWithinPage()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidPushStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidReplaceStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidPopStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchWillClose()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidReceiveIcon()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidStartProvisionalLoad()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidReceiveTitle(const String&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidChangeIcons()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidCommitLoad()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidFinishDocumentLoad()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidFirstLayout()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidFirstVisuallyNonEmptyLayout()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchShow()
{
    notImplemented();
}

void FrameLoaderClientWinCE::cancelPolicyCheck()
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::didChangeTitle(DocumentLoader* documentLoader)
{
    setTitle(documentLoader->title(), documentLoader->url());
}

bool FrameLoaderClientWinCE::canHandleRequest(const WebCore::ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool FrameLoaderClientWinCE::canShowMIMEType(const String& type) const
{
    return (MIMETypeRegistry::isSupportedImageMIMEType(type)
            || MIMETypeRegistry::isSupportedNonImageMIMEType(type)
            || MIMETypeRegistry::isSupportedMediaMIMEType(type)
            || PluginDatabase::installedPlugins()->isMIMETypeRegistered(type));
}

bool FrameLoaderClientWinCE::representationExistsForURLScheme(const String&) const
{
    notImplemented();
    return false;
}

String FrameLoaderClientWinCE::generatedMIMETypeForURLScheme(const String&) const
{
    notImplemented();
    return String();
}

void FrameLoaderClientWinCE::finishedLoading(DocumentLoader* documentLoader)
{
    if (!m_pluginView) {
        FrameLoader* loader = documentLoader->frameLoader();
        loader->writer()->setEncoding(m_response.textEncodingName(), false);
        return;
    }

    m_pluginView->didFinishLoading();
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

void FrameLoaderClientWinCE::provisionalLoadStarted()
{
    notImplemented();
}

void FrameLoaderClientWinCE::didFinishLoad()
{
    notImplemented();
}

void FrameLoaderClientWinCE::prepareForDataSourceReplacement()
{
    notImplemented();
}

void FrameLoaderClientWinCE::setTitle(const String&, const KURL&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long, int)
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidFinishLoading(DocumentLoader*, unsigned long)
{
    notImplemented();
}

void FrameLoaderClientWinCE::dispatchDidFailLoading(DocumentLoader*, unsigned long, const ResourceError&)
{
    notImplemented();
}

bool FrameLoaderClientWinCE::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, int)
{
    notImplemented();
    return false;
}

void FrameLoaderClientWinCE::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    dispatchDidFailLoad(error);
}

void FrameLoaderClientWinCE::dispatchDidFailLoad(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::download(ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&)
{
    notImplemented();
}

ResourceError FrameLoaderClientWinCE::cancelledError(const WebCore::ResourceRequest&)
{
    return ResourceError();
}

ResourceError FrameLoaderClientWinCE::blockedError(const WebCore::ResourceRequest&)
{
    return ResourceError();
}

ResourceError FrameLoaderClientWinCE::cannotShowURLError(const WebCore::ResourceRequest&)
{
    return ResourceError();
}

ResourceError FrameLoaderClientWinCE::interruptForPolicyChangeError(const WebCore::ResourceRequest&)
{
    return ResourceError();
}

ResourceError FrameLoaderClientWinCE::cannotShowMIMETypeError(const WebCore::ResourceResponse&)
{
    return ResourceError();
}

ResourceError FrameLoaderClientWinCE::fileDoesNotExistError(const WebCore::ResourceResponse&)
{
    return ResourceError();
}

ResourceError FrameLoaderClientWinCE::pluginWillHandleLoadError(const WebCore::ResourceResponse&)
{
    return ResourceError();
}

bool FrameLoaderClientWinCE::shouldFallBack(const ResourceError& error)
{
    return !(error.isCancellation());
}

bool FrameLoaderClientWinCE::canCachePage() const
{
    return true;
}

Frame* FrameLoaderClientWinCE::dispatchCreatePage()
{
    notImplemented();
    return 0;
}

void FrameLoaderClientWinCE::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (!m_pluginView)
        return;

    m_pluginView->didFail(error);
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

void FrameLoaderClientWinCE::startDownload(const WebCore::ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClientWinCE::updateGlobalHistory()
{
    notImplemented();
}

void FrameLoaderClientWinCE::updateGlobalHistoryRedirectLinks()
{
    notImplemented();
}

void FrameLoaderClientWinCE::savePlatformDataToCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientWinCE::transitionToCommittedForNewPage()
{
    Page* page = m_frame->page();
    ASSERT(page);

    bool isMainFrame = m_frame == page->mainFrame();

    m_frame->setView(0);

    RefPtr<FrameView> frameView;
    if (isMainFrame) {
        RECT rect;
        m_webView->frameRect(&rect);
        frameView = FrameView::create(m_frame, IntRect(rect).size());
    } else
        frameView = FrameView::create(m_frame);

    m_frame->setView(frameView);

    if (m_frame->ownerRenderer())
        m_frame->ownerRenderer()->setWidget(frameView);
}

PassRefPtr<WebCore::FrameNetworkingContext> FrameLoaderClientWinCE::createNetworkingContext()
{
    return FrameNetworkingContextWinCE::create(m_frame, userAgent(KURL()));
}

} // namespace WebKit
