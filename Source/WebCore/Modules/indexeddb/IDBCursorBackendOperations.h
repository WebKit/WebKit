/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef IDBCursorBackendOperations_h
#define IDBCursorBackendOperations_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorBackend.h"
#include "IDBOperation.h"

namespace WebCore {

class CursorIterationOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(PassRefPtr<IDBCursorBackend> cursor, PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new CursorIterationOperation(cursor, key, callbacks));
    }
    virtual void perform(std::function<void()> completionCallback) override FINAL;

    IDBKey* key() const { return m_key.get(); }
    IDBCallbacks* callbacks() const { return m_callbacks.get(); }

private:
    CursorIterationOperation(PassRefPtr<IDBCursorBackend> cursor, PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks> callbacks)
        : m_cursor(cursor)
        , m_key(key)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBCursorBackend> m_cursor;
    RefPtr<IDBKey> m_key;
    RefPtr<IDBCallbacks> m_callbacks;
};

class CursorAdvanceOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(PassRefPtr<IDBCursorBackend> cursor, unsigned long count, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new CursorAdvanceOperation(cursor, count, callbacks));
    }
    virtual void perform(std::function<void()> completionCallback) override FINAL;

    unsigned long count() const { return m_count; }
    IDBCallbacks* callbacks() const { return m_callbacks.get(); }

private:
    CursorAdvanceOperation(PassRefPtr<IDBCursorBackend> cursor, unsigned long count, PassRefPtr<IDBCallbacks> callbacks)
        : m_cursor(cursor)
        , m_count(count)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBCursorBackend> m_cursor;
    unsigned long m_count;
    RefPtr<IDBCallbacks> m_callbacks;
};

class CursorPrefetchIterationOperation : public IDBOperation {
public:
    static PassRefPtr<IDBOperation> create(PassRefPtr<IDBCursorBackend> cursor, int numberToFetch, PassRefPtr<IDBCallbacks> callbacks)
    {
        return adoptRef(new CursorPrefetchIterationOperation(cursor, numberToFetch, callbacks));
    }
    virtual void perform(std::function<void()> completionCallback) override FINAL;

    int numberToFetch() const { return m_numberToFetch; }
    IDBCallbacks* callbacks() const { return m_callbacks.get(); }

private:
    CursorPrefetchIterationOperation(PassRefPtr<IDBCursorBackend> cursor, int numberToFetch, PassRefPtr<IDBCallbacks> callbacks)
        : m_cursor(cursor)
        , m_numberToFetch(numberToFetch)
        , m_callbacks(callbacks)
    {
    }

    RefPtr<IDBCursorBackend> m_cursor;
    int m_numberToFetch;
    RefPtr<IDBCallbacks> m_callbacks;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBCursorBackendOperations_h
