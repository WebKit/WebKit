/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#import <pal/cocoa/RevealSoftLink.h>

#if PLATFORM(MAC)

#import "StringUtilities.h"
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/mac/DataDetectorsSPI.h>

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
#import <pal/spi/mac/TelephonyUtilitiesSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(TelephonyUtilities)
SOFT_LINK_CLASS(TelephonyUtilities, TUCall)
#endif

@interface WKEmptyPresenterHighlightDelegate : NSObject <RVPresenterHighlightDelegate>
@end

@implementation WKEmptyPresenterHighlightDelegate

- (NSArray <NSValue *> *)revealContext:(RVPresentingContext *)context rectsForItem:(RVItem *)item
{
    return @[ ];
}

@end

namespace WebKit {

#if ENABLE(TELEPHONE_NUMBER_DETECTION)

NSString *menuItemTitleForTelephoneNumberGroup()
{
    return [getTUCallClass() supplementalDialTelephonyCallString];
}

#if HAVE(DATA_DETECTORS_MAC_ACTION)
static DDMacAction *actionForMenuItem(NSMenuItem *item)
#else
static DDAction *actionForMenuItem(NSMenuItem *item)
#endif
{
    NSDictionary *representedObject = item.representedObject;
    if (![representedObject isKindOfClass:[NSDictionary class]])
        return nil;

    id action = [representedObject objectForKey:@"DDAction"];

#if HAVE(DATA_DETECTORS_MAC_ACTION)
    if (![action isKindOfClass:getDDMacActionClass()])
        return nil;
#else
    if (![action isKindOfClass:getDDActionClass()])
        return nil;
#endif

    return action;
}

NSMenuItem *menuItemForTelephoneNumber(const String& telephoneNumber)
{
    if (!DataDetectorsLibrary())
        return nil;

    RetainPtr<DDActionContext> actionContext = adoptNS([allocDDActionContextInstance() init]);
    [actionContext setAllowedActionUTIs:@[ @"com.apple.dial" ]];

    NSArray *proposedMenuItems = [[getDDActionsManagerClass() sharedManager] menuItemsForValue:(NSString *)telephoneNumber type:getDDBinderPhoneNumberKey() service:nil context:actionContext.get()];
    for (NSMenuItem *item in proposedMenuItems) {
        auto action = actionForMenuItem(item);
        if ([action.actionUTI hasPrefix:@"com.apple.dial"]) {
            item.title = formattedPhoneNumberString(telephoneNumber);
            return item;
        }
    }

    return nil;
}

RetainPtr<NSMenu> menuForTelephoneNumber(const String& telephoneNumber)
{
    if (!PAL::isRevealFrameworkAvailable() || !PAL::isRevealCoreFrameworkAvailable())
        return nil;

    RetainPtr<NSMenu> menu = adoptNS([[NSMenu alloc] init]);
    auto viewForPresenter = adoptNS([[NSView alloc] init]);
    auto urlComponents = adoptNS([[NSURLComponents alloc] init]);
    [urlComponents setScheme:@"tel"];
    [urlComponents setPath:telephoneNumber];
    auto item = adoptNS([PAL::allocRVItemInstance() initWithURL:[urlComponents URL] rangeInContext:NSMakeRange(0, telephoneNumber.length())]);
    auto presenter = adoptNS([PAL::allocRVPresenterInstance() init]);
    auto delegate = adoptNS([[WKEmptyPresenterHighlightDelegate alloc] init]);
    auto context = adoptNS([PAL::allocRVPresentingContextInstance() initWithPointerLocationInView:NSZeroPoint inView:viewForPresenter.get() highlightDelegate:delegate.get()]);
    NSArray *proposedMenuItems = [presenter menuItemsForItem:item.get() documentContext:nil presentingContext:context.get() options:nil];
    
    [menu setItemArray:proposedMenuItems];

    return menu;
}

#endif

} // namespace WebKit

#endif
