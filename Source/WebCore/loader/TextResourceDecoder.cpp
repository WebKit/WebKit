/*
    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2003-2017 Apple Inc. All rights reserved.
    Copyright (C) 2005, 2006, 2007 Alexey Proskuryakov (ap@nypop.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/


#include "config.h"
#include "TextResourceDecoder.h"

#include "HTMLMetaCharsetParser.h"
#include "HTMLNames.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include <pal/text/TextCodec.h>
#include <pal/text/TextEncoding.h>
#include <pal/text/TextEncodingDetector.h>
#include <pal/text/TextEncodingRegistry.h>
#include <wtf/ASCIICType.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>

namespace WebCore {

using namespace HTMLNames;

// You might think we should put these find functions elsewhere, perhaps with the
// similar functions that operate on char16_t, but arguably only the decoder has
// a reason to process strings of char rather than char16_t.

static size_t find(std::span<const uint8_t> subject, std::span<const uint8_t> target)
{
    if (target.size() > subject.size())
        return notFound;

    size_t sizeDifference = subject.size() - target.size();
    for (size_t i = 0; i < sizeDifference; ++i) {
        bool match = true;
        for (size_t j = 0; j < target.size(); ++j) {
            if (subject[i + j] != target[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }
    return notFound;
}

static PAL::TextEncoding findTextEncoding(std::span<const LChar> encodingName)
{
    return StringView { encodingName };
}

class KanjiCode {
public:
    enum class Type : uint8_t { ASCII, JIS, EUC, SJIS, UTF16, UTF8 };
    static enum Type judge(std::span<const uint8_t>);
    static constexpr int ESC = 0x1b;
    static const std::array<uint8_t, 256> sjisMap;
    static int ISkanji(int code)
    {
        if (code >= 0x100)
            return 0;
        return sjisMap[code & 0xff] & 1;
    }
    static int ISkana(int code)
    {
        if (code >= 0x100)
            return 0;
        return sjisMap[code & 0xff] & 2;
    }
};

const std::array<uint8_t, 256> KanjiCode::sjisMap {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0
};

/*
 * EUC-JP is
 *     [0xa1 - 0xfe][0xa1 - 0xfe]
 *     0x8e[0xa1 - 0xfe](SS2)
 *     0x8f[0xa1 - 0xfe][0xa1 - 0xfe](SS3)
 *
 * Shift_Jis is
 *     [0x81 - 0x9f, 0xe0 - 0xef(0xfe?)][0x40 - 0x7e, 0x80 - 0xfc]
 *
 * Shift_Jis Hankaku Kana is
 *     [0xa1 - 0xdf]
 */

/*
 * KanjiCode::judge() is based on judge_jcode() from jvim
 *     http://hp.vector.co.jp/authors/VA003457/vim/
 *
 * Special Thanks to Kenichi Tsuchida
 */

auto KanjiCode::judge(std::span<const uint8_t> string) -> Type
{
    Type code;
    bool bfr = false;            /* Kana Moji */
    int bfk = 0;                /* EUC Kana */
    int sjis = 0;
    int euc = 0;

    code = Type::ASCII;

    size_t i = 0;
    while (i < string.size()) {
        if (string[i] == ESC && (string.size() - i >= 3)) {
            auto substring = string.subspan(i + 1);
            if (spanHasPrefix(substring, "$B"_span)
                || spanHasPrefix(substring, "(B"_span)
                || spanHasPrefix(substring, "$@"_span)
                || spanHasPrefix(substring, "(J"_span)) {
                code = Type::JIS;
                return code;
            }
            if (spanHasPrefix(substring, "(I"_span) || spanHasPrefix(substring, ")I"_span)) {
                code = Type::JIS;
                i += 3;
            } else
                ++i;
            bfr = false;
            bfk = 0;
        } else {
            if (string[i] < 0x20) {
                bfr = false;
                bfk = 0;
                /* ?? check kudokuten ?? && ?? hiragana ?? */
                if ((i >= 2) && (string[i - 2] == 0x81) && (0x41 <= string[i - 1] && string[i - 1] <= 0x49)) {
                    code = Type::SJIS;
                    sjis += 100;        /* kudokuten */
                } else if ((i >= 2) && (string[i - 2] == 0xa1) && (0xa2 <= string[i - 1] && string[i - 1] <= 0xaa)) {
                    code = Type::EUC;
                    euc += 100;         /* kudokuten */
                } else if ((i >= 2) && (string[i - 2] == 0x82) && (0xa0 <= string[i - 1]))
                    sjis += 40;         /* hiragana */
                else if ((i >= 2) && (string[i - 2] == 0xa4) && (0xa0 <= string[i - 1]))
                    euc += 40;          /* hiragana */
            } else {
                /* ?? check hiragana or katana ?? */
                if ((string.size() - i > 1) && (string[i] == 0x82) && (0xa0 <= string[i + 1]))
                    sjis++;     /* hiragana */
                else if ((string.size() - i > 1) && (string[i] == 0x83) && (0x40 <= string[i + 1] && string[i + 1] <= 0x9f))
                    sjis++;     /* katakana */
                else if ((string.size() - i > 1) && (string[i] == 0xa4) && (0xa0 <= string[i + 1]))
                    euc++;      /* hiragana */
                else if ((string.size() - i > 1) && (string[i] == 0xa5) && (0xa0 <= string[i + 1]))
                    euc++;      /* katakana */

                if (bfr) {
                    if ((i >= 1) && (0x40 <= string[i] && string[i] <= 0xa0) && ISkanji(string[i - 1])) {
                        code = Type::SJIS;
                        return code;
                    }
                    if ((i >= 1) && (0x81 <= string[i - 1] && string[i - 1] <= 0x9f) && ((0x40 <= string[i] && string[i] < 0x7e) || (0x7e < string[i] && string[i] <= 0xfc))) {
                        code = Type::SJIS;
                        return code;
                    }
                    if ((i >= 1) && (0xfd <= string[i] && string[i] <= 0xfe) && (0xa1 <= string[i - 1] && string[i - 1] <= 0xfe)) {
                        code = Type::EUC;
                        return code;
                    }
                    if ((i >= 1) && (0xfd <= string[i - 1] && string[i - 1] <= 0xfe) && (0xa1 <= string[i] && string[i] <= 0xfe)) {
                        code = Type::EUC;
                        return code;
                    }
                    if ((i >= 1) && (string[i] < 0xa0 || 0xdf < string[i]) && (0x8e == string[i - 1])) {
                        code = Type::SJIS;
                        return code;
                    }
                    if (string[i] <= 0x7f) {
                        code = Type::SJIS;
                        return code;
                    }
                    if (0xa1 <= string[i] && string[i] <= 0xa6)
                        ++euc;      /* sjis hankaku kana kigo */
                    else if (0xa1 <= string[i] && string[i] <= 0xdf) {
                        /* sjis hankaku kana */
                    } else if (0xa1 <= string[i] && string[i] <= 0xfe)
                        ++euc;
                    else if (0x8e == string[i])
                        ++euc;
                    else if (0x20 <= string[i] && string[i] <= 0x7f)
                        ++sjis;
                    bfr = false;
                    bfk = 0;
                } else if (0x8e == string[i]) {
                    if (string.size() - i <= 1) {
                    } else if (0xa1 <= string[i + 1] && string[i + 1] <= 0xdf) {
                        /* EUC KANA or SJIS KANJI */
                        if (bfk == 1) {
                            euc += 100;
                        }
                        ++bfk;
                        ++i;
                    } else {
                        /* SJIS only */
                        code = Type::SJIS;
                        return code;
                    }
                } else if (0x81 <= string[i] && string[i] <= 0x9f) {
                    /* SJIS only */
                    code = Type::SJIS;
                    if ((string.size() - i >= 1) && ((0x40 <= string[i + 1] && string[i + 1] <= 0x7e) || (0x80 <= string[i + 1] && string[i + 1] <= 0xfc)))
                        return code;
                } else if (0xfd <= string[i] && string[i] <= 0xfe) {
                    /* EUC only */
                    code = Type::EUC;
                    if ((string.size() - i >= 1) && (0xa1 <= string[i + 1] && string[i + 1] <= 0xfe))
                        return code;
                } else if (string[i] <= 0x7f)
                    ;
                else {
                    bfr = true;
                    bfk = 0;
                }
            }
            ++i;
        }
    }
    if (code == Type::ASCII) {
        if (sjis > euc)
            code = Type::SJIS;
        else if (sjis < euc)
            code = Type::EUC;
    }
    return code;
}

TextResourceDecoder::ContentType TextResourceDecoder::determineContentType(const String& mimeType)
{
    if (equalLettersIgnoringASCIICase(mimeType, "text/css"_s))
        return CSS;
    if (equalLettersIgnoringASCIICase(mimeType, "text/html"_s))
        return HTML;
    if (MIMETypeRegistry::isXMLMIMEType(mimeType) || mimeType == "text/xsl"_s)
        return XML;
    return PlainText;
}

const PAL::TextEncoding& TextResourceDecoder::defaultEncoding(ContentType contentType, const PAL::TextEncoding& specifiedDefaultEncoding)
{
    // Despite 8.5 "Text/xml with Omitted Charset" of RFC 3023, we assume UTF-8 instead of US-ASCII 
    // for text/xml. This matches Firefox.
    if (contentType == XML)
        return PAL::UTF8Encoding();
    if (!specifiedDefaultEncoding.isValid())
        return PAL::Latin1Encoding();
    return specifiedDefaultEncoding;
}

inline TextResourceDecoder::TextResourceDecoder(ContentType contentType, const PAL::TextEncoding& encoding, bool usesEncodingDetector)
    : m_contentType(contentType)
    , m_encoding(encoding)
    , m_usesEncodingDetector(usesEncodingDetector)
{
}

Ref<TextResourceDecoder> TextResourceDecoder::create(const String& mimeType, const PAL::TextEncoding& encoding, bool usesEncodingDetector)
{
    auto contentType = determineContentType(mimeType);
    return adoptRef(*new TextResourceDecoder(contentType, defaultEncoding(contentType, encoding), usesEncodingDetector));
}

Ref<TextResourceDecoder> TextResourceDecoder::create(ContentType contentType, const PAL::TextEncoding& encoding, bool usesEncodingDetector)
{
    return adoptRef(*new TextResourceDecoder(contentType, encoding, usesEncodingDetector));
}

TextResourceDecoder::~TextResourceDecoder() = default;

// https://encoding.spec.whatwg.org/#utf-8-decode
String TextResourceDecoder::textFromUTF8(std::span<const uint8_t> data)
{
    constexpr std::array<uint8_t, 3> byteOrderMarkUTF8 = { 0xEF, 0xBB, 0xBF };

    auto decoder = TextResourceDecoder::create("text/plain"_s, "UTF-8"_s);
    if (!spanHasPrefix(data, std::span { byteOrderMarkUTF8 }))
        decoder->decode(byteOrderMarkUTF8);
    return decoder->decodeAndFlush(data);
}

void TextResourceDecoder::setEncoding(const PAL::TextEncoding& encoding, EncodingSource source)
{
    if (m_alwaysUseUTF8)
        return;

    // In case the encoding didn't exist, we keep the old one (helps some sites specifying invalid encodings).
    if (!encoding.isValid())
        return;

    // When encoding comes from meta tag (i.e. it cannot be XML files sent via XHR),
    // treat x-user-defined as windows-1252 (bug 18270)
    if (source == EncodingFromMetaTag && equalLettersIgnoringASCIICase(encoding.name(), "x-user-defined"_s))
        m_encoding = "windows-1252"_s;
    else if (source == EncodingFromMetaTag || source == EncodingFromXMLHeader || source == EncodingFromCSSCharset)        
        m_encoding = encoding.closestByteBasedEquivalent();
    else
        m_encoding = encoding;

    m_codec = nullptr;
    m_source = source;
}

bool TextResourceDecoder::hasEqualEncodingForCharset(const String& charset) const
{
    return defaultEncoding(m_contentType, charset) == m_encoding;
}

// Returns the position of the encoding string.
static size_t findXMLEncoding(std::span<const uint8_t> string, size_t& encodingLength)
{
    size_t position = find(string, "encoding"_span8);
    if (position == notFound)
        return notFound;
    position += 8;
    
    // Skip spaces and stray control characters.
    while (position < string.size() && string[position] <= ' ')
        ++position;

    // Skip equals sign.
    if (position >= string.size() || string[position] != '=')
        return notFound;
    ++position;

    // Skip spaces and stray control characters.
    while (position < string.size() && string[position] <= ' ')
        ++position;

    // Skip quotation mark.
    if (position >= string.size())
        return notFound;
    char quoteMark = string[position];
    if (quoteMark != '"' && quoteMark != '\'')
        return notFound;
    ++position;

    // Find the trailing quotation mark.
    size_t end = position;
    while (end < string.size() && string[end] != quoteMark)
        ++end;
    if (end >= string.size())
        return notFound;

    encodingLength = end - position;
    return position;
}

size_t TextResourceDecoder::checkForBOM(std::span<const uint8_t> data)
{
    // Check for UTF-16 or UTF-8 BOM mark at the beginning, which is a sure sign of a Unicode encoding.
    // We let it override even a user-chosen encoding.
    constexpr size_t maximumBOMLength = 3;

    ASSERT(!m_checkedForBOM);

    size_t lengthOfBOM = 0;

    size_t bufferLength = m_buffer.size();

    auto buffer1 = m_buffer.span();
    auto buffer2 = data;
    uint8_t c1 = !buffer1.empty() ? consume(buffer1) : !buffer2.empty() ? consume(buffer2) : 0;
    uint8_t c2 = !buffer1.empty() ? consume(buffer1) : !buffer2.empty() ? consume(buffer2) : 0;
    uint8_t c3 = !buffer1.empty() ? consume(buffer1) : !buffer2.empty() ? consume(buffer2) : 0;

    // Check for the BOM.
    if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
        ASSERT(PAL::UTF8Encoding().isValid());
        setEncoding(PAL::UTF8Encoding(), AutoDetectedEncoding);
        lengthOfBOM = 3;
    } else if (!m_alwaysUseUTF8) {
        if (c1 == 0xFF && c2 == 0xFE) {
            ASSERT(PAL::UTF16LittleEndianEncoding().isValid());
            setEncoding(PAL::UTF16LittleEndianEncoding(), AutoDetectedEncoding);
            lengthOfBOM = 2;
        } else if (c1 == 0xFE && c2 == 0xFF) {
            ASSERT(PAL::UTF16BigEndianEncoding().isValid());
            setEncoding(PAL::UTF16BigEndianEncoding(), AutoDetectedEncoding);
            lengthOfBOM = 2;
        }
    }

    if (lengthOfBOM || bufferLength + data.size() >= maximumBOMLength)
        m_checkedForBOM = true;

    ASSERT(lengthOfBOM <= maximumBOMLength);
    return lengthOfBOM;
}

bool TextResourceDecoder::checkForCSSCharset(std::span<const uint8_t> data, bool& movedDataToBuffer)
{
    if (m_source != DefaultEncoding && m_source != EncodingFromParentFrame) {
        m_checkedForCSSCharset = true;
        return true;
    }

    size_t oldSize = m_buffer.size();
    m_buffer.grow(oldSize + data.size());
    memcpySpan(m_buffer.mutableSpan().subspan(oldSize), data);

    movedDataToBuffer = true;

    if (m_buffer.size() <= 13) // strlen('@charset "x";') == 13
        return false;

    data = m_buffer.span();

    if (skipCharactersExactly(data, "@charset \""_span8)) {
        size_t index = 0;
        while (index < data.size() && data[index] != '"')
            ++index;

        if (index == data.size())
            return false;

        auto encodingName = data.first(index);
        
        ++index;
        if (index == data.size())
            return false;

        if (data[index] == ';')
            setEncoding(findTextEncoding(encodingName), EncodingFromCSSCharset);
    }

    m_checkedForCSSCharset = true;
    return true;
}

bool TextResourceDecoder::checkForHeadCharset(std::span<const uint8_t> data, bool& movedDataToBuffer)
{
    if (m_source != DefaultEncoding && m_source != EncodingFromParentFrame) {
        m_checkedForHeadCharset = true;
        return true;
    }

    // This is not completely efficient, since the function might go
    // through the HTML head several times.

    size_t oldSize = m_buffer.size();
    m_buffer.grow(oldSize + data.size());
    memcpySpan(m_buffer.mutableSpan().subspan(oldSize), data);

    movedDataToBuffer = true;

    // Continue with checking for an HTML meta tag if we were already doing so.
    if (m_charsetParser)
        return checkForMetaCharset(data);

    auto bufferData = m_buffer.span();

    // Is there enough data available to check for XML declaration?
    if (bufferData.size() < 8)
        return false;

    // Handle XML declaration, which can have encoding in it. This encoding is honored even for HTML documents.
    // It is an error for an XML declaration not to be at the start of an XML document, and it is ignored in HTML documents in such case.
    static constexpr std::array<uint8_t, 5> xmlPrefix { '<', '?', 'x', 'm', 'l' };
    static constexpr std::array<uint8_t, 6> xmlPrefixLittleEndian { '<', 0, '?', 0, 'x', 0 };
    static constexpr std::array<uint8_t, 6> xmlPrefixBigEndian { 0, '<', 0, '?', 0, 'x' };
    if (spanHasPrefix(bufferData, std::span { xmlPrefix })) {
        auto xmlDeclarationEnd = bufferData;
        skipUntil(xmlDeclarationEnd, '>');
        if (xmlDeclarationEnd.empty())
            return false;
        // No need for +1, because we have an extra "?" to lose at the end of XML declaration.
        size_t length = 0;
        size_t position = findXMLEncoding(bufferData.first(xmlDeclarationEnd.data() - bufferData.data()), length);
        if (position != notFound)
            setEncoding(findTextEncoding(bufferData.subspan(position, length)), EncodingFromXMLHeader);
        // continue looking for a charset - it may be specified in an HTTP-Equiv meta
    } else if (spanHasPrefix(bufferData, std::span { xmlPrefixLittleEndian })) {
        setEncoding(PAL::UTF16LittleEndianEncoding(), AutoDetectedEncoding);
        return true;
    } else if (spanHasPrefix(bufferData, std::span { xmlPrefixBigEndian })) {
        setEncoding(PAL::UTF16BigEndianEncoding(), AutoDetectedEncoding);
        return true;
    }

    // The HTTP-EQUIV meta has no effect on XHTML.
    if (m_contentType == XML)
        return true;

    m_charsetParser = makeUnique<HTMLMetaCharsetParser>();
    return checkForMetaCharset(data);
}

bool TextResourceDecoder::checkForMetaCharset(std::span<const uint8_t> data)
{
    if (!m_charsetParser->checkForMetaCharset(data))
        return false;

    setEncoding(m_charsetParser->encoding(), EncodingFromMetaTag);
    m_charsetParser = nullptr;
    m_checkedForHeadCharset = true;
    return true;
}

void TextResourceDecoder::detectJapaneseEncoding(std::span<const uint8_t> data)
{
    switch (KanjiCode::judge(data)) {
    case KanjiCode::Type::JIS:
        setEncoding("ISO-2022-JP"_s, AutoDetectedEncoding);
        break;
    case KanjiCode::Type::EUC:
        setEncoding("EUC-JP"_s, AutoDetectedEncoding);
        break;
    case KanjiCode::Type::SJIS:
        setEncoding("Shift_JIS"_s, AutoDetectedEncoding);
        break;
    case KanjiCode::Type::ASCII:
    case KanjiCode::Type::UTF16:
    case KanjiCode::Type::UTF8:
        break;
    }
}

// We use the encoding detector in two cases:
//   1. Encoding detector is turned ON and no other encoding source is
//      available (that is, it's DefaultEncoding).
//   2. Encoding detector is turned ON and the encoding is set to
//      the encoding of the parent frame, which is also auto-detected.
//   Note that condition #2 is NOT satisfied unless parent-child frame
//   relationship is compliant to the same-origin policy. If they're from
//   different domains, |m_source| would not be set to EncodingFromParentFrame
//   in the first place. 
bool TextResourceDecoder::shouldAutoDetect() const
{
    return m_usesEncodingDetector
        && (m_source == DefaultEncoding || (m_source == EncodingFromParentFrame && m_parentFrameAutoDetectedEncoding));
}

String TextResourceDecoder::decode(std::span<const uint8_t> data)
{
    size_t lengthOfBOM = 0;
    if (!m_checkedForBOM)
        lengthOfBOM = checkForBOM(data);

    bool movedDataToBuffer = false;

    if (m_contentType == CSS && !m_checkedForCSSCharset)
        if (!checkForCSSCharset(data, movedDataToBuffer))
            return emptyString();

    if ((m_contentType == HTML || m_contentType == XML) && !m_checkedForHeadCharset) // HTML and XML
        if (!checkForHeadCharset(data, movedDataToBuffer))
            return emptyString();

    // FIXME: It is wrong to change the encoding downstream after we have already done some decoding.
    if (shouldAutoDetect()) {
        if (m_encoding.isJapanese())
            detectJapaneseEncoding(data); // FIXME: We should use detectTextEncoding() for all languages.
        else {
            PAL::TextEncoding detectedEncoding;
            if (detectTextEncoding(data, m_parentFrameAutoDetectedEncoding, &detectedEncoding))
                setEncoding(detectedEncoding, AutoDetectedEncoding);
        }
    }

    ASSERT(m_encoding.isValid());

    if (!m_codec)
        m_codec = newTextCodec(m_encoding);

    if (m_buffer.isEmpty())
        return m_codec->decode(data.subspan(lengthOfBOM), false, m_contentType == XML, m_sawError);

    if (!movedDataToBuffer) {
        size_t oldSize = m_buffer.size();
        m_buffer.grow(oldSize + data.size());
        memcpySpan(m_buffer.mutableSpan().subspan(oldSize), data);
    }

    String result = m_codec->decode(m_buffer.subspan(lengthOfBOM), false, m_contentType == XML && !m_useLenientXMLDecoding, m_sawError);
    m_buffer.clear();
    return result;
}

String TextResourceDecoder::flush()
{
    // If we can not identify the encoding even after a document is completely
    // loaded, we need to detect the encoding if other conditions for
    // autodetection is satisfied.
    if (m_buffer.size() && shouldAutoDetect()
        && ((!m_checkedForHeadCharset && (m_contentType == HTML || m_contentType == XML)) || (!m_checkedForCSSCharset && (m_contentType == CSS)))) {
        PAL::TextEncoding detectedEncoding;
        if (detectTextEncoding(m_buffer.span(), m_parentFrameAutoDetectedEncoding, &detectedEncoding))
            setEncoding(detectedEncoding, AutoDetectedEncoding);
    }

    if (!m_codec)
        m_codec = newTextCodec(m_encoding);

    String result = m_codec->decode(m_buffer.span(), true, m_contentType == XML && !m_useLenientXMLDecoding, m_sawError);
    m_buffer.clear();
    m_codec = nullptr;
    m_checkedForBOM = false; // Skip BOM again when re-decoding.
    return result;
}

String TextResourceDecoder::decodeAndFlush(std::span<const uint8_t> data)
{
    auto decoded = decode(data);
    auto result = tryMakeString(decoded, flush());
    if (result.isNull())
        RELEASE_LOG_ERROR(TextDecoding, "TextResourceDecoder::decodeAndFlush() failed, size too large (%zu)", data.size());
    return result;
}

const PAL::TextEncoding* TextResourceDecoder::encodingForURLParsing()
{
    // For UTF-{7,16,32}, we want to use UTF-8 for the query part as
    // we do when submitting a form. A form with GET method
    // has its contents added to a URL as query params and it makes sense
    // to be consistent.
    auto& encoding = m_encoding.encodingForFormSubmissionOrURLParsing();
    if (encoding == PAL::UTF8Encoding())
        return nullptr;
    return &encoding;
}

}
