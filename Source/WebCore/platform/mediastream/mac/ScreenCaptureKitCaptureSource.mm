/*
 * Copyright (C) 2021-2026 Apple Inc. All rights reserved.
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
#import "ImageTransferSessionVT.h"
#import "Logging.h"
#import "PlatformMediaSessionManager.h"
#import "PlatformScreen.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeVideoUtilities.h"
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/mac/ScreenCaptureKitSoftLink.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

using namespace WebCore;
@interface WebCoreScreenCaptureKitHelper : NSObject<SCStreamDelegate, SCStreamOutput> {
    WeakPtr<ScreenCaptureKitCaptureSource> _callback;
}

- (instancetype)initWithCallback:(WeakPtr<ScreenCaptureKitCaptureSource>&&)callback;
- (void)disconnect;
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error;
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type;
- (void)outputVideoEffectDidStartForStream:(SCStream *)stream;
- (void)outputVideoEffectDidStopForStream:(SCStream *)stream;
@end

@implementation WebCoreScreenCaptureKitHelper
- (instancetype)initWithCallback:(WeakPtr<ScreenCaptureKitCaptureSource>&&)callback
{
    self = [super init];
    if (!self)
        return self;

    _callback = WTF::move(callback);
    return self;
}

- (void)disconnect
{
    _callback = nullptr;
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
    callOnMainRunLoop([strongSelf = RetainPtr { self }, error = RetainPtr { error }]() mutable {
        if (RefPtr callback = strongSelf->_callback.get())
            callback->sessionFailedWithError(WTF::move(error), "-[SCStreamDelegate stream:didStopWithError:] called"_s);
    });
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type
{
    ASSERT(type == SCStreamOutputTypeScreen);

    if (!sampleBuffer)
        return;

    RetainPtr attachments = (__bridge NSArray *)PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
    SCFrameStatus status = SCFrameStatusStopped;
    [attachments enumerateObjectsUsingBlock:makeBlockPtr([&] (NSDictionary *attachment, NSUInteger, BOOL *stop) {
        RetainPtr statusNumber = dynamic_objc_cast<NSNumber>(attachment[SCStreamFrameInfoStatus]);
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
        if (RefPtr callback = strongSelf->_callback.get())
            callback->streamDidOutputVideoSampleBuffer(WTF::move(sampleBuffer));
    });
}

- (void)outputVideoEffectDidStartForStream:(SCStream *)stream
{
    callOnMainRunLoop([strongSelf = RetainPtr { self }]() mutable {
        if (RefPtr callback = strongSelf->_callback.get())
            callback->outputVideoEffectDidStartForStream();
    });
}

- (void)outputVideoEffectDidStopForStream:(SCStream *)stream
{
    callOnMainRunLoop([strongSelf = RetainPtr { self }]() mutable {
        if (RefPtr callback = strongSelf->_callback.get())
            callback->outputVideoEffectDidStopForStream();
    });
}

@end

#pragma clang diagnostic pop

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScreenCaptureKitCaptureSource);

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN

bool ScreenCaptureKitCaptureSource::isAvailable()
{
    return PAL::isScreenCaptureKitFrameworkAvailable();
}

Expected<uint32_t, CaptureSourceError> ScreenCaptureKitCaptureSource::computeDeviceID(const CaptureDevice& device)
{
    ASSERT(device.type() == CaptureDevice::DeviceType::Screen || device.type() == CaptureDevice::DeviceType::Window);

    auto deviceID = parseInteger<uint32_t>(device.persistentId());
    if (!deviceID)
        return makeUnexpected(CaptureSourceError { "Invalid display device ID"_s, MediaAccessDenialReason::PermissionDenied });

    return *deviceID;
}

ScreenCaptureKitCaptureSource::ScreenCaptureKitCaptureSource(CapturerObserver& observer, const CaptureDevice& device, uint32_t deviceID)
    : DisplayCaptureSourceCocoa::Capturer(observer)
    , m_captureDevice(device)
    , m_deviceID(deviceID)
{
}

ScreenCaptureKitCaptureSource::~ScreenCaptureKitCaptureSource()
{
    if (!m_sessionSource)
        ScreenCaptureKitSharingSessionManager::singleton().cancelPendingSessionForDevice(m_captureDevice);

    if (auto callback = std::exchange(m_whenReadyCallback, { })) {
        callOnMainRunLoop([callback = WTF::move(callback)]() mutable {
            callback({ "Source no longer needed"_s , MediaAccessDenialReason::InvalidAccess });
        });
    }
}

void ScreenCaptureKitCaptureSource::whenReady(CompletionHandler<void(CaptureSourceError&&)>&& callback)
{
    if (m_didReceiveVideoFrame) {
        callback({ });
        return;
    }

    m_whenReadyCallback = WTF::move(callback);

    if (m_isRunning)
        return;

    m_isPrewarming = true;
    // We start to get the first frame. The frame size allows to finalize initialization of the source settings.
    startInternal(IsPrewarming::Yes);
}

bool ScreenCaptureKitCaptureSource::start()
{
    startInternal(IsPrewarming::No);
    return m_isRunning;
}

void ScreenCaptureKitCaptureSource::startInternal(IsPrewarming isPrewarming)
{
    ASSERT(isAvailable());
    ASSERT(!m_whenReadyCallback || !m_isRunning);

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_isPrewarming = isPrewarming == IsPrewarming::Yes;

    if (m_isRunning)
        return;

    m_isRunning = true;
    startContentStream();
}

void ScreenCaptureKitCaptureSource::stop()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_isRunning = false;
    if (!contentStream())
        return;

    auto stopHandler = makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {
        callOnMainRunLoop([weakThis = WTF::move(weakThis), error = RetainPtr { error }]() mutable {
            if (!error)
                return;

            if (RefPtr protectedThis = weakThis.get())
                protectedThis->sessionFailedWithError(WTF::move(error), "-[SCStream stopCaptureWithCompletionHandler:] failed"_s);
        });
    });
    [contentStream() stopCaptureWithCompletionHandler:stopHandler.get()];

    // We do not nullify m_sessionSource to keep the picker active since it is helping capture for some fullscreen cases.
    if (m_sessionSource)
        m_contentFilter = m_sessionSource->contentFilter();
}

void ScreenCaptureKitCaptureSource::end()
{
    stop();
}

void ScreenCaptureKitCaptureSource::sessionFailedWithError(RetainPtr<NSError>&& error, const String& message)
{
    ASSERT(isMainThread());

    if (!m_isRunning)
        return;

    ERROR_LOG_IF(loggerPtr() && error, LOGIDENTIFIER, message, " with error '", error.get(), "'");
    ERROR_LOG_IF(loggerPtr() && !error, LOGIDENTIFIER, message);

    captureFailed();
    m_sessionSource = nullptr;
}

void ScreenCaptureKitCaptureSource::sessionFilterDidChange(SCContentFilter* contentFilter)
{
    ASSERT(isMainThread());

    std::optional<CaptureDevice> device;
    switch ([contentFilter style]) {
    case SCShareableContentStyleWindow: {
        RetainPtr windows = retainPtr(contentFilter.includedWindows);
        ASSERT([windows count] == 1);
        if (![windows count])
            return;

        RetainPtr window = retainPtr(windows.get()[0]);
        device = CaptureDevice(String::number([window windowID]), CaptureDevice::DeviceType::Window, [window title], emptyString(), true);
        m_content = window;
        break;
    }
    case SCShareableContentStyleDisplay: {
        RetainPtr displays = retainPtr(contentFilter.includedDisplays);
        ASSERT([displays count] == 1);
        if (![displays count])
            return;

        RetainPtr display = retainPtr(displays.get()[0]);
        device = CaptureDevice(String::number([display displayID]), CaptureDevice::DeviceType::Screen, "Screen"_str, emptyString(), true);
        m_content = display;
        break;
    }
    case SCShareableContentStyleNone:
    case SCShareableContentStyleApplication:
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

            callOnMainRunLoop([weakThis = WTF::move(weakThis), error = RetainPtr { error }]() mutable {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->sessionFailedWithError(WTF::move(error), "-[SCStream updateContentFilter:completionHandler:] failed"_s);
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
    [m_streamConfiguration setPresenterOverlayPrivacyAlertSetting:SCPresenterOverlayAlertSettingNever];

    if (m_frameRate)
        [m_streamConfiguration setMinimumFrameInterval:PAL::CMTimeMakeWithSeconds(1 / m_frameRate, 1000)];

    auto width = m_width;
    auto height = m_height;

    if (!width && !height) {
        width = m_contentSize.width();
        height = m_contentSize.height();
    } else if (!m_contentSize.isEmpty()) {
        if (!width)
            width = height * m_contentSize.aspectRatio();
        else
            height = width / m_contentSize.aspectRatio();
    }

    if (width && height) {
        [m_streamConfiguration setWidth:width];
        [m_streamConfiguration setHeight:height];
    }

    return m_streamConfiguration;
}

void ScreenCaptureKitCaptureSource::startContentStream()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    if (!m_captureHelper)
        m_captureHelper = adoptNS([[WebCoreScreenCaptureKitHelper alloc] initWithCallback:this]);

    if (!m_contentFilter) {
        m_contentFilter = ScreenCaptureKitSharingSessionManager::singleton().contentFilter(m_captureDevice);
        m_contentSize = FloatSize { m_contentFilter.get().contentRect.size };
        m_contentSize.scale(m_contentFilter.get().pointPixelScale);
    }

    if (!m_contentFilter) {
        sessionFailedWithError(nil, "Unknown display device - no content filter"_s);
        return;
    }

    m_sessionSource = ScreenCaptureKitSharingSessionManager::singleton().createSessionSourceForDevice(*this, m_contentFilter.get(), streamConfiguration().get(), (SCStreamDelegate*)m_captureHelper.get());
    if (!m_sessionSource) {
        sessionFailedWithError(nil, "Failed to allocate stream"_s);
        return;
    }

    switch (contentFilter().style) {
    case SCShareableContentStyleWindow: {
        RetainPtr windows = retainPtr(contentFilter().includedWindows);
        ASSERT([windows count] == 1);
        if (![windows count])
            return;

        m_content = retainPtr(windows.get()[0]);
        break;
    }
    case SCShareableContentStyleDisplay: {
        RetainPtr displays = retainPtr(contentFilter().includedDisplays);
        ASSERT([displays count] == 1);
        if (![displays count])
            return;

        m_content = retainPtr(displays.get()[0]);
        break;
    }
    case SCShareableContentStyleNone:
    case SCShareableContentStyleApplication:
        ASSERT_NOT_REACHED();
        return;
        break;
    }

    NSError *error;
    if (![contentStream() addStreamOutput:m_captureHelper.get() type:SCStreamOutputTypeScreen sampleHandlerQueue:captureQueue() error:&error]) {
        sessionFailedWithError(WTF::move(error), "-[SCStream addStreamOutput:type:sampleHandlerQueue:error:] failed"_s);
        return;
    }

    auto completionHandler = makeBlockPtr([weakThis = WeakPtr { *this }, identifier = LOGIDENTIFIER] (NSError *error) mutable {
        callOnMainRunLoop([weakThis = WTF::move(weakThis), error = RetainPtr { error }, identifier]() mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (error) {
                protectedThis->sessionFailedWithError(WTF::move(error), "-[SCStream startCaptureWithCompletionHandler:] failed"_s);
                return;
            }

            protectedThis->m_intrinsicSize = { };
            protectedThis->configurationChanged();
            ALWAYS_LOG_WITH_THIS_IF_POSSIBLE(protectedThis, identifier, "stream started");
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

        callOnMainRunLoop([weakThis = WTF::move(weakThis), error = RetainPtr { error }]() mutable {
            if (RefPtr protectedThis = weakThis.get())
                weakThis->sessionFailedWithError(WTF::move(error), "-[SCStream updateConfiguration:completionHandler:] failed"_s);
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

    if (m_didReceiveVideoFrame && m_isPrewarming)
        return;

    if (!sampleBuffer) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::streamDidOutputSampleBuffer: NULL sample buffer!");
        return;
    }

    RetainPtr attachments = (__bridge NSArray *)PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer.get(), false);
    SCFrameStatus status = SCFrameStatusStopped;

    double contentScale = 1;
    double scaleFactor = 1;
    FloatRect contentRect;
    bool shouldDisallowReconfiguration = false;
    [attachments.get() enumerateObjectsUsingBlock:makeBlockPtr([weakThis = WeakPtr { *this }, &scaleFactor, &contentScale, &contentRect, &shouldDisallowReconfiguration, &status] (NSDictionary *attachment, NSUInteger, BOOL *stop) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (RetainPtr scaleFactorNumber = dynamic_objc_cast<NSNumber>(attachment[SCStreamFrameInfoScaleFactor]))
            scaleFactor = [scaleFactorNumber floatValue];

        if (RetainPtr contentScaleNumber = dynamic_objc_cast<NSNumber>(attachment[SCStreamFrameInfoContentScale]))
            contentScale = [contentScaleNumber floatValue];

        if (RetainPtr contentRectDictionary = dynamic_cf_cast<CFDictionaryRef>(attachment[SCStreamFrameInfoContentRect])) {
            CGRect cgRect;
            if (CGRectMakeWithDictionaryRepresentation(contentRectDictionary.get(), &cgRect))
                contentRect = cgRect;
        }

        if (protectedThis->m_isVideoEffectEnabled) {
            if (RetainPtr overlayRectDictionary = dynamic_cf_cast<CFDictionaryRef>(attachment[SCStreamFrameInfoPresenterOverlayContentRect])) {
                CGRect overlayRect;
                if (CGRectMakeWithDictionaryRepresentation(overlayRectDictionary.get(), &overlayRect))
                    shouldDisallowReconfiguration = overlayRect.origin.x && overlayRect.origin.y;
            }
        }

        RetainPtr statusNumber = dynamic_objc_cast<NSNumber>(attachment[SCStreamFrameInfoStatus]);
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

    m_currentFrame = WTF::move(sampleBuffer);

    if (scaleFactor != 1)
        contentRect.scale(scaleFactor);

    auto scaledContentRect = contentRect;
    if (contentScale && contentScale != 1)
        scaledContentRect.scale(1 / contentScale);

    auto areSizesRoughlyEqual = [] (auto sizeA, auto sizeB) {
        return std::fabs(sizeA.width() - sizeB.width()) < 2 && std::abs(sizeA.height() - sizeB.height()) < 2;
    };
    // FIXME: for now we will rely on cropping to handle large presenter overlay.
    // We might further want to reduce calling updateStreamConfiguration once we crop when user is resizing.
    if (!shouldDisallowReconfiguration && !areSizesRoughlyEqual(m_contentSize, scaledContentRect.size())) {
        m_contentSize = scaledContentRect.size();
        m_streamConfiguration = nullptr;
        updateStreamConfiguration();
    }

    auto intrinsicSize = FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(PAL::CMSampleBufferGetFormatDescription(m_currentFrame.get()), true, true));

    if (!areSizesRoughlyEqual(contentRect.size(), intrinsicSize)) {
        if (!m_transferSession)
            m_transferSession = ImageTransferSessionVT::create(preferedPixelBufferFormat());

        m_transferSession->setCroppingRectangle(contentRect, intrinsicSize);
        if (auto newFrame = m_transferSession->convertCMSampleBuffer(m_currentFrame.get(), IntSize { contentRect.size() })) {
            m_currentFrame = WTF::move(newFrame);
            intrinsicSize = FloatSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(PAL::CMSampleBufferGetFormatDescription(m_currentFrame.get()), true, true));
        }
    }

    if (!m_intrinsicSize || *m_intrinsicSize != IntSize(intrinsicSize)) {
        m_intrinsicSize = IntSize(intrinsicSize);
        configurationChanged();
    }

    if (!m_didReceiveVideoFrame) {
        m_didReceiveVideoFrame = true;
        if (m_whenReadyCallback)
            m_whenReadyCallback({ });
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
