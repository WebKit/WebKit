/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ScreenCaptureKitCaptureSource.h"

#if HAVE(SCREEN_CAPTURE_KIT)

#import "DisplayCaptureManager.h"
#import "Logging.h"
#import "PlatformMediaSessionManager.h"
#import "PlatformScreen.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeVideoUtilities.h"
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/ScreenCaptureKitSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/mac/ScreenCaptureKitSoftLink.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

#if HAVE(SC_CONTENT_SHARING_PICKER)
// FIXME: Remove this once it is in a public header.
typedef NS_ENUM(NSInteger, SCPresenterOverlayAlertSetting);

typedef NS_ENUM(NSInteger, WK_SCPresenterOverlayAlertSetting) {
    WK_SCPresenterOverlayAlertSettingSystem,
    WK_SCPresenterOverlayAlertSettingNever,
    WK_SCPresenterOverlayAlertSettingAlways
};

@interface SCStreamConfiguration (SCStreamConfiguration_Pending_Public_API)
@property (nonatomic, assign) SCPresenterOverlayAlertSetting presenterOverlayPrivacyAlertSetting;
@end
#endif

using namespace WebCore;
@interface WebCoreScreenCaptureKitHelper : NSObject<SCStreamDelegate, SCStreamOutput> {
    WeakPtr<ScreenCaptureKitCaptureSource> _callback;
}

- (instancetype)initWithCallback:(WeakPtr<ScreenCaptureKitCaptureSource>&&)callback;
- (void)disconnect;
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error;
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type;
@end

@implementation WebCoreScreenCaptureKitHelper
- (instancetype)initWithCallback:(WeakPtr<ScreenCaptureKitCaptureSource>&&)callback
{
    self = [super init];
    if (!self)
        return self;

    _callback = WTFMove(callback);
    return self;
}

- (void)disconnect
{
    _callback = nullptr;
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
    callOnMainRunLoop([self, strongSelf = RetainPtr { self }, error = RetainPtr { error }]() mutable {
        if (!_callback)
            return;

        _callback->sessionFailedWithError(WTFMove(error), "-[SCStreamDelegate stream:didStopWithError:] called"_s);
    });
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type
{
    ASSERT(type == SCStreamOutputTypeScreen);

    if (!sampleBuffer)
        return;

    auto attachments = (__bridge NSArray *)PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
    SCFrameStatus status = SCFrameStatusStopped;
    [attachments enumerateObjectsUsingBlock:makeBlockPtr([&] (NSDictionary *attachment, NSUInteger, BOOL *stop) {
        auto statusNumber = (NSNumber *)attachment[SCStreamFrameInfoStatus];
        if (!statusNumber)
            return;

        status = (SCFrameStatus)[statusNumber integerValue];
        *stop = YES;
    }).get()];

    switch (status) {
    case SCFrameStatusStarted:
    case SCFrameStatusComplete:
        break;

    case SCFrameStatusIdle:
    case SCFrameStatusBlank:
    case SCFrameStatusSuspended:
    case SCFrameStatusStopped:
        return;
    }

    callOnMainRunLoop([strongSelf = RetainPtr { self }, sampleBuffer = RetainPtr { sampleBuffer }]() mutable {
        if (!strongSelf->_callback)
            return;

        strongSelf->_callback->streamDidOutputVideoSampleBuffer(WTFMove(sampleBuffer));
    });
}
@end

#pragma clang diagnostic pop

namespace WebCore {

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN

bool ScreenCaptureKitCaptureSource::isAvailable()
{
    return PAL::isScreenCaptureKitFrameworkAvailable();
}

Expected<UniqueRef<DisplayCaptureSourceCocoa::Capturer>, CaptureSourceError> ScreenCaptureKitCaptureSource::create(const CaptureDevice& device, const MediaConstraints*)
{
    ASSERT(device.type() == CaptureDevice::DeviceType::Screen || device.type() == CaptureDevice::DeviceType::Window);

    auto deviceID = parseInteger<uint32_t>(device.persistentId());
    if (!deviceID)
        return makeUnexpected(CaptureSourceError { "Invalid display device ID"_s, MediaAccessDenialReason::PermissionDenied });

    return UniqueRef<DisplayCaptureSourceCocoa::Capturer>(makeUniqueRef<ScreenCaptureKitCaptureSource>(device, deviceID.value()));
}

ScreenCaptureKitCaptureSource::ScreenCaptureKitCaptureSource(const CaptureDevice& device, uint32_t deviceID)
    : DisplayCaptureSourceCocoa::Capturer()
    , m_captureDevice(device)
    , m_deviceID(deviceID)
{
}

ScreenCaptureKitCaptureSource::~ScreenCaptureKitCaptureSource()
{
    if (!m_sessionSource)
        ScreenCaptureKitSharingSessionManager::singleton().cancelPendingSessionForDevice(m_captureDevice);
}

bool ScreenCaptureKitCaptureSource::start()
{
    ASSERT(isAvailable());

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (m_isRunning)
        return true;

    m_isRunning = true;
    startContentStream();

    return m_isRunning;
}

void ScreenCaptureKitCaptureSource::stop()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_isRunning = false;
    if (!contentStream())
        return;

    auto stopHandler = makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {
        callOnMainRunLoop([weakThis = WTFMove(weakThis), error = RetainPtr { error }]() mutable {
            if (weakThis && error)
                weakThis->sessionFailedWithError(WTFMove(error), "-[SCStream stopCaptureWithCompletionHandler:] failed"_s);
        });
    });
    [contentStream() stopCaptureWithCompletionHandler:stopHandler.get()];
    m_sessionSource = nullptr;
}

void ScreenCaptureKitCaptureSource::sessionFailedWithError(RetainPtr<NSError>&& error, const String& message)
{
    ASSERT(isMainThread());

    ERROR_LOG_IF(loggerPtr() && error, LOGIDENTIFIER, message, " with error '", error.get(), "'");
    ERROR_LOG_IF(loggerPtr() && !error, LOGIDENTIFIER, message);

    captureFailed();
    m_sessionSource = nullptr;
}

void ScreenCaptureKitCaptureSource::sessionFilterDidChange(SCContentFilter* contentFilter)
{
    ASSERT(isMainThread());

    std::optional<CaptureDevice> device;
    switch ([contentFilter type]) {
    case SCContentFilterTypeDesktopIndependentWindow: {
        auto *window = [contentFilter desktopIndependentWindowInfo].window;
        device = CaptureDevice(String::number(window.windowID), CaptureDevice::DeviceType::Window, window.title, emptyString(), true);
        m_content = window;
        break;
    }
    case SCContentFilterTypeDisplay: {
        auto *display = [contentFilter displayInfo].display;
        device = CaptureDevice(String::number(display.displayID), CaptureDevice::DeviceType::Screen, makeString("Screen"), emptyString(), true);
        m_content = display;
        break;
    }
    case SCContentFilterTypeNothing:
    case SCContentFilterTypeAppsAndWindowsPinnedToDisplay:
    case SCContentFilterTypeClientShouldImplementDefault:
        ASSERT_NOT_REACHED();
        return;
    }
    if (!device) {
        sessionFailedWithError(nil, "Unknown CaptureDevice after content changed"_s);
        return;
    }

    m_captureDevice = device.value();
    m_intrinsicSize = { };
    if (contentStream()) {
        auto completionHandler = makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {
            if (!error)
                return;

            callOnMainRunLoop([weakThis = WTFMove(weakThis), error = RetainPtr { error }]() mutable {
                if (weakThis)
                    weakThis->sessionFailedWithError(WTFMove(error), "-[SCStream updateContentFilter:completionHandler:] failed"_s);
            });
        });

        [contentStream() updateContentFilter:contentFilter completionHandler:completionHandler.get()];
    }

    configurationChanged();
}

void ScreenCaptureKitCaptureSource::sessionStreamDidEnd(SCStream* stream)
{
    ASSERT_UNUSED(stream, stream == contentStream());
    sessionFailedWithError(nil, "sessionDidEnd"_s);
}

DisplayCaptureSourceCocoa::DisplayFrameType ScreenCaptureKitCaptureSource::generateFrame()
{
    return m_currentFrame;
}

RetainPtr<SCStreamConfiguration> ScreenCaptureKitCaptureSource::streamConfiguration()
{
    if (m_streamConfiguration)
        return m_streamConfiguration;

    m_streamConfiguration = adoptNS([PAL::allocSCStreamConfigurationInstance() init]);
    [m_streamConfiguration setPixelFormat:preferedPixelBufferFormat()];
    [m_streamConfiguration setShowsCursor:YES];
    [m_streamConfiguration setQueueDepth:6];
    [m_streamConfiguration setColorSpaceName:kCGColorSpaceSRGB];
    [m_streamConfiguration setColorMatrix:kCGDisplayStreamYCbCrMatrix_SMPTE_240M_1995];
#if HAVE(SC_CONTENT_SHARING_PICKER)
    if ([m_streamConfiguration respondsToSelector:@selector(setPresenterOverlayPrivacyAlertSetting:)])
        [m_streamConfiguration setPresenterOverlayPrivacyAlertSetting:static_cast<SCPresenterOverlayAlertSetting>(WK_SCPresenterOverlayAlertSettingNever)];
#endif

    if (m_frameRate)
        [m_streamConfiguration setMinimumFrameInterval:PAL::CMTimeMakeWithSeconds(1 / m_frameRate, 1000)];

    if (m_width && m_height) {
        [m_streamConfiguration setWidth:m_width];
        [m_streamConfiguration setHeight:m_height];
    }

    return m_streamConfiguration;
}

void ScreenCaptureKitCaptureSource::startContentStream()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (contentStream())
        return;

    if (!m_captureHelper)
        m_captureHelper = adoptNS([[WebCoreScreenCaptureKitHelper alloc] initWithCallback:this]);

    m_sessionSource = ScreenCaptureKitSharingSessionManager::singleton().createSessionSourceForDevice(*this, m_captureDevice, streamConfiguration().get(), (SCStreamDelegate*)m_captureHelper.get());
    if (!m_sessionSource) {
        sessionFailedWithError(nil, "Failed to allocate stream"_s);
        return;
    }

    switch (contentFilter().type) {
    case SCContentFilterTypeDesktopIndependentWindow:
        m_content = contentFilter().desktopIndependentWindowInfo.window;
        break;
    case SCContentFilterTypeDisplay:
        m_content = contentFilter().displayInfo.display;
        break;
    case SCContentFilterTypeNothing:
    case SCContentFilterTypeAppsAndWindowsPinnedToDisplay:
    case SCContentFilterTypeClientShouldImplementDefault:
        ASSERT_NOT_REACHED();
        return;
        break;
    }

    NSError *error;
    if (![contentStream() addStreamOutput:m_captureHelper.get() type:SCStreamOutputTypeScreen sampleHandlerQueue:captureQueue() error:&error]) {
        sessionFailedWithError(WTFMove(error), "-[SCStream addStreamOutput:type:sampleHandlerQueue:error:] failed"_s);
        return;
    }

    auto completionHandler = makeBlockPtr([this, weakThis = WeakPtr { *this }, identifier = LOGIDENTIFIER] (NSError *error) mutable {
        callOnMainRunLoop([this, weakThis = WTFMove(weakThis), error = RetainPtr { error }, identifier]() mutable {
            if (!weakThis)
                return;

            if (error) {
                sessionFailedWithError(WTFMove(error), "-[SCStream startCaptureWithCompletionHandler:] failed"_s);
                return;
            }

            m_intrinsicSize = { };
            configurationChanged();
            ALWAYS_LOG_IF_POSSIBLE(identifier, "stream started");
        });
    });

    [contentStream() startCaptureWithCompletionHandler:completionHandler.get()];

    m_isRunning = true;
}

IntSize ScreenCaptureKitCaptureSource::intrinsicSize() const
{
    if (m_intrinsicSize)
        return m_intrinsicSize.value();

    if (!m_content)
        return { 640, 480 };

    auto frame = switchOn(m_content.value(),
        [] (const RetainPtr<SCDisplay> display) -> CGRect {
            return [display frame];
        },
        [] (const RetainPtr<SCWindow> window) -> CGRect {
            return [window frame];
        }
    );

    m_intrinsicSize = IntSize(static_cast<int>(frame.size.width), static_cast<int>(frame.size.height));
    return m_intrinsicSize.value();
}

void ScreenCaptureKitCaptureSource::updateStreamConfiguration()
{
    ASSERT(contentStream());

    auto completionHandler = makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {
        if (!error)
            return;

        callOnMainRunLoop([weakThis = WTFMove(weakThis), error = RetainPtr { error }]() mutable {
            if (weakThis)
                weakThis->sessionFailedWithError(WTFMove(error), "-[SCStream updateConfiguration:completionHandler:] failed"_s);
        });
    });

    [contentStream() updateConfiguration:streamConfiguration().get() completionHandler:completionHandler.get()];
}

void ScreenCaptureKitCaptureSource::commitConfiguration(const RealtimeMediaSourceSettings& settings)
{
    if (m_width == settings.width() && m_height == settings.height() && m_frameRate == settings.frameRate())
        return;

    m_width = settings.width();
    m_height = settings.height();
    m_frameRate = settings.frameRate();

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER, IntSize(m_width, m_height), ", ", m_frameRate);

    if (!contentStream())
        return;

    m_streamConfiguration = nullptr;
    updateStreamConfiguration();
}

void ScreenCaptureKitCaptureSource::streamDidOutputVideoSampleBuffer(RetainPtr<CMSampleBufferRef> sampleBuffer)
{
    ASSERT(isMainThread());
    ASSERT(sampleBuffer);

    if (!sampleBuffer) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::streamDidOutputSampleBuffer: NULL sample buffer!");
        return;
    }

    auto attachments = (__bridge NSArray *)PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer.get(), false);
    SCFrameStatus status = SCFrameStatusStopped;
    [attachments enumerateObjectsUsingBlock:makeBlockPtr([&] (NSDictionary *attachment, NSUInteger, BOOL *stop) {
        auto statusNumber = (NSNumber *)attachment[SCStreamFrameInfoStatus];
        if (!statusNumber)
            return;

        status = (SCFrameStatus)[statusNumber integerValue];
        *stop = YES;
    }).get()];

    switch (status) {
    case SCFrameStatusStarted:
    case SCFrameStatusComplete:
        break;
    case SCFrameStatusIdle:
    case SCFrameStatusBlank:
    case SCFrameStatusSuspended:
    case SCFrameStatusStopped:
        return;
    }

    m_currentFrame = WTFMove(sampleBuffer);

    auto intrinsicSize = IntSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(PAL::CMSampleBufferGetFormatDescription(m_currentFrame.get()), true, true));
    if (!m_intrinsicSize || *m_intrinsicSize != intrinsicSize) {
        m_intrinsicSize = intrinsicSize;
        configurationChanged();
    }
}

dispatch_queue_t ScreenCaptureKitCaptureSource::captureQueue()
{
    if (!m_captureQueue)
        m_captureQueue = adoptOSObject(dispatch_queue_create("CGDisplayStreamCaptureSource Capture Queue", DISPATCH_QUEUE_SERIAL));

    return m_captureQueue.get();
}

CaptureDevice::DeviceType ScreenCaptureKitCaptureSource::deviceType() const
{
    return m_captureDevice.type();
}

DisplaySurfaceType ScreenCaptureKitCaptureSource::surfaceType() const
{
    return m_captureDevice.type() == CaptureDevice::DeviceType::Screen ? DisplaySurfaceType::Monitor : DisplaySurfaceType::Window;
}

std::optional<CaptureDevice> ScreenCaptureKitCaptureSource::screenCaptureDeviceWithPersistentID(const String& displayIDString)
{
    if (!isAvailable()) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::screenCaptureDeviceWithPersistentID: screen capture unavailable");
        return std::nullopt;
    }

    auto displayID = parseInteger<uint32_t>(displayIDString);
    if (!displayID) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::screenCaptureDeviceWithPersistentID: invalid display ID");
        return std::nullopt;
    }

    return CaptureDevice(String::number(displayID.value()), CaptureDevice::DeviceType::Screen, "ScreenCaptureDevice"_s, emptyString(), true);
}

std::optional<CaptureDevice> ScreenCaptureKitCaptureSource::windowCaptureDeviceWithPersistentID(const String& windowIDString)
{
    auto windowID = parseInteger<uint32_t>(windowIDString);
    if (!windowID) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::windowCaptureDeviceWithPersistentID: invalid window ID");
        return std::nullopt;
    }

    return CaptureDevice(String::number(windowID.value()), CaptureDevice::DeviceType::Window, emptyString(), emptyString(), true);
}

ALLOW_NEW_API_WITHOUT_GUARDS_END

} // namespace WebCore

#endif // HAVE(SCREEN_CAPTURE_KIT)
