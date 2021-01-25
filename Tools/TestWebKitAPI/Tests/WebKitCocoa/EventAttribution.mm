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

#if PLATFORM(IOS_FAMILY)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>

@interface MockEventAttribution : NSObject

@property (nonatomic, assign, readonly) uint8_t sourceIdentifier;
@property (nonatomic, copy, readonly) NSURL *attributeOn;
@property (nonatomic, copy, readonly) NSURL *reportEndpoint;
@property (nonatomic, copy, readonly) NSString *sourceDescription;
@property (nonatomic, copy, readonly) NSString *purchaser;
- (instancetype)initWithReportEndpoint:(NSURL *)reportEndpoint attributeOn:(NSURL *)attributeOn;

@end

@implementation MockEventAttribution

- (instancetype)initWithReportEndpoint:(NSURL *)reportEndpoint attributeOn:(NSURL *)attributeOn
{
    if (!(self = [super init]))
        return nil;

    _sourceIdentifier = 42;
    _attributeOn = attributeOn;
    _reportEndpoint = reportEndpoint;
    _sourceDescription = @"test source description";
    _purchaser = @"test purchaser";

    return self;
}

@end

namespace TestWebKitAPI {

TEST(EventAttribution, Basic)
{
    bool done = false;
    HTTPServer server([&done, connectionCount = 0] (Connection connection) mutable {
        switch (++connectionCount) {
        case 1:
            connection.receiveHTTPRequest([connection] (Vector<char>&& request1) {
                EXPECT_TRUE(strnstr(request1.data(), "GET /conversionRequestBeforeRedirect HTTP/1.1\r\n", request1.size()));
                const char* redirect = "HTTP/1.1 302 Found\r\n"
                    "Location: /.well-known/private-click-measurement/trigger-attribution/12\r\n"
                    "Content-Length: 0\r\n\r\n";
                connection.send(redirect, [connection] {
                    connection.receiveHTTPRequest([connection] (Vector<char>&& request2) {
                        EXPECT_TRUE(strnstr(request2.data(), "GET /.well-known/private-click-measurement/trigger-attribution/12 HTTP/1.1\r\n", request2.size()));
                        const char* response = "HTTP/1.1 200 OK\r\n"
                            "Content-Length: 0\r\n\r\n";
                        connection.send(response);
                    });
                });
            });
            break;
        case 2:
            connection.receiveHTTPRequest([&done] (Vector<char>&& request3) {
                request3.append('\0');
                EXPECT_TRUE(strnstr(request3.data(), "POST / HTTP/1.1\r\n", request3.size()));
                const char* bodyBegin = strnstr(request3.data(), "\r\n\r\n", request3.size()) + strlen("\r\n\r\n");
                EXPECT_STREQ(bodyBegin, "{\"source_engagement_type\":\"click\",\"source_site\":\"127.0.0.1\",\"source_id\":42,\"attributed_on_site\":\"example.com\",\"trigger_data\":12,\"version\":1}");
                done = true;
            });
            break;
        }
    }, HTTPServer::Protocol::Https);
    NSURL *serverURL = server.request().URL;

    auto exampleURL = [NSURL URLWithString:@"https://example.com/"];
    auto attribution = [[[MockEventAttribution alloc] initWithReportEndpoint:server.request().URL attributeOn:exampleURL] autorelease];
    auto webView = [[WKWebView new] autorelease];
    webView._eventAttribution = (_UIEventAttribution *)attribution;
    [webView.configuration.websiteDataStore _setResourceLoadStatisticsEnabled:YES];
    [webView.configuration.websiteDataStore _allowTLSCertificateChain:@[(id)testCertificate().get()] forHost:serverURL.host];
    [webView _setPrivateClickMeasurementConversionURLForTesting:serverURL completionHandler:^{
        [webView _setPrivateClickMeasurementOverrideTimerForTesting:YES completionHandler:^{
            NSString *html = [NSString stringWithFormat:@"<script>fetch('%@conversionRequestBeforeRedirect',{mode:'no-cors'})</script>", serverURL];
            [webView loadHTMLString:html baseURL:exampleURL];
        }];
    }];
    Util::run(&done);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
