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
#include "ReplayKitCaptureSource.h"

#if ENABLE(MEDIA_STREAM) && HAVE(REPLAYKIT)

#import "Logging.h"
#import "RealtimeVideoUtilities.h"
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/UUID.h>
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/ios/ReplayKitSoftLink.h>

using namespace WebCore;
@interface WebCoreReplayKitScreenRecorderHelper : NSObject {
    WeakPtr<ReplayKitCaptureSource> _capturer;
}

- (instancetype)initWithCallback:(WeakPtr<ReplayKitCaptureSource>&&)capturer;
- (void)disconnect;
- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context;
@end

@implementation WebCoreReplayKitScreenRecorderHelper
- (instancetype)initWithCallback:(WeakPtr<ReplayKitCaptureSource>&&)capturer
{
    self = [super init];
    if (!self)
        return self;

    _capturer = WTFMove(capturer);
    [[PAL::getRPScreenRecorderClass() sharedRecorder] addObserver:self forKeyPath:@"recording" options:NSKeyValueObservingOptionNew context:(void *)nil];

    return self;
}

- (void)disconnect
{
    _capturer = nullptr;
    [[PAL::getRPScreenRecorderClass() sharedRecorder] removeObserver:self forKeyPath:@"recording"];
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(context);

    RefPtr protectedCapturer = _capturer.get();
    if (!protectedCapturer)
        return;

    id newValue = [change valueForKey:NSKeyValueChangeNewKey];
    bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];

#if !RELEASE_LOG_DISABLED
    if (protectedCapturer->loggerPtr()) {
        auto identifier = Logger::LogSiteIdentifier("ReplayKitCaptureSource"_s, "observeValueForKeyPath"_s, protectedCapturer->logIdentifier());
        RetainPtr<NSString> valueString = adoptNS([[NSString alloc] initWithFormat:@"%@", newValue]);
        protectedCapturer->logger().logAlways(protectedCapturer->logChannel(), identifier, willChange ? "will" : "did", " change '", [keyPath UTF8String], "' to ", [valueString.get() UTF8String]);
    }
#endif

    if (willChange)
        return;

    if ([keyPath isEqualToString:@"recording"])
        protectedCapturer->captureStateDidChange();
}
@end

namespace WebCore {

bool ReplayKitCaptureSource::isAvailable()
{
    return [PAL::getRPScreenRecorderClass() sharedRecorder].isAvailable;
}

ReplayKitCaptureSource::ReplayKitCaptureSource(CapturerObserver& observer)
    : DisplayCaptureSourceCocoa::Capturer(observer)
    , m_captureWatchdogTimer(*this, &ReplayKitCaptureSource::verifyCaptureIsActive)
{
}

ReplayKitCaptureSource::~ReplayKitCaptureSource()
{
    [m_recorderHelper disconnect];
    stop();
    m_currentFrame = nullptr;
}

bool ReplayKitCaptureSource::start()
{
    ASSERT(isAvailable());

    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG_IF(loggerPtr(), identifier);

    auto *screenRecorder = [PAL::getRPScreenRecorderClass() sharedRecorder];
    if (screenRecorder.recording)
        return true;

#if !PLATFORM(APPLETV)
    // FIXME: Add support for concurrent audio capture.
    [screenRecorder setMicrophoneEnabled:NO];
#endif

    if (!m_recorderHelper)
        m_recorderHelper = ([[WebCoreReplayKitScreenRecorderHelper alloc] initWithCallback:this]);

    auto captureHandler = makeBlockPtr([this, weakThis = WeakPtr { *this }, identifier](CMSampleBufferRef _Nonnull sampleBuffer, RPSampleBufferType bufferType, NSError * _Nullable error) {

        if (bufferType != RPSampleBufferTypeVideo)
            return;

        ERROR_LOG_IF(error && loggerPtr(), identifier, "startCaptureWithHandler failed ", error);

        ++m_frameCount;

        RunLoop::main().dispatch([weakThis, sampleBuffer = retainPtr(sampleBuffer)]() mutable {
            if (!weakThis)
                return;

            weakThis->screenRecorderDidOutputVideoSample(WTFMove(sampleBuffer));
        });
    });

    auto completionHandler = makeBlockPtr([this, weakThis = WeakPtr { *this }, identifier](NSError * _Nullable error) {
        // FIXME: It should be safe to call `videoFrameAvailable` from any thread. Test this and get rid of this main thread hop.
        RunLoop::main().dispatch([this, weakThis, error = retainPtr(error), identifier]() mutable {
            if (!weakThis || !error)
                return;

            ERROR_LOG_IF(loggerPtr(), identifier, "completionHandler failed ", error.get());
            weakThis->stop();
        });
    });

    [screenRecorder startCaptureWithHandler:captureHandler.get() completionHandler:completionHandler.get()];

    return true;
}

void ReplayKitCaptureSource::screenRecorderDidOutputVideoSample(RetainPtr<CMSampleBufferRef>&& sampleBuffer)
{
    m_currentFrame = sampleBuffer.get();
    m_intrinsicSize = IntSize(PAL::CMVideoFormatDescriptionGetPresentationDimensions(PAL::CMSampleBufferGetFormatDescription(m_currentFrame.get()), true, true));
}

void ReplayKitCaptureSource::captureStateDidChange()
{
    RunLoop::main().dispatch([this, weakThis = WeakPtr { *this }, identifier = LOGIDENTIFIER]() mutable {
        if (!weakThis)
            return;

        bool isRecording = !![[PAL::getRPScreenRecorderClass() sharedRecorder] isRecording];
        if (m_isRunning == (isRecording && !m_interrupted))
            return;

        m_isRunning = isRecording && !m_interrupted;
        ALWAYS_LOG_IF(loggerPtr(), identifier, m_isRunning);
        isRunningChanged(m_isRunning);
    });
}

void ReplayKitCaptureSource::stop()
{
    auto identifier = LOGIDENTIFIER;
    ALWAYS_LOG_IF(loggerPtr(), identifier);
    m_captureWatchdogTimer.stop();
    m_interrupted = false;
    m_isRunning = false;

    auto *screenRecorder = [PAL::getRPScreenRecorderClass() sharedRecorder];
    if (screenRecorder.recording) {
        [screenRecorder stopCaptureWithHandler:^(NSError * _Nullable error) {
            ERROR_LOG_IF(error && loggerPtr(), identifier, "startCaptureWithHandler failed ", error);
        }];
    }
}

DisplayCaptureSourceCocoa::DisplayFrameType ReplayKitCaptureSource::generateFrame()
{
    return m_currentFrame;
}

void ReplayKitCaptureSource::verifyCaptureIsActive()
{
    ASSERT(m_isRunning || m_interrupted);
    auto identifier = LOGIDENTIFIER;
    if (m_lastFrameCount != m_frameCount) {
        m_lastFrameCount = m_frameCount;
        if (m_interrupted) {
            ALWAYS_LOG_IF(loggerPtr(), identifier, "frame received after interruption, unmuting");
            m_interrupted = false;
            captureStateDidChange();
        }
        return;
    }

    ALWAYS_LOG_IF(loggerPtr(), identifier, "no frame received in ", static_cast<int>(m_captureWatchdogTimer.repeatInterval().value()), " seconds, muting");
    m_interrupted = true;
    captureStateDidChange();
}

void ReplayKitCaptureSource::startCaptureWatchdogTimer()
{
    static constexpr Seconds verifyCaptureInterval = 2_s;
    if (m_captureWatchdogTimer.isActive())
        return;

    m_captureWatchdogTimer.startRepeating(verifyCaptureInterval);
    m_lastFrameCount = m_frameCount;
}

static String screenDeviceUUID()
{
    static NeverDestroyed<String> screenID = createVersion4UUIDString();
    return screenID;
}

static CaptureDevice& screenDevice()
{
    static NeverDestroyed<CaptureDevice> device = { screenDeviceUUID(), CaptureDevice::DeviceType::Screen, "Screen 1"_str, emptyString(), true };
    return device;
}

std::optional<CaptureDevice> ReplayKitCaptureSource::screenCaptureDeviceWithPersistentID(const String& displayID)
{
    if (!isAvailable()) {
        RELEASE_LOG_ERROR(WebRTC, "ReplayKitCaptureSource::screenCaptureDeviceWithPersistentID: screen capture unavailable");
        return std::nullopt;
    }

    if (displayID != screenDeviceUUID()) {
        RELEASE_LOG_ERROR(WebRTC, "ReplayKitCaptureSource::screenCaptureDeviceWithPersistentID: invalid display ID");
        return std::nullopt;
    }

    return screenDevice();
}

void ReplayKitCaptureSource::screenCaptureDevices(Vector<CaptureDevice>& displays)
{
    if (isAvailable())
        displays.append(screenDevice());
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && HAVE(REPLAYKIT)
