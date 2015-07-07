/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef VisitedLinkTableController_h
#define VisitedLinkTableController_h

#include "MessageReceiver.h"
#include "SharedMemory.h"
#include "VisitedLinkTable.h"
#include <WebCore/VisitedLinkStore.h>

namespace WebKit {

class VisitedLinkTableController final : public WebCore::VisitedLinkStore , private IPC::MessageReceiver {
public:
    static PassRefPtr<VisitedLinkTableController> getOrCreate(uint64_t identifier);
    virtual ~VisitedLinkTableController();

private:
    explicit VisitedLinkTableController(uint64_t identifier);

    // WebCore::VisitedLinkStore.
    virtual bool isLinkVisited(WebCore::Page&, WebCore::LinkHash, const WebCore::URL& baseURL, const AtomicString& attributeURL) override;
    virtual void addVisitedLink(WebCore::Page&, WebCore::LinkHash) override;

    // IPC::MessageReceiver.
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;

    void setVisitedLinkTable(const SharedMemory::Handle&);
    void visitedLinkStateChanged(const Vector<WebCore::LinkHash>&);
    void allVisitedLinkStateChanged();
    void removeAllVisitedLinks();

    uint64_t m_identifier;
    VisitedLinkTable m_visitedLinkTable;
};

} // namespace WebKit

#endif // VisitedLinkTableController_h
