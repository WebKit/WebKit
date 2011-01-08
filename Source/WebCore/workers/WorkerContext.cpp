/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#if ENABLE(WORKERS)

#include "WorkerContext.h"

#include "AbstractDatabase.h"
#include "ActiveDOMObject.h"
#include "Database.h"
#include "DatabaseCallback.h"
#include "DatabaseSync.h"
#include "DatabaseTracker.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "ErrorEvent.h"
#include "Event.h"
#include "EventException.h"
#include "InspectorController.h"
#include "KURL.h"
#include "MessagePort.h"
#include "NotImplemented.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "SecurityOrigin.h"
#include "WorkerLocation.h"
#include "WorkerNavigator.h"
#include "WorkerObjectProxy.h"
#include "WorkerScriptLoader.h"
#include "WorkerThread.h"
#include "WorkerThreadableLoader.h"
#include "XMLHttpRequestException.h"
#include <wtf/RefPtr.h>
#include <wtf/UnusedParam.h>

#if ENABLE(NOTIFICATIONS)
#include "NotificationCenter.h"
#endif

#if ENABLE(FILE_SYSTEM)
#include "AsyncFileSystem.h"
#include "DOMFileSystem.h"
#include "DOMFileSystemSync.h"
#include "ErrorCallback.h"
#include "FileError.h"
#include "FileException.h"
#include "FileSystemCallback.h"
#include "FileSystemCallbacks.h"
#include "LocalFileSystem.h"
#include "SyncCallbackHelper.h"
#endif

namespace WebCore {

class CloseWorkerContextTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<CloseWorkerContextTask> create()
    {
        return new CloseWorkerContextTask;
    }

    virtual void performTask(ScriptExecutionContext *context)
    {
        ASSERT(context->isWorkerContext());
        WorkerContext* workerContext = static_cast<WorkerContext*>(context);
        // Notify parent that this context is closed. Parent is responsible for calling WorkerThread::stop().
        workerContext->thread()->workerReportingProxy().workerContextClosed();
    }

    virtual bool isCleanupTask() const { return true; }
};

WorkerContext::WorkerContext(const KURL& url, const String& userAgent, WorkerThread* thread)
    : m_url(url)
    , m_userAgent(userAgent)
    , m_script(new WorkerScriptController(this))
    , m_thread(thread)
    , m_closing(false)
    , m_reportingException(false)
{
    setSecurityOrigin(SecurityOrigin::create(url));
}

WorkerContext::~WorkerContext()
{
    ASSERT(currentThread() == thread()->threadID());
#if ENABLE(NOTIFICATIONS)
    m_notifications.clear();
#endif

    // Make sure we have no observers.
    notifyObserversOfStop();

    // Notify proxy that we are going away. This can free the WorkerThread object, so do not access it after this.
    thread()->workerReportingProxy().workerContextDestroyed();
}

ScriptExecutionContext* WorkerContext::scriptExecutionContext() const
{
    return const_cast<WorkerContext*>(this);
}

const KURL& WorkerContext::virtualURL() const
{
    return m_url;
}

KURL WorkerContext::virtualCompleteURL(const String& url) const
{
    return completeURL(url);
}

KURL WorkerContext::completeURL(const String& url) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the KURL constructor to have this behavior?
    if (url.isNull())
        return KURL();
    // Always use UTF-8 in Workers.
    return KURL(m_url, url);
}

String WorkerContext::userAgent(const KURL&) const
{
    return m_userAgent;
}

WorkerLocation* WorkerContext::location() const
{
    if (!m_location)
        m_location = WorkerLocation::create(m_url);
    return m_location.get();
}

void WorkerContext::close()
{
    if (m_closing)
        return;

    m_closing = true;
    // Let current script run to completion but prevent future script evaluations.
    m_script->forbidExecution(WorkerScriptController::LetRunningScriptFinish);
    postTask(CloseWorkerContextTask::create());
}

WorkerNavigator* WorkerContext::navigator() const
{
    if (!m_navigator)
        m_navigator = WorkerNavigator::create(m_userAgent);
    return m_navigator.get();
}

bool WorkerContext::hasPendingActivity() const
{
    ActiveDOMObjectsMap& activeObjects = activeDOMObjects();
    ActiveDOMObjectsMap::const_iterator activeObjectsEnd = activeObjects.end();
    for (ActiveDOMObjectsMap::const_iterator iter = activeObjects.begin(); iter != activeObjectsEnd; ++iter) {
        if (iter->first->hasPendingActivity())
            return true;
    }

    // Keep the worker active as long as there is a MessagePort with pending activity or that is remotely entangled.
    HashSet<MessagePort*>::const_iterator messagePortsEnd = messagePorts().end();
    for (HashSet<MessagePort*>::const_iterator iter = messagePorts().begin(); iter != messagePortsEnd; ++iter) {
        if ((*iter)->hasPendingActivity() || ((*iter)->isEntangled() && !(*iter)->locallyEntangledPort()))
            return true;
    }

    return false;
}

void WorkerContext::postTask(PassOwnPtr<Task> task)
{
    thread()->runLoop().postTask(task);
}

int WorkerContext::setTimeout(PassOwnPtr<ScheduledAction> action, int timeout)
{
    return DOMTimer::install(scriptExecutionContext(), action, timeout, true);
}

void WorkerContext::clearTimeout(int timeoutId)
{
    DOMTimer::removeById(scriptExecutionContext(), timeoutId);
}

int WorkerContext::setInterval(PassOwnPtr<ScheduledAction> action, int timeout)
{
    return DOMTimer::install(scriptExecutionContext(), action, timeout, false);
}

void WorkerContext::clearInterval(int timeoutId)
{
    DOMTimer::removeById(scriptExecutionContext(), timeoutId);
}

void WorkerContext::importScripts(const Vector<String>& urls, ExceptionCode& ec)
{
    ec = 0;
    Vector<String>::const_iterator urlsEnd = urls.end();
    Vector<KURL> completedURLs;
    for (Vector<String>::const_iterator it = urls.begin(); it != urlsEnd; ++it) {
        const KURL& url = scriptExecutionContext()->completeURL(*it);
        if (!url.isValid()) {
            ec = SYNTAX_ERR;
            return;
        }
        completedURLs.append(url);
    }
    Vector<KURL>::const_iterator end = completedURLs.end();

    for (Vector<KURL>::const_iterator it = completedURLs.begin(); it != end; ++it) {
        WorkerScriptLoader scriptLoader(ResourceRequestBase::TargetIsScript);
        scriptLoader.loadSynchronously(scriptExecutionContext(), *it, AllowCrossOriginRequests);

        // If the fetching attempt failed, throw a NETWORK_ERR exception and abort all these steps.
        if (scriptLoader.failed()) {
            ec = XMLHttpRequestException::NETWORK_ERR;
            return;
        }

#if ENABLE(INSPECTOR)
        if (InspectorController* inspector = scriptExecutionContext()->inspectorController())
            inspector->scriptImported(scriptLoader.identifier(), scriptLoader.script());
#endif

        ScriptValue exception;
        m_script->evaluate(ScriptSourceCode(scriptLoader.script(), *it), &exception);
        if (!exception.hasNoValue()) {
            m_script->setException(exception);
            return;
        }
    }
}

void WorkerContext::reportException(const String& errorMessage, int lineNumber, const String& sourceURL)
{
    bool errorHandled = false;
    if (!m_reportingException) {
        if (onerror()) {
            m_reportingException = true;
            RefPtr<ErrorEvent> errorEvent(ErrorEvent::create(errorMessage, sourceURL, lineNumber));
            onerror()->handleEvent(this, errorEvent.get());
            errorHandled = errorEvent->defaultPrevented();
            m_reportingException = false;
        }
    }
    if (!errorHandled)
        thread()->workerReportingProxy().postExceptionToWorkerObject(errorMessage, lineNumber, sourceURL);
}

void WorkerContext::addMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    thread()->workerReportingProxy().postConsoleMessageToWorkerObject(source, type, level, message, lineNumber, sourceURL);
}

#if ENABLE(NOTIFICATIONS)
NotificationCenter* WorkerContext::webkitNotifications() const
{
    if (!m_notifications)
        m_notifications = NotificationCenter::create(scriptExecutionContext(), m_thread->getNotificationPresenter());
    return m_notifications.get();
}
#endif

#if ENABLE(DATABASE)
PassRefPtr<Database> WorkerContext::openDatabase(const String& name, const String& version, const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback, ExceptionCode& ec)
{
    if (!securityOrigin()->canAccessDatabase() || !AbstractDatabase::isAvailable()) {
        ec = SECURITY_ERR;
        return 0;
    }

    return Database::openDatabase(this, name, version, displayName, estimatedSize, creationCallback, ec);
}

void WorkerContext::databaseExceededQuota(const String&)
{
#if !PLATFORM(CHROMIUM)
    // FIXME: This needs a real implementation; this is a temporary solution for testing.
    const unsigned long long defaultQuota = 5 * 1024 * 1024;
    DatabaseTracker::tracker().setQuota(securityOrigin(), defaultQuota);
#endif
}

PassRefPtr<DatabaseSync> WorkerContext::openDatabaseSync(const String& name, const String& version, const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback, ExceptionCode& ec)
{
    if (!securityOrigin()->canAccessDatabase() || !AbstractDatabase::isAvailable()) {
        ec = SECURITY_ERR;
        return 0;
    }

    return DatabaseSync::openDatabaseSync(this, name, version, displayName, estimatedSize, creationCallback, ec);
}
#endif

bool WorkerContext::isContextThread() const
{
    return currentThread() == thread()->threadID();
}

bool WorkerContext::isJSExecutionTerminated() const
{
    return m_script->isExecutionForbidden();
}

EventTargetData* WorkerContext::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* WorkerContext::ensureEventTargetData()
{
    return &m_eventTargetData;
}

#if ENABLE(BLOB)
String WorkerContext::createObjectURL(Blob* blob)
{
    return scriptExecutionContext()->createPublicBlobURL(blob).string();
}

void WorkerContext::revokeObjectURL(const String& blobURLString)
{
    scriptExecutionContext()->revokePublicBlobURL(KURL(ParsedURLString, blobURLString));
}
#endif

#if ENABLE(FILE_SYSTEM)
void WorkerContext::requestFileSystem(int type, long long size, PassRefPtr<FileSystemCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    if (!AsyncFileSystem::isAvailable() || !securityOrigin()->canAccessFileSystem()) {
        DOMFileSystem::scheduleCallback(this, errorCallback, FileError::create(FileError::SECURITY_ERR));
        return;
    }

    AsyncFileSystem::Type fileSystemType = static_cast<AsyncFileSystem::Type>(type);
    if (fileSystemType != AsyncFileSystem::Temporary && fileSystemType != AsyncFileSystem::Persistent) {
        DOMFileSystem::scheduleCallback(this, errorCallback, FileError::create(FileError::INVALID_MODIFICATION_ERR));
        return;
    }

    LocalFileSystem::localFileSystem().requestFileSystem(this, fileSystemType, size, FileSystemCallbacks::create(successCallback, errorCallback, this), false);
}

PassRefPtr<DOMFileSystemSync> WorkerContext::requestFileSystemSync(int type, long long size, ExceptionCode& ec)
{
    ec = 0;
    if (!AsyncFileSystem::isAvailable() || !securityOrigin()->canAccessFileSystem()) {
        ec = FileException::SECURITY_ERR;
        return 0;
    }

    AsyncFileSystem::Type fileSystemType = static_cast<AsyncFileSystem::Type>(type);
    if (fileSystemType != AsyncFileSystem::Temporary && fileSystemType != AsyncFileSystem::Persistent) {
        ec = FileException::INVALID_MODIFICATION_ERR;
        return 0;
    }

    FileSystemSyncCallbackHelper helper;
    LocalFileSystem::localFileSystem().requestFileSystem(this, fileSystemType, size, FileSystemCallbacks::create(helper.successCallback(), helper.errorCallback(), this), true);
    return helper.getResult(ec);
}

COMPILE_ASSERT(static_cast<int>(WorkerContext::TEMPORARY) == static_cast<int>(AsyncFileSystem::Temporary), enum_mismatch);
COMPILE_ASSERT(static_cast<int>(WorkerContext::PERSISTENT) == static_cast<int>(AsyncFileSystem::Persistent), enum_mismatch);
#endif

WorkerContext::Observer::Observer(WorkerContext* context)
    : m_context(context)
{
    ASSERT(m_context && m_context->isContextThread());
    m_context->registerObserver(this);
}

WorkerContext::Observer::~Observer()
{
    if (!m_context)
        return;
    ASSERT(m_context->isContextThread());
    m_context->unregisterObserver(this);
}

void WorkerContext::Observer::stopObserving()
{
    if (!m_context)
        return;
    ASSERT(m_context->isContextThread());
    m_context->unregisterObserver(this);
    m_context = 0;
}

void WorkerContext::registerObserver(Observer* observer)
{
    ASSERT(observer);
    m_workerObservers.add(observer);
}

void WorkerContext::unregisterObserver(Observer* observer)
{
    ASSERT(observer);
    m_workerObservers.remove(observer);
}

void WorkerContext::notifyObserversOfStop()
{
    HashSet<Observer*>::iterator iter = m_workerObservers.begin();
    while (iter != m_workerObservers.end()) {
        WorkerContext::Observer* observer = *iter;
        observer->stopObserving();
        observer->notifyStop();
        iter = m_workerObservers.begin();
    }
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
