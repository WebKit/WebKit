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

#include "FrameInfoData.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameIdentifier.h>
#include <wtf/ObjectIdentifier.h>

#ifdef __OBJC__
#import "WKFrameInfoPrivate.h"
#import "_WKFrameHandle.h"
#endif

namespace WebKit {

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

inline WebCore::FrameIdentifier toWebCoreFrameIdentifier(const WebExtensionFrameIdentifier& identifier, const WebPage& page)
{
    if (isMainFrame(identifier))
        return page.mainWebFrame().frameID();

    WebCore::FrameIdentifier result { ObjectIdentifier<WebCore::FrameIdentifierType> { identifier.toUInt64() }, WebCore::Process::identifier() };
    ASSERT(result.object().isValid());
    return result;
}

inline bool matchesFrame(const WebExtensionFrameIdentifier& identifier, const WebFrame& frame)
{
    if (auto* coreFrame = frame.coreFrame(); coreFrame->isMainFrame() && isMainFrame(identifier))
        return true;

    if (auto* page = frame.page(); &page->mainWebFrame() == &frame && isMainFrame(identifier))
        return true;

    return frame.frameID().object().toUInt64() == identifier.toUInt64();
}

inline WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(WebCore::FrameIdentifier frameIdentifier)
{
    WebExtensionFrameIdentifier result { frameIdentifier.object().toUInt64() };
    ASSERT(result.isValid());
    return result;
}

inline WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(const WebFrame& frame)
{
    if (auto* coreFrame = frame.coreFrame(); coreFrame->isMainFrame())
        return WebExtensionFrameConstants::MainFrameIdentifier;

    if (auto* page = frame.page(); &page->mainWebFrame() == &frame)
        return WebExtensionFrameConstants::MainFrameIdentifier;

    return toWebExtensionFrameIdentifier(frame.frameID());
}

inline WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(const FrameInfoData& frameInfoData)
{
    if (frameInfoData.isMainFrame)
        return WebExtensionFrameConstants::MainFrameIdentifier;

    return toWebExtensionFrameIdentifier(frameInfoData.frameID);
}

#ifdef __OBJC__
inline WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(WKFrameInfo *frameInfo)
{
    if (frameInfo.isMainFrame)
        return WebExtensionFrameConstants::MainFrameIdentifier;

    // FIXME: <rdar://117932176> Stop using FrameIdentifier/_WKFrameHandle for WebExtensionFrameIdentifier,
    // which needs to be just one number and probably should only be generated in the UI process
    // to prevent collisions with numbers generated in different web content processes, especially with site isolation.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WebExtensionFrameIdentifier result { frameInfo._handle.frameID };
ALLOW_DEPRECATED_DECLARATIONS_END
    ASSERT(result.isValid());
    return result;
}
#endif // __OBJC__

inline std::optional<WebExtensionFrameIdentifier> toWebExtensionFrameIdentifier(double identifier)
{
    if (identifier == WebExtensionFrameConstants::MainFrame)
        return WebExtensionFrameConstants::MainFrameIdentifier;

    if (identifier == WebExtensionFrameConstants::None)
        return WebExtensionFrameConstants::NoneIdentifier;

    if (!std::isfinite(identifier) || identifier <= 0 || identifier >= static_cast<double>(WebExtensionFrameConstants::NoneIdentifier.toUInt64()))
        return std::nullopt;

    double integral;
    if (std::modf(identifier, &integral) != 0.0) {
        // Only integral numbers can be used.
        return std::nullopt;
    }

    WebExtensionFrameIdentifier result { static_cast<uint64_t>(identifier) };
    ASSERT(result.isValid());
    return result;
}

inline double toWebAPI(const WebExtensionFrameIdentifier& identifier)
{
    ASSERT(identifier.isValid());

    if (isMainFrame(identifier))
        return WebExtensionFrameConstants::MainFrame;

    if (isNone(identifier))
        return WebExtensionFrameConstants::None;

    return static_cast<double>(identifier.toUInt64());
}

}
