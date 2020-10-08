/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKUserMediaCaptureAccessAlert.h"

#if ENABLE(MEDIA_STREAM)

#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/URLHelpers.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

static NSString* visibleDomain(const String& host)
{
    auto domain = WTF::URLHelpers::userVisibleURL(host.utf8());
    return startsWithLettersIgnoringASCIICase(domain, "www.") ? domain.substring(4) : domain;
}

static NSString *alertMessageText(API::SecurityOrigin& topLevelOrigin, _WKCaptureDevices devices)
{
    bool shouldAskUserForAccessToCamera = devices & _WKCaptureDeviceCamera;
    bool shouldAskUserForAccessToMicrophone = devices & _WKCaptureDeviceMicrophone;

    auto& origin = topLevelOrigin.securityOrigin();
    if (origin.protocol() != "http" && origin.protocol() != "https")
        return nil;

    if (shouldAskUserForAccessToCamera && shouldAskUserForAccessToMicrophone)
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera and microphone?", @"Message for user media prompt"), visibleDomain(origin.host())];
    if (shouldAskUserForAccessToCamera)
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera?", @"Message for user camera access prompt"), visibleDomain(origin.host())];
    if (shouldAskUserForAccessToMicrophone)
        return [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your microphone?", @"Message for user microphone access prompt"), visibleDomain(origin.host())];
    return nil;
}

void presentUserMediaCaptureAccessAlert(WKWebView *webView, API::SecurityOrigin& topLevelOrigin, _WKCaptureDevices devices, CompletionHandler<void(bool)>&& completionHandler)
{
    auto *alertTitle = alertMessageText(topLevelOrigin, devices);
    if (!alertTitle) {
        completionHandler(false);
        return;
    }

    auto completionBlock = makeBlockPtr([completionHandler = WTFMove(completionHandler)](bool shouldAllow) mutable {
        completionHandler(shouldAllow);
    });

    NSString *allowButtonString = WEB_UI_STRING_KEY(@"Allow", "Allow (usermedia)", @"Allow button title in user media prompt");
    NSString *doNotAllowButtonString = WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (usermedia)", @"Disallow button title in user media prompt");

#if PLATFORM(MAC)
    auto alert = adoptNS([NSAlert new]);
    [alert setMessageText:alertTitle];
    [alert addButtonWithTitle:allowButtonString];
    [alert addButtonWithTitle:doNotAllowButtonString];
    [alert beginSheetModalForWindow:webView.window completionHandler:[completionBlock](NSModalResponse returnCode) {
        auto shouldAllow = returnCode == NSAlertFirstButtonReturn;
        completionBlock(shouldAllow);
    }];
#else
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:alertTitle message:nil preferredStyle:UIAlertControllerStyleAlert];
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

} // namespace WebKit

#endif // ENABLE(DEVICE_ORIENTATION)
