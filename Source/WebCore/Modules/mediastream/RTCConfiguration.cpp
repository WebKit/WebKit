/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#include "config.h"
#include "RTCConfiguration.h"

#if ENABLE(MEDIA_STREAM)

#include "ArrayValue.h"
#include "Dictionary.h"
#include "ExceptionCode.h"
#include "URL.h"

namespace WebCore {

static bool validateIceServerURL(const String& iceURL)
{
    URL url(URL(), iceURL);
    if (url.isEmpty() || !url.isValid() || !(url.protocolIs("turn") || url.protocolIs("stun")))
        return false;

    return true;
}

static RefPtr<RTCIceServer> parseIceServer(const Dictionary& iceServer, ExceptionCode& ec)
{
    String credential, username;
    iceServer.get("credential", credential);
    iceServer.get("username", username);

    // Spec says that "urls" can be either a string or a sequence, so we must check for both.
    Vector<String> urlsList;
    String urlString;
    iceServer.get("urls", urlString);
    // This is the only way to check if "urls" is a sequence or a string. If we try to convert
    // to a sequence and it fails (in case it is a string), an exception will be set and the
    // RTCPeerConnection will fail.
    // So we convert to a string always, which converts a sequence to a string in the format: "foo, bar, ..",
    // then checking for a comma in the string assures that a string was a sequence and then we convert
    // it to a sequence safely.
    if (urlString.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    if (urlString.find(',') != notFound && iceServer.get("urls", urlsList) && urlsList.size()) {
        for (auto iter = urlsList.begin(); iter != urlsList.end(); ++iter) {
            if (!validateIceServerURL(*iter)) {
                ec = INVALID_ACCESS_ERR;
                return nullptr;
            }
        }
    } else {
        if (!validateIceServerURL(urlString)) {
            ec = INVALID_ACCESS_ERR;
            return nullptr;
        }

        urlsList.append(urlString);
    }

    return RTCIceServer::create(urlsList, credential, username);
}

RefPtr<RTCConfiguration> RTCConfiguration::create(const Dictionary& configuration, ExceptionCode& ec)
{
    if (configuration.isUndefinedOrNull())
        return nullptr;

    RefPtr<RTCConfiguration> rtcConfiguration = adoptRef(new RTCConfiguration());
    rtcConfiguration->initialize(configuration, ec);
    if (ec)
        return nullptr;

    return rtcConfiguration;
}

RTCConfiguration::RTCConfiguration()
    : m_iceTransportPolicy("all")
    , m_bundlePolicy("balanced")
{
}

void RTCConfiguration::initialize(const Dictionary& configuration, ExceptionCode& ec)
{
    ArrayValue iceServers;
    bool ok = configuration.get("iceServers", iceServers);
    if (!ok || iceServers.isUndefinedOrNull()) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    size_t numberOfServers;
    ok = iceServers.length(numberOfServers);
    if (!ok || !numberOfServers) {
        ec = !ok ? TYPE_MISMATCH_ERR : INVALID_ACCESS_ERR;
        return;
    }

    for (size_t i = 0; i < numberOfServers; ++i) {
        Dictionary iceServerDict;
        ok = iceServers.get(i, iceServerDict);
        if (!ok) {
            ec = TYPE_MISMATCH_ERR;
            return;
        }

        RefPtr<RTCIceServer> iceServer = parseIceServer(iceServerDict, ec);
        if (!iceServer)
            return;

        m_iceServers.append(WTFMove(iceServer));
    }

    String iceTransportPolicy;
    if (configuration.get("iceTransportPolicy", iceTransportPolicy)) {
        if (iceTransportPolicy == "none" || iceTransportPolicy == "relay" || iceTransportPolicy == "all")
            m_iceTransportPolicy = iceTransportPolicy;
        else {
            ec = TypeError;
            return;
        }
    }

    String bundlePolicy;
    if (configuration.get("bundlePolicy", bundlePolicy)) {
        if (bundlePolicy == "balanced" || bundlePolicy == "max-compat" || bundlePolicy == "max-bundle")
            m_bundlePolicy = bundlePolicy;
        else
            ec = TypeError;
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
