/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if USE(AVFOUNDATION) && ENABLE(MEDIA_SOURCE)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestNSBundleExtras.h"
#import <WebCore/CMUtilities.h>
#import <WebCore/ContentType.h>
#import <WebCore/MediaPlayerEnums.h>
#import <WebCore/MediaSampleAVFObjC.h>
#import <WebCore/MediaSamplesDecoder.h>
#import <WebCore/MediaSourceConfiguration.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SourceBufferParser.h>
#import <WebCore/TrackInfo.h>
#import <WebCore/VideoMediaSampleRenderer.h>
#import <wtf/Logger.h>

#import <pal/cf/CoreMediaSoftLink.h>

using WebCore::ContentType;
using WebCore::MediaSampleAVFObjC;
using WebCore::MediaSamplesDecoder;
using WebCore::MediaSourceConfiguration;
using WebCore::SharedBuffer;
using WebCore::SourceBufferParser;
using WebCore::VideoMediaSampleRenderer;
using WebCore::VideoRendererPreference;
using WebCore::createVideoInfoFromFormatDescription;

namespace TestWebKitAPI {

static Vector<Ref<MediaSampleAVFObjC>> parseWebMVideoSamples(NSString *resourceName, const String& contentType)
{
    Vector<Ref<MediaSampleAVFObjC>> samples;

    RetainPtr fileData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:resourceName withExtension:@"webm"]];
    if (!fileData)
        return samples;

    RefPtr parser = SourceBufferParser::create(ContentType { contentType }, MediaSourceConfiguration { });
    if (!parser)
        return samples;

    parser->setCallOnClientThreadCallback([](auto&& function) {
        function();
    });
    parser->setDidProvideMediaDataCallback([&](Ref<MediaSampleAVFObjC>&& sample, uint64_t, const String&) {
        if (PAL::CMFormatDescriptionGetMediaType(PAL::CMSampleBufferGetFormatDescription(sample->platformSample().cmSampleBuffer())) == kCMMediaType_Video)
            samples.append(WTF::move(sample));
    });
    if (!parser->appendData(SharedBuffer::create(fileData.get())))
        return { };
    parser->flushPendingMediaData();
    return samples;
}

static RetainPtr<CMSampleBufferRef> createDecodedVideoSampleBuffer()
{
    CVPixelBufferRef rawPixelBuffer = nullptr;
    if (CVPixelBufferCreate(kCFAllocatorDefault, 16, 16, kCVPixelFormatType_32BGRA, nullptr, &rawPixelBuffer))
        return nullptr;
    RetainPtr pixelBuffer = adoptCF(rawPixelBuffer);

    CMVideoFormatDescriptionRef rawFormatDescription = nullptr;
    if (PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer, &rawFormatDescription))
        return nullptr;
    RetainPtr formatDescription = adoptCF(rawFormatDescription);

    CMSampleTimingInfo timing = { PAL::kCMTimeInvalid, PAL::kCMTimeZero, PAL::kCMTimeInvalid };
    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer, true, nullptr, nullptr, formatDescription, &timing, &rawSampleBuffer))
        return nullptr;

    return adoptCF(rawSampleBuffer);
}

TEST(VideoMediaSampleRenderer, DecodedFrameDisplayedAfterHardwareDecoderFailureAndFlush)
{
    auto samples = parseWebMVideoSamples(@"test-vp8", "video/webm; codecs=vp8"_s);
    ASSERT_GE(samples.size(), 3u);

    RefPtr info = createVideoInfoFromFormatDescription(PAL::CMSampleBufferGetFormatDescription(samples[0]->platformSample().cmSampleBuffer()));
    ASSERT_TRUE(info);
    if (!MediaSamplesDecoder::canDecode(*info, { MediaSamplesDecoder::Filter::InProcess, MediaSamplesDecoder::Filter::SoftwareAllowed, MediaSamplesDecoder::Filter::ForceSoftware }))
        GTEST_SKIP() << "VPx software decoding is not available";

    Ref logger = Logger::create(this);
    Ref renderer = VideoMediaSampleRenderer::create(nil, logger, 0);
    renderer->setPreferences(VideoRendererPreference::RequiresHardwareDecoder);

    bool gotDecodingError = false;
    renderer->notifyWhenDecodingErrorOccurred([&](NSError *) {
        gotDecodingError = true;
    });
    bool firstFrameAvailable = false;
    renderer->notifyFirstFrameAvailable([&](const MediaTime&, double) {
        firstFrameAvailable = true;
    });

    // There is no VP8 hardware decoder: the first sample fails to decode and the others stay queued.
    for (size_t index = 0; index < 3; ++index)
        renderer->enqueueSample(samples[index], { });
    ASSERT_TRUE(Util::runFor(&gotDecodingError, 5_s));

    renderer->flush();

    RetainPtr decodedSampleBuffer = createDecodedVideoSampleBuffer();
    ASSERT_TRUE(decodedSampleBuffer);
    renderer->enqueueSample(MediaSampleAVFObjC::create(decodedSampleBuffer, 0), { });
    EXPECT_TRUE(Util::runFor(&firstFrameAvailable, 5_s));
}

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION) && ENABLE(MEDIA_SOURCE)
