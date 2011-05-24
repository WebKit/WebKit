/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#import "WebFullScreenManagerProxy.h"

#import "LayerTreeContext.h"
#import "WKFullScreenWindowController.h"
#import "WKViewInternal.h"
#import <WebCore/IntRect.h>

#if ENABLE(FULLSCREEN_API)

namespace WebKit {

void WebFullScreenManagerProxy::invalidate()
{
    if (!m_webView)
        return;
    
    [m_webView closeFullScreenWindowController];
    m_webView = 0;
}

void WebFullScreenManagerProxy::enterFullScreen()
{
    if (!m_webView)
        return;
    [[m_webView fullScreenWindowController] enterFullScreen:nil];
}

void WebFullScreenManagerProxy::exitFullScreen()
{
    if (!m_webView)
        return;
    [[m_webView fullScreenWindowController] exitFullScreen];
}

void WebFullScreenManagerProxy::beganEnterFullScreenAnimation()
{
    if (!m_webView)
        return;
    [[m_webView fullScreenWindowController] beganEnterFullScreenAnimation];
}

void WebFullScreenManagerProxy::finishedEnterFullScreenAnimation(bool completed)
{
    if (!m_webView)
        return;
    [[m_webView fullScreenWindowController] finishedEnterFullScreenAnimation:completed];
}

void WebFullScreenManagerProxy::beganExitFullScreenAnimation()
{
    if (!m_webView)
        return;
    [[m_webView fullScreenWindowController] beganExitFullScreenAnimation];
}

void WebFullScreenManagerProxy::finishedExitFullScreenAnimation(bool completed)
{
    if (!m_webView)
        return;
    [[m_webView fullScreenWindowController] finishedExitFullScreenAnimation:completed];
}
    
void WebFullScreenManagerProxy::enterAcceleratedCompositingMode(const LayerTreeContext& context)
{
    if (!m_webView)
        return;
    [[m_webView fullScreenWindowController] enterAcceleratedCompositingMode:context];
}

void WebFullScreenManagerProxy::exitAcceleratedCompositingMode()
{
    if (!m_webView)
        return;
    [[m_webView fullScreenWindowController] exitAcceleratedCompositingMode];
}

void WebFullScreenManagerProxy::getFullScreenRect(WebCore::IntRect& rect)
{
    if (!m_webView)
        return;
    rect = [[m_webView fullScreenWindowController] getFullScreenRect];
}

} // namespace WebKit

#endif
