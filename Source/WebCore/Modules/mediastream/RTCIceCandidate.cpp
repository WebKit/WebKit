/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "RTCIceCandidate.h"

#if ENABLE(WEB_RTC)

namespace WebCore {

inline RTCIceCandidate::RTCIceCandidate(const String& candidate, const String& sdpMid, Optional<unsigned short> sdpMLineIndex)
    : m_candidate(candidate)
    , m_sdpMid(sdpMid)
    , m_sdpMLineIndex(sdpMLineIndex)
{
    ASSERT(!sdpMid.isNull() || sdpMLineIndex);
}

ExceptionOr<Ref<RTCIceCandidate>> RTCIceCandidate::create(const Init& dictionary)
{
    if (dictionary.sdpMid.isNull() && !dictionary.sdpMLineIndex)
        return Exception { TypeError };
    return create(dictionary.candidate, dictionary.sdpMid, dictionary.sdpMLineIndex);
}

Ref<RTCIceCandidate> RTCIceCandidate::create(const String& candidate, const String& sdpMid, Optional<unsigned short> sdpMLineIndex)
{
    return adoptRef(*new RTCIceCandidate(candidate, sdpMid, sdpMLineIndex));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
