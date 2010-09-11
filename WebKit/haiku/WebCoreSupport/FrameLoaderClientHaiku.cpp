/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
 *
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
#include "FrameLoaderClientHaiku.h"

#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
#include "ScriptController.h"
#include "WebView.h"

#include <Message.h>
#include <String.h>

#include <app/Messenger.h>


namespace WebCore {

FrameLoaderClientHaiku::FrameLoaderClientHaiku()
    : m_frame(0)
{
}

void FrameLoaderClientHaiku::setFrame(Frame* frame)
{
    m_frame = frame;
}

void FrameLoaderClientHaiku::setWebView(WebView* webview)
{
    m_webView = webview;
    m_messenger = new BMessenger(m_webView);
    ASSERT(m_messenger->IsValid());
}

void FrameLoaderClientHaiku::detachFrameLoader()
{
    m_frame = 0;
}

bool FrameLoaderClientHaiku::hasWebView() const
{
    return m_webView;
}

bool FrameLoaderClientHaiku::hasBackForwardList() const
{
    notImplemented();
    return true;
}

void FrameLoaderClientHaiku::resetBackForwardList()
{
    notImplemented();
}

bool FrameLoaderClientHaiku::provisionalItemIsTarget() const
{
    notImplemented();
    return false;
}

void FrameLoaderClientHaiku::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::forceLayout()
{
    notImplemented();
}

void FrameLoaderClientHaiku::forceLayoutForNonHTML()
{
    notImplemented();
}

void FrameLoaderClientHaiku::updateHistoryForCommit()
{
    notImplemented();
}

void FrameLoaderClientHaiku::updateHistoryForBackForwardNavigation()
{
    notImplemented();
}

void FrameLoaderClientHaiku::updateHistoryForReload()
{
    notImplemented();
}

void FrameLoaderClientHaiku::updateHistoryForStandardLoad()
{
    notImplemented();
}

void FrameLoaderClientHaiku::updateHistoryForInternalLoad()
{
    notImplemented();
}

void FrameLoaderClientHaiku::updateHistoryAfterClientRedirect()
{
    notImplemented();
}

void FrameLoaderClientHaiku::setCopiesOnScroll()
{
    // apparently mac specific
    notImplemented();
}

LoadErrorResetToken* FrameLoaderClientHaiku::tokenForLoadErrorReset()
{
    notImplemented();
    return 0;
}

void FrameLoaderClientHaiku::resetAfterLoadError(LoadErrorResetToken*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::doNotResetAfterLoadError(LoadErrorResetToken*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::willCloseDocument()
{
    notImplemented();
}

void FrameLoaderClientHaiku::detachedFromParent2()
{
    notImplemented();
}

void FrameLoaderClientHaiku::detachedFromParent3()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidHandleOnloadEvents()
{
    if (m_webView) {
        BMessage message(LOAD_ONLOAD_HANDLE);
        message.AddString("url", m_frame->loader()->documentLoader()->request().url().string());
        m_messenger->SendMessage(&message);
    }
}

void FrameLoaderClientHaiku::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidCancelClientRedirect()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchWillPerformClientRedirect(const KURL&, double interval, double fireDate)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidChangeLocationWithinPage()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidPushStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidReplaceStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidPopStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchWillClose()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidStartProvisionalLoad()
{
    if (m_webView) {
        BMessage message(LOAD_NEGOCIATING);
        message.AddString("url", m_frame->loader()->provisionalDocumentLoader()->request().url().string());
        m_messenger->SendMessage(&message);
    }
}

void FrameLoaderClientHaiku::dispatchDidReceiveTitle(const String& title)
{
    if (m_webView) {
        m_webView->SetPageTitle(title);

        BMessage message(TITLE_CHANGED);
        message.AddString("title", title);
        m_messenger->SendMessage(&message);
    }
}

void FrameLoaderClientHaiku::dispatchDidCommitLoad()
{
    if (m_webView) {
        BMessage message(LOAD_TRANSFERRING);
        message.AddString("url", m_frame->loader()->documentLoader()->request().url().string());
        m_messenger->SendMessage(&message);
    }
}

void FrameLoaderClientHaiku::dispatchDidFinishDocumentLoad()
{
    if (m_webView) {
        BMessage message(LOAD_DOC_COMPLETED);
        message.AddString("url", m_frame->loader()->url().string());
        m_messenger->SendMessage(&message);
    }
}

void FrameLoaderClientHaiku::dispatchDidFinishLoad()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidFirstLayout()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidFirstVisuallyNonEmptyLayout()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchShow()
{
    notImplemented();
}

void FrameLoaderClientHaiku::cancelPolicyCheck()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState>)
{
    // FIXME: Send an event to allow for alerts and cancellation.
    if (!m_frame)
        return;
    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

void FrameLoaderClientHaiku::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::postProgressStartedNotification()
{
    notImplemented();
}

void FrameLoaderClientHaiku::postProgressEstimateChangedNotification()
{
    notImplemented();
}

void FrameLoaderClientHaiku::postProgressFinishedNotification()
{
    if (m_webView) {
        BMessage message(LOAD_DL_COMPLETED);
        message.AddString("url", m_frame->loader()->url().string());
        m_messenger->SendMessage(&message);
    }
}

void FrameLoaderClientHaiku::progressStarted()
{
    notImplemented();
}


void FrameLoaderClientHaiku::progressCompleted()
{
    notImplemented();
}


void FrameLoaderClientHaiku::setMainFrameDocumentReady(bool)
{
    notImplemented();
    // this is only interesting once we provide an external API for the DOM
}

void FrameLoaderClientHaiku::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::didChangeTitle(DocumentLoader* docLoader)
{
    setTitle(docLoader->title(), docLoader->url());
}

void FrameLoaderClientHaiku::finishedLoading(DocumentLoader*)
{
    notImplemented();
}

bool FrameLoaderClientHaiku::canShowMIMEType(const String& MIMEType) const
{
    notImplemented();
    return true;
}

bool FrameLoaderClientHaiku::representationExistsForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return false;
}

String FrameLoaderClientHaiku::generatedMIMETypeForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return String();
}

void FrameLoaderClientHaiku::frameLoadCompleted()
{
    if (m_webView->LockLooper()) {
        m_webView->Draw(m_webView->Bounds());
        m_webView->UnlockLooper();
    }
}

void FrameLoaderClientHaiku::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::restoreViewState()
{
    notImplemented();
}

void FrameLoaderClientHaiku::restoreScrollPositionAndViewState()
{
    notImplemented();
}

void FrameLoaderClientHaiku::provisionalLoadStarted()
{
    notImplemented();
}

bool FrameLoaderClientHaiku::shouldTreatURLAsSameAsCurrent(const KURL&) const
{
    notImplemented();
    return false;
}

void FrameLoaderClientHaiku::addHistoryItemForFragmentScroll()
{
    notImplemented();
}

void FrameLoaderClientHaiku::didFinishLoad()
{
    notImplemented();
}

void FrameLoaderClientHaiku::prepareForDataSourceReplacement()
{
    notImplemented();
}

void FrameLoaderClientHaiku::setTitle(const String& title, const KURL&)
{
    notImplemented();
}

String FrameLoaderClientHaiku::userAgent(const KURL&)
{
    return String("Mozilla/5.0 (compatible; U; InfiNet 0.1; Haiku) AppleWebKit/420+ (KHTML, like Gecko)");
}

void FrameLoaderClientHaiku::dispatchDidReceiveIcon()
{
    notImplemented();
}

void FrameLoaderClientHaiku::frameLoaderDestroyed()
{
    m_frame = 0;
    m_messenger = 0;
    delete this;
}

bool FrameLoaderClientHaiku::canHandleRequest(const WebCore::ResourceRequest&) const
{
    notImplemented();
    return true;
}

void FrameLoaderClientHaiku::partClearedInBegin()
{
    notImplemented();
}

void FrameLoaderClientHaiku::updateGlobalHistory()
{
    notImplemented();
}

void FrameLoaderClientHaiku::updateGlobalHistoryRedirectLinks()
{
    notImplemented();
}

bool FrameLoaderClientHaiku::shouldGoToHistoryItem(WebCore::HistoryItem*) const
{
    notImplemented();
    return true;
}

void FrameLoaderClientHaiku::dispatchDidAddBackForwardItem(WebCore::HistoryItem*) const
{
}

void FrameLoaderClientHaiku::dispatchDidRemoveBackForwardItem(WebCore::HistoryItem*) const
{
}

void FrameLoaderClientHaiku::dispatchDidChangeBackForwardIndex() const
{
}

void FrameLoaderClientHaiku::saveScrollPositionAndViewStateToItem(WebCore::HistoryItem*)
{
    notImplemented();
}

bool FrameLoaderClientHaiku::canCachePage() const
{
    return false;
}

void FrameLoaderClientHaiku::setMainDocumentError(WebCore::DocumentLoader*, const WebCore::ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::committedLoad(WebCore::DocumentLoader* loader, const char* data, int length)
{
    loader->commitData(data, length);
}

WebCore::ResourceError FrameLoaderClientHaiku::cancelledError(const WebCore::ResourceRequest& request)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotShowURL, request.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientHaiku::blockedError(const ResourceRequest& request)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotShowURL, request.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientHaiku::cannotShowURLError(const WebCore::ResourceRequest& request)
{
    return ResourceError(String(), WebKitErrorCannotShowURL, request.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientHaiku::interruptForPolicyChangeError(const WebCore::ResourceRequest& request)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorFrameLoadInterruptedByPolicyChange, request.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientHaiku::cannotShowMIMETypeError(const WebCore::ResourceResponse& response)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotShowMIMEType, response.url().string(), String());
}

WebCore::ResourceError FrameLoaderClientHaiku::fileDoesNotExistError(const WebCore::ResourceResponse& response)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotShowURL, response.url().string(), String());
}

bool FrameLoaderClientHaiku::shouldFallBack(const WebCore::ResourceError& error)
{
    notImplemented();
    return false;
}

WTF::PassRefPtr<DocumentLoader> FrameLoaderClientHaiku::createDocumentLoader(const ResourceRequest& request,
                                                                             const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

void FrameLoaderClientHaiku::download(ResourceHandle*, const ResourceRequest&,
                                      const ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*,
                                                              const ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest& request,
                                                     const ResourceResponse& response)
{
    notImplemented();
}

bool FrameLoaderClientHaiku::shouldUseCredentialStorage(DocumentLoader*, unsigned long)
{
    notImplemented();
    return false;
}

void FrameLoaderClientHaiku::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*,
                                                                       unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidCancelAuthenticationChallenge(DocumentLoader*,
                                                                      unsigned long, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long id,
                                                        const ResourceResponse& response)
{
    notImplemented();
    m_response = response;
    m_firstData = true;
}

void FrameLoaderClientHaiku::dispatchDidReceiveContentLength(DocumentLoader* loader,
                                                             unsigned long id, int length)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidFinishLoading(DocumentLoader*, unsigned long)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidFailLoading(DocumentLoader* loader,
                                                    unsigned long, const ResourceError&)
{
    if (m_webView) {
        BMessage message(LOAD_FAILED);
        message.AddString("url", m_frame->loader()->documentLoader()->request().url().string());
        m_messenger->SendMessage(&message);
    }
}

bool FrameLoaderClientHaiku::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*,
                                                                    const ResourceRequest&,
                                                                    const ResourceResponse&, int)
{
    notImplemented();
    return false;
}

void FrameLoaderClientHaiku::dispatchDidFailProvisionalLoad(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidFailLoad(const ResourceError&)
{
    notImplemented();
}

Frame* FrameLoaderClientHaiku::dispatchCreatePage()
{
    notImplemented();
    return false;
}

void FrameLoaderClientHaiku::dispatchDecidePolicyForMIMEType(FramePolicyFunction function,
                                                             const String& mimetype,
                                                             const ResourceRequest& request)
{
    if (!m_frame)
        return;

    notImplemented();
    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

void FrameLoaderClientHaiku::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function,
                                                                    const NavigationAction&,
                                                                    const ResourceRequest& request,
                                                                    PassRefPtr<FormState>, const String& targetName)
{
    if (!m_frame)
        return;

    if (m_webView) {
        BMessage message(NEW_WINDOW_REQUESTED);
        message.AddString("url", request.url().string());
        if (m_messenger->SendMessage(&message)) {
            (m_frame->loader()->policyChecker()->*function)(PolicyIgnore);
            return;
        }
    }

    (m_frame->loader()->policyChecker()->*function)(PolicyUse);
}

void FrameLoaderClientHaiku::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function,
                                                                     const NavigationAction& action,
                                                                     const ResourceRequest& request,
                                                                     PassRefPtr<FormState>)
{
    if (!m_frame || !function)
        return;

    if (m_webView) {
        BMessage message(NAVIGATION_REQUESTED);
        message.AddString("url", request.url().string());
        m_messenger->SendMessage(&message);

        (m_frame->loader()->policyChecker()->*function)(PolicyUse);
    }
}

void FrameLoaderClientHaiku::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::startDownload(const ResourceRequest&)
{
    notImplemented();
}

PassRefPtr<Frame> FrameLoaderClientHaiku::createFrame(const KURL& url, const String& name,
                                                      HTMLFrameOwnerElement* ownerElement,
                                                      const String& referrer, bool allowsScrolling,
                                                      int marginWidth, int marginHeight)
{
    // FIXME: We should apply the right property to the frameView. (scrollbar,margins)

    RefPtr<Frame> childFrame = Frame::create(m_frame->page(), ownerElement, this);
    setFrame(childFrame.get());

    RefPtr<FrameView> frameView = FrameView::create(childFrame.get());

    frameView->setAllowsScrolling(allowsScrolling);
    frameView->deref();
    childFrame->setView(frameView.get());
    childFrame->init();

    childFrame->tree()->setName(name);
    m_frame->tree()->appendChild(childFrame);

    childFrame->loader()->loadURLIntoChildFrame(url, referrer, childFrame.get());

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();

    notImplemented();
    return 0;
}

void FrameLoaderClientHaiku::didTransferChildFrameToNewDocument()
{
}

ObjectContentType FrameLoaderClientHaiku::objectContentType(const KURL& url, const String& mimeType)
{
    notImplemented();
    return ObjectContentType();
}

PassRefPtr<Widget> FrameLoaderClientHaiku::createPlugin(const IntSize&, HTMLPlugInElement*,
                                                        const KURL&, const Vector<String>&,
                                                        const Vector<String>&, const String&,
                                                        bool loadManually)
{
    notImplemented();
    return 0;
}

void FrameLoaderClientHaiku::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplemented();
    return;
}

ResourceError FrameLoaderClientHaiku::pluginWillHandleLoadError(const ResourceResponse& response)
{
    notImplemented();
    return ResourceError(String(), WebKitErrorCannotLoadPlugIn, response.url().string(), String());
}

PassRefPtr<Widget> FrameLoaderClientHaiku::createJavaAppletWidget(const IntSize&, HTMLAppletElement*,
                                                       const KURL& baseURL,
                                                       const Vector<String>& paramNames,
                                                       const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

String FrameLoaderClientHaiku::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClientHaiku::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    if (m_webView) {
        BMessage message(JAVASCRIPT_WINDOW_OBJECT_CLEARED);
        m_messenger->SendMessage(&message);
    }
}

void FrameLoaderClientHaiku::documentElementAvailable()
{
}

void FrameLoaderClientHaiku::didPerformFirstNavigation() const
{
    notImplemented();
}

void FrameLoaderClientHaiku::registerForIconNotification(bool listen)
{
    notImplemented();
}

void FrameLoaderClientHaiku::savePlatformDataToCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientHaiku::transitionToCommittedForNewPage()
{
    ASSERT(m_frame);
    ASSERT(m_webView);

    Page* page = m_frame->page();
    ASSERT(page);

    bool isMainFrame = m_frame == page->mainFrame();

    m_frame->setView(0);

    RefPtr<FrameView> frameView;
    if (isMainFrame) {
        if (m_webView->LockLooper()) {
            // We lock the looper in order to get the bounds of the WebView.
            frameView = FrameView::create(m_frame, IntRect(m_webView->Bounds()).size());
            m_webView->UnlockLooper();
        }
    } else
        frameView = FrameView::create(m_frame);

    ASSERT(frameView);
    m_frame->setView(frameView);

    frameView->setPlatformWidget(m_webView);

    if (HTMLFrameOwnerElement* owner = m_frame->ownerElement())
        m_frame->view()->setScrollbarModes(owner->scrollingMode(), owner->scrollingMode());
}

} // namespace WebCore

