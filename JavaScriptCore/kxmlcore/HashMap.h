// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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

#ifndef KXMLCORE_HASH_MAP_H
#define KXMLCORE_HASH_MAP_H

#include "HashTable.h"
#include "HashTraits.h"
#include "HashFunctions.h"

namespace KXMLCore {

template<typename PairType> 
inline typename PairType::first_type const& extractFirst(const PairType& value)
{
    return value.first;
}

template<typename Key, typename Mapped, typename HashFunctions>
class HashMapTranslator 
{
    typedef pair<Key, Mapped> ValueType;
        
public:
    static unsigned hash(const Key& key)
    {
        return HashFunctions::hash(key);
    }
    
    static bool equal(const Key& a, const Key& b)
    {
        return HashFunctions::equal(a, b);
    }
    
    static void translate(ValueType& location, const Key& key, const Mapped& mapped, unsigned)
    {
        ValueType tmp(key, mapped);
        swap(tmp, location);
    }
};


template<typename Key, typename Mapped, typename HashFunctions = DefaultHash<Key>, typename KeyTraits = HashTraits<Key>, typename MappedTraits = HashTraits<Mapped> >
class HashMap {
 public:
    typedef Key KeyType;
    typedef Mapped MappedType;
    typedef pair<Key, Mapped> ValueType;
    typedef PairHashTraits<KeyTraits, MappedTraits> ValueTraits;
 private:
    typedef HashTable<KeyType, ValueType, extractFirst<ValueType>, HashFunctions, ValueTraits, KeyTraits> ImplType;
    typedef HashMapTranslator<Key, Mapped, HashFunctions> TranslatorType; 
 public:
    typedef typename ImplType::iterator iterator;
    typedef typename ImplType::const_iterator const_iterator;

    HashMap() {}

    int size() const;
    int capacity() const;
    bool isEmpty() const;

    // iterators iterate over pairs of keys and values
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    iterator find(const KeyType& key);
    const_iterator find(const KeyType& key) const;
    bool contains(const KeyType& key) const;
    MappedType get(const KeyType &key) const;

    // replaces value but not key if key is already present
    // return value is a pair of the iterator to the key location, 
    // and a boolean that's true if a new value was actually added
    pair<iterator, bool> set(const KeyType &key, const MappedType &mapped); 

    // does nothing if key is already present
    // return value is a pair of the iterator to the key location, 
    // and a boolean that's true if a new value was actually added
    pair<iterator, bool> add(const KeyType &key, const MappedType &mapped); 

    void remove(const KeyType& key);
    void remove(iterator it);
    void clear();

 private:
    pair<iterator, bool> inlineAdd(const KeyType &key, const MappedType &mapped);

    ImplType m_impl;
};

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
inline int HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::size() const
{
    return m_impl.size(); 
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
int HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::capacity() const
{ 
    return m_impl.capacity(); 
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
inline bool HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::isEmpty() const
{
    return size() == 0;
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
inline typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::iterator HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::begin()
{
    return m_impl.begin();
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
inline typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::iterator HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::end()
{
    return m_impl.end();
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
inline typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::const_iterator HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::begin() const
{
    return m_impl.begin();
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
inline typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::const_iterator HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::end() const
{
    return m_impl.end();
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::iterator HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::find(const KeyType& key)
{
    return m_impl.find(key);
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::const_iterator HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::find(const KeyType& key) const
{
    return m_impl.find(key);
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
bool HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::contains(const KeyType& key) const
{
    return m_impl.contains(key);
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
pair<typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::iterator, bool> HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::inlineAdd(const KeyType &key, const MappedType &mapped) 
{
    return m_impl.template insert<KeyType, MappedType, TranslatorType>(key, mapped); 
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
pair<typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::iterator, bool> HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::set(const KeyType &key, const MappedType &mapped) 
{
    pair<iterator, bool> result = inlineAdd(key, mapped);
    // the add call above won't change anything if the key is
    // already there; in that case, make sure to set the value.
    if (!result.second)
        result.first->second = mapped;    
    return result;
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
pair<typename HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::iterator, bool> HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::add(const KeyType &key, const MappedType &mapped)
{
    return inlineAdd(key, mapped);
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
inline Mapped HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::get(const KeyType &key) const
{
    const_iterator it = find(key);
    if (it == end())
        return MappedTraits::emptyValue();
    return it->second;
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
void HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::remove(const KeyType& key)
{
    m_impl.remove(key);
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
void HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::remove(iterator it)
{
    m_impl.remove(it);
}

template<typename Key, typename Mapped, typename HashFunctions, typename KeyTraits, typename MappedTraits>
void HashMap<Key, Mapped, HashFunctions, KeyTraits, MappedTraits>::clear()
{
    m_impl.clear();
}

template<typename T>
void deleteAllValues(T& collection)
{
    for (typename T::iterator it = collection.begin(); it != collection.end(); ++it) {
        delete it->second;
    }
}

} // namespace KXMLCore

using KXMLCore::HashMap;

#ifndef HASH_MAP_PTR_SPEC_WORKAROUND
#include "HashMapPtrSpec.h"
#endif

#endif /* KXMLCORE_HASH_MAP_H */
