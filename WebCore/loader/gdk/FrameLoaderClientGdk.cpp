/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
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
#include "MimeTypeRegistry.h"
#include "NotImplemented.h"
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
    notImplemented();
}

void FrameLoaderClientGdk::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClientGdk::dispatchWillSendRequest(DocumentLoader*, unsigned long , ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClientGdk::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();   
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
    notImplemented();
    return 0;
}

Frame* FrameLoaderClientGdk::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                        const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    notImplemented();
    return 0;
}

void FrameLoaderClientGdk::redirectDataToPlugin(Widget* pluginWidget)
{
    notImplemented();
    return;
}

Widget* FrameLoaderClientGdk::createJavaAppletWidget(const IntSize&, Element*, const KURL& baseURL,
                                                    const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoaderClientGdk::objectContentType(const KURL& url, const String& mimeType)
{
    if (!url.isValid())
        return ObjectContentType();

    // TODO: use more than just the extension to determine the content type?
    String rtype = MimeTypeRegistry::getMIMETypeForPath(url.path());
    if (!rtype.isEmpty())
        return ObjectContentFrame;
    return ObjectContentType();
}

String FrameLoaderClientGdk::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClientGdk::windowObjectCleared() const
{
    notImplemented();
}

void FrameLoaderClientGdk::setMainFrameDocumentReady(bool) 
{
    // this is only interesting once we provide an external API for the DOM
}

bool FrameLoaderClientGdk::hasWebView() const
{
    notImplemented();
    return true;
}

bool FrameLoaderClientGdk::hasFrameView() const
{
    notImplemented();
    return true;
}

void FrameLoaderClientGdk::dispatchDidFinishLoad() 
{ 
    FrameGdk *frameGdk = static_cast<FrameGdk*>(m_frame);
    frameGdk->onDidFinishLoad();
}

void FrameLoaderClientGdk::frameLoadCompleted() 
{
    notImplemented(); 
}

void FrameLoaderClientGdk::saveViewStateToItem(HistoryItem*)
{
    notImplemented(); 
}

void FrameLoaderClientGdk::restoreViewState()
{
    notImplemented(); 
}

bool FrameLoaderClientGdk::shouldGoToHistoryItem(HistoryItem* item) const 
{
    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return item != 0;
}

bool FrameLoaderClientGdk::privateBrowsingEnabled() const { notImplemented(); return false; }
void FrameLoaderClientGdk::makeDocumentView() { notImplemented(); }
void FrameLoaderClientGdk::makeRepresentation(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGdk::forceLayout() { notImplemented(); }
void FrameLoaderClientGdk::forceLayoutForNonHTML() { notImplemented(); }
void FrameLoaderClientGdk::setCopiesOnScroll() { notImplemented(); }
void FrameLoaderClientGdk::detachedFromParent1() { notImplemented(); }
void FrameLoaderClientGdk::detachedFromParent2() { notImplemented(); }
void FrameLoaderClientGdk::detachedFromParent3() { notImplemented(); }
void FrameLoaderClientGdk::detachedFromParent4() { notImplemented(); }
void FrameLoaderClientGdk::loadedFromCachedPage() { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidHandleOnloadEvents() {notImplemented(); }
void FrameLoaderClientGdk::dispatchDidReceiveServerRedirectForProvisionalLoad() { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidCancelClientRedirect() { notImplemented(); }
void FrameLoaderClientGdk::dispatchWillPerformClientRedirect(const KURL&, double, double) { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidChangeLocationWithinPage() { notImplemented(); }
void FrameLoaderClientGdk::dispatchWillClose() { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidReceiveIcon() { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidStartProvisionalLoad() { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidReceiveTitle(const String&) { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidCommitLoad() { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidFinishDocumentLoad() { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidFirstLayout() { notImplemented(); }
void FrameLoaderClientGdk::dispatchShow() { notImplemented(); }
void FrameLoaderClientGdk::cancelPolicyCheck() { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidLoadMainResource(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGdk::revertToProvisionalState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGdk::clearUnarchivingState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGdk::willChangeTitle(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGdk::didChangeTitle(DocumentLoader *l) { setTitle(l->title(), l->URL()); }
void FrameLoaderClientGdk::finishedLoading(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGdk::finalSetupForReplace(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientGdk::setDefersLoading(bool) { notImplemented(); }
bool FrameLoaderClientGdk::isArchiveLoadPending(ResourceLoader*) const { notImplemented(); return false; }
void FrameLoaderClientGdk::cancelPendingArchiveLoad(ResourceLoader*) { notImplemented(); }
void FrameLoaderClientGdk::clearArchivedResources() { notImplemented(); }
bool FrameLoaderClientGdk::canHandleRequest(const ResourceRequest&) const { notImplemented(); return true; }
bool FrameLoaderClientGdk::canShowMIMEType(const String&) const { notImplemented(); return true; }
bool FrameLoaderClientGdk::representationExistsForURLScheme(const String&) const { notImplemented(); return false; }
String FrameLoaderClientGdk::generatedMIMETypeForURLScheme(const String&) const { notImplemented(); return String(); }
void FrameLoaderClientGdk::provisionalLoadStarted() { notImplemented(); }
void FrameLoaderClientGdk::didFinishLoad() { notImplemented(); }
void FrameLoaderClientGdk::prepareForDataSourceReplacement() { notImplemented(); }
void FrameLoaderClientGdk::setTitle(const String&, const KURL&) { notImplemented(); }
void FrameLoaderClientGdk::setDocumentViewFromCachedPage(WebCore::CachedPage*) { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long  identifier, int lengthReceived) { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidFinishLoading(DocumentLoader*, unsigned long  identifier) { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidFailLoading(DocumentLoader*, unsigned long  identifier, const ResourceError&) { notImplemented(); }
bool FrameLoaderClientGdk::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length) { notImplemented(); return false; }
void FrameLoaderClientGdk::dispatchDidFailProvisionalLoad(const ResourceError&) { notImplemented(); }
void FrameLoaderClientGdk::dispatchDidFailLoad(const ResourceError&) { notImplemented(); }
void FrameLoaderClientGdk::download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&) { notImplemented(); }
ResourceError FrameLoaderClientGdk::cancelledError(const ResourceRequest&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::blockedError(const ResourceRequest&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::cannotShowURLError(const ResourceRequest&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::interruptForPolicyChangeError(const ResourceRequest&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::cannotShowMIMETypeError(const ResourceResponse&) { notImplemented(); return ResourceError(); }
ResourceError FrameLoaderClientGdk::fileDoesNotExistError(const ResourceResponse&) { notImplemented(); return ResourceError(); }
bool FrameLoaderClientGdk::shouldFallBack(const ResourceError&) { notImplemented(); return false; }
bool FrameLoaderClientGdk::willUseArchive(ResourceLoader*, const ResourceRequest&, const KURL& originalURL) const { notImplemented(); return false; }
void FrameLoaderClientGdk::saveDocumentViewToCachedPage(CachedPage*) { notImplemented(); }
bool FrameLoaderClientGdk::canCachePage() const { notImplemented(); return false; }
Frame* FrameLoaderClientGdk::dispatchCreatePage() { notImplemented(); return 0; }
void FrameLoaderClientGdk::dispatchUnableToImplementPolicy(const ResourceError&) { notImplemented(); }
void FrameLoaderClientGdk::setMainDocumentError(DocumentLoader*, const ResourceError&) { notImplemented(); }
void FrameLoaderClientGdk::startDownload(const ResourceRequest&) { notImplemented(); }
void FrameLoaderClientGdk::updateGlobalHistoryForStandardLoad(const KURL&) { notImplemented(); }
void FrameLoaderClientGdk::updateGlobalHistoryForReload(const KURL&) { notImplemented(); }

}
