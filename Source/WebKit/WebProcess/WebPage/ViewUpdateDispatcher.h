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

#ifndef ViewUpdateDispatcher_h
#define ViewUpdateDispatcher_h

#include "Connection.h"

#include "VisibleContentRectUpdateInfo.h"
#include <WebCore/PageIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Ref.h>

namespace WebKit {

class ViewUpdateDispatcher : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<ViewUpdateDispatcher> create();
    ~ViewUpdateDispatcher();

    void initializeConnection(IPC::Connection*);

private:
    ViewUpdateDispatcher();
    // IPC::Connection::WorkQueueMessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void visibleContentRectUpdate(WebCore::PageIdentifier, const VisibleContentRectUpdateInfo&);

    void dispatchVisibleContentRectUpdate();

    struct UpdateData {
        VisibleContentRectUpdateInfo visibleContentRectUpdateInfo;
        MonotonicTime oldestTimestamp;
    };

    Ref<WorkQueue> m_queue;
    Lock m_dataMutex;
    HashMap<WebCore::PageIdentifier, UpdateData> m_latestUpdate;
};

} // namespace WebKit

#endif // ViewportUpdateDispatcher_h
