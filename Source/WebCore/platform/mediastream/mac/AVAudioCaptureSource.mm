/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#import "AVAudioCaptureSource.h"

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "RealtimeMediaSourceSettings.h"
#import "SoftLinking.h"
#import "WebAudioSourceProviderAVFObjC.h"
#import <AVFoundation/AVCaptureInput.h>
#import <AVFoundation/AVCaptureOutput.h>
#import <AVFoundation/AVCaptureSession.h>
#import <CoreAudio/CoreAudioTypes.h>
#import <wtf/HashSet.h>

#import "CoreMediaSoftLink.h"

typedef AVCaptureAudioChannel AVCaptureAudioChannelType;
typedef AVCaptureAudioDataOutput AVCaptureAudioDataOutputType;
typedef AVCaptureConnection AVCaptureConnectionType;
typedef AVCaptureDevice AVCaptureDeviceTypedef;
typedef AVCaptureDeviceInput AVCaptureDeviceInputType;
typedef AVCaptureOutput AVCaptureOutputType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVCaptureAudioChannel)
SOFT_LINK_CLASS(AVFoundation, AVCaptureAudioDataOutput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureConnection)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDevice)
SOFT_LINK_CLASS(AVFoundation, AVCaptureDeviceInput)
SOFT_LINK_CLASS(AVFoundation, AVCaptureOutput)

#define AVCaptureAudioChannel getAVCaptureAudioChannelClass()
#define AVCaptureAudioDataOutput getAVCaptureAudioDataOutputClass()
#define AVCaptureConnection getAVCaptureConnectionClass()
#define AVCaptureDevice getAVCaptureDeviceClass()
#define AVCaptureDeviceFormat getAVCaptureDeviceFormatClass()
#define AVCaptureDeviceInput getAVCaptureDeviceInputClass()
#define AVCaptureOutput getAVCaptureOutputClass()
#define AVFrameRateRange getAVFrameRateRangeClass()

SOFT_LINK_POINTER(AVFoundation, AVMediaTypeAudio, NSString *)

#define AVMediaTypeAudio getAVMediaTypeAudio()

namespace WebCore {

RefPtr<AVMediaCaptureSource> AVAudioCaptureSource::create(AVCaptureDeviceTypedef* device, const AtomicString& id, const MediaConstraints* constraints, String& invalidConstraint)
{
    auto source = adoptRef(new AVAudioCaptureSource(device, id));
    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result) {
            invalidConstraint = result.value().first;
            source = nullptr;
        }
    }

    return source;
}

AVAudioCaptureSource::AVAudioCaptureSource(AVCaptureDeviceTypedef* device, const AtomicString& id)
    : AVMediaCaptureSource(device, id, RealtimeMediaSource::Audio)
{
    m_inputDescription = std::make_unique<AudioStreamBasicDescription>();
}
    
AVAudioCaptureSource::~AVAudioCaptureSource()
{
}

void AVAudioCaptureSource::initializeCapabilities(RealtimeMediaSourceCapabilities& capabilities)
{
    // FIXME: finish this implementation - https://webkit.org/b/122430
    capabilities.setVolume(CapabilityValueOrRange(0.0, 1.0));
}

void AVAudioCaptureSource::initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints& supportedConstraints)
{
    supportedConstraints.setSupportsVolume(true);
}

void AVAudioCaptureSource::updateSettings(RealtimeMediaSourceSettings& settings)
{
    // FIXME: support volume

    settings.setDeviceId(id());
}

void AVAudioCaptureSource::addObserver(AVAudioCaptureSource::Observer* observer)
{
    LockHolder lock(m_lock);
    m_observers.append(observer);
    if (m_inputDescription->mSampleRate)
        observer->prepare(m_inputDescription.get());
}

void AVAudioCaptureSource::removeObserver(AVAudioCaptureSource::Observer* observer)
{
    LockHolder lock(m_lock);
    size_t pos = m_observers.find(observer);
    if (pos != notFound)
        m_observers.remove(pos);
}

void AVAudioCaptureSource::setupCaptureSession()
{
    RetainPtr<AVCaptureDeviceInputType> audioIn = adoptNS([allocAVCaptureDeviceInputInstance() initWithDevice:device() error:nil]);

    if (![session() canAddInput:audioIn.get()]) {
        LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add audio input device", this);
        return;
    }
    [session() addInput:audioIn.get()];

    RetainPtr<AVCaptureAudioDataOutputType> audioOutput = adoptNS([allocAVCaptureAudioDataOutputInstance() init]);
    setAudioSampleBufferDelegate(audioOutput.get());

    if (![session() canAddOutput:audioOutput.get()]) {
        LOG(Media, "AVVideoCaptureSource::setupCaptureSession(%p), unable to add audio sample buffer output delegate", this);
        return;
    }
    [session() addOutput:audioOutput.get()];
    m_audioConnection = [audioOutput.get() connectionWithMediaType:AVMediaTypeAudio];
}

void AVAudioCaptureSource::shutdownCaptureSession()
{
    {
        LockHolder lock(m_lock);

        m_audioConnection = nullptr;
        m_inputDescription = std::make_unique<AudioStreamBasicDescription>();

        for (auto& observer : m_observers)
            observer->unprepare();
        m_observers.shrink(0);
    }

    // Don't hold the lock when destroying the audio provider, it will call back into this object
    // to remove itself as an observer.
    m_audioSourceProvider = nullptr;
}

static bool operator==(const AudioStreamBasicDescription& a, const AudioStreamBasicDescription& b)
{
    return a.mSampleRate == b.mSampleRate
        && a.mFormatID == b.mFormatID
        && a.mFormatFlags == b.mFormatFlags
        && a.mBytesPerPacket == b.mBytesPerPacket
        && a.mFramesPerPacket == b.mFramesPerPacket
        && a.mBytesPerFrame == b.mBytesPerFrame
        && a.mChannelsPerFrame == b.mChannelsPerFrame
        && a.mBitsPerChannel == b.mBitsPerChannel;
}

static bool operator!=(const AudioStreamBasicDescription& a, const AudioStreamBasicDescription& b)
{
    return !(a == b);
}

void AVAudioCaptureSource::captureOutputDidOutputSampleBufferFromConnection(AVCaptureOutputType*, CMSampleBufferRef sampleBuffer, AVCaptureConnectionType*)
{
    if (muted())
        return;

    CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);
    if (!formatDescription)
        return;

    RetainPtr<CMSampleBufferRef> buffer = sampleBuffer;
    scheduleDeferredTask([this, buffer] {
        mediaDataUpdated(MediaSampleAVFObjC::create(buffer.get()));
    });

    std::unique_lock<Lock> lock(m_lock, std::try_to_lock);
    if (!lock.owns_lock()) {
        // Failed to acquire the lock, just return instead of blocking.
        return;
    }

    if (m_observers.isEmpty())
        return;

    const AudioStreamBasicDescription* streamDescription = CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription);
    if (*m_inputDescription != *streamDescription) {
        m_inputDescription = std::make_unique<AudioStreamBasicDescription>(*streamDescription);
        for (auto& observer : m_observers)
            observer->prepare(m_inputDescription.get());
    }

    for (auto& observer : m_observers)
        observer->process(formatDescription, sampleBuffer);
}

AudioSourceProvider* AVAudioCaptureSource::audioSourceProvider()
{
    if (!m_audioSourceProvider)
        m_audioSourceProvider = WebAudioSourceProviderAVFObjC::create(*this);

    return m_audioSourceProvider.get();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
