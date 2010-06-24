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

#include "WebProcessProxy.h"

#include "PluginInfoStore.h"
#include "WebContext.h"
#include "WebPageNamespace.h"
#include "WebPageProxy.h"
#include "WebProcessLauncher.h"
#include "WebProcessManager.h"
#include "WebProcessMessageKinds.h"
#include "WebProcessProxyMessageKinds.h"
#include <WebCore/PlatformString.h>

using namespace WebCore;

namespace WebKit {

static uint64_t generatePageID()
{
    static uint64_t uniquePageID = 1;
    return uniquePageID++;
}

PassRefPtr<WebProcessProxy> WebProcessProxy::create(WebContext* context)
{
    return adoptRef(new WebProcessProxy(context));
}

WebProcessProxy::WebProcessProxy(WebContext* context)
    : m_responsivenessTimer(this)
    , m_context(context)
{
    connect();

    // FIXME: We could instead send the bundle path as part of the arguments to process creation?
    // Would that be better than sending a connection?
    if (!context->injectedBundlePath().isEmpty())
        connection()->send(WebProcessMessage::LoadInjectedBundle, 0, CoreIPC::In(context->injectedBundlePath()));
}

WebProcessProxy::~WebProcessProxy()
{
    ASSERT(!m_connection);
}

void WebProcessProxy::connect()
{
    ProcessInfo info = launchWebProcess(this, m_context->processModel() == ProcessModelSharedSecondaryThread);
    if (!info.connection) {
        // FIXME: Report an error.
        ASSERT(false);
        return;
    }

    m_connection = info.connection;
    m_platformProcessIdentifier = info.processIdentifier;
}

void WebProcessProxy::terminate()
{
#if PLATFORM(MAC)
    kill(m_platformProcessIdentifier, SIGKILL);
#elif PLATFORM(WIN)
    ::TerminateProcess(m_platformProcessIdentifier, 0);
#endif
}

WebPageProxy* WebProcessProxy::webPage(uint64_t pageID) const
{
    return m_pageMap.get(pageID).get();
}

WebPageProxy* WebProcessProxy::createWebPage(WebPageNamespace* pageNamespace)
{
    ASSERT(pageNamespace->process() == this);

    unsigned pageID = generatePageID();
    RefPtr<WebPageProxy> webPage = WebPageProxy::create(pageNamespace, pageID);
    m_pageMap.set(pageID, webPage);
    return webPage.get();
}

void WebProcessProxy::addExistingWebPage(WebPageProxy* webPage, uint64_t pageID)
{
    m_pageMap.set(pageID, webPage);
}

void WebProcessProxy::removeWebPage(uint64_t pageID)
{
    m_pageMap.remove(pageID);
}

WebProcessProxy::pages_const_iterator WebProcessProxy::pages_begin()
{
    return m_pageMap.begin().values();
}

WebProcessProxy::pages_const_iterator WebProcessProxy::pages_end()
{
    return m_pageMap.end().values();
}

size_t WebProcessProxy::numberOfPages()
{
    return m_pageMap.size();
}

void WebProcessProxy::forwardMessageToWebContext(const String& message)
{
    m_context->didRecieveMessageFromInjectedBundle(message);
}

void WebProcessProxy::getPlugins(bool refresh, Vector<PluginInfo>& plugins)
{
    if (refresh)
        PluginInfoStore::shared().refresh();
    PluginInfoStore::shared().getPlugins(plugins);
}

void WebProcessProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebProcessProxy>()) {
        switch (messageID.get<WebProcessProxyMessage::Kind>()) {
            case WebProcessProxyMessage::PostMessage: {
                String message;
                if (!arguments->decode(CoreIPC::Out(message)))
                    return;

                forwardMessageToWebContext(message);
                return;
            }
                
            // These are synchronous messages and should never be handled here.
            case WebProcessProxyMessage::GetPlugins:
                ASSERT_NOT_REACHED();
                break;
        }
    }

    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return;

    WebPageProxy* pageProxy = webPage(pageID);
    if (!pageProxy)
        return;
    
    pageProxy->didReceiveMessage(connection, messageID, *arguments);    
}

void WebProcessProxy::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    if (messageID.is<CoreIPC::MessageClassWebProcessProxy>()) {
        switch (messageID.get<WebProcessProxyMessage::Kind>()) {
            case WebProcessProxyMessage::GetPlugins: {
                bool refresh;
                if (!arguments->decode(refresh))
                    return;
                
                // FIXME: We should not do this on the main thread!
                Vector<PluginInfo> plugins;
                getPlugins(refresh, plugins);

                reply->encode(plugins);
                break;
            }

            // These are asynchronous messages and should never be handled here.
            case WebProcessProxyMessage::PostMessage:
                ASSERT_NOT_REACHED();
                break;
        }
    }
    
    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return;
    
    WebPageProxy* pageProxy = webPage(pageID);
    if (!pageProxy)
        return;
    
    pageProxy->didReceiveSyncMessage(connection, messageID, *arguments, *reply);
}

void WebProcessProxy::didClose(CoreIPC::Connection*)
{
    m_connection = 0;
    m_responsivenessTimer.stop();

    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);

    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidExit();

    // This may cause us to be deleted.
    WebProcessManager::shared().processDidClose(this, m_context);
}

void WebProcessProxy::didBecomeUnresponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidBecomeUnresponsive();
}

void WebProcessProxy::didBecomeResponsive(ResponsivenessTimer*)
{
    Vector<RefPtr<WebPageProxy> > pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0, size = pages.size(); i < size; ++i)
        pages[i]->processDidBecomeResponsive();
}

} // namespace WebKit
