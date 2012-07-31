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

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "RTCPeerConnection.h"

#include "ArrayValue.h"
#include "ExceptionCode.h"
#include "KURL.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

// FIXME: RTCIceServer and RTCConfigration are placed here temporarily.
// Their final place is in Source/WebCore/platform/mediastream.
class RTCIceServer : public RefCounted<RTCIceServer> {
public:
    static PassRefPtr<RTCIceServer> create(const KURL& uri, const String& credential) { return adoptRef(new RTCIceServer(uri, credential)); }
    virtual ~RTCIceServer() { }

    const KURL& uri() { return m_uri; }
    const String& credential() { return m_credential; }

private:
    RTCIceServer(const KURL& uri, const String& credential)
        : m_uri(uri)
        , m_credential(credential)
    {
    }

    KURL m_uri;
    String m_credential;
};

class RTCConfiguration : public RefCounted<RTCConfiguration> {
public:
    static PassRefPtr<RTCConfiguration> create() { return adoptRef(new RTCConfiguration()); }
    virtual ~RTCConfiguration() { }

    void appendServer(PassRefPtr<RTCIceServer> server) { m_servers.append(server); }
    size_t numberOfServers() { return m_servers.size(); }
    RTCIceServer* server(size_t index) { return m_servers[index].get(); }

private:
    RTCConfiguration() { }

    Vector<RefPtr<RTCIceServer> > m_servers;
};

PassRefPtr<RTCConfiguration> RTCPeerConnection::parseConfiguration(const Dictionary& configuration, ExceptionCode& ec)
{
    if (configuration.isUndefinedOrNull())
        return 0;

    ArrayValue iceServers;
    bool ok = configuration.get("iceServers", iceServers);
    if (!ok || iceServers.isUndefinedOrNull()) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }

    size_t numberOfServers;
    ok = iceServers.length(numberOfServers);
    if (!ok) {
        ec = TYPE_MISMATCH_ERR;
        return 0;
    }

    RefPtr<RTCConfiguration> rtcConfiguration = RTCConfiguration::create();

    for (size_t i = 0; i < numberOfServers; ++i) {
        Dictionary iceServer;
        ok = iceServers.get(i, iceServer);
        if (!ok) {
            ec = TYPE_MISMATCH_ERR;
            return 0;
        }

        String uri, credential;
        ok = iceServer.get("uri", uri);
        if (!ok) {
            ec = TYPE_MISMATCH_ERR;
            return 0;
        }
        KURL url(KURL(), uri);
        if (!url.isValid() || !(url.protocolIs("turn") || url.protocolIs("stun"))) {
            ec = TYPE_MISMATCH_ERR;
            return 0;
        }

        iceServer.get("credential", credential);

        rtcConfiguration->appendServer(RTCIceServer::create(url, credential));
    }
    return rtcConfiguration.release();
}

PassRefPtr<RTCPeerConnection> RTCPeerConnection::create(ScriptExecutionContext* context, const Dictionary& rtcConfiguration, const Dictionary&, ExceptionCode& ec)
{
    RefPtr<RTCConfiguration> configuration = parseConfiguration(rtcConfiguration, ec);
    if (ec)
        return 0;

    RefPtr<RTCPeerConnection> peerConnection = adoptRef(new RTCPeerConnection(context, configuration.release(), ec));
    if (ec)
        return 0;

    peerConnection->suspendIfNeeded();
    return peerConnection.release();
}

RTCPeerConnection::RTCPeerConnection(ScriptExecutionContext* context, PassRefPtr<RTCConfiguration>, ExceptionCode& ec)
    : ActiveDOMObject(context, this)
{
}

RTCPeerConnection::~RTCPeerConnection()
{
}

const AtomicString& RTCPeerConnection::interfaceName() const
{
    return eventNames().interfaceForRTCPeerConnection;
}

ScriptExecutionContext* RTCPeerConnection::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void RTCPeerConnection::stop()
{
    // FIXME: Make sure that this object stops posting events and releases resources at this stage.
}

EventTargetData* RTCPeerConnection::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* RTCPeerConnection::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
