/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "IndexingType.h"
#include "WeakGCMap.h"
#include <wtf/HashFunctions.h>
#include <wtf/text/UniquedStringImpl.h>

namespace JSC {

class JSCell;
class Structure;

static constexpr unsigned FirstInternalAttribute = 1 << 6; // Use for transitions that don't have to do with property additions.

// Support for attributes used to indicate transitions not related to properties.
// If any of these are used, the string portion of the key should be 0.
enum class NonPropertyTransition : unsigned {
    AllocateUndecided,
    AllocateInt32,
    AllocateDouble,
    AllocateContiguous,
    AllocateArrayStorage,
    AllocateSlowPutArrayStorage,
    SwitchToSlowPutArrayStorage,
    AddIndexedAccessors,
    PreventExtensions,
    Seal,
    Freeze
};
using TransitionPropertyAttributes = uint16_t;

inline unsigned toAttributes(NonPropertyTransition transition)
{
    return static_cast<unsigned>(transition) + FirstInternalAttribute;
}

inline bool changesIndexingType(NonPropertyTransition transition)
{
    switch (transition) {
    case NonPropertyTransition::AllocateUndecided:
    case NonPropertyTransition::AllocateInt32:
    case NonPropertyTransition::AllocateDouble:
    case NonPropertyTransition::AllocateContiguous:
    case NonPropertyTransition::AllocateArrayStorage:
    case NonPropertyTransition::AllocateSlowPutArrayStorage:
    case NonPropertyTransition::SwitchToSlowPutArrayStorage:
    case NonPropertyTransition::AddIndexedAccessors:
        return true;
    default:
        return false;
    }
}

inline IndexingType newIndexingType(IndexingType oldType, NonPropertyTransition transition)
{
    switch (transition) {
    case NonPropertyTransition::AllocateUndecided:
        ASSERT(!hasIndexedProperties(oldType));
        return oldType | UndecidedShape;
    case NonPropertyTransition::AllocateInt32:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || oldType == CopyOnWriteArrayWithInt32);
        return (oldType & ~IndexingShapeAndWritabilityMask) | Int32Shape;
    case NonPropertyTransition::AllocateDouble:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || hasInt32(oldType) || oldType == CopyOnWriteArrayWithDouble);
        return (oldType & ~IndexingShapeAndWritabilityMask) | DoubleShape;
    case NonPropertyTransition::AllocateContiguous:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || hasInt32(oldType) || hasDouble(oldType) || oldType == CopyOnWriteArrayWithContiguous);
        return (oldType & ~IndexingShapeAndWritabilityMask) | ContiguousShape;
    case NonPropertyTransition::AllocateArrayStorage:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || hasInt32(oldType) || hasDouble(oldType) || hasContiguous(oldType));
        return (oldType & ~IndexingShapeAndWritabilityMask) | ArrayStorageShape;
    case NonPropertyTransition::AllocateSlowPutArrayStorage:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || hasInt32(oldType) || hasDouble(oldType) || hasContiguous(oldType));
        return (oldType & ~IndexingShapeAndWritabilityMask) | SlowPutArrayStorageShape;
    case NonPropertyTransition::SwitchToSlowPutArrayStorage:
        ASSERT(hasArrayStorage(oldType));
        return (oldType & ~IndexingShapeAndWritabilityMask) | SlowPutArrayStorageShape;
    case NonPropertyTransition::AddIndexedAccessors:
        return oldType | MayHaveIndexedAccessors;
    default:
        return oldType;
    }
}

inline bool preventsExtensions(NonPropertyTransition transition)
{
    switch (transition) {
    case NonPropertyTransition::PreventExtensions:
    case NonPropertyTransition::Seal:
    case NonPropertyTransition::Freeze:
        return true;
    default:
        return false;
    }
}

inline bool setsDontDeleteOnAllProperties(NonPropertyTransition transition)
{
    switch (transition) {
    case NonPropertyTransition::Seal:
    case NonPropertyTransition::Freeze:
        return true;
    default:
        return false;
    }
}

inline bool setsReadOnlyOnNonAccessorProperties(NonPropertyTransition transition)
{
    switch (transition) {
    case NonPropertyTransition::Freeze:
        return true;
    default:
        return false;
    }
}

class StructureTransitionTable {
    static constexpr intptr_t UsingSingleSlotFlag = 1;

    
#if CPU(ADDRESS64)
    struct Hash {
        // Logically, Key is a tuple of (1) UniquedStringImpl*, (2) unsigned attributes, and (3) bool isAddition.
        // We encode (2) and (3) into (1)'s empty bits since a pointer is 48bit and lower 3 bits are usable because of alignment.
        struct Key {
            friend struct Hash;
            static_assert(WTF_OS_CONSTANT_EFFECTIVE_ADDRESS_WIDTH <= 48);
            static constexpr uintptr_t isAdditionMask = 1ULL;
            static constexpr uintptr_t stringMask = ((1ULL << 48) - 1) & (~isAdditionMask);
            static constexpr unsigned attributesShift = 48;
            static constexpr uintptr_t hashTableDeletedValue = 0x2;
            static_assert(sizeof(TransitionPropertyAttributes) * 8 <= 16);
            static_assert(hashTableDeletedValue < alignof(UniquedStringImpl));

            // Highest 16 bits are for TransitionPropertyAttributes.
            // Lowest 1 bit is for isAddition flag.
            // Remaining bits are for UniquedStringImpl*.
            Key(UniquedStringImpl* impl, unsigned attributes, bool isAddition)
                : m_encodedData(bitwise_cast<uintptr_t>(impl) | (static_cast<uintptr_t>(attributes) << attributesShift) | (static_cast<uintptr_t>(isAddition) & isAdditionMask))
            {
                ASSERT(impl == this->impl());
                ASSERT(isAddition == this->isAddition());
                ASSERT(attributes == this->attributes());
            }

            Key() = default;

            Key(WTF::HashTableDeletedValueType)
                : m_encodedData(hashTableDeletedValue)
            { }

            bool isHashTableDeletedValue() const { return m_encodedData == hashTableDeletedValue; }

            UniquedStringImpl* impl() const { return bitwise_cast<UniquedStringImpl*>(m_encodedData & stringMask); }
            bool isAddition() const { return m_encodedData & isAdditionMask; }
            unsigned attributes() const { return m_encodedData >> attributesShift; }

            friend bool operator==(const Key& a, const Key& b)
            {
                return a.m_encodedData == b.m_encodedData;
            }

            friend bool operator!=(const Key& a, const Key& b)
            {
                return a.m_encodedData != b.m_encodedData;
            }

        private:
            uintptr_t m_encodedData { 0 };
        };
        using KeyTraits = SimpleClassHashTraits<Key>;

        static unsigned hash(const Key& p)
        {
            return IntHash<uintptr_t>::hash(p.m_encodedData);
        }

        static bool equal(const Key& a, const Key& b)
        {
            return a == b;
        }

        static constexpr bool safeToCompareToEmptyOrDeleted = true;
    };
#else
    struct Hash {
        using Key = std::tuple<UniquedStringImpl*, unsigned, bool>;
        using KeyTraits = HashTraits<Key>;
        
        static unsigned hash(const Key& p)
        {
            return PtrHash<UniquedStringImpl*>::hash(std::get<0>(p)) + std::get<1>(p);
        }

        static bool equal(const Key& a, const Key& b)
        {
            return a == b;
        }

        static constexpr bool safeToCompareToEmptyOrDeleted = true;
    };
#endif

    typedef WeakGCMap<Hash::Key, Structure, Hash, Hash::KeyTraits> TransitionMap;

public:
    StructureTransitionTable()
        : m_data(UsingSingleSlotFlag)
    {
    }

    ~StructureTransitionTable()
    {
        if (!isUsingSingleSlot()) {
            delete map();
            return;
        }

        WeakImpl* impl = this->weakImpl();
        if (!impl)
            return;
        WeakSet::deallocate(impl);
    }

    void add(VM&, Structure*);
    bool contains(UniquedStringImpl*, unsigned attributes, bool isAddition) const;
    Structure* get(UniquedStringImpl*, unsigned attributes, bool isAddition) const;

private:
    friend class SingleSlotTransitionWeakOwner;

    bool isUsingSingleSlot() const
    {
        return m_data & UsingSingleSlotFlag;
    }

    TransitionMap* map() const
    {
        ASSERT(!isUsingSingleSlot());
        return bitwise_cast<TransitionMap*>(m_data);
    }

    WeakImpl* weakImpl() const
    {
        ASSERT(isUsingSingleSlot());
        return bitwise_cast<WeakImpl*>(m_data & ~UsingSingleSlotFlag);
    }

    void setMap(TransitionMap* map)
    {
        ASSERT(isUsingSingleSlot());
        
        if (WeakImpl* impl = this->weakImpl())
            WeakSet::deallocate(impl);

        // This implicitly clears the flag that indicates we're using a single transition
        m_data = bitwise_cast<intptr_t>(map);

        ASSERT(!isUsingSingleSlot());
    }

    Structure* singleTransition() const;
    void setSingleTransition(Structure*);

    intptr_t m_data;
};

} // namespace JSC
