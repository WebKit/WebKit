/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
    Copyright (C) 2005, 2006 Alexey Proskuryakov (ap@nypop.com)

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/


#include "config.h"
#include "Decoder.h"

#include "DOMImplementation.h"
#include "HTMLNames.h"
#include "StreamingTextDecoder.h"
#include "RegularExpression.h"

using namespace WebCore;
using namespace HTMLNames;

class KanjiCode
{
public:
    enum Type { ASCII, JIS, EUC, SJIS, UTF16, UTF8 };
    static enum Type judge(const char *str, int length);
    static const int ESC;
    static const int _SS2_;
    static const unsigned char kanji_map_sjis[];
    static int ISkanji(int code)
    {
        if (code >= 0x100)
                    return 0;
        return (kanji_map_sjis[code & 0xff] & 1);
    }

    static int ISkana(int code)
    {
        if (code >= 0x100)
                    return 0;
        return (kanji_map_sjis[code & 0xff] & 2);
    }

};

const int KanjiCode::ESC = 0x1b;
const int KanjiCode::_SS2_ = 0x8e;

const unsigned char KanjiCode::kanji_map_sjis[] =
{
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

enum KanjiCode::Type KanjiCode::judge(const char *str, int size)
{
    enum Type code;
    int i;
    int bfr = false;            /* Kana Moji */
    int bfk = 0;                /* EUC Kana */
    int sjis = 0;
    int euc = 0;

    const unsigned char *ptr = (const unsigned char *) str;

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

Decoder::Decoder(const String& mimeType, const String& defaultEncodingName)
  : m_encoding(defaultEncodingName.isNull() ? "iso8859-1" : defaultEncodingName.ascii().data())
  , m_encodingName(defaultEncodingName.isNull() ? "iso8859-1" : defaultEncodingName.ascii().data())
  , m_type(DefaultEncoding)
  , m_reachedBody(false)
  , m_checkedForCSSCharset(false)
  , m_checkedForBOM(false)
{
    if (mimeType == "text/css")
        m_contentType = CSS;
    else if (mimeType == "text/html")
        m_contentType = HTML;
    else if (DOMImplementation::isXMLMIMEType(mimeType)) {
        m_contentType = XML;
        // Despite 8.5 "Text/xml with Omitted Charset" of RFC 3023, we do not assume us-ascii 
        // for text/xml, to match Firefox.
        m_encoding = TextEncoding(UTF8Encoding);
        m_encodingName = "UTF-8";
    } else
        m_contentType = PlainText;

    m_decoder.set(StreamingTextDecoder::create(m_encoding));
}

Decoder::~Decoder()
{
}

void Decoder::setEncodingName(const char* encodingName, EncodingSource type)
{
    m_encodingName = encodingName;
    m_encodingName = m_encodingName.lower();

    if (m_encodingName.isEmpty())
        return;

    bool eightBitOnly = type == EncodingFromMetaTag || type == EncodingFromXMLHeader || type == EncodingFromCSSCharset;
    TextEncoding encoding = TextEncoding(m_encodingName, eightBitOnly);

    // in case the encoding didn't exist, we keep the old one (fixes some sites specifying invalid encodings)
    if (encoding.isValid()) {
        m_encodingName = encoding.name();
        m_encoding = encoding;
        m_type = type;
        m_decoder.set(StreamingTextDecoder::create(m_encoding));
    }
}

const char* Decoder::encodingName() const
{
    return m_encodingName;
}

// Other browsers allow comments in the head section, so we need to also.
// It's important not to look for tags inside the comments.
static void skipComment(const char *&ptr, const char *pEnd)
{
    const char *p = ptr;
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

DeprecatedString Decoder::decode(const char *data, int len)
{
    // Check for UTF-16 or UTF-8 BOM mark at the beginning, which is a sure sign of a Unicode encoding.
    int bufferLength = m_buffer.length();
    const int maximumBOMLength = 3;
    if (!m_checkedForBOM && bufferLength + len >= maximumBOMLength) {
        if (m_type != UserChosenEncoding) {
            // Extract the first three bytes.
            // Handle the case where some of bytes are already in the buffer.
            // The last byte is always guaranteed to not be in the buffer.
            const unsigned char *udata = (const unsigned char *)data;
            unsigned char c1 = bufferLength >= 1 ? m_buffer[0].unicode() : *udata++;
            unsigned char c2 = bufferLength >= 2 ? m_buffer[1].unicode() : *udata++;
            ASSERT(bufferLength < 3);
            unsigned char c3 = *udata;

            // Check for the BOM.
            const char *autoDetectedEncoding;
            if ((c1 == 0xFE && c2 == 0xFF) || (c1 == 0xFF && c2 == 0xFE)) {
                autoDetectedEncoding = "ISO-10646-UCS-2";
            } else if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
                autoDetectedEncoding = "UTF-8";
            } else {
                autoDetectedEncoding = 0;
            }

            // If we found a BOM, use the encoding it implies.
            if (autoDetectedEncoding != 0) {
                m_type = AutoDetectedEncoding;
                m_encoding = TextEncoding(autoDetectedEncoding);
                ASSERT(m_encoding.isValid());
                m_encodingName = m_encoding.name();
                m_decoder.set(StreamingTextDecoder::create(m_encoding));
            }
        }
        m_checkedForBOM = true;
    }
    
    bool currentChunkInBuffer = false;
    
    if (m_type == DefaultEncoding && m_contentType == CSS && !m_checkedForCSSCharset) {
        m_buffer.append(data, len);
        currentChunkInBuffer = true;

        if (len > 8) { // strlen("@charset") == 8
            const char* dataStart = m_buffer.latin1();
            const char* dataEnd = dataStart + m_buffer.length();

            if (dataStart[0] == '@' && dataStart[1] == 'c' && dataStart[2] == 'h' && dataStart[3] == 'a' && dataStart[4] == 'r' && 
                dataStart[5] == 's' && dataStart[6] == 'e' && dataStart[7] == 't') {
        
                dataStart += 8;
                const char* pos = dataStart;
                if (!skipWhitespace(pos, dataEnd))
                    return DeprecatedString::null;

                if (*pos == '"' || *pos == '\'') {
                    char quotationMark = *pos;
                    ++pos;
                    dataStart = pos;
                
                    while (pos < dataEnd && *pos != quotationMark)
                        ++pos;
                    if (pos == dataEnd)
                        return DeprecatedString::null;

                    DeprecatedCString encodingName(dataStart, pos - dataStart + 1);
                    
                    ++pos;
                    if (!skipWhitespace(pos, dataEnd))
                        return DeprecatedString::null;

                    if (*pos == ';')
                        setEncodingName(encodingName, EncodingFromCSSCharset);
                }
            }
            m_checkedForCSSCharset = true;
        }
        return DeprecatedString::null;

    } else if (m_type == DefaultEncoding && m_contentType != PlainText && !m_reachedBody) { // HTML and XML
        // this is not completely efficient, since the function might go
        // through the html head several times...
    
        m_buffer.append(data, len);
        currentChunkInBuffer = true;
        
        // we still don't have an encoding, and are in the head
        // the following tags are allowed in <head>:
        // SCRIPT|STYLE|META|LINK|OBJECT|TITLE|BASE
        
        // We stop scanning when a tag that is not permitted in <head>
        // is seen, rather when </head> is seen, because that more closely
        // matches behavior in other browsers; more details in
        // <http://bugzilla.opendarwin.org/show_bug.cgi?id=3590>.
        
        // Additionally, we ignore things that looks like tags in <title>; see
        // <http://bugzilla.opendarwin.org/show_bug.cgi?id=4560>.
        
        bool withinTitle = false;

        const char *ptr = m_buffer.latin1();
        const char *pEnd = ptr + m_buffer.length();
        while (ptr != pEnd) {
            if (*ptr == '<') {
                bool end = false;
                ptr++;

                // Handle comments.
                if (ptr[0] == '!' && ptr[1] == '-' && ptr[2] == '-') {
                    ptr += 3;
                    skipComment(ptr, pEnd);
                    continue;
                }
                
                // Handle XML declaration, which can have encoding in it.
                // This encoding is honored even for HTML documents.
                if (ptr[0] == '?' && ptr[1] == 'x' && ptr[2] == 'm' && ptr[3] == 'l') {
                    const char *end = ptr;
                    while (*end != '>' && *end != '\0')
                        end++;
                    if (*end == '\0')
                        break;
                    DeprecatedCString str(ptr, end - ptr);
                    int len;
                    int pos = findXMLEncoding(str, len);
                    if (pos != -1)
                        setEncodingName(str.mid(pos, len), EncodingFromXMLHeader);
                    // continue looking for a charset - it may be specified in an HTTP-Equiv meta
                } else if (ptr[0] == 0 && ptr[1] == '?' && ptr[2] == 0 && ptr[3] == 'x' && ptr[4] == 0 && ptr[5] == 'm' && ptr[6] == 0 && ptr[7] == 'l') {
                    // UTF-16 without BOM
                    setEncodingName(((ptr - m_buffer.latin1()) % 2) ? "UTF-16LE" : "UTF-16BE", AutoDetectedEncoding);
                    goto found;
                }
                
                // the HTTP-EQUIV meta has no effect on XHTML
                if (m_contentType == XML)
                    goto found;

                if (*ptr == '/') {
                    ++ptr;
                    end=true;
                }

                char tmp[20];
                int len = 0;
                while (
                    ((*ptr >= 'a') && (*ptr <= 'z') ||
                     (*ptr >= 'A') && (*ptr <= 'Z') ||
                     (*ptr >= '0') && (*ptr <= '9'))
                    && len < 19 )
                {
                    tmp[len] = tolower(*ptr);
                    ptr++;
                    len++;
                }
                tmp[len] = 0;
                AtomicString tag(tmp);
                
                if (tag == titleTag)
                    withinTitle = !end;
                
                if (!end && tag == metaTag) {
                    const char* end = ptr;
                    while (*end != '>' && *end != '\0')
                        end++;
                    if (*end == '\0')
                        break;
                    DeprecatedCString str(ptr, (end-ptr)+1);
                    str = str.lower();
                    int pos = 0;
                    while (pos < (int)str.length()) {
                        if ((pos = str.find("charset", pos, false)) == -1)
                            break;
                        pos += 7;
                        // skip whitespace..
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
                        setEncodingName(str.mid(pos, endpos-pos), EncodingFromMetaTag);
                        if (m_type == EncodingFromMetaTag)
                            goto found;

                        if (endpos >= str.length() || str[endpos] == '/' || str[endpos] == '>')
                            break;

                        pos = endpos + 1;
                    }
                } else if (tag != scriptTag && tag != noscriptTag && tag != styleTag &&
                           tag != linkTag && tag != metaTag && tag != objectTag &&
                           tag != titleTag && tag != baseTag && 
                           (end || tag != htmlTag) && !withinTitle &&
                           (tag != headTag) && isalpha(tmp[0])) {
                    m_reachedBody = true;
                    goto found;
                }
            }
            else
                ptr++;
        }
        return DeprecatedString::null;
    }

 found:
    // Do the auto-detect if our default encoding is one of the Japanese ones.
    if (m_type != UserChosenEncoding && m_type != AutoDetectedEncoding && m_encoding.isJapanese()) {
        const char *autoDetectedEncoding;
        switch (KanjiCode::judge(data, len)) {
            case KanjiCode::JIS:
                autoDetectedEncoding = "jis7";
                break;
            case KanjiCode::EUC:
                autoDetectedEncoding = "eucjp";
                break;
            case KanjiCode::SJIS:
                autoDetectedEncoding = "sjis";
                break;
            default:
                autoDetectedEncoding = NULL;
                break;
        }
        if (autoDetectedEncoding)
            setEncodingName(autoDetectedEncoding, AutoDetectedEncoding);
    }

    // If we still haven't found an encoding, assume latin1
    // (this can happen if an empty name is passed from outside).
    if (m_encodingName.isEmpty() || !m_encoding.isValid()) {
        m_encodingName = "iso8859-1";
        m_encoding = TextEncoding(Latin1Encoding);
    }
    m_decoder.set(StreamingTextDecoder::create(m_encoding));

    DeprecatedString out;

    if (!m_buffer.isEmpty()) {
        if (!currentChunkInBuffer)
            m_buffer.append(data, len);
        out = m_decoder->toUnicode(m_buffer.latin1(), m_buffer.length());
        m_buffer.truncate(0);
    } else
        out = m_decoder->toUnicode(data, len);

    return out;
}

DeprecatedString Decoder::flush() const
{
    return m_decoder->toUnicode(m_buffer.latin1(), m_buffer.length(), true);
}

// -----------------------------------------------------------------------------
