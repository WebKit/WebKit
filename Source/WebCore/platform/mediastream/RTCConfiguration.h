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

#include "URL.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCIceServer : public RefCounted<RTCIceServer> {
public:
    static PassRefPtr<RTCIceServer> create(const URL& uri, const String& credential, const String& username)
    {
        return adoptRef(new RTCIceServer(uri, credential, username));
    }
    virtual ~RTCIceServer() { }

    const URL& uri() { return m_uri; }
    const String& credential() { return m_credential; }
    const String& username() { return m_username; }

private:
    RTCIceServer(const URL& uri, const String& credential, const String& username)
        : m_uri(uri)
        , m_credential(credential)
        , m_username(username)
    {
    }

    URL m_uri;
    String m_credential;
    String m_username;
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

    Vector<RefPtr<RTCIceServer>> m_servers;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCConfiguration_h
