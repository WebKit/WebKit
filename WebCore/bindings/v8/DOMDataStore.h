/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef DOMDataStore_h
#define DOMDataStore_h

#include "V8DOMMap.h"
#include "V8Node.h"

#include <v8.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/Vector.h>

namespace WebCore {

    class DOMData;
    class DOMDataStore;

    typedef WTF::Vector<DOMDataStore*> DOMDataList;

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

        void visit(typename Traits::Visitor* visitor)
        {
            if (!m_chunks)
                return;

            visitEntries(m_chunks->m_entries, m_current, visitor);
            for (Chunk* chunk = m_chunks->m_previous; chunk; chunk = chunk->m_previous)
                visitEntries(chunk->m_entries, chunk->m_entries + CHUNK_SIZE, visitor);
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

        static void visitEntries(T* first, T* last, typename Traits::Visitor* visitor)
        {
            for (T* entry = first; entry < last; entry++)
                Traits::visit(entry, visitor);
        }

        Chunk* m_chunks;
        T* m_current;
        T* m_last;
    };

    // DOMDataStore
    //
    // DOMDataStore is the backing store that holds the maps between DOM objects
    // and JavaScript objects.  In general, each thread can have multiple backing
    // stores, one per isolated world.
    //
    // This class doesn't manage the lifetime of the store.  The data store
    // lifetime is managed by subclasses.
    //
    class DOMDataStore : public Noncopyable {
    public:
        enum DOMWrapperMapType {
            DOMNodeMap,
            DOMObjectMap,
            ActiveDOMObjectMap,
#if ENABLE(SVG)
            DOMSVGElementInstanceMap
#endif
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

            virtual void visit(Visitor* visitor)
            {
                m_table.visit(visitor);
            }

            virtual bool removeIfPresent(Node* key, v8::Persistent<v8::Data> value);

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

                static void visit(v8::Persistent<v8::Object>* entry, Visitor* visitor)
                {
                    Node* node = V8Node::toNative(*entry);
                    ASSERT(node->wrapper() == entry);

                    visitor->visitDOMWrapper(node, *entry);
                }
            };

            typedef ChunkedTable<v8::Persistent<v8::Object>, numberOfEntries, ChunkedTableTraits> Table;
            Table m_table;
        };

        DOMDataStore(DOMData*);
        virtual ~DOMDataStore();

        // A list of all DOMDataStore objects in the current V8 instance (thread). Normally, each World has a DOMDataStore.
        static DOMDataList& allStores();
        // Mutex to protect against concurrent access of DOMDataList.
        static WTF::Mutex& allStoresMutex();

        DOMData* domData() const { return m_domData; }

        void* getDOMWrapperMap(DOMWrapperMapType);

        DOMNodeMapping& domNodeMap() { return *m_domNodeMap; }
        DOMWrapperMap<void>& domObjectMap() { return *m_domObjectMap; }
        DOMWrapperMap<void>& activeDomObjectMap() { return *m_activeDomObjectMap; }
#if ENABLE(SVG)
        DOMWrapperMap<SVGElementInstance>& domSvgElementInstanceMap() { return *m_domSvgElementInstanceMap; }
#endif

        // Need by V8GCController.
        static void weakActiveDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);

    protected:
        static void weakNodeCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
        static void weakDOMObjectCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
#if ENABLE(SVG)
        static void weakSVGElementInstanceCallback(v8::Persistent<v8::Value> v8Object, void* domObject);
#endif
        
        DOMNodeMapping* m_domNodeMap;
        DOMWrapperMap<void>* m_domObjectMap;
        DOMWrapperMap<void>* m_activeDomObjectMap;
#if ENABLE(SVG)
        DOMWrapperMap<SVGElementInstance>* m_domSvgElementInstanceMap;
#endif

    private:
        // A back-pointer to the DOMData to which we belong.
        DOMData* m_domData;
    };

} // namespace WebCore

#endif // DOMDataStore_h
