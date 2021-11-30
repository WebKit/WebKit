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
#include <JavaScriptCore/RuntimeFlags.h>
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
class Document;
class FontFaceSet;
class Performance;
class ScheduledAction;
class ShadowRealmScriptController;
class WorkerLocation;
class WorkerNavigator;
class WorkerSWClientConnection;
class JSDOMGlobalScope;
struct WorkerParameters;

class ShadowRealmGlobalScope : public ScriptExecutionContext, public RefCounted<ShadowRealmGlobalScope>
{
    WTF_MAKE_ISO_ALLOCATED(ShadowRealmGlobalScope);
public:
    // we need to be refcounted, but ScriptExecutionContext also exposes `ref`
    // and `deref` using the virtual implementations--we pick the RefCounted
    // ones to avoid ambiguity (and unnecessary virtual dispatch)
    using RefCounted<ShadowRealmGlobalScope>::ref;
    using RefCounted<ShadowRealmGlobalScope>::deref;

    static RefPtr<ShadowRealmGlobalScope> tryCreate(JSC::VM& vm, JSDOMGlobalObject*);
    ~ShadowRealmGlobalScope();

    bool isShadowRealmGlobalScope() const final { return true; }

    JSC::RuntimeFlags javaScriptRuntimeFlags() const;
    ScriptExecutionContext* enclosingContext() const;

    // other ScriptExecutionContext obligations
    const URL& url() const final;
    URL completeURL(const String&, ForceUTF8 = ForceUTF8::No) const final;
    String userAgent(const URL&) const final;
    ReferrerPolicy referrerPolicy() const final;
    const Settings::Values& settingsValues() const final;
    bool isSecureContext() const;
    bool isJSExecutionForbidden() const final;
    EventLoopTaskGroup& eventLoop() final;
    void disableEval(const String&) final;
    void disableWebAssembly(const String&) final;
    IDBClient::IDBConnectionProxy* idbConnectionProxy() final;
    SocketProvider* socketProvider() final;

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final;
    void addConsoleMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier) final;
    SecurityOrigin& topOrigin() const final;
    JSC::VM& vm() final;
    EventTarget* errorEventTarget() final;
    bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) final;
    bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) final;
    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&&, JSC::JSGlobalObject*, unsigned long requestIdentifier) final;
    void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&&) final;

    void postTask(Task&&) final;

    void refScriptExecutionContext() final;
    void derefScriptExecutionContext() final;

    ShadowRealmGlobalScope& self() { return *this; }
    ScriptModuleLoader& moduleLoader() { return *m_moduleLoader; }
    ShadowRealmScriptController* script() { return m_scriptController.get(); }
    Document* responsibleDocument();

protected:
    ShadowRealmGlobalScope(JSC::VM& vm, JSDOMGlobalObject*);

private:
    RefPtr<JSC::VM> m_vm;
    JSC::Strong<JSDOMGlobalObject> m_incubatingWrapper;
    std::unique_ptr<ShadowRealmScriptController> m_scriptController{};
    std::unique_ptr<ScriptModuleLoader> m_moduleLoader{};
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ShadowRealmGlobalScope)
static bool isType(const WebCore::ScriptExecutionContext& context) { return context.isShadowRealmGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()
