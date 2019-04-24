/*
 * Copyright (C) 2011, 2018 Apple Inc. All rights reserved.
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
#import "WKFullKeyboardAccessWatcher.h"

#if ENABLE(FULL_KEYBOARD_ACCESS)

#import "AccessibilitySupportSPI.h"
#import "WebProcessPool.h"

#if PLATFORM(MAC)
static NSString * const KeyboardUIModeDidChangeNotification = @"com.apple.KeyboardUIModeDidChange";
static const CFStringRef AppleKeyboardUIMode = CFSTR("AppleKeyboardUIMode");
#endif

@implementation WKFullKeyboardAccessWatcher

static inline BOOL platformIsFullKeyboardAccessEnabled()
{
    BOOL fullKeyboardAccessEnabled = NO;
#if PLATFORM(MAC)
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
    
    Boolean keyExistsAndHasValidFormat;
    int mode = CFPreferencesGetAppIntegerValue(AppleKeyboardUIMode, kCFPreferencesCurrentApplication, &keyExistsAndHasValidFormat);
    if (keyExistsAndHasValidFormat) {
        // The keyboard access mode has two bits:
        // Bit 0 is set if user can set the focus to menus, the dock, and various windows using the keyboard.
        // Bit 1 is set if controls other than text fields are included in the tab order (WebKit also always includes lists).
        fullKeyboardAccessEnabled = (mode & 0x2);
    }
#elif PLATFORM(IOS_FAMILY)
    fullKeyboardAccessEnabled = _AXSFullKeyboardAccessEnabled();
#endif
    
    return fullKeyboardAccessEnabled;
}

- (void)notifyAllProcessPools
{
    for (auto* processPool : WebKit::WebProcessPool::allProcessPools())
        processPool->fullKeyboardAccessModeChanged(fullKeyboardAccessEnabled);
}

- (void)retrieveKeyboardUIModeFromPreferences:(NSNotification *)notification
{
    BOOL oldValue = fullKeyboardAccessEnabled;

    fullKeyboardAccessEnabled = platformIsFullKeyboardAccessEnabled();
    
    if (fullKeyboardAccessEnabled != oldValue)
        [self notifyAllProcessPools];
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;

    [self retrieveKeyboardUIModeFromPreferences:nil];

    NSNotificationCenter *notificationCenter = nil;
    NSString *notitificationName = nil;
    
#if PLATFORM(MAC)
    notificationCenter = [NSDistributedNotificationCenter defaultCenter];
    notitificationName = KeyboardUIModeDidChangeNotification;
#elif PLATFORM(IOS_FAMILY)
    notificationCenter = [NSNotificationCenter defaultCenter];
    notitificationName = (NSString *)kAXSFullKeyboardAccessEnabledNotification;
#endif
    
    if (notitificationName)
        [notificationCenter addObserver:self selector:@selector(retrieveKeyboardUIModeFromPreferences:) name:notitificationName object:nil];

    return self;
}

+ (BOOL)fullKeyboardAccessEnabled
{
    static WKFullKeyboardAccessWatcher *watcher = [[WKFullKeyboardAccessWatcher alloc] init];
    return watcher->fullKeyboardAccessEnabled;
}

@end

#endif // ENABLE(FULL_KEYBOARD_ACCESS)
