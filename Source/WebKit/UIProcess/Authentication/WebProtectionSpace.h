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

#ifndef WebProtectionSpace_h
#define WebProtectionSpace_h

#include "APIObject.h"
#include <WebCore/ProtectionSpace.h>

namespace WebKit {

class WebProtectionSpace : public API::ObjectImpl<API::Object::Type::ProtectionSpace> {
public:
    static Ref<WebProtectionSpace> create(const WebCore::ProtectionSpace& protectionSpace)
    {
        return adoptRef(*new WebProtectionSpace(protectionSpace));
    }
    
    const String& protocol() const;
    const String& host() const;
    int port() const;
    const String& realm() const;
    bool isProxy() const;
    WebCore::ProtectionSpace::ServerType serverType() const;
    bool receivesCredentialSecurely() const;
    WebCore::ProtectionSpace::AuthenticationScheme authenticationScheme() const;

    const WebCore::ProtectionSpace& protectionSpace() const { return m_coreProtectionSpace; }

private:
    explicit WebProtectionSpace(const WebCore::ProtectionSpace&);

    WebCore::ProtectionSpace m_coreProtectionSpace;
};

} // namespace WebKit

#endif // WebProtectionSpace_h
