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
#include <mutex>
#include <wtf/text/CodePointIterator.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

enum class TextCodecCJK::Encoding : uint8_t {
    EUC_JP,
    ISO2022JP,
    Shift_JIS,
    EUC_KR,
    Big5
};

TextCodecCJK::TextCodecCJK(Encoding encoding)
    : m_encoding(encoding)
{
    checkEncodingTableInvariants();
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

using JIS0208EncodeIndex = std::array<std::pair<UChar, uint16_t>, sizeof(jis0208()) / sizeof(jis0208()[0])>;
static const JIS0208EncodeIndex& jis0208EncodeIndex()
{
    // Allocate this at runtime because building it at compile time would make the binary much larger and this is often not used.
    static JIS0208EncodeIndex* table;
    static std::once_flag once;
    std::call_once(once, [&] {
        table = new JIS0208EncodeIndex;
        auto& index = jis0208();
        for (size_t i = 0; i < index.size(); i++)
            (*table)[i] = { index[i].second, index[i].first };
        stableSortByFirst(*table);
    });
    return *table;
}

String TextCodecCJK::decodeCommon(const uint8_t* bytes, size_t length, bool flush, bool stopOnError, bool& sawError, const Function<SawError(uint8_t, StringBuilder&)>& byteParser)
{
    StringBuilder result;
    result.reserveCapacity(length);

    if (m_prependedByte && byteParser(*std::exchange(m_prependedByte, WTF::nullopt), result) == SawError::Yes) {
        sawError = true;
        result.append(replacementCharacter);
        if (stopOnError) {
            m_lead = 0x00;
            return result.toString();
        }
    }
    for (size_t i = 0; i < length; i++) {
        if (byteParser(bytes[i], result) == SawError::Yes) {
            sawError = true;
            result.append(replacementCharacter);
            if (stopOnError) {
                m_lead = 0x00;
                return result.toString();
            }
        }
        if (m_prependedByte && byteParser(*std::exchange(m_prependedByte, WTF::nullopt), result) == SawError::Yes) {
            sawError = true;
            result.append(replacementCharacter);
            if (stopOnError) {
                m_lead = 0x00;
                return result.toString();
            }
        }
    }

    if (flush && m_lead) {
        m_lead = 0x00;
        sawError = true;
        result.append(replacementCharacter);
    }

    return result.toString();
}

static Optional<UChar> codePointJIS0208(uint16_t pointer)
{
    return findFirstInSortedPairs(jis0208(), pointer);
}

static Optional<UChar> codePointJIS0212(uint16_t pointer)
{
    return findFirstInSortedPairs(jis0212(), pointer);
}

// https://encoding.spec.whatwg.org/#euc-jp-decoder
String TextCodecCJK::eucJPDecode(const uint8_t* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    return decodeCommon(bytes, length, flush, stopOnError, sawError, [this] (uint8_t byte, StringBuilder& result) {
        if (uint8_t lead = std::exchange(m_lead, 0x00)) {
            if (lead == 0x8E && byte >= 0xA1 && byte <= 0xDF) {
                result.appendCharacter(0xFF61 - 0xA1 + byte);
                return SawError::No;
            }
            if (lead == 0x8F && byte >= 0xA1 && byte <= 0xFE) {
                m_jis0212 = true;
                m_lead = byte;
                return SawError::No;
            }
            if (lead >= 0xA1 && lead <= 0xFE && byte >= 0xA1 && byte <= 0xFE) {
                uint16_t pointer = (lead - 0xA1) * 94 + byte - 0xA1;
                if (auto codePoint = std::exchange(m_jis0212, false) ? codePointJIS0212(pointer) : codePointJIS0208(pointer)) {
                    result.append(*codePoint);
                    return SawError::No;
                }
            }
            if (isASCII(byte))
                m_prependedByte = byte;
            return SawError::Yes;
        }
        if (isASCII(byte)) {
            result.append(static_cast<char>(byte));
            return SawError::No;
        }
        if (byte == 0x8E || byte == 0x8F || (byte >= 0xA1 && byte <= 0xFE)) {
            m_lead = byte;
            return SawError::No;
        }
        return SawError::Yes;
    });
}

// https://encoding.spec.whatwg.org/#euc-jp-encoder
static Vector<uint8_t> eucJPEncode(StringView string, Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(string.length());

    auto characters = string.upconvertedCharacters();
    for (WTF::CodePointIterator<UChar> iterator(characters.get(), characters.get() + string.length()); !iterator.atEnd(); ++iterator) {
        auto codePoint = *iterator;
        if (isASCII(codePoint)) {
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
            result.append(0x8E);
            result.append(codePoint - 0xFF61 + 0xA1);
            continue;
        }
        if (codePoint == 0x2212)
            codePoint = 0xFF0D;

        auto pointer = findFirstInSortedPairs(jis0208EncodeIndex(), codePoint);
        if (!pointer) {
            unencodableHandler(codePoint, result);
            continue;
        }
        result.append(*pointer / 94 + 0xA1);
        result.append(*pointer % 94 + 0xA1);
    }
    return result;
}

// https://encoding.spec.whatwg.org/#iso-2022-jp-decoder
String TextCodecCJK::iso2022JPDecode(const uint8_t* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    auto byteParser = [&] (uint8_t byte, StringBuilder& result) {
        switch (m_iso2022JPDecoderState) {
        case ISO2022JPDecoderState::ASCII:
            if (byte == 0x1B) {
                m_iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                break;
            }
            if (byte <= 0x7F && byte != 0x0E && byte != 0x0F && byte != 0x1B) {
                m_iso2022JPOutput = false;
                result.append(byte);
                break;
            }
            m_iso2022JPOutput = false;
            return SawError::Yes;
        case ISO2022JPDecoderState::Roman:
            if (byte == 0x1B) {
                m_iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                break;
            }
            if (byte == 0x5C) {
                m_iso2022JPOutput = false;
                result.append(static_cast<UChar>(0x00A5));
                break;
            }
            if (byte == 0x7E) {
                m_iso2022JPOutput = false;
                result.append(static_cast<UChar>(0x203E));
                break;
            }
            if (byte <= 0x7F && byte != 0x0E && byte != 0x0F && byte != 0x1B && byte != 0x5C && byte != 0x7E) {
                m_iso2022JPOutput = false;
                result.append(byte);
                break;
            }
            m_iso2022JPOutput = false;
            return SawError::Yes;
        case ISO2022JPDecoderState::Katakana:
            if (byte == 0x1B) {
                m_iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                break;
            }
            if (byte >= 0x21 && byte <= 0x5F) {
                m_iso2022JPOutput = false;
                result.append(static_cast<UChar>(0xFF61 - 0x21 + byte));
                break;
            }
            m_iso2022JPOutput = false;
            return SawError::Yes;
        case ISO2022JPDecoderState::LeadByte:
            if (byte == 0x1B) {
                m_iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                break;
            }
            if (byte >= 0x21 && byte <= 0x7E) {
                m_iso2022JPOutput = false;
                m_lead = byte;
                m_iso2022JPDecoderState = ISO2022JPDecoderState::TrailByte;
                break;
            }
            m_iso2022JPOutput = false;
            return SawError::Yes;
        case ISO2022JPDecoderState::TrailByte:
            if (byte == 0x1B) {
                m_iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                return SawError::Yes;
            }
            m_iso2022JPDecoderState = ISO2022JPDecoderState::LeadByte;
            if (byte >= 0x21 && byte <= 0x7E) {
                uint16_t pointer = (m_lead - 0x21) * 94 + byte - 0x21;
                if (auto codePoint = codePointJIS0208(pointer)) {
                    result.append(*codePoint);
                    break;
                }
                return SawError::Yes;
            }
            return SawError::Yes;
        case ISO2022JPDecoderState::EscapeStart:
            if (byte == 0x24 || byte == 0x28) {
                m_lead = byte;
                m_iso2022JPDecoderState = ISO2022JPDecoderState::Escape;
                break;
            }
            m_prependedByte = byte;
            m_iso2022JPOutput = false;
            m_iso2022JPDecoderState = m_iso2022JPDecoderOutputState;
            return SawError::Yes;
        case ISO2022JPDecoderState::Escape: {
            uint8_t lead = std::exchange(m_lead, 0x00);
            Optional<ISO2022JPDecoderState> state;
            if (lead == 0x28) {
                if (byte == 0x42)
                    state = ISO2022JPDecoderState::ASCII;
                else if (byte == 0x4A)
                    state = ISO2022JPDecoderState::Roman;
                else if (byte == 0x49)
                    state = ISO2022JPDecoderState::Katakana;
            } else if (lead == 0x24 && (byte == 0x40 || byte == 0x42))
                state = ISO2022JPDecoderState::LeadByte;
            if (state) {
                m_iso2022JPDecoderState = *state;
                m_iso2022JPDecoderOutputState = *state;
                if (std::exchange(m_iso2022JPOutput, true))
                    return SawError::Yes;
                break;
            }
            m_prependedByte = lead;
            m_iso2022JPSecondPrependedByte = byte;
            m_iso2022JPOutput = false;
            m_iso2022JPDecoderState = m_iso2022JPDecoderOutputState;
            return SawError::Yes;
        }
        }
        return SawError::No;
    };
    
    StringBuilder result;
    result.reserveCapacity(length);

    if (m_prependedByte && byteParser(*std::exchange(m_prependedByte, WTF::nullopt), result) == SawError::Yes) {
        sawError = true;
        result.append(replacementCharacter);
        if (stopOnError) {
            m_lead = 0x00;
            return result.toString();
        }
    }
    if (m_iso2022JPSecondPrependedByte && byteParser(*std::exchange(m_iso2022JPSecondPrependedByte, WTF::nullopt), result) == SawError::Yes && stopOnError) {
        sawError = true;
        result.append(replacementCharacter);
        if (stopOnError) {
            m_lead = 0x00;
            return result.toString();
        }
    }
    for (size_t i = 0; i < length; i++) {
        if (byteParser(bytes[i], result) == SawError::Yes) {
            sawError = true;
            result.append(replacementCharacter);
            if (stopOnError) {
                m_lead = 0x00;
                return result.toString();
            }
        }
        if (m_prependedByte && byteParser(*std::exchange(m_prependedByte, WTF::nullopt), result) == SawError::Yes) {
            sawError = true;
            result.append(replacementCharacter);
            if (stopOnError) {
                m_lead = 0x00;
                return result.toString();
            }
        }
        if (m_iso2022JPSecondPrependedByte && byteParser(*std::exchange(m_iso2022JPSecondPrependedByte, WTF::nullopt), result) == SawError::Yes && stopOnError) {
            sawError = true;
            result.append(replacementCharacter);
            if (stopOnError) {
                m_lead = 0x00;
                return result.toString();
            }
        }
    }

    if (flush) {
        switch (m_iso2022JPDecoderState) {
        case ISO2022JPDecoderState::ASCII:
        case ISO2022JPDecoderState::Roman:
        case ISO2022JPDecoderState::Katakana:
        case ISO2022JPDecoderState::LeadByte:
            break;
        case ISO2022JPDecoderState::TrailByte:
            m_iso2022JPDecoderState = ISO2022JPDecoderState::LeadByte;
            FALLTHROUGH;
        case ISO2022JPDecoderState::EscapeStart:
            sawError = true;
            result.append(replacementCharacter);
            break;
        case ISO2022JPDecoderState::Escape:
            sawError = true;
            result.append(replacementCharacter);
            if (m_lead) {
                ASSERT(isASCII(m_lead));
                result.append(std::exchange(m_lead, 0x00));
            }
            break;
        }
    }

    return result.toString();
}

// https://encoding.spec.whatwg.org/#iso-2022-jp-encoder
static Vector<uint8_t> iso2022JPEncode(StringView string, Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler)
{
    enum class State : uint8_t { ASCII, Roman, Jis0208 };
    State state { State::ASCII };

    Vector<uint8_t> result;
    result.reserveInitialCapacity(string.length());
    
    auto changeStateToASCII = [&] {
        state = State::ASCII;
        result.append(0x1B);
        result.append(0x28);
        result.append(0x42);
    };
    
    auto statefulUnencodableHandler = [&] (UChar32 codePoint, Vector<uint8_t>& result) {
        if (state == State::Jis0208)
            changeStateToASCII();
        unencodableHandler(codePoint, result);
    };

    Function<void(UChar32)> parseCodePoint;
    parseCodePoint = [&] (UChar32 codePoint) {
        if (state == State::ASCII && isASCII(codePoint)) {
            result.append(codePoint);
            return;
        }
        if (state == State::Roman) {
            if (isASCII(codePoint) && codePoint != 0x005C && codePoint !=0x007E) {
                result.append(codePoint);
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
        if (isASCII(codePoint) && state != State::ASCII) {
            if (state != State::ASCII)
                changeStateToASCII();
            parseCodePoint(codePoint);
            return;
        }
        if ((codePoint == 0x00A5 || codePoint == 0x203E) && state != State::Roman) {
            state = State::Roman;
            result.append(0x1B);
            result.append(0x28);
            result.append(0x4A);
            parseCodePoint(codePoint);
            return;
        }
        if (codePoint == 0x2212)
            codePoint = 0xFF0D;
        if (codePoint >= 0xFF61 && codePoint <= 0xFF9F) {
            // From https://encoding.spec.whatwg.org/index-iso-2022-jp-katakana.txt
            static constexpr std::array<UChar32, 63> iso2022JPKatakana {
                0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC,
                0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
                0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
                0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x309B, 0x309C
            };
            static_assert(std::size(iso2022JPKatakana) == 0xFF9F - 0xFF61 + 1);
            codePoint = iso2022JPKatakana[codePoint - 0xFF61];
        }

        auto pointer = findFirstInSortedPairs(jis0208EncodeIndex(), codePoint);
        if (!pointer) {
            statefulUnencodableHandler(codePoint, result);
            return;
        }
        if (state != State::Jis0208) {
            state = State::Jis0208;
            result.append(0x1B);
            result.append(0x24);
            result.append(0x42);
            parseCodePoint(codePoint);
            return;
        }
        result.append(*pointer / 94 + 0x21);
        result.append(*pointer % 94 + 0x21);
    };
    
    auto characters = string.upconvertedCharacters();
    for (WTF::CodePointIterator<UChar> iterator(characters.get(), characters.get() + string.length()); !iterator.atEnd(); ++iterator)
        parseCodePoint(*iterator);

    if (state != State::ASCII)
        changeStateToASCII();
    
    return result;
}

// https://encoding.spec.whatwg.org/#shift_jis-decoder
String TextCodecCJK::shiftJISDecode(const uint8_t* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    return decodeCommon(bytes, length, flush, stopOnError, sawError, [this] (uint8_t byte, StringBuilder& result) {
        if (uint8_t lead = std::exchange(m_lead, 0x00)) {
            uint8_t offset = byte < 0x7F ? 0x40 : 0x41;
            uint8_t leadOffset = lead < 0xA0 ? 0x81 : 0xC1;
            if ((byte >= 0x40 && byte <= 0x7E) || (byte >= 0x80 && byte <= 0xFC)) {
                uint16_t pointer = (lead - leadOffset) * 188 + byte - offset;
                if (pointer >= 8836 && pointer <= 10715) {
                    result.append(static_cast<UChar>(0xE000 - 8836 + pointer));
                    return SawError::No;
                }
                if (auto codePoint = codePointJIS0208(pointer)) {
                    result.append(*codePoint);
                    return SawError::No;
                }
            }
            if (isASCII(byte))
                m_prependedByte = byte;
            return SawError::Yes;
        }
        if (isASCII(byte) || byte == 0x80) {
            result.append(byte);
            return SawError::No;
        }
        if (byte >= 0xA1 && byte <= 0xDF) {
            result.append(static_cast<UChar>(0xFF61 - 0xA1 + byte));
            return SawError::No;
        }
        if ((byte >= 0x81 && byte <= 0x9F) || (byte >= 0xE0 && byte <= 0xFC)) {
            m_lead = byte;
            return SawError::No;
        }
        return SawError::Yes;
    });
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
        
        auto range = findInSortedPairs(jis0208EncodeIndex(), codePoint);
        if (range.first == range.second) {
            unencodableHandler(codePoint, result);
            continue;
        }

        ASSERT(range.first + 3 >= range.second);
        for (auto pair = range.first; pair < range.second; pair++) {
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

using EUCKREncodingIndex = std::array<std::pair<UChar, uint16_t>, sizeof(eucKR()) / sizeof(eucKR()[0])>;
static const EUCKREncodingIndex& eucKREncodingIndex()
{
    // Allocate this at runtime because building it at compile time would make the binary much larger and this is often not used.
    static EUCKREncodingIndex* table;
    static std::once_flag once;
    std::call_once(once, [&] {
        table = new EUCKREncodingIndex;
        auto& index = eucKR();
        for (size_t i = 0; i < index.size(); i++)
            (*table)[i] = { index[i].second, index[i].first };
        sortByFirst(*table);
        ASSERT(sortedFirstsAreUnique(*table));
    });
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
            result.append(codePoint);
            continue;
        }
        
        auto pointer = findFirstInSortedPairs(eucKREncodingIndex(), codePoint);
        if (!pointer) {
            unencodableHandler(codePoint, result);
            continue;
        }
        result.append(*pointer / 190 + 0x81);
        result.append(*pointer % 190 + 0x41);
    }
    return result;
}

// https://encoding.spec.whatwg.org/#euc-kr-decoder
String TextCodecCJK::eucKRDecode(const uint8_t* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    return decodeCommon(bytes, length, flush, stopOnError, sawError, [this] (uint8_t byte, StringBuilder& result) {
        if (uint8_t lead = std::exchange(m_lead, 0x00)) {
            if (byte >= 0x41 && byte <= 0xFE) {
                if (auto codePoint = findFirstInSortedPairs(eucKR(), (lead - 0x81) * 190 + byte - 0x41)) {
                    result.append(*codePoint);
                    return SawError::No;
                }
            }
            if (isASCII(byte))
                m_prependedByte = byte;
            return SawError::Yes;
        }
        if (isASCII(byte)) {
            result.append(byte);
            return SawError::No;
        }
        if (byte >= 0x81 && byte <= 0xFE) {
            m_lead = byte;
            return SawError::No;
        }
        return SawError::Yes;
    });
}

using Big5EncodeIndex = std::array<std::pair<UChar32, uint16_t>, sizeof(big5()) / sizeof(big5()[0]) - 3904>;
static const Big5EncodeIndex& big5EncodeIndex()
{
    // Allocate this at runtime because building it at compile time would make the binary much larger and this is often not used.
    static Big5EncodeIndex* table;
    static std::once_flag once;
    std::call_once(once, [&] {
        table = new Big5EncodeIndex;
        auto& index = big5();
        // Remove the first 3094 elements because of https://encoding.spec.whatwg.org/#index-big5-pointer
        ASSERT(index[3903].first == (0xA1 - 0x81) * 157 - 1);
        ASSERT(index[3904].first == (0xA1 - 0x81) * 157);
        for (size_t i = 3904; i < index.size(); i++)
            (*table)[i - 3904] = { index[i].second, index[i].first };
        stableSortByFirst(*table);
    });
    return *table;
}

// https://encoding.spec.whatwg.org/#big5-encoder
static Vector<uint8_t> big5Encode(StringView string, Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler)
{
    Vector<uint8_t> result;
    result.reserveInitialCapacity(string.length());

    auto characters = string.upconvertedCharacters();
    for (WTF::CodePointIterator<UChar> iterator(characters.get(), characters.get() + string.length()); !iterator.atEnd(); ++iterator) {
        auto codePoint = *iterator;
        if (isASCII(codePoint)) {
            result.append(codePoint);
            continue;
        }

        auto pointerRange = findInSortedPairs(big5EncodeIndex(), codePoint);
        if (pointerRange.first == pointerRange.second) {
            unencodableHandler(codePoint, result);
            continue;
        }

        uint16_t pointer = 0;
        if (codePoint == 0x2550 || codePoint == 0x255E || codePoint == 0x2561 || codePoint == 0x256A || codePoint == 0x5341 || codePoint == 0x5345)
            pointer = (pointerRange.second - 1)->second;
        else
            pointer = pointerRange.first->second;

        if (pointer < 157 * (0xA1 - 0x81)) {
            unencodableHandler(codePoint, result);
            continue;
        }
        
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

Function<void(UChar32, Vector<uint8_t>&)> unencodableHandler(UnencodableHandling handling)
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

String TextCodecCJK::big5Decode(const uint8_t* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    return decodeCommon(bytes, length, flush, stopOnError, sawError, [this] (uint8_t byte, StringBuilder& result) {
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
                    if (auto codePoint = findFirstInSortedPairs(big5(), pointer))
                        result.appendCharacter(*codePoint);
                    else
                        return SawError::Yes;
                }
                return SawError::No;
            }
            if (isASCII(byte))
                m_prependedByte = byte;
            return SawError::Yes;
        }
        if (isASCII(byte)) {
            result.append(static_cast<char>(byte));
            return SawError::No;
        }
        if (byte >= 0x81 && byte <= 0xFE) {
            m_lead = byte;
            return SawError::No;
        }
        return SawError::Yes;
    });
}

String TextCodecCJK::decode(const char* charBytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    auto bytes = reinterpret_cast<const uint8_t*>(charBytes);
    switch (m_encoding) {
    case Encoding::EUC_JP:
        return eucJPDecode(bytes, length, flush, stopOnError, sawError);
    case Encoding::Shift_JIS:
        return shiftJISDecode(bytes, length, flush, stopOnError, sawError);
    case Encoding::ISO2022JP:
        return iso2022JPDecode(bytes, length, flush, stopOnError, sawError);
    case Encoding::EUC_KR:
        return eucKRDecode(bytes, length, flush, stopOnError, sawError);
    case Encoding::Big5:
        return big5Decode(bytes, length, flush, stopOnError, sawError);
    }
    ASSERT_NOT_REACHED();
    return { };
}

Vector<uint8_t> TextCodecCJK::encode(StringView string, UnencodableHandling handling) const
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
