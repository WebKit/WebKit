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

#import "AVAudioCaptureSource.h"

#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaStreamSourceStates.h"
#import "NotImplemented.h"
#import "SoftLinking.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>

typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceType;
typedef AVCaptureDeviceInput AVCaptureDeviceInputType;
typedef AVCaptureOutput AVCaptureOutputType;
typedef AVCaptureAudioDataOutput AVCaptureAudioDataOutputType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVCaptureAudioDataOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureConnection)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceInput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureOutput)

#define AVCaptureAudioDataOutput getAVCaptureAudioDataOutputClass()
#define AVCaptureConnection getAVCaptureConnectionClass()
#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureDeviceInput getAVCaptureDeviceInputClass()
#define AVCaptureOutput getAVCaptureOutputClass()

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)

#define AVMediaTypeAudio getAVMediaTypeAudio()

namespace WebCore {

RefPtr<AVMediaCaptureSource> AVAudioCaptureSource::create(AVCaptureDeviceType* device, const AtomicString& id, PassRefPtr<MediaConstraints> constraint)
{
    return adoptRef(new AVAudioCaptureSource(device, id, constraint));
}
    
AVAudioCaptureSource::AVAudioCaptureSource(AVCaptureDeviceType* device, const AtomicString& id, PassRefPtr<MediaConstraints> constraints)
    : AVMediaCaptureSource(device, id, MediaStreamSource::Audio, constraints)
{
    currentStates()->setSourceId(id);
    currentStates()->setSourceType(MediaStreamSourceStates::Microphone);
}
    
AVAudioCaptureSource::~AVAudioCaptureSource()
{
}

RefPtr<MediaStreamSourceCapabilities> AVAudioCaptureSource::capabilities() const
{
    notImplemented();
    return 0;
}

void AVAudioCaptureSource::updateStates()
{
    // FIXME: use [AVCaptureAudioPreviewOutput volume] for volume
}

void AVAudioCaptureSource::setupCaptureSession()
{
    RetainPtr<AVCaptureDeviceInputType> audioIn = adoptNS([[AVCaptureDeviceInput alloc] initWithDevice:device() error:nil]);
    ASSERT([session() canAddInput:audioIn.get()]);
    if ([session() canAddInput:audioIn.get()])
        [session() addInput:audioIn.get()];
    
    RetainPtr<AVCaptureAudioDataOutputType> audioOutput = adoptNS([[AVCaptureAudioDataOutput alloc] init]);
    setAudioSampleBufferDelegate(audioOutput.get());
    ASSERT([session() canAddOutput:audioOutput.get()]);
    if ([session() canAddOutput:audioOutput.get()])
        [session() addOutput:audioOutput.get()];
    m_audioConnection = adoptNS([audioOutput.get() connectionWithMediaType:AVMediaTypeAudio]);
}

void AVAudioCaptureSource::captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutputType*, CMSampleBufferRef sampleBuffer, AVCaptureConnectionType*)
{
    CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);
    if (!formatDescription)
        return;

    CFRetain(formatDescription);
    m_audioFormatDescription = adoptCF(formatDescription);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
