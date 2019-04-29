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
#import "WKOrientationAccessAlert.h"

#if ENABLE(DEVICE_ORIENTATION)

#import "WKWebViewInternal.h"
#import <WebCore/LocalizedStrings.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

void presentOrientationAccessAlert(WKWebView *view, const String& host, CompletionHandler<void(bool)>&& completionHandler)
{
    NSString *alertText = [NSString stringWithFormat:WEB_UI_NSSTRING(@"“%@” Would Like to Access Motion and Orientation", @"Message for requesting access to the device motion and orientation"), (NSString *)host];
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:alertText message:nil preferredStyle:UIAlertControllerStyleAlert];

    auto completionBlock = makeBlockPtr([completionHandler = WTFMove(completionHandler)](bool shouldAllow) mutable {
        completionHandler(shouldAllow);
    });

    NSString *cancelButtonString = WEB_UI_STRING_KEY(@"Cancel", "Cancel (device motion and orientation access)", @"Button title in Device Orientation Permission API prompt");
    UIAlertAction* cancelAction = [UIAlertAction actionWithTitle:cancelButtonString style:UIAlertActionStyleCancel handler:[completionBlock](UIAlertAction *action) {
        completionBlock(false);
    }];

    NSString *allowButtonString = WEB_UI_STRING_KEY(@"Allow", "Allow (device motion and orientation access)", @"Button title in Device Orientation Permission API prompt");
    UIAlertAction* allowAction = [UIAlertAction actionWithTitle:allowButtonString style:UIAlertActionStyleDefault handler:[completionBlock](UIAlertAction *action) {
        completionBlock(true);
    }];
    [alert addAction:cancelAction];
    [alert addAction:allowAction];

    [[UIViewController _viewControllerForFullScreenPresentationFromView:view] presentViewController:alert animated:YES completion:nil];
}

} // namespace WebKit

#endif // ENABLE(DEVICE_ORIENTATION)
