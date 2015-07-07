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

#if PLATFORM(IOS) && ENABLE(REMOTE_INSPECTOR)

#import "WebIndicateLayer.h"

#import "WebFramePrivate.h"
#import "WebView.h"
#import <QuartzCore/CALayerPrivate.h>
#import <WebCore/ColorMac.h>
#import <WebCore/WAKWindow.h>

using namespace WebCore;

@implementation WebIndicateLayer

- (id)initWithWebView:(WebView *)webView
{
    self = [super init];
    if (!self)
        return nil;

    _webView = webView;

    self.canDrawConcurrently = NO;
    self.contentsScale = [[_webView window] screenScale];

    // Blue highlight color.
    static Color highlightColor(111.0f / 255.0f, 168.0f / 255.0f, 220.0f / 255.0f, 0.66f);
    self.backgroundColor = cachedCGColor(highlightColor, ColorSpaceDeviceRGB);

    return self;
}

- (void)layoutSublayers
{
    CGFloat documentScale = [[[_webView mainFrame] documentView] scale];
    [self setTransform:CATransform3DMakeScale(documentScale, documentScale, 1.0)];
    [self setFrame:[_webView frame]];
}

- (id<CAAction>)actionForKey:(NSString *)key
{
    // Disable all implicit animations.
    return nil;
}

@end

#endif
