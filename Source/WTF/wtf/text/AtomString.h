/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#include <utility>
#include <wtf/text/AtomStringImpl.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class AtomString final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(AtomString);
public:
    AtomString();
    AtomString(std::span<const LChar>);
    AtomString(std::span<const char16_t>);

    AtomString(AtomStringImpl*);
    AtomString(RefPtr<AtomStringImpl>&&);
    AtomString(Ref<AtomStringImpl>&&);
    AtomString(const StaticStringImpl*);
    AtomString(StringImpl*);
    explicit AtomString(const String&);
    explicit AtomString(String&&);
    AtomString(StringImpl* baseString, unsigned start, unsigned length);

    // FIXME: AtomString doesn’t always have AtomStringImpl, so one of those two names needs to change.
    AtomString(UniquedStringImpl* uid);

    AtomString(ASCIILiteral);

    static AtomString lookUp(std::span<const char16_t> characters) { return AtomStringImpl::lookUp(characters); }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    AtomString(WTF::HashTableDeletedValueType) : m_string(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_string.isHashTableDeletedValue(); }

    unsigned existingHash() const { return isNull() ? 0 : impl()->existingHash(); }

    operator const String&() const { return m_string; }
    const String& string() const { return m_string; }
    String releaseString() { return WTFMove(m_string); }

    // FIXME: What guarantees this isn't a SymbolImpl rather than an AtomStringImpl?
    AtomStringImpl* impl() const LIFETIME_BOUND { SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<AtomStringImpl*>(m_string.impl()); }
    RefPtr<AtomStringImpl> releaseImpl() { return static_pointer_cast<AtomStringImpl>(m_string.releaseImpl()); }

    bool is8Bit() const { return m_string.is8Bit(); }
    std::span<const LChar> span8() const LIFETIME_BOUND { return m_string.span8(); }
    std::span<const char16_t> span16() const LIFETIME_BOUND { return m_string.span16(); }
    unsigned length() const { return m_string.length(); }

    char16_t operator[](unsigned int i) const { return m_string[i]; }

    WTF_EXPORT_PRIVATE static AtomString number(int);
    WTF_EXPORT_PRIVATE static AtomString number(unsigned);
    WTF_EXPORT_PRIVATE static AtomString number(unsigned long);
    WTF_EXPORT_PRIVATE static AtomString number(unsigned long long);
    WTF_EXPORT_PRIVATE static AtomString number(float);
    WTF_EXPORT_PRIVATE static AtomString number(double);
    // If we need more overloads of the number function, we can add all the others that String has, but these seem to do for now.

    bool contains(char16_t character) const { return m_string.contains(character); }
    bool contains(ASCIILiteral literal) const { return m_string.contains(literal); }
    bool contains(StringView) const;
    bool containsIgnoringASCIICase(StringView) const;

    size_t find(char16_t character, size_t start = 0) const { return m_string.find(character, start); }
    size_t find(ASCIILiteral literal, size_t start = 0) const { return m_string.find(literal, start); }
    size_t find(StringView, size_t start = 0) const;
    size_t findIgnoringASCIICase(StringView) const;
    size_t findIgnoringASCIICase(StringView, size_t start) const;
    size_t find(CodeUnitMatchFunction matchFunction, size_t start = 0) const { return m_string.find(matchFunction, start); }

    bool startsWith(StringView) const;
    bool startsWithIgnoringASCIICase(StringView) const;
    bool startsWith(char16_t character) const { return m_string.startsWith(character); }

    bool endsWith(StringView) const;
    bool endsWithIgnoringASCIICase(StringView) const;
    bool endsWith(char16_t character) const { return m_string.endsWith(character); }

    WTF_EXPORT_PRIVATE AtomString convertToASCIILowercase() const;
    WTF_EXPORT_PRIVATE AtomString convertToASCIIUppercase() const;

    double toDouble(bool* ok = nullptr) const { return m_string.toDouble(ok); }
    float toFloat(bool* ok = nullptr) const { return m_string.toFloat(ok); }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

#if USE(CF)
    AtomString(CFStringRef);
#endif

#if USE(FOUNDATION) && defined(__OBJC__)
    AtomString(NSString *);
    RetainPtr<NSString> createNSString() const { return m_string.createNSString(); }
#endif

#if OS(WINDOWS)
    AtomString(const wchar_t* characters, unsigned length)
        : AtomString({ ucharFrom(characters), length }) { }

    AtomString(const wchar_t* characters)
        : AtomString(characters, characters ? wcslen(characters) : 0) { }
#endif

    // AtomString::fromUTF8 will return a null string if the input data contains invalid UTF-8 sequences.
    static AtomString fromUTF8(std::span<const char>);
    static AtomString fromUTF8(const char*);

#ifndef NDEBUG
    void show() const;
#endif

private:
    explicit AtomString(const char*);

    enum class CaseConvertType { Upper, Lower };
    template<CaseConvertType> AtomString convertASCIICase() const;

    WTF_EXPORT_PRIVATE static AtomString fromUTF8Internal(std::span<const char>);

    String m_string;
};

static_assert(sizeof(AtomString) == sizeof(String), "AtomString and String must be the same size!");

inline bool operator==(const AtomString& a, const AtomString& b) { return a.impl() == b.impl(); }
inline bool operator==(const AtomString& a, ASCIILiteral b) { return WTF::equal(a.impl(), b); }
inline bool operator==(const AtomString& a, const Vector<char16_t>& b) { return a.impl() && equal(a.impl(), b.span()); }
inline bool operator==(const AtomString& a, const String& b) { return equal(a.impl(), b.impl()); }

bool equalIgnoringASCIICase(const AtomString&, const AtomString&);
bool equalIgnoringASCIICase(const AtomString&, const String&);
bool equalIgnoringASCIICase(const String&, const AtomString&);
bool equalIgnoringASCIICase(const AtomString&, ASCIILiteral);

bool equalLettersIgnoringASCIICase(const AtomString&, ASCIILiteral);
bool startsWithLettersIgnoringASCIICase(const AtomString&, ASCIILiteral);

WTF_EXPORT_PRIVATE AtomString replaceUnpairedSurrogatesWithReplacementCharacter(AtomString&&);
WTF_EXPORT_PRIVATE String replaceUnpairedSurrogatesWithReplacementCharacter(String&&);

inline AtomString::AtomString()
{
}

inline AtomString::AtomString(std::span<const LChar> string)
    : m_string(AtomStringImpl::add(string))
{
}

inline AtomString::AtomString(std::span<const char16_t> string)
    : m_string(AtomStringImpl::add(string))
{
}

inline AtomString::AtomString(AtomStringImpl* string)
    : m_string(string)
{
}

inline AtomString::AtomString(RefPtr<AtomStringImpl>&& string)
    : m_string(WTFMove(string))
{
}

inline AtomString::AtomString(Ref<AtomStringImpl>&& string)
    : m_string(WTFMove(string))
{
}

inline AtomString::AtomString(StringImpl* string)
    : m_string(AtomStringImpl::add(string))
{
}

inline AtomString::AtomString(const StaticStringImpl* string)
    : m_string(AtomStringImpl::add(string))
{
}

inline AtomString::AtomString(const String& string)
    : m_string(AtomStringImpl::add(string.impl()))
{
}

inline AtomString::AtomString(String&& string)
    : m_string(AtomStringImpl::add(string.releaseImpl()))
{
}

inline AtomString::AtomString(StringImpl* baseString, unsigned start, unsigned length)
    : m_string(AtomStringImpl::add(baseString, start, length))
{
}

inline AtomString::AtomString(UniquedStringImpl* uid)
    : m_string(uid)
{
}

#if USE(CF)

inline AtomString::AtomString(CFStringRef string)
    :  m_string(AtomStringImpl::add(string))
{
}

#endif

#if USE(FOUNDATION) && defined(__OBJC__)

inline AtomString::AtomString(NSString *string)
    : m_string(AtomStringImpl::add((__bridge CFStringRef)string))
{
}

#endif

struct StaticAtomString {
    constexpr StaticAtomString(StringImpl::StaticStringImpl* pointer)
        : m_pointer(pointer)
    {
    }

    StringImpl::StaticStringImpl* m_pointer;
};
static_assert(sizeof(AtomString) == sizeof(StaticAtomString), "AtomString and StaticAtomString must be the same size!");
extern WTF_EXPORT_PRIVATE const StaticAtomString nullAtomData;
extern WTF_EXPORT_PRIVATE const StaticAtomString emptyAtomData;

inline const AtomString& nullAtom() { SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const AtomString*>(&nullAtomData); }
inline const AtomString& emptyAtom() { SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const AtomString*>(&emptyAtomData); }

inline AtomString::AtomString(ASCIILiteral literal)
    : m_string(literal.length() ? AtomStringImpl::add(literal.span8()) : Ref { *emptyAtom().impl() })
{
}

inline AtomString AtomString::fromUTF8(std::span<const char> characters)
{
    if (!characters.data())
        return nullAtom();
    if (characters.empty())
        return emptyAtom();
    return fromUTF8Internal(characters);
}

inline AtomString AtomString::fromUTF8(const char* characters)
{
    if (!characters)
        return nullAtom();
    if (!*characters)
        return emptyAtom();
    return fromUTF8Internal(unsafeSpan(characters));
}

inline AtomString String::toExistingAtomString() const
{
    if (isNull())
        return { };
    if (impl()->isAtom())
        return Ref { static_cast<AtomStringImpl&>(*impl()) };
    return AtomStringImpl::lookUp(impl());
}

// AtomStringHash is the default hash for AtomString
template<typename> struct DefaultHash;
template<> struct DefaultHash<AtomString>;

inline bool equalLettersIgnoringASCIICase(const AtomString& string, ASCIILiteral literal)
{
    return equalLettersIgnoringASCIICase(string.string(), literal);
}

inline bool startsWithLettersIgnoringASCIICase(const AtomString& string, ASCIILiteral literal)
{
    return startsWithLettersIgnoringASCIICase(string.string(), literal);
}

inline bool equalIgnoringASCIICase(const AtomString& a, const AtomString& b)
{
    return equalIgnoringASCIICase(a.string(), b.string());
}

inline bool equalIgnoringASCIICase(const AtomString& a, const String& b)
{
    return equalIgnoringASCIICase(a.string(), b);
}

inline bool equalIgnoringASCIICase(const String& a, const AtomString& b)
{
    return equalIgnoringASCIICase(a, b.string());
}

inline bool equalIgnoringASCIICase(const AtomString& a, ASCIILiteral b)
{
    return equalIgnoringASCIICase(a.string(), b);
}

inline std::strong_ordering codePointCompare(const AtomString& a, const AtomString& b)
{
    return codePointCompare(a.string(), b.string());
}

ALWAYS_INLINE String WARN_UNUSED_RETURN makeStringByReplacingAll(const AtomString& string, char16_t target, char16_t replacement)
{
    return makeStringByReplacingAll(string.string(), target, replacement);
}

template<> struct IntegerToStringConversionTrait<AtomString> {
    using ReturnType = AtomString;
    using AdditionalArgumentType = void;
    static AtomString flush(std::span<const LChar> characters, void*) { return characters; }
};

} // namespace WTF

using WTF::AtomString;
using WTF::nullAtom;
using WTF::emptyAtom;

#include <wtf/text/StringConcatenate.h>
