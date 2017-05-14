/*
 * Copyright (C) 2015-2015 Apple Inc. All rights reserved.
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

#ifndef UserMediaPermissionCheckProxy_h
#define UserMediaPermissionCheckProxy_h

#include "APIObject.h"
#include <wtf/text/WTFString.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class UserMediaPermissionRequestManagerProxy;

class UserMediaPermissionCheckProxy : public API::ObjectImpl<API::Object::Type::UserMediaPermissionCheck> {
public:

    using CompletionHandler = std::function<void(uint64_t, String&&, bool allowed)>;

    static Ref<UserMediaPermissionCheckProxy> create(uint64_t userMediaID, uint64_t frameID, CompletionHandler&& handler, String&& userMediaDocumentOriginIdentifier, String&& topLevelDocumentOriginIdentifier)
    {
        return adoptRef(*new UserMediaPermissionCheckProxy(userMediaID, frameID, WTFMove(handler), WTFMove(userMediaDocumentOriginIdentifier), WTFMove(topLevelDocumentOriginIdentifier)));
    }

    void setUserMediaAccessInfo(String&&, bool allowed);
    void invalidate();

    uint64_t frameID() const { return m_frameID; }
    WebCore::SecurityOrigin* userMediaDocumentSecurityOrigin() { return &m_userMediaDocumentSecurityOrigin.get(); }
    WebCore::SecurityOrigin* topLevelDocumentSecurityOrigin() { return &m_topLevelDocumentSecurityOrigin.get(); }

private:
    UserMediaPermissionCheckProxy(uint64_t userMediaID, uint64_t frameID, CompletionHandler&&, String&& userMediaDocumentOriginIdentifier, String&& topLevelDocumentOriginIdentifier);

    uint64_t m_userMediaID;
    uint64_t m_frameID;
    CompletionHandler m_completionHandler;
    Ref<WebCore::SecurityOrigin> m_userMediaDocumentSecurityOrigin;
    Ref<WebCore::SecurityOrigin> m_topLevelDocumentSecurityOrigin;
};

} // namespace WebKit

#endif // UserMediaPermissionCheckProxy_h
