/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#include <WebCore/FrameIdentifier.h>
#include <wtf/ObjectIdentifier.h>

OBJC_CLASS WKFrameInfo;

namespace WebKit {

class WebFrame;
class WebPage;
struct FrameInfoData;

struct WebExtensionFrameIdentifierType;
using WebExtensionFrameIdentifier = ObjectIdentifier<WebExtensionFrameIdentifierType>;

namespace WebExtensionFrameConstants {

static constexpr double MainFrame { 0 };
static constexpr double None { -1 };

static constexpr const WebExtensionFrameIdentifier MainFrameIdentifier { std::numeric_limits<uint64_t>::max() - 1 };
static constexpr const WebExtensionFrameIdentifier NoneIdentifier { std::numeric_limits<uint64_t>::max() - 2 };

}

inline bool isMainFrame(WebExtensionFrameIdentifier identifier)
{
    return identifier == WebExtensionFrameConstants::MainFrameIdentifier;
}

inline bool isMainFrame(std::optional<WebExtensionFrameIdentifier> identifier)
{
    return identifier && isMainFrame(identifier.value());
}

inline bool isNone(WebExtensionFrameIdentifier identifier)
{
    return identifier == WebExtensionFrameConstants::NoneIdentifier;
}

inline bool isNone(std::optional<WebExtensionFrameIdentifier> identifier)
{
    return identifier && isNone(identifier.value());
}

inline bool isValid(std::optional<WebExtensionFrameIdentifier> identifier)
{
    return identifier && !isNone(identifier.value());
}

WebCore::FrameIdentifier toWebCoreFrameIdentifier(const WebExtensionFrameIdentifier&, const WebPage&);

bool matchesFrame(const WebExtensionFrameIdentifier&, const WebFrame&);

WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(std::optional<WebCore::FrameIdentifier>);
WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(const WebFrame&);
WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(const FrameInfoData&);

#ifdef __OBJC__
WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(WKFrameInfo *);
#endif

std::optional<WebExtensionFrameIdentifier> toWebExtensionFrameIdentifier(double identifier);

inline double toWebAPI(const WebExtensionFrameIdentifier& identifier)
{
    if (isMainFrame(identifier))
        return WebExtensionFrameConstants::MainFrame;

    if (isNone(identifier))
        return WebExtensionFrameConstants::None;

    return static_cast<double>(identifier.toUInt64());
}

}
