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
#include "DeprecatedStringList.h"

DeprecatedStringList DeprecatedStringList::split(const DeprecatedString &separator, const DeprecatedString &s, bool allowEmptyEntries)
{
    DeprecatedStringList result;

    int startPos = 0;
    int endPos;
    while ((endPos = s.find(separator, startPos)) != -1) {
        if (allowEmptyEntries || startPos != endPos)
            result.append(s.mid(startPos, endPos - startPos));
        startPos = endPos + separator.length();
    }
    if (allowEmptyEntries || startPos != (int)s.length())
        result.append(s.mid(startPos));
            
    return result;
}
 
DeprecatedStringList DeprecatedStringList::split(const QChar &separator, const DeprecatedString &s, bool allowEmptyEntries)
{
    return DeprecatedStringList::split(DeprecatedString(separator), s, allowEmptyEntries);
}

DeprecatedString DeprecatedStringList::join(const DeprecatedString &separator) const
{
    DeprecatedString result;
    
    for (ConstIterator i = begin(), j = ++begin(); i != end(); ++i, ++j) {
        result += *i;
        if (j != end()) {
            result += separator;
        }
    }

    return result;
}

DeprecatedString DeprecatedStringList::pop_front()
{
    DeprecatedString front = first();
    remove(begin());
    return front;
}
