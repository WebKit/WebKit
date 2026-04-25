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

#import "config.h"
#import "WebExtensionFrameIdentifier.h"

#import "WKFrameInfoPrivate.h"
#import "_WKFrameHandle.h"

namespace WebKit {

WebExtensionFrameIdentifier toWebExtensionFrameIdentifier(WKFrameInfo *frameInfo)
{
    if (frameInfo.isMainFrame)
        return WebExtensionFrameConstants::MainFrameIdentifier;

    // FIXME: <rdar://117932176> Stop using FrameIdentifier/_WKFrameHandle for WebExtensionFrameIdentifier,
    // which needs to be just one number and probably should only be generated in the UI process
    // to prevent collisions with numbers generated in different web content processes, especially with site isolation.
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto identifier = frameInfo._handle.frameID;
ALLOW_DEPRECATED_DECLARATIONS_END
    if (!WebExtensionFrameIdentifier::isValidIdentifier(identifier)) {
        ASSERT_NOT_REACHED();
        return WebExtensionFrameConstants::NoneIdentifier;
    }

    return WebExtensionFrameIdentifier { identifier };
}

}
