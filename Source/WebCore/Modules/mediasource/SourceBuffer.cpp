/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "SourceBuffer.h"

#if ENABLE(MEDIA_SOURCE)

#include "TimeRanges.h"
#include <wtf/Uint8Array.h>

namespace WebCore {

SourceBuffer::SourceBuffer(const String& id, PassRefPtr<MediaSource> source)
    : m_id(id)
    , m_source(source)
    , m_timestampOffset(0)
{
}

SourceBuffer::~SourceBuffer()
{
}

PassRefPtr<TimeRanges> SourceBuffer::buffered(ExceptionCode& ec) const
{
    if (!m_source) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return m_source->buffered(id(), ec);
}

double SourceBuffer::timestampOffset() const
{
    return m_timestampOffset;
}

void SourceBuffer::setTimestampOffset(double offset, ExceptionCode& ec)
{
    if (!m_source) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (m_source->setTimestampOffset(id(), offset, ec))
        m_timestampOffset = offset;
}

void SourceBuffer::append(PassRefPtr<Uint8Array> data, ExceptionCode& ec)
{
    if (!m_source) {
        ec = INVALID_STATE_ERR;
        return;
    }

    m_source->append(id(), data, ec);
}

void SourceBuffer::abort(ExceptionCode& ec)
{
    if (!m_source) {
        ec = INVALID_STATE_ERR;
        return;
    }

    m_source->abort(id(), ec);
}

} // namespace WebCore

#endif
