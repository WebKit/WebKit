/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebScreenClient.h"

#import "WebChromeClient.h"
#import "WebView.h"
#import <WebCore/FloatRect.h>
#import <WebCore/Screen.h>
#import <wtf/PassRefPtr.h>

using namespace WebCore;

PassRefPtr<WebScreenClient> WebScreenClient::create(WebView *webView)
{
    return new WebScreenClient(webView);
}

WebScreenClient::WebScreenClient(WebView *webView)
    : m_webView(webView)
{
}

int WebScreenClient::depth()
{
    return NSBitsPerPixelFromDepth([screen([m_webView window]) depth]);
}

int WebScreenClient::depthPerComponent()
{
    return NSBitsPerSampleFromDepth([screen([m_webView window]) depth]);
}

bool WebScreenClient::isMonochrome()
{
    NSString *colorSpace = NSColorSpaceFromDepth([screen([m_webView window]) depth]);
    return colorSpace == NSCalibratedWhiteColorSpace
        || colorSpace == NSCalibratedBlackColorSpace
        || colorSpace == NSDeviceWhiteColorSpace
        || colorSpace == NSDeviceBlackColorSpace;
}

FloatRect WebScreenClient::rect()
{
    NSScreen *s = screen([m_webView window]);
    return scaleFromScreen(flipScreenRect([s frame], s), s);
}

FloatRect WebScreenClient::usableRect()
{
    NSScreen *s = screen([m_webView window]);
    return scaleFromScreen(flipScreenRect([s visibleFrame], s), s);
}
