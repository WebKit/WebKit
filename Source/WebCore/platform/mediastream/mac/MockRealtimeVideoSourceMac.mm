/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "MockRealtimeVideoSourceMac.h"

#if ENABLE(MEDIA_STREAM)
#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "ImageTransferSessionVT.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "MockRealtimeMediaSourceCenter.h"
#import "NotImplemented.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceSettings.h"
#import "RealtimeVideoSource.h"
#import "RealtimeVideoUtilities.h"
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <objc/runtime.h>

#import "CoreVideoSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

CaptureSourceOrError MockRealtimeVideoSource::create(String&& deviceID, String&& name, String&& hashSalt, const MediaConstraints* constraints)
{
#ifndef NDEBUG
    auto device = MockRealtimeMediaSourceCenter::mockDeviceWithPersistentID(deviceID);
    ASSERT(device);
    if (!device)
        return { "No mock camera device"_s };
#endif

    auto source = adoptRef(*new MockRealtimeVideoSourceMac(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));
    if (constraints) {
        if (auto error = source->applyConstraints(*constraints))
            return WTFMove(error->message);
    }

    return CaptureSourceOrError(RealtimeVideoSource::create(WTFMove(source)));
}

Ref<MockRealtimeVideoSource> MockRealtimeVideoSourceMac::createForMockDisplayCapturer(String&& deviceID, String&& name, String&& hashSalt)
{
    return adoptRef(*new MockRealtimeVideoSourceMac(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt)));
}

MockRealtimeVideoSourceMac::MockRealtimeVideoSourceMac(String&& deviceID, String&& name, String&& hashSalt)
    : MockRealtimeVideoSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
    , m_workQueue(WorkQueue::create("MockRealtimeVideoSource Render Queue", WorkQueue::QOS::UserInteractive))
{
}

void MockRealtimeVideoSourceMac::updateSampleBuffer()
{
    auto imageBuffer = this->imageBuffer();
    if (!imageBuffer)
        return;

    if (!m_imageTransferSession)
        m_imageTransferSession = ImageTransferSessionVT::create(preferedPixelBufferFormat());

    PlatformImagePtr platformImage;
    if (auto nativeImage = imageBuffer->copyImage()->nativeImage())
        platformImage = nativeImage->platformImage();

    auto sampleTime = MediaTime::createWithDouble((elapsedTime() + 100_ms).seconds());
    auto sampleBuffer = m_imageTransferSession->createMediaSample(platformImage.get(), sampleTime, size(), sampleRotation());
    if (!sampleBuffer)
        return;

    auto captureTime = MonotonicTime::now().secondsSinceEpoch();
    m_workQueue->dispatch([this, protectedThis = Ref { *this }, sampleBuffer = WTFMove(sampleBuffer), captureTime]() mutable {
        VideoSampleMetadata metadata;
        metadata.captureTime = captureTime;
        dispatchMediaSampleToObservers(*sampleBuffer, metadata);
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
