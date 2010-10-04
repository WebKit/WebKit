/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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

#include "WebProcess.h"

#include "InjectedBundle.h"
#include "InjectedBundleMessageKinds.h"
#include "RunLoop.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPlatformStrategies.h"
#include "WebPreferencesStore.h"
#include "WebProcessProxyMessageKinds.h"
#include "WebProcessMessageKinds.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/Page.h>
#include <WebCore/PageGroup.h>
#include <WebCore/SchemeRegistry.h>
#include <WebCore/Settings.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RandomNumber.h>

#if PLATFORM(MAC)
#include "MachPort.h"
#endif

#ifndef NDEBUG
#include <WebCore/Cache.h>
#include <WebCore/GCController.h>
#endif

using namespace WebCore;

namespace WebKit {

#if OS(WINDOWS)
static void sleep(unsigned seconds)
{
    ::Sleep(seconds * 1000);
}
#endif

static void* randomCrashThread(void*)
{
    // This delay was chosen semi-arbitrarily. We want the crash to happen somewhat quickly to
    // enable useful stress testing, but not so quickly that the web process will always crash soon
    // after launch.
    static const unsigned maximumRandomCrashDelay = 180;

    sleep(randomNumber() * maximumRandomCrashDelay);
    CRASH();
    return 0;
}

static void startRandomCrashThreadIfRequested()
{
    if (!getenv("WEBKIT2_CRASH_WEB_PROCESS_RANDOMLY"))
        return;
    createThread(randomCrashThread, 0, "WebKit2: Random Crash Thread");
}

WebProcess& WebProcess::shared()
{
    static WebProcess& process = *new WebProcess;
    return process;
}

WebProcess::WebProcess()
    : m_inDidClose(false)
    , m_hasSetCacheModel(false)
    , m_cacheModel(CacheModelDocumentViewer)
#if USE(ACCELERATED_COMPOSITING) && PLATFORM(MAC)
    , m_compositingRenderServerPort(MACH_PORT_NULL)
#endif
{
#if USE(PLATFORM_STRATEGIES)
    // Initialize our platform strategies.
    WebPlatformStrategies::initialize();
#endif // USE(PLATFORM_STRATEGIES)
}

void WebProcess::initialize(CoreIPC::Connection::Identifier serverIdentifier, RunLoop* runLoop)
{
    ASSERT(!m_connection);

    m_connection = CoreIPC::Connection::createClientConnection(serverIdentifier, this, runLoop);
    m_connection->open();

    m_runLoop = runLoop;

    startRandomCrashThreadIfRequested();
}

#if ENABLE(WEB_PROCESS_SANDBOX)
void WebProcess::loadInjectedBundle(const String& path, const String& token)
#else
void WebProcess::loadInjectedBundle(const String& path)
#endif
{
    ASSERT(m_pageMap.isEmpty());
    ASSERT(!path.isEmpty());

    m_injectedBundle = InjectedBundle::create(path);
#if ENABLE(WEB_PROCESS_SANDBOX)
    m_injectedBundle->setSandboxToken(token);
#endif

    if (!m_injectedBundle->load()) {
        // Don't keep around the InjectedBundle reference if the load fails.
        m_injectedBundle.clear();
    }
}

void WebProcess::setApplicationCacheDirectory(const String& directory)
{
    cacheStorage().setCacheDirectory(directory);
}

void WebProcess::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    SchemeRegistry::registerURLSchemeAsEmptyDocument(urlScheme);
}

void WebProcess::setVisitedLinkTable(const SharedMemory::Handle& handle)
{
    RefPtr<SharedMemory> sharedMemory = SharedMemory::create(handle, SharedMemory::ReadOnly);
    if (!sharedMemory)
        return;

    m_visitedLinkTable.setSharedMemory(sharedMemory.release());
}

PageGroup* WebProcess::sharedPageGroup()
{
    return PageGroup::pageGroup("WebKit2Group");
}

void WebProcess::visitedLinkStateChanged(const Vector<WebCore::LinkHash>& linkHashes)
{
    for (size_t i = 0; i < linkHashes.size(); ++i)
        Page::visitedStateChanged(sharedPageGroup(), linkHashes[i]);
}

void WebProcess::allVisitedLinkStateChanged()
{
    Page::allVisitedStateChanged(sharedPageGroup());
}

bool WebProcess::isLinkVisited(LinkHash linkHash) const
{
    return m_visitedLinkTable.isLinkVisited(linkHash);
}

void WebProcess::addVisitedLink(WebCore::LinkHash linkHash)
{
    if (isLinkVisited(linkHash))
        return;

    m_connection->send(WebProcessProxyMessage::AddVisitedLink, 0, CoreIPC::In(linkHash));
}

void WebProcess::setCacheModel(CacheModel cacheModel)
{
    if (!m_hasSetCacheModel || cacheModel != m_cacheModel) {
        m_hasSetCacheModel = true;
        m_cacheModel = cacheModel;
        platformSetCacheModel(cacheModel);
    }
}

WebPage* WebProcess::webPage(uint64_t pageID) const
{
    return m_pageMap.get(pageID).get();
}

WebPage* WebProcess::createWebPage(uint64_t pageID, const WebPageCreationParameters& parameters)
{
    // It is necessary to check for page existence here since during a window.open() (or targeted
    // link) the WebPage gets created both in the synchronous handler and through the normal way. 
    std::pair<HashMap<uint64_t, RefPtr<WebPage> >::iterator, bool> result = m_pageMap.add(pageID, 0);
    if (result.second) {
        ASSERT(!result.first->second);
        result.first->second = WebPage::create(pageID, parameters);
    }

    ASSERT(result.first->second);
    return result.first->second.get();
}

void WebProcess::removeWebPage(uint64_t pageID)
{
    m_pageMap.remove(pageID);

    // If we don't have any pages left, shut down.
    if (m_pageMap.isEmpty() && !m_inDidClose)
        shutdown();
}

bool WebProcess::isSeparateProcess() const
{
    // If we're running on the main run loop, we assume that we're in a separate process.
    return m_runLoop == RunLoop::main();
}
 
void WebProcess::shutdown()
{
    // Keep running forever if we're running in the same process.
    if (!isSeparateProcess())
        return;

#ifndef NDEBUG
    gcController().garbageCollectNow();
    cache()->setDisabled(true);
#endif

    // Invalidate our connection.
    m_connection->invalidate();
    m_connection = 0;

    m_runLoop->stop();
}

void WebProcess::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassWebProcess>()) {
        switch (messageID.get<WebProcessMessage::Kind>()) {
            case WebProcessMessage::SetVisitedLinkTable: {
                SharedMemory::Handle handle;
                if (!arguments->decode(CoreIPC::Out(handle)))
                    return;
                
                setVisitedLinkTable(handle);
                return;
            }
            case WebProcessMessage::VisitedLinkStateChanged: {
                Vector<LinkHash> linkHashes;
                if (!arguments->decode(CoreIPC::Out(linkHashes)))
                    return;
                visitedLinkStateChanged(linkHashes);
                return;
            }
            case WebProcessMessage::AllVisitedLinkStateChanged:
                allVisitedLinkStateChanged();
                return;
            
            case WebProcessMessage::LoadInjectedBundle: {
                String path;

#if ENABLE(WEB_PROCESS_SANDBOX)
                String token;
                if (!arguments->decode(CoreIPC::Out(path, token)))
                    return;

                loadInjectedBundle(path, token);
                return;
#else
                if (!arguments->decode(CoreIPC::Out(path)))
                    return;

                loadInjectedBundle(path);
                return;
#endif
            }
            case WebProcessMessage::SetApplicationCacheDirectory: {
                String directory;
                if (!arguments->decode(CoreIPC::Out(directory)))
                    return;
                
                setApplicationCacheDirectory(directory);
                return;
            }
            case WebProcessMessage::SetShouldTrackVisitedLinks: {
                bool shouldTrackVisitedLinks;
                if (!arguments->decode(CoreIPC::Out(shouldTrackVisitedLinks)))
                    return;
                
                PageGroup::setShouldTrackVisitedLinks(shouldTrackVisitedLinks);
                return;
            }
            case WebProcessMessage::SetCacheModel: {
                uint32_t cacheModel;
                if (!arguments->decode(CoreIPC::Out(cacheModel)))
                    return;
                
                setCacheModel(static_cast<CacheModel>(cacheModel));
                return;
            }
            case WebProcessMessage::Create: {
                uint64_t pageID = arguments->destinationID();
                WebPageCreationParameters parameters;
                if (!arguments->decode(CoreIPC::Out(parameters)))
                    return;

                createWebPage(pageID, parameters);
                return;
            }
            case WebProcessMessage::RegisterURLSchemeAsEmptyDocument: {
                String message;
                if (!arguments->decode(CoreIPC::Out(message)))
                    return;

                registerURLSchemeAsEmptyDocument(message);
                return;
            }
#if USE(ACCELERATED_COMPOSITING) && PLATFORM(MAC)
            case WebProcessMessage::SetupAcceleratedCompositingPort: {
                CoreIPC::MachPort port;
                if (!arguments->decode(port))
                    return;

                m_compositingRenderServerPort = port.port();
                return;
            }
#endif
#if PLATFORM(WIN)
            case WebProcessMessage::SetShouldPaintNativeControls: {
                bool b;
                if (!arguments->decode(b))
                    return;
#if USE(SAFARI_THEME)
                Settings::setShouldPaintNativeControls(b);
#endif
                return;
            }
#endif
        }
    }

    if (messageID.is<CoreIPC::MessageClassInjectedBundle>()) {
        if (!m_injectedBundle)
            return;
        m_injectedBundle->didReceiveMessage(connection, messageID, arguments);    
        return;
    }

    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return;
    
    WebPage* page = webPage(pageID);
    if (!page)
        return;
    
    page->didReceiveMessage(connection, messageID, arguments);
}

void WebProcess::didClose(CoreIPC::Connection*)
{
    // When running in the same process the connection will never be closed.
    ASSERT(isSeparateProcess());

#ifndef NDEBUG
    m_inDidClose = true;

    // Close all the live pages.
    Vector<RefPtr<WebPage> > pages;
    copyValuesToVector(m_pageMap, pages);
    for (size_t i = 0; i < pages.size(); ++i)
        pages[i]->close();
    pages.clear();

    gcController().garbageCollectNow();
    cache()->setDisabled(true);
#endif    

    // The UI process closed this connection, shut down.
    m_runLoop->stop();
}

void WebProcess::didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::MessageID)
{
    // We received an invalid message, but since this is from the UI process (which we trust),
    // we'll let it slide.
}

WebFrame* WebProcess::webFrame(uint64_t frameID) const
{
    return m_frameMap.get(frameID);
}

void WebProcess::addWebFrame(uint64_t frameID, WebFrame* frame)
{
    m_frameMap.set(frameID, frame);
}

void WebProcess::removeWebFrame(uint64_t frameID)
{
    m_frameMap.remove(frameID);

    // We can end up here after our connection has closed when WebCore's frame life-support timer
    // fires when the application is shutting down. There's no need (and no way) to update the UI
    // process in this case.
    if (!m_connection)
        return;
    m_connection->send(WebProcessProxyMessage::DidDestroyFrame, 0, CoreIPC::In(frameID));
}

} // namespace WebKit
