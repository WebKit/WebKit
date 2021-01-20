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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Threading.h>
#import <wtf/Vector.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

static RetainPtr<WKWebView> createdWebView;
static RetainPtr<TestNavigationDelegate> navDelegate;

@interface JavascriptURLNavigationDelegate : NSObject <WKUIDelegatePrivate>
@end
@implementation JavascriptURLNavigationDelegate

- (void)_webViewRunModal:(WKWebView *)webView
{
    EXPECT_EQ(webView, createdWebView.get());
}

- (nullable WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    createdWebView = [[[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration] autorelease];
    [createdWebView setUIDelegate:self];

    navDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navDelegate setDecidePolicyForNavigationAction:[&] (WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);
    }];
    [createdWebView setNavigationDelegate:navDelegate.get()];

    return createdWebView.get();
}

- (void)_webViewClose:(WKWebView *)webView
{
    EXPECT_EQ(webView, createdWebView.get());
    [webView _close];
}

@end

static const char* mainResource = R"JSURLRESOURCE(
<body>
The initial frame should have "Hello there" in it.<br>
The second frame should not.<br>
<script>

function createURL(data, type = 'text/html') {
  return URL.createObjectURL(new Blob([data], {type: type}));
}

function waitForLoad() {
    showModalDialog(createURL(`
        <script>
        var dataURLDelay = 400;
        var earlyReturn = false;
        function tryIt() {
            if (earlyReturn)
                return;
            try {
                opener.frame.contentDocument.x;
            } catch (e) {
                earlyReturn = true;
                setTimeout(window.close, dataURLDelay * 1.5);
            }
        };
        setTimeout(tryIt, dataURLDelay);
        setTimeout(tryIt, dataURLDelay * 1.5);
        setTimeout(tryIt, dataURLDelay * 2);
        setTimeout(tryIt, dataURLDelay * 2.5);
        setTimeout(window.close, dataURLDelay * 3);
        </scrip` + 't>'
    ));
}

function runTest() {
    window.onmessage = null;

    frame = document.createElement('iframe');
    frame.src = location;
    document.body.appendChild(frame);

    frame.contentDocument.open();
    frame.contentWindow.addEventListener('readystatechange', () => {
        a = frame.contentDocument.createElement('a');
        a.href = targetURL;
        a.click();
        waitForLoad();
    }, {capture: true, once: true});

    var javascriptSource = `
    <script>
    alert(document.documentElement.outerHTML);
    function checkIt() {
        if (document.documentElement.outerHTML.includes('Hello worl' + 'd')) {
            console.log('Failed');
            if (window.webkit.messageHandlers && window.webkit.messageHandlers.testHandler)
                window.webkit.messageHandlers.testHandler.postMessage('Failed');
        } else {
            console.log('Passed');
            if (window.webkit.messageHandlers && window.webkit.messageHandlers.testHandler)
                window.webkit.messageHandlers.testHandler.postMessage('Passed');
        }
    }
    setTimeout(checkIt, 0);
    </scrip` + 't>';
    frame.src = 'javascript:"' + javascriptSource + '"';
}
window.onmessage = runTest;

var targetSource = `
<script>
function writeIt() {
    document.write('Hello world');
    parent.postMessage('go', '` + window.location.origin + `');
}
setTimeout(writeIt, 400);
</scrip` + 't>';

targetURL = 'data:text/html,' + targetSource;
loadedOnce = document.body.appendChild(document.createElement('iframe'));
loadedOnce.src = targetURL;
</script>
</body>
)JSURLRESOURCE";


TEST(WKWebView, JavascriptURLNavigation)
{
    static bool done;

    auto delegate = adoptNS([[JavascriptURLNavigationDelegate alloc] init]);
    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"jsurl"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:delegate.get()];

    [webView performAfterReceivingMessage:@"Passed" action:^() {
        done = true;
    }];
    [webView performAfterReceivingMessage:@"Failed" action:^() {
        done = true;
        FAIL();
    }];

    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {

        if (![task.request.URL.absoluteString isEqualToString:@"jsurl://host1/main.html"]) {
            // We only expect the URL above.
            FAIL();
        }

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:mainResource length:strlen(mainResource)]];
        [task didFinish];
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"jsurl://host1/main.html"]]];

    TestWebKitAPI::Util::run(&done);
}

#endif // PLATFORM(MAC)
