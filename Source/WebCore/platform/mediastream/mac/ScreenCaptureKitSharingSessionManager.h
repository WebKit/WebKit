/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if HAVE(SC_CONTENT_SHARING_SESSION)

#include <wtf/CompletionHandler.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS SCContentFilter;
OBJC_CLASS SCContentSharingSession;
OBJC_CLASS WebDisplayMediaPromptHelper;

namespace WebCore {

class CaptureDevice;

class ScreenCaptureKitSharingSessionManager : public CanMakeWeakPtr<ScreenCaptureKitSharingSessionManager> {
public:
    WEBCORE_EXPORT static ScreenCaptureKitSharingSessionManager& singleton();
    WEBCORE_EXPORT static bool isAvailable();
    WEBCORE_EXPORT static bool shouldUseSystemPicker();

    ScreenCaptureKitSharingSessionManager();
    ~ScreenCaptureKitSharingSessionManager();

    RetainPtr<SCContentSharingSession> takeSharingSessionForFilter(SCContentFilter*);

    void sessionDidChangeContent(RetainPtr<SCContentSharingSession>);
    void pickerCanceledForSession(RetainPtr<SCContentSharingSession>);
    void sessionDidEnd(RetainPtr<SCContentSharingSession>);

    WEBCORE_EXPORT void showWindowPicker(CompletionHandler<void(std::optional<CaptureDevice>)>&&);
    WEBCORE_EXPORT void showScreenPicker(CompletionHandler<void(std::optional<CaptureDevice>)>&&);

private:
    void cleanupAllSessions();

    enum class PromptType { Window, Screen };
    void promptForGetDisplayMedia(PromptType, CompletionHandler<void(std::optional<CaptureDevice>)>&&);

    Vector<RetainPtr<SCContentSharingSession>> m_pendingCaptureSessions;
    RetainPtr<WebDisplayMediaPromptHelper> m_promptHelper;
    CompletionHandler<void(std::optional<CaptureDevice>)> m_completionHandler;
    std::unique_ptr<RunLoop::Timer> m_promptWatchdogTimer;
};

} // namespace WebCore

#endif // HAVE(SC_CONTENT_SHARING_SESSION)
