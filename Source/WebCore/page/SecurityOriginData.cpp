/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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
#include "SecurityOriginData.h"

#include "Document.h"
#include "Frame.h"
#include "SecurityOrigin.h"
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebCore {

SecurityOriginData SecurityOriginData::fromSecurityOrigin(const SecurityOrigin& securityOrigin)
{
    SecurityOriginData securityOriginData;

    securityOriginData.protocol = securityOrigin.protocol();
    securityOriginData.host = securityOrigin.host();
    securityOriginData.port = securityOrigin.port();

    return securityOriginData;
}

#if !LOG_DISABLED
String SecurityOriginData::debugString() const
{
    return makeString(protocol, "://", host, ":", String::number(port));
}
#endif

SecurityOriginData SecurityOriginData::fromFrame(Frame* frame)
{
    if (!frame)
        return SecurityOriginData();
    
    Document* document = frame->document();
    if (!document)
        return SecurityOriginData();

    SecurityOrigin* origin = document->securityOrigin();
    if (!origin)
        return SecurityOriginData();
    
    return SecurityOriginData::fromSecurityOrigin(*origin);
}

Ref<SecurityOrigin> SecurityOriginData::securityOrigin() const
{
    return SecurityOrigin::create(protocol.isolatedCopy(), host.isolatedCopy(), port);
}

SecurityOriginData SecurityOriginData::isolatedCopy() const
{
    SecurityOriginData result;

    result.protocol = protocol.isolatedCopy();
    result.host = host.isolatedCopy();
    result.port = port;

    return result;
}

bool operator==(const SecurityOriginData& a, const SecurityOriginData& b)
{
    if (&a == &b)
        return true;

    return a.protocol == b.protocol
        && a.host == b.host
        && a.port == b.port;
}

} // namespace WebCore
