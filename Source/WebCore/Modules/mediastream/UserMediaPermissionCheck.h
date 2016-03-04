/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#ifndef UserMediaPermissionCheck_h
#define UserMediaPermissionCheck_h

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "MediaDevices.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class SecurityOrigin;

class UserMediaPermissionCheckClient {
public:
    virtual ~UserMediaPermissionCheckClient() { }

    virtual void didCompletePermissionCheck(const String&, bool) = 0;
};

class UserMediaPermissionCheck final : public ContextDestructionObserver, public RefCounted<UserMediaPermissionCheck> {
public:
    static Ref<UserMediaPermissionCheck> create(Document&, UserMediaPermissionCheckClient&);

    virtual ~UserMediaPermissionCheck();

    void start();
    void setClient(UserMediaPermissionCheckClient* client) { m_client = client; }

    WEBCORE_EXPORT SecurityOrigin* userMediaDocumentOrigin() const;
    WEBCORE_EXPORT SecurityOrigin* topLevelDocumentOrigin() const;

    WEBCORE_EXPORT void setUserMediaAccessInfo(const String&, bool);

    WEBCORE_EXPORT String mediaDeviceIdentifierHashSalt() const { return m_mediaDeviceIdentifierHashSalt; }

private:
    UserMediaPermissionCheck(ScriptExecutionContext&, UserMediaPermissionCheckClient&);

    // ContextDestructionObserver
    void contextDestroyed() final;

    UserMediaPermissionCheckClient* m_client;
    String m_mediaDeviceIdentifierHashSalt;
    bool m_hasPersistentPermission { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // UserMediaPermissionCheck_h
