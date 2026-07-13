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

#include <JavaScriptCore/CommonIdentifiers.h>
#include <JavaScriptCore/Identifier.h>
#include <JavaScriptCore/MathCommon.h>
#include <array>
#include <type_traits>
#include <wtf/Ref.h>
#include <wtf/SegmentedVector.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

    class ParserArenaDeletable;

    DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(IdentifierArena);
    class IdentifierArena {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(IdentifierArena, IdentifierArena);
    public:
        IdentifierArena()
        {
            clear();
        }

        template <typename T>
        ALWAYS_INLINE UniquedStringImpl* makeIdentifier(VM&, std::span<const T> characters);
        ALWAYS_INLINE UniquedStringImpl* makeEmptyIdentifier(VM&);
        ALWAYS_INLINE UniquedStringImpl* makeLatin1Identifier(VM&, std::span<const char16_t> characters);
        ALWAYS_INLINE UniquedStringImpl* makeIdentifier(VM&, SymbolImpl*);

        UniquedStringImpl* makeBigIntDecimalIdentifier(VM&, UniquedStringImpl*, uint8_t radix);
        UniquedStringImpl* makeNumericIdentifier(VM&, double number);
        UniquedStringImpl* makePrivateIdentifier(VM&, ASCIILiteral, unsigned);

    public:
        static const int MaximumCachableCharacter = 128;
        typedef SegmentedVector<Ref<UniquedStringImpl>, 64> IdentifierVector;
        void clear()
        {
            m_identifiers.clear();
            for (int i = 0; i < MaximumCachableCharacter; i++)
                m_shortIdentifiers[i] = nullptr;
            for (int i = 0; i < MaximumCachableCharacter; i++)
                m_recentIdentifiers[i] = nullptr;
        }

    private:
        IdentifierVector m_identifiers;
        std::array<UniquedStringImpl*, MaximumCachableCharacter> m_shortIdentifiers { };
        std::array<UniquedStringImpl*, MaximumCachableCharacter> m_recentIdentifiers { };
    };

    template <typename T>
    ALWAYS_INLINE UniquedStringImpl* IdentifierArena::makeIdentifier(VM& vm, std::span<const T> characters)
    {
        if (characters.empty())
            return vm.propertyNames->emptyIdentifier.impl();
        if (characters.front() >= MaximumCachableCharacter) {
            m_identifiers.append(Ref { *Identifier::fromString(vm, characters).impl() });
            return m_identifiers.last().ptr();
        }
        if (characters.size() == 1) {
            if (SUPPRESS_UNCOUNTED_LOCAL UniquedStringImpl* impl = m_shortIdentifiers[characters.front()])
                return impl;
            m_identifiers.append(Ref { *Identifier::fromString(vm, characters).impl() });
            m_shortIdentifiers[characters.front()] = m_identifiers.last().ptr();
            return m_identifiers.last().ptr();
        }
        SUPPRESS_UNCOUNTED_LOCAL UniquedStringImpl* impl = m_recentIdentifiers[characters.front()];
        if (impl && Identifier::equal(impl, characters))
            return impl;
        m_identifiers.append(Ref { *Identifier::fromString(vm, characters).impl() });
        m_recentIdentifiers[characters.front()] = m_identifiers.last().ptr();
        return m_identifiers.last().ptr();
    }

    ALWAYS_INLINE UniquedStringImpl* IdentifierArena::makeIdentifier(VM&, SymbolImpl* symbol)
    {
        ASSERT(symbol);
        m_identifiers.append(Ref { *symbol });
        return symbol;
    }

    ALWAYS_INLINE UniquedStringImpl* IdentifierArena::makeEmptyIdentifier(VM& vm)
    {
        return vm.propertyNames->emptyIdentifier.impl();
    }

    ALWAYS_INLINE UniquedStringImpl* IdentifierArena::makeLatin1Identifier(VM& vm, std::span<const char16_t> characters)
    {
        if (characters.empty())
            return vm.propertyNames->emptyIdentifier.impl();
        if (characters.front() >= MaximumCachableCharacter) {
            m_identifiers.append(Ref { *Identifier::createLatin1(vm, characters).impl() });
            return m_identifiers.last().ptr();
        }
        if (characters.size() == 1) {
            if (UniquedStringImpl* impl = m_shortIdentifiers[characters.front()])
                return impl;
            m_identifiers.append(Ref { *Identifier::fromString(vm, characters).impl() });
            m_shortIdentifiers[characters.front()] = m_identifiers.last().ptr();
            return m_identifiers.last().ptr();
        }
        SUPPRESS_UNCOUNTED_LOCAL UniquedStringImpl* impl = m_recentIdentifiers[characters.front()];
        if (impl && Identifier::equal(impl, characters))
            return impl;
        m_identifiers.append(Ref { *Identifier::createLatin1(vm, characters).impl() });
        m_recentIdentifiers[characters.front()] = m_identifiers.last().ptr();
        return m_identifiers.last().ptr();
    }

    inline UniquedStringImpl* IdentifierArena::makeNumericIdentifier(VM& vm, double number)
    {
        Identifier token;
        if (auto int32Value = tryConvertToStrictInt32(number))
            token = Identifier::from(vm, int32Value.value());
        else
            token = Identifier::from(vm, number);
        m_identifiers.append(Ref { *token.impl() });
        return m_identifiers.last().ptr();
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
            if (static_cast<size_t>(m_freeablePoolEnd - m_freeableMemory) < alignedSize) [[unlikely]]
                allocateFreeablePool();
            void* block = m_freeableMemory;
            m_freeableMemory += alignedSize;
            return block;
        }

        template<std::derived_from<ParserArenaDeletable> T>
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
            if (!m_identifierArena) [[unlikely]]
                m_identifierArena = makeUnique<IdentifierArena>();
            return *m_identifierArena;
        }

    private:
        static const size_t freeablePoolSize = 8000;

        static size_t alignSize(size_t size)
        {
            return (size + sizeof(WTF::AllocAlignmentInteger) - 1) & ~(sizeof(WTF::AllocAlignmentInteger) - 1);
        }

        void* NODELETE freeablePool();
        void allocateFreeablePool();
        void deallocateObjects();

        char* m_freeableMemory;
        char* m_freeablePoolEnd;

        std::unique_ptr<IdentifierArena> m_identifierArena;
        Vector<void*> m_freeablePools;
        Vector<ParserArenaDeletable*> m_deletableObjects;
    };

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
