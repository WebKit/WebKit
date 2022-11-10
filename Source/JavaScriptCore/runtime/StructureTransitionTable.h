/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

using TransitionPropertyAttributes = uint8_t;

enum class TransitionKind : uint8_t {
    Unknown,
    PropertyAddition,
    PropertyDeletion,
    PropertyAttributeChange,

    // Support for transitions not related to properties.
    // If any of these are used, the string portion of the key should be 0.
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
    Freeze,

    // Support for transitions related with private brand
    SetBrand
};

static constexpr auto FirstNonPropertyTransitionKind = TransitionKind::AllocateUndecided;

inline bool changesIndexingType(TransitionKind transition)
{
    switch (transition) {
    case TransitionKind::AllocateUndecided:
    case TransitionKind::AllocateInt32:
    case TransitionKind::AllocateDouble:
    case TransitionKind::AllocateContiguous:
    case TransitionKind::AllocateArrayStorage:
    case TransitionKind::AllocateSlowPutArrayStorage:
    case TransitionKind::SwitchToSlowPutArrayStorage:
    case TransitionKind::AddIndexedAccessors:
        return true;
    default:
        return false;
    }
}

inline IndexingType newIndexingType(IndexingType oldType, TransitionKind transition)
{
    switch (transition) {
    case TransitionKind::AllocateUndecided:
        ASSERT(!hasIndexedProperties(oldType));
        return oldType | UndecidedShape;
    case TransitionKind::AllocateInt32:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || oldType == CopyOnWriteArrayWithInt32);
        return (oldType & ~IndexingShapeAndWritabilityMask) | Int32Shape;
    case TransitionKind::AllocateDouble:
        ASSERT(Options::allowDoubleShape());
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || hasInt32(oldType) || oldType == CopyOnWriteArrayWithDouble);
        return (oldType & ~IndexingShapeAndWritabilityMask) | DoubleShape;
    case TransitionKind::AllocateContiguous:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || hasInt32(oldType) || hasDouble(oldType) || oldType == CopyOnWriteArrayWithContiguous);
        return (oldType & ~IndexingShapeAndWritabilityMask) | ContiguousShape;
    case TransitionKind::AllocateArrayStorage:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || hasInt32(oldType) || hasDouble(oldType) || hasContiguous(oldType));
        return (oldType & ~IndexingShapeAndWritabilityMask) | ArrayStorageShape;
    case TransitionKind::AllocateSlowPutArrayStorage:
        ASSERT(!hasIndexedProperties(oldType) || hasUndecided(oldType) || hasInt32(oldType) || hasDouble(oldType) || hasContiguous(oldType));
        return (oldType & ~IndexingShapeAndWritabilityMask) | SlowPutArrayStorageShape;
    case TransitionKind::SwitchToSlowPutArrayStorage:
        ASSERT(hasArrayStorage(oldType));
        return (oldType & ~IndexingShapeAndWritabilityMask) | SlowPutArrayStorageShape;
    case TransitionKind::AddIndexedAccessors:
        return oldType | MayHaveIndexedAccessors;
    default:
        return oldType;
    }
}

inline bool preventsExtensions(TransitionKind transition)
{
    switch (transition) {
    case TransitionKind::PreventExtensions:
    case TransitionKind::Seal:
    case TransitionKind::Freeze:
        return true;
    default:
        return false;
    }
}

inline bool setsDontDeleteOnAllProperties(TransitionKind transition)
{
    switch (transition) {
    case TransitionKind::Seal:
    case TransitionKind::Freeze:
        return true;
    default:
        return false;
    }
}

inline bool setsReadOnlyOnNonAccessorProperties(TransitionKind transition)
{
    switch (transition) {
    case TransitionKind::Freeze:
        return true;
    default:
        return false;
    }
}

class StructureTransitionTable {
    static constexpr intptr_t UsingSingleSlotFlag = 1;

    
#if CPU(ADDRESS64)
    struct Hash {
        // Logically, Key is a tuple of (1) UniquedStringImpl*, (2) unsigned attributes, and (3) transitionKind.
        struct Key {
            friend struct Hash;
            static_assert(WTF_OS_CONSTANT_EFFECTIVE_ADDRESS_WIDTH <= 48);
            static constexpr unsigned attributesShift = 48;
            static constexpr unsigned transitionKindShift = 56;
            static constexpr uintptr_t stringMask = (1ULL << attributesShift) - 1;
            static constexpr uintptr_t hashTableDeletedValue = 0x2;
            static_assert(sizeof(TransitionPropertyAttributes) * 8 <= 8);
            static_assert(sizeof(TransitionKind) * 8 <= 8);
            static_assert(hashTableDeletedValue < alignof(UniquedStringImpl));

            // Highest 8 bits are for TransitionKind; next 8 belong to TransitionPropertyAttributes.
            // Remaining bits are for UniquedStringImpl*.
            Key(UniquedStringImpl* impl, unsigned attributes, TransitionKind transitionKind)
                : m_encodedData(bitwise_cast<uintptr_t>(impl) | (static_cast<uintptr_t>(attributes) << attributesShift) | (static_cast<uintptr_t>(transitionKind) << transitionKindShift))
            {
                ASSERT(impl == this->impl());
                ASSERT(attributes <= UINT8_MAX);
                ASSERT(attributes == this->attributes());
                ASSERT(transitionKind != TransitionKind::Unknown);
                ASSERT(transitionKind == this->transitionKind());
            }

            Key() = default;

            Key(WTF::HashTableDeletedValueType)
                : m_encodedData(hashTableDeletedValue)
            { }

            bool isHashTableDeletedValue() const { return m_encodedData == hashTableDeletedValue; }

            UniquedStringImpl* impl() const { return bitwise_cast<UniquedStringImpl*>(m_encodedData & stringMask); }
            TransitionPropertyAttributes attributes() const { return (m_encodedData >> attributesShift) & UINT8_MAX; }
            TransitionKind transitionKind() const { return static_cast<TransitionKind>(m_encodedData >> transitionKindShift); }

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
        using Key = std::tuple<UniquedStringImpl*, unsigned, TransitionKind>;
        using KeyTraits = HashTraits<Key>;
        
        static unsigned hash(const Key& p)
        {
            return PtrHash<UniquedStringImpl*>::hash(std::get<0>(p)) + std::get<1>(p) + static_cast<unsigned>(std::get<2>(p));
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
    bool contains(UniquedStringImpl*, unsigned attributes, TransitionKind) const;
    Structure* get(UniquedStringImpl*, unsigned attributes, TransitionKind) const;

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
