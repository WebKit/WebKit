/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ZippedRange.h>

namespace JSC { namespace DFG {
class StructureAbstractValue;
} } // namespace JSC::DFG

namespace WTF {

// FIXME: This currently only works for types that are pointer-like: they should have the size
// of a pointer and like a pointer they should not have assignment operators, copy constructors,
// non-trivial default constructors, and non-trivial destructors. It may be possible to lift all
// of these restrictions. If we succeeded then this should be renamed to just TinySet.
// https://bugs.webkit.org/show_bug.cgi?id=145741

template<typename T>
class TinyPtrSet {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(TinyPtrSet);
    static_assert(sizeof(T) == sizeof(void*), "It's in the title of the class.");
public:
    TinyPtrSet()
        : m_pointer(0)
    {
        setEmpty();
    }
    
    TinyPtrSet(T element)
        : m_pointer(0)
    {
        set(element);
    }
    
    ALWAYS_INLINE TinyPtrSet(const TinyPtrSet& other)
        : m_pointer(0)
    {
        copyFrom(other);
    }
    
    ALWAYS_INLINE TinyPtrSet& operator=(const TinyPtrSet& other)
    {
        if (this == &other)
            return *this;
        deleteListIfNecessary();
        copyFrom(other);
        return *this;
    }
    
    ~TinyPtrSet()
    {
        deleteListIfNecessary();
    }
    
    void clear()
    {
        deleteListIfNecessary();
        setEmpty();
    }
    
    // Returns the only entry if the array has exactly one entry.
    T onlyEntry() const
    {
        if (isThin())
            return singleEntry();
        auto list = this->list()->lengthSpan();
        if (list.size() != 1)
            return T();
        return list[0];
    }
    
    bool isEmpty() const
    {
        bool result = isThin() && !singleEntry();
        if (result)
            ASSERT(m_pointer != reservedValue);
        return result;
    }
    
    // Returns true if the value was added, or false if the value was already there.
    ALWAYS_INLINE bool add(T value)
    {
        ASSERT(value);
        if (isThin()) {
            if (singleEntry() == value)
                return false;
            if (!singleEntry()) {
                set(value);
                return true;
            }
            
            OutOfLineList* list = OutOfLineList::create(defaultStartingSize);
            list->m_length = 2;
            auto listSpan = list->lengthSpan();
            listSpan[0] = singleEntry();
            listSpan[1] = value;
            set(list);
            return true;
        }
        
        return addOutOfLine(value);
    }
    
    bool remove(T value)
    {
        if (isThin()) {
            if (singleEntry() == value) {
                setEmpty();
                return true;
            }
            return false;
        }
        
        OutOfLineList* list = this->list();
        auto listSpan = list->lengthSpan();
        for (unsigned i = 0; i < list->m_length; ++i) {
            if (listSpan[i] != value)
                continue;
            listSpan[i] = listSpan[--list->m_length];
            if (!list->m_length) {
                OutOfLineList::destroy(list);
                setEmpty();
            }
            return true;
        }
        return false;
    }
    
    bool contains(T value) const
    {
        if (isThin())
            return singleEntry() == value;
        return containsOutOfLine(value);
    }
    
    ALWAYS_INLINE bool merge(const TinyPtrSet& other)
    {
        if (other.isThin()) {
            if (other.singleEntry())
                return add(other.singleEntry());
            return false;
        }
        
        return mergeOtherOutOfLine(other);
    }
    
    void forEach(NOESCAPE const Invocable<void(const T&)> auto& functor) const
    {
        if (isThin()) {
            if (!singleEntry())
                return;
            functor(singleEntry());
            return;
        }

        for (auto& item : list()->lengthSpan())
            functor(item);
    }
        
    void genericFilter(NOESCAPE const Invocable<bool(const T&)> auto& functor)
    {
        if (isThin()) {
            if (!singleEntry())
                return;
            if (functor(singleEntry()))
                return;
            clear();
            return;
        }
        
        OutOfLineList* list = this->list();
        auto listSpan = list->lengthSpan();
        for (unsigned i = 0; i < list->m_length; ++i) {
            if (functor(listSpan[i]))
                continue;
            listSpan[i--] = listSpan[--list->m_length];
        }
        if (!list->m_length)
            clear();
    }
        
    void filter(const TinyPtrSet& other)
    {
        if (other.isThin()) {
            if (!other.singleEntry() || !contains(other.singleEntry()))
                clear();
            else {
                clear();
                set(other.singleEntry());
            }
            return;
        }
        
        genericFilter([&] (T value) { return other.containsOutOfLine(value); });
    }
    
    void exclude(const TinyPtrSet& other)
    {
        if (other.isThin()) {
            if (other.singleEntry())
                remove(other.singleEntry());
            return;
        }
        
        genericFilter([&] (T value) { return !other.containsOutOfLine(value); });
    }
    
    bool isSubsetOf(const TinyPtrSet& other) const
    {
        if (isThin()) {
            if (!singleEntry())
                return true;
            return other.contains(singleEntry());
        }
        
        if (other.isThin()) {
            if (!other.singleEntry())
                return false;
            auto list = this->list()->lengthSpan();
            if (list.size() >= 2)
                return false;
            if (list[0] == other.singleEntry())
                return true;
            return false;
        }

        for (auto& item : list()->lengthSpan()) {
            if (!other.containsOutOfLine(item))
                return false;
        }
        return true;
    }
    
    bool isSupersetOf(const TinyPtrSet& other) const
    {
        return other.isSubsetOf(*this);
    }
    
    bool overlaps(const TinyPtrSet& other) const
    {
        if (isThin()) {
            if (!singleEntry())
                return false;
            return other.contains(singleEntry());
        }
        
        if (other.isThin()) {
            if (!other.singleEntry())
                return false;
            return containsOutOfLine(other.singleEntry());
        }

        for (auto& item : list()->lengthSpan()) {
            if (other.containsOutOfLine(item))
                return true;
        }
        return false;
    }
    
    size_t size() const
    {
        if (isThin())
            return !!singleEntry();
        return list()->m_length;
    }
    
    T at(size_t i) const
    {
        if (isThin()) {
            ASSERT(!i);
            ASSERT(singleEntry());
            return singleEntry();
        }
        return list()->lengthSpan()[i];
    }
    
    T operator[](size_t i) const { return at(i); }
    
    T last() const
    {
        if (isThin()) {
            ASSERT(singleEntry());
            return singleEntry();
        }
        return list()->lengthSpan().back();
    }
    
    class iterator {
    public:
        iterator()
            : m_set(nullptr)
            , m_index(0)
        {
        }
        
        iterator(const TinyPtrSet* set, size_t index)
            : m_set(set)
            , m_index(index)
        {
        }
        
        T operator*() const { return m_set->at(m_index); }
        iterator& operator++()
        {
            m_index++;
            return *this;
        }
        bool operator==(const iterator& other) const { return m_index == other.m_index; }

    private:
        const TinyPtrSet* m_set;
        size_t m_index;
    };
    
    iterator begin() const { return iterator(this, 0); }
    iterator end() const { return iterator(this, size()); }
    
    bool operator==(const TinyPtrSet& other) const
    {
        if (size() != other.size())
            return false;
        return isSubsetOf(other);
    }
    
private:
    friend class JSC::DFG::StructureAbstractValue;

    static constexpr uintptr_t fatFlag = 1;
    static constexpr uintptr_t reservedFlag = 2;
    static constexpr uintptr_t flags = fatFlag | reservedFlag;
    static constexpr uintptr_t reservedValue = 4;

    static constexpr unsigned defaultStartingSize = 4;
    
    NEVER_INLINE bool addOutOfLine(T value)
    {
        OutOfLineList* list = this->list();
        for (auto& item : list->lengthSpan()) {
            if (item == value)
                return false;
        }

        if (list->m_length < list->m_capacity) {
            list->capacitySpan()[list->m_length++] = value;
            return true;
        }
        
        OutOfLineList* newList = OutOfLineList::create(list->m_capacity * 2);
        newList->m_length = list->m_length + 1;
        auto newListSpan = newList->lengthSpan();
        for (auto [source, destination] : zippedRange(list->lengthSpan(), newListSpan.first(list->m_length)))
            destination = source;
        newListSpan.back() = value;
        OutOfLineList::destroy(list);
        set(newList);
        return true;
    }
    
    NEVER_INLINE bool mergeOtherOutOfLine(const TinyPtrSet& other)
    {
        OutOfLineList* list = other.list();
        if (list->m_length >= 2) {
            if (isThin()) {
                OutOfLineList* myNewList = OutOfLineList::create(
                    list->m_length + !!singleEntry());
                if (singleEntry()) {
                    myNewList->m_length = 1;
                    myNewList->lengthSpan()[0] = singleEntry();
                }
                set(myNewList);
            }
            bool changed = false;
            for (auto& item : list->lengthSpan())
                changed |= addOutOfLine(item);
            return changed;
        }
        return add(list->lengthSpan()[0]);
    }
    
    bool containsOutOfLine(T value) const
    {
        auto list = this->list()->lengthSpan();
        return std::ranges::find(list, value) != list.end();
    }
    
    ALWAYS_INLINE void copyFrom(const TinyPtrSet& other)
    {
        if (other.isThin() || other.m_pointer == reservedValue) {
            bool value = getReservedFlag();
            m_pointer = other.m_pointer;
            setReservedFlag(value);
            return;
        }
        copyFromOutOfLine(other);
    }
    
    NEVER_INLINE void copyFromOutOfLine(const TinyPtrSet& other)
    {
        ASSERT(!other.isThin() && other.m_pointer != reservedValue);
        OutOfLineList* otherList = other.list();
        OutOfLineList* myList = OutOfLineList::create(otherList->m_length);
        myList->m_length = otherList->m_length;
        for (auto [source, destination] : zippedRange(otherList->lengthSpan(), myList->lengthSpan()))
            destination = source;
        set(myList);
    }
    
    class OutOfLineList {
    public:
        static OutOfLineList* create(unsigned capacity)
        {
            return new (NotNull, fastMalloc(sizeof(OutOfLineList) + capacity * sizeof(T))) OutOfLineList(0, capacity);
        }
        
        static void destroy(OutOfLineList* list)
        {
            fastFree(list);
        }
        
        std::span<T> capacitySpan() { return unsafeMakeSpan(m_list, m_capacity); }
        std::span<T> lengthSpan() { return capacitySpan().first(m_length); }
        
        OutOfLineList(unsigned length, unsigned capacity)
            : m_length(length)
            , m_capacity(capacity)
        {
        }

        unsigned m_length;
        unsigned m_capacity;
        T m_list[0];
    };
    
    ALWAYS_INLINE void deleteListIfNecessary()
    {
        if (!isThin()) {
            ASSERT(m_pointer != reservedValue);
            OutOfLineList::destroy(list());
        }
    }
    
    bool isThin() const { return !(m_pointer & fatFlag); }
    
    void* pointer() const
    {
        return std::bit_cast<void*>(m_pointer & ~flags);
    }
    
    T singleEntry() const
    {
        ASSERT(isThin());
        return std::bit_cast<T>(pointer());
    }
    
    OutOfLineList* list() const
    {
        ASSERT(!isThin());
        return static_cast<OutOfLineList*>(pointer());
    }
    
    void set(T value)
    {
        set(std::bit_cast<uintptr_t>(value), true);
    }
    void set(OutOfLineList* list)
    {
        set(std::bit_cast<uintptr_t>(list), false);
    }
    void setEmpty()
    {
        set(0, true);
    }
    void set(uintptr_t pointer, bool singleEntry)
    {
        m_pointer = pointer | (singleEntry ? 0 : fatFlag) | (m_pointer & reservedFlag);
    }
    bool getReservedFlag() const { return m_pointer & reservedFlag; }
    void setReservedFlag(bool value)
    {
        if (value)
            m_pointer |= reservedFlag;
        else
            m_pointer &= ~reservedFlag;
    }
    
    uintptr_t m_pointer;
};

} // namespace WTF

using WTF::TinyPtrSet;
