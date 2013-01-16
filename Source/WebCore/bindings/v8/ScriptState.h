/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
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

#ifndef ScriptState_h
#define ScriptState_h

#include "DOMWrapperWorld.h"
#include "ScopedPersistent.h"
#include <v8.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DOMWindow;
class DOMWrapperWorld;
class Frame;
class Node;
class Page;
class ScriptExecutionContext;
class WorkerContext;

class ScriptState {
    WTF_MAKE_NONCOPYABLE(ScriptState);
public:
    bool hadException() { return !m_exception.IsEmpty(); }
    void setException(v8::Local<v8::Value> exception)
    {
        m_exception = exception;
    }
    v8::Local<v8::Value> exception() { return m_exception; }
    void clearException() { m_exception.Clear(); }

    v8::Local<v8::Context> context() const
    {
        return v8::Local<v8::Context>::New(m_context.get());
    }

    v8::Isolate* isolate()
    {
        if (!m_isolate)
            m_isolate = v8::Isolate::GetCurrent();
        return m_isolate;
    }

    DOMWindow* domWindow() const;
    ScriptExecutionContext* scriptExecutionContext() const;

    static ScriptState* forContext(v8::Local<v8::Context>);
    static ScriptState* current();

protected:
    ScriptState() { }
    ~ScriptState();

private:
    friend ScriptState* mainWorldScriptState(Frame*);
    explicit ScriptState(v8::Handle<v8::Context>);

    static void weakReferenceCallback(v8::Persistent<v8::Value> object, void* parameter);

    v8::Local<v8::Value> m_exception;
    ScopedPersistent<v8::Context> m_context;
    v8::Isolate* m_isolate;
};

class EmptyScriptState : public ScriptState {
public:
    EmptyScriptState() : ScriptState() { }
    ~EmptyScriptState() { }
};

class ScriptStateProtectedPtr {
    WTF_MAKE_NONCOPYABLE(ScriptStateProtectedPtr);
public:
    ScriptStateProtectedPtr()
        : m_scriptState(0)
    {
    }

    ScriptStateProtectedPtr(ScriptState* scriptState)
        : m_scriptState(scriptState)
    {
        v8::HandleScope handleScope;
        // Keep the context from being GC'ed. ScriptState is guaranteed to be live while the context is live.
        m_context.set(scriptState->context());
    }

    ScriptState* get() const { return m_scriptState; }

private:
    ScriptState* m_scriptState;
    ScopedPersistent<v8::Context> m_context;
};

DOMWindow* domWindowFromScriptState(ScriptState*);
ScriptExecutionContext* scriptExecutionContextFromScriptState(ScriptState*);

bool evalEnabled(ScriptState*);
void setEvalEnabled(ScriptState*, bool);

ScriptState* mainWorldScriptState(Frame*);

ScriptState* scriptStateFromNode(DOMWrapperWorld*, Node*);
ScriptState* scriptStateFromPage(DOMWrapperWorld*, Page*);

#if ENABLE(WORKERS)
ScriptState* scriptStateFromWorkerContext(WorkerContext*);
#endif

inline DOMWrapperWorld* debuggerWorld() { return mainThreadNormalWorld(); }
inline DOMWrapperWorld* pluginWorld() { return mainThreadNormalWorld(); }

}

#endif // ScriptState_h
