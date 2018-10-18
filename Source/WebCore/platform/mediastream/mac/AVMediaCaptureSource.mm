/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#import "AVMediaCaptureSource.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVCaptureDeviceManager.h"
#import "AudioSourceProvider.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "RealtimeMediaSourceSettings.h"
#import <AVFoundation/AVCaptureDevice.h>
#import <AVFoundation/AVCaptureInput.h>
#import <AVFoundation/AVCaptureOutput.h>
#import <AVFoundation/AVCaptureSession.h>
#import <AVFoundation/AVError.h>
#import <objc/runtime.h>
#import <wtf/MainThread.h>

#import <pal/cf/CoreMediaSoftLink.h>

typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceTypedef;
typedef AVCaptureDeviceInput AVCaptureDeviceInputType;
typedef AVCaptureOutput AVCaptureOutputType;
typedef AVCaptureSession AVCaptureSessionType;
typedef AVCaptureAudioDataOutput AVCaptureAudioDataOutputType;
typedef AVCaptureVideoDataOutput AVCaptureVideoDataOutputType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVCaptureAudioDataOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureConnection)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceInput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureSession)
SOFT_LINK_CLASS(AVFoundation, AVCaptureVideoDataOutput)

#define AVCaptureAudioDataOutput getAVCaptureAudioDataOutputClass()
#define AVCaptureConnection getAVCaptureConnectionClass()
#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureDeviceInput getAVCaptureDeviceInputClass()
#define AVCaptureOutput getAVCaptureOutputClass()
#define AVCaptureSession getAVCaptureSessionClass()
#define AVCaptureVideoDataOutput getAVCaptureVideoDataOutputClass()

SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeMuxed, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVMediaTypeVideo, NSString *)

#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVMediaTypeMuxed getAVMediaTypeMuxed()
#define AVMediaTypeVideo getAVMediaTypeVideo()

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionRuntimeErrorNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionWasInterruptedNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionInterruptionEndedNotification, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionInterruptionReasonKey, NSString *)
SOFT_LINK_POINTER_OPTIONAL(AVFoundation, AVCaptureSessionErrorKey, NSString *)

#define AVCaptureSessionRuntimeErrorNotification getAVCaptureSessionRuntimeErrorNotification()
#define AVCaptureSessionWasInterruptedNotification getAVCaptureSessionWasInterruptedNotification()
#define AVCaptureSessionInterruptionEndedNotification getAVCaptureSessionInterruptionEndedNotification()
#define AVCaptureSessionInterruptionReasonKey getAVCaptureSessionInterruptionReasonKey()
#define AVCaptureSessionErrorKey getAVCaptureSessionErrorKey()
#endif

using namespace WebCore;

@interface WebCoreAVMediaCaptureSourceObserver : NSObject<AVCaptureAudioDataOutputSampleBufferDelegate, AVCaptureVideoDataOutputSampleBufferDelegate>
{
    AVMediaCaptureSource* m_callback;
}

-(id)initWithCallback:(AVMediaCaptureSource*)callback;
-(void)disconnect;
-(void)addNotificationObservers;
-(void)removeNotificationObservers;
-(void)captureOutput:(AVCaptureOutputType*)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType*)connection;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context;
#if PLATFORM(IOS_FAMILY)
-(void)sessionRuntimeError:(NSNotification*)notification;
-(void)beginSessionInterrupted:(NSNotification*)notification;
-(void)endSessionInterrupted:(NSNotification*)notification;
#endif
@end

namespace WebCore {

static NSArray<NSString*>* sessionKVOProperties();

static dispatch_queue_t globaAudioCaptureSerialQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create("WebCoreAVMediaCaptureSource audio capture queue", DISPATCH_QUEUE_SERIAL);
    });
    return globalQueue;
}

static dispatch_queue_t globaVideoCaptureSerialQueue()
{
    static dispatch_queue_t globalQueue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalQueue = dispatch_queue_create_with_target("WebCoreAVMediaCaptureSource video capture queue", DISPATCH_QUEUE_SERIAL, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
    });
    return globalQueue;
}

AVMediaCaptureSource::AVMediaCaptureSource(AVCaptureDeviceTypedef* device, const AtomicString& id, RealtimeMediaSource::Type type)
    : RealtimeMediaSource(id, type, device.localizedName)
    , m_objcObserver(adoptNS([[WebCoreAVMediaCaptureSourceObserver alloc] initWithCallback:this]))
    , m_device(device)
{
#if PLATFORM(IOS_FAMILY)
    static_assert(static_cast<int>(InterruptionReason::VideoNotAllowedInBackground) == static_cast<int>(AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground), "InterruptionReason::VideoNotAllowedInBackground is not AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground as expected");
    static_assert(static_cast<int>(InterruptionReason::VideoNotAllowedInSideBySide) == AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps, "InterruptionReason::VideoNotAllowedInSideBySide is not AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps as expected");
    static_assert(static_cast<int>(InterruptionReason::VideoInUse) == AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient, "InterruptionReason::VideoInUse is not AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient as expected");
    static_assert(static_cast<int>(InterruptionReason::AudioInUse) == AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient, "InterruptionReason::AudioInUse is not AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient as expected");
#endif
    
    setPersistentID(String(device.uniqueID));
}

AVMediaCaptureSource::~AVMediaCaptureSource()
{
    [m_objcObserver disconnect];

    if (!m_session)
        return;

    for (NSString *keyName in sessionKVOProperties())
        [m_session removeObserver:m_objcObserver.get() forKeyPath:keyName];

    if ([m_session isRunning])
        [m_session stopRunning];

}

void AVMediaCaptureSource::startProducingData()
{
    if (!m_session) {
        if (!setupSession())
            return;
    }

    if ([m_session isRunning])
        return;

    [m_objcObserver addNotificationObservers];
    [m_session startRunning];
}

void AVMediaCaptureSource::stopProducingData()
{
    if (!m_session)
        return;

    [m_objcObserver removeNotificationObservers];

    if ([m_session isRunning])
        [m_session stopRunning];

    m_interruption = InterruptionReason::None;
#if PLATFORM(IOS_FAMILY)
    m_session = nullptr;
#endif
}

void AVMediaCaptureSource::beginConfiguration()
{
    if (m_session)
        [m_session beginConfiguration];
}

void AVMediaCaptureSource::commitConfiguration()
{
    if (m_session)
        [m_session commitConfiguration];
}

void AVMediaCaptureSource::initializeSettings()
{
    if (m_currentSettings.deviceId().isEmpty())
        m_currentSettings.setSupportedConstraints(supportedConstraints());

    m_currentSettings.setDeviceId(id());
    updateSettings(m_currentSettings);
}

const RealtimeMediaSourceSettings& AVMediaCaptureSource::settings() const
{
    const_cast<AVMediaCaptureSource&>(*this).initializeSettings();
    return m_currentSettings;
}

RealtimeMediaSourceSupportedConstraints& AVMediaCaptureSource::supportedConstraints()
{
    if (m_supportedConstraints.supportsDeviceId())
        return m_supportedConstraints;

    m_supportedConstraints.setSupportsDeviceId(true);
    initializeSupportedConstraints(m_supportedConstraints);

    return m_supportedConstraints;
}

void AVMediaCaptureSource::initializeCapabilities()
{
    m_capabilities = std::make_unique<RealtimeMediaSourceCapabilities>(supportedConstraints());
    m_capabilities->setDeviceId(id());

    initializeCapabilities(*m_capabilities.get());
}

const RealtimeMediaSourceCapabilities& AVMediaCaptureSource::capabilities() const
{
    if (!m_capabilities)
        const_cast<AVMediaCaptureSource&>(*this).initializeCapabilities();
    return *m_capabilities;
}

bool AVMediaCaptureSource::setupSession()
{
    if (m_session)
        return true;

    m_session = adoptNS([allocAVCaptureSessionInstance() init]);
    for (NSString* keyName in sessionKVOProperties())
        [m_session addObserver:m_objcObserver.get() forKeyPath:keyName options:NSKeyValueObservingOptionNew context:(void *)nil];

    [m_session beginConfiguration];
    bool success = setupCaptureSession();
    [m_session commitConfiguration];

    if (!success)
        captureFailed();

    return success;
}

void AVMediaCaptureSource::captureSessionIsRunningDidChange(bool state)
{
    scheduleDeferredTask([this, state] {
        if ((state == m_isRunning) && (state == !muted()))
            return;

        m_isRunning = state;
        notifyMutedChange(!m_isRunning);
    });
}

#if PLATFORM(IOS_FAMILY)
void AVMediaCaptureSource::captureSessionRuntimeError(RetainPtr<NSError> error)
{
    if (!m_isRunning || error.get().code != AVErrorMediaServicesWereReset)
        return;

    // Try to restart the session, but reset m_isRunning immediately so if it fails we won't try again.
    [m_session startRunning];
    m_isRunning = [m_session isRunning];
}

void AVMediaCaptureSource::captureSessionBeginInterruption(RetainPtr<NSNotification> notification)
{
    m_interruption = static_cast<AVMediaCaptureSource::InterruptionReason>([notification.get().userInfo[AVCaptureSessionInterruptionReasonKey] integerValue]);
}

void AVMediaCaptureSource::captureSessionEndInterruption(RetainPtr<NSNotification>)
{
    InterruptionReason reason = m_interruption;

    m_interruption = InterruptionReason::None;
    if (reason != InterruptionReason::VideoNotAllowedInSideBySide || m_isRunning || !m_session)
        return;

    [m_session startRunning];
    m_isRunning = [m_session isRunning];
}
#endif

void AVMediaCaptureSource::setVideoSampleBufferDelegate(AVCaptureVideoDataOutputType* videoOutput)
{
    [videoOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaVideoCaptureSerialQueue()];
}

void AVMediaCaptureSource::setAudioSampleBufferDelegate(AVCaptureAudioDataOutputType* audioOutput)
{
    [audioOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaAudioCaptureSerialQueue()];
}

bool AVMediaCaptureSource::interrupted() const
{
    if (m_interruption != InterruptionReason::None)
        return true;

    return RealtimeMediaSource::interrupted();
}

NSArray<NSString*>* sessionKVOProperties()
{
    static NSArray* keys = [@[@"running"] retain];
    return keys;
}

} // namespace WebCore

@implementation WebCoreAVMediaCaptureSourceObserver

- (id)initWithCallback:(AVMediaCaptureSource*)callback
{
    self = [super init];
    if (!self)
        return nil;

    m_callback = callback;

    return self;
}

- (void)disconnect
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self removeNotificationObservers];
    m_callback = nullptr;
}

- (void)addNotificationObservers
{
#if PLATFORM(IOS_FAMILY)
    ASSERT(m_callback);

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    AVCaptureSessionType* session = m_callback->session();

    [center addObserver:self selector:@selector(sessionRuntimeError:) name:AVCaptureSessionRuntimeErrorNotification object:session];
    [center addObserver:self selector:@selector(beginSessionInterrupted:) name:AVCaptureSessionWasInterruptedNotification object:session];
    [center addObserver:self selector:@selector(endSessionInterrupted:) name:AVCaptureSessionInterruptionEndedNotification object:session];
#endif
}

- (void)removeNotificationObservers
{
#if PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] removeObserver:self];
#endif
}

- (void)captureOutput:(AVCaptureOutputType*)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType*)connection
{
    if (!m_callback)
        return;

    m_callback->captureOutputDidOutputSampleBufferFromConnection(captureOutput, sampleBuffer, connection);
}

- (void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary*)change context:(void*)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(context);

    if (!m_callback)
        return;

    id newValue = [change valueForKey:NSKeyValueChangeNewKey];

#if !LOG_DISABLED
    bool willChange = [[change valueForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue];

    if (willChange)
        LOG(Media, "WebCoreAVMediaCaptureSourceObserver::observeValueForKeyPath(%p) - will change, keyPath = %s", self, [keyPath UTF8String]);
    else {
        RetainPtr<NSString> valueString = adoptNS([[NSString alloc] initWithFormat:@"%@", newValue]);
        LOG(Media, "WebCoreAVMediaCaptureSourceObserver::observeValueForKeyPath(%p) - did change, keyPath = %s, value = %s", self, [keyPath UTF8String], [valueString.get() UTF8String]);
    }
#endif

    if ([keyPath isEqualToString:@"running"])
        m_callback->captureSessionIsRunningDidChange([newValue boolValue]);
}

#if PLATFORM(IOS_FAMILY)
- (void)sessionRuntimeError:(NSNotification*)notification
{
    NSError *error = notification.userInfo[AVCaptureSessionErrorKey];
    LOG(Media, "WebCoreAVMediaCaptureSourceObserver::sessionRuntimeError(%p) - error = %s", self, [[error localizedDescription] UTF8String]);

    if (m_callback)
        m_callback->captureSessionRuntimeError(error);
}

-(void)beginSessionInterrupted:(NSNotification*)notification
{
    LOG(Media, "WebCoreAVMediaCaptureSourceObserver::beginSessionInterrupted(%p) - reason = %d", self, [notification.userInfo[AVCaptureSessionInterruptionReasonKey] integerValue]);

    if (m_callback)
        m_callback->captureSessionBeginInterruption(notification);
}

- (void)endSessionInterrupted:(NSNotification*)notification
{
    LOG(Media, "WebCoreAVMediaCaptureSourceObserver::endSessionInterrupted(%p)", self);

    if (m_callback)
        m_callback->captureSessionEndInterruption(notification);
}
#endif

@end

#endif // ENABLE(MEDIA_STREAM)
