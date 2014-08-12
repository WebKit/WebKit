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
#import "MenuUtilities.h"

#import "StringUtilities.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SoftLinking.h>
#import <objc/runtime.h>

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
SOFT_LINK_PRIVATE_FRAMEWORK(DataDetectors)
SOFT_LINK_CLASS(DataDetectors, DDAction)
SOFT_LINK_CLASS(DataDetectors, DDActionsManager)
SOFT_LINK_CONSTANT(DataDetectors, DDBinderPhoneNumberKey, CFStringRef)

@interface DDAction : NSObject
@property (readonly) NSString *actionUTI;
@end

typedef void* DDActionContext;

@interface DDActionsManager : NSObject
+ (DDActionsManager *)sharedManager;
- (NSArray *)menuItemsForValue:(NSString *)value type:(CFStringRef)type service:(NSString *)service context:(DDActionContext *)context;
@end
#endif

namespace WebKit {

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

static NSString *menuItemTitleForTelephoneNumber(const String& telephoneNumber)
{
    return [NSString stringWithFormat:WEB_UI_STRING("Call “%@” Using iPhone", "menu item for making a telephone call to a telephone number"), formattedPhoneNumberString(telephoneNumber)];
}

NSString *menuItemTitleForTelephoneNumberGroup()
{
    return WEB_UI_STRING("Call Using iPhone:", "menu item title for phone number");
}

NSMenuItem *menuItemForTelephoneNumber(const String& telephoneNumber)
{
    NSArray *proposedMenu = [[getDDActionsManagerClass() sharedManager] menuItemsForValue:(NSString *)telephoneNumber type:getDDBinderPhoneNumberKey() service:nil context:nil];
    for (NSMenuItem *item in proposedMenu) {
        NSDictionary *representedObject = item.representedObject;
        if (![representedObject isKindOfClass:[NSDictionary class]])
            continue;

        DDAction *actionObject = [representedObject objectForKey:@"DDAction"];
        if (![actionObject isKindOfClass:getDDActionClass()])
            continue;

        if ([actionObject.actionUTI hasPrefix:@"com.apple.dial"]) {
            item.title = formattedPhoneNumberString(telephoneNumber);
            return item;
        }
    }

    return nil;
}

NSArray *menuItemsForTelephoneNumber(const String& telephoneNumber)
{
    NSMutableArray *items = [NSMutableArray array];
    NSMutableArray *faceTimeItems = [NSMutableArray array];
    NSMenuItem *dialItem = nil;

    NSArray *proposedMenu = [[getDDActionsManagerClass() sharedManager] menuItemsForValue:(NSString *)telephoneNumber type:getDDBinderPhoneNumberKey() service:nil context:nil];
    for (NSMenuItem *item in proposedMenu) {
        NSDictionary *representedObject = item.representedObject;
        if (![representedObject isKindOfClass:[NSDictionary class]])
            continue;

        DDAction *actionObject = [representedObject objectForKey:@"DDAction"];
        if (![actionObject isKindOfClass:getDDActionClass()])
            continue;

        if ([actionObject.actionUTI hasPrefix:@"com.apple.dial"]) {
            item.title = menuItemTitleForTelephoneNumber(telephoneNumber);
            dialItem = item;
            continue;
        }

        if ([actionObject.actionUTI hasPrefix:@"com.apple.facetime"])
            [faceTimeItems addObject:item];
    }

    if (dialItem)
        [items addObject:dialItem];

    if (faceTimeItems.count) {
        if (items.count)
            [items addObject:[NSMenuItem separatorItem]];
        [items addObjectsFromArray:faceTimeItems];
    }

    return items.count ? items : nil;
}
#endif

} // namespace WebKit
