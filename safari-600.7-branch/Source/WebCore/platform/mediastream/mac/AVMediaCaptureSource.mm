/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "AVMediaCaptureSource.h"

#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaStreamSourceStates.h"
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
SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)

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

SOFT_LINK(CoreMedia, CMSampleBufferGetFormatDescription, CMFormatDescriptionRef, (CMSampleBufferRef sbuf), (sbuf));
SOFT_LINK(CoreMedia, CMSampleBufferGetPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf));

using namespace WebCore;

@interface WebCoreAVMediaCaptureSourceObserver : NSObject<AVCaptureAudioDataOutputSampleBufferDelegate, AVCaptureVideoDataOutputSampleBufferDelegate>
{
    AVMediaCaptureSource* m_callback;
}

-(id)initWithCallback:(AVMediaCaptureSource*)callback;
-(void)disconnect;
-(void)captureSessionStoppedRunning:(NSNotification *)notification;
-(void)captureOutput:(AVCaptureOutputType *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType *)connection;
@end

namespace WebCore {

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

AVMediaCaptureSource::AVMediaCaptureSource(AVCaptureDeviceType* device, const AtomicString& id, MediaStreamSource::Type type, PassRefPtr<MediaConstraints> constraints)
    : MediaStreamSource(id, type, emptyString())
    , m_objcObserver(adoptNS([[WebCoreAVMediaCaptureSourceObserver alloc] initWithCallback:this]))
    , m_constraints(constraints)
    , m_device(device)
    , m_isRunning(false)
{
    setName([device localizedName]);
    m_currentStates.setSourceType(type == MediaStreamSource::Video ? MediaStreamSourceStates::Camera : MediaStreamSourceStates::Microphone);
}

AVMediaCaptureSource::~AVMediaCaptureSource()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objcObserver.get()];
    [m_objcObserver disconnect];
    [m_session.get() stopRunning];

    setReadyState(Ended);
}

void AVMediaCaptureSource::startProducingData()
{
    if (!m_session)
        setupSession();
    
    if ([m_session.get() isRunning])
        return;
    
    [m_session.get() startRunning];
    setEnabled(true);
    m_isRunning = true;
}

void AVMediaCaptureSource::stopProducingData()
{
    if (!m_session || ![m_session.get() isRunning])
        return;

    [m_session.get() stopRunning];
    setEnabled(false);
    m_isRunning = true;
}

const MediaStreamSourceStates& AVMediaCaptureSource::states()
{
    updateStates();
    return m_currentStates;
}

void AVMediaCaptureSource::setupSession()
{
    if (m_session)
        return;

    m_session = adoptNS([[AVCaptureSession alloc] init]);
    
    [[NSNotificationCenter defaultCenter] addObserver:m_objcObserver.get() selector:@selector(captureSessionStoppedRunning:) name:AVCaptureSessionDidStopRunningNotification object:nil];
    
    setupCaptureSession();
    setReadyState(Live);
}
 
void AVMediaCaptureSource::captureSessionStoppedRunning()
{
    stopProducingData();
}

void AVMediaCaptureSource::setVideoSampleBufferDelegate(AVCaptureVideoDataOutputType* videoOutput)
{
    [videoOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaVideoCaptureSerialQueue()];
}

void AVMediaCaptureSource::setAudioSampleBufferDelegate(AVCaptureAudioDataOutputType* audioOutput)
{
    [audioOutput setSampleBufferDelegate:m_objcObserver.get() queue:globaAudioCaptureSerialQueue()];
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

- (void)captureSessionStoppedRunning:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    if (!m_callback)
        return;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (m_callback)
            m_callback->captureSessionStoppedRunning();
    });
}

- (void)captureOutput:(AVCaptureOutputType *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnectionType *)connection
{
    if (!m_callback)
        return;

    CFRetain(sampleBuffer);
    dispatch_async(dispatch_get_main_queue(), ^{
        if (m_callback)
            m_callback->captureOutputDidOutputSampleBufferFromConnection(captureOutput, sampleBuffer, connection);
        CFRelease(sampleBuffer);
    });
}

@end

#endif // ENABLE(MEDIA_STREAM)
