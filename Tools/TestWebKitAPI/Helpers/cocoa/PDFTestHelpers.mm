/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/PDFTestHelpers.h"

#import "Helpers/cocoa/TestNSBundleExtras.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameHandle.h>
#import <wtf/RetainPtr.h>

@implementation PDFPrintUIDelegate {
    NSSize _pageSize;
    bool _receivedSize;
    RetainPtr<_WKFrameHandle> _lastPrintedFrame;
}

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame pdfFirstPageSize:(CGSize)size completionHandler:(void (^)(void))completionHandler
{
    _pageSize = size;
    _receivedSize = true;
    _lastPrintedFrame = frame;
    completionHandler();
}

- (NSSize)waitForPageSize
{
    _receivedSize = false;
    while (!_receivedSize)
        TestWebKitAPI::Util::spinRunLoop();
    return _pageSize;
}

- (_WKFrameHandle *)lastPrintedFrame
{
    return _lastPrintedFrame.get();
}

@end

namespace TestWebKitAPI {

RetainPtr<WKWebViewConfiguration> configurationForWebViewTestingUnifiedPDF(bool hudEnabled)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"UnifiedPDFEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
        if ([feature.key isEqualToString:@"PDFPluginHUDEnabled"])
            [[configuration preferences] _setEnabled:static_cast<BOOL>(hudEnabled) forFeature:feature];
    }

    return configuration;
}

RetainPtr<NSData> testPDFData()
{
    return [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
}

static size_t appendPDFBytes(void* info, const void* buffer, size_t count)
{
    [(NSMutableData *)info appendBytes:buffer length:count];
    return count;
}

RetainPtr<NSData> testPDFDataWithLink()
{
    RetainPtr pdfData = adoptNS([[NSMutableData alloc] init]);

    CGDataConsumerCallbacks callbacks { appendPDFBytes, nullptr };
    RetainPtr consumer = adoptCF(CGDataConsumerCreate(pdfData, &callbacks));

    CGRect mediaBox = CGRectMake(0, 0, 400, 200);
    RetainPtr pdfContext = adoptCF(CGPDFContextCreate(consumer, &mediaBox, nullptr));

    CGPDFContextBeginPage(pdfContext, nullptr);

    RetainPtr font = adoptCF(CTFontCreateWithName((CFStringRef)@"Helvetica", 24, nullptr));
    RetainPtr attributedString = adoptNS([[NSAttributedString alloc] initWithString:@"Visit our website for details" attributes:@{ (id)kCTFontAttributeName: (id)font.get() }]);
    RetainPtr line = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)attributedString.get()));

    CGFloat baselineX = 20;
    CGFloat baselineY = 100;
    CGContextSetTextPosition(pdfContext, baselineX, baselineY);
    CTLineDraw(line, pdfContext);

    // "Visit our website for details"
    //  0123456789...
    // "our website" spans [6, 17).
    CGFloat linkStartX = baselineX + CTLineGetOffsetForStringIndex(line, 6, nullptr);
    CGFloat linkEndX = baselineX + CTLineGetOffsetForStringIndex(line, 17, nullptr);

    CGFloat ascent = 0;
    CGFloat descent = 0;
    CGFloat leading = 0;
    CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

    CGRect linkRect = CGRectMake(linkStartX, baselineY - descent, linkEndX - linkStartX, ascent + descent);
    CGPDFContextSetURLForRect(pdfContext, (CFURLRef)[NSURL URLWithString:@"https://www.example.com/"], linkRect);

    CGPDFContextEndPage(pdfContext);
    CGPDFContextClose(pdfContext);

    return pdfData;
}

}
