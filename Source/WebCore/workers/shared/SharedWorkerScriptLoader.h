/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "MessagePortIdentifier.h"
#include "ResourceLoaderIdentifier.h"
#include "ResourceResponse.h"
#include "WorkerOptions.h"
#include "WorkerScriptLoaderClient.h"
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class SharedWorker;
class WorkerScriptLoader;

class SharedWorkerScriptLoader;
using SharedWorkerScriptLoaderIdentifier = ObjectIdentifier<SharedWorkerScriptLoader>;

using TransferredMessagePort = std::pair<WebCore::MessagePortIdentifier, WebCore::MessagePortIdentifier>;

class SharedWorkerScriptLoader : private WorkerScriptLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SharedWorkerScriptLoader(const URL&, SharedWorker&, TransferredMessagePort&&, WorkerOptions&&);

    SharedWorkerScriptLoaderIdentifier identifier() const { return m_identifier; }
    const URL& url() const { return m_url; }
    SharedWorker& worker() { return m_worker.get(); }
    const WorkerOptions& options() { return m_options; }

private:
    void didReceiveResponse(ResourceLoaderIdentifier, const ResourceResponse&) final;
    void notifyFinished() final;

    const SharedWorkerScriptLoaderIdentifier m_identifier;
    const WorkerOptions m_options;
    const Ref<SharedWorker> m_worker;
    TransferredMessagePort m_port;
    const Ref<WorkerScriptLoader> m_loader;
    const Ref<ActiveDOMObject::PendingActivity<SharedWorker>> m_pendingActivity;
    const URL m_url;
};

} // namespace WebCore
