/*
 * Copyright (C) 2014 Apple Inc. All Rights Reserved.
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


#ifndef UnlinkedInstructionStream_h
#define UnlinkedInstructionStream_h

#include "UnlinkedCodeBlock.h"
#include <wtf/RefCountedArray.h>

namespace JSC {

class UnlinkedInstructionStream {
public:
    explicit UnlinkedInstructionStream(const Vector<UnlinkedInstruction>&);

    unsigned count() const { return m_instructionCount; }

    class Reader {
    public:
        explicit Reader(const UnlinkedInstructionStream&);

        const UnlinkedInstruction* next();
        bool atEnd() const { return m_index == m_stream.m_data.size(); }

    private:
        unsigned char read8();
        unsigned read32();

        const UnlinkedInstructionStream& m_stream;
        UnlinkedInstruction m_unpackedBuffer[16];
        unsigned m_index;
    };

#ifndef NDEBUG
    const RefCountedArray<UnlinkedInstruction>& unpackForDebugging() const;
#endif

private:
    friend class Reader;

#ifndef NDEBUG
    mutable RefCountedArray<UnlinkedInstruction> m_unpackedInstructionsForDebugging;
#endif

    RefCountedArray<unsigned char> m_data;
    unsigned m_instructionCount;
};

} // namespace JSC

#endif // UnlinkedInstructionStream_h
