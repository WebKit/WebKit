/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebSecurityOrigin_h
#define WebSecurityOrigin_h

#include "APIObject.h"
#include <WebCore/SecurityOrigin.h>
#include <wtf/PassRefPtr.h>

namespace WebKit {

class WebSecurityOrigin : public API::ObjectImpl<API::Object::Type::SecurityOrigin> {
public:
    static PassRefPtr<WebSecurityOrigin> createFromString(const String& string)
    {
        return create(WebCore::SecurityOrigin::createFromString(string));
    }

    static PassRefPtr<WebSecurityOrigin> create(const String& protocol, const String& host, int port)
    {
        return create(WebCore::SecurityOrigin::create(protocol, host, port));
    }

    static PassRefPtr<WebSecurityOrigin> create(PassRefPtr<WebCore::SecurityOrigin> securityOrigin)
    {
        if (!securityOrigin)
            return 0;
        return adoptRef(new WebSecurityOrigin(securityOrigin));
    }

    WebCore::SecurityOrigin& securityOrigin() const { return *m_securityOrigin; }

private:
    WebSecurityOrigin(PassRefPtr<WebCore::SecurityOrigin> securityOrigin)
        : m_securityOrigin(securityOrigin)
    {
    }

    RefPtr<WebCore::SecurityOrigin> m_securityOrigin;
};

} // namespace WebKit

#endif
