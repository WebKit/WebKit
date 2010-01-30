/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef IDBRequest_h
#define IDBRequest_h

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBDatabaseError;
class SerializedScriptValue;

class IDBRequest : public RefCounted<IDBRequest>, public ActiveDOMObject, public EventTarget {
public:
    static PassRefPtr<IDBRequest> create(ScriptExecutionContext* context)
    {
        return adoptRef(new IDBRequest(context));
    }
    ~IDBRequest();

    void abort();
    unsigned short readyState() const { return m_readyState; }
    IDBDatabaseError* error() const { return m_error.get(); }
    SerializedScriptValue* result() const { return m_result.get(); }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(success);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    using RefCounted<IDBRequest>::ref;
    using RefCounted<IDBRequest>::deref;

    // EventTarget interface
    virtual ScriptExecutionContext* scriptExecutionContext() const { return ActiveDOMObject::scriptExecutionContext(); }
    virtual IDBRequest* toIDBRequest() { return this; }

private:
    explicit IDBRequest(ScriptExecutionContext* context);

    // EventTarget interface
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

    unsigned short m_readyState;
    RefPtr<IDBDatabaseError> m_error;
    RefPtr<SerializedScriptValue> m_result;

    EventTargetData m_eventTargetData;
};

} // namespace WebCore

#endif

#endif // IDBRequest_h

