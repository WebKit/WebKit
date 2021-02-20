/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "WorkerThread.h"

#include "IDBConnectionProxy.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "SocketProvider.h"
#include "WorkerGlobalScope.h"
#include "WorkerScriptFetcher.h"
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/Threading.h>

namespace WebCore {

static std::atomic<unsigned> workerThreadCounter { 0 };

unsigned WorkerThread::workerThreadCount()
{
    return workerThreadCounter;
}

WorkerParameters WorkerParameters::isolatedCopy() const
{
    return {
        scriptURL.isolatedCopy(),
        name.isolatedCopy(),
        identifier.isolatedCopy(),
        userAgent.isolatedCopy(),
        isOnline,
        contentSecurityPolicyResponseHeaders,
        shouldBypassMainWorldContentSecurityPolicy,
        timeOrigin,
        referrerPolicy,
        workerType,
        credentials,
        settingsValues.isolatedCopy()
    };
}

struct WorkerThreadStartupData {
    WTF_MAKE_NONCOPYABLE(WorkerThreadStartupData); WTF_MAKE_FAST_ALLOCATED;
public:
    WorkerThreadStartupData(const WorkerParameters& params, const String& sourceCode, WorkerThreadStartMode, const SecurityOrigin& topOrigin);

    WorkerParameters params;
    Ref<SecurityOrigin> origin;
    String sourceCode;
    WorkerThreadStartMode startMode;
    Ref<SecurityOrigin> topOrigin;
};

WorkerThreadStartupData::WorkerThreadStartupData(const WorkerParameters& other, const String& sourceCode, WorkerThreadStartMode startMode, const SecurityOrigin& topOrigin)
    : params(other.isolatedCopy())
    , origin(SecurityOrigin::create(other.scriptURL)->isolatedCopy())
    , sourceCode(sourceCode.isolatedCopy())
    , startMode(startMode)
    , topOrigin(topOrigin.isolatedCopy())
{
}

WorkerThread::WorkerThread(const WorkerParameters& params, const String& sourceCode, WorkerLoaderProxy& workerLoaderProxy, WorkerDebuggerProxy& workerDebuggerProxy, WorkerReportingProxy& workerReportingProxy, WorkerThreadStartMode startMode, const SecurityOrigin& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, JSC::RuntimeFlags runtimeFlags)
    : WorkerOrWorkletThread(params.identifier.isolatedCopy())
    , m_workerLoaderProxy(workerLoaderProxy)
    , m_workerDebuggerProxy(workerDebuggerProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_runtimeFlags(runtimeFlags)
    , m_startupData(makeUnique<WorkerThreadStartupData>(params, sourceCode, startMode, topOrigin))
#if ENABLE(INDEXED_DATABASE)
    , m_idbConnectionProxy(connectionProxy)
#endif
    , m_socketProvider(socketProvider)
{
#if !ENABLE(INDEXED_DATABASE)
    UNUSED_PARAM(connectionProxy);
#endif
    ++workerThreadCounter;
}

WorkerThread::~WorkerThread()
{
    ASSERT(workerThreadCounter);
    --workerThreadCounter;
}

Ref<Thread> WorkerThread::createThread()
{
    return Thread::create(isServiceWorkerThread() ? "WebCore: Service Worker" : "WebCore: Worker", [this] {
        workerOrWorkletThread();
    }, ThreadType::JavaScript);
}

RefPtr<WorkerOrWorkletGlobalScope> WorkerThread::createGlobalScope()
{
    return createWorkerGlobalScope(m_startupData->params, WTFMove(m_startupData->origin), WTFMove(m_startupData->topOrigin));
}

bool WorkerThread::shouldWaitForWebInspectorOnStartup() const
{
    return m_startupData->startMode == WorkerThreadStartMode::WaitForInspector;
}

void WorkerThread::evaluateScriptIfNecessary(String& exceptionMessage)
{
    // We are currently holding only the initial script code. If the WorkerType is Module, we should fetch the entire graph before executing the rest of this.
    // We invoke module loader as if we are executing inline module script tag in Document.

    if (m_startupData->params.workerType == WorkerType::Classic) {
        globalScope()->script()->evaluate(ScriptSourceCode(m_startupData->sourceCode, URL(m_startupData->params.scriptURL)), &exceptionMessage);
        finishedEvaluatingScript();
    } else {
        auto scriptFetcher = WorkerScriptFetcher::create(globalScope()->credentials(), globalScope()->destination(), globalScope()->referrerPolicy());
        ScriptSourceCode sourceCode(m_startupData->sourceCode, URL(m_startupData->params.scriptURL), { }, JSC::SourceProviderSourceType::Module, scriptFetcher.copyRef());
        MessageQueueWaitResult result = globalScope()->script()->loadModuleSynchronously(scriptFetcher.get(), sourceCode);
        if (result != MessageQueueTerminated) {
            if (Optional<LoadableScript::Error> error = scriptFetcher->error()) {
                if (Optional<LoadableScript::ConsoleMessage> message = error->consoleMessage)
                    exceptionMessage = message->message;
                else
                    exceptionMessage = "Importing a module script failed."_s;
                globalScope()->reportException(exceptionMessage, { }, { }, { }, { }, { });
            } else if (!scriptFetcher->wasCanceled()) {
                globalScope()->script()->linkAndEvaluateModule(scriptFetcher.get(), sourceCode, &exceptionMessage);
                finishedEvaluatingScript();
            }
        }
    }

    // Free the startup data to cause its member variable deref's happen on the worker's thread (since
    // all ref/derefs of these objects are happening on the thread at this point). Note that
    // WorkerThread::~WorkerThread happens on a different thread where it was created.
    m_startupData = nullptr;
}

IDBClient::IDBConnectionProxy* WorkerThread::idbConnectionProxy()
{
#if ENABLE(INDEXED_DATABASE)
    return m_idbConnectionProxy.get();
#else
    return nullptr;
#endif
}

SocketProvider* WorkerThread::socketProvider()
{
    return m_socketProvider.get();
}

WorkerGlobalScope* WorkerThread::globalScope()
{
    return downcast<WorkerGlobalScope>(WorkerOrWorkletThread::globalScope());
}

} // namespace WebCore
