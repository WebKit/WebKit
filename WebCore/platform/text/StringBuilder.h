/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StringBuilder_h
#define StringBuilder_h

#include "PlatformString.h"
#include <limits>
#include <wtf/Vector.h>

namespace WebCore {

enum ConcatMode {
    ConcatUnaltered,
    ConcatAddingSpacesBetweenIndividualStrings
};

class StringBuilder {
public:
    StringBuilder() : m_totalLength(std::numeric_limits<unsigned>::max()) {}

    void setNonNull()
    {
        if (isNull())
            m_totalLength = 0;
    }

    void append(const String&);
    void append(UChar);
    void append(char);
    
    void clear();
    unsigned length() const;

    String toString(ConcatMode mode = ConcatUnaltered) const;

private:
    bool isNull() const { return m_totalLength == std::numeric_limits<unsigned>::max(); }

    unsigned m_totalLength;
    Vector<String, 16> m_strings;
};

} // namespace WebCore

#endif // StringBuilder_h
