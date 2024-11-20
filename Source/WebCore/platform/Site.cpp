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

#include <wtf/HashFunctions.h>

namespace WebCore {

Site::Site(const URL& url)
    : m_protocol(url.protocol().toString())
    , m_domain(url) { }

Site::Site(String&& protocol, RegistrableDomain&& domain)
    : m_protocol(WTFMove(protocol))
    , m_domain(WTFMove(domain)) { }

Site::Site(const SecurityOriginData& data)
    : m_protocol(data.protocol())
    , m_domain(data) { }

unsigned Site::hash() const
{
    return WTF::pairIntHash(m_protocol.hash(), m_domain.hash());
}

bool Site::matches(const URL& url) const
{
    return url.protocol() == m_protocol && m_domain.matches(url);
}

String Site::string() const
{
    return isEmpty() ? emptyString() : makeString(m_protocol, "://"_s, m_domain.string());
}

} // namespace WebKit
