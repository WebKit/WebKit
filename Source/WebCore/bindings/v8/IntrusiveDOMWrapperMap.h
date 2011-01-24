/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef IntrusiveDOMWrapperMap_h
#define IntrusiveDOMWrapperMap_h

#include "DOMDataStore.h"
#include "V8Node.h"

namespace WebCore {

template <class T, int CHUNK_SIZE, class Traits>
class ChunkedTable {
  public:
    ChunkedTable() : m_chunks(0), m_current(0), m_last(0) { }

    T* add(T element)
    {
        if (m_current == m_last) {
            m_chunks = new Chunk(m_chunks);
            m_current = m_chunks->m_entries;
            m_last = m_current + CHUNK_SIZE;
        }
        ASSERT((m_chunks->m_entries <= m_current) && (m_current < m_last));
        T* p = m_current++;
        *p = element;
        return p;
    }

    void remove(T* element)
    {
        ASSERT(element);
        ASSERT(m_current > m_chunks->m_entries);
        m_current--;
        if (element != m_current)
            Traits::move(element, m_current);
        if (m_current == m_chunks->m_entries) {
            Chunk* toDelete = m_chunks;
            m_chunks = toDelete->m_previous;
            m_current = m_last = m_chunks ? m_chunks->m_entries + CHUNK_SIZE : 0;
            delete toDelete;
        }
        ASSERT(!m_chunks || ((m_chunks->m_entries < m_current) && (m_current <= m_last)));
    }

    void clear()
    {
        if (!m_chunks)
            return;

        clearEntries(m_chunks->m_entries, m_current);
        Chunk* last = m_chunks;
        while (true) {
            Chunk* previous = last->m_previous;
            if (!previous)
                break;
            delete last;
            clearEntries(previous->m_entries, previous->m_entries + CHUNK_SIZE);
            last = previous;
        }

        m_chunks = last;
        m_current = m_chunks->m_entries;
        m_last = m_current + CHUNK_SIZE;
    }

    void visit(DOMDataStore* store, typename Traits::Visitor* visitor)
    {
        if (!m_chunks)
            return;

        visitEntries(store, m_chunks->m_entries, m_current, visitor);
        for (Chunk* chunk = m_chunks->m_previous; chunk; chunk = chunk->m_previous)
            visitEntries(store, chunk->m_entries, chunk->m_entries + CHUNK_SIZE, visitor);
    }

  private:
    struct Chunk {
        explicit Chunk(Chunk* previous) : m_previous(previous) { }
        Chunk* const m_previous;
        T m_entries[CHUNK_SIZE];
    };

    static void clearEntries(T* first, T* last)
    {
        for (T* entry = first; entry < last; entry++)
            Traits::clear(entry);
    }

    static void visitEntries(DOMDataStore* store, T* first, T* last, typename Traits::Visitor* visitor)
    {
        for (T* entry = first; entry < last; entry++)
            Traits::visit(store, entry, visitor);
    }

    Chunk* m_chunks;
    T* m_current;
    T* m_last;
};


class IntrusiveDOMWrapperMap : public AbstractWeakReferenceMap<Node, v8::Object> {
public:
    IntrusiveDOMWrapperMap(v8::WeakReferenceCallback callback)
        : AbstractWeakReferenceMap<Node, v8::Object>(callback) { }

    virtual v8::Persistent<v8::Object> get(Node* obj)
    {
        v8::Persistent<v8::Object>* wrapper = obj->wrapper();
        return wrapper ? *wrapper : v8::Persistent<v8::Object>();
    }

    virtual void set(Node* obj, v8::Persistent<v8::Object> wrapper)
    {
        ASSERT(obj);
        ASSERT(!obj->wrapper());
        v8::Persistent<v8::Object>* entry = m_table.add(wrapper);
        obj->setWrapper(entry);
        wrapper.MakeWeak(obj, weakReferenceCallback());
    }

    virtual bool contains(Node* obj)
    {
        return obj->wrapper();
    }

    virtual void visit(DOMDataStore* store, Visitor* visitor)
    {
        m_table.visit(store, visitor);
    }

    virtual bool removeIfPresent(Node* obj, v8::Persistent<v8::Object> value)
    {
        ASSERT(obj);
        v8::Persistent<v8::Object>* entry = obj->wrapper();
        if (!entry)
            return false;
        if (*entry != value)
            return false;
        obj->clearWrapper();
        m_table.remove(entry);
        value.Dispose();
        return true;
    }


    virtual void clear()
    {
        m_table.clear();
    }

private:
    static int const numberOfEntries = (1 << 10) - 1;

    struct ChunkedTableTraits {
        typedef IntrusiveDOMWrapperMap::Visitor Visitor;

        static void move(v8::Persistent<v8::Object>* target, v8::Persistent<v8::Object>* source)
        {
            *target = *source;
            Node* node = V8Node::toNative(*target);
            ASSERT(node);
            node->setWrapper(target);
        }

        static void clear(v8::Persistent<v8::Object>* entry)
        {
            Node* node = V8Node::toNative(*entry);
            ASSERT(node->wrapper() == entry);

            node->clearWrapper();
            entry->Dispose();
        }

        static void visit(DOMDataStore* store, v8::Persistent<v8::Object>* entry, Visitor* visitor)
        {
            Node* node = V8Node::toNative(*entry);
            ASSERT(node->wrapper() == entry);

            visitor->visitDOMWrapper(store, node, *entry);
        }
    };

    typedef ChunkedTable<v8::Persistent<v8::Object>, numberOfEntries, ChunkedTableTraits> Table;
    Table m_table;
};

} // namespace WebCore

#endif
