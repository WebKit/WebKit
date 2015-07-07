/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef TextTrackRepresentationIOS_h
#define TextTrackRepresentationIOS_h

#if PLATFORM(IOS) && ENABLE(VIDEO_TRACK)

#import "TextTrackRepresentation.h"
#import <QuartzCore/CALayer.h>
#import <wtf/RetainPtr.h>

@class WebCoreTextTrackRepresentationIOSHelper;

namespace WebCore {

class TextTrackRepresentationIOS : public TextTrackRepresentation {
public:
    TextTrackRepresentationIOS(TextTrackRepresentationClient*);
    virtual ~TextTrackRepresentationIOS();

    virtual void update() override;
    virtual PlatformLayer* platformLayer() override { return m_layer.get(); }
    virtual void setContentScale(float) override;
    virtual IntRect bounds() const override;

    TextTrackRepresentationClient* client() const { return m_client; }

private:
    TextTrackRepresentationClient* m_client;
    RetainPtr<CALayer> m_layer;
    RetainPtr<WebCoreTextTrackRepresentationIOSHelper> m_delegate;
};

}

#endif // PLATFORM(IOS) && ENABLE(VIDEO_TRACK)

#endif // TextTrackRepresentationIOS_h
