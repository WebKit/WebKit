/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#if HAVE(SCREEN_CAPTURE_KIT)

#include "DisplayCapturePromptType.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS NSError;
OBJC_CLASS SCContentFilter;
OBJC_CLASS SCContentSharingPicker;
OBJC_CLASS SCContentSharingSession;
OBJC_CLASS SCStream;
OBJC_CLASS SCStreamConfiguration;
OBJC_CLASS SCStreamDelegate;
OBJC_CLASS WebDisplayMediaPromptHelper;

namespace WebCore {

class CaptureDevice;
class ScreenCaptureKitSharingSessionManager;

class ScreenCaptureSessionSource
    : public RefCounted<ScreenCaptureSessionSource>
    , public CanMakeWeakPtr<ScreenCaptureSessionSource> {
public:
    class Observer : public CanMakeWeakPtr<Observer> {
    public:
        virtual ~Observer() = default;

        // Session state changes.
        virtual void sessionFilterDidChange(SCContentFilter*) = 0;
        virtual void sessionStreamDidEnd(SCStream*) = 0;
    };

    using CleanupFunction = CompletionHandler<void(ScreenCaptureSessionSource&)>;
    static Ref<ScreenCaptureSessionSource> create(WeakPtr<Observer>, RetainPtr<SCStream>, RetainPtr<SCContentFilter>, RetainPtr<SCContentSharingSession>, CleanupFunction&&);
    virtual ~ScreenCaptureSessionSource();

    SCStream* stream() const { return m_stream.get(); }
    SCContentFilter* contentFilter() const { return m_contentFilter.get(); }
    SCContentSharingSession* sharingSession() const { return m_sharingSession.get(); }
    WeakPtr<Observer> observer() const { return m_observer; }

    void updateContentFilter(SCContentFilter*);
    void streamDidEnd();

    bool operator==(const ScreenCaptureSessionSource&) const;

private:
    ScreenCaptureSessionSource(WeakPtr<Observer>&&, RetainPtr<SCStream>&&, RetainPtr<SCContentFilter>&&, RetainPtr<SCContentSharingSession>&&, CleanupFunction&&);

    RetainPtr<SCStream> m_stream;
    RetainPtr<SCContentFilter> m_contentFilter;
    RetainPtr<SCContentSharingSession> m_sharingSession;
    WeakPtr<Observer> m_observer;
    CleanupFunction m_cleanupFunction;
};

class ScreenCaptureKitSharingSessionManager : public CanMakeWeakPtr<ScreenCaptureKitSharingSessionManager> {
public:
    WEBCORE_EXPORT static ScreenCaptureKitSharingSessionManager& singleton();
    WEBCORE_EXPORT static bool isAvailable();
    WEBCORE_EXPORT static bool useSCContentSharingPicker();

    ScreenCaptureKitSharingSessionManager();
    ~ScreenCaptureKitSharingSessionManager();

    void sharingSessionDidChangeContent(SCContentSharingSession*);
    void sharingSessionDidEnd(SCContentSharingSession*);
    void contentSharingPickerSelectedFilterForStream(SCContentFilter*, SCStream*);
    void contentSharingPickerFailedWithError(NSError*);
    void contentSharingPickerUpdatedFilterForStream(SCContentFilter*, SCStream*);

    void cancelPicking();

    RefPtr<ScreenCaptureSessionSource> createSessionSourceForDevice(WeakPtr<ScreenCaptureSessionSource::Observer>, const CaptureDevice&, SCStreamConfiguration*, SCStreamDelegate*);
    void cancelPendingSessionForDevice(const CaptureDevice&);

    WEBCORE_EXPORT void promptForGetDisplayMedia(DisplayCapturePromptType, CompletionHandler<void(std::optional<CaptureDevice>)>&&);

private:
    void cleanupAllSessions();
    void completeDeviceSelection(SCContentFilter*, SCContentSharingSession* = nullptr);

    bool promptWithSCContentSharingSession(DisplayCapturePromptType);
    bool promptWithSCContentSharingPicker(DisplayCapturePromptType);

    bool promptingInProgress() const { return !!m_completionHandler; }

    void cleanupSessionSource(ScreenCaptureSessionSource&);

    WeakPtr<ScreenCaptureSessionSource> findActiveSource(SCContentSharingSession*);

    Vector<WeakPtr<ScreenCaptureSessionSource>> m_activeSources;
    Vector<std::tuple<RetainPtr<SCContentFilter>, RetainPtr<SCContentSharingSession>>> m_pendingSessions;

    RetainPtr<SCContentSharingSession> m_pendingSession;
    RetainPtr<SCContentFilter> m_pendingContentFilter;

    RetainPtr<WebDisplayMediaPromptHelper> m_promptHelper;
    CompletionHandler<void(std::optional<CaptureDevice>)> m_completionHandler;
    std::unique_ptr<RunLoop::Timer> m_promptWatchdogTimer;
};

} // namespace WebCore

#endif // HAVE(SCREEN_CAPTURE_KIT)
