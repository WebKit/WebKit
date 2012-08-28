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

public:
    WeakGCMap()
    {
    }

    void clear()
    {
        typename MapType::iterator end = m_map.end();
        for (typename MapType::iterator ptr = m_map.begin(); ptr != end; ++ptr)
            WeakSet::deallocate(ptr->second);
        m_map.clear();
    }

    ExternalType get(const KeyType& key) const
    {
        WeakImpl* impl = m_map.get(key);
        if (!impl || impl->state() != WeakImpl::Live)
            return ExternalType();
        return HandleTypes<MappedType>::getFromSlot(const_cast<JSValue*>(&impl->jsValue()));
    }

    void set(JSGlobalData& globalData, const KeyType& key, ExternalType value)
    {
        ASSERT_UNUSED(globalData, globalData.apiLock().currentThreadIsHoldingLock());
        typename MapType::AddResult result = m_map.add(key, 0);
        if (!result.isNewEntry)
            WeakSet::deallocate(result.iterator->second);
        result.iterator->second = WeakSet::allocate(value, this, FinalizerCallback::finalizerContextFor(key));
    }

    void remove(const KeyType& key)
    {
        WeakImpl* impl = m_map.take(key);
        if (!impl)
            return;
        WeakSet::deallocate(impl);
    }

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
