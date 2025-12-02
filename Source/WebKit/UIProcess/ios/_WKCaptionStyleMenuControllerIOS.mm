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

#if PLATFORM(IOS_FAMILY)

#import "_WKCaptionStyleMenuControllerInternal.h"
#import <UIKit/UIKit.h>
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

#if HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER)
#import "_WKCaptionStyleMenuControllerAVKit.h"

#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/cf/CoreMediaSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVLegibleMediaOptionsMenuController)
#endif

using namespace WebCore;
using namespace WTF;

static const UIMenuIdentifier WKCaptionStyleMenuIdentifier = @"WKCaptionStyleMenuIdentifier";
static const UIMenuIdentifier WKCaptionStyleMenuProfileGroupIdentifier = @"WKCaptionStyleMenuProfileGroupIdentifier";
static const UIMenuIdentifier WKCaptionStyleMenuProfileIdentifier = @"WKCaptionStyleMenuProfileIdentifier";
static const UIMenuIdentifier WKCaptionStyleMenuSystemSettingsIdentifier = @"WKCaptionStyleMenuSystemSettingsIdentifier";

#if USE(UICONTEXTMENU)
@interface WKCaptionStyleMenuController () <UIContextMenuInteractionDelegate>
@end
#else
@interface WKCaptionStyleMenuController ()
@end
#endif

@implementation WKCaptionStyleMenuController

+ (instancetype)menuController
{
#if HAVE(AVLEGIBLEMEDIAOPTIONSMENUCONTROLLER)
    if (AVKitLibrary() && getAVLegibleMediaOptionsMenuControllerClassSingleton())
        return [[_WKCaptionStyleMenuControllerAVKit alloc] init];
#endif
    return [[super alloc] init];
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    self.savedActiveProfileID = CaptionUserPreferencesMediaAF::platformActiveProfileID().createNSString().autorelease();
    [self rebuildMenu];

    return self;
}

- (void)rebuildMenu
{
    RetainPtr stylesMenuTitle = WEB_UI_NSSTRING_KEY(@"Styles", @"Styles (Media Controls Menu)", @"Subtitles media controls menu title");

    auto profileIDs = CaptionUserPreferencesMediaAF::platformProfileIDs();
    if (profileIDs.isEmpty()) {
        self.menu = [UIMenu menuWithTitle:stylesMenuTitle.get() children:@[]];
        return;
    }

    NSMutableArray<UIMenuElement *> *profileElements = [NSMutableArray array];
    for (const auto& profileID : profileIDs) {
        auto title = CaptionUserPreferencesMediaAF::nameForProfileID(profileID);

        auto profileSelectedHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKCaptionStyleMenuController>(self), profileID = profileID] (UIAction *) {
            if (auto strongSelf = weakSelf.get())
                [strongSelf profileActionSelected:profileID];
        });
        UIAction *profileAction = [UIAction actionWithTitle:title.createNSString().get() image:nil identifier:profileID.createNSString().get() handler:profileSelectedHandler.get()];
        profileAction.attributes = UIMenuElementAttributesKeepsMenuPresented;

        if ([profileID.createNSString().autorelease() isEqualToString:self.savedActiveProfileID])
            profileAction.state = UIMenuElementStateOn;

        [profileElements addObject:profileAction];
    }
    auto *profileGroup = [UIMenu menuWithTitle:@"" image:nil identifier:WKCaptionStyleMenuProfileGroupIdentifier options:UIMenuOptionsDisplayInline children:profileElements];

    RetainPtr systemCaptionSettingsTitle = WEB_UI_NSSTRING_KEY(@"Manage Styles", @"Manage Styles (Caption Style Menu Title)", @"Subtitle Styles Submenu Link to System Preferences");

    auto systemSettingsSelectedHandler = makeBlockPtr([weakSelf = WeakObjCPtr<WKCaptionStyleMenuController>(self)] (UIAction *action) {
        if (auto strongSelf = weakSelf.get())
            [strongSelf systemCaptionStyleSettingsActionSelected:action];
    });

    UIImage *gearImage = [UIImage systemImageNamed:@"gear"];
    UIAction *systemSettingsAction = [UIAction actionWithTitle:systemCaptionSettingsTitle.get() image:gearImage identifier:WKCaptionStyleMenuSystemSettingsIdentifier handler:systemSettingsSelectedHandler.get()];

    self.menu = [UIMenu menuWithTitle:stylesMenuTitle.get() children:@[profileGroup, systemSettingsAction]];
}

- (BOOL)isAncestorOf:(PlatformMenu*)menu
{
    return [menu.identifier isEqualToString:WKCaptionStyleMenuIdentifier]
        || [menu.identifier isEqualToString:WKCaptionStyleMenuProfileGroupIdentifier]
        || [menu.identifier isEqualToString:WKCaptionStyleMenuProfileIdentifier]
        || [menu.identifier isEqualToString:WKCaptionStyleMenuSystemSettingsIdentifier];
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

#if !TARGET_OS_OSX && !TARGET_OS_WATCH
- (UIContextMenuInteraction *)interaction
{
    return _interaction.get();
}

- (void)setInteraction:(UIContextMenuInteraction *)interaction
{
    _interaction = interaction;
}
#endif

- (UIMenu *)captionStyleMenu
{
    return _menu.get();
}

#if USE(UICONTEXTMENU)
- (UIContextMenuInteraction *)contextMenuInteraction
{
    if (!_interaction)
        _interaction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
    return _interaction.get();
}
#endif

#pragma mark - Actions

- (void)profileActionSelected:(const WTF::String&)profileID
{
    for (UIMenuElement *childMenu in [_menu children]) {
        auto *childAction = dynamic_objc_cast<UIAction>(childMenu);
        if (!childAction)
            continue;

        String childActionIdentifier = childAction.identifier;
        childAction.state = profileID == childActionIdentifier ? UIMenuElementStateOn : UIMenuElementStateOff;
    }

    self.savedActiveProfileID = profileID.createNSString().autorelease();
    CaptionUserPreferencesMediaAF::setActiveProfileID(WTF::String(self.savedActiveProfileID));

    // UIMenu does not have the ability to notify clients when a submenu opens or closes.
    // Provide a similar functionality for previewing subtitle changes by triggering the
    // preview of subtitle styles when the first profile menu item is selected.
    [self notifyMenuWillOpen];
}

- (void)systemCaptionStyleSettingsActionSelected:(UIAction *)action
{
    NSURL *settingsURL = [NSURL URLWithString:@"App-prefs:ACCESSIBILITY"];
    if ([[UIApplication sharedApplication] canOpenURL:settingsURL])
        [[UIApplication sharedApplication] openURL:settingsURL options:@{ } completionHandler:nil];
}

- (void)notifyMenuWillOpen
{
    self.savedActiveProfileID = CaptionUserPreferencesMediaAF::platformActiveProfileID().createNSString().autorelease();

    if (auto delegate = self.delegate)
        [delegate captionStyleMenuWillOpen:self.menu];
}

- (void)notifyMenuDidClose
{
    if (self.savedActiveProfileID && self.savedActiveProfileID.length > 0)
        CaptionUserPreferencesMediaAF::setActiveProfileID(WTF::String(self.savedActiveProfileID));
    self.savedActiveProfileID = nil;

    if (auto delegate = self.delegate)
        [delegate captionStyleMenuDidClose:self.menu];
}

#if USE(UICONTEXTMENU)
- (nullable UIContextMenuConfiguration *)contextMenuInteraction:(nonnull UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location {
    return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:makeBlockPtr([weakSelf = WeakObjCPtr<WKCaptionStyleMenuController>(self)] (NSArray<UIMenuElement *> *) -> UIMenu * {
        if (auto strongSelf = weakSelf.get())
            return strongSelf.get().menu;
        return nil;
    }).get()];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration animator:(nullable id<UIContextMenuInteractionAnimating>)animator
{
    [self notifyMenuWillOpen];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(nullable id<UIContextMenuInteractionAnimating>)animator
{
    [self notifyMenuDidClose];
}
#endif // USE(UICONTEXTMENU)
@end

#endif
