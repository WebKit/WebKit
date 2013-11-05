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

#include "config.h"
#include "IDBBackingStoreCursorLevelDB.h"

#if ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#include "IDBBackingStoreLevelDB.h"
#include "LevelDBTransaction.h"

namespace WebCore {

IDBBackingStoreCursorLevelDB::IDBBackingStoreCursorLevelDB(const IDBBackingStoreCursorLevelDB* other)
    : m_transaction(other->m_transaction)
    , m_cursorOptions(other->m_cursorOptions)
    , m_currentKey(other->m_currentKey)
    , m_recordIdentifier(IDBRecordIdentifier::create())
{
    if (other->m_iterator) {
        m_iterator = m_transaction->createIterator();

        if (other->m_iterator->isValid()) {
            m_iterator->seek(other->m_iterator->key());
            ASSERT(m_iterator->isValid());
        }
    }

    m_recordIdentifier->reset(other->m_recordIdentifier->encodedPrimaryKey(), other->m_recordIdentifier->version());
}

bool IDBBackingStoreCursorLevelDB::firstSeek()
{
    m_iterator = m_transaction->createIterator();
    if (m_cursorOptions.forward)
        m_iterator->seek(m_cursorOptions.lowKey);
    else
        m_iterator->seek(m_cursorOptions.highKey);

    return continueFunction(0, Ready);
}

bool IDBBackingStoreCursorLevelDB::advance(unsigned long count)
{
    while (count--) {
        if (!continueFunction())
            return false;
    }
    return true;
}

bool IDBBackingStoreCursorLevelDB::continueFunction(const IDBKey* key, IteratorState nextState)
{
    RefPtr<IDBKey> previousKey = m_currentKey;

    bool firstIteration = true;

    // When iterating with PrevNoDuplicate, spec requires that the
    // value we yield for each key is the first duplicate in forwards
    // order.
    RefPtr<IDBKey> lastDuplicateKey;

    bool forward = m_cursorOptions.forward;

    for (;;) {
        if (nextState == Seek) {
            // FIXME: Optimize seeking for reverse cursors as well.
            if (firstIteration && key && forward) {
                m_iterator->seek(encodeKey(*key));
                firstIteration = false;
            } else if (forward)
                m_iterator->next();
            else
                m_iterator->prev();
        } else
            nextState = Seek; // for subsequent iterations

        if (!m_iterator->isValid()) {
            if (!forward && lastDuplicateKey.get()) {
                // We need to walk forward because we hit the end of
                // the data.
                forward = true;
                continue;
            }

            return false;
        }

        if (isPastBounds()) {
            if (!forward && lastDuplicateKey.get()) {
                // We need to walk forward because now we're beyond the
                // bounds defined by the cursor.
                forward = true;
                continue;
            }

            return false;
        }

        if (!haveEnteredRange())
            continue;

        // The row may not load because there's a stale entry in the
        // index. This is not fatal.
        if (!loadCurrentRow())
            continue;

        if (key) {
            if (forward) {
                if (m_currentKey->isLessThan(key))
                    continue;
            } else {
                if (key->isLessThan(m_currentKey.get()))
                    continue;
            }
        }

        if (m_cursorOptions.unique) {

            if (m_currentKey->isEqual(previousKey.get())) {
                // We should never be able to walk forward all the way
                // to the previous key.
                ASSERT(!lastDuplicateKey.get());
                continue;
            }

            if (!forward) {
                if (!lastDuplicateKey.get()) {
                    lastDuplicateKey = m_currentKey;
                    continue;
                }

                // We need to walk forward because we hit the boundary
                // between key ranges.
                if (!lastDuplicateKey->isEqual(m_currentKey.get())) {
                    forward = true;
                    continue;
                }

                continue;
            }
        }
        break;
    }

    ASSERT(!lastDuplicateKey.get() || (forward && lastDuplicateKey->isEqual(m_currentKey.get())));
    return true;
}

bool IDBBackingStoreCursorLevelDB::haveEnteredRange() const
{
    if (m_cursorOptions.forward) {
        if (m_cursorOptions.lowOpen)
            return IDBBackingStoreLevelDB::compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) > 0;

        return IDBBackingStoreLevelDB::compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) >= 0;
    }
    if (m_cursorOptions.highOpen)
        return IDBBackingStoreLevelDB::compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) < 0;

    return IDBBackingStoreLevelDB::compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) <= 0;
}

bool IDBBackingStoreCursorLevelDB::isPastBounds() const
{
    if (m_cursorOptions.forward) {
        if (m_cursorOptions.highOpen)
            return IDBBackingStoreLevelDB::compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) >= 0;
        return IDBBackingStoreLevelDB::compareIndexKeys(m_iterator->key(), m_cursorOptions.highKey) > 0;
    }

    if (m_cursorOptions.lowOpen)
        return IDBBackingStoreLevelDB::compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) <= 0;
    return IDBBackingStoreLevelDB::compareIndexKeys(m_iterator->key(), m_cursorOptions.lowKey) < 0;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

