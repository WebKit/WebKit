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

#ifndef RTCIceCandidate_h
#define RTCIceCandidate_h

#if ENABLE(WEB_RTC)

#include "ExceptionBase.h"
#include "ScriptWrappable.h"
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Dictionary;
class RTCIceCandidateDescriptor;

class RTCIceCandidate : public RefCounted<RTCIceCandidate>, public ScriptWrappable {
public:
    static RefPtr<RTCIceCandidate> create(const Dictionary&, ExceptionCode&);
    static Ref<RTCIceCandidate> create(const String& candidate, const String& sdpMid, Optional<unsigned short> sdpMLineIndex);
    virtual ~RTCIceCandidate() { }

    const String& candidate() const { return m_candidate; }
    void setCandidate(const String& candidate) { m_candidate = candidate; }

    const String& sdpMid() const { return m_sdpMid; }
    void setSdpMid(const String& sdpMid) { m_sdpMid = sdpMid; }

    Optional<unsigned short> sdpMLineIndex() const { return m_sdpMLineIndex; }
    void setSdpMLineIndex(Optional<unsigned short> sdpMLineIndex) { m_sdpMLineIndex = sdpMLineIndex; }

private:
    explicit RTCIceCandidate(const String& candidate, const String& sdpMid, Optional<unsigned short> sdpMLineIndex);

    String m_candidate;
    String m_sdpMid;
    Optional<unsigned short> m_sdpMLineIndex;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

#endif // RTCIceCandidate_h
