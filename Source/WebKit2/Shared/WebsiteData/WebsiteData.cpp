/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "WebsiteData.h"

#include "ArgumentCoders.h"
#include <WebCore/SecurityOriginData.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

void WebsiteData::Entry::encode(IPC::Encoder& encoder) const
{
    encoder << WebCore::SecurityOriginData::fromSecurityOrigin(*origin);
    encoder.encodeEnum(type);
    encoder << size;
}

bool WebsiteData::Entry::decode(IPC::Decoder& decoder, WebsiteData::Entry& result)
{
    WebCore::SecurityOriginData securityOriginData;
    if (!decoder.decode(securityOriginData))
        return false;

    if (!decoder.decodeEnum(result.type))
        return false;

    if (!decoder.decode(result.size))
        return false;

    result.origin = securityOriginData.securityOrigin();
    return true;
}

void WebsiteData::encode(IPC::Encoder& encoder) const
{
    encoder << entries;
    encoder << hostNamesWithCookies;
#if ENABLE(NETSCAPE_PLUGIN_API)
    encoder << hostNamesWithPluginData;
#endif
}

bool WebsiteData::decode(IPC::Decoder& decoder, WebsiteData& result)
{
    if (!decoder.decode(result.entries))
        return false;
    if (!decoder.decode(result.hostNamesWithCookies))
        return false;
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (!decoder.decode(result.hostNamesWithPluginData))
        return false;
#endif

    return true;
}

}
