/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#include "CodeLocation.h"
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace JSC {

class JITCodeMap {
private:
    struct Entry {
        Entry() { }

        Entry(unsigned bytecodeIndex, CodeLocationLabel<JSEntryPtrTag> codeLocation)
            : m_bytecodeIndex(bytecodeIndex)
            , m_codeLocation(codeLocation)
        { }

        inline unsigned bytecodeIndex() const { return m_bytecodeIndex; }
        inline CodeLocationLabel<JSEntryPtrTag> codeLocation() { return m_codeLocation; }

    private:
        unsigned m_bytecodeIndex;
        CodeLocationLabel<JSEntryPtrTag> m_codeLocation;
    };

public:
    void append(unsigned bytecodeIndex, CodeLocationLabel<JSEntryPtrTag> codeLocation)
    {
        m_entries.append({ bytecodeIndex, codeLocation });
    }

    void finish() { m_entries.shrinkToFit(); }

    CodeLocationLabel<JSEntryPtrTag> find(unsigned bytecodeIndex) const
    {
        auto* entry =
            binarySearch<Entry, unsigned>(m_entries,
                m_entries.size(), bytecodeIndex, [] (Entry* entry) {
                    return entry->bytecodeIndex();
                });
        if (!entry)
            return CodeLocationLabel<JSEntryPtrTag>();
        return entry->codeLocation();
    }

    explicit operator bool() const { return m_entries.size(); }

private:
    Vector<Entry> m_entries;
};

} // namespace JSC

#endif // ENABLE(JIT)
