// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef KXMLCORE_HASH_MAP_PTR_SPEC_H
#define KXMLCORE_HASH_MAP_PTR_SPEC_H

#include "HashMap.h"

// This file includes specializations of HashMap for pointer types using raw pointer hashing
// try to share implementation by making them use the void * version of the code
// and casting at interfaces

namespace KXMLCore {

    template<typename P, typename Mapped>
    class PointerHashIteratorAdapter {
    private:
        typedef HashMap<P *, Mapped, PointerHash<P *>, HashTraits<P *>, HashTraits<Mapped> > MapType;
        typedef typename MapType::ValueType ValueType;
        typedef typename MapType::ImplValueType ImplValueType;
        typedef typename HashMap<P *, Mapped, PointerHash<P *>, HashTraits<P *>, HashTraits<Mapped> >::ImplIterator ImplIterator;
        typedef PointerHashIteratorAdapter<P, Mapped> iterator;
        typedef ValueType& ReferenceType;
        typedef ValueType *PointerType;
        
        friend class HashMap<P *, Mapped, PointerHash<P *>, HashTraits<P *>, HashTraits<Mapped> >;
        
    public:
        PointerHashIteratorAdapter() {}
        PointerHashIteratorAdapter(const ImplIterator &impl) : m_impl(impl) {}
        
        // default copy, assignment and destructor are ok
        
        ReferenceType operator*() const { return *reinterpret_cast<PointerType>(m_impl.operator->()); }
        PointerType operator->() const { return &(operator*()); }
        
        iterator& operator++()
        {
            ++m_impl;
            return *this;
        }
        
        // postfix ++ intentionally omitted
        
        // Comparison.
        bool operator==(const iterator& other) const
        { 
            return m_impl == other.m_impl; 
        }
        
        bool operator!=(const iterator& other) const 
        { 
            return m_impl != other.m_impl; 
        }
        
    private:
        ImplIterator m_impl;
    };

    template<typename P, typename Mapped>
    class PointerHashConstIteratorAdapter {
    private:
        typedef HashMap<P *, Mapped, PointerHash<P *>, HashTraits<P *>, HashTraits<Mapped> > MapType;
        typedef typename MapType::ValueType ValueType;
        typedef typename MapType::ImplValueType ImplValueType;
        typedef typename MapType::ImplIterator ImplIterator;
        typedef typename MapType::ImplConstIterator ImplConstIterator;
        typedef PointerHashIteratorAdapter<P, Mapped> iterator;
        typedef PointerHashConstIteratorAdapter<P, Mapped> const_iterator;
        typedef const ValueType& ReferenceType;
        typedef const ValueType *PointerType;
        
        friend class HashMap<P *, Mapped, PointerHash<P *>, HashTraits<P *>, HashTraits<Mapped> >;
        
    public:
        PointerHashConstIteratorAdapter() {}
        PointerHashConstIteratorAdapter(const ImplConstIterator &impl) : m_impl(impl) {}
        PointerHashConstIteratorAdapter(const iterator &other) : m_impl(other.m_impl) { }
        
        // default copy, assignment and destructor are ok
        
        ReferenceType operator*() const { return *reinterpret_cast<PointerType>(m_impl.operator->()); }
        PointerType operator->() const { return &(operator*()); }
        
        const_iterator& operator++()
        {
            ++m_impl;
            return *this;
        }
        
        // postfix ++ intentionally omitted
        
        // Comparison.
        bool operator==(const const_iterator& other) const
        { 
            return m_impl == other.m_impl; 
        }
        
        bool operator!=(const const_iterator& other) const 
        { 
            return m_impl != other.m_impl; 
        }
        
    private:
        ImplConstIterator m_impl;
    };

    template<typename P, typename Mapped>
    class HashMap<P *, Mapped, PointerHash<P *>, HashTraits<P *>, HashTraits<Mapped> > {
    public:
        typedef P *KeyType;
        typedef Mapped MappedType;
        typedef std::pair<KeyType, Mapped> ValueType;
    private:
        // important not to use pointerHash/pointerEqual here or instantiation would recurse
        typedef HashMap<void *, MappedType, DefaultHash<void *>, HashTraits<void *>, HashTraits<Mapped> > ImplType;
    public:
        typedef typename ImplType::ValueType ImplValueType;
        typedef typename ImplType::iterator ImplIterator;
        typedef typename ImplType::const_iterator ImplConstIterator;
        typedef PointerHashIteratorAdapter<P, Mapped> iterator;
        typedef PointerHashConstIteratorAdapter<P, Mapped> const_iterator;
        
        HashMap() {}
        
        int size() const { return m_impl.size(); }
        int capacity() const { return m_impl.capacity(); }
        bool isEmpty() const { return m_impl.isEmpty(); }
        
        iterator begin() { return m_impl.begin(); }
        iterator end() { return m_impl.end(); }
        const_iterator begin() const { return m_impl.begin(); }
        const_iterator end() const { return m_impl.end(); }
        
        iterator find(const KeyType& key) { return m_impl.find((void *)(key)); }
        const_iterator find(const KeyType& key) const { return m_impl.find((void *)(key)); }
        bool contains(const KeyType& key) const { return m_impl.contains((void *)(key)); }
        MappedType get(const KeyType &key) const { return m_impl.get((void *)(key)); }
        
        std::pair<iterator, bool> set(const KeyType &key, const MappedType &mapped) 
        { return m_impl.set((void *)(key), mapped); }

        std::pair<iterator, bool> add(const KeyType &key, const MappedType &mapped) 
        { return m_impl.add((void *)(key), mapped); }
        
        void remove(const KeyType& key) { m_impl.remove((void *)(key)); }
        void remove(iterator it) { m_impl.remove(it.m_impl); }
        void clear() { m_impl.clear(); }
        
    private:
        ImplType m_impl;
    };
    
    template<typename P, typename Q>
    class HashMap<P *, Q *, PointerHash<P *>, HashTraits<P *>, HashTraits<Q *> > {
    private:
        // important not to use PointerHash here or instantiation would recurse
        typedef HashMap<void *, void *, DefaultHash<void *>, HashTraits<void *>, HashTraits<void *> > ImplMapType;
    public:
        typedef P *KeyType;
        typedef Q *MappedType;
        typedef std::pair<KeyType, MappedType> ValueType;
        typedef typename std::pair<void *, void *> ImplValueType;
        typedef HashTableIterator<void *, ImplValueType, extractFirst<ImplValueType>, DefaultHash<void *>, PairHashTraits<HashTraits<void *>, HashTraits<void *> >, HashTraits<void *> > ImplIterator;
        typedef HashTableConstIterator<void *, ImplValueType, extractFirst<ImplValueType>, DefaultHash<void *>, PairHashTraits<HashTraits<void *>, HashTraits<void *> >, HashTraits<void *> > ImplConstIterator;
        
        typedef PointerHashIteratorAdapter<P, Q *> iterator;
        typedef PointerHashConstIteratorAdapter<P, Q *> const_iterator;
        
        HashMap() {}
        
        int size() const { return m_impl.size(); }
        int capacity() const { return m_impl.capacity(); }
        bool isEmpty() const { return m_impl.isEmpty(); }
        
        iterator begin() { return m_impl.begin(); }
        iterator end() { return m_impl.end(); }
        const_iterator begin() const { return m_impl.begin(); }
        const_iterator end() const { return m_impl.end(); }
        
        iterator find(const KeyType& key) { return m_impl.find((void *)(key)); }
        const_iterator find(const KeyType& key) const { return m_impl.find((void *)(key)); }
        bool contains(const KeyType& key) const { return m_impl.contains((void *)(key)); }
        MappedType get(const KeyType &key) const { return reinterpret_cast<MappedType>(m_impl.get((void *)(key))); }
        
        std::pair<iterator, bool> set(const KeyType &key, const MappedType &mapped) 
        { return m_impl.set((void *)(key), (void *)(mapped)); }

        std::pair<iterator, bool> add(const KeyType &key, const MappedType &mapped) 
        { return m_impl.add((void *)(key), (void *)(mapped)); }
        
        void remove(const KeyType& key) { m_impl.remove((void *)(key)); }
        void remove(iterator it) { m_impl.remove(it.m_impl); }
        void clear() { m_impl.clear(); }
        
    private:
        ImplMapType m_impl;
    };

} // namespace KXMLCore

#endif // KXMLCORE_HASH_MAP_PTR_SPEC_H
