/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef HTMLEntitySearch_h
#define HTMLEntitySearch_h

#include "PlatformString.h"

namespace WebCore {

struct HTMLEntityTableEntry;

class HTMLEntitySearch {
public:
    HTMLEntitySearch();

    void advance(UChar);

    bool isEntityPrefix() const { return !!m_start; }
    int currentValue() const { return m_currentValue; }
    int currentLength() const { return m_currentLength; }

    const HTMLEntityTableEntry* lastMatch() const { return m_lastMatch; }

private:
    enum CompareResult {
        Before,
        Prefix,
        After,
    };

    CompareResult compare(const HTMLEntityTableEntry*, UChar) const;
    const HTMLEntityTableEntry* findStart(UChar) const;
    const HTMLEntityTableEntry* findEnd(UChar) const;

    void fail()
    {
        m_currentValue = 0;
        m_start = 0;
        m_end = 0;
    }

    int m_currentLength;
    int m_currentValue;

    const HTMLEntityTableEntry* m_lastMatch;
    const HTMLEntityTableEntry* m_start;
    const HTMLEntityTableEntry* m_end;
};

}

#endif
