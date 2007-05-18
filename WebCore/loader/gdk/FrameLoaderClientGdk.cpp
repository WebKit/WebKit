/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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
#include "FrameLoaderClientGdk.h"
#include "DocumentLoader.h"
#include "FrameGdk.h"
#include "FrameLoader.h"
#include "NotImplementedGdk.h"
#include "PlatformString.h"
#include "ResourceRequest.h"
#include <stdio.h>

namespace WebCore {

FrameLoaderClientGdk::FrameLoaderClientGdk()
    : m_frame(0)
    , m_firstData(false)
{
}

Frame* FrameLoaderClientGdk::frame()
{ 
    return static_cast<Frame*>(m_frame);
}

String FrameLoaderClientGdk::userAgent(const KURL&)
{
    return "Mozilla/5.0 (X11; U; Linux i686; en-US) AppleWebKit/420+ (KHTML, like Gecko)";
}

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClientGdk::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<DocumentLoader> loader = new DocumentLoader(request, substituteData);
    return loader.release();
}

void FrameLoaderClientGdk::dispatchWillSubmitForm(FramePolicyFunction policyFunction,  PassRefPtr<FormState>)
{
    // FIXME: This is surely too simple
    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction)
        return;
    (frame()->loader()->*policyFunction)(PolicyUse);
}


void FrameLoaderClientGdk::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    if (!frame())
        return;
    FrameLoader *fl = loader->frameLoader();
    fl->setEncoding(m_response.textEncodingName(), false);
    fl->addData(data, length);
}

void FrameLoaderClientGdk::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplementedGdk();
}

void FrameLoaderClientGdk::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplementedGdk();
}

void FrameLoaderClientGdk::dispatchWillSendRequest(DocumentLoader*, unsigned long , ResourceRequest&, const ResourceResponse&)
{
    notImplementedGdk();
}

void FrameLoaderClientGdk::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&)
{
    notImplementedGdk();   
}

void FrameLoaderClientGdk::postProgressStartedNotification()
{
    // no progress notification for now
}

void FrameLoaderClientGdk::postProgressEstimateChangedNotification()
{
    // no progress notification for now    
}

void FrameLoaderClientGdk::postProgressFinishedNotification()
{
    // no progress notification for now
}

void FrameLoaderClientGdk::frameLoaderDestroyed()
{
    m_frame = 0;
    delete this;
}

void FrameLoaderClientGdk::dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse& response)
{
    m_response = response;
    m_firstData = true;
}

void FrameLoaderClientGdk::dispatchDecidePolicyForMIMEType(FramePolicyFunction policyFunction, const String&, const ResourceRequest&)
{
    // FIXME: we need to call directly here (comment copied from Qt version)
    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction)
        return;
    (frame()->loader()->*policyFunction)(PolicyUse);
}

void FrameLoaderClientGdk::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction policyFunction, const NavigationAction&, const ResourceRequest&, const String&)
{
    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction)
        return;
    // FIXME: I think Qt version marshals this to another thread so when we
    // have multi-threaded download, we might need to do the same
    (frame()->loader()->*policyFunction)(PolicyIgnore);
}

void FrameLoaderClientGdk::dispatchDecidePolicyForNavigationAction(FramePolicyFunction policyFunction, const NavigationAction&, const ResourceRequest&)
{
    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction)
        return;
    (frame()->loader()->*policyFunction)(PolicyUse);
}

Widget* FrameLoaderClientGdk::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool)
{
    notImplementedGdk();
    return 0;
}

Frame* FrameLoaderClientGdk::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                        const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    notImplementedGdk();
    return 0;
}

void FrameLoaderClientGdk::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplementedGdk();
    return;
}

Widget* FrameLoaderClientGdk::createJavaAppletWidget(const IntSize&, Element*, const KURL& baseURL,
                                                    const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplementedGdk();
    return 0;
}

ObjectContentType FrameLoaderClientGdk::objectContentType(const KURL& url, const String& mimeType)
{
    notImplementedGdk();
    return ObjectContentType();
}

String FrameLoaderClientGdk::overrideMediaType() const
{
    notImplementedGdk();
    return String();
}

void FrameLoaderClientGdk::windowObjectCleared() const
{
    notImplementedGdk();
}

void FrameLoaderClientGdk::setMainFrameDocumentReady(bool) 
{
    // this is only interesting once we provide an external API for the DOM
}

bool FrameLoaderClientGdk::hasWebView() const
{
    notImplementedGdk();
    return true;
}

bool FrameLoaderClientGdk::hasFrameView() const
{
    notImplementedGdk();
    return true;
}

void FrameLoaderClientGdk::dispatchDidFinishLoad() 
{ 
    FrameGdk *frameGdk = static_cast<FrameGdk*>(m_frame);
    frameGdk->onDidFinishLoad();
}

void FrameLoaderClientGdk::frameLoadCompleted() 
{
    notImplementedGdk(); 
}

void FrameLoaderClientGdk::saveViewStateToItem(HistoryItem*)
{
    notImplementedGdk(); 
}

void FrameLoaderClientGdk::restoreViewState()
{
    notImplementedGdk(); 
}

bool FrameLoaderClientGdk::shouldGoToHistoryItem(HistoryItem* item) const 
{
    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return item != 0;
}

bool FrameLoaderClientGdk::privateBrowsingEnabled() const { notImplementedGdk(); return false; }
void FrameLoaderClientGdk::makeDocumentView() { notImplementedGdk(); }
void FrameLoaderClientGdk::makeRepresentation(DocumentLoader*) { notImplementedGdk(); }
void FrameLoaderClientGdk::forceLayout() { notImplementedGdk(); }
void FrameLoaderClientGdk::forceLayoutForNonHTML() { notImplementedGdk(); }
void FrameLoaderClientGdk::setCopiesOnScroll() { notImplementedGdk(); }
void FrameLoaderClientGdk::detachedFromParent1() { notImplementedGdk(); }
void FrameLoaderClientGdk::detachedFromParent2() { notImplementedGdk(); }
void FrameLoaderClientGdk::detachedFromParent3() { notImplementedGdk(); }
void FrameLoaderClientGdk::detachedFromParent4() { notImplementedGdk(); }
void FrameLoaderClientGdk::loadedFromCachedPage() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidHandleOnloadEvents() {notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidReceiveServerRedirectForProvisionalLoad() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidCancelClientRedirect() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchWillPerformClientRedirect(const KURL&, double, double) { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidChangeLocationWithinPage() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchWillClose() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidReceiveIcon() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidStartProvisionalLoad() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidReceiveTitle(const String&) { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidCommitLoad() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidFinishDocumentLoad() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidFirstLayout() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchShow() { notImplementedGdk(); }
void FrameLoaderClientGdk::cancelPolicyCheck() { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidLoadMainResource(DocumentLoader*) { notImplementedGdk(); }
void FrameLoaderClientGdk::revertToProvisionalState(DocumentLoader*) { notImplementedGdk(); }
void FrameLoaderClientGdk::clearUnarchivingState(DocumentLoader*) { notImplementedGdk(); }
void FrameLoaderClientGdk::willChangeTitle(DocumentLoader*) { notImplementedGdk(); }
void FrameLoaderClientGdk::didChangeTitle(DocumentLoader *l) { setTitle(l->title(), l->URL()); }
void FrameLoaderClientGdk::finishedLoading(DocumentLoader*) { notImplementedGdk(); }
void FrameLoaderClientGdk::finalSetupForReplace(DocumentLoader*) { notImplementedGdk(); }
void FrameLoaderClientGdk::setDefersLoading(bool) { notImplementedGdk(); }
bool FrameLoaderClientGdk::isArchiveLoadPending(ResourceLoader*) const { notImplementedGdk(); return false; }
void FrameLoaderClientGdk::cancelPendingArchiveLoad(ResourceLoader*) { notImplementedGdk(); }
void FrameLoaderClientGdk::clearArchivedResources() { notImplementedGdk(); }
bool FrameLoaderClientGdk::canHandleRequest(const ResourceRequest&) const { notImplementedGdk(); return true; }
bool FrameLoaderClientGdk::canShowMIMEType(const String&) const { notImplementedGdk(); return false; }
bool FrameLoaderClientGdk::representationExistsForURLScheme(const String&) const { notImplementedGdk(); return false; }
String FrameLoaderClientGdk::generatedMIMETypeForURLScheme(const String&) const { notImplementedGdk(); return String(); }
void FrameLoaderClientGdk::provisionalLoadStarted() { notImplementedGdk(); }
void FrameLoaderClientGdk::didFinishLoad() { notImplementedGdk(); }
void FrameLoaderClientGdk::prepareForDataSourceReplacement() { notImplementedGdk(); }
void FrameLoaderClientGdk::setTitle(const String&, const KURL&) { notImplementedGdk(); }
void FrameLoaderClientGdk::setDocumentViewFromCachedPage(WebCore::CachedPage*) { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long  identifier, int lengthReceived) { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidFinishLoading(DocumentLoader*, unsigned long  identifier) { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidFailLoading(DocumentLoader*, unsigned long  identifier, const ResourceError&) { notImplementedGdk(); }
bool FrameLoaderClientGdk::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length) { notImplementedGdk(); return false; }
void FrameLoaderClientGdk::dispatchDidFailProvisionalLoad(const ResourceError&) { notImplementedGdk(); }
void FrameLoaderClientGdk::dispatchDidFailLoad(const ResourceError&) { notImplementedGdk(); }
void FrameLoaderClientGdk::download(ResourceHandle*, const ResourceRequest&, const ResourceResponse&) { notImplementedGdk(); }
ResourceError FrameLoaderClientGdk::cancelledError(const ResourceRequest&) { notImplementedGdk(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::blockedError(const ResourceRequest&) { notImplementedGdk(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::cannotShowURLError(const ResourceRequest&) { notImplementedGdk(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::interruptForPolicyChangeError(const ResourceRequest&) { notImplementedGdk(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::cannotShowMIMETypeError(const ResourceResponse&) { notImplementedGdk(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::fileDoesNotExistError(const ResourceResponse&) { notImplementedGdk(); return ResourceError(); }
bool FrameLoaderClientGdk::shouldFallBack(const ResourceError&) { notImplementedGdk(); return false; }
bool FrameLoaderClientGdk::willUseArchive(ResourceLoader*, const ResourceRequest&, const KURL& originalURL) const { notImplementedGdk(); return false; }
void FrameLoaderClientGdk::saveDocumentViewToCachedPage(CachedPage*) { notImplementedGdk(); }
bool FrameLoaderClientGdk::canCachePage() const { notImplementedGdk(); return false; }
Frame* FrameLoaderClientGdk::dispatchCreatePage() { notImplementedGdk(); return 0; }
void FrameLoaderClientGdk::dispatchUnableToImplementPolicy(const ResourceError&) { notImplementedGdk(); }
void FrameLoaderClientGdk::setMainDocumentError(DocumentLoader*, const ResourceError&) { notImplementedGdk(); }
void FrameLoaderClientGdk::startDownload(const ResourceRequest&) { notImplementedGdk(); }
void FrameLoaderClientGdk::updateGlobalHistoryForStandardLoad(const KURL&) { notImplementedGdk(); }
void FrameLoaderClientGdk::updateGlobalHistoryForReload(const KURL&) { notImplementedGdk(); }

}
