/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "MediaConstraints.h"
#import "NotImplemented.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceSettings.h"
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <objc/runtime.h>

namespace WebCore {

Ref<MockRealtimeVideoSource> MockRealtimeVideoSource::create()
{
    return adoptRef(*new MockRealtimeVideoSourceMac());
}

MockRealtimeVideoSourceMac::MockRealtimeVideoSourceMac()
    : MockRealtimeVideoSource()
{
}

PlatformLayer* MockRealtimeVideoSourceMac::platformLayer() const
{
    if (m_previewLayer)
        return m_previewLayer.get();

    m_previewLayer = adoptNS([[CALayer alloc] init]);
    m_previewLayer.get().name = @"MockRealtimeVideoSourceMac preview layer";
    m_previewLayer.get().contentsGravity = kCAGravityResizeAspect;
    m_previewLayer.get().anchorPoint = CGPointZero;
    m_previewLayer.get().needsDisplayOnBoundsChange = YES;
#if !PLATFORM(IOS)
    m_previewLayer.get().autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
#endif

    updatePlatformLayer();

    return m_previewLayer.get();
}

void MockRealtimeVideoSourceMac::updatePlatformLayer() const
{
    if (!m_previewLayer)
        return;

    [CATransaction begin];
    [CATransaction setAnimationDuration:0];
    [CATransaction setDisableActions:YES];

    do {
        RefPtr<Image> image = imageBuffer()->copyImage();
        if (!image)
            break;

        m_previewImage = image->getCGImageRef();
        if (!m_previewImage)
            break;

        m_previewLayer.get().contents = (NSObject*)(m_previewImage.get());
    } while (0);

    [CATransaction commit];
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
