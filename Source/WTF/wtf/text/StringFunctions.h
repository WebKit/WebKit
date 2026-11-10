/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// Null-safe free functions on StringImpl pointer-likes (StringImpl*, RefPtr<StringImpl>, Ref<StringImpl>,
// and the same for subclasses such as AtomStringImpl). A null pointer is a null string, which is distinct
// from the empty string, but behaves like the empty string for most operations.
//
// This header is included at the end of StringView.h, which is always included by WTFString.h (via
// AtomString.h and StringConcatenate.h). It must not depend on StringTypeAdapter, which may not be
// defined yet at that point.

#include <algorithm>
#include <array>
#include <expected>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <variant>
#include <wtf/Forward.h>
#include <wtf/text/AtomStringImpl.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringView.h>

#if USE(CF)
typedef const struct __CFString * CFStringRef;
#endif

#ifdef __OBJC__
#include <wtf/RetainPtr.h>
#endif

namespace mpark {
template<typename... Ts> class variant;
}

namespace WTF {

template<> struct IntegerToStringConversionTrait<RefPtr<StringImpl>> {
    using ReturnType = RefPtr<StringImpl>;
    using AdditionalArgumentType = void;
    static RefPtr<StringImpl> flush(std::span<const Latin1Character> characters, void*) { return StringImpl::create(characters); }
};

template<> struct IntegerToStringConversionTrait<RefPtr<AtomStringImpl>> {
    using ReturnType = RefPtr<AtomStringImpl>;
    using AdditionalArgumentType = void;
    static RefPtr<AtomStringImpl> flush(std::span<const Latin1Character> characters, void*) { return AtomStringImpl::add(characters); }
};

namespace Detail {

// Out-of-line implementations. Null pointers are handled.
WTF_EXPORT_PRIVATE Latin1CString latin1(StringImpl*);
WTF_EXPORT_PRIVATE UTF8CString utf8(StringImpl*, ConversionMode);
WTF_EXPORT_PRIVATE std::expected<Vector<char16_t>, UTF8ConversionError> charactersWithoutNullTermination(StringImpl*);
WTF_EXPORT_PRIVATE double toDouble(StringImpl*, bool* ok);
WTF_EXPORT_PRIVATE float toFloat(StringImpl*, bool* ok);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> foldCase(StringImpl*);
WTF_EXPORT_PRIVATE void split(StringImpl*, char16_t separator, NOESCAPE const Function<void(StringView)>&, bool allowEmptyEntries);
WTF_EXPORT_PRIVATE Vector<RefPtr<StringImpl>> split(StringImpl*, char16_t separator, bool allowEmptyEntries);
WTF_EXPORT_PRIVATE Vector<RefPtr<StringImpl>> split(StringImpl*, StringView separator, bool allowEmptyEntries);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeStringByRemoving(StringImpl*, unsigned position, unsigned lengthToRemove);
WTF_EXPORT_PRIVATE RefPtr<AtomStringImpl> convertToASCIILowercase(AtomStringImpl*);
WTF_EXPORT_PRIVATE RefPtr<AtomStringImpl> convertToASCIIUppercase(AtomStringImpl*);
#if USE(CF)
WTF_EXPORT_PRIVATE RetainPtr<CFStringRef> createCFString(StringImpl*);
#endif
#if OS(WINDOWS)
WTF_EXPORT_PRIVATE Vector<wchar_t> wideCharacters(StringImpl*);
#endif
#ifndef NDEBUG
WTF_EXPORT_PRIVATE void show(StringImpl*);
#endif

} // namespace Detail

// Basic properties.

template<StringImplPointerLike T> NODELETE inline bool isEmpty(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return !impl || impl->isEmpty();
}

template<StringImplPointerLike T> NODELETE inline unsigned length(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->length() : 0;
}

template<StringImplPointerLike T> NODELETE inline bool is8Bit(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return !impl || impl->is8Bit();
}

template<StringImplPointerLike T> NODELETE inline std::span<const Latin1Character> span8(const T& string LIFETIME_BOUND)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->span8() : std::span<const Latin1Character>();
}

template<StringImplPointerLike T> NODELETE inline std::span<const char16_t> span16(const T& string LIFETIME_BOUND)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->span16() : std::span<const char16_t>();
}

// Return span8() or span16() depending on CharacterType.
template<IsStringStorageCharacter CharacterType, StringImplPointerLike T> NODELETE inline std::span<const CharacterType> span(const T& string LIFETIME_BOUND)
{
    if constexpr (std::is_same_v<CharacterType, Latin1Character>)
        return span8(string);
    else
        return span16(string);
}

template<StringImplPointerLike T> NODELETE inline unsigned sizeInBytes(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->length() * (impl->is8Bit() ? sizeof(Latin1Character) : sizeof(char16_t)) : 0;
}

template<StringImplPointerLike T> NODELETE inline char16_t codeUnitAt(const T& string, unsigned index)
{
    auto* impl = toStringImplPointer(string);
    if (!impl || index >= impl->length())
        return 0;
    return impl->at(index);
}

template<StringImplPointerLike T> NODELETE inline char32_t codePointAt(const T& string, unsigned index)
{
    auto* impl = toStringImplPointer(string);
    if (!impl || index >= impl->length())
        return 0;
    return impl->codePointAt(index);
}

template<StringImplPointerLike T> inline unsigned hash(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->hash() : 0;
}

template<StringImplPointerLike T> NODELETE inline unsigned existingHash(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->existingHash() : 0;
}

template<StringImplPointerLike T> NODELETE inline bool containsOnlyASCII(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return !impl || impl->containsOnlyASCII();
}

template<StringImplPointerLike T> inline bool containsOnlyLatin1(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return !impl || impl->containsOnlyLatin1();
}

template<bool isSpecialCharacter(char16_t), StringImplPointerLike T> NODELETE inline bool containsOnly(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return !impl || impl->template containsOnly<isSpecialCharacter>();
}

// Determines the writing direction using the Unicode Bidi Algorithm rules P2 and P3.
template<StringImplPointerLike T> inline std::optional<UCharDirection> defaultWritingDirection(const T& string)
{
    auto* impl = toStringImplPointer(string);
    if (impl)
        return impl->defaultWritingDirection();
    return std::nullopt;
}

// Conversions to C strings.

template<StringImplPointerLike T> inline ASCIICString ascii(const T& string)
{
    if (is8Bit(string))
        return StringImpl::asciiForCharacters(span8(string));
    return StringImpl::asciiForCharacters(span16(string));
}

template<StringImplPointerLike T> inline Latin1CString latin1(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return Detail::latin1(impl);
}

template<StringImplPointerLike T> inline std::expected<UTF8CString, UTF8ConversionError> tryGetUTF8(const T& string, ConversionMode mode)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->tryGetUTF8(mode) : UTF8CString { u8""_span };
}

template<StringImplPointerLike T> inline std::expected<UTF8CString, UTF8ConversionError> tryGetUTF8(const T& string)
{
    return tryGetUTF8(string, LenientConversion);
}

template<StringImplPointerLike T, typename Func> requires std::invocable<const Func&, std::span<const char8_t>>
inline std::expected<std::invoke_result_t<Func, std::span<const char8_t>>, UTF8ConversionError> tryGetUTF8(const T& string, NOESCAPE const Func& function, ConversionMode mode = LenientConversion)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return function(nonNullEmptyUTF8Span());
    return impl->tryGetUTF8(function, mode);
}

// Out of line, as String::utf8() was. While this was inline, GCC's -Wformat-overflow reported a null "%s" argument
// where utf8(...) was passed to printf-style logging.
template<StringImplPointerLike T> inline UTF8CString utf8(const T& string, ConversionMode mode = LenientConversion)
{
    return Detail::utf8(toStringImplPointer(string), mode);
}

// For Swift, which cannot call the constrained templates above, nor the inline members of a class template
// specialization like UTF8CString unless C++ code instantiates them. A null string becomes an empty one.
inline std::string utf8StdString(StringImpl* string)
{
    return utf8(string).toStdString();
}

template<StringImplPointerLike T> inline std::expected<Vector<char16_t>, UTF8ConversionError> charactersWithoutNullTermination(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return Detail::charactersWithoutNullTermination(impl);
}

template<StringImplPointerLike T> inline std::expected<Vector<char16_t>, UTF8ConversionError> charactersWithNullTermination(const T& string)
{
    auto result = charactersWithoutNullTermination(string);
    if (result)
        result.value().append(0);
    return result;
}

// Searching.

template<StringImplPointerLike T> NODELETE inline size_t find(const T& string, char16_t character, unsigned start = 0)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->find(character, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t find(const T& string, Latin1Character character, unsigned start = 0)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->find(character, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t find(const T& string, char character, unsigned start = 0)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->find(character, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t find(const T& string, ASCIILiteral literal, unsigned start = 0)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->find(literal, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t find(const T& string, StringView target)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->find(target) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t find(const T& string, StringView target, unsigned start)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->find(target, start) : notFound;
}

template<StringImplPointerLike T, typename CodeUnitMatchFunction>
    requires (std::is_invocable_r_v<bool, CodeUnitMatchFunction, char16_t>)
inline size_t find(const T& string, CodeUnitMatchFunction matchFunction, unsigned start = 0)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->find(matchFunction, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t findIgnoringASCIICase(const T& string, StringView target)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->findIgnoringASCIICase(target) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t findIgnoringASCIICase(const T& string, StringView target, unsigned start)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->findIgnoringASCIICase(target, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t reverseFind(const T& string, char16_t character, unsigned start = StringImpl::MaxLength)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->reverseFind(character, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t reverseFind(const T& string, ASCIILiteral literal, unsigned start = StringImpl::MaxLength)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->reverseFind(literal, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline size_t reverseFind(const T& string, StringView target, unsigned start = StringImpl::MaxLength)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->reverseFind(target, start) : notFound;
}

template<StringImplPointerLike T> NODELETE inline bool contains(const T& string, char16_t character)
{
    return find(string, character) != notFound;
}

template<StringImplPointerLike T> NODELETE inline bool contains(const T& string, ASCIILiteral literal)
{
    return find(string, literal) != notFound;
}

template<StringImplPointerLike T> NODELETE inline bool contains(const T& string, StringView target)
{
    return find(string, target) != notFound;
}

template<StringImplPointerLike T, typename CodeUnitMatchFunction>
    requires (std::is_invocable_r_v<bool, CodeUnitMatchFunction, char16_t>)
inline bool contains(const T& string, CodeUnitMatchFunction matchFunction)
{
    return find(string, matchFunction, 0) != notFound;
}

template<StringImplPointerLike T> NODELETE inline bool containsIgnoringASCIICase(const T& string, StringView target)
{
    return findIgnoringASCIICase(string, target) != notFound;
}

template<StringImplPointerLike T> NODELETE inline bool containsIgnoringASCIICase(const T& string, StringView target, unsigned start)
{
    return findIgnoringASCIICase(string, target, start) != notFound;
}

template<StringImplPointerLike T> NODELETE inline bool startsWith(const T& string, StringView prefix)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->startsWith(prefix) : prefix.isEmpty();
}

template<StringImplPointerLike T> NODELETE inline bool startsWith(const T& string, char16_t character)
{
    auto* impl = toStringImplPointer(string);
    return impl && impl->startsWith(character);
}

template<StringImplPointerLike T> NODELETE inline bool startsWithIgnoringASCIICase(const T& string, StringView prefix)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->startsWithIgnoringASCIICase(prefix) : prefix.isEmpty();
}

template<StringImplPointerLike T> NODELETE inline bool hasInfixStartingAt(const T& string, StringView prefix, unsigned start)
{
    auto* impl = toStringImplPointer(string);
    return impl && prefix && impl->hasInfixStartingAt(prefix, start);
}

template<StringImplPointerLike T> NODELETE inline bool endsWith(const T& string, StringView suffix)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->endsWith(suffix) : suffix.isEmpty();
}

template<StringImplPointerLike T> NODELETE inline bool endsWith(const T& string, char16_t character)
{
    auto* impl = toStringImplPointer(string);
    return impl && impl->endsWith(character);
}

template<StringImplPointerLike T> NODELETE inline bool endsWith(const T& string, char character)
{
    return endsWith(string, static_cast<char16_t>(character));
}

template<StringImplPointerLike T> NODELETE inline bool endsWithIgnoringASCIICase(const T& string, StringView suffix)
{
    auto* impl = toStringImplPointer(string);
    return impl ? impl->endsWithIgnoringASCIICase(suffix) : suffix.isEmpty();
}

template<StringImplPointerLike T> NODELETE inline bool hasInfixEndingAt(const T& string, StringView suffix, unsigned end)
{
    auto* impl = toStringImplPointer(string);
    return impl && suffix && impl->hasInfixEndingAt(suffix, end);
}

// Transforms. These return the same StringImpl when nothing changes.

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> substring(const T& string, unsigned position, unsigned length = StringImpl::MaxLength)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    if (!position && length >= impl->length())
        return impl;
    return impl->substring(position, length);
}

[[nodiscard]] inline RefPtr<StringImpl> substring(RefPtr<StringImpl>&& string, unsigned position, unsigned length = StringImpl::MaxLength)
{
    if (!string)
        return nullptr;
    if (!position && length >= string->length())
        return WTF::move(string);
    return string->substring(position, length);
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> substringSharingImpl(const T& string, unsigned offset, unsigned length = StringImpl::MaxLength)
{
    // FIXME: We used to check against a limit of Heap::minExtraCost / sizeof(char16_t).
    auto* impl = toStringImplPointer(string);
    unsigned stringLength = impl ? impl->length() : 0;
    offset = std::min(offset, stringLength);
    length = std::min(length, stringLength - offset);

    if (!offset && length == stringLength)
        return impl;
    return StringImpl::createSubstringSharingImpl(*impl, offset, length);
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> left(const T& string, unsigned length)
{
    return substring(string, 0, length);
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> right(const T& string, unsigned length)
{
    unsigned start = WTF::length(string) - length;
    return substring(string, start, length);
}

#define WTF_DEFINE_STRING_IMPL_TRANSFORM(name) \
    template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> name(const T& string) \
    { \
        auto* impl = toStringImplPointer(string); \
        if (!impl) \
            return nullptr; \
        return impl->name(); \
    }

#define WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(name, ArgumentType) \
    template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> name(const T& string, ArgumentType argument) \
    { \
        auto* impl = toStringImplPointer(string); \
        if (!impl) \
            return nullptr; \
        return impl->name(argument); \
    }

WTF_DEFINE_STRING_IMPL_TRANSFORM(convertToLowercaseWithoutLocale)
WTF_DEFINE_STRING_IMPL_TRANSFORM(convertToUppercaseWithoutLocale)
WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit, unsigned)
WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(convertToLowercaseWithoutLocaleStartingAtFailingIndex16Bit, unsigned)
WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(convertToUppercaseWithoutLocaleStartingAtFailingIndex8Bit, unsigned)
WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(convertToUppercaseWithoutLocaleStartingAtFailingIndex16Bit, unsigned)
WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(convertToLowercaseWithLocale, AtomStringImpl*)
WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(convertToUppercaseWithLocale, AtomStringImpl*)
WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(simplifyWhiteSpace, CodeUnitMatchFunction)
WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT(trim, CodeUnitMatchFunction)

#undef WTF_DEFINE_STRING_IMPL_TRANSFORM
#undef WTF_DEFINE_STRING_IMPL_TRANSFORM_WITH_ARGUMENT

// For non-atoms, these return RefPtr<StringImpl>.
template<StringImplPointerLike T> requires (!AtomStringImplPointerLike<T>)
[[nodiscard]] inline RefPtr<StringImpl> convertToASCIILowercase(const T& string)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->convertToASCIILowercase();
}

template<StringImplPointerLike T> requires (!AtomStringImplPointerLike<T>)
[[nodiscard]] inline RefPtr<StringImpl> convertToASCIIUppercase(const T& string)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->convertToASCIIUppercase();
}

// For atoms, these return RefPtr<AtomStringImpl>.
template<AtomStringImplPointerLike T> [[nodiscard]] inline RefPtr<AtomStringImpl> convertToASCIILowercase(const T& string)
{
    auto* atom = uncheckedDowncast<AtomStringImpl>(toStringImplPointer(string));
    return Detail::convertToASCIILowercase(atom);
}

template<AtomStringImplPointerLike T> [[nodiscard]] inline RefPtr<AtomStringImpl> convertToASCIIUppercase(const T& string)
{
    auto* atom = uncheckedDowncast<AtomStringImpl>(toStringImplPointer(string));
    return Detail::convertToASCIIUppercase(atom);
}

// Returns the string with case folded for case insensitive comparison.
// Use convertToASCIILowercase instead if ASCII case insensitive comparison is desired.
template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> foldCase(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return Detail::foldCase(impl);
}

template<StringImplPointerLike T, typename Predicate> [[nodiscard]] inline RefPtr<StringImpl> removeCharacters(const T& string, NOESCAPE const Predicate& findMatch)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->removeCharacters(findMatch);
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> makeStringByReplacingAll(const T& string, char16_t target, char16_t replacement)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->replace(target, replacement);
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> makeStringByReplacingAll(const T& string, char16_t target, ASCIILiteral literal)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->replace(target, literal.span8());
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> makeStringByReplacingAll(const T& string, char16_t target, StringView replacement)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->replace(target, replacement);
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> makeStringByReplacingAll(const T& string, StringView target, StringView replacement)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->replace(target, replacement);
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> makeStringByReplacing(const T& string, unsigned start, unsigned length, StringView replacement)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->replace(start, length, replacement);
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> makeStringByRemoving(const T& string, unsigned position, unsigned lengthToRemove)
{
    auto* impl = toStringImplPointer(string);
    return Detail::makeStringByRemoving(impl, position, lengthToRemove);
}

// Null elements and a null separator are treated as empty strings. The result is never null.
[[nodiscard]] WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeStringByJoining(std::span<const RefPtr<StringImpl>> strings, StringImpl* separator);

// Splitting. Elements are never null, except that splitAllowingEmptyEntries() of a null string results in one null element.

template<StringImplPointerLike T> inline void split(const T& string, char16_t separator, NOESCAPE const Function<void(StringView)>& functor)
{
    Detail::split(toStringImplPointer(string), separator, functor, false);
}

template<StringImplPointerLike T> [[nodiscard]] inline Vector<RefPtr<StringImpl>> split(const T& string, char16_t separator)
{
    return Detail::split(toStringImplPointer(string), separator, false);
}

template<StringImplPointerLike T> [[nodiscard]] inline Vector<RefPtr<StringImpl>> split(const T& string, StringView separator)
{
    return Detail::split(toStringImplPointer(string), separator, false);
}

template<StringImplPointerLike T> inline void splitAllowingEmptyEntries(const T& string, char16_t separator, NOESCAPE const Function<void(StringView)>& functor)
{
    Detail::split(toStringImplPointer(string), separator, functor, true);
}

template<StringImplPointerLike T> [[nodiscard]] inline Vector<RefPtr<StringImpl>> splitAllowingEmptyEntries(const T& string, char16_t separator)
{
    return Detail::split(toStringImplPointer(string), separator, true);
}

template<StringImplPointerLike T> [[nodiscard]] inline Vector<RefPtr<StringImpl>> splitAllowingEmptyEntries(const T& string, StringView separator)
{
    return Detail::split(toStringImplPointer(string), separator, true);
}

// Number parsing. Null results in 0 with ok set to false.

template<StringImplPointerLike T> inline double toDouble(const T& string, bool* ok = nullptr)
{
    auto* impl = toStringImplPointer(string);
    return Detail::toDouble(impl, ok);
}

template<StringImplPointerLike T> inline float toFloat(const T& string, bool* ok = nullptr)
{
    auto* impl = toStringImplPointer(string);
    return Detail::toFloat(impl, ok);
}

// Threading.

template<StringImplPointerLike T> NODELETE inline bool isSafeToSendToAnotherThread(const T& string)
{
    // AtomStrings are not safe to send between threads, as ~StringImpl()
    // will try to remove them from the wrong AtomStringTable.
    auto* impl = toStringImplPointer(string);
    return !impl || impl->isEmpty() || (impl->hasOneRef() && !impl->isAtom());
}

template<StringImplPointerLike T> [[nodiscard]] inline RefPtr<StringImpl> isolatedCopy(const T& string)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    return impl->isolatedCopy();
}

// This steals the StringImpl when it is safe to do so.
[[nodiscard]] inline RefPtr<StringImpl> isolatedCopy(RefPtr<StringImpl>&& string)
{
    if (isSafeToSendToAnotherThread(string))
        return WTF::move(string);
    return string->isolatedCopy();
}

// Atoms.

template<StringImplPointerLike T> inline RefPtr<AtomStringImpl> toExistingAtomString(const T& string)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nullptr;
    if (auto* atom = dynamicDowncast<AtomStringImpl>(*impl))
        return atom;
    return AtomStringImpl::lookUp(impl);
}

// In-place operations on owning pointers.

inline void convertTo16Bit(RefPtr<StringImpl>& string)
{
    if (!string || !string->is8Bit())
        return;
    std::span<char16_t> destination;
    auto convertedString = StringImpl::createUninitialized(string->length(), destination);
    StringImpl::copyCharacters(destination, string->span8());
    string = WTF::move(convertedString);
}

// Clears the pointer if the StringImpl is not referenced by anyone else. This is useful for clearing string-based caches.
inline void clearImplIfNotShared(RefPtr<StringImpl>& string)
{
    if (string && string->hasOneRef())
        string = nullptr;
}

// Comparison. equal() respects nullity: a null string is not equal to an empty string.
// Comparisons with StringView use StringView semantics, which ignore nullity.

template<StringImplPointerLike A, StringImplPointerLike B> NODELETE inline bool equal(const A& a, const B& b)
{
    return equal(static_cast<const StringImpl*>(toStringImplPointer(a)), static_cast<const StringImpl*>(toStringImplPointer(b)));
}

template<StringImplPointerLike T> NODELETE inline bool equal(const T& a, ASCIILiteral b)
{
    return equal(static_cast<const StringImpl*>(toStringImplPointer(a)), b);
}

template<StringImplPointerLike T> NODELETE inline bool equal(ASCIILiteral a, const T& b)
{
    return equal(static_cast<const StringImpl*>(toStringImplPointer(b)), a);
}

template<StringImplPointerLike T> NODELETE inline bool equal(const T& a, const char* b)
{
    return equal(static_cast<const StringImpl*>(toStringImplPointer(a)), b);
}

template<StringImplPointerLike T> NODELETE inline bool equal(const T& a, StringView b)
{
    return equal(StringView { toStringImplPointer(a) }, b);
}

template<StringImplPointerLike T> NODELETE inline bool equal(StringView a, const T& b)
{
    return equal(a, StringView { toStringImplPointer(b) });
}

template<StringImplPointerLike T> NODELETE inline bool equal(const T& a, std::span<const Latin1Character> b)
{
    return equal(static_cast<const StringImpl*>(toStringImplPointer(a)), b);
}

template<StringImplPointerLike T> NODELETE inline bool equal(const T& a, std::span<const char16_t> b)
{
    return equal(static_cast<const StringImpl*>(toStringImplPointer(a)), b);
}

// Compares with UTF-8 code units; a null string equals an empty span, as with the StringCommon.h overload.
template<StringImplPointerLike T> NODELETE inline bool equal(const T& a, std::span<const char8_t> b)
{
    return equal(StringView { toStringImplPointer(a) }, b);
}

// More constrained than the overload in StringCommon.h, which expects a string class.
template<StringImplPointerLike T, unsigned length> NODELETE inline bool equal(const T& a, const char16_t (&codeUnits)[length])
{
    return equal(a, std::span<const char16_t> { codeUnits });
}

template<StringImplPointerLike A, StringImplPointerLike B> NODELETE inline bool equalIgnoringNullity(const A& a, const B& b)
{
    return equalIgnoringNullity(toStringImplPointer(a), toStringImplPointer(b));
}

template<StringImplPointerLike A, StringImplPointerLike B> NODELETE inline bool equalIgnoringASCIICase(const A& a, const B& b)
{
    return equalIgnoringASCIICase(static_cast<const StringImpl*>(toStringImplPointer(a)), static_cast<const StringImpl*>(toStringImplPointer(b)));
}

template<StringImplPointerLike T> NODELETE inline bool equalIgnoringASCIICase(const T& a, ASCIILiteral b)
{
    return equalIgnoringASCIICase(static_cast<const StringImpl*>(toStringImplPointer(a)), b);
}

template<StringImplPointerLike T> NODELETE inline bool equalIgnoringASCIICase(const T& a, StringView b)
{
    return equalIgnoringASCIICase(StringView { toStringImplPointer(a) }, b);
}

template<StringImplPointerLike T> NODELETE inline bool equalIgnoringASCIICase(StringView a, const T& b)
{
    return equalIgnoringASCIICase(a, StringView { toStringImplPointer(b) });
}

template<StringImplPointerLike T> NODELETE inline bool equalLettersIgnoringASCIICase(const T& string, ASCIILiteral literal)
{
    return equalLettersIgnoringASCIICase(static_cast<const StringImpl*>(toStringImplPointer(string)), literal);
}

template<StringImplPointerLike T> NODELETE inline bool startsWithLettersIgnoringASCIICase(const T& string, ASCIILiteral literal)
{
    return startsWithLettersIgnoringASCIICase(static_cast<const StringImpl*>(toStringImplPointer(string)), literal);
}

// Identity comparison, for the rare cases where it is intended.
template<StringImplPointerLike A, StringImplPointerLike B> NODELETE inline bool isSameImpl(const A& a, const B& b)
{
    return toStringImplPointer(a) == toStringImplPointer(b);
}

// Bridging.

#if USE(CF)
// Null results in an empty CFString.
template<StringImplPointerLike T> inline RetainPtr<CFStringRef> createCFString(const T& string)
{
    auto* impl = toStringImplPointer(string);
    return Detail::createCFString(impl);
}
#endif

#if USE(FOUNDATION) && defined(__OBJC__)

// This converts null to an empty NSString rather than to nil. Given Cocoa idioms, this is a more useful default.
// Clients that need to preserve the null string can use nsStringNilIfNull().
template<StringImplPointerLike T> inline RetainPtr<NSString> createNSString(const T& string)
{
    if (auto* impl = toStringImplPointer(string))
        return impl->createNSString();
    return @"";
}

// Used in a small number of places where the long standing behavior has been "nil if empty".
template<StringImplPointerLike T> inline RetainPtr<NSString> nsStringNilIfEmpty(const T& string)
{
    if (isEmpty(string))
        return nil;
    return toStringImplPointer(string)->createNSString();
}

// Used in a small number of places where null strings should be converted to nil but empty strings should be maintained.
template<StringImplPointerLike T> inline RetainPtr<NSString> nsStringNilIfNull(const T& string)
{
    auto* impl = toStringImplPointer(string);
    if (!impl)
        return nil;
    return impl->createNSString();
}

#endif

#if OS(WINDOWS)
template<StringImplPointerLike T> inline Vector<wchar_t> wideCharacters(const T& string)
{
    return Detail::wideCharacters(toStringImplPointer(string));
}
#endif

#ifndef NDEBUG
template<StringImplPointerLike T> inline void show(const T& string)
{
    Detail::show(toStringImplPointer(string));
}
#endif

// contentEquals(a, b): equality with value semantics for strings, for use in operator== of structs
// with string members and for comparing containers of strings. Plain StringImpl pointer-likes compare
// by content (equal()); atoms (AtomStringImpl, UniquedStringImpl, SymbolImpl) compare by identity.
// Containers, optionals, pairs, key-value pairs, tuples, variants, expecteds and Markables compare element-wise;
// everything else uses ==.

template<typename T> concept ContentEqualsUsesIdentity = StringImplPointerLike<T> && std::is_convertible_v<StringImplPointeeType<T>*, const UniquedStringImpl*>;

template<StringImplPointerLike T> bool contentEquals(const T&, const T&);
template<typename T> requires (!StringImplPointerLike<T> && requires(const T& value) { { value == value } -> std::convertible_to<bool>; })
bool contentEquals(const T&, const T&);
template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
bool contentEquals(const Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>&, const Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>&);
template<typename T, typename Malloc> bool contentEquals(const FixedVector<T, Malloc>&, const FixedVector<T, Malloc>&);
template<typename T, size_t size> bool contentEquals(const std::array<T, size>&, const std::array<T, size>&);
template<typename T, size_t extent> bool contentEquals(const std::span<T, extent>&, const std::span<T, extent>&);
template<typename T> bool contentEquals(const std::optional<T>&, const std::optional<T>&);
template<typename T, typename Traits> bool contentEquals(const Markable<T, Traits>&, const Markable<T, Traits>&);
template<typename A, typename B> bool contentEquals(const std::pair<A, B>&, const std::pair<A, B>&);
template<typename K, typename V> bool contentEquals(const KeyValuePair<K, V>&, const KeyValuePair<K, V>&);
template<typename... Ts> bool contentEquals(const std::tuple<Ts...>&, const std::tuple<Ts...>&);
template<typename... Ts> bool contentEquals(const mpark::variant<Ts...>&, const mpark::variant<Ts...>&);
template<typename... Ts> bool contentEquals(const std::variant<Ts...>&, const std::variant<Ts...>&);
template<typename T, typename E> bool contentEquals(const std::expected<T, E>&, const std::expected<T, E>&);
template<typename K, typename V, typename H, typename KT, typename VT, typename TT, ShouldValidateKey shouldValidateKey, typename M>
bool contentEquals(const HashMap<K, V, H, KT, VT, TT, shouldValidateKey, M>&, const HashMap<K, V, H, KT, VT, TT, shouldValidateKey, M>&);
template<typename V, typename H, typename T, typename TT, ShouldValidateKey shouldValidateKey>
bool contentEquals(const HashSet<V, H, T, TT, shouldValidateKey>&, const HashSet<V, H, T, TT, shouldValidateKey>&);

namespace Detail {

template<typename Range> bool contentEqualsForRanges(const Range& a, const Range& b)
{
    // std::ranges::equal compares the sizes of sized ranges first.
    return std::ranges::equal(a, b, [](auto& aElement, auto& bElement) {
        return contentEquals(aElement, bElement);
    });
}

template<typename Variant> bool contentEqualsForVariants(const Variant& a, const Variant& b, auto alternativeCount)
{
    if (a.index() != b.index())
        return false;
    return [&]<size_t... indices>(std::index_sequence<indices...>) -> bool {
        return ((a.index() == indices && contentEquals(get<indices>(a), get<indices>(b))) || ...);
    }(std::make_index_sequence<decltype(alternativeCount)::value> { });
}

} // namespace Detail

template<StringImplPointerLike T> inline bool contentEquals(const T& a, const T& b)
{
    if constexpr (ContentEqualsUsesIdentity<T>)
        return isSameImpl(a, b);
    else
        return equal(a, b);
}

template<typename T> requires (!StringImplPointerLike<T> && requires(const T& value) { { value == value } -> std::convertible_to<bool>; })
inline bool contentEquals(const T& a, const T& b)
{
    return a == b;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline bool contentEquals(const Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& a, const Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& b)
{
    return Detail::contentEqualsForRanges(a, b);
}

template<typename T, typename Malloc> inline bool contentEquals(const FixedVector<T, Malloc>& a, const FixedVector<T, Malloc>& b)
{
    return Detail::contentEqualsForRanges(a, b);
}

template<typename T, size_t size> inline bool contentEquals(const std::array<T, size>& a, const std::array<T, size>& b)
{
    return Detail::contentEqualsForRanges(a, b);
}

template<typename T, size_t extent> inline bool contentEquals(const std::span<T, extent>& a, const std::span<T, extent>& b)
{
    return Detail::contentEqualsForRanges(a, b);
}

template<typename T> inline bool contentEquals(const std::optional<T>& a, const std::optional<T>& b)
{
    if (a.has_value() != b.has_value())
        return false;
    return !a.has_value() || contentEquals(*a, *b);
}

template<typename T, typename Traits> inline bool contentEquals(const Markable<T, Traits>& a, const Markable<T, Traits>& b)
{
    if (static_cast<bool>(a) != static_cast<bool>(b))
        return false;
    return !a || contentEquals(*a, *b);
}

template<typename A, typename B> inline bool contentEquals(const std::pair<A, B>& a, const std::pair<A, B>& b)
{
    return contentEquals(a.first, b.first) && contentEquals(a.second, b.second);
}

template<typename K, typename V> inline bool contentEquals(const KeyValuePair<K, V>& a, const KeyValuePair<K, V>& b)
{
    return contentEquals(a.key, b.key) && contentEquals(a.value, b.value);
}

template<typename... Ts> inline bool contentEquals(const std::tuple<Ts...>& a, const std::tuple<Ts...>& b)
{
    return [&]<size_t... indices>(std::index_sequence<indices...>) -> bool {
        return (contentEquals(std::get<indices>(a), std::get<indices>(b)) && ...);
    }(std::index_sequence_for<Ts...> { });
}

template<typename... Ts> inline bool contentEquals(const mpark::variant<Ts...>& a, const mpark::variant<Ts...>& b)
{
    return Detail::contentEqualsForVariants(a, b, std::integral_constant<size_t, sizeof...(Ts)> { });
}

template<typename... Ts> inline bool contentEquals(const std::variant<Ts...>& a, const std::variant<Ts...>& b)
{
    return Detail::contentEqualsForVariants(a, b, std::integral_constant<size_t, sizeof...(Ts)> { });
}

template<typename T, typename E> inline bool contentEquals(const std::expected<T, E>& a, const std::expected<T, E>& b)
{
    if (a.has_value() != b.has_value())
        return false;
    if (!a.has_value())
        return contentEquals(a.error(), b.error());
    if constexpr (std::is_void_v<T>)
        return true;
    else
        return contentEquals(*a, *b);
}

template<typename K, typename V, typename H, typename KT, typename VT, typename TT, ShouldValidateKey shouldValidateKey, typename M>
inline bool contentEquals(const HashMap<K, V, H, KT, VT, TT, shouldValidateKey, M>& a, const HashMap<K, V, H, KT, VT, TT, shouldValidateKey, M>& b)
{
    if (a.size() != b.size())
        return false;
    for (auto& entry : a) {
        auto iterator = b.find(entry.key);
        if (iterator == b.end() || !contentEquals(entry.value, iterator->value))
            return false;
    }
    return true;
}

template<typename V, typename H, typename T, typename TT, ShouldValidateKey shouldValidateKey>
inline bool contentEquals(const HashSet<V, H, T, TT, shouldValidateKey>& a, const HashSet<V, H, T, TT, shouldValidateKey>& b)
{
    if (a.size() != b.size())
        return false;
    for (auto& value : a) {
        if (!b.contains(value))
            return false;
    }
    return true;
}

} // namespace WTF

using WTF::contentEquals;
using WTF::makeStringByJoining;
using WTF::makeStringByRemoving;
using WTF::makeStringByReplacing;
using WTF::makeStringByReplacingAll;
