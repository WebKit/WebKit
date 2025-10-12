/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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
#import "AssertionCapability.h"

#if ENABLE(EXTENSION_CAPABILITIES)

#import "Logging.h"
#import "ProcessLauncher.h"
#import <BrowserEngineKit/BrowserEngineKit.h>

namespace WebKit {

AssertionCapability::AssertionCapability(String environmentIdentifier, String domain, String name, Function<void()>&& willInvalidateFunction, Function<void()>&& didInvalidateFunction)
    : m_environmentIdentifier { WTFMove(environmentIdentifier) }
    , m_domain { WTFMove(domain) }
    , m_name { WTFMove(name) }
    , m_willInvalidateBlock { willInvalidateFunction ? makeBlockPtr(WTFMove(willInvalidateFunction)) : nullptr }
    , m_didInvalidateBlock { didInvalidateFunction ? makeBlockPtr(WTFMove(didInvalidateFunction)) : nullptr }
{
    RELEASE_LOG(Process, "AssertionCapability::AssertionCapability: taking assertion %{public}s", m_name.utf8().data());
    if (m_name == "Suspended"_s)
        setPlatformCapability([BEProcessCapability suspended]);
    else if (m_name == "Background"_s)
        setPlatformCapability([BEProcessCapability background]);
    else if (m_name == "Foreground"_s)
        setPlatformCapability([BEProcessCapability foreground]);
}

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
