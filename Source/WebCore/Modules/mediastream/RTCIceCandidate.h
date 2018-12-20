/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_RTC)

#include "ExceptionOr.h"
#include "ScriptWrappable.h"

namespace WebCore {

class RTCIceCandidate : public RefCounted<RTCIceCandidate>, public ScriptWrappable {
public:
    struct Init {
        String candidate;
        String sdpMid;
        Optional<unsigned short> sdpMLineIndex;
    };

    static ExceptionOr<Ref<RTCIceCandidate>> create(const Init&);
    static Ref<RTCIceCandidate> create(const String& candidate, const String& sdpMid, Optional<unsigned short> sdpMLineIndex);

    const String& candidate() const { return m_candidate; }
    const String& sdpMid() const { return m_sdpMid; }
    Optional<unsigned short> sdpMLineIndex() const { return m_sdpMLineIndex; }

    void setCandidate(String&& candidate) { m_candidate = WTFMove(candidate); }

private:
    RTCIceCandidate(const String& candidate, const String& sdpMid, Optional<unsigned short> sdpMLineIndex);

    String m_candidate;
    String m_sdpMid;
    Optional<unsigned short> m_sdpMLineIndex;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
