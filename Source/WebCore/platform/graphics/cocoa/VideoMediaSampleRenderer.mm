/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "VideoMediaSampleRenderer.h"

#import "WebCoreDecompressionSession.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>

#pragma mark - Soft Linking

#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (WebCoreAVSampleBufferDisplayLayerQueueManagementPrivate)
- (void)expectMinimumUpcomingSampleBufferPresentationTime:(CMTime)minimumUpcomingPresentationTime;
- (void)resetUpcomingSampleBufferPresentationTimeExpectations;
@end

namespace WebCore {

VideoMediaSampleRenderer::VideoMediaSampleRenderer(WebSampleBufferVideoRendering *renderer)
    : m_renderer(renderer)
{
}

VideoMediaSampleRenderer::~VideoMediaSampleRenderer()
{
    [m_renderer flush];
    [m_renderer stopRequestingMediaData];
    if (m_decompressionSession)
        m_decompressionSession->invalidate();
}

bool VideoMediaSampleRenderer::isReadyForMoreMediaData() const
{
    return (!m_decompressionSession || m_decompressionSession->isReadyForMoreMediaData()) && [m_renderer isReadyForMoreMediaData];
}

void VideoMediaSampleRenderer::stopRequestingMediaData()
{
    [m_renderer stopRequestingMediaData];
}

void VideoMediaSampleRenderer::enqueueSample(CMSampleBufferRef sample, bool displaying)
{
#if PLATFORM(IOS_FAMILY)
    if (!m_decompressionSession && !m_currentCodec) {
        // Only use a decompression session for vp8 or vp9 when software decoded.
        CMVideoFormatDescriptionRef videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
        auto fourCC = PAL::CMFormatDescriptionGetMediaSubType(videoFormatDescription);
        if (fourCC == 'vp08' || (fourCC == 'vp09' && !(canLoad_VideoToolbox_VTIsHardwareDecodeSupported() && VTIsHardwareDecodeSupported(kCMVideoCodecType_VP9))))
            initializeDecompressionSession();
        m_currentCodec = fourCC;
    }
#endif

    if (!m_decompressionSession) {
        [m_renderer enqueueSampleBuffer:sample];
        return;
    }
    m_decompressionSession->enqueueSample(sample, displaying);
    ++m_decodePending;
    m_wasNotDisplaying |= !displaying;
    if (m_requestMediaDataWhenReadySet)
        return;
    m_requestMediaDataWhenReadySet = true;
    // We set requestMediaDataWhenReady only after calling enqueueSample once, to avoid having the callback
    // being called immediately.
    m_decompressionSession->requestMediaDataWhenReady([weakThis = ThreadSafeWeakPtr { *this }, this] {
        if (RefPtr protectedThis = weakThis.get()) {
            if (!--m_decodePending && m_minimumUpcomingPresentationTime) {
                expectMinimumUpcomingSampleBufferPresentationTime(*m_minimumUpcomingPresentationTime);
                m_minimumUpcomingPresentationTime.reset();
            }
            if (!m_wasNotDisplaying)
                return;
            m_wasNotDisplaying = false;
            if (m_readyForMoreSampleFunction)
                m_readyForMoreSampleFunction();
        }
    });
}

void VideoMediaSampleRenderer::initializeDecompressionSession()
{
    if (m_decompressionSession)
        m_decompressionSession->invalidate();
    m_decompressionSession = WebCoreDecompressionSession::createOpenGL();
    m_decompressionSession->setTimebase([m_renderer timebase]);
    m_decompressionSession->setResourceOwner(m_resourceOwner);
    m_decompressionSession->decodedFrameWhenAvailable([weakThis = ThreadSafeWeakPtr { *this }](RetainPtr<CMSampleBufferRef>&& sample) {
        if (RefPtr protectedThis = weakThis.get())
            [protectedThis->m_renderer enqueueSampleBuffer:sample.get()];
    });
    m_decompressionSession->setErrorListener([weakThis = ThreadSafeWeakPtr { *this }, this](OSStatus status) {
        if (RefPtr protectedThis = weakThis.get()) {
            // Simulate AVSBDL decoding error.
            RetainPtr error = [NSError errorWithDomain:@"com.apple.WebKit" code:status userInfo:nil];
            NSDictionary *userInfoDict = @{ (__bridge NSString *)AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey: (__bridge NSError *)error.get() };
            [NSNotificationCenter.defaultCenter postNotificationName:AVSampleBufferDisplayLayerFailedToDecodeNotification object:m_renderer.get() userInfo:userInfoDict];
            [NSNotificationCenter.defaultCenter postNotificationName:AVSampleBufferVideoRendererDidFailToDecodeNotification object:m_renderer.get() userInfo:userInfoDict];
        }
    });

    resetReadyForMoreSample();
}

void VideoMediaSampleRenderer::flush()
{
    [m_renderer flush];
    if (m_decompressionSession)
        m_decompressionSession->flush();
}

void VideoMediaSampleRenderer::requestMediaDataWhenReady(Function<void()>&& function)
{
    m_readyForMoreSampleFunction = WTFMove(function);
    resetReadyForMoreSample();
}

void VideoMediaSampleRenderer::resetReadyForMoreSample()
{
    ThreadSafeWeakPtr weakThis { *this };
    [m_renderer requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_readyForMoreSampleFunction)
            protectedThis->m_readyForMoreSampleFunction();
    }];
}

void VideoMediaSampleRenderer::expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime& time)
{
    if (![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(expectMinimumUpcomingSampleBufferPresentationTime:)])
        return;
    if (!m_decodePending)
        [m_renderer expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(time)];
    else
        m_minimumUpcomingPresentationTime = time;
}

void VideoMediaSampleRenderer::resetUpcomingSampleBufferPresentationTimeExpectations()
{
    if (![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(resetUpcomingSampleBufferPresentationTimeExpectations)])
        return;
    [m_renderer resetUpcomingSampleBufferPresentationTimeExpectations];
    m_minimumUpcomingPresentationTime.reset();
}

AVSampleBufferDisplayLayer *VideoMediaSampleRenderer::displayLayer() const
{
    return dynamic_objc_cast<AVSampleBufferDisplayLayer>(m_renderer.get(), PAL::getAVSampleBufferDisplayLayerClass());
}

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)

RetainPtr<CVPixelBufferRef> VideoMediaSampleRenderer::copyDisplayedPixelBuffer() const
{
    return adoptCF([m_renderer copyDisplayedPixelBuffer]);
}

CGRect VideoMediaSampleRenderer::bounds() const
{
    return [displayLayer() bounds];
}

#endif

} // namespace WebCore
