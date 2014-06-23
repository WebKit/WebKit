/*
 * Copyright (C) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BufferedLineReader_h
#define BufferedLineReader_h

#include "SegmentedString.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// Line collection helper for the WebVTT Parser.
//
// Converts a stream of data (== a sequence of Strings) into a set of
// lines. CR, LR or CRLF are considered linebreaks. Normalizes NULs (U+0000)
// to 'REPLACEMENT CHARACTER' (U+FFFD) and does not return the linebreaks as
// part of the result.
class BufferedLineReader {
    WTF_MAKE_NONCOPYABLE(BufferedLineReader);
public:
    BufferedLineReader()
        : m_endOfStream(false)
        , m_maybeSkipLF(false) { }

    // Append data to the internal buffer.
    void append(const String& data)
    {
        ASSERT(!m_endOfStream);
        m_buffer.append(SegmentedString(data));
    }

    // Indicate that no more data will be appended. This will cause any
    // potentially "unterminated" line to be returned from getLine.
    void setEndOfStream() { m_endOfStream = true; }

    // Attempt to read a line from the internal buffer (fed via append).
    // If successful, true is returned and |line| is set to the line that was
    // read. If no line could be read false is returned.
    bool getLine(String& line);

    // Returns true if EOS has been reached proper.
    bool isAtEndOfStream() const { return m_endOfStream && m_buffer.isEmpty(); }

    void reset() { m_buffer.clear(); }

private:
    // Consume the next character the buffer if it is the character |c|.
    void scanCharacter(UChar c)
    {
        ASSERT(!m_buffer.isEmpty());
        if (m_buffer.currentChar() == c)
            m_buffer.advance();
    }

    SegmentedString m_buffer;
    StringBuilder m_lineBuffer;
    bool m_endOfStream;
    bool m_maybeSkipLF;
};

} // namespace WebCore

#endif
