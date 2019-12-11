/*
 * Copyright (C) 2008 Collin Jackson  <collinj@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if OS(WINDOWS)
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <wtf/Forward.h>

namespace WebCore {

class WEBCORE_EXPORT IPAddress {
public:
    explicit IPAddress(const struct sockaddr_in& address)
    {
        memset(&m_address, 0, sizeof(struct sockaddr_in));
        m_address = address;
    }

    const struct in_addr& getSinAddr() { return m_address.sin_addr; };

private:
    struct sockaddr_in m_address;
};

enum class DNSError { Unknown, CannotResolve, Cancelled };

using DNSAddressesOrError = Expected<Vector<WebCore::IPAddress>, DNSError>;
using DNSCompletionHandler = WTF::CompletionHandler<void(DNSAddressesOrError&&)>;

WEBCORE_EXPORT void prefetchDNS(const String& hostname);
WEBCORE_EXPORT void resolveDNS(const String& hostname, uint64_t identifier, DNSCompletionHandler&&);
WEBCORE_EXPORT void stopResolveDNS(uint64_t identifier);

}
