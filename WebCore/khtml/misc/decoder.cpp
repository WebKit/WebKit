/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)

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
// $Id$

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
    if(enc == "visual") // hebrew visually ordered
        enc = "iso8859-8";
    bool b;
    m_codec = KGlobal::charsets()->codecForName(enc, b);
    if(m_codec->mibEnum() == 11)  {
        // iso8859-8 (visually ordered)
        m_codec = QTextCodec::codecForName("iso8859-8-i");
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
            buffer += QCString(data, len+1);

            // we still don't have an encoding, and are in the head
            // the following tags are allowed in <head>:
            // SCRIPT|STYLE|META|LINK|OBJECT|TITLE|BASE

            const char *ptr = buffer.data();
            while(*ptr != '\0')
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
			kdDebug( 6005 ) << "Decoder: no charset found, using latin1. Id=" << id << endl;
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
#ifdef _KWQ_
    else {
        buffer += QCString(data, len+1);
    }
#endif

 found:
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

    if(!buffer.isEmpty() && enc != "ISO-10646-UCS-2") {
        out = m_decoder->toUnicode(buffer, buffer.length());
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

    // the hell knows, why the output does sometimes have a QChar::null at
    // the end...
    if(out[out.length()-1] == QChar::null)
        out.truncate(out.length() - 1);
    return out;
}

QString Decoder::flush() const
{
    return m_decoder->toUnicode(buffer, buffer.length());
}

// -----------------------------------------------------------------------------
#undef DECODE_DEBUG
