/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

#include "CommonIdentifiers.h"
#include "Identifier.h"
#include <array>
#include <type_traits>
#include <wtf/SegmentedVector.h>

namespace JSC {

    class ParserArenaDeletable;

    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(IdentifierArena);
    class IdentifierArena {
        WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(IdentifierArena);
    public:
        IdentifierArena()
        {
            clear();
        }

        template <typename T>
        ALWAYS_INLINE const Identifier& makeIdentifier(VM&, const T* characters, size_t length);
        ALWAYS_INLINE const Identifier& makeEmptyIdentifier(VM&);
        ALWAYS_INLINE const Identifier& makeIdentifierLCharFromUChar(VM&, const UChar* characters, size_t length);
        ALWAYS_INLINE const Identifier& makeIdentifier(VM&, SymbolImpl*);

        const Identifier& makeBigIntDecimalIdentifier(VM&, const Identifier&, uint8_t radix);
        const Identifier& makeNumericIdentifier(VM&, double number);

    public:
        static const int MaximumCachableCharacter = 128;
        typedef SegmentedVector<Identifier, 64> IdentifierVector;
        void clear()
        {
            m_identifiers.clear();
            for (int i = 0; i < MaximumCachableCharacter; i++)
                m_shortIdentifiers[i] = 0;
            for (int i = 0; i < MaximumCachableCharacter; i++)
                m_recentIdentifiers[i] = 0;
        }

    private:
        IdentifierVector m_identifiers;
        std::array<Identifier*, MaximumCachableCharacter> m_shortIdentifiers;
        std::array<Identifier*, MaximumCachableCharacter> m_recentIdentifiers;
    };

    template <typename T>
    ALWAYS_INLINE const Identifier& IdentifierArena::makeIdentifier(VM& vm, const T* characters, size_t length)
    {
        if (!length)
            return vm.propertyNames->emptyIdentifier;
        if (characters[0] >= MaximumCachableCharacter) {
            m_identifiers.append(Identifier::fromString(vm, characters, length));
            return m_identifiers.last();
        }
        if (length == 1) {
            if (Identifier* ident = m_shortIdentifiers[characters[0]])
                return *ident;
            m_identifiers.append(Identifier::fromString(vm, characters, length));
            m_shortIdentifiers[characters[0]] = &m_identifiers.last();
            return m_identifiers.last();
        }
        Identifier* ident = m_recentIdentifiers[characters[0]];
        if (ident && Identifier::equal(ident->impl(), characters, length))
            return *ident;
        m_identifiers.append(Identifier::fromString(vm, characters, length));
        m_recentIdentifiers[characters[0]] = &m_identifiers.last();
        return m_identifiers.last();
    }

    ALWAYS_INLINE const Identifier& IdentifierArena::makeIdentifier(VM&, SymbolImpl* symbol)
    {
        ASSERT(symbol);
        m_identifiers.append(Identifier::fromUid(*symbol));
        return m_identifiers.last();
    }

    ALWAYS_INLINE const Identifier& IdentifierArena::makeEmptyIdentifier(VM& vm)
    {
        return vm.propertyNames->emptyIdentifier;
    }

    ALWAYS_INLINE const Identifier& IdentifierArena::makeIdentifierLCharFromUChar(VM& vm, const UChar* characters, size_t length)
    {
        if (!length)
            return vm.propertyNames->emptyIdentifier;
        if (characters[0] >= MaximumCachableCharacter) {
            m_identifiers.append(Identifier::createLCharFromUChar(vm, characters, length));
            return m_identifiers.last();
        }
        if (length == 1) {
            if (Identifier* ident = m_shortIdentifiers[characters[0]])
                return *ident;
            m_identifiers.append(Identifier::fromString(vm, characters, length));
            m_shortIdentifiers[characters[0]] = &m_identifiers.last();
            return m_identifiers.last();
        }
        Identifier* ident = m_recentIdentifiers[characters[0]];
        if (ident && Identifier::equal(ident->impl(), characters, length))
            return *ident;
        m_identifiers.append(Identifier::createLCharFromUChar(vm, characters, length));
        m_recentIdentifiers[characters[0]] = &m_identifiers.last();
        return m_identifiers.last();
    }
    
    inline const Identifier& IdentifierArena::makeNumericIdentifier(VM& vm, double number)
    {
        // FIXME: Why doesn't this use the Identifier::from overload that takes a double?
        // Seems we are missing out on multiple optimizations by not using it.
        m_identifiers.append(Identifier::fromString(vm, String::number(number)));
        return m_identifiers.last();
    }

    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ParserArena);

    class ParserArena {
        WTF_MAKE_NONCOPYABLE(ParserArena);
    public:
        ParserArena();
        ~ParserArena();

        void swap(ParserArena& otherArena)
        {
            std::swap(m_freeableMemory, otherArena.m_freeableMemory);
            std::swap(m_freeablePoolEnd, otherArena.m_freeablePoolEnd);
            m_identifierArena.swap(otherArena.m_identifierArena);
            m_freeablePools.swap(otherArena.m_freeablePools);
            m_deletableObjects.swap(otherArena.m_deletableObjects);
        }

        void* allocateFreeable(size_t size)
        {
            ASSERT(size);
            ASSERT(size <= freeablePoolSize);
            size_t alignedSize = alignSize(size);
            ASSERT(alignedSize <= freeablePoolSize);
            if (UNLIKELY(static_cast<size_t>(m_freeablePoolEnd - m_freeableMemory) < alignedSize))
                allocateFreeablePool();
            void* block = m_freeableMemory;
            m_freeableMemory += alignedSize;
            return block;
        }

        template<typename T, typename = std::enable_if_t<std::is_base_of<ParserArenaDeletable, T>::value>>
        void* allocateDeletable(size_t size)
        {
            // T may extend ParserArenaDeletable via multiple inheritance, but not as T's first
            // base class. m_deletableObjects is expecting pointers to objects of the shape of
            // ParserArenaDeletable. We ensure this by allocating T, and casting it to
            // ParserArenaDeletable to get the correct pointer to append to m_deletableObjects.
            T* instance = static_cast<T*>(allocateFreeable(size));
            ParserArenaDeletable* deletable = static_cast<ParserArenaDeletable*>(instance);
            m_deletableObjects.append(deletable);
            return instance;
        }

        IdentifierArena& identifierArena()
        {
            if (UNLIKELY (!m_identifierArena))
                m_identifierArena = makeUnique<IdentifierArena>();
            return *m_identifierArena;
        }

    private:
        static const size_t freeablePoolSize = 8000;

        static size_t alignSize(size_t size)
        {
            return (size + sizeof(WTF::AllocAlignmentInteger) - 1) & ~(sizeof(WTF::AllocAlignmentInteger) - 1);
        }

        void* freeablePool();
        void allocateFreeablePool();
        void deallocateObjects();

        char* m_freeableMemory;
        char* m_freeablePoolEnd;

        std::unique_ptr<IdentifierArena> m_identifierArena;
        Vector<void*> m_freeablePools;
        Vector<ParserArenaDeletable*> m_deletableObjects;
    };

} // namespace JSC
