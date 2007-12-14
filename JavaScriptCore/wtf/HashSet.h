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

#ifndef WTF_HashSet_h
#define WTF_HashSet_h

#include "HashTable.h"

namespace WTF {

    template<typename T> struct IdentityExtractor;

    template<typename Value, typename HashFunctions, typename Traits> class HashSet;
    template<typename Value, typename HashFunctions, typename Traits>
    void deleteAllValues(const HashSet<Value, HashFunctions, Traits>&);

    template<typename ValueArg, typename HashArg = typename DefaultHash<ValueArg>::Hash,
        typename TraitsArg = HashTraits<ValueArg> > class HashSet {
    private:
        typedef HashArg HashFunctions;
        typedef TraitsArg ValueTraits;

        typedef typename HashKeyStorageTraits<HashFunctions, ValueTraits>::Hash StorageHashFunctions;

        typedef typename HashKeyStorageTraits<HashFunctions, ValueTraits>::Traits StorageTraits;
        typedef typename StorageTraits::TraitType StorageType;

        typedef HashTable<StorageType, StorageType, IdentityExtractor<StorageType>,
            StorageHashFunctions, StorageTraits, StorageTraits> HashTableType;

    public:
        typedef typename ValueTraits::TraitType ValueType;
        typedef HashTableIteratorAdapter<HashTableType, ValueType> iterator;
        typedef HashTableConstIteratorAdapter<HashTableType, ValueType> const_iterator;

        HashSet();
        HashSet(const HashSet&);
        HashSet& operator=(const HashSet&);
        ~HashSet();

        void swap(HashSet&);

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

        // a special version of add() that finds the object by hashing and comparing
        // with some other type, to avoid the cost of type conversion if the object is already
        // in the table. HashTranslator should have the following methods:
        //   static unsigned hash(const T&);
        //   static bool equal(const ValueType&, const T&);
        //   static translate(ValueType&, const T&, unsigned hashCode);
        template<typename T, typename HashTranslator> pair<iterator, bool> add(const T&);

        void remove(const ValueType&);
        void remove(iterator);
        void clear();

    private:
        void refAll();
        void derefAll();

        friend void deleteAllValues<>(const HashSet&);

        HashTableType m_impl;
    };

    template<typename T> struct IdentityExtractor {
        static const T& extract(const T& t) { return t; }
    };

    template<bool canReplaceDeletedValue, typename ValueType, typename ValueTraits, typename StorageTraits, typename HashFunctions>
    struct HashSetTranslator;

    template<typename ValueType, typename ValueTraits, typename StorageTraits, typename HashFunctions>
    struct HashSetTranslator<true, ValueType, ValueTraits, StorageTraits, HashFunctions> {
        typedef typename StorageTraits::TraitType StorageType;
        static unsigned hash(const ValueType& key) { return HashFunctions::hash(key); }
        static bool equal(const StorageType& a, const ValueType& b) { return HashFunctions::equal(*(const ValueType*)&a, b); }
        static void translate(StorageType& location, const ValueType& key, const ValueType&)
        {
            Assigner<ValueTraits::needsRef, ValueType, StorageType, ValueTraits>::assign(key, location);
        }
    };

    template<typename ValueType, typename ValueTraits, typename StorageTraits, typename HashFunctions>
    struct HashSetTranslator<false, ValueType, ValueTraits, StorageTraits, HashFunctions> {
        typedef typename StorageTraits::TraitType StorageType;
        static unsigned hash(const ValueType& key) { return HashFunctions::hash(key); }
        static bool equal(const StorageType& a, const ValueType& b) { return HashFunctions::equal(*(const ValueType*)&a, b); }
        static void translate(StorageType& location, const ValueType& key, const ValueType&)
        {
            if (location == StorageTraits::deletedValue())
                location = StorageTraits::emptyValue();
            Assigner<ValueTraits::needsRef, ValueType, StorageType, ValueTraits>::assign(key, location);
        }
    };

    template<bool canReplaceDeletedValue, typename ValueType, typename StorageTraits, typename T, typename Translator>
    struct HashSetTranslatorAdapter;

    template<typename ValueType, typename StorageTraits, typename T, typename Translator>
    struct HashSetTranslatorAdapter<true, ValueType, StorageTraits, T, Translator> {
        typedef typename StorageTraits::TraitType StorageType;
        static unsigned hash(const T& key) { return Translator::hash(key); }
        static bool equal(const StorageType& a, const T& b) { return Translator::equal(*(const ValueType*)&a, b); }
        static void translate(StorageType& location, const T& key, const T&, unsigned hashCode)
        {
            Translator::translate(*(ValueType*)&location, key, hashCode);
        }
    };

    template<typename ValueType, typename StorageTraits, typename T, typename Translator>
    struct HashSetTranslatorAdapter<false, ValueType, StorageTraits, T, Translator> {
        typedef typename StorageTraits::TraitType StorageType;
        static unsigned hash(const T& key) { return Translator::hash(key); }
        static bool equal(const StorageType& a, const T& b) { return Translator::equal(*(const ValueType*)&a, b); }
        static void translate(StorageType& location, const T& key, const T&, unsigned hashCode)
        {
            if (location == StorageTraits::deletedValue())
                location = StorageTraits::emptyValue();
            Translator::translate(*(ValueType*)&location, key, hashCode);
        }
    };

    template<typename T, typename U, typename V>
    inline void HashSet<T, U, V>::refAll()
    {
        HashTableRefCounter<HashTableType, ValueTraits>::refAll(m_impl);
    }

    template<typename T, typename U, typename V>
    inline void HashSet<T, U, V>::derefAll()
    {
        HashTableRefCounter<HashTableType, ValueTraits>::derefAll(m_impl);
    }

    template<typename T, typename U, typename V>
    inline HashSet<T, U, V>::HashSet()
    {
    }

    template<typename T, typename U, typename V>
    inline HashSet<T, U, V>::HashSet(const HashSet& other)
        : m_impl(other.m_impl)
    {
        refAll();
    }

    template<typename T, typename U, typename V>
    inline HashSet<T, U, V>& HashSet<T, U, V>::operator=(const HashSet& other)
    {
        HashSet tmp(other);
        swap(tmp); 
        return *this;
    }

    template<typename T, typename U, typename V>
    inline void HashSet<T, U, V>::swap(HashSet& other)
    {
        m_impl.swap(other.m_impl); 
    }

    template<typename T, typename U, typename V>
    inline HashSet<T, U, V>::~HashSet()
    {
        derefAll();
    }

    template<typename T, typename U, typename V>
    inline int HashSet<T, U, V>::size() const
    {
        return m_impl.size(); 
    }

    template<typename T, typename U, typename V>
    inline int HashSet<T, U, V>::capacity() const
    {
        return m_impl.capacity(); 
    }

    template<typename T, typename U, typename V>
    inline bool HashSet<T, U, V>::isEmpty() const
    {
        return m_impl.isEmpty(); 
    }

    template<typename T, typename U, typename V>
    inline typename HashSet<T, U, V>::iterator HashSet<T, U, V>::begin()
    {
        return m_impl.begin(); 
    }

    template<typename T, typename U, typename V>
    inline typename HashSet<T, U, V>::iterator HashSet<T, U, V>::end()
    {
        return m_impl.end(); 
    }

    template<typename T, typename U, typename V>
    inline typename HashSet<T, U, V>::const_iterator HashSet<T, U, V>::begin() const
    {
        return m_impl.begin(); 
    }

    template<typename T, typename U, typename V>
    inline typename HashSet<T, U, V>::const_iterator HashSet<T, U, V>::end() const
    {
        return m_impl.end(); 
    }

    template<typename T, typename U, typename V>
    inline typename HashSet<T, U, V>::iterator HashSet<T, U, V>::find(const ValueType& value)
    {
        return m_impl.find(*(const StorageType*)&value); 
    }

    template<typename T, typename U, typename V>
    inline typename HashSet<T, U, V>::const_iterator HashSet<T, U, V>::find(const ValueType& value) const
    {
        return m_impl.find(*(const StorageType*)&value); 
    }

    template<typename T, typename U, typename V>
    inline bool HashSet<T, U, V>::contains(const ValueType& value) const
    {
        return m_impl.contains(*(const StorageType*)&value); 
    }

    template<typename T, typename U, typename V>
    pair<typename HashSet<T, U, V>::iterator, bool> HashSet<T, U, V>::add(const ValueType &value)
    {
        const bool canReplaceDeletedValue = !ValueTraits::needsDestruction || StorageTraits::needsDestruction;
        typedef HashSetTranslator<canReplaceDeletedValue, ValueType, ValueTraits, StorageTraits, HashFunctions> Translator;
        return m_impl.template add<ValueType, ValueType, Translator>(value, value);
    }

    template<typename Value, typename HashFunctions, typename Traits>
    template<typename T, typename Translator> 
    pair<typename HashSet<Value, HashFunctions, Traits>::iterator, bool>
    HashSet<Value, HashFunctions, Traits>::add(const T& value)
    {
        const bool canReplaceDeletedValue = !ValueTraits::needsDestruction || StorageTraits::needsDestruction;
        typedef HashSetTranslatorAdapter<canReplaceDeletedValue, ValueType, StorageTraits, T, Translator> Adapter;
        return m_impl.template addPassingHashCode<T, T, Adapter>(value, value);
    }

    template<typename T, typename U, typename V>
    inline void HashSet<T, U, V>::remove(iterator it)
    {
        if (it.m_impl == m_impl.end())
            return;
        m_impl.checkTableConsistency();
        RefCounter<ValueTraits, StorageTraits>::deref(*it.m_impl);
        m_impl.removeWithoutEntryConsistencyCheck(it.m_impl);
    }

    template<typename T, typename U, typename V>
    inline void HashSet<T, U, V>::remove(const ValueType& value)
    {
        remove(find(value));
    }

    template<typename T, typename U, typename V>
    inline void HashSet<T, U, V>::clear()
    {
        derefAll();
        m_impl.clear(); 
    }

    template<typename ValueType, typename HashTableType>
    void deleteAllValues(HashTableType& collection)
    {
        typedef typename HashTableType::const_iterator iterator;
        iterator end = collection.end();
        for (iterator it = collection.begin(); it != end; ++it)
            delete *(ValueType*)&*it;
    }

    template<typename T, typename U, typename V>
    inline void deleteAllValues(const HashSet<T, U, V>& collection)
    {
        deleteAllValues<typename HashSet<T, U, V>::ValueType>(collection.m_impl);
    }
    
    template<typename T, typename U, typename V, typename W>
    inline void copyToVector(const HashSet<T, U, V>& collection, W& vector)
    {
        typedef typename HashSet<T, U, V>::const_iterator iterator;
        
        vector.resize(collection.size());
        
        iterator it = collection.begin();
        iterator end = collection.end();
        for (unsigned i = 0; it != end; ++it, ++i)
            vector[i] = *it;
    }  

} // namespace WTF

using WTF::HashSet;

#endif /* WTF_HashSet_h */
