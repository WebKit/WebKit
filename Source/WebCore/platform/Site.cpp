/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "Site.h"

#include "BlobURL.h"
#include <wtf/HashFunctions.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const String& invalidProtocol()
{
    // Valid protocols from canonicalized URLs are always lower case.
    static NeverDestroyed<String> protocol { "invalidProtocol"_s };
    return protocol.get();
}

static String nonEmptyProtocol(String&& protocol)
{
    if (protocol.isEmpty())
        return invalidProtocol();
    return String(WTF::move(protocol));
}

static String protocolForURL(const URL& url)
{
    if (url.protocolIsBlob())
        return BlobURL::getOriginURL(url).protocol().toString();
    return url.protocol().toString();
}

Site::Site(const URL& url)
    : m_protocol(nonEmptyProtocol(protocolForURL(url)))
    , m_domain(url) { }

Site::Site(String&& protocol, RegistrableDomain&& domain)
    : m_protocol(nonEmptyProtocol(WTF::move(protocol)))
    , m_domain(WTF::move(domain)) { }

Site::Site(const SecurityOriginData& data)
    : m_protocol(nonEmptyProtocol(String(data.protocol())))
    , m_domain(data) { }

const String& Site::protocol() const
{
    if (m_protocol == invalidProtocol()) [[unlikely]]
        return emptyString();
    return m_protocol;
}

unsigned Site::hash() const
{
    return WTF::pairIntHash(m_protocol.hash(), m_domain.hash());
}

bool Site::matches(const URL& url) const
{
    return protocolForURL(url) == m_protocol && m_domain.matches(url);
}

String Site::toString() const
{
    return isEmpty() ? emptyString() : makeString(m_protocol, "://"_s, m_domain.string());
}

String Site::loggingString() const
{
    return makeString(m_protocol, "://"_s, m_domain.string());
}

TextStream& operator<<(TextStream& ts, const Site& site)
{
    ts << site.loggingString();
    return ts;
}

} // namespace WebKit
