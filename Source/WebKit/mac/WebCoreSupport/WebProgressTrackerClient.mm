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

#import "WebProgressTrackerClient.h"

#import "WebFramePrivate.h"
#import "WebViewInternal.h"

using namespace WebCore;

WebProgressTrackerClient::WebProgressTrackerClient(WebView *webView)
    : m_webView(webView)
{
}

void WebProgressTrackerClient::progressTrackerDestroyed()
{
    delete this;
}

#if !PLATFORM(IOS)
void WebProgressTrackerClient::willChangeEstimatedProgress()
{
    [m_webView _willChangeValueForKey:_WebEstimatedProgressKey];
}

void WebProgressTrackerClient::didChangeEstimatedProgress()
{
    [m_webView _didChangeValueForKey:_WebEstimatedProgressKey];
}
#endif

void WebProgressTrackerClient::progressStarted(WebCore::Frame& originatingProgressFrame)
{
#if !PLATFORM(IOS)
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressStartedNotification object:m_webView];
#else
    WebThreadPostNotification(WebViewProgressStartedNotification, m_webView, nil);
#endif
}

void WebProgressTrackerClient::progressEstimateChanged(WebCore::Frame&)
{
#if !PLATFORM(IOS)
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressEstimateChangedNotification object:m_webView];
#else
    NSNumber *progress = [NSNumber numberWithFloat:[m_webView estimatedProgress]];
    CGColorRef bodyBackgroundColor = [[m_webView mainFrame] _bodyBackgroundColor];
    
    // Use a CFDictionary so we can add the CGColorRef without compile errors. And then thanks to
    // toll-free bridging we can pass the CFDictionary as an NSDictionary to postNotification.
    CFMutableDictionaryRef userInfo = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(userInfo, WebViewProgressEstimatedProgressKey, progress);
    if (bodyBackgroundColor)
        CFDictionaryAddValue(userInfo, WebViewProgressBackgroundColorKey, bodyBackgroundColor);
    
    WebThreadPostNotification(WebViewProgressEstimateChangedNotification, m_webView, (NSDictionary *) userInfo);
    CFRelease(userInfo);
#endif
}

void WebProgressTrackerClient::progressFinished(Frame&)
{
#if !PLATFORM(IOS)
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewProgressFinishedNotification object:m_webView];
#endif
}
