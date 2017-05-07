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

#ifndef VisitedLinkStore_h
#define VisitedLinkStore_h

#include "APIObject.h"
#include "MessageReceiver.h"
#include "VisitedLinkTable.h"
#include "WebPageProxy.h"
#include "WebProcessLifetimeObserver.h"
#include <WebCore/LinkHash.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class WebPageProxy;
class WebProcessProxy;
    
class VisitedLinkStore final : public API::ObjectImpl<API::Object::Type::VisitedLinkStore>, private IPC::MessageReceiver, public WebProcessLifetimeObserver {
public:
    static Ref<VisitedLinkStore> create();

    explicit VisitedLinkStore();
    virtual ~VisitedLinkStore();

    uint64_t identifier() const { return m_identifier; }

    void addProcess(WebProcessProxy&);
    void removeProcess(WebProcessProxy&);

    void addVisitedLinkHash(WebCore::LinkHash);
    void removeAll();

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // WebProcessLifetimeObserver
    void webProcessWillOpenConnection(WebProcessProxy&, IPC::Connection&) override;
    void webProcessDidCloseConnection(WebProcessProxy&, IPC::Connection&) override;

    void addVisitedLinkHashFromPage(uint64_t pageID, WebCore::LinkHash);

    void pendingVisitedLinksTimerFired();

    void resizeTable(unsigned newTableSize);
    void sendTable(WebProcessProxy&);

    HashSet<WebProcessProxy*> m_processes;

    uint64_t m_identifier;

    unsigned m_keyCount;
    unsigned m_tableSize;
    VisitedLinkTable m_table;

    HashSet<WebCore::LinkHash, WebCore::LinkHashHash> m_pendingVisitedLinks;
    RunLoop::Timer<VisitedLinkStore> m_pendingVisitedLinksTimer;
};

} // namespace WebKit

#endif // VisitedLinkStore_h
