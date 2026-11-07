/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKLocalNetworkAccessAlert.h"

#if PLATFORM(COCOA) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <WebCore/IPAddressSpace.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SecurityOriginData.h>
#import <wtf/BlockPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitUtilities.h"
#endif

namespace WebKit {

void presentLocalNetworkAccessAlert(WKWebView *webView, const WebCore::SecurityOriginData& requestingOrigin, const WebCore::SecurityOriginData& topOrigin, WebCore::IPAddressSpace addressSpace, CompletionHandler<void(bool)>&& completionHandler)
{
    RetainPtr requestingHost = requestingOrigin.host().createNSString();
    RetainPtr topHost = topOrigin.host().createNSString();

    RetainPtr<NSString> alertTitle;
    if (addressSpace == WebCore::IPAddressSpace::Loopback)
        SUPPRESS_UNRETAINED_ARG alertTitle = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to connect to software on this device?", @"Title of the prompt asking whether a website may make requests to software running on this device."), requestingHost.get()]);
    else
        SUPPRESS_UNRETAINED_ARG alertTitle = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to connect to devices on your local network?", @"Title of the prompt asking whether a website may make requests to devices on the local network."), requestingHost.get()]);

    SUPPRESS_UNRETAINED_ARG RetainPtr informativeText = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"While you are visiting “%@”, this website could reach devices and software that are not meant to be reachable from the internet.", @"Informative text in the prompt asking whether a website may make local network requests."), topHost.get()]);

    RetainPtr allowButtonString = WEB_UI_STRING_KEY(@"Allow", "Allow (local network access)", @"Button title in the local network access prompt").createNSString();
    RetainPtr doNotAllowButtonString = WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (local network access)", @"Button title in the local network access prompt").createNSString();

    auto completionBlock = makeBlockPtr([completionHandler = WTF::move(completionHandler)](bool allowed) mutable {
        completionHandler(allowed);
    });

#if PLATFORM(MAC)
    RetainPtr alert = adoptNS([NSAlert new]);
    [alert setMessageText:alertTitle.get()];
    [alert setInformativeText:informativeText.get()];
    [alert addButtonWithTitle:doNotAllowButtonString.get()];
    [alert addButtonWithTitle:allowButtonString.get()];
    [alert beginSheetModalForWindow:retainPtr(webView.window).get() completionHandler:makeBlockPtr([completionBlock](NSModalResponse returnCode) {
        completionBlock(returnCode == NSAlertSecondButtonReturn);
    }).get()];
#else
    RetainPtr alert = WebKit::createUIAlertController(alertTitle.get(), informativeText.get());

    RetainPtr doNotAllowAction = [UIAlertAction actionWithTitle:doNotAllowButtonString.get() style:UIAlertActionStyleCancel handler:[completionBlock](UIAlertAction *) {
        completionBlock(false);
    }];
    RetainPtr allowAction = [UIAlertAction actionWithTitle:allowButtonString.get() style:UIAlertActionStyleDefault handler:[completionBlock](UIAlertAction *) {
        completionBlock(true);
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
