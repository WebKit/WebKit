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

#ifndef WebKitSourceBuffer_h
#define WebKitSourceBuffer_h

#if ENABLE(MEDIA_SOURCE)

#include "ExceptionCode.h"
#include "ScriptWrappable.h"
#include <runtime/Uint8Array.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SourceBufferPrivate;
class TimeRanges;
class WebKitMediaSource;

class WebKitSourceBuffer : public RefCounted<WebKitSourceBuffer>, public ScriptWrappable {
public:
    static PassRefPtr<WebKitSourceBuffer> create(PassOwnPtr<SourceBufferPrivate>, PassRefPtr<WebKitMediaSource>);

    virtual ~WebKitSourceBuffer();

    // WebKitSourceBuffer.idl methods
    PassRefPtr<TimeRanges> buffered(ExceptionCode&) const;
    double timestampOffset() const;
    void setTimestampOffset(double, ExceptionCode&);
    void append(PassRefPtr<JSC::Uint8Array> data, ExceptionCode&);
    void abort(ExceptionCode&);

    void removedFromMediaSource();

private:
    WebKitSourceBuffer(PassOwnPtr<SourceBufferPrivate>, PassRefPtr<WebKitMediaSource>);

    bool isRemoved() const;

    OwnPtr<SourceBufferPrivate> m_private;
    RefPtr<WebKitMediaSource> m_source;

    double m_timestampOffset;
};

} // namespace WebCore

#endif
#endif
