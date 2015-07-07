/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCConfigurationPrivate_h
#define RTCConfigurationPrivate_h

#if ENABLE(MEDIA_STREAM)

#include "RTCIceServerPrivate.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCConfigurationPrivate : public RefCounted<RTCConfigurationPrivate> {
public:
    static PassRefPtr<RTCConfigurationPrivate> create() { return adoptRef(new RTCConfigurationPrivate()); }
    virtual ~RTCConfigurationPrivate() { }

    void appendServer(PassRefPtr<RTCIceServerPrivate> server) { m_privateServers.append(server); }
    size_t numberOfServers() { return m_privateServers.size(); }
    RTCIceServerPrivate* server(size_t index) { return m_privateServers[index].get(); }

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

    Vector<RefPtr<RTCIceServerPrivate>> iceServers() const { return m_privateServers; }

private:
    RTCConfigurationPrivate()
        : m_iceTransports("all")
        , m_requestIdentity("ifconfigured")
    {
    }

    Vector<RefPtr<RTCIceServerPrivate>> m_privateServers;
    String m_iceTransports;
    String m_requestIdentity;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCConfigurationPrivate_h
