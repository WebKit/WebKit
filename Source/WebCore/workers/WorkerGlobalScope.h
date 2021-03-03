/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include "Base64Utilities.h"
#include "CacheStorageConnection.h"
#include "ImageBitmap.h"
#include "ScriptExecutionContext.h"
#include "Supplementable.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerOrWorkletScriptController.h"
#include <wtf/URL.h>
#include "WorkerCacheStorageConnection.h"
#include "WorkerMessagePortChannelProvider.h"
#include "WorkerThread.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <memory>

namespace WebCore {

class CSSValuePool;
class ContentSecurityPolicyResponseHeaders;
class Crypto;
class Performance;
class ScheduledAction;
class WorkerLocation;
class WorkerNavigator;
class WorkerSWClientConnection;
struct WorkerParameters;

namespace IDBClient {
class IDBConnectionProxy;
}

class WorkerGlobalScope : public Supplementable<WorkerGlobalScope>, public Base64Utilities, public WorkerOrWorkletGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(WorkerGlobalScope);
public:
    virtual ~WorkerGlobalScope();

    virtual bool isDedicatedWorkerGlobalScope() const { return false; }
    virtual bool isServiceWorkerGlobalScope() const { return false; }

    const URL& url() const final { return m_url; }
    String origin() const;
    const String& identifier() const { return m_identifier; }

#if ENABLE(INDEXED_DATABASE)
    IDBClient::IDBConnectionProxy* idbConnectionProxy() final;
    void suspend() final;
    void resume() final;
#endif

    WorkerCacheStorageConnection& cacheStorageConnection();
    MessagePortChannelProvider& messagePortChannelProvider();
#if ENABLE(SERVICE_WORKER)
    WorkerSWClientConnection& swClientConnection();
#endif

    WorkerThread& thread() const;

    using ScriptExecutionContext::hasPendingActivity;

    WorkerGlobalScope& self() { return *this; }
    WorkerLocation& location() const;
    void close();

    virtual ExceptionOr<void> importScripts(const Vector<String>& urls);
    WorkerNavigator& navigator();

    void setIsOnline(bool);

    ExceptionOr<int> setTimeout(JSC::JSGlobalObject&, std::unique_ptr<ScheduledAction>, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
    void clearTimeout(int timeoutId);
    ExceptionOr<int> setInterval(JSC::JSGlobalObject&, std::unique_ptr<ScheduledAction>, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
    void clearInterval(int timeoutId);

    bool isSecureContext() const final;

    WorkerNavigator* optionalNavigator() const { return m_navigator.get(); }
    WorkerLocation* optionalLocation() const { return m_location.get(); }

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final;

    SecurityOrigin& topOrigin() const final { return m_topOrigin.get(); }

    Crypto& crypto();
    Performance& performance() const;

    void prepareForDestruction() override;

    void removeAllEventListeners() final;

    void createImageBitmap(ImageBitmap::Source&&, ImageBitmapOptions&&, ImageBitmap::Promise&&);
    void createImageBitmap(ImageBitmap::Source&&, int sx, int sy, int sw, int sh, ImageBitmapOptions&&, ImageBitmap::Promise&&);

    CSSValuePool& cssValuePool();

    ReferrerPolicy referrerPolicy() const final;

    const Settings::Values& settingsValues() const final { return m_settingsValues; }

    FetchOptions::Credentials credentials() const { return m_credentials; }

protected:
    WorkerGlobalScope(WorkerThreadType, const WorkerParameters&, Ref<SecurityOrigin>&&, WorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*);

    void applyContentSecurityPolicyResponseHeaders(const ContentSecurityPolicyResponseHeaders&);

private:
    void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) final;

    // The following addMessage and addConsoleMessage functions are deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::JSGlobalObject*, unsigned long requestIdentifier) final;
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier) final;

    bool isWorkerGlobalScope() const final { return true; }

    URL completeURL(const String&, ForceUTF8 = ForceUTF8::No) const final;
    String userAgent(const URL&) const final;

    EventTarget* errorEventTarget() final;
    String resourceRequestIdentifier() const final { return m_identifier; }
    SocketProvider* socketProvider() final;

    bool shouldBypassMainWorldContentSecurityPolicy() const final { return m_shouldBypassMainWorldContentSecurityPolicy; }

#if ENABLE(WEB_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey) final;
    bool unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) final;
#endif

#if ENABLE(INDEXED_DATABASE)
    void stopIndexedDatabase();
#endif

    URL m_url;
    String m_identifier;
    String m_userAgent;

    mutable RefPtr<WorkerLocation> m_location;
    mutable RefPtr<WorkerNavigator> m_navigator;

    bool m_isOnline;
    bool m_shouldBypassMainWorldContentSecurityPolicy;

    Ref<SecurityOrigin> m_topOrigin;

#if ENABLE(INDEXED_DATABASE)
    RefPtr<IDBClient::IDBConnectionProxy> m_connectionProxy;
#endif

    RefPtr<SocketProvider> m_socketProvider;

    RefPtr<Performance> m_performance;
    mutable RefPtr<Crypto> m_crypto;

    RefPtr<WorkerCacheStorageConnection> m_cacheStorageConnection;
    std::unique_ptr<WorkerMessagePortChannelProvider> m_messagePortChannelProvider;
#if ENABLE(SERVICE_WORKER)
    RefPtr<WorkerSWClientConnection> m_swClientConnection;
#endif
    std::unique_ptr<CSSValuePool> m_cssValuePool;
    ReferrerPolicy m_referrerPolicy;
    Settings::Values m_settingsValues;
    WorkerType m_workerType;
    FetchOptions::Credentials m_credentials;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkerGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isWorkerGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
