/*
 * Copyright (C) 2015-2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#pragma once

#include <wtf/text/UniquedStringImpl.h>

namespace WTF {

class RegisteredSymbolImpl;

// SymbolImpl is used to represent the symbol string impl.
// It is uniqued string impl, but is not registered in Atomic String tables, so it's not atomic.
class SymbolImpl : public UniquedStringImpl {
public:
    using Flags = unsigned;
    static constexpr Flags s_flagDefault = 0u;
    static constexpr Flags s_flagIsNullSymbol = 0b001u;
    static constexpr Flags s_flagIsRegistered = 0b010u;
    static constexpr Flags s_flagIsPrivate = 0b100u;

    unsigned hashForSymbol() const { return m_hashForSymbol; }
    bool isNullSymbol() const { return m_flags & s_flagIsNullSymbol; }
    bool isRegistered() const { return m_flags & s_flagIsRegistered; }
    bool isPrivate() const { return m_flags & s_flagIsPrivate; }

    SymbolRegistry* symbolRegistry() const;

    RegisteredSymbolImpl* asRegisteredSymbolImpl();

    WTF_EXPORT_PRIVATE static Ref<SymbolImpl> createNullSymbol();
    WTF_EXPORT_PRIVATE static Ref<SymbolImpl> create(StringImpl& rep);

    class StaticSymbolImpl final : private StringImplShape {
        WTF_MAKE_NONCOPYABLE(StaticSymbolImpl);
    public:
        template<unsigned characterCount>
        constexpr StaticSymbolImpl(const char (&characters)[characterCount], Flags flags = s_flagDefault)
            : StringImplShape(s_refCountFlagIsStaticString, characterCount - 1, characters,
                s_hashFlag8BitBuffer | s_hashFlagDidReportCost | StringSymbol | BufferInternal | (StringHasher::computeLiteralHashAndMaskTop8Bits(characters) << s_flagCount), ConstructWithConstExpr)
            , m_hashForSymbol(StringHasher::computeLiteralHashAndMaskTop8Bits(characters) << s_flagCount)
            , m_flags(flags)
        {
        }

        template<unsigned characterCount>
        constexpr StaticSymbolImpl(const char16_t (&characters)[characterCount], Flags flags = s_flagDefault)
            : StringImplShape(s_refCountFlagIsStaticString, characterCount - 1, characters,
                s_hashFlagDidReportCost | StringSymbol | BufferInternal | (StringHasher::computeLiteralHashAndMaskTop8Bits(characters) << s_flagCount), ConstructWithConstExpr)
            , m_hashForSymbol(StringHasher::computeLiteralHashAndMaskTop8Bits(characters) << s_flagCount)
            , m_flags(flags)
        {
        }

        operator SymbolImpl&()
        {
            return *reinterpret_cast<SymbolImpl*>(this);
        }

        StringImpl* m_owner { nullptr }; // We do not make StaticSymbolImpl BufferSubstring. Thus we can make this nullptr.
        unsigned m_hashForSymbol;
        Flags m_flags;
    };

protected:
    WTF_EXPORT_PRIVATE static unsigned nextHashForSymbol();

    friend class StringImpl;

    SymbolImpl(const LChar* characters, unsigned length, Ref<StringImpl>&& base, Flags flags = s_flagDefault)
        : UniquedStringImpl(CreateSymbol, characters, length)
        , m_owner(&base.leakRef())
        , m_hashForSymbol(nextHashForSymbol())
        , m_flags(flags)
    {
        ASSERT(StringImpl::tailOffset<StringImpl*>() == OBJECT_OFFSETOF(SymbolImpl, m_owner));
    }

    SymbolImpl(const UChar* characters, unsigned length, Ref<StringImpl>&& base, Flags flags = s_flagDefault)
        : UniquedStringImpl(CreateSymbol, characters, length)
        , m_owner(&base.leakRef())
        , m_hashForSymbol(nextHashForSymbol())
        , m_flags(flags)
    {
        ASSERT(StringImpl::tailOffset<StringImpl*>() == OBJECT_OFFSETOF(SymbolImpl, m_owner));
    }

    SymbolImpl(Flags flags = s_flagDefault)
        : UniquedStringImpl(CreateSymbol)
        , m_owner(StringImpl::empty())
        , m_hashForSymbol(nextHashForSymbol())
        , m_flags(flags | s_flagIsNullSymbol)
    {
        ASSERT(StringImpl::tailOffset<StringImpl*>() == OBJECT_OFFSETOF(SymbolImpl, m_owner));
    }

    // The pointer to the owner string should be immediately following after the StringImpl layout,
    // since we would like to align the layout of SymbolImpl to the one of BufferSubstring StringImpl.
    StringImpl* m_owner;
    unsigned m_hashForSymbol;
    Flags m_flags { s_flagDefault };
};
static_assert(sizeof(SymbolImpl) == sizeof(SymbolImpl::StaticSymbolImpl), "");

class PrivateSymbolImpl final : public SymbolImpl {
public:
    WTF_EXPORT_PRIVATE static Ref<PrivateSymbolImpl> createNullSymbol();
    WTF_EXPORT_PRIVATE static Ref<PrivateSymbolImpl> create(StringImpl& rep);

private:
    PrivateSymbolImpl(const LChar* characters, unsigned length, Ref<StringImpl>&& base)
        : SymbolImpl(characters, length, WTFMove(base), s_flagIsPrivate)
    {
    }

    PrivateSymbolImpl(const UChar* characters, unsigned length, Ref<StringImpl>&& base)
        : SymbolImpl(characters, length, WTFMove(base), s_flagIsPrivate)
    {
    }

    PrivateSymbolImpl()
        : SymbolImpl(s_flagIsPrivate)
    {
    }
};

class RegisteredSymbolImpl final : public SymbolImpl {
private:
    friend class StringImpl;
    friend class SymbolImpl;
    friend class SymbolRegistry;

    SymbolRegistry* symbolRegistry() const { return m_symbolRegistry; }
    void clearSymbolRegistry() { m_symbolRegistry = nullptr; }

    static Ref<RegisteredSymbolImpl> create(StringImpl& rep, SymbolRegistry&);

    RegisteredSymbolImpl(const LChar* characters, unsigned length, Ref<StringImpl>&& base, SymbolRegistry& registry)
        : SymbolImpl(characters, length, WTFMove(base), s_flagIsRegistered)
        , m_symbolRegistry(&registry)
    {
    }

    RegisteredSymbolImpl(const UChar* characters, unsigned length, Ref<StringImpl>&& base, SymbolRegistry& registry)
        : SymbolImpl(characters, length, WTFMove(base), s_flagIsRegistered)
        , m_symbolRegistry(&registry)
    {
    }

    SymbolRegistry* m_symbolRegistry;
};

inline unsigned StringImpl::symbolAwareHash() const
{
    if (isSymbol())
        return static_cast<const SymbolImpl*>(this)->hashForSymbol();
    return hash();
}

inline unsigned StringImpl::existingSymbolAwareHash() const
{
    if (isSymbol())
        return static_cast<const SymbolImpl*>(this)->hashForSymbol();
    return existingHash();
}

inline SymbolRegistry* SymbolImpl::symbolRegistry() const
{
    if (isRegistered())
        return static_cast<const RegisteredSymbolImpl*>(this)->symbolRegistry();
    return nullptr;
}

inline RegisteredSymbolImpl* SymbolImpl::asRegisteredSymbolImpl()
{
    ASSERT(isRegistered());
    return static_cast<RegisteredSymbolImpl*>(this);
}

#if !ASSERT_DISABLED
// SymbolImpls created from StaticStringImpl will ASSERT
// in the generic ValueCheck<T>::checkConsistency
// as they are not allocated by fastMalloc.
// We don't currently have any way to detect that case
// so we ignore the consistency check for all SymbolImpls*.
template<> struct
ValueCheck<SymbolImpl*> {
    static void checkConsistency(const SymbolImpl*) { }
};

template<> struct
ValueCheck<const SymbolImpl*> {
    static void checkConsistency(const SymbolImpl*) { }
};
#endif

} // namespace WTF

using WTF::SymbolImpl;
using WTF::PrivateSymbolImpl;
using WTF::RegisteredSymbolImpl;
