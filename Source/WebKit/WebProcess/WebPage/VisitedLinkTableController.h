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

#pragma once

#include "MessageReceiver.h"
#include "SharedStringHashTableReadOnly.h"
#include "VisitedLinkTableIdentifier.h"
#include <WebCore/SharedMemory.h>
#include <WebCore/VisitedLinkStore.h>

namespace WebKit {

class VisitedLinkTableController final : public WebCore::VisitedLinkStore, public IPC::MessageReceiver {
public:
    static Ref<VisitedLinkTableController> getOrCreate(VisitedLinkTableIdentifier);
    virtual ~VisitedLinkTableController();

private:
    explicit VisitedLinkTableController(VisitedLinkTableIdentifier);

    // WebCore::VisitedLinkStore.
    bool isLinkVisited(WebCore::Page&, WebCore::SharedStringHash, const URL& baseURL, const AtomString& attributeURL) override;
    void addVisitedLink(WebCore::Page&, WebCore::SharedStringHash) override;

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void setVisitedLinkTable(WebCore::SharedMemory::Handle&&);
    void visitedLinkStateChanged(const Vector<WebCore::SharedStringHash>&);
    void allVisitedLinkStateChanged();
    void removeAllVisitedLinks();

    VisitedLinkTableIdentifier m_identifier;
    SharedStringHashTableReadOnly m_visitedLinkTable;
};

} // namespace WebKit
