/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "KWQCString.h"

#include <kxmlcore/Assertions.h>
#include <ctype.h>

using std::ostream;

QCString::QCString()
{
}

QCString::QCString(int size) : ByteArray(size)
{
    if( size>0 && data() )
    {
        data()[0] = 0;		// first null
        data()[size-1] = 0;	// last byte
    }
    // else null
}


QCString::QCString(const char *str)
{
    size_t len;
    if( str && (len=strlen(str)+1) && resize(len) )	// include null
        strcpy( data(), str );
    // else null
}


QCString::QCString(const char *str, uint max)
{
    if( str && max )
    {
        // perform a truncated strlen on str
        const char* p = str;
	uint len = 1;			// for the null
        while( *p++ && len<max )
            len ++;

        if( resize(len) )
        {
            char *dest = data();
            strncpy( dest, str, len );
            dest[len-1] = 0;	// re-terminate
        }
    }
    // else null
}

bool QCString::isEmpty() const
{ return length()==0; }


uint QCString::length() const
{
    const char *d = data();
    return d ? strlen(d) : 0;
}


bool QCString::resize(uint len)
{
    bool success = ByteArray::resize(len);
    if( success && len>0 )
        data()[len-1] = 0;	// always terminate last byte

    return success;
}


bool QCString::truncate(uint pos)
{
    return resize(pos+1);
}


QCString QCString::lower() const
{
    // convert
    QCString tmp = *this;	// copy
    char* str = tmp.data();
    if( str )
    {
        while( *str != 0 )
        {
            *str = tolower(*str);
            str++;
        }
    }

    return tmp;
}


QCString QCString::upper() const
{
    QCString tmp = *this;	// copy
    char* str = tmp.data();
    if( str )
    {
        while( *str != 0 )
        {
            *str = toupper(*str);
            str++;
        }
    }

    return tmp;
}


inline QCString QCString::left(uint len) const
{ return mid(0, len); }


inline QCString QCString::right(uint len) const
{ return mid(length() - len, len); }


QCString QCString::mid(uint index, uint len) const
{
    uint size = length();
    if( data() && index<size )	// return null if index out-of-range
    {
        // clip length
        if( len > size - index )
            len = size - index;

        // copy and return
        return QCString( &(data()[index]), len+1);		// include nul
    }

    // degenerate case
    return QCString();
}

int QCString::find(const char *sub, int index, bool cs) const
{
    const char* str = data();
    if( str && str[0] && sub && index>=0 )	// don't search empty strings
    {
        // advance until we get to index
        int pos = 0;
        while( pos < index )
            if( str[pos++] == 0 )
                return -1;		// index is beyond end of str
        
        // now search from index onward
        while( str[index] != 0 )
        {
            char a, b;
            
            // compare until we reach the end or a mismatch
            pos = 0;
            if( cs )
                while( (a=sub[pos]) && (b=str[index]) && a==b )
                    pos++, index++;
            else
                while( (a=sub[pos]) && (b=str[index]) && tolower(a)==tolower(b) )
                    pos++, index++;
            
            // reached the end of our compare string without a mismatch?
            if( sub[pos] == 0 )
                return index - pos;
            
            index ++;
        }
    }
    
    return -1;
}

int QCString::contains(char c, bool cs) const
{
    uint found = 0;
    uint len = length();

    if (len) {
        const char *str = data();

        if (cs) {
            for (unsigned i = 0; i != len; ++i) {
                found += str[i] == c;
            }
        } else {
            c = tolower(c);

            for (unsigned i = 0; i != len; ++i) {
                char chr = str[i];
                chr = tolower(chr);
                found += chr == c;
            }
        }
    }

    return found;
}

QCString &QCString::operator=(const char *assignFrom)
{
    duplicate(assignFrom, (assignFrom ? strlen(assignFrom) : 0) + 1);
    return *this;
}

QCString& QCString::append(const char *s)
{
    if (s) {
        uint len2 = strlen(s);
        if (len2) {
            detach();
            uint len1 = length();
            if (ByteArray::resize(len1 + len2 + 1)) {
                memcpy(data() + len1, s, len2 + 1);
            }
        }
    }

    return *this;
}

QCString &QCString::append(char c)
{
    detach();
    uint len = length();

    if (ByteArray::resize(len + 2)) {
        *(data() + len) = c;
        *(data() + len + 1) = '\0';
    }

    return *this;
}

QCString &QCString::replace(char c1, char c2)
{
    uint len = length();

    if (len) {
        // Search for the first instance of c1 before detaching,
        // just in case there is nothing to replace. In that case
        // we don't want to detach this from other shared instances
        // since we have no need to modify it.
        unsigned i;
        {
            const char *s = data();
            for (i = 0; i != len; ++i) {
                if (s[i] == c1) {
                    break;
                }
            }
        }

        if (i != len) {
            detach();
            char *s = data();
            // Start at the first instance of c1; no need to rescan earlier chars.
            for (; i != len; ++i) {
                if (s[i] == c1) {
                    s[i] = c2;
                }
            }
        }
    }

    return *this;
}

bool operator==(const QCString &s1, const char *s2)
{
    if (s1.size() == 0 && !s2) {
        return TRUE;
    }
    else if (s1.size() == 0 && s2) {
        return FALSE;
    }
    return (strcmp(s1, s2) == 0);
}
