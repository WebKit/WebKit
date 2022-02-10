/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <WebCore/SharedWorkerIdentifier.h>
#include <WebCore/SharedWorkerKey.h>
#include <WebCore/SharedWorkerObjectIdentifier.h>
#include <WebCore/TransferredMessagePort.h>
#include <WebCore/WorkerFetchResult.h>
#include <WebCore/WorkerOptions.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class RegistrableDomain;
}

namespace WebKit {

class WebSharedWorkerServer;
class WebSharedWorkerServerToContextConnection;

class WebSharedWorker : public CanMakeWeakPtr<WebSharedWorker> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebSharedWorker(WebSharedWorkerServer&, const WebCore::SharedWorkerKey&, const WebCore::WorkerOptions&);
    ~WebSharedWorker();

    static WebSharedWorker* fromIdentifier(WebCore::SharedWorkerIdentifier);

    WebCore::SharedWorkerIdentifier identifier() const { return m_identifier; }
    const WebCore::SharedWorkerKey& key() const { return m_key; }
    const WebCore::WorkerOptions& workerOptions() const { return m_workerOptions; }
    const WebCore::ClientOrigin& origin() const { return m_key.origin; }
    const URL& url() const { return m_key.url; }
    WebCore::RegistrableDomain registrableDomain() const;
    WebSharedWorkerServerToContextConnection* contextConnection() const;

    void addSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier, const WebCore::TransferredMessagePort&);
    void removeSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier);
    unsigned sharedWorkerObjectsCount() const { return m_sharedWorkerObjects.size(); }
    void forEachSharedWorkerObject(const Function<void(WebCore::SharedWorkerObjectIdentifier, const WebCore::TransferredMessagePort&)>&) const;

    void didCreateContextConnection(WebSharedWorkerServerToContextConnection&);

    bool isRunning() const { return m_isRunning; }
    void markAsRunning() { m_isRunning = true; }

    const WebCore::WorkerFetchResult& fetchResult() const { return m_fetchResult; }
    void setFetchResult(WebCore::WorkerFetchResult&& fetchResult) { m_fetchResult = WTFMove(fetchResult); }
    bool didFinishFetching() const { return !!m_fetchResult.script; }

private:
    WebSharedWorker(WebSharedWorker&&) = delete;
    WebSharedWorker& operator=(WebSharedWorker&&) = delete;
    WebSharedWorker(const WebSharedWorker&) = delete;
    WebSharedWorker& operator=(const WebSharedWorker&) = delete;

    WebSharedWorkerServer& m_server;
    WebCore::SharedWorkerIdentifier m_identifier;
    WebCore::SharedWorkerKey m_key;
    WebCore::WorkerOptions m_workerOptions;
    HashMap<WebCore::SharedWorkerObjectIdentifier, WebCore::TransferredMessagePort> m_sharedWorkerObjects;
    WebCore::WorkerFetchResult m_fetchResult;
    bool m_isRunning { false };
};

} // namespace WebKit
