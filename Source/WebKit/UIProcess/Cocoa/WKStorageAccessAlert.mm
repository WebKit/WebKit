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

#if PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

#import "WKWebViewInternal.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/RegistrableDomain.h>
#import <wtf/BlockPtr.h>

namespace WebKit {

void presentStorageAccessAlert(WKWebView *webView, const WebCore::RegistrableDomain& requesting, const WebCore::RegistrableDomain& current, CompletionHandler<void(bool)>&& completionHandler)
{
    auto completionBlock = makeBlockPtr([completionHandler = WTFMove(completionHandler)](bool shouldAllow) mutable {
        completionHandler(shouldAllow);
    });

    auto requestingDomain = requesting.string().createCFString();
    auto currentDomain = current.string().createCFString();

#if PLATFORM(MAC)
    NSString *alertTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Do you want to allow “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), requestingDomain.get(), currentDomain.get()];
#else
    NSString *alertTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use cookies and website data while browsing “%@”?", @"Message for requesting cross-site cookie and website data access."), requestingDomain.get(), currentDomain.get()];
#endif

    NSString *informativeText = [NSString stringWithFormat:WEB_UI_NSSTRING(@"This will allow “%@” to track your activity.", @"Informative text for requesting cross-site cookie and website data access."), requestingDomain.get()];
    NSString *allowButtonString = WEB_UI_STRING_KEY(@"Allow", "Allow (cross-site cookie and website data access)", @"Button title in Storage Access API prompt");
    NSString *doNotAllowButtonString = WEB_UI_STRING_KEY(@"Don't Allow", "Don't Allow (cross-site cookie and website data access)", @"Button title in Storage Access API prompt");

#if PLATFORM(MAC)
    auto alert = adoptNS([NSAlert new]);
    [alert setMessageText:alertTitle];
    [alert setInformativeText:informativeText];
    [alert addButtonWithTitle:allowButtonString];
    [alert addButtonWithTitle:doNotAllowButtonString];
    [alert beginSheetModalForWindow:webView.window completionHandler:[completionBlock](NSModalResponse returnCode) {
        auto shouldAllow = returnCode == NSAlertFirstButtonReturn;
        completionBlock(shouldAllow);
    }];
#else
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:alertTitle message:informativeText preferredStyle:UIAlertControllerStyleAlert];

    UIAlertAction* allowAction = [UIAlertAction actionWithTitle:allowButtonString style:UIAlertActionStyleCancel handler:[completionBlock](UIAlertAction *action) {
        completionBlock(true);
    }];

    UIAlertAction* doNotAllowAction = [UIAlertAction actionWithTitle:doNotAllowButtonString style:UIAlertActionStyleDefault handler:[completionBlock](UIAlertAction *action) {
        completionBlock(false);
    }];

    [alert addAction:doNotAllowAction];
    [alert addAction:allowAction];

    [[UIViewController _viewControllerForFullScreenPresentationFromView:webView] presentViewController:alert animated:YES completion:nil];
#endif
}

}

#endif // PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
