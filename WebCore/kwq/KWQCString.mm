/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#include <qcstring.h>

#include <ctype.h>

QCString::QCString() : QByteArray(0)
{
}

QCString::QCString(int size) : QByteArray(size)
{
    if (size > 0) {
        *data() = '\0';
        *(data() + (size - 1)) = '\0';
    }
}

QCString::QCString(const char *s)
{
    duplicate(s, strlen(s) + 1);
}

QCString::QCString(const char *s, uint max)
{
    if (s == 0) {
        return;
    }
    uint len; // index of first '\0'
    for (len = 0; len < max - 1; len++) {
        if (s[len] == '\0') {
            break;
        }
    }
    QByteArray::resize(len + 1);
    memcpy(data(), s, len);
    data()[len] = 0;
}

bool QCString::isEmpty() const
{
    const char *s = data();
    return !(s && *s);
}

int QCString::find(const char *s, int index, bool cs) const
{
    int result;
    char *tmp;

    result = -1;

    if (!s || index >= (int)size()) {
        result = -1;
    }
    else if (!*s) {
        result = index;
    }
    else {
        if (cs) {
            tmp = strstr(data() + index, s);
        }
        else {
            tmp = data() + index;
            int len = strlen(s);
            while (*tmp) {
                if (strncasecmp(tmp, s, len) == 0) {
                    break;
                }
                tmp++;
            }
            if (!*tmp) {
                tmp = 0;
            }
        }
        
        if (tmp) {
            result = (int)(tmp - data());
        }
    }

    return result;
}

int QCString::contains(char c, bool cs) const
{
    int result;
    char *tmp;

    result = 0;
    tmp = data();

    if (tmp) {
        if (cs) {
            while (*tmp) {
                if (*tmp == c) {
                    result++;
                }
                tmp++;
            }
        }
        else {
            c = tolower((uchar)c);
            while (*tmp) {
                if (tolower((uchar)*tmp) == c) {
                    result++;
                }
                tmp++;
            }
        }
    }

    return result;
}

uint QCString::length() const
{
    const char *s = data();
    return s == NULL ? 0 : strlen(s);
}

bool QCString::resize(uint len)
{
    detach();
    uint oldlen;

    oldlen = length();

    if (!QByteArray::resize(len)) {
        return false;
    }
    if (len) {
        *(data() + len-1) = '\0';
    }

    if (len > 0 && oldlen == 0) {
        *(data()) = '\0';
    }

    return true;
}

bool QCString::truncate(uint pos)
{
    return resize(pos + 1);
}

QCString QCString::lower() const
{
    QCString result(data());

    char *p = result.data();
    if (p) {
        while(*p) {
            *p = tolower((uchar)*p);
            p++;
        }
    }

    return result;
}

QCString QCString::upper() const
{
    QCString result(data());

    char *p = result.data();
    if (p) {
        while(*p) {
            *p = toupper((uchar)*p);
            p++;
        }
    }

    return result;
}

QCString QCString::left(uint len) const
{
    if (isEmpty()) {
        QCString empty;
        return empty;
    } 
    else if (len >= size()) {
        QCString same(data());
        return same;
    } 
    else {
        QCString s(len + 1);
        strncpy(s.data(), data(), len);
        *(s.data() + len) = '\0';
        return s;
    }
}

QCString QCString::right(uint len) const
{
    if (isEmpty()) {
        QCString empty;
        return empty;
    } 
    else {
        uint l = length();
        if (len > l) {
            len = l;
        }
        char *p = data() + (l - len);
        return QCString(p);
    }
}

QCString QCString::mid(uint index, uint len) const
{
    uint slen;
    
    slen = strlen(data());

    if (len == 0xffffffff) {
        len = length() - index;
    }

    if (isEmpty() || index >= slen) {
        QCString result;
        return result;
    } 
    else {
        char *p = data() + index;
        QCString result(len + 1);
        strncpy(result.data(), p, len);
        *(result.data() + len) = '\0';
        return result;
    }
}

QCString &QCString::operator=(const char *assignFrom)
{
    duplicate(assignFrom, (assignFrom ? strlen(assignFrom) : 0) + 1);
    return *this;
}

QCString& QCString::operator+=(const char *s)
{
    if (s) {
        detach();
        uint len1 = length();
        uint len2 = strlen(s);
        if (QByteArray::resize(len1 + len2 + 1)) {
            memcpy(data() + len1, s, len2 + 1);
        }
    }

    return *this;
}

QCString &QCString::operator+=(char c)
{
    uint len;

    detach();
    len = length();

    if (QByteArray::resize(len + 2)) {
        *(data() + len) = c;
        *(data() + len + 1) = '\0';
    }

    return *this;
}

bool operator==(const QCString &s1, const char *s2)
{
    if (s1.size() == 0 && !s2) {
        return FALSE;
    }
    else if (s1.size() == 0 && s2) {
        return TRUE;
    }
    return (strcmp(s1, s2) == 0);
}

#ifdef _KWQ_IOSTREAM_
std::ostream &operator<<(std::ostream &o, const QCString &s)
{
    return o << (const char *)s.data();
}
#endif
