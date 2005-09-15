/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2003 Apple Computer, Inc.

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
//----------------------------------------------------------------------------
//
// KDE HTML Widget -- decoder for input stream

//#define DECODE_DEBUG

#include "decoder.h"
using namespace khtml;

#include <qregexp.h>
#include <qtextcodec.h>

#include <kglobal.h>
#include <kcharsets.h>

#include <ctype.h>
#include <kdebug.h>
#include <klocale.h>

#include "htmlnames.h"

using namespace DOM::HTMLNames;

using DOM::AtomicString;
using DOM::nullAtom;

class KanjiCode
{
public:
    enum Type {ASCII, JIS, EUC, SJIS, UNICODE, UTF8 };
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

/*
 * Maybe we should use QTextCodec::heuristicContentMatch()
 * But it fails detection. It's not useful.
 */

enum KanjiCode::Type KanjiCode::judge(const char *str, int size)
{
    enum Type code;
    int i;
    int bfr = FALSE;		/* Kana Moji */
    int bfk = 0;		/* EUC Kana */
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
	    bfr = FALSE;
	    bfk = 0;
	} else {
	    if (ptr[i] < 0x20) {
		bfr = FALSE;
		bfk = 0;
		/* ?? check kudokuten ?? && ?? hiragana ?? */
		if ((i >= 2) && (ptr[i - 2] == 0x81)
			&& (0x41 <= ptr[i - 1] && ptr[i - 1] <= 0x49)) {
		    code = SJIS;
		    sjis += 100;	/* kudokuten */
		} else if ((i >= 2) && (ptr[i - 2] == 0xa1)
			&& (0xa2 <= ptr[i - 1] && ptr[i - 1] <= 0xaa)) {
		    code = EUC;
		    euc += 100;		/* kudokuten */
		} else if ((i >= 2) && (ptr[i - 2] == 0x82) && (0xa0 <= ptr[i - 1])) {
		    sjis += 40;		/* hiragana */
		} else if ((i >= 2) && (ptr[i - 2] == 0xa4) && (0xa0 <= ptr[i - 1])) {
		    euc += 40;	/* hiragana */
		}
	    } else {
		/* ?? check hiragana or katana ?? */
		if ((size - i > 1) && (ptr[i] == 0x82) && (0xa0 <= ptr[i + 1])) {
		    sjis++;	/* hiragana */
		} else if ((size - i > 1) && (ptr[i] == 0x83)
			 && (0x40 <= ptr[i + 1] && ptr[i + 1] <= 0x9f)) {
		    sjis++;	/* katakana */
		} else if ((size - i > 1) && (ptr[i] == 0xa4) && (0xa0 <= ptr[i + 1])) {
		    euc++;	/* hiragana */
		} else if ((size - i > 1) && (ptr[i] == 0xa5) && (0xa0 <= ptr[i + 1])) {
		    euc++;	/* katakana */
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
			    euc++;	/* sjis hankaku kana kigo */
			} else if (0xa1 <= ptr[i] && ptr[i] <= 0xdf) {
			    ;	/* sjis hankaku kana */
			} else if (0xa1 <= ptr[i] && ptr[i] <= 0xfe) {
			    euc++;
			} else if (0x8e == ptr[i]) {
			    euc++;
			} else if (0x20 <= ptr[i] && ptr[i] <= 0x7f) {
			    sjis++;
			}
			bfr = FALSE;
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
		    bfr = TRUE;
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

Decoder::Decoder() 
{
    _refCount = 1;
    // latin1
    m_codec = QTextCodec::codecForName("iso8859-1");
    m_decoder = m_codec->makeDecoder();
    enc = 0;
    m_type = DefaultEncoding;
    body = false;
    beginning = true;
    visualRTL = false;
}
Decoder::~Decoder()
{
    assert(_refCount == 0);
    delete m_decoder;
}

void Decoder::setEncoding(const char *_encoding, EncodingType type)
{
#ifdef DECODE_DEBUG
    kdDebug(6005) << "setEncoding " << _encoding << " " << type << endl;
#endif
    enc = _encoding;
#ifdef DECODE_DEBUG
    kdDebug(6005) << "old encoding is:" << m_codec->name() << endl;
#endif
    enc = enc.lower();
#ifdef DECODE_DEBUG
    kdDebug(6005) << "requesting:" << enc << endl;
#endif
    if(enc.isNull() || enc.isEmpty())
        return;

#if APPLE_CHANGES
    QTextCodec *codec = (type == EncodingFromMetaTag || type == EncodingFromXMLHeader)
        ? QTextCodec::codecForNameEightBitOnly(enc)
        : QTextCodec::codecForName(enc);
    if (codec) {
        enc = codec->name();
        visualRTL = codec->usesVisualOrdering();
    }
#else
    if(enc == "visual") // hebrew visually ordered
        enc = "iso8859-8";
    bool b;
    QTextCodec *codec = KGlobal::charsets()->codecForName(enc, b);
    if (!b)
        codec = 0;

    if (codec && codec->mibEnum() == 11)  {
        // visually ordered unless one of the following
        if( !(enc == "iso-8859-8-i" || enc == "iso_8859-8-i"
                || enc == "csiso88598i" || enc == "logical") )
            visualRTL = true;
    }
#endif

    if( codec ) { // in case the codec didn't exist, we keep the old one (fixes some sites specifying invalid codecs)
        m_codec = codec;
        m_type = type;
        delete m_decoder;
        m_decoder = m_codec->makeDecoder();
    }
    
#ifdef DECODE_DEBUG
    kdDebug(6005) << "Decoder::encoding used is " << m_codec->name() << endl;
#endif
}

const char *Decoder::encoding() const
{
    return enc;
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
static int findXMLEncoding(const QCString &str, int &encodingLength)
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

QString Decoder::decode(const char *data, int len)
{
    // Check for UTF-16 or UTF-8 BOM mark at the beginning, which is a sure sign of a Unicode encoding.
    int bufferLength = buffer.length();
    const int maximumBOMLength = 3;
    if (beginning && bufferLength + len >= maximumBOMLength) {
        if (m_type != UserChosenEncoding) {
            // Extract the first three bytes.
            // Handle the case where some of bytes are already in the buffer.
            // The last byte is always guaranteed to not be in the buffer.
            const uchar *udata = (const uchar *)data;
            uchar c1 = bufferLength >= 1 ? (uchar)buffer[0] : *udata++;
            uchar c2 = bufferLength >= 2 ? (uchar)buffer[1] : *udata++;
            assert(bufferLength < 3);
            uchar c3 = *udata;

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
                m_codec = QTextCodec::codecForName(autoDetectedEncoding);
                assert(m_codec);
                enc = m_codec->name();
                delete m_decoder;
                m_decoder = m_codec->makeDecoder();
            }
        }
        beginning = false;
    }
    
    // this is not completely efficient, since the function might go
    // through the html head several times...

    bool lookForMetaTag = m_type == DefaultEncoding && !body;
    
    if (lookForMetaTag) {
#ifdef DECODE_DEBUG
        kdDebug(6005) << "looking for charset definition" << endl;
#endif
        { // extra level of braces to keep indenting matching original for better diff'ing
#if APPLE_CHANGES
            buffer.append(data, len);
#else
            if(m_codec->mibEnum() != 1000) // utf16
            {
                // ### hack for a bug in QTextCodec. It cut's the input stream
                // in case there are \0 in it. ZDNET has them inside... :-(
                char *d = const_cast<char *>(data);
                int i = len - 1;
                while(i >= 0) {
                    if(*(d+i) == 0) *(d+i) = ' ';
                    i--;
                }
            }
            buffer += QCString(data, len+1);
#endif
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

#if APPLE_CHANGES
            const char *ptr = buffer.latin1();
            const char *pEnd = ptr + buffer.length();
#else
            const char *ptr = buffer.data();
            const char *pEnd = buffer.length();
#endif
            while(ptr != pEnd)
            {
                if(*ptr == '<') {
                    bool end = false;
                    ptr++;

                    // Handle comments.
                    if (ptr[0] == '!' && ptr[1] == '-' && ptr[2] == '-') {
                        ptr += 3;
                        skipComment(ptr, pEnd);
                        continue;
                    }
                    
                    // Handle XML header, which can have encoding in it.
                    if (ptr[0] == '?' && ptr[1] == 'x' && ptr[2] == 'm' && ptr[3] == 'l') {
                        const char *end = ptr;
                        while (*end != '>' && *end != '\0') end++;
                        if (*end == '\0')
                            break;
                        QCString str(ptr, end - ptr);
                        int len;
                        int pos = findXMLEncoding(str, len);
                        if (pos != -1) {
                            setEncoding(str.mid(pos, len), EncodingFromXMLHeader);
                            if (m_type == EncodingFromXMLHeader)
                                goto found;
                        }
                        setEncoding("UTF-8", EncodingFromXMLHeader);
                        goto found;
                    }

                    if(*ptr == '/') ptr++, end=true;
                    char tmp[20];
                    int len = 0;
                    while (
                        ((*ptr >= 'a') && (*ptr <= 'z') ||
                         (*ptr >= 'A') && (*ptr <= 'Z') ||
                         (*ptr >= '0') && (*ptr <= '9'))
                        && len < 19 )
                    {
                        tmp[len] = tolower( *ptr );
                        ptr++;
                        len++;
                    }
		    tmp[len] = 0;
                    AtomicString tag(tmp);
                    
                    if (tag == titleTag)
                        withinTitle = !end;
                    
                    if (!end && tag == metaTag) {
                        // found a meta tag...
                        //ptr += 5;
                        const char * end = ptr;
                        while(*end != '>' && *end != '\0') end++;
                        if ( *end == '\0' ) break;
                        QCString str( ptr, (end-ptr)+1);
                        str = str.lower();
                        int pos = 0;
                        //if( (pos = str.find("http-equiv", pos)) == -1) break;
                        //if( (pos = str.find("content-type", pos)) == -1) break;
			while( pos < ( int ) str.length() ) {
			    if( (pos = str.find("charset", pos, false)) == -1) break;
			    pos += 7;
                            // skip whitespace..
			    while(  pos < (int)str.length() && str[pos] <= ' ' ) pos++;
                            if ( pos == ( int )str.length()) break;
                            if ( str[pos++] != '=' ) continue;
                            while ( pos < ( int )str.length() &&
                                    ( str[pos] <= ' ' ) || str[pos] == '=' || str[pos] == '"' || str[pos] == '\'')
				pos++;

                            // end ?
                            if ( pos == ( int )str.length() ) break;
			    uint endpos = pos;
			    while( endpos < str.length() &&
                                   (str[endpos] != ' ' && str[endpos] != '"' && str[endpos] != '\''
                                    && str[endpos] != ';' && str[endpos] != '>') )
				endpos++;
#ifdef DECODE_DEBUG
			    kdDebug( 6005 ) << "Decoder: found charset: " << str.mid(pos, endpos-pos) << endl;
#endif
			    setEncoding(str.mid(pos, endpos-pos), EncodingFromMetaTag);
			    if( m_type == EncodingFromMetaTag ) goto found;

                            if ( endpos >= str.length() || str[endpos] == '/' || str[endpos] == '>' ) break;

			    pos = endpos + 1;
			}
		    } else if (tag != scriptTag && tag != noscriptTag && tag != styleTag &&
                               tag != linkTag && tag != metaTag && tag != objectTag &&
                               tag != titleTag && tag != baseTag && 
                               (end || tag != htmlTag) && !withinTitle &&
                               (tag != headTag) && isalpha(tmp[0])) {
                        body = true;
#ifdef DECODE_DEBUG
                        kdDebug( 6005 ) << "Decoder: no charset found (bailing because of \"" << tag.ascii() << "\")." << endl;
#endif
                        goto found;
                    }
                }
                else
                    ptr++;
            }
            return QString::null;
        }
    }

 found:
#if APPLE_CHANGES
    // Do the auto-detect if our default encoding is one of the Japanese ones.
    if (m_type != UserChosenEncoding && m_type != AutoDetectedEncoding && m_codec && m_codec->isJapanese())
#else
    if (m_type == DefaultEncoding && KGlobal::locale()->languageList()[0] == "ja")
#endif
    {
#ifdef DECODE_DEBUG
	kdDebug( 6005 ) << "Decoder: use auto-detect (" << strlen(data) << ")" << endl;
#endif
	const char *autoDetectedEncoding;
        switch ( KanjiCode::judge( data, len ) ) {
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
#ifdef DECODE_DEBUG
	kdDebug( 6005 ) << "Decoder: auto detect encoding is "
            << (autoDetectedEncoding ? autoDetectedEncoding : "NULL") << endl;
#endif
	if (autoDetectedEncoding != 0) {
	    setEncoding(autoDetectedEncoding, AutoDetectedEncoding);
	}
    }

    // if we still haven't found an encoding latin1 will be used...
    // this is according to HTML4.0 specs
    if (!m_codec)
    {
        if(enc.isEmpty()) enc = "iso8859-1";
        m_codec = QTextCodec::codecForName(enc);
        // be sure not to crash
        if(!m_codec) {
            enc = "iso8859-1";
            m_codec = QTextCodec::codecForName(enc);
        }
        delete m_decoder;
        m_decoder = m_codec->makeDecoder();
    }
    QString out;

#if APPLE_CHANGES
    if (!buffer.isEmpty()) {
        if (!lookForMetaTag)
            buffer.append(data, len);
        out = m_decoder->toUnicode(buffer.latin1(), buffer.length());
        buffer.truncate(0);
    } else {
        out = m_decoder->toUnicode(data, len);
    }
#else
    if(!buffer.isEmpty() && enc != "ISO-10646-UCS-2") {
        out = m_decoder->toUnicode(buffer.latin1(), buffer.length());
        buffer = "";
    } else {
        if(m_codec->mibEnum() != 1000) // utf16
        {
            // ### hack for a bug in QTextCodec. It cut's the input stream
            // in case there are \0 in it. ZDNET has them inside... :-(
            char *d = const_cast<char *>(data);
            int i = len - 1;
            while(i >= 0) {
                if(*(d+i) == 0) *(d+i) = ' ';
                i--;
            }
        }
        out = m_decoder->toUnicode(data, len);
    }

    if (out.isNull()) {
        fprintf(stderr, "ERROR:  decoded string is null\n");
    } else if (out.length() == 0) {
        fprintf(stderr, "ERROR:  decoded string length == 0\n");
    }
    // the hell knows, why the output does sometimes have a QChar::null at
    // the end...
    else if(out[out.length()-1] == QChar::null)
        out.truncate(out.length() - 1);
#endif

    return out;
}

QString Decoder::flush() const
{
#if APPLE_CHANGES
    return m_decoder->toUnicode(buffer.latin1(), buffer.length(), true);
#else
    return m_decoder->toUnicode(buffer.latin1(), buffer.length());
#endif
}

// -----------------------------------------------------------------------------
