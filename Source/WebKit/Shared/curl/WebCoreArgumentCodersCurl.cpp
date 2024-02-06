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

}
