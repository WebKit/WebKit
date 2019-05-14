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

#import "TCPServer.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKFoundation.h>

static size_t putPDFBytesCallback(void* info, void* buffer, size_t count)
{
    [(NSMutableData *)info appendBytes:buffer length:count];
    return count;
}

static void emptyReleaseInfoCallback(void* __nullable)
{
}

static RetainPtr<NSData> createPDFWithLinkToURL(NSURL *url)
{
    auto pdfData = adoptNS([[NSMutableData alloc] init]);
    
    CGDataConsumerCallbacks callbacks;
    callbacks.putBytes = (CGDataConsumerPutBytesCallback)putPDFBytesCallback;
    callbacks.releaseConsumer = (CGDataConsumerReleaseInfoCallback)emptyReleaseInfoCallback;
    auto consumer = adoptCF(CGDataConsumerCreate(pdfData.get(), &callbacks));
    auto contextDictionary = adoptCF(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CGRect rectangle = CGRectMake(0, 0, 1000, 1000);
    auto pdfContext = adoptCF(CGPDFContextCreate(consumer.get(), &rectangle, contextDictionary.get()));

    auto pageDictionary = adoptCF(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto boxData = adoptCF(CFDataCreate(NULL, (const UInt8 *)&rectangle, sizeof(CGRect)));
    CFDictionarySetValue(pageDictionary.get(), kCGPDFContextMediaBox, boxData.get());
    CGPDFContextBeginPage(pdfContext.get(), pageDictionary.get());
    
    CGContextSetRGBFillColor(pdfContext.get(), 1.0, 0.0, 0.0, 1.0);
    CGContextSetRGBStrokeColor(pdfContext.get(), 1.0, 0.0, 0.0, 1.0);
    CGContextFillRect(pdfContext.get(), rectangle);
    CGPDFContextSetURLForRect(pdfContext.get(), (CFURLRef)url, rectangle);
    CGPDFContextEndPage(pdfContext.get());

    return pdfData;
}

TEST(WebKit, PDFLinkReferrer)
{
    using namespace TestWebKitAPI;
    TCPServer server([] (int socket) {
        // This assumes all the data from the HTTP request is available to be read at once,
        // which is probably an okay assumption.
        auto requestBytes = TCPServer::read(socket);

        // Look for a referer header.
        const auto* currentLine = reinterpret_cast<const char*>(requestBytes.data());
        while (currentLine) {
            EXPECT_NE(strncasecmp(currentLine, "referer:", 8), 0);
            const char* nextLine = strchr(currentLine, '\n');
            currentLine = nextLine ? nextLine + 1 : 0;
        }

        const char* responseHeader =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n\r\n";
        TCPServer::write(socket, responseHeader, strlen(responseHeader));
    });

    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    webView.get().navigationDelegate = navigationDelegate.get();

    auto *linkURL = [NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%hu", server.port()]];
    auto pdfData = createPDFWithLinkToURL(linkURL);

    [webView loadData:pdfData.get() MIMEType:@"application/pdf" characterEncodingName:@"" baseURL:linkURL];
    [navigationDelegate waitForDidFinishNavigation];

    navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        EXPECT_NULL([action.request valueForHTTPHeaderField:@"Referer"]);
        decisionHandler(WKNavigationActionPolicyAllow);
    };
    
    [webView sendClicksAtPoint:NSMakePoint(75, 75) numberOfClicks:1];
    [navigationDelegate waitForDidFinishNavigation];

}

#endif // PLATFORM(MAC)
