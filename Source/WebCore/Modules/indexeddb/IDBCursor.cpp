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
#include "IDBCursor.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBAny.h"
#include "IDBCallbacks.h"
#include "IDBCursorBackendInterface.h"
#include "IDBKey.h"
#include "IDBRequest.h"
#include "IDBTracing.h"
#include "IDBTransaction.h"
#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"

namespace WebCore {

PassRefPtr<IDBCursor> IDBCursor::create(PassRefPtr<IDBCursorBackendInterface> backend, IDBRequest* request, IDBAny* source, IDBTransaction* transaction)
{
    return adoptRef(new IDBCursor(backend, request, source, transaction));
}

const AtomicString& IDBCursor::directionNext()
{
    DEFINE_STATIC_LOCAL(AtomicString, next, ("next"));
    return next;
}

const AtomicString& IDBCursor::directionNextUnique()
{
    DEFINE_STATIC_LOCAL(AtomicString, nextunique, ("nextunique"));
    return nextunique;
}

const AtomicString& IDBCursor::directionPrev()
{
    DEFINE_STATIC_LOCAL(AtomicString, prev, ("prev"));
    return prev;
}

const AtomicString& IDBCursor::directionPrevUnique()
{
    DEFINE_STATIC_LOCAL(AtomicString, prevunique, ("prevunique"));
    return prevunique;
}


IDBCursor::IDBCursor(PassRefPtr<IDBCursorBackendInterface> backend, IDBRequest* request, IDBAny* source, IDBTransaction* transaction)
    : m_backend(backend)
    , m_request(request)
    , m_source(source)
    , m_transaction(transaction)
    , m_transactionNotifier(transaction, this)
    , m_gotValue(false)
{
    ASSERT(m_backend);
    ASSERT(m_request);
    ASSERT(m_source->type() == IDBAny::IDBObjectStoreType || m_source->type() == IDBAny::IDBIndexType);
    ASSERT(m_transaction);
}

IDBCursor::~IDBCursor()
{
}

const String& IDBCursor::direction() const
{
    IDB_TRACE("IDBCursor::direction");
    ExceptionCode ec = 0;
    const AtomicString& direction = directionToString(m_backend->direction(), ec);
    ASSERT(!ec);
    return direction;
}

PassRefPtr<IDBKey> IDBCursor::key() const
{
    IDB_TRACE("IDBCursor::key");
    return m_currentKey;
}

PassRefPtr<IDBKey> IDBCursor::primaryKey() const
{
    IDB_TRACE("IDBCursor::primaryKey");
    return m_currentPrimaryKey;
}

PassRefPtr<IDBAny> IDBCursor::value() const
{
    IDB_TRACE("IDBCursor::value");
    return m_currentValue;
}

IDBAny* IDBCursor::source() const
{
    return m_source.get();
}

PassRefPtr<IDBRequest> IDBCursor::update(ScriptExecutionContext* context, PassRefPtr<SerializedScriptValue> prpValue, ExceptionCode& ec)
{
    IDB_TRACE("IDBCursor::update");

    if (!m_gotValue) {
        ec = INVALID_STATE_ERR;
        return 0;
    }
    RefPtr<SerializedScriptValue> value = prpValue;
    if (value->blobURLs().size() > 0) {
        // FIXME: Add Blob/File/FileList support
        ec = DATA_CLONE_ERR;
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->update(value, request, ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

void IDBCursor::advance(unsigned long count, ExceptionCode& ec)
{
    IDB_TRACE("IDBCursor::advance");
    if (!m_gotValue) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!m_request) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return;
    }

    if (!count) {
        // FIXME: spec says we should throw a JavaScript TypeError
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (!m_request->resetReadyState(m_transaction.get())) {
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
        return;
    }
    m_request->setCursor(this);
    m_gotValue = false;
    m_backend->advance(count, m_request, ec);
}

void IDBCursor::continueFunction(PassRefPtr<IDBKey> key, ExceptionCode& ec)
{
    IDB_TRACE("IDBCursor::continue");
    if (key && (key->type() == IDBKey::InvalidType)) {
        ec = IDBDatabaseException::DATA_ERR;
        return;
    }

    if (!m_request) {
        ec = IDBDatabaseException::TRANSACTION_INACTIVE_ERR;
        return;
    }

    if (!m_gotValue) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // FIXME: We're not using the context from when continue was called, which means the callback
    //        will be on the original context openCursor was called on. Is this right?
    if (m_request->resetReadyState(m_transaction.get())) {
        m_request->setCursor(this);
        m_gotValue = false;
        m_backend->continueFunction(key, m_request, ec);
    } else
        ec = IDBDatabaseException::NOT_ALLOWED_ERR;
}

PassRefPtr<IDBRequest> IDBCursor::deleteFunction(ScriptExecutionContext* context, ExceptionCode& ec)
{
    IDB_TRACE("IDBCursor::delete");
    if (!m_gotValue) {
        ec = INVALID_STATE_ERR;
        return 0;
    }
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_backend->deleteFunction(request, ec);
    if (ec) {
        request->markEarlyDeath();
        return 0;
    }
    return request.release();
}

void IDBCursor::postSuccessHandlerCallback()
{
    m_backend->postSuccessHandlerCallback();
}

void IDBCursor::close()
{
    ASSERT(m_request);
    m_request->finishCursor();
    m_request.clear();
}

void IDBCursor::setValueReady()
{
    m_currentKey = m_backend->key();
    m_currentPrimaryKey = m_backend->primaryKey();
    m_currentValue = IDBAny::create(m_backend->value());
    m_gotValue = true;
}

unsigned short IDBCursor::stringToDirection(const String& directionString, ExceptionCode& ec)
{
    if (directionString == IDBCursor::directionNext())
        return IDBCursor::NEXT;
    if (directionString == IDBCursor::directionNextUnique())
        return IDBCursor::NEXT_NO_DUPLICATE;
    if (directionString == IDBCursor::directionPrev())
        return IDBCursor::PREV;
    if (directionString == IDBCursor::directionPrevUnique())
        return IDBCursor::PREV_NO_DUPLICATE;

    // FIXME: should be a JavaScript TypeError. See https://bugs.webkit.org/show_bug.cgi?id=85513
    ec = IDBDatabaseException::NON_TRANSIENT_ERR;
    return 0;
}

const AtomicString& IDBCursor::directionToString(unsigned short direction, ExceptionCode& ec)
{
    switch (direction) {
    case IDBCursor::NEXT:
        return IDBCursor::directionNext();

    case IDBCursor::NEXT_NO_DUPLICATE:
        return IDBCursor::directionNextUnique();

    case IDBCursor::PREV:
        return IDBCursor::directionPrev();

    case IDBCursor::PREV_NO_DUPLICATE:
        return IDBCursor::directionPrevUnique();

    default:
        ec = IDBDatabaseException::NON_TRANSIENT_ERR;
        return IDBCursor::directionNext();
    }
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
