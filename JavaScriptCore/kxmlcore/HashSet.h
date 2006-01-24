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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef KXMLCORE_HASH_SET_H
#define KXMLCORE_HASH_SET_H

#include "HashTable.h"
#include "HashTraits.h"
#include "HashFunctions.h"

namespace KXMLCore {

    template <typename T>
    struct IdentityExtractor
    {
        static const T& extract(const T& t) 
        { 
            return t; 
        }
    };

    template<typename Value, typename T, typename HashSetTranslator>
    struct HashSetTranslatorAdapter 
    {
        static unsigned hash(const T& key)
        {
            return HashSetTranslator::hash(key);
        }
        
        static bool equal(const Value& a, const T& b)
        {
            return HashSetTranslator::equal(a, b);
        }
        
        static void translate(Value& location, const T& key, const T&, unsigned hashCode)
        {
            HashSetTranslator::translate(location, key, hashCode);
        }
    };
    
    template<typename Value, typename HashFunctions = DefaultHash<Value>, typename Traits = HashTraits<Value> >
    class HashSet {
    private:
        typedef HashTable<Value, Value, IdentityExtractor<Value>, HashFunctions, Traits, Traits> ImplType;
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
        
        // the return value is a pair of an interator to the new value's location, 
        // and a bool that is true if an new entry was added
        std::pair<iterator, bool> add(const ValueType &value);
        
        // a special version of add() that finds the object by hashing and comparing
        // with some other type, to avoid the cost of type conversion if the object is already
        // in the table. HashTranslator should have the following methods:
        //   static unsigned hash(const T&);
        //   static bool equal(const ValueType&, const T&);
        //   static translate(ValueType&, const T&, unsigned hashCode);
        template<typename T, typename HashTranslator> 
        std::pair<iterator, bool> add(const T& value);
        
        void remove(const ValueType& value);
        void remove(iterator it);
        void clear();
        
    private:
        ImplType m_impl;
    };
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline int HashSet<Value, HashFunctions, Traits>::size() const
    {
        return m_impl.size(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    int HashSet<Value, HashFunctions, Traits>::capacity() const
    {
        return m_impl.capacity(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline bool HashSet<Value, HashFunctions, Traits>::isEmpty() const
    {
        return size() == 0; 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashSet<Value, HashFunctions, Traits>::iterator HashSet<Value, HashFunctions, Traits>::begin()
    {
        return m_impl.begin(); 
    }

    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashSet<Value, HashFunctions, Traits>::iterator HashSet<Value, HashFunctions, Traits>::end()
    {
        return m_impl.end(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashSet<Value, HashFunctions, Traits>::const_iterator HashSet<Value, HashFunctions, Traits>::begin() const
    {
        return m_impl.begin(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashSet<Value, HashFunctions, Traits>::const_iterator HashSet<Value, HashFunctions, Traits>::end() const
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
    inline bool HashSet<Value, HashFunctions, Traits>::contains(const ValueType& value) const
    {
        return m_impl.contains(value); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    std::pair<typename HashSet<Value, HashFunctions, Traits>::iterator, bool> HashSet<Value, HashFunctions, Traits>::add(const ValueType &value)
    {
        return m_impl.add(value); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    template<typename T, typename HashSetTranslator> 
    std::pair<typename HashSet<Value, HashFunctions, Traits>::iterator, bool> HashSet<Value, HashFunctions, Traits>::add(const T& value)
    {
        return m_impl.template add<T, T, HashSetTranslatorAdapter<ValueType, T, HashSetTranslator> >(value, value); 
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

} // namespace khtml

using KXMLCore::HashSet;

#endif /* KXMLCORE_HASH_SET_H */


