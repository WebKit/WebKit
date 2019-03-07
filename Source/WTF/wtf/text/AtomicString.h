/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include <wtf/text/AtomicStringImpl.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
#include <wtf/text/win/WCharStringExtras.h>
#endif

// Define 'NO_IMPLICIT_ATOMICSTRING' before including this header,
// to disallow (expensive) implicit String-->AtomicString conversions.
#ifdef NO_IMPLICIT_ATOMICSTRING
#define ATOMICSTRING_CONVERSION explicit
#else
#define ATOMICSTRING_CONVERSION
#endif

namespace WTF {

struct AtomicStringHash;

class AtomicString {
public:
    WTF_EXPORT_PRIVATE static void init();

    AtomicString();
    AtomicString(const LChar*);
    AtomicString(const char*);
    AtomicString(const LChar*, unsigned length);
    AtomicString(const UChar*, unsigned length);
    AtomicString(const UChar*);

    template<size_t inlineCapacity>
    explicit AtomicString(const Vector<UChar, inlineCapacity>& characters)
        : m_string(AtomicStringImpl::add(characters.data(), characters.size()))
    {
    }

    AtomicString(AtomicStringImpl*);
    AtomicString(RefPtr<AtomicStringImpl>&&);
    AtomicString(const StaticStringImpl*);
    ATOMICSTRING_CONVERSION AtomicString(StringImpl*);
    ATOMICSTRING_CONVERSION AtomicString(const String&);
    AtomicString(StringImpl* baseString, unsigned start, unsigned length);

    // FIXME: AtomicString doesn’t always have AtomicStringImpl, so one of those two names needs to change..
    AtomicString(UniquedStringImpl* uid);

    enum ConstructFromLiteralTag { ConstructFromLiteral };
    AtomicString(const char* characters, unsigned length, ConstructFromLiteralTag)
        : m_string(AtomicStringImpl::addLiteral(characters, length))
    {
    }

    template<unsigned characterCount> ALWAYS_INLINE AtomicString(const char (&characters)[characterCount], ConstructFromLiteralTag)
        : m_string(AtomicStringImpl::addLiteral(characters, characterCount - 1))
    {
        COMPILE_ASSERT(characterCount > 1, AtomicStringFromLiteralNotEmpty);
        COMPILE_ASSERT((characterCount - 1 <= ((unsigned(~0) - sizeof(StringImpl)) / sizeof(LChar))), AtomicStringFromLiteralCannotOverflow);
    }

    // We have to declare the copy constructor and copy assignment operator as well, otherwise
    // they'll be implicitly deleted by adding the move constructor and move assignment operator.
    AtomicString(const AtomicString& other) : m_string(other.m_string) { }
    AtomicString(AtomicString&& other) : m_string(WTFMove(other.m_string)) { }
    AtomicString& operator=(const AtomicString& other) { m_string = other.m_string; return *this; }
    AtomicString& operator=(AtomicString&& other) { m_string = WTFMove(other.m_string); return *this; }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    AtomicString(WTF::HashTableDeletedValueType) : m_string(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return m_string.isHashTableDeletedValue(); }

    unsigned existingHash() const { return isNull() ? 0 : impl()->existingHash(); }

    operator const String&() const { return m_string; }
    const String& string() const { return m_string; };

    AtomicStringImpl* impl() const { return static_cast<AtomicStringImpl *>(m_string.impl()); }

    bool is8Bit() const { return m_string.is8Bit(); }
    const LChar* characters8() const { return m_string.characters8(); }
    const UChar* characters16() const { return m_string.characters16(); }
    unsigned length() const { return m_string.length(); }

    UChar operator[](unsigned int i) const { return m_string[i]; }

    WTF_EXPORT_PRIVATE static AtomicString number(int);
    WTF_EXPORT_PRIVATE static AtomicString number(unsigned);
    WTF_EXPORT_PRIVATE static AtomicString number(unsigned long);
    WTF_EXPORT_PRIVATE static AtomicString number(unsigned long long);
    WTF_EXPORT_PRIVATE static AtomicString number(float);
    WTF_EXPORT_PRIVATE static AtomicString number(double);
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

    WTF_EXPORT_PRIVATE AtomicString convertToASCIILowercase() const;
    WTF_EXPORT_PRIVATE AtomicString convertToASCIIUppercase() const;

    int toInt(bool* ok = 0) const { return m_string.toInt(ok); }
    double toDouble(bool* ok = 0) const { return m_string.toDouble(ok); }
    float toFloat(bool* ok = 0) const { return m_string.toFloat(ok); }
    bool percentage(int& p) const { return m_string.percentage(p); }

    bool isNull() const { return m_string.isNull(); }
    bool isEmpty() const { return m_string.isEmpty(); }

#if USE(CF)
    AtomicString(CFStringRef);
#endif

#ifdef __OBJC__
    AtomicString(NSString*);
    operator NSString*() const { return m_string; }
#endif

#if OS(WINDOWS) && U_ICU_VERSION_MAJOR_NUM >= 59
    AtomicString(const wchar_t* characters, unsigned length)
        : AtomicString(ucharFrom(characters), length) { }

    AtomicString(const wchar_t* characters)
        : AtomicString(ucharFrom(characters)) { }
#endif

    // AtomicString::fromUTF8 will return a null string if the input data contains invalid UTF-8 sequences.
    static AtomicString fromUTF8(const char*, size_t);
    static AtomicString fromUTF8(const char*);

#ifndef NDEBUG
    void show() const;
#endif

private:
    // The explicit constructors with AtomicString::ConstructFromLiteral must be used for literals.
    AtomicString(ASCIILiteral);

    enum class CaseConvertType { Upper, Lower };
    template<CaseConvertType> AtomicString convertASCIICase() const;

    WTF_EXPORT_PRIVATE static AtomicString fromUTF8Internal(const char*, const char*);

    String m_string;
};

static_assert(sizeof(AtomicString) == sizeof(String), "AtomicString and String must be same size!");

inline bool operator==(const AtomicString& a, const AtomicString& b) { return a.impl() == b.impl(); }
bool operator==(const AtomicString&, const LChar*);
inline bool operator==(const AtomicString& a, const char* b) { return WTF::equal(a.impl(), reinterpret_cast<const LChar*>(b)); }
inline bool operator==(const AtomicString& a, const Vector<UChar>& b) { return a.impl() && equal(a.impl(), b.data(), b.size()); }    
inline bool operator==(const AtomicString& a, const String& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const LChar* a, const AtomicString& b) { return b == a; }
inline bool operator==(const String& a, const AtomicString& b) { return equal(a.impl(), b.impl()); }
inline bool operator==(const Vector<UChar>& a, const AtomicString& b) { return b == a; }

inline bool operator!=(const AtomicString& a, const AtomicString& b) { return a.impl() != b.impl(); }
inline bool operator!=(const AtomicString& a, const LChar* b) { return !(a == b); }
inline bool operator!=(const AtomicString& a, const char* b) { return !(a == b); }
inline bool operator!=(const AtomicString& a, const String& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const AtomicString& a, const Vector<UChar>& b) { return !(a == b); }
inline bool operator!=(const LChar* a, const AtomicString& b) { return !(b == a); }
inline bool operator!=(const String& a, const AtomicString& b) { return !equal(a.impl(), b.impl()); }
inline bool operator!=(const Vector<UChar>& a, const AtomicString& b) { return !(a == b); }

bool equalIgnoringASCIICase(const AtomicString&, const AtomicString&);
bool equalIgnoringASCIICase(const AtomicString&, const String&);
bool equalIgnoringASCIICase(const String&, const AtomicString&);
bool equalIgnoringASCIICase(const AtomicString&, const char*);

template<unsigned length> bool equalLettersIgnoringASCIICase(const AtomicString&, const char (&lowercaseLetters)[length]);

inline AtomicString::AtomicString()
{
}

inline AtomicString::AtomicString(const LChar* string)
    : m_string(AtomicStringImpl::add(string))
{
}

inline AtomicString::AtomicString(const char* string)
    : m_string(AtomicStringImpl::add(string))
{
}

inline AtomicString::AtomicString(const LChar* string, unsigned length)
    : m_string(AtomicStringImpl::add(string, length))
{
}

inline AtomicString::AtomicString(const UChar* string, unsigned length)
    : m_string(AtomicStringImpl::add(string, length))
{
}

inline AtomicString::AtomicString(const UChar* string)
    : m_string(AtomicStringImpl::add(string))
{
}

inline AtomicString::AtomicString(AtomicStringImpl* string)
    : m_string(string)
{
}

inline AtomicString::AtomicString(RefPtr<AtomicStringImpl>&& string)
    : m_string(WTFMove(string))
{
}

inline AtomicString::AtomicString(StringImpl* string)
    : m_string(AtomicStringImpl::add(string))
{
}

inline AtomicString::AtomicString(const StaticStringImpl* string)
    : m_string(AtomicStringImpl::add(string))
{
}

inline AtomicString::AtomicString(const String& string)
    : m_string(AtomicStringImpl::add(string.impl()))
{
}

inline AtomicString::AtomicString(StringImpl* baseString, unsigned start, unsigned length)
    : m_string(AtomicStringImpl::add(baseString, start, length))
{
}

inline AtomicString::AtomicString(UniquedStringImpl* uid)
    : m_string(uid)
{
}

#if USE(CF)

inline AtomicString::AtomicString(CFStringRef string)
    :  m_string(AtomicStringImpl::add(string))
{
}

#endif

#ifdef __OBJC__

inline AtomicString::AtomicString(NSString* string)
    : m_string(AtomicStringImpl::add((__bridge CFStringRef)string))
{
}

#endif

// Define external global variables for the commonly used atomic strings.
// These are only usable from the main thread.
extern WTF_EXPORT_PRIVATE LazyNeverDestroyed<AtomicString> nullAtomData;
extern WTF_EXPORT_PRIVATE LazyNeverDestroyed<AtomicString> emptyAtomData;
extern WTF_EXPORT_PRIVATE LazyNeverDestroyed<AtomicString> starAtomData;
extern WTF_EXPORT_PRIVATE LazyNeverDestroyed<AtomicString> xmlAtomData;
extern WTF_EXPORT_PRIVATE LazyNeverDestroyed<AtomicString> xmlnsAtomData;

inline const AtomicString& nullAtom() { return nullAtomData.get(); }
inline const AtomicString& emptyAtom() { return emptyAtomData.get(); }
inline const AtomicString& starAtom() { return starAtomData.get(); }
inline const AtomicString& xmlAtom() { return xmlAtomData.get(); }
inline const AtomicString& xmlnsAtom() { return xmlnsAtomData.get(); }

inline AtomicString AtomicString::fromUTF8(const char* characters, size_t length)
{
    if (!characters)
        return nullAtom();
    if (!length)
        return emptyAtom();
    return fromUTF8Internal(characters, characters + length);
}

inline AtomicString AtomicString::fromUTF8(const char* characters)
{
    if (!characters)
        return nullAtom();
    if (!*characters)
        return emptyAtom();
    return fromUTF8Internal(characters, nullptr);
}

// AtomicStringHash is the default hash for AtomicString
template<typename T> struct DefaultHash;
template<> struct DefaultHash<AtomicString> {
    typedef AtomicStringHash Hash;
};

template<unsigned length> inline bool equalLettersIgnoringASCIICase(const AtomicString& string, const char (&lowercaseLetters)[length])
{
    return equalLettersIgnoringASCIICase(string.string(), lowercaseLetters);
}

inline bool equalIgnoringASCIICase(const AtomicString& a, const AtomicString& b)
{
    return equalIgnoringASCIICase(a.string(), b.string());
}

inline bool equalIgnoringASCIICase(const AtomicString& a, const String& b)
{
    return equalIgnoringASCIICase(a.string(), b);
}

inline bool equalIgnoringASCIICase(const String& a, const AtomicString& b)
{
    return equalIgnoringASCIICase(a, b.string());
}

inline bool equalIgnoringASCIICase(const AtomicString& a, const char* b)
{
    return equalIgnoringASCIICase(a.string(), b);
}

template<> struct IntegerToStringConversionTrait<AtomicString> {
    using ReturnType = AtomicString;
    using AdditionalArgumentType = void;
    static AtomicString flush(LChar* characters, unsigned length, void*) { return { characters, length }; }
};

} // namespace WTF

#ifndef ATOMICSTRING_HIDE_GLOBALS
using WTF::AtomicString;
using WTF::nullAtom;
using WTF::emptyAtom;
using WTF::starAtom;
using WTF::xmlAtom;
using WTF::xmlnsAtom;
#endif

#include <wtf/text/StringConcatenate.h>
