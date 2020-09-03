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
}

void TextCodecCJK::registerEncodingNames(EncodingNameRegistrar registrar)
{
    // https://encoding.spec.whatwg.org/#names-and-labels
    auto registerAliases = [&] (std::initializer_list<const char*> list) {
        for (auto* alias : list)
            registrar(alias, *list.begin());
    };

    registerAliases({
        "Big5",
        "big5-hkscs",
        "cn-big5",
        "csbig5",
        "x-x-big5"
    });

    registerAliases({
        "EUC-JP",
        "cseucpkdfmtjapanese",
        "x-euc-jp"
    });

    registerAliases({
        "Shift_JIS",
        "csshiftjis",
        "ms932",
        "ms_kanji",
        "shift-jis",
        "sjis",
        "windows-31j",
        "x-sjis"
    });

    registerAliases({
        "EUC-KR",
        "cseuckr",
        "csksc56011987",
        "iso-ir-149",
        "korean",
        "ks_c_5601-1987",
        "ks_c_5601-1989",
        "ksc5601",
        "ksc_5601",
        "windows-949",
        
        // These aliases are not in the specification, but WebKit has historically supported them.
        "x-windows-949",
        "x-uhc",
    });
    
    registerAliases({
        "ISO-2022-JP",
        "csiso2022jp"
    });
}

void TextCodecCJK::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("EUC-JP", [] {
        return makeUnique<TextCodecCJK>(Encoding::EUC_JP);
    });
    registrar("Big5", [] {
        return makeUnique<TextCodecCJK>(Encoding::Big5);
    });
    registrar("Shift_JIS", [] {
        return makeUnique<TextCodecCJK>(Encoding::Shift_JIS);
    });
    registrar("EUC-KR", [] {
        return makeUnique<TextCodecCJK>(Encoding::EUC_KR);
    });
    registrar("ISO-2022-JP", [] {
        return makeUnique<TextCodecCJK>(Encoding::ISO2022JP);
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
        
        auto pointerRange = std::equal_range(std::begin(jis0208), std::end(jis0208), std::pair<UChar, uint16_t>(static_cast<UChar>(c), 0), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        if (pointerRange.first == pointerRange.second) {
            unencodableHandler(c, result);
            continue;
        }
        uint16_t pointer = (pointerRange.second - 1)->second;
        result.append(pointer / 94 + 0xA1);
        result.append(pointer % 94 + 0xA1);
    }
    return result;
}

// https://encoding.spec.whatwg.org/#iso-2022-jp-encoder
Vector<uint8_t> TextCodecCJK::iso2022JPEncode(StringView string, Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(string.length());
    
    auto changeStateToASCII = [&] {
        m_iso2022JPEncoderState = ISO2022JPEncoderState::ASCII;
        result.append(0x1B);
        result.append(0x28);
        result.append(0x42);
    };
    
    auto statefulUnencodableHandler = [&] (UChar32 codePoint, Vector<uint8_t>& result) {
        if (m_iso2022JPEncoderState == ISO2022JPEncoderState::Jis0208)
            changeStateToASCII();
        unencodableHandler(codePoint, result);
    };

    auto parseCodePoint = [&] (UChar32 codePoint) {
        if (m_iso2022JPEncoderState == ISO2022JPEncoderState::ASCII && isASCII(codePoint)) {
            result.append(static_cast<uint8_t>(codePoint));
            return;
        }
        if (m_iso2022JPEncoderState == ISO2022JPEncoderState::Roman) {
            if (isASCII(codePoint) && codePoint != 0x005C && codePoint !=0x007E) {
                result.append(static_cast<uint8_t>(codePoint));
                return;
            }
            if (codePoint == 0x00A5) {
                result.append(0x5C);
                return;
            }
            if (codePoint == 0x203E) {
                result.append(0x7E);
                return;
            }
        }
        if (isASCII(codePoint) && m_iso2022JPEncoderState != ISO2022JPEncoderState::ASCII) {
            m_prependedCodePoint = codePoint;
            if (m_iso2022JPEncoderState != ISO2022JPEncoderState::ASCII)
                changeStateToASCII();
            return;
        }
        if ((codePoint == 0x00A5 || codePoint == 0x203E) && m_iso2022JPEncoderState != ISO2022JPEncoderState::Roman) {
            m_prependedCodePoint = codePoint;
            m_iso2022JPEncoderState = ISO2022JPEncoderState::Roman;
            result.append(0x1B);
            result.append(0x28);
            result.append(0x4A);
            return;
        }
        if (codePoint == 0x2212)
            codePoint = 0xFF0D;
        if (codePoint >= 0xFF61 && codePoint <= 0xFF9F) {
            // From https://encoding.spec.whatwg.org/index-iso-2022-jp-katakana.txt
            static const UChar32 iso2022JPKatakana[63] {
                0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC,
                0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
                0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
                0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x309B, 0x309C
            };
            codePoint = iso2022JPKatakana[codePoint - 0xFF61];
        }
        
        auto codeUnit = static_cast<UChar>(codePoint);
        if (codeUnit != codePoint) {
            statefulUnencodableHandler(codePoint, result);
            return;
        }
        
        auto range = std::equal_range(std::begin(jis0208), std::end(jis0208), std::pair<UChar, uint16_t>(codeUnit, 0), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        if (range.first == range.second) {
            statefulUnencodableHandler(codePoint, result);
            return;
        }
        if (m_iso2022JPEncoderState != ISO2022JPEncoderState::Jis0208) {
            m_prependedCodePoint = codePoint;
            m_iso2022JPEncoderState = ISO2022JPEncoderState::Jis0208;
            result.append(0x1B);
            result.append(0x24);
            result.append(0x42);
            return;
        }
        uint16_t pointer = (range.second - 1)->second;
        result.append(pointer / 94 + 0x21);
        result.append(pointer % 94 + 0x21);
    };
    
    auto characters = string.upconvertedCharacters();
    for (WTF::CodePointIterator<UChar> iterator(characters.get(), characters.get() + string.length()); !iterator.atEnd(); ++iterator) {
        parseCodePoint(*iterator);
        if (m_prependedCodePoint)
            parseCodePoint(*std::exchange(m_prependedCodePoint, WTF::nullopt));
    }

    if (m_iso2022JPEncoderState != ISO2022JPEncoderState::ASCII)
        changeStateToASCII();
    
    return result;
}

// https://encoding.spec.whatwg.org/#shift_jis-encoder
static Vector<uint8_t> shiftJISEncode(StringView string, Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(string.length());

    auto characters = string.upconvertedCharacters();
    for (WTF::CodePointIterator<UChar> iterator(characters.get(), characters.get() + string.length()); !iterator.atEnd(); ++iterator) {
        auto codePoint = *iterator;
        if (isASCII(codePoint) || codePoint == 0x0080) {
            result.append(codePoint);
            continue;
        }
        if (codePoint == 0x00A5) {
            result.append(0x5C);
            continue;
        }
        if (codePoint == 0x203E) {
            result.append(0x7E);
            continue;
        }
        if (codePoint >= 0xFF61 && codePoint <= 0xFF9F) {
            result.append(codePoint - 0xFF61 + 0xA1);
            continue;
        }
        if (codePoint == 0x2212)
            codePoint = 0xFF0D;
        
        auto codeUnit = static_cast<UChar>(codePoint);
        if (codeUnit != codePoint) {
            unencodableHandler(codePoint, result);
            continue;
        }
        
        auto range = std::equal_range(std::begin(jis0208), std::end(jis0208), std::pair<UChar, uint16_t>(codeUnit, 0), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        if (range.first == range.second) {
            unencodableHandler(codePoint, result);
            continue;
        }
        
        ASSERT(range.first + 3 >= range.second);
        for (auto* pair = range.second - 1; pair >= range.first; pair--) {
            uint16_t pointer = pair->second;
            if (pointer >= 8272 && pointer <= 8835)
                continue;
            uint8_t lead = pointer / 188;
            uint8_t leadOffset = lead < 0x1F ? 0x81 : 0xC1;
            uint8_t trail = pointer % 188;
            uint8_t offset = trail < 0x3F ? 0x40 : 0x41;
            result.append(lead + leadOffset);
            result.append(trail + offset);
            break;
        }
    }
    return result;
}

using EUCKREncodingIndex = std::array<std::pair<UChar, uint16_t>, WTF_ARRAY_LENGTH(eucKRDecodingIndex)>;
static const EUCKREncodingIndex& eucKREncodingIndex()
{
    static auto* table = [] {
        auto table = new EUCKREncodingIndex();
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(eucKRDecodingIndex); i++)
            (*table)[i] = { eucKRDecodingIndex[i].second, eucKRDecodingIndex[i].first };
        std::sort(table->begin(), table->end(), [] (auto& a, auto& b) {
            return a.first < b.first;
        });
        return table;
    }();
    return *table;
}

// https://encoding.spec.whatwg.org/#euc-kr-encoder
static Vector<uint8_t> eucKREncode(StringView string, Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(string.length());

    auto characters = string.upconvertedCharacters();
    for (WTF::CodePointIterator<UChar> iterator(characters.get(), characters.get() + string.length()); !iterator.atEnd(); ++iterator) {
        auto codePoint = *iterator;
        if (isASCII(codePoint)) {
            result.append(static_cast<uint8_t>(codePoint));
            continue;
        }
        
        auto codeUnit = static_cast<UChar>(codePoint);
        if (codeUnit != codePoint) {
            unencodableHandler(codePoint, result);
            continue;
        }
        auto& index = eucKREncodingIndex();
        auto range = std::equal_range(index.begin(), index.end(), std::pair<UChar, uint16_t>(codePoint, 0), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        if (range.first == range.second) {
            unencodableHandler(codePoint, result);
            continue;
        }
        uint16_t pointer = range.first->second;
        result.append(pointer / 190 + 0x81);
        result.append(pointer % 190 + 0x41);
    }
    return result;
}

// https://encoding.spec.whatwg.org/#euc-kr-decoder
String TextCodecCJK::eucKRDecode(const uint8_t* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    StringBuilder result;

    auto parseByte = [&] (uint8_t byte) {
        if (uint8_t lead = std::exchange(m_lead, 0x00)) {
            if (byte >= 0x41 && byte <= 0xFE) {
                uint16_t pointer = (lead - 0x81) * 190 + byte - 0x41;
                auto range = std::equal_range(std::begin(eucKRDecodingIndex), std::end(eucKRDecodingIndex), std::pair<uint16_t, UChar>(pointer, 0), [] (const auto& a, const auto& b) {
                    return a.first < b.first;
                });
                if (range.first != range.second) {
                    result.append(range.first->second);
                    return;
                }
            }
            if (isASCII(byte))
                m_prependedByte = byte;
            sawError = true;
            result.append(replacementCharacter);
            return;
        }
        if (isASCII(byte)) {
            result.append(byte);
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
            parseByte(bytes[i]);
            if (sawError) {
                m_lead = 0x00;
                return result.toString();
            }
            if (m_prependedByte)
                parseByte(*std::exchange(m_prependedByte, WTF::nullopt));
            if (sawError) {
                m_lead = 0x00;
                return result.toString();
            }
        }
    } else {
        for (size_t i = 0; i < length; i++) {
            parseByte(bytes[i]);
            if (m_prependedByte)
                parseByte(*std::exchange(m_prependedByte, WTF::nullopt));
        }
    }

    if (flush && m_lead) {
        m_lead = 0x00;
        sawError = true;
        result.append(replacementCharacter);
    }
    return result.toString();
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

        auto pointerRange = std::equal_range(std::begin(big5EncodingMap), std::end(big5EncodingMap), std::pair<UChar32, uint16_t>(c, 0), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        
        if (pointerRange.first == pointerRange.second) {
            unencodableHandler(c, result);
            continue;
        }

        uint16_t pointer = 0;
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
            parseByte(bytes[i]);
            if (sawError) {
                m_lead = 0x00;
                return result.toString();
            }
            if (m_prependedByte)
                parseByte(*std::exchange(m_prependedByte, WTF::nullopt));
            if (sawError) {
                m_lead = 0x00;
                return result.toString();
            }
        }
    } else {
        for (size_t i = 0; i < length; i++) {
            parseByte(bytes[i]);
            if (m_prependedByte)
                parseByte(*std::exchange(m_prependedByte, WTF::nullopt));
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
    const char* icuName = nullptr;
    switch (m_encoding) {
    case Encoding::EUC_JP:
        icuName = "euc-jp-2007";
        break;
    case Encoding::Shift_JIS:
        icuName = "ibm-943_P15A-2003";
        break;
    case Encoding::ISO2022JP:
        icuName = "ISO_2022,locale=ja,version=0";
        break;
    case Encoding::EUC_KR:
        return eucKRDecode(reinterpret_cast<const uint8_t*>(bytes), length, flush, stopOnError, sawError);
    case Encoding::Big5:
        return big5Decode(reinterpret_cast<const uint8_t*>(bytes), length, flush, stopOnError, sawError);
    }

    if (!m_icuCodec)
        m_icuCodec = makeUnique<TextCodecICU>(icuName, icuName);
    return m_icuCodec->decode(bytes, length, flush, stopOnError, sawError);
}

Vector<uint8_t> TextCodecCJK::encode(StringView string, UnencodableHandling handling)
{
    switch (m_encoding) {
    case Encoding::EUC_JP:
        return eucJPEncode(string, unencodableHandler(handling));
    case Encoding::Shift_JIS:
        return shiftJISEncode(string, unencodableHandler(handling));
    case Encoding::ISO2022JP:
        return iso2022JPEncode(string, unencodableHandler(handling));
    case Encoding::EUC_KR:
        return eucKREncode(string, unencodableHandler(handling));
    case Encoding::Big5:
        return big5Encode(string, unencodableHandler(handling));
    }
    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore

