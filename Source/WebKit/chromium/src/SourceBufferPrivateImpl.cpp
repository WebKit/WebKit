/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "SourceBufferPrivateImpl.h"

#if ENABLE(MEDIA_SOURCE)

#include "WebSourceBuffer.h"

namespace WebKit {

SourceBufferPrivateImpl::SourceBufferPrivateImpl(PassOwnPtr<WebSourceBuffer> sourceBuffer)
    : m_sourceBuffer(sourceBuffer)
{
}

PassRefPtr<WebCore::TimeRanges> SourceBufferPrivateImpl::buffered()
{
    if (!m_sourceBuffer)
        return WebCore::TimeRanges::create();

    WebTimeRanges webRanges = m_sourceBuffer->buffered();
    RefPtr<WebCore::TimeRanges> ranges = WebCore::TimeRanges::create();
    for (size_t i = 0; i < webRanges.size(); ++i)
        ranges->add(webRanges[i].start, webRanges[i].end);
    return ranges.release();
}

void SourceBufferPrivateImpl::append(const unsigned char* data, unsigned length)
{
    if (!m_sourceBuffer)
        return;
    m_sourceBuffer->append(data, length);
}

bool SourceBufferPrivateImpl::abort()
{
    if (!m_sourceBuffer)
        return false;
    return m_sourceBuffer->abort();
}

bool SourceBufferPrivateImpl::setTimestampOffset(double offset)
{
    if (!m_sourceBuffer)
        return false;

    return m_sourceBuffer->setTimestampOffset(offset);
}

void SourceBufferPrivateImpl::removedFromMediaSource()
{
    if (!m_sourceBuffer)
        return;
    m_sourceBuffer->removedFromMediaSource();
    m_sourceBuffer.clear();
}

}

#endif
