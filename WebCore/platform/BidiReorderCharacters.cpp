/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "BidiReorderCharacters.h"

#include "BidiResolver.h"
#include "PlatformString.h"

using namespace WTF::Unicode;

namespace WebCore {

class CharacterBufferIterator {
public:
    CharacterBufferIterator()
        : m_characterBuffer(0)
        , m_bufferLength(0)
        , m_offset(0)
    {
    }

    CharacterBufferIterator(CharacterBuffer* buffer, unsigned length, unsigned offset)
        : m_characterBuffer(buffer)
        , m_bufferLength(length)
        , m_offset(offset)
    {
    }

    CharacterBufferIterator(const CharacterBufferIterator& other)
        : m_characterBuffer(other.m_characterBuffer)
        , m_bufferLength(other.m_bufferLength)
        , m_offset(other.m_offset)
    {
    }

    unsigned offset() const { return m_offset; }

    void increment(BidiResolver<CharacterBufferIterator, BidiCharacterRun>&) { m_offset++; }

    bool atEnd() const { return m_offset >= m_bufferLength; }

    UChar current() const { return (*m_characterBuffer)[m_offset]; }

    Direction direction() const { return atEnd() ? OtherNeutral : WTF::Unicode::direction(current()); }

    bool operator==(const CharacterBufferIterator& other)
    {
        return m_offset == other.m_offset && m_characterBuffer == other.m_characterBuffer;
    }

    bool operator!=(const CharacterBufferIterator& other) { return !operator==(other); }

private:
    CharacterBuffer* m_characterBuffer;
    unsigned m_bufferLength;
    unsigned m_offset;
};

template <>
void BidiResolver<CharacterBufferIterator, BidiCharacterRun>::appendRun()
{
    if (emptyRun || eor.atEnd())
        return;

    BidiCharacterRun* bidiRun = new BidiCharacterRun(sor.offset(), eor.offset() + 1, context(), m_direction);
    if (!m_firstRun)
        m_firstRun = bidiRun;
    else
        m_lastRun->m_next = bidiRun;
    m_lastRun = bidiRun;
    m_runCount++;

    eor.increment(*this);
    sor = eor;
    m_direction = OtherNeutral;
    m_status.eor = OtherNeutral;
}

void bidiReorderCharacters(CharacterBuffer& characterBuffer, bool rtl, bool override, bool visuallyOrdered)
{
    unsigned bufferLength = characterBuffer.size();
    // Create a local copy of the buffer.
    String string(characterBuffer.data(), bufferLength);

    // Call createBidiRunsForLine.
    BidiResolver<CharacterBufferIterator, BidiCharacterRun> bidi;

    RefPtr<BidiContext> startEmbed = new BidiContext(rtl ? 1 : 0, rtl ? RightToLeft : LeftToRight, override);
    bidi.setLastStrongDir(startEmbed->dir());
    bidi.setLastDir(startEmbed->dir());
    bidi.setEorDir(startEmbed->dir());
    bidi.setContext(startEmbed);
    
    bidi.createBidiRunsForLine(CharacterBufferIterator(&characterBuffer, bufferLength, 0), CharacterBufferIterator(&characterBuffer, bufferLength, bufferLength), visuallyOrdered);

    // Fill the characterBuffer.
    int index = 0;
    BidiCharacterRun* r = bidi.firstRun();
    while (r) {
        bool reversed = r->reversed(visuallyOrdered);
        // If there's only one run, and it doesn't need to be reversed, we are done.
        if (bidi.runCount() == 1 && !reversed)
            break;

        for (int i = r->m_start; i < r->m_stop; ++i) {
            if (reversed)
                characterBuffer[index] = string[r->m_stop + r->m_start - i - 1];
            else
                characterBuffer[index] = string[i];
            ++index;
        }
        r = r->m_next;
    }

    bidi.deleteRuns();
}

} // namespace WebCore
