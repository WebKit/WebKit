/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "LoopbackCocoa.h"

#include <arpa/inet.h>
#include <cstdio>
#include <netinet/in.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

bool isLoopbackURL(const String& string)
{
    URL url { string };
    if (!url.isValid())
        return false;

    auto host = url.host();
    if (equalLettersIgnoringASCIICase(host, "localhost"_s))
        return true;

    if (host.startsWith('[') && host.endsWith(']'))
        host = host.substring(1, host.length() - 2);
    auto address = host.utf8();

    in_addr address4;
    if (inet_pton(AF_INET, address.legacyCStringPointer(), &address4) == 1)
        return IN_LOOPBACK(ntohl(address4.s_addr));

    in6_addr address6;
    if (inet_pton(AF_INET6, address.legacyCStringPointer(), &address6) == 1)
        return IN6_IS_ADDR_LOOPBACK(&address6);

    return false;
}

bool validateListeningHost(const String& host)
{
    if (host.isEmpty() || host == "local"_s)
        return true;

    SAFE_FPRINTF(stderr, "minidriver only listens on this machine, so --host=%s isn't supported.\n", host.utf8());
    return false;
}

} // namespace WebDriver
