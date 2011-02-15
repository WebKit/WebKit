/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef Global_h
#define Global_h

#include "Assertions.h"
#include "Handle.h"
#include "HandleHeap.h"
#include "JSGlobalData.h"

namespace JSC {

/*
    A Global is a persistent handle whose lifetime is not limited to any given
    scope. Use Globals for data members and global variables.
*/

template <typename T> class Global : public Handle<T> {
public:
    typedef typename Handle<T>::ExternalType ExternalType;
    Global(JSGlobalData& globalData, ExternalType ptr = ExternalType())
        : Handle<T>(globalData.allocateGlobalHandle())
    {
        internalSet(ptr);
    }

    Global(JSGlobalData& globalData, Handle<T> handle)
        : Handle<T>(globalData.allocateGlobalHandle())
    {
        internalSet(handle.get());
    }
    
    enum EmptyValueTag { EmptyValue };
    Global(EmptyValueTag)
        : Handle<T>(0, HandleBase::DontNullCheckSlot)
    {
    }

    ~Global()
    {
        HandleSlot slot = this->slot();
        if (slot)
            HandleHeap::heapFor(slot)->deallocate(slot);
    }

    void set(JSGlobalData& globalData, ExternalType value)
    {
        if (!value) {
            clear();
            return;
        }
        if (!this->slot())
            this->setSlot(globalData.allocateGlobalHandle());
        internalSet(value);
    }

    template <typename U> Global& operator=(const Global<U>& handle)
    {
        if (handle.slot()) {
            if (!this->slot())
                this->setSlot(HandleHeap::heapFor(handle.slot())->allocate());
            internalSet(handle.get());
        } else
            clear();
        
        return *this;
    }
    
    Global& operator=(const Global& handle)
    {
        if (handle.slot()) {
            if (!this->slot())
                this->setSlot(HandleHeap::heapFor(handle.slot())->allocate());
            internalSet(handle.get());
        } else
            clear();
        
        return *this;
    }

    void clear()
    {
        if (this->slot())
            internalSet(ExternalType());
    }

    enum HashTableDeletedValueType { HashTableDeletedValue };
    const static intptr_t HashTableDeletedValueTag = 0x1;
    Global(HashTableDeletedValueType)
        : Handle<T>(reinterpret_cast<HandleSlot>(HashTableDeletedValueTag))
    {
    }
    bool isHashTableDeletedValue() const { return slot() == reinterpret_cast<HandleSlot>(HashTableDeletedValueTag); }

    template <typename U> Global(const Global<U>& other)
        : Handle<T>(other.slot() ? HandleHeap::heapFor(other.slot())->allocate() : 0, Handle<T>::DontNullCheckSlot)
    {
        if (other.slot())
            internalSet(other.get());
    }
    
    Global(const Global& other)
        : Handle<T>(other.slot() ? HandleHeap::heapFor(other.slot())->allocate() : 0, Handle<T>::DontNullCheckSlot)
    {
        if (other.slot())
            internalSet(other.get());
    }

protected:
    void internalSet(ExternalType value)
    {
        JSValue newValue(HandleTypes<T>::toJSValue(value));
        HandleSlot slot = this->slot();
        ASSERT(slot);
        HandleHeap::heapFor(slot)->writeBarrier(slot, newValue);
        *slot = newValue;
    }

    using Handle<T>::slot;
    
};

}

namespace WTF {

template<typename P> struct HashTraits<JSC::Global<P> > : GenericHashTraits<JSC::Global<P> > {
    static const bool emptyValueIsZero = true;
    static JSC::Global<P> emptyValue() { return JSC::Global<P>(JSC::Global<P>::EmptyValue); }
    static void constructDeletedValue(JSC::Global<P>& slot) { new (&slot) JSC::Global<P>(JSC::Global<P>::HashTableDeletedValue); }
    static bool isDeletedValue(const JSC::Global<P>& value) { return value.isHashTableDeletedValue(); }
};

}

#endif // Global_h
