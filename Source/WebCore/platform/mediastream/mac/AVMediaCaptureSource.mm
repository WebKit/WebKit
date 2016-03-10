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
#import "CoreMediaSoftLink.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "RealtimeMediaSourceSettings.h"
#import "SoftLinking.h"
#import "UUID.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceType;
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

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeMuxed, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVMediaTypeVideo, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset1280x720, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset640x480, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPreset352x288, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionPresetLow, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVCaptureSessionDidStopRunningNotification, NSString *)

#define AVMediaTypeAudio getAVMediaTypeAudio()
#define AVMediaTypeMuxed getAVMediaTypeMuxed()
#define AVMediaTypeVideo getAVMediaTypeVideo()
#define AVCaptureSessionPreset1280x720 getAVCaptureSessionPreset1280x720()
#define AVCaptureSessionPreset640x480 getAVCaptureSessionPreset640x480()
#define AVCaptureSessionPreset352x288 getAVCaptureSessionPreset352x288()
#define AVCaptureSessionPresetLow getAVCaptureSessionPresetLow()
#define AVCaptureSessionDidStopRunningNotification getAVCaptureSessionDidStopRunningNotification()

using namespace WebCore;

@interface WebCoreAVMediaCaptureSourceObserver : NSObject<AVCaptureAudioDataOutputSampleBufferDelegate, AVCaptureVideoDataOutputSampleBufferDelegate>
{
    AVMediaCaptureSource* m_callback;
}

-(id)initWithCallback:(AVMediaCaptureSource*)callback;
-(void)disconnect;
-(void)captureOutput:(AVCaptureOutputType *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType *)connection;
-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(void*)context;
@end

namespace WebCore {

static NSArray* sessionKVOProperties();

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
        globalQueue = dispatch_queue_create("WebCoreAVMediaCaptureSource video capture queue", DISPATCH_QUEUE_SERIAL);
        dispatch_set_target_queue(globalQueue, dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_HIGH, 0));
    });
    return globalQueue;
}

AVMediaCaptureSource::AVMediaCaptureSource(AVCaptureDeviceType* device, const AtomicString& id, RealtimeMediaSource::Type type, PassRefPtr<MediaConstraints> constraints)
    : RealtimeMediaSource(id, type, emptyString())
    , m_weakPtrFactory(this)
    , m_objcObserver(adoptNS([[WebCoreAVMediaCaptureSourceObserver alloc] initWithCallback:this]))
    , m_constraints(constraints)
    , m_device(device)
{
    setName(device.localizedName);
    setPersistentID(device.uniqueID);
    setMuted(true);
}

AVMediaCaptureSource::~AVMediaCaptureSource()
{
    [m_objcObserver disconnect];

    if (m_session) {
        for (NSString *keyName in sessionKVOProperties())
            [m_session.get() removeObserver:m_objcObserver.get() forKeyPath:keyName];
        [m_session.get() stopRunning];
    }
}

void AVMediaCaptureSource::startProducingData()
{
    if (!m_session)
        setupSession();
    
    if ([m_session.get() isRunning])
        return;
    
    [m_session.get() startRunning];
}

void AVMediaCaptureSource::stopProducingData()
{
    if (!m_session || ![m_session.get() isRunning])
        return;

    [m_session.get() stopRunning];
}

const RealtimeMediaSourceSettings& AVMediaCaptureSource::settings()
{
    if (m_currentSettings.deviceId().isEmpty())
        m_currentSettings.setSupportedConstraits(supportedConstraints());

    m_currentSettings.setDeviceId(id());
    updateSettings(m_currentSettings);
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

RefPtr<RealtimeMediaSourceCapabilities> AVMediaCaptureSource::capabilities()
{
    if (!m_capabilities) {
        m_capabilities = RealtimeMediaSourceCapabilities::create(AVCaptureDeviceManager::singleton().supportedConstraints());
        m_capabilities->setDeviceId(id());

        initializeCapabilities(*m_capabilities.get());
    }
    return m_capabilities;
}

void AVMediaCaptureSource::setupSession()
{
    if (m_session)
        return;

    m_session = adoptNS([allocAVCaptureSessionInstance() init]);
    for (NSString *keyName in sessionKVOProperties())
        [m_session.get() addObserver:m_objcObserver.get() forKeyPath:keyName options:NSKeyValueObservingOptionNew context:(void *)nil];

    setupCaptureSession();
}

void AVMediaCaptureSource::reset()
{
    RealtimeMediaSource::reset();
    m_isRunning = false;
    for (NSString *keyName in sessionKVOProperties())
        [m_session.get() removeObserver:m_objcObserver.get() forKeyPath:keyName];
    shutdownCaptureSession();
    m_session = nullptr;
}

void AVMediaCaptureSource::captureSessionIsRunningDidChange(bool state)
{
    scheduleDeferredTask([this, state] {
        if (state == m_isRunning)
            return;

        m_isRunning = state;
        setMuted(!m_isRunning);
    });
}

void AVMediaCaptureSource::setVideoSampleBufferDelegate(AVCaptureVideoDataOutputType* videoOutput)
{
    [videoOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaVideoCaptureSerialQueue()];
}

void AVMediaCaptureSource::setAudioSampleBufferDelegate(AVCaptureAudioDataOutputType* audioOutput)
{
    [audioOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaAudioCaptureSerialQueue()];
}

void AVMediaCaptureSource::scheduleDeferredTask(std::function<void ()> function)
{
    ASSERT(function);

    auto weakThis = createWeakPtr();
    callOnMainThread([weakThis, function] {
        if (!weakThis)
            return;

        function();
    });
}

AudioSourceProvider* AVMediaCaptureSource::audioSourceProvider()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

NSArray* sessionKVOProperties()
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
    m_callback = 0;
}

- (void)captureOutput:(AVCaptureOutputType *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType *)connection
{
    if (!m_callback)
        return;

    m_callback->captureOutputDidOutputSampleBufferFromConnection(captureOutput, sampleBuffer, connection);
}

-(void)observeValueForKeyPath:keyPath ofObject:(id)object change:(NSDictionary *)change context:(void*)context
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

@end

#endif // ENABLE(MEDIA_STREAM)
