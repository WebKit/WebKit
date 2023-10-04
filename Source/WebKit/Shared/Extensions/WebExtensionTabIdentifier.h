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

#include <wtf/ObjectIdentifier.h>

namespace WebKit {

struct WebExtensionTabIdentifierType;
using WebExtensionTabIdentifier = ObjectIdentifier<WebExtensionTabIdentifierType>;

namespace WebExtensionTabConstants {

    static constexpr double None { -1 };

    static constexpr const WebExtensionTabIdentifier NoneIdentifier { std::numeric_limits<uint64_t>::max() - 1 };

}

inline bool isNone(WebExtensionTabIdentifier identifier)
{
    return identifier == WebExtensionTabConstants::NoneIdentifier;
}

inline bool isNone(std::optional<WebExtensionTabIdentifier> identifier)
{
    return identifier && isNone(identifier.value());
}

inline bool isValid(std::optional<WebExtensionTabIdentifier> identifier)
{
    return identifier && !isNone(identifier.value());
}

inline std::optional<WebExtensionTabIdentifier> toWebExtensionTabIdentifier(double identifier)
{
    if (identifier == WebExtensionTabConstants::None)
        return WebExtensionTabConstants::NoneIdentifier;

    if (!std::isfinite(identifier) || identifier <= 0 || identifier >= static_cast<double>(WebExtensionTabConstants::NoneIdentifier.toUInt64()))
        return std::nullopt;

    double integral;
    if (std::modf(identifier, &integral) != 0.0) {
        // Only integral numbers can be used.
        return std::nullopt;
    }

    WebExtensionTabIdentifier result { static_cast<uint64_t>(identifier) };
    ASSERT(result.isValid());
    return result;
}

inline double toWebAPI(const WebExtensionTabIdentifier& identifier)
{
    ASSERT(identifier.isValid());

    if (isNone(identifier))
        return WebExtensionTabConstants::None;

    return static_cast<double>(identifier.toUInt64());
}

}
