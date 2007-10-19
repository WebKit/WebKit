/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#ifndef WTF_Deque_h
#define WTF_Deque_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

namespace WTF {

    template<typename T>
    class DequeNode {
    public:
        DequeNode(const T& item) : m_value(item), m_next(0) { }

        T m_value;
        DequeNode* m_next;
    };

    template<typename T>
    class Deque : Noncopyable {
    public:
        Deque()
            : m_size(0)
            , m_first(0)
            , m_last(0)
        {
        }

        ~Deque()
        {
            clear();
        }

        size_t size() const { return m_size; }
        bool isEmpty() const { return !size(); }

        void append(const T& item)
        {
            DequeNode<T>* newNode = new DequeNode<T>(item);
            if (m_last)
                m_last->m_next = newNode;
            m_last = newNode;
            if (!m_first)
                m_first = newNode;
            ++m_size;
        }

        void prepend(const T& item)
        {
            DequeNode<T>* newNode = new DequeNode<T>(item);
            newNode->m_next = m_first;
            m_first = newNode;
            if (!m_last)
                m_last = newNode;
            ++m_size;
        }

        T& first() { ASSERT(m_first); return m_first->m_value; }
        const T& first() const { ASSERT(m_first); return m_first->m_value; }
        T& last() { ASSERT(m_last); return m_last->m_value; }
        const T& last() const { ASSERT(m_last); return m_last->m_value; }

        void removeFirst() 
        {
            ASSERT(m_first);
            if (DequeNode<T>* n = m_first) {
                m_first = m_first->m_next;
                if (n == m_last)
                    m_last = 0;

                m_size--;
                delete n;
            }
        }

        void clear() 
        {
            DequeNode<T>* n = m_first;
            m_first = 0;
            m_last = 0;
            m_size = 0;
            while (n) {
                DequeNode<T>* next = n->m_next;
                delete n;
                n = next;
            }
        }

    private:
        size_t m_size;
        DequeNode<T>* m_first;
        DequeNode<T>* m_last;

    };

} // namespace WTF

using WTF::Deque;

#endif // WTF_Deque_h
