/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2013 Apple Inc. All rights reserved.
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

#ifndef RefPtrHashMap_h
#define RefPtrHashMap_h

namespace WTF {

    // This specialization is a copy of HashMap for use with RefPtr keys, with overloaded functions
    // to allow for lookup by pointer instead of RefPtr, avoiding ref-count churn.
    
     // FIXME: Find a way to do this with traits that doesn't require a copy of the HashMap template.
    
    template<typename T, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    class HashMap<RefPtr<T>, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg> {
        WTF_MAKE_FAST_ALLOCATED;
    private:
        typedef KeyTraitsArg KeyTraits;
        typedef MappedTraitsArg MappedTraits;
        typedef KeyValuePairHashTraits<KeyTraits, MappedTraits> ValueTraits;

    public:
        typedef typename KeyTraits::TraitType KeyType;
        typedef T* RawKeyType;
        typedef typename MappedTraits::TraitType MappedType;
        typedef typename ValueTraits::TraitType ValueType;

    private:
        typedef typename MappedTraits::PeekType MappedPeekType;
        
        typedef HashArg HashFunctions;

        typedef HashTable<KeyType, ValueType, KeyValuePairKeyExtractor<ValueType>,
            HashFunctions, ValueTraits, KeyTraits> HashTableType;

        typedef HashMapTranslator<ValueTraits, HashFunctions>
            Translator;

    public:
        typedef HashTableIteratorAdapter<HashTableType, ValueType> iterator;
        typedef HashTableConstIteratorAdapter<HashTableType, ValueType> const_iterator;
        typedef typename HashTableType::AddResult AddResult;

        void swap(HashMap&);

        int size() const;
        int capacity() const;
        bool isEmpty() const;

        // iterators iterate over pairs of keys and values
        iterator begin();
        iterator end();
        const_iterator begin() const;
        const_iterator end() const;

        IteratorRange<typename iterator::Keys> keys() { return makeIteratorRange(begin().keys(), end().keys()); }
        const IteratorRange<typename const_iterator::Keys> keys() const { return makeIteratorRange(begin().keys(), end().keys()); }

        IteratorRange<typename iterator::Values> values() { return makeIteratorRange(begin().values(), end().values()); }
        const IteratorRange<typename const_iterator::Values> values() const { return makeIteratorRange(begin().values(), end().values()); }

        iterator find(const KeyType&);
        iterator find(RawKeyType);
        const_iterator find(const KeyType&) const;
        const_iterator find(RawKeyType) const;
        bool contains(const KeyType&) const;
        bool contains(RawKeyType) const;
        MappedPeekType get(const KeyType&) const;
        MappedPeekType get(RawKeyType) const;
        MappedPeekType inlineGet(RawKeyType) const;

        // replaces value but not key if key is already present
        // return value is a pair of the iterator to the key location, 
        // and a boolean that's true if a new value was actually added
        template<typename V> AddResult set(const KeyType&, V&&);
        template<typename V> AddResult set(RawKeyType, V&&);

        // does nothing if key is already present
        // return value is a pair of the iterator to the key location, 
        // and a boolean that's true if a new value was actually added
        template<typename V> AddResult add(const KeyType&, V&&);
        template<typename V> AddResult add(RawKeyType, V&&);

        bool remove(const KeyType&);
        bool remove(RawKeyType);
        bool remove(iterator);
        void clear();

        MappedType take(const KeyType&); // efficient combination of get with remove
        MappedType take(RawKeyType); // efficient combination of get with remove

    private:
        template<typename V>
        AddResult inlineAdd(const KeyType&, V&&);

        template<typename V>
        AddResult inlineAdd(RawKeyType, V&&);

        HashTableType m_impl;
    };
    
    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::swap(HashMap& other)
    {
        m_impl.swap(other.m_impl); 
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
        return m_impl.find(key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::iterator HashMap<RefPtr<T>, U, V, W, X>::find(RawKeyType key)
    {
        return m_impl.template find<Translator>(key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::const_iterator HashMap<RefPtr<T>, U, V, W, X>::find(const KeyType& key) const
    {
        return m_impl.find(key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline typename HashMap<RefPtr<T>, U, V, W, X>::const_iterator HashMap<RefPtr<T>, U, V, W, X>::find(RawKeyType key) const
    {
        return m_impl.template find<Translator>(key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline bool HashMap<RefPtr<T>, U, V, W, X>::contains(const KeyType& key) const
    {
        return m_impl.contains(key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline bool HashMap<RefPtr<T>, U, V, W, X>::contains(RawKeyType key) const
    {
        return m_impl.template contains<Translator>(key);
    }

    template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    template<typename V>
    auto HashMap<RefPtr<KeyArg>, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>::inlineAdd(const KeyType& key, V&& mapped) -> AddResult
    {
        return m_impl.template add<Translator>(key, std::forward<V>(mapped));
    }

    template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    template<typename V>
    auto HashMap<RefPtr<KeyArg>, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>::inlineAdd(RawKeyType key, V&& mapped) -> AddResult
    {
        return m_impl.template add<Translator>(key, std::forward<V>(mapped));
    }

    template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    template<typename V>
    auto HashMap<RefPtr<KeyArg>, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>::set(const KeyType& key, V&& value) -> AddResult
    {
        AddResult result = inlineAdd(key, std::forward<V>(value));
        if (!result.isNewEntry) {
            // The inlineAdd call above found an existing hash table entry; we need to set the mapped value.
            result.iterator->value = std::forward<V>(value);
        }
        return result;
    }

    template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    template<typename V>
    auto HashMap<RefPtr<KeyArg>, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>::set(RawKeyType key, V&& value) -> AddResult
    {
        AddResult result = inlineAdd(key, std::forward<V>(value));
        if (!result.isNewEntry) {
            // The inlineAdd call above found an existing hash table entry; we need to set the mapped value.
            result.iterator->value = std::forward<V>(value);
        }
        return result;
    }

    template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    template<typename V>
    auto HashMap<RefPtr<KeyArg>, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>::add(const KeyType& key, V&& value) -> AddResult
    {
        return inlineAdd(key, std::forward<V>(value));
    }

    template<typename KeyArg, typename MappedArg, typename HashArg, typename KeyTraitsArg, typename MappedTraitsArg>
    template<typename V>
    auto HashMap<RefPtr<KeyArg>, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>::add(RawKeyType key, V&& value) -> AddResult
    {
        return inlineAdd(key, std::forward<V>(value));
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedPeekType
    HashMap<RefPtr<T>, U, V, W, MappedTraits>::get(const KeyType& key) const
    {
        ValueType* entry = const_cast<HashTableType&>(m_impl).lookup(key);
        if (!entry)
            return MappedTraits::peek(MappedTraits::emptyValue());
        return MappedTraits::peek(entry->value);
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedPeekType
    inline HashMap<RefPtr<T>, U, V, W, MappedTraits>::inlineGet(RawKeyType key) const
    {
        ValueType* entry = const_cast<HashTableType&>(m_impl).template lookup<Translator>(key);
        if (!entry)
            return MappedTraits::peek(MappedTraits::emptyValue());
        return MappedTraits::peek(entry->value);
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    typename HashMap<RefPtr<T>, U, V, W, MappedTraits>::MappedPeekType
    HashMap<RefPtr<T>, U, V, W, MappedTraits>::get(RawKeyType key) const
    {
        return inlineGet(key);
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline bool HashMap<RefPtr<T>, U, V, W, X>::remove(iterator it)
    {
        if (it.m_impl == m_impl.end())
            return false;
        m_impl.internalCheckTableConsistency();
        m_impl.removeWithoutEntryConsistencyCheck(it.m_impl);
        return true;
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline bool HashMap<RefPtr<T>, U, V, W, X>::remove(const KeyType& key)
    {
        return remove(find(key));
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline bool HashMap<RefPtr<T>, U, V, W, X>::remove(RawKeyType key)
    {
        return remove(find(key));
    }

    template<typename T, typename U, typename V, typename W, typename X>
    inline void HashMap<RefPtr<T>, U, V, W, X>::clear()
    {
        m_impl.clear();
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    auto HashMap<RefPtr<T>, U, V, W, MappedTraits>::take(const KeyType& key) -> MappedType
    {
        iterator it = find(key);
        if (it == end())
            return MappedTraits::emptyValue();
        MappedType value = std::move(it->value);
        remove(it);
        return value;
    }

    template<typename T, typename U, typename V, typename W, typename MappedTraits>
    auto HashMap<RefPtr<T>, U, V, W, MappedTraits>::take(RawKeyType key) -> MappedType
    {
        iterator it = find(key);
        if (it == end())
            return MappedTraits::emptyValue();
        MappedType value = std::move(it->value);
        remove(it);
        return value;
    }

} // namespace WTF

#endif // RefPtrHashMap_h
