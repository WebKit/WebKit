/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/ObjectIdentifier.h>

namespace WebKit {

struct WebExtensionWindowIdentifierType;
using WebExtensionWindowIdentifier = ObjectIdentifier<WebExtensionWindowIdentifierType>;

namespace WebExtensionWindowConstants {

    static constexpr double None { -1 };
    static constexpr double Current { -2 };

    static constexpr const WebExtensionWindowIdentifier NoneIdentifier { std::numeric_limits<uint64_t>::max() - 1 };
    static constexpr const WebExtensionWindowIdentifier CurrentIdentifier { std::numeric_limits<uint64_t>::max() - 2 };

}

inline bool isNone(WebExtensionWindowIdentifier identifier)
{
    return identifier == WebExtensionWindowConstants::NoneIdentifier;
}

inline bool isNone(std::optional<WebExtensionWindowIdentifier> identifier)
{
    return identifier && isNone(identifier.value());
}

inline bool isCurrent(WebExtensionWindowIdentifier identifier)
{
    return identifier == WebExtensionWindowConstants::CurrentIdentifier;
}

inline bool isCurrent(std::optional<WebExtensionWindowIdentifier> identifier)
{
    return identifier && isCurrent(identifier.value());
}

inline bool isValid(std::optional<WebExtensionWindowIdentifier> identifier)
{
    return identifier && !isNone(identifier.value());
}

inline std::optional<WebExtensionWindowIdentifier> toWebExtensionWindowIdentifier(double identifier)
{
    if (identifier == WebExtensionWindowConstants::None)
        return WebExtensionWindowConstants::NoneIdentifier;

    if (identifier == WebExtensionWindowConstants::Current)
        return WebExtensionWindowConstants::CurrentIdentifier;

    if (!std::isfinite(identifier) || identifier <= 0 || identifier >= static_cast<double>(WebExtensionWindowConstants::CurrentIdentifier.toUInt64()))
        return std::nullopt;

    double integral;
    if (std::modf(identifier, &integral) != 0.0) {
        // Only integral numbers can be used.
        return std::nullopt;
    }

    WebExtensionWindowIdentifier result { static_cast<uint64_t>(identifier) };
    ASSERT(result.isValid());
    return result;
}

inline double toWebAPI(const WebExtensionWindowIdentifier& identifier)
{
    ASSERT(identifier.isValid());

    if (isNone(identifier))
        return WebExtensionWindowConstants::None;

    if (isCurrent(identifier)) {
        ASSERT_NOT_REACHED_WITH_MESSAGE("The current window identifier should not be returned to JavaScript. It is only an input value.");
        return WebExtensionWindowConstants::None;
    }

    return static_cast<double>(identifier.toUInt64());
}

}
