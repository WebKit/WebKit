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

#import "config.h"
#import "ScreenCaptureKitCaptureSource.h"

#if HAVE(SCREEN_CAPTURE_KIT)

#import "DisplayCaptureManager.h"
#import "Logging.h"
#import "PlatformMediaSessionManager.h"
#import "PlatformScreen.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeVideoUtilities.h"
#import "ScreenCaptureKitSharingSessionManager.h"
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/mac/ScreenCaptureKitSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/Scope.h>
#import <wtf/UUID.h>
#import <wtf/text/StringToIntegerConversion.h>
#include <wtf/threads/BinarySemaphore.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/mac/ScreenCaptureKitSoftLink.h>

@interface SCStreamConfiguration (SCStreamConfiguration_New)
@property (nonatomic, assign) CMTime minimumFrameInterval;
@end

@interface SCStream (SCStream_Deprecated)
- (instancetype)initWithFilter:(SCContentFilter *)contentFilter captureOutputProperties:(SCStreamConfiguration *)streamConfig delegate:(id<SCStreamDelegate>)delegate;
- (void)startCaptureWithFrameHandler:(void (^)(SCStream *stream, CMSampleBufferRef sampleBuffer))frameHandler completionHandler:(void (^)(NSError *error))completionHandler;
- (void)stopWithCompletionHandler:(void (^)(NSError * error))completionHandler;
- (void)updateStreamConfiguration:(SCStreamConfiguration *)streamConfig completionHandler:(void (^)(NSError *error))completionHandler;
@end

@interface SCStreamConfiguration (SCStreamConfiguration_Deprecated)
@property (nonatomic, assign) float minimumFrameTime;
@end

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

using namespace WebCore;
@interface WebCoreScreenCaptureKitHelper : NSObject<SCStreamDelegate,
#if HAVE(SC_CONTENT_SHARING_SESSION)
    SCContentSharingSessionProtocol,
#endif
    SCStreamOutput> {
    WeakPtr<ScreenCaptureKitCaptureSource> _callback;
}

- (instancetype)initWithCallback:(WeakPtr<ScreenCaptureKitCaptureSource>&&)callback;
- (void)disconnect;
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error;
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type;
#if HAVE(SC_CONTENT_SHARING_SESSION)
- (void)sessionDidEnd:(SCContentSharingSession *)session;
- (void)sessionDidChangeContent:(SCContentSharingSession *)session;
- (void)pickerCanceledForSession:(SCContentSharingSession *)session;
#endif
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

        _callback->streamFailedWithError(WTFMove(error), "-[SCStreamDelegate stream:didStopWithError:] called"_s);
    });
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type
{
    callOnMainRunLoop([strongSelf = RetainPtr { self }, sampleBuffer = RetainPtr { sampleBuffer }]() mutable {
        if (!strongSelf->_callback)
            return;

        strongSelf->_callback->streamDidOutputSampleBuffer(WTFMove(sampleBuffer), ScreenCaptureKitCaptureSource::SampleType::Video);
    });
}

#if HAVE(SC_CONTENT_SHARING_SESSION)
- (void)sessionDidEnd:(SCContentSharingSession *)session
{
    RunLoop::main().dispatch([self, strongSelf = RetainPtr { self }, session = RetainPtr { session }]() mutable {
        if (_callback)
            _callback->sessionDidEnd(session);
    });
}

- (void)sessionDidChangeContent:(SCContentSharingSession *)session
{
    RunLoop::main().dispatch([self, strongSelf = RetainPtr { self }, session = RetainPtr { session }]() mutable {
        if (_callback)
            _callback->sessionDidChangeContent(session);
    });
}

- (void)pickerCanceledForSession:(SCContentSharingSession *)session
{
}
#endif
@end

#pragma clang diagnostic pop

namespace WebCore {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static void forEachNSWindow(const Function<bool(NSDictionary *, unsigned, const String&)>&);

bool ScreenCaptureKitCaptureSource::isAvailable()
{
    return PAL::isScreenCaptureKitFrameworkAvailable();
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
    if (m_contentSharingSession) {
        [m_contentSharingSession end];
        m_contentSharingSession = nullptr;
    }
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
    if (!m_isRunning)
        return;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_isRunning = false;

    if (m_contentStream) {
        auto stopHandler = makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {

            callOnMainRunLoop([weakThis = WTFMove(weakThis), error = RetainPtr { error }]() mutable {
                if (!weakThis)
                    return;

                weakThis->m_contentStream = nil;
                if (error)
                    weakThis->streamFailedWithError(WTFMove(error), "-[SCStream stopCaptureWithCompletionHandler:] failed"_s);
            });
        });

        [m_contentStream stopCaptureWithCompletionHandler:stopHandler.get()];
    }
}

void ScreenCaptureKitCaptureSource::streamFailedWithError(RetainPtr<NSError>&& error, const String& message)
{
    ASSERT(isMainThread());

    ERROR_LOG_IF(loggerPtr() && error, LOGIDENTIFIER, message, " with error '", [[error localizedDescription] UTF8String], "'");
    ERROR_LOG_IF(loggerPtr() && !error, LOGIDENTIFIER, message);

    captureFailed();
}

#if HAVE(SC_CONTENT_SHARING_SESSION)
void ScreenCaptureKitCaptureSource::sessionDidChangeContent(RetainPtr<SCContentSharingSession> session)
{
    ASSERT(isMainThread());

    if ([session content].type == SCContentFilterTypeNothing)
        return;

    std::optional<CaptureDevice> device;
    SCContentFilter* content = [session content];
    switch (content.type) {
    case SCContentFilterTypeDesktopIndependentWindow: {
        auto *window = content.desktopIndependentWindowInfo.window;
        device = CaptureDevice(String::number(window.windowID), CaptureDevice::DeviceType::Window, window.title, emptyString(), true);
        m_content = content.desktopIndependentWindowInfo.window;
        break;
    }
    case SCContentFilterTypeDisplay:
        device = CaptureDevice(String::number(content.displayInfo.display.displayID), CaptureDevice::DeviceType::Screen, makeString("Screen"), emptyString(), true);
        m_content = content.displayInfo.display;
        break;
    case SCContentFilterTypeNothing:
    case SCContentFilterTypeAppsAndWindowsPinnedToDisplay:
    case SCContentFilterTypeClientShouldImplementDefault:
        ASSERT_NOT_REACHED();
        return;
    }
    if (!device) {
        streamFailedWithError(nil, "Failed to find CaptureDevice after content changed"_s);
        return;
    }

    m_captureDevice = device.value();
    m_intrinsicSize = { };
    configurationChanged();
}

void ScreenCaptureKitCaptureSource::sessionDidEnd(RetainPtr<SCContentSharingSession>)
{
    if (m_contentSharingSession) {
        [m_contentSharingSession end];
        m_contentSharingSession = nullptr;
    }

    streamFailedWithError(nil, "sessionDidEnd"_s);
}
#endif

DisplayCaptureSourceCocoa::DisplayFrameType ScreenCaptureKitCaptureSource::generateFrame()
{
    return m_currentFrame;
}

static void findSharableDevice(RetainPtr<SCShareableContent>&& shareableContent, CaptureDevice::DeviceType deviceType, uint32_t deviceID, CompletionHandler<void(std::optional<ScreenCaptureKitCaptureSource::Content>, uint32_t)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    uint32_t index = 0;
    std::optional<ScreenCaptureKitCaptureSource::Content> content;
    if (deviceType == CaptureDevice::DeviceType::Screen) {
        [[shareableContent displays] enumerateObjectsUsingBlock:makeBlockPtr([&] (SCDisplay *display, NSUInteger, BOOL *stop) {
            if (display.displayID == deviceID) {
                content = display;
                *stop = YES;
            }
            ++index;
        }).get()];
    } else if (deviceType == CaptureDevice::DeviceType::Window) {
        [[shareableContent windows] enumerateObjectsUsingBlock:makeBlockPtr([&] (SCWindow *window, NSUInteger, BOOL *stop) {
            if (window.windowID == deviceID) {
                content = window;
                *stop = YES;
            }
            ++index;
        }).get()];
    } else {
        ASSERT_NOT_REACHED();
    }

    completionHandler(WTFMove(content), index);
}

void ScreenCaptureKitCaptureSource::findShareableContent()
{
    ASSERT(!m_content);

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    [PAL::getSCShareableContentClass() getShareableContentWithCompletionHandler:makeBlockPtr([this, weakThis = WeakPtr { *this }, identifier = LOGIDENTIFIER] (SCShareableContent *shareableContent, NSError *error) mutable {
        callOnMainRunLoop([this, weakThis = WTFMove(weakThis), shareableContent = RetainPtr { shareableContent }, error = RetainPtr { error }, identifier]() mutable {
            if (!weakThis)
                return;

            if (error) {
                streamFailedWithError(WTFMove(error), "-[SCStream getShareableContentWithCompletionHandler:] failed"_s);
                return;
            }

            ALWAYS_LOG_IF_POSSIBLE(identifier, "have content list");

            findSharableDevice(WTFMove(shareableContent), m_captureDevice.type(), m_deviceID, [this, weakThis = WTFMove(weakThis)] (std::optional<Content> content, uint32_t) mutable {
                if (!content) {
                    streamFailedWithError(nil, "capture device not found"_s);
                    return;
                }
                m_content = WTFMove(content);
                startContentStream();
            });
        });
    }).get()];
}

RetainPtr<SCStreamConfiguration> ScreenCaptureKitCaptureSource::streamConfiguration()
{
    if (m_streamConfiguration)
        return m_streamConfiguration;

    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);

    m_streamConfiguration = adoptNS([PAL::allocSCStreamConfigurationInstance() init]);
    [m_streamConfiguration setPixelFormat:preferedPixelBufferFormat()];
    [m_streamConfiguration setShowsCursor:YES];
    [m_streamConfiguration setQueueDepth:6];
    [m_streamConfiguration setColorSpaceName:kCGColorSpaceSRGB];
    [m_streamConfiguration setColorMatrix:kCGDisplayStreamYCbCrMatrix_SMPTE_240M_1995];

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

    if (m_contentStream)
        return;

    if (!m_content) {
        findShareableContent();
        return;
    }

    if (!m_contentFilter) {
        m_contentFilter = switchOn(m_content.value(),
            [] (const RetainPtr<SCDisplay> display) -> RetainPtr<SCContentFilter> {
                return adoptNS([PAL::allocSCContentFilterInstance() initWithDisplay:display.get() excludingWindows:@[]]);
            },
            [] (const RetainPtr<SCWindow> window)  -> RetainPtr<SCContentFilter> {
                return adoptNS([PAL::allocSCContentFilterInstance() initWithDesktopIndependentWindow:window.get()]);
            }
        );

        if (!m_contentFilter) {
            streamFailedWithError(nil, "Failed to allocate SCContentFilter"_s);
            return;
        }
    }

    if (!m_captureHelper)
        m_captureHelper = ([[WebCoreScreenCaptureKitHelper alloc] initWithCallback:this]);

    m_contentStream = adoptNS([PAL::allocSCStreamInstance() initWithFilter:m_contentFilter.get() configuration:streamConfiguration().get() delegate:m_captureHelper.get()]);

#if HAVE(SC_CONTENT_SHARING_SESSION)
    if (ScreenCaptureKitSharingSessionManager::isAvailable()) {
        if (!m_contentSharingSession) {
            m_contentSharingSession = ScreenCaptureKitSharingSessionManager::singleton().takeSharingSessionForFilter(m_contentFilter.get());
            if (!m_contentSharingSession) {
                streamFailedWithError(nil, "Failed to get SharingSession"_s);
                return;
            }
        }

        m_contentStream = adoptNS([PAL::allocSCStreamInstance() initWithSharingSession:m_contentSharingSession.get() captureOutputProperties:streamConfiguration().get() delegate:m_captureHelper.get()]);
    } else
#endif
        m_contentStream = adoptNS([PAL::allocSCStreamInstance() initWithFilter:m_contentFilter.get() captureOutputProperties:streamConfiguration().get() delegate:m_captureHelper.get()]);


    if (!m_contentStream) {
        streamFailedWithError(nil, "Failed to allocate SCStream"_s);
        return;
    }

    NSError *error;
    if (![m_contentStream addStreamOutput:m_captureHelper.get() type:SCStreamOutputTypeScreen sampleHandlerQueue:captureQueue() error:&error]) {
        streamFailedWithError(WTFMove(error), "-[SCStream addStreamOutput:type:sampleHandlerQueue:error:] failed"_s);
        return;
    }

    auto completionHandler = makeBlockPtr([this, weakThis = WeakPtr { *this }, identifier = LOGIDENTIFIER] (NSError *error) mutable {
        callOnMainRunLoop([this, weakThis = WTFMove(weakThis), error = RetainPtr { error }, identifier]() mutable {
            if (!weakThis)
                return;

            if (error)
                streamFailedWithError(WTFMove(error), "-[SCStream startCaptureWithFrameHandler:completionHandler:] failed"_s);
            else
                ALWAYS_LOG_IF_POSSIBLE(identifier, "stream started");
        });
    });

    [m_contentStream startCaptureWithCompletionHandler:completionHandler.get()];

    m_isRunning = true;
}

IntSize ScreenCaptureKitCaptureSource::intrinsicSize() const
{
    if (m_intrinsicSize)
        return m_intrinsicSize.value();

    if (m_content) {
        auto frame = switchOn(m_content.value(),
            [] (const RetainPtr<SCDisplay> display) -> CGRect {
                return [display frame];
            },
            [] (const RetainPtr<SCWindow> window) -> CGRect {
                return [window frame];
            }
        );

        return { static_cast<int>(frame.size.width), static_cast<int>(frame.size.height) };
    }
#if HAVE(SC_CONTENT_SHARING_SESSION)
    IntSize size;
    BinarySemaphore semaphore;
    [PAL::getSCShareableContentClass() getShareableContentWithCompletionHandler:makeBlockPtr([&size, &semaphore, deviceID = m_deviceID, deviceType = m_captureDevice.type()] (SCShareableContent *shareableContent, NSError *error) mutable {
        auto scope = makeScopeExit([&semaphore] {
            semaphore.signal();
        });
        if (error) {
            RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::intrinsicSize getShareableContentWithCompletionHandler failed with error %s", [[error localizedDescription] UTF8String]);
            return;
        }
        if (deviceType == CaptureDevice::DeviceType::Screen) {
            [[shareableContent displays] enumerateObjectsUsingBlock:makeBlockPtr([&] (SCDisplay *display, NSUInteger, BOOL *stop) {
                if (display.displayID == deviceID) {
                    size = { static_cast<int>(display.width), static_cast<int>(display.height) };
                    *stop = YES;
                }
            }).get()];
            return;
        }
        if (deviceType == CaptureDevice::DeviceType::Window) {
            [[shareableContent windows] enumerateObjectsUsingBlock:makeBlockPtr([&] (SCWindow *window, NSUInteger, BOOL *stop) {
                if (window.windowID == deviceID) {
                    size = { static_cast<int>(window.frame.size.width), static_cast<int>(window.frame.size.height) };
                    *stop = YES;
                }
            }).get()];
        }
    }).get()];
    semaphore.wait();
    return size;
#else
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
#endif
}

void ScreenCaptureKitCaptureSource::updateStreamConfiguration()
{
    ASSERT(m_contentStream);

    auto completionHandler = makeBlockPtr([weakThis = WeakPtr { *this }] (NSError *error) mutable {
        if (!error)
            return;

        callOnMainRunLoop([weakThis = WTFMove(weakThis), error = RetainPtr { error }]() mutable {
            if (weakThis)
                weakThis->streamFailedWithError(WTFMove(error), "-[SCStream updateStreamConfiguration:] failed"_s);
        });
    });

    [m_contentStream updateConfiguration:streamConfiguration().get() completionHandler:completionHandler.get()];
}

void ScreenCaptureKitCaptureSource::commitConfiguration(const RealtimeMediaSourceSettings& settings)
{
    if (m_width == settings.width() && m_height == settings.height() && m_frameRate == settings.frameRate())
        return;

    m_width = settings.width();
    m_height = settings.height();
    m_frameRate = settings.frameRate();

    if (!m_contentStream)
        return;

    m_streamConfiguration = nullptr;
    updateStreamConfiguration();
}

ScreenCaptureKitCaptureSource::SCContentStreamUpdateCallback ScreenCaptureKitCaptureSource::frameAvailableHandler()
{
    if (m_frameAvailableHandler)
        return m_frameAvailableHandler.get();

    m_frameAvailableHandler = makeBlockPtr([this, weakThis = WeakPtr { *this }] (SCStream *, CMSampleBufferRef sampleBuffer) mutable {
        RunLoop::main().dispatch([this, weakThis, sampleBuffer = RetainPtr { sampleBuffer }]() mutable {
            if (weakThis)
                streamDidOutputSampleBuffer(WTFMove(sampleBuffer), SampleType::Video);
        });
    });

    return m_frameAvailableHandler.get();
}

void ScreenCaptureKitCaptureSource::streamDidOutputSampleBuffer(RetainPtr<CMSampleBufferRef> sampleBuffer, SampleType)
{
    ASSERT(isMainThread());

    if (!sampleBuffer) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::streamDidOutputSampleBuffer: NULL sample buffer!");
        return;
    }

    static NSString* frameInfoKey;
    if (!frameInfoKey) {
        if (PAL::canLoad_ScreenCaptureKit_SCStreamFrameInfoStatus())
            frameInfoKey = PAL::get_ScreenCaptureKit_SCStreamFrameInfoStatus();
        ASSERT(frameInfoKey);
        if (!frameInfoKey)
            RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitCaptureSource::streamDidOutputSampleBuffer: unable to load status key!");
    }
    if (!frameInfoKey)
        return;

    auto attachments = (__bridge NSArray *)PAL::CMSampleBufferGetSampleAttachmentsArray(sampleBuffer.get(), false);
    SCFrameStatus status = SCFrameStatusStopped;
    [attachments enumerateObjectsUsingBlock:makeBlockPtr([&] (NSDictionary *attachment, NSUInteger, BOOL *stop) {
        auto statusNumber = (NSNumber *)attachment[frameInfoKey];
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

    m_intrinsicSize = IntSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(PAL::CMSampleBufferGetFormatDescription(sampleBuffer.get()), true, true));
    m_currentFrame = WTFMove(sampleBuffer);
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

RealtimeMediaSourceSettings::DisplaySurfaceType ScreenCaptureKitCaptureSource::surfaceType() const
{
    return m_captureDevice.type() == CaptureDevice::DeviceType::Screen ? RealtimeMediaSourceSettings::DisplaySurfaceType::Monitor : RealtimeMediaSourceSettings::DisplaySurfaceType::Window;
}

void ScreenCaptureKitCaptureSource::captureDeviceWithPersistentID(CaptureDevice::DeviceType deviceType, uint32_t deviceID, CompletionHandler<void(std::optional<CaptureDevice>)>&& completionHandler)
{
    [PAL::getSCShareableContentClass() getShareableContentWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler), deviceID, deviceType] (SCShareableContent *shareableContent, NSError *error) mutable {
        callOnMainRunLoop([shareableContent = RetainPtr { shareableContent }, error = RetainPtr { error }, completionHandler = WTFMove(completionHandler), deviceID, deviceType]() mutable {

            if (error) {
                RELEASE_LOG_ERROR(WebRTC, "getShareableContentWithCompletionHandler failed with error %s", [[error localizedDescription] UTF8String]);
                completionHandler(std::nullopt);
                return;
            }

            findSharableDevice(WTFMove(shareableContent), deviceType, deviceID, [completionHandler = WTFMove(completionHandler)] (std::optional<Content> content, uint32_t index) mutable {
                if (!content) {
                    RELEASE_LOG_ERROR(WebRTC, "capture device not found");
                    return;
                }

                auto device = switchOn(content.value(),
                    [index] (const RetainPtr<SCDisplay> display) -> std::optional<CaptureDevice> {
                        return CaptureDevice(String::number([display displayID]), CaptureDevice::DeviceType::Screen, makeString("Screen ", index), emptyString(), true);
                    },
                    [] (const RetainPtr<SCWindow> window)  -> std::optional<CaptureDevice> {
                        return CaptureDevice(String::number([window windowID]), CaptureDevice::DeviceType::Window, [window title], emptyString(), true);
                    }
                );

                completionHandler(WTFMove(device));
            });
        });
    }).get()];
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

        // Skip windows that aren't on screen.
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
