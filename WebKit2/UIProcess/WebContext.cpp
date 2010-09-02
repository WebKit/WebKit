/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebContext.h"

#include "ImmutableArray.h"
#include "InjectedBundleMessageKinds.h"
#include "RunLoop.h"
#include "WebContextMessageKinds.h"
#include "WebContextUserMessageCoders.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageNamespace.h"
#include "WebPreferences.h"
#include "WebProcessManager.h"
#include "WebProcessMessageKinds.h"
#include "WebProcessProxy.h"
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>

#include "WKContextPrivate.h"

#include <WebCore/LinkHash.h>

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;

namespace WebKit {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter webContextCounter("WebContext");
#endif

WebContext* WebContext::sharedProcessContext()
{
    RunLoop::initializeMainRunLoop();
    static WebContext* context = adoptRef(new WebContext(ProcessModelSharedSecondaryProcess, String())).leakRef();
    return context;
}

WebContext* WebContext::sharedThreadContext()
{
    RunLoop::initializeMainRunLoop();
    static WebContext* context = adoptRef(new WebContext(ProcessModelSharedSecondaryThread, String())).leakRef();
    return context;
}

PassRefPtr<WebContext> WebContext::create(const String& injectedBundlePath)
{
    RunLoop::initializeMainRunLoop();
    return adoptRef(new WebContext(ProcessModelSecondaryProcess, injectedBundlePath));
}
    
WebContext::WebContext(ProcessModel processModel, const WTF::String& injectedBundlePath)
    : m_processModel(processModel)
    , m_injectedBundlePath(injectedBundlePath)
    , m_visitedLinkProvider(this)
#if PLATFORM(WIN)
    , m_shouldPaintNativeControls(true)
#endif
{
    RunLoop::initializeMainRunLoop();

    m_preferences = WebPreferences::shared();
    m_preferences->addContext(this);

#ifndef NDEBUG
    webContextCounter.increment();
#endif
}

WebContext::~WebContext()
{
    ASSERT(m_pageNamespaces.isEmpty());
    m_preferences->removeContext(this);

#ifndef NDEBUG
    webContextCounter.decrement();
#endif
}

void WebContext::initializeInjectedBundleClient(const WKContextInjectedBundleClient* client)
{
    m_injectedBundleClient.initialize(client);
}

void WebContext::initializeHistoryClient(const WKContextHistoryClient* client)
{
    m_historyClient.initialize(client);
    
    if (!hasValidProcess())
        return;
        
    m_process->send(WebProcessMessage::SetShouldTrackVisitedLinks, 0, CoreIPC::In(m_historyClient.shouldTrackVisitedLinks()));
}

void WebContext::ensureWebProcess()
{
    if (hasValidProcess())
        return;

    m_process = WebProcessManager::shared().getWebProcess(this);

    m_process->send(WebProcessMessage::SetShouldTrackVisitedLinks, 0, CoreIPC::In(m_historyClient.shouldTrackVisitedLinks()));

    for (HashSet<String>::iterator it = m_schemesToRegisterAsEmptyDocument.begin(), end = m_schemesToRegisterAsEmptyDocument.end(); it != end; ++it)
        m_process->send(WebProcessMessage::RegisterURLSchemeAsEmptyDocument, 0, CoreIPC::In(*it));

    platformSetUpWebProcess();
}

void WebContext::processDidFinishLaunching(WebProcessProxy* process)
{
    // FIXME: Once we support multiple processes per context, this assertion won't hold.
    ASSERT(process == m_process);

    m_visitedLinkProvider.populateVisitedLinksIfNeeded();
}

WebPageProxy* WebContext::createWebPage(WebPageNamespace* pageNamespace)
{
    ensureWebProcess();
    return m_process->createWebPage(pageNamespace);
}

void WebContext::reviveIfNecessary()
{
    ensureWebProcess();
}

WebPageNamespace* WebContext::createPageNamespace()
{
    RefPtr<WebPageNamespace> pageNamespace = WebPageNamespace::create(this);
    m_pageNamespaces.add(pageNamespace.get());
    return pageNamespace.release().releaseRef();
}

void WebContext::pageNamespaceWasDestroyed(WebPageNamespace* pageNamespace)
{
    ASSERT(m_pageNamespaces.contains(pageNamespace));
    m_pageNamespaces.remove(pageNamespace);
}

void WebContext::setPreferences(WebPreferences* preferences)
{
    ASSERT(preferences);

    if (preferences == m_preferences)
        return;

    m_preferences->removeContext(this);
    m_preferences = preferences;
    m_preferences->addContext(this);

    // FIXME: Update all Pages/PageNamespace with the new WebPreferences.
}

WebPreferences* WebContext::preferences() const
{
    return m_preferences.get();
}

void WebContext::preferencesDidChange()
{
    if (!m_process)
        return;

    for (HashSet<WebPageNamespace*>::iterator it = m_pageNamespaces.begin(), end = m_pageNamespaces.end(); it != end; ++it) {
        WebPageNamespace* pageNamespace = *it;
        pageNamespace->preferencesDidChange();
    }
}

void WebContext::postMessageToInjectedBundle(const String& messageName, APIObject* messageBody)
{
    if (!hasValidProcess())
        return;

    // FIXME: We should consider returning false from this function if the messageBody cannot
    // be encoded.
    m_process->send(InjectedBundleMessage::PostMessage, 0, CoreIPC::In(messageName, WebContextUserMessageEncoder(messageBody)));
}

// InjectedBundle client

void WebContext::didReceiveMessageFromInjectedBundle(const String& messageName, APIObject* messageBody)
{
    m_injectedBundleClient.didReceiveMessageFromInjectedBundle(this, messageName, messageBody);
}

// HistoryClient

void WebContext::didNavigateWithNavigationData(WebFrameProxy* frame, const WebNavigationDataStore& store) 
{
    ASSERT(frame->page());
    m_historyClient.didNavigateWithNavigationData(this, frame->page(), store, frame);
}

void WebContext::didPerformClientRedirect(WebFrameProxy* frame, const String& sourceURLString, const String& destinationURLString)
{
    ASSERT(frame->page());
    m_historyClient.didPerformClientRedirect(this, frame->page(), sourceURLString, destinationURLString, frame);
}

void WebContext::didPerformServerRedirect(WebFrameProxy* frame, const String& sourceURLString, const String& destinationURLString)
{
    ASSERT(frame->page());
    m_historyClient.didPerformServerRedirect(this, frame->page(), sourceURLString, destinationURLString, frame);
}

void WebContext::didUpdateHistoryTitle(WebFrameProxy* frame, const String& title, const String& url)
{
    ASSERT(frame->page());
    m_historyClient.didUpdateHistoryTitle(this, frame->page(), title, url, frame);
}

void WebContext::populateVisitedLinks()
{
    m_historyClient.populateVisitedLinks(this);
}

void WebContext::getStatistics(WKContextStatistics* statistics)
{
    memset(statistics, 0, sizeof(WKContextStatistics));

    statistics->numberOfWKPageNamespaces = m_pageNamespaces.size();

    for (HashSet<WebPageNamespace*>::iterator it = m_pageNamespaces.begin(), end = m_pageNamespaces.end(); it != end; ++it)
        (*it)->getStatistics(statistics);
}

void WebContext::setAdditionalPluginsDirectory(const WTF::String& directory)
{
    Vector<String> directories;
    directories.append(directory);

    m_pluginInfoStore.setAdditionalPluginsDirectories(directories);
}

void WebContext::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    m_schemesToRegisterAsEmptyDocument.add(urlScheme);

    if (!hasValidProcess())
        return;

    m_process->send(WebProcessMessage::RegisterURLSchemeAsEmptyDocument, 0, CoreIPC::In(urlScheme));
}

void WebContext::addVisitedLink(const String& visitedURL)
{
    if (visitedURL.isEmpty())
        return;

    LinkHash linkHash = visitedLinkHash(visitedURL.characters(), visitedURL.length());
    addVisitedLink(linkHash);
}

void WebContext::addVisitedLink(LinkHash linkHash)
{
    m_visitedLinkProvider.addVisitedLink(linkHash);
}
        
void WebContext::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<WebContextMessage::Kind>()) {
        case WebContextMessage::PostMessage: {
            String messageName;
            RefPtr<APIObject> messageBody;
            WebContextUserMessageDecoder messageDecoder(messageBody, this);
            if (!arguments->decode(CoreIPC::Out(messageName, messageDecoder)))
                return;

            didReceiveMessageFromInjectedBundle(messageName, messageBody.get());
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

} // namespace WebKit
