/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/ActiveDOMCallback.h>
#include <WebCore/CallbackResult.h>
#include <WebCore/CaptionDisplaySettingsClient.h>
#include <WebCore/ResolvedCaptionDisplaySettingsOptionsWrapper.h>

namespace WebCore {

class DOMPromise;
class HTMLMediaElement;
struct ResolvedCaptionDisplaySettingsOptions;

class MockCaptionDisplaySettingsClientCallback
    : public RefCounted<MockCaptionDisplaySettingsClientCallback>
    , public ActiveDOMCallback
    , public CaptionDisplaySettingsClient {
public:
    using ActiveDOMCallback::ActiveDOMCallback;
    void ref() const override { RefCounted::ref(); }
    void deref() const override { RefCounted::deref(); }

    virtual CallbackResult<RefPtr<DOMPromise>> invoke(HTMLMediaElement&, const ResolvedCaptionDisplaySettingsOptionsWrapper&) = 0;

    void showCaptionDisplaySettings(HTMLMediaElement&, const ResolvedCaptionDisplaySettingsOptions&, CompletionHandler<void(ExceptionOr<void>)>&&) override;

private:
    virtual bool hasCallback() const = 0;
};

}
