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

#include "config.h"
#include "IDBRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "IDBDatabaseRequest.h"
#include "IDBErrorEvent.h"
#include "IDBSuccessEvent.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

IDBRequest::IDBRequest(ScriptExecutionContext* context, PassRefPtr<IDBAny> source)
    : ActiveDOMObject(context, this)
    , m_source(source)
    , m_result(IDBAny::create())
    , m_timer(this, &IDBRequest::timerFired)
    , m_stopped(false)
    , m_aborted(false)
    , m_readyState(INITIAL)
{
}

IDBRequest::~IDBRequest()
{
    if (m_readyState != DONE)
        abort();
}

void IDBRequest::onError(PassRefPtr<IDBDatabaseError> error)
{
    onEventCommon();
    m_error = error;
}

void IDBRequest::onSuccess(PassRefPtr<IDBDatabase> idbDatabase)
{
    onEventCommon();
    m_result->set(IDBDatabaseRequest::create(idbDatabase));
}

void IDBRequest::onSuccess(PassRefPtr<SerializedScriptValue> serializedScriptValue)
{
    onEventCommon();
    m_result->set(serializedScriptValue);
}

void IDBRequest::abort()
{
    m_timer.stop();
    m_aborted = true;
    // FIXME: This should cancel any pending work being done in the backend.
}

ScriptExecutionContext* IDBRequest::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void IDBRequest::stop()
{
    abort();
    m_selfRef = 0; // Could trigger a delete.
}

void IDBRequest::suspend()
{
    m_timer.stop();
    m_stopped = true;
}

void IDBRequest::resume()
{
    m_stopped = false;
    // We only hold our self ref when we're waiting to dispatch an event.
    if (m_selfRef && !m_aborted)
        m_timer.startOneShot(0);
}

EventTargetData* IDBRequest::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* IDBRequest::ensureEventTargetData()
{
    return &m_eventTargetData;
}

void IDBRequest::timerFired(Timer<IDBRequest>*)
{
    ASSERT(m_readyState == DONE);
    ASSERT(m_selfRef);
    ASSERT(!m_stopped);
    ASSERT(!m_aborted);

    // We need to keep self-referencing ourself, otherwise it's possible we'll be deleted.
    // But in some cases, suspend() could be called while we're dispatching an event, so we
    // need to make sure that resume() doesn't re-start the timer based on m_selfRef being set.
    RefPtr<IDBRequest> selfRef = m_selfRef.release();

    if (m_error) {
        ASSERT(m_result->type() == IDBAny::UndefinedType);
        dispatchEvent(IDBErrorEvent::create(m_source, *m_error));
    } else {
        ASSERT(m_result->type() != IDBAny::UndefinedType);
        dispatchEvent(IDBSuccessEvent::create(m_source, m_result));        
    }
}

void IDBRequest::onEventCommon()
{
    ASSERT(m_readyState < DONE);
    ASSERT(m_result->type() == IDBAny::UndefinedType);
    ASSERT(!m_error);
    ASSERT(!m_selfRef);
    ASSERT(!m_timer.isActive());

    if (m_aborted)
        return;

    m_readyState = DONE;
    m_selfRef = this;
    if (!m_stopped)
        m_timer.startOneShot(0);
}

} // namespace WebCore

#endif
