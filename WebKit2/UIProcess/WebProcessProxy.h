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

#ifndef WebProcessProxy_h
#define WebProcessProxy_h

#include "Connection.h"
#include "PlatformProcessIdentifier.h"
#include "ProcessModel.h"
#include "ResponsivenessTimer.h"
#include "WebPageProxy.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebPageNamespace;

class WebProcessProxy : public RefCounted<WebProcessProxy>, CoreIPC::Connection::Client, ResponsivenessTimer::Client {
public:
    typedef HashMap<uint64_t, RefPtr<WebPageProxy> > WebPageProxyMap;
    typedef WebPageProxyMap::const_iterator::Values pages_const_iterator;

    static PassRefPtr<WebProcessProxy> create(ProcessModel);
    ~WebProcessProxy();

    void terminate();

    CoreIPC::Connection* connection() const { return m_connection.get(); }

    WebPageProxy* webPage(uint64_t pageID) const;
    WebPageProxy* createWebPage(WebPageNamespace*);
    void addExistingWebPage(WebPageProxy*, uint64_t pageID);
    void removeWebPage(uint64_t pageID);

    pages_const_iterator pages_begin();
    pages_const_iterator pages_end();
    size_t numberOfPages();

    ResponsivenessTimer* responsivenessTimer() { return &m_responsivenessTimer; }

    bool isValid() const { return m_connection; }

    ProcessModel processModel() const { return m_processModel; }

    PlatformProcessIdentifier processIdentifier() const { return m_platformProcessIdentifier; }

private:
    explicit WebProcessProxy(ProcessModel);
    
    void connect();

    // CoreIPC::Connection::Client
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);
    void didClose(CoreIPC::Connection*);
        
    // ResponsivenessTimer::Client
    void didBecomeUnresponsive(ResponsivenessTimer*);
    void didBecomeResponsive(ResponsivenessTimer*);

    ResponsivenessTimer m_responsivenessTimer;
    RefPtr<CoreIPC::Connection> m_connection;
    PlatformProcessIdentifier m_platformProcessIdentifier;

    ProcessModel m_processModel;

    // NOTE: This map is for pages in all WebPageNamespaces that use this process.
    WebPageProxyMap m_pageMap;
};

} // namespace WebKit

#endif // WebProcessProxy_h
