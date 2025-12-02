/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ConsoleMessage.h>
#include <WebCore/CacheStorageConnection.h>
#include <WebCore/ClientOrigin.h>
#include <WebCore/ImageBitmap.h>
#include <WebCore/ReportingClient.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/Settings.h>
#include <WebCore/Supplementable.h>
#include <WebCore/WindowOrWorkerGlobalScope.h>
#include <WebCore/WorkerOrWorkletGlobalScope.h>
#include <WebCore/WorkerOrWorkletScriptController.h>
#include <WebCore/WorkerType.h>
#include <memory>
#include <wtf/FixedVector.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class CSSFontSelector;
class CSSValuePool;
class CacheStorageConnection;
class ContentSecurityPolicyResponseHeaders;
class Crypto;
class CryptoKey;
class FileSystemStorageConnection;
class FontFaceSet;
class MessagePortChannelProvider;
class Performance;
class ReportingScope;
class ScheduledAction;
class ScriptBuffer;
class ScriptBufferSourceProvider;
class TrustedScriptURL;
class WorkerCacheStorageConnection;
class WorkerClient;
class WorkerFileSystemStorageConnection;
class WorkerLocation;
class WorkerMessagePortChannelProvider;
class WorkerNavigator;
class WorkerSWClientConnection;
class WorkerStorageConnection;
class WorkerStorageConnection;
class WorkerThread;
struct WorkerParameters;

enum class ViolationReportType : uint8_t;

namespace IDBClient {
class IDBConnectionProxy;
}

class WorkerGlobalScope : public Supplementable<WorkerGlobalScope>, public WindowOrWorkerGlobalScope, public WorkerOrWorkletGlobalScope, public ReportingClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WorkerGlobalScope);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WorkerGlobalScope);
public:
    virtual ~WorkerGlobalScope();

    enum class Type : uint8_t { DedicatedWorker, ServiceWorker, SharedWorker };
    virtual Type type() const = 0;

    const URL& url() const final { return m_url; }
    const URL& cookieURL() const final { return url(); }
    const URL& ownerURL() const { return m_ownerURL; }
    String origin() const;
    const String& inspectorIdentifier() const { return m_inspectorIdentifier; }

    IDBClient::IDBConnectionProxy* idbConnectionProxy() final;
    void suspend() final;
    void resume() final;
    GraphicsClient* graphicsClient() final;


    USING_CAN_MAKE_WEAKPTR(EventTarget);

    WorkerStorageConnection& storageConnection();
    static void postFileSystemStorageTask(Function<void()>&&);
    WorkerFileSystemStorageConnection& getFileSystemStorageConnection(Ref<FileSystemStorageConnection>&&);
    WEBCORE_EXPORT WorkerFileSystemStorageConnection* fileSystemStorageConnection();
    CacheStorageConnection& cacheStorageConnection();
    MessagePortChannelProvider& messagePortChannelProvider();

    WorkerSWClientConnection& swClientConnection();
    void updateServiceWorkerClientData() final;

    Ref<WorkerThread> thread() const;

    using ScriptExecutionContext::hasPendingActivity;

    WorkerGlobalScope& self() { return *this; }
    WorkerLocation& location() const;
    void close();

    virtual ExceptionOr<void> importScripts(const FixedVector<Variant<RefPtr<TrustedScriptURL>, String>>& urls);
    WorkerNavigator& navigator();
    Ref<WorkerNavigator> protectedNavigator();

    void setIsOnline(bool);
    bool isOnline() const { return m_isOnline; }

    ExceptionOr<int> setTimeout(std::unique_ptr<ScheduledAction>, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments);
    void clearTimeout(int timeoutId);
    ExceptionOr<int> setInterval(std::unique_ptr<ScheduledAction>, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments);
    void clearInterval(int timeoutId);

    bool isSecureContext() const final;
    bool crossOriginIsolated() const;

    WorkerNavigator* optionalNavigator() const { return m_navigator.get(); }
    WorkerLocation* optionalLocation() const { return m_location.get(); }

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final;

    SecurityOrigin& topOrigin() const final { return m_topOrigin.get(); }

    Crypto& crypto();
    Performance& performance() const;
    Ref<Performance> protectedPerformance() const;
    ReportingScope& reportingScope() const { return m_reportingScope.get(); }

    void prepareForDestruction() override;

    void removeAllEventListeners() final;

    void createImageBitmap(ImageBitmap::Source&&, ImageBitmapOptions&&, ImageBitmap::Promise&&);
    void createImageBitmap(ImageBitmap::Source&&, int sx, int sy, int sw, int sh, ImageBitmapOptions&&, ImageBitmap::Promise&&);

    CSSValuePool& cssValuePool() final;
    CSSFontSelector* cssFontSelector() final;
    Ref<FontFaceSet> fonts();
    RefPtr<FontLoadRequest> fontLoadRequest(const String& url, bool isSVG, bool isInitiatingElementInUserAgentShadowTree, LoadedFromOpaqueSource) final;
    void beginLoadingFontSoon(FontLoadRequest&) final;

    const SettingsValues& settingsValues() const final { return m_settingsValues; }

    FetchOptions::Credentials credentials() const { return m_credentials; }

    void releaseMemory(Synchronous);
    static void releaseMemoryInWorkers(Synchronous);
    static void dumpGCHeapForWorkers();

    void setMainScriptSourceProvider(ScriptBufferSourceProvider&);
    void addImportedScriptSourceProvider(const URL&, ScriptBufferSourceProvider&);

    ClientOrigin clientOrigin() const { return { topOrigin().data(), securityOrigin()->data() }; }

    WorkerClient* workerClient() { return m_workerClient.get(); }

    void reportErrorToWorkerObject(const String&);

protected:
    WorkerGlobalScope(WorkerThreadType, const WorkerParameters&, Ref<SecurityOrigin>&&, WorkerThread&, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy*, SocketProvider*, std::unique_ptr<WorkerClient>&&);

    void applyContentSecurityPolicyResponseHeaders(const ContentSecurityPolicyResponseHeaders&);
    void updateSourceProviderBuffers(const ScriptBuffer& mainScript, const HashMap<URL, ScriptBuffer>& importedScripts);

    void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier) override;

private:
    void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) final;

    // The following addMessage and addConsoleMessage functions are deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::JSGlobalObject*, unsigned long requestIdentifier) final;

    bool isWorkerGlobalScope() const final { return true; }

    void deleteJSCodeAndGC(Synchronous);
    void clearDecodedScriptData();

    URL completeURL(const String&, ForceUTF8 = ForceUTF8::No) const final;
    String userAgent(const URL&) const final;

    EventTarget* errorEventTarget() final;
    String resourceRequestIdentifier() const final { return m_inspectorIdentifier; }
    SocketProvider* socketProvider() final;
    RefPtr<SocketProvider> protectedSocketProvider();
    RefPtr<RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() final;

    bool shouldBypassMainWorldContentSecurityPolicy() const final { return m_shouldBypassMainWorldContentSecurityPolicy; }

    std::optional<Vector<uint8_t>> serializeAndWrapCryptoKey(CryptoKeyData&&) final;
    std::optional<Vector<uint8_t>> unwrapCryptoKey(const Vector<uint8_t>& wrappedKey) final;

    // ReportingClient.
    void notifyReportObservers(Ref<Report>&&) final;
    String endpointURIForToken(const String&) const final;
    void sendReportToEndpoints(const URL& baseURL, std::span<const String> endpointURIs, std::span<const String> endpointTokens, Ref<FormData>&& report, ViolationReportType) final;
    String httpUserAgent() const final { return m_userAgent; }

    URL m_url;
    URL m_ownerURL;
    String m_inspectorIdentifier;
    String m_userAgent;

    const RefPtr<WorkerLocation> m_location;
    const RefPtr<WorkerNavigator> m_navigator;

    bool m_isOnline;
    bool m_shouldBypassMainWorldContentSecurityPolicy;

    const Ref<SecurityOrigin> m_topOrigin;

    const RefPtr<IDBClient::IDBConnectionProxy> m_connectionProxy;

    const RefPtr<SocketProvider> m_socketProvider;

    RefPtr<Performance> m_performance;
    const Ref<ReportingScope> m_reportingScope;
    mutable RefPtr<Crypto> m_crypto;

    WeakPtr<ScriptBufferSourceProvider> m_mainScriptSourceProvider;
    MemoryCompactRobinHoodHashMap<URL, WeakHashSet<ScriptBufferSourceProvider>> m_importedScriptsSourceProviders;

    const RefPtr<CacheStorageConnection> m_cacheStorageConnection;
    const RefPtr<WorkerMessagePortChannelProvider> m_messagePortChannelProvider;
    const RefPtr<WorkerSWClientConnection> m_swClientConnection;
    const std::unique_ptr<CSSValuePool> m_cssValuePool;
    const std::unique_ptr<WorkerClient> m_workerClient;
    const RefPtr<CSSFontSelector> m_cssFontSelector;
    SettingsValues m_settingsValues;
    WorkerType m_workerType;
    FetchOptions::Credentials m_credentials;
    const RefPtr<WorkerStorageConnection> m_storageConnection;
    RefPtr<WorkerFileSystemStorageConnection> m_fileSystemStorageConnection;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkerGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isWorkerGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
