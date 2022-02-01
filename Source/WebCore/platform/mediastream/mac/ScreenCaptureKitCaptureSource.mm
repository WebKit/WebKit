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

#include "config.h"
#include "ScreenCaptureKitCaptureSource.h"

#if HAVE(SCREEN_CAPTURE_KIT)

#import "DisplayCaptureManager.h"
#import "Logging.h"
#import "PlatformMediaSessionManager.h"
#import "PlatformScreen.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeVideoUtilities.h"
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/UUID.h>
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/mac/ScreenCaptureKitSoftLink.h>

typedef NS_ENUM(NSInteger, WKSCFrameStatus) {
    WKSCFrameStatusFrameComplete,
    WKSCFrameStatusFrameIdle,
    WKSCFrameStatusFrameBlank,
    WKSCFrameStatusFrameSuspended,
    WKSCFrameStatusFrameStarted,
    WKSCFrameStatusFrameStopped
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

using namespace WebCore;
@interface WebCoreScreenCaptureKitHelper : NSObject<SCStreamDelegate> {
    WeakPtr<ScreenCaptureKitCaptureSource> _callback;
}

- (instancetype)initWithCallback:(WeakPtr<ScreenCaptureKitCaptureSource>&&)callback;
- (void)disconnect;
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error;
@end

@implementation WebCoreScreenCaptureKitHelper
- (instancetype)initWithCallback:(WeakPtr<ScreenCaptureKitCaptureSource>&&)callback
{
    self = [super init];
    if (!self)
        return self;

    return self;
}

- (void)disconnect
{
    _callback = nullptr;
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
    callOnMainRunLoop([strongSelf = RetainPtr { self }, error = RetainPtr { error }]() mutable {
        if (!strongSelf->_callback)
            return;

        strongSelf->_callback->streamFailedWithError(WTFMove(error), "-[SCStreamDelegate stream:didStopWithError:] called"_s);
    });
}
@end

#pragma clang diagnostic pop

namespace WebCore {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static void forEachNSWindow(const Function<bool(NSDictionary *, unsigned, const String&)>&);

bool ScreenCaptureKitCaptureSource::m_enabled;

void ScreenCaptureKitCaptureSource::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool ScreenCaptureKitCaptureSource::isAvailable()
{
    return m_enabled && PAL::isScreenCaptureKitFrameworkAvailable();
}

Expected<UniqueRef<DisplayCaptureSourceCocoa::Capturer>, String> ScreenCaptureKitCaptureSource::create(const CaptureDevice& device, const MediaConstraints*)
{
    ASSERT(device.type() == CaptureDevice::DeviceType::Screen || device.type() == CaptureDevice::DeviceType::Window);

    auto deviceID = parseInteger<uint32_t>(device.persistentId());
    if (!deviceID)
        return makeUnexpected("Invalid display device ID"_s);

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
}

bool ScreenCaptureKitCaptureSource::start()
{
    ASSERT(isAvailable());

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    if (m_isRunning)
        return true;

    m_isRunning = true;
    startContentStream();

    return m_isRunning;
}

void ScreenCaptureKitCaptureSource::stop()
{
    if (!m_isRunning)
        return;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    m_isRunning = false;

    if (m_contentStream) {
        [m_contentStream stopWithCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {
            if (!error)
                return;

            callOnMainRunLoop([weakThis = WTFMove(weakThis), error = RetainPtr { error }]() mutable {
                if (weakThis)
                    weakThis->streamFailedWithError(WTFMove(error), "-[SCStream stopWithCompletionHandler:] failed"_s);
            });
        }).get()];
    }
}

void ScreenCaptureKitCaptureSource::streamFailedWithError(RetainPtr<NSError>&& error, const String& message)
{
    ASSERT(isMainThread());

    ERROR_LOG_IF(loggerPtr() && error, LOGIDENTIFIER, message, " with error '", [[error localizedDescription] UTF8String], "'");
    ERROR_LOG_IF(loggerPtr() && !error, LOGIDENTIFIER, message);

    captureFailed();
}

DisplayCaptureSourceCocoa::DisplayFrameType ScreenCaptureKitCaptureSource::generateFrame()
{
    return m_currentFrame;
}

void ScreenCaptureKitCaptureSource::processSharableContent(RetainPtr<SCShareableContent>&& shareableContent, RetainPtr<NSError>&& error)
{
    ASSERT(isMainRunLoop());

    if (error) {
        streamFailedWithError(WTFMove(error), "-[SCStream getShareableContentWithCompletionHandler:] failed"_s);
        return;
    }

    if (m_captureDevice.type() == CaptureDevice::DeviceType::Screen) {
        [[shareableContent displays] enumerateObjectsUsingBlock:makeBlockPtr([&] (SCDisplay *display, NSUInteger, BOOL *stop) {
            if (display.displayID == m_deviceID) {
                m_content = display;
                *stop = YES;
            }
        }).get()];
    } else if (m_captureDevice.type() == CaptureDevice::DeviceType::Window) {
        [[shareableContent windows] enumerateObjectsUsingBlock:makeBlockPtr([&] (SCWindow *window, NSUInteger, BOOL *stop) {
            if (window.windowID == m_deviceID) {
                m_content = window;
                *stop = YES;
            }
        }).get()];
    } else {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!m_content) {
        streamFailedWithError(nil, "capture device not found"_s);
        return;
    }

    startContentStream();
}

void ScreenCaptureKitCaptureSource::findShareableContent()
{
    ASSERT(!m_content);

    [PAL::getSCShareableContentClass() getShareableContentWithCompletionHandler:makeBlockPtr([weakThis = WeakPtr { *this }] (SCShareableContent *shareableContent, NSError *error) mutable {
        callOnMainRunLoop([weakThis = WTFMove(weakThis), shareableContent = RetainPtr { shareableContent }, error = RetainPtr { error }]() mutable {
            if (weakThis)
                weakThis->processSharableContent(WTFMove(shareableContent), WTFMove(error));
        });
    }).get()];
}

RetainPtr<SCStreamConfiguration> ScreenCaptureKitCaptureSource::streamConfiguration()
{
    if (m_streamConfiguration)
        return m_streamConfiguration;

    m_streamConfiguration = adoptNS([PAL::allocSCStreamConfigurationInstance() init]);
    [m_streamConfiguration setPixelFormat:preferedPixelBufferFormat()];
    [m_streamConfiguration setShowsCursor:YES];
    [m_streamConfiguration setQueueDepth:6];
    [m_streamConfiguration setColorSpaceName:kCGColorSpaceLinearSRGB];
    [m_streamConfiguration setColorMatrix:kCGDisplayStreamYCbCrMatrix_SMPTE_240M_1995];

    if (m_frameRate)
        [m_streamConfiguration setMinimumFrameTime:1 / m_frameRate];

    if (m_width && m_height) {
        [m_streamConfiguration setWidth:m_width];
        [m_streamConfiguration setHeight:m_height];
    }

    return m_streamConfiguration;
}

void ScreenCaptureKitCaptureSource::startContentStream()
{
    if (m_contentStream)
        return;

    if (!m_content) {
        findShareableContent();
        return;
    }

    m_contentFilter = switchOn(m_content.value(),
        [] (const RetainPtr<SCDisplay> display) -> RetainPtr<SCContentFilter> {
            return adoptNS([PAL::allocSCContentFilterInstance() initWithDisplay:display.get() excludingWindows:nil]);
        },
        [] (const RetainPtr<SCWindow> window)  -> RetainPtr<SCContentFilter> {
            return adoptNS([PAL::allocSCContentFilterInstance() initWithDesktopIndependentWindow:window.get()]);
        }
    );

    if (!m_contentFilter) {
        streamFailedWithError(nil, "Failed to allocate SCContentFilter"_s);
        return;
    }

    if (!m_captureHelper)
        m_captureHelper = ([[WebCoreScreenCaptureKitHelper alloc] initWithCallback:this]);

    m_contentStream = adoptNS([PAL::allocSCStreamInstance() initWithFilter:m_contentFilter.get() captureOutputProperties:streamConfiguration().get() delegate:m_captureHelper.get()]);
    if (!m_contentStream) {
        streamFailedWithError(nil, "Failed to allocate SLContentStream"_s);
        return;
    }

    auto completionHandler = makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {
        if (!error)
            return;

        callOnMainRunLoop([weakThis = WTFMove(weakThis), error = RetainPtr { error }]() mutable {
            if (weakThis)
                weakThis->streamFailedWithError(WTFMove(error), "-[SCStream startCaptureWithFrameHandler:completionHandler:] failed"_s);
        });
    });

    [m_contentStream startCaptureWithFrameHandler:frameAvailableHandler() completionHandler:completionHandler.get()];

    m_isRunning = true;
}

IntSize ScreenCaptureKitCaptureSource::intrinsicSize() const
{
    if (m_intrinsicSize)
        return m_intrinsicSize.value();

    if (m_captureDevice.type() == CaptureDevice::DeviceType::Screen) {
        auto displayMode = adoptCF(CGDisplayCopyDisplayMode(m_deviceID));
        auto screenWidth = CGDisplayModeGetPixelsWide(displayMode.get());
        auto screenHeight = CGDisplayModeGetPixelsHigh(displayMode.get());

        return { Checked<int>(screenWidth), Checked<int>(screenHeight) };
    }

    CGRect bounds = CGRectZero;
    forEachNSWindow([&] (NSDictionary *windowInfo, unsigned windowID, const String&) mutable {
        if (windowID != m_deviceID)
            return false;

        NSDictionary *boundsDict = windowInfo[(__bridge NSString *)kCGWindowBounds];
        if (![boundsDict isKindOfClass:NSDictionary.class])
            return false;

        CGRectMakeWithDictionaryRepresentation((CFDictionaryRef)boundsDict, &bounds);
        return true;
    });

    return { static_cast<int>(bounds.size.width), static_cast<int>(bounds.size.height) };
}

void ScreenCaptureKitCaptureSource::updateStreamConfiguration()
{
    ASSERT(m_contentStream);

    [m_contentStream updateStreamConfiguration:streamConfiguration().get() completionHandler:makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {
        if (!error)
            return;

        callOnMainRunLoop([weakThis = WTFMove(weakThis), error = RetainPtr { error }]() mutable {
            if (weakThis)
                weakThis->streamFailedWithError(WTFMove(error), "-[SCStream updateStreamConfiguration:] failed"_s);
        });
    }).get()];

}

void ScreenCaptureKitCaptureSource::commitConfiguration(const RealtimeMediaSourceSettings& settings)
{
    if (m_width == settings.width() && m_height == settings.height() && m_frameRate == settings.frameRate())
        return;

    m_width = settings.width();
    m_height = settings.height();
    m_frameRate = settings.frameRate();

    if (m_contentStream) {
        m_streamConfiguration = nullptr;
        updateStreamConfiguration();
    }
}

ScreenCaptureKitCaptureSource::SCContentStreamUpdateCallback ScreenCaptureKitCaptureSource::frameAvailableHandler()
{
    if (m_frameAvailableHandler)
        return m_frameAvailableHandler.get();

    m_frameAvailableHandler = makeBlockPtr([weakThis = WeakPtr { *this }] (SCStream *, CMSampleBufferRef sampleBuffer) mutable {
        if (!weakThis)
            return;

        if (!sampleBuffer) {
            RunLoop::main().dispatch([weakThis = WTFMove(weakThis), sampleBuffer = retainPtr(sampleBuffer)]() mutable {
                if (weakThis)
                    RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::frameAvailableHandler: NULL sample buffer!");
            });
            return;
        }

        auto attachments = (__bridge NSArray *)PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
        WKSCFrameStatus status = WKSCFrameStatusFrameStopped;
        [attachments enumerateObjectsUsingBlock:makeBlockPtr([&] (NSDictionary *attachment, NSUInteger, BOOL *stop) {
            auto statusNumber = (NSNumber *)attachment[PAL::SCStreamFrameInfoStatusKey];
            if (!statusNumber)
                return;

            status = (WKSCFrameStatus)[statusNumber integerValue];
            *stop = YES;
        }).get()];

        switch (status) {
        case WKSCFrameStatusFrameStarted:
        case WKSCFrameStatusFrameComplete:
            break;
        case WKSCFrameStatusFrameIdle:
        case WKSCFrameStatusFrameBlank:
        case WKSCFrameStatusFrameSuspended:
        case WKSCFrameStatusFrameStopped:
            return;
        }

        RunLoop::main().dispatch([weakThis = WTFMove(weakThis), sampleBuffer = retainPtr(sampleBuffer)]() mutable {
            if (!weakThis)
                return;

            weakThis->m_intrinsicSize = IntSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(PAL::CMSampleBufferGetFormatDescription(sampleBuffer.get()), true, true));
            weakThis->m_currentFrame = WTFMove(sampleBuffer);
        });
    });

    return m_frameAvailableHandler.get();
}

CaptureDevice::DeviceType ScreenCaptureKitCaptureSource::deviceType() const
{
    return m_captureDevice.type();
}

RealtimeMediaSourceSettings::DisplaySurfaceType ScreenCaptureKitCaptureSource::surfaceType() const
{
    return m_captureDevice.type() == CaptureDevice::DeviceType::Screen ? RealtimeMediaSourceSettings::DisplaySurfaceType::Monitor : RealtimeMediaSourceSettings::DisplaySurfaceType::Window;
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

void ScreenCaptureKitCaptureSource::screenCaptureDevices(Vector<CaptureDevice>& displays)
{
    if (!isAvailable())
        return;

    uint32_t displayCount = 0;
    auto err = CGGetActiveDisplayList(0, nullptr, &displayCount);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::screenCaptureDevices - CGGetActiveDisplayList() returned error %d when trying to get display count", (int)err);
        return;
    }

    if (!displayCount) {
        RELEASE_LOG_ERROR(WebRTC, "CGGetActiveDisplayList() returned a display count of 0");
        return;
    }

    Vector<CGDirectDisplayID> activeDisplays(displayCount);
    err = CGGetActiveDisplayList(displayCount, activeDisplays.data(), &displayCount);
    if (err) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::screenCaptureDevices - CGGetActiveDisplayList() returned error %d when trying to get the active display list", (int)err);
        return;
    }

    int count = 0;
    for (auto displayID : activeDisplays) {
        CaptureDevice displayDevice(String::number(displayID), CaptureDevice::DeviceType::Screen, makeString("Screen ", String::number(count++)));
        displayDevice.setEnabled(CGDisplayIDToOpenGLDisplayMask(displayID));
        displays.append(WTFMove(displayDevice));
    }
}

std::optional<CaptureDevice> ScreenCaptureKitCaptureSource::windowCaptureDeviceWithPersistentID(const String& windowIDString)
{
    auto windowID = parseInteger<uint32_t>(windowIDString);
    if (!windowID) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::windowCaptureDeviceWithPersistentID: invalid window ID");
        return std::nullopt;
    }

    std::optional<CaptureDevice> device;
    forEachNSWindow([&] (NSDictionary *, unsigned id, const String& windowTitle) mutable {
        if (id != windowID.value())
            return false;

        device = CaptureDevice(String::number(windowID.value()), CaptureDevice::DeviceType::Window, windowTitle, emptyString(), true);
        return true;
    });

    return device;
}

void ScreenCaptureKitCaptureSource::windowCaptureDevices(Vector<CaptureDevice>& windows)
{
    if (!isAvailable())
        return;

    forEachNSWindow([&] (NSDictionary *, unsigned windowID, const String& windowTitle) mutable {
        windows.append({ String::number(windowID), CaptureDevice::DeviceType::Window, windowTitle, emptyString(), true });
        return false;
    });
}

void ScreenCaptureKitCaptureSource::windowDevices(Vector<DisplayCaptureManager::WindowCaptureDevice>& devices)
{
    if (!isAvailable())
        return;

    forEachNSWindow([&] (NSDictionary *windowInfo, unsigned windowID, const String& windowTitle) mutable {
        auto *applicationName = (__bridge NSString *)(windowInfo[(__bridge NSString *)kCGWindowOwnerName]);
        devices.append({ { String::number(windowID), CaptureDevice::DeviceType::Window, windowTitle, emptyString(), true }, applicationName });
        return false;
    });
}

void forEachNSWindow(const Function<bool(NSDictionary *info, unsigned windowID, const String& title)>& predicate)
{
    RetainPtr<NSArray> windowList = adoptNS((__bridge NSArray *)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID));
    if (!windowList)
        return;

    [windowList enumerateObjectsUsingBlock:makeBlockPtr([&] (NSDictionary *windowInfo, NSUInteger, BOOL *stop) {
        *stop = NO;

        // Menus, the dock, etc have layers greater than 0, skip them.
        int windowLayer = [(NSNumber *)windowInfo[(__bridge NSString *)kCGWindowLayer] integerValue];
        if (windowLayer)
            return;

        // Skip windows that aren't on screen
        if (![(NSNumber *)windowInfo[(__bridge NSString *)kCGWindowIsOnscreen] integerValue])
            return;

        auto *windowTitle = (__bridge NSString *)(windowInfo[(__bridge NSString *)kCGWindowName]);
        auto windowID = (CGWindowID)[(NSNumber *)windowInfo[(__bridge NSString *)kCGWindowNumber] integerValue];
        if (predicate(windowInfo, windowID, windowTitle))
            *stop = YES;
    }).get()];
}
#pragma clang diagnostic pop

} // namespace WebCore

#endif // HAVE(SCREEN_CAPTURE_KIT)
