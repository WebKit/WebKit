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

#include "RunLoop.h"
#include "WebCoreTypeArgumentMarshalling.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPreferencesStore.h"
#include "WebProcessMessageKinds.h"
#include <wtf/PassRefPtr.h>

#ifndef NDEBUG
#include <WebCore/Cache.h>
#include <WebCore/GCController.h>
#endif

using namespace WebCore;

namespace WebKit {

WebProcess& WebProcess::shared()
{
    static WebProcess& process = *new WebProcess;
    return process;
}

WebProcess::WebProcess()
    : m_inDidClose(false)
{
}

void WebProcess::initialize(CoreIPC::Connection::Identifier serverIdentifier, RunLoop* runLoop)
{
    ASSERT(!m_connection);

    m_connection = CoreIPC::Connection::createClientConnection(serverIdentifier, this, runLoop);
    m_connection->open();

    m_runLoop = runLoop;
}

WebPage* WebProcess::webPage(uint64_t pageID) const
{
    return m_pageMap.get(pageID).get();
}

WebPage* WebProcess::createWebPage(uint64_t pageID, const IntSize& viewSize, const WebPreferencesStore& store, DrawingArea::Type drawingAreaType)
{
    // It is necessary to check for page existence here since during a window.open() (or targeted
    // link) the WebPage gets created both in the synchronous handler and through the normal way. 
    std::pair<HashMap<uint64_t, RefPtr<WebPage> >::iterator, bool> result = m_pageMap.add(pageID, 0);
    if (result.second) {
        ASSERT(!result.first->second);
        result.first->second = WebPage::create(pageID, viewSize, store, drawingAreaType);
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
            case WebProcessMessage::Create: {
                uint64_t pageID = arguments->destinationID();
                IntSize viewSize;
                WebPreferencesStore store;
                uint32_t drawingAreaType;
                if (!arguments->decode(CoreIPC::Out(viewSize, store, drawingAreaType)))
                    return;

                createWebPage(pageID, viewSize, store, static_cast<DrawingArea::Type>(drawingAreaType));
                return;
            }
        }
    }

    uint64_t pageID = arguments->destinationID();
    if (!pageID)
        return;
    
    WebPage* page = webPage(pageID);
    if (!page)
        return;
    
    page->didReceiveMessage(connection, messageID, *arguments);    
}

void WebProcess::didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*)
{
    // The web process should never ever get sync messages.
    ASSERT_NOT_REACHED();
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

} // namespace WebKit
