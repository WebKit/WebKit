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
#import "Helpers/cocoa/PrintingTestUtilities.h"

#if PLATFORM(MAC) && HAVE(PDFKIT)

#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestPDFDocument.h"
#import <AppKit/AppKit.h>

@interface PrintOperationObserver : NSObject
- (void)waitForPrintOperation;
@end

@implementation PrintOperationObserver {
    bool _didRun;
}

- (void)printOperationDidRun:(NSPrintOperation *)printOperation success:(BOOL)success contextInfo:(void *)contextInfo
{
    EXPECT_TRUE(success);
    _didRun = true;
}

- (void)waitForPrintOperation
{
    TestWebKitAPI::Util::run(&_didRun);
}

@end

namespace TestWebKitAPI {

RetainPtr<TestPDFDocument> printToPDF(MakePrintOperation makePrintOperation, NSWindow *window)
{
    RetainPtr printInfo = adoptNS([[NSPrintInfo alloc] init]);
    [printInfo setPaperSize:NSMakeSize(612, 792)];

    RetainPtr pdfURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.pdf", NSUUID.UUID.UUIDString]] isDirectory:NO];
    [printInfo setJobDisposition:NSPrintSaveJob];
    [printInfo dictionary][NSPrintJobSavingURL] = pdfURL.get();

    RetainPtr operation = makePrintOperation(printInfo.get());
    [operation setShowsPrintPanel:NO];
    [operation setShowsProgressPanel:NO];

    if (window) {
        RetainPtr observer = adoptNS([PrintOperationObserver new]);
        [operation runOperationModalForWindow:window delegate:observer.get() didRunSelector:@selector(printOperationDidRun:success:contextInfo:) contextInfo:nullptr];
        [observer waitForPrintOperation];
    } else
        EXPECT_TRUE([operation runOperation]);

    RetainPtr data = adoptNS([[NSData alloc] initWithContentsOfURL:pdfURL.get()]);
    [NSFileManager.defaultManager removeItemAtURL:pdfURL.get() error:nil];
    EXPECT_NOT_NULL(data.get());
    if (!data)
        return nil;
    return adoptNS([[TestPDFDocument alloc] initFromData:data.get()]);
}

CGPoint firstCharacterPosition(TestPDFDocument *document)
{
    EXPECT_EQ([document pageCount], 1);
    RetainPtr page = [document pageAtIndex:0];
    CGRect characterRect = [page rectForCharacterAtIndex:0];
    return CGPointMake(CGRectGetMinX(characterRect), CGRectGetMaxY([page bounds]) - CGRectGetMaxY(characterRect));
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && HAVE(PDFKIT)
