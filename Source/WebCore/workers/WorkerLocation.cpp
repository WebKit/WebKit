/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "WorkerLocation.h"

#include "SecurityOrigin.h"
#include "WebCoreOpaqueRoot.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

RefPtr<StringImpl> WorkerLocation::href() const
{
    return m_url.string();
}

RefPtr<StringImpl> WorkerLocation::protocol() const
{
    return makeString(m_url.protocol(), ':');
}

RefPtr<StringImpl> WorkerLocation::host() const
{
    return m_url.hostAndPort();
}

RefPtr<StringImpl> WorkerLocation::hostname() const
{
    return m_url.host().toString();
}

RefPtr<StringImpl> WorkerLocation::port() const
{
    auto port = m_url.port();
    return port ? makeString(*port) : emptyString();
}

RefPtr<StringImpl> WorkerLocation::pathname() const
{
    auto path = m_url.path();
    return path.isEmpty() ? "/"_si : path.toString();
}

RefPtr<StringImpl> WorkerLocation::search() const
{
    return m_url.query().isEmpty() ? emptyString() : m_url.queryWithLeadingQuestionMark().toString();
}

RefPtr<StringImpl> WorkerLocation::hash() const
{
    return m_url.fragmentIdentifier().isEmpty() ? emptyString() : m_url.fragmentIdentifierWithLeadingNumberSign().toString();
}

RefPtr<StringImpl> WorkerLocation::origin() const
{
    return m_origin;
}

WebCoreOpaqueRoot root(WorkerLocation* location)
{
    return WebCoreOpaqueRoot { location };
}

} // namespace WebCore
