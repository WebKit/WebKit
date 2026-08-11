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

#if PLATFORM(COCOA) && HAVE(WK_SECURE_CODING_NSURLPROTECTIONSPACE)

#import "CoreIPCData.h"
#import "CoreIPCSecTrust.h"
#import "CoreIPCString.h"

#import <WebCore/ProtectionSpace.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

OBJC_CLASS NSURLProtectionSpace;

namespace WebKit {

struct CoreIPCNSURLProtectionSpaceData {
    std::optional<WebKit::CoreIPCString> host;
    uint16_t port;
    WebCore::ProtectionSpace::ServerType type;
    std::optional<WebKit::CoreIPCString> realm;
    WebCore::ProtectionSpace::AuthenticationScheme scheme;
    std::optional<CoreIPCSecTrust> trust;
    std::optional<Vector<WebKit::CoreIPCData>> distnames;
};

class CoreIPCNSURLProtectionSpace {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCNSURLProtectionSpace);
public:
    CoreIPCNSURLProtectionSpace(NSURLProtectionSpace *);
    CoreIPCNSURLProtectionSpace(CoreIPCNSURLProtectionSpaceData&&);
    CoreIPCNSURLProtectionSpace(const RetainPtr<NSURLProtectionSpace>&);

    RetainPtr<id> toID() const;
private:
    friend struct IPC::ArgumentCoder<CoreIPCNSURLProtectionSpace>;
    CoreIPCNSURLProtectionSpaceData m_data;
};

} // namespace WebKit

#endif // PLATFORM(COCOA) && HAVE(WK_SECURE_CODING_NSURLPROTECTIONSPACE)
