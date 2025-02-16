/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "PageIdentifier.h"
#include "ScriptExecutionContextIdentifier.h"
#include <variant>
#include <wtf/CheckedPtr.h>
#include <wtf/CheckedRef.h>
#include <wtf/FastMalloc.h>
#include <wtf/Function.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

// All of these methods should be called on the Main Thread.
// Used to send messages to the WorkerInspector on the WorkerThread.

namespace WebCore {

class ScriptExecutionContext;
class WorkerThread;

enum class WorkerThreadStartMode;

class WorkerInspectorProxy : public RefCounted<WorkerInspectorProxy>, public CanMakeWeakPtr<WorkerInspectorProxy, WeakPtrFactoryInitialization::Eager> {
    WTF_MAKE_TZONE_ALLOCATED(WorkerInspectorProxy);
    WTF_MAKE_NONCOPYABLE(WorkerInspectorProxy);
public:
    static Ref<WorkerInspectorProxy> create(const String& identifier)
    {
        return adoptRef(*new WorkerInspectorProxy(identifier));
    }

    ~WorkerInspectorProxy();

    // A Worker's inspector messages come in and go out through the Page's WorkerAgent.
    class PageChannel : public CanMakeThreadSafeCheckedPtr<PageChannel> {
        WTF_MAKE_TZONE_ALLOCATED(PageChannel);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PageChannel);

    public:
        virtual ~PageChannel() = default;

        virtual void ref() const = 0;
        virtual void deref() const = 0;
        virtual void sendMessageFromWorkerToFrontend(WorkerInspectorProxy&, String&&) = 0;
    };

    static Vector<Ref<WorkerInspectorProxy>> proxiesForPage(PageIdentifier);
    static Vector<Ref<WorkerInspectorProxy>> proxiesForWorkerGlobalScope(ScriptExecutionContextIdentifier);

    const URL& url() const { return m_url; }
    const String& name() const { return m_name; }
    const String& identifier() const { return m_identifier; }
    ScriptExecutionContext* scriptExecutionContext() const { return m_scriptExecutionContext.get(); }

    WorkerThreadStartMode workerStartMode(ScriptExecutionContext&);
    void workerStarted(ScriptExecutionContext&, WorkerThread*, const URL&, const String& name);
    void workerTerminated();

    void resumeWorkerIfPaused();
    void connectToWorkerInspectorController(PageChannel&);
    void disconnectFromWorkerInspectorController();
    void sendMessageToWorkerInspectorController(const String&);
    void sendMessageFromWorkerToFrontend(String&&);

private:
    explicit WorkerInspectorProxy(const String& identifier);

    using PageOrWorkerGlobalScopeIdentifier = std::variant<PageIdentifier, ScriptExecutionContextIdentifier>;
    static std::optional<PageOrWorkerGlobalScopeIdentifier> pageOrWorkerGlobalScopeIdentifier(ScriptExecutionContext&);

    void addToProxyMap();
    void removeFromProxyMap();

    RefPtr<ScriptExecutionContext> m_scriptExecutionContext;
    std::optional<PageOrWorkerGlobalScopeIdentifier> m_contextIdentifier;
    RefPtr<WorkerThread> m_workerThread;
    String m_identifier;
    URL m_url;
    String m_name;
    CheckedPtr<PageChannel> m_pageChannel;
};

} // namespace WebCore
