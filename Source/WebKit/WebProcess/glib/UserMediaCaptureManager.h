/*
 * Copyright (C) 2020 Igalia S.L.
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

#if USE(GLIB) && ENABLE(MEDIA_STREAM)

#include "MessageReceiver.h"
#include "WebProcessSupplement.h"
#include <wtf/CheckedRef.h>
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class CaptureDevice;
struct CaptureDeviceWithCapabilities;
struct MediaDeviceHashSalts;
struct MediaStreamRequest;

enum class MediaConstraintType : uint8_t;
}

namespace WebKit {

class WebProcess;

class UserMediaCaptureManager : public WebProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(UserMediaCaptureManager);
    WTF_MAKE_NONCOPYABLE(UserMediaCaptureManager);
public:
    explicit UserMediaCaptureManager(WebProcess&);
    ~UserMediaCaptureManager();

    void ref() const final;
    void deref() const final;

    static ASCIILiteral supplementName() { return "UserMediaCaptureManager"_s; }

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages::UserMediaCaptureManager
    using ValidateUserMediaRequestConstraintsCallback = CompletionHandler<void(std::optional<WebCore::MediaConstraintType> invalidConstraint, Vector<WebCore::CaptureDevice>& audioDevices, Vector<WebCore::CaptureDevice>& videoDevices)>;
    void validateUserMediaRequestConstraints(WebCore::MediaStreamRequest, WebCore::MediaDeviceHashSalts&&, ValidateUserMediaRequestConstraintsCallback&&);
    ValidateUserMediaRequestConstraintsCallback m_validateUserMediaRequestConstraintsCallback;

    using GetMediaStreamDevicesCallback = CompletionHandler<void(Vector<WebCore::CaptureDeviceWithCapabilities>&&)>;
    void getMediaStreamDevices(bool revealIdsAndLabels, GetMediaStreamDevicesCallback&&);

    CheckedRef<WebProcess> m_process;
};

} // namespace WebKit

#endif
