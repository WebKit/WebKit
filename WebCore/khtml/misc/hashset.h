/*
 * This file is part of the KDE libraries
 *
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

#ifndef HASHSET_H
#define HASHSET_H

#include "hashtable.h"

namespace khtml {

template<typename Key, unsigned Hash(const Key&), bool Equal(const Key&, const Key&)>
class HashSet {
 public:
    typedef HashTable<Key, Hash, Equal> ImplType;
    typedef typename ImplType::KeyType KeyType;

    HashSet() {}

    int size() const { return m_impl.count(); }
    bool isEmpty() const { return size() == 0; }

    KeyType insert(const KeyType &key)
    {
        return m_impl.insert(key);
    }

    // a special version of insert() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table
    template<typename T, unsigned HashT(const T&), bool EqualT(const KeyType&, const T&), KeyType ConvertT(const T&, unsigned)> KeyType insert(const T& key)
    {
        return m_impl.insert<T, HashT, EqualT, ConvertT>(key);
    }

#if 0
    Iterator find(const KeyType& key)
    {
        return m_impl.find(key); 
    }
#endif

    bool contains(const KeyType& key)
    {
        return find(key) != 0;
    }

    void remove(const KeyType& key)
    {
        m_impl.remove(key);
    }

 private:
    ImplType m_impl;
};

} // namespace khtml

#endif /* HASHSET_H */
