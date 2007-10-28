// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_ListHashSet_h
#define WTF_ListHashSet_h

#include "Assertions.h"
#include "HashSet.h"
#include "OwnPtr.h"

namespace WTF {

    // ListHashSet: Just like HashSet, this class provides a Set
    // interface - a collection of unique objects with O(1) insertion,
    // removal and test for containership. However, it also has an
    // order - iterating it will always give back values in the order
    // in which they are added.

    // In theory it would be possible to add prepend, insertAfter, insertBefore,
    // and an append that moves the element to the end even if already present,
    // but unclear yet if these are needed.

    template<typename Value, typename HashFunctions> class ListHashSet;

    template<typename T> struct IdentityExtractor;

    template<typename Value, typename HashFunctions>
    void deleteAllValues(const ListHashSet<Value, HashFunctions>&);

    template<typename ValueArg, typename HashArg> class ListHashSetIterator;
    template<typename ValueArg, typename HashArg> class ListHashSetConstIterator;

    template<typename ValueArg> struct ListHashSetNode;
    template<typename ValueArg> struct ListHashSetNodeAllocator;
    template<typename ValueArg, typename HashArg> struct ListHashSetNodeHashFunctions;

    template<typename ValueArg, typename HashArg = typename DefaultHash<ValueArg>::Hash> class ListHashSet {
    private:
        typedef ListHashSetNode<ValueArg> Node;
        typedef ListHashSetNodeAllocator<ValueArg> NodeAllocator;

        typedef HashTraits<Node*> NodeTraits;
        typedef ListHashSetNodeHashFunctions<ValueArg, HashArg> NodeHash;

        typedef HashTable<Node*, Node*, IdentityExtractor<Node*>, NodeHash, NodeTraits, NodeTraits> ImplType;

        typedef HashArg HashFunctions;

    public:
        typedef ValueArg ValueType;
        typedef ListHashSetIterator<ValueType, HashArg> iterator;
        typedef ListHashSetConstIterator<ValueType, HashArg> const_iterator;

        friend class ListHashSetConstIterator<ValueType, HashArg>;

        ListHashSet();
        ListHashSet(const ListHashSet&);
        ListHashSet& operator=(const ListHashSet&);
        ~ListHashSet();

        void swap(ListHashSet&);

        int size() const;
        int capacity() const;
        bool isEmpty() const;

        iterator begin();
        iterator end();
        const_iterator begin() const;
        const_iterator end() const;

        iterator find(const ValueType&);
        const_iterator find(const ValueType&) const;
        bool contains(const ValueType&) const;

        // the return value is a pair of an interator to the new value's location, 
        // and a bool that is true if an new entry was added
        pair<iterator, bool> add(const ValueType&);

        void remove(const ValueType&);
        void remove(iterator);
        void clear();

    private:
        void unlinkAndDelete(Node*);
        void appendNode(Node*);
        void deleteAllNodes();
        iterator makeIterator(Node*);
        const_iterator makeConstIterator(Node*) const;

        friend void deleteAllValues<>(const ListHashSet&);

        ImplType m_impl;
        Node* m_head;
        Node* m_tail;
        OwnPtr<NodeAllocator> m_allocator;
    };

    template<typename ValueArg> struct ListHashSetNodeAllocator {
        typedef ListHashSetNode<ValueArg> Node;
        typedef ListHashSetNodeAllocator<ValueArg> NodeAllocator;

        ListHashSetNodeAllocator() 
            : m_freeList(pool())
            , m_isDoneWithInitialFreeList(false)
        { 
            memset(m_pool.pool, 0, sizeof(m_pool.pool));
        }

        Node* allocate()
        { 
            Node* result = m_freeList;

            if (!result)
                return static_cast<Node*>(fastMalloc(sizeof(Node)));

            ASSERT(!result->m_isAllocated);

            Node* next = result->m_next;
            ASSERT(!next || !next->m_isAllocated);
            if (!next && !m_isDoneWithInitialFreeList) {
                next = result + 1;
                if (next == pastPool()) {
                    m_isDoneWithInitialFreeList = true;
                    next = 0;
                } else {
                    ASSERT(inPool(next));
                    ASSERT(!next->m_isAllocated);
                }
            }
            m_freeList = next;

            return result;
        }

        void deallocate(Node* node) 
        {
            if (inPool(node)) {
#ifndef NDEBUG
                node->m_isAllocated = false;
#endif
                node->m_next = m_freeList;
                m_freeList = node;
                return;
            }

            fastFree(node);
        }

    private:
        Node* pool() { return reinterpret_cast<Node*>(m_pool.pool); }
        Node* pastPool() { return pool() + m_poolSize; }

        bool inPool(Node* node)
        {
            return node >= pool() && node < pastPool();
        }

        Node* m_freeList;
        bool m_isDoneWithInitialFreeList;
        static const size_t m_poolSize = 256;
        union {
            char pool[sizeof(Node) * m_poolSize];
            double forAlignment;
        } m_pool;
    };

    template<typename ValueArg> struct ListHashSetNode {
        typedef ListHashSetNodeAllocator<ValueArg> NodeAllocator;

        ListHashSetNode(ValueArg value)
            : m_value(value)
            , m_prev(0)
            , m_next(0)
#ifndef NDEBUG
            , m_isAllocated(true)
#endif
        {
        }

        void* operator new(size_t, NodeAllocator* allocator)
        {
            return allocator->allocate();
        }
        void destroy(NodeAllocator* allocator)
        {
            this->~ListHashSetNode();
            allocator->deallocate(this);
        }

        ValueArg m_value;
        ListHashSetNode* m_prev;
        ListHashSetNode* m_next;

#ifndef NDEBUG
        bool m_isAllocated;
#endif
    };

    template<typename ValueArg, typename HashArg> struct ListHashSetNodeHashFunctions {
        typedef ListHashSetNode<ValueArg> Node;
        
        static unsigned hash(Node* const& key) { return HashArg::hash(key->m_value); }
        static bool equal(Node* const& a, Node* const& b) { return HashArg::equal(a->m_value, b->m_value); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    template<typename ValueArg, typename HashArg> class ListHashSetIterator {
    private:
        typedef ListHashSet<ValueArg, HashArg> ListHashSetType;
        typedef ListHashSetIterator<ValueArg, HashArg> iterator;
        typedef ListHashSetConstIterator<ValueArg, HashArg> const_iterator;
        typedef ListHashSetNode<ValueArg> Node;
        typedef ValueArg ValueType;
        typedef ValueType& ReferenceType;
        typedef ValueType* PointerType;

        friend class ListHashSet<ValueArg, HashArg>;

        ListHashSetIterator(const ListHashSetType* set, Node* position) : m_iterator(set, position) { }

    public:
        ListHashSetIterator() { }

        // default copy, assignment and destructor are OK

        PointerType get() const { return const_cast<PointerType>(m_iterator.get()); }
        ReferenceType operator*() const { return *get(); }
        PointerType operator->() const { return get(); }

        iterator& operator++() { ++m_iterator; return *this; }

        // postfix ++ intentionally omitted

        iterator& operator--() { --m_iterator; return *this; }

        // postfix -- intentionally omitted

        // Comparison.
        bool operator==(const iterator& other) const { return m_iterator == other.m_iterator; }
        bool operator!=(const iterator& other) const { return m_iterator != other.m_iterator; }

        operator const_iterator() const { return m_iterator; }

    private:
        Node* node() { return m_iterator.node(); }

        const_iterator m_iterator;
    };

    template<typename ValueArg, typename HashArg> class ListHashSetConstIterator {
    private:
        typedef ListHashSet<ValueArg, HashArg> ListHashSetType;
        typedef ListHashSetIterator<ValueArg, HashArg> iterator;
        typedef ListHashSetConstIterator<ValueArg, HashArg> const_iterator;
        typedef ListHashSetNode<ValueArg> Node;
        typedef ValueArg ValueType;
        typedef const ValueType& ReferenceType;
        typedef const ValueType* PointerType;

        friend class ListHashSet<ValueArg, HashArg>;
        friend class ListHashSetIterator<ValueArg, HashArg>;

        ListHashSetConstIterator(const ListHashSetType* set, Node* position)
            : m_set(set)
            , m_position(position)
        {
        }

    public:
        ListHashSetConstIterator()
        {
        }

        PointerType get() const
        {
            return &m_position->m_value;
        }
        ReferenceType operator*() const { return *get(); }
        PointerType operator->() const { return get(); }

        const_iterator& operator++()
        {
            ASSERT(m_position != 0);
            m_position = m_position->m_next;
            return *this;
        }

        // postfix ++ intentionally omitted

        const_iterator& operator--()
        {
            ASSERT(m_position != m_set->m_head);
            m_position = m_position->m_prev;
            return *this;
        }

        // postfix -- intentionally omitted

        // Comparison.
        bool operator==(const const_iterator& other) const
        {
            return m_position == other.m_position;
        }
        bool operator!=(const const_iterator& other) const
        {
            return m_position != other.m_position;
        }

    private:
        Node* node() { return m_position; }

        const ListHashSetType* m_set;
        Node* m_position;
    };


    template<typename ValueType, typename HashFunctions>
    struct ListHashSetTranslator {
    private:
        typedef ListHashSetNode<ValueType> Node;
        typedef ListHashSetNodeAllocator<ValueType> NodeAllocator;
    public:
        static unsigned hash(const ValueType& key) { return HashFunctions::hash(key); }
        static bool equal(Node* const& a, const ValueType& b) { return HashFunctions::equal(a->m_value, b); }
        static void translate(Node*& location, const ValueType& key, NodeAllocator* allocator)
        {
            location = new (allocator) Node(key);
        }
    };

    template<typename T, typename U>
    inline ListHashSet<T, U>::ListHashSet()
        : m_head(0)
        , m_tail(0)
        , m_allocator(new NodeAllocator)
    {
    }

    template<typename T, typename U>
    inline ListHashSet<T, U>::ListHashSet(const ListHashSet& other)
        : m_head(0)
        , m_tail(0)
        , m_allocator(new NodeAllocator)
    {
        const_iterator end = other.end();
        for (const_iterator it = other.begin(); it != end; ++it)
            add(*it);
    }

    template<typename T, typename U>
    inline ListHashSet<T, U>& ListHashSet<T, U>::operator=(const ListHashSet& other)
    {
        ListHashSet tmp(other);
        swap(tmp);
        return *this;
    }

    template<typename T, typename U>
    inline void ListHashSet<T, U>::swap(ListHashSet& other)
    {
        m_impl.swap(other.m_impl);
        std::swap(m_head, other.m_head);
        std::swap(m_tail, other.m_tail);
        m_allocator.swap(other.m_allocator);
        return *this;
    }

    template<typename T, typename U>
    inline ListHashSet<T, U>::~ListHashSet()
    {
        deleteAllNodes();
    }

    template<typename T, typename U>
    inline int ListHashSet<T, U>::size() const
    {
        return m_impl.size(); 
    }

    template<typename T, typename U>
    inline int ListHashSet<T, U>::capacity() const
    {
        return m_impl.capacity(); 
    }

    template<typename T, typename U>
    inline bool ListHashSet<T, U>::isEmpty() const
    {
        return m_impl.isEmpty(); 
    }

    template<typename T, typename U>
    inline typename ListHashSet<T, U>::iterator ListHashSet<T, U>::begin()
    {
        return makeIterator(m_head); 
    }

    template<typename T, typename U>
    inline typename ListHashSet<T, U>::iterator ListHashSet<T, U>::end()
    {
        return makeIterator(0);
    }

    template<typename T, typename U>
    inline typename ListHashSet<T, U>::const_iterator ListHashSet<T, U>::begin() const
    {
        return makeConstIterator(m_head); 
    }

    template<typename T, typename U>
    inline typename ListHashSet<T, U>::const_iterator ListHashSet<T, U>::end() const
    {
        return makeConstIterator(0); 
    }

    template<typename T, typename U>
    inline typename ListHashSet<T, U>::iterator ListHashSet<T, U>::find(const ValueType& value)
    {
        typedef ListHashSetTranslator<ValueType, HashFunctions> Translator;
        typename ImplType::iterator it = m_impl.template find<ValueType, Translator>(value);
        if (it == m_impl.end())
            return end();
        return makeIterator(*it); 
    }

    template<typename T, typename U>
    inline typename ListHashSet<T, U>::const_iterator ListHashSet<T, U>::find(const ValueType& value) const
    {
        typedef ListHashSetTranslator<ValueType, HashFunctions> Translator;
        typename ImplType::const_iterator it = m_impl.template find<ValueType, Translator>(value);
        if (it == m_impl.end())
            return end();
        return makeConstIterator(*it);
    }

    template<typename T, typename U>
    inline bool ListHashSet<T, U>::contains(const ValueType& value) const
    {
        typedef ListHashSetTranslator<ValueType, HashFunctions> Translator;
        return m_impl.template contains<ValueType, Translator>(value);
    }

    template<typename T, typename U>
    pair<typename ListHashSet<T, U>::iterator, bool> ListHashSet<T, U>::add(const ValueType &value)
    {
        typedef ListHashSetTranslator<ValueType, HashFunctions> Translator;
        pair<typename ImplType::iterator, bool> result = m_impl.template add<ValueType, NodeAllocator*, Translator>(value, m_allocator.get());
        if (result.second)
            appendNode(*result.first);
        return std::make_pair(makeIterator(*result.first), result.second);
    }

    template<typename T, typename U>
    inline void ListHashSet<T, U>::remove(iterator it)
    {
        if (it == end())
            return;
        m_impl.remove(it.node());
        unlinkAndDelete(it.node());
    }

    template<typename T, typename U>
    inline void ListHashSet<T, U>::remove(const ValueType& value)
    {
        remove(find(value));
    }

    template<typename T, typename U>
    inline void ListHashSet<T, U>::clear()
    {
        deleteAllNodes();
        m_impl.clear(); 
        m_head = 0;
        m_tail = 0;
    }

    template<typename T, typename U>
    void ListHashSet<T, U>::unlinkAndDelete(Node* node)
    {
        if (!node->m_prev) {
            ASSERT(node == m_head);
            m_head = node->m_next;
        } else {
            ASSERT(node != m_head);
            node->m_prev->m_next = node->m_next;
        }

        if (!node->m_next) {
            ASSERT(node == m_tail);
            m_tail = node->m_prev;
        } else {
            ASSERT(node != m_tail);
            node->m_next->m_prev = node->m_prev;
        }

        node->destroy(m_allocator.get());
    }

    template<typename T, typename U>
    void ListHashSet<T, U>::appendNode(Node* node)
    {
        node->m_prev = m_tail;
        node->m_next = 0;

        if (m_tail) {
            ASSERT(m_head);
            m_tail->m_next = node;
        } else {
            ASSERT(!m_head);
            m_head = node;
        }

        m_tail = node;
    }

    template<typename T, typename U>
    void ListHashSet<T, U>::deleteAllNodes()
    {
        if (!m_head)
            return;

        for (Node* node = m_head, *next = m_head->m_next; node; node = next, next = node ? node->m_next : 0)
            node->destroy(m_allocator.get());
    }

    template<typename T, typename U>
    inline ListHashSetIterator<T, U> ListHashSet<T, U>::makeIterator(Node* position) 
    {
        return ListHashSetIterator<T, U>(this, position); 
    }

    template<typename T, typename U>
    inline ListHashSetConstIterator<T, U> ListHashSet<T, U>::makeConstIterator(Node* position) const
    { 
        return ListHashSetConstIterator<T, U>(this, position); 
    }

    template<bool, typename ValueType, typename HashTableType>
    void deleteAllValues(HashTableType& collection)
    {
        typedef typename HashTableType::const_iterator iterator;
        iterator end = collection.end();
        for (iterator it = collection.begin(); it != end; ++it)
            delete (*it)->m_value;
    }

    template<typename T, typename U>
    inline void deleteAllValues(const ListHashSet<T, U>& collection)
    {
        deleteAllValues<true, typename ListHashSet<T, U>::ValueType>(collection.m_impl);
    }

} // namespace WTF

using WTF::ListHashSet;

#endif /* WTF_ListHashSet_h */
