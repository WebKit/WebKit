/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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
 * 3. Neither the name of Ericsson nor the names of its contributors
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

#ifndef MediaEndpointConfiguration_h
#define MediaEndpointConfiguration_h

#if ENABLE(MEDIA_STREAM)

#include "URL.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class IceServerInfo : public RefCounted<IceServerInfo> {
public:
    static RefPtr<IceServerInfo> create(const Vector<String>& urls, const String& credential, const String& username)
    {
        return adoptRef(new IceServerInfo(urls, credential, username));
    }
    virtual ~IceServerInfo() { }

    const Vector<URL>& urls() const { return m_urls; }
    const String& credential() const { return m_credential; }
    const String& username() const { return m_username; }

private:
    IceServerInfo(const Vector<String>& urls, const String& credential, const String& username);

    Vector<URL> m_urls;
    String m_credential;
    String m_username;
};

class MediaEndpointConfiguration : public RefCounted<MediaEndpointConfiguration> {
public:
    static RefPtr<MediaEndpointConfiguration> create(Vector<RefPtr<IceServerInfo>>& iceServers, const String& iceTransportPolicy, const String& bundlePolicy)
    {
        return adoptRef(new MediaEndpointConfiguration(iceServers, iceTransportPolicy, bundlePolicy));
    }

    enum class IceTransportPolicy {
        None,
        Relay,
        All
    };

    enum class BundlePolicy {
        Balanced,
        MaxCompat,
        MaxBundle
    };

    const Vector<RefPtr<IceServerInfo>>& iceServers() const { return m_iceServers; }
    IceTransportPolicy iceTransportPolicy() const { return m_iceTransportPolicy; }
    BundlePolicy bundlePolicy() const { return m_bundlePolicy; }

private:
    MediaEndpointConfiguration(Vector<RefPtr<IceServerInfo>>&, const String& iceTransportPolicy, const String& bundlePolicy);

    Vector<RefPtr<IceServerInfo>> m_iceServers;
    IceTransportPolicy m_iceTransportPolicy;
    BundlePolicy m_bundlePolicy;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaEndpointConfiguration_h
