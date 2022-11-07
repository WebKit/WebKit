/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreArgumentCoders.h"

#include "DataReference.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/CurlProxySettings.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/text/CString.h>

namespace IPC {

using namespace WebCore;

template<typename Encoder>
void ArgumentCoder<CertificateInfo>::encode(Encoder& encoder, const CertificateInfo& certificateInfo)
{
    encoder << certificateInfo.verificationError();
    encoder << certificateInfo.certificateChain().size();

    for (auto certificate : certificateInfo.certificateChain())
        encoder << certificate;
}
template void ArgumentCoder<WebCore::CertificateInfo>::encode<Encoder>(Encoder&, const WebCore::CertificateInfo&);

template<typename Decoder>
std::optional<CertificateInfo> ArgumentCoder<CertificateInfo>::decode(Decoder& decoder)
{
    std::optional<int> verificationError;
    decoder >> verificationError;
    if (!verificationError)
        return std::nullopt;

    std::optional<size_t> certificateChainSize;
    decoder >> certificateChainSize;
    if (!certificateChainSize)
        return std::nullopt;

    CertificateInfo::CertificateChain certificateChain;
    for (size_t i = 0; i < *certificateChainSize; i++) {
        std::optional<CertificateInfo::Certificate> certificate;
        decoder >> certificate;
        if (!certificate)
            return std::nullopt;

        certificateChain.append(WTFMove(*certificate));
    }

    return CertificateInfo { *verificationError, WTFMove(certificateChain) };
}
template std::optional<WebCore::CertificateInfo> ArgumentCoder<WebCore::CertificateInfo>::decode<Decoder>(Decoder&);

void ArgumentCoder<ResourceError>::encodePlatformData(Encoder& encoder, const ResourceError& resourceError)
{
    encoder << resourceError.type();
    if (resourceError.isNull())
        return;

    encoder << resourceError.domain();
    encoder << resourceError.errorCode();
    encoder << resourceError.failingURL().string();
    encoder << resourceError.localizedDescription();
    encoder << resourceError.sslErrors();
}

bool ArgumentCoder<ResourceError>::decodePlatformData(Decoder& decoder, ResourceError& resourceError)
{
    ResourceErrorBase::Type errorType;
    if (!decoder.decode(errorType))
        return false;
    if (errorType == ResourceErrorBase::Type::Null) {
        resourceError = { };
        return true;
    }

    String domain;
    if (!decoder.decode(domain))
        return false;

    int errorCode;
    if (!decoder.decode(errorCode))
        return false;

    String failingURL;
    if (!decoder.decode(failingURL))
        return false;

    String localizedDescription;
    if (!decoder.decode(localizedDescription))
        return false;

    unsigned sslErrors;
    if (!decoder.decode(sslErrors))
        return false;

    resourceError = ResourceError(domain, errorCode, URL { failingURL }, localizedDescription, errorType);
    resourceError.setSslErrors(sslErrors);

    return true;
}

void ArgumentCoder<ProtectionSpace>::encodePlatformData(Encoder& encoder, const ProtectionSpace& space)
{
    encoder << space.host() << space.port() << space.realm();
    encoder << space.authenticationScheme();
    encoder << space.serverType();
    encoder << space.certificateInfo();
}

bool ArgumentCoder<ProtectionSpace>::decodePlatformData(Decoder& decoder, ProtectionSpace& space)
{
    String host;
    if (!decoder.decode(host))
        return false;

    int port;
    if (!decoder.decode(port))
        return false;

    String realm;
    if (!decoder.decode(realm))
        return false;

    ProtectionSpace::AuthenticationScheme authenticationScheme;
    if (!decoder.decode(authenticationScheme))
        return false;

    ProtectionSpace::ServerType serverType;
    if (!decoder.decode(serverType))
        return false;

    CertificateInfo certificateInfo;
    if (!decoder.decode(certificateInfo))
        return false;

    space = ProtectionSpace(host, port, serverType, realm, authenticationScheme, certificateInfo);
    return true;
}

void ArgumentCoder<Credential>::encodePlatformData(Encoder&, const Credential&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<Credential>::decodePlatformData(Decoder&, Credential&)
{
    ASSERT_NOT_REACHED();
    return false;
}

void ArgumentCoder<CurlProxySettings>::encode(Encoder& encoder, const CurlProxySettings& settings)
{
    encoder << settings.mode();
    if (settings.mode() != CurlProxySettings::Mode::Custom)
        return;

    encoder << settings.url();
    encoder << settings.ignoreHosts();
}

std::optional<CurlProxySettings> ArgumentCoder<CurlProxySettings>::decode(Decoder& decoder)
{
    CurlProxySettings::Mode mode;
    if (!decoder.decode(mode))
        return std::nullopt;

    if (mode != CurlProxySettings::Mode::Custom)
        return CurlProxySettings { mode };

    URL url;
    if (!decoder.decode(url))
        return std::nullopt;

    String ignoreHosts;
    if (!decoder.decode(ignoreHosts))
        return std::nullopt;

    return CurlProxySettings { WTFMove(url), WTFMove(ignoreHosts) };
}

#if ENABLE(VIDEO)
void ArgumentCoder<SerializedPlatformDataCueValue>::encodePlatformData(Encoder& encoder, const SerializedPlatformDataCueValue& value)
{
    ASSERT_NOT_REACHED();
}

std::optional<SerializedPlatformDataCueValue>  ArgumentCoder<SerializedPlatformDataCueValue>::decodePlatformData(Decoder& decoder, WebCore::SerializedPlatformDataCueValue::PlatformType platformType)
{
    ASSERT_NOT_REACHED();
    return std::nullopt;
}
#endif

}
