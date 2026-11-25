/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/TypeCasts.h>
#include <wtf/text/UniquedStringImpl.h>

namespace WTF {

class AtomStringTable;

class AtomStringImpl final : public UniquedStringImpl {
public:
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> lookUp(std::span<const Latin1Character>);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> lookUp(std::span<const char16_t>);
    static RefPtr<AtomStringImpl> lookUp(StringImpl*);

    static void remove(AtomStringImpl*);

    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(std::span<const Latin1Character>);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(std::span<const char16_t>);
    ALWAYS_INLINE static RefPtr<AtomStringImpl> add(std::span<const char> characters);

    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(HashTranslatorCharBuffer<Latin1Character>&);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(HashTranslatorCharBuffer<char16_t>&);

    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(StringImpl*, unsigned offset, unsigned length);
    ALWAYS_INLINE static RefPtr<AtomStringImpl> add(StringImpl*);
    ALWAYS_INLINE static RefPtr<AtomStringImpl> add(RefPtr<StringImpl>&&);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(const StaticStringImpl&);
    ALWAYS_INLINE static Ref<AtomStringImpl> add(ASCIILiteral);

    // Returns null if the input data contains an invalid UTF-8 sequence.
    static RefPtr<AtomStringImpl> add(std::span<const char8_t>);

    // Same semantics as the AtomString static functions of the same names.
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> number(int);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> number(unsigned);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> number(unsigned long);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> number(unsigned long long);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> number(float);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> number(double);

    // Null input results in null, empty input in the empty atom, invalid UTF-8 in null.
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> fromUTF8(std::span<const char>);
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> fromUTF8(const char*);

#if USE(CF)
    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> add(CFStringRef);
#endif

    template<typename StringTableProvider>
    ALWAYS_INLINE static RefPtr<AtomStringImpl> addWithStringTableProvider(StringTableProvider&, StringImpl*);

#if ASSERT_ENABLED
    WTF_EXPORT_PRIVATE static bool isInAtomStringTable(StringImpl*);
#endif

private:
    AtomStringImpl() = delete;
    ~AtomStringImpl();

    ALWAYS_INLINE static Ref<AtomStringImpl> add(StringImpl&);
    ALWAYS_INLINE static Ref<AtomStringImpl> add(Ref<StringImpl>&&);
    WTF_EXPORT_PRIVATE static Ref<AtomStringImpl> addLiteral(std::span<const Latin1Character>);

    ALWAYS_INLINE static Ref<AtomStringImpl> add(AtomStringTable&, StringImpl&);

    WTF_EXPORT_PRIVATE static Ref<AtomStringImpl> addSlowCase(StringImpl&);
    WTF_EXPORT_PRIVATE static Ref<AtomStringImpl> addSlowCase(Ref<StringImpl>&&);
    WTF_EXPORT_PRIVATE static Ref<AtomStringImpl> addSlowCase(AtomStringTable&, StringImpl&);

    WTF_EXPORT_PRIVATE static RefPtr<AtomStringImpl> lookUpSlowCase(StringImpl&);
} DERIVED_CLASS_SWIFT_SHARED_REFERENCE(.ref, .deref);

inline RefPtr<AtomStringImpl> AtomStringImpl::lookUp(StringImpl* string)
{
    if (!string)
        return nullptr;
    if (auto* atom = dynamicDowncast<AtomStringImpl>(*string))
        return atom;
    return lookUpSlowCase(*string);
}

ALWAYS_INLINE RefPtr<AtomStringImpl> AtomStringImpl::add(std::span<const char> characters)
{
    return add(byteCast<Latin1Character>(characters));
}

ALWAYS_INLINE RefPtr<AtomStringImpl> AtomStringImpl::add(StringImpl* string)
{
    if (!string)
        return nullptr;
    return add(*string);
}

ALWAYS_INLINE RefPtr<AtomStringImpl> AtomStringImpl::add(RefPtr<StringImpl>&& string)
{
    if (!string)
        return nullptr;
    return add(string.releaseNonNull());
}

ALWAYS_INLINE Ref<AtomStringImpl> AtomStringImpl::add(ASCIILiteral literal)
{
    return addLiteral(literal.span8());
}

template<typename StringTableProvider>
ALWAYS_INLINE RefPtr<AtomStringImpl> AtomStringImpl::addWithStringTableProvider(StringTableProvider& stringTableProvider, StringImpl* string)
{
    if (!string)
        return nullptr;
    return add(*stringTableProvider.atomStringTable(), *string);
}

ALWAYS_INLINE Ref<AtomStringImpl> AtomStringImpl::add(StringImpl& string)
{
    if (auto* atom = dynamicDowncast<AtomStringImpl>(string)) {
        ASSERT_WITH_MESSAGE(!string.length() || isInAtomStringTable(&string), "The atom string comes from an other thread!");
        return *atom;
    }
    return addSlowCase(string);
}

ALWAYS_INLINE Ref<AtomStringImpl> AtomStringImpl::add(Ref<StringImpl>&& string)
{
    if (is<AtomStringImpl>(string)) {
        ASSERT_WITH_MESSAGE(!string->length() || isInAtomStringTable(string.ptr()), "The atom string comes from an other thread!");
        return uncheckedDowncast<AtomStringImpl>(WTF::move(string));
    }
    return addSlowCase(WTF::move(string));
}

ALWAYS_INLINE Ref<AtomStringImpl> AtomStringImpl::add(AtomStringTable& stringTable, StringImpl& string)
{
    if (auto* atom = dynamicDowncast<AtomStringImpl>(string)) {
        ASSERT_WITH_MESSAGE(!string.length() || isInAtomStringTable(&string), "The atom string comes from an other thread!");
        return *atom;
    }
    return addSlowCase(stringTable, string);
}

#if ASSERT_ENABLED

// AtomStringImpls created from StaticStringImpl will ASSERT in the generic ValueCheck<T>::checkConsistency,
// as they are not allocated by fastMalloc. We don't currently have any way to detect that case, so we don't
// do any consistency check for AtomStringImpl*.

template<> struct ValueCheck<AtomStringImpl*> {
    static void checkConsistency(const AtomStringImpl*) { }
};

template<> struct ValueCheck<const AtomStringImpl*> {
    static void checkConsistency(const AtomStringImpl*) { }
};

#endif // ASSERT_ENABLED

inline void printInternal(PrintStream& out, const AtomStringImpl* value) { printInternal(out, std::bit_cast<const StringImpl*>(value)); }
inline void printInternal(PrintStream& out, AtomStringImpl* value) { printInternal(out, static_cast<const AtomStringImpl*>(value)); }

#if WTF_STRINGIMPL_HASHTRAITS_OWNING_PEEK
inline RefPtr<AtomStringImpl> HashTraits<RefPtr<AtomStringImpl>>::peek(const RefPtr<AtomStringImpl>& value) { return value; }
inline RefPtr<AtomStringImpl> HashTraits<RefPtr<AtomStringImpl>>::peek(AtomStringImpl* value) { return value; }
#endif

// Hash for atoms: identity equality with the content-derived hash that atoms always have,
// like AtomStringHash. This keeps iteration order deterministic and allows RobinHood tables.
struct AtomStringImplHash {
    static unsigned hash(const AtomStringImpl* key) { return key->existingHash(); }
    static unsigned hash(const RefPtr<AtomStringImpl>& key) { return key->existingHash(); }
    static unsigned hash(const Ref<AtomStringImpl>& key) { return key->existingHash(); }

    static bool equal(const AtomStringImpl* a, const AtomStringImpl* b) { return a == b; }
    static bool equal(const RefPtr<AtomStringImpl>& a, const RefPtr<AtomStringImpl>& b) { return a.get() == b.get(); }
    static bool equal(const RefPtr<AtomStringImpl>& a, const AtomStringImpl* b) { return a.get() == b; }
    static bool equal(const AtomStringImpl* a, const RefPtr<AtomStringImpl>& b) { return a == b.get(); }
    static bool equal(const Ref<AtomStringImpl>& a, const Ref<AtomStringImpl>& b) { return a.ptrAllowingHashTableEmptyValue() == b.ptrAllowingHashTableEmptyValue(); }
    static bool equal(const Ref<AtomStringImpl>& a, const AtomStringImpl* b) { return a.ptrAllowingHashTableEmptyValue() == b; }
    static bool equal(const AtomStringImpl* a, const Ref<AtomStringImpl>& b) { return a == b.ptrAllowingHashTableEmptyValue(); }

    // Equality is a pointer comparison, so comparing to the empty and deleted values is safe.
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
    static constexpr bool hasHashInValue = true;
};

template<typename> struct DefaultHash;
template<> struct DefaultHash<RefPtr<AtomStringImpl>> : AtomStringImplHash { };
template<> struct DefaultHash<Ref<AtomStringImpl>> : AtomStringImplHash { };

inline namespace StringLiterals {

// "foo"_atom is a RefPtr<AtomStringImpl>; same semantics and cost as AtomString("foo"_s).
inline RefPtr<AtomStringImpl> operator""_atom(const char* characters, size_t)
{
    return static_cast<RefPtr<AtomStringImpl>>(ASCIILiteral::fromLiteralUnsafe(characters));
}

} // inline StringLiterals

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_BEGIN(WTF::AtomStringImpl) \
    static bool isType(const WTF::StringImpl& string) { return string.isAtom(); } \
SPECIALIZE_TYPE_TRAITS_END()

namespace WTF {

// Same semantics as AtomString(ASCIILiteral): null literal results in null, empty literal in the empty atom.
inline ASCIILiteral::operator RefPtr<AtomStringImpl>() const
{
    if (isNull())
        return nullptr;
    if (!length())
        return uncheckedDowncast<AtomStringImpl>(StringImpl::empty());
    return AtomStringImpl::add(*this);
}

} // namespace WTF

using WTF::AtomStringImpl;
