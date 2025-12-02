/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "_WKCaptionStyleMenuController.h"

#if PLATFORM(MAC)

#import "_WKCaptionStyleMenuControllerInternal.h"
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

#if HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER)
#import "_WKCaptionStyleMenuControllerAVKitMac.h"

#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVLegibleMediaOptionsMenuController)
#endif

using namespace WebCore;
using namespace WTF;

@interface WKCaptionStyleMenuController () <NSMenuDelegate>
@end

@implementation WKCaptionStyleMenuController

+ (instancetype)menuController
{
#if HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER)
    if (AVKitLibrary() && getAVLegibleMediaOptionsMenuControllerClassSingleton())
        return [[[_WKCaptionStyleMenuControllerAVKitMac alloc] init] autorelease];
#endif
    return [[[self alloc] init] autorelease];
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _menu = adoptNS([[NSMenu alloc] initWithTitle:@""]);
    [_menu setDelegate:self];
    self.savedActiveProfileID = CaptionUserPreferencesMediaAF::platformActiveProfileID().createNSString().get();

    [self rebuildMenu];

    return self;
}

- (void)rebuildMenu
{
    [_menu removeAllItems];
    auto profileIDs = CaptionUserPreferencesMediaAF::platformProfileIDs();

    // Add Caption Profile items
    for (const auto& profileID : profileIDs) {
        auto title = CaptionUserPreferencesMediaAF::nameForProfileID(profileID);
        RetainPtr item = adoptNS([[NSMenuItem alloc] initWithTitle:title.createNSString().get() action:@selector(profileMenuItemSelected:) keyEquivalent:@""]);
        [item setTarget:self];
        [item setRepresentedObject:profileID.createNSString().get()];

        // Add checkmark for currently selected item
        if ([profileID.createNSString().get() isEqualToString:self.savedActiveProfileID])
            [item setState:NSControlStateValueOn];

        [_menu addItem:item.get()];
    }

    if (!profileIDs.isEmpty())
        [_menu addItem:[NSMenuItem separatorItem]];

    // Add Deep-link to System Caption Settings
    RetainPtr<NSString> systemCaptionSettingsTitle = WEB_UI_NSSTRING_KEY(@"Manage Styles", @"Manage Styles (Caption Style Menu Title)", @"Subtitle Styles Submenu Link to System Preferences");
    RetainPtr systemCaptionSettingsItem = adoptNS([[NSMenuItem alloc] initWithTitle:systemCaptionSettingsTitle.get() action:@selector(systemCaptionStyleSettingsItemSelected:) keyEquivalent:@""]);
    [systemCaptionSettingsItem setTarget:self];
    [systemCaptionSettingsItem setImage:[NSImage imageWithSystemSymbolName:@"gear" accessibilityDescription:nil]];
    [_menu addItem:systemCaptionSettingsItem.get()];
}

- (BOOL)isAncestorOf:(PlatformMenu *)menu
{
    do {
        if (_menu.get() == menu)
            return true;
        menu = menu.supermenu;
    } while (menu);

    return false;
}

- (NSMenu *)captionStyleMenu
{
    return _menu.get();
}

#pragma mark - Properties

- (PlatformMenu *)menu
{
    return _menu.get();
}

- (void)setMenu:(PlatformMenu *)menu
{
    _menu = menu;
}

#pragma mark - Actions

- (void)profileMenuItemSelected:(NSMenuItem *)item
{
    if (!item)
        return;

    NSString *nsProfileID = (NSString *)item.representedObject;
    if (![nsProfileID isKindOfClass:NSString.class])
        return;

    self.savedActiveProfileID = nsProfileID;
    CaptionUserPreferencesMediaAF::setActiveProfileID(WTF::String(self.savedActiveProfileID));
    [self rebuildMenu];
}

- (void)profileMenuItemHighlighted:(NSMenuItem *)item
{
    NSString *nsProfileID = (NSString *)item.representedObject;
    if ([nsProfileID isKindOfClass:NSString.class]) {
        CaptionUserPreferencesMediaAF::setActiveProfileID(WTF::String(nsProfileID));
        return;
    }

    if (self.savedActiveProfileID && self.savedActiveProfileID.length > 0)
        CaptionUserPreferencesMediaAF::setActiveProfileID(WTF::String(self.savedActiveProfileID));
}

- (void)systemCaptionStyleSettingsItemSelected:(NSMenuItem *)sender
{
    [NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.Accessibility?Captions"]];
}

#pragma mark - NSMenuDelegate

- (void)menuWillOpen:(NSMenu *)menu
{
    self.savedActiveProfileID = CaptionUserPreferencesMediaAF::platformActiveProfileID().createNSString().get();

    if (auto delegate = self.delegate)
        [delegate captionStyleMenuWillOpen:_menu.get()];
}

- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item
{
    [self profileMenuItemHighlighted:item];
}

- (void)menuDidClose:(NSMenu *)menu
{
    if (self.savedActiveProfileID && self.savedActiveProfileID.length > 0)
        CaptionUserPreferencesMediaAF::setActiveProfileID(WTF::String(self.savedActiveProfileID));
    self.savedActiveProfileID = nil;

    if (auto delegate = self.delegate)
        [delegate captionStyleMenuDidClose:_menu.get()];
}

@end

#endif
