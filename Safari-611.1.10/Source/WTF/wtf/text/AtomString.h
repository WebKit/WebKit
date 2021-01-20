/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomStringImpl.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
#include <wtf/text/win/WCharStringExtras.h>
#endif

namespace WTF {

class AtomString final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE static void init();

    AtomString();
    AtomString(const LChar*);
    AtomString(const char*);
    AtomString(const LChar*, unsigned length);
    AtomString(const UChar*, unsigned length);
    AtomString(const UChar*);

    template<size_t inlineCapacity>
    explicit AtomString(const Vector<UChar, inlineCapacity>& characters)
        : m_string(AtomStringImpl::add(characters.data(), characters.size()))
    {
    }

    AtomString(AtomStringImpl*);
    AtomString(RefPtr<AtomStringImpl>&&);
    AtomString(const StaticStringImpl*);
    AtomString(StringImpl*);
    AtomString(const String&);
    AtomString(StringImpl* baseString, unsigned start, unsigned length);

    // FIXME: AtomString doesn’t always have AtomStringImpl, so one of those two names needs to change.
    AtomString(UniquedStringImpl* uid);

    enum ConstructFromLiteralTag { ConstructFromLiteral };
    AtomString(const char* characters, unsigned length, ConstructFromLiteralTag)
        : m_string(AtomStringImpl::addLiteral(characters, length))
    {
    }

    template<unsigned characterCount> ALWAYS_INLINE AtomString(const char (&characters)[characterCount], ConstructFromLiteralTag)
        : m_string(AtomStringImpl::addLiteral(characters, characterCount - 1))
    {
        COMPILE_ASSERT(characterCount > 1, AtomStringFromLiteralNotEmpty);
        COMPILE_ASSERT((characterCount - 1 <= ((unsigned(~0) - sizeof(StringImpl)) / sizeof(LChar))), AtomStringFromLiteralCannotOverflow);
    }

    // We have to declare the copy constructor and copy assignment operator as well, otherwise
    // they'll be implicitly deleted by adding the move constructor and move assignment operator.
    AtomString(const AtomString& other) : m_string(other.m_string) { }
    AtomString(AtomString&& other) : m_string(WTFMove(other.m_string)) { }
    AtomString& operator=(const AtomString& other) { m_string = other.m_string; return *this; }
    AtomString& operator=(AtomString&& other) { m_string = WTFMove(other.m_string); return *this; }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    AtomString(WTF::HashTableDeletedValueType) : m_string(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_string.isHashTableDeletedValue(); }

    unsigned existingHash() const { return isNull() ? 0 : impl()->existingHash(); }

    operator const String&() const { return m_string; }
    const String& string() const { return m_string; };

    // FIXME: What guarantees this isn't a SymbolImpl rather than an AtomStringImpl?
    AtomStringImpl* impl() const { return static_cast<AtomStringImpl*>(m_string.impl()); }

    bool is8Bit() const { return m_string.is8Bit(); }
    const LChar* characters8() const { return m_string.characters8(); }
    const UChar* characters16() const { return m_string.characters16(); }
    unsigned length() const { return m_string.length(); }

    UChar operator[](unsigned int i) const { return m_string[i]; }

    WTF_EXPORT_PRIVATE static AtomString number(int);
    WTF_EXPORT_PRIVATE static AtomString number(unsigned);
    WTF_EXPORT_PRIVATE static AtomString number(unsigned long);
    WTF_EXPORT_PRIVATE static AtomString number(unsigned long long);
    WTF_EXPORT_PRIVATE static AtomString number(float);
    WTF_EXPORT_PRIVATE static AtomString number(double);
    // If we need more overloads of the number function, we can add all the others that String has, but these seem to do for now.

    bool contains(UChar character) const { return m_string.contains(character); }
    bool contains(const LChar* string) const { return m_string.contains(string); }
    bool contains(const String& string) const { return m_string.contains(string); }
    bool containsIgnoringASCIICase(const String& string) const { return m_string.containsIgnoringASCIICase(string); }

    size_t find(UChar character, unsigned start = 0) const { return m_string.find(character, start); }
    size_t find(const LChar* string, unsigned start = 0) const { return m_string.find(string, start); }
    size_t find(const String& string, unsigned start = 0) const { return m_string.find(string, start); }
    size_t findIgnoringASCIICase(const String& string) const { return m_string.findIgnoringASCIICase(string); }
    size_t findIgnoringASCIICase(const String& string, unsigned startOffset) const { return m_string.findIgnoringASCIICase(string, startOffset); }
    size_t find(CodeUnitMatchFunction matchFunction, unsigned start = 0) const { return m_string.find(matchFunction, start); }

    bool startsWith(const String& string) const { return m_string.startsWith(string); }
    bool startsWithIgnoringASCIICase(const String& string) const { return m_string.startsWithIgnoringASCIICase(string); }
    bool startsWith(UChar character) const { return m_string.startsWith(character); }
    template<unsigned matchLength> bool startsWith(const char (&prefix)[matchLength]) const { return m_string.startsWith<matchLength>(prefix); }

    bool endsWith(const String& string) const { return m_string.endsWith(string); }
    bool endsWithIgnoringASCIICase(const String& string) const { return m_string.endsWithIgnoringASCIICase(string); }
    bool endsWith(UChar character) const { return m_string.endsWith(character); }
    template<unsigned matchLength> bool endsWith(const char (&prefix)[matchLength]) const { return m_string.endsWith<matchLength>(prefix); }

    WTF_EXPORT_PRIVATE AtomString convertToASCIILowercase() const;
    WTF_EXPORT_PRIVATE AtomString convertToASCIIUppercase() const;

    int toInt(bool* ok = nullptr) const { return m_string.toInt(ok); }
    double toDouble(bool* ok = nullptr) const { return m_string.toDouble(ok); }
    float toFloat(bool* ok = nullptr) const { return m_string.toFloat(ok); }
    bool percentage(int& p) const { return m_string.percentage(p); }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

#if USE(CF)
    AtomString(CFStringRef);
#endif

#ifdef __OBJC__
    AtomString(NSString *);
    operator NSString *() const { return m_string; }
#endif

#if OS(WINDOWS)
    AtomString(const wchar_t* characters, unsigned length)
        : AtomString(ucharFrom(characters), length) { }

    AtomString(const wchar_t* characters)
        : AtomString(ucharFrom(characters)) { }
#endif

    // AtomString::fromUTF8 will return a null string if the input data contains invalid UTF-8 sequences.
    static AtomString fromUTF8(const char*, size_t);
    static AtomString fromUTF8(const char*);

#ifndef NDEBUG
    void show() const;
#endif

private:
    // The explicit constructors with AtomString::ConstructFromLiteral must be used for literals.
    AtomString(ASCIILiteral);

    enum class CaseConvertType { Upper, Lower };
    template<CaseConvertType> AtomString convertASCIICase() const;

    WTF_EXPORT_PRIVATE static AtomString fromUTF8Internal(const char*, const char*);

    String m_string;
};

static_assert(sizeof(AtomString) == sizeof(String), "AtomString and String must be the same size!");

inline bool operator==(const AtomString& a, const AtomString& b) { return a.impl() == b.impl(); }
bool operator==(const AtomString&, const LChar*);
inline bool operator==(const AtomString& a, const char* b) { return WTF::equal(a.impl(), reinterpret_cast<const LChar*>(b)); }
inline bool operator==(const AtomString& a, const Vector<UChar>& b) { return a.impl() && equal(a.impl(), b.data(), b.size()); }    
inline bool operator==(const AtomString& a, const String& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const LChar* a, const AtomString& b) { return b == a; }
inline bool operator==(const String& a, const AtomString& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const Vector<UChar>& a, const AtomString& b) { return b == a; }

inline bool operator!=(const AtomString& a, const AtomString& b) { return a.impl() != b.impl(); }
inline bool operator!=(const AtomString& a, const LChar* b) { return !(a == b); }
inline bool operator!=(const AtomString& a, const char* b) { return !(a == b); }
inline bool operator!=(const AtomString& a, const String& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const AtomString& a, const Vector<UChar>& b) { return !(a == b); }
inline bool operator!=(const LChar* a, const AtomString& b) { return !(b == a); }
inline bool operator!=(const String& a, const AtomString& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const Vector<UChar>& a, const AtomString& b) { return !(a == b); }

bool equalIgnoringASCIICase(const AtomString&, const AtomString&);
bool equalIgnoringASCIICase(const AtomString&, const String&);
bool equalIgnoringASCIICase(const String&, const AtomString&);
bool equalIgnoringASCIICase(const AtomString&, const char*);

template<unsigned length> bool equalLettersIgnoringASCIICase(const AtomString&, const char (&lowercaseLetters)[length]);

inline AtomString::AtomString()
{
}

inline AtomString::AtomString(const LChar* string)
    : m_string(AtomStringImpl::add(string))
{
}

inline AtomString::AtomString(const char* string)
    : m_string(AtomStringImpl::add(string))
{
}

inline AtomString::AtomString(const LChar* string, unsigned length)
    : m_string(AtomStringImpl::add(string, length))
{
}

inline AtomString::AtomString(const UChar* string, unsigned length)
    : m_string(AtomStringImpl::add(string, length))
{
}

inline AtomString::AtomString(const UChar* string)
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

#ifdef __OBJC__

inline AtomString::AtomString(NSString *string)
    : m_string(AtomStringImpl::add((__bridge CFStringRef)string))
{
}

#endif

// nullAtom and emptyAtom are special AtomString. They can be used from any threads since their StringImpls are not actually registered into AtomStringTable.
extern WTF_EXPORT_PRIVATE LazyNeverDestroyed<const AtomString> nullAtomData;
extern WTF_EXPORT_PRIVATE LazyNeverDestroyed<const AtomString> emptyAtomData;

// Define external global variables for the commonly used atom strings.
// These are only usable from the main thread.
extern WTF_EXPORT_PRIVATE MainThreadLazyNeverDestroyed<const AtomString> starAtomData;
extern WTF_EXPORT_PRIVATE MainThreadLazyNeverDestroyed<const AtomString> xmlAtomData;
extern WTF_EXPORT_PRIVATE MainThreadLazyNeverDestroyed<const AtomString> xmlnsAtomData;

inline const AtomString& nullAtom() { return nullAtomData.get(); }
inline const AtomString& emptyAtom() { return emptyAtomData.get(); }
inline const AtomString& starAtom() { return starAtomData.get(); }
inline const AtomString& xmlAtom() { return xmlAtomData.get(); }
inline const AtomString& xmlnsAtom() { return xmlnsAtomData.get(); }

inline AtomString AtomString::fromUTF8(const char* characters, size_t length)
{
    if (!characters)
        return nullAtom();
    if (!length)
        return emptyAtom();
    return fromUTF8Internal(characters, characters + length);
}

inline AtomString AtomString::fromUTF8(const char* characters)
{
    if (!characters)
        return nullAtom();
    if (!*characters)
        return emptyAtom();
    return fromUTF8Internal(characters, nullptr);
}

// AtomStringHash is the default hash for AtomString
template<typename> struct DefaultHash;
template<> struct DefaultHash<AtomString>;

template<unsigned length> inline bool equalLettersIgnoringASCIICase(const AtomString& string, const char (&lowercaseLetters)[length])
{
    return equalLettersIgnoringASCIICase(string.string(), lowercaseLetters);
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

inline bool equalIgnoringASCIICase(const AtomString& a, const char* b)
{
    return equalIgnoringASCIICase(a.string(), b);
}

template<> struct IntegerToStringConversionTrait<AtomString> {
    using ReturnType = AtomString;
    using AdditionalArgumentType = void;
    static AtomString flush(LChar* characters, unsigned length, void*) { return { characters, length }; }
};

} // namespace WTF

using WTF::AtomString;
using WTF::nullAtom;
using WTF::emptyAtom;
using WTF::starAtom;
using WTF::xmlAtom;
using WTF::xmlnsAtom;

#include <wtf/text/StringConcatenate.h>
