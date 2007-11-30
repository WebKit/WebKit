/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef TextStream_h
#define TextStream_h

#include <wtf/Vector.h>

namespace WebCore {

class DeprecatedChar;
class DeprecatedString;
class String;
class TextStream;

typedef TextStream& (*TextStreamManipulator)(TextStream&);

TextStream& endl(TextStream&);

class TextStream {
public:
    TextStream(DeprecatedString*);

    TextStream& operator<<(char);
    TextStream& operator<<(const DeprecatedChar&);
    TextStream& operator<<(short);
    TextStream& operator<<(unsigned short);
    TextStream& operator<<(int);
    TextStream& operator<<(unsigned);
    TextStream& operator<<(long);
    TextStream& operator<<(unsigned long);
    TextStream& operator<<(float);
    TextStream& operator<<(double);
    TextStream& operator<<(const char*);
    TextStream& operator<<(const String&);
    TextStream& operator<<(const DeprecatedString&);
    TextStream& operator<<(void*);

    TextStream& operator<<(const TextStreamManipulator&);

    int precision(int);

private:
    TextStream(const TextStream&);
    TextStream& operator=(const TextStream&);

    bool m_hasByteArray;
    Vector<char> m_byteArray;
    DeprecatedString* m_string;
    int m_precision;
};

}

#endif
