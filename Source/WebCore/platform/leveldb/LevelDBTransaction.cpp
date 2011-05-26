/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "LevelDBTransaction.h"

#include "LevelDBDatabase.h"
#include "LevelDBSlice.h"
#include "LevelDBWriteBatch.h"

#if ENABLE(INDEXED_DATABASE)
#if ENABLE(LEVELDB)

namespace WebCore {

PassRefPtr<LevelDBTransaction> LevelDBTransaction::create(LevelDBDatabase* db)
{
    return adoptRef(new LevelDBTransaction(db));
}

LevelDBTransaction::LevelDBTransaction(LevelDBDatabase* db)
    : m_db(db)
    , m_comparator(db->comparator())
    , m_finished(false)
{
    m_tree.abstractor().m_comparator = m_comparator;
}

void LevelDBTransaction::clearTree()
{
    TreeType::Iterator iterator;
    iterator.start_iter_least(m_tree);

    while (*iterator) {
        delete *iterator;
        ++iterator;
    }
    m_tree.purge();
}

LevelDBTransaction::~LevelDBTransaction()
{
    clearTree();
}

static void initVector(const LevelDBSlice& slice, Vector<char>* vector)
{
    vector->clear();
    vector->append(slice.begin(), slice.end() - slice.begin());
}

bool LevelDBTransaction::set(const LevelDBSlice& key, const Vector<char>& value, bool deleted)
{
    ASSERT(!m_finished);
    bool newNode = false;
    AVLTreeNode* node = m_tree.search(key);

    if (!node) {
        node = new AVLTreeNode;
        initVector(key, &node->key);
        m_tree.insert(node);
        newNode = true;
    }
    node->value = value;
    node->deleted = deleted;

    if (newNode)
        resetIterators();
    return true;
}

bool LevelDBTransaction::put(const LevelDBSlice& key, const Vector<char>& value)
{
    return set(key, value, false);
}

bool LevelDBTransaction::remove(const LevelDBSlice& key)
{
    return set(key, Vector<char>(), true);
}

bool LevelDBTransaction::get(const LevelDBSlice& key, Vector<char>& value)
{
    ASSERT(!m_finished);
    AVLTreeNode* node = m_tree.search(key);

    if (node) {
        if (node->deleted)
            return false;

        value = node->value;
        return true;
    }

    return m_db->get(key, value);
}

bool LevelDBTransaction::commit()
{
    ASSERT(!m_finished);
    OwnPtr<LevelDBWriteBatch> writeBatch = LevelDBWriteBatch::create();

    TreeType::Iterator iterator;
    iterator.start_iter_least(m_tree);

    while (*iterator) {
        AVLTreeNode* node = *iterator;
        if (!node->deleted)
            writeBatch->put(node->key, node->value);
        else
            writeBatch->remove(node->key);
        ++iterator;
    }

    if (!m_db->write(*writeBatch))
        return false;

    clearTree();
    m_finished = true;
    return true;
}

void LevelDBTransaction::rollback()
{
    ASSERT(!m_finished);
    m_finished = true;
    clearTree();
}

PassOwnPtr<LevelDBIterator> LevelDBTransaction::createIterator()
{
    return TransactionIterator::create(this);
}

PassOwnPtr<LevelDBTransaction::TreeIterator> LevelDBTransaction::TreeIterator::create(LevelDBTransaction* transaction)
{
    return adoptPtr(new TreeIterator(transaction));
}

bool LevelDBTransaction::TreeIterator::isValid() const
{
    return *m_iterator;
}

void LevelDBTransaction::TreeIterator::seekToLast()
{
    m_iterator.start_iter_greatest(*m_tree);
    if (isValid())
        m_key = (*m_iterator)->key;
}

void LevelDBTransaction::TreeIterator::seek(const LevelDBSlice& target)
{
    m_iterator.start_iter(*m_tree, target, TreeType::EQUAL);
    if (!isValid())
        m_iterator.start_iter(*m_tree, target, TreeType::GREATER);

    if (isValid())
        m_key = (*m_iterator)->key;
}

void LevelDBTransaction::TreeIterator::next()
{
    ASSERT(isValid());
    ++m_iterator;
    if (isValid()) {
        ASSERT(m_transaction->m_comparator->compare((*m_iterator)->key, m_key) > 0);
        m_key = (*m_iterator)->key;
    }
}

void LevelDBTransaction::TreeIterator::prev()
{
    ASSERT(isValid());
    --m_iterator;
    if (isValid()) {
        ASSERT(m_tree->abstractor().m_comparator->compare((*m_iterator)->key, m_key) < 0);
        m_key = (*m_iterator)->key;
    }
}

LevelDBSlice LevelDBTransaction::TreeIterator::key() const
{
    ASSERT(isValid());
    return m_key;
}

LevelDBSlice LevelDBTransaction::TreeIterator::value() const
{
    ASSERT(isValid());
    ASSERT(!isDeleted());
    return (*m_iterator)->value;
}

bool LevelDBTransaction::TreeIterator::isDeleted() const
{
    ASSERT(isValid());
    return (*m_iterator)->deleted;
}

void LevelDBTransaction::TreeIterator::reset()
{
    // FIXME: Be lazy: set a flag and do the actual reset next time we use the iterator.
    if (isValid()) {
        m_iterator.start_iter(*m_tree, m_key, TreeType::EQUAL);
        ASSERT(isValid());
    }
}

LevelDBTransaction::TreeIterator::~TreeIterator()
{
    m_transaction->unregisterIterator(this);
}

LevelDBTransaction::TreeIterator::TreeIterator(LevelDBTransaction* transaction)
    : m_tree(&transaction->m_tree)
    , m_transaction(transaction)
{
    transaction->registerIterator(this);
}

PassOwnPtr<LevelDBTransaction::TransactionIterator> LevelDBTransaction::TransactionIterator::create(PassRefPtr<LevelDBTransaction> transaction)
{
    return adoptPtr(new TransactionIterator(transaction));
}

LevelDBTransaction::TransactionIterator::TransactionIterator(PassRefPtr<LevelDBTransaction> transaction)
    : m_transaction(transaction)
    , m_comparator(m_transaction->m_comparator)
    , m_treeIterator(TreeIterator::create(m_transaction.get()))
    , m_dbIterator(m_transaction->m_db->createIterator())
    , m_current(0)
    , m_direction(kForward)
{
}

bool LevelDBTransaction::TransactionIterator::isValid() const
{
    return m_current;
}

void LevelDBTransaction::TransactionIterator::seekToLast()
{
    m_treeIterator->seekToLast();
    m_dbIterator->seekToLast();
    m_direction = kReverse;

    handleConflictsAndDeletes();
    setCurrentIteratorToLargestKey();
}

void LevelDBTransaction::TransactionIterator::seek(const LevelDBSlice& target)
{
    m_treeIterator->seek(target);
    m_dbIterator->seek(target);
    m_direction = kForward;

    handleConflictsAndDeletes();
    setCurrentIteratorToSmallestKey();
}

void LevelDBTransaction::TransactionIterator::next()
{
    ASSERT(isValid());

    if (m_direction != kForward) {
        // Ensure the non-current iterator is positioned after key().

        LevelDBIterator* nonCurrent = (m_current == m_dbIterator.get()) ? m_treeIterator.get() : m_dbIterator.get();

        nonCurrent->seek(key());
        if (nonCurrent->isValid() && !m_comparator->compare(nonCurrent->key(), key()))
            nonCurrent->next(); // Take an extra step so the non-current key is strictly greater than key().

        ASSERT(!nonCurrent->isValid() || m_comparator->compare(nonCurrent->key(), key()) > 0);

        m_direction = kForward;
    }

    m_current->next();
    handleConflictsAndDeletes();
    setCurrentIteratorToSmallestKey();
}

void LevelDBTransaction::TransactionIterator::prev()
{
    ASSERT(isValid());

    if (m_direction != kReverse) {
        // Ensure the non-current iterator is positioned before key().

        LevelDBIterator* nonCurrent = (m_current == m_dbIterator.get()) ? m_treeIterator.get() : m_dbIterator.get();

        nonCurrent->seek(key());
        if (nonCurrent->isValid()) {
            // Iterator is at first entry >= key().
            // Step back once to entry < key.
            // This is why we don't check for the keys being the same before
            // stepping, like we do in next() above.
            nonCurrent->prev();
        } else
            nonCurrent->seekToLast(); // Iterator has no entries >= key(). Position at last entry.

        ASSERT(!nonCurrent->isValid() || m_comparator->compare(nonCurrent->key(), key()) < 0);

        m_direction = kReverse;
    }

    m_current->prev();
    handleConflictsAndDeletes();
    setCurrentIteratorToLargestKey();
}

LevelDBSlice LevelDBTransaction::TransactionIterator::key() const
{
    ASSERT(isValid());
    return m_current->key();
}

LevelDBSlice LevelDBTransaction::TransactionIterator::value() const
{
    ASSERT(isValid());
    return m_current->value();
}

void LevelDBTransaction::TransactionIterator::handleConflictsAndDeletes()
{
    bool loop = true;

    while (loop) {
        loop = false;

        if (m_treeIterator->isValid() && m_dbIterator->isValid() && !m_comparator->compare(m_treeIterator->key(), m_dbIterator->key())) {
            // For equal keys, the tree iterator takes precedence, so move the database iterator another step.
            if (m_direction == kForward)
                m_dbIterator->next();
            else
                m_dbIterator->prev();
        }

        if (m_treeIterator->isValid() && m_treeIterator->isDeleted()) {
            // If the tree iterator is on a delete marker, take another step.
            if (m_direction == kForward)
                m_treeIterator->next();
            else
                m_treeIterator->prev();

            loop = true;
        }
    }
}

void LevelDBTransaction::TransactionIterator::setCurrentIteratorToSmallestKey()
{
    LevelDBIterator* smallest = 0;

    if (m_treeIterator->isValid())
        smallest = m_treeIterator.get();

    if (m_dbIterator->isValid()) {
        if (!smallest || m_comparator->compare(m_dbIterator->key(), smallest->key()) < 0)
            smallest = m_dbIterator.get();
    }

    m_current = smallest;
}

void LevelDBTransaction::TransactionIterator::setCurrentIteratorToLargestKey()
{
    LevelDBIterator* largest = 0;

    if (m_treeIterator->isValid())
        largest = m_treeIterator.get();

    if (m_dbIterator->isValid()) {
        if (!largest || m_comparator->compare(m_dbIterator->key(), largest->key()) > 0)
            largest = m_dbIterator.get();
    }

    m_current = largest;
}

void LevelDBTransaction::registerIterator(TreeIterator* iterator)
{
    ASSERT(!m_treeIterators.contains(iterator));
    m_treeIterators.add(iterator);
}

void LevelDBTransaction::unregisterIterator(TreeIterator* iterator)
{
    ASSERT(m_treeIterators.contains(iterator));
    m_treeIterators.remove(iterator);
}

void LevelDBTransaction::resetIterators()
{
    for (HashSet<TreeIterator*>::iterator i = m_treeIterators.begin(); i != m_treeIterators.end(); ++i) {
        TreeIterator* treeIterator = *i;
        treeIterator->reset();
    }
}

} // namespace WebCore

#endif // ENABLE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
