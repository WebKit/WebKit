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

#include "config.h"
#include "V8EventListenerList.h"

#include "V8CustomEventListener.h"

namespace WebCore {

V8EventListenerListIterator::V8EventListenerListIterator(V8EventListenerList* list)
    : m_list(list)
    , m_vectorIndex(0)
    , m_iter(list->m_table.begin())
{
}

V8EventListenerListIterator::V8EventListenerListIterator(V8EventListenerList* list, bool shouldSeekToEnd)
    : m_list(list)
    , m_vectorIndex(0)
    , m_iter(list->m_table.begin())
{
    if (shouldSeekToEnd)
        seekToEnd();
}

V8EventListenerListIterator::~V8EventListenerListIterator() { }

void V8EventListenerListIterator::operator++()
{
    if (m_iter != m_list->m_table.end()) {
        Vector<V8EventListener*>* vector = m_iter->second;
        if (m_vectorIndex + 1 < vector->size()) {
            m_vectorIndex++;
            return;
        }
        m_vectorIndex = 0;
        ++m_iter;
    }
}

bool V8EventListenerListIterator::operator==(const V8EventListenerListIterator& other)
{
    return other.m_iter == m_iter && other.m_vectorIndex == m_vectorIndex && other.m_list == m_list;
}

bool V8EventListenerListIterator::operator!=(const V8EventListenerListIterator& other)
{
    return !operator==(other);
}

V8EventListener* V8EventListenerListIterator::operator*()
{
    if (m_iter != m_list->m_table.end()) {
        Vector<V8EventListener*>* vector = m_iter->second;
        if (m_vectorIndex < vector->size())
            return vector->at(m_vectorIndex);
    }
    return 0;
}

void V8EventListenerListIterator::seekToEnd()
{
    m_iter = m_list->m_table.end();
    m_vectorIndex = 0;
}


V8EventListenerList::V8EventListenerList()
{
}

V8EventListenerList::~V8EventListenerList()
{
}

V8EventListenerListIterator V8EventListenerList::begin()
{
    return iterator(this);
}

V8EventListenerListIterator V8EventListenerList::end()
{
    return iterator(this, true);
}


static int getKey(v8::Local<v8::Object> object)
{
    // 0 is a sentinel value for the HashMap key, so we map it to 1.
    int hash = object->GetIdentityHash();
    if (!hash)
        return 1;
    return hash;
}

void V8EventListenerList::add(V8EventListener* listener)
{
    ASSERT(v8::Context::InContext());
    v8::HandleScope handleScope;

    v8::Local<v8::Object> object = listener->getListenerObject();
    int key = getKey(object);
    Vector<V8EventListener*>* vector = m_table.get(key);
    if (!vector) {
        vector = new Vector<V8EventListener*>();
        m_table.set(key, vector);
    }
    vector->append(listener);
    m_reverseTable.set(listener, key);
}

void V8EventListenerList::remove(V8EventListener* listener)
{
    if (m_reverseTable.contains(listener)) {
        int key = m_reverseTable.get(listener);
        Vector<V8EventListener*>* vector = m_table.get(key);
        if (!vector)
            return;
        for (size_t j = 0; j < vector->size(); j++) {
            if (vector->at(j) == listener) {
                vector->remove(j);
                if (!vector->size()) {
                    m_table.remove(key);
                    delete vector;
                    vector = 0;
                }
                m_reverseTable.remove(listener);
                return;
            }
        }
    }
}

void V8EventListenerList::clear()
{
    m_table.clear();
    m_reverseTable.clear();
}

V8EventListener* V8EventListenerList::find(v8::Local<v8::Object> object, bool isAttribute)
{
    ASSERT(v8::Context::InContext());
    int key = getKey(object);

    Vector<V8EventListener*>* vector = m_table.get(key);
    if (!vector)
        return 0;

    for (size_t i = 0; i < vector->size(); i++) {
        V8EventListener* element = vector->at(i);
        if (isAttribute == element->isAttribute() && object == element->getListenerObject())
            return element;
    }
    return 0;
}

} // namespace WebCore
