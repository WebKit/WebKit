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

#ifndef IDBCallbacks_h
#define IDBCallbacks_h

#include "ActiveDOMObject.h"
#include "IDBDatabaseError.h"
#include "Timer.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

// All IndexedDB callbacks must implement this class.  It handles the asynchronous firing of
// the callbacks.
template <typename ResultType>
class IDBCallbacks : public RefCounted<IDBCallbacks<ResultType> >, public ActiveDOMObject {
public:
    IDBCallbacks(ScriptExecutionContext* scriptExecutionContext, void* upcastPointer)
        : ActiveDOMObject(scriptExecutionContext, upcastPointer)
        , m_timer(this, &IDBCallbacks::timerFired)
    {
    }

    virtual ~IDBCallbacks() { }

    void onSuccess(PassRefPtr<ResultType> result)
    {
        ASSERT(!m_result);
        ASSERT(!m_error);
        m_result = result;
        ASSERT(m_result);

        ASSERT(!m_timer.isActive());
        m_selfRef = this;
        m_timer.startOneShot(0);
    }

    void onError(PassRefPtr<IDBDatabaseError> error)
    {
        ASSERT(!m_result);
        ASSERT(!m_error);
        m_error = error;
        ASSERT(m_error);

        ASSERT(!m_timer.isActive());
        m_selfRef = this;
        m_timer.startOneShot(0);
    }

protected:
    virtual void onSuccessAsync(PassRefPtr<ResultType>) = 0;
    virtual void onErrorAsync(PassRefPtr<IDBDatabaseError>) = 0;

    void timerFired(Timer<IDBCallbacks>*)
    {
        if (m_result) {
            onSuccessAsync(m_result);
            m_result = 0;
        } else {
            onErrorAsync(m_error);
            m_error = 0;
        }
        m_selfRef = 0; // May trigger a delete immediately.
    }

private:
    Timer<IDBCallbacks> m_timer;
    RefPtr<IDBCallbacks> m_selfRef;
    RefPtr<ResultType> m_result;
    RefPtr<IDBDatabaseError> m_error;
};

} // namespace WebCore

#endif

#endif // IDBCallbacks_h
