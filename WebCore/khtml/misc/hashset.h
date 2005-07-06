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
#include "hashtraits.h"
#include "hashfunctions.h"

namespace khtml {

template <typename T>
inline T identityExtract(const T& t) 
{ 
    return t; 
}

template<typename Value, typename HashFunctions = DefaultHash<Value>, typename Traits = HashTraits<Value> >
class HashSet {
 private:
    typedef HashTable<Value, Value, identityExtract<Value>, HashFunctions, Traits> ImplType;
 public:
    typedef Value ValueType;
    typedef typename ImplType::iterator iterator;
    typedef typename ImplType::const_iterator const_iterator;

    HashSet() {}

    int size() const;
    int capacity() const;
    bool isEmpty() const;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    iterator find(const ValueType& value);
    const_iterator find(const ValueType& value) const;
    bool contains(const ValueType& value) const;

    std::pair<iterator, bool> insert(const ValueType &value);

    // a special version of insert() that finds the object by hashing and comparing
    // with some other type, to avoid the cost of type conversion if the object is already
    // in the table
    template<typename T, unsigned HashT(const T&), bool EqualT(const ValueType&, const T&), ValueType ConvertT(const T&, unsigned)> 
    std::pair<iterator, bool> insert(const T& value);

    void remove(const ValueType& value);
    void remove(iterator it);
    void clear();

 private:
    template<typename T, ValueType ConvertT(const T&, unsigned)> 
    static ValueType convertAdapter(const T& t, const T&, unsigned h);

    ImplType m_impl;
};

template<typename Value, typename HashFunctions, typename Traits>
int HashSet<Value, HashFunctions, Traits>::size() const
{
    return m_impl.count(); 
}

template<typename Value, typename HashFunctions, typename Traits>
int HashSet<Value, HashFunctions, Traits>::capacity() const
{
    return m_impl.capacity(); 
}

template<typename Value, typename HashFunctions, typename Traits>
bool HashSet<Value, HashFunctions, Traits>::isEmpty() const
{
    return size() == 0; 
}

template<typename Value, typename HashFunctions, typename Traits>
typename HashSet<Value, HashFunctions, Traits>::iterator HashSet<Value, HashFunctions, Traits>::begin()
{
    return m_impl.begin(); 
}

template<typename Value, typename HashFunctions, typename Traits>
typename HashSet<Value, HashFunctions, Traits>::iterator HashSet<Value, HashFunctions, Traits>::end()
{
    return m_impl.end(); 
}

template<typename Value, typename HashFunctions, typename Traits>
typename HashSet<Value, HashFunctions, Traits>::const_iterator HashSet<Value, HashFunctions, Traits>::begin() const
{
    return m_impl.begin(); 
}

template<typename Value, typename HashFunctions, typename Traits>
typename HashSet<Value, HashFunctions, Traits>::const_iterator HashSet<Value, HashFunctions, Traits>::end() const
{
    return m_impl.end(); 
}

template<typename Value, typename HashFunctions, typename Traits>
typename HashSet<Value, HashFunctions, Traits>::iterator HashSet<Value, HashFunctions, Traits>::find(const ValueType& value)
{
    return m_impl.find(value); 
}

template<typename Value, typename HashFunctions, typename Traits>
typename HashSet<Value, HashFunctions, Traits>::const_iterator HashSet<Value, HashFunctions, Traits>::find(const ValueType& value) const
{
    return m_impl.find(value); 
}

template<typename Value, typename HashFunctions, typename Traits>
bool HashSet<Value, HashFunctions, Traits>::contains(const ValueType& value) const
{
    return m_impl.contains(value); 
}

template<typename Value, typename HashFunctions, typename Traits>
std::pair<typename HashSet<Value, HashFunctions, Traits>::iterator, bool> HashSet<Value, HashFunctions, Traits>::insert(const ValueType &value)
{
    return m_impl.insert(value); 
}

template<typename Value, typename HashFunctions, typename Traits>
template<typename T, unsigned HashT(const T&), bool EqualT(const Value&, const T&), Value ConvertT(const T&, unsigned)> 
std::pair<typename HashSet<Value, HashFunctions, Traits>::iterator, bool> HashSet<Value, HashFunctions, Traits>::insert(const T& value)
{
    return m_impl.insert<T, T, HashT, EqualT, HashSet::convertAdapter<T, ConvertT> >(value, value); 
}

template<typename Value, typename HashFunctions, typename Traits>
void HashSet<Value, HashFunctions, Traits>::remove(const ValueType& value)
{
    m_impl.remove(value); 
}

template<typename Value, typename HashFunctions, typename Traits>
void HashSet<Value, HashFunctions, Traits>::remove(iterator it)
{
    m_impl.remove(it); 
}

template<typename Value, typename HashFunctions, typename Traits>
void HashSet<Value, HashFunctions, Traits>::clear()
{
    m_impl.clear(); 
}

template<typename Value, typename HashFunctions, typename Traits>
template<typename T, Value ConvertT(const T&, unsigned)> 
inline Value HashSet<Value, HashFunctions, Traits>::convertAdapter(const T& t, const T&, unsigned h)
{ 
    return ConvertT(t, h); 
}

} // namespace khtml

#endif /* HASHSET_H */


