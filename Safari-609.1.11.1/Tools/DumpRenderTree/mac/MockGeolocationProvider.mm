/*
 * Copyright (C) 2010, 2012, 2014 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "MockGeolocationProvider.h"

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WebCoreThreadRun.h>
#endif

@implementation MockGeolocationProvider

+ (MockGeolocationProvider *)shared
{
    static MockGeolocationProvider *provider = [[MockGeolocationProvider alloc] init];
    return provider;
}

- (void)dealloc
{
    ASSERT(_registeredViews.isEmpty());

    _lastPosition.clear();
    _errorMessage.clear();
    [super dealloc];
}

- (void)resetError
{
    _hasError = NO;
    _errorMessage.clear();
}

- (void)setPosition:(WebGeolocationPosition *)position
{
    _lastPosition = position;
    
    [self resetError];

    if (!_timer)
        _timer = [NSTimer scheduledTimerWithTimeInterval:0 target:self selector:@selector(timerFired) userInfo:0 repeats:NO];
}

- (void)setPositionUnavailableErrorWithMessage:(NSString *)errorMessage
{
    _hasError = YES;
    _errorMessage = errorMessage;

    _lastPosition.clear();

    if (!_timer)
        _timer = [NSTimer scheduledTimerWithTimeInterval:0 target:self selector:@selector(timerFired) userInfo:0 repeats:NO];
}

- (void)registerWebView:(WebView *)webView
{
    _registeredViews.add((__bridge CFTypeRef)webView);

    if (!_timer)
        _timer = [NSTimer scheduledTimerWithTimeInterval:0 target:self selector:@selector(timerFired) userInfo:0 repeats:NO];
}

- (void)unregisterWebView:(WebView *)webView
{
    _registeredViews.remove((__bridge CFTypeRef)webView);
}

- (WebGeolocationPosition *)lastPosition
{
    return _lastPosition.get();
}

#if PLATFORM(IOS_FAMILY)
- (void)setEnableHighAccuracy:(BOOL)enableHighAccuracy
{
    UNUSED_PARAM(enableHighAccuracy);
}

- (void)initializeGeolocationForWebView:(WebView *)webView listener:(id<WebGeolocationProviderInitializationListener>)listener
{
    [listener initializationAllowedWebView:webView];
}

- (void)stopTrackingWebView:(WebView *)webView
{
    UNUSED_PARAM(webView);
}
#endif

- (void)stopTimer
{
    [_timer invalidate];
    _timer = 0;
}

- (BOOL)isActive
{
    return !_registeredViews.isEmpty();
}

- (void)timerFired
{
    _timer = 0;

    // Expect that views won't be (un)registered while iterating.
    auto copyOfRegisteredViews { _registeredViews };
    for (auto typelessView : copyOfRegisteredViews) {
        auto webView = (__bridge WebView *)typelessView;
#if !PLATFORM(IOS_FAMILY)
        if (_hasError)
            [webView _geolocationDidFailWithMessage:_errorMessage.get()];
        else
            [webView _geolocationDidChangePosition:_lastPosition.get()];
#else
        WebGeolocationPosition *lastPosition = _lastPosition.get();
        NSString *errorMessage = _errorMessage.get();
        if (_hasError) {
            WebThreadRun(^{
                [webView _geolocationDidFailWithMessage:errorMessage];
            });
        } else {
            WebThreadRun(^{
                [webView _geolocationDidChangePosition:lastPosition];
            });
        }
#endif
    }
}

@end
