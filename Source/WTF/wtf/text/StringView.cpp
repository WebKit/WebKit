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
#include <wtf/Optional.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace WTF {

bool StringView::containsIgnoringASCIICase(const StringView& matchString) const
{
    return findIgnoringASCIICase(matchString) != notFound;
}

bool StringView::containsIgnoringASCIICase(const StringView& matchString, unsigned startOffset) const
{
    return findIgnoringASCIICase(matchString, startOffset) != notFound;
}

size_t StringView::findIgnoringASCIICase(const StringView& matchString) const
{
    return ::WTF::findIgnoringASCIICase(*this, matchString, 0);
}

size_t StringView::findIgnoringASCIICase(const StringView& matchString, unsigned startOffset) const
{
    return ::WTF::findIgnoringASCIICase(*this, matchString, startOffset);
}

bool StringView::startsWith(UChar character) const
{
    return m_length && (*this)[0] == character;
}

bool StringView::startsWith(const StringView& prefix) const
{
    return ::WTF::startsWith(*this, prefix);
}

bool StringView::startsWithIgnoringASCIICase(const StringView& prefix) const
{
    return ::WTF::startsWithIgnoringASCIICase(*this, prefix);
}

bool StringView::endsWith(UChar character) const
{
    return m_length && (*this)[m_length - 1] == character;
}

bool StringView::endsWith(const StringView& suffix) const
{
    return ::WTF::endsWith(*this, suffix);
}

bool StringView::endsWithIgnoringASCIICase(const StringView& suffix) const
{
    return ::WTF::endsWithIgnoringASCIICase(*this, suffix);
}

Expected<CString, UTF8ConversionError> StringView::tryGetUtf8(ConversionMode mode) const
{
    if (isNull())
        return CString("", 0);
    if (is8Bit())
        return StringImpl::utf8ForCharacters(characters8(), length());
    return StringImpl::utf8ForCharacters(characters16(), length(), mode);
}

CString StringView::utf8(ConversionMode mode) const
{
    auto expectedString = tryGetUtf8(mode);
    RELEASE_ASSERT(expectedString);
    return expectedString.value();
}

size_t StringView::find(StringView matchString, unsigned start) const
{
    return findCommon(*this, matchString, start);
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
    ASSERT(m_position <= m_result.m_string.length() && !m_isDone);
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
    Impl(const StringView& stringView, Optional<NonSharedCharacterBreakIterator>&& iterator, unsigned index)
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
        if (m_stringView.is8Bit())
            return StringView(m_stringView.characters8() + m_index, m_indexEnd - m_index);
        return StringView(m_stringView.characters16() + m_index, m_indexEnd - m_index);
    }

    bool operator==(const Impl& other) const
    {
        ASSERT(&m_stringView == &other.m_stringView);
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
    const StringView& m_stringView;
    Optional<NonSharedCharacterBreakIterator> m_iterator;
    unsigned m_index;
    unsigned m_indexEnd;
};

StringView::GraphemeClusters::Iterator::Iterator(const StringView& stringView, unsigned index)
    : m_impl(makeUnique<Impl>(stringView, stringView.isNull() ? WTF::nullopt : Optional<NonSharedCharacterBreakIterator>(NonSharedCharacterBreakIterator(stringView)), index))
{
}

StringView::GraphemeClusters::Iterator::~Iterator()
{
}

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

bool StringView::GraphemeClusters::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

enum class ASCIICase { Lower, Upper };

template<ASCIICase type, typename CharacterType>
String convertASCIICase(const CharacterType* input, unsigned length)
{
    if (!input)
        return { };

    CharacterType* characters;
    auto result = String::createUninitialized(length, characters);
    for (unsigned i = 0; i < length; ++i)
        characters[i] = type == ASCIICase::Lower ? toASCIILower(input[i]) : toASCIIUpper(input[i]);
    return result;
}

String StringView::convertToASCIILowercase() const
{
    if (m_is8Bit)
        return convertASCIICase<ASCIICase::Lower>(static_cast<const LChar*>(m_characters), m_length);
    return convertASCIICase<ASCIICase::Lower>(static_cast<const UChar*>(m_characters), m_length);
}

String StringView::convertToASCIIUppercase() const
{
    if (m_is8Bit)
        return convertASCIICase<ASCIICase::Upper>(static_cast<const LChar*>(m_characters), m_length);
    return convertASCIICase<ASCIICase::Upper>(static_cast<const UChar*>(m_characters), m_length);
}

template<typename DestinationCharacterType, typename SourceCharacterType>
void getCharactersWithASCIICaseInternal(StringView::CaseConvertType type, DestinationCharacterType* destination, const SourceCharacterType* source, unsigned length)
{
    static_assert(std::is_same<SourceCharacterType, LChar>::value || std::is_same<SourceCharacterType, UChar>::value);
    static_assert(std::is_same<DestinationCharacterType, LChar>::value || std::is_same<DestinationCharacterType, UChar>::value);
    static_assert(sizeof(DestinationCharacterType) >= sizeof(SourceCharacterType));
    auto caseConvert = (type == StringView::CaseConvertType::Lower) ? toASCIILower<SourceCharacterType> : toASCIIUpper<SourceCharacterType>;
    for (unsigned i = 0; i < length; ++i)
        destination[i] = caseConvert(source[i]);
}

void StringView::getCharactersWithASCIICase(CaseConvertType type, LChar* destination) const
{
    ASSERT(is8Bit());
    getCharactersWithASCIICaseInternal(type, destination, characters8(), m_length);
}

void StringView::getCharactersWithASCIICase(CaseConvertType type, UChar* destination) const
{
    if (is8Bit()) {
        getCharactersWithASCIICaseInternal(type, destination, characters8(), m_length);
        return;
    }
    getCharactersWithASCIICaseInternal(type, destination, characters16(), m_length);
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
    UBool checkResult = unorm2_isNormalized(normalizer, string.characters16(), string.length(), &status);
    if (checkResult)
        return { string, { } };

    unsigned normalizedLength = unorm2_normalize(normalizer, string.characters16(), string.length(), nullptr, 0, &status);
    ASSERT(needsToGrowToProduceBuffer(status));

    UChar* characters;
    String result = String::createUninitialized(normalizedLength, characters);

    status = U_ZERO_ERROR;
    unorm2_normalize(normalizer, string.characters16(), string.length(), characters, normalizedLength, &status);
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

// FIXME: Should this be named parseNumber<uint16_t> instead?
// FIXME: Should we replace the toInt family of functions with this style?
Optional<uint16_t> parseUInt16(StringView string)
{
    bool ok = false;
    auto number = toIntegralType<uint16_t>(string, &ok);
    if (!ok)
        return WTF::nullopt;
    return number;
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

bool StringView::contains(const char* string) const
{
    return find(string) != notFound;
}

int codePointCompare(StringView lhs, StringView rhs)
{
    bool lhsIs8Bit = lhs.is8Bit();
    bool rhsIs8Bit = rhs.is8Bit();
    if (lhsIs8Bit) {
        if (rhsIs8Bit)
            return codePointCompare(lhs.characters8(), lhs.length(), rhs.characters8(), rhs.length());
        return codePointCompare(lhs.characters8(), lhs.length(), rhs.characters16(), rhs.length());
    }
    if (rhsIs8Bit)
        return codePointCompare(lhs.characters16(), lhs.length(), rhs.characters8(), rhs.length());
    return codePointCompare(lhs.characters16(), lhs.length(), rhs.characters16(), rhs.length());
}

#if CHECK_STRINGVIEW_LIFETIME

// Manage reference count manually so UnderlyingString does not need to be defined in the header.

struct StringView::UnderlyingString {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    std::atomic_uint refCount { 1u };
    bool isValid { true };
    const StringImpl& string;
    explicit UnderlyingString(const StringImpl&);
};

StringView::UnderlyingString::UnderlyingString(const StringImpl& string)
    : string(string)
{
}

static Lock underlyingStringsMutex;

static HashMap<const StringImpl*, StringView::UnderlyingString*>& underlyingStrings()
{
    static LazyNeverDestroyed<HashMap<const StringImpl*, StringView::UnderlyingString*>> map;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        map.construct();
    });
    return map;
}

void StringView::invalidate(const StringImpl& stringToBeDestroyed)
{
    UnderlyingString* underlyingString;
    {
        auto locker = holdLock(underlyingStringsMutex);
        underlyingString = underlyingStrings().take(&stringToBeDestroyed);
        if (!underlyingString)
            return;
    }
    ASSERT(underlyingString->isValid);
    underlyingString->isValid = false;
}

bool StringView::underlyingStringIsValid() const
{
    return !m_underlyingString || m_underlyingString->isValid;
}

void StringView::adoptUnderlyingString(UnderlyingString* underlyingString)
{
    if (m_underlyingString) {
        auto locker = holdLock(underlyingStringsMutex);
        if (!--m_underlyingString->refCount) {
            if (m_underlyingString->isValid) {
                underlyingStrings().remove(&m_underlyingString->string);
            }
            delete m_underlyingString;
        }
    }
    m_underlyingString = underlyingString;
}

void StringView::setUnderlyingString(const StringImpl* string)
{
    UnderlyingString* underlyingString;
    if (!string)
        underlyingString = nullptr;
    else {
        auto locker = holdLock(underlyingStringsMutex);
        auto result = underlyingStrings().add(string, nullptr);
        if (result.isNewEntry)
            result.iterator->value = new UnderlyingString(*string);
        else
            ++result.iterator->value->refCount;
        underlyingString = result.iterator->value;
    }
    adoptUnderlyingString(underlyingString);
}

void StringView::setUnderlyingString(const StringView& otherString)
{
    UnderlyingString* underlyingString = otherString.m_underlyingString;
    if (underlyingString) {
        // It's safe to inc the refCount here without locking underlyingStringsMutex
        // because UnderlyingString::refCount is a std::atomic_uint, and we're
        // guaranteed that the StringView we're copying it from will at least
        // have 1 ref on it, thereby keeping it alive regardless of what other
        // threads may be doing.
        ++underlyingString->refCount;
    }
    adoptUnderlyingString(underlyingString);
}

#endif // CHECK_STRINGVIEW_LIFETIME

} // namespace WTF
