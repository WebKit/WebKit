/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef FindIndicatorOverlayClientIOS_h
#define FindIndicatorOverlayClientIOS_h

#import <WebCore/Frame.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/PageOverlay.h>

namespace WebKit {

class FindIndicatorOverlayClientIOS : public WebCore::PageOverlay::Client {
public:
    FindIndicatorOverlayClientIOS(WebCore::Frame& frame)
        : m_frame(frame)
    {
    }

private:
    virtual void pageOverlayDestroyed(WebCore::PageOverlay&) override { }
    virtual void willMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override { }
    virtual void didMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override { }
    virtual void drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    virtual bool mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&) override { return false; }

    WebCore::Frame& m_frame;
};

} // namespace WebKit

#endif // FindIndicatorOverlayClientIOS_h
