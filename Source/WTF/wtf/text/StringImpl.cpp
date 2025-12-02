/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#include "config.h"
#include <wtf/text/StringImpl.h>

#include <atomic>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ZippedRange.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/CString.h>
#include <wtf/text/ExternalStringImpl.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringView.h>
#include <wtf/text/SymbolImpl.h>
#include <wtf/text/SymbolRegistry.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/UTF8Conversion.h>

#if STRING_STATS
#include <unistd.h>
#include <wtf/DataLog.h>
#endif

namespace WTF {

using namespace Unicode;

#if HAVE(36BIT_ADDRESS)
#define STRING_IMPL_SIZE(n) roundUpToMultipleOfImpl(16, n)
#else
#define STRING_IMPL_SIZE(n) n
#endif

static_assert(sizeof(StringImpl) == STRING_IMPL_SIZE(2 * sizeof(int) + 2 * sizeof(void*)), "StringImpl should stay small");

#if STRING_STATS
StringStats StringImpl::m_stringStats;

std::atomic<unsigned> StringStats::s_stringRemovesTillPrintStats(s_printStringStatsFrequency);

void StringStats::removeString(StringImpl& string)
{
    unsigned length = string.length();
    bool isSubString = string.isSubString();

    --m_totalNumberStrings;

    if (string.is8Bit()) {
        --m_number8BitStrings;
        if (!isSubString)
            m_total8BitData -= length;
    } else {
        --m_number16BitStrings;
        if (!isSubString)
            m_total16BitData -= length;
    }

    if (!--s_stringRemovesTillPrintStats) {
        s_stringRemovesTillPrintStats = s_printStringStatsFrequency;
        printStats();
    }
}

void StringStats::printStats()
{
    dataLogF("String stats for process id %d:\n", getCurrentProcessID());

    unsigned long long totalNumberCharacters = m_total8BitData + m_total16BitData;
    double percent8Bit = m_totalNumberStrings ? ((double)m_number8BitStrings * 100) / (double)m_totalNumberStrings : 0.0;
    double average8bitLength = m_number8BitStrings ? (double)m_total8BitData / (double)m_number8BitStrings : 0.0;
    dataLogF("%8u (%5.2f%%) 8 bit        %12llu chars  %12llu bytes  avg length %6.1f\n", m_number8BitStrings.load(), percent8Bit, m_total8BitData.load(), m_total8BitData.load(), average8bitLength);

    double percent16Bit = m_totalNumberStrings ? ((double)m_number16BitStrings * 100) / (double)m_totalNumberStrings : 0.0;
    double average16bitLength = m_number16BitStrings ? (double)m_total16BitData / (double)m_number16BitStrings : 0.0;
    dataLogF("%8u (%5.2f%%) 16 bit       %12llu chars  %12llu bytes  avg length %6.1f\n", m_number16BitStrings.load(), percent16Bit, m_total16BitData.load(), m_total16BitData * 2, average16bitLength);

    double averageLength = m_totalNumberStrings ? (double)totalNumberCharacters / (double)m_totalNumberStrings : 0.0;
    unsigned long long totalDataBytes = m_total8BitData + m_total16BitData * 2;
    dataLogF("%8u Total                 %12llu chars  %12llu bytes  avg length %6.1f\n", m_totalNumberStrings.load(), totalNumberCharacters, totalDataBytes, averageLength);
    unsigned long long totalSavedBytes = m_total8BitData;
    double percentSavings = totalSavedBytes ? ((double)totalSavedBytes * 100) / (double)(totalDataBytes + totalSavedBytes) : 0.0;
    dataLogF("         Total savings %12llu bytes (%5.2f%%)\n", totalSavedBytes, percentSavings);

    dataLogF("%8u StringImpl::ref calls\n", m_refCalls.load());
    dataLogF("%8u StringImpl::deref calls\n", m_derefCalls.load());
}
#endif

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringImpl);

StringImpl::StaticStringImpl StringImpl::s_emptyAtomString("", StringImpl::StringAtom);

StringImpl::~StringImpl()
{
    ASSERT(!isStatic());

    StringView::invalidate(*this);

    STRING_STATS_REMOVE_STRING(*this);

    if (isAtom()) {
        ASSERT(!isSymbol());
        if (length())
            AtomStringImpl::remove(static_cast<AtomStringImpl*>(this));
    } else if (isSymbol()) {
        auto& symbol = static_cast<SymbolImpl&>(*this);
        if (CheckedPtr symbolRegistry = symbol.symbolRegistry())
            SUPPRESS_UNCOUNTED_ARG symbolRegistry->remove(*symbol.asRegisteredSymbolImpl());
    }

    switch (bufferOwnership()) {
    case BufferInternal:
        break;
    case BufferOwned:
        // We use m_data8, but since it is a union with m_data16 this works either way.
        ASSERT(m_data8);
        StringImplMalloc::free(const_cast<Latin1Character*>(m_data8));
        break;
    case BufferExternal: {
        auto* external = static_cast<ExternalStringImpl*>(this);
        external->freeExternalBuffer(const_cast<Latin1Character*>(m_data8), sizeInBytes());
        external->m_free.~ExternalStringImplFreeFunction();
        break;
    }
    case BufferSubstring:
        ASSERT(substringBuffer());
        substringBuffer()->deref();
        break;
    }
}

void StringImpl::destroy(StringImpl* stringImpl)
{
    stringImpl->~StringImpl();
    StringImplMalloc::free(stringImpl);
}

Ref<StringImpl> StringImpl::createWithoutCopyingNonEmpty(std::span<const char16_t> characters)
{
    ASSERT(!characters.empty());
    return adoptRef(*new StringImpl(characters, ConstructWithoutCopying));
}

Ref<StringImpl> StringImpl::createWithoutCopyingNonEmpty(std::span<const Latin1Character> characters)
{
    ASSERT(!characters.empty());
    return adoptRef(*new StringImpl(characters, ConstructWithoutCopying));
}

template<typename CharacterType> inline Ref<StringImpl> StringImpl::createUninitializedInternal(size_t length, std::span<CharacterType>& data)
{
    if (!length) {
        data = { };
        return *empty();
    }

    return createUninitializedInternalNonEmpty(length, data);
}

template<typename CharacterType> inline Ref<StringImpl> StringImpl::createUninitializedInternalNonEmpty(size_t length, std::span<CharacterType>& data)
{
    ASSERT(length);

    // Allocate a single buffer large enough to contain the StringImpl
    // struct as well as the data which it contains. This removes one
    // heap allocation from this call.
    if (!isValidLength<CharacterType>(length))
        CRASH();

    SUPPRESS_UNCOUNTED_LOCAL StringImpl* string = static_cast<StringImpl*>(StringImplMalloc::malloc(allocationSize<CharacterType>(length)));
    data = unsafeMakeSpan(string->tailPointer<CharacterType>(), length);
    return constructInternal<CharacterType>(*string, length);
}

template Ref<StringImpl> StringImpl::createUninitializedInternalNonEmpty(size_t length, std::span<Latin1Character>& data);
template Ref<StringImpl> StringImpl::createUninitializedInternalNonEmpty(size_t length, std::span<char16_t>& data);

Ref<StringImpl> StringImpl::createUninitialized(size_t length, std::span<Latin1Character>& data)
{
    return createUninitializedInternal(length, data);
}

Ref<StringImpl> StringImpl::createUninitialized(size_t length, std::span<char16_t>& data)
{
    return createUninitializedInternal(length, data);
}

template<typename CharacterType> inline Expected<Ref<StringImpl>, UTF8ConversionError> StringImpl::reallocateInternal(Ref<StringImpl>&& originalString, unsigned length, CharacterType*& data)
{
    ASSERT(originalString->hasOneRef());
    ASSERT(originalString->bufferOwnership() == BufferInternal);

    if (!length) {
        data = nullptr;
        return Ref<StringImpl>(*empty());
    }

    // Same as createUninitialized() except here we use fastRealloc.
    if (!isValidLength<CharacterType>(length))
        return makeUnexpected(UTF8ConversionError::OutOfMemory);

    originalString->~StringImpl();
    SUPPRESS_UNCOUNTED_LOCAL auto* string = static_cast<StringImpl*>(StringImplMalloc::tryRealloc(&originalString.leakRef(), allocationSize<CharacterType>(length)));
    if (!string)
        return makeUnexpected(UTF8ConversionError::OutOfMemory);

    data = string->tailPointer<CharacterType>();
    return constructInternal<CharacterType>(*string, length);
}

Ref<StringImpl> StringImpl::reallocate(Ref<StringImpl>&& originalString, unsigned length, Latin1Character*& data)
{
    auto expectedStringImpl = tryReallocate(WTFMove(originalString), length, data);
    RELEASE_ASSERT(expectedStringImpl);
    return WTFMove(expectedStringImpl.value());
}

Ref<StringImpl> StringImpl::reallocate(Ref<StringImpl>&& originalString, unsigned length, char16_t*& data)
{
    auto expectedStringImpl = tryReallocate(WTFMove(originalString), length, data);
    RELEASE_ASSERT(expectedStringImpl);
    return WTFMove(expectedStringImpl.value());
}

Expected<Ref<StringImpl>, UTF8ConversionError> StringImpl::tryReallocate(Ref<StringImpl>&& originalString, unsigned length, Latin1Character*& data)
{
    ASSERT(originalString->is8Bit());
    return reallocateInternal(WTFMove(originalString), length, data);
}

Expected<Ref<StringImpl>, UTF8ConversionError> StringImpl::tryReallocate(Ref<StringImpl>&& originalString, unsigned length, char16_t*& data)
{
    ASSERT(!originalString->is8Bit());
    return reallocateInternal(WTFMove(originalString), length, data);
}

template<typename CharacterType> inline Ref<StringImpl> StringImpl::createInternal(std::span<const CharacterType> characters)
{
    if (characters.empty())
        return *empty();
    std::span<CharacterType> data;
    auto string = createUninitializedInternalNonEmpty(characters.size(), data);
    copyCharacters(data, characters);
    return string;
}

Ref<StringImpl> StringImpl::create(std::span<const char16_t> characters)
{
    return createInternal(characters);
}

Ref<StringImpl> StringImpl::create(std::span<const Latin1Character> characters)
{
    return createInternal(characters);
}

RefPtr<StringImpl> StringImpl::create(std::span<const char8_t> codeUnits)
{
    RELEASE_ASSERT(isValidLength<char8_t>(codeUnits.size()));

    if (!codeUnits.data())
        return nullptr;
    if (codeUnits.empty())
        return empty();

    if (charactersAreAllASCII(codeUnits))
        return create(byteCast<Latin1Character>(codeUnits));

    Vector<char16_t, 1024> buffer(codeUnits.size());

    auto result = Unicode::convert(codeUnits, buffer.mutableSpan());
    if (result.code != Unicode::ConversionResultCode::Success)
        return nullptr;

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(result.buffer.size() <= codeUnits.size());
    return create(result.buffer);
}

Ref<StringImpl> StringImpl::createStaticStringImpl(std::span<const Latin1Character> characters)
{
    if (characters.empty())
        return *empty();
    Ref<StringImpl> result = createInternal(characters);
    result->hash();
    result->m_refCount |= s_refCountFlagIsStaticString;
    return result;
}

Ref<StringImpl> StringImpl::createStaticStringImpl(std::span<const char16_t> characters)
{
    if (characters.empty())
        return *empty();
    Ref<StringImpl> result = create8BitIfPossible(characters);
    result->hash();
    result->m_refCount |= s_refCountFlagIsStaticString;
    return result;
}

Ref<StringImpl> StringImpl::create8BitIfPossible(std::span<const char16_t> characters)
{
    if (characters.empty())
        return *empty();

    std::span<Latin1Character> data;
    auto string = createUninitializedInternalNonEmpty(characters.size(), data);

    size_t i = 0;
    for (auto character : characters) {
        if (!isLatin1(character))
            return create(characters);
        data[i++] = static_cast<Latin1Character>(character);
    }

    return string;
}

Ref<StringImpl> StringImpl::create8BitUnconditionally(std::span<const char16_t> characters)
{
    if (characters.empty())
        return *empty();

    std::span<Latin1Character> data;
    auto string = createUninitializedInternalNonEmpty(characters.size(), data);
    copyElements(data, characters);
    return string;
}

Ref<StringImpl> StringImpl::substring(unsigned start, unsigned length)
{
    if (start >= m_length)
        return *empty();
    unsigned maxLength = m_length - start;
    if (length >= maxLength) {
        if (!start)
            return *this;
        length = maxLength;
    }
    if (is8Bit())
        return create(span8().subspan(start, length));

    return create(span16().subspan(start, length));
}

char32_t StringImpl::characterStartingAt(unsigned i)
{
    if (is8Bit())
        return span8()[i];
    auto span = span16();
    if (U16_IS_SINGLE(span[i]))
        return span[i];
    if (i + 1 < m_length && U16_IS_LEAD(span[i]) && U16_IS_TRAIL(span[i + 1]))
        return U16_GET_SUPPLEMENTARY(span[i], span[i + 1]);
    return 0;
}

Ref<StringImpl> StringImpl::convertToLowercaseWithoutLocale()
{
    // Note: At one time this was a hot function in the Dromaeo benchmark, specifically the
    // no-op code path that may return ourself if we find no upper case letters and no invalid
    // ASCII letters.

    // First scan the string for uppercase and non-ASCII characters:
    if (is8Bit()) {
        auto span = span8();
        for (unsigned i = 0; i < span.size(); ++i) {
            Latin1Character character = span[i];
            if (!isASCII(character) || isASCIIUpper(character)) [[unlikely]]
                return convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(i);
        }

        return *this;
    }

    bool noUpper = true;
    unsigned ored = 0;

    auto span = span16();
    for (unsigned i = 0; i < span.size(); ++i) {
        char16_t character = span[i];
        if (isASCIIUpper(character)) [[unlikely]]
            noUpper = false;
        ored |= character;
    }
    // Nothing to do if the string is all ASCII with no uppercase.
    if (noUpper && !(ored & ~0x7F))
        return *this;

    if (!(ored & ~0x7F)) {
        std::span<char16_t> data16;
        auto newImpl = createUninitializedInternalNonEmpty(m_length, data16);
        for (unsigned i = 0; i < span.size(); ++i)
            data16[i] = toASCIILower(span[i]);
        return newImpl;
    }

    int32_t length = m_length;

    // Do a slower implementation for cases that include non-ASCII characters.
    std::span<char16_t> data16;
    auto newImpl = createUninitializedInternalNonEmpty(m_length, data16);

    UErrorCode status = U_ZERO_ERROR;
    int32_t realLength = u_strToLower(data16.data(), length, m_data16, m_length, "", &status);
    if (U_SUCCESS(status) && realLength == length)
        return newImpl;

    newImpl = createUninitialized(realLength, data16);
    status = U_ZERO_ERROR;
    u_strToLower(data16.data(), realLength, m_data16, m_length, "", &status);
    if (U_FAILURE(status))
        return *this;
    return newImpl;
}

Ref<StringImpl> StringImpl::convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned failingIndex)
{
    ASSERT(is8Bit());
    std::span<Latin1Character> data8;
    auto newImpl = createUninitializedInternalNonEmpty(m_length, data8);

    auto span = span8();
    for (unsigned i = 0; i < failingIndex; ++i) {
        ASSERT(isASCII(span[i]));
        ASSERT(!isASCIIUpper(span[i]));
        data8[i] = span[i];
    }

    for (unsigned i = failingIndex; i < span.size(); ++i) {
        Latin1Character character = span[i];
        if (isASCII(character))
            data8[i] = toASCIILower(character);
        else {
            ASSERT(isLatin1(u_tolower(character)));
            data8[i] = static_cast<Latin1Character>(u_tolower(character));
        }
    }

    return newImpl;
}

Ref<StringImpl> StringImpl::convertToUppercaseWithoutLocale()
{
    // This function could be optimized for no-op cases the way
    // convertToLowercaseWithoutLocale() is, but in empirical testing,
    // few actual calls to upper() are no-ops, so it wouldn't be worth
    // the extra time for pre-scanning.

    if (m_length > MaxLength)
        CRASH();

    // First scan the string for uppercase and non-ASCII characters:
    if (is8Bit()) {
        auto span = span8();
        for (unsigned i = 0; i < span.size(); ++i) {
            Latin1Character character = span[i];
            if (!isASCII(character) || isASCIILower(character)) [[unlikely]]
                return convertToUppercaseWithoutLocaleStartingAtFailingIndex8Bit(i);
        }
        return *this;
    }
    return convertToUppercaseWithoutLocaleUpconvert();
}

Ref<StringImpl> StringImpl::convertToUppercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned failingIndex)
{
    ASSERT(is8Bit());
    std::span<Latin1Character> destination;
    auto newImpl = createUninitialized(m_length, destination);

    auto span = span8();
    for (unsigned i = 0; i < failingIndex; ++i) {
        ASSERT(isASCII(span[i]));
        ASSERT(!isASCIILower(span[i]));
        destination[i] = span[i];
    }

    // Do a faster loop for the case where all the characters are ASCII.
    unsigned ored = 0;
    for (unsigned i = failingIndex; i < span.size(); ++i) {
        Latin1Character character = span[i];
        ored |= character;
        destination[i] = toASCIIUpper(character);
    }
    if (!(ored & ~0x7F))
        return newImpl;

    // Do a slower implementation for cases that include non-ASCII Latin-1 characters.
    int numberSharpSCharacters = 0;

    // There are two special cases.
    //  1. Some Latin-1 characters when converted to upper case are 16 bit characters.
    //  2. Lower case sharp-S converts to "SS" (two characters)
    for (unsigned i = 0; i < span.size(); ++i) {
        Latin1Character character = span[i];
        if (character == smallLetterSharpS) [[unlikely]]
            ++numberSharpSCharacters;
        ASSERT(u_toupper(character) <= 0xFFFF);
        char16_t upper = u_toupper(character);
        if (!isLatin1(upper)) [[unlikely]] {
            // Since this upper-cased character does not fit in an 8-bit string, we need to take the 16-bit path.
            return convertToUppercaseWithoutLocaleUpconvert();
        }
        destination[i] = static_cast<Latin1Character>(upper);
    }

    if (!numberSharpSCharacters)
        return newImpl;

    // We have numberSSCharacters sharp-s characters, but none of the other special characters.
    if ((m_length + numberSharpSCharacters) > MaxLength)
        return *this;
    newImpl = createUninitialized(m_length + numberSharpSCharacters, destination);

    size_t destinationIndex = 0;
    for (unsigned i = 0; i < span.size(); ++i) {
        Latin1Character character = span[i];
        if (character == smallLetterSharpS) {
            destination[destinationIndex++] = 'S';
            destination[destinationIndex++] = 'S';
        } else {
            ASSERT(isLatin1(u_toupper(character)));
            destination[destinationIndex++] = static_cast<Latin1Character>(u_toupper(character));
        }
    }

    return newImpl;
}

Ref<StringImpl> StringImpl::convertToUppercaseWithoutLocaleUpconvert()
{
    auto upconvertedCharacters = StringView(*this).upconvertedCharacters();
    auto source16 = upconvertedCharacters.span();

    std::span<char16_t> data16;
    auto newImpl = createUninitialized(source16.size(), data16);

    // Do a faster loop for the case where all the characters are ASCII.
    unsigned ored = 0;
    for (unsigned i = 0; i < m_length; ++i) {
        char16_t character = source16[i];
        ored |= character;
        data16[i] = toASCIIUpper(character);
    }
    if (!(ored & ~0x7F))
        return newImpl;

    // Do a slower implementation for cases that include non-ASCII characters.
    UErrorCode status = U_ZERO_ERROR;
    int32_t realLength = u_strToUpper(data16.data(), m_length, source16.data(), source16.size(), "", &status);
    if (U_SUCCESS(status) && realLength == static_cast<int32_t>(m_length))
        return newImpl;
    newImpl = createUninitialized(realLength, data16);
    status = U_ZERO_ERROR;
    u_strToUpper(data16.data(), data16.size(), source16.data(), source16.size(), "", &status);
    if (U_FAILURE(status))
        return *this;
    return newImpl;
}

static inline bool needsTurkishCasingRules(const AtomString& locale)
{
    // Either "tr" or "az" locale, with ASCII case insensitive comparison and allowing for an ignored subtag.
    char16_t first = locale[0];
    char16_t second = locale[1];
    return ((isASCIIAlphaCaselessEqual(first, 't') && isASCIIAlphaCaselessEqual(second, 'r'))
        || (isASCIIAlphaCaselessEqual(first, 'a') && isASCIIAlphaCaselessEqual(second, 'z')))
        && (locale.length() == 2 || locale[2] == '-');
}

static inline bool needsGreekUppercasingRules(const AtomString& locale)
{
    // The "el" locale, with ASCII case insensitive comparison and allowing for an ignored subtag.
    return isASCIIAlphaCaselessEqual(locale[0], 'e') && isASCIIAlphaCaselessEqual(locale[1], 'l')
        && (locale.length() == 2 || locale[2] == '-');
}

static inline bool needsLithuanianCasingRules(const AtomString& locale)
{
    // The "lt" locale, with ASCII case insensitive comparison and allowing for an ignored subtag.
    return isASCIIAlphaCaselessEqual(locale[0], 'l') && isASCIIAlphaCaselessEqual(locale[1], 't')
        && (locale.length() == 2 || locale[2] == '-');
}

Ref<StringImpl> StringImpl::convertToLowercaseWithLocale(const AtomString& localeIdentifier)
{
    // Use the more-optimized code path most of the time.
    const char* locale;
    if (needsTurkishCasingRules(localeIdentifier)) {
        // Passing in the hardcoded locale "tr" is more efficient than
        // allocating memory just to turn localeIdentifier into a C string, and we assume
        // there is no difference between the lowercasing for "tr" and "az" locales.
        // FIXME: Could optimize further by looking for the three sequences that have locale-specific lowercasing.
        locale = "tr";
    } else if (needsLithuanianCasingRules(localeIdentifier))
        locale = "lt";
    else
        return convertToLowercaseWithoutLocale();

    // FIXME: Could share more code with convertToLowercaseWithoutLocale.

    if (m_length > MaxLength)
        CRASH();

    auto upconvertedCharacters = StringView(*this).upconvertedCharacters();
    auto source16 = upconvertedCharacters.span();
    std::span<char16_t> data16;
    auto newString = createUninitialized(source16.size(), data16);
    UErrorCode status = U_ZERO_ERROR;
    size_t realLength = u_strToLower(data16.data(), data16.size(), source16.data(), source16.size(), locale, &status);
    if (U_SUCCESS(status) && realLength == source16.size())
        return newString;
    newString = createUninitialized(realLength, data16);
    status = U_ZERO_ERROR;
    u_strToLower(data16.data(), data16.size(), source16.data(), source16.size(), locale, &status);
    if (U_FAILURE(status))
        return *this;
    return newString;
}

Ref<StringImpl> StringImpl::convertToUppercaseWithLocale(const AtomString& localeIdentifier)
{
    // Use the more-optimized code path most of the time.
    const char* locale;
    if (needsTurkishCasingRules(localeIdentifier) && find('i') != notFound) {
        // Passing in the hardcoded locale "tr" is more efficient than
        // allocating memory just to turn localeIdentifier into a C string, and we assume
        // there is no difference between the uppercasing for "tr" and "az" locales.
        locale = "tr";
    } else if (needsGreekUppercasingRules(localeIdentifier))
        locale = "el";
    else if (needsLithuanianCasingRules(localeIdentifier))
        locale = "lt";
    else
        return convertToUppercaseWithoutLocale();

    if (m_length > MaxLength)
        CRASH();

    auto upconvertedCharacters = StringView(*this).upconvertedCharacters();
    auto source16 = upconvertedCharacters.span();
    std::span<char16_t> data16;
    auto newString = createUninitialized(source16.size(), data16);
    UErrorCode status = U_ZERO_ERROR;
    size_t realLength = u_strToUpper(data16.data(), data16.size(), source16.data(), source16.size(), locale, &status);
    if (U_SUCCESS(status) && realLength == source16.size())
        return newString;
    newString = createUninitialized(realLength, data16);
    status = U_ZERO_ERROR;
    u_strToUpper(data16.data(), data16.size(), source16.data(), source16.size(), locale, &status);
    if (U_FAILURE(status))
        return *this;
    return newString;
}

Ref<StringImpl> StringImpl::foldCase()
{
    if (is8Bit()) {
        unsigned failingIndex;
        auto span = span8();
        for (unsigned i = 0; i < span.size(); ++i) {
            auto character = span[i];
            if (!isASCII(character) || isASCIIUpper(character)) [[unlikely]] {
                failingIndex = i;
                goto SlowPath;
            }
        }
        // String was all ASCII and no uppercase, so just return as-is.
        return *this;

SlowPath:
        bool need16BitCharacters = false;
        for (unsigned i = failingIndex; i < span.size(); ++i) {
            auto character = span[i];
            if (character == 0xB5 || character == 0xDF) {
                need16BitCharacters = true;
                break;
            }
        }

        if (!need16BitCharacters) {
            std::span<Latin1Character> data8;
            auto folded = createUninitializedInternalNonEmpty(m_length, data8);
            copyCharacters(data8, span.first(failingIndex));
            for (unsigned i = failingIndex; i < span.size(); ++i) {
                auto character = span[i];
                if (isASCII(character))
                    data8[i] = toASCIILower(character);
                else {
                    ASSERT(isLatin1(u_foldCase(character, U_FOLD_CASE_DEFAULT)));
                    data8[i] = static_cast<Latin1Character>(u_foldCase(character, U_FOLD_CASE_DEFAULT));
                }
            }
            return folded;
        }
    } else {
        // FIXME: Unclear why we use goto in the 8-bit case, and a different approach in the 16-bit case.
        bool noUpper = true;
        unsigned ored = 0;
        auto span = span16();
        for (unsigned i = 0; i < span.size(); ++i) {
            char16_t character = span[i];
            if (isASCIIUpper(character)) [[unlikely]]
                noUpper = false;
            ored |= character;
        }
        if (!(ored & ~0x7F)) {
            if (noUpper) {
                // String was all ASCII and no uppercase, so just return as-is.
                return *this;
            }
            std::span<char16_t> data16;
            auto folded = createUninitializedInternalNonEmpty(m_length, data16);
            for (unsigned i = 0; i < span.size(); ++i)
                data16[i] = toASCIILower(span[i]);
            return folded;
        }
    }

    if (m_length > MaxLength)
        CRASH();

    auto upconvertedCharacters = StringView(*this).upconvertedCharacters();
    auto source16 = upconvertedCharacters.span();

    std::span<char16_t> data;
    auto folded = createUninitializedInternalNonEmpty(source16.size(), data);
    UErrorCode status = U_ZERO_ERROR;
    size_t realLength = u_strFoldCase(data.data(), source16.size(), source16.data(), source16.size(), U_FOLD_CASE_DEFAULT, &status);
    if (U_SUCCESS(status) && realLength == source16.size())
        return folded;
    ASSERT(realLength > source16.size());
    folded = createUninitializedInternalNonEmpty(realLength, data);
    status = U_ZERO_ERROR;
    u_strFoldCase(data.data(), realLength, source16.data(), source16.size(), U_FOLD_CASE_DEFAULT, &status);
    if (U_FAILURE(status))
        return *this;
    return folded;
}

template<StringImpl::CaseConvertType type, typename CharacterType>
ALWAYS_INLINE Ref<StringImpl> StringImpl::convertASCIICase(StringImpl& impl, std::span<const CharacterType> data)
{
    size_t failingIndex;
    for (size_t i = 0; i < data.size(); ++i) {
        CharacterType character = data[i];
        if constexpr (type == CaseConvertType::Lower) {
            if (isASCIIUpper(character)) [[unlikely]] {
                failingIndex = i;
                goto SlowPath;
            }
        } else {
            if (isASCIILower(character)) [[likely]] {
                failingIndex = i;
                goto SlowPath;
            }
        }
    }
    return impl;

SlowPath:
    std::span<CharacterType> newData;
    auto newImpl = createUninitializedInternalNonEmpty(data.size(), newData);
    copyCharacters(newData, data.first(failingIndex));
    for (size_t i = failingIndex; i < data.size(); ++i)
        newData[i] = type == CaseConvertType::Lower ? toASCIILower(data[i]) : toASCIIUpper(data[i]);
    return newImpl;
}

Ref<StringImpl> StringImpl::convertToASCIILowercase()
{
    if (is8Bit())
        return convertASCIICase<CaseConvertType::Lower>(*this, span8());
    return convertASCIICase<CaseConvertType::Lower>(*this, span16());
}

Ref<StringImpl> StringImpl::convertToASCIIUppercase()
{
    if (is8Bit())
        return convertASCIICase<CaseConvertType::Upper>(*this, span8());
    return convertASCIICase<CaseConvertType::Upper>(*this, span16());
}

template<typename CharacterType, typename CodeUnitPredicate> inline Ref<StringImpl> StringImpl::trimMatchedCharacters(CodeUnitPredicate predicate)
{
    auto span = StringImpl::span<CharacterType>();
    if (span.empty())
        return *this;

    unsigned start = 0;
    unsigned end = span.size() - 1;

    // skip matched characters from start
    while (start <= end && predicate(span[start]))
        ++start;

    // only matched characters
    if (start > end) 
        return *empty();

    // skip matched characters from end
    while (end && predicate(span[end]))
        --end;

    if (!start && end == span.size() - 1)
        return *this;
    return create(span.subspan(start, end + 1 - start));
}

Ref<StringImpl> StringImpl::trim(CodeUnitMatchFunction predicate)
{
    if (is8Bit())
        return trimMatchedCharacters<Latin1Character>(predicate);
    return trimMatchedCharacters<char16_t>(predicate);
}

template<typename CharacterType, class CodeUnitPredicate> inline Ref<StringImpl> StringImpl::simplifyMatchedCharactersToSpace(CodeUnitPredicate predicate)
{
    StringBuffer<CharacterType> data(m_length);

    auto from = span<CharacterType>();
    unsigned outc = 0;
    bool changedToSpace = false;
    
    auto to = data.span();
    
    while (true) {
        while (!from.empty() && predicate(from.front())) {
            if (consume(from) != ' ')
                changedToSpace = true;
        }
        while (!from.empty() && !predicate(from.front()))
            to[outc++] = consume(from);
        if (!from.empty())
            to[outc++] = ' ';
        else
            break;
    }

    if (outc && to[outc - 1] == ' ')
        --outc;

    if (outc == m_length && !changedToSpace)
        return *this;

    data.shrink(outc);

    return adopt(WTFMove(data));
}

Ref<StringImpl> StringImpl::simplifyWhiteSpace(CodeUnitMatchFunction isWhiteSpace)
{
    if (is8Bit())
        return StringImpl::simplifyMatchedCharactersToSpace<Latin1Character>(isWhiteSpace);
    return StringImpl::simplifyMatchedCharactersToSpace<char16_t>(isWhiteSpace);
}

double StringImpl::toDouble(bool* ok)
{
    if (is8Bit())
        return charactersToDouble(span8(), ok);
    return charactersToDouble(span16(), ok);
}

float StringImpl::toFloat(bool* ok)
{
    if (is8Bit())
        return charactersToFloat(span8(), ok);
    return charactersToFloat(span16(), ok);
}

size_t StringImpl::find(std::span<const Latin1Character> matchString, size_t start)
{
    ASSERT(!matchString.empty());
    ASSERT(isValidLength<Latin1Character>(matchString.size()));

    // Check start & matchLength are in range.
    if (start > length())
        return notFound;
    size_t searchLength = length() - start;
    if (matchString.size() > searchLength)
        return notFound;
    // delta is the number of additional times to test; delta == 0 means test only once.
    size_t delta = searchLength - matchString.size();

    // Optimization: keep a running hash of the strings,
    // only call equal if the hashes match.

    if (is8Bit()) {
        auto searchCharacters = span8().subspan(start);

        unsigned searchHash = 0;
        unsigned matchHash = 0;
        for (size_t i = 0; i < matchString.size(); ++i) {
            searchHash += searchCharacters[i];
            matchHash += matchString[i];
        }

        size_t i = 0;
        while (searchHash != matchHash || !equal(searchCharacters.subspan(i).data(), matchString)) {
            if (i == delta)
                return notFound;
            searchHash += searchCharacters[i + matchString.size()];
            searchHash -= searchCharacters[i];
            ++i;
        }
        return start + i;
    }

    auto searchCharacters = span16().subspan(start);

    unsigned searchHash = 0;
    unsigned matchHash = 0;
    for (size_t i = 0; i < matchString.size(); ++i) {
        searchHash += searchCharacters[i];
        matchHash += matchString[i];
    }

    size_t i = 0;
    while (searchHash != matchHash || !equal(searchCharacters.subspan(i).data(), matchString)) {
        if (i == delta)
            return notFound;
        searchHash += searchCharacters[i + matchString.size()];
        searchHash -= searchCharacters[i];
        ++i;
    }
    return start + i;
}

size_t StringImpl::reverseFind(std::span<const Latin1Character> matchString, size_t start)
{
    ASSERT(!matchString.empty());

    if (matchString.size() > length())
        return notFound;

    if (is8Bit())
        return reverseFindInner(span8(), matchString, start);
    return reverseFindInner(span16(), matchString, start);
}

size_t StringImpl::find(StringView matchString)
{
    // Check for null string to match against
    if (!matchString) [[unlikely]]
        return notFound;
    unsigned matchLength = matchString.length();

    // Optimization 1: fast case for strings of length 1.
    if (matchLength == 1) {
        if (is8Bit()) {
            if (matchString.is8Bit())
                return WTF::find(span8(), matchString.span8().front());
            return WTF::find(span8(), matchString.span16().front());
        }
        if (matchString.is8Bit())
            return WTF::find(span16(), matchString.span8().front());
        return WTF::find(span16(), matchString.span16().front());
    }

    // Check matchLength is in range.
    if (matchLength > length())
        return notFound;

    // Check for empty string to match against
    if (!matchLength) [[unlikely]]
        return 0;

    if (is8Bit()) {
        if (matchString.is8Bit())
            return findInner(span8(), matchString.span8(), 0);
        return findInner(span8(), matchString.span16(), 0);
    }

    if (matchString.is8Bit())
        return findInner(span16(), matchString.span8(), 0);

    return findInner(span16(), matchString.span16(), 0);
}

size_t StringImpl::find(StringView matchString, size_t start)
{
    // Check for null or empty string to match against
    if (!matchString) [[unlikely]]
        return notFound;

    return findCommon(StringView { *this }, matchString, start);
}

size_t StringImpl::findIgnoringASCIICase(StringView matchString) const
{
    if (!matchString)
        return notFound;
    return ::WTF::findIgnoringASCIICase(*this, matchString, 0);
}

size_t StringImpl::findIgnoringASCIICase(StringView matchString, size_t start) const
{
    if (!matchString)
        return notFound;
    return ::WTF::findIgnoringASCIICase(*this, matchString, start);
}

size_t StringImpl::reverseFind(char16_t character, size_t start)
{
    if (is8Bit())
        return WTF::reverseFind(span8(), character, start);
    return WTF::reverseFind(span16(), character, start);
}

size_t StringImpl::reverseFind(StringView matchString, size_t start)
{
    // Check for null or empty string to match against
    if (!matchString)
        return notFound;
    if (matchString.isEmpty())
        return std::min<size_t>(start, length());

    // Optimization 1: fast case for strings of length 1.
    if (matchString.length() == 1) {
        if (is8Bit())
            return WTF::reverseFind(span8(), matchString[0], start);
        return WTF::reverseFind(span16(), matchString[0], start);
    }

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

ALWAYS_INLINE static bool equalInner(const StringImpl& string, unsigned start, std::span<const char> matchString)
{
    ASSERT(matchString.size() <= string.length());
    ASSERT(start + matchString.size() <= string.length());

    if (string.is8Bit())
        return equal(string.span8().subspan(start).data(), byteCast<Latin1Character>(matchString));
    return equal(string.span16().subspan(start).data(), byteCast<Latin1Character>(matchString));
}

ALWAYS_INLINE static bool equalInner(const StringImpl& string, unsigned start, StringView matchString)
{
    if (start > string.length())
        return false;
    if (matchString.length() > string.length())
        return false;
    if (matchString.length() + start > string.length())
        return false;

    if (string.is8Bit()) {
        if (matchString.is8Bit())
            return equal(string.span8().subspan(start).data(), matchString.span8());
        return equal(string.span8().subspan(start).data(), matchString.span16());
    }
    if (matchString.is8Bit())
        return equal(string.span16().subspan(start).data(), matchString.span8());
    return equal(string.span16().subspan(start).data(), matchString.span16());
}

bool StringImpl::startsWith(StringView string) const
{
    return !string || ::WTF::startsWith(*this, string);
}

bool StringImpl::startsWithIgnoringASCIICase(StringView prefix) const
{
    return prefix && ::WTF::startsWithIgnoringASCIICase(*this, prefix);
}

bool StringImpl::startsWith(char16_t character) const
{
    return m_length && (*this)[0] == character;
}

bool StringImpl::startsWith(std::span<const char> matchString) const
{
    return matchString.size() <= length() && equalInner(*this, 0, matchString);
}

bool StringImpl::hasInfixStartingAt(StringView matchString, size_t start) const
{
    return equalInner(*this, start, matchString);
}

bool StringImpl::endsWith(StringView suffix)
{
    return suffix && ::WTF::endsWith(*this, suffix);
}

bool StringImpl::endsWithIgnoringASCIICase(StringView suffix) const
{
    return suffix && ::WTF::endsWithIgnoringASCIICase(*this, suffix);
}

bool StringImpl::endsWith(char16_t character) const
{
    return m_length && (*this)[m_length - 1] == character;
}

bool StringImpl::endsWith(std::span<const char> matchString) const
{
    return matchString.size() <= length() && equalInner(*this, length() - matchString.size(), matchString);
}

bool StringImpl::hasInfixEndingAt(StringView matchString, size_t end) const
{
    return end >= matchString.length() && equalInner(*this, end - matchString.length(), matchString);
}

Ref<StringImpl> StringImpl::replace(char16_t target, char16_t replacement)
{
    if (target == replacement)
        return *this;

    if (is8Bit()) {
        if (!isLatin1(target)) {
            // Looking for a 16-bit character in an 8-bit string, so we're done.
            return *this;
        }
        auto span8 = this->span8();
        unsigned i;
        for (i = 0; i < span8.size(); ++i) {
            if (static_cast<char16_t>(span8[i]) == target)
                break;
        }
        if (i == span8.size())
            return *this;
        return createByReplacingInCharacters(span8, target, replacement, i);
    }

    auto span16 = this->span16();
    unsigned i;
    for (i = 0; i < span16.size(); ++i) {
        if (span16[i] == target)
            break;
    }
    if (i == span16.size())
        return *this;
    return createByReplacingInCharacters(span16, target, replacement, i);
}

Ref<StringImpl> StringImpl::replace(size_t position, size_t lengthToReplace, StringView string)
{
    position = std::min<size_t>(position, length());
    lengthToReplace = std::min(lengthToReplace, length() - position);
    size_t lengthToInsert = string.length();
    if (!lengthToReplace && !lengthToInsert)
        return *this;

    if ((length() - lengthToReplace) >= (MaxLength - lengthToInsert))
        CRASH();

    if (is8Bit() && (!string || string.is8Bit())) {
        std::span<Latin1Character> data;
        auto newImpl = createUninitialized(length() - lengthToReplace + lengthToInsert, data);
        copyCharacters(data, span8().first(position));
        if (string)
            copyCharacters(data.subspan(position), string.span8().first(lengthToInsert));
        copyCharacters(data.subspan(position + lengthToInsert), span8().subspan(position + lengthToReplace));
        return newImpl;
    }
    std::span<char16_t> data;
    auto newImpl = createUninitialized(length() - lengthToReplace + lengthToInsert, data);
    if (is8Bit())
        copyCharacters(data, span8().first(position));
    else
        copyCharacters(data, span16().first(position));
    if (string) {
        if (string.is8Bit())
            copyCharacters(data.subspan(position), string.span8().first(lengthToInsert));
        else
            copyCharacters(data.subspan(position), string.span16().first(lengthToInsert));
    }
    if (is8Bit())
        copyCharacters(data.subspan(position + lengthToInsert), span8().subspan(position + lengthToReplace));
    else
        copyCharacters(data.subspan(position + lengthToInsert), span16().subspan(position + lengthToReplace));
    return newImpl;
}

Ref<StringImpl> StringImpl::replace(char16_t pattern, StringView replacement)
{
    if (!replacement)
        return *this;
    if (replacement.is8Bit())
        return replace(pattern, replacement.span8());
    return replace(pattern, replacement.span16());
}

Ref<StringImpl> StringImpl::replace(char16_t pattern, std::span<const Latin1Character> replacement)
{
    ASSERT(replacement.data());

    size_t srcSegmentStart = 0;
    size_t matchCount = 0;

    // Count the matches.
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) != notFound) {
        ++matchCount;
        ++srcSegmentStart;
    }

    // If we have 0 matches then we don't have to do any more work.
    if (!matchCount)
        return *this;

    if (!replacement.empty() && matchCount > MaxLength / replacement.size())
        CRASH();

    size_t replaceSize = matchCount * replacement.size();
    size_t newSize = m_length - matchCount;
    if (newSize >= (MaxLength - replaceSize))
        CRASH();

    newSize += replaceSize;

    // Construct the new data.
    size_t srcSegmentEnd;
    size_t srcSegmentLength;
    srcSegmentStart = 0;
    size_t dstOffset = 0;

    if (is8Bit()) {
        std::span<Latin1Character> data;
        auto newImpl = createUninitialized(newSize, data);

        while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
            srcSegmentLength = srcSegmentEnd - srcSegmentStart;
            copyCharacters(data.subspan(dstOffset), span8().subspan(srcSegmentStart, srcSegmentLength));
            dstOffset += srcSegmentLength;
            copyCharacters(data.subspan(dstOffset), replacement);
            dstOffset += replacement.size();
            srcSegmentStart = srcSegmentEnd + 1;
        }

        srcSegmentLength = m_length - srcSegmentStart;
        copyCharacters(data.subspan(dstOffset), span8().subspan(srcSegmentStart));

        ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

        return newImpl;
    }

    std::span<char16_t> data;
    auto newImpl = createUninitialized(newSize, data);

    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        copyCharacters(data.subspan(dstOffset), span16().subspan(srcSegmentStart, srcSegmentLength));

        dstOffset += srcSegmentLength;
        copyCharacters(data.subspan(dstOffset), replacement);

        dstOffset += replacement.size();
        srcSegmentStart = srcSegmentEnd + 1;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    copyCharacters(data.subspan(dstOffset), span16().subspan(srcSegmentStart));

    ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

    return newImpl;
}

Ref<StringImpl> StringImpl::replace(char16_t pattern, std::span<const char16_t> replacement)
{
    ASSERT(replacement.data());

    size_t srcSegmentStart = 0;
    size_t matchCount = 0;

    // Count the matches.
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) != notFound) {
        ++matchCount;
        ++srcSegmentStart;
    }

    // If we have 0 matches then we don't have to do any more work.
    if (!matchCount)
        return *this;

    if (!replacement.empty() && matchCount > MaxLength / replacement.size())
        CRASH();

    size_t replaceSize = matchCount * replacement.size();
    size_t newSize = m_length - matchCount;
    if (newSize >= (MaxLength - replaceSize))
        CRASH();

    newSize += replaceSize;

    // Construct the new data.
    size_t srcSegmentEnd;
    size_t srcSegmentLength;
    srcSegmentStart = 0;
    size_t dstOffset = 0;

    if (is8Bit()) {
        std::span<char16_t> data;
        auto newImpl = createUninitialized(newSize, data);

        while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
            srcSegmentLength = srcSegmentEnd - srcSegmentStart;
            copyCharacters(data.subspan(dstOffset), span8().subspan(srcSegmentStart, srcSegmentLength));

            dstOffset += srcSegmentLength;
            copyCharacters(data.subspan(dstOffset), replacement);

            dstOffset += replacement.size();
            srcSegmentStart = srcSegmentEnd + 1;
        }

        srcSegmentLength = m_length - srcSegmentStart;
        copyCharacters(data.subspan(dstOffset), span8().subspan(srcSegmentStart));

        ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

        return newImpl;
    }

    std::span<char16_t> data;
    auto newImpl = createUninitialized(newSize, data);

    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        copyCharacters(data.subspan(dstOffset), span16().subspan(srcSegmentStart, srcSegmentLength));

        dstOffset += srcSegmentLength;
        copyCharacters(data.subspan(dstOffset), replacement);

        dstOffset += replacement.size();
        srcSegmentStart = srcSegmentEnd + 1;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    copyCharacters(data.subspan(dstOffset), span16().subspan(srcSegmentStart));

    ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

    return newImpl;
}

Ref<StringImpl> StringImpl::replace(StringView pattern, StringView replacement)
{
    if (!pattern || !replacement)
        return *this;

    unsigned patternLength = pattern.length();
    if (!patternLength)
        return *this;

    unsigned repStrLength = replacement.length();
    size_t srcSegmentStart = 0;
    unsigned matchCount = 0;

    // Count the matches.
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) != notFound) {
        ++matchCount;
        srcSegmentStart += patternLength;
    }

    // If we have 0 matches, we don't have to do any more work
    if (!matchCount)
        return *this;

    unsigned newSize = m_length - matchCount * patternLength;
    if (repStrLength && matchCount > MaxLength / repStrLength)
        CRASH();

    if (newSize > (MaxLength - matchCount * repStrLength))
        CRASH();

    newSize += matchCount * repStrLength;


    // Construct the new data
    size_t srcSegmentEnd;
    unsigned srcSegmentLength;
    srcSegmentStart = 0;
    unsigned dstOffset = 0;
    bool srcIs8Bit = is8Bit();
    bool replacementIs8Bit = replacement.is8Bit();

    // There are 4 cases:
    // 1. This and replacement are both 8 bit.
    // 2. This and replacement are both 16 bit.
    // 3. This is 8 bit and replacement is 16 bit.
    // 4. This is 16 bit and replacement is 8 bit.
    if (srcIs8Bit && replacementIs8Bit) {
        // Case 1
        std::span<Latin1Character> data;
        auto newImpl = createUninitialized(newSize, data);
        while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
            srcSegmentLength = srcSegmentEnd - srcSegmentStart;
            copyCharacters(data.subspan(dstOffset), span8().subspan(srcSegmentStart, srcSegmentLength));
            dstOffset += srcSegmentLength;
            copyCharacters(data.subspan(dstOffset), replacement.span8().first(repStrLength));
            dstOffset += repStrLength;
            srcSegmentStart = srcSegmentEnd + patternLength;
        }

        srcSegmentLength = m_length - srcSegmentStart;
        copyCharacters(data.subspan(dstOffset), span8().subspan(srcSegmentStart));

        ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

        return newImpl;
    }

    std::span<char16_t> data;
    auto newImpl = createUninitialized(newSize, data);
    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        if (srcIs8Bit) {
            // Case 3.
            copyCharacters(data.subspan(dstOffset), span8().subspan(srcSegmentStart, srcSegmentLength));
        } else {
            // Case 2 & 4.
            copyCharacters(data.subspan(dstOffset), span16().subspan(srcSegmentStart, srcSegmentLength));
        }
        dstOffset += srcSegmentLength;
        if (replacementIs8Bit) {
            // Cases 2 & 3.
            copyCharacters(data.subspan(dstOffset), replacement.span8().first(repStrLength));
        } else {
            // Case 4
            copyCharacters(data.subspan(dstOffset), replacement.span16().first(repStrLength));
        }
        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + patternLength;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    if (srcIs8Bit) {
        // Case 3.
        copyCharacters(data.subspan(dstOffset), span8().subspan(srcSegmentStart));
    } else {
        // Cases 2 & 4.
        copyCharacters(data.subspan(dstOffset), span16().subspan(srcSegmentStart));
    }

    ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

    return newImpl;
}

bool equal(const StringImpl* a, const StringImpl* b)
{
    return equalCommon(a, b);
}

template<typename CharacterType> inline bool equalInternal(const StringImpl* a, std::span<const CharacterType> b)
{
    if (!a)
        return !b.data();
    if (!b.data())
        return false;

    if (a->length() != b.size())
        return false;
    if (b.empty())
        return true;
    if (a->is8Bit())
        return a->span8().front() == b.front() && equal(a->span8().subspan(1).data(), b.subspan(1));
    return a->span16().front() == b.front() && equal(a->span16().subspan(1).data(), b.subspan(1));
}

bool equal(const StringImpl* a, std::span<const Latin1Character> b)
{
    return equalInternal(a, b);
}

bool equal(const StringImpl* a, std::span<const char16_t> b)
{
    return equalInternal(a, b);
}

bool equal(const StringImpl& a, const StringImpl& b)
{
    unsigned aHash = a.rawHash();
    unsigned bHash = b.rawHash();
    if (aHash != bHash && aHash && bHash)
        return false;
    return equalCommon(a, b);
}

bool equalIgnoringNullity(StringImpl* a, StringImpl* b)
{
    if (!a && b && !b->length())
        return true;
    if (!b && a && !a->length())
        return true;
    return equal(a, b);
}

bool equalIgnoringASCIICase(const StringImpl* a, const StringImpl* b)
{
    return a == b || (a && b && equalIgnoringASCIICase(*a, *b));
}

bool equalIgnoringASCIICaseNonNull(const StringImpl* a, const StringImpl* b)
{
    ASSERT(a);
    ASSERT(b);
    return equalIgnoringASCIICase(*a, *b);
}

std::optional<UCharDirection> StringImpl::defaultWritingDirection()
{
    for (auto codePoint : StringView(this).codePoints()) {
        auto charDirection = u_charDirection(codePoint);
        if (charDirection == U_LEFT_TO_RIGHT)
            return U_LEFT_TO_RIGHT;
        if (charDirection == U_RIGHT_TO_LEFT || charDirection == U_RIGHT_TO_LEFT_ARABIC) {
            return U_RIGHT_TO_LEFT;
        }
    }
    return std::nullopt;
}

Ref<StringImpl> StringImpl::adopt(StringBuffer<Latin1Character>&& buffer)
{
    if (!buffer.length())
        return *empty();
    return adoptRef(*new StringImpl(buffer.release()));
}

Ref<StringImpl> StringImpl::adopt(StringBuffer<char16_t>&& buffer)
{
    if (!buffer.length())
        return *empty();
    return adoptRef(*new StringImpl(buffer.release()));
}

size_t StringImpl::sizeInBytes() const
{
    // FIXME: support substrings
    size_t size = length();
    if (!is8Bit())
        size *= 2;
    return size + sizeof(*this);
}

Expected<CString, UTF8ConversionError> StringImpl::utf8ForCharacters(std::span<const Latin1Character> source)
{
    return tryGetUTF8ForCharacters([] (std::span<const char8_t> converted) {
        return CString { converted };
    }, source);
}

Expected<CString, UTF8ConversionError> StringImpl::utf8ForCharacters(std::span<const char16_t> characters, ConversionMode mode)
{
    return tryGetUTF8ForCharacters([] (std::span<const char8_t> converted) {
        return CString { converted };
    }, characters, mode);
}

Expected<size_t, UTF8ConversionError> StringImpl::utf8ForCharactersIntoBuffer(std::span<const char16_t> span, ConversionMode mode, Vector<char8_t, 1024>& bufferVector)
{
    ASSERT(bufferVector.size() == span.size() * 3);
    ConversionResult<char8_t> result;
    switch (mode) {
    case StrictConversion:
        result = Unicode::convert(span, bufferVector.mutableSpan());
        break;
    // FIXME: Lenient is exactly the same as "replacing unpaired surrogates with FFFD"; we don't need both.
    case StrictConversionReplacingUnpairedSurrogatesWithFFFD:
    case LenientConversion:
        result = Unicode::convertReplacingInvalidSequences(span, bufferVector.mutableSpan());
        break;
    }
    if (result.code == ConversionResultCode::SourceInvalid)
        return makeUnexpected(UTF8ConversionError::Invalid);
    return result.buffer.size();
}

Expected<CString, UTF8ConversionError> StringImpl::tryGetUTF8(ConversionMode mode) const
{
    if (is8Bit())
        return utf8ForCharacters(span8());
    return utf8ForCharacters(span16(), mode);
}

CString StringImpl::utf8(ConversionMode mode) const
{
    auto expectedString = tryGetUTF8(mode);
    RELEASE_ASSERT(expectedString);
    return expectedString.value();
}

NEVER_INLINE unsigned StringImpl::hashSlowCase() const
{
    if (is8Bit())
        setHash(StringHasher::computeHashAndMaskTop8Bits(span8()));
    else
        setHash(StringHasher::computeHashAndMaskTop8Bits(span16()));
    return existingHash();
}

unsigned StringImpl::concurrentHash() const
{
    unsigned hash;
    if (is8Bit())
        hash = StringHasher::computeHashAndMaskTop8Bits(span8());
    else
        hash = StringHasher::computeHashAndMaskTop8Bits(span16());
    ASSERT(((hash << s_flagCount) >> s_flagCount) == hash);
    return hash;
}

bool equalIgnoringNullity(std::span<const char16_t> a, StringImpl* b)
{
    if (!b)
        return a.empty();
    if (a.size() != b->length())
        return false;
    if (b->is8Bit()) {
        for (auto [aCharacter, bCharacter] : zippedRange(a, b->span8())) {
            if (aCharacter != bCharacter)
                return false;
        }
        return true;
    }
    return equal(a, b->span16());
}

} // namespace WTF
