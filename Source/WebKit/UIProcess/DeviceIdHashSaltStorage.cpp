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

#include "config.h"
#include "DeviceIdHashSaltStorage.h"

#include <WebCore/FileSystem.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/HexNumber.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

// FIXME: Implement persistency.

namespace WebKit {
using namespace WebCore;

static const int hashSaltSize = 48;
static const int randomDataSize = hashSaltSize / 16;

Ref<DeviceIdHashSaltStorage> DeviceIdHashSaltStorage::create()
{
    return adoptRef(*new DeviceIdHashSaltStorage());
}

const String& DeviceIdHashSaltStorage::regenerateDeviceIdHashSaltForOrigin(UserMediaPermissionCheckProxy& request)
{
    auto& documentOrigin = request.topLevelDocumentSecurityOrigin();

    auto documentOriginData = documentOrigin.data();
    m_deviceIdHashSaltForOrigins.removeIf([&documentOriginData](auto& keyAndValue) {
        return keyAndValue.value->documentOrigin == documentOriginData;
    });

    return deviceIdHashSaltForOrigin(documentOrigin);
}

const String& DeviceIdHashSaltStorage::deviceIdHashSaltForOrigin(SecurityOrigin& documentOrigin)
{
    auto& deviceIdHashSalt = m_deviceIdHashSaltForOrigins.ensure(documentOrigin.toRawString(), [&documentOrigin] () {
        uint64_t randomData[randomDataSize];
        cryptographicallyRandomValues(reinterpret_cast<unsigned char*>(randomData), sizeof(randomData));

        StringBuilder builder;
        builder.reserveCapacity(hashSaltSize);
        for (int i = 0; i < randomDataSize; i++)
            appendUnsigned64AsHex(randomData[0], builder);

        String deviceIdHashSalt = builder.toString();

        return std::make_unique<HashSaltForOrigin>(documentOrigin.data().isolatedCopy(), WTFMove(deviceIdHashSalt));
    }).iterator->value;

    deviceIdHashSalt->lastTimeUsed = WallTime::now();

    return deviceIdHashSalt->deviceIdHashSalt;
}

void DeviceIdHashSaltStorage::getDeviceIdHashSaltOrigins(CompletionHandler<void(HashSet<SecurityOriginData>&&)>&& completionHandler)
{
    HashSet<SecurityOriginData> origins;

    for (auto& hashSaltForOrigin : m_deviceIdHashSaltForOrigins)
        origins.add(hashSaltForOrigin.value->documentOrigin);

    RunLoop::main().dispatch([origins = WTFMove(origins), completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(WTFMove(origins));
    });
}

void DeviceIdHashSaltStorage::deleteDeviceIdHashSaltForOrigins(const Vector<SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    m_deviceIdHashSaltForOrigins.removeIf([&origins](auto& keyAndValue) {
        return origins.contains(keyAndValue.value->documentOrigin);
    });

    RunLoop::main().dispatch(WTFMove(completionHandler));
}

void DeviceIdHashSaltStorage::deleteDeviceIdHashSaltOriginsModifiedSince(WallTime time, CompletionHandler<void()>&& completionHandler)
{
    m_deviceIdHashSaltForOrigins.removeIf([time](auto& keyAndValue) {
        return keyAndValue.value->lastTimeUsed > time;
    });

    RunLoop::main().dispatch(WTFMove(completionHandler));
}

} // namespace WebKit
