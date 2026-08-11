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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionDataType.h"
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

enum class WebExtensionStorageAccessLevel : uint8_t {
    TrustedContexts,
    TrustedAndUntrustedContexts,
};

using WebExtensionStorageAccessLevelMap = HashMap<WebExtensionDataType, WebExtensionStorageAccessLevel>;

inline WebExtensionStorageAccessLevel defaultStorageAccessLevel(WebExtensionDataType dataType)
{
    switch (dataType) {
    case WebExtensionDataType::Local:
    case WebExtensionDataType::Sync:
        return WebExtensionStorageAccessLevel::TrustedAndUntrustedContexts;
    case WebExtensionDataType::Session:
        return WebExtensionStorageAccessLevel::TrustedContexts;
    }

    ASSERT_NOT_REACHED();
    return WebExtensionStorageAccessLevel::TrustedContexts;
}

inline WebExtensionStorageAccessLevel storageAccessLevel(const WebExtensionStorageAccessLevelMap& accessLevels, WebExtensionDataType dataType)
{
    return accessLevels.getOptional(dataType).value_or(defaultStorageAccessLevel(dataType));
}

inline bool isStorageTypeAllowedInUntrustedContexts(const WebExtensionStorageAccessLevelMap& accessLevels, WebExtensionDataType dataType)
{
    return storageAccessLevel(accessLevels, dataType) == WebExtensionStorageAccessLevel::TrustedAndUntrustedContexts;
}

inline String toAPIString(WebExtensionStorageAccessLevel accessLevel)
{
    switch (accessLevel) {
    case WebExtensionStorageAccessLevel::TrustedContexts:
        return "TRUSTED_CONTEXTS"_s;
    case WebExtensionStorageAccessLevel::TrustedAndUntrustedContexts:
        return "TRUSTED_AND_UNTRUSTED_CONTEXTS"_s;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

inline std::optional<WebExtensionStorageAccessLevel> toWebExtensionStorageAccessLevel(const String& accessLevelString)
{
    if (accessLevelString == "TRUSTED_CONTEXTS"_s)
        return WebExtensionStorageAccessLevel::TrustedContexts;
    if (accessLevelString == "TRUSTED_AND_UNTRUSTED_CONTEXTS"_s)
        return WebExtensionStorageAccessLevel::TrustedAndUntrustedContexts;

    // An empty string means no access level was saved for the data type, which is expected.
    // In this case, we fallback to the default value. Anything else implies an invalid accessLevel.
    ASSERT(accessLevelString.isEmpty());

    return std::nullopt;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
