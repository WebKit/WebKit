/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TextCodecCJK.h"

#include "EncodingTables.h"
#include "TextCodecICU.h"
#include <wtf/text/CodePointIterator.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

TextCodecCJK::TextCodecCJK(Encoding encoding)
    : m_encoding(encoding)
{
    if (encoding == Encoding::EUC_JP)
        m_icuCodec = makeUnique<TextCodecICU>("euc-jp-2007", "euc-jp-2007");
}

void TextCodecCJK::registerEncodingNames(EncodingNameRegistrar registrar)
{
    // https://encoding.spec.whatwg.org/#names-and-labels
    static const std::array<const char*, 5> big5Aliases {
        "Big5",
        "big5-hkscs",
        "cn-big5",
        "csbig5",
        "x-x-big5"
    };
    for (auto* alias : big5Aliases)
        registrar(alias, "Big5");

    static const std::array<const char*, 3> eucJPAliases {
        "EUC-JP",
        "cseucpkdfmtjapanese",
        "x-euc-jp"
    };
    for (auto* alias : eucJPAliases)
        registrar(alias, "EUC-JP");
}

void TextCodecCJK::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("euc-jp", [] {
        return makeUnique<TextCodecCJK>(Encoding::EUC_JP);
    });
    registrar("big5", [] {
        return makeUnique<TextCodecCJK>(Encoding::Big5);
    });
}

// https://encoding.spec.whatwg.org/#euc-jp-encoder
static Vector<uint8_t> eucJPEncode(StringView string, Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(string.length());

    auto characters = string.upconvertedCharacters();
    for (WTF::CodePointIterator<UChar> iterator(characters.get(), characters.get() + string.length()); !iterator.atEnd(); ++iterator) {
        auto c = *iterator;
        if (isASCII(c)) {
            result.append(c);
            continue;
        }
        if (c == 0x00A5) {
            result.append(0x5C);
            continue;
        }
        if (c == 0x203E) {
            result.append(0x7E);
            continue;
        }
        if (c >= 0xFF61 && c <= 0xFF9F) {
            result.append(0x8E);
            result.append(c - 0xFF61 + 0xA1);
            continue;
        }
        if (c == 0x2212)
            c = 0xFF0D;

        if (static_cast<UChar>(c) != c) {
            unencodableHandler(c, result);
            continue;
        }
        
        auto pointerRange = std::equal_range(std::begin(jis0208), std::end(jis0208), std::pair<UChar, UChar>(static_cast<UChar>(c), 0), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        if (pointerRange.first == pointerRange.second) {
            unencodableHandler(c, result);
            continue;
        }
        UChar pointer = (pointerRange.second - 1)->second;
        result.append(pointer / 94 + 0xA1);
        result.append(pointer % 94 + 0xA1);
    }
    return result;
}

// https://encoding.spec.whatwg.org/#big5-encoder
static Vector<uint8_t> big5Encode(StringView string, Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(string.length());

    auto characters = string.upconvertedCharacters();
    for (WTF::CodePointIterator<UChar> iterator(characters.get(), characters.get() + string.length()); !iterator.atEnd(); ++iterator) {
        auto c = *iterator;
        if (isASCII(c)) {
            result.append(c);
            continue;
        }

        auto pointerRange = std::equal_range(std::begin(big5EncodingMap), std::end(big5EncodingMap), std::pair<UChar32, UChar>(c, 0), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        
        if (pointerRange.first == pointerRange.second) {
            unencodableHandler(c, result);
            continue;
        }

        UChar pointer = 0;
        if (c == 0x2550 || c == 0x255E || c == 0x2561 || c == 0x256A || c == 0x5341 || c == 0x5345)
            pointer = (pointerRange.second - 1)->second;
        else
            pointer = pointerRange.first->second;
        
        uint8_t lead = pointer / 157 + 0x81;
        uint8_t trail = pointer % 157;
        uint8_t offset = trail < 0x3F ? 0x40 : 0x62;
        result.append(lead);
        result.append(trail + offset);
    }
    return result;
}

constexpr size_t maxUChar32Digits = 10;

static void appendDecimal(UChar32 c, Vector<uint8_t>& result)
{
    uint8_t buffer[10];
    WTF::writeIntegerToBuffer(static_cast<uint32_t>(c), buffer);
    result.append(buffer, WTF::lengthOfIntegerAsString(c));
}

static void urlEncodedEntityUnencodableHandler(UChar32 c, Vector<uint8_t>& result)
{
    result.reserveCapacity(result.size() + 9 + maxUChar32Digits);
    result.uncheckedAppend('%');
    result.uncheckedAppend('2');
    result.uncheckedAppend('6');
    result.uncheckedAppend('%');
    result.uncheckedAppend('2');
    result.uncheckedAppend('3');
    appendDecimal(c, result);
    result.uncheckedAppend('%');
    result.uncheckedAppend('3');
    result.uncheckedAppend('B');
}

static void entityUnencodableHandler(UChar32 c, Vector<uint8_t>& result)
{
    result.reserveCapacity(result.size() + 3 + maxUChar32Digits);
    result.uncheckedAppend('&');
    result.uncheckedAppend('#');
    appendDecimal(c, result);
    result.uncheckedAppend(';');
}

static void questionMarkUnencodableHandler(UChar32, Vector<uint8_t>& result)
{
    result.append('?');
}

static Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler(UnencodableHandling handling)
{
    switch (handling) {
    case UnencodableHandling::QuestionMarks:
        return questionMarkUnencodableHandler;
    case UnencodableHandling::Entities:
        return entityUnencodableHandler;
    case UnencodableHandling::URLEncodedEntities:
        return urlEncodedEntityUnencodableHandler;
    }
    ASSERT_NOT_REACHED();
    return entityUnencodableHandler;
}

using Big5DecodeIndex = std::array<std::pair<uint16_t, UChar32>, WTF_ARRAY_LENGTH(big5DecodingExtras) + WTF_ARRAY_LENGTH(big5EncodingMap)>;
static const Big5DecodeIndex& big5DecodeIndex()
{
    static auto* table = [] {
        auto table = new Big5DecodeIndex();
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(big5DecodingExtras); i++)
            (*table)[i] = big5DecodingExtras[i];
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(big5EncodingMap); i++)
            (*table)[i + WTF_ARRAY_LENGTH(big5DecodingExtras)] = { big5EncodingMap[i].second, big5EncodingMap[i].first };
        std::sort(table->begin(), table->end(), [] (auto& a, auto& b) {
            return a.first < b.first;
        });
        return table;
    }();
    return *table;
}

String TextCodecCJK::big5Decode(const uint8_t* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    StringBuilder result;

    auto parseByte = [&] (uint8_t byte) {
        if (uint8_t lead = std::exchange(m_lead, 0x00)) {
            uint8_t offset = byte < 0x7F ? 0x40 : 0x62;
            if ((byte >= 0x40 && byte <= 0x7E) || (byte >= 0xA1 && byte <= 0xFE)) {
                uint16_t pointer = (lead - 0x81) * 157 + (byte - offset);
                if (pointer == 1133) {
                    result.appendCharacter(0x00CA);
                    result.appendCharacter(0x0304);
                } else if (pointer == 1135) {
                    result.appendCharacter(0x00CA);
                    result.appendCharacter(0x030C);
                } else if (pointer == 1164) {
                    result.appendCharacter(0x00EA);
                    result.appendCharacter(0x0304);
                } else if (pointer == 1166) {
                    result.appendCharacter(0x00EA);
                    result.appendCharacter(0x030C);
                } else {
                    auto& index = big5DecodeIndex();
                    auto range = std::equal_range(index.begin(), index.end(), std::pair<uint16_t, UChar32>(pointer, 0), [](const auto& a, const auto& b) {
                        return a.first < b.first;
                    });
                    if (range.first != range.second) {
                        ASSERT(range.first + 1 == range.second);
                        result.appendCharacter(range.first->second);
                    } else {
                        sawError = true;
                        result.append(replacementCharacter);
                    }
                }
                return;
            }
            if (isASCII(byte))
                m_prependedByte = byte;
            sawError = true;
            result.append(replacementCharacter);
            return;
        }
        if (isASCII(byte)) {
            result.append(static_cast<LChar>(byte));
            return;
        }
        if (byte >= 0x81 && byte <= 0xFE) {
            m_lead = byte;
            return;
        }
        sawError = true;
        result.append(replacementCharacter);
    };
    
    if (stopOnError) {
        for (size_t i = 0; i < length; i++) {
            if (m_prependedByte)
                parseByte(*std::exchange(m_prependedByte, WTF::nullopt));
            if (sawError) {
                m_lead = 0x00;
                return result.toString();
            }
            parseByte(bytes[i]);
            if (sawError) {
                m_lead = 0x00;
                return result.toString();
            }
        }
    } else {
        for (size_t i = 0; i < length; i++) {
            if (m_prependedByte)
                parseByte(*std::exchange(m_prependedByte, WTF::nullopt));
            parseByte(bytes[i]);
        }
    }

    if (flush && m_lead) {
        m_lead = 0x00;
        sawError = true;
        result.append(replacementCharacter);
    }
    return result.toString();
}

String TextCodecCJK::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    switch (m_encoding) {
    case Encoding::EUC_JP:
        return m_icuCodec->decode(bytes, length, flush, stopOnError, sawError);
    case Encoding::Big5:
        return big5Decode(reinterpret_cast<const uint8_t*>(bytes), length, flush, stopOnError, sawError);
    }
    ASSERT_NOT_REACHED();
    return { };
}

Vector<uint8_t> TextCodecCJK::encode(StringView string, UnencodableHandling handling)
{
    switch (m_encoding) {
    case Encoding::EUC_JP:
        return eucJPEncode(string, unencodableHandler(handling));
    case Encoding::Big5:
        return big5Encode(string, unencodableHandler(handling));
    }
    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore

