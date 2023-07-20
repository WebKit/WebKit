/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005-2020 Apple Inc. All rights reserved.
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
#include <wtf/CompactPtr.h>
#include <wtf/DebugHeap.h>
#include <wtf/Expected.h>
#include <wtf/MathExtras.h>
#include <wtf/Packed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/ConversionMode.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringHasher.h>
#include <wtf/text/UTF8ConversionError.h>
#include <wtf/unicode/UTF8Conversion.h>

#if USE(CF)
typedef const struct __CFString * CFStringRef;
#endif

#ifdef __OBJC__
@class NSString;
#endif

#if HAVE(36BIT_ADDRESS)
#define STRING_IMPL_ALIGNMENT alignas(16)
#else
#define STRING_IMPL_ALIGNMENT
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
struct HashTranslatorASCIILiteral;
struct LCharBufferTranslator;
struct StringViewHashTranslator;
struct SubstringTranslator;
struct UCharBufferTranslator;

template<typename> class RetainPtr;

template<typename> struct BufferFromStaticDataTranslator;
template<typename> struct HashAndCharactersTranslator;

// Define STRING_STATS to 1 turn on runtime statistics of string sizes and memory usage.
#define STRING_STATS 0

template<bool isSpecialCharacter(UChar), typename CharacterType> bool containsOnly(const CharacterType*, size_t length);

#if STRING_STATS

struct StringStats {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
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

    static constexpr unsigned s_printStringStatsFrequency = 5000;
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

class STRING_IMPL_ALIGNMENT StringImplShape {
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

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringImpl);
class StringImpl : private StringImplShape {
    WTF_MAKE_NONCOPYABLE(StringImpl);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StringImpl);

    friend class AtomStringImpl;
    friend class JSC::LLInt::Data;
    friend class JSC::LLIntOffsetsExtractor;
    friend class PrivateSymbolImpl;
    friend class RegisteredSymbolImpl;
    friend class SymbolImpl;
    friend class ExternalStringImpl;

    friend struct WTF::CStringTranslator;
    friend struct WTF::HashAndUTF8CharactersTranslator;
    friend struct WTF::HashTranslatorASCIILiteral;
    friend struct WTF::LCharBufferTranslator;
    friend struct WTF::StringViewHashTranslator;
    friend struct WTF::SubstringTranslator;
    friend struct WTF::UCharBufferTranslator;

    template<typename> friend struct WTF::BufferFromStaticDataTranslator;
    template<typename> friend struct WTF::HashAndCharactersTranslator;

public:
    enum BufferOwnership { BufferInternal, BufferOwned, BufferSubstring, BufferExternal };

    static constexpr unsigned MaxLength = StringImplShape::MaxLength;

    // The bottom 6 bits in the hash are flags, but reserve 8 bits since StringHash only has 24 bits anyway.
    static constexpr const unsigned s_flagCount = 8;

private:
    static constexpr const unsigned s_flagMask = (1u << s_flagCount) - 1;
    static_assert(s_flagCount <= StringHasher::flagCount, "StringHasher reserves enough bits for StringImpl flags");
    static constexpr const unsigned s_flagStringKindCount = 4;

    static constexpr const unsigned s_hashZeroValue = 0;
    static constexpr const unsigned s_hashFlagStringKindIsAtom = 1u << (s_flagStringKindCount);
    static constexpr const unsigned s_hashFlagStringKindIsSymbol = 1u << (s_flagStringKindCount + 1);
    static constexpr const unsigned s_hashMaskStringKind = s_hashFlagStringKindIsAtom | s_hashFlagStringKindIsSymbol;
    static constexpr const unsigned s_hashFlagDidReportCost = 1u << 3;
    static constexpr const unsigned s_hashFlag8BitBuffer = 1u << 2;
    static constexpr const unsigned s_hashMaskBufferOwnership = (1u << 0) | (1u << 1);

    enum StringKind {
        StringNormal = 0u, // non-symbol, non-atomic
        StringAtom = s_hashFlagStringKindIsAtom, // non-symbol, atomic
        StringSymbol = s_hashFlagStringKindIsSymbol, // symbol, non-atomic
    };

    // Create a normal 8-bit string with internal storage (BufferInternal).
    enum Force8Bit { Force8BitConstructor };
    StringImpl(unsigned length, Force8Bit);

    // Create a normal 16-bit string with internal storage (BufferInternal).
    explicit StringImpl(unsigned length);

    // Create a StringImpl adopting ownership of the provided buffer (BufferOwned).
    template<typename Malloc> StringImpl(MallocPtr<LChar, Malloc>, unsigned length);
    template<typename Malloc> StringImpl(MallocPtr<UChar, Malloc>, unsigned length);
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
    ALWAYS_INLINE static Ref<StringImpl> create(const char* characters, unsigned length) { return create(reinterpret_cast<const LChar*>(characters), length); }
    WTF_EXPORT_PRIVATE static Ref<StringImpl> create8BitIfPossible(const UChar*, unsigned length);
    template<size_t inlineCapacity> static Ref<StringImpl> create8BitIfPossible(const Vector<UChar, inlineCapacity>&);

    // Not using create() naming to encourage developers to call create(ASCIILiteral) when they have a string literal.
    ALWAYS_INLINE static Ref<StringImpl> createFromCString(const char* characters) { return create(characters, strlen(characters)); }

    static Ref<StringImpl> createSubstringSharingImpl(StringImpl&, unsigned offset, unsigned length);

    ALWAYS_INLINE static Ref<StringImpl> create(ASCIILiteral literal) { return createWithoutCopying(literal.characters8(), literal.length()); }

    static Ref<StringImpl> createWithoutCopying(const UChar* characters, unsigned length) { return length ? createWithoutCopyingNonEmpty(characters, length) : Ref { *empty() }; }
    static Ref<StringImpl> createWithoutCopying(const LChar* characters, unsigned length) { return length ? createWithoutCopyingNonEmpty(characters, length) : Ref { *empty() }; }
    ALWAYS_INLINE static Ref<StringImpl> createWithoutCopying(const char* characters, unsigned length) { return createWithoutCopying(reinterpret_cast<const LChar*>(characters), length); }

    WTF_EXPORT_PRIVATE static Ref<StringImpl> createUninitialized(unsigned length, LChar*&);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createUninitialized(unsigned length, UChar*&);
    template<typename CharacterType> static RefPtr<StringImpl> tryCreateUninitialized(unsigned length, CharacterType*&);

    static Ref<StringImpl> createByReplacingInCharacters(const LChar*, unsigned length, UChar target, UChar replacement, unsigned indexOfFirstTargetCharacter);
    static Ref<StringImpl> createByReplacingInCharacters(const UChar*, unsigned length, UChar target, UChar replacement, unsigned indexOfFirstTargetCharacter);

    static Ref<StringImpl> createStaticStringImpl(const char* characters, unsigned length)
    {
        ASSERT(charactersAreAllASCII(bitwise_cast<const LChar*>(characters), length));
        return createStaticStringImpl(bitwise_cast<const LChar*>(characters), length);
    }
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createStaticStringImpl(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createStaticStringImpl(const UChar*, unsigned length);

    // Reallocate the StringImpl. The originalString must be only owned by the Ref,
    // and the buffer ownership must be BufferInternal. Just like the input pointer of realloc(),
    // the originalString can't be used after this function.
    static Ref<StringImpl> reallocate(Ref<StringImpl>&& originalString, unsigned length, LChar*& data);
    static Ref<StringImpl> reallocate(Ref<StringImpl>&& originalString, unsigned length, UChar*& data);
    static Expected<Ref<StringImpl>, UTF8ConversionError> tryReallocate(Ref<StringImpl>&& originalString, unsigned length, LChar*& data);
    static Expected<Ref<StringImpl>, UTF8ConversionError> tryReallocate(Ref<StringImpl>&& originalString, unsigned length, UChar*& data);

    static unsigned flagsOffset() { return OBJECT_OFFSETOF(StringImpl, m_hashAndFlags); }
    static constexpr unsigned flagIs8Bit() { return s_hashFlag8BitBuffer; }
    static constexpr unsigned flagIsAtom() { return s_hashFlagStringKindIsAtom; }
    static constexpr unsigned flagIsSymbol() { return s_hashFlagStringKindIsSymbol; }
    static constexpr unsigned maskStringKind() { return s_hashMaskStringKind; }
    static unsigned dataOffset() { return OBJECT_OFFSETOF(StringImpl, m_data8); }

    template<typename CharacterType, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
    static Ref<StringImpl> adopt(Vector<CharacterType, inlineCapacity, OverflowHandler, minCapacity, Malloc>&&);

    WTF_EXPORT_PRIVATE static Ref<StringImpl> adopt(StringBuffer<UChar>&&);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> adopt(StringBuffer<LChar>&&);

    unsigned length() const { return m_length; }
    static ptrdiff_t lengthMemoryOffset() { return OBJECT_OFFSETOF(StringImpl, m_length); }
    bool isEmpty() const { return !m_length; }

    bool is8Bit() const { return m_hashAndFlags & s_hashFlag8BitBuffer; }
    ALWAYS_INLINE const LChar* characters8() const { ASSERT(is8Bit()); return m_data8; }
    ALWAYS_INLINE const UChar* characters16() const { ASSERT(!is8Bit() || isEmpty()); return m_data16; }

    template<typename CharacterType> const CharacterType* characters() const;

    size_t cost() const;
    size_t costDuringGC();

    WTF_EXPORT_PRIVATE size_t sizeInBytes() const;

    bool isSymbol() const { return m_hashAndFlags & s_hashFlagStringKindIsSymbol; }
    bool isAtom() const { return m_hashAndFlags & s_hashFlagStringKindIsAtom; }
    void setIsAtom(bool);
    
    bool isExternal() const { return bufferOwnership() == BufferExternal; }

    bool isSubString() const { return bufferOwnership() == BufferSubstring; }

    static WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> utf8ForCharacters(const LChar* characters, unsigned length);
    static WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> utf8ForCharacters(const UChar* characters, unsigned length, ConversionMode = LenientConversion);
    static WTF_EXPORT_PRIVATE Expected<size_t, UTF8ConversionError> utf8ForCharactersIntoBuffer(const UChar* characters, unsigned length, ConversionMode, Vector<char, 1024>&);

    template<typename Func>
    static Expected<std::invoke_result_t<Func, std::span<const char>>, UTF8ConversionError> tryGetUTF8ForCharacters(const Func&, const LChar* characters, unsigned length);
    template<typename Func>
    static Expected<std::invoke_result_t<Func, std::span<const char>>, UTF8ConversionError> tryGetUTF8ForCharacters(const Func&, const UChar* characters, unsigned length, ConversionMode = LenientConversion);

    template<typename Func>
    Expected<std::invoke_result_t<Func, std::span<const char>>, UTF8ConversionError> tryGetUTF8(const Func&, ConversionMode = LenientConversion) const;
    WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> tryGetUTF8(ConversionMode = LenientConversion) const;
    WTF_EXPORT_PRIVATE CString utf8(ConversionMode = LenientConversion) const;

private:
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

    SUPPRESS_TSAN bool isStatic() const { return m_refCount & s_refCountFlagIsStaticString; }

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
        //    b. setIsAtom() is never called on a StaticStringImpl.
        //       setIsAtom() asserts !isStatic().
        //    c. setHash() is never called on a StaticStringImpl.
        //       StaticStringImpl's constructor sets the hash on construction.
        //       StringImpl::hash() only sets a new hash iff !hasHash().
        //       Additionally, StringImpl::setHash() asserts hasHash() and !isStatic().

        template<unsigned characterCount> explicit constexpr StaticStringImpl(const char (&characters)[characterCount], StringKind = StringNormal);
        template<unsigned characterCount> explicit constexpr StaticStringImpl(const char16_t (&characters)[characterCount], StringKind = StringNormal);
        operator StringImpl&();
    };

    WTF_EXPORT_PRIVATE static StaticStringImpl s_emptyAtomString;
    ALWAYS_INLINE static StringImpl* empty() { return reinterpret_cast<StringImpl*>(&s_emptyAtomString); }

    // FIXME: Do these functions really belong in StringImpl?
    template<typename CharacterType>
    ALWAYS_INLINE static void copyCharacters(CharacterType* destination, const CharacterType* source, unsigned length)
    {
        return copyElements(destination, source, length);
    }

    ALWAYS_INLINE static void copyCharacters(UChar* destination, const LChar* source, unsigned length)
    {
        static_assert(sizeof(UChar) == sizeof(uint16_t));
        static_assert(sizeof(LChar) == sizeof(uint8_t));
        return copyElements(bitwise_cast<uint16_t*>(destination), bitwise_cast<const uint8_t*>(source), length);
    }

    ALWAYS_INLINE static void copyCharacters(LChar* destination, const UChar* source, unsigned length)
    {
        static_assert(sizeof(UChar) == sizeof(uint16_t));
        static_assert(sizeof(LChar) == sizeof(uint8_t));
#if ASSERT_ENABLED
        for (unsigned i = 0; i < length; ++i)
            ASSERT(isLatin1(source[i]));
#endif
        return copyElements(bitwise_cast<uint8_t*>(destination), bitwise_cast<const uint16_t*>(source), length);
    }

    // Some string features, like reference counting and the atomicity flag, are not
    // thread-safe. We achieve thread safety by isolation, giving each thread
    // its own copy of the string.
    Ref<StringImpl> isolatedCopy() const;

    WTF_EXPORT_PRIVATE Ref<StringImpl> substring(unsigned position, unsigned length = MaxLength);

    UChar at(unsigned) const;
    UChar operator[](unsigned i) const { return at(i); }
    WTF_EXPORT_PRIVATE UChar32 characterStartingAt(unsigned);

    // FIXME: Like the strict functions above, these give false for "ok" when there is trailing garbage.
    // Like the non-strict functions above, these return the value when there is trailing garbage.
    // It would be better if these were more consistent with the above functions instead.
    double toDouble(bool* ok = nullptr);
    float toFloat(bool* ok = nullptr);

    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToASCIILowercase();
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToASCIIUppercase();
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToLowercaseWithoutLocale();
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned);
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToUppercaseWithoutLocale();
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToLowercaseWithLocale(const AtomString& localeIdentifier);
    WTF_EXPORT_PRIVATE Ref<StringImpl> convertToUppercaseWithLocale(const AtomString& localeIdentifier);

    Ref<StringImpl> foldCase();

    WTF_EXPORT_PRIVATE Ref<StringImpl> simplifyWhiteSpace(CodeUnitMatchFunction);

    WTF_EXPORT_PRIVATE Ref<StringImpl> trim(CodeUnitMatchFunction);
    template<typename Predicate> Ref<StringImpl> removeCharacters(const Predicate&);

    bool containsOnlyASCII() const;
    bool containsOnlyLatin1() const;
    template<bool isSpecialCharacter(UChar)> bool containsOnly() const;

    size_t find(LChar character, unsigned start = 0);
    size_t find(char character, unsigned start = 0);
    size_t find(UChar character, unsigned start = 0);
    template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>* = nullptr>
    size_t find(CodeUnitMatchFunction, unsigned start = 0);
    ALWAYS_INLINE size_t find(ASCIILiteral literal, unsigned start = 0) { return find(literal.characters8(), literal.length(), start); }
    WTF_EXPORT_PRIVATE size_t find(StringView);
    WTF_EXPORT_PRIVATE size_t find(StringView, unsigned start);
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(StringView) const;
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(StringView, unsigned start) const;

    WTF_EXPORT_PRIVATE size_t reverseFind(UChar, unsigned start = MaxLength);
    WTF_EXPORT_PRIVATE size_t reverseFind(StringView, unsigned start = MaxLength);
    ALWAYS_INLINE size_t reverseFind(ASCIILiteral literal, unsigned start = MaxLength) { return reverseFind(literal.characters8(), literal.length(), start); }

    WTF_EXPORT_PRIVATE bool startsWith(StringView) const;
    WTF_EXPORT_PRIVATE bool startsWithIgnoringASCIICase(StringView) const;
    WTF_EXPORT_PRIVATE bool startsWith(UChar) const;
    WTF_EXPORT_PRIVATE bool startsWith(const char*, unsigned matchLength) const;
    WTF_EXPORT_PRIVATE bool hasInfixStartingAt(StringView, unsigned start) const;

    WTF_EXPORT_PRIVATE bool endsWith(StringView);
    WTF_EXPORT_PRIVATE bool endsWithIgnoringASCIICase(StringView) const;
    WTF_EXPORT_PRIVATE bool endsWith(UChar) const;
    WTF_EXPORT_PRIVATE bool endsWith(const char*, unsigned matchLength) const;
    WTF_EXPORT_PRIVATE bool hasInfixEndingAt(StringView, unsigned end) const;

    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(UChar, UChar);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(UChar, StringView);
    ALWAYS_INLINE Ref<StringImpl> replace(UChar pattern, const char* replacement, unsigned replacementLength) { return replace(pattern, reinterpret_cast<const LChar*>(replacement), replacementLength); }
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(UChar, const LChar*, unsigned replacementLength);
    Ref<StringImpl> replace(UChar, const UChar*, unsigned replacementLength);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(StringView, StringView);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(unsigned start, unsigned length, StringView);

    WTF_EXPORT_PRIVATE std::optional<UCharDirection> defaultWritingDirection();

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

    template<typename T> static size_t headerSize() { return tailOffset<T>(); }
    
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

    WTF_EXPORT_PRIVATE size_t find(const LChar*, unsigned length, unsigned start);
    WTF_EXPORT_PRIVATE size_t reverseFind(const LChar*, unsigned length, unsigned start);

    bool requiresCopy() const;
    template<typename T> const T* tailPointer() const;
    template<typename T> T* tailPointer();
    StringImpl* const& substringBuffer() const;
    StringImpl*& substringBuffer();

    enum class CaseConvertType { Upper, Lower };
    template<CaseConvertType, typename CharacterType> static Ref<StringImpl> convertASCIICase(StringImpl&, const CharacterType*, unsigned);

    WTF_EXPORT_PRIVATE static Ref<StringImpl> createWithoutCopyingNonEmpty(const LChar*, unsigned length);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createWithoutCopyingNonEmpty(const UChar*, unsigned length);

    template<class CodeUnitPredicate> Ref<StringImpl> trimMatchedCharacters(CodeUnitPredicate);
    template<typename CharacterType, typename Predicate> ALWAYS_INLINE Ref<StringImpl> removeCharactersImpl(const CharacterType* characters, const Predicate&);
    template<typename CharacterType, class CodeUnitPredicate> Ref<StringImpl> simplifyMatchedCharactersToSpace(CodeUnitPredicate);
    template<typename CharacterType> static Ref<StringImpl> constructInternal(StringImpl&, unsigned);
    template<typename CharacterType> static Ref<StringImpl> createUninitializedInternal(unsigned, CharacterType*&);
    template<typename CharacterType> static Ref<StringImpl> createUninitializedInternalNonEmpty(unsigned, CharacterType*&);
    template<typename CharacterType> static Expected<Ref<StringImpl>, UTF8ConversionError> reallocateInternal(Ref<StringImpl>&&, unsigned, CharacterType*&);
    template<typename CharacterType> static Ref<StringImpl> createInternal(const CharacterType*, unsigned);
    WTF_EXPORT_PRIVATE NEVER_INLINE unsigned hashSlowCase() const;

    // The bottom bit in the ref count indicates a static (immortal) string.
    static constexpr unsigned s_refCountFlagIsStaticString = 0x1;
    static constexpr unsigned s_refCountIncrement = 0x2; // This allows us to ref / deref without disturbing the static string flag.

#if STRING_STATS
    WTF_EXPORT_PRIVATE static StringStats m_stringStats;
#endif

public:
    void assertHashIsCorrect() const;
};

using StaticStringImpl = StringImpl::StaticStringImpl;

static_assert(sizeof(StringImpl) == sizeof(StaticStringImpl));

template<typename CharacterType>
struct HashTranslatorCharBuffer {
    const CharacterType* characters;
    unsigned length;
    unsigned hash;

    HashTranslatorCharBuffer(const CharacterType* characters, unsigned length)
        : characters(characters)
        , length(length)
        , hash(StringHasher::computeHashAndMaskTop8Bits(characters, length))
    {
    }

    HashTranslatorCharBuffer(const CharacterType* characters, unsigned length, unsigned hash)
        : characters(characters)
        , length(length)
        , hash(hash)
    {
    }
};

#if ASSERT_ENABLED

// StringImpls created from StaticStringImpl will ASSERT in the generic ValueCheck<T>::checkConsistency
// as they are not allocated by fastMalloc. We don't currently have any way to detect that case
// so we ignore the consistency check for all StringImpl*.
template<> struct ValueCheck<StringImpl*> {
    static void checkConsistency(const StringImpl*) { }
};

#endif // ASSERT_ENABLED

WTF_EXPORT_PRIVATE bool equal(const StringImpl*, const StringImpl*);
WTF_EXPORT_PRIVATE bool equal(const StringImpl*, const LChar*);
inline bool equal(const StringImpl* a, const char* b) { return equal(a, reinterpret_cast<const LChar*>(b)); }
WTF_EXPORT_PRIVATE bool equal(const StringImpl*, const LChar*, unsigned);
WTF_EXPORT_PRIVATE bool equal(const StringImpl*, const UChar*, unsigned);
ALWAYS_INLINE bool equal(const StringImpl* a, ASCIILiteral b) { return equal(a, b.characters8(), b.length()); }
inline bool equal(const StringImpl* a, const char* b, unsigned length) { return equal(a, reinterpret_cast<const LChar*>(b), length); }
inline bool equal(const LChar* a, StringImpl* b) { return equal(b, a); }
inline bool equal(const char* a, StringImpl* b) { return equal(b, reinterpret_cast<const LChar*>(a)); }
WTF_EXPORT_PRIVATE bool equal(const StringImpl& a, const StringImpl& b);

WTF_EXPORT_PRIVATE bool equalIgnoringNullity(StringImpl*, StringImpl*);
WTF_EXPORT_PRIVATE bool equalIgnoringNullity(const UChar*, size_t length, StringImpl*);

bool equalIgnoringASCIICase(const StringImpl&, const StringImpl&);
WTF_EXPORT_PRIVATE bool equalIgnoringASCIICase(const StringImpl*, const StringImpl*);
bool equalIgnoringASCIICase(const StringImpl&, ASCIILiteral);
bool equalIgnoringASCIICase(const StringImpl*, ASCIILiteral);

WTF_EXPORT_PRIVATE bool equalIgnoringASCIICaseNonNull(const StringImpl*, const StringImpl*);

bool equalLettersIgnoringASCIICase(const StringImpl&, ASCIILiteral);
bool equalLettersIgnoringASCIICase(const StringImpl*, ASCIILiteral);

template<typename CodeUnit, typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, CodeUnit>>* = nullptr>
size_t find(const CodeUnit*, unsigned length, CodeUnitMatchFunction&&, unsigned start = 0);

template<typename CharacterType> size_t reverseFindLineTerminator(const CharacterType*, unsigned length, unsigned start = StringImpl::MaxLength);
template<typename CharacterType> size_t reverseFind(const CharacterType*, unsigned length, CharacterType matchCharacter, unsigned start = StringImpl::MaxLength);
size_t reverseFind(const UChar*, unsigned length, LChar matchCharacter, unsigned start = StringImpl::MaxLength);
size_t reverseFind(const LChar*, unsigned length, UChar matchCharacter, unsigned start = StringImpl::MaxLength);

template<size_t inlineCapacity> bool equalIgnoringNullity(const Vector<UChar, inlineCapacity>&, StringImpl*);

template<typename CharacterType1, typename CharacterType2> int codePointCompare(const CharacterType1*, unsigned length1, const CharacterType2*, unsigned length2);
int codePointCompare(const StringImpl*, const StringImpl*);

bool isUnicodeWhitespace(UChar);

// Deprecated as this excludes U+0085 and U+00A0 which are part of Unicode's White_Space definition:
// https://www.unicode.org/Public/UCD/latest/ucd/PropList.txt
bool deprecatedIsSpaceOrNewline(UChar);

// Inverse of deprecatedIsSpaceOrNewline for predicates
bool deprecatedIsNotSpaceOrNewline(UChar);

// StringHash is the default hash for StringImpl* and RefPtr<StringImpl>
template<typename> struct DefaultHash;
template<> struct DefaultHash<StringImpl*>;
template<> struct DefaultHash<RefPtr<StringImpl>>;
template<> struct DefaultHash<PackedPtr<StringImpl>>;
template<> struct DefaultHash<CompactPtr<StringImpl>>;

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

template<typename CodeUnit, typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, CodeUnit>>*>
inline size_t find(const CodeUnit* characters, unsigned length, CodeUnitMatchFunction&& matchFunction, unsigned start)
{
    while (start < length) {
        if (matchFunction(characters[start]))
            return start;
        ++start;
    }
    return notFound;
}

template<typename CharacterType> inline size_t reverseFindLineTerminator(const CharacterType* characters, unsigned length, unsigned start)
{
    if (!length)
        return notFound;
    if (start >= length)
        start = length - 1;
    auto character = characters[start];
    while (character != '\n' && character != '\r') {
        if (!start--)
            return notFound;
        character = characters[start];
    }
    return start;
}

template<typename CharacterType> inline size_t reverseFind(const CharacterType* characters, unsigned length, CharacterType matchCharacter, unsigned start)
{
    if (!length)
        return notFound;
    if (start >= length)
        start = length - 1;
    while (characters[start] != matchCharacter) {
        if (!start--)
            return notFound;
    }
    return start;
}

ALWAYS_INLINE size_t reverseFind(const UChar* characters, unsigned length, LChar matchCharacter, unsigned start)
{
    return reverseFind(characters, length, static_cast<UChar>(matchCharacter), start);
}

inline size_t reverseFind(const LChar* characters, unsigned length, UChar matchCharacter, unsigned start)
{
    if (!isLatin1(matchCharacter))
        return notFound;
    return reverseFind(characters, length, static_cast<LChar>(matchCharacter), start);
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

template<typename CodeUnitMatchFunction, std::enable_if_t<std::is_invocable_r_v<bool, CodeUnitMatchFunction, UChar>>*>
size_t StringImpl::find(CodeUnitMatchFunction matchFunction, unsigned start)
{
    if (is8Bit())
        return WTF::find(characters8(), m_length, matchFunction, start);
    return WTF::find(characters16(), m_length, matchFunction, start);
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

// FIXME: For LChar, isUnicodeCompatibleASCIIWhitespace(character) || character == 0x0085 || character == noBreakSpace would be enough
inline bool isUnicodeWhitespace(UChar character)
{
    return isASCII(character) ? isUnicodeCompatibleASCIIWhitespace(character) : u_isUWhiteSpace(character);
}

inline bool deprecatedIsSpaceOrNewline(UChar character)
{
    // Use isUnicodeCompatibleASCIIWhitespace() for all Latin-1 characters, which is incorrect as it
    // excludes U+0085 and U+00A0.
    return isLatin1(character) ? isUnicodeCompatibleASCIIWhitespace(character) : u_charDirection(character) == U_WHITE_SPACE_NEUTRAL;
}

inline bool deprecatedIsNotSpaceOrNewline(UChar character)
{
    return !deprecatedIsSpaceOrNewline(character);
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

inline bool StringImpl::containsOnlyASCII() const
{
    if (is8Bit())
        return charactersAreAllASCII(characters8(), length());
    return charactersAreAllASCII(characters16(), length());
}

inline bool StringImpl::containsOnlyLatin1() const
{
    if (is8Bit())
        return true;
    auto* characters = characters16();
    UChar mergedCharacterBits = 0;
    for (unsigned i = 0; i < length(); ++i)
        mergedCharacterBits |= characters[i];
    return isLatin1(mergedCharacterBits);
}

template<bool isSpecialCharacter(UChar), typename CharacterType> inline bool containsOnly(const CharacterType* characters, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (!isSpecialCharacter(characters[i]))
            return false;
    }
    return true;
}

template<bool isSpecialCharacter(UChar)> inline bool StringImpl::containsOnly() const
{
    if (is8Bit())
        return WTF::containsOnly<isSpecialCharacter>(characters8(), length());
    return WTF::containsOnly<isSpecialCharacter>(characters16(), length());
}

inline StringImpl::StringImpl(unsigned length, Force8Bit)
    : StringImplShape(s_refCountIncrement, length, tailPointer<LChar>(), s_hashFlag8BitBuffer | StringNormal | BufferInternal)
{
    ASSERT(m_data8);
    ASSERT(m_length);

    STRING_STATS_ADD_8BIT_STRING(m_length);
}

inline StringImpl::StringImpl(unsigned length)
    : StringImplShape(s_refCountIncrement, length, tailPointer<UChar>(), s_hashZeroValue | StringNormal | BufferInternal)
{
    ASSERT(m_data16);
    ASSERT(m_length);

    STRING_STATS_ADD_16BIT_STRING(m_length);
}

template<typename Malloc>
inline StringImpl::StringImpl(MallocPtr<LChar, Malloc> characters, unsigned length)
    : StringImplShape(s_refCountIncrement, length, static_cast<const LChar*>(nullptr), s_hashFlag8BitBuffer | StringNormal | BufferOwned)
{
    if constexpr (std::is_same_v<Malloc, StringImplMalloc>)
        m_data8 = characters.leakPtr();
    else {
        auto data8 = static_cast<LChar*>(StringImplMalloc::malloc(length * sizeof(LChar)));
        copyCharacters(data8, characters.get(), length);
        m_data8 = data8;
    }

    ASSERT(m_data8);
    ASSERT(m_length);

    STRING_STATS_ADD_8BIT_STRING(m_length);
}

inline StringImpl::StringImpl(const UChar* characters, unsigned length, ConstructWithoutCopyingTag)
    : StringImplShape(s_refCountIncrement, length, characters, s_hashZeroValue | StringNormal | BufferInternal)
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

template<typename Malloc>
inline StringImpl::StringImpl(MallocPtr<UChar, Malloc> characters, unsigned length)
    : StringImplShape(s_refCountIncrement, length, static_cast<const UChar*>(nullptr), s_hashZeroValue | StringNormal | BufferOwned)
{
    if constexpr (std::is_same_v<Malloc, StringImplMalloc>)
        m_data16 = characters.leakPtr();
    else {
        auto data16 = static_cast<UChar*>(StringImplMalloc::malloc(length * sizeof(UChar)));
        copyCharacters(data16, characters.get(), length);
        m_data16 = data16;
    }

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
    : StringImplShape(s_refCountIncrement, length, characters, s_hashZeroValue | StringNormal | BufferSubstring)
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

    // Copying the thing would save more memory sometimes, largely due to the size of pointer.
    size_t substringSize = allocationSize<StringImpl*>(1);
    if (rep.is8Bit()) {
        if (substringSize >= allocationSize<LChar>(length))
            return create(rep.m_data8 + offset, length);
    } else {
        if (substringSize >= allocationSize<UChar>(length))
            return create(rep.m_data16 + offset, length);
    }

    auto* ownerRep = ((rep.bufferOwnership() == BufferSubstring) ? rep.substringBuffer() : &rep);

    // We allocate a buffer that contains both the StringImpl struct as well as the pointer to the owner string.
    auto* stringImpl = static_cast<StringImpl*>(StringImplMalloc::malloc(substringSize));
    if (rep.is8Bit())
        return adoptRef(*new (NotNull, stringImpl) StringImpl(rep.m_data8 + offset, length, *ownerRep));
    return adoptRef(*new (NotNull, stringImpl) StringImpl(rep.m_data16 + offset, length, *ownerRep));
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

    result = (StringImpl*)StringImplMalloc::tryMalloc(allocationSize<CharacterType>(length));
    if (!result) {
        output = nullptr;
        return nullptr;
    }
    output = result->tailPointer<CharacterType>();

    return constructInternal<CharacterType>(*result, length);
}

template<typename CharacterType, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline Ref<StringImpl> StringImpl::adopt(Vector<CharacterType, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& vector)
{
    if constexpr (std::is_same_v<Malloc, StringImplMalloc>) {
        auto length = vector.size();
        if (!length)
            return *empty();
        if (length > MaxLength)
            CRASH();
        return adoptRef(*new StringImpl(vector.releaseBuffer(), length));
    } else
        return create(vector.data(), vector.size());
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

inline void StringImpl::setIsAtom(bool isAtom)
{
    ASSERT(!isStatic());
    ASSERT(!isSymbol());
    if (isAtom)
        m_hashAndFlags |= s_hashFlagStringKindIsAtom;
    else
        m_hashAndFlags &= ~s_hashFlagStringKindIsAtom;
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

#if TSAN_ENABLED
    if (isStatic())
        return;
#endif

    m_refCount += s_refCountIncrement;
}

inline void StringImpl::deref()
{
    STRING_STATS_DEREF_STRING(*this);

#if TSAN_ENABLED
    if (isStatic())
        return;
#endif

    unsigned tempRefCount = m_refCount - s_refCountIncrement;
    if (!tempRefCount) {
        StringImpl::destroy(this);
        return;
    }
    m_refCount = tempRefCount;
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
    : StringImplShape(s_refCountIncrement, length, characters, s_hashZeroValue | StringSymbol | BufferSubstring)
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
    return tailOffset<T>() + tailElementCount * sizeof(T);
}

template<typename CharacterType>
inline size_t StringImpl::maxInternalLength()
{
    // In order to not overflow the unsigned length, the check for (std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) is needed when sizeof(CharacterType) == 2.
    return std::min(static_cast<size_t>(MaxLength), (std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) / sizeof(CharacterType));
}

template<typename T> inline size_t StringImpl::tailOffset()
{
    return roundUpToMultipleOf<alignof(T)>(offsetof(StringImpl, m_hashAndFlags) + sizeof(StringImpl::m_hashAndFlags));
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

inline bool equalIgnoringASCIICase(const StringImpl& a, ASCIILiteral b)
{
    return equalIgnoringASCIICaseCommon(a, b.characters());
}

inline bool equalIgnoringASCIICase(const StringImpl* a, ASCIILiteral b)
{
    return a && equalIgnoringASCIICase(*a, b);
}

inline bool startsWithLettersIgnoringASCIICase(const StringImpl& string, ASCIILiteral literal)
{
    return startsWithLettersIgnoringASCIICaseCommon(string, literal);
}

inline bool startsWithLettersIgnoringASCIICase(const StringImpl* string, ASCIILiteral literal)
{
    return string && startsWithLettersIgnoringASCIICase(*string, literal);
}

inline bool equalLettersIgnoringASCIICase(const StringImpl& string, ASCIILiteral literal)
{
    return equalLettersIgnoringASCIICaseCommon(string, literal);
}

inline bool equalLettersIgnoringASCIICase(const StringImpl* string, ASCIILiteral literal)
{
    return string && equalLettersIgnoringASCIICase(*string, literal);
}

template<typename CharacterType, typename Predicate> ALWAYS_INLINE Ref<StringImpl> StringImpl::removeCharactersImpl(const CharacterType* characters, const Predicate& findMatch)
{
    auto* from = characters;
    auto* fromEnd = from + m_length;

    // Assume the common case will not remove any characters
    while (from != fromEnd && !findMatch(*from))
        ++from;
    if (from == fromEnd)
        return *this;

    StringBuffer<CharacterType> data(m_length);
    auto* to = data.characters();
    unsigned outc = from - characters;

    copyCharacters(to, characters, outc);

    do {
        while (from != fromEnd && findMatch(*from))
            ++from;
        while (from != fromEnd && !findMatch(*from))
            to[outc++] = *from++;
    } while (from != fromEnd);

    data.shrink(outc);

    return adopt(WTFMove(data));
}

template<typename Predicate>
inline Ref<StringImpl> StringImpl::removeCharacters(const Predicate& findMatch)
{
    static_assert(!std::is_function_v<Predicate>, "Passing a lambda instead of a function pointer helps the compiler with inlining");
    if (is8Bit())
        return removeCharactersImpl(characters8(), findMatch);
    return removeCharactersImpl(characters16(), findMatch);
}

inline Ref<StringImpl> StringImpl::createByReplacingInCharacters(const LChar* characters, unsigned length, UChar target, UChar replacement, unsigned indexOfFirstTargetCharacter)
{
    ASSERT(indexOfFirstTargetCharacter < length);
    if (isLatin1(replacement)) {
        LChar* data;
        LChar oldChar = target;
        LChar newChar = replacement;
        auto newImpl = createUninitializedInternalNonEmpty(length, data);
        memcpy(data, characters, indexOfFirstTargetCharacter);
        for (unsigned i = indexOfFirstTargetCharacter; i != length; ++i) {
            LChar character = characters[i];
            data[i] = character == oldChar ? newChar : character;
        }
        return newImpl;
    }

    UChar* data;
    auto newImpl = createUninitializedInternalNonEmpty(length, data);
    for (unsigned i = 0; i != length; ++i) {
        UChar character = characters[i];
        data[i] = character == target ? replacement : character;
    }
    return newImpl;
}

inline Ref<StringImpl> StringImpl::createByReplacingInCharacters(const UChar* characters, unsigned length, UChar target, UChar replacement, unsigned indexOfFirstTargetCharacter)
{
    ASSERT(indexOfFirstTargetCharacter < length);
    UChar* data;
    auto newImpl = createUninitializedInternalNonEmpty(length, data);
    copyCharacters(data, characters, indexOfFirstTargetCharacter);
    for (unsigned i = indexOfFirstTargetCharacter; i != length; ++i) {
        UChar character = characters[i];
        data[i] = character == target ? replacement : character;
    }
    return newImpl;
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, std::span<const char>>, UTF8ConversionError> StringImpl::tryGetUTF8(const Func& function, ConversionMode mode) const
{
    if (is8Bit())
        return tryGetUTF8ForCharacters(function, characters8(), length());
    return tryGetUTF8ForCharacters(function, characters16(), length(), mode);
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, std::span<const char>>, UTF8ConversionError> StringImpl::tryGetUTF8ForCharacters(const Func& function, const LChar* characters, unsigned length)
{
    if (!length) {
        constexpr const char* emptyString = "";
        return function(std::span(emptyString, emptyString));
    }

    // Allocate a buffer big enough to hold all the characters
    // (an individual LChar can only expand to 2 UTF-8 bytes).
    // Optimization ideas, if we find this function is hot:
    //  * We could speculatively create a CStringBuffer to contain 'length'
    //    characters, and resize if necessary (i.e. if the buffer contains
    //    non-ascii characters). (Alternatively, scan the buffer first for
    //    ascii characters, so we know this will be sufficient).
    //  * We could allocate a CStringBuffer with an appropriate size to
    //    have a good chance of being able to write the string into the
    //    buffer without reallocing (say, 1.5 x length).
    if (length > MaxLength / 2)
        return makeUnexpected(UTF8ConversionError::OutOfMemory);

#if CPU(ARM64)
    if (const LChar* nonASCII = find8NonASCII(characters, length)) {
        size_t prefixLength = nonASCII - characters;
        size_t remainingLength = length - prefixLength;

        Vector<char, 1024> bufferVector(prefixLength + remainingLength * 2);
        char* buffer = bufferVector.data();

        memcpy(buffer, characters, prefixLength);
        buffer += prefixLength;

        auto success = Unicode::convertLatin1ToUTF8(&nonASCII, characters + length, &buffer, buffer + (bufferVector.size() - prefixLength));
        ASSERT_UNUSED(success, success); // (length * 2) should be sufficient for any conversion from Latin1
        return function(std::span(bufferVector.data(), buffer));
    }
    return function(std::span(bitwise_cast<const char*>(characters), bitwise_cast<const char*>(characters + length)));
#else
    Vector<char, 1024> bufferVector(length * 2);
    char* buffer = bufferVector.data();
    const LChar* source = characters;
    bool success = Unicode::convertLatin1ToUTF8(&source, source + length, &buffer, buffer + bufferVector.size());
    ASSERT_UNUSED(success, success); // (length * 2) should be sufficient for any conversion from Latin1
    return function(std::span(bufferVector.data(), buffer));
#endif
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, std::span<const char>>, UTF8ConversionError> StringImpl::tryGetUTF8ForCharacters(const Func& function, const UChar* characters, unsigned length, ConversionMode mode)
{
    if (!length) {
        constexpr const char* emptyString = "";
        return function(std::span(emptyString, emptyString));
    }
    if (length > MaxLength / 3)
        return makeUnexpected(UTF8ConversionError::OutOfMemory);

    size_t bufferSize = length * 3;
    Vector<char, 1024> bufferVector(bufferSize);
    auto convertedSize = utf8ForCharactersIntoBuffer(characters, length, mode, bufferVector);
    if (!convertedSize)
        return makeUnexpected(convertedSize.error());
    return function(std::span(bufferVector.data(), *convertedSize));
}

} // namespace WTF

using WTF::StaticStringImpl;
using WTF::StringImpl;
using WTF::equal;
using WTF::isUnicodeWhitespace;
using WTF::deprecatedIsSpaceOrNewline;
using WTF::deprecatedIsNotSpaceOrNewline;
