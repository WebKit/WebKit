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

namespace WTF {

    // This specialization is a direct copy of HashMap, with overloaded functions
    // to allow for lookup by pointer instead of RefPtr, avoiding ref-count churn.
    
    // FIXME: Is there a better way to do this that doesn't just copy HashMap?
    
    template<typename T, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    class HashMap<RefPtr<T>, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg> {
    private:
        typedef KeyTraitsArg KeyTraits;
        typedef MappedTraitsArg MappedTraits;
        typedef PairBaseHashTraits<KeyTraits, MappedTraits> ValueTraits;

    public:
        typedef typename KeyTraits::TraitType KeyType;
        typedef T* RawKeyType;
        typedef typename MappedTraits::TraitType MappedType;
        typedef typename ValueTraits::TraitType ValueType;

    private:
        typedef HashArg HashFunctions;

        typedef typename HashKeyStorageTraits<HashFunctions, KeyTraits>::Hash StorageHashFunctions;

        typedef typename HashKeyStorageTraits<HashFunctions, KeyTraits>::Traits KeyStorageTraits;
        typedef typename MappedTraits::StorageTraits MappedStorageTraits;
        typedef PairHashTraits<KeyStorageTraits, MappedStorageTraits> ValueStorageTraits;

        typedef typename KeyStorageTraits::TraitType KeyStorageType;
        typedef typename MappedStorageTraits::TraitType MappedStorageType;
        typedef typename ValueStorageTraits::TraitType ValueStorageType;

        typedef HashTable<KeyStorageType, ValueStorageType, PairFirstExtractor<ValueStorageType>,
            StorageHashFunctions, ValueStorageTraits, KeyStorageTraits> HashTableType;

    public:
        typedef HashTableIteratorAdapter<HashTableType, ValueType> iterator;
        typedef HashTableConstIteratorAdapter<HashTableType, ValueType> const_iterator;

        HashMap();
        HashMap(const HashMap&);
        HashMap& operator=(const HashMap&);
        ~HashMap();

        void swap(HashMap&);

        int size() const;
        int capacity() const;
        bool isEmpty() const;

        // iterators iterate over pairs of keys and values
        iterator begin();
        iterator end();
        const_iterator begin() const;
        const_iterator end() const;

        iterator find(const KeyType&);
        iterator find(RawKeyType);
        const_iterator find(const KeyType&) const;
        const_iterator find(RawKeyType) const;
        bool contains(const KeyType&) const;
        bool contains(RawKeyType) const;
        MappedType get(const KeyType&) const;
        MappedType get(RawKeyType) const;

        // replaces value but not key if key is already present
        // return value is a pair of the iterator to the key location, 
        // and a boolean that's true if a new value was actually added
        pair<iterator, bool> set(const KeyType&, const MappedType&); 
        pair<iterator, bool> set(RawKeyType, const MappedType&); 

        // does nothing if key is already present
        // return value is a pair of the iterator to the key location, 
        // and a boolean that's true if a new value was actually added
        pair<iterator, bool> add(const KeyType&, const MappedType&); 
        pair<iterator, bool> add(RawKeyType, const MappedType&); 

        void remove(const KeyType&);
        void remove(RawKeyType);
        void remove(iterator);
        void clear();

        MappedType take(const KeyType&); // efficient combination of get with remove
        MappedType take(RawKeyType); // efficient combination of get with remove

    private:
        pair<iterator, bool> inlineAdd(const KeyType&, const MappedType&);
        pair<iterator, bool> inlineAdd(RawKeyType, const MappedType&);
        void refAll();
        void derefAll();

        HashTableType m_impl;
    };
    
    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::refAll()
    {
        HashTableRefCounter<HashTableType, ValueTraits>::refAll(m_impl);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::derefAll()
    {
        HashTableRefCounter<HashTableType, ValueTraits>::derefAll(m_impl);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline HashMap<RefPtr<T>, U, V, W, X>::HashMap()
    {
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline HashMap<RefPtr<T>, U, V, W, X>::HashMap(const HashMap& other)
        : m_impl(other.m_impl)
    {
        refAll();
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline HashMap<RefPtr<T>, U, V, W, X>& HashMap<RefPtr<T>, U, V, W, X>::operator=(const HashMap& other)
    {
        HashMap tmp(other);
        swap(tmp); 
        return *this;
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::swap(HashMap& other)
    {
        m_impl.swap(other.m_impl); 
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline HashMap<RefPtr<T>, U, V, W, X>::~HashMap()
    {
        derefAll();
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline int HashMap<RefPtr<T>, U, V, W, X>::size() const
    {
        return m_impl.size(); 
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline int HashMap<RefPtr<T>, U, V, W, X>::capacity() const
    { 
        return m_impl.capacity(); 
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline bool HashMap<RefPtr<T>, U, V, W, X>::isEmpty() const
    {
        return m_impl.isEmpty();
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::iterator HashMap<RefPtr<T>, U, V, W, X>::begin()
    {
        return m_impl.begin();
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::iterator HashMap<RefPtr<T>, U, V, W, X>::end()
    {
        return m_impl.end();
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::const_iterator HashMap<RefPtr<T>, U, V, W, X>::begin() const
    {
        return m_impl.begin();
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::const_iterator HashMap<RefPtr<T>, U, V, W, X>::end() const
    {
        return m_impl.end();
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::iterator HashMap<RefPtr<T>, U, V, W, X>::find(const KeyType& key)
    {
        return m_impl.find(*(const KeyStorageType*)&key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::iterator HashMap<RefPtr<T>, U, V, W, X>::find(RawKeyType key)
    {
        return m_impl.find(*(const KeyStorageType*)&key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::const_iterator HashMap<RefPtr<T>, U, V, W, X>::find(const KeyType& key) const
    {
        return m_impl.find(*(const KeyStorageType*)&key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::const_iterator HashMap<RefPtr<T>, U, V, W, X>::find(RawKeyType key) const
    {
        return m_impl.find(*(const KeyStorageType*)&key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline bool HashMap<RefPtr<T>, U, V, W, X>::contains(const KeyType& key) const
    {
        return m_impl.contains(*(const KeyStorageType*)&key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline bool HashMap<RefPtr<T>, U, V, W, X>::contains(RawKeyType key) const
    {
        return m_impl.contains(*(const KeyStorageType*)&key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline pair<typename HashMap<RefPtr<T>, U, V, W, X>::iterator, bool>
    HashMap<RefPtr<T>, U, V, W, X>::inlineAdd(const KeyType& key, const MappedType& mapped) 
    {
        const bool canReplaceDeletedKey = !KeyTraits::needsDestruction || KeyStorageTraits::needsDestruction;
        typedef HashMapTranslator<canReplaceDeletedKey, ValueType, ValueTraits, ValueStorageTraits, HashFunctions> TranslatorType;
        return m_impl.template add<KeyType, MappedType, TranslatorType>(key, mapped);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline pair<typename HashMap<RefPtr<T>, U, V, W, X>::iterator, bool>
    HashMap<RefPtr<T>, U, V, W, X>::inlineAdd(RawKeyType key, const MappedType& mapped) 
    {
        return inlineAdd(*(const KeyType*)&key, mapped);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    pair<typename HashMap<RefPtr<T>, U, V, W, X>::iterator, bool>
    HashMap<RefPtr<T>, U, V, W, X>::set(const KeyType& key, const MappedType& mapped) 
    {
        pair<iterator, bool> result = inlineAdd(key, mapped);
        if (!result.second)
            // add call above didn't change anything, so set the mapped value
            result.first->second = mapped;
        return result;
    }

    template<typename T, typename U, typename V, typename W, typename X>
    pair<typename HashMap<RefPtr<T>, U, V, W, X>::iterator, bool>
    HashMap<RefPtr<T>, U, V, W, X>::set(RawKeyType key, const MappedType& mapped) 
    {
        pair<iterator, bool> result = inlineAdd(key, mapped);
        if (!result.second)
            // add call above didn't change anything, so set the mapped value
            result.first->second = mapped;
        return result;
    }

    template<typename T, typename U, typename V, typename W, typename X>
    pair<typename HashMap<RefPtr<T>, U, V, W, X>::iterator, bool>
    HashMap<RefPtr<T>, U, V, W, X>::add(const KeyType& key, const MappedType& mapped)
    {
        return inlineAdd(key, mapped);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    pair<typename HashMap<RefPtr<T>, U, V, W, X>::iterator, bool>
    HashMap<RefPtr<T>, U, V, W, X>::add(RawKeyType key, const MappedType& mapped)
    {
        return inlineAdd(key, mapped);
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedType
    HashMap<RefPtr<T>, U, V, W, MappedTraits>::get(const KeyType& key) const
    {
        if (m_impl.isEmpty())
            return MappedTraits::emptyValue();
        ValueStorageType* entry = const_cast<HashTableType&>(m_impl).lookup(*(const KeyStorageType*)&key);
        if (!entry)
            return MappedTraits::emptyValue();
        return ((ValueType *)entry)->second;
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedType
    HashMap<RefPtr<T>, U, V, W, MappedTraits>::get(RawKeyType key) const
    {
        if (m_impl.isEmpty())
            return MappedTraits::emptyValue();
        ValueStorageType* entry = const_cast<HashTableType&>(m_impl).lookup(*(const KeyStorageType*)&key);
        if (!entry)
            return MappedTraits::emptyValue();
        return ((ValueType *)entry)->second;
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::remove(iterator it)
    {
        if (it.m_impl == m_impl.end())
            return;
        m_impl.checkTableConsistency();
        RefCounter<ValueTraits, ValueStorageTraits>::deref(*it.m_impl);
        m_impl.removeWithoutEntryConsistencyCheck(it.m_impl);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::remove(const KeyType& key)
    {
        remove(find(key));
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::remove(RawKeyType key)
    {
        remove(find(key));
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::clear()
    {
        derefAll();
        m_impl.clear();
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedType
    HashMap<RefPtr<T>, U, V, W, MappedTraits>::take(const KeyType& key)
    {
        // This can probably be made more efficient to avoid ref/deref churn.
        iterator it = find(key);
        if (it == end())
            return MappedTraits::emptyValue();
        typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedType result = it->second;
        remove(it);
        return result;
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedType
    HashMap<RefPtr<T>, U, V, W, MappedTraits>::take(RawKeyType key)
    {
        // This can probably be made more efficient to avoid ref/deref churn.
        iterator it = find(key);
        if (it == end())
            return MappedTraits::emptyValue();
        typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedType result = it->second;
        remove(it);
        return result;
    }

} // namespace WTF
