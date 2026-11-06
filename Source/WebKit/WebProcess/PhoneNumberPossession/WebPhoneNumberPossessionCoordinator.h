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

#include <WebCore/FrameIdentifier.h>
#include <WebCore/PhoneNumberPossessionCoordinatorClient.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/WeakRef.h>

namespace WebKit {

class WebPage;

class WebPhoneNumberPossessionCoordinator final : public WebCore::PhoneNumberPossessionCoordinatorClient, public CanMakeWeakPtr<WebPhoneNumberPossessionCoordinator>, public CanMakeCheckedPtr<WebPhoneNumberPossessionCoordinator> {
    WTF_MAKE_TZONE_ALLOCATED(WebPhoneNumberPossessionCoordinator);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebPhoneNumberPossessionCoordinator);
public:
    explicit WebPhoneNumberPossessionCoordinator(WebPage&);

private:
    void requestPhoneNumberPossession(const WebCore::LocalFrame&, const WebCore::PhoneNumberPossessionRequest&, RequestCompletionHandler&&) final;
    void isAvailable(const WebCore::LocalFrame&, CompletionHandler<void(bool)>&&) final;
    void cancel(CompletionHandler<void()>&&) final;
    void setMockConfigurationForTesting(const WebCore::MockPhoneNumberPossessionConfiguration&) final;

    WeakRef<WebPage> m_webPage;
    std::optional<WebCore::FrameIdentifier> m_pendingRequestFrameID;
};

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
