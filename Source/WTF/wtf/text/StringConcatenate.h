/*
 * Copyright (C) 2010-2024 Apple Inc. All rights reserved.
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

#include <atomic>
#include <cstring>
#include <functional>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringView.h>

#if defined(NDEBUG)
#define WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING() do { } while (0)
#else
#define WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING() do { ++WTF::Detail::wtfStringCopyCount; } while (0)
namespace WTF::Detail {
// This variable is helpful for testing how many intermediate Strings are created while evaluating an
// expression containing operator+.
WTF_EXPORT_PRIVATE extern std::atomic<int> wtfStringCopyCount;
}
#endif

namespace WTF {

class StringBuilder;

/// A type is `StringTypeAdaptable` if there is a specialization of `StringTypeAdapter` for that type.
template<typename StringType> concept StringTypeAdaptable = requires {
    typename StringTypeAdapter<StringType>;
};

template<> class StringTypeAdapter<char> {
public:
    StringTypeAdapter(char character)
        : m_character { character }
    {
    }

    unsigned length() const { return 1; }
    bool is8Bit() const { return true; }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const { destination[0] = m_character; }

private:
    char m_character;
};

template<> class StringTypeAdapter<char16_t> {
public:
    StringTypeAdapter(char16_t character)
        : m_character { character }
    {
    }

    unsigned length() const { return 1; }
    bool is8Bit() const { return isLatin1(m_character); }

    void writeTo(std::span<Latin1Character> destination) const
    {
        ASSERT(is8Bit());
        destination[0] = m_character;
    }

    void writeTo(std::span<char16_t> destination) const { destination[0] = m_character; }

private:
    char16_t m_character;
};

template<> class StringTypeAdapter<char32_t> {
public:
    StringTypeAdapter(char32_t character)
        : m_character { character }
    {
    }

    unsigned length() const { return U16_LENGTH(m_character); }
    bool is8Bit() const { return isLatin1(m_character); }

    void writeTo(std::span<Latin1Character> destination) const
    {
        ASSERT(is8Bit());
        destination[0] = m_character;
    }

    void writeTo(std::span<char16_t> destination) const
    {
        if (U_IS_BMP(m_character)) {
            destination[0] = m_character;
            return;
        }
        destination[0] = U16_LEAD(m_character);
        destination[1] = U16_TRAIL(m_character);
    }

private:
    char32_t m_character;
};

template<> class StringTypeAdapter<const char16_t*> {
public:
    StringTypeAdapter(const char16_t* characters)
        : m_characters { unsafeSpan(characters) }
    {
        RELEASE_ASSERT(m_characters.size() <= String::MaxLength);
    }

    unsigned length() const { return m_characters.size(); }
    bool is8Bit() const { return m_characters.empty(); }
    void writeTo(std::span<Latin1Character>) const { ASSERT(m_characters.empty()); }
    void writeTo(std::span<char16_t> destination) const { StringImpl::copyCharacters(destination, m_characters); }

private:
    std::span<const char16_t> m_characters;
};

template<typename CharacterType, size_t Extent> class StringTypeAdapter<std::span<CharacterType, Extent>> {
public:
    StringTypeAdapter(std::span<CharacterType, Extent> span)
        : m_characters { span }
    {
        RELEASE_ASSERT(m_characters.size() <= String::MaxLength);
    }

    unsigned length() const { return m_characters.size(); }
    static constexpr bool is8Bit() { return sizeof(CharacterType) == 1; }

    template<typename DestinationCharacterType> void writeTo(std::span<DestinationCharacterType> destination) const
    {
        using CharacterTypeForString = std::conditional_t<sizeof(CharacterType) == sizeof(Latin1Character), Latin1Character, char16_t>;
        static_assert(sizeof(CharacterTypeForString) == sizeof(CharacterType));
        StringImpl::copyCharacters(destination, spanReinterpretCast<const CharacterTypeForString>(m_characters));
    }

private:
    std::span<const CharacterType> m_characters;
};

template<> class StringTypeAdapter<CString> : public StringTypeAdapter<std::span<const char>> {
public:
    StringTypeAdapter(const CString& string)
        : StringTypeAdapter<std::span<const char>> { spanReinterpretCast<const char>(string.span()) }
    {
    }
};

template<> class StringTypeAdapter<ASCIILiteral> : public StringTypeAdapter<std::span<const Latin1Character>> {
public:
    StringTypeAdapter(ASCIILiteral characters)
        : StringTypeAdapter<std::span<const Latin1Character>> { characters.span8() }
    {
    }
};

template<typename CharacterType, size_t InlineCapacity> class StringTypeAdapter<Vector<CharacterType, InlineCapacity>> : public StringTypeAdapter<std::span<const CharacterType>> {
public:
    StringTypeAdapter(const Vector<CharacterType, InlineCapacity>& vector)
        : StringTypeAdapter<std::span<const CharacterType>> { vector.span() }
    {
    }
};

template<> class StringTypeAdapter<StringImpl*> {
public:
    StringTypeAdapter(StringImpl* string)
        : m_string { string }
    {
    }

    unsigned length() const { return m_string ? m_string->length() : 0; }
    bool is8Bit() const { return !m_string || m_string->is8Bit(); }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const
    {
        StringView { m_string }.getCharacters(destination);
        WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
    }

private:
    SUPPRESS_UNCOUNTED_MEMBER StringImpl* const m_string;
};

template<> class StringTypeAdapter<AtomStringImpl*> : public StringTypeAdapter<StringImpl*> {
public:
    StringTypeAdapter(AtomStringImpl* string)
        : StringTypeAdapter<StringImpl*> { static_cast<StringImpl*>(string) }
    {
    }
};

template<> class StringTypeAdapter<String> : public StringTypeAdapter<StringImpl*> {
public:
    StringTypeAdapter(const String& string)
        : StringTypeAdapter<StringImpl*> { string.impl() }
    {
    }
};

template<> class StringTypeAdapter<AtomString> : public StringTypeAdapter<String> {
public:
    StringTypeAdapter(const AtomString& string)
        : StringTypeAdapter<String> { string.string() }
    {
    }
};

template<> class StringTypeAdapter<StringImpl&> {
public:
    StringTypeAdapter(StringImpl& string)
        : m_string { string }
    {
    }

    unsigned length() const { return m_string.length(); }
    bool is8Bit() const { return m_string.is8Bit(); }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const
    {
        StringView { m_string }.getCharacters(destination);
        WTF_STRINGTYPEADAPTER_COPIED_WTF_STRING();
    }

private:
    SUPPRESS_UNCOUNTED_MEMBER StringImpl& m_string;
};

template<> class StringTypeAdapter<AtomStringImpl&> : public StringTypeAdapter<StringImpl&> {
public:
    StringTypeAdapter(StringImpl& string)
        : StringTypeAdapter<StringImpl&> { string }
    {
    }
};

template<> class StringTypeAdapter<Unicode::CheckedUTF8> {
public:
    StringTypeAdapter(Unicode::CheckedUTF8 characters)
        : m_characters { characters }
    {
        if (m_characters.lengthUTF16 > String::MaxLength)
            m_characters.lengthUTF16 = 0;
    }

    unsigned length() const { return m_characters.lengthUTF16; }
    bool is8Bit() const { return m_characters.isAllASCII; }
    void writeTo(std::span<Latin1Character> destination) const { memcpySpan(destination, unsafeMakeSpan(m_characters.characters.data(), m_characters.lengthUTF16)); }
#ifndef __swift__ // FIXME: This fails to compile because of rdar://136156228
    void writeTo(std::span<char16_t> destination) const { Unicode::convert(m_characters.characters, destination.first(m_characters.lengthUTF16)); }
#endif

private:
    Unicode::CheckedUTF8 m_characters;
};

template<size_t Extent> class StringTypeAdapter<std::span<const char8_t, Extent>> : public StringTypeAdapter<Unicode::CheckedUTF8> {
public:
    StringTypeAdapter(std::span<const char8_t, Extent> span)
        : StringTypeAdapter<Unicode::CheckedUTF8> { Unicode::checkUTF8(span) }
    {
    }
};

template<typename... StringTypes> class StringTypeAdapter<std::tuple<StringTypes...>> {
public:
    StringTypeAdapter(const std::tuple<StringTypes...>& tuple)
        : m_tuple { tuple }
        , m_length { std::apply(computeLength, tuple) }
        , m_is8Bit { std::apply(computeIs8Bit, tuple) }
    {
    }

    unsigned length() const { return m_length; }
    bool is8Bit() const { return m_is8Bit; }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const
    {
        std::apply([&](const StringTypes&... strings) {
            unsigned offset = 0;
            (..., (
                StringTypeAdapter<StringTypes>(strings).writeTo(destination.subspan(offset)),
                offset += StringTypeAdapter<StringTypes>(strings).length()
            ));
        }, m_tuple);
    }

private:
    static unsigned computeLength(const StringTypes&... strings)
    {
        return (... + StringTypeAdapter<StringTypes>(strings).length());
    }

    static bool computeIs8Bit(const StringTypes&... strings)
    {
        return (... && StringTypeAdapter<StringTypes>(strings).is8Bit());
    }
    const std::tuple<StringTypes...>& m_tuple;
    unsigned m_length;
    bool m_is8Bit;
};

template<typename UnderlyingElementType> struct PaddingSpecification {
    Latin1Character character;
    unsigned length;
    UnderlyingElementType underlyingElement;
};

template<typename UnderlyingElementType> PaddingSpecification<UnderlyingElementType> pad(char character, unsigned length, UnderlyingElementType element)
{
    return { byteCast<Latin1Character>(character), length, element };
}

template<typename UnderlyingElementType> class StringTypeAdapter<PaddingSpecification<UnderlyingElementType>> {
public:
    StringTypeAdapter(const PaddingSpecification<UnderlyingElementType>& padding)
        : m_padding { padding }
        , m_underlyingAdapter { m_padding.underlyingElement }
    {
    }

    unsigned length() const { return std::max(m_padding.length, m_underlyingAdapter.length()); }
    bool is8Bit() const { return m_underlyingAdapter.is8Bit(); }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const
    {
        unsigned underlyingLength = m_underlyingAdapter.length();
        unsigned count = 0;
        if (underlyingLength < m_padding.length) {
            count = m_padding.length - underlyingLength;
            for (unsigned i = 0; i < count; ++i)
                destination[i] = m_padding.character;
        }
        m_underlyingAdapter.writeTo(destination.subspan(count));
    }

private:
    const PaddingSpecification<UnderlyingElementType>& m_padding;
    StringTypeAdapter<UnderlyingElementType> m_underlyingAdapter;
};

template<unsigned N> struct Indentation {
    unsigned operator++() { return ++value; }
    unsigned operator++(int) { return value++; }
    unsigned operator--() { return --value; }
    unsigned operator--(int) { return value--; }

    unsigned value { 0 };
};

template<unsigned N>
struct IndentationScope {
    IndentationScope(Indentation<N>& indentation)
        : m_indentation(indentation)
    {
        ++m_indentation;
    }
    ~IndentationScope()
    {
        --m_indentation;
    }

    Indentation<N>& m_indentation;
};

template<unsigned N> class StringTypeAdapter<Indentation<N>> {
public:
    StringTypeAdapter(Indentation<N> indentation)
        : m_indentation { indentation }
    {
    }

    unsigned length() const
    {
        return m_indentation.value * N;
    }

    bool is8Bit() const
    {
        return true;
    }

    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const
    {
        std::fill_n(destination.data(), m_indentation.value * N, ' ');
    }

private:
    Indentation<N> m_indentation;
};

struct ASCIICaseConverter {
    StringView::CaseConvertType type;
    StringView string;
};

inline ASCIICaseConverter asASCIILowercase(StringView stringView)
{
    return { StringView::CaseConvertType::Lower, stringView };
}

inline ASCIICaseConverter asASCIIUppercase(StringView stringView)
{
    return { StringView::CaseConvertType::Upper, stringView };
}

template<> class StringTypeAdapter<ASCIICaseConverter> {
public:
    StringTypeAdapter(const ASCIICaseConverter& converter)
        : m_converter { converter }
    {
    }

    unsigned length() const { return m_converter.string.length(); }
    bool is8Bit() const { return m_converter.string.is8Bit(); }
    template<typename CharacterType> void writeTo(std::span<CharacterType> destination) const
    {
        m_converter.string.getCharactersWithASCIICase(m_converter.type, destination);
    }

private:
    const ASCIICaseConverter& m_converter;
};

template<typename C, typename E, typename B> class Interleave {
public:
    Interleave(const C& container, E each, const B& between)
        : container { container }
        , each { WTFMove(each) }
        , between { between }
    {
    }

    Interleave(const Interleave&) = delete;
    Interleave& operator=(const Interleave&) = delete;

    Interleave(Interleave&&) = default;
    Interleave& operator=(Interleave&&) = default;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    template<typename Accumulator> void writeUsing(Accumulator& accumulator) const
    {
        auto begin = std::begin(container);
        auto end = std::end(container);
        if (begin == end)
            return;

        constexpr bool eachTakesAccumulator = requires {
            { std::invoke(each, accumulator, *begin) } -> std::same_as<void>;
        };

        if constexpr (eachTakesAccumulator) {
            std::invoke(each, accumulator, *begin);

            ++begin;
            for (; begin != end; ++begin) {
                accumulator.append(between);
                std::invoke(each, accumulator, *begin);
            }
        } else {
            accumulator.append(std::invoke(each, *begin));

            ++begin;
            for (; begin != end; ++begin)
                accumulator.append(between, std::invoke(each, *begin));
        }
    }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

private:
    const C& container;
    E each;
    const B& between;
};

// The `interleave` function can be called in three different ways:
//
//  1. The most generic way provides an `each` functor taking two arguments,
//     the `accumulator` and the `value`, and returns `void`.
//
//       Vector<Foo> container = { ... };
//
//       ... interleave(
//              container,
//              [](auto& accumulator, auto& value) {
//                  accumulator.append(value.stringRepresentation(), '-', value.otherStringRepresentation());
//              },
//              ", "_s
//           ), ...
//
//     This allows for containers of non-string values to provide complex mapped
//     values without additional allocations.
//
//  2. If multiple mapped strings per-value are not required, an `each` functor
//     taking just the `value` and returning a "string-type" (i.e. something you
//     could pass to `StringBuilder::append(...)`).
//
//       Vector<Foo> container = { ... };
//
//       ... interleave(
//              container,
//              [](auto& value) {
//                  return value.stringRepresentation();
//              },
//              ", "_s
//           ), ...
//
//  3. Finally, if the container already contains "string-types", no `each` functor
//     is required at all.
//
//       Vector<String> container = { ... };
//
//       ... interleave(
//              container,
//              ", "_s
//           ), ...
//

template<typename C, typename E> concept EachTakingValue = requires(C&& container, E&& each) {
    { each(*std::begin(container)) } -> StringTypeAdaptable;
};

template<typename C, typename E> concept EachTakingAccumulatorAndValue = requires(C&& container, E&& each) {
    { each(std::declval<StringBuilder&>(), *std::begin(container)) } -> std::same_as<void>;
};

template<typename C> using EachTakingAccumulatorAndValueFunction = void(&)(StringBuilder&, const std::remove_reference_t<decltype(*std::begin(std::declval<C>()))>&);

template<typename C, std::invocable<decltype(*std::begin(std::declval<C>()))> E, StringTypeAdaptable B>
    requires EachTakingValue<C, E>
decltype(auto) interleave(const C& container, E each, const B& between)
{
    return Interleave {
        container,
        WTFMove(each),
        between
    };
}

template<typename C, std::invocable<decltype(std::declval<StringBuilder&>()), decltype(*std::begin(std::declval<C>()))> E, StringTypeAdaptable B>
    requires EachTakingAccumulatorAndValue<C, E>
decltype(auto) interleave(const C& container, E each, const B& between)
{
    return Interleave {
        container,
        WTFMove(each),
        between
    };
}

template<typename C, StringTypeAdaptable B> decltype(auto) interleave(const C& container, EachTakingAccumulatorAndValueFunction<C> each, B&& between)
{
    return Interleave {
        container,
        each,
        between
    };
}

template<typename C, StringTypeAdaptable B> decltype(auto) interleave(const C& container, const B& between)
{
    return interleave(
        container,
        []<typename A, typename V>(A& accumulator, const V& value) { accumulator.append(value); },
        between
    );
}

template<typename C, typename E, typename B> class StringTypeAdapter<Interleave<C, E, B>> {
public:
    StringTypeAdapter(const Interleave<C, E, B>& interleave)
        : m_interleave { interleave }
    {
    }

    template<typename Accumulator>
    void writeUsing(Accumulator& accumulator) const
    {
        m_interleave.writeUsing(accumulator);
    }

private:
    const Interleave<C, E, B>& m_interleave;
};

template<typename... StringTypeAdapters> inline bool are8Bit(StringTypeAdapters&& ...adapters)
{
    return (... && adapters.is8Bit());
}

template<typename ResultType, typename StringTypeAdapters>
inline void stringTypeAdapterAccumulator(std::span<ResultType> result, StringTypeAdapters adapter)
{
    adapter.writeTo(result);
}

template<typename ResultType, typename StringTypeAdapter, typename... StringTypeAdapters>
inline void stringTypeAdapterAccumulator(std::span<ResultType> result, StringTypeAdapter adapter, StringTypeAdapters ...adapters)
{
    adapter.writeTo(result);
    stringTypeAdapterAccumulator(result.subspan(adapter.length()), adapters...);
}

template<typename Func, StringTypeAdaptable... StringTypes>
auto handleWithAdapters(Func&& func, StringTypes&& ...strings) -> decltype(auto)
{
    return func(StringTypeAdapter<StringTypes>(std::forward<StringTypes>(strings))...);
}

} // namespace WTF

using WTF::Indentation;
using WTF::IndentationScope;
using WTF::asASCIILowercase;
using WTF::asASCIIUppercase;
using WTF::interleave;
using WTF::pad;
