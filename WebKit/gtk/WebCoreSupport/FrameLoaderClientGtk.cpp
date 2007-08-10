/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006, 2007 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
#include "FrameLoaderClientGtk.h"
#include "DocumentLoader.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "FrameTree.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
#include "CString.h"
#include "ProgressTracker.h"
#include "webkitgtkpage.h"
#include "webkitgtkframe.h"
#include "webkitgtkprivate.h"
#include <stdio.h>

using namespace WebKitGtk;

namespace WebCore {

FrameLoaderClientGtk::FrameLoaderClientGtk(WebKitGtkFrame* frame)
    : m_frame(frame)
    , m_firstData(false)
{
    ASSERT(m_frame);
}

String FrameLoaderClientGtk::userAgent(const KURL&)
{
    return "Mozilla/5.0 (X11; U; Linux i686; en-US) AppleWebKit/420+ (KHTML, like Gecko)";
}

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClientGtk::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<DocumentLoader> loader = new DocumentLoader(request, substituteData);
    return loader.release();
}

void FrameLoaderClientGtk::dispatchWillSubmitForm(FramePolicyFunction policyFunction,  PassRefPtr<FormState>)
{
    // FIXME: This is surely too simple
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
    (core(m_frame)->loader()->*policyFunction)(PolicyUse);
}


void FrameLoaderClientGtk::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    FrameLoader *fl = loader->frameLoader();
    fl->setEncoding(m_response.textEncodingName(), false);
    fl->addData(data, length);
}

void FrameLoaderClientGtk::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientGtk::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientGtk::dispatchWillSendRequest(DocumentLoader*, unsigned long , ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientGtk::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();   
}

void FrameLoaderClientGtk::postProgressStartedNotification()
{
    WebKitGtkPage* page = getPageFromFrame(m_frame);
    g_signal_emit_by_name(page, "load_started", m_frame);
}

void FrameLoaderClientGtk::postProgressEstimateChangedNotification()
{
    WebKitGtkPage* kitPage = getPageFromFrame(m_frame);
    Page* corePage = core(kitPage);

    g_signal_emit_by_name(kitPage, "load_progress_changed", lround(corePage->progress()->estimatedProgress()*100)); 
}

void FrameLoaderClientGtk::postProgressFinishedNotification()
{
    WebKitGtkPage* page = getPageFromFrame(m_frame);

    g_signal_emit_by_name(page, "load_finished", m_frame);
}

void FrameLoaderClientGtk::frameLoaderDestroyed()
{
    m_frame = 0;
    delete this;
}

void FrameLoaderClientGtk::dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse& response)
{
    m_response = response;
    m_firstData = true;
}

void FrameLoaderClientGtk::dispatchDecidePolicyForMIMEType(FramePolicyFunction policyFunction, const String&, const ResourceRequest&)
{
    // FIXME: we need to call directly here (comment copied from Qt version)
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
    (core(m_frame)->loader()->*policyFunction)(PolicyUse);
}

void FrameLoaderClientGtk::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction policyFunction, const NavigationAction&, const ResourceRequest&, const String&)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
    // FIXME: I think Qt version marshals this to another thread so when we
    // have multi-threaded download, we might need to do the same
    (core(m_frame)->loader()->*policyFunction)(PolicyIgnore);
}

void FrameLoaderClientGtk::dispatchDecidePolicyForNavigationAction(FramePolicyFunction policyFunction, const NavigationAction&, const ResourceRequest&)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
    (core(m_frame)->loader()->*policyFunction)(PolicyUse);
}

Widget* FrameLoaderClientGtk::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    notImplemented();
    return 0;
}

Frame* FrameLoaderClientGtk::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                        const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    Frame* coreFrame = core(webFrame());

    ASSERT(core(getPageFromFrame(webFrame())) == coreFrame->page());
    WebKitGtkFrame* gtkFrame = WEBKIT_GTK_FRAME(webkit_gtk_frame_init_with_page(getPageFromFrame(webFrame()), ownerElement));
    Frame* childFrame = core(gtkFrame);

    coreFrame->tree()->appendChild(childFrame);

    // Frames are created with a refcount of 1. Release this ref, since we've assigned it to d->frame.
    childFrame->deref();
    childFrame->tree()->setName(name);
    childFrame->init();
    childFrame->loader()->load(url, referrer, FrameLoadTypeRedirectWithLockedHistory, String(), 0, 0);

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    // Propagate the marginwidth/height and scrolling modes to the view.
    if (ownerElement->hasTagName(HTMLNames::frameTag) || ownerElement->hasTagName(HTMLNames::iframeTag)) {
        HTMLFrameElement* frameElt = static_cast<HTMLFrameElement*>(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            childFrame->view()->setScrollbarsMode(ScrollbarAlwaysOff);
        int marginWidth = frameElt->getMarginWidth();
        int marginHeight = frameElt->getMarginHeight();
        if (marginWidth != -1)
            childFrame->view()->setMarginWidth(marginWidth);
        if (marginHeight != -1)
            childFrame->view()->setMarginHeight(marginHeight);
    }

    return childFrame;
}

void FrameLoaderClientGtk::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplemented();
    return;
}

Widget* FrameLoaderClientGtk::createJavaAppletWidget(const IntSize&, Element*, const KURL& baseURL,
                                                    const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoaderClientGtk::objectContentType(const KURL& url, const String& mimeType)
{
    if (!url.isValid())
        return ObjectContentType();

    // TODO: use more than just the extension to determine the content type?
    String rtype = MIMETypeRegistry::getMIMETypeForPath(url.path());
    if (!rtype.isEmpty())
        return ObjectContentFrame;
    return ObjectContentType();
}

String FrameLoaderClientGtk::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClientGtk::windowObjectCleared() const
{
    g_signal_emit_by_name(m_frame, "cleared");
}

void FrameLoaderClientGtk::setMainFrameDocumentReady(bool) 
{
    // this is only interesting once we provide an external API for the DOM
}

bool FrameLoaderClientGtk::hasWebView() const
{
    notImplemented();
    return true;
}

bool FrameLoaderClientGtk::hasFrameView() const
{
    notImplemented();
    return true;
}

void FrameLoaderClientGtk::dispatchDidFinishLoad() 
{ 
    g_signal_emit_by_name(m_frame, "load_done", true);
}

void FrameLoaderClientGtk::frameLoadCompleted() 
{
    notImplemented(); 
}

void FrameLoaderClientGtk::saveViewStateToItem(HistoryItem*)
{
    notImplemented(); 
}

void FrameLoaderClientGtk::restoreViewState()
{
    notImplemented(); 
}

bool FrameLoaderClientGtk::shouldGoToHistoryItem(HistoryItem* item) const 
{
    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return item != 0;
}

bool FrameLoaderClientGtk::privateBrowsingEnabled() const { notImplemented(); return false; }
void FrameLoaderClientGtk::makeDocumentView() { notImplemented(); }
void FrameLoaderClientGtk::makeRepresentation(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGtk::forceLayout() { notImplemented(); }
void FrameLoaderClientGtk::forceLayoutForNonHTML() { notImplemented(); }
void FrameLoaderClientGtk::setCopiesOnScroll() { notImplemented(); }
void FrameLoaderClientGtk::detachedFromParent1() { notImplemented(); }
void FrameLoaderClientGtk::detachedFromParent2() { notImplemented(); }
void FrameLoaderClientGtk::detachedFromParent3() { notImplemented(); }
void FrameLoaderClientGtk::detachedFromParent4() { notImplemented(); }
void FrameLoaderClientGtk::loadedFromCachedPage() { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidHandleOnloadEvents() {notImplemented(); }
void FrameLoaderClientGtk::dispatchDidReceiveServerRedirectForProvisionalLoad() { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidCancelClientRedirect() { notImplemented(); }
void FrameLoaderClientGtk::dispatchWillPerformClientRedirect(const KURL&, double, double) { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidChangeLocationWithinPage() { notImplemented(); }
void FrameLoaderClientGtk::dispatchWillClose() { notImplemented(); }

void FrameLoaderClientGtk::dispatchDidReceiveIcon()
{
    WebKitGtkPage* page = getPageFromFrame(m_frame);

    g_signal_emit_by_name(page, "icon_loaded", m_frame);
}

void FrameLoaderClientGtk::dispatchDidStartProvisionalLoad()
{
}

void FrameLoaderClientGtk::dispatchDidReceiveTitle(const String& title)
{
    notImplemented();
}

void FrameLoaderClientGtk::dispatchDidCommitLoad() { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidFinishDocumentLoad() { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidFirstLayout() { notImplemented(); }
void FrameLoaderClientGtk::dispatchShow() { notImplemented(); }
void FrameLoaderClientGtk::cancelPolicyCheck() { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidLoadMainResource(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGtk::revertToProvisionalState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGtk::clearUnarchivingState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGtk::willChangeTitle(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGtk::didChangeTitle(DocumentLoader *l) { setTitle(l->title(), l->URL()); }
void FrameLoaderClientGtk::finishedLoading(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGtk::finalSetupForReplace(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGtk::setDefersLoading(bool) { notImplemented(); }
bool FrameLoaderClientGtk::isArchiveLoadPending(ResourceLoader*) const { notImplemented(); return false; }
void FrameLoaderClientGtk::cancelPendingArchiveLoad(ResourceLoader*) { notImplemented(); }
void FrameLoaderClientGtk::clearArchivedResources() { notImplemented(); }
bool FrameLoaderClientGtk::canHandleRequest(const ResourceRequest&) const { notImplemented(); return true; }
bool FrameLoaderClientGtk::canShowMIMEType(const String&) const { notImplemented(); return true; }
bool FrameLoaderClientGtk::representationExistsForURLScheme(const String&) const { notImplemented(); return false; }
String FrameLoaderClientGtk::generatedMIMETypeForURLScheme(const String&) const { notImplemented(); return String(); }

void FrameLoaderClientGtk::provisionalLoadStarted()
{
    notImplemented();
}

void FrameLoaderClientGtk::didFinishLoad() {
    notImplemented();
}

void FrameLoaderClientGtk::prepareForDataSourceReplacement() { notImplemented(); }

void FrameLoaderClientGtk::setTitle(const String& title, const KURL& url)
{
    WebKitGtkPage* page = getPageFromFrame(m_frame);

    CString titleString = title.utf8(); 
    DeprecatedCString urlString = url.prettyURL().utf8();
    g_signal_emit_by_name(m_frame, "title_changed", titleString.data(), urlString.data());

    if (m_frame == webkit_gtk_page_get_main_frame(page))
        g_signal_emit_by_name(page, "title_changed", titleString.data(), urlString.data());
}

void FrameLoaderClientGtk::setDocumentViewFromCachedPage(WebCore::CachedPage*) { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long  identifier, int lengthReceived) { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidFinishLoading(DocumentLoader*, unsigned long  identifier) { notImplemented(); }
void FrameLoaderClientGtk::dispatchDidFailLoading(DocumentLoader*, unsigned long  identifier, const ResourceError&) { notImplemented(); }
bool FrameLoaderClientGtk::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length) { notImplemented(); return false; }

void FrameLoaderClientGtk::dispatchDidFailProvisionalLoad(const ResourceError&)
{
    g_signal_emit_by_name(m_frame, "load_done", false);
}

void FrameLoaderClientGtk::dispatchDidFailLoad(const ResourceError&)
{
    g_signal_emit_by_name(m_frame, "load_done", false);
}

void FrameLoaderClientGtk::download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&) { notImplemented(); }
ResourceError FrameLoaderClientGtk::cancelledError(const ResourceRequest&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGtk::blockedError(const ResourceRequest&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGtk::cannotShowURLError(const ResourceRequest&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGtk::interruptForPolicyChangeError(const ResourceRequest&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGtk::cannotShowMIMETypeError(const ResourceResponse&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGtk::fileDoesNotExistError(const ResourceResponse&) { notImplemented(); return ResourceError(); }
bool FrameLoaderClientGtk::shouldFallBack(const ResourceError&) { notImplemented(); return false; }
bool FrameLoaderClientGtk::willUseArchive(ResourceLoader*, const ResourceRequest&, const KURL& originalURL) const { notImplemented(); return false; }
void FrameLoaderClientGtk::saveDocumentViewToCachedPage(CachedPage*) { notImplemented(); }
bool FrameLoaderClientGtk::canCachePage() const { notImplemented(); return false; }
Frame* FrameLoaderClientGtk::dispatchCreatePage() { notImplemented(); return 0; }
void FrameLoaderClientGtk::dispatchUnableToImplementPolicy(const ResourceError&) { notImplemented(); }
void FrameLoaderClientGtk::setMainDocumentError(DocumentLoader*, const ResourceError&) { notImplemented(); }
void FrameLoaderClientGtk::startDownload(const ResourceRequest&) { notImplemented(); }
void FrameLoaderClientGtk::updateGlobalHistoryForStandardLoad(const KURL&) { notImplemented(); }
void FrameLoaderClientGtk::updateGlobalHistoryForReload(const KURL&) { notImplemented(); }

}
