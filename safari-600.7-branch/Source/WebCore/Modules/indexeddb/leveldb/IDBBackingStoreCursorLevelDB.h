/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBBackingStoreCursorLevelDB_h
#define IDBBackingStoreCursorLevelDB_h

#include "IDBKey.h"
#include "IDBRecordIdentifier.h"
#include "LevelDBIterator.h"
#include <memory>

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

namespace WebCore {

class LevelDBTransaction;
class SharedBuffer;

class IDBBackingStoreCursorLevelDB : public RefCounted<IDBBackingStoreCursorLevelDB> {
public:
    enum IteratorState {
        Ready = 0,
        Seek
    };

    struct CursorOptions {
        int64_t databaseId;
        int64_t objectStoreId;
        int64_t indexId;
        Vector<char> lowKey;
        bool lowOpen;
        Vector<char> highKey;
        bool highOpen;
        bool forward;
        bool unique;
    };

    virtual PassRefPtr<IDBKey> key() const { return m_currentKey; }
    virtual bool continueFunction(const IDBKey* = 0, IteratorState = Seek);
    virtual bool advance(unsigned long);
    bool firstSeek();

    virtual PassRefPtr<IDBBackingStoreCursorLevelDB> clone() = 0;

    virtual PassRefPtr<IDBKey> primaryKey() const { return m_currentKey; }
    virtual PassRefPtr<SharedBuffer> value() const = 0;
    virtual const IDBRecordIdentifier& recordIdentifier() const { return *m_recordIdentifier; }
    virtual ~IDBBackingStoreCursorLevelDB() { }
    virtual bool loadCurrentRow() = 0;

protected:
    IDBBackingStoreCursorLevelDB(int64_t cursorID, LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
        : m_cursorID(cursorID)
        , m_transaction(transaction)
        , m_cursorOptions(cursorOptions)
        , m_recordIdentifier(IDBRecordIdentifier::create())
    {
    }
    explicit IDBBackingStoreCursorLevelDB(const IDBBackingStoreCursorLevelDB* other);

    virtual Vector<char> encodeKey(const IDBKey&) = 0;

    bool isPastBounds() const;
    bool haveEnteredRange() const;

    int64_t m_cursorID;
    LevelDBTransaction* m_transaction;
    const CursorOptions m_cursorOptions;
    std::unique_ptr<LevelDBIterator> m_iterator;
    RefPtr<IDBKey> m_currentKey;
    RefPtr<IDBRecordIdentifier> m_recordIdentifier;
};

} // namespace WebCore

#endif // USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBBackingStoreCursorLevelDB_h
