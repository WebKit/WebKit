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

#undef DECODE_DEBUG
//#define DECODE_DEBUG

#include "decoder.h"
using namespace khtml;

#include "htmlhashes.h"

#include <qregexp.h>
#include <qtextcodec.h>

#include <kglobal.h>
#include <kcharsets.h>

#include <ctype.h>
#include <kdebug.h>
#include <klocale.h>

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
    // latin1
    m_codec = QTextCodec::codecForMib(4);
    m_decoder = m_codec->makeDecoder();
    enc = 0;
    body = false;
    beginning = true;
    visualRTL = false;
    haveEncoding = false;
}
Decoder::~Decoder()
{
    delete m_decoder;
}

void Decoder::setEncoding(const char *_encoding, bool force)
{
#ifdef DECODE_DEBUG
    kdDebug(6005) << "setEncoding " << _encoding << " " << force << endl;
#endif
    enc = _encoding;

    QTextCodec *old = m_codec;
#ifdef DECODE_DEBUG
    kdDebug(6005) << "old encoding is:" << m_codec->name() << endl;
#endif
    enc = enc.lower();
#ifdef DECODE_DEBUG
    kdDebug(6005) << "requesting:" << enc << endl;
#endif
    if(enc.isNull() || enc.isEmpty())
        return;
#if !APPLE_CHANGES
    if(enc == "visual") // hebrew visually ordered
        enc = "iso8859-8";
#endif
    bool b;
    m_codec = KGlobal::charsets()->codecForName(enc, b);

    if(m_codec->mibEnum() == 11)  {
        // visually ordered unless one of the following
        if( !(enc == "iso-8859-8-i" || enc == "iso_8859-8-i"
                || enc == "csiso88598i" || enc == "logical") )
            visualRTL = true;
    }

    if( !b ) // in case the codec didn't exist, we keep the old one (fixes some sites specifying invalid codecs)
	m_codec = old;
    else
	haveEncoding = force;
    delete m_decoder;
    m_decoder = m_codec->makeDecoder();
#ifdef DECODE_DEBUG
    kdDebug(6005) << "Decoder::encoding used is" << m_codec->name() << endl;
#endif
}

const char *Decoder::encoding() const
{
    return enc;
}

QString Decoder::decode(const char *data, int len)
{
    // this is not completely efficient, since the function might go
    // through the html head several times...

    if(!haveEncoding && !body) {
#ifdef DECODE_DEBUG
        kdDebug(6005) << "looking for charset definition" << endl;
#endif
        // check for UTF-16
        uchar * uchars = (uchar *) data;
        if( uchars[0] == 0xfe && uchars[1] == 0xff ||
            uchars[0] == 0xff && uchars[1] == 0xfe ) {
            enc = "ISO-10646-UCS-2";
            haveEncoding = true;
            m_codec = QTextCodec::codecForMib(1000);
            delete m_decoder;
            m_decoder = m_codec->makeDecoder();
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
#if APPLE_CHANGES
            QString appendString(data, len);
            buffer += appendString;
#else
            buffer += QCString(data, len+1);
#endif
            // we still don't have an encoding, and are in the head
            // the following tags are allowed in <head>:
            // SCRIPT|STYLE|META|LINK|OBJECT|TITLE|BASE

#if APPLE_CHANGES
            const char *ptr = buffer.latin1();
            const char *pEnd = ptr + buffer.length();
            while(ptr != pEnd)
#else
            const char *ptr = buffer.data();
            while(*ptr != '\0')
#endif
            {
                if(*ptr == '<') {
                    bool end = false;
                    ptr++;
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
                    int id = khtml::getTagID(tmp, len);
                    if(end) id += ID_CLOSE_TAG;

                    switch( id ) {
                    case ID_META:
                    {
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
			    if( (pos = str.find("charset", pos)) == -1) break;
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
			    enc = str.mid(pos, endpos-pos);
#ifdef DECODE_DEBUG
			    kdDebug( 6005 ) << "Decoder: found charset: " << enc.data() << endl;
#endif
			    setEncoding(enc, true);
			    if( haveEncoding ) goto found;

                            if ( endpos >= str.length() || str[endpos] == '/' || str[endpos] == '>' ) break;

			    pos = endpos + 1;
			}
		    }
                    case ID_SCRIPT:
                    case (ID_SCRIPT+ID_CLOSE_TAG):
                    case ID_STYLE:
                    case (ID_STYLE+ID_CLOSE_TAG):
                    case ID_LINK:
                    case (ID_LINK+ID_CLOSE_TAG):
                    case ID_OBJECT:
                    case (ID_OBJECT+ID_CLOSE_TAG):
                    case ID_TITLE:
                    case (ID_TITLE+ID_CLOSE_TAG):
                    case ID_BASE:
                    case (ID_BASE+ID_CLOSE_TAG):
                    case ID_HTML:
                    case ID_HEAD:
                    case 0:
                    case (0 + ID_CLOSE_TAG ):
                        break;
                    default:
                        body = true;
#ifdef DECODE_DEBUG
			kdDebug( 6005 ) << "Decoder: no charset found. Id=" << id << endl;
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
#if APPLE_CHANGES
    else {
        QString appendString(data, len);
        buffer += appendString;
    } 
#endif /* APPLE_CHANGES */

 found:
#if APPLE_CHANGES
    if (0) // not ready to turn this on yet; seems to cause problems
#else
    if (!haveEncoding && KGlobal::locale()->languageList()[0] == "ja")
#endif
    {
#ifdef DECODE_DEBUG
	kdDebug( 6005 ) << "Decoder: use auto-detect (" << strlen(data) << ")" << endl;
#endif
	switch ( KanjiCode::judge( data, len ) ) {
	case KanjiCode::JIS:
	    enc = "jis7";
	    break;
	case KanjiCode::EUC:
	    enc = "eucjp";
	    break;
	case KanjiCode::SJIS:
	    enc = "sjis";
	    break;
	default:
	    enc = NULL;
	    break;
	}
#ifdef DECODE_DEBUG
	kdDebug( 6005 ) << "Decoder: auto detect encoding is " << enc << endl;
#endif
	if (!enc.isEmpty()) {
	    setEncoding(enc, true);
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
            m_codec = QTextCodec::codecForMib(4);
            enc = "iso8859-1";
        }
        delete m_decoder;
        m_decoder = m_codec->makeDecoder();
    }
    QString out;

#if APPLE_CHANGES
    // We don't need to special-case unicode like this, and doing so 
    // will drop the buffer which is really bad!
    if(!buffer.isEmpty()) {
#else
    if(!buffer.isEmpty() && enc != "ISO-10646-UCS-2") {
#endif
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
#if APPLE_CHANGES
    } // need to balance braces for benefit of ChangeLog script
#else
    }
#endif

    if (out.isNull()) {
        fprintf(stderr, "ERROR:  decoded string is null\n");
    } else if (out.length() == 0) {
        fprintf(stderr, "ERROR:  decoded string length == 0\n");
    }
    // the hell knows, why the output does sometimes have a QChar::null at
    // the end...
    else if(out[out.length()-1] == QChar::null)
        out.truncate(out.length() - 1);
    return out;
}

QString Decoder::flush() const
{
    return m_decoder->toUnicode(buffer.latin1(), buffer.length());
}

// -----------------------------------------------------------------------------
#undef DECODE_DEBUG
