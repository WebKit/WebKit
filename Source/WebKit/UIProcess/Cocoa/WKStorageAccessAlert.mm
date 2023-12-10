/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "WKStorageAccessAlert.h"

#import <wtf/HashMap.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import "UIKitUtilities.h"
#endif

#if PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

#import "WKWebViewInternal.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/RegistrableDomain.h>
#import <wtf/BlockPtr.h>

namespace WebKit {

void presentStorageAccessAlert(WKWebView *webView, const WebCore::RegistrableDomain& requesting, const WebCore::RegistrableDomain& current, CompletionHandler<void(bool)>&& completionHandler)
{
    auto requestingDomain = requesting.string().createCFString();
    auto currentDomain = current.string().createCFString();

#if PLATFORM(MAC)
    NSString *alertTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Do you want to allow “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), requestingDomain.get(), currentDomain.get()];
#else
    NSString *alertTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), requestingDomain.get(), currentDomain.get()];
#endif

    NSString *informativeText = [NSString stringWithFormat:WEB_UI_NSSTRING(@"This will allow “%@” to track your activity.", @"Informative text for requesting cross-site cookie and website data access."), requestingDomain.get()];

    displayStorageAccessAlert(webView, alertTitle, informativeText, nil, WTFMove(completionHandler));
}

void presentStorageAccessAlertQuirk(WKWebView *webView, const WebCore::RegistrableDomain& firstRequesting, const WebCore::RegistrableDomain& secondRequesting, const WebCore::RegistrableDomain& current, CompletionHandler<void(bool)>&& completionHandler)
{
    auto firstRequestingDomain = firstRequesting.string().createCFString();
    auto secondRequestingDomain = secondRequesting.string().createCFString();
    auto currentDomain = current.string().createCFString();

#if PLATFORM(MAC)
    NSString *alertTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Do you want to allow “%@” and “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), firstRequestingDomain.get(), secondRequestingDomain.get(), currentDomain.get()];
#else
    NSString *alertTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” and “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), firstRequestingDomain.get(), secondRequestingDomain.get(), currentDomain.get()];
#endif

    NSString *informativeText = [NSString stringWithFormat:WEB_UI_NSSTRING(@"This will allow “%@” and “%@” to track your activity.", @"Informative text for requesting cross-site cookie and website data access."), firstRequestingDomain.get(), secondRequestingDomain.get()];
    
    displayStorageAccessAlert(webView, alertTitle, informativeText, nil, WTFMove(completionHandler));
}

void presentStorageAccessAlertSSOQuirk(WKWebView *webView, const String& organizationName, const HashMap<WebCore::RegistrableDomain, Vector<WebCore::RegistrableDomain>>& domainPairings, CompletionHandler<void(bool)>&& completionHandler)
{
    NSString *alertTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Are you logging in to this website, and do you want to allow other %@ sites access to your website data while browsing the websites listed below?", @"Message for requesting cross-site cookie and website data access."), organizationName.createCFString().get()];
    NSString *informativeText = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allowing access to website data is necessary for the website to work correctly.", @"Informative text for requesting cross-site cookie and website data access.")];

    auto* accessoryTextList = [NSMutableArray arrayWithCapacity:domainPairings.size()];
    for (const auto& domains : domainPairings) {
        auto embeddedDomains = makeStringByJoining(domains.value.map([](auto& domain) { return domain.string(); }).span(), "\n  - "_s);
        NSString *accessoryText = [NSString stringWithFormat:WEB_UI_NSSTRING(@"While you are visiting %@, the following related websites will gain access to their cookies:\n  - %@", @"Accessory text for requesting cross-site cookie and website data access."), domains.key.string().createCFString().get(), embeddedDomains.createCFString().get()];
        [accessoryTextList addObject:accessoryText];
    }
    displayStorageAccessAlert(webView, alertTitle, informativeText, accessoryTextList, WTFMove(completionHandler));
}

void displayStorageAccessAlert(WKWebView *webView, NSString *alertTitle, NSString *informativeText, NSArray<NSString *> *accessoryTextList, CompletionHandler<void(bool)>&& completionHandler)
{
    auto completionBlock = makeBlockPtr([completionHandler = WTFMove(completionHandler)](bool shouldAllow) mutable {
        completionHandler(shouldAllow);
    });

    NSString *allowButtonString = WEB_UI_STRING_KEY(@"Allow", "Allow (cross-site cookie and website data access)", @"Button title in Storage Access API prompt");
    NSString *doNotAllowButtonString = WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (cross-site cookie and website data access)", @"Button title in Storage Access API prompt");

#if PLATFORM(MAC)
    auto alert = adoptNS([NSAlert new]);
    [alert setMessageText:alertTitle];
    [alert setInformativeText:informativeText];
    if (accessoryTextList) {
        NSTextView *accessory = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 200, 15)];
        auto mutableString = [[accessory textStorage] mutableString];
        [mutableString setString:[accessoryTextList componentsJoinedByString:@"\n"]];
        [[accessory textStorage] setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
        [[accessory textStorage] setForegroundColor:NSColor.whiteColor];
        [accessory setEditable:NO];
        [accessory setDrawsBackground:NO];
        [alert setAccessoryView:accessory];
        [alert layout];
    }
    [alert addButtonWithTitle:allowButtonString];
    [alert addButtonWithTitle:doNotAllowButtonString];
    [alert beginSheetModalForWindow:webView.window completionHandler:[completionBlock](NSModalResponse returnCode) {
        auto shouldAllow = returnCode == NSAlertFirstButtonReturn;
        completionBlock(shouldAllow);
    }];
#else
    auto alert = WebKit::createUIAlertController(alertTitle, informativeText);

    if (accessoryTextList) {
        [accessoryTextList enumerateObjectsUsingBlock:^(NSString *line, NSUInteger index, BOOL *stop) {
            [alert addTextFieldWithConfigurationHandler:[&line](UITextField *textField) {
                textField.text = line;
            }];
        }];
    }

    UIAlertAction* allowAction = [UIAlertAction actionWithTitle:allowButtonString style:UIAlertActionStyleCancel handler:[completionBlock](UIAlertAction *action) {
        completionBlock(true);
    }];

    UIAlertAction* doNotAllowAction = [UIAlertAction actionWithTitle:doNotAllowButtonString style:UIAlertActionStyleDefault handler:[completionBlock](UIAlertAction *action) {
        completionBlock(false);
    }];

    [alert addAction:doNotAllowAction];
    [alert addAction:allowAction];

    [webView._wk_viewControllerForFullScreenPresentation presentViewController:alert.get() animated:YES completion:nil];
#endif
}

}

#endif // PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
