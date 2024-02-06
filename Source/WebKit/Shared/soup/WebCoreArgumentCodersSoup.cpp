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
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SoupNetworkProxySettings.h>
#include <wtf/text/CString.h>

namespace IPC {
using namespace WebCore;

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

}
