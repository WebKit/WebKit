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

#include "config.h"
#include "IDBCursorBackendOperations.h"

#if ENABLE(INDEXED_DATABASE)

#include "Logging.h"

namespace WebCore {

class CallOnDestruct {
public:
    CallOnDestruct(std::function<void()> callback)
        : m_callback(callback)
    { }

    ~CallOnDestruct()
    {
        m_callback();
    }

private:
    std::function<void()> m_callback;
};

void CursorAdvanceOperation::perform(std::function<void()> completionCallback)
{
    CallOnDestruct callOnDestruct(completionCallback);

    LOG(StorageAPI, "CursorAdvanceOperation");
    if (!m_cursor->deprecatedBackingStoreCursor() || !m_cursor->deprecatedBackingStoreCursor()->advance(m_count)) {
        m_cursor->deprecatedSetBackingStoreCursor(0);
        m_callbacks->onSuccess(static_cast<SharedBuffer*>(0));
        return;
    }

    m_callbacks->onSuccess(m_cursor->key(), m_cursor->primaryKey(), m_cursor->value());
}

void CursorIterationOperation::perform(std::function<void()> completionCallback)
{
    CallOnDestruct callOnDestruct(completionCallback);

    LOG(StorageAPI, "CursorIterationOperation");
    if (!m_cursor->deprecatedBackingStoreCursor() || !m_cursor->deprecatedBackingStoreCursor()->continueFunction(m_key.get())) {
        m_cursor->deprecatedSetBackingStoreCursor(0);
        m_callbacks->onSuccess(static_cast<SharedBuffer*>(0));
        return;
    }

    m_callbacks->onSuccess(m_cursor->key(), m_cursor->primaryKey(), m_cursor->value());
}

void CursorPrefetchIterationOperation::perform(std::function<void()> completionCallback)
{
    CallOnDestruct callOnDestruct(completionCallback);

    LOG(StorageAPI, "CursorPrefetchIterationOperation");

    Vector<RefPtr<IDBKey>> foundKeys;
    Vector<RefPtr<IDBKey>> foundPrimaryKeys;
    Vector<RefPtr<SharedBuffer>> foundValues;

    if (m_cursor->deprecatedBackingStoreCursor())
        m_cursor->deprecatedSetSavedBackingStoreCursor(m_cursor->deprecatedBackingStoreCursor()->clone());

    const size_t maxSizeEstimate = 10 * 1024 * 1024;
    size_t sizeEstimate = 0;

    for (int i = 0; i < m_numberToFetch; ++i) {
        if (!m_cursor->deprecatedBackingStoreCursor() || !m_cursor->deprecatedBackingStoreCursor()->continueFunction(0)) {
            m_cursor->deprecatedSetBackingStoreCursor(0);
            break;
        }

        foundKeys.append(m_cursor->deprecatedBackingStoreCursor()->key());
        foundPrimaryKeys.append(m_cursor->deprecatedBackingStoreCursor()->primaryKey());

        switch (m_cursor->cursorType()) {
        case IndexedDB::CursorKeyOnly:
            foundValues.append(SharedBuffer::create());
            break;
        case IndexedDB::CursorKeyAndValue:
            sizeEstimate += m_cursor->deprecatedBackingStoreCursor()->value()->size();
            foundValues.append(m_cursor->deprecatedBackingStoreCursor()->value());
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        sizeEstimate += m_cursor->deprecatedBackingStoreCursor()->key()->sizeEstimate();
        sizeEstimate += m_cursor->deprecatedBackingStoreCursor()->primaryKey()->sizeEstimate();

        if (sizeEstimate > maxSizeEstimate)
            break;
    }

    if (!foundKeys.size()) {
        m_callbacks->onSuccess(static_cast<SharedBuffer*>(0));
        return;
    }

    m_callbacks->onSuccessWithPrefetch(foundKeys, foundPrimaryKeys, foundValues);
}

} // namespace WebCore

#endif // ENABLED(INDEXED_DATABASE)
