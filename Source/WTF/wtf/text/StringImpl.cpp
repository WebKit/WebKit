/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include <wtf/ProcessID.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/ExternalStringImpl.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>
#include <wtf/text/SymbolImpl.h>
#include <wtf/text/SymbolRegistry.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/UTF8.h>

#if STRING_STATS
#include <unistd.h>
#include <wtf/DataLog.h>
#endif

namespace WTF {

using namespace Unicode;

static_assert(sizeof(StringImpl) == 2 * sizeof(int) + 2 * sizeof(void*), "StringImpl should stay small");

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

StringImpl::StaticStringImpl StringImpl::s_atomicEmptyString("", StringImpl::StringAtomic);

StringImpl::~StringImpl()
{
    ASSERT(!isStatic());

    StringView::invalidate(*this);

    STRING_STATS_REMOVE_STRING(*this);

    if (isAtomic()) {
        ASSERT(!isSymbol());
        if (length())
            AtomicStringImpl::remove(static_cast<AtomicStringImpl*>(this));
    } else if (isSymbol()) {
        auto& symbol = static_cast<SymbolImpl&>(*this);
        auto* symbolRegistry = symbol.symbolRegistry();
        if (symbolRegistry)
            symbolRegistry->remove(*symbol.asRegisteredSymbolImpl());
    }

    BufferOwnership ownership = bufferOwnership();

    if (ownership == BufferInternal)
        return;
    if (ownership == BufferOwned) {
        // We use m_data8, but since it is a union with m_data16 this works either way.
        ASSERT(m_data8);
        fastFree(const_cast<LChar*>(m_data8));
        return;
    }
    if (ownership == BufferExternal) {
        auto* external = static_cast<ExternalStringImpl*>(this);
        external->freeExternalBuffer(const_cast<LChar*>(m_data8), sizeInBytes());
        external->m_free.~ExternalStringImplFreeFunction();
        return;
    }

    ASSERT(ownership == BufferSubstring);
    ASSERT(substringBuffer());
    substringBuffer()->deref();
}

void StringImpl::destroy(StringImpl* stringImpl)
{
    stringImpl->~StringImpl();
    fastFree(stringImpl);
}

Ref<StringImpl> StringImpl::createFromLiteral(const char* characters, unsigned length)
{
    ASSERT_WITH_MESSAGE(length, "Use StringImpl::empty() to create an empty string");
    ASSERT(charactersAreAllASCII<LChar>(reinterpret_cast<const LChar*>(characters), length));
    return adoptRef(*new StringImpl(reinterpret_cast<const LChar*>(characters), length, ConstructWithoutCopying));
}

Ref<StringImpl> StringImpl::createFromLiteral(const char* characters)
{
    return createFromLiteral(characters, strlen(characters));
}

Ref<StringImpl> StringImpl::createWithoutCopying(const UChar* characters, unsigned length)
{
    if (!length)
        return *empty();
    return adoptRef(*new StringImpl(characters, length, ConstructWithoutCopying));
}

Ref<StringImpl> StringImpl::createWithoutCopying(const LChar* characters, unsigned length)
{
    if (!length)
        return *empty();
    return adoptRef(*new StringImpl(characters, length, ConstructWithoutCopying));
}

template<typename CharacterType> inline Ref<StringImpl> StringImpl::createUninitializedInternal(unsigned length, CharacterType*& data)
{
    if (!length) {
        data = 0;
        return *empty();
    }
    return createUninitializedInternalNonEmpty(length, data);
}

template<typename CharacterType> inline Ref<StringImpl> StringImpl::createUninitializedInternalNonEmpty(unsigned length, CharacterType*& data)
{
    ASSERT(length);

    // Allocate a single buffer large enough to contain the StringImpl
    // struct as well as the data which it contains. This removes one
    // heap allocation from this call.
    if (length > ((std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) / sizeof(CharacterType)))
        CRASH();
    StringImpl* string = static_cast<StringImpl*>(fastMalloc(allocationSize<CharacterType>(length)));

    data = string->tailPointer<CharacterType>();
    return constructInternal<CharacterType>(*string, length);
}

Ref<StringImpl> StringImpl::createUninitialized(unsigned length, LChar*& data)
{
    return createUninitializedInternal(length, data);
}

Ref<StringImpl> StringImpl::createUninitialized(unsigned length, UChar*& data)
{
    return createUninitializedInternal(length, data);
}

template<typename CharacterType> inline Ref<StringImpl> StringImpl::reallocateInternal(Ref<StringImpl>&& originalString, unsigned length, CharacterType*& data)
{
    ASSERT(originalString->hasOneRef());
    ASSERT(originalString->bufferOwnership() == BufferInternal);

    if (!length) {
        data = 0;
        return *empty();
    }

    // Same as createUninitialized() except here we use fastRealloc.
    if (length > ((std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) / sizeof(CharacterType)))
        CRASH();

    originalString->~StringImpl();
    auto* string = static_cast<StringImpl*>(fastRealloc(&originalString.leakRef(), allocationSize<CharacterType>(length)));

    data = string->tailPointer<CharacterType>();
    return constructInternal<CharacterType>(*string, length);
}

Ref<StringImpl> StringImpl::reallocate(Ref<StringImpl>&& originalString, unsigned length, LChar*& data)
{
    ASSERT(originalString->is8Bit());
    return reallocateInternal(WTFMove(originalString), length, data);
}

Ref<StringImpl> StringImpl::reallocate(Ref<StringImpl>&& originalString, unsigned length, UChar*& data)
{
    ASSERT(!originalString->is8Bit());
    return reallocateInternal(WTFMove(originalString), length, data);
}

template<typename CharacterType> inline Ref<StringImpl> StringImpl::createInternal(const CharacterType* characters, unsigned length)
{
    if (!characters || !length)
        return *empty();
    CharacterType* data;
    auto string = createUninitializedInternalNonEmpty(length, data);
    copyCharacters(data, characters, length);
    return string;
}

Ref<StringImpl> StringImpl::create(const UChar* characters, unsigned length)
{
    return createInternal(characters, length);
}

Ref<StringImpl> StringImpl::create(const LChar* characters, unsigned length)
{
    return createInternal(characters, length);
}

Ref<StringImpl> StringImpl::create8BitIfPossible(const UChar* characters, unsigned length)
{
    if (!characters || !length)
        return *empty();

    LChar* data;
    auto string = createUninitializedInternalNonEmpty(length, data);

    for (size_t i = 0; i < length; ++i) {
        if (characters[i] & 0xFF00)
            return create(characters, length);
        data[i] = static_cast<LChar>(characters[i]);
    }

    return string;
}

Ref<StringImpl> StringImpl::create8BitIfPossible(const UChar* string)
{
    return StringImpl::create8BitIfPossible(string, lengthOfNullTerminatedString(string));
}

Ref<StringImpl> StringImpl::create(const LChar* string)
{
    if (!string)
        return *empty();
    size_t length = strlen(reinterpret_cast<const char*>(string));
    if (length > std::numeric_limits<unsigned>::max())
        CRASH();
    return create(string, length);
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
        return create(m_data8 + start, length);

    return create(m_data16 + start, length);
}

UChar32 StringImpl::characterStartingAt(unsigned i)
{
    if (is8Bit())
        return m_data8[i];
    if (U16_IS_SINGLE(m_data16[i]))
        return m_data16[i];
    if (i + 1 < m_length && U16_IS_LEAD(m_data16[i]) && U16_IS_TRAIL(m_data16[i + 1]))
        return U16_GET_SUPPLEMENTARY(m_data16[i], m_data16[i + 1]);
    return 0;
}

Ref<StringImpl> StringImpl::convertToLowercaseWithoutLocale()
{
    // Note: At one time this was a hot function in the Dromaeo benchmark, specifically the
    // no-op code path that may return ourself if we find no upper case letters and no invalid
    // ASCII letters.

    // First scan the string for uppercase and non-ASCII characters:
    if (is8Bit()) {
        for (unsigned i = 0; i < m_length; ++i) {
            LChar character = m_data8[i];
            if (UNLIKELY((character & ~0x7F) || isASCIIUpper(character)))
                return convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(i);
        }

        return *this;
    }

    bool noUpper = true;
    unsigned ored = 0;

    for (unsigned i = 0; i < m_length; ++i) {
        UChar character = m_data16[i];
        if (UNLIKELY(isASCIIUpper(character)))
            noUpper = false;
        ored |= character;
    }
    // Nothing to do if the string is all ASCII with no uppercase.
    if (noUpper && !(ored & ~0x7F))
        return *this;

    if (!(ored & ~0x7F)) {
        UChar* data16;
        auto newImpl = createUninitializedInternalNonEmpty(m_length, data16);
        for (unsigned i = 0; i < m_length; ++i)
            data16[i] = toASCIILower(m_data16[i]);
        return newImpl;
    }

    if (m_length > static_cast<unsigned>(std::numeric_limits<int32_t>::max()))
        CRASH();
    int32_t length = m_length;

    // Do a slower implementation for cases that include non-ASCII characters.
    UChar* data16;
    auto newImpl = createUninitializedInternalNonEmpty(m_length, data16);

    UErrorCode status = U_ZERO_ERROR;
    int32_t realLength = u_strToLower(data16, length, m_data16, m_length, "", &status);
    if (U_SUCCESS(status) && realLength == length)
        return newImpl;

    newImpl = createUninitialized(realLength, data16);
    status = U_ZERO_ERROR;
    u_strToLower(data16, realLength, m_data16, m_length, "", &status);
    if (U_FAILURE(status))
        return *this;
    return newImpl;
}

Ref<StringImpl> StringImpl::convertToLowercaseWithoutLocaleStartingAtFailingIndex8Bit(unsigned failingIndex)
{
    ASSERT(is8Bit());
    LChar* data8;
    auto newImpl = createUninitializedInternalNonEmpty(m_length, data8);

    for (unsigned i = 0; i < failingIndex; ++i) {
        ASSERT(!(m_data8[i] & ~0x7F) && !isASCIIUpper(m_data8[i]));
        data8[i] = m_data8[i];
    }

    for (unsigned i = failingIndex; i < m_length; ++i) {
        LChar character = m_data8[i];
        if (!(character & ~0x7F))
            data8[i] = toASCIILower(character);
        else {
            ASSERT(u_tolower(character) <= 0xFF);
            data8[i] = static_cast<LChar>(u_tolower(character));
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

    if (m_length > static_cast<unsigned>(std::numeric_limits<int32_t>::max()))
        CRASH();
    int32_t length = m_length;

    if (is8Bit()) {
        LChar* data8;
        auto newImpl = createUninitialized(m_length, data8);
        
        // Do a faster loop for the case where all the characters are ASCII.
        unsigned ored = 0;
        for (int i = 0; i < length; ++i) {
            LChar character = m_data8[i];
            ored |= character;
            data8[i] = toASCIIUpper(character);
        }
        if (!(ored & ~0x7F))
            return newImpl;

        // Do a slower implementation for cases that include non-ASCII Latin-1 characters.
        int numberSharpSCharacters = 0;

        // There are two special cases.
        //  1. Some Latin-1 characters when converted to upper case are 16 bit characters.
        //  2. Lower case sharp-S converts to "SS" (two characters)
        for (int32_t i = 0; i < length; ++i) {
            LChar character = m_data8[i];
            if (UNLIKELY(character == smallLetterSharpS))
                ++numberSharpSCharacters;
            ASSERT(u_toupper(character) <= 0xFFFF);
            UChar upper = u_toupper(character);
            if (UNLIKELY(upper > 0xFF)) {
                // Since this upper-cased character does not fit in an 8-bit string, we need to take the 16-bit path.
                goto upconvert;
            }
            data8[i] = static_cast<LChar>(upper);
        }

        if (!numberSharpSCharacters)
            return newImpl;

        // We have numberSSCharacters sharp-s characters, but none of the other special characters.
        newImpl = createUninitialized(m_length + numberSharpSCharacters, data8);

        LChar* dest = data8;

        for (int32_t i = 0; i < length; ++i) {
            LChar character = m_data8[i];
            if (character == smallLetterSharpS) {
                *dest++ = 'S';
                *dest++ = 'S';
            } else {
                ASSERT(u_toupper(character) <= 0xFF);
                *dest++ = static_cast<LChar>(u_toupper(character));
            }
        }

        return newImpl;
    }

upconvert:
    auto upconvertedCharacters = StringView(*this).upconvertedCharacters();
    const UChar* source16 = upconvertedCharacters;

    UChar* data16;
    auto newImpl = createUninitialized(m_length, data16);
    
    // Do a faster loop for the case where all the characters are ASCII.
    unsigned ored = 0;
    for (int i = 0; i < length; ++i) {
        UChar character = source16[i];
        ored |= character;
        data16[i] = toASCIIUpper(character);
    }
    if (!(ored & ~0x7F))
        return newImpl;

    // Do a slower implementation for cases that include non-ASCII characters.
    UErrorCode status = U_ZERO_ERROR;
    int32_t realLength = u_strToUpper(data16, length, source16, m_length, "", &status);
    if (U_SUCCESS(status) && realLength == length)
        return newImpl;
    newImpl = createUninitialized(realLength, data16);
    status = U_ZERO_ERROR;
    u_strToUpper(data16, realLength, source16, m_length, "", &status);
    if (U_FAILURE(status))
        return *this;
    return newImpl;
}

static inline bool needsTurkishCasingRules(const AtomicString& localeIdentifier)
{
    // Either "tr" or "az" locale, with case sensitive comparison and allowing for an ignored subtag.
    UChar first = localeIdentifier[0];
    UChar second = localeIdentifier[1];
    return ((isASCIIAlphaCaselessEqual(first, 't') && isASCIIAlphaCaselessEqual(second, 'r'))
        || (isASCIIAlphaCaselessEqual(first, 'a') && isASCIIAlphaCaselessEqual(second, 'z')))
        && (localeIdentifier.length() == 2 || localeIdentifier[2] == '-');
}

Ref<StringImpl> StringImpl::convertToLowercaseWithLocale(const AtomicString& localeIdentifier)
{
    // Use the more-optimized code path most of the time.
    // Assuming here that the only locale-specific lowercasing is the Turkish casing rules.
    // FIXME: Could possibly optimize further by looking for the specific sequences
    // that have locale-specific lowercasing. There are only three of them.
    if (!needsTurkishCasingRules(localeIdentifier))
        return convertToLowercaseWithoutLocale();

    // FIXME: Could share more code with the main StringImpl::lower by factoring out
    // this last part into a shared function that takes a locale string, since this is
    // just like the end of that function.

    if (m_length > static_cast<unsigned>(std::numeric_limits<int32_t>::max()))
        CRASH();
    int length = m_length;

    // Below, we pass in the hardcoded locale "tr". Passing that is more efficient than
    // allocating memory just to turn localeIdentifier into a C string, and we assume
    // there is no difference between the uppercasing for "tr" and "az" locales.
    auto upconvertedCharacters = StringView(*this).upconvertedCharacters();
    const UChar* source16 = upconvertedCharacters;
    UChar* data16;
    auto newString = createUninitialized(length, data16);
    UErrorCode status = U_ZERO_ERROR;
    int realLength = u_strToLower(data16, length, source16, length, "tr", &status);
    if (U_SUCCESS(status) && realLength == length)
        return newString;
    newString = createUninitialized(realLength, data16);
    status = U_ZERO_ERROR;
    u_strToLower(data16, realLength, source16, length, "tr", &status);
    if (U_FAILURE(status))
        return *this;
    return newString;
}

Ref<StringImpl> StringImpl::convertToUppercaseWithLocale(const AtomicString& localeIdentifier)
{
    // Use the more-optimized code path most of the time.
    // Assuming here that the only locale-specific lowercasing is the Turkish casing rules,
    // and that the only affected character is lowercase "i".
    if (!needsTurkishCasingRules(localeIdentifier) || find('i') == notFound)
        return convertToUppercaseWithoutLocale();

    if (m_length > static_cast<unsigned>(std::numeric_limits<int32_t>::max()))
        CRASH();
    int length = m_length;

    // Below, we pass in the hardcoded locale "tr". Passing that is more efficient than
    // allocating memory just to turn localeIdentifier into a C string, and we assume
    // there is no difference between the uppercasing for "tr" and "az" locales.
    auto upconvertedCharacters = StringView(*this).upconvertedCharacters();
    const UChar* source16 = upconvertedCharacters;
    UChar* data16;
    auto newString = createUninitialized(length, data16);
    UErrorCode status = U_ZERO_ERROR;
    int realLength = u_strToUpper(data16, length, source16, length, "tr", &status);
    if (U_SUCCESS(status) && realLength == length)
        return newString;
    newString = createUninitialized(realLength, data16);
    status = U_ZERO_ERROR;
    u_strToUpper(data16, realLength, source16, length, "tr", &status);
    if (U_FAILURE(status))
        return *this;
    return newString;
}

Ref<StringImpl> StringImpl::foldCase()
{
    if (is8Bit()) {
        unsigned failingIndex;
        for (unsigned i = 0; i < m_length; ++i) {
            auto character = m_data8[i];
            if (UNLIKELY(!isASCII(character) || isASCIIUpper(character))) {
                failingIndex = i;
                goto SlowPath;
            }
        }
        // String was all ASCII and no uppercase, so just return as-is.
        return *this;

SlowPath:
        bool need16BitCharacters = false;
        for (unsigned i = failingIndex; i < m_length; ++i) {
            auto character = m_data8[i];
            if (character == 0xB5 || character == 0xDF) {
                need16BitCharacters = true;
                break;
            }
        }

        if (!need16BitCharacters) {
            LChar* data8;
            auto folded = createUninitializedInternalNonEmpty(m_length, data8);
            copyCharacters(data8, m_data8, failingIndex);
            for (unsigned i = failingIndex; i < m_length; ++i) {
                auto character = m_data8[i];
                if (isASCII(character))
                    data8[i] = toASCIILower(character);
                else {
                    ASSERT(u_foldCase(character, U_FOLD_CASE_DEFAULT) <= 0xFF);
                    data8[i] = static_cast<LChar>(u_foldCase(character, U_FOLD_CASE_DEFAULT));
                }
            }
            return folded;
        }
    } else {
        // FIXME: Unclear why we use goto in the 8-bit case, and a different approach in the 16-bit case.
        bool noUpper = true;
        unsigned ored = 0;
        for (unsigned i = 0; i < m_length; ++i) {
            UChar character = m_data16[i];
            if (UNLIKELY(isASCIIUpper(character)))
                noUpper = false;
            ored |= character;
        }
        if (!(ored & ~0x7F)) {
            if (noUpper) {
                // String was all ASCII and no uppercase, so just return as-is.
                return *this;
            }
            UChar* data16;
            auto folded = createUninitializedInternalNonEmpty(m_length, data16);
            for (unsigned i = 0; i < m_length; ++i)
                data16[i] = toASCIILower(m_data16[i]);
            return folded;
        }
    }

    if (m_length > static_cast<unsigned>(std::numeric_limits<int32_t>::max()))
        CRASH();

    auto upconvertedCharacters = StringView(*this).upconvertedCharacters();

    UChar* data;
    auto folded = createUninitializedInternalNonEmpty(m_length, data);
    int32_t length = m_length;
    UErrorCode status = U_ZERO_ERROR;
    int32_t realLength = u_strFoldCase(data, length, upconvertedCharacters, length, U_FOLD_CASE_DEFAULT, &status);
    if (U_SUCCESS(status) && realLength == length)
        return folded;
    ASSERT(realLength > length);
    folded = createUninitializedInternalNonEmpty(realLength, data);
    status = U_ZERO_ERROR;
    u_strFoldCase(data, realLength, upconvertedCharacters, length, U_FOLD_CASE_DEFAULT, &status);
    if (U_FAILURE(status))
        return *this;
    return folded;
}

template<StringImpl::CaseConvertType type, typename CharacterType>
ALWAYS_INLINE Ref<StringImpl> StringImpl::convertASCIICase(StringImpl& impl, const CharacterType* data, unsigned length)
{
    unsigned failingIndex;
    for (unsigned i = 0; i < length; ++i) {
        CharacterType character = data[i];
        if (type == CaseConvertType::Lower ? UNLIKELY(isASCIIUpper(character)) : LIKELY(isASCIILower(character))) {
            failingIndex = i;
            goto SlowPath;
        }
    }
    return impl;

SlowPath:
    CharacterType* newData;
    auto newImpl = createUninitializedInternalNonEmpty(length, newData);
    copyCharacters(newData, data, failingIndex);
    for (unsigned i = failingIndex; i < length; ++i)
        newData[i] = type == CaseConvertType::Lower ? toASCIILower(data[i]) : toASCIIUpper(data[i]);
    return newImpl;
}

Ref<StringImpl> StringImpl::convertToASCIILowercase()
{
    if (is8Bit())
        return convertASCIICase<CaseConvertType::Lower>(*this, m_data8, m_length);
    return convertASCIICase<CaseConvertType::Lower>(*this, m_data16, m_length);
}

Ref<StringImpl> StringImpl::convertToASCIIUppercase()
{
    if (is8Bit())
        return convertASCIICase<CaseConvertType::Upper>(*this, m_data8, m_length);
    return convertASCIICase<CaseConvertType::Upper>(*this, m_data16, m_length);
}

template<typename CodeUnitPredicate> inline Ref<StringImpl> StringImpl::stripMatchedCharacters(CodeUnitPredicate predicate)
{
    if (!m_length)
        return *this;

    unsigned start = 0;
    unsigned end = m_length - 1;
    
    // skip white space from start
    while (start <= end && predicate(is8Bit() ? m_data8[start] : m_data16[start]))
        ++start;
    
    // only white space
    if (start > end) 
        return *empty();

    // skip white space from end
    while (end && predicate(is8Bit() ? m_data8[end] : m_data16[end]))
        --end;

    if (!start && end == m_length - 1)
        return *this;
    if (is8Bit())
        return create(m_data8 + start, end + 1 - start);
    return create(m_data16 + start, end + 1 - start);
}

Ref<StringImpl> StringImpl::stripWhiteSpace()
{
    return stripMatchedCharacters(isSpaceOrNewline);
}

Ref<StringImpl> StringImpl::stripLeadingAndTrailingCharacters(CodeUnitMatchFunction predicate)
{
    return stripMatchedCharacters(predicate);
}

template<typename CharacterType> ALWAYS_INLINE Ref<StringImpl> StringImpl::removeCharacters(const CharacterType* characters, CodeUnitMatchFunction findMatch)
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

    if (outc)
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

Ref<StringImpl> StringImpl::removeCharacters(CodeUnitMatchFunction findMatch)
{
    if (is8Bit())
        return removeCharacters(characters8(), findMatch);
    return removeCharacters(characters16(), findMatch);
}

template<typename CharacterType, class UCharPredicate> inline Ref<StringImpl> StringImpl::simplifyMatchedCharactersToSpace(UCharPredicate predicate)
{
    StringBuffer<CharacterType> data(m_length);

    auto* from = characters<CharacterType>();
    auto* fromEnd = from + m_length;
    unsigned outc = 0;
    bool changedToSpace = false;
    
    auto* to = data.characters();
    
    while (true) {
        while (from != fromEnd && predicate(*from)) {
            if (*from != ' ')
                changedToSpace = true;
            ++from;
        }
        while (from != fromEnd && !predicate(*from))
            to[outc++] = *from++;
        if (from != fromEnd)
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

Ref<StringImpl> StringImpl::simplifyWhiteSpace()
{
    if (is8Bit())
        return StringImpl::simplifyMatchedCharactersToSpace<LChar>(isSpaceOrNewline);
    return StringImpl::simplifyMatchedCharactersToSpace<UChar>(isSpaceOrNewline);
}

Ref<StringImpl> StringImpl::simplifyWhiteSpace(CodeUnitMatchFunction isWhiteSpace)
{
    if (is8Bit())
        return StringImpl::simplifyMatchedCharactersToSpace<LChar>(isWhiteSpace);
    return StringImpl::simplifyMatchedCharactersToSpace<UChar>(isWhiteSpace);
}

int StringImpl::toIntStrict(bool* ok, int base)
{
    if (is8Bit())
        return charactersToIntStrict(characters8(), m_length, ok, base);
    return charactersToIntStrict(characters16(), m_length, ok, base);
}

unsigned StringImpl::toUIntStrict(bool* ok, int base)
{
    if (is8Bit())
        return charactersToUIntStrict(characters8(), m_length, ok, base);
    return charactersToUIntStrict(characters16(), m_length, ok, base);
}

int64_t StringImpl::toInt64Strict(bool* ok, int base)
{
    if (is8Bit())
        return charactersToInt64Strict(characters8(), m_length, ok, base);
    return charactersToInt64Strict(characters16(), m_length, ok, base);
}

uint64_t StringImpl::toUInt64Strict(bool* ok, int base)
{
    if (is8Bit())
        return charactersToUInt64Strict(characters8(), m_length, ok, base);
    return charactersToUInt64Strict(characters16(), m_length, ok, base);
}

intptr_t StringImpl::toIntPtrStrict(bool* ok, int base)
{
    if (is8Bit())
        return charactersToIntPtrStrict(characters8(), m_length, ok, base);
    return charactersToIntPtrStrict(characters16(), m_length, ok, base);
}

int StringImpl::toInt(bool* ok)
{
    if (is8Bit())
        return charactersToInt(characters8(), m_length, ok);
    return charactersToInt(characters16(), m_length, ok);
}

unsigned StringImpl::toUInt(bool* ok)
{
    if (is8Bit())
        return charactersToUInt(characters8(), m_length, ok);
    return charactersToUInt(characters16(), m_length, ok);
}

int64_t StringImpl::toInt64(bool* ok)
{
    if (is8Bit())
        return charactersToInt64(characters8(), m_length, ok);
    return charactersToInt64(characters16(), m_length, ok);
}

uint64_t StringImpl::toUInt64(bool* ok)
{
    if (is8Bit())
        return charactersToUInt64(characters8(), m_length, ok);
    return charactersToUInt64(characters16(), m_length, ok);
}

intptr_t StringImpl::toIntPtr(bool* ok)
{
    if (is8Bit())
        return charactersToIntPtr(characters8(), m_length, ok);
    return charactersToIntPtr(characters16(), m_length, ok);
}

double StringImpl::toDouble(bool* ok)
{
    if (is8Bit())
        return charactersToDouble(characters8(), m_length, ok);
    return charactersToDouble(characters16(), m_length, ok);
}

float StringImpl::toFloat(bool* ok)
{
    if (is8Bit())
        return charactersToFloat(characters8(), m_length, ok);
    return charactersToFloat(characters16(), m_length, ok);
}

size_t StringImpl::find(CodeUnitMatchFunction matchFunction, unsigned start)
{
    if (is8Bit())
        return WTF::find(characters8(), m_length, matchFunction, start);
    return WTF::find(characters16(), m_length, matchFunction, start);
}

size_t StringImpl::find(const LChar* matchString, unsigned index)
{
    // Check for null or empty string to match against
    if (!matchString)
        return notFound;
    size_t matchStringLength = strlen(reinterpret_cast<const char*>(matchString));
    if (matchStringLength > std::numeric_limits<unsigned>::max())
        CRASH();
    unsigned matchLength = matchStringLength;
    if (!matchLength)
        return std::min(index, length());

    // Optimization 1: fast case for strings of length 1.
    if (matchLength == 1) {
        if (is8Bit())
            return WTF::find(characters8(), length(), matchString[0], index);
        return WTF::find(characters16(), length(), *matchString, index);
    }

    // Check index & matchLength are in range.
    if (index > length())
        return notFound;
    unsigned searchLength = length() - index;
    if (matchLength > searchLength)
        return notFound;
    // delta is the number of additional times to test; delta == 0 means test only once.
    unsigned delta = searchLength - matchLength;

    // Optimization 2: keep a running hash of the strings,
    // only call equal if the hashes match.

    if (is8Bit()) {
        const LChar* searchCharacters = characters8() + index;

        unsigned searchHash = 0;
        unsigned matchHash = 0;
        for (unsigned i = 0; i < matchLength; ++i) {
            searchHash += searchCharacters[i];
            matchHash += matchString[i];
        }

        unsigned i = 0;
        while (searchHash != matchHash || !equal(searchCharacters + i, matchString, matchLength)) {
            if (i == delta)
                return notFound;
            searchHash += searchCharacters[i + matchLength];
            searchHash -= searchCharacters[i];
            ++i;
        }
        return index + i;
    }

    const UChar* searchCharacters = characters16() + index;

    unsigned searchHash = 0;
    unsigned matchHash = 0;
    for (unsigned i = 0; i < matchLength; ++i) {
        searchHash += searchCharacters[i];
        matchHash += matchString[i];
    }

    unsigned i = 0;
    while (searchHash != matchHash || !equal(searchCharacters + i, matchString, matchLength)) {
        if (i == delta)
            return notFound;
        searchHash += searchCharacters[i + matchLength];
        searchHash -= searchCharacters[i];
        ++i;
    }
    return index + i;
}

size_t StringImpl::find(StringImpl* matchString)
{
    // Check for null string to match against
    if (UNLIKELY(!matchString))
        return notFound;
    unsigned matchLength = matchString->length();

    // Optimization 1: fast case for strings of length 1.
    if (matchLength == 1) {
        if (is8Bit()) {
            if (matchString->is8Bit())
                return WTF::find(characters8(), length(), matchString->characters8()[0]);
            return WTF::find(characters8(), length(), matchString->characters16()[0]);
        }
        if (matchString->is8Bit())
            return WTF::find(characters16(), length(), matchString->characters8()[0]);
        return WTF::find(characters16(), length(), matchString->characters16()[0]);
    }

    // Check matchLength is in range.
    if (matchLength > length())
        return notFound;

    // Check for empty string to match against
    if (UNLIKELY(!matchLength))
        return 0;

    if (is8Bit()) {
        if (matchString->is8Bit())
            return findInner(characters8(), matchString->characters8(), 0, length(), matchLength);
        return findInner(characters8(), matchString->characters16(), 0, length(), matchLength);
    }

    if (matchString->is8Bit())
        return findInner(characters16(), matchString->characters8(), 0, length(), matchLength);

    return findInner(characters16(), matchString->characters16(), 0, length(), matchLength);
}

size_t StringImpl::find(StringImpl* matchString, unsigned index)
{
    // Check for null or empty string to match against
    if (UNLIKELY(!matchString))
        return notFound;

    return findCommon(*this, *matchString, index);
}

size_t StringImpl::findIgnoringASCIICase(const StringImpl& matchString) const
{
    return ::WTF::findIgnoringASCIICase(*this, matchString, 0);
}

size_t StringImpl::findIgnoringASCIICase(const StringImpl& matchString, unsigned startOffset) const
{
    return ::WTF::findIgnoringASCIICase(*this, matchString, startOffset);
}

size_t StringImpl::findIgnoringASCIICase(const StringImpl* matchString) const
{
    if (!matchString)
        return notFound;
    return ::WTF::findIgnoringASCIICase(*this, *matchString, 0);
}

size_t StringImpl::findIgnoringASCIICase(const StringImpl* matchString, unsigned startOffset) const
{
    if (!matchString)
        return notFound;
    return ::WTF::findIgnoringASCIICase(*this, *matchString, startOffset);
}

size_t StringImpl::reverseFind(UChar character, unsigned index)
{
    if (is8Bit())
        return WTF::reverseFind(characters8(), m_length, character, index);
    return WTF::reverseFind(characters16(), m_length, character, index);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t reverseFindInner(const SearchCharacterType* searchCharacters, const MatchCharacterType* matchCharacters, unsigned index, unsigned length, unsigned matchLength)
{
    // Optimization: keep a running hash of the strings,
    // only call equal if the hashes match.

    // delta is the number of additional times to test; delta == 0 means test only once.
    unsigned delta = std::min(index, length - matchLength);
    
    unsigned searchHash = 0;
    unsigned matchHash = 0;
    for (unsigned i = 0; i < matchLength; ++i) {
        searchHash += searchCharacters[delta + i];
        matchHash += matchCharacters[i];
    }

    // keep looping until we match
    while (searchHash != matchHash || !equal(searchCharacters + delta, matchCharacters, matchLength)) {
        if (!delta)
            return notFound;
        --delta;
        searchHash -= searchCharacters[delta + matchLength];
        searchHash += searchCharacters[delta];
    }
    return delta;
}

size_t StringImpl::reverseFind(StringImpl* matchString, unsigned index)
{
    // Check for null or empty string to match against
    if (!matchString)
        return notFound;
    unsigned matchLength = matchString->length();
    unsigned ourLength = length();
    if (!matchLength)
        return std::min(index, ourLength);

    // Optimization 1: fast case for strings of length 1.
    if (matchLength == 1) {
        if (is8Bit())
            return WTF::reverseFind(characters8(), ourLength, (*matchString)[0], index);
        return WTF::reverseFind(characters16(), ourLength, (*matchString)[0], index);
    }

    // Check index & matchLength are in range.
    if (matchLength > ourLength)
        return notFound;

    if (is8Bit()) {
        if (matchString->is8Bit())
            return reverseFindInner(characters8(), matchString->characters8(), index, ourLength, matchLength);
        return reverseFindInner(characters8(), matchString->characters16(), index, ourLength, matchLength);
    }
    
    if (matchString->is8Bit())
        return reverseFindInner(characters16(), matchString->characters8(), index, ourLength, matchLength);

    return reverseFindInner(characters16(), matchString->characters16(), index, ourLength, matchLength);
}

ALWAYS_INLINE static bool equalInner(const StringImpl& string, unsigned startOffset, const char* matchString, unsigned matchLength)
{
    ASSERT(matchLength <= string.length());
    ASSERT(startOffset + matchLength <= string.length());

    if (string.is8Bit())
        return equal(string.characters8() + startOffset, reinterpret_cast<const LChar*>(matchString), matchLength);
    return equal(string.characters16() + startOffset, reinterpret_cast<const LChar*>(matchString), matchLength);
}

ALWAYS_INLINE static bool equalInner(const StringImpl& string, unsigned startOffset, const StringImpl& matchString)
{
    if (startOffset > string.length())
        return false;
    if (matchString.length() > string.length())
        return false;
    if (matchString.length() + startOffset > string.length())
        return false;

    if (string.is8Bit()) {
        if (matchString.is8Bit())
            return equal(string.characters8() + startOffset, matchString.characters8(), matchString.length());
        return equal(string.characters8() + startOffset, matchString.characters16(), matchString.length());
    }
    if (matchString.is8Bit())
        return equal(string.characters16() + startOffset, matchString.characters8(), matchString.length());
    return equal(string.characters16() + startOffset, matchString.characters16(), matchString.length());
}

bool StringImpl::startsWith(const StringImpl* string) const
{
    return string && ::WTF::startsWith(*this, *string);
}

bool StringImpl::startsWith(const StringImpl& string) const
{
    return ::WTF::startsWith(*this, string);
}

bool StringImpl::startsWithIgnoringASCIICase(const StringImpl* prefix) const
{
    return prefix && ::WTF::startsWithIgnoringASCIICase(*this, *prefix);
}

bool StringImpl::startsWithIgnoringASCIICase(const StringImpl& prefix) const
{
    return ::WTF::startsWithIgnoringASCIICase(*this, prefix);
}

bool StringImpl::startsWith(UChar character) const
{
    return m_length && (*this)[0] == character;
}

bool StringImpl::startsWith(const char* matchString, unsigned matchLength) const
{
    return matchLength <= length() && equalInner(*this, 0, matchString, matchLength);
}

bool StringImpl::hasInfixStartingAt(const StringImpl& matchString, unsigned startOffset) const
{
    return equalInner(*this, startOffset, matchString);
}

bool StringImpl::endsWith(StringImpl* suffix)
{
    return suffix && ::WTF::endsWith(*this, *suffix);
}

bool StringImpl::endsWith(StringImpl& suffix)
{
    return ::WTF::endsWith(*this, suffix);
}

bool StringImpl::endsWithIgnoringASCIICase(const StringImpl* suffix) const
{
    return suffix && ::WTF::endsWithIgnoringASCIICase(*this, *suffix);
}

bool StringImpl::endsWithIgnoringASCIICase(const StringImpl& suffix) const
{
    return ::WTF::endsWithIgnoringASCIICase(*this, suffix);
}

bool StringImpl::endsWith(UChar character) const
{
    return m_length && (*this)[m_length - 1] == character;
}

bool StringImpl::endsWith(const char* matchString, unsigned matchLength) const
{
    return matchLength <= length() && equalInner(*this, length() - matchLength, matchString, matchLength);
}

bool StringImpl::hasInfixEndingAt(const StringImpl& matchString, unsigned endOffset) const
{
    return endOffset >= matchString.length() && equalInner(*this, endOffset - matchString.length(), matchString);
}

Ref<StringImpl> StringImpl::replace(UChar target, UChar replacement)
{
    if (target == replacement)
        return *this;
    unsigned i;
    for (i = 0; i != m_length; ++i) {
        UChar character = is8Bit() ? m_data8[i] : m_data16[i];
        if (character == target)
            break;
    }
    if (i == m_length)
        return *this;

    if (is8Bit()) {
        if (target > 0xFF) {
            // Looking for a 16-bit character in an 8-bit string, so we're done.
            return *this;
        }

        if (replacement <= 0xFF) {
            LChar* data;
            LChar oldChar = static_cast<LChar>(target);
            LChar newChar = static_cast<LChar>(replacement);

            auto newImpl = createUninitializedInternalNonEmpty(m_length, data);

            for (i = 0; i != m_length; ++i) {
                LChar character = m_data8[i];
                if (character == oldChar)
                    character = newChar;
                data[i] = character;
            }
            return newImpl;
        }

        UChar* data;
        auto newImpl = createUninitializedInternalNonEmpty(m_length, data);

        for (i = 0; i != m_length; ++i) {
            UChar character = m_data8[i];
            if (character == target)
                character = replacement;
            data[i] = character;
        }

        return newImpl;
    }

    UChar* data;
    auto newImpl = createUninitializedInternalNonEmpty(m_length, data);

    for (i = 0; i != m_length; ++i) {
        UChar character = m_data16[i];
        if (character == target)
            character = replacement;
        data[i] = character;
    }
    return newImpl;
}

Ref<StringImpl> StringImpl::replace(unsigned position, unsigned lengthToReplace, StringImpl* string)
{
    position = std::min(position, length());
    lengthToReplace = std::min(lengthToReplace, length() - position);
    unsigned lengthToInsert = string ? string->length() : 0;
    if (!lengthToReplace && !lengthToInsert)
        return *this;

    if ((length() - lengthToReplace) >= (std::numeric_limits<unsigned>::max() - lengthToInsert))
        CRASH();

    if (is8Bit() && (!string || string->is8Bit())) {
        LChar* data;
        auto newImpl = createUninitialized(length() - lengthToReplace + lengthToInsert, data);
        copyCharacters(data, m_data8, position);
        if (string)
            copyCharacters(data + position, string->m_data8, lengthToInsert);
        copyCharacters(data + position + lengthToInsert, m_data8 + position + lengthToReplace, length() - position - lengthToReplace);
        return newImpl;
    }
    UChar* data;
    auto newImpl = createUninitialized(length() - lengthToReplace + lengthToInsert, data);
    if (is8Bit())
        copyCharacters(data, m_data8, position);
    else
        copyCharacters(data, m_data16, position);
    if (string) {
        if (string->is8Bit())
            copyCharacters(data + position, string->m_data8, lengthToInsert);
        else
            copyCharacters(data + position, string->m_data16, lengthToInsert);
    }
    if (is8Bit())
        copyCharacters(data + position + lengthToInsert, m_data8 + position + lengthToReplace, length() - position - lengthToReplace);
    else
        copyCharacters(data + position + lengthToInsert, m_data16 + position + lengthToReplace, length() - position - lengthToReplace);
    return newImpl;
}

Ref<StringImpl> StringImpl::replace(UChar pattern, StringImpl* replacement)
{
    if (!replacement)
        return *this;
    if (replacement->is8Bit())
        return replace(pattern, replacement->m_data8, replacement->length());
    return replace(pattern, replacement->m_data16, replacement->length());
}

Ref<StringImpl> StringImpl::replace(UChar pattern, const LChar* replacement, unsigned repStrLength)
{
    ASSERT(replacement);

    size_t srcSegmentStart = 0;
    unsigned matchCount = 0;

    // Count the matches.
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) != notFound) {
        ++matchCount;
        ++srcSegmentStart;
    }

    // If we have 0 matches then we don't have to do any more work.
    if (!matchCount)
        return *this;

    if (repStrLength && matchCount > std::numeric_limits<unsigned>::max() / repStrLength)
        CRASH();

    unsigned replaceSize = matchCount * repStrLength;
    unsigned newSize = m_length - matchCount;
    if (newSize >= (std::numeric_limits<unsigned>::max() - replaceSize))
        CRASH();

    newSize += replaceSize;

    // Construct the new data.
    size_t srcSegmentEnd;
    unsigned srcSegmentLength;
    srcSegmentStart = 0;
    unsigned dstOffset = 0;

    if (is8Bit()) {
        LChar* data;
        auto newImpl = createUninitialized(newSize, data);

        while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
            srcSegmentLength = srcSegmentEnd - srcSegmentStart;
            copyCharacters(data + dstOffset, m_data8 + srcSegmentStart, srcSegmentLength);
            dstOffset += srcSegmentLength;
            copyCharacters(data + dstOffset, replacement, repStrLength);
            dstOffset += repStrLength;
            srcSegmentStart = srcSegmentEnd + 1;
        }

        srcSegmentLength = m_length - srcSegmentStart;
        copyCharacters(data + dstOffset, m_data8 + srcSegmentStart, srcSegmentLength);

        ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

        return newImpl;
    }

    UChar* data;
    auto newImpl = createUninitialized(newSize, data);

    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        copyCharacters(data + dstOffset, m_data16 + srcSegmentStart, srcSegmentLength);

        dstOffset += srcSegmentLength;
        copyCharacters(data + dstOffset, replacement, repStrLength);

        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + 1;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    copyCharacters(data + dstOffset, m_data16 + srcSegmentStart, srcSegmentLength);

    ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

    return newImpl;
}

Ref<StringImpl> StringImpl::replace(UChar pattern, const UChar* replacement, unsigned repStrLength)
{
    ASSERT(replacement);

    size_t srcSegmentStart = 0;
    unsigned matchCount = 0;

    // Count the matches.
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) != notFound) {
        ++matchCount;
        ++srcSegmentStart;
    }

    // If we have 0 matches then we don't have to do any more work.
    if (!matchCount)
        return *this;

    if (repStrLength && matchCount > std::numeric_limits<unsigned>::max() / repStrLength)
        CRASH();

    unsigned replaceSize = matchCount * repStrLength;
    unsigned newSize = m_length - matchCount;
    if (newSize >= (std::numeric_limits<unsigned>::max() - replaceSize))
        CRASH();

    newSize += replaceSize;

    // Construct the new data.
    size_t srcSegmentEnd;
    unsigned srcSegmentLength;
    srcSegmentStart = 0;
    unsigned dstOffset = 0;

    if (is8Bit()) {
        UChar* data;
        auto newImpl = createUninitialized(newSize, data);

        while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
            srcSegmentLength = srcSegmentEnd - srcSegmentStart;
            copyCharacters(data + dstOffset, m_data8 + srcSegmentStart, srcSegmentLength);

            dstOffset += srcSegmentLength;
            copyCharacters(data + dstOffset, replacement, repStrLength);

            dstOffset += repStrLength;
            srcSegmentStart = srcSegmentEnd + 1;
        }

        srcSegmentLength = m_length - srcSegmentStart;
        copyCharacters(data + dstOffset, m_data8 + srcSegmentStart, srcSegmentLength);

        ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

        return newImpl;
    }

    UChar* data;
    auto newImpl = createUninitialized(newSize, data);

    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        copyCharacters(data + dstOffset, m_data16 + srcSegmentStart, srcSegmentLength);

        dstOffset += srcSegmentLength;
        copyCharacters(data + dstOffset, replacement, repStrLength);

        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + 1;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    copyCharacters(data + dstOffset, m_data16 + srcSegmentStart, srcSegmentLength);

    ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

    return newImpl;
}

Ref<StringImpl> StringImpl::replace(StringImpl* pattern, StringImpl* replacement)
{
    if (!pattern || !replacement)
        return *this;

    unsigned patternLength = pattern->length();
    if (!patternLength)
        return *this;
        
    unsigned repStrLength = replacement->length();
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
    if (repStrLength && matchCount > std::numeric_limits<unsigned>::max() / repStrLength)
        CRASH();

    if (newSize > (std::numeric_limits<unsigned>::max() - matchCount * repStrLength))
        CRASH();

    newSize += matchCount * repStrLength;

    
    // Construct the new data
    size_t srcSegmentEnd;
    unsigned srcSegmentLength;
    srcSegmentStart = 0;
    unsigned dstOffset = 0;
    bool srcIs8Bit = is8Bit();
    bool replacementIs8Bit = replacement->is8Bit();
    
    // There are 4 cases:
    // 1. This and replacement are both 8 bit.
    // 2. This and replacement are both 16 bit.
    // 3. This is 8 bit and replacement is 16 bit.
    // 4. This is 16 bit and replacement is 8 bit.
    if (srcIs8Bit && replacementIs8Bit) {
        // Case 1
        LChar* data;
        auto newImpl = createUninitialized(newSize, data);
        while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
            srcSegmentLength = srcSegmentEnd - srcSegmentStart;
            copyCharacters(data + dstOffset, m_data8 + srcSegmentStart, srcSegmentLength);
            dstOffset += srcSegmentLength;
            copyCharacters(data + dstOffset, replacement->m_data8, repStrLength);
            dstOffset += repStrLength;
            srcSegmentStart = srcSegmentEnd + patternLength;
        }

        srcSegmentLength = m_length - srcSegmentStart;
        copyCharacters(data + dstOffset, m_data8 + srcSegmentStart, srcSegmentLength);

        ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

        return newImpl;
    }

    UChar* data;
    auto newImpl = createUninitialized(newSize, data);
    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) != notFound) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        if (srcIs8Bit) {
            // Case 3.
            copyCharacters(data + dstOffset, m_data8 + srcSegmentStart, srcSegmentLength);
        } else {
            // Case 2 & 4.
            copyCharacters(data + dstOffset, m_data16 + srcSegmentStart, srcSegmentLength);
        }
        dstOffset += srcSegmentLength;
        if (replacementIs8Bit) {
            // Cases 2 & 3.
            copyCharacters(data + dstOffset, replacement->m_data8, repStrLength);
        } else {
            // Case 4
            copyCharacters(data + dstOffset, replacement->m_data16, repStrLength);
        }
        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + patternLength;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    if (srcIs8Bit) {
        // Case 3.
        copyCharacters(data + dstOffset, m_data8 + srcSegmentStart, srcSegmentLength);
    } else {
        // Cases 2 & 4.
        copyCharacters(data + dstOffset, m_data16 + srcSegmentStart, srcSegmentLength);
    }

    ASSERT(dstOffset + srcSegmentLength == newImpl.get().length());

    return newImpl;
}

bool equal(const StringImpl* a, const StringImpl* b)
{
    return equalCommon(a, b);
}

template<typename CharacterType> inline bool equalInternal(const StringImpl* a, const CharacterType* b, unsigned length)
{
    if (!a)
        return !b;
    if (!b)
        return false;

    if (a->length() != length)
        return false;
    if (a->is8Bit())
        return equal(a->characters8(), b, length);
    return equal(a->characters16(), b, length);
}

bool equal(const StringImpl* a, const LChar* b, unsigned length)
{
    return equalInternal(a, b, length);
}

bool equal(const StringImpl* a, const UChar* b, unsigned length)
{
    return equalInternal(a, b, length);
}

bool equal(const StringImpl* a, const LChar* b)
{
    if (!a)
        return !b;
    if (!b)
        return !a;

    unsigned length = a->length();

    if (a->is8Bit()) {
        const LChar* aPtr = a->characters8();
        for (unsigned i = 0; i != length; ++i) {
            LChar bc = b[i];
            LChar ac = aPtr[i];
            if (!bc)
                return false;
            if (ac != bc)
                return false;
        }

        return !b[length];
    }

    const UChar* aPtr = a->characters16();
    for (unsigned i = 0; i != length; ++i) {
        LChar bc = b[i];
        if (!bc)
            return false;
        if (aPtr[i] != bc)
            return false;
    }

    return !b[length];
}

bool equal(const StringImpl& a, const StringImpl& b)
{
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

UCharDirection StringImpl::defaultWritingDirection(bool* hasStrongDirectionality)
{
    for (unsigned i = 0; i < m_length; ++i) {
        auto charDirection = u_charDirection(is8Bit() ? m_data8[i] : m_data16[i]);
        if (charDirection == U_LEFT_TO_RIGHT) {
            if (hasStrongDirectionality)
                *hasStrongDirectionality = true;
            return U_LEFT_TO_RIGHT;
        }
        if (charDirection == U_RIGHT_TO_LEFT || charDirection == U_RIGHT_TO_LEFT_ARABIC) {
            if (hasStrongDirectionality)
                *hasStrongDirectionality = true;
            return U_RIGHT_TO_LEFT;
        }
    }
    if (hasStrongDirectionality)
        *hasStrongDirectionality = false;
    return U_LEFT_TO_RIGHT;
}

Ref<StringImpl> StringImpl::adopt(StringBuffer<LChar>&& buffer)
{
    unsigned length = buffer.length();
    if (!length)
        return *empty();
    return adoptRef(*new StringImpl(buffer.release(), length));
}

Ref<StringImpl> StringImpl::adopt(StringBuffer<UChar>&& buffer)
{
    unsigned length = buffer.length();
    if (!length)
        return *empty();
    return adoptRef(*new StringImpl(buffer.release(), length));
}

size_t StringImpl::sizeInBytes() const
{
    // FIXME: support substrings
    size_t size = length();
    if (!is8Bit())
        size *= 2;
    return size + sizeof(*this);
}

// Helper to write a three-byte UTF-8 code point into the buffer; caller must ensure room is available.
static inline void putUTF8Triple(char*& buffer, UChar character)
{
    ASSERT(character >= 0x0800);
    *buffer++ = static_cast<char>(((character >> 12) & 0x0F) | 0xE0);
    *buffer++ = static_cast<char>(((character >> 6) & 0x3F) | 0x80);
    *buffer++ = static_cast<char>((character & 0x3F) | 0x80);
}

UTF8ConversionError StringImpl::utf8Impl(const UChar* characters, unsigned length, char*& buffer, size_t bufferSize, ConversionMode mode)
{
    if (mode == StrictConversionReplacingUnpairedSurrogatesWithFFFD) {
        const UChar* charactersEnd = characters + length;
        char* bufferEnd = buffer + bufferSize;
        while (characters < charactersEnd) {
            // Use strict conversion to detect unpaired surrogates.
            ConversionResult result = convertUTF16ToUTF8(&characters, charactersEnd, &buffer, bufferEnd, true);
            ASSERT(result != targetExhausted);
            // Conversion fails when there is an unpaired surrogate.
            // Put replacement character (U+FFFD) instead of the unpaired surrogate.
            if (result != conversionOK) {
                ASSERT((0xD800 <= *characters && *characters <= 0xDFFF));
                // There should be room left, since one UChar hasn't been converted.
                ASSERT((buffer + 3) <= bufferEnd);
                putUTF8Triple(buffer, replacementCharacter);
                ++characters;
            }
        }
    } else {
        bool strict = mode == StrictConversion;
        const UChar* originalCharacters = characters;
        ConversionResult result = convertUTF16ToUTF8(&characters, characters + length, &buffer, buffer + bufferSize, strict);
        ASSERT(result != targetExhausted); // (length * 3) should be sufficient for any conversion

        // Only produced from strict conversion.
        if (result == sourceIllegal) {
            ASSERT(strict);
            return UTF8ConversionError::IllegalSource;
        }

        // Check for an unconverted high surrogate.
        if (result == sourceExhausted) {
            if (strict)
                return UTF8ConversionError::SourceExhausted;
            // This should be one unpaired high surrogate. Treat it the same
            // was as an unpaired high surrogate would have been handled in
            // the middle of a string with non-strict conversion - which is
            // to say, simply encode it to UTF-8.
            ASSERT_UNUSED(
                originalCharacters, (characters + 1) == (originalCharacters + length));
            ASSERT((*characters >= 0xD800) && (*characters <= 0xDBFF));
            // There should be room left, since one UChar hasn't been converted.
            ASSERT((buffer + 3) <= (buffer + bufferSize));
            putUTF8Triple(buffer, *characters);
        }
    }
    
    return UTF8ConversionError::None;
}

Expected<CString, UTF8ConversionError> StringImpl::utf8ForCharacters(const LChar* characters, unsigned length)
{
    if (!length)
        return CString("", 0);
    if (length > std::numeric_limits<unsigned>::max() / 3)
        return makeUnexpected(UTF8ConversionError::OutOfMemory);
    Vector<char, 1024> bufferVector(length * 3);
    char* buffer = bufferVector.data();
    const LChar* source = characters;
    ConversionResult result = convertLatin1ToUTF8(&source, source + length, &buffer, buffer + bufferVector.size());
    ASSERT_UNUSED(result, result != targetExhausted); // (length * 3) should be sufficient for any conversion
    return CString(bufferVector.data(), buffer - bufferVector.data());
}

Expected<CString, UTF8ConversionError> StringImpl::utf8ForCharacters(const UChar* characters, unsigned length, ConversionMode mode)
{
    if (!length)
        return CString("", 0);
    if (length > std::numeric_limits<unsigned>::max() / 3)
        return makeUnexpected(UTF8ConversionError::OutOfMemory);
    Vector<char, 1024> bufferVector(length * 3);
    char* buffer = bufferVector.data();
    UTF8ConversionError error = utf8Impl(characters, length, buffer, bufferVector.size(), mode);
    if (error != UTF8ConversionError::None)
        return makeUnexpected(error);
    return CString(bufferVector.data(), buffer - bufferVector.data());
}

Expected<CString, UTF8ConversionError> StringImpl::tryGetUtf8ForRange(unsigned offset, unsigned length, ConversionMode mode) const
{
    ASSERT(offset <= this->length());
    ASSERT(offset + length <= this->length());
    
    if (!length)
        return CString("", 0);

    // Allocate a buffer big enough to hold all the characters
    // (an individual UTF-16 UChar can only expand to 3 UTF-8 bytes).
    // Optimization ideas, if we find this function is hot:
    //  * We could speculatively create a CStringBuffer to contain 'length' 
    //    characters, and resize if necessary (i.e. if the buffer contains
    //    non-ascii characters). (Alternatively, scan the buffer first for
    //    ascii characters, so we know this will be sufficient).
    //  * We could allocate a CStringBuffer with an appropriate size to
    //    have a good chance of being able to write the string into the
    //    buffer without reallocing (say, 1.5 x length).
    if (length > std::numeric_limits<unsigned>::max() / 3)
        return makeUnexpected(UTF8ConversionError::OutOfMemory);
    Vector<char, 1024> bufferVector(length * 3);

    char* buffer = bufferVector.data();

    if (is8Bit()) {
        const LChar* characters = this->characters8() + offset;

        ConversionResult result = convertLatin1ToUTF8(&characters, characters + length, &buffer, buffer + bufferVector.size());
        ASSERT_UNUSED(result, result != targetExhausted); // (length * 3) should be sufficient for any conversion
    } else {
        UTF8ConversionError error = utf8Impl(this->characters16() + offset, length, buffer, bufferVector.size(), mode);
        if (error != UTF8ConversionError::None)
            return makeUnexpected(error);
    }

    return CString(bufferVector.data(), buffer - bufferVector.data());
}

Expected<CString, UTF8ConversionError> StringImpl::tryGetUtf8(ConversionMode mode) const
{
    return tryGetUtf8ForRange(0, length(), mode);
}

CString StringImpl::utf8(ConversionMode mode) const
{
    auto expectedString = tryGetUtf8ForRange(0, length(), mode);
    RELEASE_ASSERT(expectedString);
    return expectedString.value();
}

NEVER_INLINE unsigned StringImpl::hashSlowCase() const
{
    if (is8Bit())
        setHash(StringHasher::computeHashAndMaskTop8Bits(m_data8, m_length));
    else
        setHash(StringHasher::computeHashAndMaskTop8Bits(m_data16, m_length));
    return existingHash();
}

unsigned StringImpl::concurrentHash() const
{
    unsigned hash;
    if (is8Bit())
        hash = StringHasher::computeHashAndMaskTop8Bits(m_data8, m_length);
    else
        hash = StringHasher::computeHashAndMaskTop8Bits(m_data16, m_length);
    ASSERT(((hash << s_flagCount) >> s_flagCount) == hash);
    return hash;
}

bool equalIgnoringNullity(const UChar* a, size_t aLength, StringImpl* b)
{
    if (!b)
        return !aLength;
    if (aLength != b->length())
        return false;
    if (b->is8Bit()) {
        const LChar* bCharacters = b->characters8();
        for (unsigned i = 0; i < aLength; ++i) {
            if (a[i] != bCharacters[i])
                return false;
        }
        return true;
    }
    return !memcmp(a, b->characters16(), b->length() * sizeof(UChar));
}

} // namespace WTF
