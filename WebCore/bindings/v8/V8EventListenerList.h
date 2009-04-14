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

namespace WebCore {
    class V8EventListener;

    // This is a container for V8EventListener objects that also does some caching to speed up finding entries by v8::Object.
    class V8EventListenerList {
    public:
        static const size_t maxKeyNameLength = 254;

        // The name should be distinct from any other V8EventListenerList within the same process, and <= maxKeyNameLength characters.
        explicit V8EventListenerList(const char* name);
        ~V8EventListenerList();

        typedef Vector<V8EventListener*>::iterator iterator;
        V8EventListenerList::iterator begin();
        iterator end();

        // In addition to adding the listener to this list, this also caches the V8EventListener as a hidden property on its wrapped
        // v8 listener object, so we can quickly look it up later.
        void add(V8EventListener*);
        void remove(V8EventListener*);
        V8EventListener* find(v8::Local<v8::Object>, bool isInline);
        void clear();
        size_t size() { return m_list.size(); }

    private:
        v8::Handle<v8::String> getKey(bool isInline);
        v8::Persistent<v8::String> m_inlineKey;
        v8::Persistent<v8::String> m_nonInlineKey;
        Vector<V8EventListener*> m_list;
    };

} // namespace WebCore

#endif // V8EventListenerList_h
