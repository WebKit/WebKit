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

#import "config.h"
#import "AssertionCapability.h"

#if ENABLE(EXTENSION_CAPABILITIES)

#import "ExtensionKitSoftLink.h"

namespace WebKit {

AssertionCapability::AssertionCapability(String environmentIdentifier, String domain, String name, Function<void()>&& willInvalidateFunction, Function<void()>&& didInvalidateFunction)
    : m_environmentIdentifier { WTFMove(environmentIdentifier) }
    , m_domain { WTFMove(domain) }
    , m_name { WTFMove(name) }
    , m_willInvalidateBlock { makeBlockPtr(WTFMove(willInvalidateFunction)) }
    , m_didInvalidateBlock { makeBlockPtr(WTFMove(didInvalidateFunction)) }
{
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
RetainPtr<_SECapabilities> AssertionCapability::platformCapability() const
ALLOW_DEPRECATED_DECLARATIONS_END
{
#if USE(EXTENSIONKIT)
    if ([get_SECapabilitiesClass() respondsToSelector:@selector(assertionWithDomain:name:environmentIdentifier:willInvalidate:didInvalidate:)])
        return [get_SECapabilitiesClass() assertionWithDomain:m_domain name:m_name environmentIdentifier:environmentIdentifier() willInvalidate:m_willInvalidateBlock.get() didInvalidate:m_didInvalidateBlock.get()];
    if ([get_SECapabilitiesClass() respondsToSelector:@selector(assertionWithDomain:name:environmentIdentifier:)])
        return [get_SECapabilitiesClass() assertionWithDomain:m_domain name:m_name environmentIdentifier:environmentIdentifier()];
#endif
    return nil;
}

} // namespace WebKit

#endif // ENABLE(EXTENSION_CAPABILITIES)
