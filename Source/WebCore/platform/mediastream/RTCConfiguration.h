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

#ifndef RTCConfiguration_h
#define RTCConfiguration_h

#if ENABLE(MEDIA_STREAM)

#include "RTCIceServer.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCConfiguration : public RefCounted<RTCConfiguration> {
public:
    static PassRefPtr<RTCConfiguration> create() { return adoptRef(new RTCConfiguration()); }
    virtual ~RTCConfiguration() { }

    void appendServer(PassRefPtr<RTCIceServer> server) { m_servers.append(server); }
    size_t numberOfServers() { return m_servers.size(); }
    RTCIceServer* server(size_t index) { return m_servers[index].get(); }

    const String& iceTransports() const { return m_iceTransports; }
    void setIceTransports(const String& iceTransports)
    {
        if (iceTransports == "none" || iceTransports == "relay" || iceTransports == "all")
            m_iceTransports = iceTransports;
    }

    const String& requestIdentity() const { return m_requestIdentity; }
    void setRequestIdentity(const String& requestIdentity)
    {
        if (requestIdentity == "yes" || requestIdentity == "no" || requestIdentity == "ifconfigured")
            m_requestIdentity = requestIdentity;
    }

    Vector<RefPtr<RTCIceServer>> iceServers() const { return m_servers; }

 private:
    RTCConfiguration()
        : m_iceTransports("all")
        , m_requestIdentity("ifconfigured")
    {
    }

    Vector<RefPtr<RTCIceServer>> m_servers;
    String m_iceTransports;
    String m_requestIdentity;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCConfiguration_h
