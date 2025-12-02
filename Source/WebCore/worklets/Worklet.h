/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "ActiveDOMObject.h"
#include "ContextDestructionObserver.h"
#include "JSDOMPromiseDeferredForward.h"
#include "ScriptWrappable.h"
#include "WorkletOptions.h"
#include <wtf/HashSet.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class WorkletGlobalScopeProxy;
class WorkletPendingTasks;

class Worklet : public RefCounted<Worklet>, public ScriptWrappable, public ActiveDOMObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(Worklet);
public:
    virtual ~Worklet();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    virtual void addModule(const String& moduleURL, WorkletOptions&&, DOMPromiseDeferred<void>&&);

    void finishPendingTasks(WorkletPendingTasks&);
    Document* document();

    const Vector<Ref<WorkletGlobalScopeProxy>>& proxies() const { return m_proxies; }
    const String& identifier() const { return m_identifier; }

    virtual bool isAudioWorklet() const { return false; }

protected:
    explicit Worklet(Document&);

private:
    virtual Vector<Ref<WorkletGlobalScopeProxy>> createGlobalScopes() = 0;

    String m_identifier;
    Vector<Ref<WorkletGlobalScopeProxy>> m_proxies;
    HashSet<RefPtr<WorkletPendingTasks>> m_pendingTasksSet;
};

} // namespace WebCore
