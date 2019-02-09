/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include <limits.h>
#include <unicode/ustring.h>
#include <wtf/ASCIICType.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Expected.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/ConversionMode.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringHasher.h>
#include <wtf/text/UTF8ConversionError.h>

#if USE(CF)
typedef const struct __CFString * CFStringRef;
#endif

#ifdef __OBJC__
@class NSString;
#endif

namespace JSC {
namespace LLInt { class Data; }
class LLIntOffsetsExtractor;
}

namespace WTF {

class SymbolImpl;
class SymbolRegistry;

struct CStringTranslator;
struct HashAndUTF8CharactersTranslator;
struct LCharBufferTranslator;
struct StringHash;
struct SubstringTranslator;
struct UCharBufferTranslator;

template<typename> class RetainPtr;

template<typename> struct BufferFromStaticDataTranslator;
template<typename> struct HashAndCharactersTranslator;

// Define STRING_STATS to 1 turn on runtime statistics of string sizes and memory usage.
#define STRING_STATS 0

template<bool isSpecialCharacter(UChar), typename CharacterType> bool isAllSpecialCharacters(const CharacterType*, size_t length);

#if STRING_STATS

struct StringStats {
    void add8BitString(unsigned length, bool isSubString = false)
    {
        ++m_totalNumberStrings;
        ++m_number8BitStrings;
        if (!isSubString)
            m_total8BitData += length;
    }

    void add16BitString(unsigned length, bool isSubString = false)
    {
        ++m_totalNumberStrings;
        ++m_number16BitStrings;
        if (!isSubString)
            m_total16BitData += length;
    }

    void removeString(StringImpl&);
    void printStats();

    static const unsigned s_printStringStatsFrequency = 5000;
    static std::atomic<unsigned> s_stringRemovesTillPrintStats;

    std::atomic<unsigned> m_refCalls;
    std::atomic<unsigned> m_derefCalls;

    std::atomic<unsigned> m_totalNumberStrings;
    std::atomic<unsigned> m_number8BitStrings;
    std::atomic<unsigned> m_number16BitStrings;
    std::atomic<unsigned long long> m_total8BitData;
    std::atomic<unsigned long long> m_total16BitData;
};

#define STRING_STATS_ADD_8BIT_STRING(length) StringImpl::stringStats().add8BitString(length)
#define STRING_STATS_ADD_8BIT_STRING2(length, isSubString) StringImpl::stringStats().add8BitString(length, isSubString)
#define STRING_STATS_ADD_16BIT_STRING(length) StringImpl::stringStats().add16BitString(length)
#define STRING_STATS_ADD_16BIT_STRING2(length, isSubString) StringImpl::stringStats().add16BitString(length, isSubString)
#define STRING_STATS_REMOVE_STRING(string) StringImpl::stringStats().removeString(string)
#define STRING_STATS_REF_STRING(string) ++StringImpl::stringStats().m_refCalls;
#define STRING_STATS_DEREF_STRING(string) ++StringImpl::stringStats().m_derefCalls;

#else

#define STRING_STATS_ADD_8BIT_STRING(length) ((void)0)
#define STRING_STATS_ADD_8BIT_STRING2(length, isSubString) ((void)0)
#define STRING_STATS_ADD_16BIT_STRING(length) ((void)0)
#define STRING_STATS_ADD_16BIT_STRING2(length, isSubString) ((void)0)
#define STRING_STATS_ADD_UPCONVERTED_STRING(length) ((void)0)
#define STRING_STATS_REMOVE_STRING(string) ((void)0)
#define STRING_STATS_REF_STRING(string) ((void)0)
#define STRING_STATS_DEREF_STRING(string) ((void)0)

#endif

template<typename CharacterType> inline bool isLatin1(CharacterType character)
{
    using UnsignedCharacterType = typename std::make_unsigned<CharacterType>::type;
    return static_cast<UnsignedCharacterType>(character) <= static_cast<UnsignedCharacterType>(0xFF);
}

class StringImplShape {
    WTF_MAKE_NONCOPYABLE(StringImplShape);
public:
    static constexpr unsigned MaxLength = std::numeric_limits<int32_t>::max();

protected:
    StringImplShape(unsigned refCount, unsigned length, const LChar*, unsigned hashAndFlags);
    StringImplShape(unsigned refCount, unsigned length, const UChar*, unsigned hashAndFlags);

    enum ConstructWithConstExprTag { ConstructWithConstExpr };
    template<unsigned characterCount> constexpr StringImplShape(unsigned refCount, unsigned length, const char (&characters)[characterCount], unsigned hashAndFlags, ConstructWithConstExprTag);
    template<unsigned characterCount> constexpr StringImplShape(unsigned refCount, unsigned length, const char16_t (&characters)[characterCount], unsigned hashAndFlags, ConstructWithConstExprTag);

    unsigned m_refCount;
    unsigned m_length;
    union {
        const LChar* m_data8;
        const UChar* m_data16;
        // It seems that reinterpret_cast prevents constexpr's compile time initialization in VC++.
        // These are needed to avoid reinterpret_cast.
        const char* m_data8Char;
        const char16_t* m_data16Char;
    };
    mutable unsigned m_hashAndFlags;
};

// FIXME: Use of StringImpl and const is rather confused.
// The actual string inside a StringImpl is immutable, so you can't modify a string using a StringImpl&.
// We could mark every member function const and always use "const StringImpl&" and "const StringImpl*".
// Or we could say that "const" doesn't make sense at all and use "StringImpl&" and "StringImpl*" everywhere.
// Right now we use a mix of both, which makes code more confusing and has no benefit.

class StringImpl : private StringImplShape {
    WTF_MAKE_NONCOPYABLE(StringImpl); WTF_MAKE_FAST_ALLOCATED;

    friend class AtomicStringImpl;
    friend class JSC::LLInt::Data;
    friend class JSC::LLIntOffsetsExtractor;
    friend class PrivateSymbolImpl;
    friend class RegisteredSymbolImpl;
    friend class SymbolImpl;
    friend class ExternalStringImpl;

    friend struct WTF::CStringTranslator;
    friend struct WTF::HashAndUTF8CharactersTranslator;
    friend struct WTF::LCharBufferTranslator;
    friend struct WTF::SubstringTranslator;
    friend struct WTF::UCharBufferTranslator;

    template<typename> friend struct WTF::BufferFromStaticDataTranslator;
    template<typename> friend struct WTF::HashAndCharactersTranslator;

public:
    enum BufferOwnership { BufferInternal, BufferOwned, BufferSubstring, BufferExternal };

    static constexpr unsigned MaxLength = StringImplShape::MaxLength;

    // The bottom 6 bits in the hash are flags.
    static constexpr const unsigned s_flagCount = 6;
private:
    static constexpr const unsigned s_flagMask = (1u << s_flagCount) - 1;
    static_assert(s_flagCount <= StringHasher::flagCount, "StringHasher reserves enough bits for StringImpl flags");
    static constexpr const unsigned s_flagStringKindCount = 4;

    static constexpr const unsigned s_hashFlagStringKindIsAtomic = 1u << (s_flagStringKindCount);
    static constexpr const unsigned s_hashFlagStringKindIsSymbol = 1u << (s_flagStringKindCount + 1);
    static constexpr const unsigned s_hashMaskStringKind = s_hashFlagStringKindIsAtomic | s_hashFlagStringKindIsSymbol;
    static constexpr const unsigned s_hashFlag8BitBuffer = 1u << 3;
    static constexpr const unsigned s_hashFlagDidReportCost = 1u << 2;
    static constexpr const unsigned s_hashMaskBufferOwnership = (1u << 0) | (1u << 1);

    enum StringKind {
        StringNormal = 0u, // non-symbol, non-atomic
        StringAtomic = s_hashFlagStringKindIsAtomic, // non-symbol, atomic
        StringSymbol = s_hashFlagStringKindIsSymbol, // symbol, non-atomic
    };

    // Create a normal 8-bit string with internal storage (BufferInternal).
    enum Force8Bit { Force8BitConstructor };
    StringImpl(unsigned length, Force8Bit);

    // Create a normal 16-bit string with internal storage (BufferInternal).
    explicit StringImpl(unsigned length);

    // Create a StringImpl adopting ownership of the provided buffer (BufferOwned).
    StringImpl(MallocPtr<LChar>, unsigned length);
    StringImpl(MallocPtr<UChar>, unsigned length);
    enum ConstructWithoutCopyingTag { ConstructWithoutCopying };
    StringImpl(const UChar*, unsigned length, ConstructWithoutCopyingTag);
    StringImpl(const LChar*, unsigned length, ConstructWithoutCopyingTag);

    // Used to create new strings that are a substring of an existing StringImpl (BufferSubstring).
    StringImpl(const LChar*, unsigned length, Ref<StringImpl>&&);
    StringImpl(const UChar*, unsigned length, Ref<StringImpl>&&);

public:
    WTF_EXPORT_PRIVATE static void destroy(StringImpl*);

    WTF_EXPORT_PRIVATE static Ref<StringImpl> create(const UChar*, unsigned length);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> create(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> create8BitIfPossible(const UChar*, unsigned length);
    template<size_t inlineCapacity> static Ref<StringImpl> create8BitIfPossible(const Vector<UChar, inlineCapacity>&);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> create8BitIfPossible(const UChar*);

    ALWAYS_INLINE static Ref<StringImpl> create(const char* characters, unsigned length) { return create(reinterpret_cast<const LChar*>(characters), length); }
    WTF_EXPORT_PRIVATE static Ref<StringImpl> create(const LChar*);
    ALWAYS_INLINE static Ref<StringImpl> create(const char* string) { return create(reinterpret_cast<const LChar*>(string)); }

    static Ref<StringImpl> createSubstringSharingImpl(StringImpl&, unsigned offset, unsigned length);

    template<unsigned characterCount> static Ref<StringImpl> createFromLiteral(const char (&)[characterCount]);

    // FIXME: Replace calls to these overloads of createFromLiteral to createWithoutCopying instead.
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createFromLiteral(const char*, unsigned length);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createFromLiteral(const char*);

    WTF_EXPORT_PRIVATE static Ref<StringImpl> createWithoutCopying(const UChar*, unsigned length);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createWithoutCopying(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createUninitialized(unsigned length, LChar*&);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createUninitialized(unsigned length, UChar*&);
    template<typename CharacterType> static RefPtr<StringImpl> tryCreateUninitialized(unsigned length, CharacterType*&);

    // Reallocate the StringImpl. The originalString must be only owned by the Ref,
    // and the buffer ownership must be BufferInternal. Just like the input pointer of realloc(),
    // the originalString can't be used after this function.
    static Ref<StringImpl> reallocate(Ref<StringImpl>&& originalString, unsigned length, LChar*& data);
    static Ref<StringImpl> reallocate(Ref<StringImpl>&& originalString, unsigned length, UChar*& data);
    static Expected<Ref<StringImpl>, UTF8ConversionError> tryReallocate(Ref<StringImpl>&& originalString, unsigned length, LChar*& data);
    static Expected<Ref<StringImpl>, UTF8ConversionError> tryReallocate(Ref<StringImpl>&& originalString, unsigned length, UChar*& data);

    static unsigned flagsOffset() { return OBJECT_OFFSETOF(StringImpl, m_hashAndFlags); }
    static unsigned flagIs8Bit() { return s_hashFlag8BitBuffer; }
    static unsigned flagIsAtomic() { return s_hashFlagStringKindIsAtomic; }
    static unsigned flagIsSymbol() { return s_hashFlagStringKindIsSymbol; }
    static unsigned maskStringKind() { return s_hashMaskStringKind; }
    static unsigned dataOffset() { return OBJECT_OFFSETOF(StringImpl, m_data8); }

    template<typename CharacterType, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity>
    static Ref<StringImpl> adopt(Vector<CharacterType, inlineCapacity, OverflowHandler, minCapacity>&&);

    WTF_EXPORT_PRIVATE static Ref<StringImpl> adopt(StringBuffer<UChar>&&);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> adopt(StringBuffer<LChar>&&);

    unsigned length() const { return m_length; }
    static ptrdiff_t lengthMemoryOffset() { return OBJECT_OFFSETOF(StringImpl, m_length); }
    bool isEmpty() const { return !m_length; }

    bool is8Bit() const { return m_hashAndFlags & s_hashFlag8BitBuffer; }
    ALWAYS_INLINE const LChar* characters8() const { ASSERT(is8Bit()); return m_data8; }
    ALWAYS_INLINE const UChar* characters16() const { ASSERT(!is8Bit()); return m_data16; }

    template<typename CharacterType> const CharacterType* characters() const;

    size_t cost() const;
    size_t costDuringGC();

    WTF_EXPORT_PRIVATE size_t sizeInBytes() const;

    bool isSymbol() const { return m_hashAndFlags & s_hashFlagStringKindIsSymbol; }
    bool isAtomic() const { return m_hashAndFlags & s_hashFlagStringKindIsAtomic; }
    void setIsAtomic(bool);
    
    bool isExternal() const { return bufferOwnership() == BufferExternal; }

#if STRING_STATS
    bool isSubString() const { return bufferOwnership() == BufferSubstring; }
#endif

    static WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> utf8ForCharacters(const LChar* characters, unsigned length);
    static WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> utf8ForCharacters(const UChar* characters, unsigned length, ConversionMode = LenientConversion);

    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUtf8ForRange(unsigned offset, unsigned length, ConversionMode = LenientConversion) const;
    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUtf8(ConversionMode = LenientConversion) const;
    WTF_EXPORT_PRIVATE CString utf8(ConversionMode = LenientConversion) const;

private:
    static WTF_EXPORT_PRIVATE UTF8ConversionError utf8Impl(const UChar* characters, unsigned length, char*& buffer, size_t bufferSize, ConversionMode);
    
    // The high bits of 'hash' are always empty, but we prefer to store our flags
    // in the low bits because it makes them slightly more efficient to access.
    // So, we shift left and right when setting and getting our hash code.
    void setHash(unsigned) const;

    unsigned rawHash() const { return m_hashAndFlags >> s_flagCount; }

public:
    bool hasHash() const { return !!rawHash(); }

    unsigned existingHash() const { ASSERT(hasHash()); return rawHash(); }
    unsigned hash() const { return hasHash() ? rawHash() : hashSlowCase(); }

    WTF_EXPORT_PRIVATE unsigned concurrentHash() const;

    unsigned symbolAwareHash() const;
    unsigned existingSymbolAwareHash() const;

    bool isStatic() const { return m_refCount & s_refCountFlagIsStaticString; }

    size_t refCount() const { return m_refCount / s_refCountIncrement; }
    bool hasOneRef() const { return m_refCount == s_refCountIncrement; }
    bool hasAtLeastOneRef() const { return m_refCount; } // For assertions.

    void ref();
    void deref();

    class StaticStringImpl : private StringImplShape {
        WTF_MAKE_NONCOPYABLE(StaticStringImpl);
    public:
        // Used to construct static strings, which have an special refCount that can never hit zero.
        // This means that the static string will never be destroyed, which is important because
        // static strings will be shared across threads & ref-counted in a non-threadsafe manner.
        //
        // In order to make StaticStringImpl thread safe, we also need to ensure that the rest of
        // the fields are never mutated by threads. We have this guarantee because:
        //
        // 1. m_length is only set on construction and never mutated thereafter.
        //
        // 2. m_data8 and m_data16 are only set on construction and never mutated thereafter.
        //    We also know that a StringImpl never changes from 8 bit to 16 bit because there
        //    is no way to set/clear the s_hashFlag8BitBuffer flag other than at construction.
        //
        // 3. m_hashAndFlags will not be mutated by different threads because:
        //
        //    a. StaticStringImpl's constructor sets the s_hashFlagDidReportCost flag to ensure
        //       that StringImpl::cost() returns early.
        //       This means StaticStringImpl costs are not counted. But since there should only
        //       be a finite set of StaticStringImpls, their cost can be aggregated into a single
        //       system cost if needed.
        //    b. setIsAtomic() is never called on a StaticStringImpl.
        //       setIsAtomic() asserts !isStatic().
        //    c. setHash() is never called on a StaticStringImpl.
        //       StaticStringImpl's constructor sets the hash on construction.
        //       StringImpl::hash() only sets a new hash iff !hasHash().
        //       Additionally, StringImpl::setHash() asserts hasHash() and !isStatic().

        template<unsigned characterCount> constexpr StaticStringImpl(const char (&characters)[characterCount], StringKind = StringNormal);
        template<unsigned characterCount> constexpr StaticStringImpl(const char16_t (&characters)[characterCount], StringKind = StringNormal);
        operator StringImpl&();
    };

    WTF_EXPORT_PRIVATE static StaticStringImpl s_atomicEmptyString;
    ALWAYS_INLINE static StringImpl* empty() { return reinterpret_cast<StringImpl*>(&s_atomicEmptyString); }

    // FIXME: Does this really belong in StringImpl?
    template<typename CharacterType> static void copyCharacters(CharacterType* destination, const CharacterType* source, unsigned numCharacters);
    static void copyCharacters(UChar* destination, const LChar* source, unsigned numCharacters);

    // Some string features, like reference counting and the atomicity flag, are not
    // thread-safe. We achieve thread safety by isolation, giving each thread
    // its own copy of the string.
    Ref<StringImpl> isolatedCopy() const;

    WTF_EXPORT_PRIVATE Ref<StringImpl> substring(unsigned position, unsigned length = MaxLength);

    UChar at(unsigned) const;
    UChar operator[](unsigned i) const { return at(i); }
    WTF_EXPORT_PRIVATE UChar32 characterStartingAt(unsigned);

    int toIntStrict(bool* ok = 0, int base = 10);
    unsigned toUIntStrict(bool* ok = 0, int base = 10);
    int64_t toInt64Strict(bool* ok = 0, int base = 10);
    uint64_t toUInt64Strict(bool* ok = 0, int base = 10);
    intptr_t toIntPtrStrict(bool* ok = 0, int base = 10);

    WTF_EXPORT_PRIVATE int toInt(bool* ok = 0); // ignores trailing garbage
    unsigned toUInt(bool* ok = 0); // ignores trailing garbage
    int64_t toInt64(bool* ok = 0); // ignores trailing garbage
    uint64_t toUInt64(bool* ok = 0); // ignores trailing garbage
    intptr_t toIntPtr(bool* ok = 0); // ignores trailing garbage

    // FIXME: Like the strict functions above, these give false for "ok" when there is trailing garbage.
    // Like the non-strict functions above, these return the value when there is trailing garbage.
    // It would be better if these were more consistent with the above functions instead.
    double toDouble(bool* ok = 0);
    float toFloat(bool* ok = 0);

    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToASCIILowercase();
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToASCIIUppercase();
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToLowercaseWithoutLocale();
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned);
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToUppercaseWithoutLocale();
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToLowercaseWithLocale(const AtomicString& localeIdentifier);
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToUppercaseWithLocale(const AtomicString& localeIdentifier);

    Ref<StringImpl> foldCase();

    Ref<StringImpl> stripWhiteSpace();
    WTF_EXPORT_PRIVATE Ref<StringImpl> simplifyWhiteSpace();
    Ref<StringImpl> simplifyWhiteSpace(CodeUnitMatchFunction);

    Ref<StringImpl> stripLeadingAndTrailingCharacters(CodeUnitMatchFunction);
    Ref<StringImpl> removeCharacters(CodeUnitMatchFunction);

    bool isAllASCII() const;
    bool isAllLatin1() const;
    template<bool isSpecialCharacter(UChar)> bool isAllSpecialCharacters() const;

    size_t find(LChar character, unsigned start = 0);
    size_t find(char character, unsigned start = 0);
    size_t find(UChar character, unsigned start = 0);
    WTF_EXPORT_PRIVATE size_t find(CodeUnitMatchFunction, unsigned index = 0);
    size_t find(const LChar*, unsigned index = 0);
    ALWAYS_INLINE size_t find(const char* string, unsigned index = 0) { return find(reinterpret_cast<const LChar*>(string), index); }
    WTF_EXPORT_PRIVATE size_t find(StringImpl*);
    WTF_EXPORT_PRIVATE size_t find(StringImpl*, unsigned index);
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(const StringImpl&) const;
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(const StringImpl&, unsigned startOffset) const;
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(const StringImpl*) const;
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(const StringImpl*, unsigned startOffset) const;

    WTF_EXPORT_PRIVATE size_t reverseFind(UChar, unsigned index = MaxLength);
    WTF_EXPORT_PRIVATE size_t reverseFind(StringImpl*, unsigned index = MaxLength);

    WTF_EXPORT_PRIVATE bool startsWith(const StringImpl*) const;
    WTF_EXPORT_PRIVATE bool startsWith(const StringImpl&) const;
    WTF_EXPORT_PRIVATE bool startsWithIgnoringASCIICase(const StringImpl*) const;
    WTF_EXPORT_PRIVATE bool startsWithIgnoringASCIICase(const StringImpl&) const;
    WTF_EXPORT_PRIVATE bool startsWith(UChar) const;
    WTF_EXPORT_PRIVATE bool startsWith(const char*, unsigned matchLength) const;
    template<unsigned matchLength> bool startsWith(const char (&prefix)[matchLength]) const { return startsWith(prefix, matchLength - 1); }
    WTF_EXPORT_PRIVATE bool hasInfixStartingAt(const StringImpl&, unsigned startOffset) const;

    WTF_EXPORT_PRIVATE bool endsWith(StringImpl*);
    WTF_EXPORT_PRIVATE bool endsWith(StringImpl&);
    WTF_EXPORT_PRIVATE bool endsWithIgnoringASCIICase(const StringImpl*) const;
    WTF_EXPORT_PRIVATE bool endsWithIgnoringASCIICase(const StringImpl&) const;
    WTF_EXPORT_PRIVATE bool endsWith(UChar) const;
    WTF_EXPORT_PRIVATE bool endsWith(const char*, unsigned matchLength) const;
    template<unsigned matchLength> bool endsWith(const char (&prefix)[matchLength]) const { return endsWith(prefix, matchLength - 1); }
    WTF_EXPORT_PRIVATE bool hasInfixEndingAt(const StringImpl&, unsigned endOffset) const;

    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(UChar, UChar);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(UChar, StringImpl*);
    ALWAYS_INLINE Ref<StringImpl> replace(UChar pattern, const char* replacement, unsigned replacementLength) { return replace(pattern, reinterpret_cast<const LChar*>(replacement), replacementLength); }
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(UChar, const LChar*, unsigned replacementLength);
    Ref<StringImpl> replace(UChar, const UChar*, unsigned replacementLength);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(StringImpl*, StringImpl*);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(unsigned index, unsigned length, StringImpl*);

    WTF_EXPORT_PRIVATE UCharDirection defaultWritingDirection(bool* hasStrongDirectionality = nullptr);

#if USE(CF)
    RetainPtr<CFStringRef> createCFString();
#endif

#ifdef __OBJC__
    WTF_EXPORT_PRIVATE operator NSString *();
#endif

#if STRING_STATS
    ALWAYS_INLINE static StringStats& stringStats() { return m_stringStats; }
#endif

    BufferOwnership bufferOwnership() const { return static_cast<BufferOwnership>(m_hashAndFlags & s_hashMaskBufferOwnership); }
    
protected:
    ~StringImpl();

    // Used to create new symbol string that holds an existing [[Description]] string as a substring buffer (BufferSubstring).
    enum CreateSymbolTag { CreateSymbol };
    StringImpl(CreateSymbolTag, const LChar*, unsigned length);
    StringImpl(CreateSymbolTag, const UChar*, unsigned length);

    // Null symbol.
    explicit StringImpl(CreateSymbolTag);

private:
    template<typename> static size_t allocationSize(Checked<size_t> tailElementCount);
    template<typename> static size_t maxInternalLength();
    template<typename> static size_t tailOffset();

    bool requiresCopy() const;
    template<typename T> const T* tailPointer() const;
    template<typename T> T* tailPointer();
    StringImpl* const& substringBuffer() const;
    StringImpl*& substringBuffer();

    enum class CaseConvertType { Upper, Lower };
    template<CaseConvertType, typename CharacterType> static Ref<StringImpl> convertASCIICase(StringImpl&, const CharacterType*, unsigned);

    template<class CodeUnitPredicate> Ref<StringImpl> stripMatchedCharacters(CodeUnitPredicate);
    template<typename CharacterType> ALWAYS_INLINE Ref<StringImpl> removeCharacters(const CharacterType* characters, CodeUnitMatchFunction);
    template<typename CharacterType, class CodeUnitPredicate> Ref<StringImpl> simplifyMatchedCharactersToSpace(CodeUnitPredicate);
    template<typename CharacterType> static Ref<StringImpl> constructInternal(StringImpl&, unsigned);
    template<typename CharacterType> static Ref<StringImpl> createUninitializedInternal(unsigned, CharacterType*&);
    template<typename CharacterType> static Ref<StringImpl> createUninitializedInternalNonEmpty(unsigned, CharacterType*&);
    template<typename CharacterType> static Expected<Ref<StringImpl>, UTF8ConversionError> reallocateInternal(Ref<StringImpl>&&, unsigned, CharacterType*&);
    template<typename CharacterType> static Ref<StringImpl> createInternal(const CharacterType*, unsigned);
    WTF_EXPORT_PRIVATE NEVER_INLINE unsigned hashSlowCase() const;

    // The bottom bit in the ref count indicates a static (immortal) string.
    static const unsigned s_refCountFlagIsStaticString = 0x1;
    static const unsigned s_refCountIncrement = 0x2; // This allows us to ref / deref without disturbing the static string flag.

#if STRING_STATS
    WTF_EXPORT_PRIVATE static StringStats m_stringStats;
#endif

public:
    void assertHashIsCorrect() const;
};

using StaticStringImpl = StringImpl::StaticStringImpl;

static_assert(sizeof(StringImpl) == sizeof(StaticStringImpl), "");

#if !ASSERT_DISABLED

// StringImpls created from StaticStringImpl will ASSERT in the generic ValueCheck<T>::checkConsistency
// as they are not allocated by fastMalloc. We don't currently have any way to detect that case
// so we ignore the consistency check for all StringImpl*.
template<> struct ValueCheck<StringImpl*> {
    static void checkConsistency(const StringImpl*) { }
};

#endif

WTF_EXPORT_PRIVATE bool equal(const StringImpl*, const StringImpl*);
WTF_EXPORT_PRIVATE bool equal(const StringImpl*, const LChar*);
inline bool equal(const StringImpl* a, const char* b) { return equal(a, reinterpret_cast<const LChar*>(b)); }
WTF_EXPORT_PRIVATE bool equal(const StringImpl*, const LChar*, unsigned);
WTF_EXPORT_PRIVATE bool equal(const StringImpl*, const UChar*, unsigned);
inline bool equal(const StringImpl* a, const char* b, unsigned length) { return equal(a, reinterpret_cast<const LChar*>(b), length); }
inline bool equal(const LChar* a, StringImpl* b) { return equal(b, a); }
inline bool equal(const char* a, StringImpl* b) { return equal(b, reinterpret_cast<const LChar*>(a)); }
WTF_EXPORT_PRIVATE bool equal(const StringImpl& a, const StringImpl& b);

WTF_EXPORT_PRIVATE bool equalIgnoringNullity(StringImpl*, StringImpl*);
WTF_EXPORT_PRIVATE bool equalIgnoringNullity(const UChar*, size_t length, StringImpl*);

bool equalIgnoringASCIICase(const StringImpl&, const StringImpl&);
WTF_EXPORT_PRIVATE bool equalIgnoringASCIICase(const StringImpl*, const StringImpl*);
bool equalIgnoringASCIICase(const StringImpl&, const char*);
bool equalIgnoringASCIICase(const StringImpl*, const char*);

WTF_EXPORT_PRIVATE bool equalIgnoringASCIICaseNonNull(const StringImpl*, const StringImpl*);

template<unsigned length> bool equalLettersIgnoringASCIICase(const StringImpl&, const char (&lowercaseLetters)[length]);
template<unsigned length> bool equalLettersIgnoringASCIICase(const StringImpl*, const char (&lowercaseLetters)[length]);

size_t find(const LChar*, unsigned length, CodeUnitMatchFunction, unsigned index = 0);
size_t find(const UChar*, unsigned length, CodeUnitMatchFunction, unsigned index = 0);

template<typename CharacterType> size_t reverseFindLineTerminator(const CharacterType*, unsigned length, unsigned index = StringImpl::MaxLength);
template<typename CharacterType> size_t reverseFind(const CharacterType*, unsigned length, CharacterType matchCharacter, unsigned index = StringImpl::MaxLength);
size_t reverseFind(const UChar*, unsigned length, LChar matchCharacter, unsigned index = StringImpl::MaxLength);
size_t reverseFind(const LChar*, unsigned length, UChar matchCharacter, unsigned index = StringImpl::MaxLength);

template<size_t inlineCapacity> bool equalIgnoringNullity(const Vector<UChar, inlineCapacity>&, StringImpl*);

template<typename CharacterType1, typename CharacterType2> int codePointCompare(const CharacterType1*, unsigned length1, const CharacterType2*, unsigned length2);
int codePointCompare(const StringImpl*, const StringImpl*);

// FIXME: Should rename this to make clear it uses the Unicode definition of whitespace.
// Most WebKit callers don't want that would use isASCIISpace or isHTMLSpace instead.
bool isSpaceOrNewline(UChar32);

template<typename CharacterType> unsigned lengthOfNullTerminatedString(const CharacterType*);

// StringHash is the default hash for StringImpl* and RefPtr<StringImpl>
template<typename T> struct DefaultHash;
template<> struct DefaultHash<StringImpl*> {
    typedef StringHash Hash;
};
template<> struct DefaultHash<RefPtr<StringImpl>> {
    typedef StringHash Hash;
};

#define MAKE_STATIC_STRING_IMPL(characters) ([] { \
        static StaticStringImpl impl(characters); \
        return &impl; \
    }())

template<> ALWAYS_INLINE Ref<StringImpl> StringImpl::constructInternal<LChar>(StringImpl& string, unsigned length)
{
    return adoptRef(*new (NotNull, &string) StringImpl { length, Force8BitConstructor });
}

template<> ALWAYS_INLINE Ref<StringImpl> StringImpl::constructInternal<UChar>(StringImpl& string, unsigned length)
{
    return adoptRef(*new (NotNull, &string) StringImpl { length });
}

template<> ALWAYS_INLINE const LChar* StringImpl::characters<LChar>() const
{
    return characters8();
}

template<> ALWAYS_INLINE const UChar* StringImpl::characters<UChar>() const
{
    return characters16();
}

inline size_t find(const LChar* characters, unsigned length, CodeUnitMatchFunction matchFunction, unsigned index)
{
    while (index < length) {
        if (matchFunction(characters[index]))
            return index;
        ++index;
    }
    return notFound;
}

inline size_t find(const UChar* characters, unsigned length, CodeUnitMatchFunction matchFunction, unsigned index)
{
    while (index < length) {
        if (matchFunction(characters[index]))
            return index;
        ++index;
    }
    return notFound;
}

template<typename CharacterType> inline size_t reverseFindLineTerminator(const CharacterType* characters, unsigned length, unsigned index)
{
    if (!length)
        return notFound;
    if (index >= length)
        index = length - 1;
    auto character = characters[index];
    while (character != '\n' && character != '\r') {
        if (!index--)
            return notFound;
        character = characters[index];
    }
    return index;
}

template<typename CharacterType> inline size_t reverseFind(const CharacterType* characters, unsigned length, CharacterType matchCharacter, unsigned index)
{
    if (!length)
        return notFound;
    if (index >= length)
        index = length - 1;
    while (characters[index] != matchCharacter) {
        if (!index--)
            return notFound;
    }
    return index;
}

ALWAYS_INLINE size_t reverseFind(const UChar* characters, unsigned length, LChar matchCharacter, unsigned index)
{
    return reverseFind(characters, length, static_cast<UChar>(matchCharacter), index);
}

inline size_t reverseFind(const LChar* characters, unsigned length, UChar matchCharacter, unsigned index)
{
    if (matchCharacter & ~0xFF)
        return notFound;
    return reverseFind(characters, length, static_cast<LChar>(matchCharacter), index);
}

inline size_t StringImpl::find(LChar character, unsigned start)
{
    if (is8Bit())
        return WTF::find(characters8(), m_length, character, start);
    return WTF::find(characters16(), m_length, character, start);
}

ALWAYS_INLINE size_t StringImpl::find(char character, unsigned start)
{
    return find(static_cast<LChar>(character), start);
}

inline size_t StringImpl::find(UChar character, unsigned start)
{
    if (is8Bit())
        return WTF::find(characters8(), m_length, character, start);
    return WTF::find(characters16(), m_length, character, start);
}

template<size_t inlineCapacity> inline bool equalIgnoringNullity(const Vector<UChar, inlineCapacity>& a, StringImpl* b)
{
    return equalIgnoringNullity(a.data(), a.size(), b);
}

template<typename CharacterType1, typename CharacterType2> inline int codePointCompare(const CharacterType1* characters1, unsigned length1, const CharacterType2* characters2, unsigned length2)
{
    unsigned commonLength = std::min(length1, length2);

    unsigned position = 0;
    while (position < commonLength && *characters1 == *characters2) {
        ++characters1;
        ++characters2;
        ++position;
    }

    if (position < commonLength)
        return (characters1[0] > characters2[0]) ? 1 : -1;

    if (length1 == length2)
        return 0;
    return (length1 > length2) ? 1 : -1;
}

inline int codePointCompare(const StringImpl* string1, const StringImpl* string2)
{
    // FIXME: Should null strings compare as less than empty strings rather than equal to them?
    if (!string1)
        return (string2 && string2->length()) ? -1 : 0;
    if (!string2)
        return string1->length() ? 1 : 0;

    bool string1Is8Bit = string1->is8Bit();
    bool string2Is8Bit = string2->is8Bit();
    if (string1Is8Bit) {
        if (string2Is8Bit)
            return codePointCompare(string1->characters8(), string1->length(), string2->characters8(), string2->length());
        return codePointCompare(string1->characters8(), string1->length(), string2->characters16(), string2->length());
    }
    if (string2Is8Bit)
        return codePointCompare(string1->characters16(), string1->length(), string2->characters8(), string2->length());
    return codePointCompare(string1->characters16(), string1->length(), string2->characters16(), string2->length());
}

inline bool isSpaceOrNewline(UChar32 character)
{
    // Use isASCIISpace() for all Latin-1 characters. This will include newlines, which aren't included in Unicode DirWS.
    return character <= 0xFF ? isASCIISpace(character) : u_charDirection(character) == U_WHITE_SPACE_NEUTRAL;
}

template<typename CharacterType> inline unsigned lengthOfNullTerminatedString(const CharacterType* string)
{
    ASSERT(string);
    size_t length = 0;
    while (string[length])
        ++length;

    RELEASE_ASSERT(length < StringImpl::MaxLength);
    return static_cast<unsigned>(length);
}

inline StringImplShape::StringImplShape(unsigned refCount, unsigned length, const LChar* data8, unsigned hashAndFlags)
    : m_refCount(refCount)
    , m_length(length)
    , m_data8(data8)
    , m_hashAndFlags(hashAndFlags)
{
}

inline StringImplShape::StringImplShape(unsigned refCount, unsigned length, const UChar* data16, unsigned hashAndFlags)
    : m_refCount(refCount)
    , m_length(length)
    , m_data16(data16)
    , m_hashAndFlags(hashAndFlags)
{
}

template<unsigned characterCount> constexpr StringImplShape::StringImplShape(unsigned refCount, unsigned length, const char (&characters)[characterCount], unsigned hashAndFlags, ConstructWithConstExprTag)
    : m_refCount(refCount)
    , m_length(length)
    , m_data8Char(characters)
    , m_hashAndFlags(hashAndFlags)
{
}

template<unsigned characterCount> constexpr StringImplShape::StringImplShape(unsigned refCount, unsigned length, const char16_t (&characters)[characterCount], unsigned hashAndFlags, ConstructWithConstExprTag)
    : m_refCount(refCount)
    , m_length(length)
    , m_data16Char(characters)
    , m_hashAndFlags(hashAndFlags)
{
}

inline Ref<StringImpl> StringImpl::isolatedCopy() const
{
    if (!requiresCopy()) {
        if (is8Bit())
            return StringImpl::createWithoutCopying(m_data8, m_length);
        return StringImpl::createWithoutCopying(m_data16, m_length);
    }

    if (is8Bit())
        return create(m_data8, m_length);
    return create(m_data16, m_length);
}

inline bool StringImpl::isAllASCII() const
{
    if (is8Bit())
        return charactersAreAllASCII(characters8(), length());
    return charactersAreAllASCII(characters16(), length());
}

inline bool StringImpl::isAllLatin1() const
{
    if (is8Bit())
        return true;
    auto* characters = characters16();
    UChar ored = 0;
    for (size_t i = 0; i < length(); ++i)
        ored |= characters[i];
    return !(ored & 0xFF00);
}

template<bool isSpecialCharacter(UChar), typename CharacterType> inline bool isAllSpecialCharacters(const CharacterType* characters, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (!isSpecialCharacter(characters[i]))
            return false;
    }
    return true;
}

template<bool isSpecialCharacter(UChar)> inline bool StringImpl::isAllSpecialCharacters() const
{
    if (is8Bit())
        return WTF::isAllSpecialCharacters<isSpecialCharacter>(characters8(), length());
    return WTF::isAllSpecialCharacters<isSpecialCharacter>(characters16(), length());
}

inline StringImpl::StringImpl(unsigned length, Force8Bit)
    : StringImplShape(s_refCountIncrement, length, tailPointer<LChar>(), s_hashFlag8BitBuffer | StringNormal | BufferInternal)
{
    ASSERT(m_data8);
    ASSERT(m_length);

    STRING_STATS_ADD_8BIT_STRING(m_length);
}

inline StringImpl::StringImpl(unsigned length)
    : StringImplShape(s_refCountIncrement, length, tailPointer<UChar>(), StringNormal | BufferInternal)
{
    ASSERT(m_data16);
    ASSERT(m_length);

    STRING_STATS_ADD_16BIT_STRING(m_length);
}

inline StringImpl::StringImpl(MallocPtr<LChar> characters, unsigned length)
    : StringImplShape(s_refCountIncrement, length, characters.leakPtr(), s_hashFlag8BitBuffer | StringNormal | BufferOwned)
{
    ASSERT(m_data8);
    ASSERT(m_length);

    STRING_STATS_ADD_8BIT_STRING(m_length);
}

inline StringImpl::StringImpl(const UChar* characters, unsigned length, ConstructWithoutCopyingTag)
    : StringImplShape(s_refCountIncrement, length, characters, StringNormal | BufferInternal)
{
    ASSERT(m_data16);
    ASSERT(m_length);

    STRING_STATS_ADD_16BIT_STRING(m_length);
}

inline StringImpl::StringImpl(const LChar* characters, unsigned length, ConstructWithoutCopyingTag)
    : StringImplShape(s_refCountIncrement, length, characters, s_hashFlag8BitBuffer | StringNormal | BufferInternal)
{
    ASSERT(m_data8);
    ASSERT(m_length);

    STRING_STATS_ADD_8BIT_STRING(m_length);
}

inline StringImpl::StringImpl(MallocPtr<UChar> characters, unsigned length)
    : StringImplShape(s_refCountIncrement, length, characters.leakPtr(), StringNormal | BufferOwned)
{
    ASSERT(m_data16);
    ASSERT(m_length);

    STRING_STATS_ADD_16BIT_STRING(m_length);
}

inline StringImpl::StringImpl(const LChar* characters, unsigned length, Ref<StringImpl>&& base)
    : StringImplShape(s_refCountIncrement, length, characters, s_hashFlag8BitBuffer | StringNormal | BufferSubstring)
{
    ASSERT(is8Bit());
    ASSERT(m_data8);
    ASSERT(m_length);
    ASSERT(base->bufferOwnership() != BufferSubstring);

    substringBuffer() = &base.leakRef();

    STRING_STATS_ADD_8BIT_STRING2(m_length, true);
}

inline StringImpl::StringImpl(const UChar* characters, unsigned length, Ref<StringImpl>&& base)
    : StringImplShape(s_refCountIncrement, length, characters, StringNormal | BufferSubstring)
{
    ASSERT(!is8Bit());
    ASSERT(m_data16);
    ASSERT(m_length);
    ASSERT(base->bufferOwnership() != BufferSubstring);

    substringBuffer() = &base.leakRef();

    STRING_STATS_ADD_16BIT_STRING2(m_length, true);
}

template<size_t inlineCapacity> inline Ref<StringImpl> StringImpl::create8BitIfPossible(const Vector<UChar, inlineCapacity>& vector)
{
    return create8BitIfPossible(vector.data(), vector.size());
}

ALWAYS_INLINE Ref<StringImpl> StringImpl::createSubstringSharingImpl(StringImpl& rep, unsigned offset, unsigned length)
{
    ASSERT(length <= rep.length());

    if (!length)
        return *empty();

    auto* ownerRep = ((rep.bufferOwnership() == BufferSubstring) ? rep.substringBuffer() : &rep);

    // We allocate a buffer that contains both the StringImpl struct as well as the pointer to the owner string.
    auto* stringImpl = static_cast<StringImpl*>(fastMalloc(allocationSize<StringImpl*>(1)));
    if (rep.is8Bit())
        return adoptRef(*new (NotNull, stringImpl) StringImpl(rep.m_data8 + offset, length, *ownerRep));
    return adoptRef(*new (NotNull, stringImpl) StringImpl(rep.m_data16 + offset, length, *ownerRep));
}

template<unsigned characterCount> ALWAYS_INLINE Ref<StringImpl> StringImpl::createFromLiteral(const char (&characters)[characterCount])
{
    COMPILE_ASSERT(characterCount > 1, StringImplFromLiteralNotEmpty);
    COMPILE_ASSERT((characterCount - 1 <= ((unsigned(~0) - sizeof(StringImpl)) / sizeof(LChar))), StringImplFromLiteralCannotOverflow);

    return createWithoutCopying(reinterpret_cast<const LChar*>(characters), characterCount - 1);
}

template<typename CharacterType> ALWAYS_INLINE RefPtr<StringImpl> StringImpl::tryCreateUninitialized(unsigned length, CharacterType*& output)
{
    if (!length) {
        output = nullptr;
        return empty();
    }

    if (length > maxInternalLength<CharacterType>()) {
        output = nullptr;
        return nullptr;
    }
    StringImpl* result;
    if (!tryFastMalloc(allocationSize<CharacterType>(length)).getValue(result)) {
        output = nullptr;
        return nullptr;
    }
    output = result->tailPointer<CharacterType>();

    return constructInternal<CharacterType>(*result, length);
}

template<typename CharacterType, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity>
inline Ref<StringImpl> StringImpl::adopt(Vector<CharacterType, inlineCapacity, OverflowHandler, minCapacity>&& vector)
{
    if (size_t size = vector.size()) {
        ASSERT(vector.data());
        if (size > MaxLength)
            CRASH();
        return adoptRef(*new StringImpl(vector.releaseBuffer(), size));
    }
    return *empty();
}

inline size_t StringImpl::cost() const
{
    // For substrings, return the cost of the base string.
    if (bufferOwnership() == BufferSubstring)
        return substringBuffer()->cost();

    // Note: we must not alter the m_hashAndFlags field in instances of StaticStringImpl.
    // We ensure this by pre-setting the s_hashFlagDidReportCost bit in all instances of
    // StaticStringImpl. As a result, StaticStringImpl instances will always return a cost of
    // 0 here and avoid modifying m_hashAndFlags.
    if (m_hashAndFlags & s_hashFlagDidReportCost)
        return 0;

    m_hashAndFlags |= s_hashFlagDidReportCost;
    size_t result = m_length;
    if (!is8Bit())
        result <<= 1;
    return result;
}

inline size_t StringImpl::costDuringGC()
{
    if (isStatic())
        return 0;

    if (bufferOwnership() == BufferSubstring)
        return divideRoundedUp(substringBuffer()->costDuringGC(), refCount());

    size_t result = m_length;
    if (!is8Bit())
        result <<= 1;
    return divideRoundedUp(result, refCount());
}

inline void StringImpl::setIsAtomic(bool isAtomic)
{
    ASSERT(!isStatic());
    ASSERT(!isSymbol());
    if (isAtomic)
        m_hashAndFlags |= s_hashFlagStringKindIsAtomic;
    else
        m_hashAndFlags &= ~s_hashFlagStringKindIsAtomic;
}

inline void StringImpl::setHash(unsigned hash) const
{
    // The high bits of 'hash' are always empty, but we prefer to store our flags
    // in the low bits because it makes them slightly more efficient to access.
    // So, we shift left and right when setting and getting our hash code.

    ASSERT(!hasHash());
    ASSERT(!isStatic());
    // Multiple clients assume that StringHasher is the canonical string hash function.
    ASSERT(hash == (is8Bit() ? StringHasher::computeHashAndMaskTop8Bits(m_data8, m_length) : StringHasher::computeHashAndMaskTop8Bits(m_data16, m_length)));
    ASSERT(!(hash & (s_flagMask << (8 * sizeof(hash) - s_flagCount)))); // Verify that enough high bits are empty.

    hash <<= s_flagCount;
    ASSERT(!(hash & m_hashAndFlags)); // Verify that enough low bits are empty after shift.
    ASSERT(hash); // Verify that 0 is a valid sentinel hash value.

    m_hashAndFlags |= hash; // Store hash with flags in low bits.
}

inline void StringImpl::ref()
{
    STRING_STATS_REF_STRING(*this);

    m_refCount += s_refCountIncrement;
}

inline void StringImpl::deref()
{
    STRING_STATS_DEREF_STRING(*this);

    unsigned tempRefCount = m_refCount - s_refCountIncrement;
    if (!tempRefCount) {
        StringImpl::destroy(this);
        return;
    }
    m_refCount = tempRefCount;
}

template<typename CharacterType> inline void StringImpl::copyCharacters(CharacterType* destination, const CharacterType* source, unsigned numCharacters)
{
    if (numCharacters == 1) {
        *destination = *source;
        return;
    }
    memcpy(destination, source, numCharacters * sizeof(CharacterType));
}

ALWAYS_INLINE void StringImpl::copyCharacters(UChar* destination, const LChar* source, unsigned numCharacters)
{
    for (unsigned i = 0; i < numCharacters; ++i)
        destination[i] = source[i];
}

inline UChar StringImpl::at(unsigned i) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(i < m_length);
    return is8Bit() ? m_data8[i] : m_data16[i];
}

inline StringImpl::StringImpl(CreateSymbolTag, const LChar* characters, unsigned length)
    : StringImplShape(s_refCountIncrement, length, characters, s_hashFlag8BitBuffer | StringSymbol | BufferSubstring)
{
    ASSERT(is8Bit());
    ASSERT(m_data8);
    STRING_STATS_ADD_8BIT_STRING2(m_length, true);
}

inline StringImpl::StringImpl(CreateSymbolTag, const UChar* characters, unsigned length)
    : StringImplShape(s_refCountIncrement, length, characters, StringSymbol | BufferSubstring)
{
    ASSERT(!is8Bit());
    ASSERT(m_data16);
    STRING_STATS_ADD_16BIT_STRING2(m_length, true);
}

inline StringImpl::StringImpl(CreateSymbolTag)
    : StringImplShape(s_refCountIncrement, 0, empty()->characters8(), s_hashFlag8BitBuffer | StringSymbol | BufferSubstring)
{
    ASSERT(is8Bit());
    ASSERT(m_data8);
    STRING_STATS_ADD_8BIT_STRING2(m_length, true);
}

template<typename T> inline size_t StringImpl::allocationSize(Checked<size_t> tailElementCount)
{
    return (tailOffset<T>() + tailElementCount * sizeof(T)).unsafeGet();
}

template<typename CharacterType>
inline size_t StringImpl::maxInternalLength()
{
    // In order to not overflow the unsigned length, the check for (std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) is needed when sizeof(CharacterType) == 2.
    return std::min(static_cast<size_t>(MaxLength), (std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) / sizeof(CharacterType));
}

template<typename T> inline size_t StringImpl::tailOffset()
{
#if COMPILER(MSVC)
    // MSVC doesn't support alignof yet.
    return roundUpToMultipleOf<sizeof(T)>(sizeof(StringImpl));
#else
    return roundUpToMultipleOf<alignof(T)>(offsetof(StringImpl, m_hashAndFlags) + sizeof(StringImpl::m_hashAndFlags));
#endif
}

inline bool StringImpl::requiresCopy() const
{
    if (bufferOwnership() != BufferInternal)
        return true;

    if (is8Bit())
        return m_data8 == tailPointer<LChar>();
    return m_data16 == tailPointer<UChar>();
}

template<typename T> inline const T* StringImpl::tailPointer() const
{
    return reinterpret_cast_ptr<const T*>(reinterpret_cast<const uint8_t*>(this) + tailOffset<T>());
}

template<typename T> inline T* StringImpl::tailPointer()
{
    return reinterpret_cast_ptr<T*>(reinterpret_cast<uint8_t*>(this) + tailOffset<T>());
}

inline StringImpl* const& StringImpl::substringBuffer() const
{
    ASSERT(bufferOwnership() == BufferSubstring);

    return *tailPointer<StringImpl*>();
}

inline StringImpl*& StringImpl::substringBuffer()
{
    ASSERT(bufferOwnership() == BufferSubstring);

    return *tailPointer<StringImpl*>();
}

inline void StringImpl::assertHashIsCorrect() const
{
    ASSERT(existingHash() == StringHasher::computeHashAndMaskTop8Bits(characters8(), length()));
}

template<unsigned characterCount> constexpr StringImpl::StaticStringImpl::StaticStringImpl(const char (&characters)[characterCount], StringKind stringKind)
    : StringImplShape(s_refCountFlagIsStaticString, characterCount - 1, characters,
        s_hashFlag8BitBuffer | s_hashFlagDidReportCost | stringKind | BufferInternal | (StringHasher::computeLiteralHashAndMaskTop8Bits(characters) << s_flagCount), ConstructWithConstExpr)
{
}

template<unsigned characterCount> constexpr StringImpl::StaticStringImpl::StaticStringImpl(const char16_t (&characters)[characterCount], StringKind stringKind)
    : StringImplShape(s_refCountFlagIsStaticString, characterCount - 1, characters,
        s_hashFlagDidReportCost | stringKind | BufferInternal | (StringHasher::computeLiteralHashAndMaskTop8Bits(characters) << s_flagCount), ConstructWithConstExpr)
{
}

inline StringImpl::StaticStringImpl::operator StringImpl&()
{
    return *reinterpret_cast<StringImpl*>(this);
}

inline bool equalIgnoringASCIICase(const StringImpl& a, const StringImpl& b)
{
    return equalIgnoringASCIICaseCommon(a, b);
}

inline bool equalIgnoringASCIICase(const StringImpl& a, const char* b)
{
    return equalIgnoringASCIICaseCommon(a, b);
}

inline bool equalIgnoringASCIICase(const StringImpl* a, const char* b)
{
    return a && equalIgnoringASCIICase(*a, b);
}

template<unsigned length> inline bool startsWithLettersIgnoringASCIICase(const StringImpl& string, const char (&lowercaseLetters)[length])
{
    return startsWithLettersIgnoringASCIICaseCommon(string, lowercaseLetters);
}

template<unsigned length> inline bool startsWithLettersIgnoringASCIICase(const StringImpl* string, const char (&lowercaseLetters)[length])
{
    return string && startsWithLettersIgnoringASCIICase(*string, lowercaseLetters);
}

template<unsigned length> inline bool equalLettersIgnoringASCIICase(const StringImpl& string, const char (&lowercaseLetters)[length])
{
    return equalLettersIgnoringASCIICaseCommon(string, lowercaseLetters);
}

template<unsigned length> inline bool equalLettersIgnoringASCIICase(const StringImpl* string, const char (&lowercaseLetters)[length])
{
    return string && equalLettersIgnoringASCIICase(*string, lowercaseLetters);
}

} // namespace WTF

using WTF::StaticStringImpl;
using WTF::StringImpl;
using WTF::equal;
using WTF::isLatin1;
