/*
 * Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
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
#include "DeprecatedCString.h"

#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>

using namespace WTF;

namespace WebCore {

DeprecatedCString::DeprecatedCString()
{
}

DeprecatedCString::DeprecatedCString(int size) : DeprecatedByteArray(size)
{
    if( size>0 && data() )
    {
        data()[0] = 0;          // first null
        data()[size-1] = 0;     // last byte
    }
    // else null
}


DeprecatedCString::DeprecatedCString(const char *str)
{
    size_t len;
    if( str && (len=strlen(str)+1) && resize(len) )     // include null
        strcpy( data(), str );
    // else null
}


DeprecatedCString::DeprecatedCString(const char *str, unsigned max)
{
    if( str && max )
    {
        // perform a truncated strlen on str
        const char* p = str;
        unsigned len = 1;                   // for the null
        while( *p++ && len<max )
            len ++;

        if( resize(len) )
        {
            char *dest = data();
            strncpy( dest, str, len );
            dest[len-1] = 0;            // re-terminate
        }
    }
    // else null
}

bool DeprecatedCString::isEmpty() const
{ return length()==0; }


unsigned DeprecatedCString::length() const
{
    const char *d = data();
    return d ? strlen(d) : 0;
}


bool DeprecatedCString::resize(unsigned len)
{
    bool success = DeprecatedByteArray::resize(len);
    if( success && len>0 )
        data()[len-1] = 0;      // always terminate last byte

    return success;
}


bool DeprecatedCString::truncate(unsigned pos)
{
    return resize(pos+1);
}


DeprecatedCString DeprecatedCString::lower() const
{
    // convert
    DeprecatedCString tmp = *this;       // copy
    char* str = tmp.data();
    if( str )
    {
        while( *str != 0 )
        {
            *str = toASCIILower(*str);
            str++;
        }
    }

    return tmp;
}


DeprecatedCString DeprecatedCString::upper() const
{
    DeprecatedCString tmp = *this;       // copy
    char* str = tmp.data();
    if( str )
    {
        while( *str != 0 )
        {
            *str = toASCIIUpper(*str);
            str++;
        }
    }

    return tmp;
}


inline DeprecatedCString DeprecatedCString::left(unsigned len) const
{ return mid(0, len); }


inline DeprecatedCString DeprecatedCString::right(unsigned len) const
{ return mid(length() - len, len); }


DeprecatedCString DeprecatedCString::mid(unsigned index, unsigned len) const
{
    unsigned size = length();
    if( data() && index<size )      // return null if index out-of-range
    {
        // clip length
        if( len > size - index )
            len = size - index;

        // copy and return
        return DeprecatedCString( &(data()[index]), len+1);  // include nul
    }

    // degenerate case
    return DeprecatedCString();
}

int DeprecatedCString::find(const char *sub, int index, bool cs) const
{
    const char* str = data();
    if( str && str[0] && sub && index>=0 )  // don't search empty strings
    {
        // advance until we get to index
        int pos = 0;
        while( pos < index )
            if( str[pos++] == 0 )
                return -1;                  // index is beyond end of str
        
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
                while( (a=sub[pos]) && (b=str[index]) && toASCIILower(a)==toASCIILower(b) )
                    pos++, index++;
            
            // reached the end of our compare string without a mismatch?
            if( sub[pos] == 0 )
                return index - pos;
            
            index ++;
        }
    }
    
    return -1;
}

int DeprecatedCString::contains(char c, bool cs) const
{
    unsigned found = 0;
    unsigned len = length();

    if (len) {
        const char *str = data();

        if (cs) {
            for (unsigned i = 0; i != len; ++i) {
                found += str[i] == c;
            }
        } else {
            c = toASCIILower(c);

            for (unsigned i = 0; i != len; ++i) {
                char chr = str[i];
                chr = toASCIILower(chr);
                found += chr == c;
            }
        }
    }

    return found;
}

DeprecatedCString &DeprecatedCString::operator=(const char *assignFrom)
{
    duplicate(assignFrom, (assignFrom ? strlen(assignFrom) : 0) + 1);
    return *this;
}

DeprecatedCString& DeprecatedCString::append(const char *s)
{
    if (s) {
        unsigned len2 = strlen(s);
        if (len2) {
            detach();
            unsigned len1 = length();
            if (DeprecatedByteArray::resize(len1 + len2 + 1))
                memcpy(data() + len1, s, len2 + 1);
        }
    }

    return *this;
}

DeprecatedCString &DeprecatedCString::append(char c)
{
    detach();
    unsigned len = length();

    if (DeprecatedByteArray::resize(len + 2)) {
        *(data() + len) = c;
        *(data() + len + 1) = '\0';
    }

    return *this;
}

DeprecatedCString &DeprecatedCString::replace(char c1, char c2)
{
    unsigned len = length();

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

bool operator==(const DeprecatedCString &s1, const char *s2)
{
    if (s1.size() == 0 && !s2)
        return true;
    if (s1.size() == 0 && s2)
        return false;
    return strcmp(s1, s2) == 0;
}

}
