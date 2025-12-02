/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005-2024 Apple Inc. All rights reserved.
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

#include <wtf/Compiler.h>

#include <limits.h>
#include <unicode/uchar.h>
#include <unicode/ustring.h>
#include <wtf/ASCIICType.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/CompactPtr.h>
#include <wtf/DebugHeap.h>
#include <wtf/Expected.h>
#include <wtf/FlipBytes.h>
#include <wtf/MathExtras.h>
#include <wtf/NoVirtualDestructorBase.h>
#include <wtf/Packed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/ConversionMode.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringHasherInlines.h>
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

struct ASCIICaseInsensitiveStringViewHashTranslator;
struct HashedUTF8CharactersTranslator;
struct HashTranslatorASCIILiteral;
struct Latin1BufferTranslator;
struct StringViewHashTranslator;
struct SubstringTranslator;
struct UTF16BufferTranslator;

template<typename> class RetainPtr;

template<typename> struct BufferFromStaticDataTranslator;
template<typename> struct HashAndCharactersTranslator;

// Define STRING_STATS to 1 turn on runtime statistics of string sizes and memory usage.
#define STRING_STATS 0

template<bool isSpecialCharacter(char16_t), typename CharacterType, std::size_t Extent = std::dynamic_extent> bool containsOnly(std::span<const CharacterType, Extent>);

#if STRING_STATS

struct StringStats {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(StringStats);
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

class STRING_IMPL_ALIGNMENT StringImplShape  {
    WTF_MAKE_NONCOPYABLE(StringImplShape);
public:
    static constexpr unsigned MaxLength = std::numeric_limits<int32_t>::max();

protected:
    StringImplShape(unsigned refCount, std::span<const Latin1Character>, unsigned hashAndFlags);
    StringImplShape(unsigned refCount, std::span<const char16_t>, unsigned hashAndFlags);

    enum ConstructWithConstExprTag { ConstructWithConstExpr };
    template<unsigned characterCount> constexpr StringImplShape(unsigned refCount, unsigned length, const char (&characters)[characterCount], unsigned hashAndFlags, ConstructWithConstExprTag);
    template<unsigned characterCount> constexpr StringImplShape(unsigned refCount, unsigned length, const char16_t (&characters)[characterCount], unsigned hashAndFlags, ConstructWithConstExprTag);

    std::atomic<unsigned> m_refCount;
    unsigned m_length;
    union {
        const Latin1Character* m_data8;
        const char16_t* m_data16;
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

DECLARE_COMPACT_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringImpl);
class StringImpl : private StringImplShape, public NoVirtualDestructorBase {
    WTF_MAKE_NONCOPYABLE(StringImpl);
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED_WITH_HEAP_IDENTIFIER(StringImpl, StringImpl);

    friend class AtomStringImpl;
    friend class JSC::LLInt::Data;
    friend class JSC::LLIntOffsetsExtractor;
    friend class PrivateSymbolImpl;
    friend class RegisteredSymbolImpl;
    friend class SymbolImpl;
    friend class ExternalStringImpl;

    friend struct WTF::ASCIICaseInsensitiveStringViewHashTranslator;
    friend struct WTF::HashedUTF8CharactersTranslator;
    friend struct WTF::HashTranslatorASCIILiteral;
    friend struct WTF::Latin1BufferTranslator;
    friend struct WTF::StringViewHashTranslator;
    friend struct WTF::SubstringTranslator;
    friend struct WTF::UTF16BufferTranslator;

    template<typename> friend struct WTF::BufferFromStaticDataTranslator;
    template<typename> friend struct WTF::HashAndCharactersTranslator;

    friend WTF_EXPORT_PRIVATE bool equal(const StringImpl&, const StringImpl&);

public:
    enum BufferOwnership { BufferInternal, BufferOwned, BufferSubstring, BufferExternal };

    // Prefer using isValidLength over MaxLength when the character type is known.
    template<typename> static constexpr bool isValidLength(size_t);

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
    template<typename Malloc> explicit StringImpl(MallocSpan<Latin1Character, Malloc>);
    template<typename Malloc> explicit StringImpl(MallocSpan<char16_t, Malloc>);
    enum ConstructWithoutCopyingTag { ConstructWithoutCopying };
    StringImpl(std::span<const char16_t>, ConstructWithoutCopyingTag);
    StringImpl(std::span<const Latin1Character>, ConstructWithoutCopyingTag);

    // Used to create new strings that are a substring of an existing StringImpl (BufferSubstring).
    StringImpl(std::span<const Latin1Character>, Ref<StringImpl>&&);
    StringImpl(std::span<const char16_t>, Ref<StringImpl>&&);

public:
    WTF_EXPORT_PRIVATE static void destroy(StringImpl*);

    WTF_EXPORT_PRIVATE static Ref<StringImpl> create(std::span<const char16_t>);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> create(std::span<const Latin1Character>);
    ALWAYS_INLINE static Ref<StringImpl> create(std::span<const char> characters) { return create(byteCast<Latin1Character>(characters)); }
    WTF_EXPORT_PRIVATE static Ref<StringImpl> create8BitIfPossible(std::span<const char16_t>);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> create8BitUnconditionally(std::span<const char16_t>);

    // Construct a string with UTF-8 data, null if it contains invalid UTF-8 sequences.
    WTF_EXPORT_PRIVATE static RefPtr<StringImpl> create(std::span<const char8_t>);

    static Ref<StringImpl> createSubstringSharingImpl(StringImpl&, unsigned offset, unsigned length);

    ALWAYS_INLINE static Ref<StringImpl> create(ASCIILiteral literal) { return createWithoutCopying(literal.span8()); }

    static Ref<StringImpl> createWithoutCopying(std::span<const char16_t> characters) { return characters.empty() ?  Ref { *empty() } : createWithoutCopyingNonEmpty(characters); }
    static Ref<StringImpl> createWithoutCopying(std::span<const Latin1Character> characters) { return characters.empty() ? Ref { *empty() } : createWithoutCopyingNonEmpty(characters); }
    ALWAYS_INLINE static Ref<StringImpl> createWithoutCopying(std::span<const char> characters) { return createWithoutCopying(byteCast<Latin1Character>(characters)); }

    WTF_EXPORT_PRIVATE static Ref<StringImpl> createUninitialized(size_t length, std::span<Latin1Character>&);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createUninitialized(size_t length, std::span<char16_t>&);

    template<typename CharacterType> static RefPtr<StringImpl> tryCreateUninitialized(size_t length, std::span<CharacterType>&);

    static Ref<StringImpl> createByReplacingInCharacters(std::span<const Latin1Character>, char16_t target, char16_t replacement, size_t indexOfFirstTargetCharacter);
    static Ref<StringImpl> createByReplacingInCharacters(std::span<const char16_t>, char16_t target, char16_t replacement, size_t indexOfFirstTargetCharacter);

    static Ref<StringImpl> createStaticStringImpl(std::span<const char> characters)
    {
        ASSERT(charactersAreAllASCII(byteCast<Latin1Character>(characters)));
        return createStaticStringImpl(byteCast<Latin1Character>(characters));
    }
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createStaticStringImpl(std::span<const Latin1Character>);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createStaticStringImpl(std::span<const char16_t>);

    // Reallocate the StringImpl. The originalString must be only owned by the Ref,
    // and the buffer ownership must be BufferInternal. Just like the input pointer of realloc(),
    // the originalString can't be used after this function.
    static Ref<StringImpl> reallocate(Ref<StringImpl>&& originalString, unsigned length, Latin1Character*& data);
    static Ref<StringImpl> reallocate(Ref<StringImpl>&& originalString, unsigned length, char16_t*& data);
    static Expected<Ref<StringImpl>, UTF8ConversionError> tryReallocate(Ref<StringImpl>&& originalString, unsigned length, Latin1Character*& data);
    static Expected<Ref<StringImpl>, UTF8ConversionError> tryReallocate(Ref<StringImpl>&& originalString, unsigned length, char16_t*& data);

    static constexpr unsigned flagsOffset() { return OBJECT_OFFSETOF(StringImpl, m_hashAndFlags); }
    static constexpr unsigned flagIs8Bit() { return s_hashFlag8BitBuffer; }
    static constexpr unsigned flagIsAtom() { return s_hashFlagStringKindIsAtom; }
    static constexpr unsigned flagIsSymbol() { return s_hashFlagStringKindIsSymbol; }
    static constexpr unsigned maskStringKind() { return s_hashMaskStringKind; }
    static constexpr unsigned dataOffset() { return OBJECT_OFFSETOF(StringImpl, m_data8); }

    template<typename CharacterType, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
    static Ref<StringImpl> adopt(Vector<CharacterType, inlineCapacity, OverflowHandler, minCapacity, Malloc>&&);

    WTF_EXPORT_PRIVATE static Ref<StringImpl> adopt(StringBuffer<char16_t>&&);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> adopt(StringBuffer<Latin1Character>&&);

    unsigned length() const { return m_length; }
    static constexpr ptrdiff_t lengthMemoryOffset() { return OBJECT_OFFSETOF(StringImpl, m_length); }
    bool isEmpty() const { return !m_length; }

    bool is8Bit() const { return m_hashAndFlags & s_hashFlag8BitBuffer; }
    ALWAYS_INLINE std::span<const Latin1Character> span8() const LIFETIME_BOUND { ASSERT(is8Bit()); return unsafeMakeSpan(m_data8, length()); }
    ALWAYS_INLINE std::span<const char16_t> span16() const LIFETIME_BOUND { ASSERT(!is8Bit() || isEmpty()); return unsafeMakeSpan(m_data16, length()); }

    template<typename CharacterType> std::span<const CharacterType> span() const LIFETIME_BOUND;

    size_t cost() const;
    size_t costDuringGC();

    WTF_EXPORT_PRIVATE size_t sizeInBytes() const;

    bool isSymbol() const { return m_hashAndFlags & s_hashFlagStringKindIsSymbol; }
    bool isAtom() const { return m_hashAndFlags & s_hashFlagStringKindIsAtom; }
    void setIsAtom(bool);
    
    bool isExternal() const { return bufferOwnership() == BufferExternal; }

    bool isSubString() const { return bufferOwnership() == BufferSubstring; }

    static WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> utf8ForCharacters(std::span<const Latin1Character> characters);
    static WTF_EXPORT_PRIVATE Expected<CString, UTF8ConversionError> utf8ForCharacters(std::span<const char16_t> characters, ConversionMode = LenientConversion);
    static WTF_EXPORT_PRIVATE Expected<size_t, UTF8ConversionError> utf8ForCharactersIntoBuffer(std::span<const char16_t> characters, ConversionMode, Vector<char8_t, 1024>&);

    template<typename Func>
    static Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> tryGetUTF8ForCharacters(NOESCAPE const Func&, std::span<const Latin1Character> characters);
    template<typename Func>
    static Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> tryGetUTF8ForCharacters(NOESCAPE const Func&, std::span<const char16_t> characters, ConversionMode = LenientConversion);

    template<typename Func>
    Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> tryGetUTF8(NOESCAPE const Func&, ConversionMode = LenientConversion) const;
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

    SUPPRESS_TSAN bool isStatic() const { return m_refCount.load(std::memory_order_relaxed) & s_refCountFlagIsStaticString; }

    size_t refCount() const { return m_refCount.load(std::memory_order_relaxed) / s_refCountIncrement; }
    bool hasOneRef() const { return m_refCount.load(std::memory_order_relaxed) == s_refCountIncrement; }
    bool hasAtLeastOneRef() const { return m_refCount.load(std::memory_order_relaxed); } // For assertions.

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
        operator const StringImpl&() const;
    };

    WTF_EXPORT_PRIVATE static StaticStringImpl s_emptyAtomString;
    ALWAYS_INLINE static StringImpl* empty() { SUPPRESS_MEMORY_UNSAFE_CAST return reinterpret_cast<StringImpl*>(&s_emptyAtomString); }

    // FIXME: Do these functions really belong in StringImpl?
    template<typename CharacterType>
    ALWAYS_INLINE static void copyCharacters(std::span<CharacterType> destination, std::span<const CharacterType> source)
    {
        return copyElements(destination, source);
    }

    ALWAYS_INLINE static void copyCharacters(std::span<char16_t> destination, std::span<const Latin1Character> source)
    {
        static_assert(sizeof(char16_t) == sizeof(uint16_t));
        static_assert(sizeof(Latin1Character) == sizeof(uint8_t));
        return copyElements(spanReinterpretCast<uint16_t>(destination), source);
    }

    ALWAYS_INLINE static void copyCharacters(std::span<Latin1Character> destination, std::span<const char16_t> source)
    {
        static_assert(sizeof(char16_t) == sizeof(uint16_t));
        static_assert(sizeof(Latin1Character) == sizeof(uint8_t));
#if ASSERT_ENABLED
        for (auto character : source)
            ASSERT(isLatin1(character));
#endif
        return copyElements(destination, spanReinterpretCast<const uint16_t>(source));
    }

    // Some string features, like reference counting and the atomicity flag, are not
    // thread-safe. We achieve thread safety by isolation, giving each thread
    // its own copy of the string.
    Ref<StringImpl> isolatedCopy() const;

    WTF_EXPORT_PRIVATE Ref<StringImpl> substring(unsigned position, unsigned length = MaxLength);

    char16_t at(unsigned) const;
    char16_t operator[](unsigned i) const { return at(i); }
    WTF_EXPORT_PRIVATE char32_t characterStartingAt(unsigned);

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
    template<bool isSpecialCharacter(char16_t)> bool containsOnly() const;

    size_t find(Latin1Character, size_t start = 0);
    size_t find(char, size_t start = 0);
    size_t find(char16_t, size_t start = 0);
    template<typename CodeUnitMatchFunction>
        requires (std::is_invocable_r_v<bool, CodeUnitMatchFunction, char16_t>)
    size_t find(CodeUnitMatchFunction, size_t start = 0);
    ALWAYS_INLINE size_t find(ASCIILiteral literal, size_t start = 0) { return find(literal.span8(), start); }
    WTF_EXPORT_PRIVATE size_t find(StringView);
    WTF_EXPORT_PRIVATE size_t find(StringView, size_t start);
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(StringView) const;
    WTF_EXPORT_PRIVATE size_t findIgnoringASCIICase(StringView, size_t start) const;

    WTF_EXPORT_PRIVATE size_t reverseFind(char16_t, size_t start = MaxLength);
    WTF_EXPORT_PRIVATE size_t reverseFind(StringView, size_t start = MaxLength);
    ALWAYS_INLINE size_t reverseFind(ASCIILiteral literal, size_t start = MaxLength) { return reverseFind(literal.span8(), start); }

    WTF_EXPORT_PRIVATE bool startsWith(StringView) const;
    WTF_EXPORT_PRIVATE bool startsWithIgnoringASCIICase(StringView) const;
    WTF_EXPORT_PRIVATE bool startsWith(char16_t) const;
    WTF_EXPORT_PRIVATE bool startsWith(std::span<const char>) const;
    WTF_EXPORT_PRIVATE bool hasInfixStartingAt(StringView, size_t start) const;

    WTF_EXPORT_PRIVATE bool endsWith(StringView);
    WTF_EXPORT_PRIVATE bool endsWithIgnoringASCIICase(StringView) const;
    WTF_EXPORT_PRIVATE bool endsWith(char16_t) const;
    WTF_EXPORT_PRIVATE bool endsWith(std::span<const char>) const;
    WTF_EXPORT_PRIVATE bool hasInfixEndingAt(StringView, size_t end) const;

    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(char16_t, char16_t);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(char16_t, StringView);
    ALWAYS_INLINE Ref<StringImpl> replace(char16_t pattern, std::span<const char> replacement) { return replace(pattern, byteCast<Latin1Character>(replacement)); }
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(char16_t, std::span<const Latin1Character>);
    Ref<StringImpl> replace(char16_t, std::span<const char16_t>);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(StringView, StringView);
    WTF_EXPORT_PRIVATE Ref<StringImpl> replace(size_t start, size_t length, StringView);

    WTF_EXPORT_PRIVATE std::optional<UCharDirection> defaultWritingDirection();

#if USE(CF)
    RetainPtr<CFStringRef> createCFString();
#endif

#ifdef __OBJC__
    WTF_EXPORT_PRIVATE operator NSString *();
    WTF_EXPORT_PRIVATE RetainPtr<NSString> createNSString();
#endif

#if STRING_STATS
    ALWAYS_INLINE static StringStats& stringStats() { return m_stringStats; }
#endif

    BufferOwnership bufferOwnership() const { return static_cast<BufferOwnership>(m_hashAndFlags & s_hashMaskBufferOwnership); }

    template<typename T> static constexpr size_t headerSize() { return tailOffset<T>(); }
    
protected:
    ~StringImpl();

    // Used to create new symbol string that holds an existing [[Description]] string as a substring buffer (BufferSubstring).
    enum CreateSymbolTag { CreateSymbol };
    StringImpl(CreateSymbolTag, std::span<const Latin1Character>);
    StringImpl(CreateSymbolTag, std::span<const char16_t>);

    // Null symbol.
    explicit StringImpl(CreateSymbolTag);

private:
    template<typename> static size_t allocationSize(Checked<size_t> tailElementCount);
    template<typename> static constexpr size_t tailOffset();

    WTF_EXPORT_PRIVATE size_t find(std::span<const Latin1Character>, size_t start);
    WTF_EXPORT_PRIVATE size_t reverseFind(std::span<const Latin1Character>, size_t start);

    bool requiresCopy() const;
    template<typename T> const T* tailPointer() const;
    template<typename T> T* tailPointer();
    StringImpl* const& substringBuffer() const;
    StringImpl*& substringBuffer();

    enum class CaseConvertType { Upper, Lower };
    template<CaseConvertType, typename CharacterType> static Ref<StringImpl> convertASCIICase(StringImpl&, std::span<const CharacterType>);

    WTF_EXPORT_PRIVATE static Ref<StringImpl> createWithoutCopyingNonEmpty(std::span<const Latin1Character>);
    WTF_EXPORT_PRIVATE static Ref<StringImpl> createWithoutCopyingNonEmpty(std::span<const char16_t>);

    template<typename CharacterType, class CodeUnitPredicate> Ref<StringImpl> trimMatchedCharacters(CodeUnitPredicate);
    template<typename CharacterType, typename Predicate> ALWAYS_INLINE Ref<StringImpl> removeCharactersImpl(std::span<const CharacterType> characters, const Predicate&);
    template<typename CharacterType, class CodeUnitPredicate> Ref<StringImpl> simplifyMatchedCharactersToSpace(CodeUnitPredicate);
    template<typename CharacterType, typename Malloc> static ALWAYS_INLINE MallocSpan<CharacterType, StringImplMalloc> toStringImplMallocSpan(MallocSpan<CharacterType, Malloc>);
    template<typename CharacterType> static Ref<StringImpl> constructInternal(StringImpl&, unsigned);
    template<typename CharacterType> static Ref<StringImpl> createUninitializedInternal(size_t, std::span<CharacterType>&);
    template<typename CharacterType> static Ref<StringImpl> createUninitializedInternalNonEmpty(size_t, std::span<CharacterType>&);
    template<typename CharacterType> static Expected<Ref<StringImpl>, UTF8ConversionError> reallocateInternal(Ref<StringImpl>&&, unsigned, CharacterType*&);
    template<typename CharacterType> static Ref<StringImpl> createInternal(std::span<const CharacterType>);
    WTF_EXPORT_PRIVATE NEVER_INLINE unsigned hashSlowCase() const;
    Ref<StringImpl> convertToUppercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned);
    Ref<StringImpl> convertToUppercaseWithoutLocaleUpconvert();

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
    std::span<const CharacterType> characters;
    unsigned hash;

    HashTranslatorCharBuffer(std::span<const CharacterType> characters)
        : characters(characters)
        , hash(StringHasher::computeHashAndMaskTop8Bits(characters))
    {
    }

    HashTranslatorCharBuffer(std::span<const CharacterType> characters, unsigned hash)
        : characters(characters)
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
WTF_EXPORT_PRIVATE bool equal(const StringImpl*, std::span<const Latin1Character>);
inline bool equal(const StringImpl* a, const char* b) { return equal(a, byteCast<Latin1Character>(unsafeSpan(b))); }
WTF_EXPORT_PRIVATE bool equal(const StringImpl*, std::span<const char16_t>);
ALWAYS_INLINE bool equal(const StringImpl* a, ASCIILiteral b) { return equal(a, b.span8()); }
inline bool equal(const StringImpl* a, std::span<const char> b) { return equal(a, byteCast<Latin1Character>(b)); }
inline bool equal(const char* a, StringImpl* b) { return equal(b, byteCast<Latin1Character>(unsafeSpan(a))); }
WTF_EXPORT_PRIVATE bool equal(const StringImpl& a, const StringImpl& b);

WTF_EXPORT_PRIVATE bool equalIgnoringNullity(StringImpl*, StringImpl*);
WTF_EXPORT_PRIVATE bool equalIgnoringNullity(std::span<const char16_t>, StringImpl*);

bool equalIgnoringASCIICase(const StringImpl&, const StringImpl&);
WTF_EXPORT_PRIVATE bool equalIgnoringASCIICase(const StringImpl*, const StringImpl*);
bool equalIgnoringASCIICase(const StringImpl&, ASCIILiteral);
bool equalIgnoringASCIICase(const StringImpl*, ASCIILiteral);

WTF_EXPORT_PRIVATE bool equalIgnoringASCIICaseNonNull(const StringImpl*, const StringImpl*);

bool equalLettersIgnoringASCIICase(const StringImpl&, ASCIILiteral);
bool equalLettersIgnoringASCIICase(const StringImpl*, ASCIILiteral);

template<typename CodeUnit, typename CodeUnitMatchFunction>
    requires (std::is_invocable_r_v<bool, CodeUnitMatchFunction, CodeUnit>)
size_t find(std::span<const CodeUnit>, CodeUnitMatchFunction&&, size_t start = 0);

template<typename CharacterType> size_t reverseFindLineTerminator(std::span<const CharacterType>, size_t start = StringImpl::MaxLength);
template<typename CharacterType> size_t reverseFind(std::span<const CharacterType>, CharacterType matchCharacter, size_t start = StringImpl::MaxLength);
size_t reverseFind(std::span<const char16_t>, Latin1Character matchCharacter, size_t start = StringImpl::MaxLength);
size_t reverseFind(std::span<const Latin1Character>, char16_t matchCharacter, size_t start = StringImpl::MaxLength);

template<size_t inlineCapacity> bool equalIgnoringNullity(const Vector<char16_t, inlineCapacity>&, StringImpl*);

template<typename CharacterType1, typename CharacterType2>
std::strong_ordering codePointCompare(std::span<const CharacterType1> characters1, std::span<const CharacterType2> characters2);
std::strong_ordering codePointCompare(const StringImpl* string1, const StringImpl* string2);

bool isUnicodeWhitespace(char16_t);

// Deprecated as this excludes U+0085 and U+00A0 which are part of Unicode's White_Space definition:
// https://www.unicode.org/Public/UCD/latest/ucd/PropList.txt
bool deprecatedIsSpaceOrNewline(char16_t);

// Inverse of deprecatedIsSpaceOrNewline for predicates
bool deprecatedIsNotSpaceOrNewline(char16_t);

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

template<> ALWAYS_INLINE Ref<StringImpl> StringImpl::constructInternal<Latin1Character>(StringImpl& string, unsigned length)
{
    return adoptRef(*new (NotNull, &string) StringImpl { length, Force8BitConstructor });
}

template<> ALWAYS_INLINE Ref<StringImpl> StringImpl::constructInternal<char16_t>(StringImpl& string, unsigned length)
{
    return adoptRef(*new (NotNull, &string) StringImpl { length });
}

template<> ALWAYS_INLINE std::span<const Latin1Character> StringImpl::span<Latin1Character>() const
{
    return span8();
}

template<> ALWAYS_INLINE std::span<const char16_t> StringImpl::span<char16_t>() const
{
    return span16();
}

template<typename CodeUnit, typename CodeUnitMatchFunction>
    requires (std::is_invocable_r_v<bool, CodeUnitMatchFunction, CodeUnit>)
inline size_t find(std::span<const CodeUnit> characters, CodeUnitMatchFunction&& matchFunction, size_t start)
{
    while (start < characters.size()) {
        if (matchFunction(characters[start]))
            return start;
        ++start;
    }
    return notFound;
}

template<typename CharacterType> inline size_t reverseFindLineTerminator(std::span<const CharacterType> characters, size_t start)
{
    if (characters.empty())
        return notFound;
    if (start >= characters.size())
        start = characters.size() - 1;
    auto character = characters[start];
    while (character != '\n' && character != '\r') {
        if (!start--)
            return notFound;
        character = characters[start];
    }
    return start;
}

template<typename CharacterType> inline size_t reverseFind(std::span<const CharacterType> characters, CharacterType matchCharacter, size_t start)
{
    if (characters.empty())
        return notFound;
    if (start >= characters.size())
        start = characters.size() - 1;
    while (characters[start] != matchCharacter) {
        if (!start--)
            return notFound;
    }
    return start;
}

ALWAYS_INLINE size_t reverseFind(std::span<const char16_t> characters, Latin1Character matchCharacter, size_t start)
{
    return reverseFind(characters, static_cast<char16_t>(matchCharacter), start);
}

inline size_t reverseFind(std::span<const Latin1Character> characters, char16_t matchCharacter, size_t start)
{
    if (!isLatin1(matchCharacter))
        return notFound;
    return reverseFind(characters, static_cast<Latin1Character>(matchCharacter), start);
}

inline size_t StringImpl::find(Latin1Character character, size_t start)
{
    if (is8Bit())
        return WTF::find(span8(), character, start);
    return WTF::find(span16(), character, start);
}

ALWAYS_INLINE size_t StringImpl::find(char character, size_t start)
{
    return find(byteCast<Latin1Character>(character), start);
}

inline size_t StringImpl::find(char16_t character, size_t start)
{
    if (is8Bit())
        return WTF::find(span8(), character, start);
    return WTF::find(span16(), character, start);
}

template<typename CodeUnitMatchFunction>
    requires (std::is_invocable_r_v<bool, CodeUnitMatchFunction, char16_t>)
size_t StringImpl::find(CodeUnitMatchFunction matchFunction, size_t start)
{
    if (is8Bit())
        return WTF::find(span8(), matchFunction, start);
    return WTF::find(span16(), matchFunction, start);
}

template<size_t inlineCapacity> inline bool equalIgnoringNullity(const Vector<char16_t, inlineCapacity>& a, StringImpl* b)
{
    return equalIgnoringNullity(a.data(), a.size(), b);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
template<typename CharacterType1, typename CharacterType2> inline std::strong_ordering codePointCompare(std::span<const CharacterType1> characters1, std::span<const CharacterType2> characters2)
{
    size_t commonLength = std::min(characters1.size(), characters2.size());

    auto* characters1Ptr = characters1.data();
    auto* characters2Ptr = characters2.data();
    size_t position = 0;

#if CPU(REGISTER64) && !CPU(NEEDS_ALIGNED_ACCESS) && CPU(LITTLE_ENDIAN)
    if constexpr (sizeof(CharacterType1) == sizeof(CharacterType2) && (sizeof(CharacterType1) == 1 || sizeof(CharacterType1) == 2)) {
        using ChunkType = std::conditional_t<sizeof(CharacterType1) == 1, uint32_t, uint64_t>;
        constexpr size_t stride = sizeof(ChunkType) / sizeof(CharacterType1);
        for (; position + (stride - 1) < commonLength;) {
            auto lhs = *std::bit_cast<const ChunkType*>(characters1Ptr);
            auto rhs = *std::bit_cast<const ChunkType*>(characters2Ptr);
            if (lhs != rhs) {
                if constexpr (sizeof(CharacterType1) == 1)
                    return (flipBytes(lhs) > flipBytes(rhs)) ? std::strong_ordering::greater : std::strong_ordering::less;

#if CPU(ARM64)
                if constexpr (sizeof(CharacterType1) == 2) {
                    auto rev16 = [](uint64_t value) ALWAYS_INLINE_LAMBDA {
                        uint64_t result;
                        __asm__("rev16 %x0, %x1" : "=r"(result) : "r"(value));
                        return result;
                    };
                    return (rev16(flipBytes(lhs)) > rev16(flipBytes(rhs))) ? std::strong_ordering::greater : std::strong_ordering::less;
                }
#endif

                break;
            }

            characters1Ptr += stride;
            characters2Ptr += stride;
            position += stride;
        }
    }
#endif

    while (position < commonLength && *characters1Ptr == *characters2Ptr) {
        ++characters1Ptr;
        ++characters2Ptr;
        ++position;
    }

    if (position < commonLength)
        return (characters1Ptr[0] > characters2Ptr[0]) ? std::strong_ordering::greater : std::strong_ordering::less;

    if (characters1.size() == characters2.size())
        return std::strong_ordering::equal;
    return (characters1.size() > characters2.size()) ? std::strong_ordering::greater : std::strong_ordering::less;
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

inline std::strong_ordering codePointCompare(const StringImpl* string1, const StringImpl* string2)
{
    // FIXME: Should null strings compare as less than empty strings rather than equal to them?
    if (!string1)
        return (string2 && string2->length()) ? std::strong_ordering::less : std::strong_ordering::equal;
    if (!string2)
        return string1->length() ? std::strong_ordering::greater : std::strong_ordering::equal;

    bool string1Is8Bit = string1->is8Bit();
    bool string2Is8Bit = string2->is8Bit();
    if (string1Is8Bit) {
        if (string2Is8Bit)
            return codePointCompare(string1->span8(), string2->span8());
        return codePointCompare(string1->span8(), string2->span16());
    }
    if (string2Is8Bit)
        return codePointCompare(string1->span16(), string2->span8());
    return codePointCompare(string1->span16(), string2->span16());
}

// FIXME: For Latin1Character, isUnicodeCompatibleASCIIWhitespace(character) || character == 0x0085 || character == noBreakSpace would be enough
inline bool isUnicodeWhitespace(char16_t character)
{
    return isASCII(character) ? isUnicodeCompatibleASCIIWhitespace(character) : u_isUWhiteSpace(character);
}

inline bool deprecatedIsSpaceOrNewline(char16_t character)
{
    // Use isUnicodeCompatibleASCIIWhitespace() for all Latin-1 characters, which is incorrect as it
    // excludes U+0085 and U+00A0.
    return isLatin1(character) ? isUnicodeCompatibleASCIIWhitespace(character) : u_charDirection(character) == U_WHITE_SPACE_NEUTRAL;
}

inline bool deprecatedIsNotSpaceOrNewline(char16_t character)
{
    return !deprecatedIsSpaceOrNewline(character);
}

inline StringImplShape::StringImplShape(unsigned refCount, std::span<const Latin1Character> data, unsigned hashAndFlags)
    : m_refCount(refCount)
    , m_length(data.size())
    , m_data8(data.data())
    , m_hashAndFlags(hashAndFlags)
{
    RELEASE_ASSERT(data.size() <= MaxLength);
}

inline StringImplShape::StringImplShape(unsigned refCount, std::span<const char16_t> data, unsigned hashAndFlags)
    : m_refCount(refCount)
    , m_length(data.size())
    , m_data16(data.data())
    , m_hashAndFlags(hashAndFlags)
{
    RELEASE_ASSERT(data.size() <= MaxLength);
}

template<unsigned characterCount> constexpr StringImplShape::StringImplShape(unsigned refCount, unsigned length, const char (&characters)[characterCount], unsigned hashAndFlags, ConstructWithConstExprTag)
    : m_refCount(refCount)
    , m_length(length)
    , m_data8Char(characters)
    , m_hashAndFlags(hashAndFlags)
{
    RELEASE_ASSERT(length <= MaxLength);
}

template<unsigned characterCount> constexpr StringImplShape::StringImplShape(unsigned refCount, unsigned length, const char16_t (&characters)[characterCount], unsigned hashAndFlags, ConstructWithConstExprTag)
    : m_refCount(refCount)
    , m_length(length)
    , m_data16Char(characters)
    , m_hashAndFlags(hashAndFlags)
{
    RELEASE_ASSERT(length <= MaxLength);
}

inline Ref<StringImpl> StringImpl::isolatedCopy() const
{
    if (!requiresCopy()) {
        if (is8Bit())
            return StringImpl::createWithoutCopying(span8());
        return StringImpl::createWithoutCopying(span16());
    }

    if (is8Bit())
        return create(span8());
    return create(span16());
}

inline bool StringImpl::containsOnlyASCII() const
{
    if (is8Bit())
        return charactersAreAllASCII(span8());
    return charactersAreAllASCII(span16());
}

inline bool StringImpl::containsOnlyLatin1() const
{
    if (is8Bit())
        return true;
    auto characters = span16();
    char16_t mergedCharacterBits = 0;
    for (auto character : characters)
        mergedCharacterBits |= character;
    return isLatin1(mergedCharacterBits);
}

template<bool isSpecialCharacter(char16_t), typename CharacterType, std::size_t Extent> inline bool containsOnly(std::span<const CharacterType, Extent> characters)
{
    for (auto character : characters) {
        if (!isSpecialCharacter(character))
            return false;
    }
    return true;
}

template<bool isSpecialCharacter(char16_t)> inline bool StringImpl::containsOnly() const
{
    if (is8Bit())
        return WTF::containsOnly<isSpecialCharacter>(span8());
    return WTF::containsOnly<isSpecialCharacter>(span16());
}

inline StringImpl::StringImpl(unsigned length, Force8Bit)
    : StringImplShape(s_refCountIncrement, unsafeMakeSpan(tailPointer<Latin1Character>(), length), s_hashFlag8BitBuffer | StringNormal | BufferInternal)
{
    ASSERT(m_data8);
    ASSERT(m_length);

    STRING_STATS_ADD_8BIT_STRING(m_length);
}

inline StringImpl::StringImpl(unsigned length)
    : StringImplShape(s_refCountIncrement, unsafeMakeSpan(tailPointer<char16_t>(), length), s_hashZeroValue | StringNormal | BufferInternal)
{
    ASSERT(m_data16);
    ASSERT(m_length);

    STRING_STATS_ADD_16BIT_STRING(m_length);
}

template<typename CharacterType, typename Malloc>
MallocSpan<CharacterType, StringImplMalloc> StringImpl::toStringImplMallocSpan(MallocSpan<CharacterType, Malloc> characters)
{
    if constexpr (std::is_same_v<Malloc, StringImplMalloc>)
        return characters;
    else {
        auto buffer = MallocSpan<CharacterType, StringImplMalloc>::malloc(characters.sizeInBytes());
        copyCharacters(buffer.mutableSpan(), characters.span());
        return buffer;
    }
}

template<typename Malloc>
inline StringImpl::StringImpl(MallocSpan<Latin1Character, Malloc> characters)
    : StringImplShape(s_refCountIncrement, toStringImplMallocSpan(WTFMove(characters)).leakSpan(), s_hashFlag8BitBuffer | StringNormal | BufferOwned)
{
    ASSERT(m_data8);
    ASSERT(m_length);

    STRING_STATS_ADD_8BIT_STRING(m_length);
}

inline StringImpl::StringImpl(std::span<const char16_t> characters, ConstructWithoutCopyingTag)
    : StringImplShape(s_refCountIncrement, characters, s_hashZeroValue | StringNormal | BufferInternal)
{
    ASSERT(m_data16);
    ASSERT(m_length);

    STRING_STATS_ADD_16BIT_STRING(m_length);
}

inline StringImpl::StringImpl(std::span<const Latin1Character> characters, ConstructWithoutCopyingTag)
    : StringImplShape(s_refCountIncrement, characters, s_hashFlag8BitBuffer | StringNormal | BufferInternal)
{
    ASSERT(m_data8);
    ASSERT(m_length);

    STRING_STATS_ADD_8BIT_STRING(m_length);
}

template<typename Malloc>
inline StringImpl::StringImpl(MallocSpan<char16_t, Malloc> characters)
    : StringImplShape(s_refCountIncrement, toStringImplMallocSpan(WTFMove(characters)).leakSpan(), s_hashZeroValue | StringNormal | BufferOwned)
{
    ASSERT(m_data16);
    ASSERT(m_length);

    STRING_STATS_ADD_16BIT_STRING(m_length);
}

inline StringImpl::StringImpl(std::span<const Latin1Character> characters, Ref<StringImpl>&& base)
    : StringImplShape(s_refCountIncrement, characters, s_hashFlag8BitBuffer | StringNormal | BufferSubstring)
{
    ASSERT(is8Bit());
    ASSERT(m_data8);
    ASSERT(m_length);
    ASSERT(base->bufferOwnership() != BufferSubstring);

    substringBuffer() = &base.leakRef();

    STRING_STATS_ADD_8BIT_STRING2(m_length, true);
}

inline StringImpl::StringImpl(std::span<const char16_t> characters, Ref<StringImpl>&& base)
    : StringImplShape(s_refCountIncrement, characters, s_hashZeroValue | StringNormal | BufferSubstring)
{
    ASSERT(!is8Bit());
    ASSERT(m_data16);
    ASSERT(m_length);
    ASSERT(base->bufferOwnership() != BufferSubstring);

    substringBuffer() = &base.leakRef();

    STRING_STATS_ADD_16BIT_STRING2(m_length, true);
}

ALWAYS_INLINE Ref<StringImpl> StringImpl::createSubstringSharingImpl(StringImpl& rep, unsigned offset, unsigned length)
{
    ASSERT(length <= rep.length());

    if (!length)
        return *empty();

    // Copying the thing would save more memory sometimes, largely due to the size of pointer.
    size_t substringSize = allocationSize<StringImpl*>(1);
    if (rep.is8Bit()) {
        if (substringSize >= allocationSize<Latin1Character>(length))
            return create(rep.span8().subspan(offset, length));
    } else {
        auto span = rep.span16().subspan(offset, length);
        if (substringSize >= allocationSize<Latin1Character>(length) && charactersAreAllLatin1(span))
            return create8BitUnconditionally(span);

        if (substringSize >= allocationSize<char16_t>(length))
            return create(span);
    }

    SUPPRESS_UNCOUNTED_LOCAL auto* ownerRep = ((rep.bufferOwnership() == BufferSubstring) ? rep.substringBuffer() : &rep);

    // We allocate a buffer that contains both the StringImpl struct as well as the pointer to the owner string.
    SUPPRESS_UNCOUNTED_LOCAL auto* stringImpl = static_cast<StringImpl*>(StringImplMalloc::malloc(substringSize));
    if (rep.is8Bit())
        return adoptRef(*new (NotNull, stringImpl) StringImpl(rep.span8().subspan(offset, length), *ownerRep));
    return adoptRef(*new (NotNull, stringImpl) StringImpl(rep.span16().subspan(offset, length), *ownerRep));
}

template<typename CharacterType> ALWAYS_INLINE RefPtr<StringImpl> StringImpl::tryCreateUninitialized(size_t length, std::span<CharacterType>& output)
{
    if (!length) {
        output = { };
        return empty();
    }

    if (!isValidLength<CharacterType>(length)) {
        output = { };
        return nullptr;
    }
    SUPPRESS_UNCOUNTED_LOCAL StringImpl* result = (StringImpl*)StringImplMalloc::tryMalloc(allocationSize<CharacterType>(length));
    if (!result) {
        output = { };
        return nullptr;
    }
    output = unsafeMakeSpan(result->tailPointer<CharacterType>(), length);

    return constructInternal<CharacterType>(*result, length);
}

template<typename CharacterType, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline Ref<StringImpl> StringImpl::adopt(Vector<CharacterType, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& vector)
{
    if constexpr (std::is_same_v<Malloc, StringImplMalloc>) {
        if (!vector.size())
            return *empty();
        return adoptRef(*new StringImpl(vector.releaseBuffer()));
    } else
        return create(vector.span());
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
    ASSERT(hash == (is8Bit() ? StringHasher::computeHashAndMaskTop8Bits(span8()) : StringHasher::computeHashAndMaskTop8Bits(span16())));
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

    m_refCount.fetch_add(s_refCountIncrement, std::memory_order_relaxed);
}

inline void StringImpl::deref()
{
    STRING_STATS_DEREF_STRING(*this);

#if TSAN_ENABLED
    if (isStatic())
        return;
#endif

    auto oldRefCount = m_refCount.fetch_sub(s_refCountIncrement, std::memory_order_relaxed);
    if (oldRefCount != s_refCountIncrement)
        return;

    StringImpl::destroy(this);
}

inline char16_t StringImpl::at(unsigned i) const
{
    return is8Bit() ? span8()[i] : span16()[i];
}

inline StringImpl::StringImpl(CreateSymbolTag, std::span<const Latin1Character> characters)
    : StringImplShape(s_refCountIncrement, characters, s_hashFlag8BitBuffer | StringSymbol | BufferSubstring)
{
    ASSERT(is8Bit());
    ASSERT(m_data8);
    STRING_STATS_ADD_8BIT_STRING2(m_length, true);
}

inline StringImpl::StringImpl(CreateSymbolTag, std::span<const char16_t> characters)
    : StringImplShape(s_refCountIncrement, characters, s_hashZeroValue | StringSymbol | BufferSubstring)
{
    ASSERT(!is8Bit());
    ASSERT(m_data16);
    STRING_STATS_ADD_16BIT_STRING2(m_length, true);
}

inline StringImpl::StringImpl(CreateSymbolTag)
    : StringImplShape(s_refCountIncrement, empty()->span8(), s_hashFlag8BitBuffer | StringSymbol | BufferSubstring)
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
inline constexpr bool StringImpl::isValidLength(size_t length)
{
    // In order to not overflow the unsigned length, the check for (std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) is needed when sizeof(CharacterType) == 2.
    constexpr size_t max = std::min(static_cast<size_t>(MaxLength), (std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) / sizeof(CharacterType));
    return length <= max;
}

template<typename T> constexpr size_t StringImpl::tailOffset()
{
    return roundUpToMultipleOf<alignof(T)>(offsetof(StringImpl, m_hashAndFlags) + sizeof(StringImpl::m_hashAndFlags));
}

inline bool StringImpl::requiresCopy() const
{
    if (bufferOwnership() != BufferInternal)
        return true;

    if (is8Bit())
        return m_data8 == tailPointer<Latin1Character>();
    return m_data16 == tailPointer<char16_t>();
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
template<typename T> inline const T* StringImpl::tailPointer() const
{
    return reinterpret_cast_ptr<const T*>(reinterpret_cast<const uint8_t*>(this) + tailOffset<T>());
}

template<typename T> inline T* StringImpl::tailPointer()
{
    return reinterpret_cast_ptr<T*>(reinterpret_cast<uint8_t*>(this) + tailOffset<T>());
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

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
    ASSERT(existingHash() == StringHasher::computeHashAndMaskTop8Bits(span8()));
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
    SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<StringImpl*>(this);
}

inline StringImpl::StaticStringImpl::operator const StringImpl&() const
{
    SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const StringImpl*>(this);
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

template<typename CharacterType, typename Predicate> ALWAYS_INLINE Ref<StringImpl> StringImpl::removeCharactersImpl(std::span<const CharacterType> characters, const Predicate& findMatch)
{
    auto from = characters;

    // Assume the common case will not remove any characters
    while (!from.empty() && !findMatch(from[0]))
        skip(from, 1);
    if (from.empty())
        return *this;

    StringBuffer<CharacterType> data(m_length);
    auto to = data.span();
    unsigned outc = from.data() - characters.data();

    copyCharacters(to, characters.first(outc));

    do {
        while (!from.empty() && findMatch(from[0]))
            skip(from, 1);
        while (!from.empty() && !findMatch(from[0]))
            to[outc++] = consume(from);
    } while (!from.empty());

    data.shrink(outc);

    return adopt(WTFMove(data));
}

template<typename Predicate>
inline Ref<StringImpl> StringImpl::removeCharacters(const Predicate& findMatch)
{
    static_assert(!std::is_function_v<Predicate>, "Passing a lambda instead of a function pointer helps the compiler with inlining");
    if (is8Bit())
        return removeCharactersImpl(span8(), findMatch);
    return removeCharactersImpl(span16(), findMatch);
}

inline Ref<StringImpl> StringImpl::createByReplacingInCharacters(std::span<const Latin1Character> characters, char16_t target, char16_t replacement, size_t indexOfFirstTargetCharacter)
{
    ASSERT(indexOfFirstTargetCharacter < characters.size());
    if (isLatin1(replacement)) {
        std::span<Latin1Character> data;
        Latin1Character oldChar = target;
        Latin1Character newChar = replacement;
        auto newImpl = createUninitializedInternalNonEmpty(characters.size(), data);
        memcpySpan(data, characters.first(indexOfFirstTargetCharacter));
        for (size_t i = indexOfFirstTargetCharacter; i != characters.size(); ++i) {
            Latin1Character character = characters[i];
            data[i] = character == oldChar ? newChar : character;
        }
        return newImpl;
    }

    std::span<char16_t> data;
    auto newImpl = createUninitializedInternalNonEmpty(characters.size(), data);
    size_t i = 0;
    for (auto character : characters)
        data[i++] = character == target ? replacement : character;
    return newImpl;
}

inline Ref<StringImpl> StringImpl::createByReplacingInCharacters(std::span<const char16_t> characters, char16_t target, char16_t replacement, size_t indexOfFirstTargetCharacter)
{
    ASSERT(indexOfFirstTargetCharacter < characters.size());
    std::span<char16_t> data;
    auto newImpl = createUninitializedInternalNonEmpty(characters.size(), data);
    copyCharacters(data, characters.first(indexOfFirstTargetCharacter));
    for (size_t i = indexOfFirstTargetCharacter; i != characters.size(); ++i) {
        char16_t character = characters[i];
        data[i] = character == target ? replacement : character;
    }
    return newImpl;
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> StringImpl::tryGetUTF8(NOESCAPE const Func& function, ConversionMode mode) const
{
    if (is8Bit())
        return tryGetUTF8ForCharacters(function, span8());
    return tryGetUTF8ForCharacters(function, span16(), mode);
}

static inline std::span<const char8_t> nonNullEmptyUTF8Span()
{
    static constexpr char8_t empty = 0;
    return { &empty, 0 };
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> StringImpl::tryGetUTF8ForCharacters(NOESCAPE const Func& function, std::span<const Latin1Character> characters)
{
    if (characters.empty())
        return function(nonNullEmptyUTF8Span());

    // Allocate a buffer big enough to hold all the characters
    // (an individual Latin1Character can only expand to 2 UTF-8 bytes).
    // Optimization ideas, if we find this function is hot:
    //  * We could speculatively create a CStringBuffer to contain 'length'
    //    characters, and resize if necessary (i.e. if the buffer contains
    //    non-ascii characters). (Alternatively, scan the buffer first for
    //    ascii characters, so we know this will be sufficient).
    //  * We could allocate a CStringBuffer with an appropriate size to
    //    have a good chance of being able to write the string into the
    //    buffer without reallocing (say, 1.5 x length).

    if (productOverflows<size_t>(characters.size(), 2) || !isValidCapacityForVector<char8_t>(characters.size() * 2)) [[unlikely]]
        return makeUnexpected(UTF8ConversionError::OutOfMemory);

#if CPU(ARM64)
    if (auto* firstNonASCII = find8NonASCII(characters)) {
        size_t prefixLength = firstNonASCII - characters.data();
        size_t remainingLength = characters.size() - prefixLength;
        Vector<char8_t, 1024> buffer(prefixLength + remainingLength * 2);
        memcpySpan(buffer.mutableSpan(), characters.first(prefixLength));
        auto result = Unicode::convert(characters.subspan(prefixLength), buffer.mutableSpan().subspan(prefixLength));
        ASSERT(result.code == Unicode::ConversionResultCode::Success); // 2x is sufficient for any conversion from Latin1
        return function(buffer.span().first(prefixLength + result.buffer.size()));
    }
    return function(byteCast<char8_t>(characters));
#else
    Vector<char8_t, 1024> buffer(characters.size() * 2);
    auto result = Unicode::convert(characters, buffer.mutableSpan());
    ASSERT(result.code == Unicode::ConversionResultCode::Success); // 2x is sufficient for any conversion from Latin1
    return function(result.buffer);
#endif
}

template<typename Func>
inline Expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> StringImpl::tryGetUTF8ForCharacters(NOESCAPE const Func& function, std::span<const char16_t> characters, ConversionMode mode)
{
    if (characters.empty())
        return function(nonNullEmptyUTF8Span());

    if (productOverflows<size_t>(characters.size(), 3) || !isValidCapacityForVector<char8_t>(characters.size() * 3)) [[unlikely]]
        return makeUnexpected(UTF8ConversionError::OutOfMemory);

    size_t bufferSize = characters.size() * 3;
    Vector<char8_t, 1024> bufferVector(bufferSize);
    auto convertedSize = utf8ForCharactersIntoBuffer(characters, mode, bufferVector);
    if (!convertedSize)
        return makeUnexpected(convertedSize.error());
    return function(bufferVector.span().first(*convertedSize));
}

} // namespace WTF

using WTF::StaticStringImpl;
using WTF::StringImpl;
using WTF::equal;
using WTF::isUnicodeWhitespace;
using WTF::deprecatedIsSpaceOrNewline;
using WTF::deprecatedIsNotSpaceOrNewline;
