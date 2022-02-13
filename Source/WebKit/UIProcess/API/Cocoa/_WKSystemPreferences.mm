/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "_WKSystemPreferencesInternal.h"

#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>

constexpr auto CaptivePortalConfigurationIgnoreFileName = @"com.apple.WebKit.cpmconfig_ignore";

@implementation _WKSystemPreferences

+ (BOOL)isCaptivePortalModeEnabled
{
    auto changedNotificationKey = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, WKCaptivePortalModeEnabledKey, kCFStringEncodingUTF8));
    auto preferenceValue = adoptCF(CFPreferencesCopyValue(changedNotificationKey.get(), kCFPreferencesAnyApplication, kCFPreferencesAnyUser, kCFPreferencesCurrentHost));
    return preferenceValue.get() == kCFBooleanTrue;
}

+ (void)setCaptivePortalModeEnabled:(BOOL)enabled
{
    auto changedNotificationKey = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, WKCaptivePortalModeEnabledKey, kCFStringEncodingUTF8));
    CFPreferencesSetValue(changedNotificationKey.get(), enabled ? kCFBooleanTrue : kCFBooleanFalse, kCFPreferencesAnyApplication, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
}

+ (BOOL)isCaptivePortalModeIgnored:(NSString *)containerPath
{
#if PLATFORM(IOS_FAMILY)
    NSString *cpmconfigIgnoreFilePath = [NSString pathWithComponents:@[containerPath, @"Library/Preferences/", CaptivePortalConfigurationIgnoreFileName]];
    return [[NSFileManager defaultManager] fileExistsAtPath:cpmconfigIgnoreFilePath];
#endif
    return false;
}

+ (void)setCaptivePortalModeIgnored:(NSString *)containerPath ignore:(BOOL)ignore
{
#if PLATFORM(IOS_FAMILY)
    NSString *cpmconfigIgnoreFilePath = [NSString pathWithComponents:@[containerPath, @"Library/Preferences/", CaptivePortalConfigurationIgnoreFileName]];
    if ([[NSFileManager defaultManager] fileExistsAtPath:cpmconfigIgnoreFilePath] == ignore)
        return;
    
    if (ignore)
        [[NSFileManager defaultManager] createFileAtPath:cpmconfigIgnoreFilePath contents:NULL attributes:NULL];
    else
        [[NSFileManager defaultManager] removeItemAtPath:cpmconfigIgnoreFilePath error:NULL];

    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge CFStringRef)WKCaptivePortalModeContainerConfigurationChangedNotification, nullptr, nullptr, true);
#endif
}

@end
