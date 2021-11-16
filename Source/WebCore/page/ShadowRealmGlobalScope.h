/*
 * Copyright (C) 2021 Igalia S.L.
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

#include "Base64Utilities.h"
#include "CacheStorageConnection.h"
#include "JSEventTarget.h"
#include "ImageBitmap.h"
#include "ScriptBufferSourceProvider.h"
#include "ScriptExecutionContext.h"
#include "Supplementable.h"
#include "WindowOrWorkerGlobalScope.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerOrWorkletScriptController.h"
#include "WorkerThread.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class CSSFontSelector;
class CSSValuePool;
class ContentSecurityPolicyResponseHeaders;
class FontFaceSet;
class Performance;
class ScheduledAction;
class WorkerLocation;
class WorkerNavigator;
class WorkerSWClientConnection;
struct WorkerParameters;

class ShadowRealmGlobalScope : public Supplementable<ShadowRealmGlobalScope>, public Base64Utilities
                               , public WindowOrWorkerGlobalScope, public WorkerOrWorkletGlobalScope
  {
    WTF_MAKE_ISO_ALLOCATED(ShadowRealmGlobalScope);
public:
    ~ShadowRealmGlobalScope();

    virtual bool isDedicatedWorkerGlobalScope() const { return false; }
    virtual bool isSharedWorkerGlobalScope() const { return false; }
    virtual bool isServiceWorkerGlobalScope() const { return false; }

    const URL& url() const final { return m_url; }
    String origin() const;
    const String& identifier() const { return m_identifier; }

    using WeakValueType = EventTarget::WeakValueType;
    using EventTarget::weakPtrFactory;

    WorkerThread& thread() const;

    using ScriptExecutionContext::hasPendingActivity;

    ShadowRealmGlobalScope& self() { return *this; }
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
    bool crossOriginIsolated() const;

    WorkerNavigator* optionalNavigator() const { return m_navigator.get(); }
    WorkerLocation* optionalLocation() const { return m_location.get(); }

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final;

    SecurityOrigin& topOrigin() const final { return m_topOrigin.get(); }

    Performance& performance() const;

    void prepareForDestruction() override;

    void removeAllEventListeners() final;

    void createImageBitmap(ImageBitmap::Source&&, ImageBitmapOptions&&, ImageBitmap::Promise&&);
    void createImageBitmap(ImageBitmap::Source&&, int sx, int sy, int sw, int sh, ImageBitmapOptions&&, ImageBitmap::Promise&&);

    CSSValuePool& cssValuePool() final;
    CSSFontSelector* cssFontSelector() final;
    FontCache& fontCache() final;
    Ref<FontFaceSet> fonts();
    std::unique_ptr<FontLoadRequest> fontLoadRequest(String& url, bool isSVG, bool isInitiatingElementInUserAgentShadowTree, LoadedFromOpaqueSource) final;
    void beginLoadingFontSoon(FontLoadRequest&) final;

    ReferrerPolicy referrerPolicy() const final;

    const Settings::Values& settingsValues() const final { return m_settingsValues; }

    FetchOptions::Credentials credentials() const { return m_credentials; }

    void releaseMemory(Synchronous);
    static void releaseMemoryInWorkers(Synchronous);

    void setMainScriptSourceProvider(ScriptBufferSourceProvider&);
    void addImportedScriptSourceProvider(const URL&, ScriptBufferSourceProvider&);

protected:
    ShadowRealmGlobalScope(WorkerThreadType, const WorkerParameters&, Ref<SecurityOrigin>&&, WorkerThread&, Ref<SecurityOrigin>&& topOrigin, SocketProvider*);

    void applyContentSecurityPolicyResponseHeaders(const ContentSecurityPolicyResponseHeaders&);
    void updateSourceProviderBuffers(const ScriptBuffer& mainScript, const HashMap<URL, ScriptBuffer>& importedScripts);

private:
    void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) final;

    // The following addMessage and addConsoleMessage functions are deprecated.
    // Callers should try to create the ConsoleMessage themselves.
    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::JSGlobalObject*, unsigned long requestIdentifier) final;
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier) final;

    bool isShadowRealmGlobalScope() const final { return true; }

    void deleteJSCodeAndGC(Synchronous);
    void clearDecodedScriptData();

    URL completeURL(const String&, ForceUTF8 = ForceUTF8::No) const final;
    String userAgent(const URL&) const final;

    EventTarget* errorEventTarget() final;
    String resourceRequestIdentifier() const final { return m_identifier; }
    SocketProvider* socketProvider() final;
    RefPtr<RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() final;

    bool shouldBypassMainWorldContentSecurityPolicy() const final { return m_shouldBypassMainWorldContentSecurityPolicy; }

    URL m_url;
    String m_identifier;
    String m_userAgent;

    mutable RefPtr<WorkerLocation> m_location;
    mutable RefPtr<WorkerNavigator> m_navigator;

    bool m_isOnline;
    bool m_shouldBypassMainWorldContentSecurityPolicy;

    Ref<SecurityOrigin> m_topOrigin;

    RefPtr<SocketProvider> m_socketProvider;

    RefPtr<Performance> m_performance;

    WeakPtr<ScriptBufferSourceProvider> m_mainScriptSourceProvider;
    HashMap<URL, WeakHashSet<ScriptBufferSourceProvider>> m_importedScriptsSourceProviders;

    std::unique_ptr<CSSValuePool> m_cssValuePool;
    RefPtr<CSSFontSelector> m_cssFontSelector;
    RefPtr<FontCache> m_fontCache;
    ReferrerPolicy m_referrerPolicy;
    Settings::Values m_settingsValues;
    WorkerType m_workerType;
    FetchOptions::Credentials m_credentials;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ShadowRealmGlobalScope)
    static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isShadowRealmGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
