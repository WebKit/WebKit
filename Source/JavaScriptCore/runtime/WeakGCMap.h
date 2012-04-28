/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WeakGCMap_h
#define WeakGCMap_h

#include "Handle.h"
#include "JSGlobalData.h"
#include <wtf/HashMap.h>

namespace JSC {

// A HashMap for GC'd values that removes entries when the associated value
// dies.
template <typename KeyType, typename MappedType> struct DefaultWeakGCMapFinalizerCallback {
    static void* finalizerContextFor(KeyType key)
    {
        return reinterpret_cast<void*>(key);
    }

    static KeyType keyForFinalizer(void* context, typename HandleTypes<MappedType>::ExternalType)
    {
        return reinterpret_cast<KeyType>(context);
    }
};

template<typename KeyType, typename MappedType, typename FinalizerCallback = DefaultWeakGCMapFinalizerCallback<KeyType, MappedType>, typename HashArg = typename DefaultHash<KeyType>::Hash, typename KeyTraitsArg = HashTraits<KeyType> >
class WeakGCMap : private WeakHandleOwner {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WeakGCMap);

    typedef HashMap<KeyType, WeakImpl*, HashArg, KeyTraitsArg> MapType;
    typedef typename HandleTypes<MappedType>::ExternalType ExternalType;
    typedef typename MapType::iterator map_iterator;

public:

    struct iterator {
        friend class WeakGCMap;
        iterator(map_iterator iter)
            : m_iterator(iter)
        {
        }
        
        std::pair<KeyType, ExternalType> get() const { return std::make_pair(m_iterator->first, HandleTypes<MappedType>::getFromSlot(const_cast<JSValue*>(&m_iterator->second->jsValue()))); }
        
        iterator& operator++() { ++m_iterator; return *this; }
        
        // postfix ++ intentionally omitted
        
        // Comparison.
        bool operator==(const iterator& other) const { return m_iterator == other.m_iterator; }
        bool operator!=(const iterator& other) const { return m_iterator != other.m_iterator; }
        
    private:
        map_iterator m_iterator;
    };

    typedef WTF::HashTableAddResult<iterator> AddResult;

    WeakGCMap()
    {
    }

    bool isEmpty() { return m_map.isEmpty(); }
    void clear()
    {
        map_iterator end = m_map.end();
        for (map_iterator ptr = m_map.begin(); ptr != end; ++ptr)
            WeakSet::deallocate(ptr->second);
        m_map.clear();
    }

    bool contains(const KeyType& key) const
    {
        return m_map.contains(key);
    }

    iterator find(const KeyType& key)
    {
        return m_map.find(key);
    }

    void remove(iterator iter)
    {
        ASSERT(iter.m_iterator != m_map.end());
        WeakImpl* impl = iter.m_iterator->second;
        ASSERT(impl);
        WeakSet::deallocate(impl);
        m_map.remove(iter.m_iterator);
    }

    ExternalType get(const KeyType& key) const
    {
        return HandleTypes<MappedType>::getFromSlot(const_cast<JSValue*>(&m_map.get(key)->jsValue()));
    }

    AddResult add(JSGlobalData&, const KeyType& key, ExternalType value)
    {
        typename MapType::AddResult result = m_map.add(key, 0);
        if (result.isNewEntry)
            result.iterator->second = WeakSet::allocate(value, this, FinalizerCallback::finalizerContextFor(key));

        // WeakGCMap exposes a different iterator, so we need to wrap it and create our own AddResult.
        return AddResult(iterator(result.iterator), result.isNewEntry);
    }

    void set(JSGlobalData&, const KeyType& key, ExternalType value)
    {
        typename MapType::AddResult result = m_map.add(key, 0);
        if (!result.isNewEntry)
            WeakSet::deallocate(result.iterator->second);
        result.iterator->second = WeakSet::allocate(value, this, FinalizerCallback::finalizerContextFor(key));
    }

    ExternalType take(const KeyType& key)
    {
        WeakImpl* impl = m_map.take(key);
        if (!impl)
            return HashTraits<ExternalType>::emptyValue();
        ExternalType result = HandleTypes<MappedType>::getFromSlot(const_cast<JSValue*>(&impl->jsValue()));
        WeakSet::deallocate(impl);
        return result;
    }

    size_t size() { return m_map.size(); }

    iterator begin() { return iterator(m_map.begin()); }
    iterator end() { return iterator(m_map.end()); }
    
    ~WeakGCMap()
    {
        clear();
    }
    
private:
    virtual void finalize(Handle<Unknown> handle, void* context)
    {
        WeakImpl* impl = m_map.take(FinalizerCallback::keyForFinalizer(context, HandleTypes<MappedType>::getFromSlot(handle.slot())));
        ASSERT(impl);
        WeakSet::deallocate(impl);
    }

    MapType m_map;
};

} // namespace JSC

#endif // WeakGCMap_h
