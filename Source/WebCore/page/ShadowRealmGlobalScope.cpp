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

#include "config.h"
#include "ShadowRealmGlobalScope.h"

#include "JSDOMGlobalObject.h"
#include "CSSFontSelector.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CommonVM.h"
#include "ContentSecurityPolicy.h"
#include "FontCache.h"
#include "FontCustomPlatformData.h"
#include "FontFaceSet.h"
#include "ImageBitmapOptions.h"
#include "InspectorInstrumentation.h"
#include "JSDOMExceptionHandling.h"
#include "Performance.h"
#include "RTCDataChannelRemoteHandlerConnection.h"
#include "RuntimeEnabledFeatures.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "ScriptModuleLoader.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "ShadowRealmScriptController.h"
#include "SocketProvider.h"
#include "WorkerFontLoadRequest.h"
#include "WorkerLoaderProxy.h"
#include "WorkerLocation.h"
#include "WorkerMessagingProxy.h"
#include "WorkerNavigator.h"
#include "WorkerReportingProxy.h"
#include "WorkerSWClientConnection.h"
#include "WorkerScriptLoader.h"
#include "WorkerStorageConnection.h"
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {
using namespace Inspector;

WTF_MAKE_ISO_ALLOCATED_IMPL(ShadowRealmGlobalScope);

RefPtr<ShadowRealmGlobalScope> ShadowRealmGlobalScope::tryCreate(JSC::VM& vm, JSDOMGlobalObject* wrapper) {
    return adoptRef(new ShadowRealmGlobalScope(vm, wrapper));
}

ShadowRealmGlobalScope::ShadowRealmGlobalScope(JSC::VM& vm, JSDOMGlobalObject* wrapper)
    : m_vm(&vm)
    , m_incubatingWrapper(vm, wrapper)
{
    // m_scriptController is initialized in `JSShadowRealmGlobalScopeBase::finishCreation`
    // which .......
    // ..... is not ideal
    //
    // probably we should emulate WorkerOrWorkletGlobalScope and its
    // *ScriptController and have the scriptcontroller mediate creation of the
    // wrapper...
    m_moduleLoader = std::make_unique<ScriptModuleLoader>(*this, ScriptModuleLoader::OwnerType::ShadowRealm);
}

ScriptExecutionContext* ShadowRealmGlobalScope::enclosingContext() const
{
    return m_incubatingWrapper->scriptExecutionContext();
}

Document* ShadowRealmGlobalScope::responsibleDocument() {
    // TODO(jgriego) this should really probably be a method on ScriptExecutionContext
    auto context = enclosingContext();
    ASSERT(is<Document>(context)); // FIXME this assert will fail if the shadow realm is created by a worker
    return downcast<Document>(context);
}


// TODO(jgriego) we currently delegate everything to the enclosing
// ScriptExecutionContext--this probably needs to change for at least some of
// these ...

const URL& ShadowRealmGlobalScope::url() const
{
    return enclosingContext()->url();
}

URL ShadowRealmGlobalScope::completeURL(const String& s, ForceUTF8 f) const
{
    return enclosingContext()->completeURL(s, f);
}

String ShadowRealmGlobalScope::userAgent(const URL& url) const
{
    return enclosingContext()->userAgent(url);
}

ReferrerPolicy ShadowRealmGlobalScope::referrerPolicy() const
{
    return enclosingContext()->referrerPolicy();
}

const Settings::Values& ShadowRealmGlobalScope::settingsValues() const
{
    return enclosingContext()->settingsValues();
}

bool ShadowRealmGlobalScope::isSecureContext() const
{
    return enclosingContext()->isSecureContext();
}

bool ShadowRealmGlobalScope::isJSExecutionForbidden() const
{
    return enclosingContext()->isJSExecutionForbidden();
}

EventLoopTaskGroup& ShadowRealmGlobalScope::eventLoop()
{
    return enclosingContext()->eventLoop();
}

void ShadowRealmGlobalScope::disableEval(const String& msg)
{
    enclosingContext()->disableEval(msg);
}

void ShadowRealmGlobalScope::disableWebAssembly(const String& msg)
{
    enclosingContext()->disableWebAssembly(msg);
}

IDBClient::IDBConnectionProxy* ShadowRealmGlobalScope::idbConnectionProxy()
{
    return enclosingContext()->idbConnectionProxy();
}

SocketProvider* ShadowRealmGlobalScope::socketProvider()
{
    return enclosingContext()->socketProvider();
}


void ShadowRealmGlobalScope::addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&& msg)
{
    enclosingContext()->addConsoleMessage(WTFMove(msg));
}

void ShadowRealmGlobalScope::addConsoleMessage(MessageSource src, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    enclosingContext()->addConsoleMessage(src, level, message, requestIdentifier);
}

SecurityOrigin& ShadowRealmGlobalScope::topOrigin() const
{
    return enclosingContext()->topOrigin();
}

JSC::VM& ShadowRealmGlobalScope::vm()
{
    return enclosingContext()->vm();
}

EventTarget* ShadowRealmGlobalScope::errorEventTarget()
{
    return enclosingContext()->errorEventTarget();
}

bool ShadowRealmGlobalScope::wrapCryptoKey(const Vector<uint8_t>& in, Vector<uint8_t>& out)
{
    return enclosingContext()->wrapCryptoKey(in, out);
}

bool ShadowRealmGlobalScope::unwrapCryptoKey(const Vector<uint8_t>& in, Vector<uint8_t>& out)
{
    return enclosingContext()->unwrapCryptoKey(in, out);
}

void ShadowRealmGlobalScope::addMessage(MessageSource src, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&& stack, JSC::JSGlobalObject* global, unsigned long requestIdentifier)
{
    enclosingContext()->addMessage(src, level, message, sourceURL,
lineNumber, columnNumber, WTFMove(stack), global, requestIdentifier );
}

void ShadowRealmGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&& stack)
{
    enclosingContext()->logExceptionToConsole(errorMessage, sourceURL, lineNumber, columnNumber, WTFMove(stack));
}


void ShadowRealmGlobalScope::postTask(Task&& task)
{
    enclosingContext()->postTask(WTFMove(task));
}


void ShadowRealmGlobalScope::refScriptExecutionContext()
{
    ref();
}

void ShadowRealmGlobalScope::derefScriptExecutionContext()
{
    deref();
}

ShadowRealmGlobalScope::~ShadowRealmGlobalScope() {}
} // namespace WebCore
