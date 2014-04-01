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

#import "config.h"
#import "WKPreferencesInternal.h"

#if WK_API_ENABLED

#import "WebPreferences.h"
#import <wtf/RetainPtr.h>

@implementation WKPreferences
{
    RetainPtr<NSString> _userDefaultsPrefixKey;
}

- (instancetype)init
{
    return [self initWithUserDefaultsPrefixKey:nil];
}

- (instancetype)initWithUserDefaultsPrefixKey:(NSString *)userDefaultsPrefixKey
{
    if (!(self = [super init]))
        return nil;

    _userDefaultsPrefixKey = adoptNS([userDefaultsPrefixKey copy]);

    _preferences = WebKit::WebPreferences::create(_userDefaultsPrefixKey.get(), "WebKit");
    return self;
}

- (NSString *)userDefaultsPrefixKey
{
    return _userDefaultsPrefixKey.get();
}

- (CGFloat)minimumFontSize
{
    return _preferences->minimumFontSize();
}

- (void)setMinimumFontSize:(CGFloat)minimumFontSize
{
    _preferences->setMinimumFontSize(minimumFontSize);
}

#pragma mark iOS-specific methods

#if PLATFORM(IOS)

- (BOOL)allowsInlineMediaPlayback
{
    return _preferences->mediaPlaybackAllowsInline();
}

- (void)setAllowsInlineMediaPlayback:(BOOL)allowsInlineMediaPlayback
{
    _preferences->setMediaPlaybackAllowsInline(allowsInlineMediaPlayback);
}

- (BOOL)mediaPlaybackRequiresUserAction
{
    return _preferences->mediaPlaybackRequiresUserGesture();
}

- (void)setMediaPlaybackRequiresUserAction:(BOOL)mediaPlaybackRequiresUserAction
{
    _preferences->setMediaPlaybackRequiresUserGesture(mediaPlaybackRequiresUserAction);
}

- (BOOL)mediaPlaybackAllowsAirPlay
{
    return _preferences->mediaPlaybackAllowsAirPlay();
}

- (void)setMediaPlaybackAllowsAirPlay:(BOOL)mediaPlaybackAllowsAirPlay
{
    _preferences->setMediaPlaybackAllowsAirPlay(mediaPlaybackAllowsAirPlay);
}

#endif

@end

@implementation WKPreferences (WKPrivate)

- (BOOL)_telephoneNumberDetectionIsEnabled
{
    return _preferences->telephoneNumberParsingEnabled();
}

- (void)_setTelephoneNumberDetectionIsEnabled:(BOOL)telephoneNumberDetectionIsEnabled
{
    _preferences->setTelephoneNumberParsingEnabled(telephoneNumberDetectionIsEnabled);
}

@end

#endif // WK_API_ENABLED
