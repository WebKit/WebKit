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
#include <WebCore/WorkerInitializationData.h>
#include <WebCore/WorkerOptions.h>
#include <wtf/CheckedRef.h>
#include <wtf/Identified.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class WebSharedWorker;
}

namespace WebCore {
class RegistrableDomain;
}

namespace WebKit {

class WebSharedWorkerServer;
class WebSharedWorkerServerToContextConnection;

class WebSharedWorker : public RefCountedAndCanMakeWeakPtr<WebSharedWorker>, public Identified<WebCore::SharedWorkerIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(WebSharedWorker);
public:
    static Ref<WebSharedWorker> create(WebSharedWorkerServer&, const WebCore::SharedWorkerKey&, const WebCore::WorkerOptions&);

    ~WebSharedWorker();

    static WebSharedWorker* fromIdentifier(WebCore::SharedWorkerIdentifier);

    const WebCore::SharedWorkerKey& key() const { return m_key; }
    const WebCore::WorkerOptions& workerOptions() const { return m_workerOptions; }
    const WebCore::ClientOrigin& origin() const { return m_key.origin; }
    const URL& url() const { return m_key.url; }
    WebCore::RegistrableDomain topRegistrableDomain() const;
    WebSharedWorkerServerToContextConnection* contextConnection() const;

    void addSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier, const WebCore::TransferredMessagePort&);
    void removeSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier);
    void suspend(WebCore::SharedWorkerObjectIdentifier);
    void resume(WebCore::SharedWorkerObjectIdentifier);
    unsigned sharedWorkerObjectsCount() const { return m_sharedWorkerObjects.size(); }
    void forEachSharedWorkerObject(const Function<void(WebCore::SharedWorkerObjectIdentifier, const WebCore::TransferredMessagePort&)>&) const;
    std::optional<WebCore::ProcessIdentifier> firstSharedWorkerObjectProcess() const;

    void didCreateContextConnection(WebSharedWorkerServerToContextConnection&);

    bool isRunning() const { return m_isRunning; }
    void markAsRunning() { m_isRunning = true; }

    const WebCore::WorkerInitializationData& initializationData() const { return m_initializationData; }
    void setInitializationData(WebCore::WorkerInitializationData&& initializationData) { m_initializationData = WTFMove(initializationData); }

    const WebCore::WorkerFetchResult& fetchResult() const { return m_fetchResult; }
    void setFetchResult(WebCore::WorkerFetchResult&&);
    bool didFinishFetching() const { return !!m_fetchResult.script; }

    void launch(WebSharedWorkerServerToContextConnection&);

    struct SharedWorkerObjectState {
        bool isSuspended { false };
        std::optional<WebCore::TransferredMessagePort> port;
    };

    struct Object {
        WebCore::SharedWorkerObjectIdentifier identifier;
        SharedWorkerObjectState state;
    };

private:
    WebSharedWorker(WebSharedWorkerServer&, const WebCore::SharedWorkerKey&, const WebCore::WorkerOptions&);

    WebSharedWorker(WebSharedWorker&&) = delete;
    WebSharedWorker& operator=(WebSharedWorker&&) = delete;
    WebSharedWorker(const WebSharedWorker&) = delete;
    WebSharedWorker& operator=(const WebSharedWorker&) = delete;

    void suspendIfNeeded();
    void resumeIfNeeded();

    WeakPtr<WebSharedWorkerServer> m_server;
    WebCore::SharedWorkerKey m_key;
    WebCore::WorkerOptions m_workerOptions;
    ListHashSet<Object> m_sharedWorkerObjects;
    WebCore::WorkerFetchResult m_fetchResult;
    WebCore::WorkerInitializationData m_initializationData;
    bool m_isRunning { false };
    bool m_isSuspended { false };
};

} // namespace WebKit

namespace WTF {

struct WebSharedWorkerObjectHash {
    static unsigned hash(const WebKit::WebSharedWorker::Object& object) { return DefaultHash<WebCore::SharedWorkerObjectIdentifier>::hash(object.identifier); }
    static bool equal(const WebKit::WebSharedWorker::Object& a, const WebKit::WebSharedWorker::Object& b) { return a.identifier == b.identifier; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<WebKit::WebSharedWorker::Object> : SimpleClassHashTraits<WebSharedWorkerObjectHash> { };
template<> struct DefaultHash<WebKit::WebSharedWorker::Object> : WebSharedWorkerObjectHash { };

} // namespace WTF
