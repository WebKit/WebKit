/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Portions Copyright (c) 2013 Company 100 Inc. All rights reserved.
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

#include "ArgumentCodersGLib.h"
#include "DataReference.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/Credential.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/Font.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SoupNetworkProxySettings.h>
#include <wtf/text/CString.h>

namespace IPC {
using namespace WebCore;

void ArgumentCoder<ResourceError>::encodePlatformData(Encoder& encoder, const ResourceError& resourceError)
{
    encoder << resourceError.domain();
    encoder << resourceError.errorCode();
    encoder << resourceError.failingURL().string();
    encoder << resourceError.localizedDescription();

    encoder << CertificateInfo(resourceError);
}

bool ArgumentCoder<ResourceError>::decodePlatformData(Decoder& decoder, ResourceError& resourceError)
{
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

    resourceError = ResourceError(domain, errorCode, URL { failingURL }, localizedDescription);

    CertificateInfo certificateInfo;
    if (!decoder.decode(certificateInfo))
        return false;

    resourceError.setCertificate(certificateInfo.certificate().get());
    resourceError.setTLSErrors(certificateInfo.tlsErrors());
    return true;
}

void ArgumentCoder<SoupNetworkProxySettings>::encode(Encoder& encoder, const SoupNetworkProxySettings& settings)
{
    ASSERT(!settings.isEmpty());
    encoder << settings.mode;
    if (settings.mode != SoupNetworkProxySettings::Mode::Custom && settings.mode != SoupNetworkProxySettings::Mode::Auto)
        return;

    encoder << settings.defaultProxyURL;
    if (settings.mode == SoupNetworkProxySettings::Mode::Auto)
        return;

    uint32_t ignoreHostsCount = settings.ignoreHosts ? g_strv_length(settings.ignoreHosts.get()) : 0;
    encoder << ignoreHostsCount;
    if (ignoreHostsCount) {
        for (uint32_t i = 0; settings.ignoreHosts.get()[i]; ++i)
            encoder << CString(settings.ignoreHosts.get()[i]);
    }
    encoder << settings.proxyMap;
}

bool ArgumentCoder<SoupNetworkProxySettings>::decode(Decoder& decoder, SoupNetworkProxySettings& settings)
{
    if (!decoder.decode(settings.mode))
        return false;

    if (settings.mode != SoupNetworkProxySettings::Mode::Custom && settings.mode != SoupNetworkProxySettings::Mode::Auto)
        return true;

    if (!decoder.decode(settings.defaultProxyURL))
        return false;

    if (settings.mode == SoupNetworkProxySettings::Mode::Auto)
        return !settings.isEmpty();

    uint32_t ignoreHostsCount;
    if (!decoder.decode(ignoreHostsCount))
        return false;

    if (ignoreHostsCount) {
        settings.ignoreHosts.reset(g_new0(char*, ignoreHostsCount + 1));
        for (uint32_t i = 0; i < ignoreHostsCount; ++i) {
            CString host;
            if (!decoder.decode(host))
                return false;

            settings.ignoreHosts.get()[i] = g_strdup(host.data());
        }
    }

    if (!decoder.decode(settings.proxyMap))
        return false;

    return !settings.isEmpty();
}

void ArgumentCoder<ProtectionSpace>::encodePlatformData(Encoder&, const ProtectionSpace&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<ProtectionSpace>::decodePlatformData(Decoder&, ProtectionSpace&)
{
    ASSERT_NOT_REACHED();
    return false;
}

void ArgumentCoder<Credential>::encodePlatformData(Encoder& encoder, const Credential& credential)
{
    GRefPtr<GTlsCertificate> certificate = credential.certificate();
    encoder << certificate;
    encoder << credential.persistence();
}

bool ArgumentCoder<Credential>::decodePlatformData(Decoder& decoder, Credential& credential)
{
    std::optional<GRefPtr<GTlsCertificate>> certificate;
    decoder >> certificate;
    if (!certificate)
        return false;

    CredentialPersistence persistence;
    if (!decoder.decode(persistence))
        return false;

    credential = Credential(certificate->get(), persistence);
    return true;
}

void ArgumentCoder<Font>::encodePlatformData(Encoder&, const Font&)
{
    ASSERT_NOT_REACHED();
}

std::optional<FontPlatformData> ArgumentCoder<Font>::decodePlatformData(Decoder&)
{
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

#if ENABLE(VIDEO)
void ArgumentCoder<SerializedPlatformDataCueValue>::encodePlatformData(Encoder& encoder, const SerializedPlatformDataCueValue& value)
{
    ASSERT_NOT_REACHED();
}

std::optional<SerializedPlatformDataCueValue>  ArgumentCoder<SerializedPlatformDataCueValue>::decodePlatformData(Decoder& decoder, SerializedPlatformDataCueValue::PlatformType platformType)
{
    ASSERT_NOT_REACHED();
    return std::nullopt;
}
#endif

}
