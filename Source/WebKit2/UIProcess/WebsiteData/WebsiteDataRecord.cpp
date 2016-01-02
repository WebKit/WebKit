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
#include "WebsiteDataRecord.h"

#include <WebCore/LocalizedStrings.h>
#include <WebCore/PublicSuffix.h>
#include <WebCore/SecurityOrigin.h>

#if PLATFORM(COCOA)
#import <WebCore/CFNetworkSPI.h>
#endif

static String displayNameForLocalFiles()
{
    return WEB_UI_STRING("Local documents on your computer", "'Website' name displayed when local documents have stored local data");
}

namespace WebKit {

String WebsiteDataRecord::displayNameForCookieHostName(const String& hostName)
{
#if PLATFORM(COCOA)
    if (hostName == String(kCFHTTPCookieLocalFileDomain))
        return displayNameForLocalFiles();
#endif

#if ENABLE(PUBLIC_SUFFIX_LIST)
    return WebCore::topPrivatelyControlledDomain(hostName.startsWith('.') ? hostName.substring(1) : hostName);
#endif

    return String();
}

#if ENABLE(NETSCAPE_PLUGIN_API)
String WebsiteDataRecord::displayNameForPluginDataHostName(const String& hostName)
{
#if ENABLE(PUBLIC_SUFFIX_LIST)
    return WebCore::topPrivatelyControlledDomain(hostName);
#endif

    return String();
}
#endif

String WebsiteDataRecord::displayNameForOrigin(const WebCore::SecurityOrigin& securityOrigin)
{
    const auto& protocol = securityOrigin.protocol();

    if (protocol == "file")
        return displayNameForLocalFiles();

#if ENABLE(PUBLIC_SUFFIX_LIST)
    if (protocol == "http" || protocol == "https")
        return WebCore::topPrivatelyControlledDomain(securityOrigin.host());
#endif

    return String();
}

void WebsiteDataRecord::add(WebsiteDataTypes type, RefPtr<WebCore::SecurityOrigin>&& origin)
{
    types |= type;

    origins.add(WTFMove(origin));
}

void WebsiteDataRecord::addCookieHostName(const String& hostName)
{
    types |= WebsiteDataTypeCookies;

    cookieHostNames.add(hostName);
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void WebsiteDataRecord::addPluginDataHostName(const String& hostName)
{
    types |= WebsiteDataTypePlugInData;

    pluginDataHostNames.add(hostName);
}
#endif

}
