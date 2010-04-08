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

#ifndef V8CustomIDBCallbacks_h
#define V8CustomIDBCallbacks_h

#include "Document.h"
#include "Frame.h"
#include "IDBDatabaseError.h"
#include "V8CustomVoidCallback.h"
#include "V8IDBDatabaseError.h"
#include "WorldContextHandle.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

// FIXME: Maybe split common parts into a base class.
template <typename ResultType, typename ResultWrapperType>
class V8CustomIDBCallbacks : public IDBCallbacks<ResultType> {
public:
    static PassRefPtr<V8CustomIDBCallbacks> create(v8::Local<v8::Value> onSuccess, v8::Local<v8::Value> onError, ScriptExecutionContext* scriptExecutionContext)
    {
        return adoptRef(new V8CustomIDBCallbacks(onSuccess, onError, scriptExecutionContext));
    }

    virtual ~V8CustomIDBCallbacks()
    {
        m_onSuccess.Dispose();
        m_onError.Dispose();
    }

    // FIXME: Handle suspend/resume correctly.

private:
    V8CustomIDBCallbacks(v8::Local<v8::Value> onSuccess, v8::Local<v8::Value> onError, ScriptExecutionContext* scriptExecutionContext)
        : IDBCallbacks<ResultType>(scriptExecutionContext, this)
        , m_onSuccess(onSuccess->IsObject() ? v8::Persistent<v8::Object>::New(onSuccess->ToObject()) : v8::Persistent<v8::Object>())
        , m_onError(onError->IsObject() ? v8::Persistent<v8::Object>::New(onError->ToObject()) : v8::Persistent<v8::Object>())
        , m_worldContext(UseCurrentWorld)
    {
    }

    template <typename Type>
    void onEvent(v8::Persistent<v8::Object> callback, PassRefPtr<Type> value)
    {
        if (!ActiveDOMObject::scriptExecutionContext())
            return;
        if (callback.IsEmpty())
            return;

        v8::HandleScope handleScope;
        v8::Handle<v8::Context> context = toV8Context(ActiveDOMObject::scriptExecutionContext(), m_worldContext);
        if (context.IsEmpty())
            return;

        v8::Context::Scope scope(context);
        v8::Handle<v8::Value> argv[] = {
            toV8(value)
        };

        // FIXME: Make this work for workers.
        ASSERT(ActiveDOMObject::scriptExecutionContext()->isDocument());
        RefPtr<Frame> protector(static_cast<Document*>(ActiveDOMObject::scriptExecutionContext())->frame());

        bool callbackReturnValue = false;
        // FIXME: Do we care if this thing returns true (i.e. it raised an exception)?
        invokeCallback(callback, 1, argv, callbackReturnValue);
    }

    virtual void onSuccessAsync(PassRefPtr<ResultType> result)
    {
        onEvent(m_onSuccess, ResultWrapperType::create(result));
    }

    virtual void onErrorAsync(PassRefPtr<IDBDatabaseError> error)
    {
        onEvent(m_onError, error);
    }

    // FIXME: Use OwnHandles.
    v8::Persistent<v8::Object> m_onSuccess;
    v8::Persistent<v8::Object> m_onError;

    WorldContextHandle m_worldContext;
};

}

#endif

#endif // V8CustomIDBCallbacks_h
