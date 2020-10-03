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
#include <WebCore/Font.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/text/CString.h>

namespace IPC {

using namespace WebCore;

void ArgumentCoder<ResourceRequest>::encodePlatformData(Encoder& encoder, const ResourceRequest& resourceRequest)
{
    resourceRequest.encodeWithPlatformData(encoder);
}

bool ArgumentCoder<ResourceRequest>::decodePlatformData(Decoder& decoder, ResourceRequest& resourceRequest)
{
    return resourceRequest.decodeWithPlatformData(decoder);
}

void ArgumentCoder<CertificateInfo>::encode(Encoder& encoder, const CertificateInfo& certificateInfo)
{
    encoder << certificateInfo.verificationError();
    encoder << certificateInfo.certificateChain().size();

    for (auto certificate : certificateInfo.certificateChain())
        encoder << certificate;
}

bool ArgumentCoder<CertificateInfo>::decode(Decoder& decoder, CertificateInfo& certificateInfo)
{
    int verificationError;
    if (!decoder.decode(verificationError))
        return false;

    size_t certificateChainSize;
    if (!decoder.decode(certificateChainSize))
        return false;

    CertificateInfo::CertificateChain certificateChain;
    for (size_t i = 0; i < certificateChainSize; i++) {
        CertificateInfo::Certificate certificate;
        if (!decoder.decode(certificate))
            return false;

        certificateChain.append(certificate);
    }

    certificateInfo = CertificateInfo { verificationError, WTFMove(certificateChain) };

    return true;
}

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

    resourceError = ResourceError(domain, errorCode, URL(URL(), failingURL), localizedDescription, errorType);
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

    ProtectionSpaceAuthenticationScheme authenticationScheme;
    if (!decoder.decode(authenticationScheme))
        return false;

    ProtectionSpaceServerType serverType;
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

Optional<CurlProxySettings> ArgumentCoder<CurlProxySettings>::decode(Decoder& decoder)
{
    CurlProxySettings::Mode mode;
    if (!decoder.decode(mode))
        return WTF::nullopt;

    if (mode != CurlProxySettings::Mode::Custom)
        return CurlProxySettings { mode };

    URL url;
    if (!decoder.decode(url))
        return WTF::nullopt;

    String ignoreHosts;
    if (!decoder.decode(ignoreHosts))
        return WTF::nullopt;

    return CurlProxySettings { WTFMove(url), WTFMove(ignoreHosts) };
}

void ArgumentCoder<FontAttributes>::encodePlatformData(Encoder&, const FontAttributes&)
{
    ASSERT_NOT_REACHED();
}

Optional<FontAttributes> ArgumentCoder<FontAttributes>::decodePlatformData(Decoder&, FontAttributes&)
{
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

void ArgumentCoder<DictionaryPopupInfo>::encodePlatformData(Encoder&, const DictionaryPopupInfo&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<DictionaryPopupInfo>::decodePlatformData(Decoder&, DictionaryPopupInfo&)
{
    ASSERT_NOT_REACHED();
    return false;
}

void ArgumentCoder<Ref<Font>>::encodePlatformData(Encoder&, const Ref<Font>&)
{
    ASSERT_NOT_REACHED();
}

Optional<FontPlatformData> ArgumentCoder<Ref<Font>>::decodePlatformData(Decoder&)
{
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

#if ENABLE(VIDEO)
void ArgumentCoder<SerializedPlatformDataCueValue>::encodePlatformData(Encoder& encoder, const SerializedPlatformDataCueValue& value)
{
    ASSERT_NOT_REACHED();
}

Optional<SerializedPlatformDataCueValue>  ArgumentCoder<SerializedPlatformDataCueValue>::decodePlatformData(Decoder& decoder, WebCore::SerializedPlatformDataCueValue::PlatformType platformType)
{
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}
#endif

}
