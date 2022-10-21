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
    auto key = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, WKLockdownModeEnabledKey.characters(), kCFStringEncodingUTF8));
    auto preferenceValue = adoptCF(CFPreferencesCopyValue(key.get(), kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
    if (preferenceValue.get() == kCFBooleanTrue)
        return true;

    key = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, LDMEnabledKey, kCFStringEncodingUTF8));
    preferenceValue = adoptCF(CFPreferencesCopyValue(key.get(), kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
    return preferenceValue.get() == kCFBooleanTrue;
}

+ (void)setCaptivePortalModeEnabled:(BOOL)enabled
{
    auto key = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, WKLockdownModeEnabledKey.characters(), kCFStringEncodingUTF8));
    CFPreferencesSetValue(key.get(), enabled ? kCFBooleanTrue : kCFBooleanFalse, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge CFStringRef)WKLockdownModeContainerConfigurationChangedNotification, nullptr, nullptr, true);
}

+ (BOOL)isCaptivePortalModeIgnored:(NSString *)containerPath
{
#if PLATFORM(IOS_FAMILY)
    NSString *cpmconfigIgnoreFilePath = [NSString pathWithComponents:@[containerPath, @"System/Preferences/", CaptivePortalConfigurationIgnoreFileName]];
    return [[NSFileManager defaultManager] fileExistsAtPath:cpmconfigIgnoreFilePath];
#endif
    return false;
}

+ (void)setCaptivePortalModeIgnored:(NSString *)containerPath ignore:(BOOL)ignore
{
#if PLATFORM(IOS_FAMILY)
    NSString *cpmconfigDirectoryPath = [NSString pathWithComponents:@[containerPath, @"System/Preferences/"]];
    NSString *cpmconfigIgnoreFilePath = [NSString pathWithComponents:@[cpmconfigDirectoryPath, CaptivePortalConfigurationIgnoreFileName]];
    if ([[NSFileManager defaultManager] fileExistsAtPath:cpmconfigIgnoreFilePath] == ignore)
        return;

    BOOL cpmconfigDirectoryisDir;
    BOOL cpmconfigDirectoryPathExists = [[NSFileManager defaultManager] fileExistsAtPath:cpmconfigDirectoryPath isDirectory:&cpmconfigDirectoryisDir];

    if (!cpmconfigDirectoryisDir)
        [[NSFileManager defaultManager] removeItemAtPath:cpmconfigDirectoryPath error:NULL];

    if (!cpmconfigDirectoryPathExists || !cpmconfigDirectoryisDir)
        [[NSFileManager defaultManager] createDirectoryAtPath:cpmconfigDirectoryPath withIntermediateDirectories:YES attributes:NULL error:NULL];

    if (ignore)
        [[NSFileManager defaultManager] createFileAtPath:cpmconfigIgnoreFilePath contents:NULL attributes:NULL];
    else
        [[NSFileManager defaultManager] removeItemAtPath:cpmconfigIgnoreFilePath error:NULL];

    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge CFStringRef)WKLockdownModeContainerConfigurationChangedNotification, nullptr, nullptr, true);
#endif
}

@end
