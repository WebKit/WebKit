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
    RetainPtr<NSString> _userDefaultsKeyPrefix;
}

- (instancetype)init
{
    return [self initWithUserDefaultsKeyPrefix:nil];
}

- (instancetype)initWithUserDefaultsKeyPrefix:(NSString *)userDefaultsKeyPrefix
{
    if (!(self = [super init]))
        return nil;

    _userDefaultsKeyPrefix = adoptNS([userDefaultsKeyPrefix copy]);

    _preferences = WebKit::WebPreferences::create(_userDefaultsKeyPrefix.get(), "WebKit");
    return self;
}

- (NSString *)userDefaultsKeyPrefix
{
    return _userDefaultsKeyPrefix.get();
}

- (CGFloat)minimumFontSize
{
    return _preferences->minimumFontSize();
}

- (BOOL)isJavaScriptEnabled
{
    return _preferences->javaScriptEnabled();
}

- (void)setJavaScriptEnabled:(BOOL)javaScriptEnabled
{
    _preferences->setJavaScriptEnabled(javaScriptEnabled);
}

- (BOOL)javaScriptCanOpenWindowsAutomatically
{
    return _preferences->javaScriptCanOpenWindowsAutomatically();
}

- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)javaScriptCanOpenWindowsAutomatically
{
    _preferences->setJavaScriptCanOpenWindowsAutomatically(javaScriptCanOpenWindowsAutomatically);
}

- (BOOL)suppressesIncrementalRendering
{
    return _preferences->suppressesIncrementalRendering();
}

- (void)setSuppressesIncrementalRendering:(BOOL)suppressesIncrementalRendering
{
    _preferences->setSuppressesIncrementalRendering(suppressesIncrementalRendering);
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

#pragma mark OS X-specific methods

#if PLATFORM(MAC)

- (BOOL)isJavaEnabled
{
    return _preferences->javaEnabled();
}

- (void)setJavaEnabled:(BOOL)javaEnabled
{
    _preferences->setJavaEnabled(javaEnabled);
}

- (BOOL)arePlugInsEnabled
{
    return _preferences->pluginsEnabled();
}

- (void)setPlugInsEnabled:(BOOL)plugInsEnabled
{
    _preferences->setPluginsEnabled(plugInsEnabled);
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
