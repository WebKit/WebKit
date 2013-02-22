/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(THREADED_HTML_PARSER)

#include "BackgroundHTMLInputStream.h"

namespace WebCore {

BackgroundHTMLInputStream::BackgroundHTMLInputStream()
{
}

void BackgroundHTMLInputStream::append(const String& input)
{
    m_current.append(SegmentedString(input));
    m_segments.append(input);
}

void BackgroundHTMLInputStream::close()
{
    m_current.close();
}

HTMLInputCheckpoint BackgroundHTMLInputStream::createCheckpoint()
{
    HTMLInputCheckpoint checkpoint = m_checkpoints.size();
    m_checkpoints.append(Checkpoint(m_current, m_segments.size()));
    return checkpoint;
}

void BackgroundHTMLInputStream::rewindTo(HTMLInputCheckpoint checkpointIndex, const String& unparsedInput)
{
    ASSERT(checkpointIndex < m_checkpoints.size()); // If this ASSERT fires, checkpointIndex is invalid.
    const Checkpoint& checkpoint = m_checkpoints[checkpointIndex];

    bool isClosed = m_current.isClosed();

    m_current = checkpoint.input;

    for (size_t i = checkpoint.numberOfSegmentsAlreadyAppended; i < m_segments.size(); ++i)
        m_current.append(SegmentedString(m_segments[i]));

    if (!unparsedInput.isEmpty())
        m_current.prepend(SegmentedString(unparsedInput));

    if (isClosed && !m_current.isClosed())
        m_current.close();

    m_segments.clear();
    m_checkpoints.clear();
}

}

#endif // ENABLE(THREADED_HTML_PARSER)
