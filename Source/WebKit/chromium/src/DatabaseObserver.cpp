/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DatabaseObserver.h"

#if ENABLE(SQL_DATABASE)

#include "AbstractDatabase.h"
#include "CrossThreadCopier.h"
#include "CrossThreadTask.h"
#include "Document.h"
#include "ScriptExecutionContext.h"
#include "WebCommonWorkerClient.h"
#include "WebDatabase.h"
#include "WebDatabaseObserver.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebPermissionClient.h"
#include "WebSecurityOrigin.h"
#include "WebViewImpl.h"
#include "WebWorkerBase.h"
#include "WorkerContext.h"
#include "WorkerLoaderProxy.h"
#include "WorkerScriptController.h"
#include "WorkerThread.h"

using namespace WebKit;

namespace {

#if ENABLE(WORKERS)

static const char allowDatabaseMode[] = "allowDatabaseMode";

// This class is used to route the result of the WebWorkerBase::allowDatabase
// call back to the worker context.
class AllowDatabaseMainThreadBridge : public ThreadSafeRefCounted<AllowDatabaseMainThreadBridge> {
public:
    static PassRefPtr<AllowDatabaseMainThreadBridge> create(WebCore::WorkerLoaderProxy* workerLoaderProxy, const String& mode, WebCommonWorkerClient* commonClient, WebFrame* frame, const String& name, const String& displayName, unsigned long estimatedSize)
    {
        return adoptRef(new AllowDatabaseMainThreadBridge(workerLoaderProxy, mode, commonClient, frame, name, displayName, estimatedSize));
    }

    // These methods are invoked on the worker context.
    void cancel()
    {
        MutexLocker locker(m_mutex);
        m_workerLoaderProxy = 0;
    }

    bool result()
    {
        return m_result;
    }

    // This method is invoked on the main thread.
    void signalCompleted(const String& mode, bool result)
    {
        MutexLocker locker(m_mutex);
        if (!m_workerLoaderProxy)
            return;
        m_workerLoaderProxy->postTaskForModeToWorkerContext(createCallbackTask(&didComplete, this, result), mode);
    }

private:
    AllowDatabaseMainThreadBridge(WebCore::WorkerLoaderProxy* workerLoaderProxy, const String& mode, WebCommonWorkerClient* commonClient, WebFrame* frame, const String& name, const String& displayName, unsigned long estimatedSize)
        : m_result(false)
        , m_workerLoaderProxy(workerLoaderProxy)
    {
        WebWorkerBase::dispatchTaskToMainThread(
            createCallbackTask(&allowDatabaseTask, mode, WebCore::AllowCrossThreadAccess(commonClient),
                               WebCore::AllowCrossThreadAccess(frame),
                               name, displayName, estimatedSize,
                               this));
    }

    static void allowDatabaseTask(WebCore::ScriptExecutionContext* context, const String mode, WebCommonWorkerClient* commonClient, WebFrame* frame, const String name, const String displayName, unsigned long estimatedSize, PassRefPtr<AllowDatabaseMainThreadBridge> bridge)
    {
        bool allowDatabase = commonClient ? commonClient->allowDatabase(frame, name, displayName, estimatedSize) : false;
        bridge->signalCompleted(mode, allowDatabase);
    }

    static void didComplete(WebCore::ScriptExecutionContext* context, PassRefPtr<AllowDatabaseMainThreadBridge> bridge, bool result)
    {
        bridge->m_result = result;
    }

    bool m_result;
    Mutex m_mutex;
    WebCore::WorkerLoaderProxy* m_workerLoaderProxy;
};

bool allowDatabaseForWorker(WebCommonWorkerClient* commonClient, WebFrame* frame, const WebString& name, const WebString& displayName, unsigned long estimatedSize)
{
    WebCore::WorkerScriptController* controller = WebCore::WorkerScriptController::controllerForContext();
    WebCore::WorkerContext* workerContext = controller->workerContext();
    WebCore::WorkerThread* workerThread = workerContext->thread();
    WebCore::WorkerRunLoop& runLoop = workerThread->runLoop();
    WebCore::WorkerLoaderProxy* workerLoaderProxy =  &workerThread->workerLoaderProxy();

    // Create a unique mode just for this synchronous call.
    String mode = allowDatabaseMode;
    mode.append(String::number(runLoop.createUniqueId()));

    RefPtr<AllowDatabaseMainThreadBridge> bridge = AllowDatabaseMainThreadBridge::create(workerLoaderProxy, mode, commonClient, frame, name, displayName, estimatedSize);

    // Either the bridge returns, or the queue gets terminated.
    if (runLoop.runInMode(workerContext, mode) == MessageQueueTerminated) {
        bridge->cancel();
        return false;
    }

    return bridge->result();
}

#endif

}

namespace WebCore {

bool DatabaseObserver::canEstablishDatabase(ScriptExecutionContext* scriptExecutionContext, const String& name, const String& displayName, unsigned long estimatedSize)
{
    ASSERT(scriptExecutionContext->isContextThread());
    ASSERT(scriptExecutionContext->isDocument() || scriptExecutionContext->isWorkerContext());
    if (scriptExecutionContext->isDocument()) {
        Document* document = static_cast<Document*>(scriptExecutionContext);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        if (!webFrame)
            return false;
        WebViewImpl* webView = webFrame->viewImpl();
        if (!webView)
            return false;
        if (webView->permissionClient())
            return webView->permissionClient()->allowDatabase(webFrame, name, displayName, estimatedSize);
    } else {
#if ENABLE(WORKERS)
        WorkerContext* workerContext = static_cast<WorkerContext*>(scriptExecutionContext);
        WorkerLoaderProxy* workerLoaderProxy = &workerContext->thread()->workerLoaderProxy();
        WebWorkerBase* webWorker = static_cast<WebWorkerBase*>(workerLoaderProxy);
        WebView* view = webWorker->view();
        if (!view)
            return false;
        return allowDatabaseForWorker(webWorker->commonClient(), view->mainFrame(), name, displayName, estimatedSize);
#else
        ASSERT_NOT_REACHED();
#endif
    }

    return true;
}

void DatabaseObserver::databaseOpened(AbstractDatabase* database)
{
    ASSERT(database->scriptExecutionContext()->isContextThread());
    WebDatabase::observer()->databaseOpened(WebDatabase(database));
}

void DatabaseObserver::databaseModified(AbstractDatabase* database)
{
    ASSERT(database->scriptExecutionContext()->isContextThread());
    WebDatabase::observer()->databaseModified(WebDatabase(database));
}

void DatabaseObserver::databaseClosed(AbstractDatabase* database)
{
    ASSERT(database->scriptExecutionContext()->isContextThread());
    WebDatabase::observer()->databaseClosed(WebDatabase(database));
}

void DatabaseObserver::reportOpenDatabaseResult(AbstractDatabase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportOpenDatabaseResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportChangeVersionResult(AbstractDatabase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportChangeVersionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportStartTransactionResult(AbstractDatabase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportStartTransactionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportCommitTransactionResult(AbstractDatabase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportCommitTransactionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportExecuteStatementResult(AbstractDatabase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportExecuteStatementResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportVacuumDatabaseResult(AbstractDatabase* database, int sqliteErrorCode)
{
    WebDatabase::observer()->reportVacuumDatabaseResult(WebDatabase(database), sqliteErrorCode);
}

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
