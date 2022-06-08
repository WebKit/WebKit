/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if HAVE(SCREEN_CAPTURE_KIT)

#include "DisplayCaptureManager.h"
#include "DisplayCaptureSourceCocoa.h"
#include <wtf/BlockPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS NSError;
OBJC_CLASS SCDisplay;
OBJC_CLASS SCShareableContent;
OBJC_CLASS SCStream;
OBJC_CLASS SCContentFilter;
OBJC_CLASS SCContentSharingSession;
OBJC_CLASS SCStreamConfiguration;
OBJC_CLASS SCWindow;
OBJC_CLASS WebCoreScreenCaptureKitHelper;
using CMSampleBufferRef = struct opaqueCMSampleBuffer*;

namespace WebCore {

class ScreenCaptureKitCaptureSource final
    : public DisplayCaptureSourceCocoa::Capturer
    , public CanMakeWeakPtr<ScreenCaptureKitCaptureSource> {
public:
    static Expected<UniqueRef<DisplayCaptureSourceCocoa::Capturer>, String> create(const CaptureDevice&, const MediaConstraints*);

    explicit ScreenCaptureKitCaptureSource(const CaptureDevice&, uint32_t);
    virtual ~ScreenCaptureKitCaptureSource();

    WEBCORE_EXPORT static bool isAvailable();

    static std::optional<CaptureDevice> screenCaptureDeviceWithPersistentID(const String&);
    WEBCORE_EXPORT static void screenCaptureDevices(Vector<CaptureDevice>&);

    static void windowCaptureDevices(Vector<CaptureDevice>&);
    static std::optional<CaptureDevice> windowCaptureDeviceWithPersistentID(const String&);
    WEBCORE_EXPORT static void windowDevices(Vector<DisplayCaptureManager::WindowCaptureDevice>&);

    static void captureDeviceWithPersistentID(CaptureDevice::DeviceType, uint32_t, CompletionHandler<void(std::optional<CaptureDevice>)>&&);

    using Content = std::variant<RetainPtr<SCWindow>, RetainPtr<SCDisplay>>;
    void streamFailedWithError(RetainPtr<NSError>&&, const String&);
    enum class SampleType { Video };
    void streamDidOutputSampleBuffer(RetainPtr<CMSampleBufferRef>, SampleType);
    void sessionDidChangeContent(RetainPtr<SCContentSharingSession>);
    void sessionDidEnd(RetainPtr<SCContentSharingSession>);

private:

    // DisplayCaptureSourceCocoa::Capturer
    bool start() final;
    void stop() final;
    DisplayCaptureSourceCocoa::DisplayFrameType generateFrame() final;
    CaptureDevice::DeviceType deviceType() const final;
    RealtimeMediaSourceSettings::DisplaySurfaceType surfaceType() const final;
    void commitConfiguration(const RealtimeMediaSourceSettings&) final;
    IntSize intrinsicSize() const final;

    // LoggerHelper
    const char* logClassName() const final { return "ScreenCaptureKitCaptureSource"; }

    void startContentStream();
    void findShareableContent();
    RetainPtr<SCStreamConfiguration> streamConfiguration();
    void updateStreamConfiguration();

    using SCContentStreamUpdateCallback = void (^)(SCStream *, CMSampleBufferRef);
    SCContentStreamUpdateCallback frameAvailableHandler();

    dispatch_queue_t captureQueue();

#if HAVE(SC_CONTENT_SHARING_SESSION)
    RetainPtr<SCContentSharingSession> m_contentSharingSession;
#endif

    std::optional<Content> m_content;
    RetainPtr<WebCoreScreenCaptureKitHelper> m_captureHelper;
    RetainPtr<CMSampleBufferRef> m_currentFrame;
    RetainPtr<SCContentFilter> m_contentFilter;
    RetainPtr<SCStream> m_contentStream;
    RetainPtr<SCStreamConfiguration> m_streamConfiguration;
    OSObjectPtr<dispatch_queue_t> m_captureQueue;
    BlockPtr<void(SCStream *, CMSampleBufferRef)> m_frameAvailableHandler;
    CaptureDevice m_captureDevice;
    uint32_t m_deviceID { 0 };
    std::optional<IntSize> m_intrinsicSize;

    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    float m_frameRate { 0 };
    bool m_isRunning { false };
};

} // namespace WebCore

#endif // HAVE(SCREEN_CAPTURE_KIT)
