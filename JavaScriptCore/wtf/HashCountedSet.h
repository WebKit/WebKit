/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef WTF_HashCountedSet_h
#define WTF_HashCountedSet_h

#include "Assertions.h"
#include "FastAllocBase.h"
#include "HashMap.h"
#include "Vector.h"

namespace WTF {

    template<typename Value, typename HashFunctions = typename DefaultHash<Value>::Hash,
        typename Traits = HashTraits<Value> > class HashCountedSet : public FastAllocBase {
    private:
        typedef HashMap<Value, unsigned, HashFunctions, Traits> ImplType;
    public:
        typedef Value ValueType;
        typedef typename ImplType::iterator iterator;
        typedef typename ImplType::const_iterator const_iterator;
        
        HashCountedSet() {}
        
        int size() const;
        int capacity() const;
        bool isEmpty() const;
        
        // iterators iterate over pairs of values and counts
        iterator begin();
        iterator end();
        const_iterator begin() const;
        const_iterator end() const;
        
        iterator find(const ValueType& value);
        const_iterator find(const ValueType& value) const;
        bool contains(const ValueType& value) const;
        unsigned count(const ValueType& value) const;

        // increases the count if an equal value is already present
        // the return value is a pair of an interator to the new value's location, 
        // and a bool that is true if an new entry was added
        std::pair<iterator, bool> add(const ValueType &value);
        
        // reduces the count of the value, and removes it if count
        // goes down to zero
        void remove(const ValueType& value);
        void remove(iterator it);
 
        // removes the value, regardless of its count
        void clear(iterator it);
        void clear(const ValueType& value);

        // clears the whole set
        void clear();

    private:
        ImplType m_impl;
    };
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline int HashCountedSet<Value, HashFunctions, Traits>::size() const
    {
        return m_impl.size(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline int HashCountedSet<Value, HashFunctions, Traits>::capacity() const
    {
        return m_impl.capacity(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline bool HashCountedSet<Value, HashFunctions, Traits>::isEmpty() const
    {
        return size() == 0; 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashCountedSet<Value, HashFunctions, Traits>::iterator HashCountedSet<Value, HashFunctions, Traits>::begin()
    {
        return m_impl.begin(); 
    }

    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashCountedSet<Value, HashFunctions, Traits>::iterator HashCountedSet<Value, HashFunctions, Traits>::end()
    {
        return m_impl.end(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashCountedSet<Value, HashFunctions, Traits>::const_iterator HashCountedSet<Value, HashFunctions, Traits>::begin() const
    {
        return m_impl.begin(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashCountedSet<Value, HashFunctions, Traits>::const_iterator HashCountedSet<Value, HashFunctions, Traits>::end() const
    {
        return m_impl.end(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashCountedSet<Value, HashFunctions, Traits>::iterator HashCountedSet<Value, HashFunctions, Traits>::find(const ValueType& value)
    {
        return m_impl.find(value); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline typename HashCountedSet<Value, HashFunctions, Traits>::const_iterator HashCountedSet<Value, HashFunctions, Traits>::find(const ValueType& value) const
    {
        return m_impl.find(value); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline bool HashCountedSet<Value, HashFunctions, Traits>::contains(const ValueType& value) const
    {
        return m_impl.contains(value); 
    }

    template<typename Value, typename HashFunctions, typename Traits>
    inline unsigned HashCountedSet<Value, HashFunctions, Traits>::count(const ValueType& value) const
    {
        return m_impl.get(value);
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline std::pair<typename HashCountedSet<Value, HashFunctions, Traits>::iterator, bool> HashCountedSet<Value, HashFunctions, Traits>::add(const ValueType &value)
    {
        pair<iterator, bool> result = m_impl.add(value, 0); 
        ++result.first->second;
        return result;
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline void HashCountedSet<Value, HashFunctions, Traits>::remove(const ValueType& value)
    {
        remove(find(value));
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline void HashCountedSet<Value, HashFunctions, Traits>::remove(iterator it)
    {
        if (it == end())
            return;

        unsigned oldVal = it->second;
        ASSERT(oldVal != 0);
        unsigned newVal = oldVal - 1;
        if (newVal == 0)
            m_impl.remove(it);
        else
            it->second = newVal;
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline void HashCountedSet<Value, HashFunctions, Traits>::clear(const ValueType& value)
    {
        clear(find(value));
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline void HashCountedSet<Value, HashFunctions, Traits>::clear(iterator it)
    {
        if (it == end())
            return;

        m_impl.remove(it);
    }
    
    template<typename Value, typename HashFunctions, typename Traits>
    inline void HashCountedSet<Value, HashFunctions, Traits>::clear()
    {
        m_impl.clear(); 
    }
    
    template<typename Value, typename HashFunctions, typename Traits, typename VectorType>
    inline void copyToVector(const HashCountedSet<Value, HashFunctions, Traits>& collection, VectorType& vector)
    {
        typedef typename HashCountedSet<Value, HashFunctions, Traits>::const_iterator iterator;
        
        vector.resize(collection.size());
        
        iterator it = collection.begin();
        iterator end = collection.end();
        for (unsigned i = 0; it != end; ++it, ++i)
            vector[i] = *it;
    }

    template<typename Value, typename HashFunctions, typename Traits>
    inline void copyToVector(const HashCountedSet<Value, HashFunctions, Traits>& collection, Vector<Value>& vector)
    {
        typedef typename HashCountedSet<Value, HashFunctions, Traits>::const_iterator iterator;
        
        vector.resize(collection.size());
        
        iterator it = collection.begin();
        iterator end = collection.end();
        for (unsigned i = 0; it != end; ++it, ++i)
            vector[i] = (*it).first;
    }


} // namespace khtml

using WTF::HashCountedSet;

#endif /* WTF_HashCountedSet_h */
