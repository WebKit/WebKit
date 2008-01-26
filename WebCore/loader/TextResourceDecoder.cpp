/*
    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "CString.h"
#include "DOMImplementation.h"
#include "DeprecatedCString.h"
#include "HTMLNames.h"
#include "TextCodec.h"
#include <wtf/ASCIICType.h>

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

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

TextResourceDecoder::TextResourceDecoder(const String& mimeType, const TextEncoding& specifiedDefaultEncoding)
    : m_contentType(determineContentType(mimeType))
    , m_decoder(defaultEncoding(m_contentType, specifiedDefaultEncoding))
    , m_source(DefaultEncoding)
    , m_checkedForBOM(false)
    , m_checkedForCSSCharset(false)
    , m_checkedForHeadCharset(false)
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

    if (source == EncodingFromMetaTag || source == EncodingFromXMLHeader || source == EncodingFromCSSCharset)        
        m_decoder.reset(encoding.closest8BitEquivalent());
    else
        m_decoder.reset(encoding);

    m_source = source;
}

// Returns the position of the encoding string.
static int findXMLEncoding(const DeprecatedCString &str, int &encodingLength)
{
    int len = str.length();

    int pos = str.find("encoding");
    if (pos == -1)
        return -1;
    pos += 8;
    
    // Skip spaces and stray control characters.
    while (str[pos] <= ' ' && pos != len)
        ++pos;

    // Skip equals sign.
    if (str[pos] != '=')
        return -1;
    ++pos;

    // Skip spaces and stray control characters.
    while (str[pos] <= ' ' && pos != len)
        ++pos;

    // Skip quotation mark.
    char quoteMark = str[pos];
    if (quoteMark != '"' && quoteMark != '\'')
        return -1;
    ++pos;

    // Find the trailing quotation mark.
    int end = pos;
    while (str[end] != quoteMark)
        ++end;

    if (end == len)
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

void TextResourceDecoder::checkForBOM(const char* data, size_t len)
{
    // Check for UTF-16/32 or UTF-8 BOM mark at the beginning, which is a sure sign of a Unicode encoding.

    if (m_source == UserChosenEncoding) {
        // FIXME: Maybe a BOM should override even a user-chosen encoding.
        m_checkedForBOM = true;
        return;
    }

    // Check if we have enough data.
    size_t bufferLength = m_buffer.size();
    if (bufferLength + len < 4)
        return;

    m_checkedForBOM = true;

    // Extract the first four bytes.
    // Handle the case where some of bytes are already in the buffer.
    // The last byte is always guaranteed to not be in the buffer.
    const unsigned char* udata = reinterpret_cast<const unsigned char*>(data);
    unsigned char c1 = bufferLength >= 1 ? m_buffer[0] : *udata++;
    unsigned char c2 = bufferLength >= 2 ? m_buffer[1] : *udata++;
    unsigned char c3 = bufferLength >= 3 ? m_buffer[2] : *udata++;
    ASSERT(bufferLength < 4);
    unsigned char c4 = *udata;

    // Check for the BOM.
    if (c1 == 0xFF && c2 == 0xFE) {
        if (c3 !=0 || c4 != 0)
            setEncoding(UTF16LittleEndianEncoding(), AutoDetectedEncoding);
        else 
            setEncoding(UTF32LittleEndianEncoding(), AutoDetectedEncoding);
    }
    else if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)
        setEncoding(UTF8Encoding(), AutoDetectedEncoding);
    else if (c1 == 0xFE && c2 == 0xFF)
        setEncoding(UTF16BigEndianEncoding(), AutoDetectedEncoding);
    else if (c1 == 0 && c2 == 0 && c3 == 0xFE && c4 == 0xFF)
        setEncoding(UTF32BigEndianEncoding(), AutoDetectedEncoding);
}

bool TextResourceDecoder::checkForCSSCharset(const char* data, size_t len, bool& movedDataToBuffer)
{
    if (m_source != DefaultEncoding) {
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

                CString encodingName(dataStart, pos - dataStart + 1);
                
                ++pos;
                if (!skipWhitespace(pos, dataEnd))
                    return false;

                if (*pos == ';')
                    setEncoding(TextEncoding(encodingName.data()), EncodingFromCSSCharset);
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
    // Allow <!-->; other browsers do.
    if (*p == '>') {
        p++;
    } else {
        while (p != pEnd) {
            if (*p == '-') {
                // This is the real end of comment, "-->".
                if (p[1] == '-' && p[2] == '>') {
                    p += 3;
                    break;
                }
                // This is the incorrect end of comment that other browsers allow, "--!>".
                if (p[1] == '-' && p[2] == '!' && p[3] == '>') {
                    p += 4;
                    break;
                }
            }
            p++;
        }
    }
    ptr = p;
}

bool TextResourceDecoder::checkForHeadCharset(const char* data, size_t len, bool& movedDataToBuffer)
{
    if (m_source != DefaultEncoding) {
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
        DeprecatedCString str(ptr, xmlDeclarationEnd - ptr); // No need for +1, because we have an extra "?" to lose at the end of XML declaration.
        int len = 0;
        int pos = findXMLEncoding(str, len);
        if (pos != -1)
            setEncoding(TextEncoding(str.mid(pos, len)), EncodingFromXMLHeader);
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
    // we don't bail out until we've checked at least 512 bytes of input.

    AtomicStringImpl* enclosingTagName = 0;

    while (ptr + 3 < pEnd) { // +3 guarantees that "<!--" fits in the buffer - and certainly we aren't going to lose any "charset" that way.
        if (*ptr == '<') {
            bool end = false;
            ptr++;

            // Handle comments.
            if (ptr[0] == '!' && ptr[1] == '-' && ptr[2] == '-') {
                ptr += 3;
                skipComment(ptr, pEnd);
                continue;
            }

            // the HTTP-EQUIV meta has no effect on XHTML
            if (m_contentType == XML)
                return true;

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
                if (c >= 'a' && c <= 'z' || c >= '0' && c <= '9')
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
                DeprecatedCString str(tagContentStart, ptr - tagContentStart);
                str = str.lower();
                int pos = 0;
                while (pos < (int)str.length()) {
                    if ((pos = str.find("charset", pos, false)) == -1)
                        break;
                    pos += 7;
                    // skip whitespace
                    while (pos < (int)str.length() && str[pos] <= ' ')
                        pos++;
                    if (pos == (int)str.length())
                        break;
                    if (str[pos++] != '=')
                        continue;
                    while (pos < (int)str.length() &&
                            (str[pos] <= ' ') || str[pos] == '=' || str[pos] == '"' || str[pos] == '\'')
                        pos++;

                    // end ?
                    if (pos == (int)str.length())
                        break;
                    unsigned endpos = pos;
                    while (endpos < str.length() &&
                           str[endpos] != ' ' && str[endpos] != '"' && str[endpos] != '\'' &&
                           str[endpos] != ';' && str[endpos] != '>')
                        endpos++;
                    setEncoding(TextEncoding(str.mid(pos, endpos - pos)), EncodingFromMetaTag);
                    if (m_source == EncodingFromMetaTag)
                        return true;

                    if (endpos >= str.length() || str[endpos] == '/' || str[endpos] == '>')
                        break;

                    pos = endpos + 1;
                }
            } else if (ptr - m_buffer.data() >= 512 && tag != scriptTag && tag != noscriptTag && tag != styleTag &&
                       tag != linkTag && tag != metaTag && tag != objectTag &&
                       tag != titleTag && tag != baseTag && 
                       (end || tag != htmlTag) && !enclosingTagName &&
                       (tag != headTag) && isASCIIAlpha(tagBuffer[0])) {
                m_checkedForHeadCharset = true;
                return true;
            }
        }
        else
            ptr++;
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

String TextResourceDecoder::decode(const char* data, size_t len)
{
    if (!m_checkedForBOM)
        checkForBOM(data, len);

    bool movedDataToBuffer = false;

    if (m_contentType == CSS && !m_checkedForCSSCharset)
        if (!checkForCSSCharset(data, len, movedDataToBuffer))
            return "";

    if ((m_contentType == HTML || m_contentType == XML) && !m_checkedForHeadCharset) // HTML and XML
        if (!checkForHeadCharset(data, len, movedDataToBuffer))
            return "";

    // Do the auto-detect if our default encoding is one of the Japanese ones.
    // FIXME: It seems wrong to change our encoding downstream after we have already done some decoding.
    if (m_source != UserChosenEncoding && m_source != AutoDetectedEncoding && encoding().isJapanese())
        detectJapaneseEncoding(data, len);

    ASSERT(encoding().isValid());

    if (m_buffer.isEmpty())
        return m_decoder.decode(data, len);

    if (!movedDataToBuffer) {
        size_t oldSize = m_buffer.size();
        m_buffer.grow(oldSize + len);
        memcpy(m_buffer.data() + oldSize, data, len);
    }

    String result = m_decoder.decode(m_buffer.data(), m_buffer.size());
    m_buffer.clear();
    return result;
}

String TextResourceDecoder::flush()
{
    String result = m_decoder.decode(m_buffer.data(), m_buffer.size(), true);
    m_buffer.clear();
    return result;
}

}
