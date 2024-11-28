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

#include "config.h"
#include "WebExtensionFrameIdentifier.h"

#include "FrameInfoData.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/Frame.h>

namespace WebKit {

WebCore::FrameIdentifier toWebCoreFrameIdentifier(const WebExtensionFrameIdentifier& identifier, const WebPage& page)
{
    if (isMainFrame(identifier))
        return page.mainWebFrame().frameID();

    return { ObjectIdentifier<WebCore::FrameIdentifierType> { identifier.toUInt64() }, WebCore::Process::identifier() };
}

bool matchesFrame(const WebExtensionFrameIdentifier& identifier, const WebFrame& frame)
{
    if (RefPtr coreFrame = frame.coreFrame(); coreFrame && coreFrame->isMainFrame() && isMainFrame(identifier))
        return true;

    if (RefPtr page = frame.page(); page && &page->mainWebFrame() == &frame && isMainFrame(identifier))
        return true;

    return frame.frameID().object().toUInt64() == identifier.toUInt64();
}

WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(std::optional<WebCore::FrameIdentifier> frameIdentifier)
{
    if (!frameIdentifier) {
        ASSERT_NOT_REACHED();
        return WebExtensionFrameConstants::NoneIdentifier;
    }

    auto identifierAsUInt64 = frameIdentifier->object().toUInt64();
    if (!WebExtensionFrameIdentifier::isValidIdentifier(identifierAsUInt64)) {
        ASSERT_NOT_REACHED();
        return WebExtensionFrameConstants::NoneIdentifier;
    }

    return WebExtensionFrameIdentifier { identifierAsUInt64 };
}

WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(const WebFrame& frame)
{
    if (RefPtr coreFrame = frame.coreFrame(); coreFrame && coreFrame->isMainFrame())
        return WebExtensionFrameConstants::MainFrameIdentifier;

    if (RefPtr page = frame.page(); page && &page->mainWebFrame() == &frame)
        return WebExtensionFrameConstants::MainFrameIdentifier;

    return toWebExtensionFrameIdentifier(std::optional { frame.frameID() });
}

WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(const FrameInfoData& frameInfoData)
{
    if (frameInfoData.isMainFrame)
        return WebExtensionFrameConstants::MainFrameIdentifier;

    return toWebExtensionFrameIdentifier(frameInfoData.frameID);
}

std::optional<WebExtensionFrameIdentifier> toWebExtensionFrameIdentifier(double identifier)
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

    auto identifierAsUInt64 = static_cast<uint64_t>(identifier);
    if (!WebExtensionFrameIdentifier::isValidIdentifier(identifierAsUInt64)) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return WebExtensionFrameIdentifier { identifierAsUInt64 };
}

}
