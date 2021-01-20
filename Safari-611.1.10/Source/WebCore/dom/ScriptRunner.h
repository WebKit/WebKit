/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#include "PendingScriptClient.h"
#include "Timer.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class Document;
class ScriptElement;
class LoadableScript;

class ScriptRunner : private PendingScriptClient {
    WTF_MAKE_NONCOPYABLE(ScriptRunner); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ScriptRunner(Document&);
    ~ScriptRunner();

    enum ExecutionType { ASYNC_EXECUTION, IN_ORDER_EXECUTION };
    void queueScriptForExecution(ScriptElement&, LoadableScript&, ExecutionType);
    bool hasPendingScripts() const { return !m_scriptsToExecuteSoon.isEmpty() || !m_scriptsToExecuteInOrder.isEmpty() || !m_pendingAsyncScripts.isEmpty(); }
    void suspend();
    void resume();
    void notifyScriptReady(ScriptElement*, ExecutionType);

    void didBeginYieldingParser() { suspend(); }
    void didEndYieldingParser() { resume(); }

    void documentFinishedParsing();

private:
    void timerFired();

    void notifyFinished(PendingScript&) override;

    Document& m_document;
    Vector<Ref<PendingScript>> m_scriptsToExecuteInOrder;
    Vector<RefPtr<PendingScript>> m_scriptsToExecuteSoon; // http://www.whatwg.org/specs/web-apps/current-work/#set-of-scripts-that-will-execute-as-soon-as-possible
    HashSet<Ref<PendingScript>> m_pendingAsyncScripts;
    Timer m_timer;
};

}
