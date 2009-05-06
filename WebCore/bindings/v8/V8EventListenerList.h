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

#ifndef V8EventListenerList_h
#define V8EventListenerList_h

#include <v8.h>
#include <wtf/Vector.h>
#include <wtf/HashMap.h>

namespace WebCore {
    class V8EventListener;
    class V8EventListenerListIterator;

    // This is a container for V8EventListener objects that uses the identity hash of the v8::Object to
    // speed up lookups
    class V8EventListenerList {
    public:
        // Because v8::Object identity hashes are not guaranteed to be unique, we unfortunately can't just map
        // an int to V8EventListener. Instead we define a HashMap of int to Vector of V8EventListener
        // called a ListenerMultiMap.
        typedef Vector<V8EventListener*>* Values;
        struct ValuesTraits : HashTraits<Values> {
            static const bool needsDestruction = true;
        };
        typedef HashMap<int, Values, DefaultHash<int>::Hash, HashTraits<int>, ValuesTraits> ListenerMultiMap;

        V8EventListenerList();
        ~V8EventListenerList();

        friend class V8EventListenerListIterator;
        typedef V8EventListenerListIterator iterator;        

        iterator begin();
        iterator end();

        void add(V8EventListener*);
        void remove(V8EventListener*);
        V8EventListener* find(v8::Local<v8::Object>, bool isAttribute);
        void clear();
        size_t size() { return m_table.size(); }

    private:
        ListenerMultiMap m_table;

        // we also keep a reverse mapping of V8EventListener to v8::Object identity hash,
        // in order to speed up removal by V8EventListener
        HashMap<V8EventListener*, int> m_reverseTable;
    };

    class V8EventListenerListIterator {
    public:
        ~V8EventListenerListIterator();
        void operator++();
        bool operator==(const V8EventListenerListIterator&);
        bool operator!=(const V8EventListenerListIterator&);
        V8EventListener* operator*();
    private:
        friend class V8EventListenerList;
        explicit V8EventListenerListIterator(V8EventListenerList*);
        V8EventListenerListIterator(V8EventListenerList*, bool shouldSeekToEnd);
        void seekToEnd();

        V8EventListenerList* m_list;
        V8EventListenerList::ListenerMultiMap::iterator m_iter;
        size_t m_vectorIndex;
    };

} // namespace WebCore

#endif // V8EventListenerList_h
