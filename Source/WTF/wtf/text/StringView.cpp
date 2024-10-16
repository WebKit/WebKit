/*

Copyright (C) 2014-2019 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "config.h"
#include <wtf/text/StringView.h>

#include <unicode/ubrk.h>
#include <unicode/unorm2.h>
#include <wtf/ASCIICType.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/text/AdaptiveStringSearcher.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace WTF {

bool StringView::containsIgnoringASCIICase(StringView matchString) const
{
    return findIgnoringASCIICase(matchString) != notFound;
}

bool StringView::containsIgnoringASCIICase(StringView matchString, unsigned startOffset) const
{
    return findIgnoringASCIICase(matchString, startOffset) != notFound;
}

size_t StringView::findIgnoringASCIICase(StringView matchString) const
{
    return ::WTF::findIgnoringASCIICase(*this, matchString, 0);
}

size_t StringView::findIgnoringASCIICase(StringView matchString, unsigned startOffset) const
{
    return ::WTF::findIgnoringASCIICase(*this, matchString, startOffset);
}

bool StringView::startsWith(UChar character) const
{
    return m_length && (*this)[0] == character;
}

bool StringView::startsWith(StringView prefix) const
{
    return ::WTF::startsWith(*this, prefix);
}

bool StringView::startsWithIgnoringASCIICase(StringView prefix) const
{
    return ::WTF::startsWithIgnoringASCIICase(*this, prefix);
}

bool StringView::endsWith(UChar character) const
{
    return m_length && (*this)[m_length - 1] == character;
}

bool StringView::endsWith(StringView suffix) const
{
    return ::WTF::endsWith(*this, suffix);
}

bool StringView::endsWithIgnoringASCIICase(StringView suffix) const
{
    return ::WTF::endsWithIgnoringASCIICase(*this, suffix);
}

Expected<CString, UTF8ConversionError> StringView::tryGetUTF8(ConversionMode mode) const
{
    if (isNull())
        return CString { ""_span };
    if (is8Bit())
        return StringImpl::utf8ForCharacters(span8());
    return StringImpl::utf8ForCharacters(span16(), mode);
}

CString StringView::utf8(ConversionMode mode) const
{
    auto expectedString = tryGetUTF8(mode);
    RELEASE_ASSERT(expectedString);
    return expectedString.value();
}

size_t StringView::find(StringView matchString, unsigned start) const
{
    return findCommon(*this, matchString, start);
}

size_t StringView::find(AdaptiveStringSearcherTables& tables, StringView matchString, unsigned start) const
{
    unsigned subjectLength = length();
    unsigned matchLength = matchString.length();

    if (start > subjectLength)
        return notFound;

    if (!matchLength)
        return start;

    if (UNLIKELY(subjectLength > INT32_MAX || matchLength > INT32_MAX))
        return find(matchString, start);

    if (is8Bit()) {
        if (matchString.is8Bit())
            return searchString(tables, span8(), matchString.span8(), start);
        return searchString(tables, span8(), matchString.span16(), start);
    }

    if (matchString.is8Bit())
        return searchString(tables, span16(), matchString.span8(), start);
    return searchString(tables, span16(), matchString.span16(), start);
}

size_t StringView::find(std::span<const LChar> match, unsigned start) const
{
    ASSERT(!match.empty());
    auto length = this->length();
    if (start > length)
        return notFound;

    unsigned searchLength = length - start;
    if (match.size() > searchLength)
        return notFound;

    if (is8Bit())
        return findInner(span8().subspan(start), match, start);
    return findInner(span16().subspan(start), match, start);
}

size_t StringView::reverseFind(std::span<const LChar> match, unsigned start) const
{
    ASSERT(!match.empty());
    if (match.size() > length())
        return notFound;

    if (is8Bit())
        return reverseFindInner(span8(), match, start);
    return reverseFindInner(span16(), match, start);
}

void StringView::SplitResult::Iterator::findNextSubstring()
{
    for (size_t separatorPosition; (separatorPosition = m_result.m_string.find(m_result.m_separator, m_position)) != notFound; ++m_position) {
        if (m_result.m_allowEmptyEntries || separatorPosition > m_position) {
            m_length = separatorPosition - m_position;
            return;
        }
    }
    m_length = m_result.m_string.length() - m_position;
    if (!m_length && !m_result.m_allowEmptyEntries)
        m_isDone = true;
}

auto StringView::SplitResult::Iterator::operator++() -> Iterator&
{
    ASSERT(m_position <= m_result.m_string.length());
    ASSERT(!m_isDone);
    m_position += m_length;
    if (m_position < m_result.m_string.length()) {
        ++m_position;
        findNextSubstring();
    } else if (!m_isDone)
        m_isDone = true;
    return *this;
}

class StringView::GraphemeClusters::Iterator::Impl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Impl(StringView stringView, std::optional<NonSharedCharacterBreakIterator>&& iterator, unsigned index)
        : m_stringView(stringView)
        , m_iterator(WTFMove(iterator))
        , m_index(index)
        , m_indexEnd(computeIndexEnd())
    {
    }

    void operator++()
    {
        ASSERT(m_indexEnd > m_index);
        m_index = m_indexEnd;
        m_indexEnd = computeIndexEnd();
    }

    StringView operator*() const
    {
        return m_stringView.substring(m_index, m_indexEnd - m_index);
    }

    bool operator==(const Impl& other) const
    {
        ASSERT(m_stringView.m_characters == other.m_stringView.m_characters);
        ASSERT(m_stringView.m_length == other.m_stringView.m_length);
        auto result = m_index == other.m_index;
        ASSERT(!result || m_indexEnd == other.m_indexEnd);
        return result;
    }

    unsigned computeIndexEnd()
    {
        if (!m_iterator)
            return 0;
        if (m_index == m_stringView.length())
            return m_index;
        return ubrk_following(m_iterator.value(), m_index);
    }

private:
    StringView m_stringView;
    std::optional<NonSharedCharacterBreakIterator> m_iterator;
    unsigned m_index;
    unsigned m_indexEnd;
};

StringView::GraphemeClusters::Iterator::Iterator(StringView stringView, unsigned index)
    : m_impl(makeUnique<Impl>(stringView, stringView.isNull() ? std::nullopt : std::optional<NonSharedCharacterBreakIterator>(NonSharedCharacterBreakIterator(stringView)), index))
{
}

StringView::GraphemeClusters::Iterator::~Iterator() = default;

StringView::GraphemeClusters::Iterator::Iterator(Iterator&& other)
    : m_impl(WTFMove(other.m_impl))
{
}

auto StringView::GraphemeClusters::Iterator::operator++() -> Iterator&
{
    ++(*m_impl);
    return *this;
}

StringView StringView::GraphemeClusters::Iterator::operator*() const
{
    return **m_impl;
}

bool StringView::GraphemeClusters::Iterator::operator==(const Iterator& other) const
{
    return *m_impl == *(other.m_impl);
}

enum class ASCIICase { Lower, Upper };

template<ASCIICase type, typename CharacterType>
String convertASCIICase(std::span<const CharacterType> input)
{
    if (!input)
        return { };

    CharacterType* characters;
    auto result = String::createUninitialized(input.size(), characters);
    for (auto character : input)
        *characters++ = type == ASCIICase::Lower ? toASCIILower(character) : toASCIIUpper(character);
    return result;
}

String StringView::convertToASCIILowercase() const
{
    if (m_is8Bit)
        return convertASCIICase<ASCIICase::Lower>(span8());
    return convertASCIICase<ASCIICase::Lower>(span16());
}

String StringView::convertToASCIIUppercase() const
{
    if (m_is8Bit)
        return convertASCIICase<ASCIICase::Upper>(span8());
    return convertASCIICase<ASCIICase::Upper>(span16());
}

template<typename CharacterType>
static AtomString convertASCIILowercaseAtom(std::span<const CharacterType> input)
{
    for (auto character : input) {
        if (UNLIKELY(isASCIIUpper(character)))
            return makeAtomString(asASCIILowercase(input));
    }
    // Fast path when the StringView is already all lowercase.
    return input;
}

AtomString StringView::convertToASCIILowercaseAtom() const
{
    if (m_is8Bit)
        return convertASCIILowercaseAtom(span8());
    return convertASCIILowercaseAtom(span16());
}

template<typename DestinationCharacterType, typename SourceCharacterType>
void getCharactersWithASCIICaseInternal(StringView::CaseConvertType type, DestinationCharacterType* destination, std::span<const SourceCharacterType> source)
{
    static_assert(std::is_same<SourceCharacterType, LChar>::value || std::is_same<SourceCharacterType, UChar>::value);
    static_assert(std::is_same<DestinationCharacterType, LChar>::value || std::is_same<DestinationCharacterType, UChar>::value);
    static_assert(sizeof(DestinationCharacterType) >= sizeof(SourceCharacterType));
    auto caseConvert = (type == StringView::CaseConvertType::Lower) ? toASCIILower<SourceCharacterType> : toASCIIUpper<SourceCharacterType>;
    for (auto character : source)
        *destination++ = caseConvert(character);
}

void StringView::getCharactersWithASCIICase(CaseConvertType type, LChar* destination) const
{
    ASSERT(is8Bit());
    getCharactersWithASCIICaseInternal(type, destination, span8());
}

void StringView::getCharactersWithASCIICase(CaseConvertType type, UChar* destination) const
{
    if (is8Bit()) {
        getCharactersWithASCIICaseInternal(type, destination, span8());
        return;
    }
    getCharactersWithASCIICaseInternal(type, destination, span16());
}

StringViewWithUnderlyingString normalizedNFC(StringView string)
{
    // Latin-1 characters are unaffected by normalization.
    if (string.is8Bit())
        return { string, { } };

    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer = unorm2_getNFCInstance(&status);
    ASSERT(U_SUCCESS(status));

    // No need to normalize if already normalized.
    auto span = string.span16();
    UBool checkResult = unorm2_isNormalized(normalizer, span.data(), span.size(), &status);
    if (checkResult)
        return { string, { } };

    unsigned normalizedLength = unorm2_normalize(normalizer, span.data(), span.size(), nullptr, 0, &status);
    ASSERT(needsToGrowToProduceBuffer(status));

    UChar* characters;
    String result = String::createUninitialized(normalizedLength, characters);

    status = U_ZERO_ERROR;
    unorm2_normalize(normalizer, span.data(), span.size(), characters, normalizedLength, &status);
    ASSERT(U_SUCCESS(status));

    StringView view { result };
    return { view, WTFMove(result) };
}

String normalizedNFC(const String& string)
{
    auto result = normalizedNFC(StringView { string });
    if (result.underlyingString.isNull())
        return string;
    return result.underlyingString;
}

bool equalRespectingNullity(StringView a, StringView b)
{
    if (a.m_characters == b.m_characters) {
        ASSERT(a.is8Bit() == b.is8Bit());
        return a.length() == b.length();
    }

    if (a.isEmpty() && b.isEmpty())
        return a.isNull() == b.isNull();

    return equalCommon(a, b);
}

size_t StringView::reverseFind(StringView matchString, unsigned start) const
{
    if (isNull())
        return notFound;

    if (matchString.length())
        return std::min(start, length());

    // Check start & matchLength are in range.
    if (matchString.length() > length())
        return notFound;

    if (is8Bit()) {
        if (matchString.is8Bit())
            return reverseFindInner(span8(), matchString.span8(), start);
        return reverseFindInner(span8(), matchString.span16(), start);
    }

    if (matchString.is8Bit())
        return reverseFindInner(span16(), matchString.span8(), start);
    return reverseFindInner(span16(), matchString.span16(), start);
}

String makeStringByReplacingAll(StringView string, UChar target, UChar replacement)
{
    if (string.is8Bit()) {
        if (!isLatin1(target)) {
            // Looking for a 16-bit character in an 8-bit string, so we're done.
            return string.toString();
        }

        auto characters = string.span8();
        size_t i;
        unsigned length = string.length();
        for (i = 0; i != characters.size(); ++i) {
            if (characters[i] == target)
                break;
        }
        if (i == length)
            return string.toString();
        return StringImpl::createByReplacingInCharacters(characters, target, replacement, i);
    }

    auto characters = string.span16();
    size_t i;
    unsigned length = string.length();
    for (i = 0; i != characters.size(); ++i) {
        if (characters[i] == target)
            break;
    }
    if (i == length)
        return string.toString();
    return StringImpl::createByReplacingInCharacters(characters, target, replacement, i);
}

int codePointCompare(StringView lhs, StringView rhs)
{
    bool lhsIs8Bit = lhs.is8Bit();
    bool rhsIs8Bit = rhs.is8Bit();
    if (lhsIs8Bit) {
        if (rhsIs8Bit)
            return codePointCompare(lhs.span8(), rhs.span8());
        return codePointCompare(lhs.span8(), rhs.span16());
    }
    if (rhsIs8Bit)
        return codePointCompare(lhs.span16(), rhs.span8());
    return codePointCompare(lhs.span16(), rhs.span16());
}

template<typename CharacterType> static String makeStringBySimplifyingNewLinesSlowCase(const String& string, unsigned firstCarriageReturn)
{
    unsigned length = string.length();
    unsigned resultLength = firstCarriageReturn;
    auto characters = string.span<CharacterType>();
    CharacterType* resultCharacters;
    auto result = String::createUninitialized(length, resultCharacters);
    memcpy(resultCharacters, characters.data(), firstCarriageReturn * sizeof(CharacterType));
    for (unsigned i = firstCarriageReturn; i < length; ++i) {
        if (characters[i] != '\r')
            resultCharacters[resultLength++] = characters[i];
        else {
            resultCharacters[resultLength++] = '\n';
            if (i + 1 < length && characters[i + 1] == '\n')
                ++i;
        }
    }
    if (resultLength < length)
        result = StringImpl::createSubstringSharingImpl(*result.impl(), 0, resultLength);
    return result;
}

String makeStringBySimplifyingNewLinesSlowCase(const String& string, unsigned firstCarriageReturn)
{
    if (string.is8Bit())
        return makeStringBySimplifyingNewLinesSlowCase<LChar>(string, firstCarriageReturn);
    return makeStringBySimplifyingNewLinesSlowCase<UChar>(string, firstCarriageReturn);
}

#if CHECK_STRINGVIEW_LIFETIME

// Manage reference count manually so UnderlyingString does not need to be defined in the header.

struct StringView::UnderlyingString {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    std::atomic_uint refCount { 1u };
    bool isValid { true };
    SUPPRESS_UNCOUNTED_MEMBER const StringImpl& string;
    explicit UnderlyingString(const StringImpl&);
};

StringView::UnderlyingString::UnderlyingString(const StringImpl& string)
    : string(string)
{
}

static Lock underlyingStringsLock;

static UncheckedKeyHashMap<const StringImpl*, StringView::UnderlyingString*>& underlyingStrings() WTF_REQUIRES_LOCK(underlyingStringsLock)
{
    static NeverDestroyed<UncheckedKeyHashMap<const StringImpl*, StringView::UnderlyingString*>> map;
    return map;
}

void StringView::invalidate(const StringImpl& stringToBeDestroyed)
{
    UnderlyingString* underlyingString;
    {
        Locker locker { underlyingStringsLock };
        underlyingString = underlyingStrings().take(&stringToBeDestroyed);
        if (!underlyingString)
            return;
    }
    ASSERT(underlyingString->isValid);
    underlyingString->isValid = false;
}

bool StringView::underlyingStringIsValidImpl() const
{
    return !m_underlyingString || m_underlyingString->isValid;
}

void StringView::adoptUnderlyingString(UnderlyingString* underlyingString)
{
    if (m_underlyingString) {
        Locker locker { underlyingStringsLock };
        if (!--m_underlyingString->refCount) {
            if (m_underlyingString->isValid) {
                underlyingStrings().remove(&m_underlyingString->string);
            }
            delete m_underlyingString;
        }
    }
    m_underlyingString = underlyingString;
}

void StringView::setUnderlyingStringImpl(const StringImpl* string)
{
    UnderlyingString* underlyingString;
    if (!string)
        underlyingString = nullptr;
    else {
        Locker locker { underlyingStringsLock };
        auto result = underlyingStrings().add(string, nullptr);
        if (result.isNewEntry)
            result.iterator->value = new UnderlyingString(*string);
        else
            ++result.iterator->value->refCount;
        underlyingString = result.iterator->value;
    }
    adoptUnderlyingString(underlyingString);
}

void StringView::setUnderlyingStringImpl(const StringView& otherString)
{
    UnderlyingString* underlyingString = otherString.m_underlyingString;
    if (underlyingString) {
        // It's safe to inc the refCount here without locking underlyingStringsLock
        // because UnderlyingString::refCount is a std::atomic_uint, and we're
        // guaranteed that the StringView we're copying it from will at least
        // have 1 ref on it, thereby keeping it alive regardless of what other
        // threads may be doing.
        ++underlyingString->refCount;
    }
    adoptUnderlyingString(underlyingString);
}

#else

bool StringView::underlyingStringIsValidImpl() const
{
    return true;
}

void StringView::setUnderlyingStringImpl(const StringImpl*)
{
}

void StringView::setUnderlyingStringImpl(const StringView&)
{
}

#endif // not CHECK_STRINGVIEW_LIFETIME

#ifndef NDEBUG
void StringView::show() const
{
    toStringWithoutCopying().show();
}
#endif

#if !defined(NDEBUG)
namespace Detail {
std::atomic<int> wtfStringCopyCount;
}
#endif

} // namespace WTF
