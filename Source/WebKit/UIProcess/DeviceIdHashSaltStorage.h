/*
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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

#pragma once

#include "UserMediaPermissionCheckProxy.h"
#include <WebCore/SecurityOrigin.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>

namespace WebKit {

class DeviceIdHashSaltStorage : public RefCounted<DeviceIdHashSaltStorage> {
public:
    static Ref<DeviceIdHashSaltStorage> create();
    ~DeviceIdHashSaltStorage() = default;

    const String& deviceIdHashSaltForOrigin(const WebCore::SecurityOrigin& documentOrigin, const WebCore::SecurityOrigin& parentOrigin);

    void getDeviceIdHashSaltOrigins(CompletionHandler<void(HashSet<WebCore::SecurityOriginData>&&)>&&);
    void deleteDeviceIdHashSaltForOrigins(const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);
    void deleteDeviceIdHashSaltOriginsModifiedSince(WallTime, CompletionHandler<void()>&&);

private:
    struct HashSaltForOrigin {
        HashSaltForOrigin(WebCore::SecurityOriginData&& documentOrigin, WebCore::SecurityOriginData&& parentOrigin, String&& deviceIdHashSalt)
            : documentOrigin(WTFMove(documentOrigin))
            , parentOrigin(WTFMove(parentOrigin))
            , deviceIdHashSalt(WTFMove(deviceIdHashSalt))
            , lastTimeUsed(WallTime::now())
        { };

        WebCore::SecurityOriginData documentOrigin;
        WebCore::SecurityOriginData parentOrigin;
        String deviceIdHashSalt;
        WallTime lastTimeUsed;
    };

    DeviceIdHashSaltStorage() = default;

    HashMap<String, std::unique_ptr<HashSaltForOrigin>> m_deviceIdHashSaltForOrigins;
};

} // namespace WebKit
