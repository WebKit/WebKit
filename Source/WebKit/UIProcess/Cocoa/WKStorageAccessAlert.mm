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
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/RegistrableDomain.h>
#import <ranges>
#import <wtf/BlockPtr.h>

#if PLATFORM(MAC)
@interface _WKSSOSiteList : NSObject<NSTableViewDataSource, NSTableViewDelegate>
- (instancetype)initWithSiteList:(NSArray<NSString *> *)siteList withAlert:(RetainPtr<NSAlert>)alert withTableView:(RetainPtr<NSTableView>)tableView;
- (IBAction)toggleTableViewContents:(NSButton *)sender;
@end

@implementation _WKSSOSiteList {
    RetainPtr<NSArray<NSString *>> _siteList;
    RetainPtr<NSAlert> _alert;
    RetainPtr<NSTableView> _tableView;
}

- (instancetype)initWithSiteList:(NSArray<NSString *> *)siteList withAlert:(RetainPtr<NSAlert>)alert withTableView:(RetainPtr<NSTableView>)tableView
{
    if (!(self = [super init]))
        return self;

    _siteList = adoptNS([[NSArray alloc] initWithArray:siteList copyItems:YES]);
    _alert = alert;
    _tableView = tableView;
    return self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return _siteList.get().count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    RetainPtr viewIdenfier = adoptNS([[NSString alloc] initWithFormat:@"row%ldIdentifier", (long)row]);
    RetainPtr result = [tableView makeViewWithIdentifier:viewIdenfier.get() owner:self];
    if (!result) {
        result = adoptNS([[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 100, 300)]);
        result.get().identifier = viewIdenfier.get();
    }
    [result setEditable:NO];
    [result setDrawsBackground:NO];

    if ((NSUInteger)row < [_siteList count]) {
        [retainPtr([result textStorage].mutableString) setString:retainPtr(_siteList.get()[row]).get()];
        [result textStorage].font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
        [result textStorage].foregroundColor = NSColor.whiteColor;
    }
    return result.autorelease();
}

- (IBAction)toggleTableViewContents:(NSButton *)sender
{
    _tableView.get().hidden = (sender.state != NSControlStateValueOn);
    NSRect frame = _alert.get().window.frame;
    if (_tableView.get().hidden)
        frame.size.height -= _tableView.get().frame.size.height;
    else
        frame.size.height += _tableView.get().frame.size.height;
    [retainPtr(_alert.get().window) setFrame:frame display:YES];
    [_alert layout];
}
@end
#endif

namespace WebKit {

void presentStorageAccessAlert(WKWebView *webView, const WebCore::RegistrableDomain& requesting, const WebCore::RegistrableDomain& current, CompletionHandler<void(bool)>&& completionHandler)
{
    auto requestingDomain = requesting.string().createCFString();
    auto currentDomain = current.string().createCFString();

#if PLATFORM(MAC)
    SUPPRESS_UNRETAINED_ARG RetainPtr alertTitle = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Do you want to allow “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), requestingDomain.get(), currentDomain.get()]);
#else
    SUPPRESS_UNRETAINED_ARG RetainPtr alertTitle = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), requestingDomain.get(), currentDomain.get()]);
#endif

    SUPPRESS_UNRETAINED_ARG RetainPtr informativeText = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"This will allow “%@” to track your activity.", @"Informative text for requesting cross-site cookie and website data access."), requestingDomain.get()]);

    displayStorageAccessAlert(webView, alertTitle.get(), informativeText.get(), nil, nil, WTF::move(completionHandler));
}

void presentStorageAccessAlertQuirk(WKWebView *webView, const WebCore::RegistrableDomain& firstRequesting, const WebCore::RegistrableDomain& secondRequesting, const WebCore::RegistrableDomain& current, CompletionHandler<void(bool)>&& completionHandler)
{
    RetainPtr firstRequestingDomain = firstRequesting.string().createCFString();
    RetainPtr secondRequestingDomain = secondRequesting.string().createCFString();
    RetainPtr currentDomain = current.string().createCFString();

#if PLATFORM(MAC)
    SUPPRESS_UNRETAINED_ARG RetainPtr alertTitle = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Do you want to allow “%@” and “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), firstRequestingDomain.get(), secondRequestingDomain.get(), currentDomain.get()]);
#else
    SUPPRESS_UNRETAINED_ARG RetainPtr alertTitle = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” and “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), firstRequestingDomain.get(), secondRequestingDomain.get(), currentDomain.get()]);
#endif

    SUPPRESS_UNRETAINED_ARG RetainPtr informativeText = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"This will allow “%@” and “%@” to track your activity.", @"Informative text for requesting cross-site cookie and website data access."), firstRequestingDomain.get(), secondRequestingDomain.get()]);
    displayStorageAccessAlert(webView, alertTitle.get(), informativeText.get(), nil, nil, WTF::move(completionHandler));
}

void presentStorageAccessAlertSSOQuirk(WKWebView *webView, const String& organizationName, const HashMap<WebCore::RegistrableDomain, Vector<WebCore::RegistrableDomain>>& domainPairings, CompletionHandler<void(bool)>&& completionHandler)
{
    SUPPRESS_UNRETAINED_ARG RetainPtr alertTitle = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow related %@ websites to share cookies and website data?", @"Message for requesting cross-site cookie and website data access."), organizationName.createCFString().get()]);

    RetainPtr<NSString> informativeText;;
    RetainPtr<NSString> relatedWebsitesString;
    RetainPtr<NSMutableArray<NSString *>> accessoryTextList;

    HashSet<String> allDomains;
    for (auto&& domains : domainPairings) {
        allDomains.add(domains.key.string());
        for (auto&& subFrameDomain : domains.value)
            allDomains.add(subFrameDomain.string());
    }

    if (allDomains.size() < 2)  {
        completionHandler(true);
        return;
    }

    Vector<String> uniqueDomainList = copyToVector(allDomains);
    std::ranges::sort(uniqueDomainList, WTF::codePointCompareLessThan);

    if (uniqueDomainList.size() < 4) {
        auto lastSite = uniqueDomainList.takeLast();
        StringBuilder initialListOfSites;
        initialListOfSites.append(makeStringByJoining(uniqueDomainList.span(), ", "_s));
        if (uniqueDomainList.size() == 2)
            initialListOfSites.append(","_s);

        SUPPRESS_UNRETAINED_ARG informativeText = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Using the same cookies and website data is required for %s and %s to work correctly, but could make it easier to track your browsing across these websites.", @"Informative text for requesting cross-site cookie and website data access for two sites"), initialListOfSites.toString().utf8().data(), lastSite.utf8().data()]);
        relatedWebsitesString = nil;
        accessoryTextList = nil;
    } else {
        SUPPRESS_UNRETAINED_ARG informativeText = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Using the same cookies and website data is required for %s, %s, and %lu other websites to work correctly, but could make it easier to track your browsing across these websites.", @"Informative text for requesting cross-site cookie and website data access for four or more sites."), uniqueDomainList[0].utf8().data(), uniqueDomainList[1].utf8().data(), uniqueDomainList.size() - 2]);

        SUPPRESS_UNRETAINED_ARG relatedWebsitesString = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Related %@ websites", @"Label describing the list of related websites controlled by the same organization"), organizationName.createCFString().get()]);
        accessoryTextList = adoptNS([[NSMutableArray alloc] initWithCapacity:uniqueDomainList.size()]);
        for (const auto& domains : uniqueDomainList)
            [accessoryTextList addObject:domains.createNSString().get()];
    }

    displayStorageAccessAlert(webView, alertTitle.get(), informativeText.get(), relatedWebsitesString.get(), accessoryTextList.get(), WTF::move(completionHandler));
}

void displayStorageAccessAlert(WKWebView *webView, NSString *alertTitle, NSString *informativeText, NSString *accessoryLabel, NSArray<NSString *> *accessoryTextList, CompletionHandler<void(bool)>&& completionHandler)
{
    auto completionBlock = makeBlockPtr([completionHandler = WTF::move(completionHandler)](bool shouldAllow) mutable {
        completionHandler(shouldAllow);
    });

    RetainPtr allowButtonString = WEB_UI_STRING_KEY(@"Allow", "Allow (cross-site cookie and website data access)", @"Button title in Storage Access API prompt").createNSString();
    RetainPtr doNotAllowButtonString = WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (cross-site cookie and website data access)", @"Button title in Storage Access API prompt").createNSString();

#if PLATFORM(MAC)
    auto alert = adoptNS([NSAlert new]);
    [alert setMessageText:alertTitle];
    [alert setInformativeText:informativeText];
    RetainPtr<_WKSSOSiteList> ssoSiteList;
    if (accessoryTextList.count) {
        RetainPtr disclosureButton = adoptNS([[NSButton alloc] init]);
        disclosureButton.get().title = @"";
        disclosureButton.get().bezelStyle = NSBezelStyleDisclosure;
        [disclosureButton setButtonType:NSButtonTypePushOnPushOff];

        RetainPtr relatedWebsitesLabel = [NSTextField labelWithString:accessoryLabel];
        RetainPtr disclosureStackView = [NSStackView stackViewWithViews:@[disclosureButton.get(), relatedWebsitesLabel.get()]];

        RetainPtr siteListTableView = adoptNS([[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 1000)]);
        siteListTableView.get().allowsTypeSelect = NO;
        siteListTableView.get().enabled = NO;
        siteListTableView.get().usesSingleLineMode = YES;
        siteListTableView.get().hidden = YES;
        [siteListTableView addTableColumn:adoptNS([[NSTableColumn alloc] initWithIdentifier:@"columnIdentifier"]).get()];

        ssoSiteList = adoptNS([[_WKSSOSiteList alloc] initWithSiteList:accessoryTextList withAlert:alert withTableView:siteListTableView]);
        siteListTableView.get().dataSource = ssoSiteList.get();
        siteListTableView.get().delegate = ssoSiteList.get();

        RetainPtr accessoryStackView = [NSStackView stackViewWithViews:@[disclosureStackView.get(), siteListTableView.get()]];
        accessoryStackView.get().orientation = NSUserInterfaceLayoutOrientationVertical;
        disclosureButton.get().target = ssoSiteList.get();
        disclosureButton.get().action = @selector(toggleTableViewContents:);

        NSRect frame = alert.get().window.frame;
        frame.size.width += 100.;
        [retainPtr(alert.get().window) setFrame:frame display:YES];

        [alert setAccessoryView:accessoryStackView.get()];
        [alert layout];
    }
    [alert addButtonWithTitle:allowButtonString.get()];
    [alert addButtonWithTitle:doNotAllowButtonString.get()];
    [alert beginSheetModalForWindow:retainPtr(webView.window).get() completionHandler:makeBlockPtr([ssoSiteList, completionBlock](NSModalResponse returnCode) {
        auto shouldAllow = returnCode == NSAlertFirstButtonReturn;
        completionBlock(shouldAllow);
    }).get()];
#else
    auto alert = WebKit::createUIAlertController(alertTitle, informativeText);

    RetainPtr allowAction = [UIAlertAction actionWithTitle:allowButtonString.get() style:UIAlertActionStyleCancel handler:[completionBlock](UIAlertAction *action) {
        completionBlock(true);
    }];

    RetainPtr doNotAllowAction = [UIAlertAction actionWithTitle:doNotAllowButtonString.get() style:UIAlertActionStyleDefault handler:[completionBlock](UIAlertAction *action) {
        completionBlock(false);
    }];

    [alert addAction:doNotAllowAction.get()];
    [alert addAction:allowAction.get()];

#if PLATFORM(VISION)
    [webView _page]->dispatchWillPresentModalUI();
#endif
    [webView._wk_viewControllerForFullScreenPresentation presentViewController:alert.get() animated:YES completion:nil];
#endif
}

}

#endif // PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
