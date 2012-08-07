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

#ifndef V8DOMMap_h
#define V8DOMMap_h

#include "MemoryInstrumentation.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <v8.h>

namespace WebCore {
    class DOMDataStore;
    class Node;
    class MemoryObjectInfo;

    template <class KeyType, class ValueType> class AbstractWeakReferenceMap {
    public:
        AbstractWeakReferenceMap(v8::WeakReferenceCallback callback) : m_weakReferenceCallback(callback) { }
        virtual ~AbstractWeakReferenceMap() { }

        class Visitor {
        public:
            virtual void startMap() { }
            virtual void endMap() { }
            virtual void visitDOMWrapper(DOMDataStore* store, KeyType* key, v8::Persistent<ValueType> object) = 0;
        protected:
            virtual ~Visitor() { }
        };

        virtual v8::Persistent<ValueType> get(KeyType* obj) = 0;
        virtual void set(KeyType* obj, v8::Persistent<ValueType> wrapper) = 0;
        virtual bool contains(KeyType* obj) = 0;
        virtual void visit(DOMDataStore* store, Visitor* visitor) = 0;
        virtual bool removeIfPresent(KeyType*, v8::Persistent<ValueType>) = 0;
        virtual void clear() = 0;

        v8::WeakReferenceCallback weakReferenceCallback() { return m_weakReferenceCallback; }

        virtual void reportMemoryUsage(MemoryObjectInfo*) const = 0;

    private:
        v8::WeakReferenceCallback m_weakReferenceCallback;
    };

    typedef AbstractWeakReferenceMap<Node, v8::Object> DOMNodeMapping;

    // A table of wrappers with weak pointers.
    // This table allows us to avoid track wrapped objects for debugging
    // and for ensuring that we don't double wrap the same object.
    template<class KeyType, class ValueType> class WeakReferenceMap : public AbstractWeakReferenceMap<KeyType, ValueType> {
    public:
        typedef AbstractWeakReferenceMap<KeyType, ValueType> Parent;
        WeakReferenceMap(v8::WeakReferenceCallback callback) : Parent(callback) { }
        virtual ~WeakReferenceMap() { }

        // Get the JS wrapper object of an object.
        virtual v8::Persistent<ValueType> get(KeyType* obj)
        {
            ValueType* wrapper = m_map.get(obj);
            return wrapper ? v8::Persistent<ValueType>(wrapper) : v8::Persistent<ValueType>();
        }

        virtual void set(KeyType* obj, v8::Persistent<ValueType> wrapper)
        {
            ASSERT(!m_map.contains(obj));
            wrapper.MakeWeak(obj, Parent::weakReferenceCallback());
            m_map.set(obj, *wrapper);
        }

        virtual void forget(KeyType* obj)
        {
            ASSERT(obj);
            ValueType* wrapper = m_map.take(obj);
            if (!wrapper)
                return;

            v8::Persistent<ValueType> handle(wrapper);
            handle.Dispose();
            handle.Clear();
        }

        bool removeIfPresent(KeyType* key, v8::Persistent<ValueType> value)
        {
            typename HashMap<KeyType*, ValueType*>::iterator it = m_map.find(key);
            if (it == m_map.end() || it->second != *value)
                return false;

            m_map.remove(it);
            value.Dispose();
            return true;
        }

        void clear()
        {
            m_map.clear();
        }

        bool contains(KeyType* obj) { return m_map.contains(obj); }

        virtual void visit(DOMDataStore* store, typename Parent::Visitor* visitor)
        {
            visitor->startMap();
            typename HashMap<KeyType*, ValueType*>::iterator it = m_map.begin();
            for (; it != m_map.end(); ++it)
                visitor->visitDOMWrapper(store, it->first, v8::Persistent<ValueType>(it->second));
            visitor->endMap();
        }

        virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const OVERRIDE
        {
            MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::Binding);
            info.addHashMap(m_map);
        }

    protected:
        HashMap<KeyType*, ValueType*> m_map;
    };

    template <class KeyType> class DOMWrapperMap : public WeakReferenceMap<KeyType, v8::Object> {
    public:
        DOMWrapperMap(v8::WeakReferenceCallback callback) : WeakReferenceMap<KeyType, v8::Object>(callback) { }
    };

    // A utility class to manage the lifetime of set of DOM wrappers.
    class DOMDataStoreHandle {
    public:
        DOMDataStoreHandle();
        ~DOMDataStoreHandle();

        DOMDataStore* getStore() const { return m_store.get(); }

    private:
        OwnPtr<DOMDataStore> m_store;
    };

    // A map from DOM node to its JS wrapper.
    DOMNodeMapping& getDOMNodeMap(v8::Isolate* = 0);
    DOMNodeMapping& getActiveDOMNodeMap(v8::Isolate* = 0);
    void visitDOMNodes(DOMWrapperMap<Node>::Visitor*);
    void visitActiveDOMNodes(DOMWrapperMap<Node>::Visitor*);

    // A map from a DOM object (non-node) to its JS wrapper. This map does not contain the DOM objects which can have pending activity (active dom objects).
    DOMWrapperMap<void>& getDOMObjectMap(v8::Isolate* = 0);
    void visitDOMObjects(DOMWrapperMap<void>::Visitor*);

    // A map from a DOM object to its JS wrapper for DOM objects which can have pending activity.
    DOMWrapperMap<void>& getActiveDOMObjectMap(v8::Isolate* = 0);
    void visitActiveDOMObjects(DOMWrapperMap<void>::Visitor*);

    // This should be called to remove all DOM objects associated with the current thread when it is tearing down.
    void removeAllDOMObjects();

} // namespace WebCore

#endif // V8DOMMap_h
