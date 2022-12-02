/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "HTTPServer.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKFrameTreeNode.h>

namespace TestWebKitAPI {

TEST(SiteIsolation, ProcessIdentifiers)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];

    auto configuration = server.httpsProxyConfiguration();
    auto preferences = [configuration preferences];
    for (_WKExperimentalFeature *feature in [WKPreferences _experimentalFeatures]) {
        if ([feature.key isEqualToString:@"SiteIsolationEnabled"])
            [preferences _setEnabled:YES forExperimentalFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = delegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [delegate waitForDidFinishNavigation];

    __block bool done { false };
    [webView _frames:^(_WKFrameTreeNode *mainFrame) {
        pid_t mainFramePid = mainFrame._processIdentifier;
        pid_t childFramePid = mainFrame.childFrames.firstObject._processIdentifier;
        EXPECT_NE(mainFramePid, 0);
        EXPECT_EQ(childFramePid, 0); // FIXME: The child frame should actually start its process.
        EXPECT_NE(mainFramePid, childFramePid);
        done = true;
    }];
    Util::run(&done);
}

}
