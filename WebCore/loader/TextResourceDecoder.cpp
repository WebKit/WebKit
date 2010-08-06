/*
    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "DOMImplementation.h"
#include "HTMLNames.h"
#include "TextCodec.h"
#include "TextEncoding.h"
#include "TextEncodingDetector.h"
#include "TextEncodingRegistry.h"
#include <wtf/ASCIICType.h>
#include <wtf/StringExtras.h>

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

// You might think we should put these find functions elsewhere, perhaps with the
// similar functions that operate on UChar, but arguably only the decoder has
// a reason to process strings of char rather than UChar.

static int find(const char* subject, size_t subjectLength, const char* target)
{
    size_t targetLength = strlen(target);
    if (targetLength > subjectLength)
        return -1;
    for (size_t i = 0; i <= subjectLength - targetLength; ++i) {
        bool match = true;
        for (size_t j = 0; j < targetLength; ++j) {
            if (subject[i + j] != target[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }
    return -1;
}

static int findIgnoringCase(const char* subject, size_t subjectLength, const char* target)
{
    size_t targetLength = strlen(target);
    if (targetLength > subjectLength)
        return -1;
#ifndef NDEBUG
    for (size_t i = 0; i < targetLength; ++i)
        ASSERT(isASCIILower(target[i]));
#endif
    for (size_t i = 0; i <= subjectLength - targetLength; ++i) {
        bool match = true;
        for (size_t j = 0; j < targetLength; ++j) {
            if (toASCIILower(subject[i + j]) != target[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return i;
    }
    return -1;
}

static TextEncoding findTextEncoding(const char* encodingName, int length)
{
    Vector<char, 64> buffer(length + 1);
    memcpy(buffer.data(), encodingName, length);
    buffer[length] = '\0';
    return buffer.data();
}

class KanjiCode {
public:
    enum Type { ASCII, JIS, EUC, SJIS, UTF16, UTF8 };
    static enum Type judge(const char* str, int length);
    static const int ESC = 0x1b;
    static const unsigned char sjisMap[256];
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

const unsigned char KanjiCode::sjisMap[256] = {
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

enum KanjiCode::Type KanjiCode::judge(const char* str, int size)
{
    enum Type code;
    int i;
    int bfr = false;            /* Kana Moji */
    int bfk = 0;                /* EUC Kana */
    int sjis = 0;
    int euc = 0;

    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(str);

    code = ASCII;

    i = 0;
    while (i < size) {
        if (ptr[i] == ESC && (size - i >= 3)) {
            if ((ptr[i + 1] == '$' && ptr[i + 2] == 'B')
            || (ptr[i + 1] == '(' && ptr[i + 2] == 'B')) {
                code = JIS;
                goto breakBreak;
            } else if ((ptr[i + 1] == '$' && ptr[i + 2] == '@')
                    || (ptr[i + 1] == '(' && ptr[i + 2] == 'J')) {
                code = JIS;
                goto breakBreak;
            } else if (ptr[i + 1] == '(' && ptr[i + 2] == 'I') {
                code = JIS;
                i += 3;
            } else if (ptr[i + 1] == ')' && ptr[i + 2] == 'I') {
                code = JIS;
                i += 3;
            } else {
                i++;
            }
            bfr = false;
            bfk = 0;
        } else {
            if (ptr[i] < 0x20) {
                bfr = false;
                bfk = 0;
                /* ?? check kudokuten ?? && ?? hiragana ?? */
                if ((i >= 2) && (ptr[i - 2] == 0x81)
                        && (0x41 <= ptr[i - 1] && ptr[i - 1] <= 0x49)) {
                    code = SJIS;
                    sjis += 100;        /* kudokuten */
                } else if ((i >= 2) && (ptr[i - 2] == 0xa1)
                        && (0xa2 <= ptr[i - 1] && ptr[i - 1] <= 0xaa)) {
                    code = EUC;
                    euc += 100;         /* kudokuten */
                } else if ((i >= 2) && (ptr[i - 2] == 0x82) && (0xa0 <= ptr[i - 1])) {
                    sjis += 40;         /* hiragana */
                } else if ((i >= 2) && (ptr[i - 2] == 0xa4) && (0xa0 <= ptr[i - 1])) {
                    euc += 40;          /* hiragana */
                }
            } else {
                /* ?? check hiragana or katana ?? */
                if ((size - i > 1) && (ptr[i] == 0x82) && (0xa0 <= ptr[i + 1])) {
                    sjis++;     /* hiragana */
                } else if ((size - i > 1) && (ptr[i] == 0x83)
                         && (0x40 <= ptr[i + 1] && ptr[i + 1] <= 0x9f)) {
                    sjis++;     /* katakana */
                } else if ((size - i > 1) && (ptr[i] == 0xa4) && (0xa0 <= ptr[i + 1])) {
                    euc++;      /* hiragana */
                } else if ((size - i > 1) && (ptr[i] == 0xa5) && (0xa0 <= ptr[i + 1])) {
                    euc++;      /* katakana */
                }
                if (bfr) {
                    if ((i >= 1) && (0x40 <= ptr[i] && ptr[i] <= 0xa0) && ISkanji(ptr[i - 1])) {
                        code = SJIS;
                        goto breakBreak;
                    } else if ((i >= 1) && (0x81 <= ptr[i - 1] && ptr[i - 1] <= 0x9f) && ((0x40 <= ptr[i] && ptr[i] < 0x7e) || (0x7e < ptr[i] && ptr[i] <= 0xfc))) {
                        code = SJIS;
                        goto breakBreak;
                    } else if ((i >= 1) && (0xfd <= ptr[i] && ptr[i] <= 0xfe) && (0xa1 <= ptr[i - 1] && ptr[i - 1] <= 0xfe)) {
                        code = EUC;
                        goto breakBreak;
                    } else if ((i >= 1) && (0xfd <= ptr[i - 1] && ptr[i - 1] <= 0xfe) && (0xa1 <= ptr[i] && ptr[i] <= 0xfe)) {
                        code = EUC;
                        goto breakBreak;
                    } else if ((i >= 1) && (ptr[i] < 0xa0 || 0xdf < ptr[i]) && (0x8e == ptr[i - 1])) {
                        code = SJIS;
                        goto breakBreak;
                    } else if (ptr[i] <= 0x7f) {
                        code = SJIS;
                        goto breakBreak;
                    } else {
                        if (0xa1 <= ptr[i] && ptr[i] <= 0xa6) {
                            euc++;      /* sjis hankaku kana kigo */
                        } else if (0xa1 <= ptr[i] && ptr[i] <= 0xdf) {
                            ;           /* sjis hankaku kana */
                        } else if (0xa1 <= ptr[i] && ptr[i] <= 0xfe) {
                            euc++;
                        } else if (0x8e == ptr[i]) {
                            euc++;
                        } else if (0x20 <= ptr[i] && ptr[i] <= 0x7f) {
                            sjis++;
                        }
                        bfr = false;
                        bfk = 0;
                    }
                } else if (0x8e == ptr[i]) {
                    if (size - i <= 1) {
                        ;
                    } else if (0xa1 <= ptr[i + 1] && ptr[i + 1] <= 0xdf) {
                        /* EUC KANA or SJIS KANJI */
                        if (bfk == 1) {
                            euc += 100;
                        }
                        bfk++;
                        i++;
                    } else {
                        /* SJIS only */
                        code = SJIS;
                        goto breakBreak;
                    }
                } else if (0x81 <= ptr[i] && ptr[i] <= 0x9f) {
                    /* SJIS only */
                    code = SJIS;
                    if ((size - i >= 1)
                            && ((0x40 <= ptr[i + 1] && ptr[i + 1] <= 0x7e)
                            || (0x80 <= ptr[i + 1] && ptr[i + 1] <= 0xfc))) {
                        goto breakBreak;
                    }
                } else if (0xfd <= ptr[i] && ptr[i] <= 0xfe) {
                    /* EUC only */
                    code = EUC;
                    if ((size - i >= 1)
                            && (0xa1 <= ptr[i + 1] && ptr[i + 1] <= 0xfe)) {
                        goto breakBreak;
                    }
                } else if (ptr[i] <= 0x7f) {
                    ;
                } else {
                    bfr = true;
                    bfk = 0;
                }
            }
            i++;
        }
    }
    if (code == ASCII) {
        if (sjis > euc) {
            code = SJIS;
        } else if (sjis < euc) {
            code = EUC;
        }
    }
breakBreak:
    return (code);
}

TextResourceDecoder::ContentType TextResourceDecoder::determineContentType(const String& mimeType)
{
    if (equalIgnoringCase(mimeType, "text/css"))
        return CSS;
    if (equalIgnoringCase(mimeType, "text/html"))
        return HTML;
    if (DOMImplementation::isXMLMIMEType(mimeType))
        return XML;
    return PlainText;
}

const TextEncoding& TextResourceDecoder::defaultEncoding(ContentType contentType, const TextEncoding& specifiedDefaultEncoding)
{
    // Despite 8.5 "Text/xml with Omitted Charset" of RFC 3023, we assume UTF-8 instead of US-ASCII 
    // for text/xml. This matches Firefox.
    if (contentType == XML)
        return UTF8Encoding();
    if (!specifiedDefaultEncoding.isValid())
        return Latin1Encoding();
    return specifiedDefaultEncoding;
}

TextResourceDecoder::TextResourceDecoder(const String& mimeType, const TextEncoding& specifiedDefaultEncoding, bool usesEncodingDetector)
    : m_contentType(determineContentType(mimeType))
    , m_encoding(defaultEncoding(m_contentType, specifiedDefaultEncoding))
    , m_source(DefaultEncoding)
    , m_hintEncoding(0)
    , m_checkedForBOM(false)
    , m_checkedForCSSCharset(false)
    , m_checkedForHeadCharset(false)
    , m_useLenientXMLDecoding(false)
    , m_sawError(false)
    , m_usesEncodingDetector(usesEncodingDetector)
{
}

TextResourceDecoder::~TextResourceDecoder()
{
}

void TextResourceDecoder::setEncoding(const TextEncoding& encoding, EncodingSource source)
{
    // In case the encoding didn't exist, we keep the old one (helps some sites specifying invalid encodings).
    if (!encoding.isValid())
        return;

    // When encoding comes from meta tag (i.e. it cannot be XML files sent via XHR),
    // treat x-user-defined as windows-1252 (bug 18270)
    if (source == EncodingFromMetaTag && strcasecmp(encoding.name(), "x-user-defined") == 0)
        m_encoding = "windows-1252";
    else if (source == EncodingFromMetaTag || source == EncodingFromXMLHeader || source == EncodingFromCSSCharset)        
        m_encoding = encoding.closestByteBasedEquivalent();
    else
        m_encoding = encoding;

    m_codec.clear();
    m_source = source;
}

// Returns the position of the encoding string.
static int findXMLEncoding(const char* str, int len, int& encodingLength)
{
    int pos = find(str, len, "encoding");
    if (pos == -1)
        return -1;
    pos += 8;
    
    // Skip spaces and stray control characters.
    while (pos < len && str[pos] <= ' ')
        ++pos;

    // Skip equals sign.
    if (pos >= len || str[pos] != '=')
        return -1;
    ++pos;

    // Skip spaces and stray control characters.
    while (pos < len && str[pos] <= ' ')
        ++pos;

    // Skip quotation mark.
    if (pos >= len)
        return - 1;
    char quoteMark = str[pos];
    if (quoteMark != '"' && quoteMark != '\'')
        return -1;
    ++pos;

    // Find the trailing quotation mark.
    int end = pos;
    while (end < len && str[end] != quoteMark)
        ++end;
    if (end >= len)
        return -1;

    encodingLength = end - pos;
    return pos;
}

// true if there is more to parse
static inline bool skipWhitespace(const char*& pos, const char* dataEnd)
{
    while (pos < dataEnd && (*pos == '\t' || *pos == ' '))
        ++pos;
    return pos != dataEnd;
}

size_t TextResourceDecoder::checkForBOM(const char* data, size_t len)
{
    // Check for UTF-16/32 or UTF-8 BOM mark at the beginning, which is a sure sign of a Unicode encoding.
    // We let it override even a user-chosen encoding.
    ASSERT(!m_checkedForBOM);

    size_t lengthOfBOM = 0;

    size_t bufferLength = m_buffer.size();

    size_t buf1Len = bufferLength;
    size_t buf2Len = len;
    const unsigned char* buf1 = reinterpret_cast<const unsigned char*>(m_buffer.data());
    const unsigned char* buf2 = reinterpret_cast<const unsigned char*>(data);
    unsigned char c1 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c2 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c3 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c4 = buf2Len ? (--buf2Len, *buf2++) : 0;

    // Check for the BOM.
    if (c1 == 0xFF && c2 == 0xFE) {
        if (c3 != 0 || c4 != 0) {
            setEncoding(UTF16LittleEndianEncoding(), AutoDetectedEncoding);
            lengthOfBOM = 2;
        } else {
            setEncoding(UTF32LittleEndianEncoding(), AutoDetectedEncoding);
            lengthOfBOM = 4;
        }
    } else if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
        setEncoding(UTF8Encoding(), AutoDetectedEncoding);
        lengthOfBOM = 3;
    } else if (c1 == 0xFE && c2 == 0xFF) {
        setEncoding(UTF16BigEndianEncoding(), AutoDetectedEncoding);
        lengthOfBOM = 2;
    } else if (c1 == 0 && c2 == 0 && c3 == 0xFE && c4 == 0xFF) {
        setEncoding(UTF32BigEndianEncoding(), AutoDetectedEncoding);
        lengthOfBOM = 4;
    }

    if (lengthOfBOM || bufferLength + len >= 4)
        m_checkedForBOM = true;

    return lengthOfBOM;
}

bool TextResourceDecoder::checkForCSSCharset(const char* data, size_t len, bool& movedDataToBuffer)
{
    if (m_source != DefaultEncoding && m_source != EncodingFromParentFrame) {
        m_checkedForCSSCharset = true;
        return true;
    }

    size_t oldSize = m_buffer.size();
    m_buffer.grow(oldSize + len);
    memcpy(m_buffer.data() + oldSize, data, len);

    movedDataToBuffer = true;

    if (m_buffer.size() > 8) { // strlen("@charset") == 8
        const char* dataStart = m_buffer.data();
        const char* dataEnd = dataStart + m_buffer.size();

        if (dataStart[0] == '@' && dataStart[1] == 'c' && dataStart[2] == 'h' && dataStart[3] == 'a' && dataStart[4] == 'r' && 
            dataStart[5] == 's' && dataStart[6] == 'e' && dataStart[7] == 't') {
    
            dataStart += 8;
            const char* pos = dataStart;
            if (!skipWhitespace(pos, dataEnd))
                return false;

            if (*pos == '"' || *pos == '\'') {
                char quotationMark = *pos;
                ++pos;
                dataStart = pos;
            
                while (pos < dataEnd && *pos != quotationMark)
                    ++pos;
                if (pos == dataEnd)
                    return false;

                int encodingNameLength = pos - dataStart;
                
                ++pos;
                if (!skipWhitespace(pos, dataEnd))
                    return false;

                if (*pos == ';')
                    setEncoding(findTextEncoding(dataStart, encodingNameLength), EncodingFromCSSCharset);
            }
        }
        m_checkedForCSSCharset = true;
        return true;
    }
    return false;
}

// Other browsers allow comments in the head section, so we need to also.
// It's important not to look for tags inside the comments.
static inline void skipComment(const char*& ptr, const char* pEnd)
{
    const char* p = ptr;
    if (p == pEnd)
      return;
    // Allow <!-->; other browsers do.
    if (*p == '>') {
        p++;
    } else {
        while (p + 2 < pEnd) {
            if (*p == '-') {
                // This is the real end of comment, "-->".
                if (p[1] == '-' && p[2] == '>') {
                    p += 3;
                    break;
                }
                // This is the incorrect end of comment that other browsers allow, "--!>".
                if (p + 3 < pEnd && p[1] == '-' && p[2] == '!' && p[3] == '>') {
                    p += 4;
                    break;
                }
            }
            p++;
        }
    }
    ptr = p;
}

const int bytesToCheckUnconditionally = 1024; // That many input bytes will be checked for meta charset even if <head> section is over.

bool TextResourceDecoder::checkForHeadCharset(const char* data, size_t len, bool& movedDataToBuffer)
{
    if (m_source != DefaultEncoding && m_source != EncodingFromParentFrame) {
        m_checkedForHeadCharset = true;
        return true;
    }

    // This is not completely efficient, since the function might go
    // through the HTML head several times.

    size_t oldSize = m_buffer.size();
    m_buffer.grow(oldSize + len);
    memcpy(m_buffer.data() + oldSize, data, len);

    movedDataToBuffer = true;

    const char* ptr = m_buffer.data();
    const char* pEnd = ptr + m_buffer.size();

    // Is there enough data available to check for XML declaration?
    if (m_buffer.size() < 8)
        return false;

    // Handle XML declaration, which can have encoding in it. This encoding is honored even for HTML documents.
    // It is an error for an XML declaration not to be at the start of an XML document, and it is ignored in HTML documents in such case.
    if (ptr[0] == '<' && ptr[1] == '?' && ptr[2] == 'x' && ptr[3] == 'm' && ptr[4] == 'l') {
        const char* xmlDeclarationEnd = ptr;
        while (xmlDeclarationEnd != pEnd && *xmlDeclarationEnd != '>')
            ++xmlDeclarationEnd;
        if (xmlDeclarationEnd == pEnd)
            return false;
        // No need for +1, because we have an extra "?" to lose at the end of XML declaration.
        int len = 0;
        int pos = findXMLEncoding(ptr, xmlDeclarationEnd - ptr, len);
        if (pos != -1)
            setEncoding(findTextEncoding(ptr + pos, len), EncodingFromXMLHeader);
        // continue looking for a charset - it may be specified in an HTTP-Equiv meta
    } else if (ptr[0] == '<' && ptr[1] == 0 && ptr[2] == '?' && ptr[3] == 0 && ptr[4] == 'x' && ptr[5] == 0) {
        setEncoding(UTF16LittleEndianEncoding(), AutoDetectedEncoding);
        return true;
    } else if (ptr[0] == 0 && ptr[1] == '<' && ptr[2] == 0 && ptr[3] == '?' && ptr[4] == 0 && ptr[5] == 'x') {
        setEncoding(UTF16BigEndianEncoding(), AutoDetectedEncoding);
        return true;
    } else if (ptr[0] == '<' && ptr[1] == 0 && ptr[2] == 0 && ptr[3] == 0 && ptr[4] == '?' && ptr[5] == 0 && ptr[6] == 0 && ptr[7] == 0) {
        setEncoding(UTF32LittleEndianEncoding(), AutoDetectedEncoding);
        return true;
    } else if (ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 0 && ptr[3] == '<' && ptr[4] == 0 && ptr[5] == 0 && ptr[6] == 0 && ptr[7] == '?') {
        setEncoding(UTF32BigEndianEncoding(), AutoDetectedEncoding);
        return true;
    }

    // we still don't have an encoding, and are in the head
    // the following tags are allowed in <head>:
    // SCRIPT|STYLE|META|LINK|OBJECT|TITLE|BASE
    
    // We stop scanning when a tag that is not permitted in <head>
    // is seen, rather when </head> is seen, because that more closely
    // matches behavior in other browsers; more details in
    // <http://bugs.webkit.org/show_bug.cgi?id=3590>.
    
    // Additionally, we ignore things that looks like tags in <title>, <script> and <noscript>; see
    // <http://bugs.webkit.org/show_bug.cgi?id=4560>, <http://bugs.webkit.org/show_bug.cgi?id=12165>
    // and <http://bugs.webkit.org/show_bug.cgi?id=12389>.

    // Since many sites have charset declarations after <body> or other tags that are disallowed in <head>,
    // we don't bail out until we've checked at least bytesToCheckUnconditionally bytes of input.

    AtomicStringImpl* enclosingTagName = 0;
    bool inHeadSection = true; // Becomes false when </head> or any tag not allowed in head is encountered.

    // the HTTP-EQUIV meta has no effect on XHTML
    if (m_contentType == XML)
        return true;

    while (ptr + 3 < pEnd) { // +3 guarantees that "<!--" fits in the buffer - and certainly we aren't going to lose any "charset" that way.
        if (*ptr == '<') {
            bool end = false;
            ptr++;

            // Handle comments.
            if (ptr[0] == '!' && ptr[1] == '-' && ptr[2] == '-') {
                ptr += 3;
                skipComment(ptr, pEnd);
                if (ptr - m_buffer.data() >= bytesToCheckUnconditionally && !inHeadSection) {
                    // Some pages that test bandwidth from within the browser do it by having
                    // huge comments and measuring the time they take to load. Repeatedly scanning
                    // these comments can take a lot of CPU time.
                    m_checkedForHeadCharset = true;
                    return true;
                }
                continue;
            }

            if (*ptr == '/') {
                ++ptr;
                end = true;
            }

            // Grab the tag name, but mostly ignore namespaces.
            bool sawNamespace = false;
            char tagBuffer[20];
            int len = 0;
            while (len < 19) {
                if (ptr == pEnd)
                    return false;
                char c = *ptr;
                if (c == ':') {
                    len = 0;
                    sawNamespace = true;
                    ptr++;
                    continue;
                }
                if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                    ;
                else if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
                else
                    break;
                tagBuffer[len++] = c;
                ptr++;
            }
            tagBuffer[len] = 0;
            AtomicString tag(tagBuffer);
            
            if (enclosingTagName) {
                if (end && tag.impl() == enclosingTagName)
                    enclosingTagName = 0;
            } else {
                if (tag == titleTag)
                    enclosingTagName = titleTag.localName().impl();
                else if (tag == scriptTag)
                    enclosingTagName = scriptTag.localName().impl();
                else if (tag == noscriptTag)
                    enclosingTagName = noscriptTag.localName().impl();
            }
            
            // Find where the opening tag ends.
            const char* tagContentStart = ptr;
            if (!end) {
                while (ptr != pEnd && *ptr != '>') {
                    if (*ptr == '\'' || *ptr == '"') {
                        char quoteMark = *ptr;
                        ++ptr;
                        while (ptr != pEnd && *ptr != quoteMark)
                            ++ptr;
                        if (ptr == pEnd)
                            return false;
                    }
                    ++ptr;
                }
                if (ptr == pEnd)
                    return false;
                ++ptr;
            }
            
            if (!end && tag == metaTag && !sawNamespace) {
                const char* str = tagContentStart;
                int length = ptr - tagContentStart;
                int pos = 0;
                while (pos < length) {
                    int charsetPos = findIgnoringCase(str + pos, length - pos, "charset");
                    if (charsetPos == -1)
                        break;
                    pos += charsetPos + 7;
                    // skip whitespace
                    while (pos < length && str[pos] <= ' ')
                        pos++;
                    if (pos == length)
                        break;
                    if (str[pos++] != '=')
                        continue;
                    while ((pos < length) &&
                            (str[pos] <= ' ' || str[pos] == '=' || str[pos] == '"' || str[pos] == '\''))
                        pos++;

                    // end ?
                    if (pos == length)
                        break;
                    int end = pos;
                    while (end < length &&
                           str[end] != ' ' && str[end] != '"' && str[end] != '\'' &&
                           str[end] != ';' && str[end] != '>')
                        end++;
                    setEncoding(findTextEncoding(str + pos, end - pos), EncodingFromMetaTag);
                    if (m_source == EncodingFromMetaTag)
                        return true;

                    if (end >= length || str[end] == '/' || str[end] == '>')
                        break;

                    pos = end + 1;
                }
            } else {
                if (!enclosingTagName && tag != scriptTag && tag != noscriptTag && tag != styleTag
                    && tag != linkTag && tag != metaTag && tag != objectTag && tag != titleTag && tag != baseTag
                    && (end || tag != htmlTag) && (end || tag != headTag) && isASCIIAlpha(tagBuffer[0])) {
                    inHeadSection = false;
                }

                if (ptr - m_buffer.data() >= bytesToCheckUnconditionally && !inHeadSection) {
                    m_checkedForHeadCharset = true;
                    return true;
                }
            }
        } else
            ++ptr;
    }
    return false;
}

void TextResourceDecoder::detectJapaneseEncoding(const char* data, size_t len)
{
    switch (KanjiCode::judge(data, len)) {
        case KanjiCode::JIS:
            setEncoding("ISO-2022-JP", AutoDetectedEncoding);
            break;
        case KanjiCode::EUC:
            setEncoding("EUC-JP", AutoDetectedEncoding);
            break;
        case KanjiCode::SJIS:
            setEncoding("Shift_JIS", AutoDetectedEncoding);
            break;
        case KanjiCode::ASCII:
        case KanjiCode::UTF16:
        case KanjiCode::UTF8:
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
    // Just checking m_hintEncoding suffices here because it's only set
    // in setHintEncoding when the source is AutoDetectedEncoding.
    return m_usesEncodingDetector
        && (m_source == DefaultEncoding || (m_source == EncodingFromParentFrame && m_hintEncoding)); 
}

String TextResourceDecoder::decode(const char* data, size_t len)
{
    size_t lengthOfBOM = 0;
    if (!m_checkedForBOM)
        lengthOfBOM = checkForBOM(data, len);

    bool movedDataToBuffer = false;

    if (m_contentType == CSS && !m_checkedForCSSCharset)
        if (!checkForCSSCharset(data, len, movedDataToBuffer))
            return "";

    if ((m_contentType == HTML || m_contentType == XML) && !m_checkedForHeadCharset) // HTML and XML
        if (!checkForHeadCharset(data, len, movedDataToBuffer))
            return "";

    // FIXME: It is wrong to change the encoding downstream after we have already done some decoding.
    if (shouldAutoDetect()) {
        if (m_encoding.isJapanese())
            detectJapaneseEncoding(data, len); // FIXME: We should use detectTextEncoding() for all languages.
        else {
            TextEncoding detectedEncoding;
            if (detectTextEncoding(data, len, m_hintEncoding, &detectedEncoding))
                setEncoding(detectedEncoding, AutoDetectedEncoding);
        }
    }

    ASSERT(m_encoding.isValid());

    if (!m_codec)
        m_codec = newTextCodec(m_encoding);

    if (m_buffer.isEmpty())
        return m_codec->decode(data + lengthOfBOM, len - lengthOfBOM, false, m_contentType == XML, m_sawError);

    if (!movedDataToBuffer) {
        size_t oldSize = m_buffer.size();
        m_buffer.grow(oldSize + len);
        memcpy(m_buffer.data() + oldSize, data, len);
    }

    String result = m_codec->decode(m_buffer.data() + lengthOfBOM, m_buffer.size() - lengthOfBOM, false, m_contentType == XML && !m_useLenientXMLDecoding, m_sawError);
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
         TextEncoding detectedEncoding;
         if (detectTextEncoding(m_buffer.data(), m_buffer.size(),
                                m_hintEncoding, &detectedEncoding))
             setEncoding(detectedEncoding, AutoDetectedEncoding);
    }

    if (!m_codec)
        m_codec = newTextCodec(m_encoding);

    String result = m_codec->decode(m_buffer.data(), m_buffer.size(), true, m_contentType == XML && !m_useLenientXMLDecoding, m_sawError);
    m_buffer.clear();
    m_codec.clear();
    m_checkedForBOM = false; // Skip BOM again when re-decoding.
    return result;
}

}
