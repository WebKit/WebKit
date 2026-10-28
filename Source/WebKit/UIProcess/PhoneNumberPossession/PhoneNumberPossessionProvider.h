/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(PHONE_NUMBER_POSSESSION)

#include "PhoneNumberPossessionTypes.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class SecurityOriginData;
}

namespace WebKit {

class WebPageProxy;

// Answers phone number possession requests on behalf of a page. On Cocoa platforms, providers call
// PhoneNumberPossessionController (Swift), which adapts requests to a manager: the platform's, or a mock
// for layout tests. Providers keep themselves alive until they call their completion handlers.
class PhoneNumberPossessionProvider : public RefCounted<PhoneNumberPossessionProvider> {
    WTF_MAKE_TZONE_ALLOCATED(PhoneNumberPossessionProvider);
public:
    using RequestCompletionHandler = CompletionHandler<void(PhoneNumberPossessionResult&&)>;

    static Ref<PhoneNumberPossessionProvider> createPlatformProvider();

    // Only for tests. Callers have to be behind [EnabledBy=AllowTestOnlyIPC] messages.
    static Ref<PhoneNumberPossessionProvider> createMock(const WebCore::MockPhoneNumberPossessionConfiguration&);

    virtual ~PhoneNumberPossessionProvider() = default;

    virtual void canRequest(CompletionHandler<void(bool)>&&) = 0;

    // The page provides the presentation anchor for any UI the provider shows.
    virtual void request(WebCore::PhoneNumberPossessionRequest&&, const WebCore::SecurityOriginData&, WebPageProxy&, RequestCompletionHandler&&) = 0;

    virtual void cancel(CompletionHandler<void()>&&) = 0;
};

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
