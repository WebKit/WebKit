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
#import "VideoFullscreenCaptions.h"

#import "FloatSize.h"
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>

namespace WebCore {
    
void VideoFullscreenCaptions::setTrackRepresentationImage(PlatformImagePtr textTrack)
{
    [m_captionsLayer setContents:(__bridge id)textTrack.get()];
}

void VideoFullscreenCaptions::setTrackRepresentationContentsScale(float scale)
{
    [m_captionsLayer setContentsScale:scale];
}

void VideoFullscreenCaptions::setTrackRepresentationHidden(bool hidden)
{
    [m_captionsLayer setHidden:hidden];
}

CALayer *VideoFullscreenCaptions::captionsLayer()
{
    if (!m_captionsLayer) {
        m_captionsLayer = adoptNS([[CALayer alloc] init]);
        [m_captionsLayer setName:@"Captions layer"];
    }
    return m_captionsLayer.get();
}

void VideoFullscreenCaptions::setCaptionsFrame(const CGRect& frame)
{
    [captionsLayer() setFrame:frame];
}

void VideoFullscreenCaptions::setupCaptionsLayer(CALayer* parent, const WebCore::FloatSize& initialSize)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [captionsLayer() removeFromSuperlayer];
    [parent addSublayer:captionsLayer()];
    captionsLayer().zPosition = FLT_MAX;
    [captionsLayer() setAnchorPoint:CGPointZero];
    [captionsLayer() setBounds:CGRectMake(0, 0, initialSize.width(), initialSize.height())];
    [CATransaction commit];
}

void VideoFullscreenCaptions::removeCaptionsLayer()
{
    [m_captionsLayer removeFromSuperlayer];
    m_captionsLayer = nullptr;
}

}
